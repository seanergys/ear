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

#include <metrics/gpu/archs/dcgmi.h>


#define DCGMI_LIB_SUPPORTED_EVENTS 12

#define dcgmi_lib_ret_alloc_err() \
	return_msg(EAR_ERROR, Generr.alloc_error);


typedef struct dcgmi_lib_s
{
	int *dcgmi_set_sizes;
	int	dcgmi_set_count;
															
	uint **dcgmi_events;
	char **dcgmi_events_str;
															
	dcgmi_t **dcgmi_metrics;
	dcgmi_t **dcgmi_metrics_agg;
} dcgmi_lib_t;


state_t dcgmi_lib_data_alloc(dcgmi_lib_t *dcgmi_data, int set_count);


#endif // _DCGMI_LIB_COMMON_H_
