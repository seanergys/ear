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

//#define SHOW_DEBUGS 1

#include <stdlib.h>
#include <common/utils/args.h>
#include <common/output/debug.h>
#include <common/system/monitor.h>
#include <common/utils/strscreen.h>
#include <common/system/symplug.h>
//#include <daemon/local_api/eard_api.h>
//#include <metrics/metrics.h>
#include <plugin_manager/plugin_manager.h>

static const char *names[] = {
    "up_get_tag",
    "up_init",
    "up_callback",
    "up_post",
};

#define N 4

typedef struct plugin_manager_ops_s {
    void    (*get_tag)    (cchar **tag);
    state_t (*init)       (void **data);
    char   *(*callback)   (cchar *tag, void *data);
    char   *(*post)       (cchar *msg, void *data);
} plugin_ops_t;

typedef struct plugin_s {
    char         path[PATH_MAX];
    char        *name;
    cchar       *tag;
    plugin_ops_t ops;
    ullong       time_lapse; //In ms
    timestamp_t  time_origin;
    uint         one_time;
    uint         no_more;
    void        *data;
    uint         data_count;
    uint         init;
} plugin_t;

typedef struct plugins_s {
    plugin_t *plugins;
    uint      count;
} plugins_t;

static plugins_t p;
static char date[128];

static state_t plugins_init(void *whatever)
{
    int i, j, k;
    for (i = 0; i < p.count; ++i) {
        debug("PLUG%d %s: open, called every %llu seconds", i, p.plugins[i].path, p.plugins[i].time_lapse);
        if (state_fail(plug_open(p.plugins[i].path, (void **) &p.plugins[i].ops, names, N, RTLD_NOW | RTLD_GLOBAL))) {
            fprintf(stderr, "%s open failed: %s\n", p.plugins[i].path, state_msg);
            continue;
        }
        if (p.plugins[i].ops.init != NULL) {
            debug("PLUG%d %s: init", i, p.plugins[i].path);
            if (state_fail(p.plugins[i].ops.init(&p.plugins[i].data))) {
                fprintf(stderr, "%s init failed: %s\n", p.plugins[i].path, state_msg);
                continue;
            }
            p.plugins[i].init = 1;
        }
        if (p.plugins[i].ops.get_tag != NULL) {
            p.plugins[i].ops.get_tag(&p.plugins[i].tag);
            debug("PLUG%d %s: get_tag returned, tag %s", i, p.plugins[i].path, (char *) p.plugins[i].tag);
        }
    }
    return EAR_SUCCESS;
}

static void format_output()
{
    time_t tm = time(NULL);
    sprintf(date, "%s", ctime(&tm));
    date[strlen(date)-1] = '\0';
}

static void print_output(char *plug, cchar *tag, char *output)
{
    if (output != NULL) {
        printf("[%s, %s, %s] %s\n", date, plug, tag, output);
    }
}

static void time_and_callback(int i)
{
    timestamp_t time_now;
    ullong time_passed;
    uint time_isup;
    char *output;
    int j;

    // Caso base
    if (i == p.count) {
        return;
    }
    // Time calculation
    timestamp_get(&time_now);
    time_passed = timestamp_diff(&time_now, &p.plugins[i].time_origin, TIME_MSECS);
    time_isup   = time_passed >= p.plugins[i].time_lapse;

    if (time_isup) {
        p.plugins[i].time_origin = time_now;
    }
    // Switched off
    if (p.plugins[i].no_more) {
        time_isup = 0;
    }
    // One timers
    if (p.plugins[i].one_time) {
        p.plugins[i].one_time = 0;
        p.plugins[i].no_more = 1;
        time_isup = 1;
    }
    debug("PLUG%d time's up: %d, called every %llu ms, data %p", i,
        time_isup, p.plugins[i].time_lapse, p.plugins[i].data);
    #define PRINT(f) print_output(p.plugins[i].name, p.plugins[i].tag, f); 
    // If the plugin has data allocated to work with
    if (time_isup && p.plugins[i].data != NULL) {
        if (p.plugins[i].ops.callback != NULL) {
            debug("PLUG%d, %s: callback calling itself with tag %s", i, p.plugins[i].name, p.plugins[i].tag);
            PRINT(p.plugins[i].ops.callback(p.plugins[i].tag, p.plugins[i].data));
        }
        // Feeding others with own data
        for (j = 0; j < p.count; ++j) {
            if (j == i) {
                continue;
            }
            // If pluginI has data valid for pluginJ
            if (p.plugins[j].ops.callback != NULL) { //&& strstr(p.plugins[j].tag, p.plugins[i].tag)
                debug("PLUG%d, %s: callback calling plugin %s with tag %s", i, p.plugins[i].name, p.plugins[j].name, p.plugins[i].tag);
                PRINT(p.plugins[j].ops.callback(p.plugins[i].tag, p.plugins[i].data));
            }
        }
    }
    time_and_callback(i+1);
}

static state_t plugins_callback(void *whatever)
{
    format_output();
    time_and_callback(0);
}

static void plugin_manager_configure_paths(char *plugins, char *ppaths)
{
    char *p = plugins;
    uint list_count;
    char **list;
    
    // Get list of plugins to load and times
    debug("plugin string: %s", plugins);
    if (strchr(plugins, ',')) {
        strtoa(plugins, ',', &list, &list_count);
    } else {
        list = &p;
        list_count = 1;
    }
    // Allocating p
    p.count = list_count;
    p.plugins = calloc(p.count, sizeof(plugin_t));
    debug("detected plugins: #%d", list_count);
    // Filling p
    for (i = 0; i < list_count; ++i) {
        if ((c = strchr(list[i], ':')) != NULL) {
            c[0] = '\0';
            p.plugins[i].time_lapse = (ullong) atoi(&c[1]);
        }
        if (p.plugins[i].time_lapse == 0LLU) {
            p.plugins[i].time_lapse = 100000LLU;
            p.plugins[i].one_time   = 1;
        }
        sprintf(p.plugins[i].path, "%s", list[i]);
        // Getting the plugin name for printing purposes
        for (j = strlen(p.plugins[i].path)-2; j > 0; --j) {
            p.plugins[i].name = &p.plugins[i].path[j+1];
            if (p.plugins[i].path[j] == '/') {
                break;
            }
        }
    }
    //free(*list);
}

static int plugin_manager_configure(int argc, char *argv[])
{
    char buffer_plugins[8192];
    char buffer_ppaths[4096];
    uint i, j;
    char *c;

    if (argc == 1 || args_get(argc, argv, "help", buffer)) {
        verbose(0, "Usage: %s [OPTIONS]\n", argv[0]);
        verbose(0, "Options:");
  //      verbose(0, "    --eard       Connect to EAR Daemon.");
        verbose(0, "    --plugins    List of separated plugins to load.");
        verbose(0, "    --path       Add an priority path to search plugins.");
        verbose(0, "    --help       If you see it you already typed --help.");
        return 0;
    }
    //if (args_get(argc, argv, "eard", buffer)) {
        //eards_connection();
    //}
    if (!args_get(argc, argv, "plugins", buffer_plugins)) {
        return 0;
    }
    if (args_get(argc, argv, "path", buffer_ppaths)) {
        return plugin_manager_configure_paths(buffer_plugins, buffer_ppaths);
    }
    return plugin_manager_configure_paths(buffer_plugins, NULL);
}

void plugin_mananger_post(cchar *msg, void *data)
{
    int i;
    for (i = 0; i < p.count; ++i) {
        if (p.plugins[i].ops.post == NULL) {
            continue;
        }
        p.plugins[i].ops.post(msg, data);
    }
}

int plugin_manager_init(int argc, char *argv[])
{
    // Read configuration
    if (!plugin_manager_configure(argc, argv)) {
        // Error message
        return 0;
    }
    // Suscriptions
    suscription_t *sus = suscription();
    sus->call_init     = plugins_init;
    sus->call_main     = plugins_callback;
    sus->time_relax    = 249;
    sus->time_burst    = 249;
    sus->suscribe(sus);
    // Monitoring metrics
    debug("Initializing monitor");
    monitor_init();

    return 1;
}

#if MAIN
int main(int argc, char *argv[])
{
    return !plugin_manager_init(argc, argv);
}
#endif
