/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#define SHOW_DEBUGS 1

#include <common/system/time.h>
#include <common/output/debug.h>
#include <common/math_operations.h>
#include <metrics/common/perf.h>
#include <metrics/cache/archs/perf.h>

typedef struct arith_s {
    void *p_res;
    void *p_op1;
    void *p_op2;
    ulong offset_res;
    ulong offset_op1;
    ulong offset_op2;
    char  operation;
    char  type;
} arith_t;

static perf_t  perfs[5];
static uint    perfs_count;
static arith_t ariths[3];
static uint    ariths_count;
static ulong   ll_offset;
static double  line_size;
static uint    started;

static void set_perf(ullong event, uint type, ulong offset, const char *event_desc)
{
    perf_open(&perfs[perfs_count], NULL, 0, type, event);
    strcpy(perfs[perfs_count].event_name, event_desc);
    perfs[perfs_count].aux = (llong) offset;
    ++perfs_count;
}

static void arith_add(ulong offset_res, ulong offset_op1, ulong offset_op2, char op)
{
    ariths[ariths_count].offset_res = offset_res;
    ariths[ariths_count].offset_op1 = offset_op1;
    ariths[ariths_count].offset_op2 = offset_op2;
    ariths[ariths_count].operation  = op;
    ++ariths_count;
}

static void ariths_calc(void *base_addr, arith_t *ariths, uint ariths_count)
{
    int i;
    for (i = 0; i < ariths_count; ++i) {
        void *p_res = (base_addr) ? base_addr + ariths[i].offset_res: ariths[i].p_res;
        void *p_op1 = (base_addr) ? base_addr + ariths[i].offset_op1: ariths[i].p_op1;
        void *p_op2 = (base_addr) ? base_addr + ariths[i].offset_op2: ariths[i].p_op2;
        //printf("%p %p %p %llu %llu\n", p_res, p_op1, p_op2, *((ullong *) p_op1), *((ullong *) p_op2));
        if (ariths[i].operation == '+') *((ullong *) p_res) = *((ullong *) p_op1) + *((ullong *) p_op2);
        if (ariths[i].operation == '-') *((ullong *) p_res) = *((ullong *) p_op1) - *((ullong *) p_op2);
        if (ariths[i].operation == '=') *((ullong *) p_res) = *((ullong *) p_op1);
    }
}

#define O(l, v) (offsetof(cache_t, l) + offsetof(cache_level_t, v))

void cache_perf_load(topology_t *tp, cache_ops_t *ops)
{
    // Saving the last cache level
    line_size = (double) tp->cache_line_size;
    //
    if (tp->vendor == VENDOR_INTEL && tp->model >= MODEL_TIGERLAKE) {
        set_perf(0x01d1, PERF_TYPE_RAW, O(l1d, hits    ), "MEM_LOAD_RETIRED.L1_HIT       ");
        set_perf(0x08d1, PERF_TYPE_RAW, O(l1d, misses  ), "MEM_LOAD_RETIRED.L1_MISS      ");
        set_perf(0xe724, PERF_TYPE_RAW, O(l2 , accesses), "L2_RQSTS.ALL_DEMAND_REFERENCES");
        set_perf(0x2724, PERF_TYPE_RAW, O(l2 , misses  ), "L2_RQSTS.ALL_DEMAND_MISS      ");
        arith_add(O(l1d, accesses), O(l1d, hits    ), O(l1d, misses), '+');
        arith_add(O(l2 , hits    ), O(l2 , accesses), O(l2 , misses), '-');
        arith_add(O(ll , misses  ), O(l2 , misses  ),              0, '=');
    } else if (tp->vendor == VENDOR_AMD && tp->family >= FAMILY_ZEN3) {
        set_perf(0xf064, PERF_TYPE_RAW, O(l2 , hits    ), "PMCx064.1111.0000");
        set_perf(0x0864, PERF_TYPE_RAW, O(l2 , misses  ), "PMCx064.0000.1000");
        set_perf(0x0729, PERF_TYPE_RAW, O(l1d, accesses), "PMCx029.0000.0111");
        arith_add(O(l2 , accesses), O(l2 , misses  ), O(l2 , hits  ), '+');
        arith_add(O(l1d, misses  ), O(l2 , accesses),              0, '=');
        arith_add(O(l1d, hits    ), O(l1d, accesses), O(l1d, misses), '-');
    } else {
        return;
    }
    apis_put(ops->get_info,        cache_perf_get_info);
    apis_put(ops->init,            cache_perf_init);
    apis_put(ops->dispose,         cache_perf_dispose);
    apis_put(ops->read,            cache_perf_read);
    apis_put(ops->internals_tostr, cache_perf_internals_tostr);
    debug("Loaded PERF")
}

void cache_perf_get_info(apinfo_t *info)
{
    info->api         = API_PERF;
    info->scope       = SCOPE_PROCESS;
    info->granularity = GRANULARITY_PROCESS;
    info->devs_count  = 1;
}

state_t cache_perf_init()
{
    int i;
	if (started) {
        return EAR_SUCCESS;
    }
    for (i = 0; i < perfs_count; ++i) {
        if (perfs[i].fd >= 0) {
            perf_start(&perfs[i]);
        }
    }
    ++started;
	return EAR_SUCCESS;
}

state_t cache_perf_dispose()
{
    int i;
    if (started){
        for (i = 0; i < perfs_count; ++i) {
            if (perfs[i].fd >= 0) {
                perf_close(&perfs[i]);
            }
        }
    }
    started = 0;
    perfs_count = 0;
    return EAR_SUCCESS;
}

state_t cache_perf_read(cache_t *ca)
{
    void *p = (void *) ca;
    llong value;
    state_t s;
    int i;
    // Cleaning
    memset(ca, 0, sizeof(cache_t));
    // Reading
    timestamp_get(&ca->time);
    for (i = 0; i < perfs_count; ++i) {
        if (perfs[i].fd >= 0) {
            value = 0;
            if (state_fail(s = perf_read(&perfs[i], &value))) {
                return s;
            }
            *((llong *) (p+perfs[i].aux)) += value;
            debug("%s: %llu", perfs[i].event_name, *((ullong *) (p+perfs[i].aux)));
        }
    }
    ariths_calc(p, ariths, ariths_count);
    #if SHOW_DEBUGS
    cache_data_print(ca, 0.0, fderr);
    #endif
    return EAR_SUCCESS;
}

void cache_perf_internals_tostr(char *buffer, int length)
{
    int i, c;
    for (i = c = 0; i < perfs_count; ++i) {
        c += sprintf(&buffer[c], "%s: %s (event %s) %s\n",
             perfs[i].event_name, (perfs[i].fd > 0) ? "working": "not-working");
    }
}