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

#ifndef METRICS_COMMON_ISST_H
#define METRICS_COMMON_ISST_H

#include <common/types.h>
#include <common/states.h>
#include <common/hardware/topology.h>

typedef struct clos_s {
    ullong max_khz;
    ullong min_khz;
    uint high;
    uint low;
    uint idx;
} clos_t;

state_t sst_init(topology_t *tp);

state_t sst_dispose();

state_t sst_enable();

state_t sst_disable();

state_t sst_clos_count();

state_t sst_clos_get_list();

state_t sst_clos_set_list();

state_t sst_assoc_get_list();

state_t sst_assoc_set_list();

state_t sst_assoc_set();

#endif
