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

#include <library/metrics/metrics_fresh.h>

// state assert
#define sa(function) \
    if (state_fail(f)) { \
        serror(#f " FAILED"); \
        exit(0); \
    }

// general
// mgt_cpufreq
static pstate_t *mcpu_available_list;
static pstate_t *mcpu_current_list;
static uint     *mcpu_set_list;
       uint      mcpu_available_count;
static uint      mcpu_devs_count;
static uint      mcpu_api;
static uint      mcpu_nominal_pstate;
// mgt_imcfreq
static pstate_t *mimc_available_list;
static pstate_t *mimc_current_list;
static uint     *mimc_set_list;
static uint      mimc_available_count;
static uint      mimc_devs_count;
static uint      mimc_api;

// TODO: tp may be a topology_t*, where it is declared?
void metrics_fresh_init()
{
    // Load
    sa(mgt_cpufreq_load(tp, EARD));
    sa(mgt_imcfreq_load(tp, EARD));
    // Get API
    sa(mgt_cpufreq_get_api(&mcpu_api));
    sa(mgt_imcfreq_get_api(&mimc_api));
    // Init
    sa(mgt_imcfreq_init(no_ctx));
    sa(mgt_imcfreq_init(no_ctx));
    // Count (cache does not need)
    sa(mgt_cpufreq_count_devices(no_ctx, &mcpu_devs_count));
    sa(mgt_imcfreq_count_devices(no_ctx, &mimc_devs_count));
    // Availability
    sa(mgt_cpufreq_get_available_list(no_ctx, (const pstate_t **) &mcpu_available_list, &mcpu_available_count));
    sa(mgt_imcfreq_get_available_list(no_ctx, (const pstate_t **) &mimc_available_list, &mimc_available_count));
    // Current list allocation
    sa(mgt_cpufreq_data_alloc(&mcpu_current_list, &mcpu_set_list));
    sa(mgt_imcfreq_data_alloc(&mimc_current_list, &mimc_set_list));
    // Nominal CPU P_STATE
    sa(mgt_cpufreq_get_nominal(no_ctx, &mcpu_nominal_pstate));
}

void metrics_fresh_dispose()
{
    // CPU
    sa(mgt_cpufreq_dispose(no_ctx));
    sa(mgt_imcfreq_dispose(no_ctx));
}
