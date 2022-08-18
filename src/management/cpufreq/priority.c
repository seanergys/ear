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

#include <common/output/debug.h>
#include <metrics/common/isst.h>
#include <management/cpufreq/cpufreq.h>

static char      *strprio[2]  = { "high", "low" };
static cpuprio_t  avail; //Dummy available list
static uint       avail_idx;
static uint      *current;
static uint       cpus_count;
static uint       api;

static void dummy_init(topology_t *tp)
{
    const pstate_t *ps_list;
    uint ps_count;
    
    avail[0].max_khz = PRIO_FREQ_MAX;
    avail[0].min_khz = 0LLU;
    avail[0].high    = 1;
    avail[0].low     = 0;
    avail[0].idx     = 0;
    // Dummy priorities
    if (state_ok(mgt_cpufreq_get_available_list(no_ctx, &ps_list, &ps_count))) {
        avail.min_khz = ps_list[ps_count-1].khz;
    }
    // Saving current list
    cpus_count = tp->cpu_count;
    current = calloc(cpus_count, sizeof(uint));
}

void mgt_cpufreq_prio_load(topology_t *tp, int eard)
{
    // TODO: split dummy and isst in archs folder
    if (state_ok(isst_init(tp))) {
        api = API_ISST;
    } else {
        dummy_init(tp);
        api = API_DUMMY;
    }
}

void mgt_cpufreq_prio_get_api(uint *_api)
{
    *_api = api;
}

state_t mgt_cpufreq_prio_init()
{
    return EAR_SUCCESS;
}

state_t mgt_cpufreq_prio_dispose()
{
    if (api == API_ISST) {
        return isst_dispose();
    }
    return EAR_SUCCESS;
}

staet_t mgt_cpufreq_prio_enable()
{
    if (api == API_ISST) {
        return isst_enable();
    }
    return mgt_cpufreq_set_current(no_ctx, avail_idx, all_cpus);
}

staet_t mgt_cpufreq_prio_disable()
{
    if (api == API_ISST) {
        return isst_disable();
    }
    return EAR_SUCCESS;
}

state_t mgt_cpufreq_prio_count_devices(uint *devs_count)
{
    if (api == API_ISST) {
        return isst_assoc_count_devices(devs_count);
    } else {
        *devs_count = cpu_count;
    }
    return EAR_SUCCESS;
}

state_t mgt_cpufreq_prio_get_available_list(cpuprio_t *prio_list)
{
    if (api == API_ISST) {
        return isst_clos_get_list((clos_t *) prio_list); 
    } else {
        memcpy(prio_list, avail, sizeof(cpuprio_t)); 
    }
    return EAR_SUCCESS;
}

state_t mgt_cpufreq_prio_set_available_list(cpuprio_t *prio_list)
{
    if (api == API_ISST) {
        return isst_clos_set_list((clos_t *) prio_list);
    }
    memcpy(avail, prio_list, sizeof(cpuprio_t));
    pstate_freqtoidx_nearest(ps_list, ps_count, avail[0].max_khz, &avail_idx, MODE_UPPER);
    return mgt_cpufreq_prio_set_current(avail_idx, all_cpus);
}

state_t mgt_cpufreq_prio_get_current_list(uint *idx_list)
{
    if (api == API_ISST) {
        return isst_assoc_get_list(idx_list);
    }
    memset((void *) list_idx, 0, sizeof(uint)*cpus_count);
    return EAR_SUCCESS;
}

state_t mgt_cpufreq_prio_set_current_list(uint *idx_list)
{
    if (api == API_ISST) {
        return isst_assoc_set_list(idx_list);
    }
    return EAR_SUCCESS;
}

state_t mgt_cpufreq_prio_set_current(uint prio, int cpu)
{
    if (api == API_ISST) {
        return isst_assoc_set(prio, cpu);
    }
    return EAR_SUCCESS;
}

void mgt_cpufreq_prio_data_count(uint *prios_count, uint *idxs_count)
{
    state_t s;
    if (api == API_ISST) {
        if (state_fail(s = isst_clos_count(prio_count))) {
            return s;
        }
        return isst_assoc_count_devices(idx_count);
    }
    prios_count = 1;
    idxs_count  = cpus_count;
    return EAR_SUCCESS; 
}

void mgt_cpufreq_prio_data_alloc(cpuoprio_t **prio_list, uint **idx_list)
{
    if (api == API_ISST) {
        if (state_fail(s = isst_clos_alloc_list(prio_list))) {
            return s;
        }
        return isst_assoc_alloc_list(idx_list);
    }
    prio_list = calloc(1, sizeof(cpuprio_t));
    idx_list  = calloc(cpus_count, sizeof(uint));
    return EAR_SUCCESS;
}

void mgt_cpufreq_prio_data_print(cpuprio_t *prio_list, uint *idx_list, int fd)
{
    char buffer[1024];
    mgt_cpufreq_prio_data_tostr(prio_list, idx_list, buffer, 1024);
    dprintf(fd, "%s", buffer); 
}

void mgt_cpufreq_prio_data_tostr(cpuprio_t *prio_list, uint *idx_list, char *buffer, int length)
{
}
