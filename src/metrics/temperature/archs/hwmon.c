/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

//#define SHOW_DEBUGS 1

#include <common/output/debug.h>
#include <metrics/common/hwmon.h>
#include <metrics/temperature/archs/hwmon.h>

static hwmon_t *chips;
static uint chips_count;
static uint sockets_count;
static uint items_count;
static char *label;
static char *driver_name;

TEMP_F_LOAD(hwmon)
{
    if (tp->vendor == VENDOR_AMD || tp->family >= FAMILY_ZEN) {
        items_count = tp->core_count / 8; // 8 cores * CCD
        driver_name = "k10temp";
        label       = "Tccd";
    } else if (tp->vendor == VENDOR_INTEL || tp->family >= MODEL_ICELAKE_X) {
        items_count = tp->socket_count;
        driver_name = "coretemp";
        label       = "Package";
    } else {
        return;
    }
    if (state_fail(hwmon_open(driver_name, "temp", &chips, &chips_count))) {
        debug("Failed: %s", state_msg);
        return;
    }
    if (hwmon_count_items(chips, "input", label) != items_count) {
        hwmon_close(&chips);
        return;
    }
    sockets_count = tp->socket_count;
    apis_put(ops->get_info, temp_hwmon_get_info);
    apis_put(ops->init,     temp_hwmon_init);
    apis_put(ops->dispose,  temp_hwmon_dispose);
    apis_put(ops->read,     temp_hwmon_read);
}

TEMP_F_GET_INFO(hwmon)
{
    info->api         = API_HWMON;
    info->scope       = SCOPE_NODE;
    info->granularity = GRANULARITY_SOCKET;
    info->devs_count  = sockets_count;
}

TEMP_F_INIT(hwmon)
{
    // Nothing
    return EAR_SUCCESS;
}

TEMP_F_DISPOSE(hwmon)
{
    if (chips != NULL) {
        hwmon_close(&chips);
        chips = NULL;
    }
}

TEMP_F_READ(hwmon)
{
    llong temp_aux;
    hwmon_t *chip;
    int c = 0;

    if (average != NULL) {
        *average = 0LL;
    }
    // Reading and calculating the average per chip
    hwmon_read(chips);
    hwmon_calc_average(chips, label);
    while ((chip = hwmon_iter_chips(chips)) != NULL) {
        temp_aux = ((llong) chip->devs_avg.input_toint) / 1000LL;
        if (temp != NULL)
            temp[c] = temp_aux;
        if (average != NULL)
            *average += (llong) chip->devs_avg.input_toint;
        ++c;
    }
    if (average != NULL) {
        *average /= (llong) c;
    }
    return EAR_SUCCESS;
}
