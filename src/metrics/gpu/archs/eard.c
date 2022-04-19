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

#include <stdlib.h>
#include <common/output/debug.h>
#include <metrics/gpu/archs/eard.h>
#include <daemon/local_api/eard_api_rpc.h>

static uint devs_count;

void gpu_eard_load(gpu_ops_t *ops, int eard)
{
	uint eard_api;
	state_t s;

	if (!eard) {
		debug("EARD (daemon) not required");
        return;
	}
	if (!eards_connected()) {
		debug("EARD (daemon) not connected");
        return;
	}
	if (state_fail(s = eard_rpc(RPC_MET_GPU_GET_API, NULL, 0, (char *) &eard_api, sizeof(uint)))) {
		debug("Received: %s", state_msg)
        return;
	}
	if (eard_api == API_NONE || eard_api == API_DUMMY) {
		debug("EARD (daemon) has loaded DUMMY/NONE API");
        return;
	}
    apis_put(ops->get_api,       gpu_eard_get_api);
    apis_put0(ops->init,          gpu_eard_init);
    apis_put0(ops->dispose,       gpu_eard_dispose);
    apis_put(ops->get_devices,   gpu_eard_get_devices);
    apis_put(ops->count_devices, gpu_eard_count_devices);
    apis_put0(ops->read,          gpu_eard_read);
    apis_put(ops->read_raw,      gpu_eard_read_raw);
}

void gpu_eard_get_api(uint *api)
{
    *api = API_EARD;
}

state_t gpu_eard_init(ctx_t *c)
{
	state_t s;
	if (state_fail(s = eard_rpc(RPC_MET_GPU_COUNT_DEVICES, NULL, 0, (char *) &devs_count, sizeof(uint)))) {
		return s;
	}
	return EAR_SUCCESS;
}

state_t gpu_eard_dispose(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t gpu_eard_get_devices(ctx_t *c, const gpu_devs_t **devs, uint *devs_count);
{
    if (devs != NULL) {
        *devs = dev;
    }
    return gpu_eard_count(c, devs_count);
}

state_t gpu_eard_count_devices(ctx_t *c, uint *devs_count_in)
{
	*devs_count_in = (uint) devs_count;
	return EAR_SUCCESS;
}

state_t gpu_eard_read(ctx_t *c, gpu_t *data)
{
	size_t size = (size_t) c->context * sizeof(gpu_t);
	return eard_rpc(RPC_MET_GPU_GET_METRICS, NULL, 0, (char *) data, size);
}

state_t gpu_eard_read_raw(ctx_t *c, gpu_t *data)
{
	size_t size = (size_t) c->context * sizeof(gpu_t);
	return eard_rpc(RPC_MET_GPU_GET_METRICS_RAW, NULL, 0, (char *) data, size);
}
