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

//#define SHOW_DEBUGS 1
#include <pthread.h>
#include <common/output/debug.h>
#include <metrics/cache/cache.h>
#include <metrics/cache/archs/perf.h>
#include <metrics/cache/archs/dummy.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static double line_size;
static cache_ops_t ops;
static uint load;

void cache_load(topology_t *tp, int eard)
{
	while (pthread_mutex_trylock(&lock));
    if (load) {
        goto out;
    }
    cache_perf_load(tp, &ops);
    cache_dummy_load(tp, &ops);
    // Getting cache line, is used to get the bandwidth
    line_size = (double) tp->cache_line_size;
	load = 1;
out:
	pthread_mutex_unlock(&lock);
	return EAR_SUCCESS;
}

void cache_get_api(uint *api)
{
    *api = API_NONE;
    return ops.get_api(api);
}

void cache_get_granularity(uint *granularity)
{
    *granularity = GRANULARITY_NONE;
    return ops.get_granularity(granularity);
}

state_t cache_init(ctx_t *c)
{
	while (pthread_mutex_trylock(&lock));
	state_t s = ops.init(c);
	pthread_mutex_unlock(&lock);
	return s;
}

state_t cache_dispose(ctx_t *c)
{
	return ops.dispose(c);
}

state_t cache_read(ctx_t *c, cache_t *ca)
{
	return ops.read(c, ca);
}

state_t cache_read_diff(ctx_t *c, cache_t *ca2, cache_t *ca1, cache_t *caD, double *gbs)
{
	state_t s;
	if (state_fail(s = cache_read(c, ca2))) {
		return s;
	}
	cache_data_diff(ca2, ca1, caD, gbs);
    return s;
}

state_t cache_read_copy(ctx_t *c, cache_t *ca2, cache_t *ca1, cache_t *caD, double *gbs)
{
	state_t s;
	if (state_fail(s = cache_read_diff(c, ca2, ca1, caD, gbs))) {
		return s;
	}
	cache_data_copy(ca1, ca2);
    return s;
}

void cache_data_diff(cache_t *ca2, cache_t *ca1, cache_t *caD, double *gbs)
{
    ullong time = 0LLU;
    double secs = 0.0;
    double cas  = 0.0;

    memset(caD, 0, sizeof(cache_t));
    // Ms to seconds (for decimals)
    time = timestamp_diff(&ca2->time, &ca1->time, TIME_MSECS);
    if (time == 0LLU) {
        return;
    }
    secs = (double) time;
    secs = secs / 1000.0;
    //
    caD->l1d_misses = overflow_zeros_u64(ca2->l1d_misses, ca1->l1d_misses);
    caD->l2_misses  = overflow_zeros_u64(ca2->l2_misses , ca1->l2_misses );
    caD->llr_misses = overflow_zeros_u64(ca2->llr_misses, ca1->llr_misses);
    caD->llw_misses = overflow_zeros_u64(ca2->llw_misses, ca1->llw_misses);
    caD->ll_misses  = overflow_zeros_u64(ca2->ll_misses , ca1->ll_misses );
    debug("Total L1D misses: %lld", caD->l1d_misses);
    debug("Total L2  misses: %lld", caD->l2_misses);
    debug("Total LLR misses: %lld", caD->l3_misses);
    debug("Total LLD misses: %lld", caD->l4_misses);
    debug("Total LL  misses: %lld", caD->ll_misses);
    cas = (double) caD->ll_misses;
    cas = (cas / secs);
    cas = (cas * line_size);
    cas = (cas / B_TO_GB);
    *gbs = cas;
}

void cache_data_copy(cache_t *dst, cache_t *src)
{
    memcpy(dst, src, sizeof(cache_t));
}

void cache_data_print(cache_t *ca, double gbs, int fd)
{
    char buffer[SZ_BUFFER];
    cache_dummy_data_tostr(ca, gbs, buffer, SZ_BUFFER);
    dprintf(fd, "%s", buffer);
}

void cache_data_tostr(cache_t *ca, double gbs, char *buffer, size_t length)
{
    snprintf(buffer, length,
         "L1D misses: %llu\nL2 misses : %llu\nL3 misses : %llu (%0.2lf GB/s)\n",
         ca->l1d_misses, ca->l2_misses, ca->l3_misses, gbs);
}