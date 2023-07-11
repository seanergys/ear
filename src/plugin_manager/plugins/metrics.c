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

#include <stdio.h>
#include <common/output/debug.h>
#include <plugin_manager/plugins/metrics.h>

static topology_t tp;
static mets_t     m;
static cchar     *self_tag = "metrics";

declr_up_get_tag()
{
    *tag = self_tag;
}

declr_up_init()
{
    topology_init(&tp);
    metrics_init(&m.mi, &tp, NO_EARD);
    metrics_data_alloc(&m.mr, NULL, NULL);
    metrics_read(&m.mr);
    return EAR_SUCCESS;
}

declr_up_callback()
{
    metrics_read(&m.mr);
}

declr_up_data_alloc()
{
    *data = &m;
}
