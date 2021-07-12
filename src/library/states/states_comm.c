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

//#define SHOW_DEBUGS 1
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

void state_verbose_signature(loop_t *sig,int master_rank,int show,char *aname,char *nname,int iterations,ulong prevf,ulong new_freq,char *mode)
{
    char io_info[256];
    double CPI, TPI, GBS, POWER, TIME, ENERGY, EDP, VPI, IN_MPI_PERC,IN_MPI_SEC,GPU_POWER = 0, FLOPS;
    float AVGF = 0,AVGGPUF = 0,PFREQ = 0,NFREQ = 0,VI;
    float l1_sec=0,l2_sec=0,l3_sec=0;
    ulong GPU_UTIL = 0,GPU_FREQ = 0,gpuused=0;
    ulong elapsed;
    timestamp end;
    timestamp_getfast(&end);
    elapsed=timestamp_diff(&end,&ear_application_time_init,TIME_SECS);

    float imcf;
    int gpui;
    char gpu_buff[256];
    if (master_rank == 0 || show || using_verb_files){
        if (sig->signature.sig_ext != NULL) imcf = (float)sig->signature.sig_ext->imc_freq/1000000.0;
        else imcf = 0.0;
        CPI = sig->signature.CPI; 
        GBS = sig->signature.GBS; 
        POWER = sig->signature.DC_power; 
        TPI = sig->signature.TPI; 
        TIME = sig->signature.time; 
        FLOPS = sig->signature.Gflops;
        AVGF = (float)sig->signature.avg_f/1000000.0; 
        VI=metrics_vec_inst(&sig->signature); 
        VPI=(double)VI/(double)sig->signature.instructions; 
        ENERGY = TIME * POWER; 
        EDP = ENERGY * TIME;
        GPU_POWER=0;GPU_FREQ=0;GPU_UTIL=0; 
        if (prevf > 0 ){ PFREQ = (float)prevf/1000000.0;}
        if (new_freq > 0 ){ NFREQ = (float)new_freq/1000000.0;}
        io_tostr(&sig->signature.sig_ext->iod,io_info,sizeof(io_info));
        strcpy(gpu_buff,"");
#if USE_GPUS
        if (sig->signature.gpu_sig.num_gpus>0){ 
            verbose(3,"");
            for(gpui=0;gpui<sig->signature.gpu_sig.num_gpus;gpui++){ 
                verbosen(3,"(GPU[%d] Power %.2f Util %lu Freq %lu)",gpui,sig->signature.gpu_sig.gpu_data[gpui].GPU_power,sig->signature.gpu_sig.gpu_data[gpui].GPU_util,
                        sig->signature.gpu_sig.gpu_data[gpui].GPU_freq);
                if (sig->signature.gpu_sig.gpu_data[gpui].GPU_util){
                    gpuused++;
                    GPU_POWER += sig->signature.gpu_sig.gpu_data[gpui].GPU_power; 
                    GPU_FREQ += sig->signature.gpu_sig.gpu_data[gpui].GPU_freq; 
                    GPU_UTIL += sig->signature.gpu_sig.gpu_data[gpui].GPU_util; 
                }
            } 
            verbose(3,"");
            if (gpuused){
                GPU_POWER = GPU_POWER/(float)gpuused;
                AVGGPUF = (float)GPU_FREQ/(float)(gpuused*1000000.0); 
                GPU_UTIL = GPU_UTIL/gpuused; 
            }
        } 
#endif
        verbose(2,"EAR+%s(%s) at %.2f in %s: LoopID=%lu, LoopSize=%lu-%lu,iterations=%d",mode,aname, PFREQ,nname,sig->id.event,sig->id.size,sig->id.level,iterations); 
        verbosen(1,"EAR+%s(%s) at %.2f in %s MR[%d] elapsed %lu sec.: ",mode,aname, PFREQ,nname,master_rank,elapsed); 
        verbosen(1,"%s",COL_GRE);
        if (GPU_UTIL) sprintf(gpu_buff,"(Used GPUS %lu GPU_power %.2lfW GPU_freq %.2fGHz GPU_util %lu)",gpuused,GPU_POWER,AVGGPUF,GPU_UTIL);
        verbosen(1,"(CPI=%.3lf GBS=%.2lf Power=%.2lfW Time=%.3lfsec.  AVGF=%.2fGHz GFlops=%.2lf)\n\t %s", CPI, GBS, POWER, TIME,  AVGF, FLOPS, gpu_buff);
        if (new_freq > 0){ verbosen(1," Next Frequency %.2f",NFREQ);}
        verbosen(2,"\n\t IMC avg freq %.2f",imcf);
        verbosen(2," DRAM power %.2lf PCK power %.2lf",sig->signature.DRAM_power,sig->signature.PCK_power);
        l1_sec = sig->signature.L1_misses/(sig->signature.time*1024*1024*1024);
        l2_sec = sig->signature.L2_misses/(sig->signature.time*1024*1024*1024);
        l3_sec = sig->signature.L3_misses/(sig->signature.time*1024*1024*1024);
        verbosen(2,"\n\t L1GB/sec %.2f L2GB/sec %.2f L3GB/sec %.2f",l1_sec,l2_sec,l3_sec);
        verbosen(2," VPI %.2f Total MPI calls %u",VPI,total_mpi);
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


static void debug_loop_signature(char *title, signature_t *loop)
{
    float avg_f = (float) loop->avg_f / 1000000.0;

    debug("(%s) Avg. freq: %.2lf (GHz), CPI/TPI: %0.3lf/%0.3lf, GBs: %0.3lf, DC power: %0.3lf, time: %0.3lf, GFLOPS: %0.3lf",
            title, avg_f, loop->CPI, loop->TPI, loop->GBS, loop->DC_power, loop->time, loop->Gflops);
}

