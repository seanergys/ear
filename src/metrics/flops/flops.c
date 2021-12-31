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
* This file is licensed under both the BSD-3 license for individual/non-commercial
* use and EPL-1.0 license for commercial use. Full text of both licenses can be
* found in COPYING.BSD and COPYING.EPL files.
*/

#include <pthread.h>
// #define SHOW_DEBUGS 1
#include <common/output/debug.h>
#include <metrics/flops/flops.h>
#include <metrics/flops/archs/perf.h>
#include <metrics/flops/archs/dummy.h>

#define FLOPS_EVENTS 8

#define NULL_PTR(ptr)\
    if (ptr == NULL)\
        return EAR_ERROR;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static flops_ops_t ops;
static uint init;
static uint api;

// Helper functions to get a single flop event
static state_t flops_get_f64( flops_t *flops_st, ullong *flop_event);
static state_t flops_get_f128(flops_t *flops_st, ullong *flop_event);
static state_t flops_get_f256(flops_t *flops_st, ullong *flop_event);
static state_t flops_get_f512(flops_t *flops_st, ullong *flop_event);
static state_t flops_get_d64( flops_t *flops_st, ullong *flop_event);
static state_t flops_get_d128(flops_t *flops_st, ullong *flop_event);
static state_t flops_get_d256(flops_t *flops_st, ullong *flop_event);
static state_t flops_get_d512(flops_t *flops_st, ullong *flop_event);

// Declare the function type which gets some flops_t pointer and fills
// an unsigned long long pointer with a specific flop event
typedef state_t (*flops_get_event_func)(flops_t *, ullong *);

// This array maps the above helper functions
static flops_get_event_func flops_event_mapper[FLOPS_EVENTS] = {
    &flops_get_f64,
    &flops_get_f128,
    &flops_get_f256,
    &flops_get_f512,
    &flops_get_d64,
    &flops_get_d128,
    &flops_get_d256,
    &flops_get_d512
};


state_t flops_load(topology_t *tp, int eard)
{
	while (pthread_mutex_trylock(&lock));
	// Already initialized
	if (api != API_NONE) {
		return EAR_SUCCESS;
	}
	api = API_DUMMY;
	if (state_ok(flops_perf_load(tp, &ops))) {
		api = API_PERF;
	}
	flops_dummy_load(tp, &ops);
	init = 1;

	pthread_mutex_unlock(&lock);
	return EAR_SUCCESS;
}

state_t flops_get_api(uint *api_in)
{
	*api_in = api;
	return EAR_SUCCESS;
}

state_t flops_init(ctx_t *c)
{
	while (pthread_mutex_trylock(&lock));
	state_t s = ops.init(c);
	pthread_mutex_unlock(&lock);
	return s;
}

state_t flops_dispose(ctx_t *c)
{
	return ops.dispose(c);
}

state_t flops_read(ctx_t *c, flops_t *fl)
{
	return ops.read(c, fl);
}

// Helpers
state_t flops_read_diff(ctx_t *c, flops_t *fl2, flops_t *fl1, flops_t *flD, double *gfs)
{
	state_t s;
	if (state_fail(s = ops.read(c, fl2))) {
		return s;
	}
	return ops.data_diff(fl2, fl1, flD, gfs);
}

state_t flops_read_copy(ctx_t *c, flops_t *fl2, flops_t *fl1, flops_t *flD, double *gfs)
{
	state_t s;
	if (state_fail(s = flops_read_diff(c, fl2, fl1, flD, gfs))) {
		return s;
	}
	return ops.data_copy(fl2, fl1);
}

state_t flops_data_diff(flops_t *fl2, flops_t *fl1, flops_t *flD, double *gfs)
{
	return ops.data_diff(fl2, fl1, flD, gfs);
}

state_t flops_data_copy(flops_t *dst, flops_t *src)
{
	return ops.data_copy(dst, src);
}

state_t flops_data_print(flops_t *flD, double gfs, int fd)
{
	return ops.data_print(flD, gfs, fd);
}

state_t flops_data_tostr(flops_t *flD, double gfs, char *buffer, size_t length)
{
	return ops.data_tostr(flD, gfs, buffer, length);
}


static state_t flops_get_f64(flops_t *flops_st,  ullong *flop_event)
{
    NULL_PTR(flops_st);

    debug("flops_get_f64");

    *flop_event = flops_st->f64;

    return EAR_SUCCESS;
}

static state_t flops_get_f128(flops_t *flops_st, ullong *flop_event)
{
    NULL_PTR(flops_st);

    debug("flops_get_f128");

    *flop_event = flops_st->f128;

    return EAR_SUCCESS;
}

static state_t flops_get_f256(flops_t *flops_st, ullong *flop_event)
{
    NULL_PTR(flops_st);

    debug("flops_get_f256");

    *flop_event = flops_st->f256;

    return EAR_SUCCESS;
}

static state_t flops_get_f512(flops_t *flops_st, ullong *flop_event)
{
    NULL_PTR(flops_st);

    debug("flops_get_f512");

    *flop_event = flops_st->f512;

    return EAR_SUCCESS;
}

static state_t flops_get_d64(flops_t *flops_st,  ullong *flop_event)
{
    NULL_PTR(flops_st);

    *flop_event = flops_st->d64;

    debug("flops_get_d64");

    return EAR_SUCCESS;
}

static state_t flops_get_d128(flops_t *flops_st, ullong *flop_event)
{
    NULL_PTR(flops_st);

    *flop_event = flops_st->d128;

    debug("flops_get_d128");

    return EAR_SUCCESS;
}

static state_t flops_get_d256(flops_t *flops_st, ullong *flop_event)
{
    NULL_PTR(flops_st);

    debug("flops_get_d256");

    *flop_event = flops_st->d256;

    return EAR_SUCCESS;
}

static state_t flops_get_d512(flops_t *flops_st, ullong *flop_event)
{
    NULL_PTR(flops_st);

    debug("flops_get_d512");

    *flop_event = flops_st->d512;

    return EAR_SUCCESS;
}

state_t flops_map_idx_event(flops_t *fl, uint idx, ullong *fl_event)
{
    if (idx >= FLOPS_EVENTS)
        return EAR_ERROR;

    debug("flops_map_idx_event");

    return flops_event_mapper[idx](fl, fl_event);
}

/* Legacy */

int get_number_fops_events()
{
	return 1;
}

void get_total_fops(llong *flops)
{
	*flops = 1;
}

void get_weigth_fops_instructions(int *weights)
{
	*weights = 1;
}
