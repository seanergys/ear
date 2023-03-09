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
#include <string.h>
#include <common/output/debug.h>
#include <metrics/cache/archs/dummy.h>

void cache_dummy_load(topology_t *tp, cache_ops_t *ops)
{
    apis_put(ops->get_api,         cache_dummy_get_api);
    apis_put(ops->get_granularity, cache_dummy_get_granularity);
    apis_put(ops->init,            cache_dummy_init);
    apis_put(ops->dispose,         cache_dummy_dispose);
    apis_put(ops->read,            cache_dummy_read);
    apis_put(ops->data_diff,       cache_dummy_data_diff);
    apis_put(ops->info_tostr,      cache_dummy_info_tostr);
}

void cache_dummy_get_api(uint *api)
{
    *api = API_DUMMY;
}

void cache_dummy_get_granularity(uint *granularity)
{
    *granularity = GRANULARITY_DUMMY;
}

state_t cache_dummy_init(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t cache_dummy_dispose(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t cache_dummy_read(ctx_t *c, cache_t *ca)
{
	memset(ca, 0, sizeof(cache_t));
	return EAR_SUCCESS;
}

void cache_dummy_data_diff(cache_t *ca2, cache_t *ca1, cache_t *caD, double *gbs)
{
    memset(caD, 0, sizeof(cache_t));
    if (gbs != NULL) {
        *gbs = 0.0;
    }
}

void cache_dummy_info_tostr(char *buffer, int length)
{
    sprintf(buffer, "L1 DUMMY: loaded\n");
}