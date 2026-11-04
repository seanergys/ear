/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

/* clang-format off */
#ifndef METRICS_COMMON_EASYBUS_H
#define METRICS_COMMON_EASYBUS_H

#include <common/types.h>
#include <common/states.h>
#include <metrics/gpu/gpu.h>
#if __has_include(<accessd/access_easybus.h>)
#include <accessd/access_easybus.h>
#endif

#if !__has_include(<accessd/access_easybus.h>)
#error
enum {
    All,
    Met,
    Act,
    Cpu,
    Pow,
    Freq,
};

typedef struct metric_s {
    int devs_count;
} metric_t;
#endif

typedef struct easybus_s {
    int  (*connect) (char *address);
    void (*disconnect) ();
    int  (*is_connected) ();
    int  (*get_avail_metrics) (metric_t **mets, int *mets_count);
    int  (*search_avail_metrics) (int dom, int met, int dev, metric_t **_met);
    int  (*request_get) (int dom, int met, int dev, int dev_no, metric_t **mets, int *mets_count);
    int  (*request_set) (int met, int dev, int dev_no, uint64_t *vals, int vals_count, char *err_msg);
    int  (*request_set_imm) (int met, int dev, int dev_no, uint64_t val, char *err_msg);
    int  (*metrics_alloc) (metric_t **mets);
} easybus_t;

int easybus_open(easybus_t *easybus, char *address);

void easybus_close();

#endif // METRICS_COMMON_EASYBUS_H