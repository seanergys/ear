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
    "up_action_init",
    "up_action_periodic",
    "up_post_data",
};

#define N 4

typedef struct plugin_manager_ops_s {
    void    (*get_tag)         (cchar **tag, cchar **tags_deps);
    char   *(*action_init)     (cchar *tag, void **data_alloc, void *data);
    char   *(*action_periodic) (cchar *tag, void *data);
    char   *(*post_data)       (cchar *msg, void *data);
} plugin_ops_t;

typedef struct plugin_s {
    char          path[4096];
    char         *file_name;
    char          tag_supposed[256];
    cchar        *tag;
    cchar        *tags_deps;
    plugin_ops_t  ops;
    ullong        time_lapse; //In ms
    timestamp_t   time_origin;
    uint          one_time;
    uint          priority;
    void         *data;
    uint          data_count;
    uint          is_enabled;
} plugin_t;

typedef struct plugins_s {
    plugin_t **plugins_sorted;
    plugin_t  *plugins;
    uint       max_priority;
    uint       count;
} plugins_t;

static plugins_t   p;
static char      **priority_paths;
static char        date[128];

static int plugins_dependencies_read(cchar *string, ullong time_lapse, uint priority)
{
    uint list_count;
    char **list;
    int l, j;
    char *c;

    // Get list of plugins to load and times
    debug("plugin string: %s", string);
    if (strtoa(string, ',', &list, &list_count) == NULL) {
        return 0;
    }
    // Allocating p
    debug("detected plugins: #%d", list_count);
    // Filling p (i string index, j plugins index)
    for (l = 0; l < list_count; ++l) {
        if ((c = strchr(list[l], ':')) != NULL) {
            c[0] = '\0';
            p.plugins[p.count].time_lapse = (ullong) atoi(&c[1]);
        }
        if (time_lapse != 0) {
            p.plugins[p.count].time_lapse = time_lapse;
        }
        if (p.plugins[p.count].time_lapse == 0LLU) {
            //p.plugins[p.count].time_lapse = 0LLU;
            p.plugins[p.count].one_time   = 1;
        }
        // Checking if the file have the .so extension
        if (strcmp(&list[l][strlen(list[l])-3], ".so")) {
            sprintf(p.plugins[p.count].path, "libup%s.so", list[l]);
        } else {
            sprintf(p.plugins[p.count].path, "%s", list[l]);
        }
        // Adding one priority
        p.plugins[p.count].priority = priority+list_count-l;
        if (p.plugins[p.count].priority > p.max_priority) {
            p.max_priority = p.plugins[p.count].priority;
        }
        // Getting the plugin file_name for printing purposes
        for (j = strlen(p.plugins[p.count].path)-2; j >= 0; --j) {
            p.plugins[p.count].file_name = &p.plugins[p.count].path[j];
            if (p.plugins[p.count].path[j] == '/') {
                p.plugins[p.count].file_name = &p.plugins[p.count].path[j+1];
                break;
            }
        }
        // Looking this new plugin in the already processed plugins
        for (j = 0; j < p.count; ++j) {
            if (strcmp(p.plugins[p.count].file_name, p.plugins[j].file_name) == 0) {
                if (p.plugins[p.count].priority > p.plugins[j].priority) {
                    p.plugins[j].priority = p.plugins[p.count].priority;
                }
                break;
            }
        }
        // Not found, so this is a new plugin
        p.count += (j == p.count);
    }
    strtoa_free(list);
    return 1;
}

static void plugins_open_sort(uint priority, uint index)
{
    int i;
    // Base case
    if (priority == 0) {
        return;
    }
    for (i = 0; i < p.count; ++i) {
        if (priority == p.plugins[i].priority) {
            verbose(0, "[DEBUG] LOADED %15s: time lapse '%4llu', priority '%u'",
                    p.plugins[i].path, p.plugins[i].time_lapse, p.plugins[i].priority);
            p.plugins_sorted[index++] = &p.plugins[i];
        }
    }
    plugins_open_sort(priority-1, index);
}

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
        verbose(0, "[Configuration] Trying to open: %s... FAILED (%s)", new_path, state_msg);
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

static int plugins_open()
{
    int open_count = 0;
    int i;

    for (i = 0; i < p.count; ++i) {
        if (state_fail(plugins_open_so(i))) {
            continue;
        }
        if (p.plugins[i].ops.get_tag != NULL) {
            p.plugins[i].ops.get_tag(&p.plugins[i].tag, &p.plugins[i].tags_deps);
            verbose(0, "[DEBUG] %s after get_tag(): tag '%s', tags_deps '%s'",
                    p.plugins[i].path, (char *) p.plugins[i].tag, (char *) p.plugins[i].tags_deps);
        }
        // Loading dependencies
        if (p.plugins[i].tags_deps != NULL) {
            plugins_dependencies_read(p.plugins[i].tags_deps, p.plugins[i].time_lapse, p.plugins[i].priority);
        }
        p.plugins[i].is_enabled = 1;
        ++open_count;
    }
    return open_count;
}

static void format_output()
{
    time_t tm = time(NULL);
    sprintf(date, "%s", ctime(&tm));
    date[strlen(date)-1] = '\0';
}

static void print_output(char *file_from, char *file_to, cchar *tag, char *output)
{
    if (output != NULL) {
        printf("[%s, %s -> %s, %s] %s\n", date, file_from, file_to, tag, output);
    }
}

static void plugins_action_periodic(int i)
{
    timestamp_t time_now;
    ullong time_passed;
    uint time_isup;
    char *sout;
    int j;

    // Base case
    if (i == p.count) {
        return;
    }
    // Not initialized case
    if (!p.plugins_sorted[i]->is_enabled) {
        goto next;
    }
    // Time calculation
    timestamp_get(&time_now);
    time_passed = timestamp_diff(&time_now, &p.plugins_sorted[i]->time_origin, TIME_MSECS);
    time_isup   = time_passed >= p.plugins_sorted[i]->time_lapse;

    if (time_isup) {
        p.plugins_sorted[i]->time_origin = time_now;
    }
    // One timers
    if (p.plugins_sorted[i]->one_time) {
        p.plugins_sorted[i]->is_enabled = 0;
        time_isup = 1;
    }
    debug("PLUG%d time's up: %d, called every %llu ms, data %p", i,
        time_isup, p.plugins_sorted[i]->time_lapse, p.plugins_sorted[i]->data);
    // If the plugin has data allocated to work with
    if (time_isup && p.plugins_sorted[i]->data != NULL) {
        if (p.plugins_sorted[i]->ops.action_periodic != NULL) {
            sout = p.plugins_sorted[i]->ops.action_periodic(p.plugins_sorted[i]->tag, p.plugins_sorted[i]->data);
            print_output(p.plugins_sorted[i]->file_name, p.plugins_sorted[i]->file_name, p.plugins_sorted[i]->tag, sout);
        }
        // Feeding others with own data
        for (j = 0; j < p.count; ++j) {
            if (j == i) {
                continue;
            }
            // If pluginI has data valid for pluginJ
            if (p.plugins_sorted[j]->ops.action_periodic != NULL) {
                sout = p.plugins_sorted[j]->ops.action_periodic(p.plugins_sorted[i]->tag, p.plugins_sorted[i]->data);
                print_output(p.plugins_sorted[i]->file_name, p.plugins_sorted[i]->file_name, p.plugins_sorted[i]->tag, sout);
            }
        }
    }
next:
    // I think this is OK because the priority system moves the dependencies calls
    // before the own call. I can interpret that a data from a dependency coming
    // before means that data is required to perform the dependent main function,
    // which makes sense.
    plugins_action_periodic(i+1);
}

static void plugins_action_init(int i)
{
    char *sout;
    int j;
    // Base case
    if (i == p.count) {
        return;
    }
    if (i == 0) {
        format_output();
    }
    //#define PRINT(f) print_output(p.plugins_sorted[i]->file_name, p.plugins_sorted[i]->tag, f)
    p.plugins_sorted[i]->data = NULL;
    if (p.plugins_sorted[i]->ops.action_init != NULL) {
        sout = p.plugins_sorted[i]->ops.action_init(p.plugins_sorted[i]->tag, &p.plugins_sorted[i]->data, NULL);
        print_output(p.plugins_sorted[i]->file_name, p.plugins_sorted[i]->file_name, p.plugins_sorted[i]->tag, sout);
    }
    // Feeding others with own data
    for (j = 0; p.plugins_sorted[i]->data != NULL && j < p.count; ++j) {
        if (j == i) {
            continue;
        }
        if (p.plugins_sorted[j]->ops.action_init != NULL) {
            sout = p.plugins_sorted[j]->ops.action_init(p.plugins_sorted[i]->tag, NULL, p.plugins_sorted[i]->data);
            print_output(p.plugins_sorted[i]->file_name, p.plugins_sorted[j]->file_name, p.plugins_sorted[i]->tag, sout);
        }
    }
    plugins_action_init(i+1);
}

static state_t plugins_init(void *whatever)
{
    if (plugins_open()) {
        plugins_open_sort(p.max_priority, 0);
        plugins_action_init(0);
    } else {
        monitor_dispose();
    }
    return EAR_SUCCESS;
}

static state_t plugins_callback(void *whatever)
{
    format_output();
    plugins_action_periodic(0);
}

static int plugin_manager_configure(int argc, char *argv[])
{
    char buffer[8192];
    uint i, j;

    if (argc == 1 || args_get(argc, argv, "help", buffer)) {
        verbose(0, "Usage: %s [OPTIONS]\n", argv[0]);
        verbose(0, "Options:");
        verbose(0, "    --plugins    List of comma separated plugins to load.");
        verbose(0, "    --paths      List of comma separated priority paths to search plugins.");
        verbose(0, "    --eard       Connect to EAR Daemon.");
        verbose(0, "    --help       If you see it you already typed --help.");
        return 0;
    }

    // Processing EARD connection
    if (args_get(argc, argv, "eard", buffer)) {
        //eards_connection();
    }
    // Processing list of priority paths
    if (args_get(argc, argv, "paths", buffer)) {
        // NUll terminated list, count is not required
        strtoa(buffer, ',', &priority_paths, NULL);
    }
    // Processing list of plugins (by now is hardcoded)
    if (!args_get(argc, argv, "plugins", buffer)) {
        return 0;
    }
    p.plugins = calloc(128, sizeof(plugin_t));
    p.plugins_sorted = calloc(128, sizeof(plugin_t *));
    return plugins_dependencies_read(buffer, 0LLU, 0);
}

void plugin_mananger_post(cchar *msg, void *data)
{
    int i;
    // Wait until action_init has been called at least.
    for (i = 0; i < p.count; ++i) {
        if (p.plugins[i].ops.post_data == NULL) {
            continue;
        }
        p.plugins[i].ops.post_data(msg, data);
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
