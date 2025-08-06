/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#ifndef METRICS_GPU_H
#define METRICS_GPU_H

#include <common/types.h>
#include <common/states.h>
#include <common/plugins.h>
#include <common/system/time.h>
#include <metrics/common/apis.h>

typedef struct gpu_devs_s {
    ullong serial;
    uint index;
} gpu_devs_t;

typedef struct gpu_s {
    timestamp_t time;
    ulong samples;
    ulong freq_gpu; // khz
    ulong freq_mem; // khz
    ulong util_gpu; // percent
    ulong util_mem; // percent
    ulong temp_gpu; // celsius
    ulong temp_mem; // celsius
    double energy_j;
    double power_w;
    uint working;
    uint correct;
} gpu_t;

typedef struct gpu_ops_s {
    void    (*unload)      ();
    void    (*get_info)    (apinfo_t *info);
    void    (*get_devices) (gpu_devs_t **devs, uint *devs_count);
    void    (*set_monitoring_mode) (int mode);
    state_t (*read)        (gpu_t *data);
    double  (*read_raw)    (gpu_t *data);
    void    (*data_diff)   (gpu_t *data2, gpu_t *data1, gpu_t *data_diff);
} gpu_ops_t;

// API building scheme
#define GPU_F_LOAD(name)        void gpu_##name##_load(gpu_ops_t *ops, int options)
#define GPU_F_UNLOAD(name)      void gpu_##name##_unload()
#define GPU_F_GET_INFO(name)    void gpu_##name##_get_info(apinfo_t *info)
#define GPU_F_GET_DEVICES(name) void gpu_##name##_get_devices(gpu_devs_t **devs, uint *devs_count)
#define GPU_F_SET_MONITORING_MODE(name) void gpu_##name##_set_monitoring_mode(int mode)
#define GPU_F_READ(name)        state_t gpu_##name##_read(gpu_t *d)
#define GPU_F_READ_RAW(name)    state_t gpu_##name##_read_raw(gpu_t *d)
#define GPU_F_DATA_DIFF(name)   void gpu_##name##_data_diff(gpu_t *d2, gpu_t *d1, gpu_t *dD)

#define GPU_DEFINES(name)  \
    GPU_F_LOAD(name);      \
    GPU_F_UNLOAD(name);    \
    GPU_F_GET_INFO(name);  \
    GPU_F_GET_DEVICES(name); \
    GPU_F_SET_MONITORING_MODE(name); \
    GPU_F_READ(name);      \
    GPU_F_READ_RAW(name);  \
    GPU_F_DATA_DIFF(name);

// Discovers the low level API.
void gpu_load(int options);

void gpu_unload();

// Returns all available static information (Work in progress)
void gpu_get_info(apinfo_t *info);

// Information about devices. Freeing devs allocation is your responsibility.
void gpu_get_devices(gpu_devs_t **devs, uint *devs_count);

// You can use it to increase the monitoring rate.
void gpu_set_monitoring_mode(int mode);

// Reads the GPU device data and stores it in the gpu_t array data (1 per device).
state_t gpu_read(gpu_t *data);

// Reads the GPU device data directly from the hardware (not pooled).
state_t gpu_read_raw(gpu_t *data);

// Performs a gpu_read() over data2, a gpu_data_diff() and copies data2 in data1.
state_t gpu_read_diff(gpu_t *data2, gpu_t *data1, gpu_t *data_diff);

// Performs a gpu_read() over data2, a gpu_data_diff() and copies data2 in data1.
state_t gpu_read_copy(gpu_t *data2, gpu_t *data1, gpu_t *data_diff);

/** Helpers */
// Substracts the elements of the gpu_t array (data_diff = data2 - data1).
void gpu_data_diff(gpu_t *data2, gpu_t *data1, gpu_t *data_diff);

// Makes an average of all the elements of the data_diff array.
void gpu_data_merge(gpu_t *data_diff, gpu_t *data_merge);

// Allocates an array of gpu_t (1 per device).
void gpu_data_alloc(gpu_t **data);

// Frees an array of gpu_t.
void gpu_data_free(gpu_t **data);

// Sets to 0 an array of gpu_t.
void gpu_data_null(gpu_t *data);

// Copies an array of gpu_t.
void gpu_data_copy(gpu_t *data_dst, gpu_t *data_src);

// Prints an array of gpu_t in the channel fd.
void gpu_data_print(gpu_t *data, int fd);

// Copies the printing string in a char buffer.
char *gpu_data_tostr(gpu_t *data, char *buffer, int length);

int gpu_is_supported();

#endif