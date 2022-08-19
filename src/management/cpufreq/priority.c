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

#include <pthread.h>
#include <management/cpufreq/priority.h>
#include <management/cpufreq/archs/prio_eard.h>
#include <management/cpufreq/archs/prio_isst.h>
#include <management/cpufreq/archs/prio_dummy.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static mgt_prio_ops_t  ops;
static uint            init;

void mgt_cpufreq_prio_load(topology_t *tp, int eard)
{
    while (pthread_mutex_trylock(&lock));
    if (init) {
        goto leave;
    }
    mgt_prio_isst_load(tp, &ops);
    mgt_prio_eard_load(tp, &ops, eard);
    mgt_prio_dummy_load(tp, &ops);
    init = 1;
leave:
    pthread_mutex_unlock(&lock);
}

void mgt_cpufreq_prio_get_api(uint *api)
{
    return ops.get_api(api);
}

state_t mgt_cpufreq_prio_init()
{
    return ops.init();
}

state_t mgt_cpufreq_prio_dispose()
{
    return ops.dispose();
}

state_t mgt_cpufreq_prio_enable()
{
    return ops.enable();
}

state_t mgt_cpufreq_prio_disable()
{
    return ops.disable();
}

state_t mgt_cpufreq_prio_get_available_list(cpuprio_t *prio_list)
{
    return ops.get_available_list(prio_list);
}

state_t mgt_cpufreq_prio_set_available_list(cpuprio_t *prio_list)
{
    return ops.set_available_list(prio_list);
}

state_t mgt_cpufreq_prio_get_current_list(uint *idx_list)
{
    return ops.get_current_list(idx_list);
}

state_t mgt_cpufreq_prio_set_current_list(uint *idx_list)
{
    return ops.set_current_list(idx_list);
}

state_t mgt_cpufreq_prio_set_current(uint prio, int cpu)
{
    return ops.set_current(prio, cpu);
}

void mgt_cpufreq_prio_data_count(uint *prios_count, uint *idxs_count)
{
    return ops.data_count(prios_count, idxs_count);
}

void mgt_cpufreq_prio_data_alloc(cpuprio_t **prio_list, uint **idx_list)
{
    // mgt_cpufreq_prio_data_count
    // calloc
    // calloc
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

#if 1
int main(int argc, char *argv[])
{
    topology_t tp;

    topology_init(&tp);

    return 0;
}
#endif
