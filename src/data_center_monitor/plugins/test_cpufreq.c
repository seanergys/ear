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

#include <string.h>
#include <common/states.h>
#include <common/output/debug.h>
#include <data_center_monitor/plugins/keyboard.h>
#include <data_center_monitor/plugins/management.h>

static manages_info_t *m;
static char buffer[4096];

declr_up_get_tag()
{
    *tag = "test_cpufreq";
    *tags_deps = "!management,!keyboard";
}

declr_up_action_init(_management)
{
    m = (manages_info_t *) data;
    debug("Received management data");
    return NULL;
}

static void governors_print(char **params)
{
    static uint govs[1024];
    int g;

    if (state_fail(mgt_cpufreq_governor_get_list(no_ctx, govs))) {
        debug("mgt_cpufreq_governor_get_list: %s", state_msg);
    }
    for (g = 0; g < m->cpu.devs_count; ++g) {
        governor_tostr(govs[g], buffer);
        printf("%12s ", buffer);
        if (g != 0 && ((g+1) % 4) == 0) {
            printf("\n");
        }
    }
    if (((g) % 4) != 0) {
        printf("\n");
    }
}

static void governors_set(char **params)
{
    uint governor;
    governor_toint(params[0], &governor);
    debug("changing to governor: %u", governor);
    if (state_fail(mgt_cpufreq_governor_set(no_ctx, governor))) {
        debug("mgt_cpufreq_governor_set: %s", state_msg);
    }
    governors_print(NULL);
}

static void freqs_print(char **params)
{
    pstate_t freqs[1024];
    if (state_fail(mgt_cpufreq_get_current_list(no_ctx, freqs))) {
        debug("mgt_cpufreq_get_current_list: %s", state_msg);
    }
    mgt_cpufreq_data_print(freqs, m->cpu.devs_count, verb_channel);
}

static void freqs_set(char **params)
{
    pstate_t freqs[1024];
    if (state_fail(mgt_cpufreq_set_current(no_ctx, atoi(params[0]), atoi(params[1])))) {
        debug("mgt_cpufreq_get_current_list: %s", state_msg);
    }
    freqs_print(NULL);
}

declr_up_action_init(test_cpufreq)
{
    command_register("governors_print", "", governors_print);
    command_register("governors_set", "%s", governors_set);
    command_register("freqs_print", "", freqs_print);
    command_register("freqs_set", "%s %s", freqs_set);
    return NULL;
}