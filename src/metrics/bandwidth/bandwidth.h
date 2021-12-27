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
* This file is licensed under both the BSD-3 license for individual/non-commercial
* use and EPL-1.0 license for commercial use. Full text of both licenses can be
* found in COPYING.BSD and COPYING.EPL files.
*/

#ifndef METRICS_BANDWIDTH_H
#define METRICS_BANDWIDTH_H

#include <common/sizes.h>
#include <common/states.h>
#include <common/plugins.h>
#include <common/system/time.h>
#include <common/types/generic.h>
#include <common/hardware/topology.h>
#include <metrics/common/apis.h>

// Compatibility:
//  -----------------------------------------------
//  | Architecture    | F/M | Comp. | Granularity |
//  -----------------------------------------------
//  | Intel HASWELL   | 63  | v     | Socket      |
//  | Intel BROADWELL | 79  | v     | Socket      |
//  | Intel SKYLAKE   | 85  | v     | Socket      |
//  | AMD ZEN+/2      | 17h | x     | -           |
//  | AMD ZEN3        | 19h | x     | -           |
//  -----------------------------------------------
// Props:
//  - Thread safe: yes
//  - Requires root: no

typedef struct bwidth_s {
	union {
		timestamp_t time;
		ullong cas;
	};
} bwidth_t;

typedef struct bwidth_ops_s {
	state_t (*init)            (ctx_t *c);
	state_t (*dispose)         (ctx_t *c);
	state_t (*count_devices)   (ctx_t *c, uint *dev_count);
	state_t (*get_granularity) (ctx_t *c, uint *granularity);
	state_t (*read)            (ctx_t *c, bwidth_t *b);
	state_t (*data_diff)       (bwidth_t *b2, bwidth_t *b1, ullong *cas, double *gbs);
	state_t (*data_alloc)      (bwidth_t **b);
	state_t (*data_free)       (bwidth_t **b);
	state_t (*data_copy)       (bwidth_t *dst, bwidth_t *src);
	state_t (*data_print)      (ullong cas, double gbs, int fd);
	state_t (*data_tostr)      (ullong cas, double gbs, char *buffer, size_t length);
} bwidth_ops_t;

state_t bwidth_load(topology_t *tp, int eard);

state_t bwidth_get_api(uint *api);

//
state_t bwidth_init(ctx_t *c);

state_t bwidth_dispose(ctx_t *c);

state_t bwidth_count_devices(ctx_t *c, uint *dev_count);

state_t bwidth_get_granularity(ctx_t *c, uint *granularity);

state_t bwidth_read(ctx_t *c, bwidth_t *b);

// Helpers

/* CAS and GBS are just one value. */
state_t bwidth_read_diff(ctx_t *c, bwidth_t *b2, bwidth_t *b1, ullong *cas, double *gbs);

state_t bwidth_read_copy(ctx_t *c, bwidth_t *b2, bwidth_t *b1, ullong *cas, double *gbs);

/* Returns the total node CAS and total node GBs. */
state_t bwidth_data_diff(bwidth_t *b2, bwidth_t *b1, ullong *cas, double *gbs);

state_t bwidth_data_alloc(bwidth_t **b);

state_t bwidth_data_free(bwidth_t **b);

state_t bwidth_data_copy(bwidth_t *dst, bwidth_t *src);

state_t bwidth_data_print(ullong cas, double gbs, int fd);

state_t bwidth_data_tostr(ullong cas, double gbs, char *buffer, size_t length);

#endif
