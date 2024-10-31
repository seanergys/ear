/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

static hwmon_t *chips;
static uint     chips_count;
static uint     sockets_count;
static uint     items_count;
static char    *label;
static char    *driver_name;

TEMP_F_LOAD(hwmon)
{
    if (tp->vendor == VENDOR_AMD || tp->family >= FAMILY_ZEN) {
        items_count = tp->core_count*4;
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
    }
    if (hwmon_count_items(chips, "input", label) != items_count) {
        hwmon_close(&chips);
        return;
    }
    sockets_count = tp->socket_count;
    apis_set(ops->get_info, temp_hwmon_get_info);
    apis_set(ops->init,     temp_hwmon_init);
    apis_set(ops->dispose,  temp_hwmon_dispose);
    apis_set(ops->read,     temp_hwmon_read);
}

TEMP_F_GET_INFO(hwmon)
{
    info->api         = API_HWMON;
    info->scope       = SCOPE_NODE;
    info->granularity = GRANULARITY_SOCKET;
    info->devs_count  = socket_count;
}

TEMP_F_INIT(hwmon)
{
    // Nothing
}

TEMP_F_DISPOSE(hwmon)
{
    hwmon_close(&chips);
}

TEMP_F_READ(hwmon)
{
    int c = 0;

    if (average != NULL) {
        *average = 0LL;
    }
    // Reading and calculating the average per chip
    hwmon_read(chips);
    hwmon_calc_average(chips, label);
    while((chip = hwmon_iter_chips(chips)) != NULL) {
        if (temp    != NULL) temp[c]   = (llong) chip->dev_avg.input_toint;
        if (average != NULL) *average += (llong) chip->dev_avg.input_toint;
        ++c;
    }
    if (average != NULL) {
        *average /= (llong) c;
    }
}