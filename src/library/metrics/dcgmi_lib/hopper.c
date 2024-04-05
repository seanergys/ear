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


#ifdef HAVE_CONFIG_H
#include <common/config/config.h>
#endif

#include <library/metrics/dcgmi_lib/hopper.h>

#include <stdlib.h>


state_t dcgmi_lib_hopper_init(dcgmi_lib_t *dcgmi_data)
{
	if (dcgmi_data)
	{
		debug("dcgmi_lib_hopper_init");
		if (state_ok(dcgmi_lib_data_alloc(dcgmi_data, 1)))
		{
			dcgmi_data->dcgmi_set_sizes[0] = DCGMI_LIB_SUPPORTED_EVENTS;

			// clean first all pointers to then safetely call free func. in case of error.
			dcgmi_data->dcgmi_events[0] = NULL;
			dcgmi_data->dcgmi_events_str[0] = NULL;

			// Allocate memory for event set array and event set string
			dcgmi_data->dcgmi_events[0] = (uint *) malloc(dcgmi_data->dcgmi_set_sizes[0] * sizeof(uint));
			dcgmi_data->dcgmi_events_str[0] = (char *) calloc(dcgmi_data->dcgmi_set_sizes[0], ID_SIZE);

			if (!dcgmi_data->dcgmi_events[0] || !dcgmi_data->dcgmi_events_str[0])
			{
				debug("Allocating events and string data");
				free(dcgmi_data->dcgmi_events[0]);
				free(dcgmi_data->dcgmi_events_str[0]);

				free(dcgmi_data->dcgmi_metrics_agg);
				free(dcgmi_data->dcgmi_metrics);

				free(dcgmi_data->dcgmi_events_str);

				free(dcgmi_data->dcgmi_events);
				free(dcgmi_data->dcgmi_set_sizes);

				dcgmi_lib_ret_alloc_err();
			}

			// Set 1
			dcgmi_data->dcgmi_events[0][0]	= gr_engine_active;
			dcgmi_data->dcgmi_events[0][1] 	= sm_active;
			dcgmi_data->dcgmi_events[0][2] 	= sm_occupancy;
			dcgmi_data->dcgmi_events[0][3] 	= tensor_active;
			dcgmi_data->dcgmi_events[0][4] 	= dram_active;
			dcgmi_data->dcgmi_events[0][5] 	= fp64_active;
			dcgmi_data->dcgmi_events[0][6] 	= fp32_active;
			dcgmi_data->dcgmi_events[0][7] 	= fp16_active;
			dcgmi_data->dcgmi_events[0][8] 	= pcie_tx_bytes;
			dcgmi_data->dcgmi_events[0][9] 	= pcie_rx_bytes;
			dcgmi_data->dcgmi_events[0][10] = nvlink_tx_bytes;
			dcgmi_data->dcgmi_events[0][11] = nvlink_rx_bytes;

			return EAR_SUCCESS;
		} else
		{
			char err_msg[64];
			snprintf(err_msg, sizeof(err_msg), "Allocating dcgmi lib data (%s)", state_msg);
			return_msg(EAR_ERROR, err_msg);
		}
	} else
	{
		return_msg(EAR_ERROR, Generr.input_null);
	}
}


/*
state_t dcgmi_lib_hooper_load()
{
// 	dcgmi_instances = calloc(dcgmi_set_count, sizeof(uint));

	gpu_dcgmi_events_to_str(dcgmi_events_ids, DCGMI_LIB_SUPPORTED_EVENTS, dcgmi_events_str);

	gpu_dcgmi_init(dcgmi_events_str, DCGMI_LIB_SUPPORTED_EVENTS);

	gpu_dcgmi_data_alloc(&dcgmi_metrics);
	gpu_dcgmi_data_alloc(&dcgmi_metrics_agg);

	if (state_ok(monitor_init()))
	{
		dcgmi_monitor							= suscription();
		dcgmi_monitor->call_init 	= dcgmi_paction_init;
		dcgmi_monitor->call_main 	= dcgmi_paction;
		// TODO: Below parameters must be configurable
		dcgmi_monitor->time_relax	= 2000;
		dcgmi_monitor->time_burst	= 2000;

		if (state_fail(monitor_register(dcgmi_monitor)))
		{
			module_enabled = 0;
			return_msg(EAR_ERROR, "DCGMI subsctiption failed.");
		}

		module_enabled = 1;

		return EAR_SUCCESS;
	} else
	{
		char err_msg[64];
		snprintf(err_msg, sizeof(err_msg), "Monitor init failed (%s)", state_msg);

		return_msg(EAR_ERROR, err_msg);
	}
}


state_t dcgmi_lib_hooper_get_current_metrics(dcgmi_sig_t *dcgmi_sig)
{
	if (module_enabled)
	{
		if (dcgmi_sig)
		{
			while(ear_trylock(&dcgmi_lock) != EAR_SUCCESS);

			verbose_master(DCGMI_MET_LVL, "DCGMI loop metrics computation...");

			gpu_dcgmi_data_copy(dcgmi_sig->dcgmi_metrics, dcgmi_metrics);

			// dcgmi_sig->dcgmi_instances = dcgmi_instances;

			ear_unlock(&dcgmi_lock);

			return EAR_SUCCESS;
		} else
		{
			return_msg(EAR_ERROR, Generr.input_null);
		}
	} else
	{
			return_msg(EAR_ERROR, HOOPER_MODULE_DISABLED_MSG);
	}
}


state_t dcgmi_lib_hooper_get_global_metrics(dcgmi_sig_t *dcgmi_sig)
{
	if (module_enabled)
	{
		if (dcgmi_sig)
		{
			while(ear_trylock(&dcgmi_lock) != EAR_SUCCESS);

			dcgmi_monitor_stop = 1;

			verbose_master(DCGMI_MET_LVL, "DCGMI accumulated metrics:");

			// 3rd argument is not used
			gpu_dcgmi_data_avg(dcgmi_metrics_agg, elapsed_time, dcgmi_events, DCGMI_LIB_SUPPORTED_EVENTS);

			if (VERB_ON(DCGMI_MET_LVL))
			{
				char *dcgmi_metrics_str = calloc(gpu_dcgmi_gpus_detected() * DCGMI_LIB_SUPPORTED_EVENTS * ID_SIZE, sizeof(char));
				if (dcgmi_metrics_str)
				{
					gpu_dcgmi_data_str(dcgmi_metrics_agg, dcgmi_metrics_str,
							dcgmi_events, dcgmi_set_sizes, DCGMI_LIB_SUPPORTED_EVENTS);

					verbose_master(DCGMI_MET_LVL, "DCGMI app metrics %s elapsed %.3lf sec",
												 dcgmi_metrics_str, elapsed);
				}
				free(dcgmi_metrics_str);
			}

			gpu_dcgmi_data_copy(dcgmi_sig->dcgmi_metrics, dcgmi_metrics_agg);

			ear_unlock(&dcgmi_lock);

			return EAR_SUCCESS;
		} else
		{
			return_msg(EAR_ERROR, Generr.input_null);
		}
	} else
	{
			return_msg(EAR_ERROR, TURING_MODULE_DISABLED_MSG);
	}
}


void dcgmi_lib_hooper_report_total_metrics()
{
	if (module_enabled)
	{
		dcgmi_t total;
		uint    active[DCGMI_LIB_SUPPORTED_EVENTS];
		uint		gpu_count = gpu_dcgmi_gpus_detected();

		* Cleaning *
		memset(&total, 0, sizeof(dcgmi_t));
		memset(&active, 0, sizeof(uint) * DCGMI_LIB_SUPPORTED_EVENTS);

		* Moving from accumulated data, per gpu to total. *

		for (uint gpu_idx = 0; gpu_idx < gpu_count; gpu_idx++)
		{
			for (uint ev = 0; ev < DCGMI_LIB_SUPPORTED_EVENTS; ev++)
			{
				uint index = dcgmi_events[ev] - dcgmi_first_event;
				double event_value = dcgmi_metrics_agg[gpu_idx].values[ev];

				active[index] += (event_value > 0);
				total.values[index] += event_value;
			}
		}

		* Report *
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


void dcgmi_lib_hooper_dcgmi_sig_init(dcgmi_sig_t *dcgmi_sig)
{
	if (dcgmi_sig)
	{
		dcgmi_sig->dcgmi_sets 			= 1;
		dcgmi_sig->dcgmi_gpus 			= gpu_dcgmi_gpus_detected();
		dcgmi_sig->dcgmi_num_ev 		= DCGMI_LIB_SUPPORTED_EVENTS;
		dcgmi_sig->dcgmi_events 		= calloc(1, sizeof(uint *));
		dcgmi_sig->dcgmi_instances	= NULL;
		dcgmi_sig->dcgmi_metrics 	  = calloc(1, sizeof(dcgmi_t *));

		verbose_master(DCGMI_INIT_LVL, "DCGMI: %u GPUs detected", dcgmi_sig->dcgmi_gpus);

		verbose_master(DCGMI_INIT_LVL, "Allocating events...");
		dcgmi_sig->dcgmi_events = calloc(DCGMI_LIB_SUPPORTED_EVENTS, sizeof(uint));

		verbose_master(DCGMI_INIT_LVL, "Allocating metrics...");
		gpu_dcgmi_data_alloc(&dcgmi_sig->dcgmi_metrics);

		* The list of events is static. *
		memcpy(dcgmi_sig->dcgmi_events, dcgmi_events, DCGMI_LIB_SUPPORTED_EVENTS * sizeof(uint));
	}
}


void dcgmi_lib_hooper_reset_instances(dcgmi_sig_t *dcgmi_sig)
{
	return;
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

		* Read and accumulate current set of events. *
		gpu_dcgmi_read(dcgmi_metrics);

		gpu_dcgmi_accum_data(dcgmi_metrics_agg, dcgmi_metrics, time_s,
												 dcgmi_events, DCGMI_LIB_SUPPORTED_EVENTS);

		if (VERB_ON(DCGMI_MET_LVL))
		{
			char *dcgmi_metrics_str = calloc(gpu_dcgmi_gpus_detected() * DCGMI_LIB_SUPPORTED_EVENTS * ID_SIZE, sizeof(char));
			if (dcgmi_metrics_str)
			{
				gpu_dcgmi_data_str(dcgmi_metrics, dcgmi_metrics_str,
													 dcgmi_events, DCGMI_LIB_SUPPORTED_EVENTS);

				verbose_master(DCGMI_MET_LVL, "DCGMI periodic metrics %s elapsed %.3lf sec (total %.3lf sec)", dcgmi_metrics_str, time_s, elapsed_time);
			}
			free(dcgmi_metrics_str);
		}

		dcgmi_instances[dcgmi_curr_ev_set]++;
		dcgmi_curr_ev_set = (dcgmi_curr_ev_set + 1) % dcgmi_set_count;

		* Change the active set of events *
		gpu_dcgmi_set_events(dcgmi_events_str[dcgmi_curr_ev_set], dcgmi_set_sizes[dcgmi_curr_ev_set]);

		ear_unlock(&dcgmi_lock);

	} else
	{
		// TODO: Could we unsibscribe here?
	}
	return EAR_SUCCESS;
}
*/
