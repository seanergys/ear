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
#include <common/config.h>
#include <common/states.h>
//#define SHOW_DEBUGS 1
#include <common/output/verbose.h>
#include <common/system/time.h>
#include <library/common/library_shared_data.h>
#include <library/api/mpi_support.h>
#include <library/policies/policy_ctx.h>
#include <library/policies/mpi_stats_support.h>
#include <library/common/externs.h>
#include <library/common/verbose_lib.h>


static timestamp pol_time_init,app_init;
static mpi_information_t mpi_stats;
static ulong total_mpi_time_sync = 0;
static ulong total_mpi_time_collectives = 0;
static ulong total_mpi_time_blocking = 0;
static ulong total_blocking_calls = 0;
static ulong total_collectives = 0;
static ulong total_synchronizations = 0;

state_t mpi_app_init(polctx_t *c)
{
  
  if (c!=NULL){ 
    sig_shared_region[my_node_id].mpi_info.mpi_time=0;
    sig_shared_region[my_node_id].mpi_info.total_mpi_calls=0;
    sig_shared_region[my_node_id].mpi_info.exec_time=0;
    sig_shared_region[my_node_id].mpi_info.perc_mpi=0;

		/* Gobal mpi statistics */
		mpi_stats.mpi_time = 0;
		mpi_stats.total_mpi_calls = 0;
		mpi_stats.exec_time = 0;
		mpi_stats.perc_mpi = 0;
		if (is_mpi_enabled()){
			mpi_stats.rank = sig_shared_region[my_node_id].mpi_info.rank;
		}else{
			mpi_stats.rank = 0;
		}
		timestamp_getfast(&app_init);
    return EAR_SUCCESS;
  }else return EAR_ERROR;

}

state_t mpi_app_end(polctx_t *c)
{
  timestamp end;
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

state_t mpi_call_init(polctx_t *c,mpi_call call_type)
{
	if (is_mpi_enabled()){
  	timestamp_getfast(&pol_time_init);
	}
  return EAR_SUCCESS;
}


state_t mpi_call_end(polctx_t *c,mpi_call call_type)
{
  timestamp end;
  ullong elap,elap_app;
	if (is_mpi_enabled()){
  	timestamp_getfast(&end);
  	elap = timestamp_diff(&end,&pol_time_init,TIME_USECS);
  	elap_app = timestamp_diff(&end,&app_init,TIME_USECS);

		if	(is_mpi_blocking(call_type)){ 											
			total_blocking_calls++;
			total_mpi_time_blocking += (ulong)elap;
		}
		if (is_collective(remove_blocking(call_type))){					
			total_collectives++;
			total_mpi_time_collectives += (ulong)elap;
		}
		if (is_synchronization(remove_blocking(call_type))){ 	
			total_synchronizations++;
			total_mpi_time_sync += (ulong)elap;
		}
  	sig_shared_region[my_node_id].mpi_info.mpi_time += elap;
  	sig_shared_region[my_node_id].mpi_info.total_mpi_calls++;
  	sig_shared_region[my_node_id].mpi_info.exec_time = elap_app;

		/* Global statistics */
		mpi_stats.mpi_time += elap;
		mpi_stats.total_mpi_calls++;
  	mpi_stats.exec_time = elap_app;
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
	return EAR_SUCCESS;	
}


state_t mpi_call_new_phase(polctx_t *c)
{
#if RESET_STATISTICS_AT_SIGNATURE
	clean_my_mpi_info(&sig_shared_region[my_node_id].mpi_info);	
#endif
	return EAR_SUCCESS;
}


state_t mpi_call_types_read_diff(mpi_calls_types_t *diff,mpi_calls_types_t *last)
{
	mpi_calls_types_t a;
	mpi_call_get_types(&a);
	diff->sync = a.sync - last->sync;
	diff->collec =	a.collec - last->collec;
	diff->blocking = a.blocking - last->blocking;
	diff->time_sync = a.time_sync - last->time_sync;
	diff->time_collec = a.time_collec - last->time_collec;
	diff->time_blocking =	a.time_blocking - last->time_blocking;
	memcpy(last,&a,sizeof(mpi_calls_types_t));
	return EAR_SUCCESS;
}

state_t mpi_call_get_types(mpi_calls_types_t *t)
{
  t->sync = total_synchronizations;
  t->collec = total_collectives;
  t->blocking = total_blocking_calls;
  t->time_sync = total_mpi_time_sync ;
  t->time_collec = total_mpi_time_collectives;
  t->time_blocking = total_mpi_time_blocking;

	return EAR_SUCCESS;
}

void verbose_mpi_types(int vl,mpi_calls_types_t *t)
{
	verbose_master(vl,"MPI sync calls %lu collec %lu blocking %lu time sync %lu time collec %lu time blocking %lu",t->sync,t->collec,t->blocking,t->time_sync,t->time_collec,t->time_blocking);
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
	if ((data == NULL) || (sig == NULL) || (node_mpi_calls == NULL) || (last_node == NULL)) return EAR_ERROR;
	for (i=0;i<data->num_processes;i++){
		mpi_call_read_diff(&node_mpi_calls[i],&last_node[i],i);
	}
	return EAR_SUCCESS;
}

state_t evaluate_cp(int nump,mpi_information_t *node_mpi_calls,int *max,int *min)
{
	int i = 0;
	*max = 0;
	*min = 0;		
	for (i=0; i< nump;i++) verbosen_master(2,"MR[%d]= %.2lf ",i,node_mpi_calls[i].perc_mpi);
	verbose_master(2,"");
	for (i=1 ; i < nump ; i++){
		if (node_mpi_calls[i].perc_mpi > node_mpi_calls[*max].perc_mpi)	*max = i;
		if (node_mpi_calls[i].perc_mpi < node_mpi_calls[*min].perc_mpi)	*min = i;
	}	
	return EAR_SUCCESS;
}
