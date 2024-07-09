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

#define metric_is(x)    (x == MAX_GFLOPS_WATT?"MAX_GFLOPS_WATT":(x == MIN_GPU_ENERGY?"MIN_GPU_ENERGY":(x == EDP?"EDP":"UNDEF")))


/* Configuration */
typedef enum {
	MAX_GFLOPS_WATT = 0,
  MIN_GPU_ENERGY,
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

static uint set_instances_cnt = 0;
static uint dcgmi_ref_set; 

static energy_model_t gpu_energy_model;

static double **power_projections;
static double **time_projections;

static gpu_opt_metric_t metric_to_optimize = MIN_GPU_ENERGY;
static gpu_opt_type_t   metric_strategy    = min_value;
static char optimize_criteria[1024];

static void update_optimize_options(char *metric);

static void GPU_get_metric(uint gpu_idx, signature_t *signature, float *curr);

static uint GPU_metric_ready(signature_t *sig);

static void GPU_metric_ready_update(signature_t *signature);

static uint GPU_ok(uint gpu_idx, signature_t *signature, float curr, float last);

// TODO to be replaced by the dcgmi_lib function gflops_cycle or use direcrly the gflops from signature
static float get_gflops_cycle(signature_t *signature, uint gpu_idx);

state_t policy_init(polctx_t *c)
{
	debug("gpu_optimize: policy_init");
  int dram_info[2];
  int fp16_info[2];
  int fp32_info[2];
  int fp64_info[2];
  int tensor_info[2];
	int dram_set, fp16_set, fp32_set, fp64_set, tensor_set;

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

    dcgmi_lib_get_event_info(dram_active, dram_info);
	  dcgmi_lib_get_event_info(fp16_active, fp16_info);
	  dcgmi_lib_get_event_info(fp32_active, fp32_info);
	  dcgmi_lib_get_event_info(fp64_active, fp64_info);
	  dcgmi_lib_get_event_info(tensor_active, tensor_info);
    dram_set = dram_info[event_info_set];
    fp16_set = fp16_info[event_info_set];
    fp32_set = fp32_info[event_info_set];
    fp64_set = fp64_info[event_info_set];
    tensor_set = tensor_info[event_info_set];
    
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
  // min_gpu_energy
  int i, j, best_pstate;
  uint metric_ready, total_ready, tutil;
  ulong util;
  double power_ref, time_ref, energy_ref;
  double power_proj, time_proj, energy_proj;
  double time_max, penalty, best_solution;
  
  
  ulong *new_freq=freqs->gpu_freq;
  *ready = EAR_POLICY_READY;
  
  // The min_gpu_energy threashold 
  char *gpu_min_energy_pen = ear_getenv(HACK_GPU_POLICY_PENALTY);
  if (gpu_min_energy_pen!=NULL) 
  {
    penalty = atof(gpu_min_energy_pen);
  }else{
	  penalty = 0.5;
  }

	debug("PENALTY %lf",penalty);

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
	
  if (gpu_optimize)
  {
    if(GPU_metric_ready(my_sig))
    {
	    metric_ready = 1;
      best_pstate = 0;
      power_ref = 0;
      time_ref = sig_aux.time;
      time_max = time_ref + (time_ref * penalty);
      
      for(i=0; i<my_sig->gpu_sig.num_gpus; i++)
      {
        power_ref += sig_aux.gpu_sig.gpu_data[i].GPU_power;
      }
      
      best_solution = power_ref*time_ref;
      debug("power_ref %lf time_ref %lf time_max %lf ",power_ref, time_ref, time_max);
      
      for (j=1; j<gpuf_list_items[0]; j++)
      {
        if (energy_model_projection_available(gpu_energy_model, 0, j))
        {
          // get the projections
          energy_model_project_power(gpu_energy_model, my_sig, 0, j, power_projections[j]);
          energy_model_project_time(gpu_energy_model, my_sig, 0, j, time_projections[j]);
          
          time_proj = time_projections[j][0];
          power_proj = 0;
          for(i=0; i<my_sig->gpu_sig.num_gpus; i++)
          {
            power_proj = power_proj + power_projections[j][i];
          }
          // evaluate the min_gpu_energy conditions
          energy_proj = power_proj * time_proj;
          debug("power_proj %lf time_proj %lf, ",energy_proj, time_proj);
          if ((energy_proj < best_solution) && (time_proj < time_max)) 
          {
            best_pstate = j;
            best_solution = energy_proj;
            debug("new best solution found %d",best_pstate);
          }
        } //if proj available
      }// loop over j=1->gpuf_list_items
      
      // set the selected freq
      for(i=0; i<my_sig->gpu_sig.num_gpus; i++)
      {
        new_freq[i] = gpuf_list[i][best_pstate];
	      last_gpufreq_set[i] = new_freq[i];
        debug("GPU[%d] Best P-State %u, freq %lu",i, best_pstate, new_freq[i]);
      }

      *ready = EAR_POLICY_TRY_AGAIN;
    }
    else // GPU metric not ready
    {
      for(i=0; i<my_sig->gpu_sig.num_gpus; i++)
      {
        util = my_sig->gpu_sig.gpu_data[i].GPU_util;
        debug("GPU[%d] optimize metric NOT ready instances %d, util %lf", i, set_instances_cnt, util);
        //new_freq[i] = last_gpufreq_set[i];
        *ready = EAR_POLICY_TRY_AGAIN;
      }
    }
  }
  else // gpu not optimize
  {
    debug("GPU not optimized.");
    for(i=0; i<my_sig->gpu_sig.num_gpus; i++)
    {
	    new_freq[i] = gpu_def_freq[i];
    }
  }

	if (metric_ready) GPU_metric_ready_update(my_sig);
	
  tutil = 0;
	for (int i = 0; i < my_sig->gpu_sig.num_gpus; i++)
	{
	  tutil += my_sig->gpu_sig.gpu_data[i].GPU_util;
	}
	if (tutil > 0) last_gpu_state = _GPU_Comp;

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
		if (strcmp(metric, "MAX_GFLOPS_WATT") == 0)  metric_to_optimize = MAX_GFLOPS_WATT;
		if (strcmp(metric, "MIN_GPU_ENERGY") == 0)
    {
      metric_to_optimize = MIN_GPU_ENERGY;
      metric_strategy = 2;
    }
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
  float gpu_gflops;
  
  // TODO in CPU_GFLOPS_W use total gpu_power (from all used GPUs) ?
  *curr = 0;
  switch(metric_to_optimize)
  {
#if DCGMI
    case MAX_GFLOPS_WATT: 
    {
      gpu_gflops = get_gflops_cycle(signature, gpu_idx);
      gpu_freq = (float)(signature->gpu_sig.gpu_data[gpu_idx].GPU_freq/1000000.0);
      gpu_power = (float) signature->gpu_sig.gpu_data[gpu_idx].GPU_power;
      float node_power = (float) signature->DC_power;
		  float cpu_gflops = (float) signature->Gflops;    
      
      debug("GPU[%d] GPU_FREQ %f CPU_GFLOPS %f GPU_GFLOPS %f", gpu_idx, gpu_freq, cpu_gflops, gpu_gflops*gpu_freq); 
      debug("GPU[%d] CPU_GFLOPS_W %f GPU_GFLOPS_W %f", gpu_idx, cpu_gflops/node_power, (gpu_gflops*gpu_freq)/gpu_power); 
      
      *curr = cpu_gflops/node_power + (gpu_gflops*gpu_freq)/gpu_power; 
      break;
    }
    // TODO EDP and/or ED2P once the time model is ready
#endif
    default: *curr = (float) 0;
  }
}

static uint GPU_metric_ready(signature_t *sig)
{
	switch(metric_to_optimize)
	{
		case EDP :return 1;
    case MIN_GPU_ENERGY:
		case MAX_GFLOPS_WATT :
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
#endif
		}
	}
	return 0;
}

static void GPU_metric_ready_update(signature_t *signature)
{
  switch(metric_to_optimize){
	  case EDP            : return;
    case MIN_GPU_ENERGY :
	  case MAX_GFLOPS_WATT:
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
		}
	}
	return;
}

// optimanl gpu freq selection (min ed2p or max flops/w)
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

// Return FP activies by second for each GPU
static float get_gflops_cycle(signature_t *signature, uint gpu_idx)
{
	if (signature == NULL) return 0;
	
  sig_ext_t *sigex = (sig_ext_t *) signature->sig_ext;
  if (sigex == NULL)  return 0;

#if DCGMI
  dcgmi_sig_t * dcgmis = (dcgmi_sig_t *) &sigex->dcgmis;
	if (dcgmis == NULL) return 0;
  
  int fp64_info[2];
	dcgmi_lib_get_event_info(fp64_active, fp64_info);
	gpuprof_t *dcgmis_data_64 = dcgmis->event_metrics_sets[fp64_info[event_info_set]];

	int fp32_info[2];
	dcgmi_lib_get_event_info(fp32_active, fp32_info);
	gpuprof_t *dcgmis_data_32 = dcgmis->event_metrics_sets[fp32_info[event_info_set]];

	int fp16_info[2];
	dcgmi_lib_get_event_info(fp16_active, fp16_info);
	gpuprof_t *dcgmis_data_16 = dcgmis->event_metrics_sets[fp16_info[event_info_set]];

  int tensor_info[2];
	dcgmi_lib_get_event_info(tensor_active, tensor_info);
	gpuprof_t *dcgmis_data_ten = dcgmis->event_metrics_sets[tensor_info[event_info_set]];

  float fp16 = (float)(dcgmis_data_16[gpu_idx].values[fp16_info[event_info_pos]]);
  float fp32 = (float)(dcgmis_data_32[gpu_idx].values[fp32_info[event_info_pos]]);
  float fp64 = (float)(dcgmis_data_64[gpu_idx].values[fp64_info[event_info_pos]]);
  float tensor = (float)(dcgmis_data_ten[gpu_idx].values[tensor_info[event_info_pos]]);

	debug("GPU[%u] data for optimization: fp64 %f fp32 %f fp16 %f tensor %f", gpu_idx, fp64, fp32, fp16, tensor);
  
  //return 6912*fp64 + 13824*fp32 + 55296*fp16 + 110592*tensor; //A100
  return (16384*fp64 + 32768*fp32 + 65536*fp16 + 32768*tensor)/100; //H100

#else
	return 0;
#endif
}
