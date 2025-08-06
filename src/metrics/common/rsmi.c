/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

// #define SHOW_DEBUGS 1

#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <common/states.h>
#include <common/output/debug.h>
#include <common/system/symplug.h>
#include <metrics/common/rsmi.h>

#ifndef RSMI_BASE
#define RSMI_PATH ""
#else
#define RSMI_PATH RSMI_BASE
#endif
#define RSMI_LIB "librocm_smi64.so"
#define RSMI_N   22

#define ccv(f)                                                                                                         \
    if (f != RSMI_STATUS_SUCCESS) {                                                                                    \
        debug(#f ": error");                                                                                           \
    }

// https://docs.amd.com/bundle/rocm_smi_lib.5.0/page/globals_func.html
static const char *rsmi_names[] = {
    "rsmi_init",
    "rsmi_shut_down",
    "rsmi_num_monitor_devices",
    "rsmi_dev_serial_number_get",
    "rsmi_dev_energy_count_get",
    "rsmi_dev_power_ave_get",
    "rsmi_dev_power_cap_get",
    "rsmi_dev_power_cap_default_get",
    "rsmi_dev_clk_range_set",
    "rsmi_dev_power_cap_set",
    "rsmi_dev_temp_metric_get",
    "rsmi_dev_gpu_clk_freq_get",
    "rsmi_dev_od_clk_info_set",
    "rsmi_dev_clk_range_set",
    "rsmi_utilization_count_get",
    "rsmi_dev_busy_percent_get",
    "rsmi_dev_memory_busy_percent_get",
    "rsmi_dev_memory_total_get",
    "rsmi_dev_memory_usage_get",
    "rsmi_dev_pci_bandwidth_get",
    "rsmi_compute_process_info_get",
    "rsmi_compute_process_gpus_get",
};

// Nomenclatures:
// SYS  -> system (GPU, i think stream processors)
// DF   -> data fabric
// DCEF -> display controller engine
// SOC  -> system on a chip
// MEM  -> memory
// sclk -> system clock
// mclk -> memory clock
// fclk -> ¿df clock? It seems

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static rsmi_t          rsmi;
static void           *handler;
static int             counter;

static state_t library_load()
{
    #define _open_test(path)                                                                       \
    debug("Openning %s", path);                                                                    \
    if ((handler = plug_open2(path, (void **) &rsmi, rsmi_names, RSMI_N, RTLD_NOW | RTLD_LOCAL)) != NULL) { \
        return EAR_SUCCESS;                                                                        \
    } else {                                                                                       \
        debug("Failed: %s", state_msg);                                                            \
    }

    #define open_test(path)          \
    _open_test(path RSMI_LIB);       \
    _open_test(path RSMI_LIB ".1");

    _open_test(getenv("HACK_RSMI_FILE"));
    open_test(RSMI_PATH "/targets/x86_64-linux/lib/");
    open_test(RSMI_PATH "/lib64/");
    open_test(RSMI_PATH "/lib/");
    open_test("/usr/lib/targets/x86_64-linux/lib/");
    open_test("/usr/lib64/");
    open_test("/usr/lib/");

    return_msg(EAR_ERROR, "library not found");
}

static state_t library_init()
{
    uint devs;
    if (rsmi.init(0) != RSMI_STATUS_SUCCESS) {
        debug("rsmi.init(0) failed");
        return EAR_ERROR;
    }
    if (rsmi.devs_count(&devs) != RSMI_STATUS_SUCCESS) {
        debug("rsmi.devs_count(&devs) failed");
        return EAR_ERROR;
    }
    if (devs == 0) {
        return_msg(EAR_ERROR, "no devices detected");
    }
    return EAR_SUCCESS;
}

static void library_destroy()
{
    if (counter) {
        rsmi.shut_down();
    }
    memset(&rsmi, 0, sizeof(rsmi_t));
    dlclose(handler);
    handler = NULL;
}

state_t rsmi_open(rsmi_t *rsmi_in)
{
    state_t s = EAR_SUCCESS;
    #ifndef RSMI_BASE
    debug("No RSMI_BASE path provided");
    return EAR_ERROR;
    #endif
    while (pthread_mutex_trylock(&lock));
    if (counter) {
        ++counter;
        goto fini;
    }
    if (state_ok(s = library_load())) {
        if (state_fail(s = library_init())) {
            debug("Initialized RSMI");
            counter = 1;
        }
    }
    if (state_fail(s)) {
        library_destroy();
    }
fini:
    pthread_mutex_unlock(&lock);
    if (counter && rsmi_in != NULL) {
        memcpy(rsmi_in, &rsmi, sizeof(rsmi_t));
    }
    return s;
}

state_t rsmi_close()
{
    if (counter > 0) {
        if (counter == 1) {
            library_destroy();
        }
        --counter;
    }
    return EAR_SUCCESS;
}

int rsmi_is_privileged()
{
    // Provisional
    return (getuid() == 0);
}

#if 0
int main(int argc, char *argv[])
{
    rsmi_open(NULL);
    return 0;
}
#endif
