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
#include <common/hardware/topology.h>

// PRIORITY is a subsystem of CPUFREQ. It can take the control over the
// frequency by using the enable function. Also, you can set the maximum
// and minimum frequency per priority.

#define PRIO_FREQ_MAX      25500000 // Max boosted frequency
#define PRIO_FREQ_MAX_NB  (PRIO_FREQ_MAX-1) // Max non-boosted frequency
#define PRIO_HIGH         (UINT_MAX-2)
#define PRIO_LOW          (UINT_MAX-3)

typedef struct cpuprio_s {
    ullong max_khz;
    ullong min_khz;
    uint high;
    uint low;
    uint idx;
} cpuprio_t;

typedef struct mgt_prio_ops_s
{
    void    (*get_api) (uint *api);
    state_t (*init)    ();
    state_t (*dispose) ();
    state_t (*enable)  ();
    state_t (*disable) ();
    int     (*is_enabled) ();
    state_t (*get_available_list) (cpuprio_t *prio_list);
    state_t (*set_available_list) (cpuprio_t *prio_list);
    state_t (*get_current_list)   (uint *list_idx);
    state_t (*set_current_list)   (uint *list_idx);
    state_t (*set_current)        (uint idx, int cpu);
    void    (*data_count)         (uint *prio_count, uint *idx_count);
} mgt_prio_ops_t;

/* PRIORITY is a CPUFREQ subsystem, loading CPUFREQ also loads PRIORITY automatically. */
void mgt_cpufreq_prio_load(topology_t *tp, int eard);

void mgt_cpufreq_prio_get_api(uint *api);

state_t mgt_cpufreq_prio_init();

state_t mgt_cpufreq_prio_dispose();
/* Enables the PRIORITY subsystem, taking the control of the frequency. */
state_t mgt_cpufreq_prio_enable();

state_t mgt_cpufreq_prio_disable();

int mgt_cpufreq_prio_is_enabled();

state_t mgt_cpufreq_prio_get_available_list(cpuprio_t *prio_list);

state_t mgt_cpufreq_prio_set_available_list(cpuprio_t *prio_list);

state_t mgt_cpufreq_prio_get_current_list(uint *list_idx);

state_t mgt_cpufreq_prio_set_current_list(uint *list_idx);

state_t mgt_cpufreq_prio_set_current(uint idx, int cpu);

void mgt_cpufreq_prio_data_count(uint *prio_count, uint *idx_count);

void mgt_cpufreq_prio_data_alloc(cpuprio_t **prio_list, uint **idx_list);
    
void mgt_cpufreq_prio_data_print(cpuprio_t *prio_list, uint *idx_list, int fd);

void mgt_cpufreq_prio_data_tostr(cpuprio_t *prio_list, uint *idx_list, char *buffer, int length);

// Short names
#define mgt_cpuprio_get_api            mgt_cpufreq_prio_get_api
#define mgt_cpuprio_enable             mgt_cpufreq_prio_enable
#define mgt_cpuprio_disable            mgt_cpufreq_prio_disable
#define mgt_cpuprio_is_enabled         mgt_cpufreq_prio_is_enabled
#define mgt_cpuprio_get_available_list mgt_cpufreq_prio_get_available_list
#define mgt_cpuprio_set_available_list mgt_cpufreq_prio_set_available_list
#define mgt_cpuprio_get_current_list   mgt_cpufreq_prio_get_current_list
#define mgt_cpuprio_set_current_list   mgt_cpufreq_prio_set_current_list
#define mgt_cpuprio_set_current        mgt_cpufreq_prio_set_current
#define mgt_cpuprio_data_count         mgt_cpufreq_prio_data_count
#define mgt_cpuprio_data_alloc         mgt_cpufreq_prio_data_alloc

#endif //MANAGEMENT_PRIORITY_H
