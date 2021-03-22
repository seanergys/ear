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
#include <library/policies/policy_api.h>
#include <daemon/local_api/eard_api.h>
#include <daemon/powercap/powercap_status.h>
#include <library/policies/policy_state.h>
#include <library/common/externs.h>


typedef unsigned long ulong;
#ifdef EARL_RESEARCH
extern unsigned long ext_def_freq;
#define FREQ_DEF(f) (!ext_def_freq?f:ext_def_freq)
#else
#define FREQ_DEF(f) f
#endif

#if NEW_IMCFREQ
extern ulong policy_max_imcfreq,policy_min_imcfreq;
extern uint dyn_unc;
extern ulong imc_max_khz,imc_min_khz;
extern ulong def_imc_max_khz,def_imc_min_khz;
extern uint dynais_enabled;
extern volatile uint ear_periodic_mode;
ulong avg_to_khz(ulong freq_khz);
static uint imc_freq_selected = 0;
static uint last_cpu_pstate,last_imc_pstate;
static int min_pstate;
static uint ref_imc_pstate;

struct imc_data_t{
	double time;
	double GBS;
	double power;
	ull  	 instructions;
	ull		 cycles;
	ull 	 FLOPS[FLOPS_EVENTS];
};
#define NUM_UNC_FREQ 20
static struct imc_data_t *imc_data;
#define IMC_STEP 100000
#define IMC_RANGE 100000
#define SELECT_CPUFREQ 100
#define SELECT_IMCFREQ 101
#define COMP_IMCREF		 102
static uint min_energy_state = SELECT_CPUFREQ;
static uint imc_dynamic = 0;
static uint min_imc_pstate = 0;
#endif




static ulong req_f;
#define USE_FLOPS 0
#define USE_GBS 1
#define USE_CPI	0
static uint must_increase_imc(uint cpu_pstate,signature_t *sig,double penalty)
{
	double metric,metric_ref;
	double metric_max,metric_min;
	double metric_var,power_var;
	int i;
	ull tflops = 0;
	ull cflops = 0;
	double tflopss,cflopss;
	uint ref_pstate;
	if (ear_periodic_mode == PERIODIC_MODE_OFF) ref_pstate = min_pstate;
	else ref_pstate = cpu_pstate;
	verbose(2,"Validating IMC freq selection: cpu_pstate %u imc_pstate %u",ref_pstate,ref_imc_pstate);
	power_var = (imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].power - sig->DC_power)/imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].power; 
	if (ear_periodic_mode == PERIODIC_MODE_OFF){
		metric_max = imc_data[min_pstate*NUM_UNC_FREQ+ref_imc_pstate].time + (penalty* imc_data[min_pstate*NUM_UNC_FREQ+ref_imc_pstate].time);
		metric = sig->time;
		metric_var = (sig->time-imc_data[min_pstate*NUM_UNC_FREQ+ref_imc_pstate].time)/imc_data[min_pstate*NUM_UNC_FREQ+ref_imc_pstate].time;
		verbose(2,"Using time as reference : Current %.3lf Max %.3lf energy_saving %.2lf (%.2lf - %.2lf)",metric,metric_max,(power_var-metric_var)*100,power_var,metric_var);
		return (metric > metric_max);
	}else{
		/* We were using ref_pstate = min_pstate */
		#if USE_CPI
		double CPI;
		CPI = (double)imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].cycles/(double)imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].instructions;
		metric_max = CPI + CPI*penalty;
		metric = sig->CPI;
		metric_var = (CPI-metric)/CPI;
		verbose(2,"Using CPI as reference : Current %.3lf Max %.3lf energy_saving %.2lf (%.2lf - %.2lf)",metric,metric_max,(power_var-metric_var)*100,power_var,metric_var);
		return (metric > metric_max);	
		#endif
		#if USE_GBS
		metric_min = imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].GBS - (penalty* imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].GBS);
		metric = sig->GBS;
		metric_var = (imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].GBS-sig->GBS)/imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].GBS;
		verbose(2,"Using GBS as reference : Current %.3lf Min %.3lf energy_saving %.2lf (%.2lf - %.2lf)",metric,metric_min,(power_var-metric_var)*100,power_var,metric_var);
		return (metric < metric_min);
		#endif
		#if USE_FLOPS
		for (i=0;i<FLOPS_EVENTS;i++) tflops += imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].FLOPS[i];	
		for (i=0;i<FLOPS_EVENTS;i++) cflops += sig->FLOPS[i];	
		tflopss = (double)tflops / (imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].time*1000000.0);
		cflopss = (double)cflops / (sig->time*1000000.0);
		metric_min =  tflopss - (penalty* tflopss);
		metric = cflopss;
		metric_var = (tflopss-cflopss)/tflopss;
		verbose(2,"Using FLOPS as reference : Current %.3lf Min %.3lf energy_saving %.2lf (%.2lf - %.2lf)",metric,metric_min,(power_var-metric_var)*100,power_var,metric_var);
		return (metric < metric_min);
		#endif
	}
}

static void copy_imc_data_from_signature(uint cpu_pstate,uint imc_pstate,signature_t *s)
{
	int i;
	ullong tfops = 0;
	float flops = 0;
  imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].time = s->time;
  imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].power = s->DC_power;
  imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].GBS = s->GBS;
  imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].instructions = s->instructions;
  imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].cycles = s->cycles;
  memcpy(imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].FLOPS , s->FLOPS,sizeof(ull)*FLOPS_EVENTS);
	for (i=0;i<FLOPS_EVENTS;i++) tfops += s->FLOPS[i];
	flops = (float)tfops/(s->time*1000000000.0);
	verbose(2,"IMC data: time %.3lf power %2.lf GBS %.2lf I/s %.2f flops %.2f",imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].time,imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].power,imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].GBS,s->instructions/(s->time*1000000000.0),flops);
}

state_t policy_init(polctx_t *c)
{
	if (c!=NULL){ 
		#if NEW_IMCFREQ
		imc_data = calloc(c->num_pstates*NUM_UNC_FREQ,sizeof(struct imc_data_t));
		#endif
		return EAR_SUCCESS;
	} else return EAR_ERROR;
}

state_t policy_loop_init(polctx_t *c,loop_id_t *l)
{
	if (c!=NULL){
    projection_reset(c->num_pstates);
		return EAR_SUCCESS;
	}else{
		return EAR_ERROR;
	}
}

state_t policy_loop_end(polctx_t *c,loop_id_t *l)
{
    if ((c!=NULL) && (c->reset_freq_opt))
    {
        *(c->ear_frequency) = eards_change_freq(FREQ_DEF(c->app->def_freq));
    }
	return EAR_SUCCESS;
}


state_t policy_apply(polctx_t *c,signature_t *sig,ulong *new_freq,int *ready)
{
	signature_t *my_app;
	int i;
	unsigned int ref;
	double power_proj,time_proj,energy_proj,best_solution,energy_ref;
	double power_ref,time_ref,max_penalty,time_max;
	ulong imc_pstate;

  ulong best_freq,best_pstate,freq_ref,eff_f;
	ulong curr_freq,nominal;
	ulong curr_pstate,def_pstate,def_freq,from;
	state_t st;
	uint power_status;
	double sig_vpi,node_sig_vpi;
	


	if ((c == NULL) || (c->app == NULL)){
    *ready=EAR_POLICY_CONTINUE;
    return EAR_ERROR;
	}

	if (!are_signatures_ready(lib_shared_region,sig_shared_region)){
		*new_freq = *(c->ear_frequency);
		*ready = EAR_POLICY_TRY_AGAIN;
		verbose(2,"Node signature not ready, selecting %lu CPU f",*new_freq);
		return EAR_SUCCESS;
	}
	verbose(2, "MR[%d] Node signatures ready",masters_info.my_master_rank);
	my_app = calloc(1,sizeof(signature_t));
	signature_copy(my_app,sig);
	compute_total_node_instructions(lib_shared_region,sig_shared_region,&my_app->instructions);	
	compute_total_node_flops(lib_shared_region,sig_shared_region,my_app->FLOPS);
	compute_total_node_cycles(lib_shared_region,sig_shared_region,&my_app->cycles);
	compute_vpi(&sig_vpi,sig);
	compute_vpi(&node_sig_vpi,my_app);
	verbose(2,"MR[%d] SIGNATURE CPI %.3lf VPI %.3lf/Node CPI %.3lf VPI %.3lf",masters_info.my_master_rank,sig->CPI,sig_vpi,(double)my_app->cycles/(double)my_app->instructions,node_sig_vpi);
	my_app->CPI = (double)my_app->cycles/(double)my_app->instructions;
  if (c->use_turbo) min_pstate=0;
  else min_pstate=frequency_closest_pstate(c->app->max_freq);

	nominal=frequency_pstate_to_freq(min_pstate);



	// Default values

	max_penalty=c->app->settings[0];
	def_freq=FREQ_DEF(c->app->def_freq);
	def_pstate=frequency_closest_pstate(def_freq);

		// This is the frequency at which we were running
  #ifdef POWERCAP
  if (c->pc_limit > 0){
			curr_freq=frequency_closest_high_freq(my_app->avg_f,1);
	}else{
			curr_freq=*(c->ear_frequency);
	}
  #else
  curr_freq=*(c->ear_frequency);
  #endif

	curr_pstate = frequency_closest_pstate(curr_freq);
	eff_f=frequency_closest_high_freq(my_app->avg_f,1);

  #if NEW_IMCFREQ
  if (dyn_unc){
			imc_pstate = (def_imc_max_khz - policy_max_imcfreq)/IMC_STEP;
			verbose(2,"CPU pstate %lu IMC freq %lu IMC pstate %lu",curr_pstate,my_app->sig_ext->imc_freq,imc_pstate);
			copy_imc_data_from_signature(curr_pstate,imc_pstate,my_app);
  }
	if ((min_energy_state == SELECT_CPUFREQ) || (dyn_unc == 0)){
		if (dyn_unc){
			*ready = EAR_POLICY_TRY_AGAIN;
		}else *ready=EAR_POLICY_READY;
		verbose(2,"SELECT_CPUFREQ state");
	#else
		*ready=EAR_POLICY_READY;
	#endif
		last_cpu_pstate = curr_pstate;
		last_imc_pstate = imc_pstate;


		// If is not the default P_STATE selected in the environment, a projection
		// is made for the reference P_STATE in case the coefficents were available.
		// NEW: This code is new because projections from lower pstates are reporting bad cpu freqs 
		if (curr_freq != def_freq){
			*ready = EAR_POLICY_TRY_AGAIN;
			*new_freq = def_freq;
			if (dyn_unc){
				policy_max_imcfreq = imc_max_khz;
      	policy_min_imcfreq = policy_max_imcfreq - IMC_RANGE;
			}
			return EAR_SUCCESS;
		}
		if (curr_freq != def_freq) // Use configuration when available
		{
		if (projection_available(curr_pstate,def_pstate)==EAR_SUCCESS)
		{
				st=project_power(my_app,curr_pstate,def_pstate,&power_ref);
				st=project_time(my_app,curr_pstate,def_pstate,&time_ref);
				best_freq=def_freq;
        verbose(2,"Comp.Ref:projecting from %lu to %lu\t time: %.2lf\t power: %.2lf", curr_pstate, def_pstate, time_ref, power_ref);
		}
		else
		{
				time_ref=my_app->time;
				power_ref=my_app->DC_power;
				best_freq=curr_freq;
				verbose(2,"Comp.Ref: Using time %lf power %lf freq %lu",time_ref,power_ref,best_freq);
		}
		}
		// If it is the default P_STATE selected in the environment, then a projection
		// is not needed, so the signature will be enough as a reference. 
		else
		{ // we are running at default frequency , signature is our reference
			if (curr_freq == nominal){
				/* And we are the nominal freq */
				time_ref=my_app->time;
				power_ref=my_app->DC_power;
				best_freq=curr_freq;
				verbose(2,"Comp.Ref: Using time %lf power %lf freq %lu",time_ref,power_ref,best_freq);
			}else{
				/* Nominal is the reference , if projections are ready, we project time and power */
				debug("current_freq is not the nominal");

				if (projection_available(curr_pstate,min_pstate) == EAR_SUCCESS){
					project_power(my_app,curr_pstate,min_pstate,&power_ref);
					project_time(my_app,curr_pstate,min_pstate,&time_ref);
					best_freq=nominal;
					verbose(2,"Comp.Ref:projecting to nominal\t time: %.2lf\t power: %.2lf", time_ref, power_ref);
				}else{
        	time_ref=my_app->time;
        	power_ref=my_app->DC_power;
        	best_freq=curr_freq;
					verbose(2,"Comp.Ref: Using time %lf power %lf freq %lu",time_ref,power_ref,best_freq);
				}
			}

		}
		energy_ref=power_ref*time_ref;
		best_solution=energy_ref;

	// We compute the maximum performance loss
	time_max = time_ref + (time_ref * max_penalty);

  verbose(2,"Max_freq set to %lu min_pstate = %d nominal %lu curr_frequency %lu curr_pstate %lu time_max: %.2lf",
		c->app->max_freq,min_pstate,nominal,curr_freq,curr_pstate,time_max);

	verbose(2,"Policy Signature (CPI=%lf GBS=%lf Power=%lf Time=%lf TPI=%lf)",
		my_app->CPI,my_app->GBS,my_app->DC_power,my_app->time,my_app->TPI);

	// MIN_ENERGY_TO_SOLUTION ALGORITHM
	// Calcular el min_pstate que este dentro del limite
	from = curr_pstate;
	for (i = min_pstate; i < c->num_pstates;i++)
	{
		debug("testing projection between P%d and P%d", curr_pstate, i);
		if (projection_available(from,i)==EAR_SUCCESS)
		{
			debug("ok");
			st=project_power(my_app,from,i,&power_proj);
			st=project_time(my_app,from,i,&time_proj);
			projection_set(i,time_proj,power_proj);
			energy_proj=power_proj*time_proj;

      verbose(2,"projected from %lu to %d\t time: %.2lf\t power: %.2lf energy: %.2lf",
				from, i, time_proj, power_proj, energy_proj);
			
			if ((energy_proj < best_solution) && (time_proj < time_max)) {
				best_freq = frequency_pstate_to_freq(i);
        verbose(2,"new best solution found %lu",best_freq);
				best_solution = energy_proj;
				best_pstate=i;
			}
		}
		*new_freq=best_freq;
		#if NEW_IMCFREQ	
		if (dyn_unc){
    	policy_max_imcfreq = imc_max_khz;
			policy_min_imcfreq = policy_max_imcfreq - IMC_RANGE;
			min_imc_pstate = imc_pstate;
			if (*new_freq != curr_freq) min_energy_state = COMP_IMCREF;
			else min_energy_state = SELECT_IMCFREQ;
		}else{
			min_energy_state = SELECT_CPUFREQ;
		}
		#endif
	}
	
	#if NEW_IMCFREQ
	}else if ((min_energy_state == SELECT_IMCFREQ) && (dyn_unc)){
		verbose(2,"SELECT_IMCFREQ state: nominal %d - last CPU %u,IMC %u -- current CPU %lu, IMC %lu",min_pstate,last_cpu_pstate,last_imc_pstate,curr_pstate,imc_pstate);
		*new_freq = frequency_pstate_to_freq(curr_pstate);
		uint increase_imc = must_increase_imc(curr_pstate,my_app,max_penalty);
		//time_max = imc_data[min_pstate*NUM_UNC_FREQ+last_imc_pstate].time + (max_penalty* imc_data[min_pstate*NUM_UNC_FREQ+last_imc_pstate].time);
		/* If we are over the limit, we increase the IMC */
		if (increase_imc){
			verbose(2,"%sWarning, passing the limit",COL_RED);
			policy_max_imcfreq = ear_min(policy_max_imcfreq + IMC_STEP,imc_max_khz);	
      policy_min_imcfreq = policy_max_imcfreq - IMC_RANGE;
			verbose(2,"Selecting %lu - %lu and policy ready%s",policy_max_imcfreq,policy_min_imcfreq,COL_CLR);
			*ready = EAR_POLICY_READY;
			min_energy_state = SELECT_CPUFREQ;
		}else{
			policy_max_imcfreq = ear_max(policy_max_imcfreq - IMC_STEP,imc_min_khz-IMC_RANGE);
			policy_min_imcfreq = policy_max_imcfreq - IMC_RANGE;
			verbose(2,"%sTrying a lower IMC freq %lu-%lu%s",COL_GRE,policy_max_imcfreq,policy_min_imcfreq,COL_CLR);
			if (policy_min_imcfreq > imc_min_khz) *ready = EAR_POLICY_TRY_AGAIN;
			else{ 
				*ready = EAR_POLICY_READY;
				min_energy_state = SELECT_CPUFREQ;
			}
			
		}
		
	}else if (min_energy_state == COMP_IMCREF){
		*new_freq = curr_freq;
		ref_imc_pstate = imc_pstate;
		verbose(2,"COMP_IMCREF cpufreq %lu imc %lu-%lu",*new_freq,policy_max_imcfreq,policy_min_imcfreq);
		min_energy_state = SELECT_IMCFREQ;
		*ready = EAR_POLICY_TRY_AGAIN;
	}
	#endif
	verbose(2,"Next min_energy state %u next policy state %u",min_energy_state,*ready);
	return EAR_SUCCESS;
}


state_t policy_ok(polctx_t *c,signature_t *curr_sig,signature_t *last_sig,int *ok)
{
	double energy_last, energy_curr;
	uint power_status,next_status;
	ulong eff_f;

	if ((c==NULL) || (curr_sig==NULL) || (last_sig==NULL)) return EAR_ERROR;

	/* This part is generic */

	if (curr_sig->def_f==last_sig->def_f){ 
		*ok=1;
		return EAR_SUCCESS;
	}
	energy_last = last_sig->time*last_sig->DC_power;
	energy_curr = curr_sig->time * curr_sig->DC_power;

	if ((energy_curr<energy_last)&&(curr_sig->time<(last_sig->time*c->app->settings[0]))) *ok=1;
	else *ok=0;
	return EAR_SUCCESS;
}

state_t  policy_get_default_freq(polctx_t *c, ulong *freq_set)
{
		if (c!=NULL){
			*freq_set=c->app->max_freq;
		}else EAR_ERROR;
		return EAR_SUCCESS;
}

state_t policy_max_tries(polctx_t *c,int *intents)
{
  *intents = 1;
  return EAR_SUCCESS;
}

