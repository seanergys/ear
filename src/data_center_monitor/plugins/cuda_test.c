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
#include <data_center_monitor/plugins/management.h>

// Metrics
static metrics_read_t     mr1;
static metrics_read_t     mr2;
static metrics_diff_t     mrD;
// Information
static int                devs_count;
static int                sms_count;
static const ulong      **freq_list; // KHz
static const uint        *freq_list_count;
static ulong             *freq_list_cur;
static ulong             *freq_list_def;
static ulong              *pcap_list_max; // W
static ulong              *pcap_list_min;
static ulong              *pcap_list_def;
static ulong              *pcap_list_cur;
static ulong               pcap_current;
static int                 cfg_dev;

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
// Cuda Cores (CC) (FP32)      | 14.592 | 5.120  |
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
    int i, j;
    struct cudaDeviceProp prop;

    // Configuration
    if (getenv("GPU_TEST_DEV")) {
        cfg_dev = atoi(getenv("GPU_TEST_DEV")); // 0 by default
    }

    // We are using CUDA Runtime API
    cudaGetDeviceCount(&devs_count);
    if (devs_count > 0) {
        cudaGetDeviceProperties(&prop, 0);
        sms_count = prop.multiProcessorCount; 
    }
    // Allocation
    mgt_gpu_data_alloc(&pcap_list_min);
    mgt_gpu_data_alloc(&pcap_list_max);
    mgt_gpu_data_alloc(&pcap_list_cur);
    mgt_gpu_data_alloc(&pcap_list_def);
    mgt_gpu_data_alloc(&freq_list_cur);
    mgt_gpu_data_alloc(&freq_list_def);
    // Frequency configuration
    mgt_gpu_freq_get_available(no_ctx, &freq_list, &freq_list_count);
    mgt_gpu_freq_limit_get_current(no_ctx, freq_list_def);
    // Powercap configuration
    mgt_gpu_power_cap_get_rank(no_ctx, pcap_list_min, pcap_list_max); 
    mgt_gpu_power_cap_get_current(no_ctx, pcap_list_def);

    for (i = 0; i < devs_count && i < 1; ++i) {
        fprintf(stderr, "GPU%d: %lu W to %lu W\n", i, pcap_list_min[i], pcap_list_max[i]);
        for (j = 0; j < freq_list_count[i]; ++j) {
            //fprintf(stderr, "GPU%d: PS%d %lu KHz\n", i, j, freq_list[i][j]);
        }
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

static char *set_powercap(int dev, ulong pcap)
{
    static char buffer[512];
    mgt_gpu_power_cap_get_current(no_ctx, pcap_list_cur);
    pcap_list_cur[dev] = pcap;
    //mgt_gpu_freq_limit_set(no_ctx, pcap_list_cur);
    sprintf(buffer, "GPU%d: powercap to %lu W", dev, pcap);
    return buffer;
}

static int test_powercap(int dev, char **output)
{
    static long p = 0;
    long pcap;

    pcap = (long) pcap_list_max[dev]-p; 
    if (pcap <= pcap_list_min[dev]) {
        *output = set_powercap(dev, pcap_list_def[dev]);
        p = 0;
        return 0;
    }
    *output = set_powercap(dev, (ulong) pcap);
    p += 20;
    return 1;
}

static char *set_frequency(int dev, ulong freq)
{
    static char buffer[512];
    mgt_gpu_freq_limit_get_current(no_ctx, freq_list_cur);
    freq_list_cur[dev] = freq; 
    mgt_gpu_freq_limit_set(no_ctx, freq_list_cur);
    sprintf(buffer, "GPU%d: frequency to %lu KHz", dev, freq);
    return buffer;
}

static int test_frequency(int dev, char **output)
{
    static int f = 0;
    
    if (f >= freq_list_count[dev]) {
        *output = set_frequency(dev, freq_list_def[dev]);
        f = 0;
        return 0;
    }
    *output = set_frequency(dev, freq_list[dev][f]);
    f += 10;
    return 1;
}

char *test_select()
{
    static int flip_flop = 0;
    char *output = NULL;

    if (!flip_flop) {
        flip_flop = !test_frequency(cfg_dev, &output);
    } else {
        flip_flop = test_powercap(cfg_dev, &output);
    }
    return output;
}

declr_up_action_periodic(_cuda_test)
{
    metrics_data_diff(&mr2, &mr1, &mrD);
    metrics_data_copy(&mr1, &mr2);
    return test_select();
}
