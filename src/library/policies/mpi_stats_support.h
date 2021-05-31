/*
*
* EAR provides a dynamic, transparent and ligth-weigth solution for
* Energy management. It has been developed in the context of the
* Barcelona Supercomputing Center (BSC)&Lenovo Collaboration project.
*
* Copyright © 2017-present BSC-Lenovo
* BSC Contact   mailto:ear-support@bsc.es
* Lenovo contact  mailto:hpchelp@lenovo.com
*
* This file is licensed under both the BSD-3 license for individual/non-commercial
* use and EPL-1.0 license for commercial use. Full text of both licenses can be
* found in COPYING.BSD and COPYING.EPL files.
*/

#ifndef MPI_SUPPORT_H
#define MPI_SUPPORT_H
#include <library/api/mpi_support.h>
#include <library/policies/policy_ctx.h>
#include <library/common/library_shared_data.h>


state_t mpi_app_init(polctx_t *c);
state_t mpi_app_end(polctx_t *c);
state_t mpi_call_init(polctx_t *c,mpi_call call_type);
state_t mpi_call_end(polctx_t *c,mpi_call call_type);


state_t mpi_call_read_diff(mpi_information_t *diff,mpi_information_t *last,int i);
state_t mpi_call_new_phase(polctx_t *c);


state_t mpi_call_get_stats(mpi_information_t *dst,mpi_information_t * src,int i);
state_t mpi_call_get_types(mpi_calls_types_t *dst,shsignature_t * src,int i);


state_t clean_my_mpi_type_info(mpi_calls_types_t *t);
state_t mpi_call_types_read_diff(mpi_calls_types_t *diff,mpi_calls_types_t *last, int i);
state_t read_diff_node_mpi_type_info(lib_shared_data_t *data,shsignature_t *sig,mpi_calls_types_t *node_mpi_calls,mpi_calls_types_t *last_node);



void verbose_mpi_types(int vl,mpi_calls_types_t *t);
void verbose_node_mpi_types(int vl,uint nump,mpi_calls_types_t *t);
void verbose_mpi_data(int vl,mpi_information_t *mc);

state_t copy_node_mpi_data(lib_shared_data_t *data,shsignature_t *sig,mpi_information_t *node_mpi_calls);
state_t read_diff_node_mpi_data(lib_shared_data_t *data,shsignature_t *sig,mpi_information_t *node_mpi_calls,mpi_information_t *last_node);
state_t evaluate_cp(int nump,mpi_information_t *node_mpi_calls,int *max,int *min);

/*  Statistics */
state_t mpi_stats_evaluate_sd(int nump, double* percs_mpi, double mean, double *sd);
state_t mpi_stats_evaluate_mean(int nump, double *percs_mpi, double *mn);
state_t mpi_stats_get_only_percs_mpi(mpi_information_t *node_mpi_calls, int nump, double *percs_mpi);
state_t mpi_stats_evaluate_similarity(double *current_perc_mpi, double *last_perc_mpi, size_t size, double *similarity);
state_t mpi_stats_sort_perc_mpi(double *perc_mpi_arr, size_t nump);
state_t mpi_stats_evaluate_mean_sd(int nump, double *percs_mpi, double *mean, double *sd);

#endif
