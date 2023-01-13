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
#include <common/output/debug.h>
#include <common/hardware/bithack.h>
#include <metrics/common/msr.h>
#include <metrics/common/apis.h>
#include <management/cpupow/cpupow.h>
#include <management/cpupow/archs/intel63.h>

#define MSR_RAPL_POWER_UNIT     0x606
#define MSR_PKG_POWER_LIMIT     0x610
#define MSR_PKG_ENERGY_STATUS   0x611
#define MSR_PKG_PERF_STATUS     0x613
#define MSR_PKG_POWER_INFO      0x614
#define MSR_DRAM_POWER_LIMIT    0x618
#define MSR_DRAM_ENERGY_STATUS  0x619
#define MSR_DRAM_PERF_STATUS    0x61b
#define MSR_DRAM_POWER_INFO     0x61c
#define MSR_PP0_POWER_LIMIT     0x638
#define MSR_PP0_ENERGY_STATUS   0x639
#define MSR_PP0_POLICY          0x63a
#define MSR_PP0_PERF_STATUS     0x000
#define MSR_PP1_POWER_LIMIT     0x640
#define MSR_PP1_ENERGY_STATUS   0x641
#define MSR_PP1_POLICY          0x642
#define MSR_PP1_PERF_STATUS     0x000

typedef struct msr_power_info_s {
    uint max_power; //W
    uint min_power; //W
    uint max_timew; //time window (uS)
    uint tdp; //W
} msr_power_info_t;

typedef struct msr_power_unit_s {
    double mult_power;
    double mult_energy;
    double mult_time;
} msr_power_unit_t;

typedef struct msr_power_limit_s {
    ullong reg;
    char keep_name[64];
    uint keep_load;
    uint pl1_power;
    uint pl1_enable;
    uint pl1_clamp;
    uint pl1_timew;
    uint pl2_power;
    uint pl2_enable;
    uint pl2_clamp;
    uint pl2_timew;
    uint lock;
} msr_power_limit_t;

static topology_t         tp_cores;
static topology_t         tp_sockets;
static msr_power_info_t  *pi_sock;
static msr_power_info_t  *pi_dram;
static msr_power_unit_t  *pu_sock; // its read per socket, but is used in all components
static msr_power_limit_t *pl_sock;
static msr_power_limit_t *pl_dram;

void mgt_cpupow_intel63_load(topology_t *tp_in, mgt_cpupow_ops_t *ops)
{
    ullong reg = 0;
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

    for (i = 0; i < tp_cores.cpu_count; ++i) {
        if (state_fail(msr_open(tp_cores.cpus[i].id, MSR_WR))) {
            debug("msr_open: %s", state_msg);
            return;
        }
    }
    // Memory allocation
    pi_sock = calloc(tp_sockets.cpu_count, sizeof (msr_power_info_t));
    pi_dram = calloc(tp_sockets.cpu_count, sizeof (msr_power_info_t));
    pu_sock = calloc(tp_sockets.cpu_count, sizeof (msr_power_unit_t));
    // MSR_RAPL_POWER_UNIT
    for (i = 0; i < tp_sockets.cpu_count; ++i) {
        if (state_fail(msr_read(tp_sockets.cpus[i].id, &reg, sizeof(ullong), MSR_RAPL_POWER_UNIT))) {
            debug("msr_read: %s", state_msg);
            return;
        }
        // Get the multipliers
        pu_sock[i].mult_power  = pow(0.5, (double) getbits64(reg, 3, 0));
        pu_sock[i].mult_energy = pow(0.5, (double) getbits64(reg, 12, 8));
        pu_sock[i].mult_time   = pow(0.5, (double) getbits64(reg, 19, 16));
        debug("pu_sock[%d].mult_power : %0.3lf", i, pu_sock[i].mult_power);
        debug("pu_sock[%d].mult_energy: %0.3lf", i, pu_sock[i].mult_energy);
        debug("pu_sock[%d].mult_time  : %0.3lf", i, pu_sock[i].mult_time);
    }
    // MSR_PKG_POWER_INFO & MSR_DRAM_POWER_INFO
    for (i = 0; i < tp_sockets.cpu_count; ++i) {
        // MSR_PKG_POWER_INFO
        if ((s = msr_read(tp_sockets.cpus[i].id, &reg, sizeof(ullong), MSR_PKG_POWER_INFO))) {
            return s;
        }
        pi_sock[i].tdp       = (uint) (((double) getbits64(reg, 14,  0)) * pu_sock[i].mult_power);
        pi_sock[i].min_power = (uint) (((double) getbits64(reg, 30, 16)) * pu_sock[i].mult_power);
        pi_sock[i].max_power = (uint) (((double) getbits64(reg, 46, 32)) * pu_sock[i].mult_power);
        pi_sock[i].max_timew = (uint) (((double) getbits64(reg, 53, 48)) * pu_sock[i].mult_time);
        debug("pi_sock[%d].tdp      : %u W" , i, pi_sock[i].tdp);
        debug("pi_sock[%d].max_power: %u W" , i, pi_sock[i].min_power);
        debug("pi_sock[%d].min_power: %u W" , i, pi_sock[i].max_power);
        debug("pi_sock[%d].max_timew: %u mS", i, pi_sock[i].max_timew);
        // MSR_DRAM_POWER_INFO
        if ((s = msr_read(tp_sockets.cpus[i].id, &reg, sizeof(ullong), MSR_PKG_POWER_INFO))) {
            return s;
        }
        pi_dram[i].tdp       = (uint) (((double) getbits64(reg, 14,  0)) * pu_sock[i].mult_power);
        pi_dram[i].min_power = (uint) (((double) getbits64(reg, 30, 16)) * pu_sock[i].mult_power);
        pi_dram[i].max_power = (uint) (((double) getbits64(reg, 46, 32)) * pu_sock[i].mult_power);
        pi_dram[i].max_timew = (uint) (((double) getbits64(reg, 53, 48)) * pu_sock[i].mult_time);
        debug("pi_dram[%d].tdp      : %u W" , i, pi_dram[i].tdp);
        debug("pi_dram[%d].max_power: %u W" , i, pi_dram[i].min_power);
        debug("pi_dram[%d].min_power: %u W" , i, pi_dram[i].max_power);
        debug("pi_dram[%d].max_timew: %u us", i, pi_dram[i].max_timew);
    }
    // MSR_PKG_POWER_LIMIT & MSR_DRAM_POWER_LIMIT
    for (i = 0; i < tp_sockets.cpu_count; ++i) {
        // MSR_PKG_POWER_LIMIT
        if ((s = msr_read(tp_sockets.cpus[i].id, &pl_sock[i].reg, sizeof(ullong), MSR_PKG_POWER_LIMIT))) {
            return s;
        }
        // Keeper
        sprintf(pl_sock[i].keep_name, "MsrPkgPowerLimit%d", i);
        if (!(pl_sock[i].keep_load = keeper_load_uint64(pl_sock[i].keep_name, &pl_sock[i].reg))) {
            keeper_save_uint64(pl_sock[i].keep_name, pl_sock[i].reg);
        }
        pl_sock[i].pl1_power  = (uint) getbits64(pl_sock[i].reg, 14,  0);
        pl_sock[i].pl1_enable = (uint) getbits64(pl_sock[i].reg, 15, 15);
        pl_sock[i].pl1_clamp  = (uint) getbits64(pl_sock[i].reg, 16, 16);
        pl_sock[i].pl1_timew  = (uint) getbits64(pl_sock[i].reg, 23, 17);
        pl_sock[i].pl2_power  = (uint) getbits64(pl_sock[i].reg, 46, 32);
        pl_sock[i].pl2_enable = (uint) getbits64(pl_sock[i].reg, 47, 47);
        pl_sock[i].pl2_clamp  = (uint) getbits64(pl_sock[i].reg, 48, 48);
        pl_sock[i].pl2_timew  = (uint) getbits64(pl_sock[i].reg, 55, 49);
        pl_sock[i].lock       = (uint) getbits64(pl_sock[i].reg, 63, 63);
        debug("pl_sock[%d].reg: %llu, %u lock, %u loaded from keeper", i,
              pl_sock[i].reg, pl_sock[i].lock, pl_sock[i].keep_load);
        debug("pl_sock[%d].pl1: %u power, %u enable, %u clamp, %u timew", i,
              pl_sock[i].pl1_power, pl_sock[i].pl1_enable, pl_sock[i].pl1_clamp, pl_sock[i].pl1_timew);
        debug("pl_sock[%d].pl2: %u power, %u enable, %u clamp, %u timew", i,
              pl_sock[i].pl2_power, pl_sock[i].pl2_enable, pl_sock[i].pl2_clamp, pl_sock[i].pl2_timew);
        // MSR_DRAM_POWER_LIMIT (later, first better test PCK)
        if ((s = msr_read(tp_sockets.cpus[i].id, &pl_dram[i].reg, sizeof(ullong), MSR_DRAM_POWER_LIMIT))) {
            return s;
        }
        // PP0 POWER LIMIT is not included because we don't have MSR_PP0_POWER_INFO
    }
    // Keeper for MSR_*_POWER_LIMIT
    apis_put(ops->get_api      , mgt_cpupow_intel63_get_api);
    apis_put(ops->count_devices, mgt_cpupow_intel63_count_devices);
    apis_put(ops->get_powercap , mgt_cpupow_intel63_get_powercap);
    apis_put(ops->set_powercap , mgt_cpupow_intel63_set_powercap);
    apis_put(ops->get_tdp      , mgt_cpupow_intel63_get_tdp);
}

void mgt_cpupow_intel63_get_api(uint *api)
{
    *api = API_INTEL63;
}

state_t mgt_cpupow_intel63_count_devices(int type, uint *devs_count)
{
    return EAR_SUCCESS;
}

state_t mgt_cpupow_intel63_get_powercap(int type, uint *watts)
{
    return EAR_SUCCESS;
}

state_t mgt_cpupow_intel63_set_powercap(int type, uint *watts)
{
    ullong reg;
    #if 0
    for (i = 0; i < tp_sockets.cpu_count; ++i) {
        msr_write(tp_sockets.cpus[i].id, reg, sizeof(ullong), MSR_PKG_POWER_LIMIT);
    }
    #endif
    return EAR_SUCCESS;
}

state_t mgt_cpupow_intel63_get_tdp(uint *watts)
{
    state_t s;
    int i;
    // Cleaning
    memset(watts, 0, sizeof(uint)*tp_sockets.cpu_count);
    // Iterating over sockets
    for (i = 0; i < tp_sockets.cpu_count; ++i) {
        watts[i] = pi_sock[i].tdp;
        debug("read tdp: %u", watts[i]);
    }
    return EAR_SUCCESS;
}
