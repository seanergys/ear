/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#ifndef METRICS_COMMON_HWMON
#define METRICS_COMMON_HWMON

#include <common/types.h>
#include <common/states.h>

/*
    There is only one value per file, unlike the older /proc specification.
    The common scheme for files naming is: <type><number>_<item>. Usual
    types for sensor chips are "in" (voltage), "temp" (temperature) and
    "fan" (fan). Usual items are "input" (measured value), "max" (high
    threshold, "min" (low threshold). Numbering usually starts from 1,
    except for voltages which start from 0 (because most data sheets use
    this). A number is always used for elements that can be present more
    than once, even if there is a single element of the given type on the
    specific chip. Other files do not refer to a specific element, so
    they have a simple name, and no number.
 */

#define HWMON_DEV_TEMP   "temp"
#define HWMON_DEV_POWER  "power"
#define HWMON_DEV_ENERGY "energy"

#define hwmon_add(var) \
    int   var ##_fd; \
    char  var[32];     \
    int   var ##_toint;

typedef struct hwmon_s {
    hwmon_add(input);
    hwmon_add(label);
    hwmon_add(max);
    hwmon_add(min);
    hwmon_add(average);
    hwmon_add(average_interval);
} hwmon_t;

state_t hwmon_open(char *chip_name, uint *chips_count, char *devs_name, hwmon_t ***devs, uint **devs_count);

state_t hwmon_read(hwmon_t **devs);

#endif //METRICS_COMMON_HWMON