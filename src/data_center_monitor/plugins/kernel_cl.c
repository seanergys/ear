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

#include <CL/cl.h>
#include <unistd.h>
#include <pthread.h>
#include <common/utils/string.h>
#include <common/system/plugin_manager.h>

#define N 8

static pthread_t  thread;
static uint       devs_running[N];
static uint       devs_count = 4;
static uint       is_running = 1;
static uint       dev;

declr_up_get_tag()
{
    *tag = "kernel_cl";
    *tags_deps = NULL;
}

int kcl_count_devices()
{
    return 8;
}

static void *static_kernel_execute(void *x)
{
    FILE             *fp;
    char             *source_str;
    size_t            source_size;
    size_t            global_size;
    size_t            local_size;
    cl_platform_id    platform_id;
    cl_device_id      device_id[N];
    cl_uint           ret_num_devices;
    cl_uint           ret_num_platforms;
    cl_int            err;
    cl_context        context;
    cl_command_queue  command_queue[N];
    cl_program        program;
    cl_kernel         kernel;
    cl_mem            r_mem_obj;
    int              *result;   
    int               d; 

    #define LIST_SIZE         1024
    #define MAX_SOURCE_SIZE  (0x100000)

    // Loading source
    fp          = fopen("./kernel_cl_ptx.cl", "r");
    source_str  = (char *) malloc(MAX_SOURCE_SIZE);
    source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
    fclose(fp);

    // Context -> Program -> Kernel -> CommandQueue
    platform_id   = NULL;
    err           = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
    err           = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ALL, devs_count, device_id, &ret_num_devices);
    result        = (int *) malloc(sizeof(int) * LIST_SIZE);
    global_size   = LIST_SIZE;
    local_size    = 64;
    
  
    // Main program 
    while (is_running)
    {
        context       = clCreateContext(NULL, ret_num_devices, device_id, NULL, NULL, &err);
        r_mem_obj     = clCreateBuffer(context, CL_MEM_WRITE_ONLY, LIST_SIZE * sizeof(int), NULL, &err);
        program       = clCreateProgramWithSource(context, 1, (const char **) &source_str, (const size_t *) &source_size, &err);
        err           = clBuildProgram(program, 1, device_id, NULL, NULL, NULL);
        kernel        = clCreateKernel(program, "cl_kernel", &err);
        err           = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *) &r_mem_obj);

        for (d = 0; d < ret_num_devices; ++d) {
            if (!devs_running[d]) {
                continue;
            }
            command_queue[d] = clCreateCommandQueue(context, device_id[d], 0, &err);
            err = clEnqueueNDRangeKernel(command_queue[d], kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
            err = clEnqueueReadBuffer(command_queue[d], r_mem_obj, CL_TRUE, 0, LIST_SIZE * sizeof(int), result, 0, NULL, NULL);
            err = clFlush(command_queue[d]);
            err = clFinish(command_queue[d]);
            err = clReleaseCommandQueue(command_queue[d]);
            err = clReleaseKernel(kernel);
            err = clReleaseProgram(program);
            err = clReleaseMemObject(r_mem_obj);
            err = clReleaseContext(context);
            #if 0 
            int i;
            for(i = 0; i < LIST_SIZE; i++) {
                printf("R[%d] = %d\n", i, result[i]);
            }
            #endif
        } 
        sleep(10);
    }
    // Clean up
    free(result);

    return NULL;
}

declr_up_action_init(_kernel_cl)
{
    char **args = (char **) data;
    // Running devices
    if (args == NULL || !rantoa(args[0], devs_running, devs_count)) {
        // If the list is empty, we are selecting the GPU0.
        devs_running[0] = 1;
    }
    if (args != ) {}
    pthread_create(&thread, NULL, static_kernel_execute, NULL);
    return "Launched kernel";
}
