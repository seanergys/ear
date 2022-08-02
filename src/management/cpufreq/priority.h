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

#ifndef MANAGEMENT_PRIORITY_H
#define MANAGEMENT_PRIORITY_H

#include <common/types.h>
#include <common/states.h>
#include <common/plugins.h>

typedef struct cpuprio_s {
    ullong max_khz;
    ullong min_khz;
    uint high;
    uint low;
    uint idx;
} cpuprio_t;

void mgt_cpufreq_prio_load();

state_t mgt_cpufreq_prio_get_available(ctx_t *c, const cpuprio_t **prio_list);

state_t mgt_cpufreq_prio_get_current(ctx_t *c, cpuprio_t **prio_list);

state_t mgt_cpufreq_prio_get_granularity(ctx_t *c, uint *prio_granularity);

state_t mgt_cpufreq_prio_set_current(ctx_t *c, uint *prio_list);

#endif //MANAGEMENT_PRIORITY_H
