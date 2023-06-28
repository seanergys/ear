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

#define SHOW_DEBUGS 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <common/output/debug.h>
#include <metrics/common/msr.h>
#include <metrics/imcfreq/archs/intel63.h>

#define GLBL_CTL_OFS    0x000700 // U_MSR_PMON_GLOBAL_CTL
#define GLBL_CMD_UNF    0x2000000000000000
#define UBOX_CMD_STA	0x400000 //
#define UBOX_CMD_STO	0x000000
#define SHIFT           22

// In this capacity, the UBox acts as the central unit for a variety of functions.
// Means just one register per socket?
// See 'Additional IMC Performance Monitoring' in 'Intel Xeon Processor Scalable
// Memory Family Uncore Performance Monitoring Reference Manual'

static off_t address_ctl;
static off_t address_ctr;
static topology_t tp;

void imcfreq_intel63_load(topology_t *tp_in, imcfreq_ops_t *ops)
{
    if (state_fail(imcfreq_intel63_ext_load_addresses(tp_in))) {
        return;
    }
    if (state_fail(msr_test(tp_in, MSR_WR))) {
        return;
    }
    if(state_fail(topology_select(tp_in, &tp, TPSelect.socket, TPGroup.merge, 0))) {
        return;
    }
    apis_set(ops->get_api,       imcfreq_intel63_get_api);
    apis_set(ops->init,          imcfreq_intel63_init);
    apis_set(ops->dispose,       imcfreq_intel63_dispose);
    apis_set(ops->count_devices, imcfreq_intel63_count_devices);
    apis_set(ops->read,          imcfreq_intel63_read);
    debug("INTEL63 loaded full API");
}

void imcfreq_intel63_get_api(uint *api, uint *api_intern)
{
    *api = API_INTEL63;
    if (api_intern) {
        *api_intern = API_NONE;
    }
}

state_t imcfreq_intel63_ext_load_addresses(topology_t *tp)
{
    if (tp->vendor != VENDOR_INTEL) {
        debug("Detected vendor is not Intel");
        return EAR_ERROR;
    }
    // We don't know the details in Tiger Lake. Maybe the same SR addresses?
    if (tp->model == MODEL_TIGERLAKE) {
        debug("Detected vendor is Tiger Lake");
        return EAR_ERROR;
    }
    if (tp->model >= MODEL_SAPPHIRE_RAPIDS) {
        debug("Detected vendor is Sapphire Rapids");
        address_ctl = 0x2FDE; // UCLK Counter Control
        address_ctr = 0x2FDF; // UCLK Counter
    } else {
        debug("Detected vendor is Haswell or greater");
        address_ctl = 0x0703; // U_MSR_PMON_FIXED_CTL
        address_ctr = 0x0704; // U_MSR_PMON_FIXED_CTR
    }
    return EAR_SUCCESS;
}

state_t imcfreq_intel63_ext_enable_cpu(int cpu)
{
    ullong com, com1;
    state_t s;
    // Unfreeze
    if (state_fail(s = msr_open(cpu, MSR_WR))) {
        return s;
    }
    if (state_ok(msr_read(cpu, &com, sizeof(ullong), GLBL_CTL_OFS))) {
        com = (com | GLBL_CMD_UNF);
        msr_write(cpu, &com, sizeof(ullong), GLBL_CTL_OFS);
    }
    if (state_fail(s = msr_read(cpu, &com, sizeof(ullong), address_ctl))) {
	return s;
    }
    com1 = ((com & UBOX_CMD_STA) >> SHIFT);
    if (!com1) {
        com = com | UBOX_CMD_STA;
        if (state_fail(s = msr_write(cpu, &com, sizeof(ullong), address_ctl))) {
            return s;
        }
    }
    return EAR_SUCCESS;
}

state_t imcfreq_intel63_ext_read_cpu(int cpu, ulong *freq)
{
    return msr_read(cpu, freq, sizeof(ulong), address_ctr);
}

state_t imcfreq_intel63_init(ctx_t *c)
{
	static int init = 0;
	state_t s;
	int cpu;
	
	if (init) {
		return EAR_SUCCESS;
	}	
	// Opening MSR
	for (cpu = 0; cpu < tp.cpu_count; ++cpu) {
		if (state_fail(s = imcfreq_intel63_ext_enable_cpu(tp.cpus[cpu].id))) {
			return s;
		}
	}
	init = 1;
	return EAR_SUCCESS;
}

state_t imcfreq_intel63_dispose(ctx_t *c)
{
	int cpu;
	for (cpu = 0; cpu < tp.cpu_count; ++cpu) {
		msr_close(tp.cpus[cpu].id);
	}
	return EAR_SUCCESS;
}

state_t imcfreq_intel63_count_devices(ctx_t *c, uint *dev_count)
{
	if (dev_count == NULL) {
		return_msg(EAR_ERROR, Generr.input_null);
	}
	*dev_count = tp.cpu_count;
	return EAR_SUCCESS;
}

static void data_clean(imcfreq_t *i)
{
	int cpu;
	// Cleaning
	memset(i, 0, tp.cpu_count * sizeof(imcfreq_t));
	for (cpu = 0; cpu < tp.cpu_count; ++cpu) {
		i[cpu].error = 1;
	}
}

state_t imcfreq_intel63_read(ctx_t *c, imcfreq_t *i)
{
	//int enabled;
	state_t s;
	int cpu;

	if (i == NULL) {
		return_msg(EAR_ERROR, Generr.input_null);
	}
	// Cleaning
	data_clean(i);
	// Time is required to compute hertzs.
	timestamp_getfast(&i[0].time);
	// Iterating per socket.
	for (cpu = 0; cpu < tp.cpu_count; ++cpu) {
		i[cpu].time = i[0].time;
		if (state_fail(s = imcfreq_intel63_ext_read_cpu(tp.cpus[cpu].id, &i[cpu].freq))) {
			return s;
		}
		debug("IMC%d read register: %lu (address 0x%lx)", tp.cpus[cpu].id, i[cpu].freq, address_ctr);
		i[cpu].error = state_fail(s);
	}

	return EAR_SUCCESS;
}
