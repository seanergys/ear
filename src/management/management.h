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
* EAR is an open source software, and it is licensed under both the BSD-3 license
* and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
* and COPYING.EPL files.
*/

#ifndef MANAGEMENT_H
#define MANAGEMENT_H

#include <management/gpu/gpu.h>
#include <management/cpufreq/cpufreq.h>
#include <management/imcfreq/imcfreq.h>

typedef struct _manages_s {
    ctx_t c;
    uint  api;
    uint  ok;
    uint  granularity;
    uint  devs_count;
    void *list1;
    void *list2;
    void *list3;
    uint  list1_count;
    uint  list2_count;
    uint  list3_count;
} _manages_t;

typedef struct manages_s {
	_manages_t cpufreq;
	_manages_t cpuprio;
	_manages_t imcfreq;
	_manages_t gpu;
} manages_t;

void management_init(topology_t *tp, int eard);

void management_dispose();

void management_print_apis();
// RPC management (remember to add this part wen new EARD API is introduced)
int management_rpcs(uint rpc_id, size_t rpc_size, int fd_req, int fd_ack);

#endif //MANAGEMENT_H
