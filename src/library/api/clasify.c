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

#include <stdio.h>
#include <stdlib.h>
#include <common/config.h>
#include <common/states.h>
#include <common/output/debug.h>
#include <common/hardware/topology.h>
#include <library/common/verbose_lib.h>
#include <library/common/externs.h>
#include <library/dynais/dynais.h>
#include <library/api/clasify.h>
#include <library/states/states_comm.h>
#include <library/loader/module_cuda.h>

#define INFO_CLASSIFY 3

extern ulong perf_accuracy_min_time;
extern uint signature_reported;

static float CPI_CPU_BOUND =       0.6;
static float GBS_CPU_BOUND =      30.0;
static float CPI_BUSY_WAITING =    0.6;
static float GBS_BUSY_WAITING =    3.0;
static float GFLOPS_BUSY_WAITING = 0.1;
static float IO_TH               = 10.0;
static float MPI_TH              = 10000;
static float MIN_MPI_TH          = 200;
static float CPI_MEM_BOUND       = 1.0;
static float GBS_MEM_BOUND       = 60.0;

void get_classify_limits(ear_classify_t *limits)
{
  limits->cpi_cpu_bound     = CPI_CPU_BOUND;
  limits->gbs_cpu_bound     = GBS_CPU_BOUND;
  limits->cpi_busy_waiting  = CPI_BUSY_WAITING;
  limits->gbs_busy_waiting  = GBS_BUSY_WAITING;
  limits->gflops_busy_waiting = GFLOPS_BUSY_WAITING;
  limits->io_th               = IO_TH;
  limits->mpi_th              = MPI_TH;
  limits->cpi_mem_bound       = CPI_MEM_BOUND;
  limits->gbs_mem_bound       = GBS_MEM_BOUND;
}


state_t classify_init(topology_t *tp_in)
{
  switch (tp_in->vendor){
    case VENDOR_INTEL:
      /* Before Sylake */
      if (tp_in->model <= MODEL_BROADWELL_X){
        return EAR_SUCCESS;
      }
      /* Skylake */
      if (tp_in->model < MODEL_ICELAKE_X){
        return EAR_SUCCESS;
      }
      /* Icelake */
      if (tp_in->model >= MODEL_ICELAKE_X){
        return EAR_SUCCESS;
      }
      break;
    case VENDOR_AMD:
      /* Zen2 = Rome */
      if (tp_in->family < FAMILY_ZEN3){
        return EAR_SUCCESS;
      }
      /* Zen3 = Milan */
      if (tp_in->family >= FAMILY_ZEN3){
        return EAR_SUCCESS;
      }
      break;
    default:break;
  }
  return EAR_SUCCESS;
}

state_t classify(uint iobound,uint mpibound, uint *app_state)
{
	*app_state = APP_COMP_BOUND;
	if (iobound && !mpibound){
		*app_state = APP_IO_BOUND;
	} else if (mpibound) {
		*app_state = APP_MPI_BOUND;
	}
	return EAR_SUCCESS;
}

state_t is_io_bound(signature_t *sig, uint  num_cpus, uint *iobound)
{
    float rwsec = sig->IO_MBS;
    *iobound = 0;
    if (rwsec > IO_TH) *iobound = 1;
    return EAR_SUCCESS;

}

state_t is_network_bound(float mpisec,uint *mpibound)
{
	*mpibound = 0;
	if (mpisec > MPI_TH){
		*mpibound = 1;
	}
	return EAR_SUCCESS;
}

state_t must_switch_to_time_guide(float mpisec, ulong last_sig_elapsed, uint *ear_guided)
{
    if (mpisec < MIN_MPI_TH){
        *ear_guided = TIME_GUIDED;
    }
    if (signature_reported && (last_sig_elapsed > ((5*perf_accuracy_min_time)/1000000))){
      *ear_guided = TIME_GUIDED;
    }
    return EAR_SUCCESS;
}

state_t is_cpu_busy_waiting(signature_t *sig, uint *busy)
{
    
    if (!sig || !busy) {
        return_msg(EAR_ERROR, Generr.input_null);
    }

	uint c1 = (sig->CPI    < CPI_BUSY_WAITING);
	uint c2 = (sig->GBS    < GBS_BUSY_WAITING);
	uint c3 = (sig->Gflops < GFLOPS_BUSY_WAITING);

	*busy = USEFUL_COMP;

    if (c1 && c2 && c3) {
        *busy = BUSY_WAITING;
    }

    if (VERB_ON(INFO_CLASSIFY)) {

        if (c1 || c2 || c3) {
            char hdr_msg[] = "--- Busy waiting values (value/thresh) ---";

            char cpi_msg[32];
            snprintf(cpi_msg, sizeof cpi_msg, "CPI (%.3lf/%.3lf)", sig->CPI, CPI_BUSY_WAITING);

            char gbs_msg[32];
            snprintf(gbs_msg, sizeof gbs_msg, "Mem. bwidth (%.3lf/%.3lf)", sig->GBS, GBS_BUSY_WAITING);

            char gflops_msg[32];
            snprintf(gflops_msg, sizeof gflops_msg, "GFLOP/s (%.3lf/%.3lf)", sig->Gflops, GFLOPS_BUSY_WAITING);

            char busy_waiting_msg[64];
            snprintf(busy_waiting_msg, sizeof busy_waiting_msg,
                    "App node workload declared as busy waiting: %u", *busy);

            int max_strlen = strlen(busy_waiting_msg);
            /*
               int max_strlen = ear_max(strlen(hdr_msg), strlen(cpi_msg));
               max_strlen = ear_max(max_strlen, strlen(gbs_msg));
               max_strlen = ear_max(max_strlen, strlen(gflops_msg));
               */

            verbose_master(INFO_CLASSIFY, "%*s%s\n%*s%s\n%*s%s\n%*s%s\n%s\n",
                    (int) (max_strlen - strlen(hdr_msg)) / 2,    " ",  hdr_msg,
                    (int) (max_strlen - strlen(cpi_msg)) / 2,    " ",  cpi_msg,
                    (int) (max_strlen - strlen(gbs_msg)) / 2,    " ",  gbs_msg,
                    (int) (max_strlen - strlen(gflops_msg)) / 2, " ",  gflops_msg,
                    busy_waiting_msg);
        }

    }

	return EAR_SUCCESS;
}

uint    low_mem_activity(signature_t *sig,uint num_cpus)
{
  float per_proc_gbs = sig->GBS / num_cpus;
  return (per_proc_gbs < GBS_BUSY_WAITING);

}

state_t is_cpu_bound(signature_t *sig, uint num_cpus, uint *cbound)
{
    *cbound = 0;
    if ((sig->CPI < CPI_CPU_BOUND) && (sig->GBS < GBS_CPU_BOUND)) *cbound = 1;
    return EAR_SUCCESS;
}

state_t is_mem_bound(signature_t *sig,uint num_cpus, uint *mbound)
{
    *mbound = 0;
    if ((sig->CPI > CPI_MEM_BOUND) && (sig->GBS > GBS_MEM_BOUND)) *mbound = 1;
    return EAR_SUCCESS;
}

state_t is_gpu_idle(signature_t *sig, gpu_state_t *gpu_state)
{
#if USE_GPUS
    if (!is_cuda_enabled()) {
        /* One of the following cases:
         * - Node does not have GPUs
         * - Node has GPUs but Job does not use them
         */
        *gpu_state = _GPU_NoGPU;
        return EAR_SUCCESS;
    }
    // for sure the Job has been assigned to use GPUs, but are they being used?
	for (int i = 0; i < sig->gpu_sig.num_gpus; i++) {
        if (sig->gpu_sig.gpu_data[i].GPU_util > 0) {
            *gpu_state = _GPU_Comp;
            return EAR_SUCCESS;
        }
    }
    // GPUs are idle here
    *gpu_state = _GPU_Idle;
#else
	*gpu_state = _GPU_NoGPU;
#endif // USE_GPUS
	return EAR_SUCCESS;
}
