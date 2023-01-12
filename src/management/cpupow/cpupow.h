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

#ifndef MANAGEMENT_CPUPOW_H
#define MANAGEMENT_CPUPOW_H

#include <common/types.h>
#include <common/states.h>

// Types
#define CPUPOW_SOCKET 0
#define CPUPOW_CORE   1 //PP0
#define CPUPOW_DRAM   2

typedef struct mgt_cpupow_ops_s
{
    void    (*get_api)       (uint *api);
    state_t (*count_devices) (int type, uint *devs_count);
    state_t (*get_powercap)  (int type, uint *watts);
    state_t (*set_powercap)  (int type, uint *watts);
    state_t (*get_tdp)       (int type, uint *watts);
} mgt_cpupow_ops_t;

void mgt_cpupow_load(topology_t *tp, int eard);

void mgt_cpupow_get_api(uint *api);

state_t mgt_cpupow_count_devices(int type, uint *devs_count);

state_t mgt_cpupow_get_powercap(int type, uint *watts);

state_t mgt_cpupow_set_powercap(int type, uint *watts);

/* Socket TDP. */
state_t mgt_cpupow_get_tdp(uint *watts);

#endif