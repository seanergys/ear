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

#include <stdlib.h>
#include <management/management.h>
#include <common/output/verbose.h>

#define sa(f) \
	printf("calling " #f "\n"); \
    if (state_fail(f)) { \
        serror(#f " FAILED"); \
        exit(0); \
    }

static manages_apis_t m;

state_t manages_apis_init(topology_t *tp, uint eard, manages_apis_t **m_in)
{
	state_t s;
	// Load
	sa(mgt_cpufreq_load(tp, eard));
	sa(mgt_imcfreq_load(tp, eard));
	// Get API
	sa(mgt_cpufreq_get_api(&m.cpu.api));
	sa(mgt_imcfreq_get_api(&m.imc.api));
	//sa(    mgt_gpu_get_api(&m.gpu.api));
	// Init
	sa(mgt_cpufreq_init(&m.cpu.c));
	sa(mgt_imcfreq_init(&m.imc.c));
	// Count (cache does not need)
	sa(mgt_cpufreq_count_devices(&m.cpu.c, &m.cpu.devs_count));
	sa(mgt_imcfreq_count_devices(&m.imc.c, &m.imc.devs_count));
	// Availability
	sa(mgt_cpufreq_get_available_list(&m.cpu.c, (const pstate_t **) &m.cpu.avail_list, &m.cpu.avail_count));
	sa(mgt_imcfreq_get_available_list(&m.imc.c, (const pstate_t **) &m.imc.avail_list, &m.imc.avail_count));
    //
    sa(mgt_cpufreq_data_alloc((pstate_t **) &m.cpu.current_list, empty));
    sa(mgt_imcfreq_data_alloc((pstate_t **) &m.imc.current_list, empty));

    //
	if (m_in != NULL) {
		*m_in = &m;
	}

	return EAR_SUCCESS;
}

state_t manages_apis_dispose()
{
	sa(mgt_imcfreq_dispose(&m.imc.c));

	return EAR_SUCCESS;
}

state_t manages_apis_print()
{
	apis_print(m.cpu.api, "MGT_CPUFREQ: ");
	apis_print(m.imc.api, "MGT_IMCFREQ: ");
	
	return EAR_SUCCESS;
}

state_t manages_apis_print_available()
{
    char buffer[SZ_BUFFER_EXTRA];
	
    printf("CPUFREQ available list of frequencies:\n");
    mgt_cpufreq_data_print(m.cpu.avail_list, m.cpu.avail_count, verb_channel);
    printf("IMCFREQ available list of frequencies:\n");
	mgt_imcfreq_data_print(m.imc.avail_list, m.imc.avail_count, verb_channel);

    mgt_cpufreq_get_current_list(no_ctx, m.cpu.current_list);
    mgt_imcfreq_get_current_list(no_ctx, m.imc.current_list);
    
    printf("CPUFREQ current list of frequencies:\n");
    mgt_cpufreq_data_print(m.cpu.current_list, m.cpu.devs_count, verb_channel);
    printf("IMCFREQ current list of frequencies:\n");
	mgt_imcfreq_data_print(m.imc.current_list, m.imc.devs_count, verb_channel);
	
	return EAR_SUCCESS;
}

#if TEST
#include <stdio.h>
#include <unistd.h>
#include <management/management.h>
#include <daemon/local_api/eard_api_rpc.h>

static manages_apis_t *mans;
static topology_t tp;

// /hpc/base/ctt/packages/compiler/gnu/9.1.0/bin/gcc -I .. -DTEST=1 -o test management.c ./libmgt.a ../metrics/libmetrics.a ../daemon/local_api/libeard.a ../common/libcommon.a -lpthread -ldl -lm
// /opt/rh/devtoolset-7/root/bin/gcc -I .. -DTEST=1 -o test management.c ./libmgt.a ../metrics/libmetrics.a ../daemon/local_api/libeard.a ../common/libcommon.a -lpthread -ldl -lm

int main(int argc, char *argv[])
{
	sa(topology_init(&tp));

	eards_connection();
	
	// Management
	sa(manages_apis_init(&tp, EARD, &mans));
	sa(manages_apis_print());
	sa(manages_apis_print_available());
sleep(10); 
	return EAR_SUCCESS;
}
#endif
