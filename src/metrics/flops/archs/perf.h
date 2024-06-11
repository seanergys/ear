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

#ifndef METRICS_FLOPS_PERF_H
#define METRICS_FLOPS_PERF_H

#include <metrics/flops/flops.h>

void flops_perf_load(topology_t *tp, flops_ops_t *ops);

void flops_perf_get_info(apinfo_t *info);

state_t flops_perf_init();

state_t flops_perf_dispose();

state_t flops_perf_read(flops_t *ca);

void flops_perf_data_diff(flops_t *fl2, flops_t *fl1, flops_t *flD, double *gfs);

void flops_perf_internals_tostr(char *buffer, int length);

#endif //METRICS_FLOPS_PERF_H
