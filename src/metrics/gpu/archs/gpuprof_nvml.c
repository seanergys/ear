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

#include <stdlib.h>
#include <common/output/debug.h>
#include <metrics/common/nvml.h>
#include <metrics/gpu/archs/gpuprof_nvml.h>

static nvmlGpmMetricsGet_t *gpms; //N devices
static gpuprof_evs_t       *mevs;
static uint                 mevs_count;
static nvml_t               nvml;
static uint			        devs_count;
static nvmlDevice_t        *devs;

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

static int samples_free(nvmlGpmMetricsGet_t *sev)
{

}

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

GPUPROF_F_LOAD(gpuprof_nvml_load)
{
    int i = 0;

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
    // Allocating space for our own event list (mevs)
    mevs = calloc(NVML_GPM_METRIC_MAX, sizeof(gpuprof_evs_t));
    // Allocating space for NVML metrics for all devices + additional for testing
    gpms = calloc(devs_count + 1, sizeof(nvmlGpmMetricsGet_t));
    // Allocating samples
    while (i < (devs_count+1)) {
        if (!samples_alloc(&gpms[i])) {
            return;
        }
        i++;
    }
    // Getting event names and ids
    i = 1;
    while(events_fill(&gpms[devs_count], i++));
    // Creating polling thread
    pthread_create(&thread, NULL, poll, NULL);

    apis_put(ops->get_info,         gpuprof_nvml_get_info);
    apis_put(ops->events_get,       gpuprof_nvml_events_get);
    apis_put(ops->events_set,       gpuprof_nvml_events_set);
    apis_put(ops->read,             gpuprof_nvml_read);
    apis_put(ops->read_raw,         gpuprof_nvml_read_raw);
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

state_t gpu_nvml_pool(void *p)
{
    nvmlReturn_t r;

    sleep(1);
    if ((r = nvml.GpmSampleGet(devs[0], gpms[0].sample2)) != NVML_SUCCESS) {
        debug("Error while loading NVML sample: %s", nvml.ErrorString(r));
        return NULL;
    }
    if ((r = nvml.GpmMetricsGet(&gpms[0])) != NVML_SUCCESS) {
        debug("Error while loading NVML metrics: %s", nvml.ErrorString(r));
        return NULL;
    }
    memcpy(gpms[0].sample1, gpms[0].sample2, sizeof(nvmlGpmSample_t));
    debug("%s: value.%lf %s", gpms[0].metrics[0].metricInfo.longName, gpms[0].metrics[0].value, gpms[0].metrics[0].metricInfo.unit);
    return NULL;
}

GPUPROF_F_EVENTS_SET(gpuprof_nvml_events_set)
{
    nvmlReturn_t r;
    pthread_t thread;

    gpms[0].numMetrics          = 1;
    gpms[0].metrics[0].metricId = 10;
    if ((r = nvml.GpmSampleGet(devs[0], gpms[0].sample1)) != NVML_SUCCESS) {
        debug("Error while loading NVML sample: %s", nvml.ErrorString(r));
        return;
    }
    sleep(100);
}

GPUPROF_F_READ(gpuprof_nvml_read)
{
    return EAR_SUCCESS;
}

GPUPROF_F_READ_RAW(gpuprof_nvml_read_raw)
{
    return EAR_SUCCESS;
}
