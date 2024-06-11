/*
*
* This program is part of the EAR software.
*
* EAR provides a dynamic, transparent and ligth-weigth solution for
* Energy management. It has been developed in the context of the
* Barcelona Supercomputing Center (BSC)&Lenovo Collaboration project.
*
* 
* BSC Contact   mailto:ear-support@bsc.es
* 
*
* EAR is an open source software, and it is licensed under both the BSD-3 license
* and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
* and COPYING.EPL files.
*/

#ifndef METRICS_GPUPROF_DCGMI_H
#define METRICS_GPUPROF_DCGMI_H

#include <metrics/gpu/gpuprof.h>

// DCGMI limits all GPUs to monitor the same events. Because under the hood we
// are using POPEN which opens an additional process. If we wanted to add
// different events in different GPUs, we would have to open multiple POPEN
// processes.

GPUPROF_DEFINES(dcgmi);

#endif //METRICS_GPUPROF_DCGMI_H