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
#include <plugin_manager/plugin_manager.h>

static cchar *self_tag = "dummy";
static ullong content = 0LLU;

declr_up_get_tag ()
{
    *tag = self_tag;
}

declr_up_init()
{
    fprintf(stderr, "dummy init\n");
    return EAR_SUCCESS;
}

declr_up_callback()
{
    fprintf(stderr, "dummy callback: received tag %s\n", tag);
}

declr_up_post()
{
    fprintf(stderr, "dummy post\n");
}

declr_up_data_alloc()
{
    fprintf(stderr, "dummy data alloc\n");
    *data = &content;
}
