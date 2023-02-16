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

#define SHOW_DEBUGS 1

#include <stdlib.h>
#include <metrics/common/popen.h>
#include <metrics/gpu/archs/dcgmi.h>

static char command[1024];
static uint events_count  = 0;
static uint gpus_count    = 0;

state_t gpu_dcgmi_init(char *events, uint events_count_in)
{
    popen_t p;
    state_t s;

    if (events == NULL) return_msg(EAR_BAD_ARGUMENT, Generr.input_null);

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

    if ((gpus_count == 0) || (events_count == 0) || (d == NULL)) return_msg(EAR_BAD_ARGUMENT, Generr.input_null);

    if (state_fail(s = popen_open(command, 2, 1, &p))) {
        return s;
    }
    while(popen_read(&p, "aiD", &gpu, values)) {
        memcpy(d[gpu].values, values, sizeof(double)*events_count);
        if (events_count >= 2) debug("GPU%d: %0.2lf %0.2lf\n", gpu, values[0], values[1]);
    }
    popen_close(&p);
    return EAR_SUCCESS;
}

void gpu_dcgmi_data_alloc(dcgmi_t **d)
{
    if (d == NULL) return;

    if (gpus_count){
      *d = calloc(gpus_count, sizeof(dcgmi_t));
    }else *d = NULL;
}

void gpu_dcgmi_data_str(dcgmi_t *d, char *d_str)
{
  uint g, e;
  char ev_str[64];

  if ((d == NULL) || (d_str == NULL) || (events_count == 0)) return;

  d_str[0] = '\0';
  for (g = 0; g < gpus_count; g++){
    snprintf(ev_str, sizeof(ev_str), "GPU[%u] ", g);
    strcat(d_str, ev_str);
    for (e = 0; e < events_count; e++){
      if (e) strcat(d_str,",");
      snprintf(ev_str, sizeof(ev_str), "ev[%u] = %lf " , e, d[g].values[e]);
      strcat(d_str, ev_str);
    }
  }
}

void gpu_dcgmi_events_to_str(uint *dcgmi_events, uint num_ev,char * dcgmi_events_str)
{
  uint i;
  char ev_str[64];

  if ((dcgmi_events == NULL) || (dcgmi_events_str == NULL)) return;

  dcgmi_events_str[0] = '\0';
  for (i = 0; i < num_ev; i++){
    if (i) strcat(dcgmi_events_str, ",");
    snprintf(ev_str, 64, "%u", dcgmi_events[i]);
    strcat(dcgmi_events_str,ev_str);
  }
}

#if TEST
int main(int argc, char *argv[])
{
    dcgmi_t *d;
    gpu_dcgmi_init("1009,1010", 2);
    gpu_dcgmi_data_alloc(&d);
    gpu_dcgmi_read(d);
    return 0;
}
#endif
