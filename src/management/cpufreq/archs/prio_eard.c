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
* EAR is an open source software, and it is licensed under both the BSD-3 license
* and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
* and COPYING.EPL files.
*/

#include <management/cpufreq/archs/eard.h>

void mgt_prio_eard_load(topology_t *tp, mgt_prio_ops_t *ops, int eard)
{
}

void mgt_prio_eard_get_api(uint *api)
{
}

state_t mgt_prio_eard_init()
{
    return EAR_SUCCESS;
}

state_t mgt_prio_eard_dispose()
{
    return EAR_SUCCESS;
}

state_t mgt_prio_eard_enable()
{
    return EAR_SUCCESS;
}

state_t mgt_prio_eard_disable()
{
    return EAR_SUCCESS;
}

int mgt_prio_eard_is_enabled()
{
    return 0;
}

state_t mgt_prio_eard_get_available_list(cpuprio_t *prio_list)
{
    return EAR_SUCCESS;
}

state_t mgt_prio_eard_set_available_list(cpuprio_t *prio_list)
{
    return EAR_SUCCESS;
}

state_t mgt_prio_eard_get_current_list(uint *idx_list)
{
    return EAR_SUCCESS;
}

state_t mgt_prio_eard_set_current_list(uint *idx_list)
{
    return EAR_SUCCESS;
}

state_t mgt_prio_eard_set_current(uint idx, int cpu)
{
    return EAR_SUCCESS;
}

void mgt_prio_eard_data_count(uint *prios_count, uint *idxs_count)
{
}
