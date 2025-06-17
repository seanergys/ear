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

#include <signal.h>

#include <common/output/debug.h>
#include <common/output/verbose.h>
#include <metrics/common/apis.h>
#include <metrics/imcfreq/archs/amd17.h>
#include <metrics/imcfreq/archs/dummy.h>
#include <metrics/imcfreq/archs/eard.h>
#include <metrics/imcfreq/archs/intel63.h>
#include <metrics/imcfreq/imcfreq.h>
#include <pthread.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static imcfreq_ops_t ops;

void imcfreq_load(topology_t *tp, int options)
{
    while (pthread_mutex_trylock(&lock));
    if (ops.init != NULL) {
        goto done;
    }
    if (API_IS(options, API_DUMMY)) {
        goto dummy;
    }
    debug("loading imcfreq metrics");
    imcfreq_eard_load(tp, &ops, API_IS(options, API_EARD));
    imcfreq_intel63_load(tp, &ops);
    imcfreq_amd17_load(tp, &ops, API_IS(options, API_EARD));
dummy:
    imcfreq_dummy_load(tp, &ops);
done:
    pthread_mutex_unlock(&lock);
    debug("loading imcfreq metrics finalized");
}

void imcfreq_unload()
{
    ops.unload();
}

void temp_get_info(apinfo_t *info)
{
    memset(info, 0, sizeof(apinfo_t));
    ops.get_info(info);
}

state_t imcfreq_read(ctx_t *c, imcfreq_t *i)
{
    preturn(ops.read, c, i);
}

state_t imcfreq_read_diff(ctx_t *c, imcfreq_t *i2, imcfreq_t *i1, ulong *freqs, ulong *average)
{
    state_t s;
    if (state_fail(s = imcfreq_read(c, i2))) {
        return s;
    }
    return imcfreq_data_diff(i2, i1, freqs, average);
}

state_t imcfreq_read_copy(ctx_t *c, imcfreq_t *i2, imcfreq_t *i1, ulong *freqs, ulong *average)
{
    state_t s;
    if (state_fail(s = imcfreq_read_diff(c, i2, i1, freqs, average))) {
        return s;
    }
    return imcfreq_data_copy(i1, i2);
}

void imcfreq_data_diff(imcfreq_t *i2, imcfreq_t *i1, ulong *freq_list, ulong *average)
{
    preturn(ops.data_diff, i2, i1, freq_list, average);
}

void imcfreq_data_alloc(imcfreq_t **i, ulong **freq_list)
{
    preturn(ops.data_alloc, i, freq_list);
}

void imcfreq_data_free(imcfreq_t **i, ulong **freq_list)
{
    preturn(ops.data_free, i, freq_list);
}

void imcfreq_data_copy(imcfreq_t *i2, imcfreq_t *i1)
{
    preturn(ops.data_copy, i2, i1);
}

void imcfreq_data_print(ulong *freq_list, ulong *average, int fd)
{
    ops.data_print(freq_list, average, fd);
}

char *imcfreq_data_tostr(ulong *freq_list, ulong *average, char *buffer, size_t length)
{
    return ops.data_tostr(freq_list, average, buffer, length);
}