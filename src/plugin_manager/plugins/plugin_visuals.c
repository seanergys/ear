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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <metrics/metrics.h>
#include <metrics/energy_cpu/energy_cpu.h>
#include <common/utils/special.h>
#include <common/utils/strtable.h>
#include <common/utils/strscreen.h>
#include <plugin_manager/plugins/plugin_rapl.h>
#include <plugin_manager/plugins/plugin_rapl_alerts.h>
#include <plugin_manager/plugins/plugin_metrics.h>
#include <plugin_manager/plugins/plugin_visuals.h>

static topology_t    tp;
static strtable_t    st;
static strscreen_t   ss;
static mets_t        *m;
static rapl_t        *r;
static rapl_alerts_t *a;
// Terminal windows
static int            id_logo;
static int            id_apis;
static int            id_topo;
static int            id_cpu;
static int            id_imc;
static int            id_ram;
static int            id_temp;
static int            id_rapl;
static int            id_alert;

declr_up_init()
{
    char b[16384];

    wprintf_init(&ss, 50, 180, -1, '#'); // 0 to 49, 0 to 179
    wprintf_divide(&ss, (int[]) {  1, 154}, (int[]) {  6, 177}, &id_logo , "ear-logo"   );
    wprintf_divide(&ss, (int[]) {  8, 124}, (int[]) { 17, 177}, &id_apis , "api-table"  );
    wprintf_divide(&ss, (int[]) { 19, 154}, (int[]) { 34, 177}, &id_topo , "topology"   );
    wprintf_divide(&ss, (int[]) {  1,   2}, (int[]) { 34,  39}, &id_cpu  , "cpufreq"    );
    wprintf_divide(&ss, (int[]) { 36,   2}, (int[]) { 37,  39}, &id_imc  , "imcfreq"    );
    wprintf_divide(&ss, (int[]) { 39,   2}, (int[]) { 40,  39}, &id_ram  , "bandwidth"  );
    wprintf_divide(&ss, (int[]) { 42,   2}, (int[]) { 43,  39}, &id_temp , "temperature");
    wprintf_divide(&ss, (int[]) {  1,  41}, (int[]) {  2,  79}, &id_rapl , "rapl"       );
    wprintf_divide(&ss, (int[]) {  4,  41}, (int[]) {  5,  79}, &id_alert, "rapl-alerts");

    topology_init(&tp);
    topology_tostr(&tp, b, sizeof(b));

    wsprintf(&ss, id_topo, 0, 0, b);
    wsprintf(&ss, id_logo, 0, 0, special_ear_logo());

    return EAR_SUCCESS;
}

declr_up_get_tag ()
{
    *tag = "metrics,rapl-alerts";
}

declr_up_execute()
{
    static int one_time = 0;
    char b[16384];

    if (strcmp(tag, "metrics") == 0) {
        m = (mets_t *) data;

        if (!one_time) {
            tprintf_init2(&st, fderr, STR_MODE_COL, "13 9 8 10 20");
            tsprintf(b, &st, 0, "E.API||I.API||#Devs||Scope||Granularity");
            tsprintf(b, &st, 1, "-----||-----||-----||-----||-----------");
            tsprintf(b, &st, 1, "%s||%s||%u||%s||%s", m->mi.cpu.layer, m->mi.cpu.api_str, m->mi.cpu.devs_count, m->mi.cpu.scope_str, m->mi.cpu.granularity_str);
            tsprintf(b, &st, 1, "%s||%s||%u||%s||%s", m->mi.imc.layer, m->mi.imc.api_str, m->mi.imc.devs_count, m->mi.imc.scope_str, m->mi.imc.granularity_str);
            tsprintf(b, &st, 1, "%s||%s||%u||%s||%s", m->mi.ram.layer, m->mi.ram.api_str, m->mi.ram.devs_count, m->mi.ram.scope_str, m->mi.ram.granularity_str);
            tsprintf(b, &st, 1, "%s||%s||%u||%s||%s", m->mi.cch.layer, m->mi.cch.api_str, m->mi.cch.devs_count, m->mi.cch.scope_str, m->mi.cch.granularity_str);
            tsprintf(b, &st, 1, "%s||%s||%u||%s||%s", m->mi.flp.layer, m->mi.flp.api_str, m->mi.flp.devs_count, m->mi.flp.scope_str, m->mi.flp.granularity_str);
            tsprintf(b, &st, 1, "%s||%s||%u||%s||%s", m->mi.tmp.layer, m->mi.tmp.api_str, m->mi.tmp.devs_count, m->mi.tmp.scope_str, m->mi.tmp.granularity_str);
            tsprintf(b, &st, 1, "%s||%s||%u||%s||%s", m->mi.cpi.layer, m->mi.cpi.api_str, m->mi.cpi.devs_count, m->mi.cpi.scope_str, m->mi.cpi.granularity_str);
            tsprintf(b, &st, 1, "%s||%s||%u||%s||%s", m->mi.gpu.layer, m->mi.gpu.api_str, m->mi.gpu.devs_count, m->mi.gpu.scope_str, m->mi.gpu.granularity_str);
            wsprintf(&ss, id_apis, 0, 0, b);
            one_time = 1;
        }
        wsprintf(&ss, id_cpu, 0, 1, cpufreq_data_tostr(m->mrD.cpu_diff,  m->mrD.cpu_avrg, b, sizeof(b)));
        wsprintf(&ss, id_imc, 0, 1, imcfreq_data_tostr(m->mrD.imc_diff, &m->mrD.imc_avrg, b, sizeof(b)));
        wsprintf(&ss, id_ram , 0, 1,  bwidth_data_tostr(m->mrD.ram_diff,  m->mrD.ram_avrg, b, sizeof(b)));
        wsprintf(&ss, id_temp   , 0, 1,    temp_data_tostr(m->mrD.tmp_diff,  m->mrD.tmp_avrg, b, sizeof(b)));
    }
    if (strcmp(tag, "rapl") == 0) {
        r = (rapl_t *) data;
        sprintf(b, "%llu W", r->power);
        wsprintf(&ss, id_rapl, 0, 1, b);
    }
    if (strcmp(tag, "rapl-alerts") == 0) {
        a = (rapl_alerts_t *) data;
        sprintf(b, "STATUS: %s", a->alert ? "<red>ALERT!</red>": "<green>OK</green>");
        wsprintf(&ss, id_alert, 0, 1, b);
    }

    return EAR_SUCCESS;
}

declr_up_data_tostr()
{
    return wprintf(&ss);
}