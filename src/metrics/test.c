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

#include <stdlib.h>
#include <metrics/metrics.h>
#include <common/utils/stress.h>
#include <common/utils/overhead.h>
#include <common/output/verbose.h>

#define sa(f) \
	if (state_fail(f)) { \
		serror(#f " FAILED"); \
		exit(0); \
	}

#define printline() \
    dprintf(verb_channel, "-------------------------------------------------------------------\n");

static metrics_apis_t m;

state_t metrics_init(topology_t *tp, uint eard, metrics_apis_t **m_in)
{
	cpufreq_load(tp, eard);
	imcfreq_load(tp, eard);
     bwidth_load(tp, eard);
      cache_load(tp, eard);
      flops_load(tp, eard);
       temp_load(tp      );
       rapl_load(tp      );
        cpi_load(tp, eard);
	    gpu_load(    eard);

	cpufreq_get_api(&m.cpu.api);
	imcfreq_get_api(&m.imc.api);
     bwidth_get_api(&m.ram.api);
      cache_get_api(&m.cch.api);
      flops_get_api(&m.flp.api);
       temp_get_api(&m.tmp.api);
       rapl_get_api(&m.rap.api);
        cpi_get_api(&m.cpi.api);
	    gpu_get_api(&m.gpu.api);

	sa(cpufreq_init(&m.cpu.c));
	sa(imcfreq_init(&m.imc.c));
    sa( bwidth_init(&m.ram.c));
    sa(  cache_init(&m.cch.c));
    sa(  flops_init(&m.flp.c));
    sa(   temp_init(&m.tmp.c));
    sa(   rapl_init(&m.rap.c));
    sa(    cpi_init(&m.cpi.c));
	sa(    gpu_init(&m.gpu.c));
	// Count (cache, flops and cpi does not have yet)
	sa(cpufreq_count_devices(&m.cpu.c, &m.cpu.devs_count));
	sa(imcfreq_count_devices(&m.imc.c, &m.imc.devs_count));
    sa( bwidth_count_devices(&m.ram.c, &m.ram.devs_count));
    sa(   temp_count_devices(&m.tmp.c, &m.tmp.devs_count));
    sa(   rapl_count_devices(&m.rap.c, &m.rap.devs_count));
    sa(    gpu_count_devices(&m.gpu.c, &m.gpu.devs_count));

    apis_print(m.cpu.api, "MET_CPUFREQ: ");
    apis_print(m.imc.api, "MET_IMCFREQ: ");
    apis_print(m.ram.api, "MET_BWIDTH : ");
    apis_print(m.cch.api, "MET_CACHE  : ");
    apis_print(m.flp.api, "MET_FLOPS  : ");
    apis_print(m.tmp.api, "MET_TEMP   : ");
    apis_print(m.rap.api, "MET_RAPL   : ");
    apis_print(m.cpi.api, "MET_CPI    : ");
    apis_print(m.gpu.api, "MET_GPU    : ");
    printline();

	if (m_in != NULL) {
		*m_in = &m;
	}
	return EAR_SUCCESS;
}

state_t metrics_read(metrics_read_t *mr)
{
    timestamp_get(&mr->time);

    sa(cpufreq_read(&m.cpu.c,  mr->cpu));
    sa(imcfreq_read(&m.imc.c,  mr->imc));
    sa( bwidth_read(&m.ram.c,  mr->ram));
    sa(  cache_read(&m.cch.c, &mr->cch));
    sa(  flops_read(&m.flp.c, &mr->flp));
    sa(   rapl_read(&m.rap.c,  mr->rap));
    sa(    cpi_read(&m.cpi.c, &mr->cpi));
    sa(    gpu_read(&m.gpu.c,  mr->gpu));

    return EAR_SUCCESS;
}

state_t metrics_apis_print()
{


	return EAR_SUCCESS;
}

state_t metrics_read_diff(metrics_read_t *mr2, metrics_read_t *mr1, metrics_diff_t *mrD)
{
	mrD->time = timestamp_diffnow(&mr1->time, TIME_SECS);

	sa(cpufreq_read_diff(&m.cpu.c,  mr2->cpu,  mr1->cpu,        mrD->cpu_diff, &mrD->cpu_avrg));
	sa(imcfreq_read_diff(&m.imc.c,  mr2->imc,  mr1->imc,        mrD->imc_diff, &mrD->imc_avrg));
    sa( bwidth_read_diff(&m.ram.c,  mr2->ram,  mr1->ram, NULL, &mrD->ram_diff, &mrD->ram_avrg));
    sa(  cache_read_diff(&m.cch.c, &mr2->cch, &mr1->cch,       &mrD->cch_diff, &mrD->cch_avrg));
    sa(  flops_read_diff(&m.flp.c, &mr2->flp, &mr1->flp,       &mrD->flp_diff, &mrD->flp_avrg));
    sa(   rapl_read_diff(&m.rap.c,  mr2->rap,  mr1->rap,        mrD->rap_diff                ));
    sa(    cpi_read_diff(&m.cpi.c, &mr2->cpi, &mr1->cpi,       &mrD->cpi_diff, &mrD->cpi_avrg));
	sa(    gpu_read_diff(&m.gpu.c,  mr2->gpu,  mr1->gpu,        mrD->gpu_diff                ));

	return EAR_SUCCESS;
}

state_t metrics_read_copy(metrics_read_t *mr2, metrics_read_t *mr1, metrics_diff_t *mrD)
{
	mrD->time = timestamp_diffnow(&mr1->time, TIME_SECS);
	
	sa(cpufreq_read_copy(&m.cpu.c,  mr2->cpu,  mr1->cpu,        mrD->cpu_diff, &mrD->cpu_avrg));
	sa(imcfreq_read_copy(&m.imc.c,  mr2->imc,  mr1->imc,        mrD->imc_diff, &mrD->imc_avrg));
    sa( bwidth_read_copy(&m.ram.c,  mr2->ram,  mr1->ram, NULL, &mrD->ram_diff, &mrD->ram_avrg));
    sa(  cache_read_copy(&m.cch.c, &mr2->cch, &mr1->cch,       &mrD->cch_diff, &mrD->cch_avrg));
    sa(  flops_read_copy(&m.flp.c, &mr2->flp, &mr1->flp,       &mrD->flp_diff, &mrD->flp_avrg));
    sa(   temp_read     (&m.tmp.c,                              mrD->tmp_diff, &mrD->tmp_avrg));
    sa(   rapl_read_copy(&m.rap.c,  mr2->rap,  mr1->rap,        mrD->rap_diff                ));
    sa(    cpi_read_copy(&m.cpi.c, &mr2->cpi, &mr1->cpi,       &mrD->cpi_diff, &mrD->cpi_avrg));
	sa(    gpu_read_copy(&m.gpu.c,  mr2->gpu,  mr1->gpu,        mrD->gpu_diff                ));

	return EAR_SUCCESS;
}


state_t metrics_data_alloc(metrics_read_t *mr1, metrics_read_t *mr2, metrics_diff_t *mrD)
{
    // Read 1
    sa(cpufreq_data_alloc(&mr->cpu1, empty));
    sa(imcfreq_data_alloc(&mr->imc1, empty));
    sa( bwidth_data_alloc(&mr->ram1));
    sa(   rapl_data_alloc(&mr->rap1));
    sa(    gpu_data_alloc(&mr->gpu1));
    // Read 2
    sa(cpufreq_data_alloc(&mr->cpu2, empty));
    sa(imcfreq_data_alloc(&mr->imc2, empty));
    sa( bwidth_data_alloc(&mr->ram2));
    sa(   rapl_data_alloc(&mr->rap2));
    sa(    gpu_data_alloc(&mr->gpu2));
    // Diff
    sa(cpufreq_data_alloc(empty, &md->cpu_diff));
    sa(imcfreq_data_alloc(empty, &md->imc_diff));
    sa(   temp_data_alloc(&md->tmp_diff));
    sa(   rapl_data_alloc(&md->rap_diff));
    sa(    gpu_data_alloc(&md->gpu_diff));
    // Data registers
    dreg_init(&md->imc_dreg, sizeof(ullong)*2);
    dreg_init(&md->tmp_dreg, sizeof(ullong)*2);
    dreg_init(&md->rap_dreg, sizeof(rapl_t)*2);

    return EAR_SUCCESS;
}

state_t metrics_diff_print(metrics_diff_t *mrD)
{
	cpufreq_data_print( mrD->cpu_diff, mrD->cpu_avrg, verb_channel);
	imcfreq_data_print( mrD->imc_diff,&mrD->imc_avrg, verb_channel);
       temp_data_print( mrD->tmp_diff, mrD->tmp_avrg, verb_channel);

    dreg_add(&mrD->imc_dreg, mrD->imc_diff);
    dreg_add(&mrD->tmp_dreg, mrD->tmp_diff);
    dreg_add(&mrD->rap_dreg, mrD->rap_diff);

    // Printing data registers
    dreg_print(&mrD->imc_dreg, DREG_STYLE_CSV, "!*llu", verb_channel); 
    dreg_print(&mrD->tmp_dreg, DREG_STYLE_CSV, "!*llu", verb_channel); 
    dreg_print(&mrD->rap_dreg, DREG_STYLE_CSV, "!llu|llu|-llu|llu|llu|-llu", verb_channel); 

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
        printline();
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
	sus->time_relax    = 1000;
	sus->time_burst    = 1000;
	sa(sus->suscribe(sus));

	return 0;
}

int is(char *buf, char *string)
{
    return strcmp(buf, string) == 0;
}

int main(int argc, char *argv[])
{
	char cmnd[SZ_PATH];

	while(1)
    {
    printline();
    scanf("%s", cmnd);

    if (is(cmnd, "connect")) {
        eards_connection();
    } else if (is(cmnd, "disconnect")) {
        eards_disconnect();
    } else if (is(cmnd, "init")) {
        init();
    } else if (is(cmnd, "dispose")) {
        metrics_apis_dispose();
    } else if (is(cmnd, "oh")) {
       overhead_report(); 
    }
	outif:
	;
	}

	return 0;
}
#endif
