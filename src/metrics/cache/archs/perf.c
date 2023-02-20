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

// It seems that you can't mix OP_READ and OP_WRITE.
static ullong hwell_l1d_ev = HW_CACHE_L1D | (HW_OP_READ  << 8) | (HW_RES_MISS << 16);
static ullong hwell_l2_ev  = 0x2724; // L2_RQSTS.ALL_DEMAND_MISS
static ullong hwell_ll_ev  = 0x412e; // LLC Misses (architectural)
static ullong hwell_llr_ev = HW_CACHE_LL  | (HW_OP_READ  << 8) | (HW_RES_MISS << 16);
static ullong hwell_llw_ev = HW_CACHE_LL  | (HW_OP_WRITE << 8) | (HW_RES_MISS << 16);
static ullong zen_l1d_ev   = HW_CACHE_L1D | (HW_OP_READ  << 8) | (HW_RES_MISS << 16);
static ullong zen_llr_ev   = HW_CACHE_LL  | (HW_OP_READ  << 8) | (HW_RES_MISS << 16);
static ullong zen_llw_ev   = HW_CACHE_LL  | (HW_OP_WRITE << 8) | (HW_RES_MISS << 16);
static ullong arm_l1d_ev   = HW_CACHE_L1D | (HW_OP_READ  << 8) | (HW_RES_MISS << 16);
static ullong arm_llr_ev   = HW_CACHE_LL  | (HW_OP_READ  << 8) | (HW_RES_MISS << 16);
static ullong arm_llw_ev   = HW_CACHE_LL  | (HW_OP_WRITE << 8) | (HW_RES_MISS << 16);
//
static topology_t *tp;
static perf_t      perfs [7]; // Sorted indian line
static int         offset[7]; // Sorted indian line
static int         loaded[7]; // Sorted by cache level
static uint        counter;
static uint        started;
static uint        cache_last_level;

int set_cache(uint enable, ullong event, uint type, uint offset_in)
{
	state_t s;
    if (!enable) {
        return 0;
    }
	if (state_fail(s = perf_open(&perfs[counter], &perfs[0], 0, type, event))) {
		debug("perf_open for level %d returned %d (%s)", level, s, state_msg);
		return 0;
	}
    offset[counter] = offset_in;
    loaded[offset]  = 1;
	counter++;
    return 1;
}

int OFFSET(int level)
{
    #define ll  INT_MAX
    #define llr INT_MAX-1
    #define llw INT_MAX-2
    #define lx  INT_MAX-3
    // Based on cache_t structure:
    if (level ==   1) return  0;
    if (level ==   2) return  1;
    if (level ==   3) return  2;
    if (level ==   4) return -1;
    if (level == ll ) return  3;
    if (level == llr) return  4;
    if (level == llw) return  5;
    if (level ==  lx) return offset(cache_last_level);
    return -1;
}

void cache_perf_load(topology_t *tp, cache_ops_t *ops)
{
    // Saving the last cache level (our lx)
    cache_last_level = tp->cache_last_level;
    //
	if (tp->vendor == VENDOR_INTEL && tp->model >= MODEL_HASWELL_X) {
		int l1  = set_cache(   1, hwell_l1d_ev, PERF_TYPE_HW_CACHE, OFFSET(  1));
		int l2  = set_cache(   1, hwell_l2_ev , PERF_TYPE_RAW     , OFFSET(  2));
		int llr = set_cache(   1, hwell_llr_ev, PERF_TYPE_HW_CACHE, OFFSET(llr));
        int llw = set_cache( llr, hwell_llw_ev, PERF_TYPE_HW_CACHE, OFFSET(llw));
        int ll  = set_cache(!llr, hwell_ll_ev , PERF_TYPE_RAW     , OFFSET(ll ));
	}
	else if (tp->vendor == VENDOR_AMD && tp->family >= FAMILY_ZEN) {
		set_cache(1, zen_l1d_ev, PERF_TYPE_HW_CACHE, OFFSET(  1));
		set_cache(1, zen_llr_ev, PERF_TYPE_HW_CACHE, OFFSET(llr));
        set_cache(1, zen_llw_ev, PERF_TYPE_HW_CACHE, OFFSET(llw));
	}
    else if (tp->vendor == VENDOR_ARM) {
        set_cache(1, arm_l1d_ev, PERF_TYPE_HW_CACHE, OFFSET(  1));
        set_cache(1, arm_llr_ev, PERF_TYPE_HW_CACHE, OFFSET(llr));
        set_cache(1, arm_llw_ev, PERF_TYPE_HW_CACHE, OFFSET(llw));
    } else {
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
        p[offset[i]] = values[i];
    }
    // LL: last level as tag ll
    // LX: last level as number 4, 3 or 2.
    int ll_values = loaded[OFFSET(ll)];
    int lx_values = loaded[OFFSET(lx)];
    // If not loaded LL but loaded LLR and LLW, then LL then have values
    if (!loaded[OFFSET(ll)] && loaded[OFFSET(llr)] && loaded[OFFSET(llw)]) {
        ca->ll_misses = ca->llr_misses + ca->llw_misses;
        ll_values = 1;
    }
    // If LL still don't have values, but LX have values
    if (!ll_values && lx_values) {
        ca->ll_misses = p[OFFSET(lx)];
    }
    // If LX don't have values, but LL have values
    if (!lx_values && ll_values) {
        p[OFFSET(lx)] = ca->ll_misses;
    }
    debug("total L1D misses: %llu", ca->l1d_misses);
	debug("total L2  misses: %llu", ca->l2_misses );
    debug("total L3  misses: %llu", ca->l3_misses );
    debug("total LL  misses: %llu", ca->ll_misses );
    debug("total LLR misses: %llu", ca->llr_misses);
    debug("total LLW misses: %llu", ca->llw_misses);

	return EAR_SUCCESS;
}