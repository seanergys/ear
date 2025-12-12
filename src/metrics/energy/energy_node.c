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

// #define SHOW_DEBUGS 1

#include <common/config.h>
#include <common/environment_common.h>
#include <common/hardware/hardware_info.h>
#include <common/includes.h>
#include <common/output/debug.h>
#include <common/system/symplug.h>
#include <common/types/configuration/cluster_conf.h>
#include <common/utils/string.h>
#include <metrics/energy/energy_node.h>
#include <pthread.h>
/* Static plugins headers */
#include <metrics/energy/node/energy_cpu_gpu.h>
#include <metrics/energy/node/energy_dcmi.h>
#include <metrics/energy/node/energy_dummy.h>

struct energy_op energy_ops;

const char *energy_names[] = {"energy_load",        "energy_unload",       "energy_datasize",    "energy_frequency",
                              "energy_dc_read",     "energy_dc_time_read", "energy_ac_read",     "energy_units",
                              "energy_accumulated", "energy_to_str",       "energy_power_limit", "energy_data_is_null"};

static int energy_loaded    = 0;
static int energy_nops      = 12;
static void *energy_context = NULL;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void split_plugin_args(const char *plugin_name, char *name_no_args)
{
    char *args;
    strncpy(name_no_args, plugin_name, MAX_PATH_SIZE);
    // In case energy_obj = plugin.so:arg1:arg2:arg3
    // - energy_obj will be rewritten as plugin.so
    // - and EAR_ENERGY_ARGS will be arg1:arg2:arg3
    // This is specially required in IPMI plugins. Take a look into these
    // plugins to see how this environment variable is used.
    if (strchr(name_no_args, '"' )) { remove_chars(name_no_args, '"'); }
    if (strchr(name_no_args, '\'')) { remove_chars(name_no_args, '\''); }
    args = strchr(name_no_args, ':');
    if (args != NULL) {
        *args = '\0';
        args++;
        setenv("EAR_ENERGY_ARGS", args, 1);
        debug("EAR_ENERGY_ARGS %s", args);
    }
}

static state_t load_dynamic_plugin(const char *dir_plug, const char *plugin_name)
{
    char energy_obj[MAX_PATH_SIZE];
    char name_no_args[MAX_PATH_SIZE];

    if (plugin_name == NULL || dir_plug == NULL) {
        return EAR_ERROR;
    }

    // Split plugin name and arguments
    split_plugin_args(plugin_name, name_no_args);

    snprintf(energy_obj, sizeof(energy_obj), "%s/energy/%s", dir_plug, name_no_args);
    debug("loading shared object '%s'", energy_obj);

    if (state_fail(symplug_open(energy_obj, (void **) &energy_ops, energy_names, energy_nops))) {
        return EAR_ERROR;
    }

    return energy_ops.load(&energy_context);
}

state_t energy_load(int options)
{
    cluster_conf_t cluster_conf;
    my_node_conf_t *node_conf = NULL;
    char ear_conf_path[MAX_PATH_SIZE];
    state_t ret     = EAR_ERROR;
    char *hack_path = NULL;
    char *dir_plug  = NULL;
    int conf_loaded = 0;
    char install_path_buf[MAX_PATH_SIZE];

    pthread_mutex_lock(&lock);
    debug("energy_load: entry");

    if (energy_loaded) {
        debug("energy_load: already loaded");
        pthread_mutex_unlock(&lock);
        return_msg(EAR_ERROR, Generr.api_initialized);
    }

    // HACK: Allow loading specific plugin via environment variable
    hack_path = getenv("HACK_ENERGY_PLUGIN");
    if (hack_path != NULL) {
        char hack_path_no_args[MAX_PATH_SIZE];
        char *args;

        debug("Loading HACK energy plugin: %s", hack_path);

        split_plugin_args(hack_path, hack_path_no_args);

        if (state_ok(symplug_open(hack_path_no_args, (void **) &energy_ops, energy_names, energy_nops))) {
            if (state_ok(energy_ops.load(&energy_context))) {
                ret = EAR_SUCCESS;
                goto end;
            }
        }
    }

    // If DUMMY is requested, we go straight for it.
    if (API_IS(options, API_DUMMY)) {
        debug("energy_load: DUMMY requested");
        goto dummy;
    }

    if (API_IS(options, API_EARD)) {
        verbose(VCONF, "EARD API not yet available");
    }

    memset(&cluster_conf, 0, sizeof(cluster_conf));

    if (get_ear_conf_path(ear_conf_path) != EAR_SUCCESS ||
        read_cluster_conf(ear_conf_path, &cluster_conf) != EAR_SUCCESS) {

        debug("energy_load: could not load EAR configuration");
        char *ear_install_path = get_ear_install_path();
        if (ear_install_path != NULL) {
            snprintf(install_path_buf, sizeof(install_path_buf), "%s/lib/plugins", ear_install_path);
            dir_plug = install_path_buf;
            debug("Could not load EAR configuration. Using fallback plugins directory: %s", dir_plug);
        } else {
            debug("Could not load EAR configuration to find plugins directory. Trying static plugins.");
            dir_plug = NULL;
        }
    } else {
        debug("energy_load: EAR configuration loaded");
        conf_loaded = 1;
        dir_plug    = cluster_conf.install.dir_plug;
    }

    // Only attempt to get node configuration if cluster config was loaded
    if (conf_loaded) {
        char nodename[MAX_PATH_SIZE];
        if (gethostname(nodename, sizeof(nodename)) < 0) {
            error("Error getting node name (%s)", strerror(errno));
            goto end;
        }

        node_conf = get_my_node_conf(&cluster_conf, nodename);

        // Try loading plugins in order of priority.
        if (node_conf != NULL) {
            debug("energy_load: node_conf found, trying %s", node_conf->energy_plugin);
            if (state_ok(load_dynamic_plugin(dir_plug, node_conf->energy_plugin))) {
                ret = EAR_SUCCESS;
                goto end;
            }
        }
        // By default cluster_conf.install.obj_ener gets DEFAULT_ENERGY_PLUGIN
        debug("energy_load: trying cluster conf plugin %s", cluster_conf.install.obj_ener);
        if (state_ok(load_dynamic_plugin(dir_plug, cluster_conf.install.obj_ener))) {
            ret = EAR_SUCCESS;
            goto end;
        }
    } else {
        // Fallback: try default plugin directly
        debug("energy_load: trying default plugin directly");
        if (state_ok(load_dynamic_plugin(dir_plug, DEFAULT_ENERGY_PLUGIN))) {
            ret = EAR_SUCCESS;
            goto end;
        }
    }

    debug("energy_load: trying static plugins");
    if (state_ok(energy_cpu_gpu_load_plugin(&energy_ops, &energy_context))) {
        if (state_ok(energy_ops.load(&energy_context))) {
            ret = EAR_SUCCESS;
            debug("Loaded static plugin: energy_cpu_gpu");
            goto end;
        }
    }

    if (state_ok(energy_dcmi_load_plugin(&energy_ops, &energy_context))) {
        if (state_ok(energy_ops.load(&energy_context))) {
            ret = EAR_SUCCESS;
            debug("Loaded static plugin: energy_dcmi");
            goto end;
        }
    }

dummy:
    debug("energy_load: trying dummy plugin");
    if (state_ok(energy_dummy_load_plugin(&energy_ops, &energy_context))) {
        if (state_ok(energy_ops.load(&energy_context))) {
            ret = EAR_SUCCESS;
            debug("Loaded static plugin: energy_dummy");
            goto end;
        }
    }

end:
    if (state_ok(ret)) {
        energy_loaded = 1;
    } else {
        error("No energy plugin found");
    }

    if (node_conf != NULL) {
        free(node_conf);
    }
    if (conf_loaded) {
        free_cluster_conf(&cluster_conf);
    }
    pthread_mutex_unlock(&lock);
    return ret;
}

state_t energy_is_loaded()
{
    return energy_loaded ? EAR_SUCCESS : EAR_ERROR;
}

state_t energy_unload()
{
    state_t s = EAR_SUCCESS;

    if (energy_ops.unload != NULL) {
        s = energy_ops.unload(&energy_context);
    }
    energy_loaded = 0;

    return s;
}

// state_t energy_handler_clean(ehandler_t *eh)
// {
//     // memset(eh, 0, sizeof(eh->context));
//     return EAR_SUCCESS;
// }

state_t energy_datasize(size_t *size)
{
    preturn(energy_ops.datasize, size);
}

state_t energy_frequency(ulong *fus)
{
    int intents = 0;
    timestamp ts1;
    timestamp ts2;
    ulong e1;
    ulong e2;
    void *c;

    c = energy_context;

    // Dedicated frequency
    if (energy_ops.frequency != NULL) {
        return energy_ops.frequency(fus);
    }

    if (energy_ops.dc_read == NULL) {
        return EAR_UNDEFINED;
    }
    // Generic frequency
    if (state_ok(energy_ops.dc_read(energy_context, &e1))) {
        do {
            energy_ops.dc_read(c, &e2);
            intents++;
        } while ((e1 == e2) && (intents < 5000));

        //
        if (intents == 5000) {
            return 0;
        }

        timestamp_getprecise(&ts1);
        e1 = e2;

        do {
            energy_ops.dc_read(c, &e2);
        } while (e1 == e2);

        timestamp_getprecise(&ts2);
        *fus = (timestamp_diff(&ts2, &ts1, TIME_USECS) / 2) / MAX_POWER_ERROR;
    }

    return EAR_SUCCESS;
}

state_t energy_dc_read(edata_t emj)
{
    preturn(energy_ops.dc_read, energy_context, emj);
}

state_t energy_dc_time_read(edata_t emj, ulong *tms)
{
    preturn(energy_ops.dc_time_read, energy_context, emj, tms);
}

state_t energy_ac_read(edata_t emj)
{
    preturn(energy_ops.ac_read, energy_context, emj);
}

/* Energy units are 1=Joules, 1000=mJ, 1000000=uJ, 1000000000nJ */
state_t energy_units(uint *units)
{
    *units = 1;
    preturn(energy_ops.units, units);
}

state_t energy_accumulated(unsigned long *e, edata_t init, edata_t end)
{
    *e = 0;
    preturn(energy_ops.accumulated, e, init, end);
}

state_t energy_to_str(char *str, edata_t e)
{
    preturn(energy_ops.energy_to_str, str, e);
}

state_t energy_set_power_limit(ulong limit, ulong target)
{
    preturn(energy_ops.power_limit, energy_context, limit, target);
}

uint energy_data_is_null(edata_t e)
{
    if (energy_ops.is_null != NULL) {
        preturn(energy_ops.is_null, e);
    } else {
        /* Should we return 1 or 0 by default */
        return 1;
    }
}