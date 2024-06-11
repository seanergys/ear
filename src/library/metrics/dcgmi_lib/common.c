/*
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



#include <library/metrics/dcgmi_lib/common.h>

#include <stdlib.h>
#include <errno.h>

#include <common/output/debug.h>
#include <metrics/gpu/gpuprof.h>


state_t dcgmi_lib_data_alloc(dcgmi_lib_t *dcgmi_lib_data, int set_count)
{
	debug("dcgmi_lib_data_alloc");
	if (dcgmi_lib_data)
	{
		// Set count
		dcgmi_lib_data->dcgmi_set_count = set_count;

		// clean first all pointers to then safetely call free func. in case of error.
		dcgmi_lib_data->dcgmi_set_sizes			= NULL;
		dcgmi_lib_data->dcgmi_event_ids			= NULL;
		dcgmi_lib_data->dcgmi_event_ids_csv	= NULL;
		dcgmi_lib_data->gpuprof_read1				= NULL;
		dcgmi_lib_data->gpuprof_read2 			= NULL;
		dcgmi_lib_data->gpuprof_diff				= NULL;
		dcgmi_lib_data->gpuprof_agg					= NULL;

		// Set sizes array
		dcgmi_lib_data->dcgmi_set_sizes = (int *) malloc(set_count * sizeof(int));

		// Event sets array
		dcgmi_lib_data->dcgmi_event_ids = (uint **) calloc(set_count, sizeof(uint *));

		// Event sets string array
		dcgmi_lib_data->dcgmi_event_ids_csv = (char **) calloc(set_count, sizeof(char *));

		// Event sets metrics array
		dcgmi_lib_data->gpuprof_read1 = (gpuprof_t **) malloc(set_count * sizeof(gpuprof_t *));
		dcgmi_lib_data->gpuprof_read2 = (gpuprof_t **) malloc(set_count * sizeof(gpuprof_t *));
		dcgmi_lib_data->gpuprof_diff	= (gpuprof_t **) malloc(set_count * sizeof(gpuprof_t *));
		dcgmi_lib_data->gpuprof_agg		= (gpuprof_t **) malloc(set_count * sizeof(gpuprof_t *));

		if (dcgmi_lib_data->dcgmi_set_sizes &&
				dcgmi_lib_data->dcgmi_event_ids &&
				dcgmi_lib_data->dcgmi_event_ids_csv &&
				dcgmi_lib_data->gpuprof_read1 &&
				dcgmi_lib_data->gpuprof_read2 &&
				dcgmi_lib_data->gpuprof_diff &&
				dcgmi_lib_data->gpuprof_agg
				)
		{
			return EAR_SUCCESS;
		} else
		{
			free(dcgmi_lib_data->dcgmi_set_sizes);

			free(dcgmi_lib_data->dcgmi_event_ids);
			free(dcgmi_lib_data->dcgmi_event_ids_csv);

			free(dcgmi_lib_data->gpuprof_read1);
			free(dcgmi_lib_data->gpuprof_read2);
			free(dcgmi_lib_data->gpuprof_diff);
			free(dcgmi_lib_data->gpuprof_agg);

			dcgmi_lib_data->dcgmi_set_sizes			= NULL;
			dcgmi_lib_data->dcgmi_event_ids			= NULL;
			dcgmi_lib_data->dcgmi_event_ids_csv	= NULL;
			dcgmi_lib_data->gpuprof_read1				= NULL;
			dcgmi_lib_data->gpuprof_read2 			= NULL;
			dcgmi_lib_data->gpuprof_diff				= NULL;
			dcgmi_lib_data->gpuprof_agg					= NULL;

			return_msg(EAR_ERROR, Generr.alloc_error);
		}
	} else
	{
		return_msg(EAR_ERROR, Generr.input_null);
	}
}
