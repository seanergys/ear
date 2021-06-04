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
 * This file is licensed under both the BSD-3 license for individual/non-commercial
 * use and EPL-1.0 license for commercial use. Full text of both licenses can be
 * found in COPYING.BSD and COPYING.EPL files.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>
#include <common/config.h>
#include <common/states.h>
#define SHOW_DEBUGS 1
#include <common/output/verbose.h>
#include <common/system/time.h>
#include <library/common/library_shared_data.h>
#include <library/api/mpi_support.h>
#include <library/policies/policy_ctx.h>
#include <library/policies/mpi_stats_support.h>
#include <library/common/externs.h>
#include <library/common/verbose_lib.h>
#include <library/common/library_shared_data.h>
#include <common/math_operations.h>


static timestamp pol_time_init,app_init;
static mpi_information_t mpi_stats;
static ulong total_mpi_time_sync = 0;
static ulong total_mpi_time_collectives = 0;
static ulong total_mpi_time_blocking = 0;
static ulong total_blocking_calls = 0;
static ulong total_collectives = 0;
static ulong total_synchronizations = 0;
static ulong total_sync_block = 0;
static ulong time_sync_block = 0;
static ulong max_sync_block = 0;
static useconds_t wait_time = 1000;
static uint mpi_cases[3];
state_t mpi_app_init(polctx_t *c)
{

    if (c!=NULL){ 
        sig_shared_region[my_node_id].mpi_info.mpi_time=0;
        sig_shared_region[my_node_id].mpi_info.total_mpi_calls=0;
        sig_shared_region[my_node_id].mpi_info.exec_time=0;
        sig_shared_region[my_node_id].mpi_info.perc_mpi=0;


        sig_shared_region[my_node_id].mpi_types_info.sync = 0;
        sig_shared_region[my_node_id].mpi_types_info.collec = 0;
        sig_shared_region[my_node_id].mpi_types_info.blocking = 0;
        sig_shared_region[my_node_id].mpi_types_info.time_sync = 0;
        sig_shared_region[my_node_id].mpi_types_info.time_collec = 0;
        sig_shared_region[my_node_id].mpi_types_info.time_blocking = 0;

        /* Gobal mpi statistics */
        mpi_stats.mpi_time = 0;
        mpi_stats.total_mpi_calls = 0;
        mpi_stats.exec_time = 0;
        mpi_stats.perc_mpi = 0;
        if (is_mpi_enabled()){
            char *cwait_time;
            mpi_stats.rank = sig_shared_region[my_node_id].mpi_info.rank;
            cwait_time = getenv("I_MPI_WAIT_TIMEOUT");
            if (cwait_time != NULL){ 
                wait_time = (useconds_t)atoi(cwait_time);
                wait_time = wait_time*1000;
            }
            verbose_master(2,"Using wait time (usecs) %u ",(uint)wait_time);
        }else{
            mpi_stats.rank = 0;
        }
        timestamp_getfast(&app_init);
        return EAR_SUCCESS;
    }else return EAR_ERROR;

}

state_t mpi_app_end(polctx_t *c)
{
    timestamp_t end;
    ullong elap;
    int fd;
    char buff[256],buff2[300];
    char file_name[256];
    char *stats=getenv(SCHED_GET_EAR_STATS);
    char stats_per_call[256];
    /* Global statistics */
    timestamp_getfast(&end);
    elap=timestamp_diff(&end,&app_init,TIME_USECS);
    mpi_stats.exec_time = elap;
    if (mpi_stats.total_mpi_calls){

        mpi_stats.perc_mpi = (float) mpi_stats.mpi_time/(float)mpi_stats.exec_time;
        if (stats != NULL){
            sprintf(file_name,"%s",stats);
            sprintf(stats_per_call,"%s.mpi_call_dist.csv",stats);
            verbose(2,"Using %s STATS output file and mpi calls dist file %s",stats,stats_per_call);
            fd=open(file_name,O_WRONLY|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR);
            if (fd < 0){
                mpi_info_to_str(&mpi_stats,buff,sizeof(buff));
                verbose(0,"MR[%d] %s",lib_shared_region->master_rank,buff);
            }else{
                if (masters_info.my_master_rank == 0){
                    mpi_info_head_to_str_csv(buff,sizeof(buff));
                    sprintf(buff2,"mrank;%s\n",buff);
                    write(fd,buff2,strlen(buff2));
                }
                mpi_info_to_str_csv(&mpi_stats,buff,sizeof(buff));
                sprintf(buff2,"%d;%s\n",lib_shared_region->master_rank,buff);
                write(fd,buff2,strlen(buff2));
                close(fd);
            }
            fd = open(stats_per_call,O_WRONLY|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR);
            if (fd >= 0){
                if (mpi_stats.rank == 0){
                    sprintf(buff,"MASTER;RANK;Total MPI calls;MPI time;Time synchronizing;Time Blocking;Time Collectives;Total MPI sync calls;Total blocking calls;Total collective calls\n");
                    write(fd,buff,strlen(buff));
                }
                sprintf(buff,"%d;%d;%u;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f\n",lib_shared_region->master_rank,mpi_stats.rank,mpi_stats.total_mpi_calls,mpi_stats.perc_mpi*100,((float)total_mpi_time_sync/(float)mpi_stats.exec_time)*100.0,((float)total_mpi_time_blocking/(float)mpi_stats.exec_time)*100.0,((float)total_mpi_time_collectives/(float)mpi_stats.exec_time)*100.0,
                        ((float)total_synchronizations/(float)mpi_stats.total_mpi_calls)*100.0,
                        ((float)total_blocking_calls/(float)mpi_stats.total_mpi_calls)*100.0,
                        ((float)total_synchronizations/(float)mpi_stats.total_mpi_calls)*100.0);	
                write(fd,buff,strlen(buff));
            }
        }
    }
    return EAR_SUCCESS;
}
static uint in_mpi = 0;
state_t mpi_call_init(polctx_t *c,mpi_call call_type)
{
    mpi_call nbcall_type;
    if (is_mpi_enabled()){
        assert(in_mpi == 0);
        in_mpi = 1;
        timestamp_getfast(&pol_time_init);
    }
    return EAR_SUCCESS;
}


state_t mpi_call_end(polctx_t *c,mpi_call call_type)
{
    timestamp_t end;
    mpi_call nbcall_type;
    ullong elap,elap_app;
    if (is_mpi_enabled()){
        assert(in_mpi == 1);

        timestamp_getfast(&end);
        elap = timestamp_diff(&end,&pol_time_init,TIME_USECS);
        elap_app = timestamp_diff(&end,&app_init,TIME_USECS);

        if	(is_mpi_blocking(call_type)){ 											
            max_sync_block = ear_max(max_sync_block, (ulong)elap);
            total_blocking_calls++;
            total_mpi_time_blocking += (ulong)elap;
            nbcall_type = (mpi_call)remove_blocking(call_type);
            if (is_collective(nbcall_type)){					
                total_collectives++;
                total_mpi_time_collectives += (ulong)elap;
            }else if (is_synchronization(nbcall_type)){ 	
                total_synchronizations++;
                total_mpi_time_sync += (ulong)elap;
            }else{
                total_sync_block++;
                time_sync_block += (ulong)elap;
            }
        }
        sig_shared_region[my_node_id].mpi_info.mpi_time += elap;
        sig_shared_region[my_node_id].mpi_info.total_mpi_calls++;
        sig_shared_region[my_node_id].mpi_info.exec_time = elap_app;


        sig_shared_region[my_node_id].mpi_types_info.sync = total_synchronizations;
        sig_shared_region[my_node_id].mpi_types_info.collec = total_collectives;
        sig_shared_region[my_node_id].mpi_types_info.blocking = total_blocking_calls;
        sig_shared_region[my_node_id].mpi_types_info.time_sync = total_mpi_time_sync;
        sig_shared_region[my_node_id].mpi_types_info.time_collec = total_mpi_time_collectives;
        sig_shared_region[my_node_id].mpi_types_info.time_blocking = total_mpi_time_blocking;
        sig_shared_region[my_node_id].mpi_types_info.sync_block = total_sync_block;
        sig_shared_region[my_node_id].mpi_types_info.time_sync_block = time_sync_block;
        sig_shared_region[my_node_id].mpi_types_info.max_sync_block = max_sync_block;

        /* Global statistics */
        mpi_stats.mpi_time += elap;
        mpi_stats.total_mpi_calls++;
        mpi_stats.exec_time = elap_app;
        in_mpi = 0;
    }
    return EAR_SUCCESS;
}
/* If src is null, dst = local data, else dst = src */
state_t mpi_call_get_stats(mpi_information_t *dst,mpi_information_t * src,int i)
{

    if (dst == NULL) return EAR_ERROR;
    if (src == NULL){
        if (i < 0) src = &sig_shared_region[my_node_id].mpi_info;
        else src = &sig_shared_region[i].mpi_info;
    }
    memcpy(dst,src,sizeof(mpi_information_t));
    return EAR_SUCCESS;
}

static void mpi_call_copy(mpi_information_t *dst,mpi_information_t * src)
{
    if ((dst == NULL) || (src == NULL)) return;
    memcpy(dst,src,sizeof(mpi_information_t));
}

static state_t mpi_call_diff(mpi_information_t *diff,mpi_information_t *end,mpi_information_t *init)
{
    diff->total_mpi_calls = end->total_mpi_calls - init->total_mpi_calls;
    diff->exec_time       = end->exec_time - init->exec_time;
    diff->mpi_time        = end->mpi_time - init->mpi_time; 	
    return EAR_SUCCESS;
}

void verbose_mpi_data(int vl,mpi_information_t *mc)
{
    verbose_master(vl,"Total mpi calls %u MPI time %llu exec time %llu perc mpi %.2lf",mc->total_mpi_calls,mc->mpi_time,mc->exec_time,mc->perc_mpi);
}

state_t mpi_call_read_diff(mpi_information_t *diff,mpi_information_t *last,int i)
{
    mpi_information_t current;
    mpi_call_get_stats(&current,NULL,i);
    mpi_call_diff(diff,&current,last);
    diff->perc_mpi =  (double)diff->mpi_time*100.0/(double)diff->exec_time;
    mpi_call_copy(last,&current);
    last->perc_mpi =  (double)last->mpi_time*100.0/(double)last->exec_time;

    /*  Check wether values are correctly bounded */
    if (diff->perc_mpi < 0 || diff->perc_mpi > 100 || last->perc_mpi < 0 || last->perc_mpi > 100){
        verbose_master(2, "%s[WARNING] Bad perc_mpi computation%s: diff = %lf / %lf = %lf, last = %lf / %lf = %lf", COL_RED, COL_CLR, (double)diff->mpi_time*100.0, (double)diff->exec_time,
                diff->perc_mpi, (double)last->mpi_time*100.0, (double)last->exec_time, last->perc_mpi);
        return EAR_WARNING;
    }
    return EAR_SUCCESS;
}

state_t mpi_call_new_phase(polctx_t *c)
{
#if RESET_STATISTICS_AT_SIGNATURE
    clean_my_mpi_info(&sig_shared_region[my_node_id].mpi_info);	
#endif
    return EAR_SUCCESS;
}

static state_t mpi_call_types_diff(mpi_calls_types_t * diff, mpi_calls_types_t *current, mpi_calls_types_t *last)
{
    diff->mpi 						= current->mpi - last->mpi;
    diff->time_mpi 				= current->time_mpi - last->time_mpi;
    diff->time_period 		= current->time_period - last->time_period;
    diff->sync 						= current->sync - last->sync;
    diff->collec 					=  current->collec - last->collec;
    diff->blocking 				= current->blocking - last->blocking;
    diff->time_sync 			= current->time_sync - last->time_sync;
    diff->time_collec 		= current->time_collec - last->time_collec;
    diff->time_blocking 	= current->time_blocking - last->time_blocking;
    diff->sync_block  		= current->sync_block - last->sync_block;
    diff->time_sync_block = current->time_sync_block - last->time_sync_block;
    diff->max_sync_block  = ear_max(current->max_sync_block , last->max_sync_block);
    return EAR_SUCCESS;
}

state_t mpi_call_types_read_diff(mpi_calls_types_t *diff,mpi_calls_types_t *last, int i)
{
    mpi_calls_types_t current;
    mpi_call_get_types(&current, NULL, i);
    mpi_call_types_diff(diff, &current, last);
    memcpy(last,&current,sizeof(mpi_calls_types_t));
    return EAR_SUCCESS;
}


state_t mpi_call_get_types(mpi_calls_types_t *dst,shsignature_t * src,int i)
{
    mpi_information_t *src_stat;
    mpi_calls_types_t * src_types;
    if (dst == NULL) return EAR_ERROR;
    if (src == NULL){
        if (i < 0){ 
            src_types = &sig_shared_region[my_node_id].mpi_types_info;
            src_stat  = &sig_shared_region[my_node_id].mpi_info;
        }else{				
            src_types = &sig_shared_region[i].mpi_types_info;
            src_stat  = &sig_shared_region[i].mpi_info;
        }
    }
    memcpy(dst,src_types,sizeof(mpi_calls_types_t));
    dst->time_mpi 		= src_stat->mpi_time;
    dst->time_period 	= src_stat->exec_time;
    dst->mpi 					= src_stat->total_mpi_calls;

    return EAR_SUCCESS;
}


void verbose_mpi_types(int vl,mpi_calls_types_t *t)
{
    char psync[64], pcollect[64];
    if (t->time_period == 0){
        sprintf(psync,"NAV");
        sprintf(pcollect," NAV");
    }else{
        sprintf(psync,"%.2lf",(float)t->time_sync/(float)t->time_period);
        sprintf(pcollect," %.2lf",(float)t->time_collec/(float)t->time_mpi);
    }
    verbose_master(vl,"MPI_TYPES: mpis %lu mpi_time %lu exec time %lu sync calls %lu collec %lu blocking %lu time sync %lu time collec %lu time blocking %lu mpis/s=%.2f/pmpi=%.2f/syncp=%s/collecp=%s sblock=%lu/%lu/%lu",t->mpi,t->time_mpi,t->time_period,t->sync,t->collec,t->blocking,t->time_sync,t->time_collec,t->time_blocking,(float)t->mpi*1000000.0/(float)t->time_period,(float)t->time_mpi/(float)t->time_period,psync,pcollect,t->sync_block, t->time_sync_block,t->max_sync_block);
}

void verbose_node_mpi_types(int vl,uint nump,mpi_calls_types_t *t)
{
    int i;
    ulong tmpi = 0, texec = 0;
    float max_mpi = 0.0, min_mpi = 100.0,  pmpi;
    if (vl == 1){
        verbose_mpi_types(vl,&t[0]);
    }
    if (vl == 2){
        for (i=0; i<nump; i++){
            tmpi += t[i].time_mpi;
            texec += t[i].time_period;
            pmpi = (float)t[i].time_mpi/(float)t[i].time_period;
            max_mpi = ear_max(pmpi,max_mpi);
            min_mpi = ear_min(pmpi,min_mpi);
        }
        verbose_master(vl,"MPI_TYPES_SUMMARY: avg mpi %.2f max mpi %.2f min mpi %.2f ",(float)tmpi/(float)texec, max_mpi, min_mpi);
    }
    if (vl >= 3){
        for (i=0;i<nump;i++){
            verbose_mpi_types(vl,&t[i]);
        }
    }
}


state_t clean_my_mpi_type_info(mpi_calls_types_t *t)
{
    memset(t,0,sizeof(mpi_calls_types_t));
    return EAR_SUCCESS;
}

state_t copy_node_mpi_data(lib_shared_data_t *data,shsignature_t *sig,mpi_information_t *node_mpi_calls)
{
    int i;
    if ((data == NULL) || (sig == NULL) || (node_mpi_calls == NULL)) return EAR_ERROR;
    for (i=0;i<data->num_processes;i++){
        memcpy(&node_mpi_calls[i],&sig[i].mpi_info,sizeof(mpi_information_t));
        node_mpi_calls[i].perc_mpi = (double)node_mpi_calls[i].mpi_time*100.0/(double)node_mpi_calls[i].exec_time;
    }	
    return EAR_SUCCESS;
}

state_t read_diff_node_mpi_data(lib_shared_data_t *data,shsignature_t *sig,mpi_information_t *node_mpi_calls,mpi_information_t *last_node)
{
    int i;
    int bad_read_diff = 0;
    if ((data == NULL) || (sig == NULL) || (node_mpi_calls == NULL) || (last_node == NULL)) return EAR_ERROR;
    for (i=0;i<data->num_processes;i++){
        if (mpi_call_read_diff(&node_mpi_calls[i],&last_node[i],i) != EAR_SUCCESS){
            bad_read_diff = 1;
        }
    }
    if (bad_read_diff){
        return EAR_WARNING;
    }
    return EAR_SUCCESS;
}

state_t read_diff_node_mpi_type_info(lib_shared_data_t *data,shsignature_t *sig,mpi_calls_types_t *node_mpi_calls,mpi_calls_types_t *last_node)
{
    int i = 0;
    if ((data == NULL) || (sig == NULL) || (node_mpi_calls == NULL) || (last_node == NULL)) return EAR_ERROR;
    for (i=0;i<data->num_processes;i++){
        mpi_call_types_read_diff(&node_mpi_calls[i],&last_node[i],i);
    }
    return EAR_SUCCESS;
}


state_t evaluate_cp(int nump,mpi_information_t *node_mpi_calls,int *max,int *min)
{
    int i = 0;
    *max = 0;
    *min = 0;
    verbosen_master(2,"LB_DATA ");
    for (i=0; i< nump;i++){ 
        verbosen_master(2,"MR[%d]= %.2lf ",i,node_mpi_calls[i].perc_mpi);
        verbosen_master(3," (%lu/%lu) ",node_mpi_calls[i].mpi_time,node_mpi_calls[i].exec_time);
    }
    verbose_master(2,"");
    for (i=1 ; i < nump ; i++){
        if (node_mpi_calls[i].perc_mpi > node_mpi_calls[*max].perc_mpi)	*max = i;
        if (node_mpi_calls[i].perc_mpi < node_mpi_calls[*min].perc_mpi)	*min = i;
    }	
    return EAR_SUCCESS;
}

state_t mpi_stats_get_only_percs_mpi(mpi_information_t *node_mpi_calls, int nump, double *percs_mpi){
    mpi_information_t **mpi_calls = calloc(nump, sizeof(mpi_information_t*));
    for (int i = 0; i < nump; i++) {
        mpi_calls[i] = &node_mpi_calls[i];
    }
    void** percs_mpi_v = ear_math_apply((void**) mpi_calls, nump, mpi_info_get_perc_mpi);
    free(mpi_calls);

    for (int i = 0; i < nump; i++) {
        percs_mpi[i] = *(double*)percs_mpi_v[i];
    }
    ear_math_free_gen_array(percs_mpi_v);
    return EAR_SUCCESS;
}

state_t mpi_stats_evaluate_sd(int nump, double *percs_mpi, double mean, double *sd){
    *sd = ear_math_standard_deviation(percs_mpi, nump, mean);
    return EAR_SUCCESS;
}

state_t mpi_stats_evaluate_mean(int nump, double *percs_mpi, double *mn){
    *mn = ear_math_mean(percs_mpi, nump);
    return EAR_SUCCESS;
}

state_t mpi_stats_evaluate_similarity(double *current_perc_mpi, double *last_perc_mpi, size_t size, double *similarity) {
    *similarity = ear_math_cosine_similarity(current_perc_mpi, last_perc_mpi, size);
    return EAR_SUCCESS;
}

int compare_perc_mpi(const void *p1, const void *p2) {
    return (*(double * const *) p1 < *(double * const *) p2) ? -1 : (*(double * const *) p1 > *(double * const *) p2) ? 1 : 0;
}

state_t mpi_stats_sort_perc_mpi(double *perc_mpi_arr, size_t nump) {
    qsort(perc_mpi_arr, nump, sizeof(double), compare_perc_mpi);
    return EAR_SUCCESS;
}

state_t mpi_stats_evaluate_mean_sd(int nump, double *percs_mpi, double *mean, double *sd){
    mean_sd_t mean_sd = ear_math_mean_sd(percs_mpi, nump);
    *mean = mean_sd.mean;
    *sd = mean_sd.sd;
    return EAR_SUCCESS;
}

state_t mpi_stats_evaluate_mean_sd_mag(int nump, double *percs_mpi, double *mean, double *sd, double *mag){
    mean_sd_t mean_sd_mag = ear_math_mean_sd(percs_mpi, nump);
    *mean = mean_sd_mag.mean;
    *sd = mean_sd_mag.sd;
    *mag = mean_sd_mag.mag;
    return EAR_SUCCESS;
}

state_t mpi_stats_evaluate_median(int nump, double *percs_mpi, double *median){
    *median = ear_math_median(percs_mpi, nump);
    return EAR_SUCCESS;
}


state_t is_blocking_busy_waiting(uint *block_type)
{
    char *env;
    *block_type = BUSY_WAITING_BLOCK;
    if ((env = getenv("I_MPI_WAIT_MODE")) != NULL){
        verbose_master(2,"I_MPI_WAIT_MODE %s",env);
        if (atoi(env) != 0 ) *block_type = YIELD_BLOCK;
    }
    if ((env = getenv("I_MPI_THREAD_YIELD")) != NULL){
        verbose_master(2,"I_MPI_THREAD_YIELD %s",env);
        if (atoi(env) != 0 ) *block_type = YIELD_BLOCK;
    }
    if ((env = getenv("OMPI_MCA_mpi_yield_when_idle")) != NULL){
        verbose_master(2,"OMPI_MCA_mpi_yield_when_idle %s",env);
        if (atoi(env) != 0 ) *block_type = YIELD_BLOCK;
    }
    return EAR_SUCCESS;
}

