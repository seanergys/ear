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
#include <library/common/externs.h>


static timestamp pol_time_init,app_init;
static mpi_information_t mpi_stats;
static ulong total_mpi_sync = 0;
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
	/* Global statistics */
  timestamp_getfast(&end);
  elap=timestamp_diff(&end,&app_init,TIME_USECS);
	mpi_stats.exec_time = elap;
	mpi_stats.perc_mpi = (float) mpi_stats.mpi_time/(float)mpi_stats.exec_time;
	if (sig_shared_region[my_node_id].mpi_info.total_mpi_calls){

		verbose(2,"EARL: Total time synchronizing %lu. Total MPI sync calls %lu. Total blocking calls %lu. Total collective calls %lu ",total_mpi_sync,total_synchronizations,total_blocking_calls,total_collectives);
		if (stats != NULL){
			sprintf(file_name,"%s",stats);
			verbose(2,"Using %s STATS output file",stats);
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
  ullong elap;
	if (is_mpi_enabled()){
  timestamp_getfast(&end);
  elap=timestamp_diff(&end,&pol_time_init,TIME_USECS);
	if	(is_mpi_blocking(call_type)) 												total_blocking_calls++;
	if (is_collective(remove_blocking(call_type)))					total_collectives++;
	if (is_synchronization(remove_blocking(call_type))){ 	
		total_synchronizations++;
		total_mpi_sync += elap;
	}
  sig_shared_region[my_node_id].mpi_info.mpi_time=sig_shared_region[my_node_id].mpi_info.mpi_time+elap;
  sig_shared_region[my_node_id].mpi_info.total_mpi_calls++;

	/* Global statistics */
	mpi_stats.mpi_time+=elap;
	mpi_stats.total_mpi_calls++;
	}
  return EAR_SUCCESS;
}

