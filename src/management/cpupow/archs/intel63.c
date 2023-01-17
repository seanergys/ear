/*
*
* This program is part of the EAR software.
*
* EAR provides a dynamic, transparent and ligth-weigth solution for
* Energy management. It has been developed in the context of the
* Barcelona Supercomputing Center (BSC)&Lenovo Collaboration project.
*
* Copyright © 2017-present BSC-Lenovo
* BSC Contact   mailto:ear-support@bsc.es
* Lenovo contact  mailto:hpchelp@lenovo.com
*
* This file is licensed under both the BSD-3 license for individual/non-commercial
* use and EPL-1.0 license for commercial use. Full text of both licenses can be
* found in COPYING.BSD and COPYING.EPL files.
*/

#define SHOW_DEBUGS 1

#include <math.h>
#include <stdlib.h>
#include <common/output/debug.h>
#include <common/utils/keeper.h>
#include <common/hardware/bithack.h>
#include <metrics/common/msr.h>
#include <metrics/common/apis.h>
#include <management/cpupow/cpupow.h>
#include <management/cpupow/archs/intel63.h>

#define MSR_NO_REG              0x000
#define MSR_RAPL_POWER_UNIT     0x606
#define MSR_PKG_POWER_LIMIT     0x610
#define MSR_PKG_ENERGY_STATUS   0x611
#define MSR_PKG_POLICY          MSR_NO_REG
#define MSR_PKG_PERF_STATUS     0x613
#define MSR_PKG_POWER_INFO      0x614
#define MSR_DRAM_POWER_LIMIT    0x618
#define MSR_DRAM_ENERGY_STATUS  0x619
#define MSR_DRAM_POLICY         MSR_NO_REG
#define MSR_DRAM_PERF_STATUS    0x61b
#define MSR_DRAM_POWER_INFO     0x61c
#define MSR_PP0_POWER_LIMIT     0x638
#define MSR_PP0_ENERGY_STATUS   0x639
#define MSR_PP0_POLICY          0x63a
#define MSR_PP0_PERF_STATUS     MSR_NO_REG
#define MSR_PP0_POWER_INFO      MSR_NO_REG
#define MSR_PP1_POWER_LIMIT     0x640
#define MSR_PP1_ENERGY_STATUS   0x641
#define MSR_PP1_POLICY          0x642
#define MSR_PP1_PERF_STATUS     MSR_NO_REG
#define MSR_PP1_POWER_INFO      MSR_NO_REG

// 7 bits: 2 + 5, this is for a default Time Unit of 1/(2^10) = 0.000977
#define TIME_WINDOW_1S      0x0a // (2^10)×(1+(0÷4)) * Time Unit (0.000977) = 10 & 0 = 0001010 = 0x0a
#define TIME_WINDOW_100MS   0x66 // (2^6 )×(1+(3÷4)) * Time unit (0.000977) = 6  & 3 = 1100110 = 0x66
#define TIME_WINDOW_10MS    0x43 // (2^3 )×(1+(2÷4)) * Time unit (0.000977) = 3  & 2 = 1000011 = 0x36

typedef struct msr_power_limit_s {
    ullong reg;
    uint pl1_power; // Power limit #1 (long term)
    uint pl1_enable;
    uint pl1_clamp;
    uint pl1_timey; // time Y
    uint pl1_timez; // time Z
    double pl1_timew; // time window
    uint pl2_power; // Power limit #2 (short term)
    uint pl2_enable;
    uint pl2_clamp;
    uint pl2_timey; // time Y
    uint pl2_timez; // time Z
    double pl2_timew; // time window
    uint lock;
} msr_power_limit_t;

typedef struct msr_power_info_s {
    ullong reg;
    uint max_power; //W
    uint min_power; //W
    double max_timew; //time window (uS)
    uint tdp; //W
} msr_power_info_t;

static topology_t tp_cores;
static topology_t tp_sockets;
static ullong     mult_reg;
static double     mult_power;
static double     mult_energy;
static double     mult_time;

struct primitive_s {
    topology_t        *tp;
    const char        *name;
    msr_power_limit_t *pl;
    off_t              pl_address; // MSR_*_POWER_LIMIT
    int                pl_bit_lock;
    const char        *pl_keeper_name;
    off_t              es_address; // MSR_*_ENERGY_STATUS
    off_t              po_address; // MSR_*_POLICY
    off_t              ps_address; // MSR_*_PERF_STATUS
    msr_power_info_t  *pi;
    off_t              pi_address; // MSR_*_POWER_INFO
};

#define PRIM_INIT(_tp, domain, pl_bl, pl_kn) { \
	.tp = _tp, \
    .name = #domain, \
	.pl_address = MSR_ ## domain ## _POWER_LIMIT, \
    .pl_bit_lock = pl_bl, \
    .pl_keeper_name = pl_kn, \
    .es_address = MSR_ ## domain ## _ENERGY_STATUS, \
	.po_address = MSR_ ## domain ## _POLICY, \
	.ps_address = MSR_ ## domain ## _PERF_STATUS, \
	.pi_address = MSR_ ## domain ## _POWER_INFO \
}

static struct primitive_s prims[] = {
    PRIM_INIT(&tp_sockets, PKG, 63, "MsrPkgPowerLimit"),
    PRIM_INIT(&tp_sockets, DRAM, 31, "MsrDramPowerLimit"),
    { NULL, 0, 0, 0, 0, 0, 0 },
};

static int prim_read_power_info(const char *name, off_t address, uint cpu, msr_power_info_t *pi)
{
    if ((msr_read(cpu, &pi->reg, sizeof(ullong), address))) {
        return 0;
    }
    pi->tdp       = (uint) (((double) getbits64(pi->reg, 14,  0)) * mult_power);
    pi->min_power = (uint) (((double) getbits64(pi->reg, 30, 16)) * mult_power);
    pi->max_power = (uint) (((double) getbits64(pi->reg, 46, 32)) * mult_power);
    pi->max_timew =        (((double) getbits64(pi->reg, 53, 48)) * mult_time );
    debug("%s[%d].tdp      : %6u W [%4llu * %lf]", name, cpu, pi->tdp      , getbits64(pi->reg, 14,  0), mult_power);
    debug("%s[%d].min_power: %6u W [%4llu * %lf]", name, cpu, pi->min_power, getbits64(pi->reg, 30, 16), mult_power);
    debug("%s[%d].max_power: %6u W [%4llu * %lf]", name, cpu, pi->max_power, getbits64(pi->reg, 46, 32), mult_power);
    debug("%s[%d].max_timew: %06.3lf S [%4llu * %lf] (?)", name, cpu, pi->max_timew, getbits64(pi->reg, 53, 48), mult_time );
    return 1;
}

static void prim_bit_power_limit(const char *name, uint cpu, msr_power_limit_t *pl, int bl)
{
    char *enabled[2] = { "enabled", "disabled" };
    pl->lock       = (uint) getbits64(pl->reg, bl, bl);
    pl->pl1_enable = (uint) getbits64(pl->reg, 15, 15);
    pl->pl2_enable = (uint) getbits64(pl->reg, 47, 47);
    pl->pl1_clamp  = (uint) getbits64(pl->reg, 16, 16);
    pl->pl2_clamp  = (uint) getbits64(pl->reg, 48, 48);
    pl->pl1_timey  = (uint) getbits64(pl->reg, 21, 17);
    pl->pl1_timez  = (uint) getbits64(pl->reg, 23, 22);
    pl->pl2_timey  = (uint) getbits64(pl->reg, 53, 49);
    pl->pl2_timez  = (uint) getbits64(pl->reg, 55, 54);
    pl->pl1_power  = (uint) ((double) (getbits64(pl->reg, 14,  0) * mult_power));
    pl->pl2_power  = (uint) ((double) (getbits64(pl->reg, 46, 32) * mult_power));
    pl->pl1_timew  = (double) (pow(2.0, pl->pl1_timey) * (1.0 + ((double) pl->pl1_timez)/4.0)) * mult_time;
    pl->pl2_timew  = (double) (pow(2.0, pl->pl2_timey) * (1.0 + ((double) pl->pl2_timez)/4.0)) * mult_time;
    debug("%s[%d].pl1_power: %6u W [%u * %0.3lf]", name, cpu, pl->pl1_power, (uint) getbits64(pl->reg, 14,  0), mult_power);
    debug("%s[%d].pl2_power: %6u W [%u * %0.3lf]", name, cpu, pl->pl2_power, (uint) getbits64(pl->reg, 46, 32), mult_power);
    debug("%s[%d].pl1_time : %06.3lf S [(2^%02u * (1+(%u/4))) * %lf]", name, cpu, pl->pl1_timew, pl->pl1_timey, pl->pl1_timez, mult_time);
    debug("%s[%d].pl2_time : %06.3lf S [(2^%02u * (1+(%u/4))) * %lf]", name, cpu, pl->pl2_timew, pl->pl2_timey, pl->pl2_timez, mult_time);
    debug("%s[%d].more     : pl1 %s, pl2 %s, lock %d", name, cpu, enabled[pl->pl1_enable], enabled[pl->pl2_enable], pl->lock);
}

static int prim_read_power_limit(const char *name, off_t address, uint cpu, msr_power_limit_t *pl, int bl)
{
    if ((msr_read(cpu, &pl->reg, sizeof(ullong), address))) {
        return 0;
    }
    prim_bit_power_limit(name, cpu, pl, bl);
    return 1;
}

static int prim_init(int domain)
{
    struct primitive_s *p = &prims[domain];
    char buffer[64];
    int i;

    // Allocation
    p->pi = calloc(p->tp->cpu_count, sizeof(msr_power_info_t));
    p->pl = calloc(p->tp->cpu_count, sizeof(msr_power_limit_t));
    // 
    for (i = 0; i < p->tp->cpu_count; ++i) {
        // MSR_*_POWER_INFO
        if (!prim_read_power_info(p->name, p->pi_address, p->tp->cpus[i].id, &p->pi[i])) {
            return 0;
        }
        // MSR_*_POWER_LIMIT
        if (!prim_read_power_limit(p->name, p->pl_address, p->tp->cpus[i].id, &p->pl[i], p->pl_bit_lock)) {
            return 0;
        }
        debug("----------------------------------------");
        // Recovering MSR_*_POWER_LIMIT registers
        sprintf(buffer, "%s%d", p->pl_keeper_name, i);
        if (!keeper_load_uint64(buffer, &p->pl[i].reg)) {
            keeper_save_uint64(buffer, p->pl[i].reg);
        } else {
            debug("Loaded %s from keeper:", buffer);
            prim_bit_power_limit(p->name, p->tp->cpus[i].id, &p->pl[i], p->pl_bit_lock);
            debug("----------------------------------------");
        }
    }
    return 1;
}

void mgt_cpupow_intel63_load(topology_t *tp_in, mgt_cpupow_ops_t *ops)
{
    int i;

    if (state_fail(msr_test(tp_in, MSR_WR))) {
        debug("failed msr test: %s", state_msg);
        return;
    }
    if (tp_in->vendor == VENDOR_INTEL && tp_in->model >= MODEL_HASWELL_X) {
    } else if (tp_in->vendor == VENDOR_AMD && tp_in->family >= FAMILY_ZEN) {
    } else {
        // API incompatible
        debug("api incompatible");
        return;
    }
    topology_select(tp_in, &tp_cores  , TPSelect.core  , TPGroup.merge, 0);
    topology_select(tp_in, &tp_sockets, TPSelect.socket, TPGroup.merge, 0);
    // Opening MSRs
    for (i = 0; i < tp_cores.cpu_count; ++i) {
        if (state_fail(msr_open(tp_cores.cpus[i].id, MSR_WR))) {
            debug("msr_open: %s", state_msg);
            return;
        }
    }
    // MSR_RAPL_POWER_UNIT
    for (i = 0; i < tp_sockets.cpu_count; ++i) {
        if (state_fail(msr_read(tp_sockets.cpus[i].id, &mult_reg, sizeof(ullong), MSR_RAPL_POWER_UNIT))) {
            debug("msr_read: %s", state_msg);
            return;
        }
        // Get the multipliers
        mult_power  = pow(0.5, (double) getbits64(mult_reg,  3,  0));
        mult_energy = pow(0.5, (double) getbits64(mult_reg, 12,  8));
        mult_time   = pow(0.5, (double) getbits64(mult_reg, 19, 16));
        debug("SOCKET[%d].mult_power : %lf W [1/(2^%u)]", i, mult_power , (uint) getbits64(mult_reg,  3,  0));
        debug("SOCKET[%d].mult_energy: %lf J [1/(2^%u)]", i, mult_energy, (uint) getbits64(mult_reg, 12,  8));
        debug("SOCKET[%d].mult_time  : %lf S [1/(2^%u)]", i, mult_time  , (uint) getbits64(mult_reg, 19, 16));
        debug("----------------------------------------");
    }
    // PKG and DRAM initializations
    if (!prim_init(CPUPOW_SOCKET)) return;
    if (!prim_init(CPUPOW_DRAM)) return;
    //
    apis_put(ops->get_api            , mgt_cpupow_intel63_get_api);
    apis_put(ops->count_devices      , mgt_cpupow_intel63_count_devices);
    apis_put(ops->is_powercap_capable, mgt_cpupow_intel63_is_powercap_capable);
    apis_put(ops->powercap_is_enabled, mgt_cpupow_intel63_powercap_is_enabled);
    apis_put(ops->powercap_get       , mgt_cpupow_intel63_powercap_get);
    apis_put(ops->powercap_set       , mgt_cpupow_intel63_powercap_set);
    apis_put(ops->powercap_reset     , mgt_cpupow_intel63_powercap_reset);
    apis_put(ops->tdp_get            , mgt_cpupow_intel63_tdp_get);
}

void mgt_cpupow_intel63_get_api(uint *api)
{
    *api = API_INTEL63;
}

int mgt_cpupow_intel63_count_devices(int domain)
{
    return prims[domain].tp->cpu_count;
}

int mgt_cpupow_intel63_is_powercap_capable(int domain)
{
    // TODO: a method to get the capability of different domains
    return 1;
}

state_t mgt_cpupow_intel63_powercap_is_enabled(int domain, uint *enabled)
{
    struct primitive_s *p = &prims[domain];
    msr_power_limit_t pl;
    int i;

    // Cleaning
    memset(0, 0, sizeof(uint)*p->tp->cpu_count);
    //
    for (i = 0; i < p->tp->cpu_count; ++i) {
        if (prim_read_power_limit(p->name, p->pl_address, p->tp->cpus[i].id, &pl, p->pl_bit_lock)) {
            return EAR_ERROR;
        }
        enabled[i] = pl.pl1_enable;
    }
    return EAR_SUCCESS;
}

state_t mgt_cpupow_intel63_powercap_get(int domain, uint *watts)
{
    struct primitive_s *p = &prims[domain];
    msr_power_limit_t pl;
    int i;

    // Cleaning
    if (watts != NULL) {
        memset(watts, 0, sizeof(uint)*p->tp->cpu_count);
    }
    //
    for (i = 0; i < p->tp->cpu_count; ++i) {
        if (prim_read_power_limit(p->name, p->pl_address, p->tp->cpus[i].id, &pl, p->pl_bit_lock)) {
            return EAR_ERROR;
        }
        if (pl.pl1_enable) {
            watts[i] = pl.pl1_power;
        }
    }
    return EAR_SUCCESS;
}

state_t mgt_cpupow_intel63_powercap_set(int domain, uint *watts)
{
    // TODO: formula to get time window
    struct primitive_s *p = &prims[domain];
    double power;
    ullong reg;
    int i;

    for (i = 0; i < p->tp->cpu_count; ++i) {
        // Getting power value
        power = ((double) watts[i]) / mult_power;
        // Writing
        reg = p->pl->reg;
        reg = setbits64(reg,   (ullong) power, 14,  0); // power limit
        reg = setbits64(reg,                1, 15, 15); // enable
        reg = setbits64(reg, TIME_WINDOW_10MS, 23, 17); // time window
        debug("writting watts %u -> %lf, regs %llu %llu", watts[i], power, p->pl->reg, reg);
        msr_write(p->tp->cpus[i].id, &reg, sizeof(ullong), p->pl_address);
    }
    #if SHOW_DEBUGS
    mgt_cpupow_intel63_powercap_get(domain, NULL); 
    #endif
    return EAR_SUCCESS;
}

state_t mgt_cpupow_intel63_powercap_reset(int domain)
{
    struct primitive_s *p = &prims[domain];
    int i;

    for (i = 0; i < p->tp->cpu_count; ++i) {
        msr_write(p->tp->cpus[i].id, &p->pl->reg, sizeof(ullong), p->pl_address);
    }
    return EAR_SUCCESS;
}

state_t mgt_cpupow_intel63_tdp_get(int domain, uint *watts)
{
    struct primitive_s *p = &prims[domain];
    int i;

    // Cleaning
    memset(watts, 0, sizeof(uint)*p->tp->cpu_count);
    // Iterating over sockets
    for (i = 0; i < p->tp->cpu_count; ++i) {
        watts[i] = p->pi[i].tdp;
        debug("read tdp: %u", watts[i]);
    }
    return EAR_SUCCESS;
}
