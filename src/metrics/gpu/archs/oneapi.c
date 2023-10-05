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

//#define SHOW_DEBUGS 1

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <common/output/debug.h>
#include <common/system/monitor.h>
#include <common/system/symplug.h>
#include <metrics/common/oneapi.h>
#include <metrics/gpu/archs/oneapi.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static gpu_t          *pool; // change to sysmans_pool
static timestamp_t     pool_time1;
static timestamp_t     pool_time2;
static suscription_t  *sus;
static uint            devs_count;
static ze_handlers_t  *devs;
static ze_t            ze;

#define if_fail(function, action) \
    if ((z = function) != ZE_RESULT_SUCCESS) { \
        debug("Failed " #function ": %s (0x%x)", oneapi_strerror(z), z); \
        action; \
    }

void gpu_oneapi_load(gpu_ops_t *ops)
{
    if (state_fail(oneapi_open(&ze))) {
        debug("oneapi_open failed: %s", state_msg);
        return;
    }
    if (state_fail(oneapi_get_devices(&devs, &devs_count))) {
        debug("oneapi_get_devices failed: %s", state_msg);
        return;
    }
    apis_set(ops->get_api,       gpu_oneapi_get_api);
    apis_set(ops->init,          gpu_oneapi_init);
    apis_set(ops->dispose,       gpu_oneapi_dispose);
    apis_set(ops->count_devices, gpu_oneapi_count_devices);
    apis_set(ops->read,          gpu_oneapi_read);
    apis_set(ops->read_raw,      gpu_oneapi_read_raw);
}

void gpu_oneapi_get_api(uint *api)
{
    *api = API_ONEAPI;
}

state_t gpu_oneapi_init(ctx_t *c)
{
    state_t s;
    #if 1
	// Monitor suscription
	pool = calloc(devs_count, sizeof(gpu_t));
	// Initializing monitoring thread suscription.
	sus = suscription();
	sus->call_main  = gpu_oneapi_pool;
	sus->time_relax = 2000;
	sus->time_burst = 1000;
	// Initializing monitoring thread.
	if (state_fail(s = sus->suscribe(sus))) {
		debug("GPU monitor FAILS");
		monitor_unregister(sus);
		free(pool);
	}
    #endif
	return EAR_SUCCESS;
}

state_t gpu_oneapi_dispose(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t gpu_oneapi_count_devices(ctx_t *c, uint *dev_count)
{
	*dev_count = devs_count;
	return EAR_SUCCESS;
}

static state_t oneapi_read_power(int dv, gpu_t *data)
{
    zes_pwr_handle_t           card;
	zes_power_energy_counter_t info;
	uint                       d;
	ze_result_t                z;

    // Retrieving the domains
    data->energy_j = 0.0;

	for (d = 0; d < devs[dv].spowers_count; ++d) {
		if_fail(ze.sPowerGetEnergyCounter(devs[dv].spowers[d], &info), continue);
        debug("DEV%d POWER DOMAIN%d: %020lu uJ at %lu uS (sub %d)", dv, d, info.energy, info.timestamp, devs[dv].spowers_props[d].subdeviceId);
        // DOMAIN0 seems to be the whole GPU
		data->energy_j = ((double) info.energy) / 1000000.0;
        return EAR_SUCCESS;
	}
    // It seems CARD matches DOMAIN0 (all the PCI CARD)
    if_fail(ze.sDeviceGetCardPowerDomain(devs[dv].sdevice, &devs[dv].scard), return EAR_SUCCESS);
    if_fail(ze.sPowerGetEnergyCounter(card, &info), return EAR_SUCCESS);
    debug("DEV%d CARDP DOMAIN%d: %020lu uJ at %lu uS", dv, 0, info.energy, info.timestamp);
	data->energy_j = ((double) info.energy) / 1000000.0;

	return EAR_SUCCESS;
}

static state_t oneapi_read_frequency(int dv, gpu_t *data)
{
    zes_freq_state_t      info;
    int                   d;
    ze_result_t           z;

    // Retrieving the domains
    data->freq_gpu = 0LU;
    data->freq_mem = 0LU;

    for (d = devs[dv].sfreqs_count-1; d >= 0; --d) {
        if_fail(ze.sFrequencyGetState(devs[dv].sfreqs[d], &info), continue);
        debug("DEV%d FREQ DOMAIN%d: act/eff/tdp %.0lf/%.0lf/%.0lf (%.0lf to %.0lf MHz) (sub %d)", dv, d,
            info.actual, info.efficient, info.tdp, devs[dv].sfreqs_props[d].max,
            devs[dv].sfreqs_props[d].min, devs[dv].sfreqs_props[d].subdeviceId);
        // Taking DOMAIN0
        if (devs[dv].sfreqs_props[d].type == ZES_FREQ_DOMAIN_GPU) {
            data->freq_gpu = (ulong) info.actual; // MHz
        }
        if (devs[dv].sfreqs_props[d].type == ZES_FREQ_DOMAIN_MEMORY) {
            data->freq_mem = (ulong) info.actual; // MHz
        }
    }
    return EAR_SUCCESS;
}

static state_t oneapi_read_utilization(int dv, gpu_t *data)
{
	zes_engine_stats_t      info;
	uint                    g;
	ze_result_t             z;

	for (g = 0; g < devs[dv].sengines_count; ++g) {
        if_fail(ze.sEngineGetActivity(devs[dv].sengines[g], &info), continue);
        debug("DEV%d ENGINE DOMAIN%d: %lu active uS (%lu timestamp)", dv, g,
            info.activeTime, info.timestamp);
		if (devs[dv].sengines_props[g].type == ZES_ENGINE_GROUP_ALL) {
			data->util_gpu = (ulong) (info.activeTime / info.timestamp); // In microseconds
		}
	}
	return EAR_SUCCESS;
}

static state_t oneapi_read_temperature(int dv, gpu_t *data)
{
    zes_psu_state_t       state;
    double                temp;
    uint                  s;
    ze_result_t           z;

    for (s = 0; s < devs[dv].stemps_count; ++s) {
        if_fail(ze.sTemperatureGetState(devs[dv].stemps[s], &temp), continue);
        debug("DEV%d TEMP DOMAIN%d: %lf celsius (%lf max) (type %d)", dv, s,
                temp, devs[dv].stemps_props[s].maxTemperature, devs[dv].stemps_props[s].type);
        if (devs[dv].stemps_props[s].type != ZES_TEMP_SENSORS_GPU) {
            data->temp_gpu = (ulong) temp;
        }
        if (devs[dv].stemps_props[s].type != ZES_TEMP_SENSORS_MEMORY) {
            data->temp_mem = (ulong) temp;
        }
    }
    for (s = 0; s < devs[dv].spsus_count; ++s) {
        if_fail(ze.sPsuGetState(devs[dv].spsus[s], &state), continue);
        debug("DEV%d PSU DOMAIN%d: %d celsius", dv, s, state.temperature);
    }
    return EAR_SUCCESS;
}

static void read_single(int dv, gpu_t *gpu)
{
	// Cleanin
	memset(gpu, 0, sizeof(gpu_t));
	// Reading
	oneapi_read_power(dv, gpu);
	oneapi_read_frequency(dv, gpu);
	oneapi_read_utilization(dv, gpu);
	oneapi_read_temperature(dv, gpu);
}

state_t gpu_oneapi_pool(void *p)
{
	gpu_t       current;
	int         dv;

	// Locking
	while (pthread_mutex_trylock(&lock));
	// Getting current time
	timestamp_getfast(&pool_time2);
	// Adding to the pool
	for (dv = 0; dv < devs_count; ++dv)
	{
		read_single(dv, &current);

		pool[dv].time      = pool_time2;
		pool[dv].samples  += 1;
		pool[dv].freq_mem += current.freq_mem;
		pool[dv].freq_gpu += current.freq_gpu;
		pool[dv].util_mem += 0LU;
		pool[dv].util_gpu += current.util_gpu;
		pool[dv].temp_gpu += current.temp_gpu;
		pool[dv].temp_mem += current.temp_mem;
		pool[dv].energy_j += current.energy_j;
		pool[dv].power_w  += 0.0;
		pool[dv].working   = current.working;
		pool[dv].correct   = current.correct;
		
		debug("dev | metric   | value    ");
		debug("--- | -------- | -----    ");
		debug("%d   | samps    | %u      ", dv, pool[dv].samples );
		debug("%d   | freq_gpu | %lu MHz ", dv, pool[dv].freq_gpu);
		debug("%d   | freq_mem | %lu MHz ", dv, pool[dv].freq_mem);
		debug("%d   | util_gpu | %lu %%  ", dv, pool[dv].util_gpu);
		debug("%d   | util_mem | %lu %%  ", dv, pool[dv].util_mem);
		debug("%d   | temp_gpu | %lu cels", dv, pool[dv].temp_gpu);
		debug("%d   | temp_mem | %lu cels", dv, pool[dv].temp_mem);
		debug("%d   | energy   | %0.2lf J", dv, pool[dv].energy_j);
		debug("%d   | working  | %d      ", dv, pool[dv].working );
	}
	// Copying time
	pool_time1 = pool_time2;
	// Unlock
	pthread_mutex_unlock(&lock);

	return EAR_SUCCESS;
}

state_t gpu_oneapi_read(ctx_t *c, gpu_t *data)
{
	while (pthread_mutex_trylock(&lock));
	memcpy(data, pool, devs_count * sizeof(gpu_t));
	pthread_mutex_unlock(&lock);
	return EAR_SUCCESS;
}

state_t gpu_oneapi_read_raw(ctx_t *c, gpu_t *data)
{
    gpu_oneapi_pool(NULL);
	return EAR_SUCCESS;
}
