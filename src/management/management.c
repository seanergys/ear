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

    // The next 5 paragraphs are obsolete
    m.cpu.info.layer = "MGT_CPUFREQ";
    m.pri.info.layer = "MGT_CPUPRIO";
    m.imc.info.layer = "MGT_IMCFREQ";
    m.gpu.info.layer = "MGT_GPU";

    mgt_cpufreq_get_api(&m.cpu.info.api);
    mgt_cpuprio_get_api(&m.pri.info.api);
    mgt_imcfreq_get_api(&m.imc.info.api);
        mgt_gpu_get_api(&m.gpu.info.api);

    m.cpu.info.granularity = GRANULARITY_THREAD;
    m.pri.info.granularity = GRANULARITY_THREAD;
    m.imc.info.granularity = GRANULARITY_SOCKET;
    m.gpu.info.granularity = GRANULARITY_PERIPHERAL;

    m.cpu.info.scope = SCOPE_NODE;
    m.pri.info.scope = SCOPE_NODE;
    m.imc.info.scope = SCOPE_NODE;
    m.gpu.info.scope = SCOPE_NODE;

    mgt_cpufreq_count_devices(no_ctx, &m.cpu.info.devs_count);
    mgt_cpuprio_count_devices(no_ctx, &m.pri.info.devs_count);
    mgt_imcfreq_count_devices(no_ctx, &m.imc.info.devs_count);
        mgt_gpu_count_devices(no_ctx, &m.gpu.info.devs_count);

    // New and powerful get_info()
    mgt_cpupow_get_info(&m.pow.info);

    apinfo_tostr(&m.cpu.info);
    apinfo_tostr(&m.pri.info);
    apinfo_tostr(&m.pow.info);
    apinfo_tostr(&m.imc.info);
    apinfo_tostr(&m.gpu.info);

    // Get available frequencies
    mgt_cpufreq_get_available_list(no_ctx, (const pstate_t **) &m.cpu.list1, &m.cpu.list1_count);
    mgt_imcfreq_get_available_list(no_ctx, (const pstate_t **) &m.imc.list1, &m.imc.list1_count);

    // Get available priorities
    mgt_cpuprio_data_alloc((cpuprio_t **) &m.pri.list1, (uint **) &m.pri.list2);
    mgt_cpuprio_get_available_list(m.pri.list1);

    // Get current TDPs
    mgt_cpupow_data_alloc(CPUPOW_SOCKET, (uint **) &m.pow.list1              );
    mgt_cpupow_tdp_get(CPUPOW_SOCKET, m.pow.list1);

    // Get GPUs information
    mgt_gpu_get_devices(no_ctx, (gpu_devs_t **) &m.gpu.list1, &m.gpu.list1_count);

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
    char buffer[16384];

    topology_init(&tp);

    management_init(&tp, EARD, NULL);

    wprintf_init(&ss, 50, 180, 0, '#'); // 0 to 49, 0 to 179
    wprintf_divide(&ss, (int[]) {  1,   2}, (int[]) { 14,  39}, 3, "cpufreq"  );
    wprintf_divide(&ss, (int[]) { 16,   2}, (int[]) { 29,  39}, 4, "cpuprio"  );
    wprintf_divide(&ss, (int[]) { 31,   2}, (int[]) { 44,  39}, 5, "cpupow"   );
    wprintf_divide(&ss, (int[]) {  1,  41}, (int[]) { 44,  77}, 6, "imcfreq"  );
    wprintf_divide(&ss, (int[]) {  1, 154}, (int[]) {  6, 177}, 0, "ear-logo" );
    wprintf_divide(&ss, (int[]) {  8, 124}, (int[]) { 14, 177}, 1, "api-table");
    wprintf_divide(&ss, (int[]) { 16, 154}, (int[]) { 31, 177}, 2, "topology" );

    topology_tostr(&tp, buffer, sizeof(buffer));
    wsprintf(&ss, 2, 0, 0, buffer);

    tprintf_init2(&st, fdout, STR_MODE_COL, "13 9 8 10 20");
    tsprintf(buffer, &st, 0, "E.API||I.API||#Devs||Scope||Granularity");
    tsprintf(buffer, &st, 1, "-----||-----||-----||-----||-----------");
    tsprintf(buffer, &st, 1, "%s||%s||%u||%s||%s", m.cpu.info.layer, m.cpu.info.api_str, m.cpu.info.devs_count, m.cpu.info.scope_str, m.cpu.info.granularity_str);
    tsprintf(buffer, &st, 1, "%s||%s||%u||%s||%s", m.pri.info.layer, m.pri.info.api_str, m.pri.info.devs_count, m.pri.info.scope_str, m.pri.info.granularity_str);
    tsprintf(buffer, &st, 1, "%s||%s||%u||%s||%s", m.pow.info.layer, m.pow.info.api_str, m.pow.info.devs_count, m.pow.info.scope_str, m.pow.info.granularity_str);
    tsprintf(buffer, &st, 1, "%s||%s||%u||%s||%s", m.imc.info.layer, m.imc.info.api_str, m.imc.info.devs_count, m.imc.info.scope_str, m.imc.info.granularity_str);
    tsprintf(buffer, &st, 1, "%s||%s||%u||%s||%s", m.gpu.info.layer, m.gpu.info.api_str, m.gpu.info.devs_count, m.gpu.info.scope_str, m.gpu.info.granularity_str);

    wsprintf(&ss, 0, 0, 0, ear_logo());
    wsprintf(&ss, 1, 0, 0, buffer    );

    mgt_cpufreq_data_tostr(m.cpu.list1, m.cpu.list1_count, buffer, sizeof(buffer));
    wsprintf(&ss, 3, 0, 1, buffer);

    mgt_cpuprio_data_tostr(m.pri.list1, m.pri.list2, buffer, sizeof(buffer));
    wsprintf(&ss, 4, 0, 1, buffer);

    mgt_cpupow_tdp_tostr(CPUPOW_SOCKET, m.pow.list1, buffer, sizeof(buffer));
    wsprintf(&ss, 5, 0, 1, buffer);

    mgt_imcfreq_data_tostr(m.imc.list1, m.imc.list1_count, buffer, sizeof(buffer));
    wsprintf(&ss, 6, 0, 1, buffer);

    wprintf(&ss);

    return EAR_SUCCESS;
}

static state_t monitor_apis_update(void *whatever)
{
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