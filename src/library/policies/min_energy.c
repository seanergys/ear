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
#include <library/common/externs.h>
#include <library/common/verbose_lib.h>
#include <library/api/clasify.h>
#include <management/cpufreq/frequency.h>
#include <common/types/projection.h>
#include <daemon/local_api/eard_api.h>
#include <daemon/powercap/powercap_status.h>
#include <library/policies/policy_api.h>
#include <library/policies/policy_state.h>
#include <library/policies/imc_policy_support.h>
#include <library/policies/gpu_support.h>
#include <library/policies/cpu_support.h>
#include <library/policies/mpi_stats_support.h>
#include <library/metrics/gpu.h>


typedef unsigned long ulong;
#ifdef EARL_RESEARCH
extern unsigned long ext_def_freq;
#define FREQ_DEF(f) (!ext_def_freq?f:ext_def_freq)
#else
#define FREQ_DEF(f) f
#endif

#if NEW_IMCFREQ
extern uint dyn_unc;
extern uint imc_set_by_hw;
extern double imc_extra_th;
extern ulong imc_max_khz,imc_min_khz;
extern ulong def_imc_max_khz,def_imc_min_khz;
extern uint dynais_enabled;
extern volatile uint ear_periodic_mode;
ulong avg_to_khz(ulong freq_khz);
static uint imc_freq_selected = 0;
static uint last_cpu_pstate,last_imc_pstate;
static int min_pstate;
static uint ref_imc_pstate;
extern int my_globalrank;
static int nump;
static uint enable_load_balance = 1;
static uint use_turbo_for_cp = 0;

static imc_data_t *imc_data;
#define IMC_STEP 100000
#define IMC_RANGE 100000
#define SELECT_CPUFREQ 100
#define SELECT_IMCFREQ 101
#define COMP_IMCREF		 102
#define LB_TH 10.0
#define CP_TH 5.0
#define EXTRA_TH  0.05
static uint min_energy_state = SELECT_CPUFREQ;
static uint imc_dynamic = 0;
static uint min_imc_pstate = 0;
static uint use_imc_ref = 0;
#endif



static ulong req_f;
static signature_t *my_app;
static mpi_information_t *my_node_mpi_calls,*last_node_mpi_calls;
static mpi_calls_types_t *my_node_mpi_types_calls,*last_node_mpi_types_calls;
static polsym_t gpus;
static ulong *last_cpufreq_sel;
static const ulong **gpuf_pol_list;
static const uint *gpuf_pol_list_items;
static uint num_processes;
static uint *critical_path;

state_t policy_init(polctx_t *c)
{
    char *cuse_imc_ref;
		char *cenable_load_balance = getenv(SCHED_ENABLE_LOAD_BALANCE);
		char *cuse_turbo_for_cp = getenv(SCHED_USE_TURBO_FOR_CP);
    int i;
    if (c!=NULL){ 
#if NEW_IMCFREQ
        imc_data = calloc(c->num_pstates*NUM_UNC_FREQ,sizeof(imc_data_t));
#endif
        if (is_mpi_enabled()){
            mpi_app_init(c);
        }
				if (cenable_load_balance != NULL) enable_load_balance = atoi(cenable_load_balance);
				verbose_master(2,"Load balance enabled = %u", enable_load_balance);
				if (cuse_turbo_for_cp != NULL) use_turbo_for_cp = atoi(cuse_turbo_for_cp);
				verbose_master(2,"Using turbo for CP when load unbalance set to %u",use_turbo_for_cp);
        my_app = calloc(1,sizeof(signature_t));
        my_app->sig_ext =  calloc(1,sizeof(io_data_t));
        my_node_mpi_calls = calloc(lib_shared_region->num_processes,sizeof(mpi_information_t));
        my_node_mpi_types_calls = calloc(lib_shared_region->num_processes,sizeof(mpi_calls_types_t));
        last_node_mpi_calls = calloc(lib_shared_region->num_processes,sizeof(mpi_information_t));
        last_node_mpi_types_calls = calloc(lib_shared_region->num_processes,sizeof(mpi_calls_types_t));
        last_cpufreq_sel = calloc(MAX_CPUS_SUPPORTED,sizeof(unsigned long));
				num_processes = lib_shared_region->num_processes;
				critical_path = calloc(lib_shared_region->num_processes,sizeof(uint));
        for (i=0;i<num_processes; i++) last_cpufreq_sel[i] = *(c->ear_frequency);
#if USE_GPUS
        if (c->num_gpus){
            if (policy_gpu_load(c->app,&gpus) != EAR_SUCCESS){
                verbose_master(2,"Error loading GPU policy");
            }
            if (gpus.init != NULL) gpus.init(c);
            gpu_lib_freq_list(&gpuf_pol_list, &gpuf_pol_list_items);
        }

#endif
        return EAR_SUCCESS;
    } else return EAR_ERROR;
}


state_t policy_end(polctx_t *c)
{
    if (c != NULL) return  mpi_app_end(c);
    else return EAR_ERROR;
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


state_t policy_apply(polctx_t *c,signature_t *sig,node_freqs_t *freqs,int *ready)
{
    ulong *new_freq=freqs->cpu_freq;
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
    double sig_vpi,node_sig_vpi,max_vpi,local_penalty, base_penalty;
    uint local_process_with_high_vpi;
    uint sid = 0;
    int gready;
    int freq_per_core = 0;
    mpi_information_t sig_mpi_data;	
    int max_mpi,min_mpi;
		uint signature_is_unbalance = 0;


    if ((c == NULL) || (c->app == NULL)){
        *ready=EAR_POLICY_CONTINUE;
        return EAR_ERROR;
    }

    verbose_master(2,"%s MIN_energy ----------------%s",COL_GRE,COL_CLR);

    if (gpus.node_policy_apply != NULL){
        /* Basic support for GPUS */
        gpus.node_policy_apply(c,sig,freqs,&gready);
    }
		if (is_mpi_enabled()){
        read_diff_node_mpi_type_info(lib_shared_region,sig_shared_region,my_node_mpi_types_calls,last_node_mpi_types_calls);
				verbose_mpi_types(2,my_node_mpi_types_calls);
		}

    if (!are_signatures_ready(lib_shared_region,sig_shared_region)){
        memcpy(new_freq,last_cpufreq_sel,sizeof(ulong)*MAX_CPUS_SUPPORTED);
        *ready = EAR_POLICY_TRY_AGAIN;
        verbose_master(2,"Node signature not ready, selecting %lu CPU f",*new_freq);
        return EAR_SUCCESS;
    }
    verbose_master(2, "%s------MR[%d] Node signatures ready-----%s",COL_GRE,my_globalrank,COL_CLR);
    max_penalty = c->app->settings[0];
		base_penalty = max_penalty;				
    if (is_mpi_enabled()){
				if (earl_phase_classification == APP_MPI_BOUND){
					verbose_master(2,"%sMPI_BOUND%s application, not reducing CPU freq",COL_BLU,COL_CLR);
					freq_per_core = 0;
					nump = 1;
					base_penalty = 0;	
				}else{
					/* Even though it's compute bound, we check if we must re distribute power */
        	read_diff_node_mpi_data(lib_shared_region,sig_shared_region,my_node_mpi_calls,last_node_mpi_calls);
        	evaluate_cp(num_processes,my_node_mpi_calls,&max_mpi,&min_mpi);
					memset(critical_path,0,sizeof(uint)*num_processes);
					signature_is_unbalance = ((my_node_mpi_calls[max_mpi].perc_mpi - my_node_mpi_calls[min_mpi].perc_mpi) > LB_TH);
        	verbose_master(2,"Node load unbalance, max perc mpi %lf min perc mpi %lf unbalance = %u",my_node_mpi_calls[max_mpi].perc_mpi,my_node_mpi_calls[min_mpi].perc_mpi, signature_is_unbalance && enable_load_balance);
        	if (enable_load_balance && signature_is_unbalance){
            	freq_per_core = 1;
            	nump = num_processes;
							for (i=0; i < num_processes; i++){
								if ((my_node_mpi_calls[i].perc_mpi - my_node_mpi_calls[min_mpi].perc_mpi) < CP_TH) critical_path[i] = 1;
							}	
        	}else{ nump = 1;}
				}
    }else nump = 1;
    if (c->use_turbo) min_pstate = 0;
    else min_pstate = frequency_closest_pstate(c->app->max_freq);

    nominal = frequency_pstate_to_freq(min_pstate);
    // Default values

    def_freq = FREQ_DEF(c->app->def_freq);
    def_pstate = frequency_closest_pstate(def_freq);

    // This is the frequency at which we were running
#ifdef POWERCAP
    if (c->pc_limit > 0){
        curr_freq = frequency_closest_high_freq(my_app->avg_f,1);
    }else{
        curr_freq = *(c->ear_frequency);
    }
#else
    curr_freq = *(c->ear_frequency);
#endif

    signature_copy(my_app,sig);
    curr_pstate = frequency_closest_pstate(curr_freq);
    eff_f = frequency_closest_high_freq(my_app->avg_f,1);

#if NEW_IMCFREQ
    if (dyn_unc){
        // imc_pstate = (def_imc_max_khz - policy_max_imcfreq)/IMC_STEP;
        ulong curr_imc_freq = avg_to_khz(my_app->sig_ext->imc_freq);
        imc_pstate = (def_imc_max_khz - curr_imc_freq)/IMC_STEP;
        verbose_master(2,"CPU pstate %lu IMC freq %lu IMC pstate %lu",curr_pstate,my_app->sig_ext->imc_freq,imc_pstate);
        copy_imc_data_from_signature(imc_data,curr_pstate,imc_pstate,my_app);
    }
    if ((min_energy_state == SELECT_CPUFREQ) || (dyn_unc == 0)){
        if (dyn_unc){
            *ready = EAR_POLICY_TRY_AGAIN;
        }else{ 
            *ready = EAR_POLICY_READY;
        }
        verbose_master(2,"%sSELECT_CPUFREQ%s state dyn_unc %u",COL_BLU,COL_CLR,dyn_unc);
#else
        *ready=EAR_POLICY_READY;
#endif
        last_cpu_pstate = curr_pstate;
        last_imc_pstate = imc_pstate;


        // If is not the default P_STATE selected in the environment, a projection
        // is made for the reference P_STATE in case the coefficents were available.
        // NEW: This code is new because projections from lower pstates are reporting bad cpu freqs 
        i = 0;
        while((i < num_processes) && (last_cpufreq_sel[i] == def_freq)) i++;
        if (i < num_processes){
            *ready = EAR_POLICY_TRY_AGAIN;
            new_freq[0] = def_freq;
            for (i=1;i<num_processes;i++) new_freq[i] = new_freq[0];
            if (dyn_unc){
                freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = imc_max_khz;
                freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = imc_min_khz;
            }
            memcpy(last_cpufreq_sel,new_freq,sizeof(ulong)*MAX_CPUS_SUPPORTED);	
            return EAR_SUCCESS;
        }
        for (i=0;i<nump;i++){
            verbose_master(2,"Processing proc %d freq per core = %d",i,freq_per_core);
            if (!freq_per_core){
                compute_total_node_instructions(lib_shared_region,sig_shared_region,&my_app->instructions); 
                compute_total_node_flops(lib_shared_region,sig_shared_region,my_app->FLOPS);
                compute_total_node_cycles(lib_shared_region,sig_shared_region,&my_app->cycles);
                compute_vpi(&sig_vpi,sig);
                compute_vpi(&node_sig_vpi,my_app);
                local_process_with_high_vpi = compute_max_vpi(lib_shared_region,sig_shared_region,&max_vpi);
                verbose(2,"AVGNodeSig MR[%d] SIGNATURE CPI %.3lf VPI %.3lf/Node CPI %.3lf VPI %.3lf/MAX VPI %.3lf ",my_globalrank,sig->CPI,sig_vpi,(double)my_app->cycles/(double)my_app->instructions,node_sig_vpi,max_vpi);
                my_app->CPI = (double)my_app->cycles/(double)my_app->instructions;
                /* With these two lines, we force the VPI of the signatura to the most loaded in the node */
                my_app->instructions = sig_shared_region[local_process_with_high_vpi].sig.instructions;
                memcpy(my_app->FLOPS,sig_shared_region[local_process_with_high_vpi].sig.FLOPS,sizeof(ull)*FLOPS_EVENTS);
                local_penalty = base_penalty;
            }else{
                from_minis_to_sig(my_app,&sig_shared_region[i].sig);
                verbose_master(2,"Per-processSig MR[%d][%d] SIGNATURE CPI %.3lf (%lf,%lf)",my_globalrank,i,my_app->CPI,my_node_mpi_calls[i].perc_mpi,my_node_mpi_calls[min_mpi].perc_mpi);
                if (critical_path[i]){
									if (use_turbo_for_cp){
										new_freq[i] = frequency_pstate_to_freq(0);
									}else{ 
										new_freq[i] = frequency_pstate_to_freq(1);
									}
            			verbose_master(2,"CPU freq for CP process %d is %lu",i,new_freq[i]);
									continue;
								}else{ 
                    local_penalty = base_penalty + EXTRA_TH*(my_node_mpi_calls[i].perc_mpi - my_node_mpi_calls[min_mpi].perc_mpi)/10.0;
                }
								verbose_master(2,"Penalty %f", local_penalty);
            }
            compute_reference(c,my_app,&curr_freq,&def_freq,&best_freq,&time_ref,&power_ref);
            verbose_master(3,"Time ref %lf Power ref %lf Freq reqf %lu",time_ref,power_ref,best_freq);
            compute_cpu_freq_min_energy(c,my_app,best_freq,time_ref,power_ref,local_penalty,curr_pstate,min_pstate,c->num_pstates,&new_freq[i]);
            verbose_master(2,"CPU freq for process %d is %lu",i,new_freq[i]);
        }
        if (!freq_per_core){
            for (i=1;i<num_processes;i++) new_freq[i] = new_freq[0];
        }
        memcpy(last_cpufreq_sel,new_freq,sizeof(ulong)*num_processes);

#if NEW_IMCFREQ	
        if (dyn_unc){
            freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = imc_max_khz;
            if (imc_set_by_hw) 		freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = imc_min_khz;
            else 									freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = freqs->imc_freq[sid*IMC_VAL+IMC_MAX] - IMC_RANGE;
            i = 0;
            while((i<num_processes) && (last_cpufreq_sel[i] == def_freq)) i++;
            if (i < num_processes){ 
              min_energy_state = COMP_IMCREF;
            }else{ 
        			ref_imc_pstate = imc_pstate;
              min_energy_state = SELECT_IMCFREQ;
            }
        }else{
            min_energy_state = SELECT_CPUFREQ;
            *ready = EAR_POLICY_READY;
        }
#endif

#if NEW_IMCFREQ
    }else if (min_energy_state == COMP_IMCREF){
        ulong curr_imc_freq = avg_to_khz(my_app->sig_ext->imc_freq);
        ulong curr_imc_pstate = (def_imc_max_khz - curr_imc_freq)/IMC_STEP;
        memcpy(new_freq,last_cpufreq_sel,sizeof(ulong)*MAX_CPUS_SUPPORTED);
        ref_imc_pstate = curr_imc_pstate;
        freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = curr_imc_freq;
        //policy_min_imcfreq = policy_max_imcfreq - IMC_RANGE;
        freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = imc_min_khz;
        verbose(2,"%sCOMP_IMCREF%s cpufreq %lu imc %lu-%lu",COL_BLU,COL_CLR,*new_freq,freqs->imc_freq[sid*IMC_VAL+IMC_MAX],freqs->imc_freq[sid*IMC_VAL+IMC_MIN]);
        min_energy_state = SELECT_IMCFREQ;
        *ready = EAR_POLICY_TRY_AGAIN;
    }else if ((min_energy_state == SELECT_IMCFREQ) && (dyn_unc)){
        verbose(2,"%sSELECT_IMCFREQ%s state: nominal %d - last CPU %u,IMC %u -- current CPU %lu, IMC %lu",COL_BLU,COL_CLR,min_pstate,last_cpu_pstate,last_imc_pstate,curr_pstate,imc_pstate);
        memcpy(new_freq,last_cpufreq_sel,sizeof(ulong)*MAX_CPUS_SUPPORTED);
        uint increase_imc = must_increase_imc(imc_data,curr_pstate,imc_pstate,curr_pstate,ref_imc_pstate,my_app,imc_extra_th);
        /* If we are over the limit, we increase the IMC */
        if (increase_imc){
            verbose(2,"%sWarning, passing the limit",COL_RED);
            freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = ear_min(freqs->imc_freq[sid*IMC_VAL+IMC_MAX] + IMC_STEP,imc_max_khz);	
            //policy_min_imcfreq = policy_max_imcfreq - IMC_RANGE;
            freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = imc_min_khz;
            verbose(2,"Selecting %lu - %lu and policy ready%s",freqs->imc_freq[sid*IMC_VAL+IMC_MAX],freqs->imc_freq[sid*IMC_VAL+IMC_MIN],COL_CLR);
            *ready = EAR_POLICY_READY;
            min_energy_state = SELECT_CPUFREQ;
        }else{
            freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = ear_max(freqs->imc_freq[sid*IMC_VAL+IMC_MAX] - IMC_STEP,imc_min_khz-IMC_RANGE);
            //policy_min_imcfreq = policy_max_imcfreq - IMC_RANGE;
            freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = imc_min_khz;
            verbose(2,"%sTrying a lower IMC freq %lu-%lu%s",COL_GRE,freqs->imc_freq[sid*IMC_VAL+IMC_MAX],freqs->imc_freq[sid*IMC_VAL+IMC_MIN],COL_CLR);
            if (freqs->imc_freq[sid*IMC_VAL+IMC_MAX] > (imc_min_khz + IMC_RANGE)) *ready = EAR_POLICY_TRY_AGAIN;
            else{ 
                *ready = EAR_POLICY_READY;
                min_energy_state = SELECT_CPUFREQ;
            }
        }
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

    if (curr_sig->def_f == last_sig->def_f){ 
        *ok=1;
        return EAR_SUCCESS;
    }
    energy_last = last_sig->time*last_sig->DC_power;
    energy_curr = curr_sig->time * curr_sig->DC_power;

    if (ear_periodic_mode == PERIODIC_MODE_OFF){
        if (curr_sig->time <= (last_sig->time + (last_sig->time*c->app->settings[0]))) *ok=1;
        else *ok=0;
    }else{
        if (curr_sig->GBS >= (last_sig->GBS - (last_sig->GBS*c->app->settings[0]))) *ok = 1;
        else *ok = 0;
    }
    return EAR_SUCCESS;
}

state_t policy_get_default_freq(polctx_t *c, ulong *freq_set)
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

state_t policy_domain(polctx_t *c,node_freq_domain_t *domain)
{
    domain->cpu = POL_GRAIN_CORE;
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

state_t policy_mpi_init(polctx_t *c,mpi_call call_type)
{
    if (c != NULL) return mpi_call_init(c,call_type);
    else return EAR_ERROR;
}
state_t policy_mpi_end(polctx_t *c,mpi_call call_type)
{
    if (c != NULL) return mpi_call_end(c,call_type);
    else return EAR_ERROR;
}

state_t policy_io_settings(polctx_t *c,signature_t *my_sig,node_freqs_t *freqs)
{
    int i;
    uint sock_id;
    if (c == NULL) return EAR_ERROR;
    for (i=0;i<num_processes;i++) freqs->cpu_freq[i] = frequency_pstate_to_freq(c->num_pstates - 1);
    memcpy(last_cpufreq_sel,freqs->cpu_freq,sizeof(ulong)*MAX_CPUS_SUPPORTED);
    if (dyn_unc){
        for (sock_id=0;sock_id<arch_desc.top.socket_count;sock_id++){
            freqs->imc_freq[sock_id*IMC_VAL+IMC_MAX] = imc_min_khz;
            freqs->imc_freq[sock_id*IMC_VAL+IMC_MIN] = imc_min_khz;
        }
    }
#if USE_GPUS
    for (uint gpuid=0;gpuid<c->num_gpus;gpuid++){
        freqs->gpu_freq[gpuid] = gpuf_pol_list[gpuid][gpuf_pol_list_items[gpuid]-1];
    }
#endif
    /* GPU mem is pending */

    return EAR_SUCCESS;
}
state_t policy_busy_wait_settings(polctx_t *c,signature_t *my_sig,node_freqs_t *freqs)
{
    int i;
    uint sock_id;
    if (c == NULL) return EAR_ERROR;
    for (i=0;i<num_processes;i++) freqs->cpu_freq[i] = frequency_pstate_to_freq(c->num_pstates - 1);
    if (dyn_unc){
        memcpy(last_cpufreq_sel,freqs->cpu_freq,sizeof(ulong)*MAX_CPUS_SUPPORTED);
        for (uint sock_id=0;sock_id<arch_desc.top.socket_count;sock_id++){
            freqs->imc_freq[sock_id*IMC_VAL+IMC_MAX] = imc_min_khz;
            freqs->imc_freq[sock_id*IMC_VAL+IMC_MIN] = imc_min_khz;
        }
    }
#if USE_GPUS
    for (uint gpuid=0;gpuid<c->num_gpus;gpuid++){
        freqs->gpu_freq[gpuid] = gpuf_pol_list[gpuid][0];
    }
#endif
    /* GPU mem is pending */

    return EAR_SUCCESS;
}
state_t policy_restore_settings(polctx_t *c,signature_t *my_sig,node_freqs_t *freqs)
{
    int i;
    uint sock_id;
    if (c == NULL) return EAR_ERROR;
    for (i=0;i<num_processes;i++) freqs->cpu_freq[i] = c->app->max_freq;
    memcpy(last_cpufreq_sel,freqs->cpu_freq,sizeof(ulong)*MAX_CPUS_SUPPORTED);
    if (dyn_unc){
        for (uint sock_id=0;sock_id<arch_desc.top.socket_count;sock_id++){
            freqs->imc_freq[sock_id*IMC_VAL+IMC_MAX] = imc_max_khz;
            freqs->imc_freq[sock_id*IMC_VAL+IMC_MIN] = imc_min_khz;
        }
    }
#if USE_GPUS
    for (uint gpuid=0;gpuid<c->num_gpus;gpuid++){
        freqs->gpu_freq[gpuid] = gpuf_pol_list[gpuid][0];
    }
#endif
    /* GPU mem is pending */

    return EAR_SUCCESS;
}

