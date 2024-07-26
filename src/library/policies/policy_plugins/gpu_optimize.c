/*
*
* This program is part of the EAR software.
*
* EAR provides a dynamic, transparent and ligth-weigth solution for
* Energy management. It has been developed in the context of the
* Barcelona Supercomputing Center (BSC)&Lenovo Collaboration project.
*
* 
* BSC Contact   mailto:ear-support@bsc.es
* 
*
* EAR is an open source software, and it is licensed under both the BSD-3 license
* and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
* and COPYING.EPL files.
*/


#define SHOW_DEBUGS 1

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


#ifdef EARL_RESEARCH
extern unsigned long ext_def_freq;
#define DEF_FREQ(f) (!ext_def_freq?f:ext_def_freq)
#else
#define DEF_FREQ(f) f
#endif

#define FLAG_GPU_METRIC_TO_OPTIMIZE "EAR_GPU_METRIC_TO_OPTIMIZE"

#define metric_is(x) (x == MAX_GPU_GFLOPS_W?"MAX_GPU_GFLOPS_W":(x == MIN_GPU_ENERGY?"MIN_GPU_ENERGY":"UNDEF"))


/* Configuration */
typedef enum {
  MIN_GPU_ENERGY = 0,
	MAX_GPU_GFLOPS_W
} gpu_opt_metric_t;

typedef enum {
  min_value = 0,
	max_value
} gpu_opt_type_t;

extern gpu_state_t last_gpu_state;
extern uint gpu_ready;
extern uint gpu_optimize;

static const ulong **gpuf_list;
static const uint *gpuf_list_items;
static ulong *gpu_def_freq;
static ulong *last_gpufreq_set;
static int gpu_count;
static ulong last_pstate = 0;

static uint set_instances_cnt = 0;
static uint dcgmi_ref_set; 

static energy_model_t gpu_energy_model;

static double **power_projections;
static double **time_projections;

static gpu_opt_metric_t metric_to_optimize = MIN_GPU_ENERGY;
static gpu_opt_type_t   metric_strategy    = min_value;
static char optimize_criteria[1024];

static double time_penalty;

static void update_optimize_options(char *metric);

static uint GPU_metric_ready(signature_t *sig);

static void GPU_metric_ready_update(signature_t *signature);

static uint GPU_ok(uint gpu_idx, signature_t *signature, float curr, float last);

static ulong closet_gpu_def_freq(double avg_gpu_freq);


state_t policy_init(polctx_t *c)
{
	debug("gpu_optimize: policy_init");

	if (!c) return EAR_ERROR;

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
      char *cmetric_to_optimize   = ear_getenv(FLAG_GPU_METRIC_TO_OPTIMIZE);

      /* This function checks the metric */
      update_optimize_options(cmetric_to_optimize);
      // Warning: We are assuming all GPUs in the node hav the same frequency list
      power_projections = (double **) malloc(gpuf_list_items[0] * sizeof(double *));
      time_projections = (double **) malloc(gpuf_list_items[0] * sizeof(double *));
      for (int i = 0; i < gpuf_list_items[0]; i++)
      {
        power_projections[i] = (double *) calloc(c->num_gpus, sizeof(double));
        time_projections[i] = (double *) calloc(1, sizeof(double *));
      }
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
  int i;
  uint metric_ready;
  ulong ps_from, best_pstate;
  double power_ref, time_ref;
  double power_proj, time_proj, energy_proj;
  double time_max, best_solution, avg_gpu_freq;
  
  ulong *new_freq=freqs->gpu_freq;
  

#if USE_GPUS
	verbose_master(2, "%s GPU Optimize start ....%s %s", COL_BLU, optimize_criteria, COL_CLR);

#if SHOW_DEBUGS
	debug("apply: num_gpus %d",my_sig->gpu_sig.num_gpus);
  debug("DC_power %lf", my_sig->DC_power); 
  for (int i = 0; i < my_sig->gpu_sig.num_gpus; i++) 
  {
	  debug("[GPU %d Power %.2lf Freq %.2f Mem_freq %.2f Util %lu Mem_util %lu]", i,
	  my_sig->gpu_sig.gpu_data[i].GPU_power,
    (float) my_sig->gpu_sig.gpu_data[i].GPU_freq / 1000.0,
    (float) my_sig->gpu_sig.gpu_data[i].GPU_mem_freq/1000.0,
    my_sig->gpu_sig.gpu_data[i].GPU_util, my_sig->gpu_sig.gpu_data[i].GPU_mem_util);
	}
#endif // SHOW_DEBUGS
  
  
  signature_t sig_aux;
	signature_copy(&sig_aux, my_sig);

  metric_ready = 0;

  // 	MIN_GPU_ENERGY Policy (applied to all used GPU globally)
  if (gpu_optimize)
  {
    if(GPU_metric_ready(my_sig))
    {
	    metric_ready = 1;
      power_ref = 0;
      avg_gpu_freq = 0;
      for(i=0; i<my_sig->gpu_sig.num_gpus; i++)
      {
        power_ref += sig_aux.gpu_sig.gpu_data[i].GPU_power;
        avg_gpu_freq += (double) sig_aux.gpu_sig.gpu_data[i].GPU_freq;
      }

      avg_gpu_freq = avg_gpu_freq/my_sig->gpu_sig.num_gpus;
      ps_from = closet_gpu_def_freq(avg_gpu_freq);
      debug("ps_from %lu last_pstate %lu avg_gpu_freq %lf def_gpu_freq %lu last_gpu_freq %lu",ps_from, last_pstate, avg_gpu_freq, gpuf_list[0][ps_from], gpuf_list[0][last_pstate]);
      best_pstate = ps_from;
      
      time_ref = sig_aux.time;
      time_max = time_ref + (time_ref * time_penalty);
      best_solution = power_ref*time_ref;
      debug("power_ref %lf time_ref %lf time_max %lf ",power_ref, time_ref, time_max);
      
      for (ulong ps_to=0; ps_to < gpuf_list_items[0]; ps_to++)
      {
        if(ps_from != ps_to){
          if (energy_model_projection_available(gpu_energy_model, ps_from, ps_to))
          {
            // get the projections
            energy_model_project_power(gpu_energy_model, my_sig, ps_from, ps_to, power_projections[ps_to]);
            energy_model_project_time(gpu_energy_model, my_sig, ps_from, ps_to, time_projections[ps_to]);
            
            time_proj = time_projections[ps_to][0];
            power_proj = 0;
            for(i=0; i<my_sig->gpu_sig.num_gpus; i++)
            {
              power_proj = power_proj + power_projections[ps_to][i];
            }
            // evaluate the min_gpu_energy conditions
            energy_proj = power_proj * time_proj;
            debug("power_proj %lf time_proj %lf ",power_proj, time_proj);
            if ((energy_proj < best_solution) && (time_proj < time_max)) 
            {
              best_pstate = ps_to;
              best_solution = energy_proj;
              debug("new best solution found %lu ",best_pstate);
            }
          } //if proj available
        } //if ps_from != ps_to
      }// loop over j=1->gpuf_list_items
      
      // set the selected freq
      last_pstate = best_pstate;
      for(i=0; i<my_sig->gpu_sig.num_gpus; i++)
      {
        new_freq[i] = gpuf_list[i][best_pstate];
	      last_gpufreq_set[i] = new_freq[i];
        debug("GPU[%d] Best P-State %lu, freq %lu",i, best_pstate, new_freq[i]);
      }
    }
    else // GPU metric not ready
    {
      debug("GPU optimize metric NOT ready instances %d", set_instances_cnt);
      //new_freq[i] = last_gpufreq_set[i];
    }
    *ready = EAR_POLICY_TRY_AGAIN;
  }
  else // gpu not optimize
  {
    debug("GPU not optimized.");
    for(i=0; i<my_sig->gpu_sig.num_gpus; i++)
    {
	    new_freq[i] = gpu_def_freq[i];
    }
    *ready = EAR_POLICY_READY;
  }

  if (metric_ready) GPU_metric_ready_update(my_sig);
	
// ulong tutil = 0;
// for (int i = 0; i < my_sig->gpu_sig.num_gpus; i++)
//	{
//	  tutil += my_sig->gpu_sig.gpu_data[i].GPU_util;
//	}
//	if (tutil > 0) last_gpu_state = _GPU_Comp;

	verbose_master(2,"%sGPU Optimize end.%s %s", COL_BLU, optimize_criteria, COL_CLR);
#endif // USE_GPUS

	return EAR_SUCCESS;
}


state_t policy_restore_settings(polctx_t *c, signature_t *my_sig, node_freqs_t *freqs)
{
	/* WARNING: my_sig can be NULL */
	if (c == NULL) return EAR_ERROR;

#if USE_GPUS
	verbose_master(3, "Restoring GPU freqs: restore settings");
	if (my_sig == NULL)
	{
		for (int i=0; i < c->num_gpus; i++)
		{
			freqs->gpu_freq[i] = gpu_def_freq[i];
		}
		return EAR_SUCCESS;
	}

#if 0
  ulong util = 0;
  ulong tutil = 0;
  for (i=0; i<my_sig->gpu_sig.num_gpus; i++) 
  {
    util = my_sig->gpu_sig.gpu_data[i].GPU_util;
    tutil += util;
    
    if (util == 0)
    {
      verbose_master(2, "restore> util = 0 GPU[%d] freq %lu", i, gpuf_list[i][gpuf_list_items[i]-1]);
      freqs->gpu_freq[i] = gpuf_list[i][gpuf_list_items[i]-1];
    }
    else
    if(util !=0)
    {
      verbose_master(2, "restore> GPU[%d] util  %lf freq %lu", i, util, gpu_def_freq[i]);
      freqs->gpu_freq[i] = gpu_def_freq[i];
    }
    
    gpu_opt_st[i] = first_selection;
	  last_gpufreq_set[i] = gpu_def_freq[i];
  }
  
  if (tutil > 0) last_gpu_state = _GPU_Comp;
#endif

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


static void update_optimize_options(char *metric)
{
	if (metric != NULL)
	{
	  if (strcmp(metric, "MAX_GPU_GFLOPS_W") == 0)
    {
      metric_to_optimize = MAX_GPU_GFLOPS_W;
      metric_strategy = max_value;
    }

	  if (strcmp(metric, "MIN_GPU_ENERGY") == 0)
    {
      metric_to_optimize = MIN_GPU_ENERGY;
      metric_strategy = min_value;
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
	}

	sprintf(optimize_criteria,"GPU Metric=%s", metric_is(metric_to_optimize));
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
  else return 0;
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


static uint GPU_ok(uint gpu_idx, signature_t *signature, float curr, float last)
{
	//debug("GPU ok validation curr %f last %f", curr, last);
	switch (metric_strategy)
	{
		case max_value : return (curr > last);
		case min_value : return (curr < last);
		default        : return 1;
	}
	return 0;
}


static ulong closet_gpu_def_freq(double avg_gpu_freq)
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
}
