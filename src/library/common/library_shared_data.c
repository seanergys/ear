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

//#define SHOW_DEBUGS 1
#define _GNU_SOURCE
#include <sched.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <common/config.h>
#include <common/states.h>
#include <common/hardware/hardware_info.h>
#include <common/output/verbose.h>
#include <common/types/projection.h>
#include <common/types/configuration/cluster_conf.h>

#include <metrics/flops/flops.h>

#include <daemon/local_api/node_mgr.h>
#include <library/common/externs.h>
#include <library/common/library_shared_data.h>
#include <library/common/verbose_lib.h>
#include <library/common/global_comm.h>
#include <common/environment.h>

/* Node mgr variables */
extern ear_njob_t *node_mgr_data;
extern uint node_mgr_index;
extern node_mgr_sh_data_t *node_mgr_job_info;
extern char lib_shared_region_path_jobs[GENERIC_NAME];
extern char sig_shared_region_path_jobs[GENERIC_NAME];
static int fd_conf,fd_signatures;
extern uint exclusive;

int  get_lib_shared_data_path(char *tmp, uint ID, char *path)
{
	if ((tmp==NULL) || (path==NULL)) return EAR_ERROR;
	sprintf(path,"%s/%u/.ear_lib_shared_data",tmp, ID);
	return EAR_SUCCESS;	
}
int  get_shared_signatures_path(char *tmp, uint ID, char *path)
{
	if ((tmp==NULL) || (path==NULL)) return EAR_ERROR;
	sprintf(path,"%s/%u/.ear_shared_signatures",tmp, ID);
	return EAR_SUCCESS;	
}




/***** SPECIFIC FUNCTIONS *******************/



lib_shared_data_t * create_lib_shared_data_area(char * path)  
{      	
	lib_shared_data_t sh_data,*my_area;
	my_area=(lib_shared_data_t *)create_shared_area(path,(char *)&sh_data,sizeof(lib_shared_data_t),&fd_conf,1);
	return my_area;
}

lib_shared_data_t * attach_lib_shared_data_area(char * path, int *fd)
{
    return (lib_shared_data_t *)attach_shared_area(path,sizeof(lib_shared_data_t),O_RDWR,fd,NULL);
}                                
void dettach_lib_shared_data_area(int fd)
{
	dettach_shared_area(fd);
}

void lib_shared_data_area_dispose(char * path)
{
	dispose_shared_area(path,fd_conf);
}


void print_lib_shared_data(lib_shared_data_t *sh_data)
{
	fprintf(stderr,"sh_data num_processes %d signatures %d cas_counters %lf\n",sh_data->num_processes,sh_data->num_signatures,sh_data->cas_counters);

}

/// SIGNATURES

// Creates a shared memory region between eard and ear_lib. returns NULL if error.
shsignature_t * create_shared_signatures_area(char * path, int np)  
{      	
	shsignature_t *my_sig,*p2;

	my_sig=(shsignature_t*)malloc(sizeof(shsignature_t)*np);
	p2=create_shared_area(path,(char *)my_sig,sizeof(shsignature_t)*np,&fd_signatures,1);
	free(my_sig);
	return p2;
}

shsignature_t * attach_shared_signatures_area(char * path,int np, int *fd)
{
    return (shsignature_t *)attach_shared_area(path,sizeof(shsignature_t)*np,O_RDWR,fd,NULL);
}                                
void dettach_shared_signatures_area(int fd)
{
	dettach_shared_area(fd);
}
void shared_signatures_area_dispose(char * path)
{
	dispose_shared_area(path,fd_signatures);
}

/************** Marks the signature is computed *****************/
void signature_ready(shsignature_t *sig,int cur_state)
{
	debug("Process %d rank %d signature_ready",my_node_id,sig->mpi_info.rank);
	sig->ready = 1;
	sig->app_state = cur_state;
}

int all_signatures_initialized(lib_shared_data_t *data,shsignature_t *sig)
{
	int i,total=0;
  for (i=0;i<data->num_processes;i++){
    total += (sig[i].pid > 0 );
  }
  return (total == data->num_processes);

}
void aggregate_all_the_cpumasks(lib_shared_data_t *data,shsignature_t *sig, cpu_set_t *m)
{
	CPU_ZERO(m);
	for (uint p = 0; p < data->num_processes; p++){
		aggregate_cpus_to_mask(m, &sig[p].cpu_mask);
	}
}


int compute_total_signatures_ready(lib_shared_data_t *data,shsignature_t *sig)
{
	int i,total=0;
  for (i=0;i<data->num_processes;i++){ 
		total = total+sig[i].ready;
		if (!sig[i].ready) debug(" Process %d not ready",i);
	}
	data->num_signatures = total;
	return total;
}
int are_signatures_ready(lib_shared_data_t *data,shsignature_t *sig)
{
	compute_total_signatures_ready(data,sig);
	debug("There are %d signatures ready from %d",data->num_signatures,data->num_processes);
	return(data->num_signatures == data->num_processes);
}

void print_sig_readiness(lib_shared_data_t *data,shsignature_t *sig)
{
	int i;
	for (i=0;i<data->num_processes;i++){
		debug("P[%d] ready %u ",i,sig[i].ready);
	}
}

void clean_signatures(lib_shared_data_t *data,shsignature_t *sig)
{
	int i;
  for (i=0;i<data->num_processes;i++) sig[i].ready = 0;
}

void clean_mpi_info(lib_shared_data_t *data,shsignature_t *sig)
{
  int i;
  for (i=0;i<data->num_processes;i++){ 
		sig[i].mpi_info.mpi_time				=	0;
		sig[i].mpi_info.total_mpi_calls	=	0;
		sig[i].mpi_info.perc_mpi				=	0;
		sig[i].mpi_info.exec_time				=	0;
	}
}

void clean_my_mpi_info(mpi_information_t *info)
{
	info->mpi_time 				= 0;
	info->total_mpi_calls = 0;
	info->perc_mpi				= 0;
	info->exec_time				= 0;
}

int select_cp(lib_shared_data_t *data,shsignature_t *sig)
{
  int i,rank = sig[0].mpi_info.rank;
	double minp = sig[0].mpi_info.perc_mpi;
  for (i=1;i<data->num_processes;i++){
    if (sig[i].mpi_info.perc_mpi<minp){ 
			rank = sig[i].mpi_info.rank;
			minp = sig[i].mpi_info.perc_mpi;
		}
  }
	return rank;
}

double min_perc_mpi_in_node(lib_shared_data_t *data,shsignature_t *sig)
{
  int i;
  double minp =sig[0].mpi_info.perc_mpi;
  for (i=1;i<data->num_processes;i++){
    if (sig[i].mpi_info.perc_mpi<minp){
      minp = sig[i].mpi_info.perc_mpi;
    }
  }
  return minp;
}

void compute_avg_sh_signatures(int size,int max,int *ppn,shsignature_t *my_sh_sig,signature_t *sig)
{
	signature_t avgs,totals;
	int i,j;
	int nums = 0;
	signature_init(sig);
	signature_init(&avgs);
	signature_init(&totals);
  for (i=0;i<size;i++){
    for (j=0;j<ear_min(max,ppn[i]);j++){
			from_minis_to_sig(&avgs,&my_sh_sig[i*max+j].sig);
			acum_sig_metrics(&totals,&avgs);
			nums++;
   	}
	} 
	compute_avg_sig(&avgs,&totals,nums);
	signature_copy(sig,&avgs);
	
}
int shsig_id(int node,int rank)
{
	int i;
	if (sh_sig_per_proces){
		for (i=node*masters_info.max_ppn;i<(node*masters_info.max_ppn+masters_info.ppn[node]);i++){
			if (masters_info.nodes_info[i].mpi_info.rank == rank) return i;
		}
  }else{
		return node;	
	}
	debug("Node %d rank %d not found in shared info",node,rank);
	return node;
}
int my_shsig_id()
{
	int max_ppn;
	  if (sh_sig_per_proces){
			max_ppn = masters_info.max_ppn;
			return masters_info.my_master_rank*max_ppn+my_node_id;
    }else{
			return masters_info.my_master_rank;
    }

}

/* This function must not be used and must be replaced by the one at mpi_support in policies folder */
int select_global_cp(int size,int max,int *ppn,shsignature_t *my_sh_sig,int *node_cp,int *rank_cp)
{
	int i,j;
	int rank = 0;
	double minp = 100.0,maxp = 0.0;
	unsigned int total_mpi = 0;
	unsigned long long total_mpi_time = 0, total_exec_time = 0;
	/* Node loop */
	for (i=0;i<size;i++){
		/* Inside node */
		for (j=0;j<ear_min(max,ppn[i]);j++){
			total_mpi 			+= my_sh_sig[i*max+j].mpi_info.total_mpi_calls;
			total_mpi_time	+= my_sh_sig[i*max+j].mpi_info.mpi_time;
			total_exec_time	+= my_sh_sig[i*max+j].mpi_info.exec_time;
			if (minp > my_sh_sig[i*max+j].mpi_info.perc_mpi){
				rank 			= my_sh_sig[i*max+j].mpi_info.rank;
				minp 			= my_sh_sig[i*max+j].mpi_info.perc_mpi;
				*node_cp 	= i;
			}
			if (maxp < my_sh_sig[i*max+j].mpi_info.perc_mpi) maxp = my_sh_sig[i*max+j].mpi_info.perc_mpi;
		}
	}
	*rank_cp = rank;
	debug("(MIN PERC MPI %.1lf, MAX PERC MPI %.1lf) (MPI_CALLS %u MPI_TIME %.3fsec USER_TIME=%.3fsec)\n",minp*100.0,maxp*100.0,total_mpi,(float)total_mpi_time/1000000.0,(float)total_exec_time/1000000.0);
	return rank;
}

void print_local_mpi_info(mpi_information_t *info)
{
	fprintf(stderr,"total_mpi_calls %u exec_time %llu mpi_time %llu rank %d perc_mpi %.3lf \n",info->total_mpi_calls,info->exec_time,info->mpi_time,info->rank,info->perc_mpi);
}

void mpi_info_to_str(mpi_information_t *info,char *msg,size_t max)
{
	snprintf(msg,max,"RANK[%d] total_mpi_calls %u exec_time %llu mpi_time %llu perc_mpi %.3lf ",info->rank,info->total_mpi_calls,info->exec_time,info->mpi_time,info->perc_mpi*100.0);
}
void mpi_info_head_to_str_csv(char *msg,size_t max)
{
	snprintf(msg,max,"lrank;total_mpi_calls;exec_time;mpi_time;perc_mpi_time");
}
void mpi_info_to_str_csv(mpi_information_t *info,char *msg,size_t max)
{
  snprintf(msg,max,"%d;%u;%llu;%llu;%.3lf",info->rank,info->total_mpi_calls,info->exec_time,info->mpi_time,info->perc_mpi*100.0);
}


void print_sh_signature(int localid,shsignature_t *sig)
{
	 	float t;
		float avgf,deff,newf;
		avgf=(float)sig->sig.avg_f/1000000.0;
		deff=(float)sig->sig.def_f/1000000.0;
		newf=(float)sig->new_freq/1000000.0;
    t = (float) sig->mpi_info.exec_time/1000000.0;

	  fprintf(stderr,"RANK[%d]= %d mpi_data={total_mpi_calls %u mpi_time %llu exec_time %.3f PercTime %lf }\n",localid,
    sig->mpi_info.rank,sig->mpi_info.total_mpi_calls,sig->mpi_info.mpi_time,t,sig->mpi_info.perc_mpi);
    fprintf(stderr,"RANK[%d]= %d signature={cpi %.3lf tpi %.3lf time %.3lf Gflops %f dc_power %.3lf avgf %.1f deff %.1f} state %d new_freq %.1f\n",localid,sig->mpi_info.rank,sig->sig.CPI,sig->sig.TPI, sig->sig.time,sig->sig.Gflops,sig->sig.DC_power,avgf,deff,sig->app_state,newf);
}

void print_shared_signatures(lib_shared_data_t *data,shsignature_t *sig)
{
	int i;

	for (i=0;i<data->num_processes;i++){
		print_sh_signature(i,&sig[i]);
	}
}
void print_ready_shared_signatures(int master_rank,lib_shared_data_t *data,shsignature_t *sig)
{
	if (master_rank < 0) return;
	if (are_signatures_ready(data,sig)){
		print_shared_signatures(data,sig);
	}
}



void copy_my_sig_info(lib_shared_data_t *data,shsignature_t *sig,shsignature_t *rem_sig)
{
	memcpy(rem_sig,sig,sizeof(shsignature_t)*data->num_processes);
}

void shsignature_copy(shsignature_t *dst,shsignature_t *src)
{
	memcpy(dst,src,sizeof(shsignature_t));
}

void compute_per_node_avg_mpi_info(lib_shared_data_t *data,shsignature_t *sig,mpi_information_t *my_mpi_info)
{
	mpi_information_t avg_mpi;
	avg_mpi.total_mpi_calls = sig[0].mpi_info.total_mpi_calls;
	avg_mpi.exec_time = sig[0].mpi_info.exec_time;
	avg_mpi.mpi_time = sig[0].mpi_info.mpi_time;
	avg_mpi.rank = sig[0].mpi_info.rank;
	avg_mpi.perc_mpi = sig[0].mpi_info.perc_mpi;
  int i;
	//debug("compute_per_node_mpi_info, num_procs %d",data->num_processes);
  for (i=1;i<data->num_processes;i++){
		avg_mpi.total_mpi_calls += sig[i].mpi_info.total_mpi_calls;
		avg_mpi.exec_time += sig[i].mpi_info.exec_time;
		avg_mpi.mpi_time += sig[i].mpi_info.mpi_time;
		avg_mpi.perc_mpi += sig[i].mpi_info.perc_mpi;
  }
	my_mpi_info->rank = 0;
	my_mpi_info->total_mpi_calls = avg_mpi.total_mpi_calls/data->num_processes;
	my_mpi_info->exec_time = avg_mpi.exec_time/data->num_processes;
	my_mpi_info->mpi_time = avg_mpi.mpi_time/data->num_processes;
	/* Should we use the minimum? */
	my_mpi_info->perc_mpi = avg_mpi.perc_mpi/data->num_processes;
}

void compute_total_node_instructions(lib_shared_data_t *data,shsignature_t *sig,ull *t_inst)
{
	int i;
	*t_inst = 0;
	for (i=0;i<data->num_processes;i++){
		*t_inst += sig[i].sig.instructions;
	}
}

void compute_total_node_flops(lib_shared_data_t *data,shsignature_t *sig,ull *flops)
{
	int i,f;
	memset(flops,0,FLOPS_EVENTS*sizeof(ull));
	for (i=0;i<data->num_processes;i++){
		for (f=0;f<FLOPS_EVENTS;f++) flops[f] += sig[i].sig.FLOPS[f];
	}
}

void compute_total_node_cycles(lib_shared_data_t *data,shsignature_t *sig,ull *t_cycles)
{
  int i;
  *t_cycles = 0;
  for (i=0;i<data->num_processes;i++){
    *t_cycles += sig[i].sig.cycles;
  }
}

void compute_total_node_L3_misses(lib_shared_data_t *data,shsignature_t *sig,ull *t_L3)
{
  int i;
  *t_L3 = 0;
  for (i=0;i<data->num_processes;i++){
    *t_L3 += sig[i].sig.L3_misses;
  }
}

void compute_total_node_CPI(lib_shared_data_t *data,shsignature_t *sig,double *CPI)
{
  int i;
  *CPI = 0;
  for (i=0;i<data->num_processes;i++){
    *CPI += (double)sig[i].sig.CPI;
  }
	*CPI = *CPI/data->num_processes;
}

void compute_job_cpus(lib_shared_data_t *data,shsignature_t *sig,uint *cpus)
{
  int i, c;
	*cpus = 0;
  for (i=0;i<data->num_processes;i++){
		uint lcpus = num_cpus_in_mask(&sig[i].cpu_mask);
		*cpus += lcpus;
  }
	verbose_master(2,"Job %u has %u cpus ",node_mgr_index,*cpus);
}





uint compute_max_vpi(lib_shared_data_t *data,shsignature_t *sig,double *max_vpi)
{
	int i;
	double local_max = 0.0;
	uint max_ssig = 0;
	*max_vpi = 0.0;
	for (i=0;i<data->num_processes;i++){
		compute_ssig_vpi(&local_max,&sig[i].sig);
		if (local_max > *max_vpi ){ 
			*max_vpi = local_max;
			max_ssig = i;
		}
	}
	return max_ssig;
}


int compute_per_node_most_loaded_process(lib_shared_data_t *data,shsignature_t *sig)
{
	int i,mostload=0;
	double mostloadp;
	mostloadp = sig[0].mpi_info.perc_mpi;
	for (i=1;i<data->num_processes;i++){
		if (sig[i].mpi_info.perc_mpi < mostloadp){
			mostloadp = sig[i].mpi_info.perc_mpi;
			mostload = i;
		}
	}
	return mostload;
}


void compute_per_node_avg_sig_info(lib_shared_data_t *data,shsignature_t *sig,shsignature_t *my_node_sig)
{
	int i;
	//debug("compute_per_node_sig_info");
	compute_per_node_avg_mpi_info(data,sig,&my_node_sig->mpi_info);
	ssig_t avg_node;
	//print_shared_signatures(data,sig);
	set_global_metrics(&avg_node,&sig[0].sig);
	for (i=0;i<data->num_processes;i++){
		acum_ssig_metrics(&avg_node,&sig[i].sig);
	}
	compute_avg_node_sig(&avg_node,data->num_processes);
	copy_mini_sig(&my_node_sig->sig,&avg_node);
	#if 0
	fprintf(stderr,"AVG per node sig (using rank %d) \n",my_node_sig->mpi_info.rank);
	print_sh_signature(my_node_sig);	
	#endif
}


void load_app_mgr_env()
{
	char *cshow_sig=getenv(SCHED_EAR_SHOW_SIGNATURES);
	char *csh_sig_per_process=getenv(SHARE_INFO_PER_PROCESS);
	char *csh_sig_per_node=getenv(SHARE_INFO_PER_NODE);
	char *creport_node_sig=getenv(REPORT_NODE_SIGNATURES);
	char *creport_all_sig=getenv(REPORT_ALL_SIGNATURES);

	if (cshow_sig != NULL) show_signatures = atoi(cshow_sig);
	if (csh_sig_per_process != NULL) sh_sig_per_proces = atoi(csh_sig_per_process);
	if (csh_sig_per_node != NULL) sh_sig_per_node = atoi(csh_sig_per_node);
	if (creport_node_sig != NULL) report_node_sig = atoi(creport_node_sig);	
	if (creport_all_sig != NULL) report_all_sig = atoi(creport_all_sig);

	verbose_master(2,"Show_signatures %u share_sig_per_process %u share_sig_per_node %u report_node_sig %u report_all_sig %u",
	show_signatures,sh_sig_per_proces,sh_sig_per_node,report_node_sig,report_all_sig);
}

void* mpi_info_get_perc_mpi(void* mpi_inf){
    mpi_information_t mpi_info = *((mpi_information_t*) mpi_inf);
    double* res = malloc(sizeof(double));
    *res = mpi_info.perc_mpi;
    return res;
}


double compute_node_flops(lib_shared_data_t *data,shsignature_t *sig)
{
	double total = 0;
	int i;
	for (i=0;i<data->num_processes;i++) total += sig[i].sig.Gflops;
	return total;
}

void compute_total_node_avx_and_avx_fops(lib_shared_data_t *data,shsignature_t *sig,ullong *avx)
{
	ull total = 0;
	for (uint i=0;i<data->num_processes;i++){	
		total += sig[i].sig.FLOPS[INDEX_256F];
		total += sig[i].sig.FLOPS[INDEX_256D];
		total += sig[i].sig.FLOPS[INDEX_512F];
		total += sig[i].sig.FLOPS[INDEX_512D];
	}
	*avx = total;
}





/* Node mgr support functions */


void  init_earl_node_mgr_info()
{
  for (uint j = 0;j < MAX_CPUS_SUPPORTED; j++){
    uint ID;
    job_id id,sid;

		char *tmp = get_ear_tmp();
    id = node_mgr_data[j].jid;
    sid = node_mgr_data[j].sid;
    if ((id != -1) && (sid != -1)){
      /* Double validation */
      if (node_mgr_info_lock() == EAR_SUCCESS){
        id = node_mgr_data[j].jid;
        sid = node_mgr_data[j].sid;
        node_mgr_job_info[j].creation_time = node_mgr_data[j].creation_time;
        node_mgr_info_unlock();
      }
      if ((id != -1) && (sid != -1) && (j != node_mgr_index) ){
        verbose_master(2,"Job %lu.%lu is sharing the node", id,sid);
        ID = create_ID(id, sid);
        if (get_lib_shared_data_path(tmp,ID,lib_shared_region_path_jobs) == EAR_SUCCESS){
          node_mgr_job_info[j].libsh = attach_lib_shared_data_area(lib_shared_region_path_jobs, &node_mgr_job_info[j].fd_lib);
        }else{
          node_mgr_job_info[j].creation_time = 0;
        }
				if (get_shared_signatures_path(tmp,ID,sig_shared_region_path_jobs) == EAR_SUCCESS){
					node_mgr_job_info[j].shsig = attach_shared_signatures_area(sig_shared_region_path_jobs, node_mgr_job_info[j].libsh->num_processes, &node_mgr_job_info[j].fd_sig);
				}
      }else if (j == node_mgr_index){
        node_mgr_job_info[j].libsh =  lib_shared_region;
				node_mgr_job_info[j].shsig =  sig_shared_region;
      }
    }
  }
}
void    update_earl_node_mgr_info()
{
		uint current_jobs_in_node = 0;
		char *tmp = get_ear_tmp();
    for (uint j = 0; j < MAX_CPUS_SUPPORTED; j ++){
      job_id jid,sid;
      time_t ct;
      jid = node_mgr_data[j].jid;
      sid = node_mgr_data[j].sid;
      ct = node_mgr_data[j].creation_time;
      if (ct != node_mgr_job_info[j].creation_time){
        while (node_mgr_info_lock() != EAR_SUCCESS);
        jid = node_mgr_data[j].jid;
        sid = node_mgr_data[j].sid;
        ct = node_mgr_data[j].creation_time;
        node_mgr_info_unlock();
        /* Double validation after getting the lock */
        if (ct != node_mgr_job_info[j].creation_time){
          if (ct == 0){
            dettach_lib_shared_data_area(node_mgr_job_info[j].fd_lib);
						dettach_shared_signatures_area(node_mgr_job_info[j].fd_sig);
            node_mgr_job_info[j].creation_time = 0;
          }else{
            uint ID = create_ID(jid,sid);
            get_lib_shared_data_path(tmp, ID, lib_shared_region_path_jobs );
            node_mgr_job_info[j].libsh = attach_lib_shared_data_area(lib_shared_region_path_jobs,&node_mgr_job_info[j].fd_lib);
						get_shared_signatures_path(tmp, ID, sig_shared_region_path_jobs);
						node_mgr_job_info[j].shsig = attach_shared_signatures_area(sig_shared_region_path_jobs, node_mgr_job_info[j].libsh->num_processes, &node_mgr_job_info[j].fd_sig);
            node_mgr_job_info[j].creation_time = ct;
          }
        }
     
    }
		}
  	nodemgr_get_num_jobs_attached(&current_jobs_in_node);
  	exclusive = exclusive && (current_jobs_in_node == 1);
		
}

void verbose_jobs_in_node(int vl,ear_njob_t *nmgr_eard,node_mgr_sh_data_t *nmgr_earl)
{
	if ((nmgr_eard == NULL) || (nmgr_earl == NULL)) return;
	verbose_master(vl,"%sJobs in node list",COL_BLU);
	for (uint i=0;i<MAX_CPUS_SUPPORTED; i++){
		if (nmgr_eard[i].creation_time){
			verbose_master(vl,"JOB[%u]=%lu.%lu", i,nmgr_eard[i].jid,nmgr_eard[i].sid);
			signature_print_simple_fd(vl,&nmgr_earl[i].libsh->node_signature);	
			verbose_master(vl," ");
		}
	}
	verbose_master(vl,"%s",COL_CLR);
}


void estimate_power_and_gbs(signature_t *lsig, lib_shared_data_t *data,shsignature_t *sig, node_mgr_sh_data_t *nmgr, signature_t *new_sig, uint type, int lid)
{
	ull job_L3 = 0, node_L3 = 0;
	double GBS = lsig->GBS;
	double job_ratio = 0;
	double job_CPI;
	double hw_cache_line_size = arch_desc.top.cache_line_size;
  int cpus_in_node = arch_desc.top.cpu_count;	
	double power_estimations[MAX_CPUS_SUPPORTED];
	double ipc[MAX_CPUS_SUPPORTED];
	double tpi[MAX_CPUS_SUPPORTED];
	double gbs[MAX_CPUS_SUPPORTED];
	double vpi[MAX_CPUS_SUPPORTED];
	ulong  f[MAX_CPUS_SUPPORTED];
	ull    L3[MAX_CPUS_SUPPORTED];
	uint   job_in_process[MAX_CPUS_SUPPORTED];
	uint   cpus[MAX_CPUS_SUPPORTED];
	double power_ratio;
	uint   node_process = 0;
	int    gid = -1;

	double *my_power = &new_sig->DC_power;
	double *my_GBS   = &new_sig->GBS;

	verbose(2, "Estimating GBS and power type %u lid %d", type, lid);

	memset(ipc,0,sizeof(double)*MAX_CPUS_SUPPORTED);
	memset(tpi,0,sizeof(double)*MAX_CPUS_SUPPORTED);
	memset(vpi,0,sizeof(double)*MAX_CPUS_SUPPORTED);
	memset(gbs,0,sizeof(double)*MAX_CPUS_SUPPORTED);
	memset(f,0,sizeof(ulong)*MAX_CPUS_SUPPORTED);
	memset(L3,0,sizeof(ull)*MAX_CPUS_SUPPORTED);


	if ((lsig == NULL) || (data == NULL) || (sig == NULL) || (nmgr == NULL) || (my_GBS == NULL) || (my_power == NULL)) return;
	*my_power = lsig->DC_power;
	*my_GBS   = 0;

	
	/* compute_total_node_fff compute the metrics only for the calling job. It is needed to iterate over the jobs to have per node */
	
	for (uint j = 0; j < MAX_CPUS_SUPPORTED; j ++){
		/* Power condition is to do only in case the signature is ready */
		if (nmgr[j].creation_time && (nmgr[j].libsh != NULL) && (nmgr[j].shsig != NULL) && (nmgr[j].libsh->node_signature.DC_power)){  
			ull avx = 0;
			ull LL3 = 0;
			ull inst = 0;
			double LCPI = 0.0;
			for (uint lp = 0; lp < nmgr[j].libsh->num_processes; lp++){
				job_in_process[node_process] = j;
				L3[node_process]  = nmgr[j].shsig[lp].sig.L3_misses;
				ipc[node_process] = 1/nmgr[j].shsig[lp].sig.CPI;
				tpi[node_process] = (double)L3[node_process]/(double)nmgr[j].shsig[lp].sig.instructions;
				avx = nmgr[j].shsig[lp].sig.FLOPS[INDEX_256F] + nmgr[j].shsig[lp].sig.FLOPS[INDEX_256D] + nmgr[j].shsig[lp].sig.FLOPS[INDEX_512F] + nmgr[j].shsig[lp].sig.FLOPS[INDEX_512D];
				vpi[node_process] = (double)avx/(double)nmgr[j].shsig[lp].sig.instructions;
				f[node_process]   = nmgr[j].shsig[lp].sig.avg_f;
				node_L3 += L3[node_process];
				if ((j == node_mgr_index) && (lp == lid)) gid = node_process;
				node_process++;
			}

		}
	}
	verbose(2,"My gid is %d", gid);

	for (uint j = 0; j < node_process; j ++){
    if (L3[j] && (job_in_process[j] == node_mgr_index)){
        gbs[j] = ((double)L3[j]/(double)node_L3)*lsig->GBS;
				if (type == PER_JOB) *my_GBS += gbs[j];
				else if (j == gid )  *my_GBS = gbs[j];
    }else gbs[j] = 0;
  }


  //*my_GBS = gbs[node_mgr_index];
	verbose_master(2,"My GB/s is %.2lf", *my_GBS);


	/* Power : Power is estimated based on CPU activity and Memory activity. Less CPI means more CPU activity and more L3 means more Mem actitivy */


	/* Model 1
	*	Intercepción	-28,53203477 --> Constant
	*	IPC	12,90765889 --> 1/CPI
	*	TPI2	0,475457643 --> TPI
	*	VPI	5,45086405 --> (avx2+avx512)/instrunctions
	*	F	9,83884E-05  --> AVG CPU 

	#define IPC_COEFF (double)12.90765889
	#define TPI_COEFF (double)0.475457643
	#define VPI_COEFF (double)5.45086405
	#define F_COEFF   (double)9.83884E-05
	#define INTERCEPT (double)-28,53203477 */

	/* Model 2, GB/s rather than TPI, easy to compute 
 * Intercepción	-28,85109789
 * VPI	28,21247619
 * F	9,26892E-05
 * GBS2	0,601652592
 * IPC2	11,40897054 */
	#define IPC_COEFF (double)11.40897054
  #define GBS_COEFF (double)0.601652592
  #define VPI_COEFF (double)28.21247619
  #define F_COEFF   (double)9.26892E-05
  #define INTERCEPT (double)-28.85109789



	/* Per job in node loop */
	double total_power_estimated = 0, CPU_power, GPU_power = 0;
	uint lp = 0;
	for (uint j = 0; j < MAX_CPUS_SUPPORTED; j ++){
		/* If job is active we compute its power estimated */
		if (nmgr[j].creation_time && (nmgr[j].libsh != NULL) && (nmgr[j].shsig != NULL) && nmgr[j].libsh->node_signature.avg_f){
			for (uint proc = 0;proc < nmgr[j].libsh->num_processes; proc++){
				double P;
				P = (ipc[lp] * IPC_COEFF) + (gbs[lp] * GBS_COEFF) + (vpi[lp] * VPI_COEFF) + (f[lp] * F_COEFF ) + INTERCEPT;
				power_estimations[lp] = P*nmgr[j].shsig[proc].num_cpus/cpus_in_node;
				verbose(2,"Power estimated for job %u/%u --> %.2lf/with %u cpus = %.3lf (ipc %.3lf,vpi %.3lf, gbs %.3lf f %.2lf",j,proc, P,nmgr[j].shsig[proc].num_cpus, power_estimations[lp],ipc[lp],vpi[lp],gbs[lp],(double)f[lp]/1000000.0);
				total_power_estimated += power_estimations[lp];
			
				lp++;
			}
		}
	}
	power_ratio = 0.0;
	for (lp =0; lp < node_process; lp++){
		if (type == PER_JOB){
			if (job_in_process[lp] == node_mgr_index){ 
				power_ratio += power_estimations[lp]/total_power_estimated;
			}
		}else{ 
			if (lp == gid) power_ratio = power_estimations[lp]/total_power_estimated;
		}
	}
	CPU_power = lsig->DC_power;
	#if USE_GPUS
	for (uint gid = 0; gid < lsig->gpu_sig.num_gpus; gid++){	
		GPU_power +=  lsig->gpu_sig.gpu_data[gid].GPU_power;
	}
	#endif
	CPU_power -= GPU_power;
	*my_power = (CPU_power * power_ratio) + GPU_power;
	verbose(2,"My power is %lf (node (CPU %lf + GPU %lf) x ratio %lf)", *my_power, CPU_power, GPU_power, power_ratio);

	return;	
}
