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
#include <metrics/metrics.h>
#include <common/output/verbose.h>

#define sa(f) \
	if (state_fail(f)) { \
		serror(#f " FAILED"); \
		exit(0); \
	}

static metrics_apis_t m;

state_t metrics_apis_init(topology_t *tp, uint eard, metrics_apis_t **m_in)
{
	// Load
	gpu_load(eard);
	// Get API
	gpu_get_api(&m.gpu.api);
	// Working
	m.gpu.working = (m.gpu.api > API_DUMMY);
	// Init
	sa(gpu_init(&m.gpu.c));
	// Count (cache, flops and cpi does not have yet)
	sa(gpu_count_devices(&m.gpu.c, &m.gpu.devs_count));

	if (m_in != NULL) {
		*m_in = &m;
	}

	return EAR_SUCCESS;
}

state_t metrics_apis_dispose()
{
	sa(gpu_dispose(&m.gpu.c));
	
	return EAR_SUCCESS;
}

state_t metrics_apis_print()
{
	apis_print(m.gpu.api, "MET_GPU    : ");
	
	return EAR_SUCCESS;
}

state_t metrics_read_alloc(metrics_read_t *mr)
{
	sa(gpu_data_alloc(&mr->gpu));

	return EAR_SUCCESS;
}

state_t metrics_read_free(metrics_read_t *mr)
{
	return EAR_SUCCESS;
}

state_t metrics_read(metrics_read_t *mr)
{
	timestamp_get(&mr->time);

	sa(gpu_read(&m.gpu.c,  mr->gpu));

	return EAR_SUCCESS;
}

state_t metrics_read_diff(metrics_read_t *mr2, metrics_read_t *mr1, metrics_diff_t *mrD)
{
	mrD->time = timestamp_diffnow(&mr1->time, TIME_SECS);
	
    sa(gpu_read_diff(&m.gpu.c, mr2->gpu, mr1->gpu, mrD->gpu_diff));

	return EAR_SUCCESS;
}

state_t metrics_read_copy(metrics_read_t *mr2, metrics_read_t *mr1, metrics_diff_t *mrD)
{
	mrD->time = timestamp_diffnow(&mr1->time, TIME_SECS);
	
	sa(gpu_read_copy(&m.gpu.c, mr2->gpu, mr1->gpu, mrD->gpu_diff));

	return EAR_SUCCESS;
}


state_t metrics_diff_alloc(metrics_diff_t *md)
{
	sa(gpu_data_alloc(&md->gpu_diff));

	return EAR_SUCCESS;
}

state_t metrics_diff_free(metrics_read_t *md)
{
	return EAR_SUCCESS;
}

state_t metrics_diff_print(metrics_diff_t *mrD)
{
	gpu_data_print( mrD->gpu_diff, verb_channel);

	return EAR_SUCCESS;
}

#if TEST
#include <stdio.h>
#include <unistd.h>
#include <common/system/monitor.h>
#include <management/management.h>
#include <daemon/local_api/eard_api_rpc.h>

static metrics_apis_t *mets;
static metrics_read_t mr1;
static metrics_read_t mr2;
static metrics_diff_t mrD;
static topology_t tp;
static int print;

state_t monitor_apis_init(void *whatever)
{
	sa(topology_init(&tp));

	//eards_connection();
	
	// Metrics
	sa(metrics_apis_init(&tp, EARD, &mets));
	sa(metrics_apis_print());
	
	printf("-------------------------------------------------------------------\n");

	sa(metrics_read_alloc(&mr1));
	sa(metrics_read_alloc(&mr2));
	sa(metrics_diff_alloc(&mrD));
	sa(metrics_read(&mr1));
	
	return EAR_SUCCESS;
}

state_t monitor_apis_update(void *whatever)
{
	if (print) {
		sa(metrics_read_copy(&mr2, &mr1, &mrD));
		sa(metrics_diff_print(&mrD));
	    printf("-------------------------------------------------------------------\n");
	}

	return EAR_SUCCESS;
} 

int init()
{
    print = 1;

	// Monitoring   
	sa(monitor_init());
	suscription_t *sus = suscription();
	sus->call_init     = monitor_apis_init; 
	sus->call_main     = monitor_apis_update;
	sus->time_relax    = 2000;
	sus->time_burst    = 2000;
	sa(sus->suscribe(sus));

	return 0;
}

int is(char *buf, char *string)
{
        return strcmp(buf, string) == 0;
}

//gcc -I .. -DTEST=1 -o test metrics.c ../management/libmgt.a libmetrics.a ../daemon/local_api/libeard.a ../common/libcommon.a -lpthread -ldl -lm

int main(int argc, char *argv[])
{
	char cmnd[SZ_PATH];

	while(1)
    {
	printf("-------------------------------------------------------------------\n");
    scanf("%s", cmnd);

    if (is(cmnd, "connect")) {
        eards_connection();
    } else if (is(cmnd, "disconnect")) {
        eards_disconnect();
    } else if (is(cmnd, "init")) {
        init();
    } else if (is(cmnd, "dispose")) {
        metrics_apis_dispose();
    } else if (is(cmnd, "print")) {
    }
	outif:
	;
	}

	return 0;
}
#endif
