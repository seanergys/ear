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

//#define SHOW_DEBUGS 1

#include <pthread.h>
#include <management/cpupow/cpupow.h>
#include <management/cpupow/archs/intel63.h>

static pthread_mutex_t  lock = PTHREAD_MUTEX_INITIALIZER;
static uint             loaded;
static mgt_cpupow_ops_t ops;

void mgt_cpupow_load(topology_t *tp, int eard)
{
	while (pthread_mutex_trylock(&lock));
	if (loaded) {
		goto ret;
	}
    loaded = 1;
ret:
    mgt_cpupow_intel63_load(tp, &ops);
	pthread_mutex_unlock(&lock);
}

void mgt_cpupow_get_api(uint *api);
{
    return ops.get_api(api);
}

state_t mgt_cpupow_count_devices(int type, uint *devs_count)
{
    return ops.count_devices(type, devs_count);
}

state_t mgt_cpupow_get_powercap(int type, uint *watts)
{
    return ops.get_powercap(type, watts);
}

state_t mgt_cpupow_set_powercap(int type, uint *watts)
{
    return ops.set_powercap(type, watts);
}

state_t mgt_cpupow_get_tdp(uint *watts)
{
    return ops.get_tdp(watts);
}