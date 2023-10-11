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

#include <library/metrics/dcgmi_lib/t4.h>


#define DCGMI_LIB_NOT_SUPPORTED_MSG		"EARL not configured to support GPUs and DCGMI."
#define DCGMI_LIB_MODULE_DISABLED_MSG "DCGMI module not enabled."


static pthread_mutex_t dcgmi_lock = PTHREAD_MUTEX_INITIALIZER;

static uint dcgmi_lib_enabled = (USE_GPUS && DCGMI) ? DCGMI_DEFAULT : 0;


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

			if (state_fail(dcgmi_lib_t4_load()))
			{
				char err_msg[64];
				snprintf(err_msg, sizeof(err_msg), "Loading T4 module (%s)", state_msg);
				return_msg(EAR_ERROR, err_msg);
			}
		}

		return EAR_SUCCESS;

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
