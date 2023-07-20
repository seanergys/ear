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

#include <string.h>
#include <report/report.h>
#include <common/output/debug.h>
#include <common/system/plugin_manager.h>

static ullong zero = 0LLU;
static char buffer[512];

declr_up_get_tag ()
{
    *tag = "dummy";
    *tags_deps = "conf";
}

declr_up_action_init(_dummy)
{
    *data_alloc = &zero;
    sprintf(buffer, "dummy action init(_dummy): received tag %s", tag);
    return buffer;
}

declr_up_action_init(_conf)
{
    sprintf(buffer, "dummy action init(_conf): received tag %s", tag);
    return buffer;
}

declr_up_action_periodic(_dummy)
{
    sprintf(buffer, "dummy action periodic(_dummy): received tag %s", tag);
    return buffer;
}

declr_up_post_data()
{
    sprintf(buffer, "dummy post data: received msg %s", msg);
    return buffer;
}