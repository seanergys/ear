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
#include <string.h>
#include <pthread.h>
#include <common/system/time.h>
#include <common/output/debug.h>
#include <metrics/flops/flops.h>
#include <metrics/flops/archs/perf.h>
#include <metrics/flops/archs/dummy.h>

// 0       1       2      3      4      5      6      7      8
// CPU-SP, CPU-DP, 128SP, 128DP, 256SP, 256DP, 512HP, 512SP, 512DP
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static flops_ops_t     ops;
static apinfo_t        info;

void flops_load(topology_t *tp, int options)
{
    while (pthread_mutex_trylock(&lock));
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
    while (pthread_mutex_trylock(&lock));
    if (info.api != API_NONE) {
        ops.unload();
        memset(&ops, 0, sizeof(flops_ops_t));
        memset(&info, 0, sizeof(apinfo_t));
    }
    pthread_mutex_unlock(&lock);
}

void flops_get_info(apinfo_t *info)
{
    info->layer       = "FLOPS";
    info->api         = API_NONE;
    if (ops.get_info != NULL) {
        ops.get_info(info);
    }
}

state_t flops_read(flops_t *fl)
{
    return ops.read(fl);
}

// Helpers
state_t flops_read_diff(flops_t *fl2, flops_t *fl1, flops_t *flD, double *gfs)
{
    state_t s;
    if (state_ok(s = flops_read(fl2))) {
        flops_data_diff(fl2, fl1, flD, gfs);
    }
    return s;
}

state_t flops_read_copy(flops_t *fl2, flops_t *fl1, flops_t *flD, double *gfs)
{
    state_t s;
    if (state_ok(s = flops_read_diff(fl2, fl1, flD, gfs))) {
        flops_data_copy(fl1, fl2);
    }
    return s;
}

void flops_data_diff(flops_t *fl2, flops_t *fl1, flops_t *flD, double *gfs)
{
    return ops.data_diff(fl2, fl1, flD, gfs);
}

void flops_data_copy(flops_t *dst, flops_t *src)
{
    memcpy(dst, src, sizeof(flops_t));
}

void flops_data_print(flops_t *flD, double gfs, int fd)
{
    char buffer[SZ_BUFFER];
    flops_data_tostr(flD, gfs, buffer, SZ_BUFFER);
    dprintf(fd, "%s", buffer);
}

void flops_data_tostr(flops_t *flD, double gfs, char *buffer, size_t length)
{
    int added = 0;
    added += sprintf(&buffer[added], "Total x86 insts f/d: %llu %llu\n", flD->f64, flD->d64);
    added += sprintf(&buffer[added], "Total 128 insts f/d: %llu %llu\n", flD->f128, flD->d128);
    added += sprintf(&buffer[added], "Total 256 insts f/d: %llu %llu\n", flD->f256, flD->d256);
    added += sprintf(&buffer[added], "Total 512 insts f/d: %llu %llu ", flD->f512, flD->d512);
    added += sprintf(&buffer[added], "(%0.2lf GF/s)\n", gfs);
}

void flops_data_accum(flops_t *flA, flops_t *flD, double *gfs)
{
    // flAccum
    if (flD != NULL) {
        // TODO: overflow control
        flA->f64  += flD->f64;
        flA->d64  += flD->d64;
        flA->f128 += flD->f128;
        flA->d128 += flD->d128;
        flA->f256 += flD->f256;
        flA->d256 += flD->d256;
        flA->f512 += flD->f512;
        flA->d512 += flD->d512;
        flA->secs += flD->secs;
        // Individual counters are already weigthed
        flA->gflops = (
            flA->f64  + flA->d64  +
            flA->f128 + flA->d128 +
            flA->f256 + flA->d256 +
            flA->f512 + flA->d512) / flA->secs;
        flA->gflops /= ((double) 1E9);
    }
    if (gfs != NULL) {
        *gfs = flA->gflops;
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
    flops[0] = flD->f64;
    flops[4] = flD->d64;
    flops[1] = flD->f128;
    flops[5] = flD->d128;
    flops[2] = flD->f256;
    flops[6] = flD->d256;
    flops[3] = flD->f512;
    flops[7] = flD->d512;
    debug("flops[0]: %llu", flops[0]);
    debug("flops[1]: %llu", flops[1]);
    debug("flops[2]: %llu", flops[2]);
    debug("flops[3]: %llu", flops[3]);
    debug("flops[4]: %llu", flops[4]);
    debug("flops[5]: %llu", flops[5]);
    debug("flops[6]: %llu", flops[6]);
    debug("flops[7]: %llu", flops[7]);
    return flops;
}

#if TEST2
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

static topology_t tp;
static apinfo_t   info;
static flops_t    t1[16];
static flops_t    t2[16];
static flops_t    tD[16];
static double     tA;
static pid_t      pid;
static int        forked;

static int count_fds()
{
    int dummy_fd = open("/dev/null", O_RDONLY);
    close(dummy_fd);
    return dummy_fd;
}

int main(int argc, char *argv[])
{
    topology_init(&tp);
reload:
    dprintf(STDOUT_FILENO, "%d: Loading... (%d fds)\n", getpid(), count_fds());
    flops_load(&tp, SCOPE_PROCESS | API_FREE);
    flops_get_info(&info);
    apinfo_tostr(&info);
    dprintf(STDOUT_FILENO, "%d: Loaded %s with %d fds\n", getpid(), info.api_str, count_fds());
reread:
    flops_read(t1);
    sleep(3);
    int i, j;
    for (i = j = 0; i < 1000000; ++i) {
        j += 1;
    }
    flops_read_copy(t2, t1, tD, &tA);
    dprintf(STDOUT_FILENO, "%d Printing...\n", getpid());
    flops_data_print(tD, tA, STDOUT_FILENO);
    if (!forked) {
        sleep(1);
        pid = fork();
        forked = 1;
        if (pid == 0) {
            sleep(1);
            dprintf(STDOUT_FILENO, "%d: Unloading...\n", getpid());
            flops_unload();
            goto reload;
        }
    }
    goto reread;
    return 0;
}
#endif