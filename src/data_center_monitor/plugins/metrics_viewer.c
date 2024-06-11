/*
*
* This program is part of the EAR software.
*
* EAR provides a dynamic, transparent and ligth-weigth solution for
* Energy management. It has been developed in the context of the
* Barcelona Supercomputing Center (BSC)&Lenovo Collaboration project.
*
* 
* BSC Contact   mailto:ear-support@bsc.es
* 
*
* EAR is an open source software, and it is licensed under both the BSD-3 license
* and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
* and COPYING.EPL files.
*/

//#define SHOW_DEBUGS 1

#include <string.h>
#include <report/report.h>
#include <common/output/debug.h>
#include <common/system/plugin_manager.h>
#include <data_center_monitor/plugins/metrics.h>

static metrics_read_t     mr1;
static metrics_read_t     mr2;
static metrics_diff_t     mrD;

declr_up_get_tag()
{
    *tag = "metrics_viewer";
    *tags_deps = "!metrics";
}

static void metrics_read_static(metrics_read_t *mr)
{
    metrics_data_copy(mr, &((mets_t *) plugin_manager_action("metrics"))->mr);
}

declr_up_action_init(_metrics_viewer)
{
    metrics_data_alloc(&mr1, &mr2, &mrD);
    metrics_read_static(&mr1);
    return NULL;
}

declr_up_action_periodic(_metrics_viewer)
{
    metrics_read_static(&mr2);
    metrics_data_diff(&mr2, &mr1, &mrD);
    metrics_data_copy(&mr1, &mr2);
    printf("%s",  metrics_data_tostr(&mrD));
    return NULL;
}
