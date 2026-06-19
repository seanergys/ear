/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

/* clang-format off */
#define _GNU_SOURCE
#include <stdlib.h>
#include <common/output/debug.h>
#include <common/system/symplug.h>
#include <common/utils/overhead.h>
#include <common/utils/strscreen.h>
#include <common/config/config_env.h>
#include <common/system/plugin_manager_internal.h>

int plugin_manager_internal_read(plugins_t *ctx, cchar *string, ullong dependent_time_lapse, uint priority, uint *dependent_deps_table)
{
    uint   list1_count;
    uint   list2_count;
    char **list1;
    char **list2;
    int    priority_plus;
    int    time_lapse_inherit;
    int    mandatory;
    int    l, j;
    char  *c;

    debug("plugin string: %s", string);
    // 'string' is splitted in a list of plugins
    if (strtoa(string, SEPARATOR, &list1, &list1_count) == NULL) {
        return 0;
    }
    debug("detected plugins: #%d", list1_count);
    // This for extends to the end of the function
    for (l = 0; l < list1_count; ++l) {
        debug("list1[%d]: %s", l, list1[l]);
        if (ctx->count == MAX_PLUGINS) {
            verbose(0, "[ERROR] Maximum number of plugins reached (%u).", MAX_PLUGINS);
            strtoa_free(list1);
            return 0;
        }
        // Cleaning
        memset(&ctx->plugins[ctx->count], 0, sizeof(plugin_t));
        // Configuration = file.so:time:conf1:conf2
        list2              = NULL; // Configuration splitted in items
        list2_count        = 0;
        priority_plus      = 0;
        time_lapse_inherit = 0;
        mandatory          = 0;
        if (strtoa(list1[l], ':', &list2, &list2_count) != NULL) {
            for (j = 1; list2[j] != NULL; ++j) {
                adebug("[Debug] %s: arg[%d] %s", list2[0], j, list2[j]);
            }
            // If time is valid
            if (list2_count > 1 && strlen(list2[1]) > 0 && strisnum(list2[1])) {
                ctx->plugins[ctx->count].time_lapse = (ullong) atoi(list2[1]);
                ctx->plugins[ctx->count].time_lapse_is_set = ctx->plugins[ctx->count].time_lapse > 0;
                // If configuration exists
                if (list2_count > 2) {
                    ctx->plugins[ctx->count].data_conf = (void *) &list2[2];
                }
            } else {
                // If time is not valid, this is configuration then
                if (list2_count > 1 && strlen(list2[1]) > 0) {
                    ctx->plugins[ctx->count].data_conf = (void *) &list2[1];
                } else {
                    // If list2[1] is empty, then the configuration is this
                    if (list2_count > 2) {
                        ctx->plugins[ctx->count].data_conf = (void *) &list2[2];
                    }
                }
            }
            c = list2[0];
        } else {
            c = list1[l];
        }
        // Parsing the file
        while (*c == '!' || *c == '<' || *c == '^' || *c == '-') {
            if (*c == '!') {
                mandatory = 1;
            }
            if (*c == '<') {
                time_lapse_inherit = 1;
            }
            if (*c == '^') {
                priority_plus = MAX_PLUGINS;
            }
            if (*c == '-') {
                ctx->plugins[ctx->count].is_silenced = 1;
            }
            ++c;
        }
        // Checking if the file have the .so extension
        if (strcmp(&c[strlen(c) - 3], ".so")) {
            sprintf(ctx->plugins[ctx->count].path, "%s.so", c);
        } else {
            sprintf(ctx->plugins[ctx->count].path, "%s", c);
        }
        // Getting the plugin file_name for printing and compare purposes
        for (j = strlen(ctx->plugins[ctx->count].path) - 2; j >= 0; --j) {
            ctx->plugins[ctx->count].file_name = &ctx->plugins[ctx->count].path[j];
            if (ctx->plugins[ctx->count].path[j] == '/') {
                ctx->plugins[ctx->count].file_name = &ctx->plugins[ctx->count].path[j + 1];
                break;
            }
        }
        // Priority system:
        //   - priority is 0 by default.
        //   - priority_plus increases the priority to the max by parameter.
        // The priority is increased based on the number of plugins in the list.
        // The first plugin receives the maximum priority, equal to the number
        // of plugins present in the list. When the plugin is opened and its
        // dependency list is read, the priority is processed again with these
        // new values. But if the plugin is already found in the context, its
        // priority and other data are just updated few lines ahead.
        ctx->plugins[ctx->count].priority = priority + (list1_count - l) + priority_plus;
        if (ctx->plugins[ctx->count].priority > ctx->max_priority) {
            ctx->max_priority = ctx->plugins[ctx->count].priority;
        }
        // Time-lapse. When requested, a dependency inherits the interval of
        // the plugin that depends on it so their periodic calls stay aligned.
        if (!ctx->plugins[ctx->count].time_lapse_is_set && time_lapse_inherit && dependent_time_lapse > 0) {
            ctx->plugins[ctx->count].time_lapse = dependent_time_lapse;
        }
        // Comparing this new added plugin in the already processed plugins.
        // If the plugin is already in the list, then the priority and other
        // data are just updated.
        for (j = 0; j < ctx->count; ++j) {
            plugin_t *p_current = &ctx->plugins[ctx->count];
            plugin_t *p_found   = &ctx->plugins[j];
            // If the found plugin is the same current
            if (strcmp(p_found->file_name, p_current->file_name) == 0) {
                // Replacing priority if has increased its value
                if (p_found->priority < p_current->priority) {
                    p_found->priority = p_current->priority;
                }
                // Replacing timelapse if:
                //   1) The time-lapse between actions is set to be inherited by the dependent plugin.
                //   2) The dependent plugin has its action-time-lapse set.
                //   3.1) If the time-lapse of the current plugin is 0.
                //   3.2) Or if the time of the dependent plugin is lesser than the current.
                if (time_lapse_inherit && (dependent_time_lapse > 0) ) {
                    if (!p_found->time_lapse_is_set || dependent_time_lapse < p_found->time_lapse) {
                        p_found->time_lapse = dependent_time_lapse;
                }}
                // Replacing if the plugin is silenced
                if (p_current->is_silenced) {
                    p_found->is_silenced = 1;
                }
                // Replacing dependency type
                if (dependent_deps_table != NULL) {
                    dependent_deps_table[j] = (uint) mandatory;
                }
                break; // Plugin already found, skipping the rest of the loop.
            }
        }
        // If found, passing to next dependency
        if (j < ctx->count) {
            continue;
        }
        // If not found, update the dependency table of the dependent plugin.
        if (dependent_deps_table != NULL) {
            dependent_deps_table[ctx->count] = mandatory;
        }
        // Not found, so this is a newly added plugin
        ctx->count += 1;
    }
    strtoa_free(list1);
    return 1;
}

static void plugins_open_sort(plugins_t *ctx, uint priority, uint index)
{
    int i;
    // Base case
    if (priority == 0) {
        return;
    }
    for (i = 0; i < ctx->count; ++i) {
        if (priority == ctx->plugins[i].priority) {
            adebug("[Debug] %s: time lapse '%4llu', time_lapse_is_set %u, priority '%u'", ctx->plugins[i].path,
                   ctx->plugins[i].time_lapse, ctx->plugins[i].time_lapse_is_set, ctx->plugins[i].priority);
            ctx->plugins_sorted[index++] = &ctx->plugins[i];
        }
    }
    plugins_open_sort(ctx, priority - 1, index);
}

void plugin_manager_internal_sort(plugins_t *ctx)
{
    // Rebuild the whole sorted view after new plugins or dependencies change priorities.
    memset(ctx->plugins_sorted, 0, MAX_PLUGINS * sizeof(plugin_t *));
    plugins_open_sort(ctx, ctx->max_priority, 0);
}

static state_t plugins_open_so(char *path, void **handler)
{
    if (access(path, X_OK) != 0) {
        return_msg(EAR_ERROR, strerror(errno));
    }
    // Lazy binding allows to load plugins thar require symbols of other plugins
    // that haven't been loaded yet.
    debug("Loding %s", path);
    if ((*handler = dlopen(path, RTLD_LAZY | RTLD_GLOBAL)) == NULL) {
        return_msg(EAR_ERROR, dlerror());
    }
    return EAR_SUCCESS;
}

static state_t plugins_open_file(plugins_t *ctx, char **priority_paths, int i)
{
    char new_path[8192];
    char *e = NULL;
    int j   = 0;

    // Priority paths
    while (priority_paths != NULL && priority_paths[j] != NULL) {
        sprintf(new_path, "%s/%s", priority_paths[j], ctx->plugins[i].path);
        if (state_ok(plugins_open_so(new_path, (void **) &ctx->plugins[i].handler))) {
            verbose(4, "[Configuration] Trying to open: %s... OK", new_path);
            return EAR_SUCCESS;
        }
        verbose(4, "[Configuration] Trying to open: %s... FAILED (%s)", new_path, state_msg);
        ++j;
    }
    // Written paths
    if (state_ok(plugins_open_so(ctx->plugins[i].path, (void **) &ctx->plugins[i].handler))) {
        verbose(4, "[Configuration] Trying to open: %s... OK", ctx->plugins[i].path);
        return EAR_SUCCESS;
    }
    verbose(4, "[Configuration] Trying to open: %s... FAILED", ctx->plugins[i].path);
    // Environment paths
    if ((e = ear_getenv(ENV_PATH_EAR)) != NULL) {
        sprintf(new_path, "%s/lib/plugins/monitoring/%s", e, ctx->plugins[i].path);
        if (state_ok(plugins_open_so(new_path, (void **) &ctx->plugins[i].handler))) {
            verbose(4, "[Configuration] Trying to open: %s... OK", new_path);
            return EAR_SUCCESS;
        }
        verbose(4, "[Configuration] Trying to open: %s... FAILED", new_path);
    }
    return EAR_ERROR;
}

int plugin_manager_internal_open(plugins_t *ctx, char **priority_paths)
{
    plugin_t *plug;
    get_tag_f *get_tag;
    int enabled_count = 0;
    int i, j;

    for (i = 0; i < ctx->count; ++i) {
        // Hot-loading reuses this path; already opened plugins must not be dlopen()'d again.
        if (ctx->plugins[i].is_opened) {
            continue;
        }
        if (state_fail(plugins_open_file(ctx, priority_paths, i))) {
            continue;
        }
        plug = &ctx->plugins[i];
        if ((get_tag = dlsym(plug->handler, "up_get_tag")) == NULL) {
            debug("[DEBUG] %s get_tag() not detected", plug->path);
            continue;
        }
        // Maybe we can explore in the future a multi tag system
        get_tag(&plug->tag, &plug->tags_deps);
        debug("[DEBUG] %s after call get_tag(): tag '%s', tags_deps '%s'",
              plug->path, (char *) plug->tag, (char *) plug->tags_deps);
        plug->is_opened = 1;
        // Loading dependencies
        if (plug->tags_deps != NULL) {
            // Reading (2) all the plugins specified in the dependency tags.
            plugin_manager_internal_read(ctx, plug->tags_deps, plug->time_lapse, plug->priority, plug->deps_table);
        }
        // Overhead measurment suscriptions
        overhead_subsprint(&plug->id_overhead_periodic, "periodic %s", plug->tag);
    }
    // Testing if dependencies are at least opened.
    for (i = 0; i < ctx->count; ++i) {
        plug = &ctx->plugins[i];
        for (j = 0; j < ctx->count; ++j) {
            // If mandatory but the dependency wasn't opened
            if (plug->deps_table[j] && !ctx->plugins[j].is_opened) {
                break;
            }
        }
        // Enabling if all the dependencies are opened.
        plug->is_enabled = plug->is_opened && (j == ctx->count);
        verbose(4, "[Summary] %s %s: called every %06llu ms with priority %u%s",
                (plug->is_opened) ? "Opened" : "Not opened",
                plug->file_name, plug->time_lapse, plug->priority,
                (plug->is_enabled) ? "" : ", but disabled");
        enabled_count += plug->is_enabled;
    }
    return enabled_count;
}
