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
static ulong min_cpufreq = 0;
static uint network_use_imc = 0;
static uint try_turbo_enabled = 0;
static float last_pmpi, pmpi;
static node_freqs_t min_energy_def_freqs;
static node_freqs_t last_nodefreq_sel;


static imc_data_t *imc_data;
#define IMC_STEP 100000
#define IMC_RANGE 100000
#define SELECT_CPUFREQ 		100
#define SELECT_IMCFREQ 		101
#define COMP_IMCREF		 		102
#define TRY_TURBO_CPUFREQ 103
#define LB_TH 10.0
#define CP_TH 5.0
#define EXTRA_TH  0.05
static uint min_energy_state = SELECT_CPUFREQ;
static uint imc_dynamic = 0;
static uint min_imc_pstate = 0;
static uint use_imc_ref = 0;


static int master_sig_without_sig = 0;
static ulong req_f;
static signature_t *my_app;
static mpi_information_t *my_node_mpi_calls,*last_node_mpi_calls;
static mpi_calls_types_t *my_node_mpi_types_calls,*last_node_mpi_types_calls;
static polsym_t gpus;
static const ulong **gpuf_pol_list;
static const uint *gpuf_pol_list_items;
static uint num_processes;
static uint *critical_path;
static ulong nominal_node;
static signature_t cpufreq_signature;
static double *percs_mpi;
static uint block_type;

extern uint cpu_ready, gpu_ready, gidle;

state_t policy_init(polctx_t *c)
{
    char *cuse_imc_ref;
		char *cenable_load_balance = getenv(SCHED_ENABLE_LOAD_BALANCE);
		char *cuse_turbo_for_cp = getenv(SCHED_USE_TURBO_FOR_CP);
		char *cmin_cpufreq = getenv(SCHED_MIN_CPUFREQ);
		char *cnetwork_use_imc = getenv(SCHED_NETWORK_USE_IMC);
		char *ctry_turbo_enabled = getenv(SCHED_TRY_TURBO);
    int i, sid = 0;
    if (c!=NULL){ 
        imc_data = calloc(c->num_pstates*NUM_UNC_FREQ,sizeof(imc_data_t));
				nominal_node = frequency_get_nominal_freq();
				verbose_master(2,"Nominal is %lu",nominal_node);
        if (is_mpi_enabled()){
            mpi_app_init(c);
						is_blocking_busy_waiting(&block_type);
						verbose_master(1,"Busy_waiting MPI set to %u",block_type == BUSY_WAITING_BLOCK);
        }
				if (cenable_load_balance != NULL) enable_load_balance = atoi(cenable_load_balance);
				verbose_master(2,"Load balance enabled = %u", enable_load_balance);
				if (cuse_turbo_for_cp != NULL) use_turbo_for_cp = atoi(cuse_turbo_for_cp);
				verbose_master(2,"Using turbo for CP when load unbalance set to %u",use_turbo_for_cp);
				if (cmin_cpufreq != NULL){
					min_cpufreq = atoi(cmin_cpufreq);
				}
				if (min_cpufreq) verbose_master(2,"Min CPUfreq is limited = %lu",min_cpufreq);
				if (cnetwork_use_imc != NULL) network_use_imc = atoi(cnetwork_use_imc);
				verbose_master(2,"Network uses IMC set to %u",network_use_imc);
				if (ctry_turbo_enabled != NULL) try_turbo_enabled = atoi(ctry_turbo_enabled);
				verbose_master(2,"Using turbo for cbound app set to %u",try_turbo_enabled);

				num_processes = lib_shared_region->num_processes;
				
        my_app = calloc(1,sizeof(signature_t));
        my_app->sig_ext =  calloc(1,sizeof(io_data_t));
        my_node_mpi_calls = calloc(lib_shared_region->num_processes,sizeof(mpi_information_t));
        my_node_mpi_types_calls = calloc(lib_shared_region->num_processes,sizeof(mpi_calls_types_t));
        last_node_mpi_calls = calloc(lib_shared_region->num_processes,sizeof(mpi_information_t));
        last_node_mpi_types_calls = calloc(lib_shared_region->num_processes,sizeof(mpi_calls_types_t));
				percs_mpi = calloc(num_processes, sizeof(double));
				last_pmpi = 100.0 * num_processes;
				critical_path = calloc(lib_shared_region->num_processes,sizeof(uint));
				node_freqs_alloc(&min_energy_def_freqs);
				node_freqs_alloc(&last_nodefreq_sel);
				/* Configuring default freqs */
				for (i=0;i<num_processes;i++) min_energy_def_freqs.cpu_freq[i] = *(c->ear_frequency);
				min_energy_def_freqs.imc_freq[sid*IMC_VAL+IMC_MAX] = imc_max_khz;
    		min_energy_def_freqs.imc_freq[sid*IMC_VAL+IMC_MIN] = imc_min_khz;
	
#if USE_GPUS
        if (c->num_gpus){
            if (policy_gpu_load(c->app,&gpus) != EAR_SUCCESS){
                verbose_master(2,"Error loading GPU policy");
            }
						verbose_master(2,"Initialzing GPU policy part");
            if (gpus.init != NULL) gpus.init(c);
            if (gpu_lib_freq_list(&gpuf_pol_list, &gpuf_pol_list_items) != EAR_SUCCESS){
							verbose_master(2,"Error asking for gpu freq list");
						}
						for (i=0;i<c->num_gpus;i++){
							min_energy_def_freqs.gpu_freq[i] = gpuf_pol_list[i][0];
						}
        }
				verbose_node_freqs(2,&min_energy_def_freqs);
#endif
				node_freqs_copy(&last_nodefreq_sel, &min_energy_def_freqs);
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
    ulong curr_freq, nominal;
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
		uint cbound, mbound, turbo_set = 0;
		ulong max_cpufreq = 0, min_cpufreq_sel = 10000000 ;
		double mpi_stats_perc_mpi_sd, mpi_stats_perc_mpi_mean, lvpi,lvpi2;
		ulong eff_p_cpuf, process_avgcpuf;


    if ((c == NULL) || (c->app == NULL)){
        *ready=EAR_POLICY_CONTINUE;
        return EAR_ERROR;
    }
    verbose_master(2,"%s MIN_energy ----------------%s",COL_GRE,COL_CLR);

    if (gpus.node_policy_apply != NULL){
        /* Basic support for GPUS */
        gpus.node_policy_apply(c,sig,freqs,&gready);
				gpu_ready = gready;
    }else{
			gpu_ready = EAR_POLICY_READY;
		}

		if (cpu_ready == EAR_POLICY_READY){
			return EAR_SUCCESS;	
		}
		if (is_mpi_enabled()){
        read_diff_node_mpi_type_info(lib_shared_region,sig_shared_region,my_node_mpi_types_calls,last_node_mpi_types_calls);
      	read_diff_node_mpi_data(lib_shared_region,sig_shared_region,my_node_mpi_calls,last_node_mpi_calls);
		}

    if (!are_signatures_ready(lib_shared_region,sig_shared_region)){
        memcpy(freqs->cpu_freq,last_nodefreq_sel.cpu_freq,sizeof(ulong)*MAX_CPUS_SUPPORTED);
        memcpy(freqs->imc_freq,last_nodefreq_sel.imc_freq,arch_desc.top.socket_count*IMC_VAL*sizeof(ulong));
        *ready = EAR_POLICY_TRY_AGAIN;
        verbose_master(2,"Node signature not ready, selecting %lu CPU f",*new_freq);
				master_sig_without_sig++;
				if (master_sig_without_sig > 3) print_sig_readiness(lib_shared_region,sig_shared_region);	
        return EAR_SUCCESS;
    }
		master_sig_without_sig = 0;
    verbose_master(2, "%s------MR[%d] Node signatures ready-----%s",COL_GRE,my_globalrank,COL_CLR);
		for (int i=0;i < num_processes;i++){
			compute_ssig_vpi(&lvpi, &sig_shared_region[i].sig);
			compute_ssig_vpi2(&lvpi2, &sig_shared_region[i].sig);
			process_avg_cpu_freq(&sig_shared_region[i].cpu_mask,cpufreq_data_per_core,&process_avgcpuf);
			verbosen_master(2,"[%d]=(LVPI=%.2lf/%.2lf AvgCPU=%.2f CPI=%.2lf)",i,lvpi2,lvpi,(float)process_avgcpuf/1000000.0,sig_shared_region[i].sig.CPI);
		}
		verbose_master(2,"");
    max_penalty = c->app->settings[0];
		base_penalty = max_penalty;				
    if (is_mpi_enabled() && (min_energy_state == SELECT_CPUFREQ) && enable_load_balance){
			/* Even though it's compute bound, we check if we must re distribute power */
      evaluate_cp(num_processes,my_node_mpi_calls,&max_mpi,&min_mpi);
			memset(critical_path,0,sizeof(uint)*num_processes);
      mpi_stats_get_only_percs_mpi(my_node_mpi_calls, num_processes, percs_mpi);
      mpi_stats_evaluate_mean_sd(num_processes, percs_mpi, &mpi_stats_perc_mpi_mean, &mpi_stats_perc_mpi_sd);
      signature_is_unbalance = (mpi_stats_perc_mpi_sd > LB_TH);
			uint unbalance = enable_load_balance && signature_is_unbalance;

			// signature_is_unbalance = ((my_node_mpi_calls[max_mpi].perc_mpi - my_node_mpi_calls[min_mpi].perc_mpi) > LB_TH);
       verbose_master(2,"%sNode load unbalance%s: LH_TH %lf max perc mpi %lf min perc mpi %lf mean %lf standard deviation %f unbalance ? %u",
                COL_MGT, COL_CLR, LB_TH, percs_mpi[max_mpi],
                percs_mpi[min_mpi], mpi_stats_perc_mpi_mean, mpi_stats_perc_mpi_sd, unbalance);

      if (unbalance){
				 verbose_master(2,"%sPer-core CPUfreq because app is unbalanced%s",COL_RED,COL_CLR);
         freq_per_core = 1;
         nump = num_processes;
				pmpi = 0;
				for (i=0; i < num_processes; i++){
					pmpi += my_node_mpi_calls[i].perc_mpi;
          /*  Decides whether process i is part of the critical path */
          if (abs(percs_mpi[i] - percs_mpi[min_mpi]) < abs(percs_mpi[i] - mpi_stats_perc_mpi_mean)){
              verbose_master(2, "Process %d selected as part of the critical path", i);
              critical_path[i] = 1;
          }else if ((percs_mpi[i] > mpi_stats_perc_mpi_mean) && ((percs_mpi[i]- mpi_stats_perc_mpi_mean) >= ((percs_mpi[max_mpi] - mpi_stats_perc_mpi_mean)*0.75))){ 
						critical_path[i] = 2;
					}
				}	
      }else{ nump = 1;}
    }else nump = 1;
    if (c->use_turbo) min_pstate = 0;
    else min_pstate = frequency_closest_pstate(c->app->max_freq);

    nominal = frequency_pstate_to_freq(min_pstate);
    // Default values

    def_freq = FREQ_DEF(c->app->def_freq);
    def_pstate = frequency_closest_pstate(def_freq);

    // This is the frequency at which we were running
    signature_copy(my_app,sig);
#ifdef POWERCAP
    if (c->pc_limit > 0){
        curr_freq = frequency_closest_high_freq(my_app->avg_f,1);
    }else{
        curr_freq = *(c->ear_frequency);
    }
#else
    curr_freq = *(c->ear_frequency);
#endif

    curr_pstate = frequency_closest_pstate(curr_freq);
    eff_f = frequency_closest_high_freq(my_app->avg_f,1);

		if (pmpi > last_pmpi) verbose_master(2,"%s Warning, more MPI than before (%.2f/%.2f)%s",COL_MGT,last_pmpi,pmpi,COL_CLR);
		last_pmpi = pmpi;

    if (dyn_unc){
        // imc_pstate = (def_imc_max_khz - policy_max_imcfreq)/IMC_STEP;
        ulong curr_imc_freq = avg_to_khz(my_app->sig_ext->imc_freq);
        imc_pstate = (def_imc_max_khz - curr_imc_freq)/IMC_STEP;
        verbose_master(2,"CPU pstate %lu IMC freq %lu IMC pstate %lu",curr_pstate,my_app->sig_ext->imc_freq,imc_pstate);
        copy_imc_data_from_signature(imc_data,curr_pstate,imc_pstate,my_app);
    }
		/* If signature has changed, we must start again */
    if ((min_energy_state != SELECT_CPUFREQ) && signatures_different(&cpufreq_signature, sig, 0.2)){
          verbose_master(2,"%sSignature is too different from the one used for CPU freq, starting again%s",COL_RED,COL_CLR);
          min_energy_state = SELECT_CPUFREQ;
					signature_copy(&cpufreq_signature, sig);
    }

    if ((min_energy_state == SELECT_CPUFREQ) || (dyn_unc == 0)){
				/**** SELECT_CPUFREQ ****/
        if (dyn_unc){
            *ready = EAR_POLICY_TRY_AGAIN;
        }else{ 
            *ready = EAR_POLICY_READY;
        }
        verbose_master(2,"%sSELECT_CPUFREQ%s state dyn_unc %u",COL_BLU,COL_CLR,dyn_unc);
        last_cpu_pstate = curr_pstate;
        last_imc_pstate = imc_pstate;
				is_cpu_bound(my_app, &cbound);
				is_mem_bound(my_app, &mbound);
				verbose_master(2,"App CPU bound %u MEM bound %u",cbound,mbound);

        // If is not the default P_STATE selected in the environment, a projection
        // is made for the reference P_STATE in case the coefficents were available.
        // NEW: This code is new because projections from lower pstates are reporting bad cpu freqs 
				if (!are_default_settings(&last_nodefreq_sel,&min_energy_def_freqs)){
					verbose_node_freqs(2,&last_nodefreq_sel);
					verbose_node_freqs(2,&min_energy_def_freqs);
					set_default_settings(freqs,&min_energy_def_freqs);
					node_freqs_copy(&last_nodefreq_sel,freqs);
					verbose_master(2,"Setting default conf for ME");
					*ready = EAR_POLICY_TRY_AGAIN;
					return EAR_SUCCESS;	
				}
				/* here we are running at the default freqs */
        for (i=0;i<nump;i++){
            verbose_master(2,"Processing proc %d freq per core = %d",i,freq_per_core);
            if (!freq_per_core){
                compute_total_node_instructions(lib_shared_region,sig_shared_region,&my_app->instructions); 
                compute_total_node_flops(lib_shared_region,sig_shared_region,my_app->FLOPS);
                compute_total_node_cycles(lib_shared_region,sig_shared_region,&my_app->cycles);
                compute_vpi(&sig_vpi,sig);
                compute_vpi(&node_sig_vpi,my_app);
                local_process_with_high_vpi = compute_max_vpi(lib_shared_region,sig_shared_region,&max_vpi);
                verbose_master(2,"AVGNodeSig MR[%d] SIGNATURE CPI %.3lf VPI %.3lf/Node CPI %.3lf VPI %.3lf/MAX VPI %.3lf ",my_globalrank,sig->CPI,sig_vpi,(double)my_app->cycles/(double)my_app->instructions,node_sig_vpi,max_vpi);
                my_app->CPI = (double)my_app->cycles/(double)my_app->instructions;
                /* With these two lines, we force the VPI of the signatura to the most loaded in the node */
                my_app->instructions = sig_shared_region[local_process_with_high_vpi].sig.instructions;
                memcpy(my_app->FLOPS,sig_shared_region[local_process_with_high_vpi].sig.FLOPS,sizeof(ull)*FLOPS_EVENTS);
                local_penalty = base_penalty;
            }else{
								compute_ssig_vpi(&lvpi, &sig_shared_region[i].sig);
								ulong eff_p_cpuf, process_avgcpuf;
								process_avg_cpu_freq(&sig_shared_region[i].cpu_mask,cpufreq_data_per_core,&process_avgcpuf);
                from_minis_to_sig(my_app,&sig_shared_region[i].sig);
                verbose_master(2,"Per-processSig MR[%d][%d] CPI %.3lf AvgCPUF %.2f (%lf,%lf) LVPI=%.3lf CP=%u",my_globalrank,i,my_app->CPI,(float)process_avgcpuf/1000000.0,my_node_mpi_calls[i].perc_mpi,my_node_mpi_calls[min_mpi].perc_mpi, lvpi, critical_path[i]);
                if (critical_path[i] == 1){
									/* Critical path */
									if (!mbound){
										eff_p_cpuf = frequency_closest_high_freq(process_avgcpuf,0);
										if (eff_p_cpuf < process_avgcpuf){
											eff_p_cpuf = frequency_closest_high_freq(process_avgcpuf+100000,0);
										}
										if (use_turbo_for_cp){
											new_freq[i] = frequency_pstate_to_freq(0);
										}else{ 
											new_freq[i] = ear_min(eff_p_cpuf, frequency_pstate_to_freq(1));
										}
            				verbose_master(2,"CPU freq for CP process %d is %lu (!MBOUND use_turbo %u MIN_MPI %u)",i,new_freq[i],use_turbo_for_cp,percs_mpi[i] == percs_mpi[min_mpi]);
										continue;
									}else{
										local_penalty = base_penalty / 2;
									}
								}else if (critical_path[i] == 2){
									/* Lot of MPI */
									local_penalty = 0.5;
									sig_shared_region[i].unbalanced = 1;	
								}else{ 
									/* Mix process */
                    local_penalty = base_penalty + EXTRA_TH*(my_node_mpi_calls[i].perc_mpi - my_node_mpi_calls[min_mpi].perc_mpi)/10.0;
                }
								verbose_master(2,"Penalty %f", local_penalty);
            }
						curr_freq = def_freq;
						curr_pstate = def_pstate;
            compute_reference(c,my_app,&curr_freq,&def_freq,&best_freq,&time_ref,&power_ref);
            verbose_master(3,"Time ref %lf Power ref %lf Freq reqf %lu",time_ref,power_ref,best_freq);
            compute_cpu_freq_min_energy(c,my_app,best_freq,time_ref,power_ref,local_penalty,curr_pstate,min_pstate,c->num_pstates,&new_freq[i]);
						if (min_cpufreq) new_freq[i] = ear_max(min_cpufreq, new_freq[i]);
            verbose_master(2,"CPU freq for process %d is %lu",i,new_freq[i]);
						max_cpufreq = ear_max(max_cpufreq, new_freq[i]);
						min_cpufreq_sel = ear_min(min_cpufreq_sel, new_freq[i]);
        }
        if (!freq_per_core){
            for (i=1;i<num_processes;i++) new_freq[i] = new_freq[0];
        }
				/* We check if turbo is an option */
        if ((min_cpufreq_sel == nominal_node) && cbound && try_turbo_enabled){
						turbo_set = 1;
						verbose_master(2,"Using turbo because it is cboun and turbo enabled");
            for (i=0;i<num_processes;i++) new_freq[i] = frequency_pstate_to_freq(0);
				}
        memcpy(last_nodefreq_sel.cpu_freq,new_freq,sizeof(ulong)*num_processes);
				/* We will use this signature to detect changes in the sig compared when the one used to compute the CPU freq */
				signature_copy(&cpufreq_signature, sig);
				/* IMC freq selection */
        if (dyn_unc && !((earl_phase_classification == APP_MPI_BOUND) && network_use_imc)){
            if ((min_cpufreq == nominal_node) && (cbound)){ 
							/* If we are compute bound, and CPU freq is nominal, we can reduce the IMC freq */
							if (sig->GBS <= GBS_BUSY_WAITING) freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = imc_min_khz + IMC_RANGE;
							else 															freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = select_imc_freq(imc_max_khz,imc_min_khz,0.25);
						}else 																					 freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = imc_max_khz;
            if (imc_set_by_hw) 		freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = imc_min_khz;
            else 									freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = freqs->imc_freq[sid*IMC_VAL+IMC_MAX] - IMC_RANGE;
					verbose_master(2,"%sIMC freq selection%s %lu-%lu",COL_GRE,COL_CLR,freqs->imc_freq[sid*IMC_VAL+IMC_MAX],freqs->imc_freq[sid*IMC_VAL+IMC_MIN]);
        }else{
						verbose_master(2,"%sNot IMC freq selection%s : dyn_unc %u MPI_BOUND = %u Network_IMC %u",COL_RED,COL_CLR,dyn_unc,earl_phase_classification == APP_MPI_BOUND,network_use_imc);
						if (dyn_unc && (earl_phase_classification == APP_MPI_BOUND) && network_use_imc){
							freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = imc_max_khz;
							freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = imc_max_khz - IMC_RANGE;
						}
        }
				/* Next state selection */
				if (turbo_set){
          min_energy_state = TRY_TURBO_CPUFREQ;
          *ready = EAR_POLICY_TRY_AGAIN;
        }else{
					if (!dyn_unc){
						*ready = EAR_POLICY_READY;
						min_energy_state = SELECT_CPUFREQ;
					}else{
						i = 0;
						while((i<num_processes) && (last_nodefreq_sel.cpu_freq[i] == def_freq)) i++;
            if ((i < num_processes) || (cbound)){
                min_energy_state = COMP_IMCREF;
            }else{
              ref_imc_pstate = imc_pstate;
              min_energy_state = SELECT_IMCFREQ;
            }
					}
				}
				memcpy(last_nodefreq_sel.imc_freq,freqs->imc_freq,arch_desc.top.socket_count*IMC_VAL*sizeof(ulong));
    }else if ((min_energy_state == COMP_IMCREF) || (min_energy_state == TRY_TURBO_CPUFREQ)){
				/**** COMP_IMCREF ***/
        ulong curr_imc_freq = avg_to_khz(my_app->sig_ext->imc_freq);
        ulong curr_imc_pstate = (def_imc_max_khz - curr_imc_freq)/IMC_STEP;
        ref_imc_pstate = curr_imc_pstate;
        freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = curr_imc_freq;
        //policy_min_imcfreq = policy_max_imcfreq - IMC_RANGE;
        freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = imc_min_khz;
        verbose(2,"%sCOMP_IMCREF OR TRY_TURBO_CPUFREQ%s cpufreq %lu imc %lu-%lu",COL_BLU,COL_CLR,*new_freq,freqs->imc_freq[sid*IMC_VAL+IMC_MAX],freqs->imc_freq[sid*IMC_VAL+IMC_MIN]);
        min_energy_state = SELECT_IMCFREQ;
        *ready = EAR_POLICY_TRY_AGAIN;
				memcpy(freqs->cpu_freq,last_nodefreq_sel.cpu_freq,sizeof(ulong)*num_processes);
				memcpy(last_nodefreq_sel.imc_freq,freqs->imc_freq,arch_desc.top.socket_count*IMC_VAL*sizeof(ulong));
    }else if ((min_energy_state == SELECT_IMCFREQ) && (dyn_unc)){
			/**** IMC_FREQ ***/
        verbose(2,"%sSELECT_IMCFREQ%s state: nominal %d - last CPU %u,IMC %u -- current CPU %lu, IMC %lu",COL_BLU,COL_CLR,min_pstate,last_cpu_pstate,last_imc_pstate,curr_pstate,imc_pstate);
        uint increase_imc = must_increase_imc(imc_data,curr_pstate,imc_pstate,curr_pstate,ref_imc_pstate,my_app,imc_extra_th);
        /* If we are over the limit, we increase the IMC */
        if (increase_imc){
						uint must_start_again = must_start(imc_data,curr_pstate,imc_pstate,curr_pstate,ref_imc_pstate,my_app);
            verbose(2,"%sWarning, passing the limit start_again %u",COL_RED,must_start_again);
            freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = ear_min(freqs->imc_freq[sid*IMC_VAL+IMC_MAX] + IMC_STEP,imc_max_khz);	
            //policy_min_imcfreq = policy_max_imcfreq - IMC_RANGE;
            freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = imc_min_khz;
            verbose(2,"Selecting %lu - %lu and policy ready%s",freqs->imc_freq[sid*IMC_VAL+IMC_MAX],freqs->imc_freq[sid*IMC_VAL+IMC_MIN],COL_CLR);
            if (!must_start_again)  *ready = EAR_POLICY_READY;
						else 										*ready = EAR_POLICY_TRY_AGAIN;
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
				memcpy(freqs->cpu_freq,last_nodefreq_sel.cpu_freq,sizeof(ulong)*num_processes);
				memcpy(last_nodefreq_sel.imc_freq,freqs->imc_freq,arch_desc.top.socket_count*IMC_VAL*sizeof(ulong));

    }
    verbose(2,"Next min_energy state %u next policy state %u",min_energy_state,*ready);
    return EAR_SUCCESS;
}


state_t policy_ok(polctx_t *c,signature_t *curr_sig,signature_t *last_sig,int *ok)
{
    double energy_last, energy_curr;
    uint power_status,next_status;
    ulong eff_f;
		int i;

    if ((c==NULL) || (curr_sig==NULL) || (last_sig==NULL)) return EAR_ERROR;
		*ok = 1;
    if (signatures_different(curr_sig, last_sig,0.2)){
    	verbose_master(2,"Signature is different, aplying the policy again"); 
			*ok = 0;
    }

    return EAR_SUCCESS;
}

state_t policy_get_default_freq(polctx_t *c, node_freqs_t *freq_set)
{
    if (c!=NULL){
			node_freqs_copy(freq_set,&min_energy_def_freqs);
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
		cpu_ready = EAR_POLICY_READY;
    for (i=0;i<num_processes;i++) freqs->cpu_freq[i] = frequency_pstate_to_freq(c->num_pstates - 1);
		memcpy(last_nodefreq_sel.cpu_freq,freqs->cpu_freq,MAX_CPUS_SUPPORTED*sizeof(ulong));
    if (dyn_unc){
        for (sock_id=0;sock_id<arch_desc.top.socket_count;sock_id++){
            freqs->imc_freq[sock_id*IMC_VAL+IMC_MAX] = imc_min_khz;
            freqs->imc_freq[sock_id*IMC_VAL+IMC_MIN] = imc_min_khz;
        }
				memcpy(last_nodefreq_sel.imc_freq,freqs->imc_freq,arch_desc.top.socket_count*IMC_VAL*sizeof(ulong));
    }
#if USE_GPUS
		gpu_ready = EAR_POLICY_READY;
		return EAR_SUCCESS;
		if (gidle){
			gpu_ready = EAR_POLICY_READY;
    	for (uint gpuid=0;gpuid<c->num_gpus;gpuid++){
        freqs->gpu_freq[gpuid] = gpuf_pol_list[gpuid][gpuf_pol_list_items[gpuid]-1];
    	}
			memcpy(last_nodefreq_sel.gpu_freq,freqs->gpu_freq,c->num_gpus*sizeof(ulong));
		}
#else
		gpu_ready = EAR_POLICY_READY;
#endif
    /* GPU mem is pending */

    return EAR_SUCCESS;
}
state_t policy_busy_wait_settings(polctx_t *c,signature_t *my_sig,node_freqs_t *freqs)
{
    int i;
    uint sock_id;
    if (c == NULL) return EAR_ERROR;
		cpu_ready = EAR_POLICY_READY;
    for (i=0;i<num_processes;i++) freqs->cpu_freq[i] = frequency_pstate_to_freq(c->num_pstates - 1);
		memcpy(last_nodefreq_sel.cpu_freq,freqs->cpu_freq,MAX_CPUS_SUPPORTED*sizeof(ulong));
    if (dyn_unc){
        for (uint sock_id=0;sock_id<arch_desc.top.socket_count;sock_id++){
            freqs->imc_freq[sock_id*IMC_VAL+IMC_MAX] = imc_min_khz;
            freqs->imc_freq[sock_id*IMC_VAL+IMC_MIN] = imc_min_khz;
        }
				memcpy(last_nodefreq_sel.imc_freq,freqs->imc_freq,arch_desc.top.socket_count*IMC_VAL*sizeof(ulong));
    }
#if USE_GPUS
		gpu_ready = EAR_POLICY_READY;
		return EAR_SUCCESS;
		if (gidle){
			gpu_ready = EAR_POLICY_READY;
    	for (uint gpuid=0;gpuid<c->num_gpus;gpuid++){
        freqs->gpu_freq[gpuid] = gpuf_pol_list[gpuid][0];
    	}
			memcpy(last_nodefreq_sel.gpu_freq,freqs->gpu_freq,c->num_gpus*sizeof(ulong));
		}
#else
		gpu_ready = EAR_POLICY_READY;
#endif
    /* GPU mem is pending */

    return EAR_SUCCESS;
}
state_t policy_restore_settings(polctx_t *c,signature_t *my_sig,node_freqs_t *freqs)
{
    int i;
    uint sock_id;
    if (c == NULL) return EAR_ERROR;
		node_freqs_copy(freqs, &min_energy_def_freqs);
    node_freqs_copy(&last_nodefreq_sel,freqs);

    return EAR_SUCCESS;
}

state_t policy_new_iteration(polctx_t *c,loop_id_t *loop_id)
{
	int i;
	return EAR_SUCCESS;
}

