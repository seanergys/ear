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

CPUPOW_F_LOAD(dummy)
{
    apis_put(ops->get_api            , mgt_cpupow_dummy_get_api);
    apis_put(ops->count_devices      , mgt_cpupow_dummy_count_devices);
    apis_put(ops->powercap_is_capable, mgt_cpupow_dummy_powercap_is_capable);
    apis_put(ops->powercap_is_enabled, mgt_cpupow_dummy_powercap_is_enabled);
    apis_put(ops->powercap_get       , mgt_cpupow_dummy_powercap_get);
    apis_put(ops->powercap_set       , mgt_cpupow_dummy_powercap_set);
    apis_put(ops->powercap_reset     , mgt_cpupow_dummy_powercap_reset);
    apis_put(ops->tdp_get            , mgt_cpupow_dummy_tdp_get);
}

CPUPOW_F_GET_INFO(dummy)
{
    info->api         = API_DUMMY;
    info->scope       = SCOPE_DUMMY;
    info->granularity = GRANULARITY_DUMMY;
    info->devs_count  = 1;
}

CPUPOW_F_COUNT_DEVICES(dummy)
{
    return 1;
}

CPUPOW_F_POWERCAP_IS_CAPABLE(dummy)
{
    return 0;
}

CPUPOW_F_POWERCAP_IS_ENABLED(dummy)
{
    *enabled = 0;
    return EAR_SUCCESS;
}

CPUPOW_F_POWERCAP_GET(dummy)
{
    return EAR_SUCCESS;
}

CPUPOW_F_POWERCAP_SET(dummy)
{
    return EAR_SUCCESS;
}

CPUPOW_F_POWERCAP_RESET(dummy)
{
    return EAR_SUCCESS;
}

CPUPOW_F_TDP_GET(dummy)
{
    return EAR_SUCCESS;
}