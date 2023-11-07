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

#define SHOW_DEBUGS 1

#include <math.h>
#include <stdlib.h>
#include <pthread.h>
#include <common/output/debug.h>
#include <common/utils/string.h>
#include <common/system/monitor.h>
#include <common/math_operations.h>
#include <metrics/common/nvml.h>
#include <metrics/gpu/archs/gpuprof_nvml.h>

static pthread_mutex_t      lock = PTHREAD_MUTEX_INITIALIZER;
static nvmlGpmMetricsGet_t *gpms; //N devices
static gpuprof_evs_t       *mevs;
static uint                 mevs_count;
static char               **pool_refs;
static gpuprof_t            pool;
static nvml_t               nvml;
static uint			        devs_count;
static nvmlDevice_t        *devs;
static suscription_t       *sus;


// Why this class uses the monitor thread instead using multiple nvmlGpmMetricsGet_t?
// Because if you add a void pointer to gpuprof_t to keep nvmlGpmMetricsGet_t 
// independent, you have to:
//  1) Remove the data1 or data2 and use single data, because nvmlGpmMetricsGet_t
//     keeps the sample2 and sample1 itself, and you can't allocate two gpuprof_t
//     and know in advance to which gpuprof_t is related. But is not recommended
//     to change the design just because one intern API is different.
//  2) If someone changes the events through events_set, the nvmlGpmMetricsGet_t 
//     will become invalid.
//

static state_t gpuprof_nvml_pool(void *p);

static int support_test()
{
    static nvmlGpmSupport_t support;
    nvmlReturn_t r;

    support.version = NVML_GPM_METRICS_GET_VERSION;
    if (nvml.GpmQueryDeviceSupport == NULL || nvml.GpmMetricsGet == NULL) {
        debug("GPM symbols not found");
        return 0;
    }
    if ((r = nvml.GpmQueryDeviceSupport(devs[0], &support)) != NVML_SUCCESS) {
        debug("Error while checking NVML GPM support: %s", nvml.ErrorString(r));
        return 0;
    }
    return 1;
}

static int samples_alloc(nvmlGpmMetricsGet_t *sev)
{
    nvmlReturn_t r;
    
    sev->version = NVML_GPM_METRICS_GET_VERSION;
    if ((r = nvml.GpmSampleAlloc(&sev->sample1)) != NVML_SUCCESS) {
        debug("Error while allocating sample1: %s", nvml.ErrorString(r));
        return 0;
    }
    if ((r = nvml.GpmSampleAlloc(&sev->sample2)) != NVML_SUCCESS) {
        debug("Error while allocating sample2: %s", nvml.ErrorString(r));
        return 0;
    }
    return 1;
}

#if 0
static int samples_free(nvmlGpmMetricsGet_t *sev)
{
    return 0;
}
#endif

static int events_fill(nvmlGpmMetricsGet_t *sev, int i)
{
    nvmlReturn_t r;
    if (i >= NVML_GPM_METRIC_MAX) {
        return 0;
    }
    // Cleaning metric
    memset(&sev->metrics[0], 0, sizeof(nvmlGpmMetric_t));
    // 
    sev->numMetrics          = 1;
    sev->metrics[0].metricId = i;
    
    if ((r = nvml.GpmMetricsGet(sev)) != NVML_SUCCESS) {
        debug("Error while loading NVML metrics: %s", nvml.ErrorString(r));
        return 1;
    }
    if (sev->metrics[0].metricInfo.longName == NULL) {
        return 1;
    }
    strcpy(mevs[mevs_count].name, sev->metrics[0].metricInfo.longName);
    //debug("EV%d: %s (%s)", i, mevs[mevs_count].name, sev->metrics[0].metricInfo.unit);
    mevs[mevs_count].id = (uint) i;
    mevs_count++;
    return 1;
}
    
static state_t gpuprof_nvml_pool2(void *p);

GPUPROF_F_LOAD(gpuprof_nvml_load)
{
    int i = 0;
    int d = 0;
    int m = 0;

    if (state_fail(nvml_open(&nvml))) {
        debug("nvml_open failed: %s", state_msg);
        return;
    }
    if (state_fail(nvml_get_devices(&devs, &devs_count))) {
        debug("nvml_get_devices failed: %s", state_msg);
        return;
    }
    if (!support_test()) {
        return;
    }
    // Allocating space for NVML metrics for all devices + additional for testing
    gpms = calloc(devs_count + 1, sizeof(nvmlGpmMetricsGet_t));
    // Allocating space for our own event list (mevs)
    mevs = calloc(NVML_GPM_METRIC_MAX, sizeof(gpuprof_evs_t));
    // Allocating samples
    while (i < (devs_count+1)) {
        if (!samples_alloc(&gpms[i])) {
            return;
        }
        i++;
    }
    // Allocating space for pooling counters
    gpuprof_nvml_data_alloc(&pool);
    // Allocating space for events references
    pool_refs = calloc(devs_count, sizeof(char **));
    // Getting event names and ids
    i = 1;
    while(events_fill(&gpms[devs_count], i++));
    // Creating pooling thread
    sus = suscription();
    sus->call_main  = gpuprof_nvml_pool;
    sus->time_relax = 2000;
    sus->time_burst = 2000;
    sus->suscribe(sus);

    apis_put(ops->get_info,         gpuprof_nvml_get_info);
    apis_put(ops->events_get,       gpuprof_nvml_events_get);
    apis_put(ops->events_set,       gpuprof_nvml_events_set);
    apis_put(ops->events_unset,     gpuprof_nvml_events_unset);
    apis_put(ops->read,             gpuprof_nvml_read);
    apis_put(ops->read_raw,         gpuprof_nvml_read_raw);
    apis_put(ops->data_diff,        gpuprof_nvml_data_diff);
    apis_put(ops->data_alloc,       gpuprof_nvml_data_alloc);
    apis_put(ops->data_copy,        gpuprof_nvml_data_copy);
    debug("Loaded NVML");
}

GPUPROF_F_GET_INFO(gpuprof_nvml_get_info)
{
    info->api         = API_NVML;
    info->scope       = SCOPE_NODE;
    info->granularity = GRANULARITY_PERIPHERAL;
    info->devs_count  = 1;
}

GPUPROF_F_EVENTS_GET(gpuprof_nvml_events_get)
{
    *evs = mevs;
    *evs_count = mevs_count;
}

static state_t gpuprof_nvml_pool(void *p)
{
    nvmlReturn_t r;
    int d, m;

    // nvmlGpmMetricsGet and nvmlGpmSampleGet overhead was measured between 0 and 3 ms.

    // Thread lock
    while (pthread_mutex_trylock(&lock));
    for (d = 0; d < devs_count; ++d) {
        // Reference is a pointer to the events list
        if (pool_refs[d] == NULL) {
            continue;
        }
        // Getting sample2
        if ((r = nvml.GpmSampleGet(devs[d], gpms[d].sample2)) != NVML_SUCCESS) {
            debug("Error while loading NVML sample: %s", nvml.ErrorString(r));
            continue;
        }
        // Differences
        if ((r = nvml.GpmMetricsGet(&gpms[d])) != NVML_SUCCESS) {
            debug("Error while loading NVML metrics: %s", nvml.ErrorString(r));
            continue;
        }
        for (m = 0; m < gpms[d].numMetrics; ++m) {
            if (!isnan(gpms[d].metrics[m].value)) {
                pool.values[d][m] += gpms[d].metrics[m].value;
                pool.samples[d]   += 1.0;
            }
            debug("%s: %lf.[%d][%d] += %lf %s (nvmlReturn %d: %s)",
                gpms[d].metrics[m].metricInfo.longName, pool.values[d][m], d, m,
                gpms[d].metrics[m].value, gpms[d].metrics[m].metricInfo.unit,
                gpms[d].metrics[m].nvmlReturn, nvml.ErrorString(gpms[d].metrics[m].nvmlReturn));
        }
        // Getting sample 1
        //memcpy(gpms[d].sample1, gpms[d].sample2, sizeof(nvmlGpmSample_t));
        if ((r = nvml.GpmSampleGet(devs[d], gpms[d].sample1)) != NVML_SUCCESS) {
            debug("Error while loading NVML sample: %s", nvml.ErrorString(r));
            continue;
        }
    }
    pthread_mutex_unlock(&lock);
    return EAR_SUCCESS;
}

static void unset(int dev)
{
    pool_refs[dev] = NULL;
    pool.samples[dev] = 0.0;
    memset(gpms[dev].metrics, 0, sizeof(gpms[dev].metrics));
    memset(pool.values[dev], 0, NVML_GPM_METRIC_MAX*sizeof(double));
}

static void set(int dev, char *ref, uint *ids, uint ids_count)
{
    nvmlReturn_t r;
    int i = 0;

    if (pool_refs[dev] == ref) {
        return;
    }
    // If different reference, unset
    unset(dev);
    pool_refs[dev] = ref;
    while (i < ids_count) {
        gpms[dev].metrics[i].metricId = ids[i];
        ++i;
    }
    gpms[dev].numMetrics = ids_count;
    //
    if ((r = nvml.GpmMetricsGet(&gpms[dev])) != NVML_SUCCESS) {
        debug("Error while loading NVML metrics: %s", nvml.ErrorString(r));
        return;
    }
    if ((r = nvml.GpmSampleGet(devs[dev], gpms[dev].sample1)) != NVML_SUCCESS) {
        debug("Error while loading NVML sample: %s", nvml.ErrorString(r));
        return;
    }
}

GPUPROF_F_EVENTS_SET(gpuprof_nvml_events_set)
{
    uint ids_count;
    uint *ids;
    int d;

    // String treatment (is a NULL terminated list)
    ids = (uint *) strtoat(evs, ',', NULL, &ids_count, ID_UINT);
    //
    while (pthread_mutex_trylock(&lock));
    if (dev == all_devs) {
        for (d = 0; d < devs_count; ++d) {
            set(d, evs, ids, ids_count);
        }
    } else {
        set(dev, evs, ids, ids_count);
    }
    pthread_mutex_unlock(&lock);
    free(ids);
}

GPUPROF_F_EVENTS_UNSET(gpuprof_nvml_events_unset)
{
    int d;
    while (pthread_mutex_trylock(&lock));
    if (dev == all_devs) {
        for (d = 0; d < devs_count; ++d) {
            unset(d);
        }
    } else {
        unset(dev);
    }
    pthread_mutex_unlock(&lock);
}

GPUPROF_F_READ(gpuprof_nvml_read)
{
//    gpuprof_nvml_pool(NULL);
	while (pthread_mutex_trylock(&lock));
    gpuprof_nvml_data_copy(data, &pool);
	pthread_mutex_unlock(&lock);
    return EAR_SUCCESS;
}

GPUPROF_F_READ_RAW(gpuprof_nvml_read_raw)
{
    return EAR_SUCCESS;
}

GPUPROF_F_DATA_DIFF(gpuprof_nvml_data_diff)
{
    int d, m;
    // Float time in seconds with micro seconds precission
    dataD->time_s = timestamp_fdiff(&data2->time, &data1->time, TIME_SECS, TIME_USECS);
    for (d = 0; d < devs_count; ++d) {
        for (m = 0; m < gpms[d].numMetrics; ++m) {
            if (data2->samples[d] > data1->samples[d]) {
                dataD->samples[d] = data2->samples[d] - data1->samples[d];
                dataD->values[d][m] = overflow_zeros_f64(data2->values[d][m], data1->values[d][m]) /  (dataD->samples[d]);
            } else {
                dataD->samples[d] = 0U;
                dataD->values[d][m] = 0.0;
            }
        }
    }
}

GPUPROF_F_DATA_ALLOC(gpuprof_nvml_data_alloc)
{
    double **aux;
    int d;

    data->devs_count   = devs_count;
    data->samples      = calloc(devs_count, sizeof(double));
    data->values       = calloc(devs_count, sizeof(double *));
    data->values[0]    = calloc(devs_count*NVML_GPM_METRIC_MAX, sizeof(double));
    data->values_count = calloc(devs_count, sizeof(uint)); 
    // Setting all values to 0
    memset(data->values[0], 0, devs_count*NVML_GPM_METRIC_MAX*sizeof(double));
    // Setting pointers
    for (d = 0; d < devs_count; ++d) {
        data->values[d] = &data->values[0][d*NVML_GPM_METRIC_MAX];
        data->values_count[d] = gpms[d].numMetrics;
    }
}

GPUPROF_F_DATA_COPY(gpuprof_nvml_data_copy)
{
    memcpy(dataD->values[0], dataS->values[0], devs_count*NVML_GPM_METRIC_MAX*sizeof(double));
    memcpy(dataD->samples  , dataS->samples  , devs_count*sizeof(double));
    memcpy(&dataD->time, &dataS->time, sizeof(timestamp_t));
}
