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

#define _GNU_SOURCE

//#define SHOW_DEBUGS           1
#define INFO_METRICS           2
#define IO_VERB                3
#define SET_DYN_UNC_BY_DEFAULT 1

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <sys/mman.h>

#include <common/config.h>
#include <common/states.h>
#include <common/system/time.h>
#include <common/output/verbose.h>
#include <common/types/signature.h>
#include <common/math_operations.h>
#include <common/hardware/hardware_info.h>

#include <library/api/clasify.h>
#include <library/common/externs.h>
#include <library/common/verbose_lib.h>
#include <library/common/library_shared_data.h>
#include <library/metrics/metrics.h>
#include <library/policies/common/mpi_stats_support.h> /* Should we move this file out out policies? */
#if USE_GPUS
#include <library/metrics/gpu.h>
#endif

#include <metrics/io/io.h>
#include <metrics/cpi/cpi.h>
#include <metrics/cache/cache.h>
#include <metrics/flops/flops.h>
#include <metrics/cpufreq/cpufreq.h>
#include <metrics/imcfreq/imcfreq.h>
#include <metrics/bandwidth/bandwidth.h>
#include <metrics/energy/energy_node_lib.h>
#include <management/cpufreq/cpufreq.h>
#include <management/imcfreq/imcfreq.h>
#include <daemon/local_api/eard_api.h>

// Registers
#define LOO         0 // loop
#define APP         1 // application
#define ACUM        2
#define LOO_NODE    2 // We will share the same pos than for accumulated values
#define APP_NODE    3
#define SIG_BEGIN 	0
#define SIG_END		1

extern int         dispose;
// Hardware
static double      hw_cache_line_size;
// Options
static int         rapl_elements;
static ulong       node_energy_datasize;
static uint        node_energy_units;
/* These vectors have position APP for application granularity and LOO for loop granularity */
static int         num_packs=0;
static ull        *metrics_rapl[2]; // nJ (vec)
static ull        *aux_rapl; // nJ (vec)
static ull        *last_rapl; // nJ (vec)
static llong       aux_time;
static edata_t     aux_energy;
static edata_t     metrics_ipmi[2]; // mJ
static ulong       acum_ipmi[2];
static edata_t     aux_energy_stop;
static llong       metrics_usecs[2]; // uS
/* In the case of IO we will collect per process, per loop and per node */
static io_data_t   metrics_io_init[4];
static io_data_t   metrics_io_end[4];
static io_data_t   metrics_io_diff[4];
static ctx_t       ctx_io;
/* MPI statistics */
mpi_information_t *metrics_mpi_calls[2];
mpi_information_t *metrics_last_mpi_calls[2];
mpi_calls_types_t *metrics_mpi_calls_types[2];
mpi_calls_types_t *metrics_last_mpi_calls_types[2];
#if USE_GPUS
/* These vectors have position APP for application granularity and LOO for loop granularity */
static gpu_t      *gpu_metrics_init[2];
static gpu_t      *gpu_metrics_end[2];
static gpu_t      *gpu_metrics_diff[2];
static ctx_t       gpu_lib_ctx;
       uint        gpu_initialized = 0;
static uint        gpu_loop_stopped = 0;
static uint        earl_gpu_model = MODEL_UNDEFINED;
char               gpu_str[256];
uint               ear_num_gpus_in_node=0;
#endif
/* These vectors have position APP for application granularity and LOO for loop granularity */
static llong       last_elap_sec = 0;
static llong       app_start;
/* Node mgr data */
extern uint        exclusive;
extern node_mgr_sh_data_t *node_mgr_job_info;
/* New MEM FREQ mgt API */
uint        imc_devices;
pstate_t   *imc_pstates; //*
uint        imc_num_pstates; //*
ulong       def_imc_max_khz; //*
ulong       def_imc_min_khz; //*
ulong       imc_max_khz; //*
ulong       imc_min_khz; //*
uint       *imc_max_pstate; //*
uint       *imc_min_pstate; //*
uint        imc_curr_max_pstate; //*
uint        imc_curr_min_pstate; //*
/* New apis */
static imcfreq_t  *imcfreq_read1[2];
static imcfreq_t  *imcfreq_read2[2];
static ulong       imcfreq_avrg[2];
static ulong      *imcfreq_diff[2];
static cpufreq_t  *cpufreq_read1[2];
static cpufreq_t  *cpufreq_read2[2];
       ulong      *cpufreq_diff;
static ulong       cpufreq_avrg[2];
static bwidth_t   *bwidth_read1[2];
static bwidth_t   *bwidth_read2[2];
static bwidth_t   *bwidth_diff[2];
static ullong      bwidth_cas[2];
static flops_t     flops_read1[2];
static flops_t     flops_read2[2];
static flops_t     flops_diff[2];
static cache_t     cache_read1[2];
static cache_t     cache_read2[2];
static cache_t     cache_diff[2];
static double      cache_avrg[2];
static cpi_t       cpi_read1[2];
static cpi_t       cpi_read2[2];
static cpi_t       cpi_diff[2];
static double      cpi_avrg[2];
/* New metrics */
static metrics_t   mgt_cpufreq;
static metrics_t   mgt_imcfreq;
static metrics_t   met_cpufreq;
static metrics_t   met_imcfreq;
static metrics_t   met_bwidth;
static metrics_t   met_flops;
static metrics_t   met_cache;
static metrics_t   met_cpi;
/* New status */
static uint master;
static uint eard;

const metrics_t *metrics_get(uint api)
{
    switch(api) {
        case MGT_CPUFREQ: return &mgt_cpufreq;
        case MGT_IMCFREQ: return &mgt_imcfreq;
        case MET_CPUFREQ: return &met_cpufreq;
        case MET_IMCFREQ: return &met_imcfreq;
        case MET_BWIDTH:  return &met_bwidth;
        case MET_FLOPS:   return &met_flops;
        case MET_CACHE:   return &met_cache;
        case MET_CPI:     return &met_cpi;
        default: return NULL;
    }
}

static void metrics_static_init(topology_t *tp)
{
    char buffer[SZ_BUFFER_EXTRA];

    #define sa(function) \
        function;

    master = (masters_info.my_master_rank >= 0);
    eard   = (master);
    // Load
    sa(mgt_cpufreq_load(tp, eard));
    sa(mgt_imcfreq_load(tp, eard));
    sa(    cpufreq_load(tp, eard));
    sa(    imcfreq_load(tp, eard));
    sa(     bwidth_load(tp, eard));
    sa(      flops_load(tp, eard));
    sa(      cache_load(tp, eard));
    sa(        cpi_load(tp, eard));
    // This extinguishable
    frequency_init(eard);
    // Get API
    sa(mgt_cpufreq_get_api(&mgt_cpufreq.api));
    sa(mgt_imcfreq_get_api(&mgt_imcfreq.api));
    sa(    cpufreq_get_api(&met_cpufreq.api));
    sa(    imcfreq_get_api(&met_imcfreq.api));
    sa(     bwidth_get_api(&met_bwidth.api ));
    sa(      flops_get_api(&met_flops.api  ));
    sa(      cache_get_api(&met_cache.api  ));
    sa(        cpi_get_api(&met_cpi.api    ));
    // Working
    mgt_cpufreq.ok = (mgt_cpufreq.api > API_DUMMY);
    mgt_imcfreq.ok = (mgt_imcfreq.api > API_DUMMY);
    met_cpufreq.ok = (met_cpufreq.api > API_DUMMY);
    met_imcfreq.ok = (met_imcfreq.api > API_DUMMY);
    met_bwidth.ok  = (met_bwidth.api  > API_DUMMY);
    met_flops.ok   = (met_flops.api   > API_DUMMY);
    met_cache.ok   = (met_cache.api   > API_DUMMY);
    met_cpi.ok     = (met_cpi.api     > API_DUMMY);
    // Init
    sa(mgt_cpufreq_init(no_ctx));
    sa(mgt_imcfreq_init(no_ctx));
    sa(    cpufreq_init(no_ctx));
    sa(    imcfreq_init(no_ctx));
    sa(     bwidth_init(no_ctx));
    sa(      flops_init(no_ctx));
    sa(      cache_init(no_ctx));
    sa(        cpi_init(no_ctx));
    // Count
    sa(mgt_cpufreq_count_devices(no_ctx, &mgt_cpufreq.devs_count));
    sa(mgt_imcfreq_count_devices(no_ctx, &mgt_imcfreq.devs_count));
    sa(    cpufreq_count_devices(no_ctx, &met_cpufreq.devs_count));
    sa(    imcfreq_count_devices(no_ctx, &met_imcfreq.devs_count));
    sa(     bwidth_count_devices(no_ctx, &met_bwidth.devs_count ));
    // Available lists
    sa(mgt_cpufreq_get_available_list(no_ctx, (const pstate_t **) &mgt_cpufreq.avail_list, &mgt_cpufreq.avail_count));
    sa(mgt_imcfreq_get_available_list(no_ctx, (const pstate_t **) &mgt_imcfreq.avail_list, &mgt_imcfreq.avail_count));

    pstate_tostr(mgt_cpufreq.avail_list, mgt_cpufreq.avail_count, buffer, SZ_BUFFER_EXTRA);
    verbose_master(2, "CPUFREQ available list of frequencies:\n%s", buffer);
    pstate_tostr(mgt_imcfreq.avail_list, mgt_imcfreq.avail_count, buffer, SZ_BUFFER_EXTRA);
    verbose_master(2, "IMCFREQ available list of frequencies:\n%s", buffer);

    apis_print(mgt_cpufreq.api, "MGT_CPUFREQ: ");
    apis_print(mgt_imcfreq.api, "MGT_IMCFREQ: ");
    apis_print(met_cpufreq.api, "MET_CPUFREQ: ");
    apis_print(met_imcfreq.api, "MET_IMCFREQ: ");
    apis_print(met_bwidth.api , "MET_BWIDTH : ");
    apis_print(met_flops.api  , "MET_FLOPS  : ");
    apis_print(met_cache.api  , "MET_CACHE  : ");
    apis_print(met_cpi.api    , "MET_CPI    : ");

    cpufreq_data_alloc(&cpufreq_read1[0], empty);
    cpufreq_data_alloc(&cpufreq_read1[1], empty);
    cpufreq_data_alloc(&cpufreq_read2[0], empty);
    cpufreq_data_alloc(&cpufreq_read2[1], &cpufreq_diff);

    imcfreq_data_alloc(&imcfreq_read1[0], empty);
    imcfreq_data_alloc(&imcfreq_read1[1], empty);
    imcfreq_data_alloc(&imcfreq_read2[0], &imcfreq_diff[0]);
    imcfreq_data_alloc(&imcfreq_read2[1], &imcfreq_diff[1]);

    bwidth_data_alloc(&bwidth_read1[0]);
    bwidth_data_alloc(&bwidth_read1[1]);
    bwidth_data_alloc(&bwidth_read2[0]);
    bwidth_data_alloc(&bwidth_read2[1]);
    bwidth_data_alloc(&bwidth_diff[0]);
    bwidth_data_alloc(&bwidth_diff[1]);
}

void metrics_static_dispose()
{
}

// Legacy code
static void cpufreq_my_avgcpufreq(ulong *nodecpuf, ulong *avgf)
{
	cpu_set_t m;
	ulong total = 0, total_cpus = 0;
	ulong lavg, lcpus;
	int p;
    for (p = 0; p < lib_shared_region->num_processes ; p++)
    {
        m = sig_shared_region[p].cpu_mask;
		pstate_freqtoavg(m, nodecpuf, met_cpufreq.devs_count, &lavg, &lcpus);
		total += lavg*lcpus;
		total_cpus += lcpus;
	}
	*avgf = total/total_cpus;
}

void set_null_dc_energy(edata_t e)
{
    memset((void *)e,0,node_energy_datasize);
}

void set_null_rapl(ull *erapl)
{
    memset((void*)erapl,0,rapl_elements*sizeof(ull));
}

long long metrics_time()
{
    return (llong) timestamp_getconvert(TIME_USECS);
}

static void metrics_global_start()
{
	aux_time = metrics_time();
	app_start = aux_time;
    state_t s;
	int ret;

	if (masters_info.my_master_rank>=0)
	{
		/* Avg CPU freq */
        if (state_fail(s = cpufreq_read(no_ctx, cpufreq_read1[APP]))) {
            debug("CPUFreq data read in global_start");
        }
		/* Reads IMC data */
		if (mgt_imcfreq.ok){
		    debug("imcfreq_read");
		    if (state_fail(s = imcfreq_read(no_ctx, imcfreq_read1[APP]))){
			    debug("IMCFreq data read in global_start");
		    }
		}
		/* Energy */
		debug("eards_node_dc_energy");
		ret = eards_node_dc_energy(aux_energy,node_energy_datasize);
		if ((ret == EAR_ERROR) || energy_lib_data_is_null(aux_energy)){
			debug("MR[%d] Error reading Node energy at application start",masters_info.my_master_rank);
		}
		/* RAPL energy metrics */
		eards_read_rapl(aux_rapl);

		#if USE_GPUS
		/* GPu metrics */
		if (gpu_initialized){
			gpu_lib_data_null(gpu_metrics_init[APP]);
			if (gpu_lib_read(&gpu_lib_ctx,gpu_metrics_init[APP]) != EAR_SUCCESS){
				debug("Error in gpu_read in application start");
			}
			debug("gpu_lib_read in global start");
			gpu_lib_data_copy(gpu_metrics_end[LOO],gpu_metrics_init[APP]);
		}
		#endif
		/* Only the master computes per node metrics */
		metrics_get_total_io(&metrics_io_init[APP_NODE]);
	}else{
		set_null_dc_energy(aux_energy);
		set_null_rapl(aux_rapl);
	}

	/* Per process IO data */
	if (io_read(&ctx_io,&metrics_io_init[APP]) != EAR_SUCCESS){
		verbose_master(2,"Warning: IO data not available");
	}

    // New metrics
    bwidth_read(no_ctx, bwidth_read1[APP]);
     flops_read(no_ctx, &flops_read1[APP]);
     cache_read(no_ctx, &cache_read1[APP]);
       cpi_read(no_ctx,   &cpi_read1[APP]);
}
 
static void metrics_global_stop()
{
	char io_info[256];
	char file_name[256];
	char *verb_path=getenv(SCHED_EARL_VERBOSE_PATH);
    state_t s;
	int i;

    verb(2, "%s--- process %d calling METRICS GLOBAL STOP ---%s", COL_YLW, my_node_id, COL_CLR);
	
	if (masters_info.my_master_rank < 0){
		while(sig_shared_region[my_node_id].ready){ 
			usleep(10);
		}
	}
	imcfreq_avrg[APP] = 0;
	cpufreq_avrg[APP] = 0;
	/* Onle the master will collect these metrics */
	if (masters_info.my_master_rank>=0) {
        /* MPI statistics */
        if (is_mpi_enabled()){
          read_diff_node_mpi_data(lib_shared_region, sig_shared_region, metrics_mpi_calls[APP], metrics_last_mpi_calls[APP]);
          read_diff_node_mpi_type_info(lib_shared_region, sig_shared_region, metrics_mpi_calls_types[APP], metrics_last_mpi_calls_types[APP]);
        }

        /* Avg CPU frequency */
        if (state_fail(s = cpufreq_read(no_ctx, cpufreq_read2[APP]))) {
            error("CPUFreq data read in global_stop");
        }
        if (state_fail(s = cpufreq_data_diff(cpufreq_read2[APP], cpufreq_read1[APP], cpufreq_diff, &cpufreq_avrg[APP]))){
            error("CPUFreq data diff");
        }else{
            debug("CPUFreq average for the application is %.2fGHz", (float) cpufreq_avrg[APP] / 1000000.0);
        }
        cpufreq_my_avgcpufreq(cpufreq_diff, &cpufreq_avrg[APP]);

        /* AVG IMC frequency */
        if (mgt_imcfreq.ok) {
            if (state_fail(s = imcfreq_read(no_ctx, imcfreq_read2[APP]))) {
                error("IMCFreq data read in global_start");
            }
            if (state_fail(s = imcfreq_data_diff(imcfreq_read2[APP],imcfreq_read1[APP], imcfreq_diff[APP], &imcfreq_avrg[APP]))){
                error("IMC data diff fails");
            }
            verbose_master(2,"AVG IMC frequency %.2f", (float)imcfreq_avrg[APP]/1000000.0);
            for (i=0; i< mgt_imcfreq.devs_count;i++) {
                imc_max_pstate[i]= imc_num_pstates-1;
                imc_min_pstate[i] = 0;
            }
            if (state_fail(s = mgt_imcfreq_set_current_ranged_list(no_ctx, imc_min_pstate,imc_max_pstate))){
                error("Setting the IMC freq in global stop");
            }
            verbose_master(2,"IMC freq restored to %.2f-%.2f",(float)imc_pstates[imc_min_pstate[0]].khz/1000000.0,(float)imc_pstates[imc_max_pstate[0]].khz/1000000.0);
        }
	} //master_rank

	/* Only the master will collect these metrics */
	if (masters_info.my_master_rank>=0){
		/* Uncore counters to compute memory bandwith */
		#if USE_GPUS
		/* GPU */
		if (gpu_initialized){
		if (gpu_lib_read(&gpu_lib_ctx,gpu_metrics_end[APP])!= EAR_SUCCESS){
			debug("gpu_lib_read error");
		}
		debug("gpu_read in global_stop");
		gpu_lib_data_diff(gpu_metrics_end[APP], gpu_metrics_init[APP], gpu_metrics_diff[APP]);
		gpu_lib_data_tostr(gpu_metrics_diff[APP],gpu_str,sizeof(gpu_str));
		debug("gpu_data in global_stop %s",gpu_str);
		}
		#endif
		/* IO node data */
		metrics_get_total_io(&metrics_io_end[APP_NODE]);
		io_diff(&metrics_io_diff[APP_NODE],&metrics_io_init[APP_NODE],&metrics_io_end[APP_NODE]);
	}

	/* Per process IO */
	if (io_read(&ctx_io,&metrics_io_end[APP]) != EAR_SUCCESS){
		verbose_master(2,"Warning: IO data not available");
	}
	io_diff(&metrics_io_diff[APP],&metrics_io_init[APP],&metrics_io_end[APP]);
	if (verb_level >= IO_VERB){
		int fd;
		char *io_buffer;
		io_tostr(&metrics_io_diff[APP],io_info,sizeof(io_info));
		if (verb_path != NULL){ 
			sprintf(file_name, "%s/earl_io_info.%d.%d", verb_path,my_step_id, my_job_id);
			fd = open(file_name,O_WRONLY|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR|S_IRGRP);
		}else fd = 2;
		io_buffer = malloc(strlen(io_info) + 200);
		sprintf(io_buffer,"[%d] APP IO DATA: %s\n",ear_my_rank,io_info);
		write(fd,io_buffer,strlen(io_buffer));
		if (verb_path != NULL) close(fd);
	}

    // New metrics
    bwidth_read_diff(no_ctx, bwidth_read2[APP], bwidth_read1[APP], NULL, NULL, NULL);
     flops_read_diff(no_ctx, &flops_read2[APP], &flops_read1[APP], NULL, NULL);
     cache_read_diff(no_ctx, &cache_read2[APP], &cache_read1[APP], &cache_diff[APP], &cache_avrg[APP]);
       cpi_read_diff(no_ctx,   &cpi_read2[APP],   &cpi_read1[APP],   &cpi_diff[APP],   &cpi_avrg[APP]);
}

static void metrics_partial_start()
{
    state_t s;
    int i;

    /* This must be replaced by a copy of energy */
    /* Energy node */
    memcpy(metrics_ipmi[LOO],aux_energy,node_energy_datasize);
    /* Time */
    metrics_usecs[LOO]=aux_time;
    last_elap_sec = 0;
    /* Per process IO data */
    if (io_read(&ctx_io,&metrics_io_init[LOO]) != EAR_SUCCESS){
        verbose_master(2,"Warning:IO data not available");
    }
    /* These data is measured only by the master */
    if (masters_info.my_master_rank>=0){
        /* Avg CPU freq */
        if (state_fail(s = cpufreq_read(no_ctx, cpufreq_read1[LOO]))) {
            verbose_master(2, "CPUFreq data read in partial start");
        }

        /* Avg IMC freq */
        if (mgt_imcfreq.ok){
            /* Reads the IMC */
            if (state_fail(s = imcfreq_read(no_ctx,imcfreq_read1[LOO]))){
			    verbose_master(2,"IMCFreq data read in global_start");
            }
        }
        /* Per NODE IO data */
        metrics_get_total_io(&metrics_io_init[LOO_NODE]);
#if USE_GPUS
        /* GPUS */
        if (gpu_initialized){
            if (gpu_loop_stopped) {
                gpu_lib_data_copy(gpu_metrics_init[LOO],gpu_metrics_end[LOO]);
            } else{
                gpu_lib_data_copy(gpu_metrics_init[LOO],gpu_metrics_init[APP]);
            }
            debug("gpu_copy in partial start");
        }
#endif
    } //master_rank

    /* Energy RAPL */
    for (i = 0; i < rapl_elements; i++) {
        last_rapl[i]=aux_rapl[i];
    }

    // New metrics
    bwidth_read(no_ctx, bwidth_read1[LOO]);
     flops_read(no_ctx, &flops_read1[LOO]);
     cache_read(no_ctx, &cache_read1[LOO]);
       cpi_read(no_ctx,   &cpi_read1[LOO]);
}

/****************************************************************************************************************************************************************/
/*************************** This function is executed every N seconds to check if signature can be compute *****************************************************/
/****************************************************************************************************************************************************************/

extern uint ear_iterations;
extern int ear_guided;

static int metrics_partial_stop(uint where)
{
	int i,ret;
	ulong c_energy;
	long long c_time;
	float c_power;
	long long aux_time_stop;
	char stop_energy_str[256],start_energy_str[256];
    state_t s;

	  if (!eards_connected() && (masters_info.my_master_rank >= 0)){
      lib_shared_region->earl_on = 0;
		}

    /* If the signature of the master is not ready, we cannot compute our signature */
    //debug("My rank is %d",masters_info.my_master_rank);
    if ((masters_info.my_master_rank<0) && (!sig_shared_region[0].ready)){
        debug("Master signature not ready at time %lld",(metrics_time() - app_start)/1000000);
        debug("Iterations %u ear_guided %u",ear_iterations,ear_guided);
        return EAR_NOT_READY;
    }

	// Manual IPMI accumulation
	if (masters_info.my_master_rank>=0){
		/* Energy node */
		ret = eards_node_dc_energy(aux_energy_stop,node_energy_datasize);
		if (energy_lib_data_is_null(aux_energy_stop) || (ret == EAR_ERROR)){ 
			if (eards_connected()) return EAR_NOT_READY;
			else 							     return EAR_SUCCESS;
		}
		energy_lib_data_accumulated(&c_energy,metrics_ipmi[LOO],aux_energy_stop);
		energy_lib_data_to_str(start_energy_str,metrics_ipmi[LOO]);	
		energy_lib_data_to_str(stop_energy_str,aux_energy_stop);	
		if ((where==SIG_END) && (c_energy==0) && (masters_info.my_master_rank>=0)){ 
			debug("EAR_NOT_READY because of accumulated energy %lu\n",c_energy);
			if (dispose) fprintf(stderr,"partial stop and EAR_NOT_READY\n");
			return EAR_NOT_READY;
		}
	}
	/* Time */
	aux_time_stop = metrics_time();
	/* Sometimes energy is not zero but power is not correct */
	c_time=metrics_usecs_diff(aux_time_stop, metrics_usecs[LOO]);
	/* energy is computed in node_energy_units and time in usecs */
	//debug("Energy computed %lu, time %lld",c_energy,c_time);
	c_power=(float)(c_energy*(1000000.0/(double)node_energy_units))/(float)c_time;

	if (masters_info.my_master_rank>=0){
	if (dispose && ((c_power<0) || (c_power>system_conf->max_sig_power))){
		debug("dispose and c_power %lf\n",c_power);	
		debug("power %f energy %lu time %llu\n",c_power,c_energy,c_time);
	}
	}

	/* If we are not the node master, we will continue */
	if ((where==SIG_END) && (c_power<system_conf->min_sig_power) && (masters_info.my_master_rank>=0)){ 
		debug("EAR_NOT_READY because of power %f\n",c_power);
		return EAR_NOT_READY;
	}

	/* This is new to avoid cases where uncore gets frozen */
	if (masters_info.my_master_rank>=0){
		#if USE_GPUS
		/* GPUs */
    if (gpu_initialized){
    if (gpu_lib_read(&gpu_lib_ctx,gpu_metrics_end[LOO])!= EAR_SUCCESS){
      debug("gpu_lib_read error");
    }
		debug("gpu_read in partial stop");
		gpu_loop_stopped=1;
    gpu_lib_data_diff(gpu_metrics_end[LOO], gpu_metrics_init[LOO], gpu_metrics_diff[LOO]);
		gpu_lib_data_tostr(gpu_metrics_diff[LOO],gpu_str,sizeof(gpu_str));
		debug("gpu_diff in partial_stop: %s",gpu_str);
    }
		#endif
	}
	/* End new section to check frozen uncore counters */
	energy_lib_data_copy(aux_energy,aux_energy_stop);
	aux_time=aux_time_stop;

	if (masters_info.my_master_rank>=0){
		if (c_power < system_conf->max_sig_power){
			acum_ipmi[LOO] = c_energy;
		}else{
			verbose_master(2,"Computed power was not correct (%lf) reducing it to %lf\n",c_power,system_conf->min_sig_power);
			acum_ipmi[LOO] = system_conf->min_sig_power*c_time;
		}
		acum_ipmi[APP] += acum_ipmi[LOO];
	}
	/* Per process IO data */
  if (io_read(&ctx_io,&metrics_io_end[LOO]) != EAR_SUCCESS){
    verbose_master(2,"Warning:IO data not available");
  }
	io_diff(&metrics_io_diff[LOO],&metrics_io_init[LOO],&metrics_io_end[LOO]);


	//  Manual time accumulation
	metrics_usecs[LOO] = c_time;
	metrics_usecs[APP] += metrics_usecs[LOO];
	
    // Daemon metrics
    if (masters_info.my_master_rank>=0) {
        /* MPI statistics */
        if (is_mpi_enabled()){
        read_diff_node_mpi_data(lib_shared_region, sig_shared_region, metrics_mpi_calls[LOO], metrics_last_mpi_calls[LOO]);
        read_diff_node_mpi_type_info(lib_shared_region, sig_shared_region, metrics_mpi_calls_types[LOO], metrics_last_mpi_calls_types[LOO]);
        }

        /* Avg CPU freq */
        if (state_fail(s = cpufreq_read(no_ctx, cpufreq_read2[LOO]))) {
            verbose_master(2,"CPUFreq data read in partial stop");
        }
        if (xtate_fail(s,cpufreq_data_diff(cpufreq_read2[LOO], cpufreq_read1[LOO], cpufreq_diff, &cpufreq_avrg[LOO]))){
            verbose_master(2,"CPUFreq data diff");
        }else{
            verbose_master(2,"CPUFreq average is %.2fGHz",(float)cpufreq_avrg[LOO]/1000000.0);
        }
		for (int cpu = 0; cpu < mtopo.cpu_count;cpu++){ 
			verbosen_master(3,"AVGCPU[%d] = %.2f ",cpu,(float)cpufreq_diff[cpu]/1000000.0);
		}
		verbosen_master(3,"\n");
		cpufreq_my_avgcpufreq(cpufreq_diff, &cpufreq_avrg[LOO]);

        /* Avg IMC frequency */
        if (mgt_imcfreq.ok){
            if (state_fail(s = imcfreq_read(no_ctx,imcfreq_read2[LOO]))){
                verbose_master(2,"IMCFreq data read in partial_stop");
            }
            if (state_fail(s = imcfreq_data_diff(imcfreq_read2[LOO],imcfreq_read1[LOO], imcfreq_diff[LOO], &imcfreq_avrg[LOO]))){
                verbose_master(2,"IMC data diff fails");
            }
            debug("AVG IMC frequency %.2f",(float)imcfreq_avrg[LOO]/1000000.0);
		}

		/* Per node IO data */
		metrics_get_total_io(&metrics_io_end[LOO_NODE]);
		io_diff(&metrics_io_diff[LOO_NODE],&metrics_io_init[LOO_NODE],&metrics_io_end[LOO_NODE]);

	/* This code needs to be adapted to read , compute the difference, and copy begin=end 
 	* diff_uncores(values,values_end,values_begin,num_counters);
 	* copy_uncores(values_begin,values_end,num_counters);
 	*/

	/* RAPL energy */
		eards_read_rapl(aux_rapl);

		// We read acuumulated energy
		for (i = 0; i < rapl_elements; i++) {
			if (aux_rapl[i] < last_rapl[i]) {
				metrics_rapl[LOO][i] = ullong_diff_overflow(last_rapl[i], aux_rapl[i]);
			}
			else {
				metrics_rapl[LOO][i]=aux_rapl[i]-last_rapl[i];		
			}
		}
	
		// Manual RAPL accumulation
		for (i = 0; i < rapl_elements; i++) {
				metrics_rapl[APP][i] += metrics_rapl[LOO][i];
		}
    } //master_rank (metrics collected by node_master)

	// New metrics
    bwidth_read_diff(no_ctx, bwidth_read2[LOO], bwidth_read1[LOO], bwidth_diff[LOO], NULL, NULL);
     flops_read_diff(no_ctx, &flops_read2[LOO], &flops_read1[LOO], &flops_diff[LOO], NULL);
     cache_read_diff(no_ctx, &cache_read2[LOO], &cache_read1[LOO], &cache_diff[LOO], &cache_avrg[LOO]);
       cpi_read_diff(no_ctx,   &cpi_read2[LOO],   &cpi_read1[LOO],   &cpi_diff[LOO],   &cpi_avrg[LOO]);

    // New accumulations: by now these application metrics are taken accumulating loop values.
    // Not sure if required this way in the future, is just because if there are too long apps
    // that could overflow these metric counters.
    bwidth_data_accum(bwidth_diff[APP], bwidth_diff[LOO], NULL, NULL);
     flops_data_accum(&flops_diff[APP], &flops_diff[LOO], NULL);

	return EAR_SUCCESS;
}

ull metrics_vec_inst(signature_t *metrics)
{
    ullong fops = 0LLU;
    if (metrics->FLOPS[INDEX_512F] > 0) fops += metrics->FLOPS[INDEX_512F] / WEIGHT_512F;
    if (metrics->FLOPS[INDEX_512D] > 0) fops += metrics->FLOPS[INDEX_512D] / WEIGHT_512D;
    return fops;
}

void copy_node_data(signature_t *dest,signature_t *src)
{
    dest->DC_power	=	src->DC_power;
    dest->DRAM_power=	src->DRAM_power;
    dest->PCK_power	=	src->PCK_power;
    dest->avg_f			= src->avg_f;
    dest->avg_imc_f = src->avg_imc_f;
#if USE_GPUS
    memcpy(&dest->gpu_sig,&src->gpu_sig,sizeof(gpu_signature_t));
#endif
}

/******************* This function computes the signature : per loop (global = LOO) or application ( global = APP)  **************/
/******************* The node master has computed the per-node metrics and the rest of processes uses this information ***********/
static void metrics_compute_signature_data(uint global, signature_t *metrics, uint iterations, ulong procs)
{
				sig_ext_t *sig_ext;
				double time_s;
				int s;
				
                verbose(2,"Compute signature process %d %sapp/loop(1/0) %u%s", my_node_id, COL_YLW, global, COL_CLR);

				// If global is 1, it means that the global application metrics are required
				// instead the small time metrics for loops. 's' is just a signature index.
				s = global;

				/* Total time */
				time_s = (double) metrics_usecs[s] / 1000000.0;

				if ((s == APP) && exclusive) verbose_master(2," Process %d ends in exclusive mode", my_node_id);
				if ((s == APP) && !exclusive) verbose_master(2," Process %d ends in NOT exclusive mode", my_node_id);

				/* Avg CPU frequency */
				metrics->avg_f = cpufreq_avrg[s];

				/* Avg IMC frequency */
				metrics->avg_imc_f = imcfreq_avrg[s];

				/* Time per iteration */
				metrics->time = time_s / (double) iterations;

				/* Cache misses */
				metrics->L1_misses = cache_diff[s].l1d_misses;
				metrics->L2_misses = cache_diff[s].l2_misses;
				metrics->L3_misses = cache_diff[s].l3_misses;
 
                if (metrics->sig_ext == NULL) {
                    metrics->sig_ext = (void *)calloc(1,sizeof(sig_ext_t));
                }
				sig_ext = metrics->sig_ext;

				/* FLOPS */
                flops_data_accum(&flops_diff[APP], NULL, &metrics->Gflops);
                //verbose(0, "GFS[%d]: %0.2lf", s, metrics->Gflops);
                flops_help_toold(&flops_diff[APP], metrics->FLOPS);

                /* CPI */
                metrics->cycles       = cpi_diff[s].cycles;
                metrics->instructions = cpi_diff[s].instructions;
                metrics->CPI          = cpi_avrg[s];

                if (metrics->CPI > 1) {
                    char buff[128];
                    cpi_data_tostr(&cpi_read1[s], cpi_avrg[s], buff, sizeof(buff));
                    verb(INFO_METRICS, "Start %s", buff);
                    cpi_data_tostr(&cpi_read2[s], cpi_avrg[s], buff, sizeof(buff));
                    verb(INFO_METRICS, "Stop %s", buff);
                    cpi_data_tostr(&cpi_diff[s], cpi_avrg[s], buff, sizeof(buff));
                    verb(INFO_METRICS, "Diff %s", buff);
                }

				/* TPI and Memory bandwith */
                //if (master) {
                if (masters_info.my_master_rank >= 0) {
                    bwidth_data_accum(bwidth_diff[APP], NULL, &bwidth_cas[s], &metrics->GBS);
                    //verbose(0, "GBS[%d]: %0.2lf", s, metrics->GBS);
                    lib_shared_region->cas_counters = (double) bwidth_cas[s];
                } else {
                    bwidth_cas[s] = (ullong) lib_shared_region->cas_counters;
                    metrics->GBS = bwidth_help_castogbs(bwidth_cas[s], time_s);
                }
                metrics->TPI = bwidth_help_castotpi(bwidth_cas[s], metrics->instructions);

				/* Per process IO data */
                io_copy(&(sig_ext->iod),&metrics_io_diff[s]);
				double iogb;
				int ios;
				ullong mpit, exect;
				/* Per Node IO data */
				ios = s;
				if ((masters_info.my_master_rank>=0) && (s == APP)) 								ios = APP_NODE;
				else if ((masters_info.my_master_rank>=0)  && (s == LOO))          	ios = LOO_NODE;
				iogb = (double)metrics_io_diff[ios].rchar/(double)(1024*1024) + (double)metrics_io_diff[ios].wchar/(double)(1024*1024);
				/* Each process will compute its own IO_MBS, except the master, that computes per node IO_MBS */
				metrics->IO_MBS = iogb/time_s;

				if (masters_info.my_master_rank>=0){

								/* MPI statistics */
								if (is_mpi_enabled()){
												double pmpi = 0.0, lpmpi;
												for (int i=0;i<lib_shared_region->num_processes;i++){	
																mpit = ((mpi_information_t *)metrics_mpi_calls[s])[i].mpi_time;
																exect = ((mpi_information_t *)metrics_mpi_calls[s])[i].exec_time;
																if ((metrics_mpi_calls[s])[i].mpi_time > 0) lpmpi = (double)(mpit*100.0 / exect);
																else lpmpi = 0;
																pmpi += lpmpi;
																verbose_master(3,"PerMPI Proc %d %.2lf mpit = %llu exect = %llu",i,lpmpi,mpit,exect);
												}
												metrics->perc_MPI = pmpi/lib_shared_region->num_processes;		
								}else{
												metrics->perc_MPI = 0.0;
								}
								sig_ext->mpi_stats = metrics_mpi_calls[s];
								sig_ext->mpi_types = metrics_mpi_calls_types[s];


								/* Power: Node, DRAM, PCK */
								metrics->DC_power = (double) acum_ipmi[s] / (time_s * node_energy_units);
								
								/* DRAM and PCK PENDING for Njobs */
								metrics->PCK_power=0;
								metrics->DRAM_power=0;
								int p;
								for (p=0;p<num_packs;p++) metrics->DRAM_power=metrics->DRAM_power+(double) metrics_rapl[s][p];
								for (p=0;p<num_packs;p++) metrics->PCK_power=metrics->PCK_power+(double) metrics_rapl[s][num_packs+p];
								metrics->PCK_power   = (metrics->PCK_power / 1000000000.0) / time_s;
								metrics->DRAM_power  = (metrics->DRAM_power / 1000000000.0) / time_s;
								metrics->EDP = time_s * time_s * metrics->DC_power;

								/* GPUS */
#if USE_GPUS
								if (gpu_initialized){
												metrics->gpu_sig.num_gpus = ear_num_gpus_in_node;
												if (earl_gpu_model == MODEL_DUMMY){ 
																debug("Setting num_gpu to 0 because model is DUMMY");
																metrics->gpu_sig.num_gpus = 0;
												}
												for (p=0;p<metrics->gpu_sig.num_gpus;p++){
																metrics->gpu_sig.gpu_data[p].GPU_power    = gpu_metrics_diff[s][p].power_w;
																metrics->gpu_sig.gpu_data[p].GPU_freq     = gpu_metrics_diff[s][p].freq_gpu;
																metrics->gpu_sig.gpu_data[p].GPU_mem_freq = gpu_metrics_diff[s][p].freq_mem;
																metrics->gpu_sig.gpu_data[p].GPU_util     = gpu_metrics_diff[s][p].util_gpu;
																metrics->gpu_sig.gpu_data[p].GPU_mem_util = gpu_metrics_diff[s][p].util_mem;
												}	
								}
#endif
				}else{
								copy_node_data(metrics,&lib_shared_region->node_signature);
				}

				/* Copying my info in the shared signature */
				from_sig_to_mini(&sig_shared_region[my_node_id].sig,metrics);
				/* If I'm the master, I have to copy in the special section */
				if (masters_info.my_master_rank >= 0){
								signature_copy(&lib_shared_region->node_signature,metrics);
								signature_copy(&lib_shared_region->job_signature,metrics);
				}
				sig_shared_region[my_node_id].period = time_s;
				if (verb_level >= 2) signature_print_simple_fd(2, metrics);
				verbose(2," ");

				debug("Signature ready node %d at time %lld",my_node_id,(metrics_time() - app_start)/1000000);
				update_earl_node_mgr_info();

				signature_ready(&sig_shared_region[my_node_id],0);
				verbose(2,"End Compute signature process %d. Period %lf secs", my_node_id, sig_shared_region[my_node_id].period);
				if (msync(&sig_shared_region[my_node_id], sizeof(shsignature_t),MS_SYNC) < 0) verbose_master(2,"Memory sync fails %s",strerror(errno));

}

/**************************** Init function used in ear_init ******************/
int metrics_init(topology_t *topo)
{
	ulong rapl_size;
	char *cimc_max_khz,*cimc_min_khz;
	char imc_buffer[1024];
	pstate_t tmp_def_imc_pstate;
	state_t st;
	state_t s;
	int i;

    metrics_static_init(topo);

	//debug("Masters region %p size %lu",&masters_info,sizeof(masters_info));
	//debug("My master rank %d",masters_info.my_master_rank);

	hw_cache_line_size = topo->cache_line_size;
	num_packs = topo->socket_count;
	topology_copy(&mtopo,topo);

	//debug("detected cache line size: %0.2lf bytes", hw_cache_line_size);
	//debug("detected sockets: %d", num_packs);

	if (hw_cache_line_size == 0) {
		return_msg(EAR_ERROR, "error detecting the cache line size");
	}
	if (num_packs == 0) {
		return_msg(EAR_ERROR, "error detecting number of packges");
	}

	st=energy_lib_init(system_conf);
	if (st!=EAR_SUCCESS){
		return_msg(EAR_ERROR, "error loading energy plugin");
	}
	if (io_init(&ctx_io,getpid()) != EAR_SUCCESS){
		verbose_master(2,"Warning: IO data not available");
	}else{
		verbose_master(INFO_METRICS,"IO data initialized");
	}	

	// Daemon metrics allocation
	if (masters_info.my_master_rank>=0){
		rapl_size = eards_get_data_size_rapl();
	}else{
		rapl_size=sizeof(unsigned long long);
	}

	verbose_master(2,"RAPL size %lu",rapl_size);
	rapl_elements = rapl_size / sizeof(unsigned long long);

	// Allocating data for energy node metrics
	// node_energy_datasize=eards_node_energy_data_size();
	energy_lib_datasize(&node_energy_datasize);
	energy_lib_units(&node_energy_units);
	/* We should create a data_alloc for enerrgy and a set_null */
	aux_energy=(edata_t)malloc(node_energy_datasize);
	aux_energy_stop=(edata_t)malloc(node_energy_datasize);
	metrics_ipmi[0]=(edata_t)malloc(node_energy_datasize);
	metrics_ipmi[1]=(edata_t)malloc(node_energy_datasize);
	memset(metrics_ipmi[0],0,node_energy_datasize);
	memset(metrics_ipmi[1],0,node_energy_datasize);
	acum_ipmi[0]=0;acum_ipmi[1]=0;

	metrics_rapl[LOO] = malloc(rapl_size);
	metrics_rapl[APP] = malloc(rapl_size);
	aux_rapl = malloc(rapl_size);
	last_rapl = malloc(rapl_size);

	memset(metrics_rapl[LOO], 0, rapl_size);
	memset(metrics_rapl[APP], 0, rapl_size);
	memset(aux_rapl, 0, rapl_size);
	memset(last_rapl, 0, rapl_size);
	if (masters_info.my_master_rank>=0){
	/* We will collect MPI statistics */
	metrics_mpi_calls[APP] = calloc(lib_shared_region->num_processes,sizeof(mpi_information_t));
  metrics_last_mpi_calls[APP] = calloc(lib_shared_region->num_processes,sizeof(mpi_information_t));
	metrics_mpi_calls[LOO] = calloc(lib_shared_region->num_processes,sizeof(mpi_information_t));
  metrics_last_mpi_calls[LOO] = calloc(lib_shared_region->num_processes,sizeof(mpi_information_t));
	metrics_mpi_calls_types[APP] = calloc(lib_shared_region->num_processes,sizeof(mpi_calls_types_t));
  metrics_last_mpi_calls_types[APP] = calloc(lib_shared_region->num_processes,sizeof(mpi_calls_types_t));
	metrics_mpi_calls_types[LOO] = calloc(lib_shared_region->num_processes,sizeof(mpi_calls_types_t));
  metrics_last_mpi_calls_types[LOO] = calloc(lib_shared_region->num_processes,sizeof(mpi_calls_types_t));

	/* IMC average frequency initialization */
    if (mgt_imcfreq.ok){
	/* IMC management */
	char imc_mgt_txt[32];
	apis_tostr(mgt_imcfreq.api,imc_mgt_txt,sizeof(imc_mgt_txt));
	verbose_master(INFO_METRICS,"IMC API selected %s",imc_mgt_txt);
    imc_devices = mgt_imcfreq.devs_count;
	imc_max_pstate = calloc(mgt_imcfreq.devs_count,sizeof(uint));
	imc_min_pstate = calloc(mgt_imcfreq.devs_count,sizeof(uint));
	if ((imc_max_pstate == NULL) || (imc_min_pstate == NULL)){
		error("Asking for memory");
		return EAR_ERROR;
	}
	verbose_master(INFO_METRICS," IMC num devices %u",mgt_imcfreq.devs_count);
	if (state_fail(s = mgt_imcfreq_get_available_list(NULL,(const pstate_t **)&imc_pstates, &imc_num_pstates))){
		error("Asking for IMC frequency list");
	}
	mgt_imcfreq_data_tostr(imc_pstates,imc_num_pstates,imc_buffer,sizeof(imc_buffer));
	verbose_master(2,"IMC pstates lust:%s",imc_buffer);
	/* Default values */
	def_imc_max_khz = imc_pstates[0].khz;
	def_imc_min_khz = imc_pstates[imc_num_pstates-1].khz;
	for (i=0; i< mgt_imcfreq.devs_count;i++) imc_min_pstate[i] = 0;
	for (i=0; i< mgt_imcfreq.devs_count;i++) imc_max_pstate[i]= imc_num_pstates-1;
	/* This section checks for HACKS */
  cimc_max_khz = getenv(SCHED_MAX_IMC_FREQ);
  cimc_min_khz = getenv(SCHED_MIN_IMC_FREQ);
	imc_max_khz = def_imc_max_khz;
	imc_min_khz = def_imc_min_khz;
  if (cimc_max_khz != (char*)NULL){
		ulong tmp_def_imc = atoi(cimc_max_khz);
		if (state_ok(pstate_freqtops(imc_pstates,imc_num_pstates,tmp_def_imc,&tmp_def_imc_pstate))){
			for (i=0; i< mgt_imcfreq.devs_count;i++) imc_min_pstate[i] = tmp_def_imc_pstate.idx;
			imc_max_khz = imc_pstates[imc_min_pstate[0]].khz;
		}
	}
  if (cimc_min_khz != (char*)NULL){
		ulong tmp_def_imc = atoi(cimc_min_khz);
		if (state_ok(pstate_freqtops(imc_pstates,imc_num_pstates,tmp_def_imc,&tmp_def_imc_pstate))){
			for (i=0; i< mgt_imcfreq.devs_count;i++) imc_max_pstate[i] = tmp_def_imc_pstate.idx;
			imc_min_khz = imc_pstates[imc_max_pstate[0]].khz;
		}
  }
	/* End HACKS */
	if (state_fail(s = mgt_imcfreq_set_current_ranged_list(NULL,imc_min_pstate,imc_max_pstate))){
		error("Initialzing the IMC freq");
	}
	/* We assume all the sockets have the same configuration */
	imc_curr_max_pstate = imc_max_pstate[0];
	imc_curr_min_pstate = imc_min_pstate[0];	
	verbose_master(INFO_METRICS,"New CPUfreq and IMC initialized IMC freq set to %.2f-%.2f",(float)imc_pstates[imc_min_pstate[0]].khz/1000000.0,(float)imc_pstates[imc_max_pstate[0]].khz/1000000.0);
		
	/* End of IMC */	
	}
	}

	#if USE_GPUS
	if (masters_info.my_master_rank>=0){
	if (gpu_lib_load(system_conf) !=EAR_SUCCESS){
      gpu_initialized=0;
			debug("Error in gpu_lib_load");
	}else gpu_initialized=1;
	if (gpu_initialized){
		debug("Initializing GPU");
		if (gpu_lib_init(&gpu_lib_ctx) != EAR_SUCCESS){
				error("Error in GPU initiaization");
				gpu_initialized=0;
		}else{
			debug("GPU initialization successfully");
			gpu_lib_model(&gpu_lib_ctx,&earl_gpu_model);
			debug("GPU model %u",earl_gpu_model);
			gpu_lib_data_alloc(&gpu_metrics_init[LOO]);gpu_lib_data_alloc(&gpu_metrics_init[APP]);
			gpu_lib_data_alloc(&gpu_metrics_end[LOO]);gpu_lib_data_alloc(&gpu_metrics_end[APP]);
			gpu_lib_data_alloc(&gpu_metrics_diff[LOO]);gpu_lib_data_alloc(&gpu_metrics_diff[APP]);
			gpu_lib_count(&ear_num_gpus_in_node);
			debug("GPU library data initialized");
		}
	}
	debug("GPU initialization successfully");
	}
	fflush(stderr);	
	#endif

	//debug( "detected %d RAPL counters for %d packages: %d events por package", rapl_elements,num_packs,rapl_elements/num_packs);

	metrics_global_start();
	metrics_partial_start();

	return EAR_SUCCESS;
}

void metrics_dispose(signature_t *metrics, ulong procs)
{
	sig_ext_t *se;
	signature_t nodeapp;

	if (!eards_connected()) return;
    
    metrics_static_dispose();

	verbose(2,"Metrics_dispose_init %d. End local signature",my_node_id);
  while (metrics_partial_stop(SIG_BEGIN) == EAR_NOT_READY) usleep(10);

	metrics_compute_signature_data(LOO, metrics, 1, procs);
    /* Waiting for node signatures */
    verbose_master(2,"Waiting for node signatures");
    while ( are_signatures_ready(lib_shared_region,sig_shared_region) == 0) usleep(100);

	compute_per_process_and_job_metrics(&nodeapp);

	/* At this point we mark signatures are not ready to be computed again */
	if (masters_info.my_master_rank >= 0)  free_node_signatures(lib_shared_region,sig_shared_region);

	verbose(2,"Global stop ID=%d",my_node_id);	
	metrics_global_stop();
	metrics_compute_signature_data(APP, metrics, 1, procs);
    verbose_master(2,"Waiting for node signatures");
    
    /* Waiting for node signatures */
    while ( are_signatures_ready(lib_shared_region,sig_shared_region) == 0 ) usleep(100);

	/* Estimate local metrics, accumulate */
	if (masters_info.my_master_rank >= 0){
  	verbose_master(2,"Computing application node signature");
  	se = (sig_ext_t *)metrics->sig_ext;
  	if (!exclusive) metrics_app_node_signature(metrics, &nodeapp);
		else           metrics_node_signature(&lib_shared_region->job_signature, &nodeapp);
  	signature_copy(metrics, &nodeapp);
  	metrics->sig_ext = se;
	}
	debug("Metrics_dispose_end %d",my_node_id);
}

void metrics_compute_signature_begin()
{
    metrics_partial_stop(SIG_BEGIN);
    metrics_partial_start();
}

void metrics_get_total_io(io_data_t *total)
{
	int i;
	io_data_t io;
	ctx_t lio;
	if (total == NULL) return;
	memset((char *)total,0,sizeof(io_data_t));
	for (i=0; i<lib_shared_region->num_processes;i++){
		  if (io_init(&lio,sig_shared_region[i].pid) != EAR_SUCCESS){
				verbose_master(2,"IO data for process %d-%d can not be read",i,sig_shared_region[i].pid);
			}else{
				io_read(&lio,&io);
				total->rchar += io.rchar;
				total->wchar += io.wchar;
				total->syscr += io.syscr;
				total->syscw += io.syscw;
				total->read_bytes += io.read_bytes;
				total->write_bytes += io.write_bytes;
				total->cancelled += io.cancelled;
				io_dispose(&lio);
			}
	}
}

int time_ready_signature(ulong min_time_us)
{
				long long f_time;
				f_time = metrics_usecs_diff(metrics_time(), metrics_usecs[LOO]);
				if (f_time<min_time_us) return 0;
				else return 1;
}

/****************************************************************************************************************************************************************/
/******************* This function checks if data is ready to be shared **************************************/
/****************************************************************************************************************************************************************/


int metrics_compute_signature_finish(signature_t *metrics, uint iterations, ulong min_time_us, ulong procs)
{
				long long aux_time;

                if (!sig_shared_region[my_node_id].ready){

								// Time requirements
								aux_time = metrics_usecs_diff(metrics_time(), metrics_usecs[LOO]);

								//
								if ((aux_time<min_time_us) && !equal_with_th((double)aux_time, (double)min_time_us,0.1)) {
#if 0
												verbose(1,"EAR_NOT_READY because of time %llu\n",aux_time);
#endif
												metrics->time=0;

												return EAR_NOT_READY;
								}

								//Master: Returns not ready when power is not ready
								//Not master: when master signature not ready
								if (metrics_partial_stop(SIG_END)==EAR_NOT_READY) return EAR_NOT_READY;

								//Marks the signature as ready
								metrics_compute_signature_data(LOO, metrics, iterations, procs);

								//
								metrics_partial_start();
				}
				if (!are_signatures_ready(lib_shared_region,sig_shared_region)){
								if ((masters_info.my_master_rank >= 0) && (sig_shared_region[0].ready)){
												lib_shared_region->master_ready = 1;
								}
								return EAR_NOT_READY;
				}else if (masters_info.my_master_rank >= 0){
								lib_shared_region->master_ready = 0;
				}
				return EAR_SUCCESS;
}

long long metrics_usecs_diff(long long end, long long init)
{
				long long to_max;

				if (end < init)
				{
								debug( "Timer overflow (end: %lld - init: %lld)\n", end, init);
								to_max = LLONG_MAX - init;
								return (to_max + end);
				}

				return (end - init);
}

void metrics_app_node_signature(signature_t *master,signature_t *ns)
{
  ullong inst;
  ullong cycles;
  ullong FLOPS[FLOPS_EVENTS];
  sig_ext_t *se;
  int i;
	double valid_period;

  se = (sig_ext_t *)master->sig_ext;
  signature_copy(ns,master);
  if (se != NULL) memcpy(ns->sig_ext,se,sizeof(sig_ext_t));

	verbose_master(2, " metrics_app_node_signature");

  compute_total_node_instructions(lib_shared_region, sig_shared_region, &inst);
  compute_total_node_flops(lib_shared_region, sig_shared_region, FLOPS);
  compute_total_node_cycles(lib_shared_region, sig_shared_region, &cycles);
  compute_total_node_L3_misses(lib_shared_region, sig_shared_region, &ns->L3_misses);

  ns->CPI = (double)cycles/(double)inst;
  /* verbose_master(2,"CPI %lf CPI2 %lf",ns->CPI, CPI/(double)lib_shared_region->num_processes); */
  if (ns->CPI > 2.0){
    for (i=0; i< lib_shared_region->num_processes;i ++){
      verbosen_master(2," CPI[%d]=%lf",i,sig_shared_region[i].sig.CPI);
    } 
    verbose_master(2," ");
  }
  ns->Gflops = compute_node_flops(lib_shared_region, sig_shared_region);
	uint tcpus     = 0;
	ns->DC_power   = 0;
	ns->GBS        = 0;
	ns->avg_f      = 0;
	ns->DRAM_power = 0;
	ns->PCK_power  = 0;
  if (masters_info.my_master_rank >= 0){
		for (uint lp = 0; lp < lib_shared_region->num_processes; lp ++){
			valid_period    = sig_shared_region[lp].sig.valid_time;
			ns->DC_power   += sig_shared_region[lp].sig.accum_energy / valid_period;
			ns->DRAM_power += sig_shared_region[lp].sig.accum_dram_energy / valid_period;
			ns->PCK_power  += sig_shared_region[lp].sig.accum_pack_energy / valid_period;
			ns->GBS        += sig_shared_region[lp].sig.accum_mem_access / (valid_period * B_TO_GB);
			ns->avg_f      += (sig_shared_region[lp].sig.accum_avg_f * sig_shared_region[lp].num_cpus) / (ulong)valid_period;
			tcpus          += sig_shared_region[lp].num_cpus;
			verbose_master(2," p[%u]: Power %lf GBs %lf avg_f %lu dram %lf pck %lf  period %lf", lp, (double)(sig_shared_region[lp].sig.accum_energy / valid_period), (double)(sig_shared_region[lp].sig.accum_mem_access/ (valid_period * 1024 * 1024 * 1024)), sig_shared_region[lp].sig.accum_avg_f/(ulong)valid_period, ns->DRAM_power, ns->PCK_power, valid_period);
		}
		ns->avg_f /= tcpus;
		verbose_master(2,"AVG  %lu", ns->avg_f);
  }
  signature_copy(&lib_shared_region->node_signature,ns);

}

void metrics_node_signature(signature_t *master,signature_t *ns)
{
	ullong inst;
	ullong cycles;
	ullong FLOPS[FLOPS_EVENTS];
	sig_ext_t *se;
	int i;

	se = (sig_ext_t *)master->sig_ext;	
	signature_copy(ns,master);
	if (se != NULL) memcpy(ns->sig_ext,se,sizeof(sig_ext_t));
	
	compute_total_node_instructions(lib_shared_region, sig_shared_region, &inst);
	compute_total_node_flops(lib_shared_region, sig_shared_region, FLOPS);
	compute_total_node_cycles(lib_shared_region, sig_shared_region, &cycles);	
	compute_total_node_L3_misses(lib_shared_region, sig_shared_region, &ns->L3_misses);

	ns->CPI = (double)cycles/(double)inst;
	/* verbose_master(2,"CPI %lf CPI2 %lf",ns->CPI, CPI/(double)lib_shared_region->num_processes); */
	if (ns->CPI > 2.0){
		for (i=0; i< lib_shared_region->num_processes;i ++){
			verbosen_master(2," CPI[%d]=%lf",i,sig_shared_region[i].sig.CPI);
		}	
		verbose_master(2," ");
	}
	ns->Gflops = compute_node_flops(lib_shared_region, sig_shared_region);
	#if 0
	signature_copy(&lib_shared_region->node_signature,ns);

	if (masters_info.my_master_rank >= 0){
		/* Updating shared info */
  	update_earl_node_mgr_info();
  	estimate_power_and_gbs(master,lib_shared_region,sig_shared_region, node_mgr_job_info, ns, PER_JOB, -1);
	}
	#endif
}

extern uint last_earl_phase_classification;
extern uint last_earl_gpu_phase_classification;

state_t metrics_new_iteration(signature_t *sig)
{
    llong f_time, elap_sec;

#if USE_GPUS
    gpu_t *gpud, *gpuddiff;
    int p;
#endif

    if (masters_info.my_master_rank < 0) {
        return EAR_ERROR;
    }
    /* FLOPS are used to determine the CPU BUSY waiting */
    if ((last_earl_phase_classification == APP_IO_BOUND) || (last_earl_phase_classification == APP_BUSY_WAITING))
    {
        f_time = metrics_usecs_diff(metrics_time(), metrics_usecs[LOO]);
        elap_sec = f_time / 1000000;
        debug("elap %lld last %lld: Computing flops because of application phase is IO =%d or Busy waiting=%d",elap_sec,last_elap_sec, last_earl_phase_classification == APP_IO_BOUND, last_earl_phase_classification == APP_BUSY_WAITING);

        if (elap_sec <= last_elap_sec) {
            return EAR_ERROR;
        }
        last_elap_sec = elap_sec;

        // FLOPs
        flops_read_diff(no_ctx, &flops_read2[LOO], &flops_read1[LOO], &flops_diff[LOO], &sig->Gflops);
        flops_help_toold(&flops_diff[LOO], sig->FLOPS);

        sig->time = elap_sec;
        debug("Flops in metrics for validation %lf time %lld",sig->Gflops, elap_sec);
    }

#if USE_GPUS
    if ((last_earl_gpu_phase_classification == GPU_IDLE) && (ear_num_gpus_in_node > 0)){
        debug("Computing GPU util because of application gpu phase is IDLE = %d",last_earl_gpu_phase_classification == GPU_IDLE);
        /* GPUs */
        if (gpu_initialized){
            gpu_lib_data_alloc(&gpud);
            gpu_lib_data_alloc(&gpuddiff);
            if (gpu_lib_read(&gpu_lib_ctx,gpud) != EAR_SUCCESS){
                debug("gpu_lib_read error");
            }
            gpu_lib_data_diff(gpud, gpu_metrics_init[LOO], gpuddiff);
            sig->gpu_sig.num_gpus = ear_num_gpus_in_node;
            if (earl_gpu_model == MODEL_DUMMY){
                debug("Setting num_gpu to 0 because model is DUMMY");
                sig->gpu_sig.num_gpus = 0;
            }
            for (p=0; p < sig->gpu_sig.num_gpus;p++){
                sig->gpu_sig.gpu_data[p].GPU_power    = gpuddiff[p].power_w;
                sig->gpu_sig.gpu_data[p].GPU_freq     = gpuddiff[p].freq_gpu;
                sig->gpu_sig.gpu_data[p].GPU_mem_freq = gpuddiff[p].freq_mem;
                sig->gpu_sig.gpu_data[p].GPU_util     = gpuddiff[p].util_gpu;
                sig->gpu_sig.gpu_data[p].GPU_mem_util = gpuddiff[p].util_mem;
            }
            gpu_lib_data_free(&gpud);
            gpu_lib_data_free(&gpuddiff);
        }
    }
#endif
				return EAR_SUCCESS;
}

/* This function computes local metrics per process, computes per job and computes the accumulated */
void compute_per_process_and_job_metrics(signature_t *dest)
{
	if (masters_info.my_master_rank >= 0){
		update_earl_node_mgr_info();
		estimate_power_and_gbs(lib_shared_region, sig_shared_region, node_mgr_job_info)	;
		accum_estimations(lib_shared_region, sig_shared_region);
		signature_copy(dest, &lib_shared_region->job_signature);	
	}
		
}

