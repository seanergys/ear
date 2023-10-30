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
#define FLAG_GPU_STRATEGY_TO_OPTIMIZE "EAR_GPU_STRATEGY_TO_OPTIMIZE"

#define metric_is(x)    (x == UTIL_W?"UTIL_W":(x == FLOPS_W?"FLOPS_W":(x == GFLOPS_W?"GPU_GFLOPS_W":(x == SM_OCC?"SM_OCC":"UNDEF"))))
#define strategy_is(x)  (x == max_value?"MAX":"UNDEF")


/* Configuration */
typedef enum {
	UTIL_W      = 0,
	FLOPS_W     = 1,
	GFLOPS_W    = 2,
	SM_OCC      = 3,
} gpu_opt_metric_t;

typedef enum {
	max_value = 0,
} gpu_opt_type_t;

typedef enum
{
	first_selection = 1,
	partial_ready
} gpu_opt_state_t;


extern gpu_state_t last_gpu_state;
extern uint gpu_ready;
extern uint gpu_optimize;

static const ulong **gpuf_list;
static const uint *gpuf_list_items;
static ulong *gpu_def_freq;
static ulong *last_gpufreq_set;
static int gpu_count;

static gpu_opt_state_t *gpu_opt_st;
// static uint *GPU_policy_state;
static signature_t prev_gpu_sig;
static float *last_value;
static uint dcgmi_instances;
static uint dcgmi_ref_set;

static energy_model_t gpu_energy_model;

static double **power_projections;

static gpu_opt_metric_t metric_to_optimize = UTIL_W;
static gpu_opt_type_t   metric_strategy    = max_value;
static char optimize_criteria[1024];

static void update_optimize_options(char *metric, char *strategy);

static void GPU_get_metric(uint gpu_idx, signature_t *signature, float *curr);

static uint GPU_metric_ready(signature_t *sig, uint gpu_idx);

static void GPU_metric_ready_update(signature_t *signature);

static uint GPU_ok(uint gpu_idx, signature_t *signature, float curr, float last);

static float get_sm_occupancy(signature_t *signature, uint gpu_idx);

static float estimate_system_gflops(signature_t *signature, uint gpu_idx);


state_t policy_init(polctx_t *c)
{
	if (c)
	{
		ulong g_freq = 0;

		char *gpu_freq = ear_getenv(FLAG_GPU_DEF_FREQ);
		if (gpu_freq && c->app->user_type == AUTHORIZED)
		{
			g_freq = atol(gpu_freq);
		}

		gpuf_list       = (const ulong **) metrics_gpus_get(MGT_GPU)->avail_list;
		gpuf_list_items = (const uint *)   metrics_gpus_get(MGT_GPU)->avail_count;

		gpu_opt_st = (gpu_opt_state_t *) calloc(c->num_gpus, sizeof(gpu_opt_state_t));

		mgt_gpu_data_alloc(&gpu_def_freq);
		mgt_gpu_data_alloc(&last_gpufreq_set);

		gpu_count = c->num_gpus;

		for (int i = 0; i < gpu_count; i++)
		{
			// The size of our projection matrix is determined bu the maximum number of frequencies. This procedure is not much useful since we are assumming later all GPUs
			// have the same clock list.

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

			gpu_opt_st[i] = first_selection;
		}

		mgt_gpu_freq_limit_set(no_ctx, gpu_def_freq);

		verbose_master(2, "GPU optimization during GPU comp: %s", (gpu_optimize ? "Enabled" : "Disabled"));

		if (gpu_optimize)
		{

			gpu_energy_model = energy_model_load_gpu_model(c->app->user_type, &c->app->installation, &arch_desc);
			if (gpu_energy_model)
			{
				char *cmetric_to_optimize   = ear_getenv(FLAG_GPU_METRIC_TO_OPTIMIZE);
				char *cstrategy_to_optimize = ear_getenv(FLAG_GPU_STRATEGY_TO_OPTIMIZE);

				/* This function checks the metric and strategy (max, min) */
				update_optimize_options(cmetric_to_optimize, cstrategy_to_optimize);

				last_value        = (float *) calloc(c->num_gpus, sizeof(float));

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
	ulong util, tutil = 0;;
	*ready = EAR_POLICY_READY;

#if USE_GPUS
	verbose_master(2, "%sGPU Optimize start ....%s %s", COL_BLU, optimize_criteria, COL_CLR);

#if SHOW_DEBUGS
	debug("apply: num_gpus %d",my_sig->gpu_sig.num_gpus);
	for (int i = 0; i < my_sig->gpu_sig.num_gpus; i++) {
		debug("[GPU %d Power %.2lf Freq %.2f Mem_freq %.2f Util %lu Mem_util %lu]", i,
				my_sig->gpu_sig.gpu_data[i].GPU_power,
				(float) my_sig->gpu_sig.gpu_data[i].GPU_freq / 1000.0,
				(float) my_sig->gpu_sig.gpu_data[i].GPU_mem_freq/1000.0,
				my_sig->gpu_sig.gpu_data[i].GPU_util, my_sig->gpu_sig.gpu_data[i].GPU_mem_util);
	}
#endif // SHOW_DEBUGS

	if (gpu_optimize)
	{
		// TODO: Warning: We are assuming all GPUs are running at the default frequency, also we are assumming al GPUs have the same clock list.
		for (int i = 0; i < gpuf_list_items[0]; i++)
		{
			if (energy_model_projection_available(gpu_energy_model, gpu_def_freq[0], gpuf_list[0][i]))
			{
				energy_model_project_power(gpu_energy_model, my_sig, gpu_def_freq[0], gpuf_list[0][i], power_projections[i]);
			}
		}
	}

	signature_t sig_aux;
	signature_copy(&sig_aux, my_sig);

	/* when using DCGMI, we must wait until the data is ready */
	uint metric_ready = 0;
	for (int i = 0; i < my_sig->gpu_sig.num_gpus; i++)
	{
		util = my_sig->gpu_sig.gpu_data[i].GPU_util;
		if (util == 0) {
			// We set the minimum GPU frequency if the Utilization is 0
			new_freq[i] = gpuf_list[i][gpuf_list_items[i]-1];
		} else
		{
			if (!gpu_optimize)
			{
				debug("GPU not optimized.");
				new_freq[i] = gpu_def_freq[i];
			} else
			{
				if (GPU_metric_ready(my_sig, i))
				{
					verbose_master(2, "GPU[%d] optimize metric ready instances %d", i, dcgmi_instances);
					metric_ready = 1;

					/* GPU data ready */
					if (gpu_opt_st[i] & first_selection)
					{
						debug("Selecting optimal P-State for GPU %d", i);

						float current_metric; // TODO: The metric you are going to project
						GPU_get_metric(i, my_sig, &current_metric); // TODO: Do some error check

						uint best_pstate = last_gpufreq_set[i];

						for (int j = 0; j < gpuf_list_items[i]; j++)
						{
							if (power_projections[j][i] != 0)
							{
								sig_aux.gpu_sig.gpu_data[i].GPU_power = power_projections[j][i];

								// TODO: Fill the aux signature with the projected metrics
								// e.g., sig_aux.gpu_sig.gpu_data[i].GPU_power = projected_power;

								float new_metric;
								GPU_get_metric(i, my_sig, &new_metric); // TODO: Do some error check

								if (GPU_ok(i, &sig_aux, new_metric, current_metric))
								{
									best_pstate = j;
								}
							}
						}

						/* We save here valuable data for later usage. */

						last_value[i] = current_metric;
						signature_copy(&prev_gpu_sig, my_sig);

						new_freq[i] = gpuf_list[i][best_pstate];

						gpu_opt_st[i] = partial_ready;
						// *ready = EAR_POLICY_TRY_AGAIN;
						debug("Best p-state %u, freq %lu", best_pstate, new_freq[i]);

					} else if (gpu_opt_st[i] & partial_ready)
					{
						// As for the GPU i the policy is ready, we set again the last selected frequency.
						new_freq[i] = last_gpufreq_set[i];
					} else
					{
						// We should never reach this error.
						return EAR_ERROR;
					}
				} else
				{
					/* GPU data not ready yet */
					new_freq[i] = last_gpufreq_set[i];
					*ready = EAR_POLICY_TRY_AGAIN;
				}
				tutil += util; // Accumulate GPU utilization
			}
		}
		last_gpufreq_set[i] = new_freq[i];
	}

	if (metric_ready) GPU_metric_ready_update(my_sig);

	/* We compute if all the gpus are done */
	uint total_ready = 0;
	for (int i = 0; i < my_sig->gpu_sig.num_gpus; i++)
	{
		total_ready += (gpu_opt_st[i] & partial_ready);
	}

	if (total_ready == my_sig->gpu_sig.num_gpus)
	{
		for (int i = 0; i < my_sig->gpu_sig.num_gpus; i++) gpu_opt_st[i] = first_selection;
	}

	if (tutil > 0) {
		last_gpu_state = _GPU_Comp;
	}

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


static void update_optimize_options(char *metric, char * strategy)
{
	if (metric != NULL)
	{
		if (strcmp(metric, "UTIL_W") == 0)       metric_to_optimize = UTIL_W;
		if (strcmp(metric, "GFLOPS_W") == 0)     metric_to_optimize = FLOPS_W;
		if (strcmp(metric, "GPU_GFLOPS_W") == 0) metric_to_optimize = GFLOPS_W;
		if (strcmp(metric, "SM_OCC") == 0)			 metric_to_optimize = SM_OCC;
	}

	if (strategy != NULL)
	{
		if (strcmp(strategy, "MAX") == 0)       metric_strategy = max_value;
	}

	sprintf(optimize_criteria,"GPU Metric=%s GPU Strategy=%s", metric_is(metric_to_optimize), strategy_is(metric_strategy));
}


static void GPU_get_metric(uint gpu_idx, signature_t *signature, float *curr)
{
	*curr = 0;
	switch(metric_to_optimize)
	{
		case UTIL_W:
			*curr = (float) (signature->gpu_sig.gpu_data[gpu_idx].GPU_util *
							last_gpufreq_set[gpu_idx]) / (float) signature->DC_power;
			break;
		case SM_OCC:
										{
#if DCGMI
											float sm_occ = get_sm_occupancy(signature, gpu_idx);
											debug("sm occupancy: %f", sm_occ);
											*curr = sm_occ * last_gpufreq_set[gpu_idx]/(float)signature->DC_power;
#endif
											break;
										}
		case GFLOPS_W:
		case FLOPS_W  : {
#if DCGMI
											float gflops = estimate_system_gflops(signature, gpu_idx);
											*curr = gflops / (float) signature->DC_power;
											verbose_master(2, " GPU[%u] data for optimization: %f", gpu_idx, *curr);
											break;
#else
											*curr = (float) 0;
#endif
										}
		default    :  *curr = (float) 0;
	}
}


static uint GPU_metric_ready(signature_t *sig, uint gpu_idx)
{
	switch(metric_to_optimize)
	{
		case UTIL_W   :return 1;
		case GFLOPS_W :
		case FLOPS_W  :
		case SM_OCC   :
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


static void GPU_metric_ready_update(signature_t *signature)
{
	switch(metric_to_optimize){
		case UTIL_W   :return ;
		case FLOPS_W  :
		case GFLOPS_W :
		case SM_OCC   :
									 {
#if DCGMI
										 sig_ext_t *sigex;
										 dcgmi_sig_t *dcgmis;
										 if (signature == NULL) return ;
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


static uint GPU_ok(uint gpu_idx, signature_t *signature, float curr, float last)
{
	verbose_master(2, "GPU ok validation curr %f last %f", curr, last);
	switch (metric_strategy)
	{
		case max_value : return (curr > last);
		default        : return 0;
	}
	return 0;
}


static float get_sm_occupancy(signature_t *signature, uint gpu_idx)
{
	if (signature == NULL) return 0;

	sig_ext_t * sigex = (sig_ext_t *) signature->sig_ext;
	if (sigex == NULL)  return 0;

#if DCGMI
	dcgmi_sig_t * dcgmis = (dcgmi_sig_t *) &sigex->dcgmis;
	if (dcgmis == NULL) return 0;

	int sm_occ_info[2];
	dcgmi_lib_get_event_info(sm_occupancy, sm_occ_info);

	dcgmi_t *dcgmis_data_sm = dcgmis->dcgmi_metrics[sm_occ_info[event_info_set]];
	return (float) (dcgmis_data_sm[gpu_idx].values[sm_occ_info[event_info_pos]]);
#else
	return 0;
#endif
}


static float estimate_system_gflops(signature_t *signature, uint gpu_idx)
{
	if (signature == NULL) return 0;

	sig_ext_t *sigex = (sig_ext_t *) signature->sig_ext;
	if (sigex == NULL)  return 0;

	dcgmi_sig_t * dcgmis = (dcgmi_sig_t *) &sigex->dcgmis;
	if (dcgmis == NULL) return 0;

	float cpu_gflops = 0;
	float f64,f32,f16;

	int fp64_info[2];
	dcgmi_lib_get_event_info(fp64_active, fp64_info);
	dcgmi_t *dcgmis_data_64 = dcgmis->dcgmi_metrics[fp64_info[event_info_set]];

	int fp32_info[2];
	dcgmi_lib_get_event_info(fp32_active, fp32_info);
	dcgmi_t *dcgmis_data_32 = dcgmis->dcgmi_metrics[fp32_info[event_info_set]];

	int fp16_info[2];
	dcgmi_lib_get_event_info(fp16_active, fp16_info);
	dcgmi_t *dcgmis_data_16 = dcgmis->dcgmi_metrics[fp16_info[event_info_set]];

	verbose_master(2, "GPU[%u] computing CPU gflops...", gpu_idx);

	if (metric_to_optimize == FLOPS_W) {
		cpu_gflops = (float) signature->Gflops;
	}
	verbose_master(2, "GPU[%u] computing GPU gflops...", gpu_idx);

	f64 = (float) (dcgmis_data_64[gpu_idx].values[fp64_info[event_info_pos]]*64);
	f32 = (float) (dcgmis_data_32[gpu_idx].values[fp32_info[event_info_pos]]*32);
	f16 = (float) (dcgmis_data_16[gpu_idx].values[fp16_info[event_info_pos]]*16);

	verbose_master(2, "GPU[%u] data for optimization: %f CPU-Flops GPU (%f 64 %f 32 %f 16) elapsed %f", gpu_idx, cpu_gflops, f64, f32, f16, sigex->elapsed);

	return cpu_gflops + (f64 + f32 + f16) * last_gpufreq_set[gpu_idx] / (float) 1000000.0;
}
