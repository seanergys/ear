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

#if MPI
#include <mpi.h>
#endif
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

// #define SHOW_DEBUGS 1
#include <metrics/common/pstate.h>
#include <common/config.h>
#include <common/states.h>
#include <common/output/verbose.h>
#include <common/colors.h>
#include <common/math_operations.h>
#include <common/types/log.h>
#include <common/types/application.h>
#include <library/common/externs.h>
#include <library/common/global_comm.h>
#include <library/metrics/metrics.h>
#include <library/tracer/tracer.h>

extern masters_info_t masters_info;
extern float ratio_PPN;
extern uint total_mpi;
extern uint using_verb_files;

#ifdef SHOW_DEBUGS
static void debug_loop_signature(char *title, signature_t *loop);
#endif

void state_verbose_signature(loop_t *sig, int master_rank, int show, char *aname, char *nname,
        int iterations, ulong prevf, ulong new_freq, char *mode)
{
    char io_info[256];
    char pmpi_txt[256];
    double CPI, GBS, POWER, TIME, VPI, GPU_POWER = 0, FLOPS, gpu_tpower =0;
    float AVGF = 0, AVGGPUF = 0, PFREQ = 0, NFREQ = 0, VI, gpuf =0 ;
    float l1_sec=0,l2_sec=0,l3_sec=0;
    float IO_MBS;
    ulong GPU_UTIL = 0,GPU_FREQ = 0,gpuused=0;
    ulong elapsed;
    timestamp end;
    sig_ext_t *se = (sig_ext_t *)sig->signature.sig_ext;
    timestamp_getfast(&end);
    elapsed=timestamp_diff(&end,&ear_application_time_init,TIME_SECS);

    float imcf;
    int gpui;
    char gpu_buff[256];

    if (master_rank == 0 || show || using_verb_files) {
        debug("%swriting pmpi_txt%s", COL_YLW, COL_CLR);

#if 0
        double perc_mpi = (using_verb_files) ? ((double) (sig_shared_region[my_node_id].mpi_info.mpi_time * 100) / (double) sig_shared_region[my_node_id].mpi_info.exec_time) : sig->signature.perc_MPI;
#endif
				sig_ext_t *sigex = sig->signature.sig_ext; 
        float perc_mpi = (using_verb_files) ? (sig_shared_region[my_node_id].mpi_info.perc_mpi) : sig->signature.perc_MPI;
        float max_mpi = (master_rank >= 0) ? (sigex->max_mpi) : 0.0;
        float min_mpi = (master_rank >= 0) ? (sigex->min_mpi) : 0.0;

        if (perc_mpi > 0.0) {

            char pmpi_msg[64];
            if (using_verb_files) {
                snprintf(pmpi_msg, sizeof(pmpi_msg), "MPI_time/exec_Time (max/avg/min)");
            } else {
                // Since when only master verboses, it prints the average across all processes of the job
                snprintf(pmpi_msg, sizeof(pmpi_msg), "MPI_time/exec_Time (max/avg/min)");
            }

            snprintf(pmpi_txt, sizeof(pmpi_txt), "%s: %.1f/%.1f/%.1f %%", pmpi_msg, max_mpi, perc_mpi, min_mpi);

        } else {
            sprintf(pmpi_txt,"No MPI");
        }

        // IMC
        debug("imc");
        imcf = (float)sig->signature.avg_imc_f/1000000.0;

        // CPI
        debug("cpi");
        CPI = (using_verb_files) ? (double) sig_shared_region[my_node_id].sig.CPI : sig->signature.CPI; 

        // GB/s
        debug("gbs");
        GBS = sig->signature.GBS;

        // I/O MB/s
        io_data_t *io_data = (using_verb_files) ? &sig_shared_region[my_node_id].sig.iod : &se->iod;

        // TODO: period or time ?
        IO_MBS = (using_verb_files) ? ((float) io_data->rchar / (float) (1024 * 1024) + (float) io_data->wchar / (float) (1024 * 1024)) / sig_shared_region[my_node_id].period : (float) sig->signature.IO_MBS;

        // DC Node Power
        POWER = sig->signature.DC_power; 

        // Time
        TIME = (using_verb_files) ? sig_shared_region[my_node_id].sig.time : sig->signature.time; 

        // GFlop/s
        FLOPS = (using_verb_files) ? sig_shared_region[my_node_id].sig.Gflops : sig->signature.Gflops;

        // CPU
        debug("avgf");
        if (!using_verb_files) {
            AVGF = (float) sig->signature.avg_f / 1000000.0;
        } else {
            cpu_set_t *mask = &sig_shared_region[my_node_id].cpu_mask;
            ulong freq_avg, cpus_count;
            pstate_freqtoavg(*mask, lib_shared_region->avg_cpufreq, metrics_get(MET_CPUFREQ)->devs_count, &freq_avg, &cpus_count);
            AVGF = (float) freq_avg / 1000000.0;
        }
         

        // AVX 
        if (using_verb_files) {
            // TODO ssig_vpi vs. ssig vpi2 in signature.h
            compute_ssig_vpi(&VPI, &sig_shared_region[my_node_id].sig);
        } else {
            compute_vpi(&VPI, &sig->signature);
        }
#if 0
        VI=metrics_vec_inst(&sig->signature); 
        VPI=(double)VI/(double)sig->signature.instructions; 
#endif

        GPU_POWER=0;GPU_FREQ=0;GPU_UTIL=0; 

        debug("prevf");
        if (prevf > 0 ){ PFREQ = (float)prevf/1000000.0;}

        if (new_freq > 0 ){ NFREQ = (float)new_freq/1000000.0;}

        debug("io");
        
        if (se != NULL) {
            io_tostr(io_data, io_info, sizeof(io_info)); // io_data declared above
        } else {
            sprintf(io_info,"No IO data");
        }
#if 0
        if (se != NULL && master_rank == 0) io_tostr(&se->iod, io_info, sizeof(io_info));
        else sprintf(io_info,"No IO data");
#endif

#if USE_GPUS
        if (master_rank == 0 && sig->signature.gpu_sig.num_gpus > 0) { 
            debug("%swriting gpu info%s", COL_YLW, COL_CLR);
            verbose(3," ");

            for(gpui=0;gpui<sig->signature.gpu_sig.num_gpus;gpui++) { 
                verbosen(3,"(GPU[%d] Power %.2f Util %lu Freq %lu)",
                        gpui, sig->signature.gpu_sig.gpu_data[gpui].GPU_power,
                        sig->signature.gpu_sig.gpu_data[gpui].GPU_util, sig->signature.gpu_sig.gpu_data[gpui].GPU_freq);

                gpu_tpower += sig->signature.gpu_sig.gpu_data[gpui].GPU_power;
                gpuf += (float)sig->signature.gpu_sig.gpu_data[gpui].GPU_freq;

                if (sig->signature.gpu_sig.gpu_data[gpui].GPU_util){
                    gpuused++;
                    GPU_POWER += sig->signature.gpu_sig.gpu_data[gpui].GPU_power; 
                    GPU_FREQ += sig->signature.gpu_sig.gpu_data[gpui].GPU_freq; 
                    GPU_UTIL += sig->signature.gpu_sig.gpu_data[gpui].GPU_util; 
                }
            } 
            verbose(3," ");

            if (gpuused){
                GPU_POWER = GPU_POWER/(float)gpuused;
                AVGGPUF = (float)GPU_FREQ/(float)(gpuused*1000000.0); 
                GPU_UTIL = GPU_UTIL/gpuused; 
            }

            gpuf = gpuf / sig->signature.gpu_sig.num_gpus;
            snprintf(gpu_buff, sizeof(gpu_buff), "\n\t(total gpupower %.0lf avg_gfreq %.2fGHz)", gpu_tpower, gpuf/1000000.0);
        } 
#endif

        debug("%swriting app info%s", COL_YLW, COL_CLR);
        verbose(2,"EAR+%s(%s) at %.2f in %s: LoopID=%lu, LoopSize=%lu-%lu, iterations=%d",
                mode, aname, PFREQ, nname, sig->id.event, sig->id.size, sig->id.level, iterations); 

        debug("%swriting aname at elapsed%s", COL_YLW, COL_CLR)
        verbosen(1,"EAR+%s(%s) at %.2f in %s MR[%d] elapsed %lu sec.: ",
                mode, aname, PFREQ, nname, master_rank, elapsed); 
        verbosen(1,"%s",COL_GRE);

#if USE_GPUS
        if (sig->signature.gpu_sig.num_gpus > 0 ) verbosen(2,"%s", gpu_buff);
#endif

        strcpy(gpu_buff," ");
        if (GPU_UTIL) snprintf(gpu_buff,sizeof(gpu_buff),"\n\t(Used GPUS %lu GPU_power %.2lfW GPU_freq %.2fGHz GPU_util %lu)",gpuused,GPU_POWER,AVGGPUF,GPU_UTIL);

        debug("%swriting CPI GBS power...%s", COL_YLW, COL_CLR);
        verbosen(1,"(CPI=%.3lf GBS=%.2lf Power=%.1lfW Time=%.3lfs\n\tAvgCPUF=%.2fGHz/AvgIMCF=%.2fGHz GFlop/s=%.2lf IO=%.2fMB/s %s) %s", CPI, GBS, POWER, TIME,  AVGF, imcf, FLOPS, IO_MBS, pmpi_txt, gpu_buff);

        if (new_freq > 0){ verbosen(1,"\n\t Next Frequency %.2f",NFREQ);}

        verbosen(2,"\n\t DRAM power %.2lf PCK power %.2lf",sig->signature.DRAM_power,sig->signature.PCK_power);

        ullong l1_miss = (using_verb_files) ? sig_shared_region[my_node_id].sig.L1_misses : sig->signature.L1_misses;
        ullong l2_miss = (using_verb_files) ? sig_shared_region[my_node_id].sig.L2_misses : sig->signature.L2_misses;
        ullong l3_miss = (using_verb_files) ? sig_shared_region[my_node_id].sig.L3_misses : sig->signature.L3_misses;

        l1_sec = l1_miss/(TIME * 1024*1024*1024);
        l2_sec = l2_miss/(TIME * 1024*1024*1024);
        l3_sec = l3_miss/(TIME * 1024*1024*1024);

        verbosen(2,"\n\t L1GB/sec %.2f L2GB/sec %.2f L3GB/sec %.2f",l1_sec,l2_sec,l3_sec);

        uint n_mpi_calls = (using_verb_files) ? sig_shared_region[my_node_id].mpi_info.total_mpi_calls : total_mpi;

        verbosen(2," VPI %.2f Total MPI calls %u",VPI, n_mpi_calls);
        verbosen(2,"\n\t IO DATA: %s ",io_info);

        verbose(1,"%s",COL_CLR);
    }
}

void state_report_traces(int master_rank, int my_rank,int lid, loop_t *lsig,ulong freq,ulong status)
{
    if (master_rank >= 0 && traces_are_on()){
        traces_new_signature(my_rank, lid, &lsig->signature);
        if (freq > 0 ) traces_frequency(my_rank, lid, freq); 
        traces_policy_state(my_rank, lid,status);
    }
}

void state_report_traces_state(int master_rank, int my_rank,int lid,ulong status)
{
    if (master_rank >= 0 && traces_are_on()) traces_policy_state(my_rank, lid,status);
}

/* Auxiliary functions */

void state_print_policy_state(int master_rank,int st)
{
#if 0
    if (master_rank >= 0){
        switch(st){
            case EAR_POLICY_READY:verbose(2,"Policy new state EAR_POLICY_READY");break;
            case EAR_POLICY_CONTINUE:verbose(2,"Policy new state EAR_POLICY_CONTINUE");break;
            case EAR_POLICY_GLOBAL_EV:verbose(2,"Policy new state EAR_POLICY_GLOBAL_EV");break;
            case EAR_POLICY_GLOBAL_READY: verbose(2,"Policy new state EAR_POLICY_GLOBAL_READY"); break;
        }
    }
#endif
}


#ifdef SHOW_DEBUGS
static void debug_loop_signature(char *title, signature_t *loop)
{
    float avg_f = (float) loop->avg_f / 1000000.0;

    debug("(%s) Avg. freq: %.2lf (GHz), CPI/TPI: %0.3lf/%0.3lf, GBs: %0.3lf, DC power: %0.3lf, time: %0.3lf, GFLOPS: %0.3lf",
            title, avg_f, loop->CPI, loop->TPI, loop->GBS, loop->DC_power, loop->time, loop->Gflops);
}
#endif

