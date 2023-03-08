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

#define PERF_L1D     PERF_COUNT_HW_CACHE_L1D
#define PERF_LL      PERF_COUNT_HW_CACHE_LL
#define PERF_LOAD    PERF_COUNT_HW_CACHE_OP_READ
#define PERF_STORE   PERF_COUNT_HW_CACHE_OP_WRITE
#define PERF_MISS    PERF_COUNT_HW_CACHE_RESULT_MISS

// It seems that you can't mix OP_READ and OP_WRITE.
// 
// Skylake:
//   - It has a victim cache, so most of the data avoids the L3, passing directly
//     from L2 to DRAM. (Look LIKWID Github: L2-L3-MEM-traffic-on-Intel-Skylake)
//   - Intel don't offer a a PERF_STORE event for L1D cache.
static ullong hwl_l1d = PERF_L1D | (PERF_LOAD  << 8) | (PERF_MISS << 16);
static ullong hwl_l2  = 0x3f24; // L2_RQSTS.MISS (All requests that missed L2)
static ullong zen_l1d = PERF_L1D | (PERF_LOAD  << 8) | (PERF_MISS << 16);
static ullong zen_l2x = 0x430864; // L2 Cache Miss from DC Miss
static ullong zen_l2y = 0x431F71; // L2 Cache Miss from L2 HWPF (1)
static ullong zen_l2z = 0x431F72; // L2 Cache Miss from L2 HWPF (2)
static ullong arm_l1d = PERF_L1D | (PERF_LOAD  << 8) | (PERF_MISS << 16);
static ullong arm_l2x = 0x17; //L2D_CACHE_REFILL
static ullong arm_l2y = 0x18; //L2D_CACHE_WB

typedef struct cdata_s {
    perf_t perf;
    int    offset;
    int    bandwidth;
} cdata_t;

static topology_t *tp;
static int         is_level[5]; // Sorted by cache level
static cdata_t     cdata[5];
static uint        counter;
static uint        started;
static uint        cache_last_level;
static double      line_size;

int set_cache(uint enable, ullong event, uint type, uint offset, uint bandwidth)
{
	state_t s;
    if (!enable) {
        return 0;
    }
	if (state_fail(s = perf_open(&cdata[counter].perf, &cdata[0].perf, 0, type, event))) {
		debug("perf_open for level %d returned %d (%s)", offset, s, state_msg);
		return 0;
	}
    cdata[counter].bandwidth = bandwidth;
    cdata[counter].offset = offset;
    is_level[offset] = 1;
	counter++;
    return 1;
}

int OFFSET(int level)
{
    #define LBW INT_MAX-0
    #define LL  INT_MAX-1
    #define LX  INT_MAX-2
    // Based on cache_t structure:
    if (level == LBW) return  0;
    if (level ==   1) return  1;
    if (level ==   2) return  2;
    if (level ==   3) return  3;
    if (level ==   4) return -1;
    if (level ==  LL) return  4;
    if (level ==  LX) return OFFSET(cache_last_level);
    return -1;
}

void cache_perf_load(topology_t *tp, cache_ops_t *ops)
{
    // Saving the last cache level (our LX)
    cache_last_level = tp->cache_last_level;
    line_size = (double) tp->cache_line_size;
    //
	if (tp->vendor == VENDOR_INTEL && tp->model >= MODEL_HASWELL_X) {
		set_cache(1, hwl_l1d, PERF_TYPE_HW_CACHE, OFFSET(01), 0);
		set_cache(1, hwl_l2 , PERF_TYPE_RAW     , OFFSET(02), 1);
	} else if (tp->vendor == VENDOR_AMD && tp->family >= FAMILY_ZEN) {
		set_cache(1, zen_l1d, PERF_TYPE_HW_CACHE, OFFSET(01), 0);
        set_cache(1, zen_l2x, PERF_TYPE_RAW     , OFFSET(02), 1);
        set_cache(1, zen_l2y, PERF_TYPE_RAW     , OFFSET(02), 1);
        set_cache(1, zen_l2z, PERF_TYPE_RAW     , OFFSET(02), 1);
	} else if (tp->vendor == VENDOR_ARM) {
        set_cache(1, arm_l1d, PERF_TYPE_HW_CACHE, OFFSET(01), 0);
        set_cache(1, arm_l2x, PERF_TYPE_RAW     , OFFSET(02), 1);
        set_cache(1, arm_l2y, PERF_TYPE_RAW     , OFFSET(02), 1);
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
    apis_put(ops->data_diff,       cache_perf_data_diff);
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

state_t cache_perf_init()
{
	if (!started) {
		perf_start(&cdata[0].perf);
		++started;
	}
	return EAR_SUCCESS;
}

state_t cache_perf_dispose()
{
	return EAR_SUCCESS;
}

state_t cache_perf_read(cache_t *ca)
{
    ullong *p = (ullong *) ca;
	state_t s;
	int i;
    // Cleaning
    memset(ca, 0, sizeof(cache_t));
    // Reading
	if (state_fail(s = perf_read(&cdata[0].perf, p))) {
		return s;
	}
	timestamp_get(&ca->time);
	for (i = 0; i < counter; ++i) {
        debug("values[%d]: %llu (%u offset)", i, p[i]);
    }
    return EAR_SUCCESS;
}

void cache_perf_data_diff(cache_t *ca2, cache_t *ca1, cache_t *caD, double *gbs)
{
    ullong *pD = (ullong *) caD;
    ullong *p2 = (ullong *) ca2;
    ullong *p1 = (ullong *) ca1;
    ullong time = 0LLU;
    double secs = 0.0;
    double cas  = 0.0;
    int i;

    memset(caD, 0, sizeof(cache_t));
    // Ms to seconds (for decimals)
    time = timestamp_diff(&ca2->time, &ca1->time, TIME_MSECS);
    if (time == 0LLU) {
        return;
    }
    secs = (double) time;
    secs = secs / 1000.0;
    //
    for (i = 0; i < counter; ++i) {
        pD[cdata[i].offset] += overflow_zeros_u64(p2[i], p1[i]);
        // If this data is also valid to calc bandwidth
        if (cdata[i].bandwidth) {
            caD->lbw_misses = pD[cdata[i].offset];
        }
    }
    // If LX don't have values, but LL have values
    if (!is_level[OFFSET(LX)] && is_level[OFFSET(LL)]) {
        pD[OFFSET(LX)] = caD->ll_misses;
    }
    debug("Total L1D misses: %lld", caD->l1d_misses);
    debug("Total L2  misses: %lld", caD->l2_misses );
    debug("Total L3  misses: %lld", caD->l3_misses );
    debug("Total LL  misses: %lld", caD->ll_misses );
    debug("Total LBW misses: %lld", caD->lbw_misses);
    // Converting to bandwidth
    cas  = (double) caD->lbw_misses;
    cas  = cas / secs;
    cas  = cas * ((double) line_size);
    cas  = cas / ((double) 1E9);
    *gbs = cas;
}