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


#ifndef _DCGMI_LIB_T4_H_
#define _DCGMI_LIB_T4_H_


#include <common/states.h>
#include <library/metrics/dcgmi_lib/common.h>


state_t dcgmi_lib_t4_load();


state_t dcgmi_lib_t4_get_current_metrics(dcgmi_sig_t *dcgmi_sig);


state_t dcgmi_lib_t4_get_global_metrics(dcgmi_sig_t *dcgmi_sig);


void dcgmi_lib_t4_report_total_metrics();


void dcgmi_lib_t4_dcgmi_sig_init(dcgmi_sig_t *dcgmi_sig);


void dcgmi_lib_t4_reset_instances(dcgmi_sig_t *dcgmi_sig);


#endif // _DCGMI_LIB_T4_H_
