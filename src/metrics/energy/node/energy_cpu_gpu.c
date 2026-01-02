/*********************************************************************
 * Copyright (c) 2024 Energy Aware Solutions, S.L
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **********************************************************************/

// #define SHOW_DEBUGS 1
#define VCPU_GPU 3

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <common/config.h>
#include <common/states.h> //clean
#include <metrics/energy/energy_node.h>
#include <metrics/energy/node/energy_cpu_gpu.h>
#include <metrics/energy/node/energy_nm.h>

static uint gpu_num         = 0;
static uint gpu_num_no_priv = 0;
#define NUM_DOMAINS_CPU_GPU 2 /* CPU and MEM */

typedef struct cpu_gpu {
    ulong ac_energy;
    ullong cpu_energy[MAX_SOCKETS_SUPPORTED * NUM_DOMAINS_CPU_GPU];
    gpu_t gpu_energy[MAX_GPUS_SUPPORTED];
    uint gpu_num;
} cpu_gpu_t;

#define plugin_exit_action return EAR_ERROR

/*
 * MAIN FUNCTIONS
 */
static ullong *energy_cpu_data = NULL;
static ullong *energy_cpu_diff = NULL;
static uint energy_cpu_count   = 0;
static gpu_t *gpu_data         = NULL;
static uint cpu_energy_loaded = 0, gpu_energy_loaded = 0;

ENERGY_NODE_F_LOAD(cpu_gpu)
{
    topology_t tp;
    state_t s;

    if (c == NULL) {
        return EAR_ERROR;
    }

    /* CPU load */
    verbose(VCPU_GPU, "CPU-GPU plugin: loaded");
    if (!cpu_energy_loaded) {
        verbose(VCPU_GPU, "CPU-GPU plugin: loading CPU");
        state_assert(s, topology_init(&tp), plugin_exit_action);
        state_assert(s, energy_cpu_load(&tp, NO_EARD), plugin_exit_action);
        state_assert(s, energy_cpu_init(no_ctx), plugin_exit_action);
        state_assert(s, energy_cpu_data_alloc(no_ctx, &energy_cpu_diff, &energy_cpu_count), plugin_exit_action);
        state_assert(s, energy_cpu_data_alloc(no_ctx, &energy_cpu_data, &energy_cpu_count), plugin_exit_action);
        cpu_energy_loaded = 1;
        verbose(VCPU_GPU, "CPU-GPU plugin: CPU load ok");
    }

    if (!gpu_energy_loaded) {
        verbose(VCPU_GPU, "CPU-GPU plugin: loading GPU");
        // Loading GPU
        gpu_load(NO_EARD);
        gpu_data_alloc(&gpu_data);
        gpu_get_devices(NULL, &gpu_num);
        gpu_energy_loaded = 1;
        verbose(VCPU_GPU, "CPU-GPU plugin: GPU load ok %u GPUs", gpu_num);
    }
    verbose(VCPU_GPU, "CPU-GPU plugin: PLUGIN loaded");
    return EAR_SUCCESS;
}

ENERGY_NODE_F_UNLOAD(cpu_gpu)
{
    if ((c == NULL) || (*c == NULL))
        return EAR_ERROR;
    if (cpu_energy_loaded) {
        energy_cpu_dispose(no_ctx);
        cpu_energy_loaded = 0;
        /* Free data allocated */
        energy_cpu_data_free(no_ctx, &energy_cpu_data);
    }
    if (gpu_energy_loaded) {
        gpu_energy_loaded = 0;
        /* Free data allocated */
        gpu_data_free(&gpu_data);
        gpu_unload();
    }
    return EAR_SUCCESS;
}

uint energy_cpu_gpu_is_privileged()
{
    return 0;
}

ENERGY_NODE_F_DATASIZE(cpu_gpu)
{
    verbose(VCPU_GPU, "CPU-GPU plugin:data size %lu", sizeof(cpu_gpu_t));
    *size = sizeof(cpu_gpu_t);
    return EAR_SUCCESS;
}

ENERGY_NODE_F_FREQUENCY(cpu_gpu)
{
    verbose(VCPU_GPU, "CPU-GPU plugin: freq");
    *freq_us = 1000000;
    return EAR_SUCCESS;
}

ENERGY_NODE_F_DC_READ(cpu_gpu)
{
    verbose(VCPU_GPU, "CPU-GPU plugin: READ");
    cpu_gpu_t *penergy_mj = (cpu_gpu_t *) energy_mj;

    if (!cpu_energy_loaded || !gpu_energy_loaded)
        return EAR_ERROR;

    debug("energy_dc_read\n");

    if (penergy_mj == NULL)
        return EAR_ERROR;

    /* CPU */
    energy_cpu_read(no_ctx, energy_cpu_data);

    /* GPU */
    penergy_mj->gpu_num = (uint) gpu_num;
    gpu_read(gpu_data);

    /* COPY data */
    energy_cpu_data_copy(no_ctx, penergy_mj->cpu_energy, energy_cpu_data);
    gpu_data_copy(penergy_mj->gpu_energy, gpu_data);

    /* Energy is reported in mJ but we cannot convert here because we will not be able to compute the accum later */

    return EAR_SUCCESS;
}

ENERGY_NODE_F_DC_TIME_READ(cpu_gpu)
{
    verbose(VCPU_GPU, "CPU-GPU plugin: TIME ENERGY READ");
    if (!cpu_energy_loaded || !gpu_energy_loaded)
        return EAR_ERROR;
    /* Time */
    struct timeval t;
    *time_ms = 0;
    gettimeofday(&t, NULL);
    *time_ms = t.tv_sec * 1000 + t.tv_usec / 1000;

    /* Energy */
    energy_cpu_gpu_dc_read(c, energy_mj);
    return EAR_SUCCESS;
}

ENERGY_NODE_F_AC_READ(cpu_gpu)
{
    cpu_gpu_t *penergy_mj = (cpu_gpu_t *) energy_mj;

    if (penergy_mj == NULL)
        return EAR_ERROR;
    if (!cpu_energy_loaded || !gpu_energy_loaded)
        return EAR_ERROR;

    penergy_mj->ac_energy = 0;
    return EAR_SUCCESS;
}

ENERGY_NODE_F_UNITS(cpu_gpu)
{
    verbose(VCPU_GPU, "CPU-GPU plugin: Units");
    *units = 1000;
    return EAR_SUCCESS;
}

ENERGY_NODE_F_ACCUMULATED(cpu_gpu)
{
    verbose(VCPU_GPU, "CPU-GPU plugin: energy_accumulated");
    cpu_gpu_t *pinit = (cpu_gpu_t *) init;
    cpu_gpu_t *pend  = (cpu_gpu_t *) end;
    gpu_t gpu_diff[MAX_GPUS_SUPPORTED];
    ulong total_gpu_energy  = 0;
    ullong total_cpu_energy = 0;

    if (e == NULL || pinit == NULL || pend == NULL)
        return EAR_ERROR;

    verbose(VCPU_GPU, "CPU-GPU plugin: CPU energy");
    /* CPU energy */
    energy_cpu_data_diff(no_ctx, pinit->cpu_energy, pend->cpu_energy, energy_cpu_diff);
    for (uint i = 0; i < MAX_SOCKETS_SUPPORTED; i++) {
        verbose(VCPU_GPU, "CPU-GPU: CPU energy %llu ", energy_cpu_diff[i]);
        total_cpu_energy += energy_cpu_diff[i];
    }
    verbose(VCPU_GPU, "CPU-GPU: CPU energy %llu", total_cpu_energy);

    verbose(VCPU_GPU, "CPU-GPU plugin: GPU energy");
    /* GPU energy */
    if (pinit->gpu_num) {
        verbose(VCPU_GPU, "Computing energy per GPU (%u GPUs)", pinit->gpu_num);
        // gpu_system_data_diff(pend->gpu_energy, pinit->gpu_energy, gpu_diff);
        gpu_data_diff(pend->gpu_energy, pinit->gpu_energy, gpu_diff);
        for (uint i = 0; i < pinit->gpu_num; i++) {
            verbose(VCPU_GPU, "CPU-GPU: GPU[%d]=%lu acum %lu", i, (unsigned long) gpu_diff[i].energy_j,
                    total_gpu_energy);
            total_gpu_energy += (unsigned long) gpu_diff[i].energy_j;
        }
    }

    /* Accumulated */
    total_cpu_energy = total_cpu_energy / 1000000;
    total_gpu_energy = total_gpu_energy * 1000;
    *e               = (ulong) ((ulong) total_cpu_energy + (ulong) total_gpu_energy);
    verbose(VCPU_GPU, "CPU-GPU: total_cpu_energy %llu total_gpu_energy %lu: total %lu mJ", total_cpu_energy,
            total_gpu_energy, *e);
    return EAR_SUCCESS;
}

static char cpu_buff[1024];
static char gpu_buff[1024];

ENERGY_NODE_F_TO_STR(cpu_gpu)
{
    verbose(VCPU_GPU, "CPU-GPU:  to str");
    cpu_gpu_t *pe = (cpu_gpu_t *) energy_mj;

    if (pe == NULL)
        return EAR_ERROR;

    energy_cpu_to_str(no_ctx, cpu_buff, pe->cpu_energy);
    gpu_data_tostr(pe->gpu_energy, gpu_buff, sizeof(gpu_buff));

    sprintf(str, "CPU data (%s)\n GPU (%s)", cpu_buff, gpu_buff);

    verbose(VCPU_GPU, "energy_to_str:%s", str);
    return EAR_SUCCESS;
}

ENERGY_NODE_F_IS_NULL(cpu_gpu)
{
    verbose(VCPU_GPU, "CPU-GPU: energy_data_is_null");
    cpu_gpu_t *pe = (cpu_gpu_t *) energy_mj;

    if (!pe) {
        verbose(VCPU_GPU, "CPU-GPU: Energy data is a null pointer.");
        return 1;
    }

    for (int i = 0; i < MAX_SOCKETS_SUPPORTED * NUM_DOMAINS_CPU_GPU; i++) {
        if (pe->cpu_energy[i] != 0)
            return 0;
    }

    for (int i = 0; i < MAX_GPUS_SUPPORTED; i++) {
        if (pe->gpu_energy[i].power_w != 0)
            return 0;
    }

    return 1;
}

ENERGY_NODE_F_POWER_LIMIT(cpu_gpu)
{
    return EAR_ERROR;
}

ENERGY_NODE_F_LOAD_PLUGIN(cpu_gpu)
{
    ops->load          = energy_cpu_gpu_load;
    ops->unload        = energy_cpu_gpu_unload;
    ops->datasize      = energy_cpu_gpu_datasize;
    ops->frequency     = energy_cpu_gpu_frequency;
    ops->dc_read       = energy_cpu_gpu_dc_read;
    ops->dc_time_read  = energy_cpu_gpu_dc_time_read;
    ops->ac_read       = energy_cpu_gpu_ac_read;
    ops->units         = energy_cpu_gpu_units;
    ops->accumulated   = energy_cpu_gpu_accumulated;
    ops->energy_to_str = energy_cpu_gpu_to_str;
    ops->power_limit   = energy_cpu_gpu_power_limit;
    ops->is_null       = energy_cpu_gpu_data_is_null;
    return EAR_SUCCESS;
}

state_t energy_not_privileged_init()
{
    state_t s;
    topology_t tp;

    /* We must initialize data */
    if (!cpu_energy_loaded) {
        verbose(VCPU_GPU, "CPU-GPU plugin: loading CPU not privileged");
        state_assert(s, topology_init(&tp), plugin_exit_action);
        state_assert(s, energy_cpu_load(&tp, EARD), plugin_exit_action);
        state_assert(s, energy_cpu_init(no_ctx), plugin_exit_action);
        state_assert(s, energy_cpu_data_alloc(no_ctx, &energy_cpu_diff, &energy_cpu_count), plugin_exit_action);
        state_assert(s, energy_cpu_data_alloc(no_ctx, &energy_cpu_data, &energy_cpu_count), plugin_exit_action);
        cpu_energy_loaded = 1;
    }
    /* GPU */
    if (!gpu_energy_loaded) {
        verbose(VCPU_GPU, "CPU-GPU plugin: loading GPU not privileged");
        // Loading GPU
        gpu_load(EARD);
        // gpu_count_system_devices(no_ctx, &gpu_num_no_priv);
        gpu_data_alloc(&gpu_data);
        gpu_get_devices(NULL, &gpu_num_no_priv);
        gpu_num = gpu_num_no_priv;
        verbose(VCPU_GPU, "CPU-GPU plugin:  detected %u GPUs\n", gpu_num_no_priv);
        gpu_energy_loaded = 1;
    }

    return EAR_SUCCESS;
}
