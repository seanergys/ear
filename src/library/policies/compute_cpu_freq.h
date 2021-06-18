#ifndef _COMP_CPUFREQ_H
#define _COMP_CPUFREQ_H

#include <common/states.h>
#include <common/types/signature.h>

/**
 * This function selects a new cpu freq based on min_time_to_solution policy.
 * Read wiki for more info about policies.  */
state_t compute_cpu_freq_min_time(signature_t *my_app, int min_pstate, double time_ref,
        double min_eff_gain, ulong curr_pstate, ulong best_pstate, ulong best_freq, ulong def_freq, ulong *newf);

#endif
