/*
 * *
 * * This program is part of the EAR software.
 * *
 * * EAR provides a dynamic, transparent and ligth-weigth solution for
 * * Energy management. It has been developed in the context of the
 * * Barcelona Supercomputing Center (BSC)&Lenovo Collaboration project.
 * *
 * * Copyright © 2017-present BSC-Lenovo
 * * BSC Contact   mailto:ear-support@bsc.es
 * * Lenovo contact  mailto:hpchelp@lenovo.com
 * *
 * * This file is licensed under both the BSD-3 license for individual/non-commercial
 * * use and EPL-1.0 license for commercial use. Full text of both licenses can be
 * * found in COPYING.BSD and COPYING.EPL files.
 * */
//#define SHOW_DEBUGS 1
#include <errno.h>
#include <stdlib.h>

#include <common/states.h>
#include <common/output/debug.h>

#include <common/hardware/topology.h>
#include <metrics/energy_cpu/archs/dummy.h>
#include <common/system/time.h>
#include <common/system/loadavg.h>

static uint socket_count, cpu_count;
static ulong   freq_base;
static topology_t cpu_util_topo;
static timestamp last;
static ullong *last_energy_reported;
state_t energy_cpu_util_load(topology_t *topo)
{
	if (socket_count == 0) {
		if (topo->socket_count > 0) {
			socket_count = topo->socket_count;
			cpu_count    = topo->cpu_count;
		} else {
			socket_count = 1;
		}
	}
	topology_copy(&cpu_util_topo , topo);
	last_energy_reported = (ullong *)calloc(socket_count, sizeof(ullong));
	return EAR_SUCCESS;
}

state_t energy_cpu_util_init(ctx_t *c)
{
	timestamp_getfast(&last);
	return EAR_SUCCESS;
}

state_t energy_cpu_util_dispose(ctx_t *c)
{
	return EAR_SUCCESS;
}

// Data
state_t energy_cpu_util_count_devices(ctx_t *c, uint *count)
{
	if (count != NULL) {
		*count = socket_count;
	}
	return EAR_SUCCESS;
}

#define CPU_UTIL_ENERGY_UNITS 1000000000

// Getters
state_t energy_cpu_util_read(ctx_t *c, ullong *values)
{
	timestamp curr;
	ulong elapsed;
	if (values != NULL) {
		if (memset((void *) values, 0, NUM_PACKS*sizeof(ullong)*socket_count) != values) {
			return_msg(EAR_ERROR, strerror(errno));
		}
	}
	float min, Vmin, XVmin, ratio;
	uint runnable, total, lastpid;

	timestamp_getfast(&curr);
	elapsed = timestamp_diff(&curr, &last,TIME_SECS);

	loadavg(&min,&Vmin,&XVmin,&runnable,&total,&lastpid);

	debug("Average utilization %f", min);

	ratio = (float)min/cpu_util_topo.cpu_count;
	
	debug("Ratio %f", ratio);
	float CPU_Power = (cpu_util_topo.TDP_cpu * socket_count) * ear_min(1.0, ratio);
	
	ullong CPU_energy = CPU_Power * elapsed * CPU_UTIL_ENERGY_UNITS;

	debug("Estimated power %f energy %llu elapsed %lu ratio %f", CPU_Power, CPU_energy, elapsed, ratio);

	/* DRAM is 0 */
	for (uint s = 0; s < socket_count; s++){
		values[s] = 0;
	}
	/* CPU */
	for (uint s = socket_count ; s < socket_count * 2; s++){
		values[s] = (CPU_energy / socket_count) + last_energy_reported[s - socket_count];
		debug("Energy[%u] = %llu", s, values[ s]);
		/* last_energy_reported is the accumulated energy */
		last_energy_reported[s - socket_count] += values[s];
	}
	

	last = curr;

	return EAR_SUCCESS;
}

