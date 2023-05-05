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

#include <upm/plugin_rapl.h>
#include <metrics/energy_cpu/energy_cpu.h>

static topology_t tp;
static rapl_t r;

UPM_F_INIT()
{
    topology_init(&tp);

    energy_cpu_load(&tp, NO_EARD);
    energy_cpu_init(no_ctx);
    energy_cpu_data_alloc(no_ctx, &r.data1, &r.devs_count);
    energy_cpu_data_alloc(no_ctx, &r.data2, &r.devs_count);
    energy_cpu_data_alloc(no_ctx, &r.dataD, &r.devs_count);
    energy_cpu_read(no_ctx, r.data1);

    return EAR_SUCCESS;
}

UPM_F_GET_INFO ()
{
    *tag = "rapl";
}

UPM_F_WORK()
{
    energy_cpu_read(no_ctx, r.data2);
    energy_cpu_data_diff(no_ctx, r.data2, r.data1, r.dataD);
    energy_cpu_data_copy(no_ctx, r.data1, r.data2);
    r.power = (ullong) energy_cpu_compute_power(r.dataD[1], 2);
    return EAR_SUCCESS;
}

UPM_F_DATA_ALLOC()
{
    *data = &r;
}