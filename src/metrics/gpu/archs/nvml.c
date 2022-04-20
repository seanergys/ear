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

#include <dlfcn.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <common/types.h>
#include <common/output/debug.h>
#include <common/system/monitor.h>
#include <common/system/symplug.h>
#include <common/config/config_env.h>
#include <metrics/common/nvml.h>
#include <metrics/gpu/archs/nvml.h>

//static nvml_t nvml;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static nvml_t         nvml;
static nvmlDevice_t  *devs;
static suscription_t *sus;
static uint           devs_count;
static gpu_t         *pool;

void gpu_nvml_load(gpu_ops_t *ops)
{
    // The locking and duplicate control is at higher level
    if (state_fail(nvml_open(&nvml))) {
        debug("nvml_open failed: %s", state_msg);
        return;
    }
    if (state_fail(nvml_get_devices(&devs, &devs_count))) {
        debug("nvml_get_devices failed: %s", state_msg);
        return;
    }
    apis_set(ops->get_api,       gpu_nvml_get_api);
    apis_set(ops->init,          gpu_nvml_init);
    apis_set(ops->dispose,       gpu_nvml_dispose);
    apis_set(ops->count_devices, gpu_nvml_count_devices);
    apis_set(ops->read,          gpu_nvml_read);
    apis_set(ops->read_raw,      gpu_nvml_read_raw);
}

static state_t static_init()
{
	state_t s = EAR_SUCCESS;
	// Allocation
	pool = calloc(devs_count, sizeof(gpu_t));
	// Initializing monitoring thread suscription.
	sus = suscription();
	sus->call_main  = gpu_nvml_pool;
	sus->time_relax = 2000;
	sus->time_burst = 1000;
	// Initializing monitoring thread.
	if (state_fail(s = sus->suscribe(sus))) {
		debug("GPU monitor FAILS");
		monitor_unregister(sus);
		free(pool);
	}
	return s;
}

void gpu_nvml_get_api(uint *api)
{
    *api = API_NVML;
}

state_t gpu_nvml_init(ctx_t *c)
{
    return static_init();
}

state_t gpu_nvml_dispose(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t gpu_nvml_count_devices(ctx_t *c, uint *devs_count_in)
{
	if (devs_count_in != NULL) {
		*devs_count_in = devs_count;
	}
	return EAR_SUCCESS;
}

static int static_read(int i, gpu_t *metric)
{
	nvmlEnableState_t mode;
	int s;

	// Cleaning
	memset(metric, 0, sizeof(gpu_t));
	// Testing if all is right
	if ((s = nvml.GetPowerMode(devs[i], &mode)) != NVML_SUCCESS) {
		return 0;
	}
	if (mode != NVML_FEATURE_ENABLED) {
		return 0;
	}
	nvmlProcessInfo_t procs[8];
	nvmlUtilization_t util;
	uint proc_count = 8;
	uint freq_gpu_mhz;
	uint freq_mem_mhz;
	uint temp_gpu;
	uint power_mw;
	// Getting the metrics by calling NVML (no MEM temp)
	s = nvml.GetPowerUsage(devs[i], &power_mw);
	s = nvml.GetClocks(devs[i], NVML_CLOCK_MEM, &freq_mem_mhz);
	s = nvml.GetClocks(devs[i], NVML_CLOCK_SM , &freq_gpu_mhz);
	s = nvml.GetTemp(devs[i], NVML_TEMPERATURE_GPU, &temp_gpu);
	s = nvml.GetUtil(devs[i], &util);
	s = nvml.GetProcs(devs[i], &proc_count, procs);
	// Pooling the data (time is not set here)
	metric->samples  = 1;
	metric->freq_mem = (ulong) freq_mem_mhz * 1000LU;
	metric->freq_gpu = (ulong) freq_gpu_mhz * 1000LU;
	metric->util_mem = (ulong) util.memory;
	metric->util_gpu = (ulong) util.gpu;
	metric->temp_gpu = (ulong) temp_gpu;
	metric->temp_mem = 0;
	metric->energy_j = 0;
	metric->power_w  = (double) power_mw;
	metric->working  = proc_count > 0;
	metric->correct  = 1;

	return 1;
}

state_t gpu_nvml_pool(void *p)
{
	timestamp_t time;
	gpu_t metric;
	int working;
	int i;

	//
	working = 0;
	// Lock
	while (pthread_mutex_trylock(&lock));
	timestamp_getfast(&time);
	//
	for (i = 0; i < devs_count; ++i) {
		if (!static_read(i, &metric)) {
			continue;
		}
		// Pooling the data
		pool[i].time      = time;
		pool[i].samples  += metric.samples;
		pool[i].freq_mem += metric.freq_mem;
		pool[i].freq_gpu += metric.freq_gpu;
		pool[i].util_mem += metric.util_mem;
		pool[i].util_gpu += metric.util_gpu;
		pool[i].temp_gpu += metric.temp_gpu;
		pool[i].temp_mem += metric.temp_mem;
		pool[i].energy_j  = metric.energy_j;
		pool[i].power_w  += metric.power_w;
		pool[i].working   = metric.working;
		pool[i].correct   = metric.correct;
		// Burst or not
		working += pool[i].working;
	}
	// Lock
	pthread_mutex_unlock(&lock);
		
	if (working  > 0 && !monitor_is_bursting(sus)) {
		debug("bursting");
		monitor_burst(sus);
	}
	if (working == 0 &&  monitor_is_bursting(sus)) {
		debug("relaxing");
		monitor_relax(sus);
	}

	return EAR_SUCCESS;
}

state_t gpu_nvml_read(ctx_t *c, gpu_t *data)
{
	while (pthread_mutex_trylock(&lock));
	memcpy(data, pool, devs_count * sizeof(gpu_t));
	pthread_mutex_unlock(&lock);
	return EAR_SUCCESS;
}

state_t gpu_nvml_read_raw(ctx_t *c, gpu_t *data)
{
	timestamp_t time;
	int i;
	timestamp_getfast(&time);
	for (i = 0; i < devs_count; ++i) {
		static_read(i, &data[i]);
		data[i].time     = time;
		data[i].power_w /= 1000;
	}
	return EAR_SUCCESS;
}
