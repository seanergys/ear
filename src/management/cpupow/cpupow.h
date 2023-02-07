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
#include <common/hardware/topology.h>

// Domain/device types
#define CPUPOW_DOMAINS  3
#define CPUPOW_SOCKET   0 //PKG (#devs = #sockets)
#define CPUPOW_DRAM     1 //    (#devs = #sockets)
#define CPUPOW_CORE     2 //PP0 (#devs = #cores), remember: cores != threads
#define CPUPOW_UNCORE   3 //PP1 (#devs = ¿#sockets?)
// Flag to use in powercap_set
#define POWERCAP_DISABLE UINT_MAX

typedef struct mgt_cpupow_ops_s
{
    void    (*get_api)             (uint *api);
    int     (*count_devices)       (int domain);
    int     (*is_powercap_capable) (int domain);
    state_t (*powercap_is_enabled) (int domain, int *enabled);
    state_t (*powercap_get)        (int domain, uint *watts);
    state_t (*powercap_set)        (int domain, uint *watts);
    state_t (*powercap_reset)      (int domain);
    state_t (*tdp_get)             (int domain, uint *watts);
} mgt_cpupow_ops_t;

void mgt_cpupow_load(topology_t *tp, int eard);

void mgt_cpupow_get_api(uint *api);

int mgt_cpupow_count_devices(int domain);

int mgt_cpupow_is_powercap_capable(int domain);

state_t mgt_cpupow_powercap_is_enabled(int domain, int *enabled);

state_t mgt_cpupow_powercap_get(int domain, uint *watts);

state_t mgt_cpupow_powercap_set(int domain, uint *watts);

state_t mgt_cpupow_powercap_reset(int domain);

state_t mgt_cpupow_tdp_get(int domain, uint *watts);

#endif
