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

extern "C"
{
//#define SHOW_DEBUGS 1

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <cuda_runtime.h>
#include <common/output/debug.h>
#include <common/utils/string.h>
 
static pthread_t  thread;
static int        is_running;
static uint      *list_doing;
static uint       devs_count;

__global__ void kcuda_kernel()
{
    const long long delay_time = 10000000000;
    long long start = clock64();
    while (clock64() < (start + delay_time));
}

int kcuda_is_running()
{
    return is_running;
}

int kcuda_count_devices()
{
    int devs_count = 0;
    cudaGetDeviceCount(&devs_count);
    return devs_count;
}

static void *static_kcuda_execute(void *x)
{
    static struct cudaDeviceProp prop;
    int d;  
 
    while (1)
    {
        is_running = 1;

        for (d = 0; d < devs_count; ++d) {
            debug("Running in GPU%d? %u", d, list_doing[d]);
            if (!list_doing[d]) {
                continue;
            }
            if (cudaSetDevice(d)) {
                printf("GPU%d error: %s\n", d, cudaGetErrorString(cudaGetLastError()));
                list_doing[d] = 0;
                continue;
            }
            if (cudaGetDeviceProperties(&prop, d) != cudaSuccess) {
                printf("GPU%d error: %s\n", d, cudaGetErrorString(cudaGetLastError()));
                list_doing[d] = 0;
                continue;
            }
            kcuda_kernel<<<prop.multiProcessorCount, 64>>>();
        }
        if (cudaDeviceSynchronize() != cudaSuccess) {
            printf("CUDA error: %s\n", cudaGetErrorString(cudaGetLastError()));
            return NULL;
        }
        cudaDeviceReset();
        is_running = 0;
        sleep(2);
    }

    return NULL;
}

int kcuda_execute(char *conf)
{
    devs_count = kcuda_count_devices();
    list_doing = (uint *) calloc(devs_count, sizeof(uint));
    // 
    if (!rantoa(conf, list_doing, devs_count)) {
        // If the list is empty, we are selecting the GPU0.
        list_doing[0] = 1;
    }
    pthread_create(&thread, NULL, static_kcuda_execute, NULL);    
    return 1;
}
}
