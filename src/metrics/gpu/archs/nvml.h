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

#ifndef METRICS_GPU_NVML_H
#define METRICS_GPU_NVML_H

#include <metrics/gpu/gpu.h>

state_t nvml_load();

state_t nvml_init(ctx_t *c);

state_t nvml_dispose(ctx_t *c);

state_t nvml_count(ctx_t *c, uint *gpu_count);

state_t nvml_pool(void *c);

state_t nvml_read(ctx_t *c, gpu_t *data);

/* Reads the data directly from the GPU API (not preprocessed data). */
state_t nvml_read_raw(ctx_t *c, gpu_t *data);

#endif //METRICS_GPU_NVML_H
