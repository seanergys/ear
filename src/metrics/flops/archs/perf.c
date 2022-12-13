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
#include <metrics/flops/archs/perf.h>

static ullong hwell_evs[8]  = { 0x02c7, 0x01c7, 0x08c7, 0x04c7, 0x20c7, 0x10c7, 0x80c7, 0x40c7 };
static ullong zen_evs[8]    = { 0xff03 }; // Old event: 0x04cb
static ullong fx1000_evs[3] = { 0x0074, 0x80c2, 0x80c4, 0x80c6 }; // 128SP, 512HP, 512SP, 512DP
static perf_t perfs[8];
static uint   levels[8];
static ullong divisors[8];
static uint   counter;
static uint   started;

void set_flops(ullong event, uint type, uint level, ullong divisor)
{
	state_t s;

	if (state_fail(s = perf_open(&perfs[counter], NULL, 0, type, (ulong) event))) {
		debug("perf_open returned %d (%s)", s, state_msg);
		return;
	}
    divisors[counter] = divisor;
	levels[counter]   = level;
	counter++;
}

state_t flops_perf_load(topology_t *tp, flops_ops_t *ops)
{
	if (tp->vendor == VENDOR_INTEL && tp->model >= MODEL_HASWELL_X) {
		set_flops(hwell_evs[0], PERF_TYPE_RAW, 1, 1);
		set_flops(hwell_evs[1], PERF_TYPE_RAW, 2, 1);
		set_flops(hwell_evs[2], PERF_TYPE_RAW, 3, 1);
		set_flops(hwell_evs[3], PERF_TYPE_RAW, 4, 1);
		set_flops(hwell_evs[4], PERF_TYPE_RAW, 5, 1);
		set_flops(hwell_evs[5], PERF_TYPE_RAW, 6, 1);
		set_flops(hwell_evs[6], PERF_TYPE_RAW, 7, 1);
		set_flops(hwell_evs[7], PERF_TYPE_RAW, 8, 1);
	}
	else if (tp->vendor == VENDOR_AMD && tp->family >= FAMILY_ZEN) {
		set_flops(zen_evs[0], PERF_TYPE_RAW, 4, 2);
	}
    else if (tp->vendor == VENDOR_ARM) { // && tp->model == FUJI_FX1000) {
        // It counts also the 16 bits operations, but we don't have 16 bits
        // field in the flops_t structure, so we are saving it in 32 bits field.
        set_flops(fx1000_evs[0], PERF_TYPE_RAW, 3, 1);
        set_flops(fx1000_evs[0], PERF_TYPE_RAW, 7, 8); // Its is divided to
        set_flops(fx1000_evs[1], PERF_TYPE_RAW, 7, 4); // match with the tests
        set_flops(fx1000_evs[2], PERF_TYPE_RAW, 8, 2); // iterations.
    } else {
		return_msg(EAR_ERROR, Generr.api_incompatible);
	}
    if (!counter) {
        return EAR_ERROR;
    }
	replace_ops(ops->init,    flops_perf_init);
	replace_ops(ops->dispose, flops_perf_dispose);
	replace_ops(ops->read,    flops_perf_read);

	return EAR_SUCCESS;
}

state_t flops_perf_init(ctx_t *c)
{
	uint i;
	if (!started) {
		for (i = 0; i < counter; ++i) {
			perf_start(&perfs[i]);
		}
		++started;
	}
	return EAR_SUCCESS;
}

state_t flops_perf_dispose(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t flops_perf_read(ctx_t *c, flops_t *fl)
{
	ullong *p = (ullong *) fl;
	ullong value;
	int i;

	memset(fl, 0, sizeof(flops_t));
	timestamp_get(&fl->time);
	for (i = 0; i < counter; ++i) {
		value = 0LU;
		perf_read(&perfs[i], (long long *)&value);
		debug("values[%d]: %lu", i, value);
		p[levels[i]-1] = value / divisor;
	}
	return EAR_SUCCESS;
}
