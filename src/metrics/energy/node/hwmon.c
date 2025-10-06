/*********************************************************************
 * Copyright (c) 2024 Energy Aware Solutions, S.L
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **********************************************************************/

#define SHOW_DEBUGS 1
#include <common/output/debug.h>
#include <common/system/monitor.h>
#include <metrics/common/hwmon.h>
#include <metrics/energy_cpu/archs/hwmon.h>

#define MONITORING_TIME 3

static hwmon_t       *chips;
static uint           chips_count;
static uint           items_count;
static suscription_t *sus;
static ullong        *energy_uw;
static char          *label;
static topology_t     tp;

static state_t energy_cpu_hwmon_init(ctx_t *c)
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

state_t energy_init(void **c)
{
    topology_init(&tp);
    if (tp.vendor == VENDOR_ARM && tp.model == MODEL_NEOVERSE_V2) {
        if (tp.socket_count > 1) {
            label = "Grace Power Socket"; // Energy is in micro Watts
        } else {
            label = "Module Power Socket"; // Energy is in micro Watts
        }
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
    debug("sockets_count: %d", tp.socket_count);
    energy_uw = calloc(chips_count, sizeof(ullong));
    return energy_cpu_hwmon_init();
}

static state_t static_read(ullong *values)
{
    hwmon_t *chip;
    int i = 0;

    hwmon_read(chips);
    hwmon_calc_average(chips, NULL);
    while ((chip = hwmon_iter_chips(chips)) != NULL) {
        // Converted to micro Joules
        values[i] += ((ullong) chip->devs_avg.average_toint) * (ullong) MONITORING_TIME;
        debug("ENERGY[%d]: %llu += %llu * %d", i, values[i], ((ullong) chip->devs_avg.average_toint), MONITORING_TIME);
        i++;
    }
    return EAR_SUCCESS;
}

static state_t static_pool(void *data)
{
    while (pthread_mutex_trylock(&lock));
    static_read(energy_uw);
    if (data != NULL) {
        memcpy(data, energy_uw, sizeof(ullong)*chips_count);
    }
    pthread_mutex_unlock(&lock);
    return EAR_SUCCESS;
}

state_t energy_dispose(void **c)
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

state_t energy_dc_read(void *c, void *data)
{
    return static_pool(data);
}

state_t energy_datasize(size_t *size)
{
    *size = sizeof(ullong) * chips_count;
    return EAR_SUCCESS;
}

state_t energy_units(uint *units)
{
    *units = 1000; // mJ
    return EAR_SUCCESS;
}

state_t energy_accumulated(ulong *energy_mj, void *data1, void *data2)
{
    ullong *readings2 = (ullong *) data2;
    ullong *readings1 = (ullong *) data1;
    int s;

    *energy_mj = 0LU;
    for (s = 0; s < sensors_count; ++s) {
        if (readings2->samples != 0LLU && readings1->samples != 0LLU) {
            *energy_mj += (ulong) (overflow_zeros_u64(readings2[s], readings1[s]) / 1000LLU);
            debug("energy: %lu mJ (%lu uJ - %lu uJ)",
                *energy_mj, readings2[s], readings1[s]);
        }
    }
    return EAR_SUCCESS;
}

state_t energy_to_str(char *buffer, void *data)
{
    sprintf(buffer, "%llu", ((ullong *) data));
    return EAR_SUCCESS;
}

uint energy_data_is_null(void *data)
{
    return ((ullong *) data) == 0LLU;
}