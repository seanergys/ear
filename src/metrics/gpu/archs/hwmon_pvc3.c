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

// #define SHOW_DEBUGS 1

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common/config.h>
#include <common/output/debug.h>
#include <common/sizes.h>
#include <common/system/symplug.h>
#include <metrics/common/apis.h>
#include <metrics/common/hwmon.h>
#include <metrics/gpu/archs/hwmon_pvc.h>

// =============================================================================
// Static variables
// =============================================================================

static hwmon_t *chips;
static uint     chips_count;
static uint     gpus_count;
static uint     items_count;
static char    *driver_name = "i915";
static char    *device_type = "energy";
static timestamp_t energy_time;
static uint total_samples = 1;

// =============================================================================
// Constants
// =============================================================================

#define DUMMY_MEM_FREQ 500000
#define DUMMY_FREQ     1600000

// =============================================================================
// API Functions
// =============================================================================

state_t gpu_pvc_hwmon_load(gpu_ops_t *ops, int force_api)
{
    debug("asking for status");
    debug("Received API %d", force_api);
    
    if (!USE_PVC_HWMON) {
        return;
    }
    
    // Try to open hwmon for i915 driver
    if (state_fail(hwmon_open(driver_name, device_type, &chips, &chips_count))) {
        debug("Failed to open hwmon for %s: %s", driver_name, state_msg);
        return;
    }
    
    debug("HWMON opened for %s", driver_name);
    
    // Count energy input items
    items_count = hwmon_count_items(chips, "input", NULL);
    if (items_count == 0) {
        debug("No energy input items found");
        hwmon_close(&chips);
        return;
    }
    
    gpus_count = items_count;
    debug("Detected %u HWMON energy items", items_count);
    
    // Set up API functions
    apis_set(ops->get_info, pvc_hwmon_get_info);
    apis_set(ops->init, pvc_hwmon_init);
    apis_set(ops->dispose, pvc_hwmon_dispose);
    apis_set(ops->get_devices, pvc_hwmon_get_devices);
    apis_set(ops->read, pvc_hwmon_read);
    apis_set(ops->read_raw, pvc_hwmon_read_raw);
    apis_set(ops->set_monitoring_mode, pvc_hwmon_set_monitoring_mode);
    
    return EAR_SUCCESS;
}

// =============================================================================
// Info and Device Management
// =============================================================================

void pvc_hwmon_get_info(apinfo_t *info)
{
    debug("pvc_hwmon_get_info");
    info->api        = API_PVC_HWMON;
    info->devs_count = gpus_count;
}

state_t pvc_hwmon_init(ctx_t *c)
{
    return EAR_SUCCESS;
}

void pvc_hwmon_set_monitoring_mode(int mode_in)
{
    // No specific monitoring mode needed for PVC
}

void pvc_hwmon_get_devices(gpu_devs_t **devs_in, uint *devs_count_in)
{
    int dv;

    debug("pvc_hwmon_get_devices");
    if (chips == NULL) {
        return;
    }

    if (devs_in != NULL) {
        debug("Allocating data for %u GPU dev", gpus_count);
        *devs_in = calloc(gpus_count, sizeof(gpu_devs_t));
        
        for (dv = 0; dv < gpus_count; ++dv) {
            (*devs_in)[dv].serial = 0;
            (*devs_in)[dv].index  = dv;
            debug("DEV[%d] serial %d index %d", dv, (*devs_in)[dv].serial, (*devs_in)[dv].index);
        }
    }
    
    if (devs_count_in != NULL) {
        *devs_count_in = gpus_count;
    }
}

// =============================================================================
// Resource Management
// =============================================================================

state_t pvc_hwmon_dispose(ctx_t *c)
{
    if (chips != NULL) {
        hwmon_close(&chips);
        chips_count = 0;
        items_count = 0;
        gpus_count = 0;
        chips = NULL;
    }
    return EAR_SUCCESS;
}

state_t pvc_hwmon_count_devices(ctx_t *c, uint *count)
{
    if (chips == NULL) {
        return EAR_ERROR;
    }
    
    if (count != NULL) {
        *count = gpus_count;
    }

    return EAR_SUCCESS;
}

// =============================================================================
// Data Reading Functions
// =============================================================================

state_t pvc_hwmon_read(ctx_t *c, gpu_t *data)
{
    hwmon_t *chip;
    hwmon_dev_t *dev;
    int gpu_index = 0;

    debug("pvc_hwmon_read");
    
    if (chips == NULL) {
        return EAR_ERROR;
    }
    if (data == NULL) {
        return EAR_ERROR;
    }
    
    memset(data, 0, sizeof(gpu_t) * gpus_count);
    timestamp_getfast(&energy_time);
    
    // Read all chips and devices
    hwmon_read(chips);
    
    // Iterate through all chips and devices to collect energy data
    while ((chip = hwmon_iter_chips(chips)) != NULL) {
        while ((dev = hwmon_iter_devs(chip, NULL)) != NULL) {
            if (gpu_index >= gpus_count) {
                debug("Warning: more devices than expected");
                break;
            }
            
            // Convert energy from microjoules to joules
            data[gpu_index].energy_j = ((double)dev->input_toint) / 1000000.0;
            data[gpu_index].working  = 1;
            data[gpu_index].correct  = 1;
            data[gpu_index].util_gpu = 100 * total_samples;
            data[gpu_index].util_mem = 100 * total_samples;
            data[gpu_index].freq_gpu = DUMMY_FREQ * total_samples;
            data[gpu_index].freq_mem = DUMMY_MEM_FREQ * total_samples;
            data[gpu_index].time     = energy_time;
            data[gpu_index].samples  = total_samples;

            // Debug output
            debug("dev | metric   | value    ");
            debug("--- | -------- | -----    ");
            debug("%d   | samps    | %lu     ", gpu_index, data[gpu_index].samples);
            debug("%d   | freq_gpu | %lu MHz ", gpu_index, data[gpu_index].freq_gpu);
            debug("%d   | freq_mem | %lu MHz ", gpu_index, data[gpu_index].freq_mem);
            debug("%d   | util_gpu | %lu %%  ", gpu_index, data[gpu_index].util_gpu);
            debug("%d   | util_mem | %lu %%  ", gpu_index, data[gpu_index].util_mem);
            debug("%d   | temp_gpu | %lu cels", gpu_index, data[gpu_index].temp_gpu);
            debug("%d   | temp_mem | %lu cels", gpu_index, data[gpu_index].temp_mem);
            debug("%d   | energy   | %0.2lf J", gpu_index, data[gpu_index].energy_j);
            debug("%d   | working  | %d      ", gpu_index, data[gpu_index].working);
            
            gpu_index++;
        }
    }
    
    total_samples++;
    return EAR_SUCCESS;
}

state_t pvc_hwmon_read_raw(ctx_t *c, gpu_t *data)
{
    debug("pvc_hwmon_read_raw");
    return pvc_hwmon_read(c, data);
}

state_t pvc_hwmon_set_mode(ctx_t *c, uint32_t mode)
{
    debug("pvc_hwmon_set_mode");
    return EAR_SUCCESS;
}