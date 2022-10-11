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
#define FAKE_GPUS   0

#include <stdlib.h>
#include <common/output/debug.h>
#include <common/utils/serial_buffer.h>
#include <daemon/local_api/eard_api_rpc.h>
#include <metrics/gpu/archs/eard.h>

static uint        root_devs_count;
static uint        user_devs_count;
static gpu_devs_t *root_devs;
static gpu_devs_t *user_devs;

void gpu_eard_load(gpu_ops_t *ops, int eard)
{
    wide_buffer_t b;
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
    debug("EARD (daemon) is connected");
	if (state_fail(s = eard_rpc(RPC_MET_GPU_GET_API, NULL, 0, (char *) &eard_api, sizeof(uint)))) {
		debug("Received: %s", state_msg)
        return;
	}
	if (eard_api == API_NONE || eard_api == API_DUMMY) {
		debug("EARD (daemon) has loaded DUMMY/NONE API");
        return;
	}
    // Get a list of devices in root space
    if (state_fail(s = eard_rpc_buffered(RPC_MGT_GPU_GET_DEVICES, NULL, 0, (char **) &b, NULL))) {
        return;
    }
    serial_copy_elem(&b, (char *) &root_devs_count, NULL);
    debug("Received from EARD %u GPUs", root_devs_count);
    if (root_devs_count == 0) {
        debug("There are no GPUs");
        return;
    }
    root_devs = (gpu_devs_t *) serial_copy_elem(&b, NULL, NULL);
    user_devs_count = root_devs_count;
    // Get a list of devices in user space
    if (ops->get_devices != NULL) {
        ops->get_devices(NULL, &user_devs, &user_devs_count);
    } else {
        // If not, just copy root devices
        user_devs = calloc(user_devs_count, sizeof(gpu_devs_t));
        memcpy(user_devs, root_devs, user_devs_count*sizeof(gpu_devs_t));
    }
    #if FAKE_GPUS
    user_devs_count = 1;
    user_devs[0].serial = root_devs[0].serial;
    //user_devs[1].serial = root_devs[3].serial;
    user_devs[0].index = root_devs[0].index;
    //user_devs[1].index = root_devs[3].index;
    #endif
    #if SHOW_DEBUGS
    int i;

    for (i = 0; i < root_devs_count; ++i) {
        debug("root D%d: %d_%llu", i, root_devs[i].index, root_devs[i].serial);
    }
    for (i = 0; i < user_devs_count; ++i) {
        debug("user D%d: %d_%llu", i, user_devs[i].index, user_devs[i].serial);
    }
    #endif
    // If something already loaded
    if (apis_loaded(ops)){
        return;
    }
    apis_put(ops->init,          gpu_eard_init);
    apis_put(ops->get_api,       gpu_eard_get_api);
    apis_put(ops->dispose,       gpu_eard_dispose);
    apis_put(ops->count_devices, gpu_eard_count_devices);
    apis_set(ops->read,          gpu_eard_read);
    apis_set(ops->read_raw,      gpu_eard_read_raw);
}

void gpu_eard_get_api(uint *api)
{
    *api = API_EARD;
}

state_t gpu_eard_init(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t gpu_eard_dispose(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t gpu_eard_count_devices(ctx_t *c, uint *devs_count_in)
{
	*devs_count_in = (uint) user_devs_count;
	return EAR_SUCCESS;
}

static state_t static_read(uint call, gpu_t *data)
{
	size_t size = ((size_t) root_devs_count)*sizeof(gpu_t);
    gpu_t root_data[128];
    int u, r, f;
    state_t s;
    
    memset((char *) root_data, 0, size); 
	if (state_fail(s = eard_rpc(call, NULL, 0, (char *) root_data, size))) {
        return s;
    }
    for (r = f = 0; r < root_devs_count; ++r, f = 0) {
        for (u = 0; u < user_devs_count; ++u) {
            if (user_devs[u].serial == root_devs[r].serial) {
                data[u] = root_data[r];
                f = 1;
            }
        }
        if (f) {
            debug("Received D%d", r);
        } else {
            debug("Received D%d (ignored)", r);
        }
    }
    return EAR_SUCCESS;
}

state_t gpu_eard_read(ctx_t *c, gpu_t *data)
{
    return static_read(RPC_MET_GPU_GET_METRICS, data);
}

state_t gpu_eard_read_raw(ctx_t *c, gpu_t *data)
{
    return static_read(RPC_MET_GPU_GET_METRICS_RAW, data);
}
