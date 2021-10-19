/*
*
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
* This file is licensed under both the BSD-3 license for individual/non-commercial
* use and EPL-1.0 license for commercial use. Full text of both licenses can be
* found in COPYING.BSD and COPYING.EPL files.
*/

#ifndef MANAGEMENT_H
#define MANAGEMENT_H

#include <management/gpu/gpu.h>
#include <management/cpufreq/cpufreq.h>
#include <management/imcfreq/imcfreq.h>

typedef struct apis_info_s {
    ctx_t c;
    uint  api;
    uint  working;
    uint  granularity;
    uint  devs_count;
    uint  avail_count;
    void *current_list;
    void *avail_list;
} apis_info_t;

typedef struct manages_apis_s {
	apis_info_t cpu; // cpufreq
	apis_info_t imc; // imcfreq
	apis_info_t gpu; // gpu
} manages_apis_t;

state_t manages_apis_init(topology_t *tp, uint eard, manages_apis_t **m_in);

state_t manages_apis_dispose();

state_t manages_apis_print();

state_t manages_apis_print_available();

#endif //MANAGEMENT_H
