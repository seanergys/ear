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

#ifndef METRICS_GPUPROF_H
#define METRICS_GPUPROF_H

#include <common/types.h>
#include <common/states.h>
#include <common/system/time.h>
#include <metrics/common/apis.h>

typedef struct gpuprof_s {
	timestamp_t time;
    ullong     *values;
} gpuprof_t;

typedef struct gpuprof_evs_s {
    char name[64];
    uint  id;
} gpuprof_evs_t;

// API building scheme
#define GPUPROF_F_LOAD(name)             void name (gpuprof_ops_t *ops)
#define GPUPROF_F_GET_INFO(name)         void name (apinfo_t *info)
#define GPUPROF_F_EVENTS_GET(name)       void name (const gpuprof_evs_t **evs, uint *evs_count)
#define GPUPROF_F_EVENTS_SET(name)       void name (int device, char *evs, gpuprof_t *data)
#define GPUPROF_F_READ(name)             state_t name (gpuprof_t *data)
#define GPUPROF_F_READ_RAW(name)         state_t name (gpuprof_t *data)

#define GPUPROF_DEFINES(name) \
GPUPROF_F_LOAD             (gpuprof_ ##name ##_load); \
GPUPROF_F_GET_INFO         (gpuprof_ ##name ##_get_info); \
GPUPROF_F_EVENTS_GET       (gpuprof_ ##name ##_events_get); \
GPUPROF_F_EVENTS_SET       (gpuprof_ ##name ##_events_set); \
GPUPROF_F_READ             (gpuprof_ ##name ##_read); \
GPUPROF_F_READ_RAW         (gpuprof_ ##name ##_read_raw); \

typedef struct gpuprof_ops_s {
    GPUPROF_F_GET_INFO((*get_info));
    GPUPROF_F_EVENTS_GET((*events_get));
    GPUPROF_F_EVENTS_SET((*events_set));
    GPUPROF_F_READ((*read));
    GPUPROF_F_READ_RAW((*read_raw));
} gpuprof_ops_t;

void gpuprof_load(int force_api);

void gpuprof_get_info(apinfo_t *info);

void gpuprof_events_get(const gpuprof_evs_t **evs, uint *evs_count);

// Events is a list in id, short or long name format. Device can be all_devs.
void gpuprof_events_set(int device, char *evs, gpuprof_t *data);

state_t gpuprof_read(gpuprof_t *data);

state_t gpuprof_read_raw(gpuprof_t *data);

state_t gpuprof_read_diff(gpuprof_t *data2, gpuprof_t *data1, gpuprof_t *data_diff);

state_t gpuprof_read_copy(gpuprof_t *data2, gpuprof_t *data1, gpuprof_t *data_diff);

// Data
void gpuprof_data_diff(gpuprof_t *data2, gpuprof_t *data1, gpuprof_t *data_diff);

void gpuprof_data_alloc(gpuprof_t **data);

void gpuprof_data_free(gpuprof_t **data);

void gpuprof_data_null(gpuprof_t *data);

void gpuprof_data_copy(gpuprof_t *data_dst, gpuprof_t *data_src);

void gpuprof_data_print(gpuprof_t *data, int fd);

char *gpuprof_data_tostr(gpuprof_t *data, char *buffer, int length);

#endif
