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
#include <common/system/plugin_manager.h>
#include <daemon/local_api/eard_api.h>

declr_up_get_tag()
{
    *tag = "eardcon";
    *tags_deps = NULL;
}

declr_up_action_init(_eardcon)
{
    eards_connection();
    return rsprintf("EARD connection state: %s", eards_connected() ? "CONNECTED": "NOT CONNECTED");
}
