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
#include <stdlib.h>
#include <common/output/debug.h>
#include <data_center_monitor/plugins/conf.h>
#include <data_center_monitor/plugins/management.h>

static mans_t     m;
static conf_t    *conf;
static char       buffer[4096];

declr_up_get_tag()
{
    *tag = "management";
    *tags_deps = "!conf,!metrics";
}

declr_up_action_init(_conf)
{
    conf = (conf_t *) data;
    return NULL;
}

declr_up_action_init(_management)
{
    management_init(&m.mi, &conf->tp, API_EARD);
    return rsprintf("Management plugin initialized correctly");
}