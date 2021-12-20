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
	//cpufreq_load(tp, eard);
	imcfreq_load(tp, eard);
	 bwidth_load(tp, eard);
	  cache_load(tp, eard);
	  flops_load(tp, eard);
	    cpi_load(tp, eard);
	    gpu_load(    eard);
	// Get API
	//cpufreq_get_api(&m.cpu.api);
	imcfreq_get_api(&m.imc.api);
	 bwidth_get_api(&m.ram.api);
	  cache_get_api(&m.cch.api);
	  flops_get_api(&m.flp.api);
	    cpi_get_api(&m.cpi.api);
	    gpu_get_api(&m.gpu.api);
	// Working
	m.cpu.working = (m.cpu.api > API_DUMMY);
	m.imc.working = (m.imc.api > API_DUMMY);
	m.ram.working = (m.ram.api > API_DUMMY);
	m.cch.working = (m.cch.api > API_DUMMY);
	m.flp.working = (m.flp.api > API_DUMMY);
	m.cpi.working = (m.cpi.api > API_DUMMY);
	m.gpu.working = (m.gpu.api > API_DUMMY);
	// Init
	//sa(cpufreq_init(&m.cpu.c));
	sa(imcfreq_init(&m.imc.c));
	sa( bwidth_init(&m.ram.c));
	sa(  cache_init(&m.cch.c));
	sa(  flops_init(&m.flp.c));
	sa(    cpi_init(&m.cpi.c));
	sa(    gpu_init(&m.gpu.c));
	// Count (cache, flops and cpi does not have yet)
	//sa(cpufreq_count_devices(&m.cpu.c, &m.cpu.devs_count));
	sa(imcfreq_count_devices(&m.imc.c, &m.imc.devs_count));
	sa( bwidth_count_devices(&m.ram.c, &m.ram.devs_count));
	sa(    gpu_count_devices(&m.gpu.c, &m.gpu.devs_count));
	m.cch.devs_count = 1;
	m.flp.devs_count = 1;
	m.cpi.devs_count = 1;

	if (m_in != NULL) {
		*m_in = &m;
	}

	return EAR_SUCCESS;
}

state_t metrics_apis_dispose()
{
	//sa(cpufreq_dispose(&m.cpu.c));
	sa(imcfreq_dispose(&m.imc.c));
	sa( bwidth_dispose(&m.ram.c));
	sa(  cache_dispose(&m.flp.c));
	sa(  flops_dispose(&m.cch.c));
	sa(    cpi_dispose(&m.cpi.c));
	sa(    gpu_dispose(&m.gpu.c));
	
	return EAR_SUCCESS;
}

state_t metrics_apis_print()
{
	//apis_print(m.cpu.api, "MET_CPUFREQ: ");
	apis_print(m.imc.api, "MET_IMCFREQ: ");
	apis_print(m.ram.api, "MET_BWIDTH : ");
	apis_print(m.cch.api, "MET_CACHE  : ");
	apis_print(m.flp.api, "MET_FLOPS  : ");
	apis_print(m.cpi.api, "MET_CPI    : ");
	apis_print(m.gpu.api, "MET_GPU    : ");
	
	return EAR_SUCCESS;
}

state_t metrics_read_alloc(metrics_read_t *mr)
{
	//sa(cpufreq_data_alloc(&mr->cpu, empty));
	sa(imcfreq_data_alloc(&mr->imc, empty));
	sa( bwidth_data_alloc(&mr->ram));
	sa(    gpu_data_alloc(&mr->gpu));

	return EAR_SUCCESS;
}

state_t metrics_read_free(metrics_read_t *mr)
{
	return EAR_SUCCESS;
}

state_t metrics_read(metrics_read_t *mr)
{
	timestamp_get(&mr->time);
	
	//sa(cpufreq_read(&m.cpu.c,  mr->cpu));
	sa(imcfreq_read(&m.imc.c,  mr->imc));
	sa( bwidth_read(&m.ram.c,  mr->ram));
	sa(  cache_read(&m.cch.c, &mr->cch));
	sa(  flops_read(&m.flp.c, &mr->flp));
	sa(    cpi_read(&m.cpi.c, &mr->cpi));
	sa(    gpu_read(&m.gpu.c,  mr->gpu));

	return EAR_SUCCESS;
}

state_t metrics_read_diff(metrics_read_t *mr2, metrics_read_t *mr1, metrics_diff_t *mrD)
{
	mrD->time = timestamp_diffnow(&mr1->time, TIME_SECS);

	//sa(cpufreq_read_diff(&m.cpu.c,  mr2->cpu,  mr1->cpu,  mrD->cpu_diff, &mrD->cpu_avrg));
	sa(imcfreq_read_diff(&m.imc.c,  mr2->imc,  mr1->imc,  mrD->imc_diff, &mrD->imc_avrg));
	sa( bwidth_read_diff(&m.ram.c,  mr2->ram,  mr1->ram, &mrD->ram_diff, &mrD->ram_avrg));
	sa(  cache_read_diff(&m.cch.c, &mr2->cch, &mr1->cch, &mrD->cch_diff, &mrD->cch_avrg));
	sa(  flops_read_diff(&m.flp.c, &mr2->flp, &mr1->flp, &mrD->flp_diff, &mrD->flp_avrg));
	sa(    cpi_read_diff(&m.cpi.c, &mr2->cpi, &mr1->cpi, &mrD->cpi_diff, &mrD->cpi_avrg));
	sa(    gpu_read_diff(&m.gpu.c,  mr2->gpu,  mr1->gpu,  mrD->gpu_diff                ));

	return EAR_SUCCESS;
}

state_t metrics_read_copy(metrics_read_t *mr2, metrics_read_t *mr1, metrics_diff_t *mrD)
{
	mrD->time = timestamp_diffnow(&mr1->time, TIME_SECS);
	
	//sa(cpufreq_read_copy(&m.cpu.c,  mr2->cpu,  mr1->cpu,  mrD->cpu_diff, &mrD->cpu_avrg));
	sa(imcfreq_read_copy(&m.imc.c,  mr2->imc,  mr1->imc,  mrD->imc_diff, &mrD->imc_avrg));
	sa( bwidth_read_copy(&m.ram.c,  mr2->ram,  mr1->ram, &mrD->ram_diff, &mrD->ram_avrg));
	sa(  cache_read_copy(&m.cch.c, &mr2->cch, &mr1->cch, &mrD->cch_diff, &mrD->cch_avrg));
	sa(  flops_read_copy(&m.flp.c, &mr2->flp, &mr1->flp, &mrD->flp_diff, &mrD->flp_avrg));
	sa(    cpi_read_copy(&m.cpi.c, &mr2->cpi, &mr1->cpi, &mrD->cpi_diff, &mrD->cpi_avrg));
	sa(    gpu_read_copy(&m.gpu.c,  mr2->gpu,  mr1->gpu,  mrD->gpu_diff                ));

	return EAR_SUCCESS;
}


state_t metrics_diff_alloc(metrics_diff_t *md)
{
	//sa(cpufreq_data_alloc(empty, &md->cpu_diff));
	sa(imcfreq_data_alloc(empty, &md->imc_diff));
	sa(    gpu_data_alloc(&md->gpu_diff));

	return EAR_SUCCESS;
}

state_t metrics_diff_free(metrics_read_t *md)
{
	return EAR_SUCCESS;
}

state_t metrics_diff_print(metrics_diff_t *mrD)
{
	//cpufreq_data_print( mrD->cpu_diff, mrD->cpu_avrg, verb_channel);
	imcfreq_data_print( mrD->imc_diff,&mrD->imc_avrg, verb_channel);
	 bwidth_data_print( mrD->ram_diff, mrD->ram_avrg, verb_channel);
	  cache_data_print(&mrD->cch_diff, mrD->cch_avrg, verb_channel);
	  flops_data_print(&mrD->flp_diff, mrD->flp_avrg, verb_channel);
	    cpi_data_print(&mrD->cpi_diff, mrD->cpi_avrg, verb_channel);
	    gpu_data_print( mrD->gpu_diff,                verb_channel);

	return EAR_SUCCESS;
}

#if TEST
#include <stdio.h>
#include <unistd.h>
#include <common/system/monitor.h>
#include <management/management.h>
#include <daemon/local_api/eard_api_rpc.h>

static metrics_apis_t *mets;
static manages_apis_t *mans;
static metrics_read_t mr1;
static metrics_read_t mr2;
static metrics_diff_t mrD;
static topology_t tp;
static int print;

state_t monitor_apis_init(void *whatever)
{
	sa(topology_init(&tp));

	eards_connection();
	
	// Metrics
	sa(metrics_apis_init(&tp, EARD, &mets));
	sa(metrics_apis_print());
	
	// Management
	//sa(manages_apis_init(&tp, EARD, &mans));
	//sa(manages_apis_print());
	//verbose(0, "-------------------------------------------------------------------");
	
	//sa(manages_apis_print_available());
	verbose(0, "-------------------------------------------------------------------");

	sa(metrics_read_alloc(&mr1));
	sa(metrics_read_alloc(&mr2));
	sa(metrics_diff_alloc(&mrD));
	sa(metrics_read(&mr1));
	
	return EAR_SUCCESS;
}

//	#define N 65536
	#define N 262144
	static ullong array1[N];
	static ullong array2[N];
	static ullong array3[N];
state_t monitor_apis_update(void *whatever)
{
	int i;

	for (i = 2; i < N; ++i) {
		array1[i] = array2[i-1] + array3[i-2];
	}


	if (print) {
		sa(metrics_read_copy(&mr2, &mr1, &mrD));
		sa(metrics_diff_print(&mrD));
		verbose(0, "-------------------------------------------------------------------");
	}

	return EAR_SUCCESS;
} 

int init()
{
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

// /hpc/base/ctt/packages/compiler/gnu/9.1.0/bin/gcc -I .. -DTEST=1 -o test metrics.c ../management/libmgt.a libmetrics.a ../daemon/local_api/libeard.a ../common/libcommon.a -lpthread -ldl

int main(int argc, char *argv[])
{
	char cmnd[SZ_PATH];
	char opt1[SZ_PATH];
	int aux1;
	int aux2;
	int aux3;
	int i;

	pstate_t buf_pstate[1024];
	ulong    buf_ulong[1024];
	uint     buf_uint[1024];

	#define sb(f, action) \
		if (state_fail(f)) { \
			serror("Something happened"); \
			action; \
		}

	// Control
	while(1) {
		verbose(0, "-------------------------------------------------------------------");
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
                        print = 1;
                } else if (is(cmnd, "set-gov")) {
                        scanf("%s", opt1);
                        sb(mgt_governor_toint(opt1, (uint *) &aux1), goto outif);
                        sb(mgt_cpufreq_set_governor(&mans->cpu.c, (uint) aux1),);
                } else if (is(cmnd, "set-freq")) {
                        scanf("%d", &aux1);
                        sb(mgt_cpufreq_get_index(&mans->cpu.c, (ullong) aux1, (uint *) &aux2, 1), goto outif);
                        sb(mgt_cpufreq_set_current(&mans->cpu.c, (uint) aux2, -1),);
                } else if (is(cmnd, "set-freq-cpu")) {
                        scanf("%d", &aux1);
                        scanf("%d", &aux3);
                        sb(mgt_cpufreq_get_index(&mans->cpu.c, (ullong) aux1, (uint *) &aux2, 1), goto outif);
                        sb(mgt_cpufreq_set_current(&mans->cpu.c, (uint) aux2, aux3),);
			sb(mgt_imcfreq_set_auto(&mans->imc.c),);
			// MGT GPU
                } else if(is(cmnd, "gpu-pow-set")) {
                        scanf("%lu", &buf_ulong[0]);
                        for (i = 1; i < mans->gpu.devs_count; ++i) {
                                buf_ulong[i] = buf_ulong[0];
                        }
                        sb(mgt_gpu_power_cap_set(&mans->gpu.c, buf_ulong),);
		// MGT IMCFREQ
		} else if(is(cmnd, "imc-freq-get")) {
			sb(mgt_imcfreq_get_current_list(&mans->imc.c, buf_pstate),);
			mgt_imcfreq_data_print(buf_pstate, mans->imc.devs_count, verb_channel);
                } else if (is(cmnd, "imc-freq-set")) {
                        scanf("%d", &aux1);
                        for (i = 0; i < mans->imc.devs_count; ++i) {
                                buf_uint[i] = (uint) aux1;
                        }
                        sb(mgt_imcfreq_set_current_list(&mans->imc.c, buf_uint),);
                } else if (is(cmnd, "imc-freq-reset")) {
                } else if(is(cmnd, "imc-min-set")) {
                        #if 1
                        for (i = 0; i < mans->imc.devs_count; ++i) {
                            printf("max ps for dev %d: ", i);
                            scanf("%d", &buf_uint[i]);
                        }
                        for (; i < mans->imc.devs_count*2; ++i) {
                            printf("min ps for dev %d: ", i - mans->imc.devs_count);
                            scanf("%d", &buf_uint[i]);
                        }
                        sb(mgt_imcfreq_set_current_ranged_list(&mans->imc.c, &buf_uint[0], &buf_uint[2]),);
                        #endif
                } else if(is(cmnd, "imc-min-get")) {
			#if 1
                        sb(mgt_imcfreq_get_current_ranged_list(&mans->imc.c, &buf_pstate[0], &buf_pstate[mans->imc.devs_count]),);
                        for (i = 0; i < mans->imc.devs_count; ++i) {
				printf("dev%d: %llu %llu\n", i,
					buf_pstate[i].khz, buf_pstate[i+mans->imc.devs_count].khz);
			}
			#endif 
                } else if(is(cmnd, "gpu-pow-reset")) {
                        sb(mgt_gpu_power_cap_reset(&mans->gpu.c),);
                } else if(is(cmnd, "gpu-freq-set")) {
                     scanf("%lu", &buf_ulong[0]);
                        for (i = 1; i < mans->gpu.devs_count; ++i) {
                                buf_ulong[i] = buf_ulong[0];
                        }
                        sb(mgt_gpu_freq_limit_set(&mans->gpu.c, buf_ulong),);
                } else if(is(cmnd, "gpu-freq-reset")) {
                        sb(mgt_gpu_freq_limit_reset(&mans->gpu.c),);
        }
		outif:
		;
	}

	return 0;
}
#endif
