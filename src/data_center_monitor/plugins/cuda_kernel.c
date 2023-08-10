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

static int devs_count;
static int sms_count;

declr_up_get_tag()
{
    *tag = "cuda_kernel";
    *tags_deps = NULL;
}

declr_up_action_init(_cuda_kernel)
{
    struct cudaDeviceProp prop;

    cudaGetDeviceCount(&devs_count);
    if (devs_count == 0) {
        return "[D]";
    }
    cudaGetDeviceProperties(&prop, 0);
    sms_count = prop.multiProcessorCount; 
    
    return rsprintf("CUDA returned %d devices of %d SMs", devs_count, sms_count);
}
