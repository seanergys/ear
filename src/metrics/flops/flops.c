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

#include <common/output/debug.h>
#include <common/system/time.h>
#include <metrics/flops/archs/dummy.h>
#include <metrics/flops/archs/perf.h>
#include <metrics/flops/flops.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 0       1       2      3      4      5      6      7      8
// CPU-SP, CPU-DP, 128SP, 128DP, 256SP, 256DP, 512HP, 512SP, 512DP
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static flops_ops_t ops;
static apinfo_t info;

void flops_load(topology_t *tp, int options)
{
    while (pthread_mutex_trylock(&lock))
        ;
    if (info.api != API_NONE) {
        goto leave;
    }
    if (API_IS(options, API_DUMMY)) {
        goto dummy;
    }
    flops_perf_load(tp, &ops, options);
dummy:
    flops_dummy_load(tp, &ops, options);
    flops_get_info(&info);
leave:
    pthread_mutex_unlock(&lock);
}

void flops_unload()
{
    while (pthread_mutex_trylock(&lock))
        ;
    if (info.api != API_NONE) {
        ops.unload();
        memset(&ops, 0, sizeof(flops_ops_t));
        memset(&info, 0, sizeof(apinfo_t));
    }
    pthread_mutex_unlock(&lock);
}

void flops_get_info(apinfo_t *info)
{
    memset(info, 0, sizeof(apinfo_t));
    info->layer = "FLOPS";
    info->api   = API_NONE;
    if (ops.get_info != NULL) {
        ops.get_info(info);
    }
}

state_t flops_read(flops_t *fl)
{
    return ops.read(fl);
}

// Helpers
state_t flops_read_diff(flops_t *fl2, flops_t *fl1, flops_t *flD, double *gflops_tot)
{
    state_t s;
    if (state_ok(s = flops_read(fl2))) {
        flops_data_diff(fl2, fl1, flD, gflops_tot);
    }
    return s;
}

state_t flops_read_copy(flops_t *fl2, flops_t *fl1, flops_t *flD, double *gflops_tot)
{
    state_t s;
    if (state_ok(s = flops_read_diff(fl2, fl1, flD, gflops_tot))) {
        flops_data_copy(fl1, fl2);
    }
    return s;
}

void flops_data_diff(flops_t *fl2, flops_t *fl1, flops_t *flD, double *gflops_tot)
{
    return ops.data_diff(fl2, fl1, flD, gflops_tot);
}

void flops_data_alloc(flops_t **fl)
{
    if (fl != NULL) {
        *fl = calloc(info.devs_count, sizeof(flops_t));
    }
}

void flops_data_free(flops_t **fl)
{
    if (fl != NULL) {
        free(*fl);
        *fl = NULL;
    }
}

void flops_data_copy(flops_t *dst, flops_t *src)
{
    memcpy(dst, src, sizeof(flops_t) * info.devs_count);
}

void flops_data_print(flops_t *flD, double gflops_tot, int fd)
{
    char buffer[SZ_BUFFER];
    flops_data_tostr(flD, gflops_tot, buffer, SZ_BUFFER);
    dprintf(fd, "%s", buffer);
}

void flops_data_tostr(flops_t *flD, double gflops_tot, char *buffer, size_t length)
{
    int added = 0;
    int i;

    for (i = 0; i < info.devs_count; ++i) {
        added += sprintf(&buffer[added], "DEV%d insts ", i);
        added += sprintf(&buffer[added], "[%14llu %14llu] ", flD[i].f64, flD[i].d64);
        added += sprintf(&buffer[added], "[%14llu %14llu] ", flD[i].f128, flD[i].d128);
        added += sprintf(&buffer[added], "[%14llu %14llu] ", flD[i].f256, flD[i].d256);
        added += sprintf(&buffer[added], "[%14llu %14llu] ", flD[i].f512, flD[i].d512);
        added += sprintf(&buffer[added], "(%0.2lf GF/s in %0.2lf s)\n", flD[i].gflops, flD[i].secs);
    }
}

void flops_data_accum(flops_t *flA, flops_t *flD, double *gflops_tot)
{
    double gflops = 0.0;
    int i;

    for (i = 0; i < info.devs_count; ++i) {
        if (flD != NULL) {
            // flAccum
            flA[i].f64 += flD[i].f64;
            flA[i].d64 += flD[i].d64;
            flA[i].f128 += flD[i].f128;
            flA[i].d128 += flD[i].d128;
            flA[i].f256 += flD[i].f256;
            flA[i].d256 += flD[i].d256;
            flA[i].f512 += flD[i].f512;
            flA[i].d512 += flD[i].d512;
            flA[i].secs += flD[i].secs;
            // flA[i].gflops += flD[i].gflops;
        }
        // Individual counters are already weigthed
        if (flA[i].secs > 0.0) {
            flA[i].gflops = (flA[i].f64 + flA[i].d64 + flA[i].f128 + flA[i].d128 + flA[i].f256 + flA[i].d256 +
                             flA[i].f512 + flA[i].d512) /
                            flA[i].secs;
            flA[i].gflops /= ((double) 1E9);
        }
        gflops += flA[i].gflops;
#if SHOW_DEBUGS
        dprintf(debug_channel, "DEV%d accum ", i);
        dprintf(debug_channel, "[%14lld %14lld] ", flA[i].f64, flA[i].d64);
        dprintf(debug_channel, "[%14lld %14lld] ", flA[i].f128, flA[i].d128);
        dprintf(debug_channel, "[%14lld %14lld] ", flA[i].f256, flA[i].d256);
        dprintf(debug_channel, "[%14lld %14lld] ", flA[i].f512, flA[i].d512);
        dprintf(debug_channel, "(%0.2lf GF/s in %0.2lf s)\n", flA[i].gflops, flA[i].secs);
#endif
    }
    dprintf(debug_channel, "DEVS accum %0.2lf GF/s\n", gflops);
    if (gflops_tot != NULL) {
        *gflops_tot = gflops;
    }
}

void flops_internals_print(int fd)
{
    char buffer[1024];
    flops_internals_tostr(buffer, 1024);
    dprintf(fd, "%s", buffer);
}

void flops_internals_tostr(char *buffer, int length)
{
    ops.internals_tostr(buffer, length);
}

ullong *flops_help_toold(flops_t *flD, ullong *flops)
{
    int i;

    for (i = 0; i < info.devs_count; i += 8) {
        flops[i + 0] = flD[i + 0].f64;
        flops[i + 4] = flD[i + 4].d64;
        flops[i + 1] = flD[i + 1].f128;
        flops[i + 5] = flD[i + 5].d128;
        flops[i + 2] = flD[i + 2].f256;
        flops[i + 6] = flD[i + 6].d256;
        flops[i + 3] = flD[i + 3].f512;
        flops[i + 7] = flD[i + 7].d512;
        debug("flops[%d]: %llu", i + 0, flops[i + 0]);
        debug("flops[%d]: %llu", i + 1, flops[i + 1]);
        debug("flops[%d]: %llu", i + 2, flops[i + 2]);
        debug("flops[%d]: %llu", i + 3, flops[i + 3]);
        debug("flops[%d]: %llu", i + 4, flops[i + 4]);
        debug("flops[%d]: %llu", i + 5, flops[i + 5]);
        debug("flops[%d]: %llu", i + 6, flops[i + 6]);
        debug("flops[%d]: %llu", i + 7, flops[i + 7]);
    }
    return flops;
}
