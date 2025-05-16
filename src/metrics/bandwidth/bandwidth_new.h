/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#ifndef METRICS_BANDWIDTH_H
#define METRICS_BANDWIDTH_H

typedef struct bwidth_s {
    union {
        timestamp_t time;
        double secs;
        ullong cas;
    };
} bwidth_t;

#define API_MODE_KEEP   0
#define API_MODE_RELOAD 1

// Combined with adebug()
void bwidth_load(topology_t *tp, int mode, int force_api);

void bwidth_get_info(apinfo_t *api);

// Include overhead calls.
state_t bwidth_read(ctx_t *c, bwidth_t *b);

state_t bwidth_read_diff(ctx_t *c, bwidth_t *b2, bwidth_t *b1, bwidth_t *bD, ullong *cas, double *gbs);

state_t bwidth_read_copy(ctx_t *c, bwidth_t *b2, bwidth_t *b1, bwidth_t *bD, ullong *cas, double *gbs);

void bwidth_data_diff(bwidth_t *b2, bwidth_t *b1, bwidth_t *bD, ullong *cas, double *gbs);

void bwidth_data_alloc(bwidth_t **b);

void bwidth_data_free(bwidth_t **b);

void bwidth_data_null(bwidth_t *bws);

void bwidth_data_copy(bwidth_t *dst, bwidth_t *src);

void bwidth_data_print(ullong cas, double gbs, int fd);

char *bwidth_data_tostr(ullong cas, double gbs, char *buffer, size_t length);

#endif