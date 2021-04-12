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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <common/config.h>
#include <common/states.h>
#include <common/output/verbose.h>
#include <management/cpufreq/frequency.h>
#include <common/types/projection.h>
#include <library/common/externs.h>
#include <library/common/verbose_lib.h>
#include <library/policies/policy_ctx.h>




state_t compute_reference(polctx_t *c,signature_t *my_app,ulong *curr_freq,ulong *def_freq,ulong *freq_ref,double *time_ref,double *power_ref)
{
	ulong def_pstate,curr_pstate;
	state_t st;

	def_pstate = frequency_closest_pstate(def_freq[0]);
  curr_pstate = frequency_closest_pstate(curr_freq[0]);
	if (curr_freq[0] != def_freq[0]) // Use configuration when available
	{
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

state_t compute_cpu_freq_min_energy(polctx_t *c,signature_t *my_app,ulong freq_ref,double time_ref,double power_ref,double penalty,ulong curr_pstate,ulong  minp,ulong maxp,ulong *newf)
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
	verbose_master(3,"CPUfreq algorithm for min_energy timeref %lf powerref %lf energy %lf F=%lu VPI %lf",time_ref,power_ref,energy_ref,best_freq,vpi);
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
