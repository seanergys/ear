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

#include <library/metrics/dcgmi_lib/common.h>

#include <stdlib.h>
#include <errno.h>

#include <common/output/debug.h>


state_t dcgmi_lib_data_alloc(dcgmi_lib_t *dcgmi_lib_data, int set_count)
{
	debug("dcgmi_lib_data_alloc");
	if (dcgmi_lib_data)
	{
		// Set count
		dcgmi_lib_data->dcgmi_set_count = set_count;

		// clean first all pointers to then safetely call free func. in case of error.
		dcgmi_lib_data->dcgmi_set_sizes		= NULL;
		dcgmi_lib_data->dcgmi_events			= NULL;
		dcgmi_lib_data->dcgmi_events_str	= NULL;
		dcgmi_lib_data->dcgmi_metrics			= NULL;
		dcgmi_lib_data->dcgmi_metrics_agg = NULL;

		// Set sizes array
		dcgmi_lib_data->dcgmi_set_sizes = (int *) malloc(set_count * sizeof(int));

		// Event sets array
		dcgmi_lib_data->dcgmi_events = (uint **) calloc(set_count, sizeof(uint *));

		// Event sets string array
		dcgmi_lib_data->dcgmi_events_str = (char **) calloc(set_count, sizeof(char *));

		// Event sets metrics array
		dcgmi_lib_data->dcgmi_metrics = (dcgmi_t **) malloc(set_count * sizeof(dcgmi_t *));

		// Event sets aggregated metrics array
		dcgmi_lib_data->dcgmi_metrics_agg = (dcgmi_t **) malloc(set_count * sizeof(dcgmi_t *));

		if (dcgmi_lib_data->dcgmi_set_sizes &&
				dcgmi_lib_data->dcgmi_events &&
				dcgmi_lib_data->dcgmi_events_str &&
				dcgmi_lib_data->dcgmi_metrics &&
				dcgmi_lib_data->dcgmi_metrics_agg)
		{
			return EAR_SUCCESS;
		} else
		{
			free(dcgmi_lib_data->dcgmi_set_sizes);

			free(dcgmi_lib_data->dcgmi_events);
			free(dcgmi_lib_data->dcgmi_events_str);

			free(dcgmi_lib_data->dcgmi_metrics);
			free(dcgmi_lib_data->dcgmi_metrics_agg);

#if SHOW_DEBUGS
			if (!dcgmi_lib_data->dcgmi_set_sizes)
			{
				debug("dcgmi_set_sizes: %d", errno);
			}

			if (!dcgmi_lib_data->dcgmi_events)
			{
				debug("dcgmi_events: %d", errno);
			}

			if (!dcgmi_lib_data->dcgmi_events_str)
			{
				debug("dcgmi_events_str: %d", errno);
			}

			if (!dcgmi_lib_data->dcgmi_metrics)
			{
				debug("dcgmi_metrics: %d", errno);
			}

			if (!dcgmi_lib_data->dcgmi_metrics_agg)
			{
				debug("dcgmi_metrics_agg: %d", errno);
			}
#endif // SHOW_DEBUGS

			dcgmi_lib_ret_alloc_err();
		}
	} else
	{
		return_msg(EAR_ERROR, Generr.input_null);
	}
}
