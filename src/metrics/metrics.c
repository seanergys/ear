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
#include <common/utils/stress.h>
#include <common/utils/overhead.h>
#include <common/output/verbose.h>
#include <common/system/monitor.h>
#include <common/utils/strtable.h>
#include <common/utils/strscreen.h>
#include <common/config/config_install.h>
#include <daemon/local_api/eard_api.h>
#include <metrics/metrics.h>

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
        gpu_load(    eard);

    cpufreq_init(&m.cpu.c);
    imcfreq_init(&m.imc.c);
     bwidth_init(&m.ram.c);
      cache_init(&m.cch.c);
      flops_init(&m.flp.c);
       temp_init(&m.tmp.c);
        cpi_init(&m.cpi.c);
        gpu_init(&m.gpu.c);

    //
    cache_get_info(&m.cch.info);

    m.cpu.info.layer = "CPUFREQ";
    m.imc.info.layer = "IMCFREQ";
    m.ram.info.layer = "BANDWIDTH";
    m.flp.info.layer = "FLOPS";
    m.tmp.info.layer = "TEMPERATURE";
    m.cpi.info.layer = "CPI";
    m.gpu.info.layer = "GPU";

    cpufreq_get_api(&m.cpu.info.api);
    imcfreq_get_api(&m.imc.info.api);
     bwidth_get_api(&m.ram.info.api);
      flops_get_api(&m.flp.info.api);
       temp_get_api(&m.tmp.info.api);
        cpi_get_api(&m.cpi.info.api);
        gpu_get_api(&m.gpu.info.api);

    cpufreq_count_devices(&m.cpu.c, &m.cpu.info.devs_count);
    imcfreq_count_devices(&m.imc.c, &m.imc.info.devs_count);
     bwidth_count_devices(&m.ram.c, &m.ram.info.devs_count);
       temp_count_devices(&m.tmp.c, &m.tmp.info.devs_count);
        gpu_count_devices(&m.gpu.c, &m.gpu.info.devs_count);

    m.cpu.info.scope = SCOPE_NODE;
    m.imc.info.scope = SCOPE_NODE;
    m.ram.info.scope = SCOPE_NODE;
    m.flp.info.scope = SCOPE_PROCESS;
    m.tmp.info.scope = SCOPE_NODE;
    m.cpi.info.scope = SCOPE_PROCESS;
    m.gpu.info.scope = SCOPE_NODE;

    m.cpu.info.granularity = GRANULARITY_THREAD;
    m.imc.info.granularity = GRANULARITY_SOCKET;
    m.ram.info.granularity = GRANULARITY_IMC;
    m.flp.info.granularity = GRANULARITY_PROCESS;
    m.tmp.info.granularity = GRANULARITY_SOCKET;
    m.cpi.info.granularity = GRANULARITY_PROCESS;
    m.gpu.info.granularity = GRANULARITY_PERIPHERAL;

    apinfo_tostr(&m.cpu.info);
    apinfo_tostr(&m.imc.info);
    apinfo_tostr(&m.ram.info);
    apinfo_tostr(&m.cch.info);
    apinfo_tostr(&m.flp.info);
    apinfo_tostr(&m.tmp.info);
    apinfo_tostr(&m.cpi.info);
    apinfo_tostr(&m.gpu.info);

}

void metrics_read(metrics_read_t *mr)
{
    timestamp_get(&mr->time);

    cpufreq_read(&m.cpu.c,  mr->cpu);
    imcfreq_read(&m.imc.c,  mr->imc);
     bwidth_read(&m.ram.c,  mr->ram);
      cache_read(&m.cch.c, &mr->cch);
      flops_read(&m.flp.c, &mr->flp);
        cpi_read(&m.cpi.c, &mr->cpi);
        gpu_read(&m.gpu.c,  mr->gpu);
}

void metrics_read_copy(metrics_read_t *mr2, metrics_read_t *mr1, metrics_diff_t *mrD)
{
	mrD->time = timestamp_diffnow(&mr1->time, TIME_SECS);

    cpufreq_read_copy(&m.cpu.c,  mr2->cpu,  mr1->cpu,        mrD->cpu_diff, &mrD->cpu_avrg);
    imcfreq_read_copy(&m.imc.c,  mr2->imc,  mr1->imc,        mrD->imc_diff, &mrD->imc_avrg);
     bwidth_read_copy(&m.ram.c,  mr2->ram,  mr1->ram, NULL, &mrD->ram_diff, &mrD->ram_avrg);
      cache_read_copy(&m.cch.c, &mr2->cch, &mr1->cch,       &mrD->cch_diff, &mrD->cch_avrg);
      flops_read_copy(&m.flp.c, &mr2->flp, &mr1->flp,       &mrD->flp_diff, &mrD->flp_avrg);
       temp_read     (&m.tmp.c,                              mrD->tmp_diff, &mrD->tmp_avrg);
        cpi_read_copy(&m.cpi.c, &mr2->cpi, &mr1->cpi,       &mrD->cpi_diff, &mrD->cpi_avrg);
        gpu_read_copy(&m.gpu.c,  mr2->gpu,  mr1->gpu,        mrD->gpu_diff                );
}

void metrics_data_alloc(metrics_read_t *mr1, metrics_read_t *mr2, metrics_diff_t *mrD)
{
    // Read 1
    cpufreq_data_alloc(&mr1->cpu, empty);
    imcfreq_data_alloc(&mr1->imc, empty);
     bwidth_data_alloc(&mr1->ram);
        gpu_data_alloc(&mr1->gpu);
    // Read 2
    cpufreq_data_alloc(&mr2->cpu, empty);
    imcfreq_data_alloc(&mr2->imc, empty);
     bwidth_data_alloc(&mr2->ram);
        gpu_data_alloc(&mr2->gpu);
    // Diff
    cpufreq_data_alloc(empty, &mrD->cpu_diff);
    imcfreq_data_alloc(empty, &mrD->imc_diff);
       temp_data_alloc(&mrD->tmp_diff);
        gpu_data_alloc(&mrD->gpu_diff);
}

#if TEST
static topology_t     tp;
static strtable_t     st;
static strscreen_t    ss;
static metrics_read_t mr1;
static metrics_read_t mr2;
static metrics_diff_t mrD;

static char *ear_logo()
{
    char *logo = "    _________    ____ \n"
                 "   / ____/   |  / __ \\\n"
                 "  / __/ / /| | / /_/ /\n"
                 " / /___/ ___ |/ _, _/ \n"
                 "/_____/_/  |_/_/ |_|  \n";
    return logo;
}

static state_t monitor_apis_init(void *whatever)
{
    char buffer[16384];

    topology_init(&tp);

    metrics_init(&tp, EARD);
    metrics_data_alloc(&mr1, &mr2, &mrD);
    metrics_read(&mr1);

    wprintf_init(&ss, 50, 180, 0, '#'); // 0 to 49, 0 to 179
    wprintf_divide(&ss, (int[]) {  1,   2}, (int[]) { 34,  39}, 3, "cpufreq"  );
    wprintf_divide(&ss, (int[]) { 36,   2}, (int[]) { 37,  39}, 4, "imcfreq");
    wprintf_divide(&ss, (int[]) { 39,   2}, (int[]) { 40,  39}, 5, "bandwidth");
    wprintf_divide(&ss, (int[]) { 42,   2}, (int[]) { 43,  39}, 6, "temperature");
    wprintf_divide(&ss, (int[]) {  1, 154}, (int[]) {  6, 177}, 0, "ear-logo" );
    wprintf_divide(&ss, (int[]) {  8, 124}, (int[]) { 17, 177}, 1, "api-table");
    wprintf_divide(&ss, (int[]) { 19, 154}, (int[]) { 34, 177}, 2, "topology" );

    topology_tostr(&tp, buffer, sizeof(buffer));
    wsprintf(&ss, 2, 0, 0, buffer);

    tprintf_init2(&st, fdout, STR_MODE_COL, "13 9 8 10 20");
    tsprintf(buffer, &st, 0, "E.API||I.API||#Devs||Scope||Granularity");
    tsprintf(buffer, &st, 1, "-----||-----||-----||-----||-----------");
    tsprintf(buffer, &st, 1, "%s||%s||%u||%s||%s", m.cpu.info.layer, m.cpu.info.api_str, m.cpu.info.devs_count, m.cpu.info.scope_str, m.cpu.info.granularity_str);
    tsprintf(buffer, &st, 1, "%s||%s||%u||%s||%s", m.imc.info.layer, m.imc.info.api_str, m.imc.info.devs_count, m.imc.info.scope_str, m.imc.info.granularity_str);
    tsprintf(buffer, &st, 1, "%s||%s||%u||%s||%s", m.ram.info.layer, m.ram.info.api_str, m.ram.info.devs_count, m.ram.info.scope_str, m.ram.info.granularity_str);
    tsprintf(buffer, &st, 1, "%s||%s||%u||%s||%s", m.cch.info.layer, m.cch.info.api_str, m.cch.info.devs_count, m.cch.info.scope_str, m.cch.info.granularity_str);
    tsprintf(buffer, &st, 1, "%s||%s||%u||%s||%s", m.flp.info.layer, m.flp.info.api_str, m.flp.info.devs_count, m.flp.info.scope_str, m.flp.info.granularity_str);
    tsprintf(buffer, &st, 1, "%s||%s||%u||%s||%s", m.tmp.info.layer, m.tmp.info.api_str, m.tmp.info.devs_count, m.tmp.info.scope_str, m.tmp.info.granularity_str);
    tsprintf(buffer, &st, 1, "%s||%s||%u||%s||%s", m.cpi.info.layer, m.cpi.info.api_str, m.cpi.info.devs_count, m.cpi.info.scope_str, m.cpi.info.granularity_str);
    tsprintf(buffer, &st, 1, "%s||%s||%u||%s||%s", m.gpu.info.layer, m.gpu.info.api_str, m.gpu.info.devs_count, m.gpu.info.scope_str, m.gpu.info.granularity_str);

    wsprintf(&ss, 0, 0, 0, ear_logo());
    wsprintf(&ss, 1, 0, 0, buffer    );

    wprintf(&ss);

    return EAR_SUCCESS;
}

static state_t monitor_apis_update(void *whatever)
{
    char b[16384];

    metrics_read_copy(&mr2, &mr1, &mrD);

    wsprintf(&ss, 3, 0, 1, cpufreq_data_tostr(mrD.cpu_diff,  mrD.cpu_avrg, b, sizeof(b)));
    wsprintf(&ss, 4, 0, 1, imcfreq_data_tostr(mrD.imc_diff, &mrD.imc_avrg, b, sizeof(b)));
    wsprintf(&ss, 5, 0, 1,  bwidth_data_tostr(mrD.ram_diff,  mrD.ram_avrg, b, sizeof(b)));
    wsprintf(&ss, 6, 0, 1,    temp_data_tostr(mrD.tmp_diff,  mrD.tmp_avrg, b, sizeof(b)));
    wprintf(&ss);

    #if 0
    printf("—— cpi ————————————————————————————————————————————————————————————————————————\n");
        cpi_data_print(&mrD.cpi_diff,  mrD.cpi_avrg, verb_channel);
    printf("—— cache ——————————————————————————————————————————————————————————————————————\n");
      cache_data_print(&mrD.cch_diff,  mrD.cch_avrg, verb_channel);
    printf("—— gpu ————————————————————————————————————————————————————————————————————————\n");
        gpu_data_print( mrD.gpu_diff,                verb_channel);
    #endif
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
	monitor_init();
	suscription_t *sus = suscription();
	sus->call_init     = monitor_apis_init; 
	sus->call_main     = monitor_apis_update;
	sus->time_relax    = 2000;
	sus->time_burst    = 2000;
	sus->suscribe(sus);

    sleep(100);

    return 0;
}
#endif