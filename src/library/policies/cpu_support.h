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
#ifndef _CPU_SUPPORT_H
#define _CPU_SUPPORT_H

#define LET_TURBO 1

state_t compute_reference(polctx_t *c, signature_t *my_app, ulong *curr_freq, ulong *def_freq, ulong *freq_ref, double *time_ref, double *power_ref);
state_t compute_cpu_freq_min_energy(polctx_t *c,signature_t *my_app,ulong freq_ref,double time_ref,double power_ref,double penalty,ulong curr_pstate,ulong  minp,ulong maxp,ulong *newf);

/**
 * This function selects a new cpu freq based on min_time_to_solution policy.
 * Read wiki for more info about policies.  */
state_t compute_cpu_freq_min_time(signature_t *my_app, int min_pstate, double time_ref,
        double min_eff_gain, ulong curr_pstate, ulong best_pstate, ulong best_freq, ulong def_freq, ulong *newf);

#if LET_TURBO
state_t try_boost_cpu_freq(int freq_per_core, int nproc, int min_mpi, ulong *freqs, int min_pstate, int use_turbo);
#endif

#endif

