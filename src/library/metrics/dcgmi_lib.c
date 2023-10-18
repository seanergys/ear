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
#include <common/system/lock.h>

#include <library/common/verbose_lib.h>
#include <library/common/externs.h>

#include <library/metrics/dcgmi_lib/turing.h>


#define DCGMI_LIB_NOT_SUPPORTED_MSG		"EARL not configured to support GPUs and DCGMI."
#define DCGMI_LIB_MODULE_DISABLED_MSG "DCGMI module not enabled."


struct dcgmi_lib_sym_s
{
	state_t (*load) ();
	state_t (*get_current_metrics) (dcgmi_sig_t *dcgmi_sig);
	state_t (*get_global_metrics) (dcgmi_sig_t *dcgmi_sig);
	void (*report_total_metrics) ();
	void (*dcgmi_sig_init) (dcgmi_sig_t *dcgmi_sig);
	void (*reset_instances) (dcgmi_sig_t *dcgmi_sig);
};


static uint dcgmi_lib_enabled = (USE_GPUS && DCGMI) ? DCGMI_DEFAULT : 0;


/*
static int *dcgmi_set_sizes;
static int	dcgmi_set_count;

static int **dcgmi_events;
static char **dcgmi_events_str;

static dcgmi_t **dcgmi_metrics;
static dcgmi_t **dcgmi_metrics_agg;

*/

static dcgmi_lib_t dcgmi_data;

static uint *dcgmi_instances;

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


state_t dcgmi_lib_load()
{
	if (USE_GPUS && DCGMI)
	{
		verbose_master(DCGMI_INIT_LVL, "GPU and DCGMI are supported.");

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
			// TODO: Allocate and initialize here all data structures.
			if (state_ok(dcgmi_lib_t4_load()))
			{
				/* Common set up */
				for (uint set = 0; set < dcgmi_set_count; set++)
				{
					gpu_dcgmi_events_to_str(dcgmi_events[set], dcgmi_set_sizes[set], dcgmi_events_str[set]);

					/* The init is done only once because because it is not needed to do it more times. */
					if (set == 0)
					{
						gpu_dcgmi_init(dcgmi_events_str[set], dcgmi_set_sizes[set]);
					}

					gpu_dcgmi_data_alloc(&dcgmi_metrics[set]);
					gpu_dcgmi_data_alloc(&dcgmi_metrics_agg[set]);
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

				verbose(3, "Allocating data (%s)", state_msg);

				return_msg(EAR_ERROR, "Allocating data");
			}
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
		dcgmi_lib_t4_dcgmi_sig_init(dcgmi_sig);
	}
}


state_t dcgmi_lib_get_current_metrics(dcgmi_sig_t *dcgmi_sig)
{
	if (dcgmi_lib_enabled)
	{
		if (dcgmi_sig)
		{
			if (state_ok(dcgmi_lib_t4_get_current_metrics(dcgmi_sig)))
			{
				return EAR_SUCCESS;
			} else
			{
				char err_msg[64];
				snprintf(err_msg, sizeof(err_msg), "Getting current metrics from T4 module (%s)", state_msg);

				return_msg(EAR_ERROR, err_msg);
			}
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
			if (state_ok(dcgmi_lib_t4_get_global_metrics(dcgmi_sig)))
			{
				return EAR_SUCCESS;
			} else
			{
				char err_msg[64];
				snprintf(err_msg, sizeof(err_msg), "Getting global metrics from T4 module (%s)", state_msg);

				return_msg(EAR_ERROR, err_msg);
			}
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
		dcgmi_lib_t4_report_total_metrics();
	}
}


void dcgmi_lib_reset_instances(dcgmi_sig_t *dcgmi_sig)
{
	if (dcgmi_lib_enabled)
	{
		dcgmi_lib_t4_reset_instances(dcgmi_sig);
	}
}
