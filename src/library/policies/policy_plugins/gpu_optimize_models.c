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


#define SHOW_DEBUGS 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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


#define FLAG_GPU_METRIC_TO_OPTIMIZE   "EAR_GPU_METRIC_TO_OPTIMIZE"

#define metric_is(x)    (x == SYS_FLOPS_W?"SYS_FLOPS_W":(x == GPU_FLOPS_W?"GPU_FLOPS_W":(x == EDP?"EDP":"UNDEF")))


/* Configuration */
typedef enum {
	GPU_FLOPS_W = 0,
	SYS_FLOPS_W,
	EDP
} gpu_opt_metric_t;

typedef enum {
	max_value = 0,
  min_value
} gpu_opt_type_t;

extern gpu_state_t last_gpu_state;
extern uint gpu_ready;
extern uint gpu_optimize;

static const ulong **gpuf_list;
static const uint *gpuf_list_items;
static ulong *gpu_def_freq;
static ulong *last_gpufreq_set;
static int gpu_count;

static uint dcgmi_instances;
static uint dcgmi_ref_set;

static energy_model_t gpu_energy_model;

static double **power_projections;

static gpu_opt_metric_t metric_to_optimize = GPU_FLOPS_W;
static gpu_opt_type_t   metric_strategy    = max_value;
static char optimize_criteria[1024];

static void update_optimize_options(char *metric);

static void GPU_get_metric(uint gpu_idx, signature_t *signature, float *curr);

static uint GPU_metric_ready(signature_t *sig, uint gpu_idx);

static uint GPU_metric_ready_all(signature_t *sig);

static void GPU_metric_ready_update(signature_t *signature);

static uint GPU_ok(uint gpu_idx, signature_t *signature, float curr, float last);

static float get_fp_activities(signature_t *signature, uint gpu_idx);

state_t policy_init(polctx_t *c)
{
  
	//debug("gpu_optimize_models: policy_init");
	if (c)
	{
		ulong g_freq = 0;
#if 0
		char *gpu_freq = ear_getenv(FLAG_GPU_DEF_FREQ);
		if (gpu_freq && c->app->user_type == AUTHORIZED)
		{
			g_freq = atol(gpu_freq);
		}
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

		mgt_gpu_freq_limit_set(no_ctx, gpu_def_freq);

		verbose_master(2, "gpu_optimize_models> GPU optimization during GPU comp: %s", (gpu_optimize ? "Enabled" : "Disabled"));

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
				for (int i = 0; i < gpuf_list_items[0]; i++)
				{
					power_projections[i] = (double *) calloc(c->num_gpus, sizeof(double));
				}

			} else
			{
				error_lib("Loading GPU energy model.");
				gpu_optimize = 0;

				return EAR_ERROR;
			}
		} else
		{
			gpu_energy_model = NULL;
		}

		return EAR_SUCCESS;

	} else
	{
		return EAR_ERROR;
	}
}


state_t policy_apply(polctx_t *c, signature_t *my_sig, node_freqs_t *freqs, int *ready)
{
  ulong *new_freq=freqs->gpu_freq;
  ulong util, tutil = 0;
  *ready = EAR_POLICY_READY;

#if USE_GPUS
	verbose_master(2, "%s GPU Optimize start ....%s %s", COL_BLU, optimize_criteria, COL_CLR);

#if SHOW_DEBUGS
	debug("apply: num_gpus %d",my_sig->gpu_sig.num_gpus);
  for (int i = 0; i < my_sig->gpu_sig.num_gpus; i++) 
  {
	  debug("[GPU %d Power %.2lf Freq %.2f Mem_freq %.2f Util %lu Mem_util %lu]", i,
	  my_sig->gpu_sig.gpu_data[i].GPU_power,
    (float) my_sig->gpu_sig.gpu_data[i].GPU_freq / 1000.0,
    (float) my_sig->gpu_sig.gpu_data[i].GPU_mem_freq/1000.0,
    my_sig->gpu_sig.gpu_data[i].GPU_util, my_sig->gpu_sig.gpu_data[i].GPU_mem_util);
	}
#endif // SHOW_DEBUGS
  
	if (gpu_optimize && GPU_metric_ready_all(my_sig))
  {
	  // Warning: We are assuming all GPUs are running at the default frequency, also we are assumming al GPUs have the same clock list.
	  for (int i = 0; i < gpuf_list_items[0]; i++)
	  {
	    if (energy_model_projection_available(gpu_energy_model, 0, i))
	    {
	      energy_model_project_power(gpu_energy_model, my_sig, 0, i, power_projections[i]);
	    }
	  }
	}

	signature_t sig_aux;
	signature_copy(&sig_aux, my_sig);

	// when using DCGMI, we must wait until the data is ready
	uint metric_ready = 0;
	for (int i = 0; i < my_sig->gpu_sig.num_gpus; i++)
	{
	  util = my_sig->gpu_sig.gpu_data[i].GPU_util;
	  if (util == 0) 
    {
	    // We set the minimum GPU frequency if the Utilization is 0
	    new_freq[i] = gpuf_list[i][gpuf_list_items[i]-1];
	  } 
    else
	  {
	    if (!gpu_optimize)
	    {
	      debug("GPU not optimized.");
	      new_freq[i] = gpu_def_freq[i];
	    } 
      else
	    {
	      if (GPU_metric_ready(my_sig, i))
	      {
	        verbose_master(2, "GPU[%d] optimize metric ready instances %d", i, dcgmi_instances);
	        metric_ready = 1;
          
	        int best_pstate = 0;
          float current_metric, last_metric;
	        sig_aux.gpu_sig.gpu_data[i].GPU_freq = gpuf_list[i][0];
          GPU_get_metric(i, &sig_aux, &last_metric);
          
	        for (int j = 1; j < gpuf_list_items[i]; j++)
	        {
	          if (power_projections[j][i] != 0)
	          {
              debug("GPU[%d] pstate %d gfreq %ld power_projections %lf", i, j, gpuf_list[i][j], power_projections[j][i]);
	            sig_aux.gpu_sig.gpu_data[i].GPU_power = power_projections[j][i];
	            sig_aux.gpu_sig.gpu_data[i].GPU_freq = gpuf_list[i][j];
              GPU_get_metric(i, &sig_aux, &current_metric);
              if (GPU_ok(i, &sig_aux, current_metric, last_metric))
              {
                last_metric = current_metric;
	              best_pstate = j;
              } 
	          }
          }// for loop
	        
          new_freq[i] = gpuf_list[i][best_pstate];
	        debug("Best p-state %u, freq %lu, metric %f", best_pstate, new_freq[i], last_metric);
         
	      }
        else
	      {
	        // GPU data not ready yet
	        new_freq[i] = last_gpufreq_set[i];
	        *ready = EAR_POLICY_TRY_AGAIN;
	      }
	      tutil += util; // Accumulate GPU utilization
	    }
	  }
	  last_gpufreq_set[i] = new_freq[i];
	}

	if (metric_ready) GPU_metric_ready_update(my_sig);

	if (tutil > 0) last_gpu_state = _GPU_Comp;

	verbose_master(2,"%sGPU Optimize end ....%s %s", COL_BLU, optimize_criteria, COL_CLR);
#endif // USE_GPUS
	return EAR_SUCCESS;
}

state_t policy_restore_settings(polctx_t *c,signature_t *my_sig,node_freqs_t *freqs)
{
	/* WARNING: my_sig can be NULL */
	if (c == NULL) return EAR_ERROR;

#if USE_GPUS
	if (my_sig == NULL)
	{
		verbose_master(2, "Restoring GPU freqs: restore settings");

		for (int i = 0 ; i < c->num_gpus ; i++)
		{
			freqs->gpu_freq[i] = gpu_def_freq[i];
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
		if (strcmp(metric, "SYS_FLOPS_W") == 0)  metric_to_optimize = SYS_FLOPS_W;
		if (strcmp(metric, "GPU_FLOPS_W") == 0)  metric_to_optimize = GPU_FLOPS_W;
		if (strcmp(metric, "EDP") == 0)
    {
      metric_to_optimize = EDP;
      metric_strategy = min_value;
    }
	}

	sprintf(optimize_criteria,"GPU Metric=%s", metric_is(metric_to_optimize));
}

static void GPU_get_metric(uint gpu_idx, signature_t *signature, float *curr)
{
  float gpu_freq;
  float gpu_power;
  *curr = 0;
  switch(metric_to_optimize)
  {
#if DCGMI
    case GPU_FLOPS_W:
    {
      gpu_freq = (float)(signature->gpu_sig.gpu_data[gpu_idx].GPU_freq/1000000.0);
      gpu_power = (float) signature->gpu_sig.gpu_data[gpu_idx].GPU_power;
      *curr = gpu_freq/gpu_power;
      break;
    } 
    case SYS_FLOPS_W: 
    {
      float fp_act = get_fp_activities(signature, gpu_idx);
		  float cpu_gflops = (float) signature->Gflops;    
      // TODO use Hz instead of GHz 
      gpu_freq = (float)(signature->gpu_sig.gpu_data[gpu_idx].GPU_freq/1000000.0);
      // TODO in CPU_FLOPS_W use total gpu_power (from all used GPUs)
      gpu_power = (float) signature->gpu_sig.gpu_data[gpu_idx].GPU_power;
      debug("GPU_FREQ %f CPU_FLOPS_W %f GPU_FLOPS_W %f",gpu_freq, cpu_gflops/(signature->DC_power - gpu_power), (fp_act*gpu_freq)/gpu_power); 
      *curr = cpu_gflops/(signature->DC_power - gpu_power) + (fp_act*gpu_freq)/gpu_power; 
      break;
    }
    // TODO EDP and ED2P once the time model is ready
#endif
    default: *curr = (float) 0;
  }
}

static uint GPU_metric_ready(signature_t *sig, uint gpu_idx)
{
	switch(metric_to_optimize)
	{
		case EDP :return 1;
		case SYS_FLOPS_W :
		case GPU_FLOPS_W :
		{
#if DCGMI
      sig_ext_t *sigex;
      dcgmi_sig_t *dcgmis;
      if (sig == NULL) return 0;
      sigex = (sig_ext_t *)sig->sig_ext;
      if (sigex == NULL)  return 0;
      dcgmis = (dcgmi_sig_t *)&sigex->dcgmis;
      if (dcgmis == NULL) return 0;
      if (dcgmis->dcgmi_instances[dcgmi_ref_set] > dcgmi_instances)  return 1;
      else return 0;
#endif
		}
	}
	return 0;
}

static uint GPU_metric_ready_all(signature_t *sig)
{
	for (int i = 0; i < sig->gpu_sig.num_gpus; i++){
    debug("gpu[%d] GPU_metric_ready_all\n", i);
    if(GPU_metric_ready(sig, i) == 0) return 0;
  }
  return 1;
}

static void GPU_metric_ready_update(signature_t *signature)
{
  switch(metric_to_optimize){
	  case EDP         : return ;
	  case SYS_FLOPS_W :
	  case GPU_FLOPS_W :
    {
#if DCGMI
      sig_ext_t *sigex;
			dcgmi_sig_t *dcgmis;
			if (signature == NULL) return;
			sigex = (sig_ext_t *)signature->sig_ext;
			if (sigex == NULL)  return ;
			dcgmis = (dcgmi_sig_t *)&sigex->dcgmis;
			if (dcgmis == NULL) return ;
			verbose_master(2,"Updating DCGMI number of instances to %d", dcgmis->dcgmi_instances[dcgmi_ref_set]);
			dcgmi_instances = dcgmis->dcgmi_instances[dcgmi_ref_set];
#endif
		}
	}
	return;
}

// optimanl gpu freq selection (min ed2p or max flops/w)
static uint GPU_ok(uint gpu_idx, signature_t *signature, float curr, float last)
{
	verbose_master(2, "GPU ok validation curr %f last %f", curr, last);
	switch (metric_strategy)
	{
		case max_value : return (curr > last);
		case min_value : return (curr < last);
		default        : return 0;
	}
	return 0;
}


static float get_fp_activities(signature_t *signature, uint gpu_idx)
{
	if (signature == NULL) return 0;

	sig_ext_t * sigex = (sig_ext_t *) signature->sig_ext;
	if (sigex == NULL)  return 0;

#if DCGMI
	dcgmi_sig_t *dcgmis = (dcgmi_sig_t *) &sigex->dcgmis;
	if (dcgmis == NULL) return 0;

  int fp64_info[2];
	dcgmi_lib_get_event_info(fp64_active, fp64_info);
	dcgmi_t *dcgmis_data_64 = dcgmis->dcgmi_metrics[fp64_info[event_info_set]];

	int fp32_info[2];
	dcgmi_lib_get_event_info(fp32_active, fp32_info);
	dcgmi_t *dcgmis_data_32 = dcgmis->dcgmi_metrics[fp32_info[event_info_set]];

	int fp16_info[2];
	dcgmi_lib_get_event_info(fp16_active, fp16_info);
	dcgmi_t *dcgmis_data_16 = dcgmis->dcgmi_metrics[fp16_info[event_info_set]];

  int tensor_info[2];
	dcgmi_lib_get_event_info(tensor_active, tensor_info);
	dcgmi_t *dcgmis_data_ten = dcgmis->dcgmi_metrics[tensor_info[event_info_set]];

  verbose_master(2, "GPU[%u] computing CPU gflops...", gpu_idx);
  float fp16 = (float)(dcgmis_data_16[gpu_idx].values[fp16_info[event_info_pos]]*16);
  float fp32 = (float)(dcgmis_data_32[gpu_idx].values[fp32_info[event_info_pos]]*32);
  float fp64 = (float)(dcgmis_data_64[gpu_idx].values[fp64_info[event_info_pos]]*64);
  float tensor = (float)(dcgmis_data_ten[gpu_idx].values[tensor_info[event_info_pos]]*128);

  return fp16 + fp32 + fp64 + tensor;
#else
	return 0;
#endif
}
