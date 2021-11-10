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

#include <library/common/verbose_lib.h>
#include <library/metrics/metrics_fresh.h>

// state assert
#define sa(function) \
    if (state_fail(function)) { \
        serror(#function " FAILED"); \
        exit(0); \
    }

typedef struct metstatic_s {
    pstate_t *cpu_avail_list;
    uint      cpu_avail_count;
    pstate_t *cpu_curr_list;
    uint      cpu_devs_count;
    uint      cpu_nominal_idx;
    uint      cpu_api;
    pstate_t *imc_avail_list;
    uint      imc_avail_count;
    pstate_t *imc_curr_list;
    uint      imc_devs_count;
    uint      imc_api;
} metstatic_t;

metstatic_t m;

const metstatic_t *metstatic_get()
{
    return &m;
}

void metrics_fresh_init(topology_t *tp)
{
    char buffer[SZ_BUFFER_EXTRA];

    // Load
    sa(mgt_cpufreq_load(tp, EARD));
    sa(mgt_imcfreq_load(tp, EARD));
    // Get API
    sa(mgt_cpufreq_get_api(&m.cpu_api));
    sa(mgt_imcfreq_get_api(&m.imc_api));
    // Init
    sa(mgt_imcfreq_init(no_ctx));
    sa(mgt_imcfreq_init(no_ctx));
    // Count (cache does not need)
    sa(mgt_cpufreq_count_devices(no_ctx, &m.cpu_devs_count));
    sa(mgt_imcfreq_count_devices(no_ctx, &m.imc_devs_count));
    // Availability
    sa(mgt_cpufreq_get_available_list(no_ctx, (const pstate_t **) &m.cpu_avail_list, &m.cpu_avail_count));
    sa(mgt_imcfreq_get_available_list(no_ctx, (const pstate_t **) &m.imc_avail_list, &m.imc_avail_count));
    // Current list allocation
    sa(mgt_cpufreq_data_alloc(&m.cpu_curr_list, empty));
    sa(mgt_imcfreq_data_alloc(&m.imc_curr_list, empty));
    // Nominal CPU P_STATE
    sa(mgt_cpufreq_get_nominal(no_ctx, &m.cpu_nominal_idx));

    pstate_tostr(m.cpu_avail_list, m.cpu_avail_count, buffer, SZ_BUFFER_EXTRA);
    verbose(2, "CPUFREQ available list of frequencies:\n%s", buffer);
    pstate_tostr(m.imc_avail_list, m.imc_avail_count, buffer, SZ_BUFFER_EXTRA);
    verbose(2, "IMCFREQ available list of frequencies:\n%s", buffer);
}

void metrics_fresh_dispose()
{
    // CPU
    sa(mgt_cpufreq_dispose(no_ctx));
    sa(mgt_imcfreq_dispose(no_ctx));
}
