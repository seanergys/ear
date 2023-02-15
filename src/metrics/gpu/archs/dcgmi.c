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

#include <metrics/common/popen.h>
#include <metrics/gpu/archs/dcgmi.h>

static char command[1024];
static uint events_count;
static uint gpus_count;

state_t gpu_dcgmi_init(char *events, uint events_count_in)
{
    state_t s;
    // Getting the number of GPUS
    if (state_fail(s = popen_open("dcgmi discovery -l", 0, 1, &p))) {
        debug("Failed to open: %s", state_msg);
        return s;
    }
    popen_read(&p, "ia", &gpus_count);
    popen_close(&p);
    debug("gpus_count: %u", gpus_count);
    if (gpus_count == 0) {
        return_msg(EAR_ERROR, "No GPUs detected");
    }
    // Formatting command
    sprintf(command, "dcgmi dmon -e %s -c 1", events);
    events_count = events_count_in;
    return EAR_SUCCESS;
}

state_t gpu_dcgmi_read(dcgmi_t *d)
{
    double values[15];
    popen_t p;
    state_t s;
    int gpu;

    if (state_fail(s = popen_open(command, 2, 1, &p))) {
        return s;
    }
    while(popen_read(&p, "aiD", &gpu, &aux)) {
        memcpy(d[gpu].values, values, sizeof(double)*events_count);
        debug("GPU%d: %lf\n", gpu, d[gpu].values[0]);
    }
    popen_close(&p);
    return EAR_SUCCESS;
}

void gpu_dcgmi_data_alloc(dcgmi_t **d)
{
    *d = calloc(gpus_count, sizeof(dcgmi_t));
}