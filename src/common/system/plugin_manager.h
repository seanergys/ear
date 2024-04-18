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
#include <common/system/poll.h>

// Universal Plugin function declarations
#define declr_up_get_tag()               void   up_get_tag                 (cchar **tag, cchar **tags_deps)
#define declr_up_action_init(suffix)     char * up_action_init##suffix     (cchar *tag, void **data_alloc, void *data)
#define declr_up_action_periodic(suffix) char * up_action_periodic##suffix (cchar *tag, void *data)
#define declr_up_post_data()             char * up_post_data               (cchar *msg, void *data)
#define declr_up_fds_register()          char * up_fds_register            (afd_set_t *set)
#define declr_up_fds_attend()            char * up_fds_attend              (afd_set_t *set)
#define declr_up_close()                 char * up_close                   ()

#define is_tag(t) (strcmp(t, tag) == 0)
#define is_msg(t) (strcmp(t, msg) == 0)

#define ARG0(var) (var != NULL)
#define ARG1(var) ARG0(var) && (var [1] != NULL)
#define ARG2(var) ARG1(var) && (var [2] != NULL)
#define ARGE(...) __VA_ARGS__
#define ARG(v, n) ARGE(ARG ## n)(v)

// Init as main binary function
int plugin_manager_main(int argc, char *argv[]);
// Init as a component of a binary
int plugin_manager_init(char *files, char *paths);
// Closes Plugin Manager main thread.
void plugin_manager_close();
// Wait until Plugin Manager exits.
void plugin_manager_wait();
// Asking for an action. Intended to be called from plugins.
void *plugin_manager_action(cchar *tag);
// Passing data to plugins. Intended to be called outside PM.
void plugin_mananger_post(cchar *msg, void *data);

#endif //PLUGIN_MANAGER_H
