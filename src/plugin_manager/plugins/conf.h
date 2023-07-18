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

#ifndef UP_CONFIG_H
#define UP_CONFIG_H

#include <metrics/metrics.h>
#include <plugin_manager/plugin_manager.h>
#include <common/types/configuration/cluster_conf.h>

typedef struct conf_s {
    char            hostname[256]; //Alias
    char            hostname_full[256];
    cluster_conf_t  cluster;
    my_node_conf_t *node;
} conf_t;

#endif //UP_CONFIG_H