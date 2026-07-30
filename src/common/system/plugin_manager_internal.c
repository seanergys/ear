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

typedef struct plugin_spec_s {
    char    path[sizeof(((plugin_t *) 0)->path)];
    char  **config_tokens;
    void   *data_conf;
    ullong  time_lapse;
    uint    time_lapse_is_set;
    uint    priority;
    uint    mandatory;
    uint    time_lapse_inherit;
    uint    is_silenced;
} plugin_spec_t;

static int plugin_spec_parse(cchar *string, ullong dependent_time_lapse, uint priority, uint priority_offset,
                             plugin_spec_t *spec)
{
    uint count = 0;
    char **tokens = NULL;
    char *file;
    size_t length;
    uint priority_plus = 0;

    memset(spec, 0, sizeof(*spec));
    if (strtoa(string, ':', &tokens, &count) != NULL) {
        uint i;
        uint first_config = 1;
        for (i = 1; tokens[i] != NULL; ++i) {
            adebug("[Debug] %s: arg[%u] %s", tokens[0], i, tokens[i]);
        }
        if (count > 1 && strisnum(tokens[1])) {
            spec->time_lapse = (ullong) atoi(tokens[1]);
            spec->time_lapse_is_set = spec->time_lapse > 0;
            first_config = 2;
        }
        if (first_config < count) {
            spec->data_conf = &tokens[first_config];
        }
        spec->config_tokens = tokens;
        file = tokens[0];
    } else {
        file = (char *) string;
    }
    while (*file == '!' || *file == '<' || *file == '^' || *file == '-') {
        spec->mandatory          |= (*file == '!');
        spec->time_lapse_inherit |= (*file == '<');
        spec->is_silenced        |= (*file == '-');
        priority_plus            |= (*file == '^') * MAX_PLUGINS;
        ++file;
    }
    if (*file == '\0') {
        goto error;
    }
    length = strlen(file);
    if (snprintf(spec->path, sizeof(spec->path), "%s%s", file,
                 (length >= 3 && strcmp(file + length - 3, ".so") == 0) ? "" : ".so")
        >= (int) sizeof(spec->path)) {
        verbose(0, "[ERROR] Plugin path is too long: %s", file);
        goto error;
    }
    spec->priority = priority + priority_offset + priority_plus;
    if (!spec->time_lapse_is_set && spec->time_lapse_inherit && dependent_time_lapse > 0) {
        spec->time_lapse = dependent_time_lapse;
    }
    return 1;

error:
    if (tokens != NULL) {
        strtoa_free(tokens);
    }
    return 0;
}

int plugin_manager_internal_read(plugins_t *ctx, cchar *string, ullong dependent_time_lapse, uint priority, uint *dependent_deps_table)
{
    uint list_count;
    char **list;
    uint l;

    debug("plugin string: %s", string);
    if (strtoa(string, SEPARATOR, &list, &list_count) == NULL) {
        return 0;
    }
    debug("detected plugins: #%u", list_count);
    for (l = 0; l < list_count; ++l) {
        plugin_spec_t spec;
        plugin_t *plug;
        char *file_name;
        uint i;

        debug("plugin[%u]: %s", l, list[l]);
        if (ctx->count == MAX_PLUGINS) {
            verbose(0, "[ERROR] Maximum number of plugins reached (%u).", MAX_PLUGINS);
            strtoa_free(list);
            return 0;
        }
        if (!plugin_spec_parse(list[l], dependent_time_lapse, priority, list_count - l, &spec)) {
            strtoa_free(list);
            return 0;
        }
        file_name = strrchr(spec.path, '/');
        file_name = (file_name != NULL) ? file_name + 1 : spec.path;
        for (i = 0; i < ctx->count && strcmp(ctx->plugins[i].file_name, file_name) != 0; ++i) {
        }
        plug = &ctx->plugins[i];
        if (dependent_deps_table != NULL) {
            dependent_deps_table[i] = spec.mandatory;
        }
        if (i < ctx->count) {
            if (plug->priority < spec.priority) {
                plug->priority = spec.priority;
            }
            if (spec.time_lapse_inherit && dependent_time_lapse > 0
                && (!plug->time_lapse_is_set || dependent_time_lapse < plug->time_lapse)) {
                plug->time_lapse = dependent_time_lapse;
            }
            plug->is_silenced |= spec.is_silenced;
            if (spec.config_tokens != NULL) {
                strtoa_free(spec.config_tokens);
            }
            continue;
        }
        memset(plug, 0, sizeof(*plug));
        snprintf(plug->path, sizeof(plug->path), "%s", spec.path);
        file_name = strrchr(plug->path, '/');
        plug->file_name = (file_name != NULL) ? file_name + 1 : plug->path;
        plug->config_tokens = spec.config_tokens;
        plug->data_conf     = spec.data_conf;
        plug->time_lapse    = spec.time_lapse;
        plug->time_lapse_is_set = spec.time_lapse_is_set;
        plug->priority      = spec.priority;
        plug->is_silenced   = spec.is_silenced;
        ++ctx->count;
    }
    strtoa_free(list);
    return 1;
}

void plugin_manager_internal_sort(plugins_t *ctx)
{
    uint i;

    for (i = 0; i < ctx->count; ++i) {
        plugin_t *plug = &ctx->plugins[i];
        uint insertion = i;

        while (insertion > 0 && ctx->plugins_sorted[insertion - 1]->priority < plug->priority) {
            ctx->plugins_sorted[insertion] = ctx->plugins_sorted[insertion - 1];
            --insertion;
        }
        ctx->plugins_sorted[insertion] = plug;
    }
    memset(&ctx->plugins_sorted[ctx->count], 0, (MAX_PLUGINS - ctx->count) * sizeof(plugin_t *));
    for (i = 0; i < ctx->count; ++i) {
        plugin_t *plug = ctx->plugins_sorted[i];
        adebug("[Debug] %s: time lapse '%4llu', time_lapse_is_set %u, priority '%u'", plug->path,
               plug->time_lapse, plug->time_lapse_is_set, plug->priority);
    }
}

static state_t plugins_open_so(char *path, void **handler)
{
    if (access(path, X_OK) != 0) {
        return_msg(EAR_ERROR, strerror(errno));
    }
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

    while (priority_paths != NULL && priority_paths[j] != NULL) {
        if (snprintf(new_path, sizeof(new_path), "%s/%s", priority_paths[j], ctx->plugins[i].path)
            >= (int) sizeof(new_path)) {
            verbose(4, "[Configuration] Plugin path is too long: %s/%s", priority_paths[j], ctx->plugins[i].path);
            ++j;
            continue;
        }
        if (state_ok(plugins_open_so(new_path, (void **) &ctx->plugins[i].handler))) {
            verbose(4, "[Configuration] Trying to open: %s... OK", new_path);
            return EAR_SUCCESS;
        }
        verbose(4, "[Configuration] Trying to open: %s... FAILED (%s)", new_path, state_msg);
        ++j;
    }
    if (state_ok(plugins_open_so(ctx->plugins[i].path, (void **) &ctx->plugins[i].handler))) {
        verbose(4, "[Configuration] Trying to open: %s... OK", ctx->plugins[i].path);
        return EAR_SUCCESS;
    }
    verbose(4, "[Configuration] Trying to open: %s... FAILED", ctx->plugins[i].path);
    if ((e = ear_getenv(ENV_PATH_EAR)) != NULL) {
        if (snprintf(new_path, sizeof(new_path), "%s/lib/plugins/monitoring/%s", e, ctx->plugins[i].path)
            >= (int) sizeof(new_path)) {
            verbose(4, "[Configuration] Environment plugin path is too long: %s", ctx->plugins[i].path);
            return EAR_ERROR;
        }
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
    uint i, j;

    for (i = 0; i < ctx->count; ++i) {
        if (ctx->plugins[i].is_opened) {
            continue;
        }
        if (state_fail(plugins_open_file(ctx, priority_paths, i))) {
            continue;
        }
        plug = &ctx->plugins[i];
        if ((get_tag = dlsym(plug->handler, "up_get_tag")) == NULL) {
            debug("[DEBUG] %s get_tag() not detected", plug->path);
            dlclose(plug->handler);
            plug->handler = NULL;
            continue;
        }
        get_tag(&plug->tag, &plug->tags_deps);
        if (plug->tag == NULL || plug->tag[0] == '\0') {
            debug("[DEBUG] %s returned an empty tag", plug->path);
            dlclose(plug->handler);
            plug->handler = NULL;
            continue;
        }
        debug("[DEBUG] %s after call get_tag(): tag '%s', tags_deps '%s'",
              plug->path, (char *) plug->tag, (char *) plug->tags_deps);
        plug->is_opened = 1;
        if (plug->tags_deps != NULL) {
            plugin_manager_internal_read(ctx, plug->tags_deps, plug->time_lapse, plug->priority, plug->deps_table);
        }
        overhead_subsprint(&plug->id_overhead_periodic, "periodic %s", plug->tag);
    }
    for (i = 0; i < ctx->count; ++i) {
        plug = &ctx->plugins[i];
        for (j = 0; j < ctx->count; ++j) {
            if (plug->deps_table[j] && !ctx->plugins[j].is_opened) {
                break;
            }
        }
        plug->is_enabled = plug->is_opened && (j == ctx->count);
        verbose(4, "[Summary] %s %s: called every %06llu ms with priority %u%s",
                (plug->is_opened) ? "Opened" : "Not opened",
                plug->file_name, plug->time_lapse, plug->priority,
                (plug->is_enabled) ? "" : ", but disabled");
        enabled_count += plug->is_enabled;
    }
    return enabled_count;
}

void plugin_manager_internal_dispose(plugins_t *ctx)
{
    uint i;

    for (i = 0; i < ctx->count; ++i) {
        if (ctx->plugins[i].config_tokens != NULL) {
            strtoa_free(ctx->plugins[i].config_tokens);
            ctx->plugins[i].config_tokens = NULL;
            ctx->plugins[i].data_conf = NULL;
        }
    }
}
