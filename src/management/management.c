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
        if (state_fail(f)) { \
                serror(#f " FAILED"); \
                exit(0); \
        }

static manages_apis_t m;

state_t manages_apis_init(topology_t *tp, uint eard, manages_apis_t **m_in)
{
	state_t s;
	// Load
	sa(mgt_imcfreq_load(tp, eard));
	// Get API
	sa(mgt_imcfreq_get_api(&m.imc.api));
	//sa(    mgt_gpu_get_api(&m.gpu.api));
	// Init
	sa(mgt_imcfreq_init(&m.imc.c));
	// Count (cache does not need)
	sa(mgt_imcfreq_count_devices(&m.imc.c, &m.imc.devs_count));
	// Availability
	sa(mgt_imcfreq_get_available_list(&m.imc.c, (const pstate_t **) &m.imc.avail_list, &m.imc.avail_count));
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
    apis_print(m.imc.api, "MGT_IMCFREQ: ");
	
	return EAR_SUCCESS;
}

state_t manages_apis_print_available()
{
	mgt_imcfreq_data_print(m.imc.avail_list, m.imc.avail_count, verb_channel);
	
	return EAR_SUCCESS;
}

#if TEST
#include <stdio.h>
#include <unistd.h>
#include <management/management.h>
#include <daemon/local_api/eard_api_rpc.h>

static manages_apis_t *mans;
static topology_t tp;

// /hpc/base/ctt/packages/compiler/gnu/9.1.0/bin/gcc -I .. -DTEST=1 -o test metrics.c ../management/libmgt.a libmetrics.a ../daemon/local_api/libeard.a ../common/libcommon.a -lpthread -ldl -lm

int main(int argc, char *argv[])
{
	sa(topology_init(&tp));

	eards_connection();
	
	// Management
	sa(manages_apis_init(&tp, EARD, &mans));
	sa(manages_apis_print());
	
	return EAR_SUCCESS;
}

#endif
