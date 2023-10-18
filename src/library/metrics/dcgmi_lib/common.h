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
#include <metrics/gpu/archs/dcgmi.h>


#define DCGMI_INIT_LVL	1
#define DCGMI_MET_LVL		3

#define DCGMI_LIB_SUPPORTED_EVENTS 12

#define DCGMI_LIB_SAMPLING_PERIOD 2


typedef struct dcgmi_sig
{
	uint    dcgmi_sets;
	uint    dcgmi_gpus;
	uint    dcgmi_num_ev;
	uint    **dcgmi_events;
	uint    *dcgmi_instances;
	dcgmi_t **dcgmi_metrics;
} dcgmi_sig_t;


#endif // _DCGMI_LIB_COMMON_H_
