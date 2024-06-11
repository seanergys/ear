/*
*
* This program is part of the EAR software.
*
* EAR provides a dynamic, transparent and ligth-weigth solution for
* Energy management. It has been developed in the context of the
* Barcelona Supercomputing Center (BSC)&Lenovo Collaboration project.
*
* 
* BSC Contact   mailto:ear-support@bsc.es
* 
*
* EAR is an open source software, and it is licensed under both the BSD-3 license
* and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
* and COPYING.EPL files.
*/


#include <library/policies/policy_api.h>
#include <stdio.h>


state_t policy_init(polctx_t *c)
{
	printf("policy_init\n");
	return EAR_SUCCESS;
}

state_t policy_apply(polctx_t *c, signature_t *my_sig, node_freqs_t *freqs, int *ready)
{
	printf("policy_apply\n");
	return EAR_SUCCESS;
}

state_t policy_app_apply(polctx_t *c, signature_t *my_sig, node_freqs_t *freqs,int *ready)
{
	printf("policy_app_apply\n");
	return EAR_SUCCESS;
}

state_t policy_get_default_freq(polctx_t *c, node_freqs_t *freq_set, signature_t *sig)
{
	printf("policy_get_default_freq\n");
	return EAR_SUCCESS;
}

state_t policy_ok(polctx_t *c, signature_t *curr_sig, signature_t *prev_sig, int *ok)
{
	printf("policy_ok\n");
	return EAR_SUCCESS;
}

state_t policy_max_tries(polctx_t *c, int *intents)
{
	printf("policy_max_tries\n");
	return EAR_SUCCESS;
}

state_t policy_end(polctx_t *c)
{
	printf("policy_end\n");
	return EAR_SUCCESS;
}

state_t policy_loop_init(polctx_t *c, loop_id_t *loop_id)
{
	printf("policy_loop_init\n");
	return EAR_SUCCESS;
}

state_t policy_loop_end(polctx_t *c, loop_id_t *loop_id)
{
	printf("policy_loop_end\n");
	return EAR_SUCCESS;
}

state_t policy_new_iteration(polctx_t *c, signature_t *sig)
{
	printf("policy_new_iteration\n");
	return EAR_SUCCESS;
}

state_t policy_mpi_init(polctx_t *c, mpi_call call_type, node_freqs_t *freqs, int *process_id)
{
	printf("policy_mpi_init\n");
	return EAR_SUCCESS;
}

state_t policy_mpi_end(polctx_t *c, mpi_call call_type, node_freqs_t *freqs, int *process_id)
{
	printf("policy_mpi_end\n");
	return EAR_SUCCESS;
}

state_t policy_configure(polctx_t *c)
{
	printf("policy_configure\n");
	return EAR_SUCCESS;
}

state_t policy_domain(polctx_t *c, node_freq_domain_t *domain)
{
	printf("policy_domain\n");
	return EAR_SUCCESS;
}

state_t policy_io_settings(polctx_t *c, signature_t *my_sig, node_freqs_t *freqs)
{
	printf("policy_io_settings\n");
	return EAR_SUCCESS;
}

state_t policy_cpu_gpu_settings(polctx_t *c, signature_t *my_sig, node_freqs_t *freqs)
{
	printf("policy_cpu_gpu_settings\n");
	return EAR_SUCCESS;
}

state_t policy_busy_wait_settings(polctx_t *c, signature_t *my_sig, node_freqs_t *freqs)
{
	printf("policy_busy_wait_settings\n");
	return EAR_SUCCESS;
}

state_t policy_restore_settings(polctx_t *c, signature_t *my_sig, node_freqs_t *freqs)
{
	printf("policy_restore_settings\n");
	return EAR_SUCCESS;
}
