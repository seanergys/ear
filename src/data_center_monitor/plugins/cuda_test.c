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
#include <cuda_runtime.h>
#include <common/output/debug.h>
#include <data_center_monitor/plugins/metrics.h>

// Metrics
static metrics_read_t     mr1;
static metrics_read_t     mr2;
static metrics_diff_t     mrD;
// Information
static int                devs_count;
static int                sms_count;

declr_up_get_tag()
{
    *tag = "cuda_test";
    *tags_deps = "!metrics,!management";
}

declr_up_action_init(_metrics)
{
    metrics_data_alloc(&mr1, &mr2, &mrD);
    return NULL;
}

// Nvidia                      | H100   | V100   | Comments
// ----------------------------|--------|--------|-----------------------------------------------------------------------------------------------
// Cuda Cores (CC) (FP32)      | 14.592 | 5120   |
// Stream Multiprocessors (SM) | 114    | 80     | SMs process only one block at time. You need 114/80 blocks to occupy the system. 
// CCs (FP32) per SM           | 128    | ?      | ^This is because shared memory, which is a per block resource.
// Cuda Cores per thread       | 1      | 1      |
// Threads per Warp            | 32     | 32     | Warp is an implementation decision, not part of CUDA programming model.
// Threads per block (TB)      | 1024   | ?      |
// Warps per thread block      | 32     | ?      |
// Concurrent warps per SM     | ?      | 2      | In theory depends on the instruction. 4 FP32 warps fit in an SM. But depends on the schedulers.
// Thread Block Cluster        | ?      | -      | New concept. 

// module load cuda/12.2.0
// nvcc -I ../../ -Xcompiler -fPIC -shared -o cuda_test.so cuda_test.c ../../common/libcommon.a 
// ../edcmon --plugins=cuda_test.so:2000 --paths /home/xjaneas/ngit/ear_private/src/data_center_monitor/plugins

declr_up_action_init(_cuda_test)
{
    struct cudaDeviceProp prop;
    // We are using CUDA Runtime API
    cudaGetDeviceCount(&devs_count);
    if (devs_count > 0) {
        cudaGetDeviceProperties(&prop, 0);
        sms_count = prop.multiProcessorCount; 
    }
    return rsprintf("CUDA returned %d devices of %d SMs", devs_count, sms_count);
}

declr_up_action_periodic(_metrics)
{
    mets_t *mets = (mets_t *) data;
    if (mr1.samples > 0) {
        metrics_data_copy(&mr2, &mets->mr);
    } else {
        metrics_data_copy(&mr1, &mets->mr);
    }
    return NULL;
}

declr_up_action_periodic(_cuda_test)
{
    metrics_data_diff(&mr2, &mr1, &mrD);
    // Launch test
    metrics_data_copy(&mr1, &mr2);
    return NULL;
}
