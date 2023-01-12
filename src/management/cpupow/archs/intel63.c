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

//#define SHOW_DEBUGS 1

#include <common/output/debug.h>
#include <metrics/common/msr.h>
#include <management/cpupow/cpupow.h>

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

static topology_t tp_cores;
static topology_t tp_sockets;
static double mult_power;
static double mult_energy;
static double mult_time;

void mgt_cpupow_intel63_load(topology_t *tp_in)
{
    ullong reg = 0;

    if (state_fail(msr_test(tp_in, MSR_WR))) {
        return;
    }
    if (tp_in->vendor == VENDOR_INTEL && tp_in->model >= MODEL_HASWELL_X) {
    } else if (tp_in->vendor == VENDOR_AMD && tp_in->family >= FAMILY_ZEN) {
    } else {
        // API incompatible
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
    if (state_fail(msr_read(tp_sockets.cpus[0].id, &reg, sizeof(ullong), MSR_RAPL_POWER_UNIT))) {
        debug("msr_read: %s", state_msg);
        return;
    }
    // Get the multipliers
    mult_power  = pow(0.5, (double) getbits64(reg,  3,  0));
    mult_energy = pow(0.5, (double) getbits64(reg, 12,  8));
    mult_time   = pow(0.5, (double) getbits64(reg, 19, 16));
    debug("multiplier power : %0.3lf", mult_power);
    debug("multiplier energy: %0.3lf", mult_energy);
    debug("multiplier time  : %0.3lf", mult_time);

    apis_put(ops->get_api          , mgt_cpupow_intel63_get_api);
    apis_put(ops->get_count_devices, mgt_cpupow_intel63_count_devices);
    apis_put(ops->get_powercap     , mgt_cpupow_intel63_get_powercap);
    apis_put(ops->set_powercap     , mgt_cpupow_intel63_set_powercap);
    apis_put(ops->get_tdp          , mgt_cpupow_intel63_get_tdp);
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
    return EAR_SUCCESS;
}

state_t mgt_cpupow_intel63_get_tdp(uint *watts)
{
    double dreg;
    ullong reg;
    state_t s;
    int i;

    // Cleaning
    memset(watts, 0, sizeof(uint)*tp_sockets.cpu_count);
    // Iterating over sockets
    for (i = 0; i < tp_sockets.cpu_count; ++i) {
        if ((s = msr_read(tp_sockets.cpus[i].id, &reg, sizeof(reg), MSR_PKG_POWER_INFO))) {
            return s;
        }
        dreg = ((double) getbits64(reg, 14, 0)) * mult_power;
        watts[i] = (uint) dreg;
    }
    return EAR_SUCCESS;
}