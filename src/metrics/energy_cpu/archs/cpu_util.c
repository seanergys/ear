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

#include <errno.h>
#include <stdlib.h>
#include <common/hardware/topology.h>
#include <metrics/energy_cpu/archs/dummy.h>
#include <common/system/time.h>

static uint socket_count;
static ulong   freq_base;
static topology_t cpu_util_topo;
static timestamp last;
state_t energy_cpu_dummy_load(topology_t *topo)
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
	return EAR_SUCCESS;
}

state_t energy_cpu_dummy_init(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t energy_cpu_dummy_dispose(ctx_t *c)
{
	return EAR_SUCCESS;
}

// Data
state_t energy_cpu_dummy_count_devices(ctx_t *c, uint *count)
{
	if (count != NULL) {
		*count = socket_count;
	}
	return EAR_SUCCESS;
}

// Getters
state_t energy_cpu_dummy_read(ctx_t *c, ullong *values)
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
	ulong curr, total;

	timestamp_getfast(&curr);
	elapsed = timestamp_diff(&curr, &last,TIME_MSECS);

	loadavg(&min,&Vmin,&XVmin,&runnable,&total,&lastpid);

	ratio = (float)cpu_util_topo.cpu_count / min;
	float CPU_Power = cpu_util_topo.TDP_cpu * ear_min(1.0, ratio);
	
	ullong CPU_energy = CPU_Power * elapsed;

	last = curr;

	return EAR_SUCCESS;
}

