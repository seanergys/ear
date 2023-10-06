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


#include <library/metrics/dcgmi_lib.h>

#include <stdlib.h>

#include <common/config/config_env.h>
#include <common/config/config_install.h>
#include <common/environment_common.h>
#include <common/system/monitor.h>
#include <common/system/lock.h>

#include <library/common/verbose_lib.h>
#include <library/common/externs.h>



/*
   struct dcgmi_signature_s
   {
   dcgmi_t	**dcgmi_metrics;
   uint   	**dcgmi_events;
   uint		*dcgmi_instances;
   uint		dcgmi_sets;
   uint		dcgmi_gpus;
   uint		dcgmi_num_ev;
   };
   */
#define DCGMI_INIT_LVL	1
#define DCGMI_MET_LVL	3


// #if USE_GPUS
#if DCGMI
#define DCGMI_NUM_EVENTS 4
#define DCGMI_SETS       3
static pthread_mutex_t dcgmi_lock = PTHREAD_MUTEX_INITIALIZER;
static suscription_t*  dcgmi_monitor;
static dcgmi_t     *dcgmi_metrics[DCGMI_SETS];
static dcgmi_t     *dcgmi_metrics_accum[DCGMI_SETS];
static uint        dcgmi_events[DCGMI_SETS][DCGMI_NUM_EVENTS];
static char        dcgmi_events_str[DCGMI_SETS][DCGMI_NUM_EVENTS*64];
static uint        *dcgmi_instances = NULL;
static uint        dcgmi_curr_ev_set = 0;
static char        dcgmi_metrics_str[2048];
static double      dcgmi_elapsed = 0;
static uint        dcgmi_stop = 0;
static uint        dcgmi_lib_enabled = DCGMI_DEFAULT;
#else
static uint        dcgmi_lib_enabled = 0;
static uint        dcgmi_stop = 0;
#endif // DCGMI
// #endif // USE_GPUS


static void dcgmi_static_events_init();


static state_t dcgmi_paction_init(void *p);


static state_t dcgmi_paction(void *p);


state_t dcgmi_lib_load()
{
	if (USE_GPUS && DCGMI)
	{
		verbose_master(1, "GPU and DCGMI are supported.");

		/* First check out whether DCGMI is explicitely requested to be enabled or disabled by the user. */
		char *use_dcgmi = ear_getenv(FLAG_GPU_DCGMI_ENABLED);
		if (use_dcgmi)
		{
			dcgmi_lib_enabled = atoi(use_dcgmi);
			verbose_master(1, "DCGMI metrics %s by environment variable.",
										 dcgmi_lib_enabled ? "enabled" : "disabled");
		}

		/* Init the module */
		if (dcgmi_lib_enabled)
		{
			dcgmi_static_events_init();

			dcgmi_instances = calloc(DCGMI_SETS, sizeof(uint));

			for (uint set = 0; set < DCGMI_SETS; set++)
			{
				gpu_dcgmi_events_to_str(dcgmi_events[set], DCGMI_NUM_EVENTS, dcgmi_events_str[set]);

				/* The init is done only once because both sets have the same size. */
				if (set == 0)
				{
					gpu_dcgmi_init(dcgmi_events_str[set], DCGMI_NUM_EVENTS);
				}

				gpu_dcgmi_data_alloc(&dcgmi_metrics[set]);
				gpu_dcgmi_data_alloc(&dcgmi_metrics_accum[set]);

				verbose_master(DCGMI_INIT_LVL, "DCGMI[%u] initialized with %u events list=%s",
						set, DCGMI_NUM_EVENTS, dcgmi_events_str[set]);
			}

			if (monitor_init() != EAR_SUCCESS)
			{
				dcgmi_lib_enabled = 0;

				char err_msg[64];
				snprintf(err_msg, sizeof(err_msg), "Monitor init failed (%s)", state_msg);

				return_msg(EAR_ERROR, err_msg);
			}

			dcgmi_monitor							= suscription();
			dcgmi_monitor->call_init 	= dcgmi_paction_init;
			dcgmi_monitor->call_main 	= dcgmi_paction;
			// TODO: Below parameters must be configurable
			dcgmi_monitor->time_relax	= 2000;
			dcgmi_monitor->time_burst	= 2000;

			if (state_fail(monitor_register(dcgmi_monitor)))
			{
				dcgmi_lib_enabled = 0;
				return_msg(EAR_ERROR, "DCGMI subsctiption failed.");
			}
		}

		return EAR_SUCCESS;
	} else
	{
		dcgmi_lib_enabled = 0;
		return_msg(EAR_ERROR, "EARL not configured to support GPUs and DCGMI.");
	}
}


uint dcgmi_lib_is_enabled()
{
	return dcgmi_lib_enabled;
}


void dcgmi_lib_dcgmi_sig_init(dcgmi_sig_t *dcgmi_sig)
{
	dcgmi_sig->dcgmi_sets 			= DCGMI_SETS;
	dcgmi_sig->dcgmi_gpus 			= gpu_dcgmi_gpus_detected();
	dcgmi_sig->dcgmi_num_ev 		= DCGMI_NUM_EVENTS;
	dcgmi_sig->dcgmi_events 		= calloc(DCGMI_SETS, sizeof(uint *));
	dcgmi_sig->dcgmi_instances	= calloc(DCGMI_SETS, sizeof(uint));
	dcgmi_sig->dcgmi_metrics 	  = calloc(DCGMI_SETS, sizeof(dcgmi_t *));

	verbose_master(DCGMI_INIT_LVL, "DCGMI: %u GPUs detected", dcgmi_sig->dcgmi_gpus);

	for (uint set = 0; set < DCGMI_SETS; set++)
	{
		verbose_master(DCGMI_INIT_LVL, "Allocating events for set %u", set);
		dcgmi_sig->dcgmi_events[set] = calloc(DCGMI_NUM_EVENTS, sizeof(uint));

		verbose_master(DCGMI_INIT_LVL, "Allocating metrics for set %u", set);
		gpu_dcgmi_data_alloc(&dcgmi_sig->dcgmi_metrics[set]);

		/* The list of events is static. */
		memcpy(dcgmi_sig->dcgmi_events[set], dcgmi_events[set], DCGMI_NUM_EVENTS * sizeof(uint));
	}
}


state_t dcgmi_lib_get_current_metrics(dcgmi_sig_t *dcgmi_sig)
{
	if (dcgmi_sig)
	{
		while(ear_trylock(&dcgmi_lock) != EAR_SUCCESS);

		for (uint set = 0; set < DCGMI_SETS; set++)
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
}


state_t dcgmi_lib_get_global_metrics(dcgmi_sig_t *dcgmi_sig)
{
	if (dcgmi_sig)
	{
		while(ear_trylock(&dcgmi_lock) != EAR_SUCCESS);

		dcgmi_stop = 1;

		for (uint set = 0; set < DCGMI_SETS; set++)
		{
			verbose_master(DCGMI_MET_LVL, "DCGMI[%u] accumulated metrics", set);

			gpu_dcgmi_data_avg(dcgmi_metrics_accum[set], dcgmi_elapsed / DCGMI_SETS,
												 dcgmi_events[set], DCGMI_NUM_EVENTS);

			gpu_dcgmi_data_str(dcgmi_metrics_accum[set], dcgmi_metrics_str,
												 dcgmi_events[set], DCGMI_NUM_EVENTS);

			verbose_master(DCGMI_MET_LVL, "DCGMI app metrics %s elapsed %.3lf sec",
										 dcgmi_metrics_str, dcgmi_elapsed / DCGMI_SETS);

			gpu_dcgmi_data_copy(dcgmi_sig->dcgmi_metrics[set], dcgmi_metrics_accum[set]);
		}

		ear_unlock(&dcgmi_lock);

		return EAR_SUCCESS;
	} else
	{
		return_msg(EAR_ERROR, Generr.input_null);
	}
}


void dcgmi_lib_report_total_metrics()
{
	if (!dcgmi_lib_enabled) return;

	dcgmi_t total;
	uint    active[dcgmi_num_events];
	uint		gpu_count = gpu_dcgmi_gpus_detected();

	/* Cleaning */
	memset(&total, 0, sizeof(dcgmi_t));
	memset(&active, 0, sizeof(uint) *dcgmi_num_events);

	/* Moving from accumulated data, per group and gpu to total. */
	for (uint gpu_idx = 0; gpu_idx < gpu_count; gpu_idx++)
	{
		for (uint set = 0; set < DCGMI_SETS; set++)
		{
			for (uint ev = 0; ev < DCGMI_NUM_EVENTS; ev ++)
			{
				uint index = dcgmi_events[set][ev] - dcgmi_first_event;
				active[index] += (dcgmi_metrics_accum[set][gpu_idx].values[ev] > 0);
				total.values[index] += dcgmi_metrics_accum[set][gpu_idx].values[ev];
			}
		}
	}

	/* Report */
	for (uint ev = 0; ev < dcgmi_num_events; ev++)
	{
		if (total.values[ev] > 0)
		{
			char *event_name = gpu_dcgmi_event_to_name(ev + dcgmi_first_event);
			if (gpu_dcgmi_event_type(ev + dcgmi_first_event) == RATIO)
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


void dcgmi_lib_reset_instances(dcgmi_sig_t *dcgmi_sig)
{
	if (dcgmi_sig)
	{
		for (uint set = 0; set < DCGMI_SETS; set++)
		{
			dcgmi_sig->dcgmi_instances[set] = 0;
		}

		ear_lock(&dcgmi_lock);

		for (uint set = 0; set < DCGMI_SETS; set++)
		{
			dcgmi_instances[set] = 0;
		}

		ear_unlock(&dcgmi_lock);
	}
}


static void dcgmi_static_events_init()
{
	/* Set 0 */
	dcgmi_events[0][0] = tensor_active;
	dcgmi_events[0][1] = fp64_active;
	dcgmi_events[0][2] = dram_active;
	dcgmi_events[0][3] = pcie_tx_bytes;
	/* Set 1 */
	dcgmi_events[1][0] = fp32_active;
	dcgmi_events[1][1] = fp16_active;
	dcgmi_events[1][2] = pcie_rx_bytes;
	dcgmi_events[1][3] = gr_engine_active;
	/* Set 2 */
	dcgmi_events[2][0] = sm_active;
	dcgmi_events[2][1] = sm_occupancy;
	dcgmi_events[2][2] = nvlink_tx_bytes;
	dcgmi_events[2][3] = nvlink_rx_bytes;

}


static state_t dcgmi_paction_init(void *p)
{
	return EAR_SUCCESS;
}


static state_t dcgmi_paction(void *p)
{
	double  time_s = 2.0; // TODO: This value must be configurable

	if (!dcgmi_lib_enabled || dcgmi_stop)
		return EAR_SUCCESS;

	ear_lock(&dcgmi_lock);

	dcgmi_elapsed += time_s;

	/* Read and accumulate current set of events. */
	gpu_dcgmi_read(dcgmi_metrics[dcgmi_curr_ev_set]);
	gpu_dcgmi_accum_data(dcgmi_metrics_accum[dcgmi_curr_ev_set], dcgmi_metrics[dcgmi_curr_ev_set],
			time_s, dcgmi_events[dcgmi_curr_ev_set], DCGMI_NUM_EVENTS);

	gpu_dcgmi_data_str(dcgmi_metrics[dcgmi_curr_ev_set], dcgmi_metrics_str,
			dcgmi_events[dcgmi_curr_ev_set], DCGMI_NUM_EVENTS);

	verbose_master(DCGMI_MET_LVL, "DCGMI[%u] loop metrics %s elapsed %.3lf sec (total %.3lf sec)",
			dcgmi_curr_ev_set, dcgmi_metrics_str, time_s, dcgmi_elapsed);

	dcgmi_instances[dcgmi_curr_ev_set]++;
	dcgmi_curr_ev_set = (dcgmi_curr_ev_set + 1) % DCGMI_SETS;

	/* Change the active set of events */
	gpu_dcgmi_set_events(dcgmi_events_str[dcgmi_curr_ev_set], DCGMI_NUM_EVENTS);

	ear_unlock(&dcgmi_lock);

	return EAR_SUCCESS;
}
