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

//#define SHOW_DEBUGS 1

#include <stdlib.h>
#include <common/output/debug.h>
#include <common/output/verbose.h>
#include <common/system/monitor.h>
#include <common/utils/strtable.h>
#include <common/utils/strscreen.h>
#include <common/config/config_install.h>
#include <daemon/local_api/eard_api.h>
#include <management/management.h>

static manages_apis_t m;

void management_init(topology_t *tp, int eard, manages_apis_t **man)
{
    mgt_cpufreq_load(tp, eard      );
    mgt_cpuprio_load(tp, eard      );
    mgt_imcfreq_load(tp, eard, NULL);
     mgt_cpupow_load(tp, eard      );
        mgt_gpu_load(    eard      );

    mgt_cpufreq_init(no_ctx);
    mgt_cpuprio_init(no_ctx);
    mgt_imcfreq_init(no_ctx);
     mgt_cpupow_init(      );
        mgt_gpu_init(no_ctx);

    // The next 4 paragraphs are obsolete
    mgt_cpufreq_get_api(&m.cpu.api);
    mgt_cpuprio_get_api(&m.pri.api);
    mgt_imcfreq_get_api(&m.imc.api);
        mgt_gpu_get_api(&m.gpu.api);

    apis_tostr(m.cpu.api, m.cpu.api_str, 32);
    apis_tostr(m.pri.api, m.pri.api_str, 32);
    apis_tostr(m.imc.api, m.imc.api_str, 32);
    apis_tostr(m.gpu.api, m.gpu.api_str, 32);

    sprintf(m.cpu.gran_str, "threads");
    sprintf(m.pri.gran_str, "threads");
    sprintf(m.imc.gran_str, "sockets");
    sprintf(m.gpu.gran_str, "gpus");

    mgt_cpufreq_count_devices(no_ctx, &m.cpu.devs_count);
    mgt_cpuprio_count_devices(no_ctx, &m.pri.devs_count);
    mgt_imcfreq_count_devices(no_ctx, &m.imc.devs_count);
        mgt_gpu_count_devices(no_ctx, &m.gpu.devs_count);

    // New and powerful get_info()
    mgt_cpupow_get_info(&m.pow.info);

    //          | list1         | list2             | list3
    // -------- | ------------- | ----------------- | -----------------
    // cpufreq  | avail. ps.    | current ps.       | set index
    // cpuprio  | avail. clos   | current/set assoc | -
    // imcfreq  | avail. ps.    | current ps.       | set index
    // gpu      | devices       | current khz/watts | current khz/watts

    mgt_gpu_get_devices(no_ctx, (gpu_devs_t **) &m.gpu.list1, &m.gpu.list1_count);

    // These current lists are not allocated
	mgt_cpufreq_get_available_list(no_ctx, (const pstate_t **) &m.cpu.list1, &m.cpu.list1_count);
    mgt_imcfreq_get_available_list(no_ctx, (const pstate_t **) &m.imc.list1, &m.imc.list1_count);

    // Current data allocation
    mgt_cpufreq_data_alloc((pstate_t  **) &m.cpu.list2, (uint **) &m.cpu.list3);
    mgt_cpuprio_data_alloc((cpuprio_t **) &m.pri.list1, (uint **) &m.pri.list2);
    mgt_imcfreq_data_alloc((pstate_t  **) &m.imc.list2, (uint **) &m.imc.list3);
        mgt_gpu_data_alloc((ulong     **) &m.gpu.list2                        );
        mgt_gpu_data_alloc((ulong     **) &m.gpu.list3                        );

    // Counting data
    mgt_cpuprio_data_count(&m.pri.list1_count, &m.pri.list2_count);

    if (man != NULL) {    
        *man = &m;
    }
}

#if TEST
static topology_t  tp;
static strscreen_t ss;
static strtable_t  st;

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
    char buffer[8196];

    topology_init(&tp);

    management_init(&tp, EARD, NULL);

    wprintf_init(&ss, 50, 180, 0, '#');
    wprintf_divide(&ss, (int[]) {  1,   2}, (int[]) { 14,  39}, 0);
    wprintf_divide(&ss, (int[]) { 15,   2}, (int[]) { 29,  39}, 4);
    wprintf_divide(&ss, (int[]) { 30,   2}, (int[]) { 44,  39}, 5);
    wprintf_divide(&ss, (int[]) {  1,  39}, (int[]) { 44,  77}, 3);
    wprintf_divide(&ss, (int[]) {  8, 123}, (int[]) { 15, 178}, 1);
    wprintf_divide(&ss, (int[]) {  1, 153}, (int[]) {  7, 178}, 2);

    tprintf_init2(&st, fdout, STR_MODE_COL, "13 9 8 10 20");
    tsprintf(buffer, &st, 0, "E.API||I.API||#Devs||Scope||Granularity");
    tsprintf(buffer, &st, 1, "-----||-----||-----||-----||-----------");
    tsprintf(buffer, &st, 1, "CPUFREQ||%s||%u||node||%s", m.cpu.api_str, m.cpu.devs_count, m.cpu.gran_str);
    tsprintf(buffer, &st, 1, "CPUPRIO||%s||%u||node||%s", m.pri.api_str, m.pri.devs_count, m.pri.gran_str);
    tsprintf(buffer, &st, 1, "IMCFREQ||%s||%u||node||%s", m.imc.api_str, m.imc.devs_count, m.imc.gran_str);
    tsprintf(buffer, &st, 1, "GPU    ||%s||%u||node||%s", m.gpu.api_str, m.gpu.devs_count, m.gpu.gran_str);

    wsprintf(&ss, 1, 0, buffer    );
    wsprintf(&ss, 2, 0, ear_logo());

    return EAR_SUCCESS;
}

static state_t monitor_apis_update(void *whatever)
{
    char buffer[8196];

    #define append(buffer) &buffer[strlen(buffer)]

    buffer[0] = '\0';
    sprintf(buffer, "__ cpufreq _____________________________________________\n");
    mgt_cpufreq_data_tostr(m.cpu.list1, m.cpu.list1_count, append(buffer), 8196);
    wsprintf(&ss, 0, 0, buffer);

    sprintf(buffer, "__ cpuprio _____________________________________________\n");
    mgt_cpuprio_data_tostr(m.pri.list1, m.pri.list2, append(buffer), 8196);
    wsprintf(&ss, 4, 0, buffer);

    sprintf(buffer, "__ cpupow  _____________________________________________\n");
    mgt_cpupow_tdp_tostr(CPUPOW_SOCKET, m.pow.list1, append(buffer), 8196);
    wsprintf(&ss, 5, 0, buffer);

    sprintf(buffer, "__ imcfreq _____________________________________________\n");
    mgt_imcfreq_data_tostr(m.imc.list1, m.imc.list1_count, append(buffer), 8196);
    wsprintf(&ss, 3, 0, buffer);

    wprintf(&ss);

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