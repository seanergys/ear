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

#include <stdio.h>
#include <pthread.h>
#include <cuda_runtime.h>
#include <common/output/debug.h>
    
static struct cudaDeviceProp prop;
static pthread_t thread;
static int       is_running;

__global__ void kcuda_kernel()
{
    const long long delay_time = 10000000000;
    long long start = clock64();
    while (clock64() < (start + delay_time));
}

extern "C" {
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
    //while (1)
    {
        is_running = 1;
        kcuda_kernel<<<prop.multiProcessorCount, 64>>>();
        if (cudaDeviceSynchronize() != cudaSuccess) {
            printf("Kernel error: %s\n", cudaGetErrorString(cudaGetLastError()));
        }
        is_running = 0;
    }

    return NULL;
}

int kcuda_execute(int dev)
{
    if (cudaGetDeviceProperties(&prop, dev) != cudaSuccess) {
        debug("GPU%d error: %s", dev, cudaGetErrorString(cudaGetLastError()));
        return 0;
    }
    pthread_create(&thread, NULL, static_kcuda_execute, NULL);    
    return 1;
}
}
