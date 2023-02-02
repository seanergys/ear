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

#ifndef MANAGEMENT_INTEL_PSTATE_DRIVERS_LINUX_CPUFREQ_H
#define MANAGEMENT_INTEL_PSTATE_DRIVERS_LINUX_CPUFREQ_H

#include <management/cpufreq/cpufreq.h>
#include <management/cpufreq/governor.h>

void mgt_intel_pstate_linux_load(topology_t *tp_in, mgt_ps_driver_ops_t *ops);

state_t mgt_intel_pstate_linux_init(ctx_t *c);

state_t mgt_intel_pstate_linux_dispose(ctx_t *c);

state_t mgt_intel_pstate_linux_get_available_list(ctx_t *c, const ullong **freq_list, uint *freq_count);

state_t mgt_intel_pstate_linux_get_current_list(ctx_t *c, const ullong **freq_list);

state_t mgt_intel_pstate_linux_get_boost(ctx_t *c, uint *boost_enabled);

state_t mgt_intel_pstate_linux_set_current_list(ctx_t *c, uint *freqs_index);

state_t mgt_intel_pstate_linux_set_current(ctx_t *c, uint freq_index, int cpu);

state_t mgt_intel_pstate_linux_governor_get(ctx_t *c, uint *governor);

state_t mgt_intel_pstate_linux_governor_get_list(ctx_t *c, uint *governors);

state_t mgt_intel_pstate_linux_governor_set(ctx_t *c, uint governor);

state_t mgt_intel_pstate_linux_governor_set_mask(ctx_t *c, uint governor, cpu_set_t mask);

state_t mgt_intel_pstate_linux_governor_set_list(ctx_t *c, uint *governors);

#endif //MANAGEMENT_INTEL_PSTATE_DRIVERS_LINUX_CPUFREQ
