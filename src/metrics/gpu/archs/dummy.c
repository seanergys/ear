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
#include <metrics/gpu/archs/dummy.h>


void gpu_dummy_load(gpu_ops_t *ops)
{
    apis_put(ops->get_api,       gpu_dummy_get_api);
    apis_put(ops->init[0],       gpu_dummy_init);
    apis_put(ops->dispose[0],    gpu_dummy_dispose);
//    apis_put(ops->get_devices,   gpu_dummy_get_devices);
    apis_put(ops->count_devices, gpu_dummy_count_devices);
    apis_put(ops->read[0],       gpu_dummy_read);
    apis_put(ops->read_raw,      gpu_dummy_read_raw);
}

void gpu_dummy_get_api(uint *api)
{
    *api = API_DUMMY;
}

state_t gpu_dummy_init(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t gpu_dummy_dispose(ctx_t *c)
{
	return EAR_SUCCESS;
}

//state_t gpu_get_devices(ctx_t *c, const gpu_devs_t **devs, uint *devs_count)
//{
//    static gpu_devs_t dev;
//    if (devs != NULL) {
//        *devs = dev;
//    }
//    return gpu_dummy_count(c, devs_count);
//}

state_t gpu_dummy_count(ctx_t *c, uint *dev_count_in)
{
	if (dev_count_in != NULL) {
		*dev_count_in = 1;
	}
	return EAR_SUCCESS;
}

state_t gpu_dummy_read(ctx_t *c, gpu_t *data)
{
	return gpu_data_null(data);
}

state_t gpu_dummy_read_raw(ctx_t *c, gpu_t *data)
{
	return gpu_data_null(data);
}
