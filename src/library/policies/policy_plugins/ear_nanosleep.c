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

#include <library/policies/policy_ctx.h>
#include <library/policies/common/mpi_stats_support.h>
#include <library/common/externs.h>
#include <library/common/verbose_lib.h>

#define DEBUG_LVL 0
#define INFO_LVL  DEBUG_LVL

static uint policy_loaded;
static uint optimize_call;
static ullong total_nsecs_sleeping;

// static node_freqs_t def_freqs;


state_t policy_init(polctx_t *c)
{
    if (c != NULL && !policy_loaded) {

        if (is_mpi_enabled()) {

            mpi_app_init(c);

            uint block_type;
            is_blocking_busy_waiting(&block_type);
            verbose_master(INFO_LVL, "Busy waiting MPI: %u", block_type == BUSY_WAITING_BLOCK);

            if (ear_my_rank % 4) {

                optimize_call = 1;

                verbose(INFO_LVL, "MPI rank %d will be optimized on MPI blocking calls.", ear_my_rank);
            } else {
                verbose(INFO_LVL, "MPI rank %d won't be optimized on MPI blocking calls.", ear_my_rank);
            }

            policy_loaded = 1;
        }

        return EAR_SUCCESS;

        /* Configuring default freqs
        node_freqs_alloc(&def_freqs);

        for (int i = 0; i < c->num_processes; i++) {
            def_freqs.cpu_freq[i] = *(c->ear_frequency);
        }

        for (int i = 0; i < c->imc_devs_cnt; sid++) {
            def_freqs.imc_freq[i*IMC_VAL+IMC_MAX] = imc_min_pstate[sid];
            def_freqs.imc_freq[i*IMC_VAL+IMC_MIN] = imc_max_pstate[sid];
        }
        */

    } else if (!c) {
        return_msg(EAR_ERROR, Generr.input_null);
    } else {
        return_msg(EAR_ERROR, Generr.api_initialized);
    }
}


state_t policy_end(polctx_t *c)
{
    if (c != NULL) {
        if (optimize_call) {
            verbose(INFO_LVL, "Total time sleeping (ns): %llu", total_nsecs_sleeping);
        }
        return mpi_app_end(c);
    } else {
        return_msg(EAR_ERROR, Generr.input_null);
    }
}

#if 0
state_t policy_domain(polctx_t *c, node_freq_domain_t *domain)
{
    if (domain) {
        domain->cpu    = POL_GRAIN_CORE;
        domain->mem    = POL_NOT_SUPPORTED;

        domain->gpu    = POL_NOT_SUPPORTED;

        domain->gpumem = POL_NOT_SUPPORTED;

        return EAR_SUCCESS;
    } else {
        return_msg(EAR_ERROR, Generr.input_null);
    }

}
#endif

state_t policy_mpi_init(polctx_t *c, mpi_call call_type, node_freqs_t *freqs, int *process_id)
{

    if (c) {

        state_t st;

        if ((st = mpi_call_init(c, call_type)) == EAR_SUCCESS) {

            if (optimize_call && is_mpi_blocking(call_type)) {

                // struct timespec mpi_opt_timer = {.tv_nsec = 4000000}; // 4 ms
                                                                      //
                total_nsecs_sleeping += 3000000;

                // nanosleep(&mpi_opt_timer, NULL);
                usleep(3000);
            }

        } else {
            return st;
        }

    } else {
        return_msg(EAR_ERROR, Generr.input_null);
    }

    return EAR_SUCCESS;
}

state_t policy_mpi_end(polctx_t *c, mpi_call call_type, node_freqs_t *freqs, int *process_id)
{
    if (c) {
        return mpi_call_end(c, call_type);
    } else {
        return_msg(EAR_ERROR, Generr.input_null);
    }
}
