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

#ifndef MANAGEMENT_CPUFREQ_BASE_H
#define MANAGEMENT_CPUFREQ_BASE_H

#include <common/states.h>
#include <common/hardware/topology.h>

typedef struct cpufreq_base_s {
    ullong frequency; // In KHZ
    uint   boost_enabled;
    uint   not_reliable;
} cpufreq_base_t;
// Alias
typedef cpufreq_base_t cfb_t;

void cpufreq_base_init(topology_t *tp, cpufreq_base_t *base);

#endif //MANAGEMENT_CPUFREQ_BASE_H