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
#include <plugin_manager/plugins/config.h>

static topology_t tp;
static cchar     *self_tag = "configuration";
static conf_t   conf;

declr_up_get_tag()
{
    *tag = self_tag;
}

declr_up_init()
{
    char buffer[4096];
    if (state_fail(get_ear_conf_path(buffer))) {
        return_msg(EAR_ERROR, "Configuration path not found");
    }
    if (gethostname(conf.hostname_full, sizeof(conf.hostname_full)) < 0) {
        return_print(EAR_ERROR, "Error getting node name (%s)", strerror(errno));
    }
    strcpy(conf.hostname, conf.hostname_full);
    strtok(conf.hostname, ".");
    debug("Reading ear.conf in '%s' in node '%s'", buffer, conf.hostname);
    if (state_fail(read_cluster_conf(buffer, &conf.cluster))) {
        return_msg(EAR_ERROR, "Error while reading ear.conf file");
    }
    if ((conf.node = get_my_node_conf(&conf.cluster, conf.hostname)) == NULL) {
        if ((conf.node = get_my_node_conf(&conf.cluster, conf.hostname_full)) == NULL) {
            return_msg(EAR_ERROR, "Node not found in ear.conf");
        }
    }
    debug("database: %s", conf.cluster.database.database);
    return EAR_SUCCESS;
}

declr_up_data_alloc()
{
    *data = &conf;
}
