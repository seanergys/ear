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
#include <metrics/flops/archs/dummy.h>

void flops_dummy_load(topology_t *tp, flops_ops_t *ops)
{
    apis_put(ops->get_info,        flops_dummy_get_info);
    apis_put(ops->init,            flops_dummy_init);
    apis_put(ops->dispose,         flops_dummy_dispose);
    apis_put(ops->read,            flops_dummy_read);
    apis_put(ops->data_diff,       flops_dummy_data_diff);
}

void flops_dummy_get_info(apinfo_t *info)
{
    info->api         = API_DUMMY;
    info->scope       = SCOPE_PROCESS;
    info->granularity = GRANULARITY_PROCESS;
    info->devs_count  = 1;
    info->bits        = 0;
}

state_t flops_dummy_init()
{
	return EAR_SUCCESS;
}

state_t flops_dummy_dispose()
{
	return EAR_SUCCESS;
}

state_t flops_dummy_read(flops_t *fl)
{
	memset(fl, 0, sizeof(flops_t));
	return EAR_SUCCESS;
}

void flops_dummy_data_diff(flops_t *fl2, flops_t *fl1, flops_t *flD, double *gfs)
{
    // Cleaning
    if (gfs != NULL) { *gfs = 0.0; }
    if (flD != NULL) { memset(flD, 0, sizeof(flops_t)); }
}