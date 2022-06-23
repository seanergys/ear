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

#ifndef MANAGEMENT_CPUPRIO_H
#define MANAGEMENT_CPUPRIO_H

#include <common/states.h>
#include <common/plugins.h>
#include <common/hardware/topology.h>

void mgt_cpuprio_load(topology_t *tp, int eard);

void mgt_cpuprio_get_api(uint *api);

state_t mgt_cpuprio_init(ctx_t *c);

state_t mgt_cpuprio_dispose(ctx_t *c);

state_t mgt_cpuprio_count_devices(ctx_t *c, uint *dev_count);

#endif //MANAGEMENT_CPUPRIO_H
