/*
*
* This program is part of the EAR software.
*
* EAR provides a dynamic, transparent and ligth-weigth solution for
* Energy management. It has been developed in the context of the
* Barcelona Supercomputing Center (BSC)&Lenovo Collaboration project.
*
* 
* BSC Contact   mailto:ear-support@bsc.es
* 
*
* EAR is an open source software, and it is licensed under both the BSD-3 license
* and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
* and COPYING.EPL files.
*/

//#define SHOW_DEBUGS 1

#include <stdlib.h>
#include <common/output/debug.h>
#include <metrics/cpufreq/archs/dummy.h>
#include <metrics/cpufreq/cpufreq_base.h>

static uint cpu_count;
static cfb_t bf;

state_t cpufreq_dummy_status(topology_t *tp, cpufreq_ops_t *ops)
{
    cpu_count = tp->cpu_count;
    // Getting base frequency and boost
    cpufreq_base_init(tp, &bf);
		debug("cpufreq_dummy_status base freq %lu", bf.frequency);

	replace_ops(ops->init,          cpufreq_dummy_init);
	replace_ops(ops->dispose,       cpufreq_dummy_dispose);
	replace_ops(ops->count_devices, cpufreq_dummy_count_devices);
	replace_ops(ops->read,          cpufreq_dummy_read);
	replace_ops(ops->data_diff,     cpufreq_dummy_data_diff);
	return EAR_SUCCESS;
}

state_t cpufreq_dummy_init(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t cpufreq_dummy_dispose(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t cpufreq_dummy_count_devices(ctx_t *c, uint *cpu_count_in)
{
	*cpu_count_in = cpu_count;
	return EAR_SUCCESS;
}

state_t cpufreq_dummy_read(ctx_t *c, cpufreq_t *f)
{
	int cpu;
	debug("cpufreq_dummy_readi cpus=%d", cpu_count);
	for (cpu = 0; cpu < cpu_count; ++cpu) {
		f[cpu].freq_aperf = bf.frequency;
		f[cpu].freq_mperf = bf.frequency;
		f[cpu].error = 0;
	}
	return EAR_SUCCESS;
}

state_t cpufreq_dummy_data_diff(cpufreq_t *f2, cpufreq_t *f1, ulong *freqs, ulong *average)
{
    uint c;
	if (freqs != NULL) {
		memset(freqs, 0, cpu_count * sizeof(ulong));
		for (c = 0; c < cpu_count; c++){
			freqs[c] = bf.frequency;
		}
	}
	if (average != NULL) {
		*average = bf.frequency;
	}
	debug("Average cpufreq %lu", *average);
	return EAR_SUCCESS;
}
