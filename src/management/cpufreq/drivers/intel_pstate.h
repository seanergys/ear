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

#ifndef MANAGEMENT_DRIVERS_INTEL_PSTATE_H
#define MANAGEMENT_DRIVERS_INTEL_PSTATE_H

#include <management/cpufreq/cpufreq.h>
#include <management/cpufreq/governor.h>

void mgt_intel_pstate_load(topology_t *tp_in, mgt_ps_driver_ops_t *ops);

state_t mgt_intel_pstate_init();

state_t mgt_intel_pstate_dispose();

// Resets to initial configuration (keeper maintained)
state_t mgt_intel_pstate_reset();

void mgt_intel_pstate_get_freq_details(freq_details_t *details);

state_t mgt_intel_pstate_get_available_list(const ullong **freq_list, uint *freq_count);

state_t mgt_intel_pstate_get_current_list(const ullong **freq_list);

state_t mgt_intel_pstate_get_boost(uint *boost_enabled);

state_t mgt_intel_pstate_set_current_list(uint *freqs_index);

state_t mgt_intel_pstate_set_current(uint freq_index, int cpu);

state_t mgt_intel_pstate_governor_get(uint *governor);

state_t mgt_intel_pstate_governor_get_list(uint *governors);

state_t mgt_intel_pstate_governor_set(uint governor);

state_t mgt_intel_pstate_governor_set_mask(uint governor, cpu_set_t mask);

state_t mgt_intel_pstate_governor_set_list(uint *governors);

// Helper functions that can be shared
int mgt_intel_pstate_governor_is_available(uint governor);

int mgt_intel_pstate_read_cpuinfo(int max, ullong *khz);

int mgt_intel_pstate_read_scaling(int max, ullong *khz);

#endif //MANAGEMENT_DRIVERS_INTEL_PSTATE_H