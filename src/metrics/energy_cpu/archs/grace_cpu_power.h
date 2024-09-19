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
* This file is licensed under both the BSD-3 license for individual/non-commercial
* use and EPL-1.0 license for commercial use. Full text of both licenses can be
* found in COPYING.BSD and COPYING.EPL files.
*/


#ifndef METRICS_ENERGY_CPU_GRACE_CPU_H
#define METRICS_ENERGY_CPU_GRACE_CPU_H


#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <common/hardware/topology.h>

state_t grace_cpu_load(topology_t *tp_in);

state_t grace_cpu_init(ctx_t *c);

state_t grace_cpu_dispose(ctx_t *c);

state_t grace_cpu_count_devices(ctx_t *c, uint *devs_count_in);

state_t grace_cpu_read(ctx_t *c, ullong *values);


#endif
