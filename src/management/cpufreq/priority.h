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

#define PRIO_FREQ_MAX      ULLONG_MAX // Max boosted frequency
#define PRIO_FREQ_MAX_NB  (ULLONG_MAX-1) // Max non-boosted frequency

typedef struct cpuprio_s {
    ullong max_khz;
    ullong min_khz;
    uint high;
    uint low;
    uint idx;
} cpuprio_t;

void mgt_cpufreq_prio_load(topology_t *tp);

void mgt_cpufreq_prio_get_api(uint *api);

state_t mgt_cpufreq_prio_init();

state_t mgt_cpufreq_prio_dispose();

staet_t mgt_cpufreq_prio_enable();

staet_t mgt_cpufreq_prio_disable();

state_t mgt_cpufreq_prio_get_available_list(cpuprio_t *prio_list);

state_t mgt_cpufreq_prio_set_available_list(cpuprio_t *prio_list);

state_t mgt_cpufreq_prio_get_current_list(uint *list_idx);

state_t mgt_cpufreq_prio_set_current_list(uint *list_idx);

state_t mgt_cpufreq_prio_set_current(uint cpu, uint prio);

void mgt_cpufreq_prio_data_count(uint *prio_count, uint *idx_count);

void mgt_cpufreq_prio_data_alloc(cpuoprio_t **prio_list, uint **idx_list);

void mgt_cpufreq_prio_data_print(cpuprio_t *prio_list, uint *idx_list, int fd);

void mgt_cpufreq_prio_data_tostr(cpuprio_t *prio_list, uint *idx_list, int fd);

#endif //MANAGEMENT_PRIORITY_H
