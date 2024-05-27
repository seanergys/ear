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

#ifndef _EARL_POLICY_CTX_H
#define _EARL_POLICY_CTX_H

#include <common/config.h>
#include <common/states.h>
#include <common/types/loop.h>
#include <common/types/application.h>
#include <common/types/signature.h>
#include <daemon/shared_configuration.h>
#include <library/api/mpi_support.h>
#if MPI
#include <mpi.h>
#endif


#if MPI
typedef struct mpi_ctx {
	MPI_Comm 	master_comm;
	MPI_Comm 	comm;
} mpi_ctx_t;
#else
typedef struct mpi_ctx {
  int  master_comm;
  int  comm;
} mpi_ctx_t;
#endif


typedef struct node_freqs {
	ulong *cpu_freq; 			// Freq in KHz
	ulong *imc_freq;     	// Pstates , no frequency
	ulong *gpu_freq;      // Freq in KHz (?)
	ulong *gpu_mem_freq;
} node_freqs_t;


typedef struct policy_context
{
	int							 affinity; // Unused
	int							 pc_limit;
	int							 mpi_enabled;
	uint						 num_pstates;
	uint						 use_turbo;
	uint						 num_gpus;
	ulong						 user_selected_freq; // Unused
	ulong						 reset_freq_opt;
	ulong						*ear_frequency;
	settings_conf_t *app;
	resched_t				*reconfigure;
	ctx_t						 gpu_mgt_ctx;
	mpi_ctx_t				 mpi;
} polctx_t;

void print_policy_ctx(polctx_t *p);

typedef struct node_freq_domain {
	uint cpu;
	uint mem;
	uint gpu;
	uint gpumem;
} node_freq_domain_t;

#define POL_GRAIN_NODE 0
#define POL_GRAIN_CORE 1
#define POL_DEFAULT    2
#define POL_NOT_SUPPORTED 100
#define ALL_PROCESSES     -1
#define NO_PROCESS        -2

typedef struct policy_symbols {
    state_t (*init)               (polctx_t *c);
    state_t (*node_policy_apply)  (polctx_t *c, signature_t *my_sig, node_freqs_t *freqs,int *ready);
    state_t (*app_policy_apply)   (polctx_t *c, signature_t *my_sig, node_freqs_t *freqs,int *ready);
    state_t (*get_default_freq)   (polctx_t *c, node_freqs_t *freq_set, signature_t *s);
    state_t (*ok)                 (polctx_t *c, signature_t *curr_sig, signature_t *prev_sig, int *ok);
    state_t (*max_tries)          (polctx_t *c, int *intents);
    state_t (*end)                (polctx_t *c);
    state_t (*loop_init)          (polctx_t *c, loop_id_t *loop_id);
    state_t (*loop_end)           (polctx_t *c, loop_id_t *loop_id);
    state_t (*new_iter)           (polctx_t *c, signature_t *sig);
    state_t (*mpi_init)           (polctx_t *c, mpi_call call_type, node_freqs_t *freqs, int *process_id);
    state_t (*mpi_end)            (polctx_t *c, mpi_call call_type, node_freqs_t *freqs, int *process_id);
    state_t (*configure)          (polctx_t *c);
    state_t (*domain)             (polctx_t *c, node_freq_domain_t *domain);
	state_t (*io_settings)        (polctx_t *c, signature_t *my_sig, node_freqs_t *freqs);
  state_t (*cpu_gpu_settings) (polctx_t *c,signature_t *my_sig,node_freqs_t *freqs);
	state_t (*busy_wait_settings) (polctx_t *c, signature_t *my_sig, node_freqs_t *freqs);
	state_t (*restore_settings)   (polctx_t *c, signature_t *my_sig, node_freqs_t *freqs);
} polsym_t;

void compute_avg_sel_freq(ulong * avg_sel_mem_f, ulong *imcfreqs);

void policy_show_domain(node_freq_domain_t *dom);

/** Verboses the policy context \p policy_ctx if the verbosity level
 * of the job is equal or higher than \p verb_lvl. */
void verbose_policy_ctx(int verb_lvl, polctx_t *policy_ctx);

#endif // _EARL_POLICY_CTX_H
