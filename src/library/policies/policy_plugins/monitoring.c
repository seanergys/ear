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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

// #define SHOW_DEBUGS 1
#include <common/config.h>
#include <common/states.h>
#include <common/output/verbose.h>
#include <common/types/projection.h>
#include <common/system/file.h>

#include <management/cpufreq/frequency.h>

#include <daemon/powercap/powercap_status.h>

#include <library/common/externs.h>
#include <library/common/verbose_lib.h>
#include <library/policies/policy_api.h>
#include <library/policies/policy_state.h>
#include <library/policies/common/cpu_support.h>
#include <library/policies/common/gpu_support.h>
#include <library/policies/common/mpi_stats_support.h>

#if !NEW_GPU_API
extern uint gpu_initialized;
#endif

extern uint cpu_ready, gpu_ready;

extern int lib_shared_lock_fd;
static uint total_barriers = 0;
static uint total_barriers_resume = 0;
static uint total_barriers_suspend = 0;

static polsym_t gpus;
state_t policy_init(polctx_t *c)
{
    if (is_mpi_enabled()) {
        mpi_app_init(c);
    }


#if USE_GPUS
#if !NEW_GPU_API
    if (masters_info.my_master_rank >= 0 && gpu_initialized) {
#else
    if (masters_info.my_master_rank >= 0) {
#endif
        if (policy_gpu_init_ctx(c) != EAR_SUCCESS) {
            verbose(2, "%sError%s Loading GPU context.", COL_RED, COL_CLR);
        }
        verbose(2, "RANK %d GPU init", masters_info.my_master_rank);

        if (c->num_gpus) {
            verbose(2, "RANK %d GPUs %d", masters_info.my_master_rank, c->num_gpus);

            if (policy_gpu_load(c->app, &gpus) != EAR_SUCCESS) {
                verbose(2, "%sError%s Loading GPU policy.", COL_RED, COL_CLR);
            }

            if (gpus.init != NULL) {
                gpus.init(c);
            }
        }
    }
#endif
	return EAR_SUCCESS;
}

state_t policy_end(polctx_t *c)
{
  verbose(0,"[%d] Barriers %d Suspend %d resume %d", my_node_id, total_barriers, total_barriers_suspend, total_barriers_resume);
	if (c != NULL) {
        return mpi_app_end(c);
    }
	return EAR_ERROR;
}

state_t policy_apply(polctx_t *c, signature_t *my_sig, node_freqs_t *freqs, int *ready)
{
    ulong *new_freq = freqs->cpu_freq;
    *new_freq= DEF_FREQ(c->app->def_freq);
    /* We copy in shared  memory to be used later in MPI init/end if needed */
    for (uint lp = 0 ; lp < lib_shared_region->num_processes; lp++) sig_shared_region[lp].new_freq = new_freq[0];


    cpu_ready = EAR_POLICY_READY; // TODO: This variable is not used.

    if (gpus.node_policy_apply != NULL) {
        /* Basic support for GPUS */
        gpus.node_policy_apply(c, my_sig, freqs, (int *) &gpu_ready);
    }
    *ready = EAR_POLICY_READY;

    return EAR_SUCCESS;
}

state_t policy_ok(polctx_t *c, signature_t *curr_sig, signature_t *prev_sig, int *ok)
{
    if ((c == NULL) || (curr_sig == NULL) || (prev_sig == NULL)) return EAR_ERROR;

    *ok = 1;

    return EAR_SUCCESS;
}

state_t policy_get_default_freq(polctx_t *c, node_freqs_t *freq_set, signature_t *s)
{
    // TODO: The behaviour is the same done by policy.c by default when a plugin does not have this symbol.
	freq_set->cpu_freq[0] = DEF_FREQ(c->app->def_freq);
#if USE_GPUS
  if (gpus.get_default_freq != NULL){
    gpus.get_default_freq(c, freq_set, s);
  }
#endif

	return EAR_SUCCESS;
}

state_t policy_max_tries(polctx_t *c, int *intents)
{
	*intents = 0;
	return EAR_SUCCESS;
}

state_t policy_domain(polctx_t *c, node_freq_domain_t *domain)
{
    domain->cpu     = POL_GRAIN_NODE;
    domain->mem     = POL_NOT_SUPPORTED;

#if USE_GPUS		
    if (c->num_gpus) {
        domain->gpu = POL_GRAIN_CORE;
    } else {
        domain->gpu = POL_NOT_SUPPORTED;
    }
#else
    domain->gpu     = POL_NOT_SUPPORTED;
#endif

    domain->gpumem  = POL_NOT_SUPPORTED;
    return EAR_SUCCESS;
}

static uint resume_cpufreq = 0;
extern p2i last_buf, last_dest;
extern uint ear_mpi_opt;
state_t policy_mpi_init(polctx_t *c, mpi_call call_type, node_freqs_t *freqs, int *process_id)
{
#if 0
	if (c != NULL) {
        return mpi_call_init(c, call_type);
    }
	return EAR_ERROR;
#endif
    state_t st;
    #if !SINGLE_CONNECTION
    resume_cpufreq = 0;
    pstate_t *sel_pstate, *pstate_list;
    uint currindex;
    if (c != NULL){ 
      st = mpi_call_init(c,call_type);
      #if MPI_OPTIMIZED
      if (ear_mpi_opt){
			uint last = 0;
			/* Barriers (etc) */
			if (is_blocking_collective(call_type) && (lib_shared_region->num_processes > 1)){ 
					total_barriers ++;
					while(file_lock(lib_shared_lock_fd) < 0);
					if (lib_shared_region->processes_in_barrier == 0){
						lib_shared_region->processes_in_barrier = lib_shared_region->num_processes - 1;
						lib_shared_region->reduced_in_barrier = 1;
					}else{
						lib_shared_region->processes_in_barrier--;
						last = (lib_shared_region->processes_in_barrier == 0);
						if (last) lib_shared_region->reduced_in_barrier = 0;
					}
					file_unlock(lib_shared_lock_fd);
					if (last){
						/* If I'm the last one I restore the CPU freq */
						for (uint lp = 0; lp < lib_shared_region->num_processes; lp ++){
							freqs->cpu_freq[lp] = DEF_FREQ(c->app->def_freq);
						}
						*process_id = ALL_PROCESSES;
						total_barriers_resume++;
					}else{
          	pstate_list = metrics_get(MGT_CPUFREQ)->avail_list;
						sel_pstate = &pstate_list[metrics_get(MGT_CPUFREQ)->avail_count-1];
						freqs->cpu_freq[my_node_id] = sel_pstate->khz;
						*process_id = my_node_id;
						total_barriers_suspend++;
					}
			}else{
			//verbose(0, "[%d] checking optimizations perc MPI %.2f", my_node_id, sig_shared_region[my_node_id].mpi_info.perc_mpi);
      /* Check conditions */
			#if 0
      if (sig_shared_region[my_node_id].mpi_info.perc_mpi > MIN_PERC_MPI_OPT){  
        state_t st;
        st = must_be_optimized(call_type, last_buf, last_dest);
        if (st == EAR_SUCCESS){
          /* We should select a low freq. The list of freqs is in metrics_get(MGT_CPUFREQ)->avail_list  */
          pstate_list = metrics_get(MGT_CPUFREQ)->avail_list;
          mgt_cpufreq_get_index(no_ctx,sig_shared_region[my_node_id].new_freq, &currindex , 1);

          sel_pstate = &pstate_list[metrics_get(MGT_CPUFREQ)->avail_count-1];
          freqs->cpu_freq[my_node_id] = sel_pstate->khz;
          *process_id = my_node_id;
          resume_cpufreq = 1;
        }
      }
			#endif
      }
			}
      #endif
      return st;

    } else return EAR_ERROR;
    #else
    if (c != NULL) return mpi_call_init(c,call_type);
    #endif
    return EAR_SUCCESS;

}

state_t policy_mpi_end(polctx_t *c, mpi_call call_type, node_freqs_t *freqs, int *process_id)
{
#if 0
	if (c != NULL) {
        return mpi_call_end(c,call_type);
    }
	return EAR_ERROR;
#endif
    if (c != NULL){ 
      #if !SINGLE_CONNECTION 
      if (ear_mpi_opt ){
				if (resume_cpufreq || (is_blocking_collective(call_type) && lib_shared_region->reduced_in_barrier)){
        	resume_cpufreq = 0;
        	*process_id = my_node_id;
        	freqs->cpu_freq[my_node_id] = sig_shared_region[my_node_id].new_freq;
					lib_shared_region->reduced_in_barrier = 0;
				}
      }
      #endif
      return mpi_call_end(c,call_type);
    }
    else return EAR_ERROR;

}

state_t policy_new_iteration(polctx_t *c, signature_t *sig)
{
	return EAR_SUCCESS;
}
