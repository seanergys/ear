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

#ifndef METRICS_COMMON_PERF_H
#define METRICS_COMMON_PERF_H

#include <common/types.h>
#include <common/states.h>
#include <linux/perf_event.h>

// Options (combine with &)
#define pf_exc	0x0001
#define pf_pin	0x0002		 

typedef struct perf_s {
	struct  perf_event_attr attr;
	void   *group;
	int     fd;
    double  scale;
    char    unit[16]; // Used in files
    char    event_name[64]; // Used in files
    char    pmu_name[64]; // Used in files
} perf_t;

/* For per-process events */
state_t perf_open(perf_t *perf, perf_t *group, pid_t pid, uint type, ulong event);
// Perf open extended/extra. With more options. Using cpu > 0 will imply system wide configuration
state_t perf_open_cpu(perf_t *perf, perf_t *group, pid_t pid, uint type, ulong event, uint options, int cpu);

state_t perf_open_files(perf_t **perfs, char *pmu_name, char *ev_name, uint *perfs_count);

// Set the perf_t scale variable when the ev_name matches the event_name variable.
void perf_set_scale(perf_t *perfs, uint perfs_count, char *ev_name, double scale);

state_t perf_reset(perf_t *perf);

state_t perf_start(perf_t *perf);

state_t perf_stop(perf_t *perf);

state_t perf_close(perf_t *perf);

state_t perf_read(perf_t *perf, llong *value);

#endif //METRICS_COMMON_PERF_H
