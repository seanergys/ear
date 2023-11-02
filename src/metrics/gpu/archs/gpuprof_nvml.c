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

#include <common/output/debug.h>
#include <metrics/common/nvml.h>
#include <metrics/gpu/archs/gpuprof_nvml.h>

static nvmlGpmMetricsGet_t nevs[512];
static gpuprof_evs_t *mevs;
static uint           mevs_count;
static nvml_t         nvml;
static uint			  devs_count;
static nvmlDevice_t  *devs;

GPUPROF_F_LOAD(gpuprof_nvml_load)
{
    if (state_fail(nvml_open(&nvml))) {
        debug("nvml_open failed: %s", state_msg);
        return;
    }
    if (state_fail(nvml_get_devices(&devs, &devs_count))) {
        debug("nvml_get_devices failed: %s", state_msg);
        return;
    }
    if (nvml.GpmMetricsGet == NULL) {
        return;
    }
    if (nvml.GpmMetricsGet(nevs) == NVML_SUCCESS) {
        return;
    }
    apis_put(ops->get_info,         gpuprof_nvml_get_info);
    apis_put(ops->events_get,       gpuprof_nvml_events_get);
    apis_put(ops->events_set,       gpuprof_nvml_events_set);
    apis_put(ops->events_multiplex, gpuprof_nvml_events_multiplex);
    apis_put(ops->read,             gpuprof_nvml_read);
    apis_put(ops->read_raw,         gpuprof_nvml_read_raw);
}

GPUPROF_F_GET_INFO(gpuprof_nvml_get_info)
{
    info->api         = API_DUMMY;
    info->scope       = SCOPE_NODE;
    info->granularity = GRANULARITY_PERIPHERAL;
    info->devs_count  = 1;
}

GPUPROF_F_EVENTS_GET(gpuprof_nvml_events_get)
{
    *evs = mevs;
    *evs_count = mevs_count;
}

GPUPROF_F_EVENTS_SET(gpuprof_nvml_events_set)
{

}

GPUPROF_F_EVENTS_MULTIPLEX(gpuprof_nvml_events_multiplex)
{

}

GPUPROF_F_READ(gpuprof_nvml_read)
{
    return EAR_SUCCESS;
}

GPUPROF_F_READ_RAW(gpuprof_nvml_read_raw)
{
    return EAR_SUCCESS;
}
