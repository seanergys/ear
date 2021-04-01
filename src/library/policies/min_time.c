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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
//#define SHOW_DEBUGS 1
#include <common/config.h>
#include <common/states.h>
#include <common/output/verbose.h>
#include <management/cpufreq/frequency.h>
#include <common/types/projection.h>
#include <daemon/local_api/eard_api.h>
#include <library/policies/policy_api.h>
#include <common/math_operations.h>
#include <library/common/externs.h>
#include <common/system/time.h>
#include <daemon/powercap/powercap_status.h>
#include <library/policies/policy_state.h>
#include <library/policies/gpu_support.h>
#include <library/policies/imc_policy_support.h>
#include <library/policies/mpi_stats_support.h>
#include <library/common/verbose_lib.h>

static timestamp pol_time_init;
static ulong req_f;

typedef unsigned long ulong;


#if NEW_IMCFREQ
extern ulong policy_max_imcfreq,policy_min_imcfreq;
extern uint dyn_unc;
extern ulong imc_max_khz,imc_min_khz;
extern ulong def_imc_max_khz,def_imc_min_khz;
extern int my_globalrank;
extern uint imc_set_by_hw;
extern double imc_extra_th;
ulong avg_to_khz(ulong freq_khz);
#define IMC_STEP 100000
#define IMC_RANGE 100000
#define SELECT_CPUFREQ 100
#define SELECT_IMCFREQ 101
#define COMP_IMCREF		 102
static imc_data_t *imc_data;
static uint min_time_state = SELECT_CPUFREQ;
static uint last_cpu_pstate,last_imc_pstate, ref_imc_pstate;
static uint max_imc_pstate;
#endif

#ifdef EARL_RESEARCH
extern unsigned long ext_def_freq;

#define FREQ_DEF(f) (!ext_def_freq?f:ext_def_freq)
#else
#define FREQ_DEF(f) f
#endif

static polsym_t gpus;

state_t policy_init(polctx_t *c)
{
    if (c!=NULL){ 
        mpi_app_init(c);
#if NEW_IMCFREQ
        imc_data = calloc(c->num_pstates*NUM_UNC_FREQ,sizeof(imc_data_t));
#endif
        if (policy_gpu_init_ctx(c) != EAR_SUCCESS){
            verbose_master(2,"Error loading GPU context");
        }
        if (c->num_gpus){
            if (policy_gpu_load(c->app,&gpus) != EAR_SUCCESS){
                verbose_master(2,"Error loading GPU policy");
            }
            if (gpus.init != NULL) gpus.init(c);
        }

        sig_shared_region[my_node_id].mpi_info.mpi_time=0;
        sig_shared_region[my_node_id].mpi_info.total_mpi_calls=0;
        sig_shared_region[my_node_id].mpi_info.exec_time=0;
        sig_shared_region[my_node_id].mpi_info.perc_mpi=0;


        return EAR_SUCCESS;
    }else return EAR_ERROR;
}


state_t policy_end(polctx_t *c)
{
	return	mpi_app_end(c);	
}

state_t policy_loop_init(polctx_t *c,loop_id_t *loop_id)
{
    if (c!=NULL){ 
        projection_reset(c->num_pstates);
        return EAR_SUCCESS;
    }else{
        return EAR_ERROR;
    }
}

state_t policy_loop_end(polctx_t *c,loop_id_t *loop_id)
{
    if ((c!=NULL) && (c->reset_freq_opt))
    {
        // Use configuration when available
        *(c->ear_frequency) = eards_change_freq(FREQ_DEF(c->app->def_freq));
    }
    return EAR_SUCCESS;
}



// This is the main function in this file, it implements power policy
state_t policy_apply(polctx_t *c,signature_t *sig,node_freqs_t *freqs,int *ready)
{
    ulong *new_freq=freqs->cpu_freq;
    signature_t *my_app;
    int i, min_pstate;
    unsigned int ref,try_next;
    double freq_gain,perf_gain,min_eff_gain;
    double power_proj,time_proj;
    double power_ref,time_ref,time_current;
    ulong best_freq,best_pstate,freq_ref;
    ulong curr_freq;
    ulong curr_pstate,def_pstate,def_freq, imc_pstate;
    state_t st;
    double sig_vpi, node_sig_vpi;
    // my_app=sig;
    int gready;
    uint sid = 0;

    *ready=EAR_POLICY_READY;

    if (c==NULL) return EAR_ERROR;
    if (c->app==NULL) return EAR_ERROR;

    if (!are_signatures_ready(lib_shared_region,sig_shared_region)){
        *new_freq = *(c->ear_frequency);
        *ready = EAR_POLICY_TRY_AGAIN;
        verbose(2,"Node signature not ready, selecting %lu CPU f",*new_freq);
        return EAR_SUCCESS;
    }
    verbose(2, "MR[%d] Node signatures ready",my_globalrank);

    my_app = calloc(1,sizeof(signature_t));
    signature_copy(my_app,sig);

    compute_total_node_instructions(lib_shared_region,sig_shared_region,&my_app->instructions);	
    compute_total_node_flops(lib_shared_region,sig_shared_region,my_app->FLOPS);
    compute_total_node_cycles(lib_shared_region,sig_shared_region,&my_app->cycles);
    compute_vpi(&sig_vpi,sig);
    compute_vpi(&node_sig_vpi,my_app);
    verbose(2,"MR[%d] SIGNATURE CPI %.3lf VPI %.3lf/Node CPI %.3lf VPI %.3lf",my_globalrank,sig->CPI,sig_vpi,(double)my_app->cycles/(double)my_app->instructions,node_sig_vpi);
    my_app->CPI = (double)my_app->cycles/(double)my_app->instructions;

#if POWERCAP
    if (is_powercap_set(&c->app->pc_opt)){ 
        verbose(1,"Powercap is set to %uWatts",get_powercapopt_value(&c->app->pc_opt));
    }
#endif

    if (gpus.node_policy_apply != NULL){
        /* Basic support for GPUS */
        gpus.node_policy_apply(c,sig,freqs,&gready);
    }


    if (c->use_turbo) min_pstate=0;
    else min_pstate=frequency_closest_pstate(c->app->max_freq);

    // Default values

    min_eff_gain=c->app->settings[0];
    def_freq=FREQ_DEF(c->app->def_freq);
    def_pstate=frequency_closest_pstate(def_freq);

    // This is the frequency at which we were running
#ifdef POWERCAP
    curr_freq=frequency_closest_high_freq(my_app->avg_f,1);
#else
    curr_freq=*(c->ear_frequency);
#endif
    curr_pstate = frequency_closest_pstate(curr_freq);

#if NEW_IMCFREQ
    if (dyn_unc){
        imc_pstate = (def_imc_max_khz - avg_to_khz(my_app->sig_ext->imc_freq))/IMC_STEP;
        verbose(2,"CPU pstate %lu IMC freq %lu IMC pstate %lu",curr_pstate,my_app->sig_ext->imc_freq,imc_pstate);
        copy_imc_data_from_signature(imc_data, curr_pstate, imc_pstate, my_app);
    }
    if ((min_time_state == SELECT_CPUFREQ) || (dyn_unc == 0)){
        if (dyn_unc){
            *ready = EAR_POLICY_TRY_AGAIN;
        }else{
            *ready = EAR_POLICY_READY;
        }
        verbose(2,"SELECT_CPUFREQ state dyn_unc %u",dyn_unc);

        last_cpu_pstate = curr_pstate;
        last_imc_pstate = imc_pstate;
#endif
        // If is not the default P_STATE selected in the environment, a projection
        // is made for the reference P_STATE in case the projections were available.
        if (curr_freq != def_freq) // Use configuration when available
        {
            if (projection_available(curr_pstate,def_pstate)==EAR_SUCCESS)
            {
                st=project_power(my_app,curr_pstate,def_pstate,&power_ref);
                st=project_time(my_app,curr_pstate,def_pstate,&time_ref);
                best_freq=def_freq;
                best_pstate=def_pstate;
            }
            else
            {
                time_ref=my_app->time;
                power_ref=my_app->DC_power;
                best_freq=curr_freq;
                best_pstate=curr_pstate;
            }
        }
        else
        { 
            time_ref=my_app->time;
            power_ref=my_app->DC_power;
            best_freq=curr_freq;
            best_pstate=curr_pstate;
        }


        // ref=1 is nominal 0=turbo, we are not using it

        try_next=1;
        i=best_pstate-1;
        time_current=time_ref;
        debug("Policy Signature (CPI=%lf GBS=%lf Power=%lf Time=%lf TPI=%lf)",my_app->CPI,my_app->GBS,my_app->DC_power,my_app->time,my_app->TPI);
        debug("Starting at pstate %d, curr_freq %lu def_freq %lu min_pstate %d",i,curr_freq,def_freq,min_pstate);

        while(try_next && (i >= min_pstate))
        {
            if (projection_available(curr_pstate,i)==EAR_SUCCESS)
            {
                //debug("Looking for pstate %d",i);
                st=project_power(my_app,curr_pstate,i,&power_proj);
                st=project_time(my_app,curr_pstate,i,&time_proj);
                projection_set(i,time_proj,power_proj);
                freq_ref=frequency_pstate_to_freq(i);
                freq_gain=min_eff_gain*(double)(freq_ref-best_freq)/(double)best_freq;
                perf_gain=(time_current-time_proj)/time_current;
                debug("%lu to %d time %lf proj time %lf freq gain %lf perf_gain %lf",curr_pstate,i,my_app->time,time_proj,freq_gain,perf_gain);
                // OK
                if (perf_gain>=freq_gain)
                {
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
        /* Controlar la freq por power cap , si capado poner GREEDY, gestionar req-f */	
        if (best_freq<def_freq) best_freq=def_freq;
        *new_freq=best_freq;
#if NEW_IMCFREQ
        if (dyn_unc){
            if (!imc_set_by_hw){
                /*  Fix the imc to the first selected by hardware */
                freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = avg_to_khz(my_app->sig_ext->imc_freq);
                // policy_max_imcfreq = avg_to_khz(my_app->sig_ext->imc_freq);
                // policy_min_imcfreq = policy_max_imcfreq - IMC_STEP;
            }
            else {
                freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = imc_max_khz;
                // policy_max_imcfreq = imc_max_khz;
                // policy_min_imcfreq = imc_min_khz;
            }
            freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = freqs->imc_freq[sid*IMC_VAL+IMC_MAX] - IMC_RANGE;
            max_imc_pstate = imc_pstate;
            if (*new_freq != curr_freq) min_time_state = COMP_IMCREF;
            else {
                ref_imc_pstate = imc_pstate;
                min_time_state = SELECT_IMCFREQ;
            }
        }else{
            min_time_state = SELECT_CPUFREQ;
        }
#endif
#if NEW_IMCFREQ
    }else if ((min_time_state == SELECT_IMCFREQ) && (dyn_unc)){
        verbose(2,"SELECT_IMCFREQ state: nominal %d - last CPU %u,IMC %u -- current CPU %lu, IMC %lu",
                min_pstate,last_cpu_pstate,last_imc_pstate,curr_pstate,imc_pstate);
        *new_freq = frequency_pstate_to_freq(curr_pstate);
        uint increase_imc = must_increase_imc(imc_data,curr_pstate,imc_pstate,curr_pstate,ref_imc_pstate,my_app,imc_extra_th);
        /* If we are over the limit, we increase the IMC */
        if (increase_imc){
            verbose(2,"%sWarning, passing the limit",COL_RED);
            freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = ear_min(freqs->imc_freq[sid*IMC_VAL+IMC_MAX] + IMC_STEP,imc_max_khz);	
            //policy_min_imcfreq = policy_max_imcfreq - IMC_RANGE;
            freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = imc_min_khz;
            verbose(2,"Selecting %lu - %lu and policy ready%s",freqs->imc_freq[sid*IMC_VAL+IMC_MAX],freqs->imc_freq[sid*IMC_VAL+IMC_MIN],COL_CLR);
            *ready = EAR_POLICY_READY;
            min_time_state = SELECT_CPUFREQ;
        }else{
            freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = ear_max(freqs->imc_freq[sid*IMC_VAL+IMC_MAX] - IMC_STEP,imc_min_khz-IMC_RANGE);
            //policy_min_imcfreq = policy_max_imcfreq - IMC_RANGE;
            freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = imc_min_khz;
            verbose(2,"%sTrying a lower IMC freq %lu-%lu%s",COL_GRE,freqs->imc_freq[sid*IMC_VAL+IMC_MAX],freqs->imc_freq[sid*IMC_VAL+IMC_MIN],COL_CLR);
            if (freqs->imc_freq[sid*IMC_VAL+IMC_MAX] > (imc_min_khz + IMC_RANGE)) *ready = EAR_POLICY_TRY_AGAIN;
            else{ 
                *ready = EAR_POLICY_READY;
                min_time_state = SELECT_CPUFREQ;
            }
        }
    }else if (min_time_state == COMP_IMCREF){
        ulong curr_imc_freq = avg_to_khz(my_app->sig_ext->imc_freq);
        ulong curr_imc_pstate = (def_imc_max_khz - curr_imc_freq)/IMC_STEP;
        *new_freq = frequency_pstate_to_freq(curr_pstate);
        ref_imc_pstate = curr_imc_pstate;
        freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = curr_imc_freq;
        //policy_min_imcfreq = policy_max_imcfreq - IMC_RANGE;
        freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = imc_min_khz;
        verbose(2,"COMP_IMCREF cpufreq %lu imc %lu-%lu",*new_freq,freqs->imc_freq[sid*IMC_VAL+IMC_MAX],freqs->imc_freq[sid*IMC_VAL+IMC_MIN]);
        min_time_state = SELECT_IMCFREQ;
        *ready = EAR_POLICY_TRY_AGAIN;
    }
#endif
    verbose(2,"Next min_time state %u next policy state %u",min_time_state,*ready);
    return EAR_SUCCESS;
}


state_t policy_ok(polctx_t *c,signature_t *curr_sig,signature_t *last_sig,int *ok)
{
    state_t st=EAR_SUCCESS;
    ulong eff_f;
    uint power_status;
    uint next_status;

    if ((c==NULL) || (curr_sig==NULL) || (last_sig==NULL)) return EAR_ERROR;




    if (curr_sig->def_f==last_sig->def_f) *ok=1;

    // Check that efficiency is enough
    if (curr_sig->time < last_sig->time) *ok=1;

    // If time is similar and it was same freq it is ok	
    if (equal_with_th(curr_sig->time , last_sig->time,0.1) && (curr_sig->def_f == last_sig->def_f)) *ok=1;

    // If we run at a higher freq but we are slower, we are not ok	
    if ((curr_sig->time > last_sig->time) && (curr_sig->def_f > last_sig->def_f)) *ok=0;

    // else *ok=0;

    return st;

}

state_t policy_get_default_freq(polctx_t *c, ulong *f)
{
    if ((c!=NULL) && (c->app!=NULL)){
        // Just in case the bestPstate was the frequency at which the application was running
        *f=FREQ_DEF(c->app->def_freq);
    }
    return EAR_SUCCESS;
}

state_t policy_max_tries(polctx_t *c,int *intents)
{
    *intents=2;
    return EAR_SUCCESS;
}

state_t policy_mpi_init(polctx_t *c)
{
	return mpi_call_init(c);
}
state_t policy_mpi_end(polctx_t *c)
{
	return mpi_call_end(c);
}

state_t policy_domain(polctx_t *c,node_freq_domain_t *domain)
{
    domain->cpu = POL_GRAIN_NODE;
    if (dyn_unc) 	domain->mem = POL_GRAIN_NODE;
    else 					domain->mem = POL_NOT_SUPPORTED;
#if USE_GPUS
    if (c->num_gpus) domain->gpu = POL_GRAIN_CORE;
    else domain->gpu = POL_NOT_SUPPORTED;
#else
    domain->gpu = POL_NOT_SUPPORTED;
#endif
    domain->gpumem = POL_NOT_SUPPORTED;
    return EAR_SUCCESS;
}

