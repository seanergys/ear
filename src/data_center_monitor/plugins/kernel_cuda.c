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

#include <cuda_runtime.h>
#include <common/utils/string.h>
#include <common/system/plugin_manager.h>

int kcuda_is_running();
int kcuda_count_devices();
int kcuda_execute(char **conf);

declr_up_get_tag()
{
    *tag = "kernel_cuda";
    *tags_deps = NULL;
}

declr_up_action_init(_kernel_cuda)
{
    char **args = (char **) data;
    
    if (!kcuda_count_devices()) {
        return "[D] no GPUs detected";
    }
    if (!kcuda_execute(args)) {
        return "[D] something is not working";
    }
    return rsprintf("%d GPUs detected", kcuda_count_devices());
}

declr_up_action_periodic(_kernel_cuda)
{
    //return NULL;
    return rsprintf("Is running CUDA kernel? %d", kcuda_is_running());
}
