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

#define SHOW_DEBUGS 1

#include <stdio.h>
#include <common/output/debug.h>
#include <plugin_manager/plugins/conf.h>
#include <plugin_manager/plugins/metrics.h>

static topology_t tp;
static mets_t     m;
static char      *np_path; //Node power
static char       buffer[4096];

declr_up_get_tag()
{
    *tag = "metrics";
    *tags_deps = "conf";
}

declr_up_action_init(_conf)
{
    cluster_conf_t *conf = (cluster_conf_t *) data;
    sprintf(buffer, "%s/energy/%s", conf->install.dir_plug, conf->install.obj_ener);
    np_path = buffer;
    return NULL;
}

declr_up_action_init(_metrics)
{
    *data_alloc = &m;
    topology_init(&tp);
    metrics_init(&m.mi, &tp, np_path, NO_EARD);
    metrics_data_alloc(&m.mr, NULL, NULL);
    return rsprintf("Metrics plugin initialized correctly %s", (!np_path) ? "(missing np library)": "");
}

declr_up_action_periodic(_metrics)
{
    metrics_read(&m.mr);
    return NULL;
}