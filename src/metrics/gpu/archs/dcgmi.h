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

#ifndef METRICS_GPU_DCGMI_H
#define METRICS_GPU_DCGMI_H

#include <metrics/gpu/gpu.h>

typedef struct dcgmi_s {
    double values[15];
} dcgmi_t;

state_t gpu_dcgmi_init(char *events, uint events_count);

state_t gpu_dcgmi_read(dcgmi_t *d);

void gpu_dcgmi_data_alloc(dcgmi_t **d);

#endif //METRICS_GPU_DCGMI_H
