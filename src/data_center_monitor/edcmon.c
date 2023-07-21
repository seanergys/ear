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
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <report/report.h>
#include <common/states.h>
#include <common/output/debug.h>
#include <common/system/plugin_manager.h>
#include <daemon/local_api/eard_api.h>

#if 0
static char           command[128];
static char           option[128];
static ulong          ids[2];
static my_node_conf_t nc;
static report_id_t    rid;

static int is(char *buf, char *string)
{
    return strcmp(buf, string) == 0;
}
#endif

int main(int argc, char *argv[])
{
    #if 0
    if (argc > 1) {
        eards_connection();
    }
    #endif
    plugin_manager_init(argc, argv);
    plugin_manager_wait();
    #if 0
    while (1) {
        scanf("%s", command);
        if (is(command, "new_job")) {
            scanf("%s", option);
            ids[0] = (ulong) atoi(option);
            ids[1] = 0LU;
            plugin_mananger_post("new_job", (void *) ids);
        } else if (is(command, "end_job")) {
            plugin_mananger_post("end_job", NULL);
        }
    }
    #endif
    return 0;
}
