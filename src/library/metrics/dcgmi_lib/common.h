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


#ifndef _DCGMI_LIB_COMMON_H_
#define _DCGMI_LIB_COMMON_H_


#include <common/types.h>
#include <common/states.h>

// #include <metrics/gpu/archs/dcgmi.h> // TODO: To be removed
#include <metrics/gpu/gpuprof.h>


#define DCGMI_LIB_SUPPORTED_EVENTS 12

#define sm_active             1002
#define sm_occupancy          1003
#define tensor_active         1004
#define fp64_active           1006
#define fp16_active           1008
#define tensor_imma_active    1013
#define tensor_hmma_active    1014
#define fp32_active           1007
#define dram_active           1005
#define pcie_tx_bytes         1009
#define pcie_rx_bytes         1010
#define gr_engine_active      1001
#define nvlink_tx_bytes       1011    
#define nvlink_rx_bytes       1012


typedef struct dcgmi_lib_s
{
	int *dcgmi_set_sizes;				/**< Event set size for each set. */
	int	dcgmi_set_count;				/**< Event set count. */

	uint **dcgmi_event_ids;			/**< Event IDs for each set. */
	char **dcgmi_event_ids_csv;	/**< CSV event strings for each set. */

	gpuprof_t **gpuprof_read1;	/**< Event metrics for each set and device. */
	gpuprof_t **gpuprof_read2;	/**< Event metrics for each set and device. */
	gpuprof_t **gpuprof_diff;		/**< Event metrics for each set and device. */
	gpuprof_t **gpuprof_agg;		/**< Aggregated metrics for each set and device. */
} dcgmi_lib_t;


/** Allocates \ref dcgmi_data attributes based on \ref set_count.
 * You can use \ref state_msg to read the error message. */
state_t dcgmi_lib_data_alloc(dcgmi_lib_t *dcgmi_data, int set_count);


/** TODO: Implement this function. */
state_t dcgmi_lib_data_free(dcgmi_lib_t *dcgmi_data);


#endif // _DCGMI_LIB_COMMON_H_
