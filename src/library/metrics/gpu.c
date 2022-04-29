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

//#define SHOW_DEBUGS 1
#include <common/output/verbose.h>
#include <common/output/debug.h>
#include <library/metrics/gpu.h>
#include <library/common/externs.h>
#include <library/common/verbose_lib.h>
#include <daemon/local_api/eard_api.h>

static uint	dev_count;
static gpu_t *node_gpu_data;
static ulong *node_gpu_freqs = NULL;

state_t gpu_lib_load(settings_conf_t *settings)
{
	mgt_gpu_load(EARD);
	    gpu_load(EARD);

	return EAR_SUCCESS;
}

state_t gpu_lib_init(ctx_t *_c)
{
    mgt_gpu_init(no_ctx);
        gpu_init(no_ctx);
    // Counting devices
    gpu_count_devices(no_ctx, &dev_count);
	// Allocation
	mgt_gpu_data_alloc(&node_gpu_freqs);
	    gpu_data_alloc(&node_gpu_data);
	
    return EAR_SUCCESS;
}

state_t gpu_lib_model(ctx_t *_c,uint *model)
{
    gpu_get_api(model);
	return EAR_SUCCESS;
}

state_t gpu_lib_dispose(ctx_t *_c)
{
	mgt_gpu_dispose(no_ctx);
        gpu_dispose(no_ctx);
    return EAR_SUCCESS;
}

state_t gpu_lib_count(uint *_dev_count)
{
	*_dev_count = dev_count;
	return EAR_SUCCESS;
}

/*
 *
 * Metrics
 *
 */
state_t gpu_lib_read(ctx_t *c, gpu_t *data)
{
	state_t ret = gpu_read(no_ctx, node_gpu_data);
	memcpy(data, node_gpu_data, dev_count*sizeof(gpu_t));
	return ret;
}

state_t gpu_lib_read_raw(ctx_t *_c, gpu_t *data)
{
    return gpu_read_raw(no_ctx, data);
}

state_t gpu_lib_read_copy(gpu_t *data2, gpu_t *data1, gpu_t *data_diff)
{
    return gpu_read_copy(no_ctx, data2, data1, data_diff);
}

state_t gpu_lib_data_diff(gpu_t *data2, gpu_t *data1, gpu_t *data_diff)
{
    return gpu_data_diff(data2, data1, data_diff);
}

state_t gpu_lib_data_merge(gpu_t *data_diff, gpu_t *data_merge)
{
    return gpu_data_merge(data_diff, data_merge);
}

state_t gpu_lib_data_alloc(gpu_t **data)
{
	return gpu_data_alloc(data);
}

state_t gpu_lib_data_free(gpu_t **data)
{
	return gpu_data_free(data);
}

state_t gpu_lib_data_null(gpu_t *data)
{
	return gpu_data_null(data);
}

state_t gpu_lib_data_copy(gpu_t *data_dst, gpu_t *data_src)
{
	return gpu_data_copy(data_dst, data_src);
}

state_t gpu_lib_data_print(gpu_t *data, int fd)
{
	gpu_data_print(data, fd);
    return EAR_SUCCESS;
}

state_t gpu_lib_data_tostr(gpu_t *data, char *buffer, int length)
{
	gpu_data_tostr(data, buffer, length);
    return EAR_SUCCESS;
}

/*
 *
 * Management
 *
 */
state_t gpu_lib_alloc_array(ulong **array)
{
	return mgt_gpu_data_alloc(array);
}

state_t gpu_lib_freq_limit_get_current(ctx_t *_c, ulong *khz)
{
	return mgt_gpu_freq_limit_get_current(no_ctx, khz);
}

state_t gpu_lib_freq_limit_get_default(ulong *khz)
{
	return mgt_gpu_freq_limit_get_default(no_ctx, khz);
}

state_t gpu_lib_freq_limit_get_max(ulong *khz)
{
	return mgt_gpu_freq_limit_get_max(no_ctx, khz);
}

state_t gpu_lib_freq_limit_reset()
{
    return mgt_gpu_freq_limit_reset(no_ctx);
}

state_t gpu_lib_freq_limit_set(ulong *khz)
{
    return mgt_gpu_freq_limit_set(no_ctx, khz);
}

state_t gpu_lib_freq_list(const ulong ***list_khz, const uint **list_len)
{
	return mgt_gpu_freq_list(no_ctx, list_khz, list_len);
}

state_t gpu_lib_power_cap_get_current(ctx_t *_c, ulong *watts)
{
	return mgt_gpu_power_cap_get_current(no_ctx, watts);
}

state_t gpu_lib_power_cap_get_default(ulong *watts)
{
	return mgt_gpu_power_cap_get_default(no_ctx, watts);
}

state_t gpu_lib_power_cap_get_rank(ulong *watts_min, ulong *watts_max)
{
	return mgt_gpu_power_cap_get_rank(no_ctx, watts_min, watts_max);
}

state_t gpu_lib_power_cap_reset()
{
    return mgt_gpu_power_cap_reset(no_ctx);
}

state_t gpu_lib_power_cap_set(ulong *watts)
{
    return mgt_gpu_power_cap_set(no_ctx, watts);
}


