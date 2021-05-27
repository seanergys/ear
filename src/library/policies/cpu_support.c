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

//#define SHOW_DEBUGS 1
#define _GNU_SOURCE
#include <sched.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <common/config.h>
#include <common/states.h>
#include <common/math_operations.h>

#include <common/output/verbose.h>
#include <common/hardware/topology.h>
#include <management/cpufreq/frequency.h>
#include <common/types/projection.h>
#include <library/common/externs.h>
#include <library/common/verbose_lib.h>
#include <library/policies/policy_ctx.h>
#include <library/api/clasify.h>
#include <library/policies/cpu_support.h>
#include <library/policies/mpi_stats_support.h>

void process_avg_cpu_freq(cpu_set_t *aff,ulong *cpuflist,ulong *avgcpu)
{
	int i = 0;
	ulong cpuftotal = 0;
	int total = 0;
	for (i=0; i<mtopo.cpu_count;i++){
		if (CPU_ISSET(i,aff)){ 
			total ++;
			cpuftotal += cpuflist[i];
		}
	}
	*avgcpu = cpuftotal / total; 
	
}

state_t compute_reference(polctx_t *c,signature_t *my_app,ulong *curr_freq,ulong *def_freq,ulong *freq_ref,double *time_ref,
        double *power_ref)
{
    ulong def_pstate, curr_pstate;
    state_t st;

    def_pstate = frequency_closest_pstate(def_freq[0]);
    curr_pstate = frequency_closest_pstate(curr_freq[0]);
    if (curr_freq[0] != def_freq[0]) // Use configuration when available
    {
        verbose_master(3, "curr_freq[0] != def_freq[0] %lu != %lu", curr_freq[0], def_freq[0]);
        if (projection_available(curr_pstate,def_pstate)==EAR_SUCCESS)
        {
            st = project_power(my_app,curr_pstate,def_pstate,power_ref);
            st = project_time(my_app,curr_pstate,def_pstate,time_ref);
            freq_ref[0] = def_freq[0];
        } else {
            *time_ref = my_app->time;
            *power_ref = my_app->DC_power;
            freq_ref[0] = curr_freq[0];
        }
    }else{ 
        /* And we are the nominal freq */
        *time_ref = my_app->time;
        *power_ref = my_app->DC_power;
        freq_ref[0] = curr_freq[0];
    }
    return EAR_SUCCESS;
}

state_t compute_cpu_freq_min_energy(polctx_t *c, signature_t *my_app, ulong freq_ref, double time_ref, double power_ref,
        double penalty, ulong curr_pstate, ulong minp, ulong maxp, ulong *newf)
{
    state_t st;
    double vpi;
    double energy_ref,best_solution,time_max;
    double power_proj,time_proj,energy_proj;
    ulong i,from,best_pstate;
    ulong best_freq = freq_ref;
    energy_ref=power_ref*time_ref;
    best_solution=energy_ref;
    time_max = time_ref + (time_ref * penalty);	
    from = curr_pstate;	
    compute_vpi(&vpi,my_app);
    verbose_master(3, "CPUfreq algorithm for min_energy timeref %lf powerref %lf energy %lf F=%lu VPI %lf",time_ref,power_ref,
            energy_ref,best_freq,vpi);
    for (i = minp; i < maxp;i++)
    {
        if (projection_available(from,i) == EAR_SUCCESS)
        {
            st = project_power(my_app,from,i,&power_proj);
            st = project_time(my_app,from,i,&time_proj);
            projection_set(i,time_proj,power_proj);
            energy_proj = power_proj*time_proj;

            verbose_master(3,"projected from %lu to %lu\t time: %.2lf\t power: %.2lf energy: %.2lf",
                    from, i, time_proj, power_proj, energy_proj);

            if ((energy_proj < best_solution) && (time_proj < time_max)) {
                best_freq = frequency_pstate_to_freq(i);
                verbose(3,"new best solution found %lu",best_freq);
                best_solution = energy_proj;
                best_pstate = i;
            }
        }
    }
    *newf = best_freq;
    return EAR_SUCCESS;
}

state_t compute_cpu_freq_min_time(signature_t *my_app, int min_pstate, double time_ref,
        double min_eff_gain, ulong curr_pstate, ulong best_pstate, ulong best_freq, ulong def_freq, ulong *newf){
    int i;
    uint try_next;
    double time_current, power_proj, time_proj, freq_gain, perf_gain, vpi;
    state_t st;
    ulong freq_ref;

    compute_vpi(&vpi,my_app);
    // error al compilar con verbose_master :( error: identifier "masters_info" is undefined
    verbose_master(3, "CPUfreq algorithm for min_time. timeref %lf; pstate %lu; F %lu; deffreq %lu; VPI %lf",
            time_ref, curr_pstate, best_freq, def_freq, vpi);

    try_next = 1;
    i = best_pstate - 1;
    time_current = time_ref;
    while(try_next && (i >= min_pstate))
    {
        if (projection_available(curr_pstate,i)==EAR_SUCCESS)
        {
            verbose_master(3, "Looking for pstate %d",i);
            st = project_power(my_app, curr_pstate, i, &power_proj);
            st = project_time(my_app, curr_pstate, i, &time_proj);
            projection_set(i, time_proj, power_proj);
            freq_ref = frequency_pstate_to_freq(i);
            freq_gain = min_eff_gain * (double)(freq_ref - best_freq) / (double)best_freq;
            perf_gain = (time_current - time_proj) / time_current;
            verbose_master(3, "%lu to %d time %lf proj time %lf freq gain %lf perf_gain %lf", curr_pstate, i, my_app->time, time_proj,
                    freq_gain,perf_gain);
            // OK
            if (perf_gain>=freq_gain)
            {
                verbose(3, "New best solution found: Best freq: %lu; Best pstate: %d; Current time ref: %lf",
                        freq_ref, i, time_proj);
                best_freq=freq_ref;
                best_pstate=i;
                time_current = time_proj;
                i--;
            }
            else
            {
                try_next = 0;
            }
        } // Projections available
        else{
            try_next=0;
        }
    }
    /* Controlar la freq por power cap, si capado poner GREEDY, gestionar req-f */
    if (best_freq<def_freq) best_freq=def_freq;
    *newf=best_freq;
    verbose_master(3, "*new_freq=%lu", *newf);

    return EAR_SUCCESS;
}

uint cpu_supp_try_boost_cpu_freq(int nproc, uint *critical_path, ulong *freqs, int min_pstate){
    uint belongs_to_cp = critical_path[nproc];
    int pstate = frequency_closest_pstate(freqs[nproc]);
    verbosen_master(2, "Checking for boost process %d: Critical path ? %u | pstate ? %d | min_pstate ? %d ",
            nproc, belongs_to_cp, pstate, min_pstate);
    if (belongs_to_cp && pstate == min_pstate) {
        freqs[nproc] = frequency_pstate_to_freq(0);
        verbose_master(2, "%sOK%s", COL_GRE, COL_CLR);
        return 1;
    }
    verbose_master(2, "%sFALSE%s", COL_RED, COL_CLR);
    return 0;
}

int signatures_different(signature_t *s1, signature_t *s2,float p)
{
	if (!equal_with_th(s1->CPI,s2->CPI,p) || !equal_with_th(s1->GBS, s2->GBS,p)){ 
		if ((s1->GBS > GBS_BUSY_WAITING) || (s2->GBS > GBS_BUSY_WAITING)) return 1;
	}
	return 0;	
}
