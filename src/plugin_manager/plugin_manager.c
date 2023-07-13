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
#include <common/config/config_env.h>
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
    void    (*get_tag)    (cchar **tag, cchar **tags_deps);
    state_t (*init)       (void **data);
    char   *(*callback)   (cchar *tag, void *data);
    char   *(*post)       (cchar *msg, void *data);
} plugin_ops_t;

typedef struct plugin_s {
    char          path[4096];
    char         *name;
    cchar        *tag;
    cchar        *tags_deps;
    plugin_ops_t  ops;
    ullong        time_lapse; //In ms
    timestamp_t   time_origin;
    uint          one_time;
    void         *data;
    uint          data_count;
    uint          is_open;
    uint          is_init;
} plugin_t;

typedef struct plugins_s {
    plugin_t *plugins;
    uint      count;
} plugins_t;

static plugins_t   p;
static char      **priority_paths;
static char        date[128];

static state_t plugins_open_so(int i)
{
    char new_path[8192];
    char *e = NULL;
    int j = 0;

    // Priority paths
    while(priority_paths != NULL && priority_paths[j] != NULL) {
        sprintf(new_path, "%s/%s", priority_paths[j], p.plugins[i].path);
        if (state_ok(plug_open(new_path, (void **) &p.plugins[i].ops, names, N, RTLD_NOW | RTLD_GLOBAL))) {
            verbose(0, "[Configuration] Trying to open: %s... OK", new_path);
            return EAR_SUCCESS;
        }
        verbose(0, "[Configuration] Trying to open: %s... FAILED", new_path);
        ++j;
    }
    // Written paths
    if (state_ok(plug_open(p.plugins[i].path, (void **) &p.plugins[i].ops, names, N, RTLD_NOW | RTLD_GLOBAL))) {
        verbose(0, "[Configuration] Trying to open: %s... OK", p.plugins[i].path);
        return EAR_SUCCESS;
    }
    verbose(0, "[Configuration] Trying to open: %s... FAILED", p.plugins[i].path);
    // Environment paths
    if((e = ear_getenv(ENV_PATH_EAR)) != NULL) {
        sprintf(new_path, "%s/%s", e, p.plugins[i].path);
        if (state_ok(plug_open(new_path, (void **) &p.plugins[i].ops, names, N, RTLD_NOW | RTLD_GLOBAL))) {
            verbose(0, "[Configuration] Trying to open: %s... OK", new_path);
            return EAR_SUCCESS;
        }
        verbose(0, "[Configuration] Trying to open: %s... FAILED", new_path);
    }
    return EAR_ERROR;
}

static state_t plugins_init(void *whatever)
{
    int init_count = 0;
    int i;

    for (i = 0; i < p.count; ++i) {
        if (state_fail(plugins_open_so(i))) {
            continue;
        }
        p.plugins[i].is_open = 1;
    }
    for (i = 0; i < p.count; ++i) {
        if(!p.plugins[i].is_open) {
            continue;
        }
        if (p.plugins[i].ops.get_tag != NULL) {
            p.plugins[i].ops.get_tag(&p.plugins[i].tag, &p.plugins[i].tags_deps);
            verbose(0, "[DEBUG] %s after get_tag(): tag '%s', tags_deps '%s'",
                    p.plugins[i].path, (char *) p.plugins[i].tag, (char *) p.plugins[i].tags_deps);
        }
        if (p.plugins[i].ops.init != NULL) {
            debug("PLUG%d %s: init", i, p.plugins[i].path);
            if (state_fail(p.plugins[i].ops.init(&p.plugins[i].data))) {
                fprintf(stderr, "%s init failed: %s\n", p.plugins[i].path, state_msg);
                continue;
            }
            p.plugins[i].is_init = 1;
            ++init_count;
        }
    }
    if (init_count == 0) {
        verbose(0, "[ERROR] No plugins to load.")
        monitor_dispose();
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

    // Base case
    if (i == p.count) {
        return;
    }
    // Not initialized case
    if (!p.plugins[i].is_init) {
        goto next;
    }
    // Time calculation
    timestamp_get(&time_now);
    time_passed = timestamp_diff(&time_now, &p.plugins[i].time_origin, TIME_MSECS);
    time_isup   = time_passed >= p.plugins[i].time_lapse;

    if (time_isup) {
        p.plugins[i].time_origin = time_now;
    }
    // One timers
    if (p.plugins[i].one_time) {
        p.plugins[i].is_init = 0;
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
next:
    time_and_callback(i+1);
}

static state_t plugins_callback(void *whatever)
{
    format_output();
    time_and_callback(0);
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
        verbose(0, "    --plugins    List of comma separated plugins to load.");
        verbose(0, "    --paths      List of comma separated priority paths to search plugins.");
        verbose(0, "    --help       If you see it you already typed --help.");
        return 0;
    }

    // Processing EARD connection
    //if (args_get(argc, argv, "eard", buffer)) {
    //eards_connection();
    //}

    // Processing list of plugins
    if (!args_get(argc, argv, "plugins", buffer)) {
        return 0;
    }
    // Get list of plugins to load and times
    debug("plugin string: %s", buffer);
    if (strtoa(buffer, ',', &list, &list_count) == NULL) {
        return 0;
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
    strtoa_free(list);
    // Processing list of priority paths
    if (!args_get(argc, argv, "paths", buffer)) {
        return 1;
    }
    // NUll terminated list, count is not required
    strtoa(buffer, ',', &priority_paths, NULL);
    return 1;
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
        verbose(0, "[ERROR] No plugins to load.")
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

void plugin_manager_wait()
{
    monitor_wait();
}

#if MAIN
int main(int argc, char *argv[])
{
    return !plugin_manager_init(argc, argv);
}
#endif
