/*********************************************************************
 * Copyright (c) 2024 Energy Aware Solutions, S.L
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **********************************************************************/

//#define SHOW_DEBUGS 1

#include <pthread.h>
#include <common/output/debug.h>
#include <common/system/monitor.h>
#include <metrics/common/hwmon.h>
#include <metrics/energy_cpu/archs/hwmon.h>

#define MONITORING_TIME 2

static hwmon_t       *chips;
static uint           chips_count;
static uint           items_count;
static suscription_t *sus;
static ullong        *energy_uj; // Per chip
static ullong        *power_uw_lvv; // Last valid value, per chip
static char          *label;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

state_t energy_cpu_hwmon_load(topology_t *tp_in)
{
    //return EAR_ERROR;
    if (tp_in->vendor == VENDOR_ARM && tp_in->model == MODEL_NEOVERSE_V2) {
        // label = "CPU Power Socket"; // Energy is in micro Watts (this is the CPU)
        label = "Grace Power Socket"; // Energy is in micro Watts (this is the PKG)
    } else {
        // Energy is in micro Watts
    }
    if (state_fail(hwmon_open("power_meter", "power", label, &chips, &chips_count))) {
        return EAR_ERROR;
    }
    if ((items_count = hwmon_count_items(chips, "average", label)) == 0) {
        hwmon_close(&chips);
        return EAR_ERROR;
    }
    debug("chips_count  : %d", chips_count);
    debug("items_count  : %d", items_count);
    debug("sockets_count: %d", tp_in->socket_count);
    energy_uj    = calloc(chips_count, sizeof(ullong));
    power_uw_lvv = calloc(chips_count, sizeof(ullong));
    return EAR_SUCCESS;
}

static state_t static_read(ullong *values)
{
    ullong power_uw = 0LLU;
    hwmon_t *chip;
    int i = 0;

    hwmon_read(chips);
    hwmon_calc_average(chips, NULL);
    while ((chip = hwmon_iter_chips(chips)) != NULL) {
        // If returned 0
        if ((power_uw = (ullong) chip->devs_avg.average_toint) == 0LLU) {
            power_uw = power_uw_lvv[i];
        } else {
            power_uw_lvv[i] = power_uw;
        }
        // Converted to micro Joules. The values are set in position 1 because this API
        // order is 0 to DRAM and 1 to PKG.
        values[1] += power_uw * (ullong) MONITORING_TIME;
        debug("CHIP[%d]: %llu += %llu * %d", i, values[1], power_uw, MONITORING_TIME);
        i++;
    }
    return EAR_SUCCESS;
}

static state_t static_pool(void *data)
{
    while (pthread_mutex_trylock(&lock));
    static_read(energy_uj); // converted to micro Watts
    pthread_mutex_unlock(&lock);
    return EAR_SUCCESS;
}

state_t energy_cpu_hwmon_init(ctx_t *c)
{
    if (sus == NULL) {
        sus             = suscription();
        sus->call_main  = static_pool;
        sus->call_init  = NULL;
        sus->time_relax = MONITORING_TIME * 1000;
        sus->time_burst = MONITORING_TIME * 1000;
        sus->suscribe(sus);
    }
    return EAR_SUCCESS;
}

state_t energy_cpu_hwmon_dispose(ctx_t *c)
{
    if (sus != NULL) {
        monitor_unregister(sus);
        sus = NULL;
    }
    if (chips != NULL) {
        hwmon_close(&chips);
        chips = NULL;
    }
    return EAR_SUCCESS;
}

state_t energy_cpu_hwmon_count_devices(ctx_t *c, uint *devs_count_in)
{
    *devs_count_in = chips_count;
    return EAR_SUCCESS;
}

state_t energy_cpu_hwmon_read(ctx_t *c, ullong *values)
{
    // We disable this reading because in Watts it does not make sense.
    //   return static_pool((void *) values);
    // It is 2 because this apis is DRAM[0]+PKG[1]
    if (values != NULL) {
        memcpy(values, energy_uj, sizeof(ullong)*2);
    }
    return EAR_SUCCESS;
}
