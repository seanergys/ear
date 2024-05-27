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


#include <library/metrics/dcgmi_lib/ampere.h>

#include <stdlib.h>

#include <common/output/debug.h>


#define DCGMI_SET_COUNT 3
#define DCGMI_SET1_SIZE 10
#define DCGMI_SET2_SIZE 1
#define DCGMI_SET3_SIZE 1


state_t dcgmi_lib_ampere_init(uint api, dcgmi_lib_t *dcgmi_data, int **event_info, int event_info_rows, int event_info_cols)
{
	if (dcgmi_data && event_info)
	{
		debug("dcgmi_lib_ampere_init");
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
				dcgmi_data->dcgmi_event_ids[i] = (uint *) malloc(dcgmi_data->dcgmi_set_sizes[i] * sizeof(uint));

				if (!dcgmi_data->dcgmi_event_ids[i])
				{
					debug("Error allocating DCGMI event set %d", i);
					break;
				}

				// Event set string
				dcgmi_data->dcgmi_event_ids_csv[i] = (char *) calloc(dcgmi_data->dcgmi_set_sizes[i], ID_SIZE);

				if (!dcgmi_data->dcgmi_event_ids_csv[i])
				{
					debug("Error allocating DCGMI event set string %d", i);
					free(dcgmi_data->dcgmi_event_ids[i]);
					dcgmi_data->dcgmi_event_ids[i] = NULL;
					break;
				}

				i++;
			}

			// Check whether there was an error during allocation
			if (i < dcgmi_data->dcgmi_set_count)
			{
				for (int j = 0; j < i; j++)
				{
					free(dcgmi_data->dcgmi_event_ids[j]);
					free(dcgmi_data->dcgmi_event_ids_csv[j]);
				}

				return_msg(EAR_ERROR, Generr.alloc_error);
			}

			// Set 1
			dcgmi_data->dcgmi_event_ids[0][0] = dcgm_sm_active;
			dcgmi_data->dcgmi_event_ids[0][1] = dcgm_sm_occupancy;
			dcgmi_data->dcgmi_event_ids[0][2] = dcgm_tensor_active;
			dcgmi_data->dcgmi_event_ids[0][3] = dcgm_fp64_active;
			dcgmi_data->dcgmi_event_ids[0][4] = dcgm_dram_active;
			dcgmi_data->dcgmi_event_ids[0][5] = dcgm_pcie_tx_bytes;
			dcgmi_data->dcgmi_event_ids[0][6] = dcgm_pcie_rx_bytes;
			dcgmi_data->dcgmi_event_ids[0][7] = dcgm_gr_engine_active;
			dcgmi_data->dcgmi_event_ids[0][8] = dcgm_nvlink_tx_bytes;
			dcgmi_data->dcgmi_event_ids[0][9] = dcgm_nvlink_rx_bytes;

			// Set 2
			dcgmi_data->dcgmi_event_ids[1][0] = dcgm_fp16_active;

			// Set 3
			dcgmi_data->dcgmi_event_ids[2][0] = dcgm_fp32_active;

			int ampere_event_info[12][2] =
			{
				{0, 7}, // dcgm_gr_engine_active
				{0, 0}, // dcgm_sm_active
				{0, 1}, // dcgm_sm_occupancy
				{0, 2}, // dcgm_tensor_active
				{0, 4}, // dcgm_dram_active
				{0, 3}, // dcgm_fp64_active
				{2, 0}, // dcgm_fp32_active
				{1, 0}, // dcgm_fp16_active
				{0, 5}, // dcgm_pcie_tx_bytes
				{0, 6}, // dcgm_pcie_rx_bytes
				{0, 8}, // dcgm_nvlink_tx_bytes
				{0, 9}  // dcgm_nvlink_rx_bytes
			};

			for (i = 0; i < ear_min(event_info_rows, 12); i++)
			{
				for (int j = 0; j < ear_min(event_info_cols, 2); j++)
				{
					event_info[i][j] = ampere_event_info[i][j];
				}
			}

			return EAR_SUCCESS;
		} else
		{
			debug("Error allocating dcgmi lib data");

			return_msg(EAR_ERROR, Generr.alloc_error);
		}
	} else
	{
		return_msg(EAR_ERROR, Generr.input_null);
	}
}
