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

#include <stdlib.h>
#include <common/string_enhanced.h>
#include <metrics/common/popen.h>
#include <metrics/gpu/archs/gpuprof_dcgmi.h>

static gpuprof_evs_t *mevs;
static uint mevs_count;
static uint gpus_count;

GPUPROF_F_LOAD(gpuprof_dcgmi_load)
{
    char *event_name;
    int event_id;
    char *count;
    state_t s;
    popen_t p;

    //dcgmi dmon --list
    if (state_fail(s = popen_test("dcgmi"))) {
        debug("Failed to test: %s", state_msg);
        return;
    }
    // Getting the number of GPUS
    if (state_fail(s = popen_open("dcgmi discovery -l", 0, 1, &p))) {
        debug("Failed to open: %s", state_msg);
        return;
    }
    popen_read(&p, "s", &count);
    if (strstr(strtolow(count), "error") != NULL) {
        popen_close(&p);
        return;
    }
    gpus_count = (uint) atoi(count);
    popen_close(&p);
    if (gpus_count == 0) {
        return;
    }
    if (state_fail(s = popen_open("dcgmi dmon --list", 3, 1, &p))) {
        debug("Failed to open: %s", state_msg);
        return;
    }
    // Allocating
    if (!popen_read(&p, "sai", &event_name, &event_id)) {
        return;
    }
    if ((mevs_count = popen_count_read(&p)) == 0) {
        return;
    }
    mevs = calloc(mevs_count, sizeof(gpuprof_evs_t));
    mevs_count = 0;
    // Saving events
    do {
        strcpy(mevs[mevs_count].name, event_name);
        mevs[mevs_count].id = (uint) event_id;
        debug("EV%d: %s", event_id, event_name);
    }
    while (popen_read(&p, "sai", &event_name, &event_id));
    //
    apis_set(ops->get_info,         gpuprof_dcgmi_get_info);
    apis_set(ops->events_get,       gpuprof_dcgmi_events_get);
    apis_set(ops->events_set,       gpuprof_dcgmi_events_set);
    apis_set(ops->events_multiplex, gpuprof_dcgmi_events_multiplex);
    apis_set(ops->read,             gpuprof_dcgmi_read);
    apis_set(ops->read_raw,         gpuprof_dcgmi_read_raw);
    debug("Loaded DCGMI");
}

void gpuprof_dcgmi_get_info(apinfo_t *info)
{
    info->api         = API_DCGMI;
    info->scope       = SCOPE_NODE;
    info->granularity = GRANULARITY_PERIPHERAL;
    info->devs_count  = gpus_count;
}

void gpuprof_dcgmi_events_get(const gpuprof_evs_t **evs, uint *evs_count)
{
    *evs_count = mevs_count;
    *evs = mevs;
}

void gpuprof_dcgmi_events_set(char *evs, int group)
{

}

void gpuprof_dcgmi_events_multiplex()
{

}

state_t gpuprof_dcgmi_read(gpuprof_t *data)
{
    return EAR_SUCCESS;
}

state_t gpuprof_dcgmi_read_raw(gpuprof_t *data)
{
    return EAR_SUCCESS;
}