/*********************************************************************
 * Copyright (c) 2024 Energy Aware Solutions, S.L
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **********************************************************************/


//#define SHOW_DEBUGS 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>

#include <common/config.h>
#include <common/states.h>
#include <common/output/debug.h>

#include <metrics/flops/flops.h>
#if USE_GPUS
#include <management/gpu/gpu.h>
#include <metrics/gpu/gpu.h>
#endif // USE_GPUS

#include <library/common/externs.h>
#include <library/common/verbose_lib.h>
#include <library/metrics/metrics.h>
#include <library/api/clasify.h>
#include <library/policies/policy_api.h>
#include <library/policies/policy_state.h>
#include <library/models/energy_model.h>


extern uint gpu_ready;
extern uint gpu_optimize;

static const ulong **gpuf_list;
static const uint *gpuf_list_items;
static ulong *gpu_def_freq;
static ulong *last_gpufreq_set;
static int gpu_count;
static int last_pstate = 0;
static double time_penalty;

static uint set_instances_cnt = 0;
static uint dcgmi_ref_set; 

static energy_model_t gpu_energy_model;

static double **power_projections;
static double **time_projections;

static uint GPU_metric_ready(signature_t *sig);
static void GPU_metric_ready_update(signature_t *signature);
//static ulong closet_gpu_def_freq(double avg_gpu_freq);


state_t policy_init(polctx_t *c)
{
	debug("gpu_optimize: policy_init");

	if (!c) return EAR_ERROR;

  // Verify if the dcgmi is loaded
  if(dcgmi_lib_is_enabled()==0)
  {
    gpu_optimize = 0;
    verbose_master(1, "Warning: dcgmi lib is not enabled, the gpu optimize will be deactivated");
    return EAR_SUCCESS;
  }

	ulong g_freq = 0;
#if 0
  char *gpu_freq = ear_getenv(FLAG_GPU_DEF_FREQ);
  if (gpu_freq && c->app->user_type == AUTHORIZED) g_freq = atol(gpu_freq);
#endif

  gpuf_list       = (const ulong **) metrics_gpus_get(MGT_GPU)->avail_list;
  gpuf_list_items = (const uint *)   metrics_gpus_get(MGT_GPU)->avail_count;

  mgt_gpu_data_alloc(&gpu_def_freq);
  mgt_gpu_data_alloc(&last_gpufreq_set);

  gpu_count = c->num_gpus;

  for (int i = 0; i < gpu_count; i++)
  {
    /* The size of our projection matrix is determined bu the maximum number of frequencies. 
    This procedure is not much useful since we are assumming later all GPUs have the same clock list. 
    */
    if (VERB_ON(3))
    {
      verbose_master(3, "#Freqs. in GPU%d: %u", i, gpuf_list_items[i]);
      for (int j = 0; j < gpuf_list_items[i]; j++)
      {
        verbose_master(3, "GPU%d[%d]: %.2f", i, j, (float) gpuf_list[i][j] / 1000000.0);
      }
    }
    
    if (g_freq) gpu_def_freq[i] = g_freq;
    else 				gpu_def_freq[i] = gpuf_list[i][0];
    
    last_gpufreq_set[i] = gpu_def_freq[i];
    
    verbose_master(2, "Default GPU%d frequency: %.2f", i, (float) gpu_def_freq[i] / 1000000.0);
  }
  
  //
  int fp16_info[2];
  dcgmi_lib_get_event_info(fp16_active, &fp16_info);
  int fp16_set = fp16_info[event_info_set];

  int fp32_info[2];
  dcgmi_lib_get_event_info(fp32_active, &fp32_info);
  int fp32_set = fp32_info[event_info_set];

  int fp64_info[2];
  dcgmi_lib_get_event_info(fp64_active, &fp64_info);
  int fp64_set = fp64_info[event_info_set];

  int tensor_info[2];
  dcgmi_lib_get_event_info(tensor_active, &tensor_info);
  int tensor_set = tensor_info[event_info_set];

  int dram_info[2];
  dcgmi_lib_get_event_info(dram_active, &dram_info);
  int dram_set = dram_info[event_info_set];
  
  dcgmi_ref_set = ear_max(ear_max(ear_max(ear_max(fp16_set, fp32_set), fp64_set), tensor_set), dram_set);

  mgt_gpu_freq_limit_set(no_ctx, gpu_def_freq);

  verbose_master(2, "GPU optimization during GPU comp: %s", (gpu_optimize ? "Enabled" : "Disabled"));

  if (gpu_optimize)
  {
    gpu_energy_model = energy_model_load_gpu_model(c->app->user_type, &c->app->installation, &arch_desc);

    if (gpu_energy_model)
    {
      // Warning: We are assuming all GPUs in the node hav the same frequency list
      power_projections = (double **) malloc(gpuf_list_items[0] * sizeof(double *));
      time_projections = (double **) malloc(gpuf_list_items[0] * sizeof(double *));
      for (int i = 0; i < gpuf_list_items[0]; i++)
      {
        power_projections[i] = (double *) calloc(c->num_gpus, sizeof(double));
        time_projections[i] = (double *) calloc(1, sizeof(double *));
      }

      char *gpu_min_energy_pen = ear_getenv(HACK_GPU_POLICY_PENALTY);
      if (gpu_min_energy_pen!=NULL) 
      {
        time_penalty = atof(gpu_min_energy_pen);
      }
      else
      {
        time_penalty = 0.5;
      }
      debug("TIME_PENALTY %lf", time_penalty);
    } 
    else
    {
      error_lib("Loading GPU energy model.");
      gpu_optimize = 0;
      return EAR_ERROR;
    }
  }
  else
  {
    gpu_energy_model = NULL;
  }
  return EAR_SUCCESS;
}


state_t policy_apply(polctx_t *c, signature_t *my_sig, node_freqs_t *freqs, int *ready)
{
  // min_gpu_energy
  int i, ps_to, ps_from;
  double power_ref, power_proj;
  double time_ref, time_proj, time_max;
  double energy_proj, best_solution;
  int best_pstate = 0;
  
  ulong *new_freq=freqs->gpu_freq;

#if USE_GPUS 

#if SHOW_DEBUGS
	debug("apply: num_gpus %d",my_sig->gpu_sig.num_gpus);
  debug("DC_power %lf", my_sig->DC_power); 
  for (i = 0; i < my_sig->gpu_sig.num_gpus; i++) 
  {
	  debug("[GPU %d Power %.2lf Freq %.2f Mem_freq %.2f Util %lu Mem_util %lu]", i,
	  my_sig->gpu_sig.gpu_data[i].GPU_power,
    (float) my_sig->gpu_sig.gpu_data[i].GPU_freq / 1000.0,
    (float) my_sig->gpu_sig.gpu_data[i].GPU_mem_freq/1000.0,
    my_sig->gpu_sig.gpu_data[i].GPU_util, my_sig->gpu_sig.gpu_data[i].GPU_mem_util);
	}
#endif // SHOW_DEBUGS
 
  // get the projections
  if (gpu_optimize && GPU_metric_ready(my_sig))
  {
	  verbose_master(2,"%sGPU Optimize [Min GPU Energy] end %s", COL_BLU, COL_CLR);
    // Get the projections
	  // Warning: We are assuming all GPUs are running at the default frequency, also we are assumming al GPUs have the same clock list.
    ps_from = last_pstate;
	  for (ps_to = 0; ps_to < gpuf_list_items[0]; ps_to++)
	  {
	    if (energy_model_projection_available(gpu_energy_model, ps_from, ps_to))
	    {
	      energy_model_project_power(gpu_energy_model, my_sig, ps_from, ps_to, power_projections[ps_to]);
	      energy_model_project_time(gpu_energy_model, my_sig, ps_from, ps_to, time_projections[ps_to]);
	    }
	  }
    
    // apply the policy
    power_ref = 0;
    for(i = 0; i < my_sig->gpu_sig.num_gpus; i++)
    {
      power_ref += power_projections[0][i];
    }
    time_ref = time_projections[0][0];
    time_max = time_ref + (time_ref * time_penalty);
    best_solution = power_ref*time_ref;
    
    debug("ps %d power_ref %lf time_ref %lf time_max %lf energy_ref %lf",last_pstate, power_ref, time_ref, time_max, best_solution);
    
    for (ps_to = 1; ps_to < gpuf_list_items[0]; ps_to++)
    {
      if (energy_model_projection_available(gpu_energy_model, ps_from, ps_to))
      {
        time_proj = time_projections[ps_to][0];
        power_proj = 0;
        for(i = 0; i <my_sig->gpu_sig.num_gpus; i++)
        {
          power_proj = power_proj + power_projections[ps_to][i];
        }
        // evaluate the min_gpu_energy conditions
        energy_proj = power_proj * time_proj;
        debug("psto %d power_proj %lf time_proj %lf energy_proj %lf",ps_to, power_proj, time_proj, energy_proj);
        if ((energy_proj < best_solution) && (time_proj < time_max)) 
        {
          best_pstate = ps_to;
          best_solution = energy_proj;
          debug("new best solution found %d ",best_pstate);
        }
      }
    } 
   
    // set the selected freq
    last_pstate = best_pstate;
    for(i = 0; i < my_sig->gpu_sig.num_gpus; i++)
    {
      if(my_sig->gpu_sig.gpu_data[i].GPU_util != 0)
      {
        new_freq[i] = gpuf_list[i][best_pstate];
        last_gpufreq_set[i] = new_freq[i];
        debug("GPU[%d] Best P-State %d, freq %lu", i, best_pstate, new_freq[i]);
      }
    }
    
    *ready = EAR_POLICY_TRY_AGAIN;
    GPU_metric_ready_update(my_sig);
	  verbose_master(2,"%sGPU Optimize [Min GPU Energy] end %s", COL_BLU, COL_CLR);
  }
  else  
  {
    debug("GPU not optimized.");
    *ready = EAR_POLICY_READY;
  }
	
#endif // USE_GPUS

	return EAR_SUCCESS;
}


state_t policy_restore_settings(polctx_t *c, signature_t *my_sig, node_freqs_t *freqs)
{
	/* WARNING: my_sig can be NULL */
	if (c == NULL) return EAR_ERROR;

#if USE_GPUS
	verbose_master(2, "Restoring GPU freqs: restore settings");
	if (my_sig == NULL)
	{
		for (int i=0; i < c->num_gpus; i++)
		{
			freqs->gpu_freq[i] = gpu_def_freq[i]; //TODO is it right to set def freq?! => to check!
		}
		return EAR_SUCCESS;
	}
#endif // USE_GPUS
	return EAR_SUCCESS;
}


state_t policy_cpu_gpu_settings(polctx_t *c,signature_t *my_sig,node_freqs_t *freqs)
{
	verbose_master(2,"GPU policy_cpu_gpu_settings");
	return policy_apply(c, my_sig, freqs, (int *)&gpu_ready);
}

state_t policy_io_settings(polctx_t *c, signature_t *my_sig, node_freqs_t *freqs)
{
	verbose_master(2,"GPU policy_io_settings");
	return policy_apply(c, my_sig, freqs, (int *)&gpu_ready);
}

state_t policy_busy_wait_settings(polctx_t *c,signature_t *my_sig,node_freqs_t *freqs)
{
	verbose_master(2,"GPU policy_busy_wait_settings");
	return policy_apply(c, my_sig, freqs, (int *)&gpu_ready);
}

state_t policy_get_default_freq(polctx_t *c, node_freqs_t *freq_set, signature_t *sig)
{

	verbose_master(2,"GPU policy_get_default_freq");
	if (c && freq_set)
	{
		if (c->num_gpus == gpu_count)
		{
			for (int i = 0; i < c->num_gpus; i++)
			{
				freq_set->gpu_freq[i] = gpu_def_freq[i];
			}
			return EAR_SUCCESS;
		} else
		{
			return_msg(EAR_ERROR, "Context #GPUs differs from policy GPU count.");
		}
	} else
	{
		return_msg(EAR_ERROR, Generr.input_null);
	}
}


static uint GPU_metric_ready(signature_t *sig)
{
#if DCGMI
  sig_ext_t *sigex;
  dcgmi_sig_t *dcgmis;
  if (sig == NULL) return 0;
  sigex = (sig_ext_t *)sig->sig_ext;
  if (sigex == NULL)  return 0;
  dcgmis = (dcgmi_sig_t *)&sigex->dcgmis;
  if (dcgmis == NULL) return 0;
  
  int current_instanes = dcgmis->set_instances_cnt[dcgmi_ref_set];

  if ( current_instanes > set_instances_cnt) {
    debug("GPU optimize metric ready, instances %d ", current_instanes);
    return 1;
  }
  else
  { 
    debug("GPU optimize metric NOT ready instances %d", set_instances_cnt);
    return 0;
  }
#else
	return 0;
#endif
}


static void GPU_metric_ready_update(signature_t *signature)
{
#if DCGMI
  sig_ext_t *sigex;
  dcgmi_sig_t *dcgmis;
  if (signature == NULL) return;
  sigex = (sig_ext_t *)signature->sig_ext;
  if (sigex == NULL)  return ;
  dcgmis = (dcgmi_sig_t *)&sigex->dcgmis;
  if (dcgmis == NULL) return ;
  debug("Updating DCGMI number of instances to %d", dcgmis->set_instances_cnt[dcgmi_ref_set]);
  set_instances_cnt = dcgmis->set_instances_cnt[dcgmi_ref_set];
#endif
	return;
}


/*static ulong closet_gpu_def_freq(double avg_gpu_freq)
{
  double current_diff, smallest_diff;
  
  smallest_diff = fabs(avg_gpu_freq - (double)gpuf_list[0][0]);
  ulong closest = 0; 
  
  for (ulong i = 1; i < gpuf_list_items[0]; i++) 
  {
    current_diff = fabs(avg_gpu_freq - (double)gpuf_list[0][i]);
    if (current_diff < smallest_diff) 
    {
      smallest_diff = current_diff;
      closest = i;
    }
    else
    {
      break;
    }
  }
  return closest;
}*/

