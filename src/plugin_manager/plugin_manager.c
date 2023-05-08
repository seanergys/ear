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
#include <common/system/symplug.h>
//#include <daemon/local_api/eard_api.h>
//#include <metrics/metrics.h>
#include <plugin_manager/plugin_manager.h>

static const char *names[] = {
    "up_get_tag",
    "up_init",
    "up_execute",
    "up_data_alloc",
    "up_data_tostr",
};
#define N 5

typedef struct plugin_manager_ops_s {
    void    (*get_info)   (cchar **tag);
    state_t (*init)       ();
    state_t (*execute)    (cchar *tag, void *data);
    void    (*data_alloc) (void **data);
    char   *(*data_tostr) (void *data);
} plugin_ops_t;

typedef struct plugin_s {
    char         path[PATH_MAX];
    cchar       *tag;
    plugin_ops_t ops;
    ullong       time_lapse; //In ms
    timestamp_t  time_origin;
    void        *data;
    uint         data_count;
    uint         init;
} plugin_t;

typedef struct plugins_s {
    plugin_t *plugins;
    uint      count;
} plugins_t;

static plugins_t  p;
static void      *data;

static state_t plugins_init(void *whatever)
{
    int i, j, k;
    for (i = 0; i < p.count; ++i) {
        debug("PLUG%d init: %s, called every %llu seconds", i,
            p.plugins[i].path, p.plugins[i].time_lapse);
        if (state_fail(plug_open(p.plugins[i].path, (void **) &p.plugins[i].ops, names, N, RTLD_NOW | RTLD_GLOBAL))) {
            debug("loading failed: %s", state_msg)
        }
        if (p.plugins[i].ops.init != NULL) {
            if (state_fail(p.plugins[i].ops.init())) {
                continue;
            }
            p.plugins[i].init = 1;
        }
        if (p.plugins[i].ops.get_info != NULL) {
            p.plugins[i].ops.get_info(&p.plugins[i].tag);
            debug("PLUG%d init: %s, tag %s", i,
                p.plugins[i].path, (char *) p.plugins[i].tag);
        }
        if (p.plugins[i].ops.data_alloc != NULL) {
            p.plugins[i].ops.data_alloc(&p.plugins[i].data);
        }
    }
    return EAR_SUCCESS;
}

static void time_and_execute(int i)
{
    timestamp_t time_now;
    ullong time_passed;
    uint time_isup;
    int j;

    // Caso base
    if (i == p.count) {
        return;
    }
    // Time calculation
    timestamp_get(&time_now);
    time_passed = timestamp_diff(&time_now, &p.plugins[i].time_origin, TIME_SECS);
    time_isup   = time_passed >= p.plugins[i].time_lapse;

    if (time_isup) {
        debug("PLUG%d update: %s, called every %llu seconds", i,
                p.plugins[i].path, p.plugins[i].time_lapse);
        p.plugins[i].time_origin = time_now;
    }
    if (time_isup && p.plugins[i].ops.execute != NULL) {
        // If the plugin has data allocated to work with
        if (p.plugins[i].data != NULL) {
            p.plugins[i].ops.execute(p.plugins[i].tag, p.plugins[i].data);
            // Feeding others with own data
            for (j = 0; j < p.count; ++j) {
                if (j == i) {
                    continue;
                }
                // If pluginI has data valid for pluginJ
                if (p.plugins[j].ops.execute != NULL && strstr(p.plugins[j].tag, p.plugins[i].tag)) {
                    p.plugins[j].ops.execute(p.plugins[i].tag, p.plugins[i].data);
                }
            }
        }
    }
    time_and_execute(i+1);
    if (time_isup && p.plugins[i].ops.data_tostr != NULL) {
        printf("\e[1;1H\e[2J");
        printf("%s\n", p.plugins[i].ops.data_tostr(p.plugins[i].data));
    }
}

static state_t plugins_update(void *whatever)
{
    time_and_execute(0);
}

static int plugin_manager_configure(int argc, char *argv[])
{
    char buffer[8192];
    char *buffer_p = buffer;
    uint list_count;
    char **list;
    uint i, j;
    char *c;

    if (argc == 1 || args_get(argc, argv, "help", buffer)) {
        verbose(0, "Usage: %s [OPTIONS]\n", argv[0]);
        verbose(0, "Options:");
  //      verbose(0, "    --eard       Connect to EAR Daemon.");
        verbose(0, "    --plugins    List of separated plugins to load.");
        verbose(0, "    --help       If you see it you already typed --help.");
        return 0;
    }
    //if (args_get(argc, argv, "eard", buffer)) {
        //eards_connection();
    //}
    if (!args_get(argc, argv, "plugins", buffer)) {
        return 0;
    }
    // Get list of plugins to load and times
    debug("plugin string: %s", buffer);
    if (strchr(buffer, ',')) {
        strtoa(buffer, ',', &list, &list_count);
    } else {
        list = &buffer_p;
        list_count = 1;
    }
    // Allocating p
    p.count = list_count;
    p.plugins = calloc(p.count, sizeof(plugin_t));
    // Filling p
    for (i = 0; i < list_count; ++i) {
        if ((c = strchr(list[i], ':')) != NULL) {
            c[0] = '\0';
            p.plugins[i].time_lapse = (ullong) atoi(&c[1]);
        } else {
            p.plugins[i].time_lapse = 1LLU;
        }
        sprintf(p.plugins[i].path, "%s", list[i]);
    }
    //free(*list);
    #if SHOW_DEBUGS
    #endif
    return 1;
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
    sus->call_main     = plugins_update;
    sus->time_relax    = 499;
    sus->time_burst    = 499;
    sus->suscribe(sus);
    // Monitoring metrics
    monitor_init();
    // Main thread to sleep
    sleep(100);

    return 1;
}

int main(int argc, char *argv[])
{
    return !plugin_manager_init(argc, argv);
}