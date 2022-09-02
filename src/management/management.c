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

#include <stdlib.h>
#include <management/management.h>
#include <common/output/verbose.h>
#include <common/utils/serial_buffer.h>
#include <daemon/local_api/eard_api_rpc.h>

#define sa(f) \
    if (state_fail(f)) { \
        serror(#f " FAILED"); \
        exit(0); \
    }
#define sn(f) \
    f;

static char           cbuffer[16384];
static uint          *ibuffer = (uint *) cbuffer;
//static ullong        *lbuffer = (ullong *) cbuffer;
static wide_buffer_t  sbuffer;
static manages_t      m;

void management_init(topology_t *tp, int eard)
{
	sa(mgt_imcfreq_load(tp, eard, NULL));
	sa(mgt_cpufreq_load(tp, eard));
	sa(mgt_imcfreq_init(no_ctx));
	sa(mgt_cpufreq_init(no_ctx));
    
	sa(mgt_imcfreq_get_api(&m.imcfreq.api));
    sa(mgt_cpufreq_get_api(&m.cpufreq.api));
    sn(mgt_cpuprio_get_api(&m.cpuprio.api));
	
    sa(mgt_cpufreq_count_devices(no_ctx, &m.cpufreq.devs_count));
	sa(mgt_imcfreq_count_devices(no_ctx, &m.imcfreq.devs_count));

	sa(mgt_cpufreq_get_available_list(no_ctx, (const pstate_t **) &m.cpufreq.list1, &m.cpufreq.list1_count));
	sa(mgt_imcfreq_get_available_list(no_ctx, (const pstate_t **) &m.imcfreq.list1, &m.imcfreq.list1_count));
    
    //sn(mgt_imcfreq_data_count(&m.cpuprio.list1_count, &m.cpuprio.list2_count));
    //sn(mgt_cpufreq_data_count(&m.cpuprio.list1_count, &m.cpuprio.list2_count));
    sn(mgt_cpuprio_data_count(&m.cpuprio.list1_count, &m.cpuprio.list2_count));
	
    sa(mgt_cpufreq_data_alloc((pstate_t **)  &m.cpufreq.list2, (uint **) &m.cpufreq.list3));
	sa(mgt_cpufreq_data_alloc((pstate_t **)  &m.imcfreq.list2, (uint **) &m.imcfreq.list3));
    sn(mgt_cpuprio_data_alloc((cpuprio_t **) &m.cpuprio.list1, (uint **) &m.cpuprio.list2));

    // Serialization buffer
    serial_alloc(&sbuffer, SIZE_8KB);
}

void management_dispose()
{
	sa(mgt_imcfreq_dispose(no_ctx));
	sa(mgt_cpufreq_dispose(no_ctx));
}

void management_print_apis()
{
	apis_print(m.imcfreq.api, "MGT_IMCFREQ: ");
	apis_print(m.cpufreq.api, "MGT_CPUFREQ: ");
	apis_print(m.cpuprio.api, "MGT_CPUPRIO: ");
}

int management_rpcs(uint rpc_id, size_t rpc_size, int fd_req, int fd_ack)
{
    state_t  ret_s    = EAR_SUCCESS;
    char    *ret_data = NULL;    
    size_t   ret_size = 0;

    switch(rpc_id) {
    case RPC_MGT_PRIO_GET_STATIC_VALUES:
        serial_clean(&sbuffer);
        serial_add_elem(&sbuffer, (char *) &m.cpuprio.api        , sizeof(uint));
        serial_add_elem(&sbuffer, (char *) &m.cpuprio.list1_count, sizeof(uint));
        serial_add_elem(&sbuffer, (char *) &m.cpuprio.list2_count, sizeof(uint));
        ret_size = serial_size(&sbuffer);
        ret_data = serial_data(&sbuffer);
    break;
    case RPC_MGT_PRIO_ENABLE:
        ret_s = mgt_cpuprio_enable();
    break;
    case RPC_MGT_PRIO_DISABLE:
        ret_s = mgt_cpuprio_disable();
    break;
    case RPC_MGT_PRIO_IS_ENABLED:
        ibuffer[0] = mgt_cpuprio_is_enabled();
        ret_data = (char *) ibuffer;
        ret_size = sizeof(int);
    break;
    case RPC_MGT_PRIO_GET_AVAILABLE:
        ret_s    = mgt_cpuprio_get_available_list((cpuprio_t *) m.cpuprio.list1);
        ret_data = (char *) m.cpuprio.list1;
        ret_size = sizeof(cpuprio_t) * m.cpuprio.list1_count;
    break;
    case RPC_MGT_PRIO_SET_AVAILABLE:
        if (state_fail(ret_s = eard_rpc_read_pending(fd_req, m.cpuprio.list1, rpc_size, 0))) {
            serror(Rpcerr.pending);
        } else {
            ret_s = mgt_cpuprio_set_available_list(m.cpuprio.list1);
        }
    break;
    case RPC_MGT_PRIO_GET_CURRENT:
        ret_s = mgt_cpuprio_get_available_list(m.cpuprio.list2);
    break;
    case RPC_MGT_PRIO_SET_CURRENT_LIST:
        if (state_fail(ret_s = eard_rpc_read_pending(fd_req, m.cpuprio.list2, rpc_size, 0))) {
            serror(Rpcerr.pending);
        } else {
            ret_s = mgt_cpuprio_set_current_list(m.cpuprio.list2);
        }
    break;
    case RPC_MGT_PRIO_SET_CURRENT:
        if (state_fail(ret_s = eard_rpc_read_pending(fd_req, cbuffer, rpc_size, 0))) {
            serror(Rpcerr.pending);
        } else {
            ret_s = mgt_cpuprio_set_current(ibuffer[0], (int) ibuffer[1]);
        }
    break;
    default:
        return 0;
    }

    if (state_fail(eard_rpc_answer(fd_ack, rpc_id, ret_s, ret_data, ret_size, state_msg))) {
        return 0;
    }
    return 1;
}
