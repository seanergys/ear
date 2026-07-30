/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#ifndef PLUGIN_MANAGER_INTERNAL_H
#define PLUGIN_MANAGER_INTERNAL_H
/* clang-format off */

#include <common/types.h>

// Internal contract between plugin_manager.c and plugin_manager_internal.c.
// This exposes implementation state, so keep it out of plugin_manager.h.

#define MAX_PLUGINS      128
#define PM_DEFAULT_VERB  2
#ifndef SEPARATOR
#define SEPARATOR '+'
#endif

typedef void  (get_tag_f)         (cchar **tag, cchar **tags_deps);
typedef char *(action_init_f)     (cchar *tag, void **data_alloc, void *data);
typedef char *(action_periodic_f) (cchar *tag, void *data);
typedef char *(action_close_f)    (void);
typedef char *(message_receive_f) (cchar *tag_msg, void *data);
typedef char *(poll_attend_f)     (cchar *tag_fd, int fd);

// Plugin format:
//   file.so:time:conf1:conf2...
// Example of dependency table:
//   plugin0 [0] [1] [0]: It means that for plugin0, the plugin1 is mandatory
//   plugin1 [0] [0] [1]: It means that for plugin1, the plugin2 is mandatory
//   plugin2 [0] [0] [0]
typedef struct plugin_s {
    char               path[4096];
    char              *file_name;
    void              *handler;
    cchar             *tag;
    cchar             *tags_deps;
    char             **config_tokens; // Owns the allocation backing data_conf
    uint               deps_table[MAX_PLUGINS]; // Not sorted, table of dependecy modes
    action_init_f     *action_init[MAX_PLUGINS];
    uint               action_init_resolved[MAX_PLUGINS];
    action_periodic_f *action_periodic[MAX_PLUGINS];
    uint               action_periodic_resolved[MAX_PLUGINS];
    void              *data; // Allocated space from init function
    void              *data_conf; // From plugin format: conf1, conf2, etc.
    ullong             time_accum;
    ullong             time_lapse; // In ms
    uint               time_lapse_is_set;
    uint               priority;
    uint               is_opened;
    uint               is_enabled; // If disabled does not have any kind of interaction
    uint               is_initialized;
    uint               is_paused; // If paused is not called periodically
    uint               is_silenced;
    uint               id_overhead_periodic;
} plugin_t;

typedef struct plugins_s {
    plugin_t         **plugins_sorted; // Plugins sorted by priority
    plugin_t          *plugins;
    uint               count;
} plugins_t;

// Internal poll system
typedef struct pmfd_set_s {
    plugin_t          *plugin;
    poll_attend_f     *poll_attend;
    cchar             *tag;
} pmfd_set_t;

int  plugin_manager_internal_read(plugins_t *ctx, cchar *string, ullong dependent_time_lapse, uint priority, uint *dependent_deps_table);

int  plugin_manager_internal_open(plugins_t *ctx, char **priority_paths);

void plugin_manager_internal_sort(plugins_t *ctx);

void plugin_manager_internal_dispose(plugins_t *ctx);

/* clang-format on */
#endif // PLUGIN_MANAGER_INTERNAL_H
