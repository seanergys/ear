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
static uint            ok_pool;
static suscription_t  *sus;
static uint            devs_count;
static ze_handlers_t  *devs;
static ze_t            ze;
static int             mode;
static uint            initialized;

#define if_fail(function, action) \
    if ((z = function) != ZE_RESULT_SUCCESS) { \
        debug("Failed " #function ": %s (0x%x)", oneapi_strerror(z), z); \
        action; \
    }

static void load_atfork()
{
    // Just lock control is enough
    pthread_mutex_unlock(&lock);
}

void gpu_oneapi_load(gpu_ops_t *ops, int force_api)
{
    debug("Received API %d", force_api);
    // If api is API_EARD, then EARD provides the gpu_t reading.
    if (API_IS(force_api, API_ONEAPI)) {
        ok_pool = 1;
    } else if (API_IS(force_api, API_FREE)) {
        ok_pool = oneapi_is_privileged();
    }
    // Just sysman is needed
    if (state_fail(oneapi_open_sysman(&ze))) {
        debug("oneapi_open_sysman failed: %s", state_msg);
        return;
    }
    if (state_fail(oneapi_get_handlers(&devs, &devs_count))) {
        debug("oneapi_get_devices failed: %s", state_msg);
        return;
    }
    // Allocation
    pool = calloc(devs_count, sizeof(gpu_t));
    // Atfork control
    pthread_atfork(NULL, NULL, load_atfork);

    apis_set(ops->get_info,            gpu_oneapi_get_info);
    apis_set(ops->get_devices,         gpu_oneapi_get_devices);
    apis_set(ops->init,                gpu_oneapi_init);
    apis_set(ops->dispose,             gpu_oneapi_dispose);
    apis_set(ops->set_monitoring_mode, gpu_oneapi_set_monitoring_mode);
    apis_pin(ops->read,                gpu_oneapi_read, ok_pool);
    apis_set(ops->read_raw,            gpu_oneapi_read_raw);
}

void gpu_oneapi_get_info(apinfo_t *info)
{
    info->api         = API_ONEAPI;
    info->devs_count  = devs_count;
}

void gpu_oneapi_get_devices(gpu_devs_t **devs_in, uint *devs_count_in)
{
    int dv;
    *devs_in = calloc(devs_count, sizeof(gpu_devs_t));
    //
    for (dv = 0; dv < devs_count; ++dv) {
        (*devs_in)[dv].serial = devs[dv].uuid;
        (*devs_in)[dv].index  = dv;
    }
    if (devs_count_in != NULL) {
        *devs_count_in = devs_count;
    }
}

state_t gpu_oneapi_init(ctx_t *c)
{
    state_t s = EAR_SUCCESS;
    timestamp_t time;
    int i;

    if (initialized) {
        return s;
    }
    // Initializing pool (pool at 0 is not incorrect)
    timestamp_getfast(&time);
    for (i = 0; i < devs_count; ++i) {
        pool[i].time    = time;
        pool[i].correct = 1;
    }
    if (ok_pool) {
    // Monitor suscription
	sus = suscription();
	sus->call_main  = gpu_oneapi_pool;
	sus->time_relax = 5000;
	sus->time_burst = 3000;
	// Initializing monitoring thread.
	if (state_ok(s = sus->suscribe(sus))) {
        initialized = 1;
	}
    } // ok_pool
	return s;
}

state_t gpu_oneapi_dispose(ctx_t *c)
{
    if (initialized) {
        monitor_unregister(sus);
        initialized = 0;
    }
	return EAR_SUCCESS;
}

void gpu_oneapi_set_monitoring_mode(int mode_in)
{
    mode = mode_in;

    if (mode == MONITORING_MODE_IDLE && ok_pool) {
        monitor_relax(sus);
    }
    if (mode == MONITORING_MODE_RUN && ok_pool) {
        monitor_burst(sus, 0);
    }
}

static state_t oneapi_read_power(int dv, gpu_t *data)
{
	zes_power_energy_counter_t info;
	uint                       d;
	ze_result_t                z;
    double                     joules;

    // Retrieving the domains
    data->energy_j = 0.0;
    // It seems CARD matches DOMAIN0 (all the PCI CARD)
    if (devs[dv].scard_count > 0) {
        if (ze.sPowerGetEnergyCounter(devs[dv].scard, &info) == ZE_RESULT_SUCCESS) {
            debug("DEV%d CARDP DOMAIN%d: %020lu uJ at %lu uS", dv, 0, info.energy, info.timestamp);
            data->energy_j = ((double) info.energy) / 1000000.0;
            #if !SHOW_DEBUGS
            return EAR_SUCCESS;
            #endif
        }
    }
    // If card doesn't exists
	for (d = 0; d < devs[dv].spowers_count; ++d) {
		if_fail(ze.sPowerGetEnergyCounter(devs[dv].spowers[d], &info), continue);
        debug("DEV%d POWER DOMAIN%d: %020lu uJ at %lu uS (sub %d)", dv, d, info.energy, info.timestamp, devs[dv].spowers_props[d].subdeviceId);
        // SUBDEVICE -1 seems to be the whole GPU, but if not, we are
        // taking the greatest joules value
		joules = ((double) info.energy) / 1000000.0;
        if (joules > data->energy_j) {
    		data->energy_j = joules;
        }
        #if !SHOW_DEBUGS
        if (devs[dv].spowers_props[d].subdeviceId == -1) {
            return EAR_SUCCESS;
        }
        #endif
	}
	return EAR_SUCCESS;
}

static state_t oneapi_read_frequency(int dv, gpu_t *data)
{
    zes_freq_state_t      info;
    int                   d;
    ze_result_t           z;
    uint                  gpu_count = 0;
    uint                  mem_count = 0;

    // Retrieving the domains
    for (d = devs[dv].sfreqs_count-1; d >= 0; --d) {
        if_fail(ze.sFrequencyGetState(devs[dv].sfreqs[d], &info), continue);
        debug("DEV%d FREQ DOMAIN%d: act/eff/tdp %.0lf/%.0lf/%.0lf (%.0lf to %.0lf MHz) (sub %d)", dv, d,
            info.actual, info.efficient, info.tdp, devs[dv].sfreqs_props[d].max,
            devs[dv].sfreqs_props[d].min, devs[dv].sfreqs_props[d].subdeviceId);
        // Taking DOMAIN0
        if (devs[dv].sfreqs_props[d].type == ZES_FREQ_DOMAIN_GPU) {
            data->freq_gpu += (ulong) info.actual; // MHz
            gpu_count += 1;
        }
        if (devs[dv].sfreqs_props[d].type == ZES_FREQ_DOMAIN_MEMORY) {
            data->freq_mem += (ulong) info.actual; // MHz
            mem_count += 1;
        }
    }
    if (gpu_count > 1) data->freq_gpu /= gpu_count;
    if (mem_count > 1) data->freq_mem /= mem_count;
    return EAR_SUCCESS;
}

static const char *engtostr(int eng, int *prio)
{
    #define engstr(e, p) \
        if (eng == ZES_ENGINE_GROUP_ ##e) { \
            if (prio != NULL) { \
                *prio = p; \
            } \
            return #e; \
        }
    #define NO_PRIO 10

    if (prio != NULL) {
        *prio = NO_PRIO;
    }
    engstr(ALL, 1);
    engstr(COMPUTE_ALL, 2);
    engstr(COMPUTE_SINGLE, 3);
    engstr(RENDER_ALL, 4);
    engstr(RENDER_SINGLE, 5);
    engstr(3D_RENDER_COMPUTE_ALL, 6);
    engstr(3D_ALL, 7);
    engstr(3D_SINGLE, 8);
    engstr(MEDIA_ALL, 9);
    engstr(MEDIA_DECODE_SINGLE, NO_PRIO);
    engstr(MEDIA_ENCODE_SINGLE, NO_PRIO);
    engstr(MEDIA_ENHANCEMENT_SINGLE, NO_PRIO);
    engstr(COPY_ALL, NO_PRIO);
    engstr(COPY_SINGLE, NO_PRIO);
    return "UNKNOWN";
}

static state_t oneapi_read_utilization(int dv, gpu_t *data)
{
    static zes_engine_stats_t save[32][64]; //32 devices, 64 domains
    zes_engine_stats_t        info;
	uint                      d;
    int                       prio_count = 0;
    int                       prio_set = NO_PRIO;
    int                       prio;
    ze_result_t               z;
    #if SHOW_DEBUGS
	ze_result_t               z;
    #endif

	for (d = 0; d < devs[dv].sengines_count; ++d) {
        if_fail(ze.sEngineGetActivity(devs[dv].sengines[d], &info), continue);
        #if SHOW_DEBUGS
        debug("DEV%d ENGINE DOMAIN%d: %lu active uS (%lu timestamp) (sub %d) (%s)", dv, d,
            info.activeTime, info.timestamp, devs[dv].sengines_props[d].subdeviceId,
            engtostr(devs[dv].sengines_props[d].type, NULL));
        #endif
        // Getting the priority
        engtostr(devs[dv].sengines_props[d].type, &prio);
        // Is new or already found?
        if (prio != NO_PRIO) {
            ulong adiff = info.activeTime - save[dv][d].activeTime;
            ulong tdiff = info.timestamp - save[dv][d].timestamp;
            if (prio < prio_set) {
                debug("DEV%d ENGINE DOMAIN%d: selecting %s", dv, d, engtostr(devs[dv].sengines_props[d].type, NULL));
                data->util_gpu  = (tdiff > 0LU) ? adiff * 100 / tdiff : 0LU;
                prio_set        = prio;
                prio_count      = 1;
            } else if (prio == prio_set) {
                data->util_gpu += (tdiff > 0LU) ? adiff * 100 / tdiff : 0LU;
                prio_count     += 1;
            }
            memcpy(&save[dv][d], &info, sizeof(zes_engine_stats_t));
        }
	}
    if (prio_count > 1) {
        data->util_gpu /= prio_count;
    }
    if (data->util_gpu > 0) {
        data->working = 1;
    }
	return EAR_SUCCESS;
}

static state_t oneapi_read_temperature(int dv, gpu_t *data)
{
    zes_psu_state_t       state;
    double                temp;
    uint                  s;
    ze_result_t           z;
    uint                  gpu_count = 0;
    uint                  mem_count = 0;

    for (s = 0; s < devs[dv].stemps_count; ++s) {
        if_fail(ze.sTemperatureGetState(devs[dv].stemps[s], &temp), continue);
        debug("DEV%d TEMP DOMAIN%d: %lf celsius (%lf max) (type %d)", dv, s,
                temp, devs[dv].stemps_props[s].maxTemperature, devs[dv].stemps_props[s].type);
        if (devs[dv].stemps_props[s].type != ZES_TEMP_SENSORS_GPU) {
            data->temp_gpu += (ulong) temp;
            gpu_count += 1;
        }
        if (devs[dv].stemps_props[s].type != ZES_TEMP_SENSORS_MEMORY) {
            data->temp_mem += (ulong) temp;
            mem_count += 1;
        }
    }
    if (gpu_count > 1) data->temp_gpu /= gpu_count;
    if (mem_count > 1) data->temp_mem /= mem_count;
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
    // If monitoring mode is RUN, it means there are jobs running in GPU and we
    // can read more metrics without the fear of power consumption being increased.
    if (mode == MONITORING_MODE_RUN) {
        oneapi_read_frequency(dv, gpu);
        oneapi_read_utilization(dv, gpu);
        oneapi_read_temperature(dv, gpu);
    }
}

state_t gpu_oneapi_pool(void *p)
{
    timestamp_t time;
    double time_diff;
	gpu_t current;
	int dv;

	// Locking
	while (pthread_mutex_trylock(&lock));
    // Time operations
    timestamp_getfast(&time);
    time_diff = (double) timestamp_diff(&time, &pool[0].time, TIME_USECS);
    time_diff = time_diff / 1000000.0;
	// Adding to the pool
	for (dv = 0; dv < devs_count; ++dv)
	{
		read_single(dv, &current);

		pool[dv].time      = time;
		pool[dv].samples  += 1;
		pool[dv].freq_mem += current.freq_mem;
		pool[dv].freq_gpu += current.freq_gpu;
		pool[dv].util_mem += 0LU;
		pool[dv].util_gpu += current.util_gpu;
		pool[dv].temp_gpu += current.temp_gpu;
		pool[dv].temp_mem += current.temp_mem;
        pool[dv].power_w  += ((current.energy_j - pool[dv].energy_j) / time_diff);
		pool[dv].energy_j  = current.energy_j;
		pool[dv].working   = current.working;
		pool[dv].correct   = 1;
		
		debug("dev | metric   | value    ");
		debug("--- | -------- | -----    ");
		debug("%d   | samps    | %lu     ", dv, pool[dv].samples );
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