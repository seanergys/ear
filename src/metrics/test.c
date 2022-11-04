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

#include <stdio.h>
#include <stdlib.h>
#include <metrics/test.h>
#include <common/utils/stress.h>
#include <common/utils/overhead.h>
#include <common/output/verbose.h>
#include <common/system/monitor.h>
#include <daemon/local_api/eard_api.h>

#define sa(f) \
	if (state_fail(f)) { \
		serror(#f " FAILED"); \
		exit(0); \
	}

#define printline() \
    dprintf(verb_channel, "-------------------------------------------------------------------\n");

static metrics_apis_t m;

void metrics_init(topology_t *tp, int eard)
{
	cpufreq_load(tp, eard);
	imcfreq_load(tp, eard);
     bwidth_load(tp, eard);
      cache_load(tp, eard);
      flops_load(tp, eard);
       temp_load(tp      );
        cpi_load(tp, eard);
//	    gpu_load(    eard);

	cpufreq_get_api(&m.cpu.api);
	imcfreq_get_api(&m.imc.api);
     bwidth_get_api(&m.ram.api);
      cache_get_api(&m.cch.api);
      flops_get_api(&m.flp.api);
       temp_get_api(&m.tmp.api);
        cpi_get_api(&m.cpi.api);
//	    gpu_get_api(&m.gpu.api);

	sa(cpufreq_init(&m.cpu.c));
	sa(imcfreq_init(&m.imc.c));
    sa( bwidth_init(&m.ram.c));
    sa(  cache_init(&m.cch.c));
    sa(  flops_init(&m.flp.c));
    sa(   temp_init(&m.tmp.c));
    sa(    cpi_init(&m.cpi.c));
//	sa(    gpu_init(&m.gpu.c));
	// Count (cache, flops and cpi does not have yet)
	sa(cpufreq_count_devices(&m.cpu.c, &m.cpu.devs_count));
	sa(imcfreq_count_devices(&m.imc.c, &m.imc.devs_count));
    sa( bwidth_count_devices(&m.ram.c, &m.ram.devs_count));
    sa(   temp_count_devices(&m.tmp.c, &m.tmp.devs_count));
//    sa(    gpu_count_devices(&m.gpu.c, &m.gpu.devs_count));
    // Getting granularity (or scope)
    bwidth_get_granularity(no_ctx, &m.ram.gran);

    apis_tostr(m.cpu.api, m.cpu.api_str, 32);
    apis_tostr(m.imc.api, m.imc.api_str, 32);
    apis_tostr(m.ram.api, m.ram.api_str, 32);
    apis_tostr(m.cch.api, m.cch.api_str, 32);
    apis_tostr(m.flp.api, m.flp.api_str, 32);
    apis_tostr(m.tmp.api, m.tmp.api_str, 32);
    apis_tostr(m.cpi.api, m.cpi.api_str, 32);
//    apis_tostr(m.gpu.api, m.gpu.api_str, 32);
    
    granularity_tostr(m.ram.gran, m.ram.gran_str, 32);
   
    sprintf(m.cpu.gran_str, "threads");
    sprintf(m.imc.gran_str, "sockets");
    sprintf(m.cch.gran_str, "process");
    sprintf(m.flp.gran_str, "process");
    sprintf(m.tmp.gran_str, "sockets");
    sprintf(m.cpi.gran_str, "process");
//    sprintf(m.gpu.gran_str, "gpus");
}

void metrics_read(metrics_read_t *mr)
{
    timestamp_get(&mr->time);

    sa(cpufreq_read(&m.cpu.c,  mr->cpu));
    sa(imcfreq_read(&m.imc.c,  mr->imc));
    sa( bwidth_read(&m.ram.c,  mr->ram));
    sa(  cache_read(&m.cch.c, &mr->cch));
    sa(  flops_read(&m.flp.c, &mr->flp));
    sa(    cpi_read(&m.cpi.c, &mr->cpi));
//    sa(    gpu_read(&m.gpu.c,  mr->gpu));
}

void metrics_read_diff(metrics_read_t *mr2, metrics_read_t *mr1, metrics_diff_t *mrD)
{
	mrD->time = timestamp_diffnow(&mr1->time, TIME_SECS);

	sa(cpufreq_read_diff(&m.cpu.c,  mr2->cpu,  mr1->cpu,        mrD->cpu_diff, &mrD->cpu_avrg));
	sa(imcfreq_read_diff(&m.imc.c,  mr2->imc,  mr1->imc,        mrD->imc_diff, &mrD->imc_avrg));
    sa( bwidth_read_diff(&m.ram.c,  mr2->ram,  mr1->ram, NULL, &mrD->ram_diff, &mrD->ram_avrg));
    sa(  cache_read_diff(&m.cch.c, &mr2->cch, &mr1->cch,       &mrD->cch_diff, &mrD->cch_avrg));
    sa(  flops_read_diff(&m.flp.c, &mr2->flp, &mr1->flp,       &mrD->flp_diff, &mrD->flp_avrg));
    sa(    cpi_read_diff(&m.cpi.c, &mr2->cpi, &mr1->cpi,       &mrD->cpi_diff, &mrD->cpi_avrg));
//	sa(    gpu_read_diff(&m.gpu.c,  mr2->gpu,  mr1->gpu,        mrD->gpu_diff                ));
}

void metrics_read_copy(metrics_read_t *mr2, metrics_read_t *mr1, metrics_diff_t *mrD)
{
	mrD->time = timestamp_diffnow(&mr1->time, TIME_SECS);
	
	sa(cpufreq_read_copy(&m.cpu.c,  mr2->cpu,  mr1->cpu,        mrD->cpu_diff, &mrD->cpu_avrg));
	sa(imcfreq_read_copy(&m.imc.c,  mr2->imc,  mr1->imc,        mrD->imc_diff, &mrD->imc_avrg));
    sa( bwidth_read_copy(&m.ram.c,  mr2->ram,  mr1->ram, NULL, &mrD->ram_diff, &mrD->ram_avrg));
    sa(  cache_read_copy(&m.cch.c, &mr2->cch, &mr1->cch,       &mrD->cch_diff, &mrD->cch_avrg));
    sa(  flops_read_copy(&m.flp.c, &mr2->flp, &mr1->flp,       &mrD->flp_diff, &mrD->flp_avrg));
    sa(   temp_read     (&m.tmp.c,                              mrD->tmp_diff, &mrD->tmp_avrg));
    sa(    cpi_read_copy(&m.cpi.c, &mr2->cpi, &mr1->cpi,       &mrD->cpi_diff, &mrD->cpi_avrg));
//	sa(    gpu_read_copy(&m.gpu.c,  mr2->gpu,  mr1->gpu,        mrD->gpu_diff                ));
}

void metrics_data_alloc(metrics_read_t *mr1, metrics_read_t *mr2, metrics_diff_t *mrD)
{
    // Read 1
    sa(cpufreq_data_alloc(&mr1->cpu, empty));
    sa(imcfreq_data_alloc(&mr1->imc, empty));
    sa( bwidth_data_alloc(&mr1->ram));
//    sa(    gpu_data_alloc(&mr1->gpu));
    // Read 2
    sa(cpufreq_data_alloc(&mr2->cpu, empty));
    sa(imcfreq_data_alloc(&mr2->imc, empty));
    sa( bwidth_data_alloc(&mr2->ram));
//    sa(    gpu_data_alloc(&mr2->gpu));
    // Diff
    sa(cpufreq_data_alloc(empty, &mrD->cpu_diff));
    sa(imcfreq_data_alloc(empty, &mrD->imc_diff));
    sa(   temp_data_alloc(&mrD->tmp_diff));
//    sa(    gpu_data_alloc(&mrD->gpu_diff));
}

#if TEST
static topology_t     tp;
static metrics_read_t mr1;
static metrics_read_t mr2;
static metrics_diff_t mrD;

static state_t monitor_apis_init(void *whatever)
{
	topology_init(&tp);
    //
	metrics_init(&tp, EARD);
    //
    metrics_data_alloc(&mr1, &mr2, &mrD);
    //
    metrics_read(&mr1);
	
	return EAR_SUCCESS;
}

static state_t monitor_apis_update(void *whatever)
{
    metrics_read_copy(&mr2, &mr1, &mrD);

    // Printf clear
    system("clear");

    tprintf_init(fdout, STR_MODE_COL, "11 11 6 20");
    tprintf("API||Internal||#Devs||Granularity");
    tprintf("---||--------||-----||-----------");

    tprintf("CPUFREQ||%s||%u||%s", m.cpu.api_str, m.cpu.devs_count, m.cpu.gran_str);
    tprintf("IMCFREQ||%s||%u||%s", m.imc.api_str, m.imc.devs_count, m.imc.gran_str);
    tprintf("BWIDTH ||%s||%u||%s", m.ram.api_str, m.ram.devs_count, m.ram.gran_str);
    tprintf("FLOPS  ||%s||%u||%s", m.flp.api_str, m.flp.devs_count, m.flp.gran_str);
    tprintf("CACHE  ||%s||%u||%s", m.cch.api_str, m.cch.devs_count, m.cch.gran_str);
    tprintf("TEMP   ||%s||%u||%s", m.tmp.api_str, m.tmp.devs_count, m.tmp.gran_str);
    tprintf("CPI    ||%s||%u||%s", m.cpi.api_str, m.cpi.devs_count, m.cpi.gran_str);
//    tprintf("GPU    ||%s||%u||%s", m.gpu.api_str, m.gpu.devs_count, m.gpu.gran_str);
    printf("—— cpufreq ————————————————————————————————————————————————————————————————————\n");
    cpufreq_data_print( mrD.cpu_diff,  mrD.cpu_avrg, verb_channel);
    printf("—— imcfreq ————————————————————————————————————————————————————————————————————\n");
    imcfreq_data_print( mrD.imc_diff, &mrD.imc_avrg, verb_channel);
    printf("—— bandwidth ——————————————————————————————————————————————————————————————————\n");
     bwidth_data_print( mrD.ram_diff,  mrD.ram_avrg, verb_channel);
    printf("—— temperature ————————————————————————————————————————————————————————————————\n");
       temp_data_print( mrD.tmp_diff,  mrD.tmp_avrg, verb_channel);
    
    return EAR_SUCCESS;
}

int main(int argc, char *argv[])
{
    if (argc > 1) {
        if (atoi(argv[1])) {
            eards_connection();
        }
    }

	// Monitoring   
	sa(monitor_init());
	suscription_t *sus = suscription();
	sus->call_init     = monitor_apis_init; 
	sus->call_main     = monitor_apis_update;
	sus->time_relax    = 2000;
	sus->time_burst    = 2000;
	sa(sus->suscribe(sus));

    sleep(100);

    return 0;
}
#endif
