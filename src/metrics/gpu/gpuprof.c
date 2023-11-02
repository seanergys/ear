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
#include <metrics/gpu/gpuprof.h>
#include <metrics/gpu/archs/gpuprof_dcgmi.h>
#include <metrics/gpu/archs/gpuprof_dummy.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static gpuprof_ops_t ops;
//static uint          devs_count;
static uint          ok_load;

void gpuprof_load(int force_api)
{
    while (pthread_mutex_trylock(&lock));
    if (ok_load) {
        goto unlock_load;
    }
    if (API_IS(force_api, API_DUMMY)) {
        goto dummy;
    }
    //gpuprof_rocm_load(&ops, force_api);
    //gpuprof_nvml_load(&ops, force_api);
    gpuprof_dcgmi_load(&ops);
dummy:
    gpuprof_dummy_load(&ops);
    ok_load = 1;
unlock_load:
    pthread_mutex_unlock(&lock);
}

void gpuprof_get_info(apinfo_t *info)
{
    info->layer = "GPUPROF";
    ops.get_info(info);
}

void gpuprof_events_get(const gpuprof_evs_t **evs, uint *evs_count)
{
    ops.events_get(evs, evs_count);
}

void gpuprof_events_set(char *evs, int group)
{

}

void gpuprof_events_multiplex()
{

}

state_t gpuprof_read(gpuprof_t *data)
{
    return ops.read(data);
}

state_t gpuprof_read_raw(gpuprof_t *data)
{
    return ops.read_raw(data);
}

state_t gpuprof_read_diff(gpuprof_t *data2, gpuprof_t *data1, gpuprof_t *data_diff)
{
    state_t s;
    if (state_fail(s = gpuprof_read(data2))) {
        return s;
    }
    gpuprof_data_diff(data2, data1, data_diff);
    return s;
}

state_t gpuprof_read_copy(gpuprof_t *data2, gpuprof_t *data1, gpuprof_t *data_diff)
{
    state_t s;
    if (state_fail(s = gpuprof_read_diff(data2, data1, data_diff))) {
        return s;
    }
    gpuprof_data_copy(data1, data2);
    return s;
}

// Data
void gpuprof_data_diff(gpuprof_t *data2, gpuprof_t *data1, gpuprof_t *data_diff)
{
}

void gpuprof_data_alloc(gpuprof_t **data)
{
}

void gpuprof_data_free(gpuprof_t **data)
{
}

void gpuprof_data_null(gpuprof_t *data)
{
}

void gpuprof_data_copy(gpuprof_t *data_dst, gpuprof_t *data_src)
{
}

void gpuprof_data_print(gpuprof_t *data, int fd)
{
}

char *gpuprof_data_tostr(gpuprof_t *data, char *buffer, int length)
{
    return NULL;
}

#include <metrics/common/apis.h>

static const gpuprof_evs_t *evs;
static uint evs_count;
static apinfo_t info;

int main(int argc, char *argv[])
{
    int i;
    gpuprof_load(0);
    gpuprof_get_info(&info);
    apinfo_tostr(&info);
    printf("%s %s %s\n", info.layer, info.api_str, info.granularity_str);
    gpuprof_events_get(&evs, &evs_count);
    for (i = 0; i < evs_count; ++i) {
        printf("EV%d: %s.name %u.id\n", i, evs[i].name, evs[i].id);
    }

    return 0;
}