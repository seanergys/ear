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

#include <metrics/gpu/archs/gpuprof_dummy.h>

static gpuprof_evs_t mevs;

GPUPROF_F_LOAD(gpuprof_dummy_load)
{
    sprintf(mevs.name, "dummy");
    mevs.id = 1;

    apis_put(ops->get_info,         gpuprof_dummy_get_info);
    apis_put(ops->events_get,       gpuprof_dummy_events_get);
    apis_put(ops->events_set,       gpuprof_dummy_events_set);
    apis_put(ops->events_multiplex, gpuprof_dummy_events_multiplex);
    apis_put(ops->read,             gpuprof_dummy_read);
    apis_put(ops->read_raw,         gpuprof_dummy_read_raw);
}

GPUPROF_F_GET_INFO(gpuprof_dummy_get_info)
{
    info->api         = API_DUMMY;
    info->scope       = SCOPE_NODE;
    info->granularity = GRANULARITY_PERIPHERAL;
    info->devs_count  = 1;
}

GPUPROF_F_EVENTS_GET(gpuprof_dummy_events_get)
{
    *evs = &mevs;
    *evs_count = 1;
}

GPUPROF_F_EVENTS_SET(gpuprof_dummy_events_set)
{

}

GPUPROF_F_EVENTS_MULTIPLEX(gpuprof_dummy_events_multiplex)
{

}

GPUPROF_F_READ(gpuprof_dummy_read)
{
    return EAR_SUCCESS;
}

GPUPROF_F_READ_RAW(gpuprof_dummy_read_raw)
{
    return EAR_SUCCESS;
}
