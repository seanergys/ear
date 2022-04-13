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
static suscription_t *sus;
static uint           dev_count;
static gpu_t         *pool;
static nvmlDevice_t  *devices;
static uint           ok_init;

static state_t static_init()
{
	state_t s = EAR_SUCCESS;
	// Allocation
	pool = calloc(dev_count, sizeof(gpu_t));
	// Initializing monitoring thread suscription.
	sus = suscription();
	sus->call_main  = nvml_pool;
	sus->time_relax = 1000;
	sus->time_burst = 300;
	// Initializing monitoring thread.
	if (state_fail(s = sus->suscribe(sus))) {
		debug("GPU monitor FAILS");
		monitor_unregister(sus);
		free(pool);
	}
	return s;
}

state_t nvml_init(ctx_t *c)
{
	state_t s = EAR_SUCCESS;
	while (pthread_mutex_trylock(&lock));
	if (!ok_init) {
		if (state_ok(s = static_init())) {
			ok_init = 1;
		}
	}
	pthread_mutex_unlock(&lock);
	debug("returning %d", s);
	return s;
}

state_t nvml_dispose(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t nvml_count(ctx_t *c, uint *dev_count_in)
{
	if (dev_count_in != NULL) {
		*dev_count_in = dev_count;
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
	if ((s = nvml.Mode(devices[i], &mode)) != NVML_SUCCESS) {
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
	s = nvml.Power(devices[i], &power_mw);
	s = nvml.Clocks(devices[i], NVML_CLOCK_MEM, &freq_mem_mhz);
	s = nvml.Clocks(devices[i], NVML_CLOCK_SM , &freq_gpu_mhz);
	s = nvml.Temp(devices[i], NVML_TEMPERATURE_GPU, &temp_gpu);
	s = nvml.Util(devices[i], &util);
	s = nvml.Procs(devices[i], &proc_count, procs);
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

state_t nvml_pool(void *p)
{
	timestamp_t time;
	gpu_t metric;
	int working;
	int i;

	if (!ok_init) {
		return_msg(EAR_NOT_INITIALIZED, Error.init_not);
	}
	//
	working = 0;
	// Lock
	while (pthread_mutex_trylock(&lock));
	timestamp_getfast(&time);
	//
	for (i = 0; i < dev_count; ++i) {
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

state_t nvml_read(ctx_t *c, gpu_t *data)
{
	if (!ok_init) {
		return_msg(EAR_NOT_INITIALIZED, Error.init_not);
	}
	while (pthread_mutex_trylock(&lock));
	memcpy(data, pool, dev_count * sizeof(gpu_t));
	pthread_mutex_unlock(&lock);
	return EAR_SUCCESS;
}

state_t nvml_read_raw(ctx_t *c, gpu_t *data)
{
	timestamp_t time;
	int i;
	timestamp_getfast(&time);
	for (i = 0; i < dev_count; ++i) {
		static_read(i, &data[i]);
		data[i].time     = time;
		data[i].power_w /= 1000;
	}
	return EAR_SUCCESS;
}
