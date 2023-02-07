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

#include <stdlib.h>
#include <common/output/debug.h>
#include <metrics/common/apis.h>
#include <management/cpupow/cpupow.h>
#include <management/cpupow/archs/dummy.h>

void mgt_cpupow_dummy_load(topology_t *tp_in, mgt_cpupow_ops_t *ops)
{
    apis_put(ops->get_api            , mgt_cpupow_dummy_get_api);
    apis_put(ops->count_devices      , mgt_cpupow_dummy_count_devices);
    apis_put(ops->is_powercap_capable, mgt_cpupow_dummy_is_powercap_capable);
    apis_put(ops->powercap_is_enabled, mgt_cpupow_dummy_powercap_is_enabled);
    apis_put(ops->powercap_get       , mgt_cpupow_dummy_powercap_get);
    apis_put(ops->powercap_set       , mgt_cpupow_dummy_powercap_set);
    apis_put(ops->powercap_reset     , mgt_cpupow_dummy_powercap_reset);
    apis_put(ops->tdp_get            , mgt_cpupow_dummy_tdp_get);
}

void mgt_cpupow_dummy_get_api(uint *api)
{
    *api = API_DUMMY;
}

int mgt_cpupow_dummy_count_devices(int domain)
{
    return 1;
}

int mgt_cpupow_dummy_is_powercap_capable(int domain)
{
    return 0;
}

state_t mgt_cpupow_dummy_powercap_is_enabled(int domain, uint *enabled)
{
    *enabled = 0;
    return EAR_SUCCESS;
}

state_t mgt_cpupow_dummy_powercap_get(int domain, uint *watts)
{
    return EAR_SUCCESS;
}

state_t mgt_cpupow_dummy_powercap_set(int domain, uint *watts)
{
    return EAR_SUCCESS;
}

state_t mgt_cpupow_dummy_powercap_reset(int domain)
{
    return EAR_SUCCESS;
}

state_t mgt_cpupow_dummy_tdp_get(int domain, uint *watts)
{
    return EAR_SUCCESS;
}