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


#ifndef _DCGMI_LIB_HOOPER_H_
#define _DCGMI_LIB_HOOPER_H_


#include <common/states.h>

#include <library/metrics/dcgmi_lib/common.h>


state_t dcgmi_lib_hopper_init(uint api, uint all_events, dcgmi_lib_t *dcgmi_data, int (*event_info)[DCGMI_LIB_SUPPORTED_EVENTS][2]);

state_t dcgmi_lib_hopper_fp_coeffs(int **coeffs, size_t coeffs_length);

state_t dcgmi_lib_hopper_fp_coeffs(int **coeffs, size_t coeffs_length);

#endif // _DCGMI_LIB_HOOPER_H_
