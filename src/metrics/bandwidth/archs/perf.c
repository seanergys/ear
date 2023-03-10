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

#include <common/output/debug.h>
#include <metrics/bandwidth/archs/perf.h>

state_t bwidth_perf_load(topology_t *tp, bwidth_ops_t *ops)
{
	apis_put(ops->init,            bwidth_perf_init);
	apis_put(ops->dispose,         bwidth_perf_dispose);
	apis_put(ops->count_devices,   bwidth_perf_count_devices);
	apis_put(ops->get_granularity, bwidth_perf_get_granularity);
	apis_put(ops->read,            bwidth_perf_read);
	return EAR_SUCCESS;
}

state_t bwidth_perf_init(ctx_t *c)
{
    return EAR_SUCCESS;
}

state_t bwidth_perf_dispose(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t bwidth_perf_count_devices(ctx_t *c, uint *devs_count_in)
{
	return EAR_SUCCESS;
}

state_t bwidth_perf_get_granularity(ctx_t *c, uint *granularity)
{
	return EAR_SUCCESS;
}

state_t bwidth_perf_read(ctx_t *c, bwidth_t *bw)
{
	return EAR_SUCCESS;
}