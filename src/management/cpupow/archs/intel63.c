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
#define DEBUG_POWER_CONSUMING 1

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <common/output/debug.h>
#include <common/utils/keeper.h>
#include <common/system/monitor.h>
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
    uint time_window_10s;
    uint time_window_1s;
    uint time_window_100ms;
    uint time_window_10ms;
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
    int                capable;
    msr_power_limit_t *pl;
    off_t              pl_address; // MSR_*_POWER_LIMIT
    int                pl_bit_lock;
    double             pl_divisor; // Time Window divisor
    const char        *pl_keeper_name;
    off_t              es_address; // MSR_*_ENERGY_STATUS
    off_t              po_address; // MSR_*_POLICY
    off_t              ps_address; // MSR_*_PERF_STATUS
    msr_power_info_t  *pi;
    off_t              pi_address; // MSR_*_POWER_INFO
    timestamp_t        pm_time; // Power Monitor
    ullong            *pm_energy;
};

#define PRIM_INIT(_tp, dom, pl_bl, pl_dv, pl_kn) { \
	.tp = _tp, \
    .name = #dom, \
    .capable = 1, \
	.pl_address = MSR_ ## dom ## _POWER_LIMIT, \
    .pl_bit_lock = pl_bl, \
    .pl_divisor = pl_dv, \
    .pl_keeper_name = pl_kn, \
    .es_address = MSR_ ## dom ## _ENERGY_STATUS, \
	.po_address = MSR_ ## dom ## _POLICY, \
	.ps_address = MSR_ ## dom ## _PERF_STATUS, \
	.pi_address = MSR_ ## dom ## _POWER_INFO \
}

static struct primitive_s prims[] = {
    PRIM_INIT(&tp_sockets, PKG,  63,  4.0, "MsrPkgPowerLimit"),
    PRIM_INIT(&tp_sockets, DRAM, 31, 10.0, "MsrDramPowerLimit"),
    PRIM_INIT(&tp_cores  , PP0,  31, 10.0, "MsrPp0PowerLimit"),
    PRIM_INIT(&tp_sockets, PP1,  31, 10.0, "MsrPp1PowerLimit"),
    { NULL, NULL, 0, NULL, 0, 0, 0, NULL, 0, 0, 0, NULL, 0 },
};

static int prim_read_power_info(const char *name, off_t address, uint cpu, msr_power_info_t *pi)
{
    if ((msr_read(cpu, &pi->reg, sizeof(ullong), address))) {
        debug("Failed while reading MSR_%s_POWER_INFO: %s", name, state_msg)
        return 0;
    }
    // TODO: don't know if this is correct, can be 0 a domain which is capable?
    if (pi->reg == 0LLU) {
        debug("Failed while reading MSR_%s_POWER_INFO: the register is empty", name);
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

static uint prim_window_power_limit(double time_s, double div)
{
    double time_nearest = 0;
    double time_current = 0;
    uint y_nearest = 0;
    uint z_nearest = 0;
    uint y, z;
    // Time Window DRAM/PP0/PP1 -> (2^Y)*(1+(Z/10) * TU -> Y(21:17) & Z(23:22) (Z is F in documents)
    // Time Window PKG          -> (2^Y)*(1+(Z/4)  * TU -> Y(21:17) & Z(23:22)
    for (y = 0; y < 32; ++y) { // Y(21:17) is 5 bits, 31 max integer
        for (z = 0; z < 4; ++z) { // Z(23:22) 2 bits, 3 max integer
            time_current = (pow(2.0, ((double) y)) * (1.0 + (((double) z)/div))) * mult_time;
            // Selecting the nearest time window under time
            if (time_current <= time_s && time_current > time_nearest) {
                time_nearest = time_current;
                y_nearest = y;
                z_nearest = z;
            }
        }
    }
    return (uint) (setbits64(0LLU, z_nearest, 6, 5) | setbits64(0LLU, y_nearest, 4, 0));
}

static void prim_bit_power_limit(const char *name, uint cpu, msr_power_limit_t *pl, int bl, double div)
{
    char *enabled[2] = { "disabled", "enabled" };
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
    pl->pl1_timew  = (double) (pow(2.0, pl->pl1_timey) * (1.0 + ((double) pl->pl1_timez)/div)) * mult_time;
    pl->pl2_timew  = (double) (pow(2.0, pl->pl2_timey) * (1.0 + ((double) pl->pl2_timez)/div)) * mult_time;
    debug("%s[%d].pl1_power: %6u W [%u * %0.3lf]", name, cpu, pl->pl1_power, (uint) getbits64(pl->reg, 14,  0), mult_power);
    debug("%s[%d].pl2_power: %6u W [%u * %0.3lf]", name, cpu, pl->pl2_power, (uint) getbits64(pl->reg, 46, 32), mult_power);
    debug("%s[%d].pl1_timew: %06.3lf S [(2^%02u * (1+(%u/4))) * %lf]", name, cpu, pl->pl1_timew, pl->pl1_timey, pl->pl1_timez, mult_time);
    debug("%s[%d].pl2_timew: %06.3lf S [(2^%02u * (1+(%u/4))) * %lf]", name, cpu, pl->pl2_timew, pl->pl2_timey, pl->pl2_timez, mult_time);
    debug("%s[%d].more     : pl1 %s, pl2 %s, lock %d (reg 0x%llx)", name, cpu, enabled[pl->pl1_enable], enabled[pl->pl2_enable], pl->lock, pl->reg);
    // Calculating time window for future use
    pl->time_window_10ms  = prim_window_power_limit( 0.012, div); // 10 ms (well ~)
    pl->time_window_100ms = prim_window_power_limit( 0.110, div); // 100 ms
    pl->time_window_1s    = prim_window_power_limit( 1.000, div); // 1 s
    pl->time_window_10s   = prim_window_power_limit(10.000, div); // 10 s
    debug("%s[%d].gen_timew: 0x%x 10ms, 0x%x 100ms, 0x%x 1s, 0x%x 10s",
          name, cpu, pl->time_window_10ms, pl->time_window_100ms, pl->time_window_1s, pl->time_window_10s);
}

static int prim_read_power_limit(const char *name, off_t address, uint cpu, msr_power_limit_t *pl, int bl, double div)
{
    if ((msr_read(cpu, &pl->reg, sizeof(ullong), address))) {
        debug("Failed while reading MSR_%s_POWER_LIMIT: %s", name, state_msg)
        return 0;
    }
    // TODO: don't know if this is correct, can be 0 a domain which is capable?
    if (pl->reg == 0LLU) {
        debug("Failed while reading MSR_%s_POWER_LIMIT: the register is empty", name);
        return 0;
    }
    prim_bit_power_limit(name, cpu, pl, bl, div);
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
        if (!prim_read_power_limit(p->name, p->pl_address, p->tp->cpus[i].id, &p->pl[i], p->pl_bit_lock, p->pl_divisor)) {
            return 0;
        }
        debug("----------------------------------------");
        // Recovering MSR_*_POWER_LIMIT registers
        sprintf(buffer, "%s%d", p->pl_keeper_name, i);
        if (!keeper_load_uint64(buffer, &p->pl[i].reg)) {
            keeper_save_uint64(buffer, p->pl[i].reg);
        } else {
            debug("Loaded %s from keeper:", buffer);
            prim_bit_power_limit(p->name, p->tp->cpus[i].id, &p->pl[i], p->pl_bit_lock, p->pl_divisor);
            debug("----------------------------------------");
        }
    }
    return 1;
}

#if DEBUG_POWER_CONSUMING
static state_t debug_monitor(void *x)
{
    struct primitive_s *p = &prims[CPUPOW_SOCKET];
    static ullong energy1[2] = { 0, 0 };
    static ullong energy2[2] = { 0, 0 };
    double energy;
    double power;
    ullong reg;
    int i;

    for (i = 0; i < p->tp->cpu_count; ++i) {
        if (msr_read(p->tp->cpus[i].id, &reg, sizeof(ullong), p->es_address)) {
            debug("msr_read: %s", state_msg);
        }
        energy2[i] = getbits64(reg, 31, 0);
        if (energy1[i] > 0) {
            energy = ((double) (energy2[i] - energy1[i])) * mult_energy;
            power  = energy / 2.0; // We know its 2 seconds
            debug("SOCKET[%u] energy: %lf J, power: %lf W", i, energy, power);
        }
        energy1[i] = energy2[i];
    }
}
#endif

void mgt_cpupow_intel63_load(topology_t *tp_in, mgt_cpupow_ops_t *ops)
{
    int i;

    if (state_fail(msr_test(tp_in, MSR_WR))) {
        debug("failed msr test: %s", state_msg);
        return;
    }
    if (tp_in->vendor == VENDOR_INTEL && tp_in->model >= MODEL_HASWELL_X) {
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
        // Get the multipliers (TODO: use multipliers per socket, not global)
        mult_power  = pow(0.5, (double) getbits64(mult_reg,  3,  0));
        mult_energy = pow(0.5, (double) getbits64(mult_reg, 12,  8));
        mult_time   = pow(0.5, (double) getbits64(mult_reg, 19, 16));
        debug("SOCKET[%d].mult_power : %lf W [1/(2^%u)]", i, mult_power , (uint) getbits64(mult_reg,  3,  0));
        debug("SOCKET[%d].mult_energy: %lf J [1/(2^%u)]", i, mult_energy, (uint) getbits64(mult_reg, 12,  8));
        debug("SOCKET[%d].mult_time  : %lf S [1/(2^%u)]", i, mult_time  , (uint) getbits64(mult_reg, 19, 16));
        debug("----------------------------------------");
    }
    // PKG and DRAM initializations
    for(i = 0; prims[i].capable; ++i) {
        if (!prim_init(i)) {
            prims[i].capable = 0;
            debug("----------------------------------------");
        }
    }
    if (!prims[CPUPOW_SOCKET].capable) {
        return;
    }
    // Extreme debugging, power monitor
    #if DEBUG_POWER_CONSUMING
    suscription_t *sus = suscription();
    sus->call_main     = debug_monitor;
    sus->time_relax    = 2000;
    sus->time_burst    = 2000;
    sus->suscribe(sus);
    monitor_init();
    #endif
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
    return prims[domain].capable;
}

state_t mgt_cpupow_intel63_powercap_is_enabled(int domain, uint *enabled)
{
    struct primitive_s *p = &prims[domain];
    msr_power_limit_t pl;
    int i;

    // Cleaning
    memset(enabled, 0, sizeof(uint)*p->tp->cpu_count);
    //
    for (i = 0; i < p->tp->cpu_count; ++i) {
        if (prim_read_power_limit(p->name, p->pl_address, p->tp->cpus[i].id, &pl, p->pl_bit_lock, p->pl_divisor)) {
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
        if (prim_read_power_limit(p->name, p->pl_address, p->tp->cpus[i].id, &pl, p->pl_bit_lock, p->pl_divisor)) {
            return EAR_ERROR;
        }
        if (watts != NULL && pl.pl1_enable) {
            watts[i] = pl.pl1_power;
        }
    }
    return EAR_SUCCESS;
}

state_t mgt_cpupow_intel63_powercap_set(int domain, uint *watts)
{
    struct primitive_s *p = &prims[domain];
    double power;
    ullong reg;
    int i;

    if (!p->capable) {
        return EAR_ERROR;
    }
    for (i = 0; i < p->tp->cpu_count; ++i) {
        // Getting base register
        reg = p->pl[i].reg;
        // If is disabling
        if (watts[i] == POWERCAP_DISABLE) {
            // By now we are just using PL#1 (TODO: use TDP in power field)
            reg = setbits64(reg, 0, 15, 15); // enable
            debug("%s%d: disabling PL#1 (0x%llx)", p->name, p->tp->cpus[i].id, reg);
        } else {
            // Getting power value understood by RAPL
            power = ((double) watts[i]) / mult_power;
            reg   = setbits64(reg,        (ullong) power, 14,  0); // power limit
            reg   = setbits64(reg,                     1, 15, 15); // enable
            reg   = setbits64(reg, p->pl->time_window_1s, 23, 17); // time window
            debug("%s%d: writting watts %u -> %lf, regs %llu %llu",
                  p->name, p->tp->cpus[i].id, watts[i], power, p->pl->reg, reg);
        }
        if (state_fail(msr_write(p->tp->cpus[i].id, &reg, sizeof(ullong), p->pl_address))) {
            debug("Failed while writing MSR_%s_POWER_LIMIT[%d]: %s", p->name, p->tp->cpus[i].id, state_msg);
        }
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

    if (!p->capable) {
        return EAR_ERROR;
    }
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