/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#ifndef METRICS_ENERGY_NODE_H
#define METRICS_ENERGY_NODE_H

#include <common/includes.h>
#include <common/system/time.h>
#include <common/types/configuration/cluster_conf.h>
#include <metrics/accumulators/types.h>

struct energy_op {
    state_t (*load)(void **c);
    state_t (*unload)(void **c);
    state_t (*datasize)(size_t *size);
    state_t (*frequency)(ulong *freq);
    state_t (*dc_read)(void *c, edata_t emj);
    state_t (*dc_time_read)(void *c, edata_t emj, ulong *tms);
    state_t (*ac_read)(void *c, edata_t em);
    state_t (*units)(uint *uints);
    state_t (*accumulated)(ulong *e, edata_t init, edata_t end);
    state_t (*energy_to_str)(char *str, edata_t end);
    state_t (*power_limit)(void *c, ulong limit, ulong target);
    uint (*is_null)(edata_t end);
};

/* These two functions breaks load from initialization and simplifies the path
 * to the energy plugin, it's pending to add in EARD */
state_t energy_load(int options, cluster_conf_t *my_cluster_conf, my_node_conf_t *my_node_conf);

state_t energy_unload();

state_t energy_is_loaded();

// state_t energy_handler_clean();

state_t energy_datasize(size_t *size);

state_t energy_frequency(ulong *freq_us);

/* edata_t is a pointer and memory must be allocated using energy_datasize before using it */
state_t energy_dc_read(edata_t energy_mj);

/* edata_t is a pointer and memory must be allocated using energy_datasize before using it */
state_t energy_dc_time_read(edata_t energy_mj, ulong *time_ms);

state_t energy_ac_read(edata_t energy_mj);

/* Energy units are 1=Joules, 1000=mJ, 1000000=uJ, 1000000000nJ */

state_t energy_units(uint *units);

state_t energy_accumulated(unsigned long *e, edata_t init, edata_t end);

state_t energy_to_str(char *str, edata_t e);

state_t energy_set_power_limit(ulong limit, ulong target);

state_t energy_not_privileged_init();

#endif // EAR_ENERGY_H
