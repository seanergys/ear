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

#include <management/cpupow/archs/amd17.h>
#include <management/cpupow/archs/dummy.h>
#include <management/cpupow/archs/intel63.h>
#include <management/cpupow/cpupow.h>
#include <pthread.h>
#include <stdlib.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static uint loaded;
static mgt_cpupow_ops_t ops;
static uint *inner_watts;
static int devs_count; // #Threads

void mgt_cpupow_load(topology_t *tp, int force_api)
{
    while (pthread_mutex_trylock(&lock))
        ;
    if (loaded) {
        goto ret;
    }
    if (API_IS(force_api, API_DUMMY)) {
        goto dummy;
    }
    mgt_cpupow_intel63_load(tp, &ops);
    mgt_cpupow_amd17_load(tp, &ops);
dummy:
    mgt_cpupow_dummy_load(tp, &ops);
    loaded = 1;
    // To add quality of life functions
    devs_count  = tp->cpu_count;
    inner_watts = calloc(devs_count, sizeof(uint));
ret:
    pthread_mutex_unlock(&lock);
}

void mgt_cpupow_get_info(apinfo_t *info)
{
    info->layer = "MGT_CPUPOW";
    return ops.get_info(info);
}

int mgt_cpupow_count_devices(int domain)
{
    return ops.count_devices(domain);
}

int mgt_cpupow_powercap_is_capable(int domain)
{
    return ops.powercap_is_capable(domain);
}

state_t mgt_cpupow_powercap_is_enabled(int domain, uint *enabled)
{
    return ops.powercap_is_enabled(domain, enabled);
}

state_t mgt_cpupow_powercap_get(int domain, uint *watts)
{
    return ops.powercap_get(domain, watts);
}

state_t mgt_cpupow_powercap_get_dev(int domain, int dev, uint *watts)
{
    while (pthread_mutex_trylock(&lock))
        ;
    // Memset is not required, is done inside.
    state_t s = mgt_cpupow_powercap_get(domain, inner_watts);
    *watts    = inner_watts[dev];
    pthread_mutex_unlock(&lock);
    return s;
}

state_t mgt_cpupow_powercap_set(int domain, uint *watts)
{
    return ops.powercap_set(domain, watts);
}

state_t mgt_cpupow_powercap_set_dev(int domain, int dev, uint watts)
{
    while (pthread_mutex_trylock(&lock))
        ;
    if (dev != all_devs) {
        memset(inner_watts, watts, sizeof(uint) * devs_count);
    } else {
        memset(inner_watts, POWERCAP_DO_NOTHING, sizeof(uint) * devs_count);
        inner_watts[dev] = watts;
    }
    state_t s = mgt_cpupow_powercap_set(domain, inner_watts);
    pthread_mutex_unlock(&lock);
    return s;
}

state_t mgt_cpupow_powercap_reset(int domain)
{
    return ops.powercap_reset(domain);
}

state_t mgt_cpupow_tdp_get(int domain, uint *watts)
{
    return ops.tdp_get(domain, watts);
}

void mgt_cpupow_tdp_tostr(int domain, uint *watts, char *buffer, int length)
{
    int acc, i;
    for (acc = i = 0; i < mgt_cpupow_count_devices(domain); ++i) {
        acc += sprintf(&buffer[acc], "TDP%d: %u\n", i, watts[i]);
    }
}

void mgt_cpupow_data_alloc(int domain, uint **list)
{
    int count = mgt_cpupow_count_devices(domain);
    *list     = calloc(count, sizeof(uint));
}