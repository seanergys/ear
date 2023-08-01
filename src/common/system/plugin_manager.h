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

#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include <common/types.h>
#include <common/hardware/topology.h>

// You can load a set of plugins by passing arguments in this format:
//  --plugins=plugin1.so:time1,plugin2.so:time2...
// Time is in milliseconds. The sort order also defines the order of execution.
//
// From the plugins part, the definition of the tag identifies the data to work
// with. In example, if your plugin1 tag is "metrics" and you defined the
// 'data_alloc' function, you will allocate space for your data tagged as
// "metrics". If your plugin1 defines the function 'execute', the data tagged as
// "metrics" will be passed to 'execute'. Also, if your plugin2 defined its tag
// as "metrics-viewer", the "metrics" string will be contained in plugin2 tag.
// So the "metrics" data will be passed to plugin2 'execute' function after the
// plugin1. Moreover, if your plugin3 defined its tag as "metrics-reporter", and
// the plugin2 allocated some kind of data, the plugin3 won't receive an
// 'execute' call with the "metrics-viewer" allocated data because the tag
// "metrics-viewer" doesn't match with any "metrics-reporter" substring.
//
// Take into the account that the 'execute' function have to have some 'strcmp'
// calls to identify the tags of the data passed if the data treatment is
// different for each data tag.
//
// In case you are programming a plugin, you can use the declr_up_* defines
// instead naming explictly the functions. This is useful because if we want to
// add new parameters or change function names we can do it just editing this
// header.

// Universal Plugin function declarations
#define declr_up_get_tag()               void   up_get_tag                 (cchar **tag, cchar **tags_deps)
#define declr_up_action_init(suffix)     char * up_action_init##suffix     (cchar *tag, void **data_alloc, void *data)
#define declr_up_action_periodic(suffix) char * up_action_periodic##suffix (cchar *tag, void *data)
#define declr_up_post_data()             char * up_post_data               (cchar *msg, void *data)

#define is_tag(t) (strcmp(t, tag) == 0)
#define is_msg(t) (strcmp(t, msg) == 0)

int plugin_manager_init(int argc, char *argv[]);

void plugin_mananger_post(cchar *msg, void *data);

void plugin_manager_wait();

#endif //PLUGIN_MANAGER_H
