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
#include <management/cpupow/archs/amd17.h>

#define ReadSocketPower         0x04
#define WriteSocketPowerLimit   0x05
#define ReadSocketPowerLimit    0x06
#define ReadMaxSocketPowerLimit 0x07

static topology_t tp_cores;
static topology_t tp_sockets;

void mgt_cpupow_amd17_load(topology_t *tp_in, mgt_cpupow_ops_t *ops)
{
    int i;

    if (state_fail(msr_test(tp_in, MSR_WR))) {
        debug("failed msr test: %s", state_msg);
        return;
    }
    if (tp_in->vendor == VENDOR_AMD && tp_in->family >= FAMILY_ZEN) {
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
    // Opening HSMP
    if (state_fail(s = hsmp_scan(tp_in))) {
        serror("Failed during HSMP open");
        return;
    }
    apis_put(ops->get_api            , mgt_cpupow_amd17_get_api);
    apis_put(ops->count_devices      , mgt_cpupow_amd17_count_devices);
    apis_put(ops->is_powercap_capable, mgt_cpupow_amd17_is_powercap_capable);
    apis_put(ops->powercap_is_enabled, mgt_cpupow_amd17_powercap_is_enabled);
    apis_put(ops->powercap_get       , mgt_cpupow_amd17_powercap_get);
    apis_put(ops->powercap_set       , mgt_cpupow_amd17_powercap_set);
    apis_put(ops->powercap_reset     , mgt_cpupow_amd17_powercap_reset);
    apis_put(ops->tdp_get            , mgt_cpupow_amd17_tdp_get);
}

void mgt_cpupow_amd17_get_api(uint *api)
{
    *api = API_AMD17;
}

int mgt_cpupow_amd17_count_devices(int domain)
{
    return tp_sockets.cpu_count;
}

int mgt_cpupow_amd17_is_powercap_capable(int domain)
{
    // TODO: a method to get the capability of different domains
    return 1;
}

state_t mgt_cpupow_amd17_powercap_is_enabled(int domain, uint *enabled)
{
    return EAR_SUCCESS;
}

state_t mgt_cpupow_amd17_powercap_get(int domain, uint *watts)
{
    return EAR_SUCCESS;
}

state_t mgt_cpupow_amd17_powercap_set(int domain, uint *watts)
{
    return EAR_SUCCESS;
}

state_t mgt_cpupow_amd17_powercap_reset(int domain)
{
    return EAR_SUCCESS;
}

state_t mgt_cpupow_amd17_tdp_get(int domain, uint *watts)
{
    uint reps[2] = {  0, -1 };
    uint args[1] = { -1 };
    state_t s;
    int i;

    for (i = 0; i < tp_sockets.cpu_count; ++i) {
        if (state_fail(s = hsmp_send(tp.cpus[i].id, ReadMaxSocketPowerLimit, args, reps))) {
            return s;
        }
        // mW to W
        watts[i] = reps[0] / 1000;
    }
    return EAR_SUCCESS;
}