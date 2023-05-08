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

#include <stdlib.h>
#include <common/hardware/topology.h>
#include <plugin_manager/plugin_manager.h>

static topology_t topo;

declr_up_init()
{
    topology_init(&topo);
    return EAR_SUCCESS;
}

declr_up_get_tag ()
{
    *tag = "example";
}

// cchar *tag, (void *) data
declr_up_execute()
{
    uint *cpu_count = (uint *) data;
    *cpu_count = topo.cpu_count;
    return EAR_SUCCESS;
}

// (void *) data
declr_up_data_alloc()
{
    *data = calloc(1, sizeof(uint));
}