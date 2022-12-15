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

#include <common/system/time.h>
#include <common/output/debug.h>
#include <metrics/common/perf.h>
#include <metrics/cache/archs/perf.h>

#define HW_CACHE_L1D  PERF_COUNT_HW_CACHE_L1D
#define HW_CACHE_LL   PERF_COUNT_HW_CACHE_LL
#define HW_OP_READ    PERF_COUNT_HW_CACHE_OP_READ
#define HW_OP_WRITE   PERF_COUNT_HW_CACHE_OP_WRITE
#define HW_RES_MISS   PERF_COUNT_HW_CACHE_RESULT_MISS

// 0   1  2   3   4
// L1D L2 LLR LLW LL
// It seems that you can't mix OP_READ and OP_WRITE.
static ullong gen_l1d_ev   = HW_CACHE_L1D | (HW_OP_READ  << 8) | (HW_RES_MISS << 16);
static ullong gen_llr_ev   = HW_CACHE_LL  | (HW_OP_READ  << 8) | (HW_RES_MISS << 16);
static ullong gen_llw_ev   = HW_CACHE_LL  | (HW_OP_WRITE << 8) | (HW_RES_MISS << 16);
static ullong hwell_l1d_ev = gen_l1d_ev;
static ullong hwell_l2_ev  = 0x2724;
static ullong hwell_llr_ev = gen_llr_ev;
static ullong hwell_llw_ev = gen_llw_ev;
static ullong zen_l1d_ev   = gen_l1d_ev;
static ullong zen_llr_ev   = gen_llr_ev;
static ullong zen_llw_ev   = gen_llw_ev;
static ullong arm_l1d_ev   = gen_l1d_ev;
static ullong arm_llr_ev   = gen_llr_ev;
static ullong arm_llw_ev   = gen_llw_ev;
//
static perf_t perfs[9];
static uint levels[9];
static uint counter = 0;
static uint started;

void set_cache(ullong event, uint type, uint level)
{
	state_t s;
	if (state_fail(s = perf_open(&perfs[counter], &perfs[0], 0, type, event))) {
		debug("perf_open for level %d returned %d (%s)", level, s, state_msg);
		return;
	}
	levels[counter] = level;
	counter++;
}

void cache_perf_load(topology_t *tp, cache_ops_t *ops)
{
	if (tp->vendor == VENDOR_INTEL && tp->model >= MODEL_HASWELL_X) {
		set_cache(hwell_l1d_ev, PERF_TYPE_HW_CACHE, 0);
		set_cache(hwell_l2_ev , PERF_TYPE_RAW     , 1);
		set_cache(hwell_llr_ev, PERF_TYPE_HW_CACHE, 2);
        set_cache(hwell_llw_ev, PERF_TYPE_HW_CACHE, 3);
	}
	else if (tp->vendor == VENDOR_AMD && tp->family >= FAMILY_ZEN) {
		set_cache(zen_l1d_ev, PERF_TYPE_HW_CACHE, 0);
		set_cache(zen_llr_ev, PERF_TYPE_HW_CACHE, 2);
        set_cache(zen_llw_ev, PERF_TYPE_HW_CACHE, 3);
	}
    else if (tp->vendor == VENDOR_ARM) {
        set_cache(arm_l1d_ev, PERF_TYPE_HW_CACHE, 0);
        set_cache(arm_llr_ev, PERF_TYPE_HW_CACHE, 2);
        set_cache(arm_llw_ev, PERF_TYPE_HW_CACHE, 3);
    }
	else {
		debug(Generr.api_incompatible);
        return;
	}
    if (!counter) {
        debug(Generr.api_incompatible);
        return;
    }
    apis_put(ops->get_api,         cache_perf_get_api);
    apis_put(ops->get_granularity, cache_perf_get_granularity);
    apis_put(ops->init,            cache_perf_init);
    apis_put(ops->dispose,         cache_perf_dispose);
    apis_put(ops->read,            cache_perf_read);
    debug("Loaded PERF")
}

void cache_perf_get_api(uint *api)
{
    *api = API_PERF;
}

void cache_perf_get_granularity(uint *granularity)
{
    *granularity = GRANULARITY_PROCESS;
}

state_t cache_perf_init(ctx_t *c)
{
	if (!started) {
		perf_start(&perfs[0]);
		++started;
	}
	return EAR_SUCCESS;
}

state_t cache_perf_dispose(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t cache_perf_read(ctx_t *c, cache_t *ca)
{
    ullong *p = (ullong *) &ca->l1d_misses;
	llong values[9];
	state_t s;
	int i;

    memset(values, 0, sizeof(cache_t));
	if (state_fail(s = perf_read(&perfs[0], values))) {
		return s;
	}
	timestamp_get(&ca->time);
	for (i = 0; i < counter; ++i) {
        p[levels[i]] = values[i];
    }
    if (ca->ll_misses == 0) {
        ca->ll_misses = ca->llr_misses + ca->llw_misses;
    }
    debug("total L1D misses: %llu", ca->l1d_misses);
	debug("total L2  misses: %llu", ca->l2_misses );
    debug("total LLR misses: %llu", ca->llr_misses);
    debug("total LLW misses: %llu", ca->llw_misses);
    debug("total LL  misses: %llu", ca->ll_misses );

	return EAR_SUCCESS;
}