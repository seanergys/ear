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

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <common/sizes.h>
#include <common/output/debug.h>
#include <management/cpuprio/cpuprio.h>

static pthread_mutex_t     lock = PTHREAD_MUTEX_INITIALIZER;
static uint                cpu_count;
static uint                loaded;

// #############################################################################
void mgt_cpuprio_load(topology_t *tp, int eard)
{
	state_t s;
	while (pthread_mutex_trylock(&lock));
	if (loaded) {
		goto ret;
	}
    loaded = 1;
ret:
    pthread_mutex_unlock(&lock);
    return;
}

void mgt_cpuprio_get_api(uint *api)
{

}

state_t mgt_cpuprio_init(ctx_t *c)
{
    return EAR_SUCCESS;
}

state_t mgt_cpuprio_dispose(ctx_t *c)
{
    return EAR_SUCCESS;
}

state_t mgt_cpuprio_count_devices(ctx_t *c, uint *dev_count)
{
    return EAR_SUCCESS;
}

int isst_main(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    return isst_main(argc, argv);
}