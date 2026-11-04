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
// #define SHOW_DEBUGS 1
#include <stdlib.h>
#include <common/system/time.h>
#include <common/output/debug.h>
#include <common/math_operations.h>
#include <metrics/common/easybus.h>
#include <metrics/cpufreq/archs/easybus.h>

static easybus_t easybus;
static metric_t *mets;
static uint cpus_count;

CPUFREQ_F_LOAD(easybus)
{
    metric_t *met = NULL;

    if (ops->read != NULL) {
        return;
    }
    if (!easybus_open(&easybus, NULL)) {
        debug("EasyBus (accessd) not connected");
        return;
    }
    if (!easybus.search_avail_metrics(Met, Freq, Cpu, &met)) {
        debug("EasyBus (accessd) metric not available: %s", strerror(errno));
        return;
    }
    apis_put(ops->unload   , cpufreq_easybus_unload);
    apis_put(ops->get_info , cpufreq_easybus_get_info);
    apis_put(ops->read     , cpufreq_easybus_read);
    apis_put(ops->data_diff, cpufreq_easybus_data_diff);
    cpus_count = met->devs_count;
    easybus.metrics_alloc(&mets);
    free(met);
}

CPUFREQ_F_UNLOAD(easybus)
{
    easybus_close();
}

CPUFREQ_F_GET_INFO(easybus)
{
    info->api         = API_EASYBUS;
    info->scope       = SCOPE_NODE;
    info->granularity = GRANULARITY_CPU;
    info->devs_count  = cpus_count;
}

CPUFREQ_F_READ(easybus)
{
    int i;

    if (!easybus.request_get(Met, AccFreq, Cpu, All, &mets, NULL)) {
        return EAR_ERROR;
    }
    f[0].time_ms = timestamp_getconvert(TIME_MSECS);
    for (i = 0; i < cpus_count; i++) {
        f[i].freq_acc = mets[0].values[i];
    }
    return EAR_SUCCESS;
}

CPUFREQ_F_DATA_DIFF(easybus)
{
    uint64_t time_ms = 0;
    uint64_t freq = 0;
    int i;

    time_ms = (uint64_t) overflow_magic_u64(
        (ullong) f2[0].time_ms, (ullong) f1[0].time_ms, MAXBITS64);
    if (time_ms == 0) {
        return;
    }
    for (i = 0; i < cpus_count; i++) {
        freq = (uint64_t) overflow_magic_u64(
            (ullong) f2[i].freq_acc, (ullong) f1[i].freq_acc, MAXBITS64);
        debug("%lu - %lu = %lu in (%lu ms)", f2[i].freq_acc, f1[i].freq_acc, freq, time_ms);
        freq = (freq * 1000) / time_ms;
        if (freqs    != NULL)  freqs[i]  = freq;
        if (freq_avg != NULL) *freq_avg += freq;
    }
    if (freq_avg != NULL && cpus_count > 0) {
        *freq_avg = *freq_avg / cpus_count;
    }
}