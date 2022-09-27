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

// #define SHOW_DEBUGS 1

#include <stdlib.h>
#include <common/output/debug.h>
#include <metrics/gpu/archs/eard.h>
#include <daemon/local_api/eard_api_rpc.h>

static uint devs_count;

void gpu_eard_load(gpu_ops_t *ops, int eard)
{
	uint eard_api;
    #ifdef RPCS
	state_t s;
    #endif

	if (!eard) {
		debug("EARD (daemon) not required");
        return;
	}
	if (!eards_connected()) {
		debug("EARD (daemon) not connected");
        return;
	}
    debug("EARD (daemon) is connected");
    #ifdef RPCS
	if (state_fail(s = eard_rpc(RPC_MET_GPU_GET_API, NULL, 0, (char *) &eard_api, sizeof(uint)))) {
		debug("Received: %s", state_msg)
        return;
	}
    #else
    eards_gpu_model(&eard_api);
    #endif
	if (eard_api == API_NONE || eard_api == API_DUMMY) {
		debug("EARD (daemon) has loaded DUMMY/NONE API");
        return;
	}
    // If something already loaded
    if (apis_loaded(ops)){
        return;
    }
    apis_put(ops->get_api,       gpu_eard_get_api);
    apis_put(ops->init,          gpu_eard_init);
    apis_put(ops->dispose,       gpu_eard_dispose);
    apis_put(ops->count_devices, gpu_eard_count_devices);
    apis_put(ops->read,          gpu_eard_read);
    apis_put(ops->read_raw,      gpu_eard_read_raw);
}

void gpu_eard_get_api(uint *api)
{
    *api = API_EARD;
}

state_t gpu_eard_init(ctx_t *c)
{
    #ifdef RPCS
	state_t s;
	if (state_fail(s = eard_rpc(RPC_MET_GPU_COUNT_DEVICES, NULL, 0, (char *) &devs_count, sizeof(uint)))) {
		return s;
	}
	return EAR_SUCCESS;
    #else
    return (state_t) eards_gpu_dev_count(&devs_count);
    #endif
}

state_t gpu_eard_dispose(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t gpu_eard_count_devices(ctx_t *c, uint *devs_count_in)
{
	*devs_count_in = (uint) devs_count;
	return EAR_SUCCESS;
}

state_t gpu_eard_read(ctx_t *c, gpu_t *data)
{
    #ifdef RPCS
	size_t size = ((size_t) devs_count)*sizeof(gpu_t);
	return eard_rpc(RPC_MET_GPU_GET_METRICS, NULL, 0, (char *) data, size);
    #else
    return (state_t) eards_gpu_data_read(data, devs_count);
    #endif
}

state_t gpu_eard_read_raw(ctx_t *c, gpu_t *data)
{
	size_t size = ((size_t) devs_count)*sizeof(gpu_t);
    #ifdef RPCS
	return eard_rpc(RPC_MET_GPU_GET_METRICS_RAW, NULL, 0, (char *) data, size);
    #else
    memset(data, 0, size);
    return EAR_SUCCESS;
    #endif
}
