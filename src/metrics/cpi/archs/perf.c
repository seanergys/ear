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

#include <stdio.h>
#include <stdlib.h>
#include <common/output/debug.h>
#include <metrics/common/perf.h>
#include <metrics/common/offsets.h>
#include <metrics/cpi/archs/perf.h>

typedef struct perfs_s {
    perf_t       perfs[4];
    uint         perfs_count;
    offset_ops_t ofs;
} perfs_t;

// Generic
static perfs_t *perfs;
static uint     perfs_count;
static uint     scope;

static void add_perf(perfs_t *p, ullong event, uint type, ulong offset_result, pid_t pid, uint cpu, const char *event_desc)
{
    offsets_add(&p->ofs, offset_result, NO_OFFSET, NO_OFFSET, '=');
    if (state_ok(perf_open_cpu(&p->perfs[p->perfs_count], NULL, pid, type, event, 0, cpu))) {
        if (state_fail(perf_start(&p->perfs[p->perfs_count]))) {
            debug("perf_start() failed: %s", state_msg);
            perf_close(&p->perfs[p->perfs_count]);
            return;
        }
    } else {
        debug("perf_open() failed: %s", state_msg);
        return;
    }
    strcpy(p->perfs[p->perfs_count].event_name, event_desc);
    ++p->perfs_count;
}

static void add(ullong event, uint type, ulong offset_result, const char *event_desc)
{
    int i;
    if (scope == SCOPE_PROCESS) {
        add_perf(&perfs[0], event, type, offset_result, 0, -1, event_desc);
        return;
    }
    for (i = 0; i < perfs_count; ++i) {
        add_perf(&perfs[i], event, type, offset_result, -1, i, event_desc);
    }
}

static int perfs_count_opened_fds()
{
    int i, j, k;
    for (i = k = 0; i < perfs_count; ++i) {
        for (j = 0; j < perfs[i].perfs_count; ++j) {
            // We assume 0 means unopened file descriptor
            k += (perfs[i].perfs[j].fd > 0);
        }
    }
    return k;
}

CPI_F_LOAD(perf)
{
    // clang-format off
    #define O2(l, v) (((int) offsetof(cpi_t, l)) + ((int) offsetof(stalls_t, v)))
    #define O1(l)    (((int) offsetof(cpi_t, l)))

    if (SCOPE_IS(options, SCOPE_NODE)) {
        perfs_count = tp->cpu_count;
        perfs       = calloc(tp->cpu_count, sizeof(perfs_t));
        scope       = SCOPE_NODE;
    } else {
        perfs_count = 1;
        perfs       = calloc(1, sizeof(perfs_t));
        scope       = SCOPE_PROCESS;
    }
    if (tp->vendor == VENDOR_INTEL && tp->model >= MODEL_ICELAKE_X) {
        add(PERF_COUNT_HW_INSTRUCTIONS, PERF_TYPE_HARDWARE, O1(instructions)  , "PERF_COUNT_HW_INSTRUCTIONS ");
        add(PERF_COUNT_HW_CPU_CYCLES  , PERF_TYPE_HARDWARE, O1(cycles)        , "PERF_COUNT_HW_CPU_CYCLES   ");
        add(0x40004a3                 , PERF_TYPE_RAW     , O2(stalls, memory), "CYCLE_ACTIVITY.STALLS_TOTAL");
    } else if (tp->vendor == VENDOR_INTEL && tp->model >= MODEL_BROADWELL_X) {
        add(PERF_COUNT_HW_INSTRUCTIONS, PERF_TYPE_HARDWARE, O1(instructions)     , "PERF_COUNT_HW_INSTRUCTIONS ");
        add(PERF_COUNT_HW_CPU_CYCLES  , PERF_TYPE_HARDWARE, O1(cycles)           , "PERF_COUNT_HW_CPU_CYCLES   ");
        add(0x40004a3                 , PERF_TYPE_RAW     , O2(stalls, memory)   , "CYCLE_ACTIVITY.STALLS_TOTAL");
        add(0x00001a2                 , PERF_TYPE_RAW     , O2(stalls, resources), "RESOURCE_STALLS.ANY        ");
    } else if (tp->vendor == VENDOR_AMD && tp->family >= FAMILY_ZEN) {
        // PMCx087 is inherited from Family 15h
        add(PERF_COUNT_HW_INSTRUCTIONS, PERF_TYPE_HARDWARE, O1(instructions)        , "PERF_COUNT_HW_INSTRUCTIONS   ");
        add(PERF_COUNT_HW_CPU_CYCLES  , PERF_TYPE_HARDWARE, O1(cycles)              , "PERF_COUNT_HW_CPU_CYCLES     ");
        add(0x0000287                 , PERF_TYPE_RAW     , O2(stalls, fetch_decode), "IC_FETCH_STALL.DECODING_QUEUE");
        add(0x0000187                 , PERF_TYPE_RAW     , O2(stalls, resources)   , "IC_FETCH_STALL.BACK_PRESSURE ");
    } else {
        add(PERF_COUNT_HW_INSTRUCTIONS           , PERF_TYPE_HARDWARE, O1(instructions)        , "PERF_COUNT_HW_INSTRUCTIONS   ");
        add(PERF_COUNT_HW_CPU_CYCLES             , PERF_TYPE_HARDWARE, O1(cycles)              , "PERF_COUNT_HW_CPU_CYCLES     ");
        add(PERF_COUNT_HW_STALLED_CYCLES_FRONTEND, PERF_TYPE_HARDWARE, O2(stalls, fetch_decode), "PERF_COUNT_HW_STALLS_FRONTEND");
        add(PERF_COUNT_HW_STALLED_CYCLES_BACKEND , PERF_TYPE_HARDWARE, O2(stalls, memory)      , "PERF_COUNT_HW_STALLS_BACKEND ");
    }
    if (!perfs_count_opened_fds()) {
        return;
    }
    apis_put(ops->unload  , cpi_perf_unload  );
    apis_put(ops->get_info, cpi_perf_get_info);
    apis_put(ops->read    , cpi_perf_read    );
    // clang-format on
}

CPI_F_UNLOAD(perf)
{
    int i, j;

    if (perfs == NULL) {
        return;
    }
    for (i = 0; i < perfs_count; ++i) {
        for (j = 0; j < perfs[i].perfs_count; ++j) {
            // We assume 0 means unopened file descriptor
            if (perfs[i].perfs[j].fd <= 0) {
                continue;
            }
            perf_close(&perfs[i].perfs[j]);
        }
        perfs[i].perfs_count = 0;
    }
    free(perfs);
    perfs = NULL;
}

void cpi_perf_get_info(apinfo_t *info)
{
    info->api         = API_PERF;
    info->devs_count  = perfs_count;
    info->scope       = scope;
    info->granularity = (scope == SCOPE_NODE) ? GRANULARITY_CPU: GRANULARITY_PROCESS;
}

state_t cpi_perf_read(cpi_t *ci)
{
    llong value;
    state_t s;
    int i, j;

    for (i = 0; i < perfs_count; ++i) {
        #if SHOW_DEBUGS
        dprintf(debug_channel, "DEV%d ", i);
        #endif
        for (j = 0; j < perfs[i].perfs_count; ++j) {
            // We assume 0 means unopened file descriptor
            if (perfs[i].perfs[j].fd <= 0) {
                continue;
            }
            value = 0LL;
            if (state_fail(s = perf_read(&perfs[i].perfs[j], &value))) {
                return s;
            }
            offsets_calc_one(&perfs[i].ofs, (void *) &ci[i], j, value, 0LLU);
            #if SHOW_DEBUGS
            dprintf(debug_channel, "%14lld ", value);
            #endif
        }
        offsets_calc_all(&perfs[i].ofs, (void *) &ci[i]);
        #if SHOW_DEBUGS
        dprintf(debug_channel, "\n");
        #endif
    }
    return EAR_SUCCESS;
}