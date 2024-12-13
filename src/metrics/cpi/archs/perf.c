/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

//#define SHOW_DEBUGS 1

#include <common/output/debug.h>
#include <metrics/common/perf.h>
#include <metrics/cpi/archs/perf.h>

// Generic
static perf_t perf_ins;
static perf_t perf_cyc;
static perf_t perf_sta;
static uint started;

void cpi_perf_load(topology_t *tp, cpi_ops_t *ops)
{
    if (tp->vendor == VENDOR_INTEL && tp->model >= MODEL_ICELAKE_X) {
        perf_open(&perf_ins, NULL, 0, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
        perf_open(&perf_cyc, NULL, 0, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES  );
        perf_open(&perf_sta, NULL, 0, PERF_TYPE_RAW     , 0x40004a3                 );
    } else if (tp->vendor == VENDOR_INTEL && tp->model >= MODEL_HASWELL_X) {
        perf_open(&perf_ins, NULL, 0, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
        perf_open(&perf_cyc, NULL, 0, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES  );
        perf_open(&perf_sta, NULL, 0, PERF_TYPE_RAW     , 0x01a2                    );
	} else if (tp->vendor == VENDOR_AMD && tp->family >= FAMILY_ZEN) {
        perf_open(&perf_ins, NULL, 0, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
        perf_open(&perf_cyc, NULL, 0, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES  );
        perf_open(&perf_sta, NULL, 0, PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND);
    } else if (tp->vendor == VENDOR_ARM) {
        perf_open(&perf_ins, NULL, 0, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
        perf_open(&perf_cyc, NULL, 0, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES  );
    } else {
        // API incompatible
        return;
    }
    apis_put(ops->get_info, cpi_perf_get_info);
	apis_put(ops->init,     cpi_perf_init);
	apis_put(ops->dispose,  cpi_perf_dispose);
	apis_put(ops->read,     cpi_perf_read);
}

void cpi_perf_get_info(apinfo_t *info)
{
    info->api = API_PERF;
}

state_t cpi_perf_init()
{
	if (!started) {
		if (perf_ins.fd >= 0) perf_start(&perf_ins);
		if (perf_cyc.fd >= 0) perf_start(&perf_cyc);
		if (perf_sta.fd >= 0) perf_start(&perf_sta);
		++started;
	}
	return EAR_SUCCESS;
}

state_t cpi_perf_dispose()
{
    if (perf_ins.fd >= 0) perf_close(&perf_ins);
    if (perf_cyc.fd >= 0) perf_close(&perf_cyc);
    if (perf_sta.fd >= 0) perf_close(&perf_sta);
    started = 0;
	return EAR_SUCCESS;
}

state_t cpi_perf_read(cpi_t *ci)
{
	memset(ci, 0, sizeof(cpi_t));
	if (perf_ins.fd >= 0) perf_read(&perf_ins, (llong *) &ci->instructions);
	if (perf_cyc.fd >= 0) perf_read(&perf_cyc, (llong *) &ci->cycles);
	if (perf_sta.fd >= 0) perf_read(&perf_sta, (llong *) &ci->stalls);
	debug("Instructions: %llu", ci->instructions);
	debug("Cycles: %llu", ci->cycles);
	debug("Stalls: %llu", ci->stalls);
	return EAR_SUCCESS;
}