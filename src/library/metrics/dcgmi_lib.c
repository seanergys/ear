/*
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


#include <library/metrics/dcgmi_lib.h>

#include <stdlib.h>

#include <common/config/config_env.h>
#include <common/config/config_install.h>
#include <common/environment_common.h>
#include <common/system/lock.h>
#include <common/system/monitor.h>
#include <common/output/debug.h>

#include <library/common/verbose_lib.h>
#include <library/common/externs.h>

#include <library/metrics/dcgmi_lib/turing.h>
#include <library/metrics/dcgmi_lib/hopper.h>
#include <library/metrics/dcgmi_lib/ampere.h>

#include <metrics/gpu/gpu.h>
#include <metrics/common/nvml.h>


#define DCGMI_INIT_LVL		1
#define DCGMI_MET_LVL			1

#define DCGMI_LIB_SAMPLING_PERIOD 2

#define DCGMI_LIB_NOT_SUPPORTED_MSG		"EARL not configured to support GPUs and DCGMI."
#define DCGMI_LIB_MODULE_DISABLED_MSG "DCGMI module not enabled."


typedef state_t (*dcgmi_lib_init_fn) (dcgmi_lib_t *dcgmi_data, int **event_info);

static uint dcgmi_lib_enabled = (USE_GPUS && DCGMI) ? DCGMI_DEFAULT : 0;

static dcgmi_lib_t dcgmi_data;

static uint *dcgmi_instances;

static int event_info[DCGMI_LIB_SUPPORTED_EVENTS][2];

/* DCGMI monitor variables. */ 
static suscription_t *dcgmi_monitor;
static uint						dcgmi_curr_ev_set;
static uint						dcgmi_monitor_stop;

static pthread_mutex_t dcgmi_lock = PTHREAD_MUTEX_INITIALIZER;

static double elapsed_time;


/** DCGMI monitor subscription init function. */
static state_t dcgmi_paction_init(void *p);

/** DCGMI monitor subscription periodic action. */
static state_t dcgmi_paction(void *p);

/** Selects the dcgmi_lib init (allocation) function based on GPU model. */
static state_t set_init_fn(dcgmi_lib_init_fn *dst_fn_ptr);


state_t dcgmi_lib_load()
{
	debug("dcgmi_lib_load");
	if (USE_GPUS && DCGMI)
	{
		verbose_master(DCGMI_INIT_LVL, "GPU and DCGMI are supported.");

		/* First check out whether DCGMI is explicitely requested to be enabled or disabled by the user. */
		char *use_dcgmi = ear_getenv(FLAG_GPU_DCGMI_ENABLED);
		if (use_dcgmi)
		{
			dcgmi_lib_enabled = atoi(use_dcgmi);
			verbose_master(DCGMI_INIT_LVL, "DCGMI metrics %s by environment variable.",
										 dcgmi_lib_enabled ? "enabled" : "disabled");
		}

		/* Init the module */
		if (dcgmi_lib_enabled)
		{
			dcgmi_lib_init_fn init_fn;

			if (state_ok(set_init_fn(&init_fn)))
			{
				// TODO: Allocate and initialize here all data structures based on GPU microarch.
				if (state_ok(init_fn(&dcgmi_data, event_info)))
				{
					dcgmi_instances = calloc(dcgmi_data.dcgmi_set_count, sizeof(uint));

					if (dcgmi_instances)
					{
						/* Common set up */
						for (uint set = 0; set < dcgmi_data.dcgmi_set_count; set++)
						{
							gpu_dcgmi_events_to_str(dcgmi_data.dcgmi_events[set], dcgmi_data.dcgmi_set_sizes[set],
																			dcgmi_data.dcgmi_events_str[set]);

							/* The init is done only once because because it is not needed to do it more times. */
							if (set == 0)
							{
								gpu_dcgmi_init(dcgmi_data.dcgmi_events_str[set], dcgmi_data.dcgmi_set_sizes[set]);
							}

							gpu_dcgmi_data_alloc(&dcgmi_data.dcgmi_metrics[set]);
							gpu_dcgmi_data_alloc(&dcgmi_data.dcgmi_metrics_agg[set]);
						}

						if (state_ok(monitor_init()))
						{
							dcgmi_monitor							= suscription();
							dcgmi_monitor->call_init 	= dcgmi_paction_init;
							dcgmi_monitor->call_main 	= dcgmi_paction;

							/* Configure the sampling period time. */

							float sampling_period = DCGMI_LIB_SAMPLING_PERIOD;

							char *csampling_period = ear_getenv(FLAG_DCGMI_SAMPLING_PERIOD);

							if (csampling_period)
							{
								sampling_period = strtof(csampling_period, NULL);
								if (sampling_period <= 0)
								{
									// If invalid value, reset to the default value
									sampling_period = DCGMI_LIB_SAMPLING_PERIOD;
								}
							}

							// Time burst and relax are the same here.
							dcgmi_monitor->time_relax	= (int) (sampling_period * 1000);  // Miliseconds
							dcgmi_monitor->time_burst	= dcgmi_monitor->time_relax;

							verbose_master(DCGMI_INIT_LVL, "DCGMI library sampling period: %d ms", dcgmi_monitor->time_relax);

							if (state_fail(monitor_register(dcgmi_monitor)))
							{
								dcgmi_lib_enabled = 0;

								return_msg(EAR_ERROR, "Subscription failed.");
							}

							return EAR_SUCCESS;
						} else
						{
							dcgmi_lib_enabled = 0;

							verbose(3, "Monitor init failed (%s)", state_msg)

							return_msg(EAR_ERROR, "Monitor init failed.");
						}
					} else
					{
						dcgmi_lib_enabled = 0;

						// TODO: Free dcgmi_data memory

						dcgmi_lib_ret_alloc_err();
					}
				} else
				{
					dcgmi_lib_enabled = 0;

					verbose(3, "Allocating data (%s)", state_msg);

					return_msg(EAR_ERROR, "Allocating data");
				}
			} else
			{
				dcgmi_lib_enabled = 0;

				char err_msg[64];
				snprintf(err_msg, sizeof(err_msg), "Getting device model: %s", state_msg);

				return_msg(EAR_ERROR, err_msg);
			}
		} else
		{
			return_msg(EAR_ERROR, DCGMI_LIB_MODULE_DISABLED_MSG);
		}
	} else
	{
		dcgmi_lib_enabled = 0;
		return_msg(EAR_ERROR, DCGMI_LIB_NOT_SUPPORTED_MSG);
	}
}


uint dcgmi_lib_is_enabled()
{
	return dcgmi_lib_enabled;
}


void dcgmi_lib_dcgmi_sig_init(dcgmi_sig_t *dcgmi_sig)
{
	if (dcgmi_lib_enabled)
	{
		dcgmi_sig->dcgmi_sets 			= dcgmi_data.dcgmi_set_count;
		dcgmi_sig->dcgmi_gpus 			= gpu_dcgmi_gpus_detected();
		dcgmi_sig->dcgmi_num_ev 		= DCGMI_LIB_SUPPORTED_EVENTS;
		dcgmi_sig->dcgmi_events 		= calloc(dcgmi_sig->dcgmi_sets, sizeof(uint *));
		dcgmi_sig->dcgmi_instances	= calloc(dcgmi_sig->dcgmi_sets, sizeof(uint));
		dcgmi_sig->dcgmi_metrics 	  = calloc(dcgmi_sig->dcgmi_sets, sizeof(dcgmi_t *));

		verbose_master(DCGMI_INIT_LVL, "DCGMI: %u GPUs detected", dcgmi_sig->dcgmi_gpus);

		for (uint set = 0; set < dcgmi_sig->dcgmi_sets; set++)
		{
			// TODO: Allocation error check

			verbose_master(DCGMI_INIT_LVL, "Allocating events for set %u", set);
			dcgmi_sig->dcgmi_events[set] = calloc(dcgmi_data.dcgmi_set_sizes[set], sizeof(uint));

			verbose_master(DCGMI_INIT_LVL, "Allocating metrics for set %u", set);
			gpu_dcgmi_data_alloc(&dcgmi_sig->dcgmi_metrics[set]);

			/* The list of events is static. */
			memcpy(dcgmi_sig->dcgmi_events[set], dcgmi_data.dcgmi_events[set], dcgmi_data.dcgmi_set_sizes[set] * sizeof(uint));
		}
	}
}


state_t dcgmi_lib_get_current_metrics(dcgmi_sig_t *dcgmi_sig)
{
	if (dcgmi_lib_enabled)
	{
		if (dcgmi_sig)
		{
			while(ear_trylock(&dcgmi_lock) != EAR_SUCCESS);

			for (uint set = 0; set < dcgmi_data.dcgmi_set_count; set++)
			{
				verbose_master(DCGMI_MET_LVL, "DCGMI[%u] loop metrics computation", set);

				gpu_dcgmi_data_copy(dcgmi_sig->dcgmi_metrics[set], dcgmi_data.dcgmi_metrics[set]);

				dcgmi_sig->dcgmi_instances[set] = dcgmi_instances[set];
			}

			ear_unlock(&dcgmi_lock);

			return EAR_SUCCESS;
		} else
		{
			return_msg(EAR_ERROR, Generr.input_null);
		}
	} else
	{
			return_msg(EAR_ERROR, DCGMI_LIB_MODULE_DISABLED_MSG);
	}
}


state_t dcgmi_lib_get_global_metrics(dcgmi_sig_t *dcgmi_sig)
{
	if (dcgmi_lib_enabled)
	{
		if (dcgmi_sig)
		{
			while(ear_trylock(&dcgmi_lock) != EAR_SUCCESS);

			dcgmi_monitor_stop = 1;

			// As we are multiplexing profile metrics reading, the elapsed time must be averaged.
			double elapsed = elapsed_time / dcgmi_data.dcgmi_set_count;

			for (uint set = 0; set < dcgmi_data.dcgmi_set_count; set++)
			{
				verbose_master(DCGMI_MET_LVL, "DCGMI[%u] accumulated metrics:", set);

				// 3rd argument is not used
				gpu_dcgmi_data_avg(dcgmi_data.dcgmi_metrics_agg[set], elapsed,
													 dcgmi_data.dcgmi_events[set], dcgmi_data.dcgmi_set_sizes[set]);

				if (VERB_ON(DCGMI_MET_LVL))
				{
					char *dcgmi_metrics_str = calloc(gpu_dcgmi_gpus_detected() * dcgmi_data.dcgmi_set_sizes[set] * ID_SIZE, sizeof(char));
					if (dcgmi_metrics_str)
					{
						gpu_dcgmi_data_str(dcgmi_data.dcgmi_metrics_agg[set], dcgmi_metrics_str,
															 dcgmi_data.dcgmi_events[set], dcgmi_data.dcgmi_set_sizes[set]);

						verbose_master(DCGMI_MET_LVL, "DCGMI app metrics %s elapsed %.3lf sec",
													 dcgmi_metrics_str, elapsed);
					}
					free(dcgmi_metrics_str);
				}

				gpu_dcgmi_data_copy(dcgmi_sig->dcgmi_metrics[set], dcgmi_data.dcgmi_metrics_agg[set]);
			}

			ear_unlock(&dcgmi_lock);

			return EAR_SUCCESS;
		} else
		{
			return_msg(EAR_ERROR, Generr.input_null);
		}
	} else
	{
			return_msg(EAR_ERROR, DCGMI_LIB_MODULE_DISABLED_MSG);
	}
}


void dcgmi_lib_report_total_metrics()
{
	if (dcgmi_lib_enabled)
	{
		dcgmi_t total;
		uint    active[DCGMI_LIB_SUPPORTED_EVENTS];
		uint		gpu_count = gpu_dcgmi_gpus_detected();

		/* Cleaning */
		memset(&total, 0, sizeof(dcgmi_t));
		memset(&active, 0, sizeof(uint) * DCGMI_LIB_SUPPORTED_EVENTS);

		/* Moving from accumulated data, per set and gpu to total. */
		for (uint set_idx = 0; set_idx < dcgmi_data.dcgmi_set_count; set_idx++)
		{
			uint curr_set_size = dcgmi_data.dcgmi_set_sizes[set_idx];

			for (uint gpu_idx = 0; gpu_idx < gpu_count; gpu_idx++)
			{
				for (uint ev = 0; ev < curr_set_size; ev++)
				{
					uint index = dcgmi_data.dcgmi_events[set_idx][ev] - dcgmi_first_event;
					double event_value = dcgmi_data.dcgmi_metrics_agg[set_idx][gpu_idx].values[ev];

					active[index] += (event_value > 0);
					total.values[index] += event_value;
				}
			}
		}

		/* Report */
		for (uint ev = 0; ev < DCGMI_LIB_SUPPORTED_EVENTS; ev++)
		{
			if (total.values[ev] > 0)
			{
				uint event_id = ev + dcgmi_first_event;

				char *event_name = gpu_dcgmi_event_to_name(event_id);

				if (gpu_dcgmi_event_type(event_id) == RATIO)
				{
					verbose_master(DCGMI_INIT_LVL,"DCGMI %s = %.3lf %%  (%u GPUs actives)",
												 event_name, 100 * (total.values[ev] / active[ev]), active[ev]);
				} else
				{
					verbose_master(DCGMI_INIT_LVL, "DCGMI %s = %.3lf KB/sec",
												 event_name, total.values[ev] / 1024.0);
				}
			}
		}
	}
}


void dcgmi_lib_reset_instances(dcgmi_sig_t *dcgmi_sig)
{
	if (dcgmi_lib_enabled)
	{
		if (dcgmi_sig)
		{
			if (dcgmi_sig->dcgmi_instances)
			{
				for (uint set = 0; set < dcgmi_data.dcgmi_set_count; set++)
				{
					dcgmi_sig->dcgmi_instances[set] = 0;
				}

				ear_lock(&dcgmi_lock);

				for (uint set = 0; set < dcgmi_data.dcgmi_set_count; set++)
				{
					dcgmi_instances[set] = 0;
				}

				ear_unlock(&dcgmi_lock);
			}
		}
	}
}


state_t dcgmi_lib_get_event_info(uint event_id, int *event_info_dst)
{
	if (event_info_dst)
	{
		int req_idx = event_id - dcgmi_first_event;
		if (req_idx >= 0 && req_idx < DCGMI_LIB_SUPPORTED_EVENTS)
		{
			memcpy(event_info_dst, event_info[req_idx], 2 * sizeof(int));
			return EAR_SUCCESS;
		} else
		{
			return_msg(EAR_ERROR, Generr.arg_outbounds);
		}
	} else
	{
		return_msg(EAR_ERROR, Generr.input_null);
	}
}


static state_t dcgmi_paction_init(void *p)
{
	return EAR_SUCCESS;
}


static state_t dcgmi_paction(void *p)
{
	if (!dcgmi_monitor_stop)
	{
		double time_s = dcgmi_monitor->time_burst / 1000.0; // From ms to s Old value: 2.0;

		ear_lock(&dcgmi_lock);

		elapsed_time += time_s;

		/* Read and accumulate current set of events. */
		gpu_dcgmi_read(dcgmi_data.dcgmi_metrics[dcgmi_curr_ev_set]);

		gpu_dcgmi_accum_data(dcgmi_data.dcgmi_metrics_agg[dcgmi_curr_ev_set],
												 dcgmi_data.dcgmi_metrics[dcgmi_curr_ev_set],
												 time_s, dcgmi_data.dcgmi_events[dcgmi_curr_ev_set],
												 dcgmi_data.dcgmi_set_sizes[dcgmi_curr_ev_set]);

		if (VERB_ON(DCGMI_MET_LVL))
		{
			size_t nmemb = gpu_dcgmi_gpus_detected() * dcgmi_data.dcgmi_set_sizes[dcgmi_curr_ev_set] * ID_SIZE;

			char *dcgmi_metrics_str = calloc(nmemb, sizeof(char));
			if (dcgmi_metrics_str)
			{
				gpu_dcgmi_data_str(dcgmi_data.dcgmi_metrics[dcgmi_curr_ev_set], dcgmi_metrics_str,
													 dcgmi_data.dcgmi_events[dcgmi_curr_ev_set], dcgmi_data.dcgmi_set_sizes[dcgmi_curr_ev_set]);

				verbose_master(DCGMI_MET_LVL, "DCGMI[%u] periodic metrics %s elapsed %.3lf sec (total %.3lf sec)",
						dcgmi_curr_ev_set, dcgmi_metrics_str, time_s, elapsed_time);

				free(dcgmi_metrics_str);
			}
		}

		dcgmi_instances[dcgmi_curr_ev_set]++;
		dcgmi_curr_ev_set = (dcgmi_curr_ev_set + 1) % dcgmi_data.dcgmi_set_count;

		/* Change the active set of events */
		gpu_dcgmi_set_events(dcgmi_data.dcgmi_events_str[dcgmi_curr_ev_set],
												 dcgmi_data.dcgmi_set_sizes[dcgmi_curr_ev_set]);

		ear_unlock(&dcgmi_lock);

	} else
	{
		// TODO: Could we unsibscribe here?
	}
	return EAR_SUCCESS;
}


static state_t set_init_fn(dcgmi_lib_init_fn *dst_fn_ptr)
{
	if (dst_fn_ptr)
	{
		apinfo_t gpu_info;	
		if (state_ok(gpu_get_info(&gpu_info)))
		{
			switch (gpu_info.dev_model)
			{
				case NVML_DEVICE_ARCH_TURING:
					debug("Turing microarch detected.")
					*dst_fn_ptr = dcgmi_lib_turing_init;
					break;
				case NVML_DEVICE_ARCH_HOPPER:
					debug("Hopper microarch detected.");
					*dst_fn_ptr = dcgmi_lib_hopper_init;
					break;
				case NVML_DEVICE_ARCH_AMPERE:
					debug("Ampere microarch detected.");
					*dst_fn_ptr = dcgmi_lib_ampere_init;
					break;
				default:
					*dst_fn_ptr = NULL;
					return_msg(EAR_ERROR, "Device unknown or not supported.");
			}
		} else
		{
			return_msg(EAR_ERROR, state_msg);
		}
		return EAR_SUCCESS;
	} else
	{
		return_msg(EAR_ERROR, Generr.input_null);
	}
}
