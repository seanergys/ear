/*
 * This program is part of the EAR software.
 *
 * EAR provides a dynamic, transparent and ligth-weigth solution for
 * Energy management. It has been developed in the context of the
 * Barcelona Supercomputing Center (BSC) & Lenovo Collaboration project.
 *
 * Copyright ©		2017-present BSC-Lenovo
 * BSC Contact		mailto:ear-support@bsc.es
 * Lenovo contact mailto:hpchelp@lenovo.com
 *
 * EAR is an open source software, and it is licensed under both the BSD-3 license
 * and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
 * and COPYING.EPL files.
 */


#include <library/metrics/dcgmi_lib/turing.h>

#include <stdlib.h>

#include <common/output/debug.h>


#define DCGMI_SET_COUNT 3
#define DCGMI_SET1_SIZE 10
#define DCGMI_SET2_SIZE 1
#define DCGMI_SET3_SIZE 1


state_t dcgmi_lib_turing_init(dcgmi_lib_t *dcgmi_data, int **event_info, int event_info_rows, int event_info_cols)
{
	if (dcgmi_data && event_info)
	{
		debug("dcgmi_lib_turing_init");
		if (state_ok(dcgmi_lib_data_alloc(dcgmi_data, DCGMI_SET_COUNT)))
		{
			dcgmi_data->dcgmi_set_sizes[0] = DCGMI_SET1_SIZE;
			dcgmi_data->dcgmi_set_sizes[1] = DCGMI_SET2_SIZE;
			dcgmi_data->dcgmi_set_sizes[2] = DCGMI_SET3_SIZE;

			// Allocate memory for each event set array and event set string
			int i = 0;
			while (i < dcgmi_data->dcgmi_set_count)
			{
				// Event set array
				dcgmi_data->dcgmi_events[i] = (uint *) malloc(dcgmi_data->dcgmi_set_sizes[i] * sizeof(uint));

				if (!dcgmi_data->dcgmi_events[i])
				{
					debug("Error allocating DCGMI event set %d", i);
					break;
				}

				// Event set string
				dcgmi_data->dcgmi_events_str[i] = (char *) calloc(dcgmi_data->dcgmi_set_sizes[i], ID_SIZE);

				if (!dcgmi_data->dcgmi_events_str[i])
				{
					debug("Error allocating DCGMI event set string %d", i);
					free(dcgmi_data->dcgmi_events[i]);
					break;
				}

				i++;
			}

			// Check whether there was an error during allocation
			if (i < dcgmi_data->dcgmi_set_count)
			{
				for (int j = 0; j < i; j++)
				{
					free(dcgmi_data->dcgmi_events[j]);
					free(dcgmi_data->dcgmi_events_str[j]);
				}

				dcgmi_lib_ret_alloc_err();
			}

			// Set 1
			dcgmi_data->dcgmi_events[0][0] = sm_active;
			dcgmi_data->dcgmi_events[0][1] = sm_occupancy;
			dcgmi_data->dcgmi_events[0][2] = tensor_active;
			dcgmi_data->dcgmi_events[0][3] = fp32_active;
			dcgmi_data->dcgmi_events[0][4] = dram_active;
			dcgmi_data->dcgmi_events[0][5] = pcie_tx_bytes;
			dcgmi_data->dcgmi_events[0][6] = pcie_rx_bytes;
			dcgmi_data->dcgmi_events[0][7] = gr_engine_active;
			dcgmi_data->dcgmi_events[0][8] = nvlink_tx_bytes;
			dcgmi_data->dcgmi_events[0][9] = nvlink_rx_bytes;

			// Set 2
			dcgmi_data->dcgmi_events[1][0] = fp64_active;

			// Set 3
			dcgmi_data->dcgmi_events[2][0] = fp16_active;

			int turing_event_info[12][2] =
			{
				{0, 7}, // gr_engine_active
				{0, 0}, // sm_active
				{0, 1}, // sm_occupancy
				{0, 2}, // tensor_active
				{0, 4}, // dram_active
				{1, 0}, // fp64_active
				{0, 3}, // fp32_active
				{2, 0}, // fp16_active
				{0, 5}, // pcie_tx_bytes
				{0, 6}, // pcie_rx_bytes
				{0, 8}, // nvlink_tx_bytes
				{0, 9} // nvlink_rx_bytes
			};

			for (i = 0; i < ear_min(event_info_rows, 12); i++)
			{
				for (int j = 0; j < ear_min(event_info_cols, 2); j++)
				{
					event_info[i][j] = turing_event_info[i][j];
				}
			}

			return EAR_SUCCESS;
		} else
		{
			debug("Error allocating dcgmi lib data");
			dcgmi_lib_ret_alloc_err();
		}
	} else
	{
		return_msg(EAR_ERROR, Generr.input_null);
	}
}


// #define TURING_MODULE_DISABLED_MSG "Module not enabled."


// static uint module_enabled;

// static const int dcgmi_set_sizes[] = {DCGMI_SET1_SIZE, DCGMI_SET2_SIZE, DCGMI_SET3_SIZE};
// static const int dcgmi_set_count = DCGMI_SET_COUNT;

/* Below three arrays are used to define dcgmi_events. */
// static int dcgmi_event_set1[] = {
// 	sm_active, sm_occupancy, tensor_active, fp32_active, dram_active,
// 	pcie_tx_bytes, pcie_rx_bytes, gr_engine_active, nvlink_tx_bytes, nvlink_rx_bytes
// };
// static int dcgmi_event_set2[] = {fp64_active};
// static int dcgmi_event_set3[] = {fp16_active};

/* Main (and useful) event sets array. */
// static int *dcgmi_events[] = {
// 	dcgmi_event_set1, dcgmi_event_set2, dcgmi_event_set3
// };

/* Used to define dcgmi_events_str. */
// static char dcgmi_events_str1[DCGMI_SET1_SIZE * ID_SIZE];
// static char dcgmi_events_str2[DCGMI_SET2_SIZE * ID_SIZE];
// static char dcgmi_events_str3[DCGMI_SET3_SIZE * ID_SIZE];

/* Event sets strings array. */
// static char *dcgmi_events_str[] = {
// 	dcgmi_events_str1, dcgmi_events_str2, dcgmi_events_str3
// };

// static dcgmi_t *dcgmi_metrics[DCGMI_SET_COUNT]; // An array of metrics per GPU
// static dcgmi_t *dcgmi_metrics_agg[DCGMI_SET_COUNT]; // An array of metrics (the accumulated) per GPU

// static uint *dcgmi_instances;
// 
// DCGMI monitor variables
// static suscription_t *dcgmi_monitor;
// static uint						dcgmi_curr_ev_set;
// static uint						dcgmi_monitor_stop;
// 
// static pthread_mutex_t dcgmi_lock = PTHREAD_MUTEX_INITIALIZER;
// 
// static double elapsed_time;
// 
// 
// DCGMI monitor subscription init function.
// static state_t dcgmi_paction_init(void *p);
// 
// DCGMI monitor subscription periodic action.
// static state_t dcgmi_paction(void *p);


/*
state_t dcgmi_lib_turing_load()
{
	 Init the module 

	dcgmi_instances = calloc(dcgmi_set_count, sizeof(uint));


	for (uint set = 0; set < dcgmi_set_count; set++)
	{
		gpu_dcgmi_events_to_str(dcgmi_events[set], dcgmi_set_sizes[set], dcgmi_events_str[set]);

		The init is done only once because because it is not needed to do it more than once. *
		if (set == 0)
		{
			gpu_dcgmi_init(dcgmi_events_str[set], dcgmi_set_sizes[set]);
		}

		gpu_dcgmi_data_alloc(&dcgmi_metrics[set]);
		gpu_dcgmi_data_alloc(&dcgmi_metrics_agg[set]);

		// verbose_master(DCGMI_INIT_LVL, "DCGMI[%u] initialized with %u events list=%s",
		// 							 set, DCGMI_NUM_EVENTS, dcgmi_events_str[set]);
	}


	if (state_ok(monitor_init()))
	{
		dcgmi_monitor							= suscription();
		dcgmi_monitor->call_init 	= dcgmi_paction_init;
		dcgmi_monitor->call_main 	= dcgmi_paction;

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


state_t dcgmi_lib_turing_get_current_metrics(dcgmi_sig_t *dcgmi_sig)
{
	if (module_enabled)
	{
		if (dcgmi_sig)
		{
			while(ear_trylock(&dcgmi_lock) != EAR_SUCCESS);

			for (uint set = 0; set < dcgmi_set_count; set++)
			{
				verbose_master(DCGMI_MET_LVL, "DCGMI[%u] loop metrics computation", set);

				gpu_dcgmi_data_copy(dcgmi_sig->dcgmi_metrics[set], dcgmi_metrics[set]);

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
			return_msg(EAR_ERROR, TURING_MODULE_DISABLED_MSG);
	}
}


state_t dcgmi_lib_turing_get_global_metrics(dcgmi_sig_t *dcgmi_sig)
{
	if (module_enabled)
	{
		if (dcgmi_sig)
		{
			while(ear_trylock(&dcgmi_lock) != EAR_SUCCESS);

			dcgmi_monitor_stop = 1;

			// As we are multiplexing profile metrics reading, the elapsed time must be averaged.
			double elapsed = elapsed_time / dcgmi_set_count;

			for (uint set = 0; set < dcgmi_set_count; set++)
			{
				verbose_master(DCGMI_MET_LVL, "DCGMI[%u] accumulated metrics:", set);

				// 3rd argument is not used
				gpu_dcgmi_data_avg(dcgmi_metrics_agg[set], elapsed,
													 dcgmi_events[set], dcgmi_set_sizes[set]);

				if (VERB_ON(DCGMI_MET_LVL))
				{
					char *dcgmi_metrics_str = calloc(gpu_dcgmi_gpus_detected() * dcgmi_set_sizes[set] * ID_SIZE, sizeof(char));
					if (dcgmi_metrics_str)
					{
						gpu_dcgmi_data_str(dcgmi_metrics_agg[set], dcgmi_metrics_str,
															 dcgmi_events[set], dcgmi_set_sizes[set]);

						verbose_master(DCGMI_MET_LVL, "DCGMI app metrics %s elapsed %.3lf sec",
													 dcgmi_metrics_str, elapsed);
					}
					free(dcgmi_metrics_str);
				}

				gpu_dcgmi_data_copy(dcgmi_sig->dcgmi_metrics[set], dcgmi_metrics_agg[set]);
			}

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


void dcgmi_lib_turing_report_total_metrics()
{
	if (module_enabled)
	{
		dcgmi_t total;
		uint    active[DCGMI_LIB_SUPPORTED_EVENTS];
		uint		gpu_count = gpu_dcgmi_gpus_detected();

		* Cleaning *
		memset(&total, 0, sizeof(dcgmi_t));
		memset(&active, 0, sizeof(uint) * DCGMI_LIB_SUPPORTED_EVENTS);

		* Moving from accumulated data, per set and gpu to total. *
		for (uint set_idx = 0; set_idx < dcgmi_set_count; set_idx++)
		{
			uint curr_set_size = dcgmi_set_sizes[set_idx];

			for (uint gpu_idx = 0; gpu_idx < gpu_count; gpu_idx++)
			{
				for (uint ev = 0; ev < curr_set_size; ev++)
				{
					uint index = dcgmi_events[set_idx][ev] - dcgmi_first_event;
					double event_value = dcgmi_metrics_agg[set_idx][gpu_idx].values[ev];

					active[index] += (event_value > 0);
					total.values[index] += event_value;
				}
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


void dcgmi_lib_turing_dcgmi_sig_init(dcgmi_sig_t *dcgmi_sig)
{
	if (dcgmi_sig)
	{
		dcgmi_sig->dcgmi_sets 			= dcgmi_set_count;
		dcgmi_sig->dcgmi_gpus 			= gpu_dcgmi_gpus_detected();
		dcgmi_sig->dcgmi_num_ev 		= DCGMI_LIB_SUPPORTED_EVENTS;
		dcgmi_sig->dcgmi_events 		= calloc(dcgmi_set_count, sizeof(uint *));
		dcgmi_sig->dcgmi_instances	= calloc(dcgmi_set_count, sizeof(uint));
		dcgmi_sig->dcgmi_metrics 	  = calloc(dcgmi_set_count, sizeof(dcgmi_t *));

		verbose_master(DCGMI_INIT_LVL, "DCGMI: %u GPUs detected", dcgmi_sig->dcgmi_gpus);

		for (uint set = 0; set < dcgmi_set_count; set++)
		{
			verbose_master(DCGMI_INIT_LVL, "Allocating events for set %u", set);
			dcgmi_sig->dcgmi_events[set] = calloc(dcgmi_set_sizes[set], sizeof(uint));

			verbose_master(DCGMI_INIT_LVL, "Allocating metrics for set %u", set);
			gpu_dcgmi_data_alloc(&dcgmi_sig->dcgmi_metrics[set]);

			* The list of events is static. *
			memcpy(dcgmi_sig->dcgmi_events[set], dcgmi_events[set], dcgmi_set_sizes[set] * sizeof(uint));
		}
	}
}


void dcgmi_lib_turing_reset_instances(dcgmi_sig_t *dcgmi_sig)
{
	if (dcgmi_sig)
	{
		if (dcgmi_sig->dcgmi_instances)
		{
			for (uint set = 0; set < dcgmi_set_count; set++)
			{
				dcgmi_sig->dcgmi_instances[set] = 0;
			}

			ear_lock(&dcgmi_lock);

			for (uint set = 0; set < dcgmi_set_count; set++)
			{
				dcgmi_instances[set] = 0;
			}

			ear_unlock(&dcgmi_lock);
		}
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

		* Read and accumulate current set of events. *
		gpu_dcgmi_read(dcgmi_metrics[dcgmi_curr_ev_set]);

		gpu_dcgmi_accum_data(dcgmi_metrics_agg[dcgmi_curr_ev_set], dcgmi_metrics[dcgmi_curr_ev_set],
												 time_s, dcgmi_events[dcgmi_curr_ev_set], dcgmi_set_sizes[dcgmi_curr_ev_set]);

		if (VERB_ON(DCGMI_MET_LVL))
		{
			char *dcgmi_metrics_str = calloc(gpu_dcgmi_gpus_detected() * dcgmi_set_sizes[dcgmi_curr_ev_set] * ID_SIZE, sizeof(char));
			if (dcgmi_metrics_str)
			{
				gpu_dcgmi_data_str(dcgmi_metrics[dcgmi_curr_ev_set], dcgmi_metrics_str,
													 dcgmi_events[dcgmi_curr_ev_set], dcgmi_set_sizes[dcgmi_curr_ev_set]);

				verbose_master(DCGMI_MET_LVL, "DCGMI[%u] periodic metrics %s elapsed %.3lf sec (total %.3lf sec)",
						dcgmi_curr_ev_set, dcgmi_metrics_str, time_s, elapsed_time);
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
