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
#include <common/string_enhanced.h>
#include <common/math_operations.h>
#include <metrics/common/popen.h>
#include <metrics/gpu/archs/gpuprof_dcgmi.h>

static gpuprof_evs_t *events;
static uint           events_count;
static uint           devs_count;
static popen_t        p;
static gpuprof_t      pool;
static double        *values_buffer; //auxiliar buffer
static uint           values_count;

GPUPROF_F_LOAD(gpuprof_dcgmi_load)
{
    char *event_name;
    int event_id;
    char *count;
    state_t s;

    //dcgmi dmon --list
    if (state_fail(s = popen_test("dcgmi"))) {
        debug("Failed popen_test: %s", state_msg);
        return;
    }
    // Getting the number of GPUS
    if (state_fail(s = popen_open("dcgmi discovery -l", 0, 1, &p))) {
        debug("Failed popen_open: %s", state_msg);
        return;
    }
    popen_read(&p, "s", &count);
    if (strstr(strtolow(count), "error") != NULL) {
        debug("Failed while reading dcgmi discovery: %s", state_msg);
        popen_close(&p);
        return;
    }
    devs_count = (uint) atoi(count);
    popen_close(&p);
    if (devs_count == 0) {
        debug("No GPUs found");
        return;
    }
    if (state_fail(s = popen_open("dcgmi dmon --list", 3, 1, &p))) {
        debug("Failed popen_open: %s", state_msg);
        return;
    }
    // Allocating
    if (!popen_read(&p, "sai", &event_name, &event_id)) {
        debug("Failed popen_read: %s", state_msg);
        return;
    }
    if ((events_count = popen_count_read(&p)) == 0) {
        debug("No events found");
        return;
    }
    events = calloc(events_count, sizeof(gpuprof_evs_t));
    events_count = 0;
    // Saving events
    do {
        strcpy(events[events_count].name, event_name);
        events[events_count].id = (uint) event_id;
        debug("EV%d: %s", event_id, event_name);
        ++events_count;
    }
    while (popen_read(&p, "sai", &event_name, &event_id));
    //
    gpuprof_dcgmi_data_alloc(&pool);
    values_buffer = calloc(events_count, sizeof(double));
    //
    apis_put(ops->get_info,         gpuprof_dcgmi_get_info);
    apis_put(ops->events_get,       gpuprof_dcgmi_events_get);
    apis_put(ops->events_set,       gpuprof_dcgmi_events_set);
    apis_put(ops->events_unset,     gpuprof_dcgmi_events_unset);
    apis_put(ops->read,             gpuprof_dcgmi_read);
    apis_put(ops->read_raw,         gpuprof_dcgmi_read_raw);
    apis_put(ops->data_diff,        gpuprof_dcgmi_data_diff);
    apis_put(ops->data_alloc,       gpuprof_dcgmi_data_alloc);
    apis_put(ops->data_copy,        gpuprof_dcgmi_data_copy);
    debug("Loaded DCGMI");
}

GPUPROF_F_GET_INFO(gpuprof_dcgmi_get_info)
{
    info->api         = API_DCGMI;
    info->scope       = SCOPE_NODE;
    info->granularity = GRANULARITY_PERIPHERAL;
    info->devs_count  = devs_count;
}

GPUPROF_F_EVENTS_GET(gpuprof_dcgmi_events_get)
{
    *evs_count = events_count;
    *evs = events;
}

GPUPROF_F_EVENTS_SET(gpuprof_dcgmi_events_set)
{
    char command[512];
    state_t s;
    uint *ids;

    // String treatment (is a NULL terminated list)
    ids = (uint *) strtoat(evs, ',', NULL, &values_count, ID_UINT);
    free(ids);

    gpuprof_dcgmi_events_unset(dev);
    sprintf(command, "dcgmi dmon -e %s -d 2000", evs);
    if (state_fail(s = popen_open(command, 2, 0, &p))) {
        debug("Failed popen_open: %s", state_msg);
    }
}

GPUPROF_F_EVENTS_UNSET(gpuprof_dcgmi_events_unset)
{
    popen_close(&p);
}

GPUPROF_F_READ(gpuprof_dcgmi_read)
{
    int dev;
    int m;

    // All devices have the same events in DCGMI
    while(popen_read(&p, "aiD", &dev, values_buffer)) {
        for (m = 0; m < values_count; ++m) {
            if (!isnan(values_buffer[m])) {
                pool.values[dev][m] += values_buffer[m];
                pool.samples_count[dev] += 1.0;
            }
        }
    }
    return EAR_SUCCESS;
}

GPUPROF_F_READ_RAW(gpuprof_dcgmi_read_raw)
{
    return EAR_SUCCESS;
}

GPUPROF_F_DATA_DIFF(gpuprof_dcgmi_data_diff)
{
    int d, m;
    // Float time in seconds with micro seconds precission
    dataD->time_s = timestamp_fdiff(&data2->time, &data1->time, TIME_SECS, TIME_USECS);
    for (d = 0; d < devs_count; ++d) {
        for (m = 0; m < values_count; ++m) {
            dataD->samples_count[d] = 0U;
            dataD->values[d][m]     = 0.0;
            if (data2->samples_count[d] > data1->samples_count[d]) {
                dataD->samples_count[d] = data2->samples_count[d] - data1->samples_count[d];
                dataD->values[d][m]     = overflow_zeros_f64(data2->values[d][m], data1->values[d][m]);
                dataD->values[d][m]     = dataD->values[d][m] / (dataD->samples_count[d]);
            }
        }
    }
}

GPUPROF_F_DATA_ALLOC(gpuprof_dcgmi_data_alloc)
{
    int d;

    data->devs_count    = devs_count;
    data->values        = calloc(devs_count, sizeof(double *));
    data->values[0]     = calloc(devs_count*events_count, sizeof(double));
    data->values_count  = calloc(devs_count, sizeof(uint));
    data->samples_count = calloc(devs_count, sizeof(double));
    // Setting all values to 0
    memset(data->values[0], 0, devs_count*events_count*sizeof(double));
    // Setting pointers
    for (d = 0; d < devs_count; ++d) {
        data->values[d] = &data->values[0][d*events_count];
        data->values_count[d] = values_count;
    }
}

GPUPROF_F_DATA_COPY(gpuprof_dcgmi_data_copy)
{
    memcpy(dataD->values[0], dataS->values[0], devs_count*events_count*sizeof(double));
    memcpy(dataD->samples_count, dataS->samples_count, devs_count*sizeof(double));
    memcpy(&dataD->time, &dataS->time, sizeof(timestamp_t));
}