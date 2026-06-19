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

// Future use:
// typedef struct msg_cache_s {
//     char tag_msg[128];
//     void *handler;
// } msg_cache_t;

static plugins_t   p; // Struct that holds all plugins context
static char      **priority_paths;
static char        date[128];
static ulong       monitor_period = 100;
static ulong       relax_period   = 100;
static uint        overhead_measurement_en;
static uint        overhead_measurement_time; // in ms
static uint        exit_called;
static uint        initialized;
// Serializes hot-loading with the monitor loop, which walks p.plugins_sorted.
static pthread_mutex_t lock_pm = PTHREAD_MUTEX_INITIALIZER;
static afd_set_t   fds_active; // FDs of our internal poll
static pmfd_set_t  f[AFD_MAX]; // Struct that holds all file-descriptors context

static void output_print_format()
{
    time_t ti    = time(NULL);
    struct tm tm = *localtime(&ti);
    sprintf(date, "%d/%d %d:%02d:%02d", tm.tm_mday, tm.tm_mon + 1, tm.tm_hour, tm.tm_min, tm.tm_sec);
    date[strlen(date)] = '\0';
}

static int output_print(int i, int d, char *prefix, char *output)
{
    int stop_communcations = 0;

    if (output == NULL) {
        return 1;
    }
    if (p.plugins_sorted[i]->is_silenced) {
        return 1;
    }
    // Returning instructions:
    //  [=] Pause
    //  [D] Disable
    //  [X] Exit
    //  [S] Stop communcations
    if (output != NULL) {
        if (strstr(output, "[=]") != NULL) {
            p.plugins_sorted[i]->is_paused = 1;
            output += 3;
        } else if (strstr(output, "[D]") != NULL) {
            p.plugins_sorted[i]->is_enabled = 0;
            output += 3;
        } else if (strstr(output, "[X]") != NULL) {
            exit_called = 1;
        } else if (strstr(output, "[S]") != NULL) {
            stop_communcations = 1;
        }
    }
    // Cleaning spaces
    while (*output == ' ') {
        ++output;
    }
    if (strlen(output) > 0) {
        verbose(0, "[ %s, %21s ] %s%s", date, p.plugins_sorted[i]->file_name, prefix, output);
    }
    if (exit_called || stop_communcations) {
        return 0;
    }
    return 1;
}

static int plugins_action_periodic_call(int i, int d, void *data)
{
    char buffer[256];
    char *sout;

    if (p.plugins_sorted[i]->action_periodic[d] == (void *) UINT64_MAX) {
        return 0;
    }
    if (p.plugins_sorted[i]->action_periodic[d] == NULL) {
        sprintf(buffer, "up_action_periodic_%s", p.plugins_sorted[d]->tag);
        p.plugins_sorted[i]->action_periodic[d] = dlsym(p.plugins_sorted[i]->handler, buffer);
        // If NULL persist
        if (p.plugins_sorted[i]->action_periodic[d] == NULL) {
            p.plugins_sorted[i]->action_periodic[d] = dlsym(p.plugins_sorted[i]->handler, "up_action_periodic");
        }
        // If is still NULL, this address is unreachable
        if (p.plugins_sorted[i]->action_periodic[d] == NULL) {
            p.plugins_sorted[i]->action_periodic[d] = (void *) UINT64_MAX;
            return 0;
        }
    }
    overhead_start(p.plugins_sorted[i]->id_overhead_periodic);
    sout = p.plugins_sorted[i]->action_periodic[d](p.plugins_sorted[d]->tag, data);
    overhead_stop(p.plugins_sorted[i]->id_overhead_periodic);
    return output_print(i, d, "", sout);
}

static void plugins_action_periodic(int i, timestamp_t *time_now, ullong time_passed)
{
    uint time_isup;
    int j;

    // Base case
    if (i == p.count) {
        return;
    }
    if (exit_called) {
        return;
    }
    // If is disabled, next
    if (!p.plugins_sorted[i]->is_enabled) {
        goto next_ap;
    }
    if (p.plugins_sorted[i]->is_paused) {
        goto next_ap;
    }
    // Time calculation
    p.plugins_sorted[i]->time_accum += time_passed;
    time_isup = p.plugins_sorted[i]->time_accum >= p.plugins_sorted[i]->time_lapse;

    if (time_isup) {
        p.plugins_sorted[i]->time_origin = *time_now;
        p.plugins_sorted[i]->time_accum  = p.plugins_sorted[i]->time_accum - p.plugins_sorted[i]->time_lapse;
    }
    debug("[DEBUG] %19s P%d-%u: lapse %llu ms, passed %llu ms, accum %llu ms, time's up %d, data %d",
          p.plugins_sorted[i]->file_name, i, p.plugins_sorted[i]->priority, p.plugins_sorted[i]->time_lapse,
          time_passed, p.plugins_sorted[i]->time_accum, time_isup, p.plugins_sorted[i]->data != NULL);
    // If the plugin has data allocated to work with
    if (time_isup) {
        if (!plugins_action_periodic_call(i, i, p.plugins_sorted[i]->data)) {
            goto next_ap;
        }
        // Feeding others with own data
        for (j = 0; p.plugins_sorted[i]->data != NULL && j < p.count; ++j) {
            if (j == i || !p.plugins_sorted[j]->is_enabled) {
                continue;
            }
            plugins_action_periodic_call(j, i, p.plugins_sorted[i]->data);
        }
    }
next_ap:
    // I think this is OK because the priority system moves the dependencies calls
    // before the own call. I can interpret that a data from a dependency coming
    // before means that data is required to perform the dependent main function,
    // which makes sense.
    plugins_action_periodic(i + 1, time_now, time_passed);
}

static void plugin_manager_after_close()
{
    action_close_f *action_close;
    int i;

    for (i = 0; i < p.count; ++i) {
        if ((action_close = dlsym(p.plugins_sorted[i]->handler, "up_action_close")) != NULL) {
            action_close();
        }
    }
}

static void plugins_poll_attend_fds()
{
    char *sout;
    int fd;

    for (fd = fds_active.fd_min; fd <= fds_active.fd_max; fd++) {
        if (AFD_ISSET(fd, &fds_active)) {
            if (f[fd].poll_attend != NULL) {
                sout = f[fd].poll_attend(f[fd].tag, fd);
                output_print(f[fd].plugin_index, 0, "", sout);
            }
        }
    }
}

static void plugins_poll_attend()
{
    int fds_status;

    if ((fds_status = aselect(&fds_active, 1LLU, NULL)) != -1) {
        //dprintf(STDERR_FILENO, "fds_status %d\n", fds_status);
        if (fds_status > 0) {
            plugins_poll_attend_fds();
        } else if (fds_status == 0) {
            // Timed out
        } else {
            // Error
        }
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

    pthread_mutex_lock(&lock_pm);
    // First time, just to start counting time
    if (time_passed == ULLONG_MAX) {
        timestamp_get(&time_last);
        time_passed = 0;
        pthread_mutex_unlock(&lock_pm);
        return EAR_SUCCESS;
    }
    // Formatting the date
    output_print_format();
    timestamp_get(&time_now);
    time_passed = timestamp_diff(&time_now, &time_last, TIME_MSECS);
    time_accum_overhead += time_passed;
    // Periodic plugin functions call
    plugins_action_periodic(0, &time_now, time_passed);
    plugins_poll_attend();
    plugins_overhead_measurement(&time_accum_overhead);
    // Exit control
    if (exit_called) {
        monitor_dispose();
        plugin_manager_after_close();
    }
    time_last = time_now;
    pthread_mutex_unlock(&lock_pm);
    return EAR_SUCCESS;
}

static int plugins_action_init_call(int i, int d, void **data_alloc, void *data)
{
    char buffer[256];
    char *sout;

    if (p.plugins_sorted[i]->action_init[d] == (void *) UINT64_MAX) {
        return 0;
    }
    if (p.plugins_sorted[i]->action_init[d] == NULL) {
        sprintf(buffer, "up_action_init_%s", p.plugins_sorted[d]->tag);
        p.plugins_sorted[i]->action_init[d] = dlsym(p.plugins_sorted[i]->handler, buffer);
        // If NULL persist
        if (p.plugins_sorted[i]->action_init[d] == NULL) {
            debug("[DEBUG] %s: function '%s'... NOT DETECTED", p.plugins_sorted[i]->file_name, buffer);
            sprintf(buffer, "up_action_init");
            p.plugins_sorted[i]->action_init[d] = dlsym(p.plugins_sorted[i]->handler, buffer);
        }
        // If is still NULL, this address is unreachable
        if (p.plugins_sorted[i]->action_init[d] == NULL) {
            debug("[DEBUG] %s: function '%s'... NOT DETECTED", p.plugins_sorted[i]->file_name, buffer);
            p.plugins_sorted[i]->action_init[d] = (void *) UINT64_MAX;
            return 0;
        }
        debug("[DEBUG] %s: function '%s' DETECTED", p.plugins_sorted[i]->file_name, buffer);
    }
    debug("[DEBUG] %s: called 'action_init' with tag/data '%s'", p.plugins_sorted[i]->file_name, p.plugins_sorted[d]->tag);
    sout = p.plugins_sorted[i]->action_init[d](p.plugins_sorted[d]->tag, data_alloc, data);
    return output_print(i, d, "", sout);
}

static int plugins_action_init_one(int i)
{
    plugin_t *plug = p.plugins_sorted[i];

    if (!plug->is_enabled || plug->is_initialized) {
        return 0;
    }
    plug->is_paused = (plug->time_lapse == 0LLU);
    plug->data = NULL;
    plug->is_initialized = plugins_action_init_call(i, i, &plug->data, plug->data_conf);
    return plug->is_initialized;
}

static void plugins_action_init(int i)
{
    int j;
    // Base case
    if (i == p.count) {
        return;
    }
    // For the first plugin, the output have to be formatted
    if (i == 0) {
        output_print_format();
    }
    if (!plugins_action_init_one(i)) {
        goto next_ai;
    }
    // Feeding others with own data (if it has)
    for (j = 0; p.plugins_sorted[i]->data != NULL && j < p.count; ++j) {
        if (j == i || !p.plugins_sorted[j]->is_enabled) {
            continue;
        }
        // Calling plugin J with plugin I tag/data
        plugins_action_init_call(j, i, NULL, p.plugins_sorted[i]->data);
    }
next_ai:
    plugins_action_init(i + 1);
}

static void plugins_action_init_pending()
{
    int i, j;

    // New plugins must exchange init data with plugins that were already active.
    output_print_format();
    for (i = 0; i < p.count; ++i) {
        plugin_t *source = p.plugins_sorted[i];
        if (!plugins_action_init_one(i)) {
            continue;
        }
        for (j = 0; j < p.count; ++j) {
            plugin_t *target = p.plugins_sorted[j];
            if (j == i || !target->is_enabled || !target->is_initialized) {
                continue;
            }
            if (source->data != NULL) {
                plugins_action_init_call(j, i, NULL, source->data);
            }
            if (target->data != NULL) {
                plugins_action_init_call(i, j, NULL, target->data);
            }
        }
    }
}

static state_t plugins_init(void *whatever)
{
    int enabled = 0;
    pthread_mutex_lock(&lock_pm);
    if ((enabled = plugin_manager_internal_open(&p, priority_paths))) {
        plugin_manager_internal_sort(&p);
        afd_init(&fds_active);
        plugins_action_init(0);
        initialized = 1;
    }
    if (!enabled) {
        monitor_dispose();
    }
    pthread_mutex_unlock(&lock_pm);
    return EAR_SUCCESS;
}

static int plugin_manager_configure(int argc, char *argv[])
{
    char buffer[8192];
    char params[128];
    char verb_lvl[2] = "\0";

    if (argc == 1) {
        verbose(0, "[ERROR] No plugins to load.");
    }
    if (args_get(argc, argv, "help", buffer)) {
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
    // Processing list of priority paths
    if (args_get(argc, argv, "paths", buffer)) {
        // NUll terminated list, count is not required
        strtoa(buffer, ':', &priority_paths, NULL);
    }
    // Processing list of plugins (by now is hardcoded)
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
    // Buffer cannot be used because it has to be parsed later
    if (args_get(argc, argv, "monitor", params)) {
        monitor_period = strtoul(params, NULL, 10);
    }
    // Buffer cannot be used because it has to be parsed later
    if (args_get(argc, argv, "relax", params)) {
        relax_period = strtoul(params, NULL, 10);
    }
    // Overhead measurment
    if (args_get(argc, argv, "overhead", params)) {
        setenv("EAR_OVERHEAD_ENABLE", "all", 1);
        overhead_measurement_time = (ullong) atoi(params);
        overhead_measurement_en = 1;
    }
    verbose(0, "UPM configuration: plugins \'%s\' verbose %u monitor periods[burst %lu, relax %lu]",
        buffer, VERB_GET_LV(), monitor_period, relax_period);
    // Hidden parameters
    if (args_get(argc, argv, "debug", NULL)) {
        ADEBUG_SET_EN(1);
    }
    p.plugins        = calloc(MAX_PLUGINS, sizeof(plugin_t));
    p.plugins_sorted = calloc(MAX_PLUGINS, sizeof(plugin_t *));
    // Reading (1) all the plugins specified in the --plugins argument.
    return plugin_manager_internal_read(&p, buffer, 0LLU, 0, NULL);
}

// Header functions
int plugin_manager_main(int argc, char *argv[])
{
    // Read configuration
    if (!plugin_manager_configure(argc, argv)) {
        return 0;
    }
    // Suscriptions
    suscription_t *sus = suscription();
    sus->call_init     = plugins_init;
    sus->call_main     = plugins_main;
    sus->time_relax    = relax_period;
    sus->time_burst    = monitor_period;
    sus->suscribe(sus);
    // Monitoring metrics
    debug("Initializing monitor");
    if (state_fail(monitor_init())) {
        verbose(0, "Monitor failed: %s", state_msg);
    }
    return 1;
}

int plugin_manager_main2(char *plugins, char *paths)
{
    char p0[1024];
    char p1[1024];
    char p2[1024];
    char *argv[3];
    argv[0] = p0;
    argv[1] = p1;
    argv[2] = p2;
    sprintf(p0, "./bin");
    sprintf(p1, "--plugins=%s", plugins);
    sprintf(p2, "--paths=%s", paths);
    verbose(0, "UPM main: plugin list \'%s\' paths \'%s\'", plugins, paths);
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
        enabled = plugin_manager_internal_open(&p, priority_paths);
        if (enabled) {
            plugin_manager_internal_sort(&p);
            plugins_action_init_pending();
        }
        ok = enabled > 0;
    }
    pthread_mutex_unlock(&lock_pm);
    return ok;
}

void plugin_manager_exit()
{
    exit_called = 1;
}

void plugin_manager_wait()
{
    monitor_wait();
}

void *plugin_manager_action_trigger(cchar *tag)
{
    int i;
    for (i = 0; i < p.count; ++i) {
        if (is_tag(p.plugins_sorted[i]->tag)) {
            if (!p.plugins_sorted[i]->is_enabled) {
                return NULL;
            }
            if (plugins_action_periodic_call(i, i, p.plugins_sorted[i]->data)) {
                return p.plugins_sorted[i]->data;
            }
        }
    }
    return NULL;
}

static void static_plugin_manager_message_send(cchar *tag_msg, void *data_array[], int increase)
{
    message_receive_f *call = NULL;
    char buffer[256];
    char *sout;
    int i;

    for (i = 0; i < p.count; ++i) {
        if (!p.plugins_sorted[i]->is_enabled) {
            continue;
        }
        sprintf(buffer, "up_message_receive_%s", tag_msg);
        // This call could be saved into plugins_sorted structure after the
        // first dlsym() to reduce the overhead.
        call = dlsym(p.plugins_sorted[i]->handler, buffer);
        if (call == NULL) {
            call = dlsym(p.plugins_sorted[i]->handler, "up_message_receive");
        }
        if (call != NULL) {
            sout = call(tag_msg, *data_array);
            if (!output_print(i, 0, "", sout)) {
                // If detected a stopping tag
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
    static_plugin_manager_message_send(tag_msg, &data, 0);
}

void plugin_manager_message_send_each(cchar *tag_msg, void *data_array[])
{
    static_plugin_manager_message_send(tag_msg, data_array, 1);
}

int plugin_manager_poll_add(cchar *tag_own, cchar *tag, int fd)
{
    char buffer[256];
    int i;

    for (i = 0; i < p.count; ++i) {
        if (strcmp(p.plugins_sorted[i]->tag, tag_own) == 0) {
            f[fd].plugin_index = i;
            f[fd].tag = tag;
            if (tag != NULL) {
                sprintf(buffer, "up_poll_attend_%s", tag);
                f[fd].poll_attend = dlsym(p.plugins_sorted[i]->handler, buffer);
            }
            if (f[fd].poll_attend == NULL) {
                f[fd].poll_attend = dlsym(p.plugins_sorted[i]->handler, "up_poll_attend");
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
    f[fd].plugin_index = UINT32_MAX;
    f[fd].poll_attend = NULL;
    AFD_CLR(fd, &fds_active);
}
