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

state_t gpu_eard_status(int eard)
{
	uint eard_api;
	state_t s;

	if (!eard) {
		return_msg(EAR_ERROR, "EARD (daemon) not required");
	}
	if (!eards_connected()) {
		return_msg(EAR_ERROR, "EARD (daemon) not connected");
	}
	if (xtate_fail(s, eard_rpc(RPC_MET_GPU_GET_API, NULL, 0, (char *) &eard_api, sizeof(uint)))) {
		return s;
	}
	if (eard_api == API_NONE || eard_api == API_DUMMY) {
		return_msg(EAR_ERROR, "EARD (daemon) has loaded DUMMY/NONE API");
	}
	return EAR_SUCCESS;
}

state_t gpu_eard_init(ctx_t *c)
{
	state_t s;
	c->context = NULL;
	if (state_fail(s = eard_rpc(RPC_MET_GPU_COUNT_DEVICES, NULL, 0, (char *) &c->context, sizeof(uint)))) {
		return s;
	}
	if (((ulong) c->context) == 0) {
		return_msg(EAR_ERROR, "There are no GPUs");
	}
	return EAR_SUCCESS;
}

state_t gpu_eard_dispose(ctx_t *c)
{
	c->context = NULL;
	return EAR_SUCCESS;
}

state_t gpu_eard_count(ctx_t *c, uint *dev_count_in)
{
	ulong aux = (ulong) c->context;
	*dev_count_in = (uint) aux;
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
