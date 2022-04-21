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
#include <unistd.h>
#include <management/management.h>
#include <common/output/verbose.h>

#define sa(f) \
        if (state_fail(f)) { \
                serror(#f " FAILED"); \
                exit(0); \
        }

static manages_apis_t m;

void manages_apis_init(topology_t *tp, uint eard, manages_apis_t **m_in)
{
	state_t s;
	// Load
	mgt_gpu_load(eard);
	// Get API
	mgt_gpu_get_api(&m.gpu.api);
	// Init
	sa(mgt_gpu_init(no_ctx));
	// Count (cache does not need)
	sa(mgt_gpu_count_devices(no_ctx, &m.gpu.devs_count));
	
	if (m_in != NULL) {
		*m_in = &m;
	}
}

void manages_apis_dispose()
{
	sa(mgt_gpu_dispose(no_ctx));
}

void manages_apis_print()
{
	apis_print(m.gpu.api, "MGT_GPU: ");
}

#define printline() \
    verbose(0, "------------------------------");

void manages_apis_print_available()
{
}

void manages_apis_print_current()
{
}

#if TEST
// gcc -I .. -DTEST=1 -o test management.c ../management/libmgt.a ../metrics/libmetrics.a ../daemon/local_api/libeard.a ../common/libcommon.a -lpthread -ldl

#include <daemon/local_api/eard_api_rpc.h>

static topology_t tp;

int is(char *buf, char *string)
{
    return strcmp(buf, string) == 0;
}

int main(int argc, char *argv[])
{
    sa(topology_init(&tp));

    // Connecting with EARD
    eards_connection();
    
    // General init
    manages_apis_init(&tp, EARD, NULL);
    manages_apis_print();
    manages_apis_print_available();
    printline();
 
    ulong freqs[2] = { 135000, 135000 }; 
    // Main loop 
    while(1) {
        mgt_gpu_freq_limit_set(no_ctx, freqs);
        sleep(10);
    }

    return 0;
}
#endif
