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

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static gpuprof_evs_t  *events;
static uint            events_count;
static uint            devs_count;
static popen_t         p;
static gpuprof_t       pool;
static double         *pool_aux_buffer; //auxiliar buffer

// DCGMI limits all GPUs to monitor the same events

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
    // Allocating pool and an auxiliar buffer
    gpuprof_dcgmi_data_alloc(&pool);
    pool_aux_buffer = calloc(events_count, sizeof(double));
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

static void invalidate(gpuprof_t *data, int dev)
{
    pool.hashes[dev]        = NULL;
    pool.samples_count[dev] = 1.0;
    pool.values_count[dev]  = 0U;
    memset(pool.values[dev] , 0, NVML_GPM_METRIC_MAX*sizeof(double));
}

static void unset(int dev)
{
    invalidate(&pool, dev);
}

static void set(int dev, char *evs, uint ids_count)
{
    if (pool.hashes[dev] == evs) {
        return;
    }
    // If different reference, unset
    unset(dev);
    // Changing pool values
    pool.hashes[dev] = evs;
    pool.values_count[dev] = ids_count;
}

GPUPROF_F_EVENTS_SET(gpuprof_dcgmi_events_set)
{
    char command[512];
    uint ids_count;
    state_t s;
    uint *ids;

    if (evs == NULL) {
        return;
    }
    // Getting the number of events to monitor
    ids = (uint *) strtoat(evs, ',', NULL, &values_count, ID_UINT);
    free(ids);
    // Filling the pool
    while (pthread_mutex_trylock(&lock));
    // Any device means all devices for DCGMI
    for (d = 0; d < devs_count; ++d) {
        set(d, evs, ids, ids_count);
    }
    // New popen every 2 seconds
    sprintf(command, "dcgmi dmon -e %s -d 2000", evs);
    if (state_fail(s = popen_open(command, 2, 0, &p))) {
        debug("Failed popen_open: %s", state_msg);
    }
    pthread_mutex_unlock(&lock);
}

GPUPROF_F_EVENTS_UNSET(gpuprof_dcgmi_events_unset)
{
    int d;
    while (pthread_mutex_trylock(&lock));
    popen_close(&p);
    for (d = 0; d < devs_count; ++d) {
        unset(d);
    }
    pthread_mutex_unlock(&lock);
}

GPUPROF_F_READ(gpuprof_dcgmi_read)
{
    char buffer[128];
    int dev;
    int m;

    while (pthread_mutex_trylock(&lock));
    // Pooling
    while(popen_read(&p, "siD", buffer, &dev, pool_aux_buffer)) {
        for (m = 0; m < values_count; ++m) {
            pool.values[dev][m] += pool_aux_buffer[m];
            debug("D%d_M%d: %lf += %lf %2.lf", dev, m, pool.values[dev][m], pool_aux_buffer[m], pool.samples_count[dev]);
        }
        pool.samples_count[dev] += 1.0;
    }
    // Once pooling is over, we copy the values to data
    gpuprof_dcgmi_data_copy(data, &pool);
    pthread_mutex_unlock(&lock);
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
        if (data2->hashes[dev] != data1->hashes[dev]) {
            invalidate(dataD);
        }
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
    data->hashes        = calloc(devs_count, sizeof(ullong));
    data->values        = calloc(devs_count, sizeof(double *));
    data->values[0]     = calloc(devs_count*events_count, sizeof(double));
    data->values_count  = calloc(devs_count, sizeof(uint));
    data->samples_count = calloc(devs_count, sizeof(double));
    // Setting values pointers
    for (d = 0; d < devs_count; ++d) {
        data->values[d] = &data->values[0][d*events_count];
    }
}

GPUPROF_F_DATA_COPY(gpuprof_dcgmi_data_copy)
{
    memcpy( dataD->values[0]    ,  dataS->values[0]    , devs_count*events_count*sizeof(double));
    memcpy( dataD->samples_count,  dataS->samples_count, devs_count*sizeof(double));
    memcpy( dataD->values_count ,  dataS->values_count , devs_count*sizeof(uint));
    memcpy( dataD->hashes       ,  dataS->hashes       , devs_count*sizeof(ullong));
    memcpy(&dataD->time         , &dataS->time         , sizeof(timestamp_t));
}
