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
//#define SHOW_DEBUGS 1
#define _GNU_SOURCE
#include <pthread.h>
#include <stdlib.h>
#include <common/utils/args.h>
#include <common/output/debug.h>
#include <common/utils/overhead.h>
#include <common/system/poll.h>
#include <common/system/monitor.h>
#include <common/system/symplug.h>
#include <common/system/plugin_manager.h>
#include <common/system/plugin_manager_internal.h>

static plugins_t   p; // Struct that holds all plugins context
static char      **priority_paths;
static char        date[128];
static ulong       monitor_period = 100;
static ulong       relax_period   = 100;
static uint        overhead_measurement_en;
static uint        exit_called;
static uint        initialized;
// Serializes hot-loading with the monitor loop, which walks p.plugins_sorted.
static pthread_mutex_t lock_pm = PTHREAD_MUTEX_INITIALIZER;
static afd_set_t   fds_active; // FDs of our internal poll
static pmfd_set_t  f[AFD_MAX]; // Struct that holds all file-descriptors context

typedef enum plugins_init_mode_e {
    PLUGINS_INIT_COLD,
    PLUGINS_INIT_PENDING
} plugins_init_mode_t;

typedef enum plugin_init_result_e {
    PLUGIN_INIT_STOPPED,
    PLUGIN_INIT_OK,
    PLUGIN_INIT_NOT_IMPLEMENTED
} plugin_init_result_t;

static uint plugin_id(plugin_t *plug)
{
    return (uint) (plug - p.plugins);
}

static void output_print_format(void)
{
    time_t ti    = time(NULL);
    struct tm tm = *localtime(&ti);
    snprintf(date, sizeof(date), "%d/%d %d:%02d:%02d", tm.tm_mday, tm.tm_mon + 1,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
}

static int plugin_output(plugin_t *plug, char *output)
{
    int stop_communcations = 0;

    if (output == NULL || plug->is_silenced) {
        return 1;
    }
    // Returning instructions:
    //  [=] Pause
    //  [D] Disable
    //  [X] Exit
    //  [S] Stop communcations
    if (strstr(output, "[=]") != NULL) {
        plug->is_paused = 1;
        output += 3;
    } else if (strstr(output, "[D]") != NULL) {
        plug->is_enabled = 0;
        output += 3;
    } else if (strstr(output, "[X]") != NULL) {
        exit_called = 1;
    } else if (strstr(output, "[S]") != NULL) {
        stop_communcations = 1;
    }
    // Cleaning spaces
    while (*output == ' ') {
        ++output;
    }
    if (strlen(output) > 0) {
        verbose(0, "[ %s, %21s ] %s", date, plug->file_name, output);
    }
    return !(exit_called || stop_communcations);
}

static void *plugin_action_resolve(plugin_t *consumer, plugin_t *source, cchar *action)
{
    char buffer[256];
    void *call;

    if (snprintf(buffer, sizeof(buffer), "up_action_%s_%s", action, source->tag) >= (int) sizeof(buffer)) {
        return NULL;
    }
    call = dlsym(consumer->handler, buffer);
    if (call == NULL && consumer == source) {
        snprintf(buffer, sizeof(buffer), "up_action_%s", action);
        call = dlsym(consumer->handler, buffer);
    }
    debug("[DEBUG] %s: function '%s' %s", consumer->file_name, buffer,
          (call != NULL) ? "DETECTED" : "NOT DETECTED");
    return call;
}

static int plugins_action_periodic_call(plugin_t *consumer, plugin_t *source, void *data)
{
    uint source_id = plugin_id(source);
    char *sout;

    if (!consumer->action_periodic_resolved[source_id]) {
        consumer->action_periodic[source_id] = plugin_action_resolve(consumer, source, "periodic");
        consumer->action_periodic_resolved[source_id] = 1;
    }
    if (consumer->action_periodic[source_id] == NULL) {
        return 0;
    }
    overhead_start(consumer->id_overhead_periodic);
    sout = consumer->action_periodic[source_id](source->tag, data);
    overhead_stop(consumer->id_overhead_periodic);
    return plugin_output(consumer, sout);
}

static void plugins_action_periodic(ullong time_passed)
{
    uint i;

    for (i = 0; i < p.count && !exit_called; ++i) {
        plugin_t *source = p.plugins_sorted[i];
        uint time_isup;
        uint j;

        if (!source->is_enabled || source->is_paused) {
            continue;
        }
        source->time_accum += time_passed;
        time_isup = source->time_accum >= source->time_lapse;
        if (time_isup) {
            source->time_accum -= source->time_lapse;
        }
        debug("[DEBUG] %19s P%u-%u: lapse %llu ms, passed %llu ms, accum %llu ms, time's up %u, data %d",
              source->file_name, i, source->priority, source->time_lapse,
              time_passed, source->time_accum, time_isup, source->data != NULL);
        if (!time_isup || !plugins_action_periodic_call(source, source, source->data)) {
            continue;
        }
        for (j = 0; source->data != NULL && j < p.count; ++j) {
            plugin_t *consumer = p.plugins_sorted[j];
            if (consumer == source || !consumer->is_enabled) {
                continue;
            }
            plugins_action_periodic_call(consumer, source, source->data);
        }
    }
}

static void plugin_manager_after_close(void)
{
    action_close_f *action_close;
    uint i;

    for (i = 0; i < p.count; ++i) {
        plugin_t *plug = p.plugins_sorted[i];
        if (plug->is_opened && plug->handler != NULL
            && (action_close = dlsym(plug->handler, "up_action_close")) != NULL) {
            (void) action_close();
        }
    }
    plugin_manager_internal_dispose(&p);
}

static void plugins_poll_attend_fds(void)
{
    char *sout;
    int fd;

    for (fd = fds_active.fd_min; fd <= fds_active.fd_max; fd++) {
        if (AFD_ISSET(fd, &fds_active) && f[fd].poll_attend != NULL) {
            sout = f[fd].poll_attend(f[fd].tag, fd);
            plugin_output(f[fd].plugin, sout);
        }
    }
}

static void plugins_poll_attend(void)
{
    if (aselect(&fds_active, 1LLU, NULL) > 0) {
        plugins_poll_attend_fds();
    }
}

static void plugins_overhead_measurement(ullong *time_accum_overhead)
{
    if (overhead_measurement_en && *time_accum_overhead >= 10000) {
        *time_accum_overhead = 0LLU;
        printf("[ ################################################################################################################################ Overhead report ## ]\n");
        overhead_report(1);
        printf("[ ################################################################################################################################################### ]\n");
    }
}

static state_t plugins_main(void *whatever)
{
    static timestamp_t time_now       = {0};
    static timestamp_t time_last      = {0};
    static ullong time_passed         = ULLONG_MAX; // In msecs
    static ullong time_accum_overhead = 0LLU;

    (void) whatever;
    pthread_mutex_lock(&lock_pm);
    if (time_passed == ULLONG_MAX) {
        timestamp_get(&time_last);
        time_passed = 0;
        pthread_mutex_unlock(&lock_pm);
        return EAR_SUCCESS;
    }
    output_print_format();
    timestamp_get(&time_now);
    time_passed = timestamp_diff(&time_now, &time_last, TIME_MSECS);
    time_accum_overhead += time_passed;
    plugins_action_periodic(time_passed);
    plugins_poll_attend();
    plugins_overhead_measurement(&time_accum_overhead);
    if (exit_called) {
        monitor_dispose();
        plugin_manager_after_close();
    }
    time_last = time_now;
    pthread_mutex_unlock(&lock_pm);
    return EAR_SUCCESS;
}

static plugin_init_result_t plugins_action_init_call(plugin_t *consumer, plugin_t *source,
                                                     void **data_alloc, void *data)
{
    uint source_id = plugin_id(source);
    char *sout;

    if (!consumer->action_init_resolved[source_id]) {
        consumer->action_init[source_id] = plugin_action_resolve(consumer, source, "init");
        consumer->action_init_resolved[source_id] = 1;
    }
    if (consumer->action_init[source_id] == NULL) {
        return PLUGIN_INIT_NOT_IMPLEMENTED;
    }
    debug("[DEBUG] %s: called 'action_init' with tag/data '%s'", consumer->file_name, source->tag);
    sout = consumer->action_init[source_id](source->tag, data_alloc, data);
    return plugin_output(consumer, sout) ? PLUGIN_INIT_OK : PLUGIN_INIT_STOPPED;
}

static int plugins_action_init_one(plugin_t *plug)
{
    plugin_init_result_t result;

    if (!plug->is_enabled || plug->is_initialized) {
        return 0;
    }
    plug->is_paused = (plug->time_lapse == 0LLU);
    plug->data = NULL;
    result = plugins_action_init_call(plug, plug, &plug->data, plug->data_conf);
    // A plugin may only consume dependency data and legitimately have no own init.
    plug->is_initialized = (result != PLUGIN_INIT_STOPPED);
    return plug->is_initialized;
}

static void plugins_initialize(plugins_init_mode_t mode)
{
    uint i;

    output_print_format();
    for (i = 0; i < p.count; ++i) {
        plugin_t *source = p.plugins_sorted[i];
        uint j;

        if (!plugins_action_init_one(source)) {
            continue;
        }
        for (j = 0; j < p.count; ++j) {
            plugin_t *target = p.plugins_sorted[j];
            if (target == source || !target->is_enabled
                || (mode == PLUGINS_INIT_PENDING && !target->is_initialized)) {
                continue;
            }
            if (source->data != NULL) {
                plugins_action_init_call(target, source, NULL, source->data);
            }
            if (mode == PLUGINS_INIT_PENDING && target->data != NULL) {
                plugins_action_init_call(source, target, NULL, target->data);
            }
        }
    }
    // All own and dependency init callbacks must finish before this phase.
    for (i = 0; i < p.count && !exit_called; ++i) {
        plugin_t *plug = p.plugins_sorted[i];
        action_periodic_f *call;

        if (!plug->is_enabled || !plug->is_initialized) {
            continue;
        }
        call = plugin_action_resolve(plug, plug, "init_post");
        if (call != NULL) {
            plugin_output(plug, call(plug->tag, plug->data));
        }
    }
}

static int plugins_open_and_initialize(plugins_init_mode_t mode, int initialize_poll)
{
    int enabled = plugin_manager_internal_open(&p, priority_paths);

    if (enabled) {
        plugin_manager_internal_sort(&p);
        if (initialize_poll) {
            afd_init(&fds_active);
        }
        plugins_initialize(mode);
    }
    return enabled;
}

static state_t plugins_init(void *whatever)
{
    int enabled;

    (void) whatever;
    pthread_mutex_lock(&lock_pm);
    enabled = plugins_open_and_initialize(PLUGINS_INIT_COLD, 1);
    if (enabled) {
        initialized = 1;
    } else {
        monitor_dispose();
    }
    pthread_mutex_unlock(&lock_pm);
    return EAR_SUCCESS;
}

static int plugin_manager_configure(int argc, char *argv[])
{
    char buffer[8192];
    char params[128];
    char verb_lvl[128] = "\0";

    if (argc == 1 || args_get(argc, argv, "help", buffer)) {
        if (argc == 1) {
            verbose(0, "[ERROR] No plugins to load.\n");
        }
        verbose(0, "Usage: %s [OPTIONS]\n", argv[0]);
        verbose(0, "Options:");
        verbose(0, "    --plugins=<list> List of '%c' separated plugins to load.", SEPARATOR);
        verbose(0, "    --paths=<list>   List of colon separated priority paths to search plugins.");
        verbose(0, "    --verbose=<lv>   Set the verbose level (1 to 4).");
        verbose(0, "    --silence        Hide messages returned by plugins.");
        verbose(0, "    --overhead       Enables the overhead measurement system.");
        verbose(0, "    --monitor=<ms>   Period at which the system wake ups to check if there is any\n");
        verbose(0, "                     plugin ready. Default 100 ms");
        verbose(0, "    --relax=<ms>     Period at which the system wakes up to check if there is any\n. Def=100 ms  ");
        verbose(0, "                     plugin ready in relaxed mode. Default 100 ms");
        verbose(0, "    --help           If you see it you already typed --help.");
        // --debug is not shown
        return 0;
    }
    if (args_get(argc, argv, "paths", buffer)) {
        strtoa(buffer, ':', &priority_paths, NULL);
    }
    if (!args_get(argc, argv, "plugins", buffer)) {
        verbose(0, "[ERROR] No plugins to load.");
        return 0;
    }
    if (args_get(argc, argv, "verbose", verb_lvl)) {
        if (verb_lvl[0] == '\0') { VERB_SET_LV(PM_DEFAULT_VERB); }
        else                     { VERB_SET_LV(atoi(verb_lvl)); }
    }
    if (args_get(argc, argv, "silence", NULL)) {
        VERB_SET_EN(0);
    }
    if (args_get(argc, argv, "monitor", params)) {
        monitor_period = strtoul(params, NULL, 10);
    }
    if (args_get(argc, argv, "relax", params)) {
        relax_period = strtoul(params, NULL, 10);
    }
    if (args_get(argc, argv, "overhead", params)) {
        setenv("EAR_OVERHEAD_ENABLE", "all", 1);
        overhead_measurement_en = 1;
    }
    verbose(0, "UPM configuration: plugins \'%s\' verbose %u monitor periods[burst %lu, relax %lu]",
        buffer, VERB_GET_LV(), monitor_period, relax_period);
    if (args_get(argc, argv, "debug", NULL)) {
        ADEBUG_SET_EN(1);
    }
    p.plugins        = calloc(MAX_PLUGINS, sizeof(plugin_t));
    p.plugins_sorted = calloc(MAX_PLUGINS, sizeof(plugin_t *));
    if (p.plugins == NULL || p.plugins_sorted == NULL) {
        verbose(0, "[ERROR] Unable to allocate the Plugin Manager context.");
        free(p.plugins);
        free(p.plugins_sorted);
        p.plugins = NULL;
        p.plugins_sorted = NULL;
        return 0;
    }
    if (!plugin_manager_internal_read(&p, buffer, 0LLU, 0, NULL)) {
        plugin_manager_internal_dispose(&p);
        free(p.plugins);
        free(p.plugins_sorted);
        p.plugins = NULL;
        p.plugins_sorted = NULL;
        return 0;
    }
    return 1;
}

int plugin_manager_main(int argc, char *argv[])
{
    if (!plugin_manager_configure(argc, argv)) {
        return 0;
    }
    suscription_t *sus = suscription();
    sus->call_init     = plugins_init;
    sus->call_main     = plugins_main;
    sus->time_relax    = relax_period;
    sus->time_burst    = monitor_period;
    sus->suscribe(sus);
    debug("Initializing monitor");
    if (state_fail(monitor_init())) {
        verbose(0, "Monitor failed: %s", state_msg);
    }
    return 1;
}

int plugin_manager_main2(char *plugins, char *paths)
{
    char p0[] = "./bin";
    char p1[8192];
    char p2[8192];
    char *argv[3];

    if (plugins == NULL
        || snprintf(p1, sizeof(p1), "--plugins=%s", plugins) >= (int) sizeof(p1)) {
        return 0;
    }
    argv[0] = p0;
    argv[1] = p1;
    if (paths != NULL) {
        if (snprintf(p2, sizeof(p2), "--paths=%s", paths) >= (int) sizeof(p2)) {
            return 0;
        }
        argv[2] = p2;
    }
    verbose(0, "UPM main: plugin list \'%s\' paths \'%s\'", plugins,
            (paths != NULL) ? paths : "(none)");
    return plugin_manager_main((paths != NULL) ? 3 : 2, argv);
}

int plugin_manager_load_more_plugins(char *plugins)
{
    int enabled;
    int ok;

    if (plugins == NULL || p.plugins == NULL || p.plugins_sorted == NULL) {
        return 0;
    }
    pthread_mutex_lock(&lock_pm);
    ok = plugin_manager_internal_read(&p, plugins, 0LLU, 0, NULL);
    if (ok && initialized) {
        enabled = plugins_open_and_initialize(PLUGINS_INIT_PENDING, 0);
        ok = enabled > 0;
    }
    pthread_mutex_unlock(&lock_pm);
    return ok;
}

void plugin_manager_exit(void)
{
    exit_called = 1;
}

void plugin_manager_wait(void)
{
    monitor_wait();
}

void *plugin_manager_action_trigger(cchar *tag)
{
    uint i;

    if (tag == NULL) {
        return NULL;
    }
    for (i = 0; i < p.count; ++i) {
        plugin_t *plug = p.plugins_sorted[i];
        if (strcmp(plug->tag, tag) == 0) {
            if (!plug->is_enabled) {
                return NULL;
            }
            if (plugins_action_periodic_call(plug, plug, plug->data)) {
                return plug->data;
            }
        }
    }
    return NULL;
}

static void plugin_manager_message_send_array(cchar *tag_msg, void *data_array[], int increase)
{
    message_receive_f *call = NULL;
    char buffer[256];
    char *sout;
    uint i;

    if (tag_msg == NULL || data_array == NULL) {
        return;
    }
    if (snprintf(buffer, sizeof(buffer), "up_message_receive_%s", tag_msg) >= (int) sizeof(buffer)) {
        return;
    }
    for (i = 0; i < p.count; ++i) {
        plugin_t *plug = p.plugins_sorted[i];
        if (!plug->is_enabled) {
            continue;
        }
        call = dlsym(plug->handler, buffer);
        if (call == NULL) {
            call = dlsym(plug->handler, "up_message_receive");
        }
        if (call != NULL) {
            sout = call(tag_msg, *data_array);
            if (!plugin_output(plug, sout)) {
                return;
            }
            if (increase) {
                ++data_array;
            }
        }
    }
}

void plugin_manager_message_send(cchar *tag_msg, void *data)
{
    plugin_manager_message_send_array(tag_msg, &data, 0);
}

void plugin_manager_message_send_each(cchar *tag_msg, void *data_array[])
{
    plugin_manager_message_send_array(tag_msg, data_array, 1);
}

int plugin_manager_poll_add(cchar *tag_own, cchar *tag, int fd)
{
    char buffer[256];
    uint i;

    if (tag_own == NULL || fd < 0 || fd >= AFD_MAX) {
        return 0;
    }
    for (i = 0; i < p.count; ++i) {
        plugin_t *plug = p.plugins_sorted[i];
        if (strcmp(plug->tag, tag_own) == 0) {
            f[fd].plugin = plug;
            f[fd].tag = tag;
            f[fd].poll_attend = NULL;
            if (tag != NULL) {
                if (snprintf(buffer, sizeof(buffer), "up_poll_attend_%s", tag) >= (int) sizeof(buffer)) {
                    return 0;
                }
                f[fd].poll_attend = dlsym(plug->handler, buffer);
            }
            if (f[fd].poll_attend == NULL) {
                f[fd].poll_attend = dlsym(plug->handler, "up_poll_attend");
            }
            if (f[fd].poll_attend != NULL) {
                AFD_SET(fd, &fds_active);
                return 1;
            }
        }
    }
    return 0;
}

void plugin_manager_poll_remove(int fd)
{
    if (fd < 0 || fd >= AFD_MAX) {
        return;
    }
    f[fd].plugin = NULL;
    f[fd].poll_attend = NULL;
    f[fd].tag = NULL;
    AFD_CLR(fd, &fds_active);
}
