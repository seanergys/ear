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

#include <stdio.h>
#include <stdlib.h>

#include <common/output/debug.h>
#include <common/states.h>
#include <common/system/time.h>
#include <metrics/energy/energy_node.h>

static timestamp_t start_time;
static uint init;

/*
 * MAIN FUNCTIONS
 */

ENERGY_NODE_F_LOAD(dummy)
{
    if (!init) {
        timestamp_get(&start_time);
        init = 1;
    }

    debug("Energy dummy plugin initialized");
    return EAR_SUCCESS;
}

ENERGY_NODE_F_UNLOAD(dummy)
{
    return EAR_SUCCESS;
}

ENERGY_NODE_F_DATASIZE(dummy)
{
    debug("energy_datasize %lu\n", sizeof(unsigned long));
    *size = sizeof(unsigned long);
    return EAR_SUCCESS;
}

ENERGY_NODE_F_FREQUENCY(dummy)
{
    *freq_us = 1000000;
    return EAR_SUCCESS;
}

ENERGY_NODE_F_TO_STR(dummy)
{
    sprintf(str, "NA-dummy");
    return EAR_SUCCESS;
}

unsigned long diff_node_energy(ulong init, ulong end)
{
    return end - init;
}

ENERGY_NODE_F_UNITS(dummy)
{
    *units = 1000;
    return EAR_SUCCESS;
}

ENERGY_NODE_F_ACCUMULATED(dummy)
{
    ulong *pinit = (ulong *) init, *pend = (ulong *) end;

    ulong total = diff_node_energy(*pinit, *pend);
    *e          = total;
    return EAR_SUCCESS;
}

ENERGY_NODE_F_DC_READ(dummy)
{

    ulong *penergy_mj = (ulong *) energy_mj;

    ullong elapsed = timestamp_diffnow(&start_time, TIME_MSECS);

    *penergy_mj = (ulong) elapsed * 150; // Dummy power: 150 W
    debug("energy_dc_read: %lu elapsed: %llu\n", *penergy_mj, elapsed);

    return EAR_SUCCESS;
}

ENERGY_NODE_F_DC_TIME_READ(dummy)
{
    ulong *penergy_mj = (ulong *) energy_mj;

    ullong elapsed = timestamp_diffnow(&start_time, TIME_MSECS);

    *penergy_mj = (ulong) elapsed * 150; // Dummy power: 150 W
    debug("energy_dc_read: %lu elapsed: %llu\n", *penergy_mj, elapsed);
    *time_ms = (ulong) time(NULL) * 1000;

    return EAR_SUCCESS;
}

ENERGY_NODE_F_IS_NULL(dummy)
{
    ulong *pe = (ulong *) energy_mj;
    return (*pe == 0);
}

ENERGY_NODE_F_AC_READ(dummy)
{
    return EAR_ERROR;
}

ENERGY_NODE_F_POWER_LIMIT(dummy)
{
    return EAR_ERROR;
}

ENERGY_NODE_F_LOAD_PLUGIN(dummy)
{
    ops->load          = energy_dummy_load;
    ops->unload        = energy_dummy_unload;
    ops->datasize      = energy_dummy_datasize;
    ops->frequency     = energy_dummy_frequency;
    ops->dc_read       = energy_dummy_dc_read;
    ops->dc_time_read  = energy_dummy_dc_time_read;
    ops->ac_read       = energy_dummy_ac_read;
    ops->units         = energy_dummy_units;
    ops->accumulated   = energy_dummy_accumulated;
    ops->energy_to_str = energy_dummy_to_str;
    ops->power_limit   = energy_dummy_power_limit;
    ops->is_null       = energy_dummy_data_is_null;
    return EAR_SUCCESS;
}
