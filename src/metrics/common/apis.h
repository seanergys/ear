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
* EAR is an open source software, and it is licensed under both the BSD-3 license
* and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
* and COPYING.EPL files.
*/

#ifndef METRICS_COMMON_APIS_H
#define METRICS_COMMON_APIS_H

#include <common/types/generic.h>

// APIs IDs
#define API_NONE         0
#define API_DUMMY        1
#define API_EARD         2
#define API_BYPASS       3
#define API_DEFAULT      4
#define API_INTEL63      5
#define API_AMD17        6
#define API_NVML         7
#define API_PERF         8
#define API_INTEL106     9
#define API_LIKWID      10
#define API_CUPTI       11
#define API_ONEAPI      12
// Load EARD API or not
#define EARD             1
#define NO_EARD          0
#define no_ctx           NULL
//
#define GRANULARITY_NONE       0
#define GRANULARITY_PROCESS    1
#define GRANULARITY_THREAD     2
#define GRANULARITY_CORE       3
#define GRANULARITY_CCX        4
#define GRANULARITY_CCD        5
#define GRANULARITY_L3_SLICE   6
#define GRANULARITY_IMC        7
#define GRANULARITY_SOCKET     8
#define GRANULARITY_NODE       9

typedef struct ctx_s {
    void *context;
} ctx_t;

typedef struct metrics_s {
    ctx_t c;
    uint  api;
    uint  ok;
    uint  granularity;
    uint  devs_count;
    void *avail_list;
    uint  avail_count;
    void *current_list;
    void *set_list;
} metrics_t;

#if NEW_GPU_API
#if USE_GPUS
// Below type was created to mantain compatibility with the current design of library/metrics and new GPU APIs
typedef struct metrics_gpus_s {
    uint  api;
    uint  ok;
    uint  devs_count;
    void **avail_list;
    uint *avail_count;
    void *current_list;
} metrics_gpus_t;
#endif // USE_GPUS
#endif // NEW_GPU_API

/* Replaces the function of the operation if its NULL. */
#define apis_put(op, func) \
    if (op == NULL) { \
        *((void **) &op) = func; \
    }

/* Replaces the currect function of the operation. */
#define apis_set(op, func) *((void **) &op) = func;

/* */
#define apis_loaded(ops) (ops->init != NULL)

/* */
#define apis_not(ops) (ops->init == NULL)

/* Adds a new function to an array. */
#define apis_add(ops, func) \
	apis_append((void **) ops, func);

/* Provisional multicall function */
#define apis_multi(ops, ...) \
    state_t s; \
    int i = 0; \
    while (ops [i] != NULL) { \
        if (state_fail(s = ops [i](__VA_ARGS__))) { \
            debug("failed " #ops "[%d]: %s", i, state_msg); \
            return s; \
        } \
        ++i; \
    }

#define apis_multiret(ops, ...) \
    apis_multi(ops, __VA_ARGS__); \
    return s;

void apis_append(void *op[], void *func);

void apis_print(uint api, char *prefix);

void apis_tostr(uint api, char *buffer, size_t size);

/* Obsolete, remove it. */
#define replace_ops(p1, p2) \
	if (p1 == NULL) { \
		p1 = p2; \
	}

#endif
