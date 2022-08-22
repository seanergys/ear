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

static uint prios_count;
static uint devs_count;
static uint eard_api

void mgt_prio_eard_load(topology_t *tp, mgt_prio_ops_t *ops, int eard)
{
    char *buffer;

    if (!eard) {
        return_msg(EAR_ERROR, "EARD (daemon) not required");
    }
    if (!eards_connected()) { 
        return_msg(EAR_ERROR, "EARD (daemon) not connected");
    }
    if (state_fail(s = eard_rpc_buffered(RPC_MGT_PRIO_GET_STATIC_VALUES, NULL, 0, &buffer, &size))) {
        return s;
    }
    serial_copy_elem((wide_buffer_t *) &buffer, (char *) &eard_api, NULL);
    serial_copy_elem((wide_buffer_t *) &buffer, (char *) &prios_count, NULL);
    serial_copy_elem((wide_buffer_t *) &buffer, (char *) &devs_count, NULL);
    
    if (eard_api == API_NONE || eard_api == API_DUMMY) {
        return_msg(EAR_ERROR, "EARD (daemon) has loaded DUMMY API");
    }
}

void mgt_prio_eard_get_api(uint *api)
{
    *api = API_EARD;
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
    return eard_rpc(RPC_PRIO_ENABLE, NULL, 0, NULL, 0);
}

state_t mgt_prio_eard_disable()
{
    return eard_rpc(RPC_PRIO_DISABLE, NULL, 0, NULL, 0);
}

int mgt_prio_eard_is_enabled()
{
    int enabled = 0;
    eard_rpc(RPC_PRIO_IS_ENABLED, &enabled, sizeof(int), NULL, 0);
    return enabled;
}

#define SIZE_PRIOS sizeof(cpuprio_t)*prios_count
#define SIZE_DEVS  sizeof(uint)*devs_count

state_t mgt_prio_eard_get_available_list(cpuprio_t *prio_list)
{
    memset((void *) prio_list, 0, SIZE_PRIOS);
    return eard_rpc(RPC_MGT_PRIO_GET_AVAILABLE, NULL, 0, (char *) prio_list, SIZE_PRIOS);
}

state_t mgt_prio_eard_set_available_list(cpuprio_t *prio_list)
{
    return eard_rpc(RPC_MGT_PRIO_SET_AVAILABLE, (char *) prio_list, SIZE_PRIOS, NULL, 0);
}

state_t mgt_prio_eard_get_current_list(uint *idx_list)
{
    memset((void *) idx_list, 0, SIZE_DEVS);
    return eard_rpc(RPC_MGT_PRIO_GET_CURRENT, NULL, 0, (char *) idx_list, SIZE_DEVS);
}

state_t mgt_prio_eard_set_current_list(uint *idx_list)
{
    return eard_rpc(RPC_MGT_PRIO_SET_CURRENT_LIST, (char *) idx_list, SIZE_DEVS, NULL, 0);
}

state_t mgt_prio_eard_set_current(uint idx, int cpu)
{
    struct data_s { uint idx; int cpu; } data = {.idx = idx, .cpu = cpu};
    return eard_rpc(RPC_MGT_PRIO_SET_CURRENT, (char *) data, sizeof(data), NULL, 0);
}

void mgt_prio_eard_data_count(uint *prios_count_in, uint *idxs_count_in)
{
    if (prios_count_in != NULL) {
        *prios_count_in = prios_count;       
    }
    if (idxs_count_in != NULL) {
        *idxs_count_in = devs_count;
    }
}
