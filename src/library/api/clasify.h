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

#ifndef APP_CLASSIFICATION
#define APP_CLASSIFICATION

#include <stdio.h>
#include <stdlib.h>
#include <common/config.h>
#include <common/hardware/topology.h>
#include <common/types/signature.h>


#define APP_COMP_BOUND   	1
#define APP_MPI_BOUND   	2
#define APP_IO_BOUND		3
#define APP_BUSY_WAITING	4

#define DYNAIS_GUIDED 0
#define TIME_GUIDED	  1

#define USEFUL_COMP  0
#define BUSY_WAITING 1
// #define CPI_CPU_BOUND 0.4

//#define FLOPS_CPU_BOUND 100.0

typedef enum {
    _GPU_NoGPU = 1,
    _GPU_Comp  = 2,
    _GPU_Idle  = 4,
} gpu_state_t;


typedef struct ear_classify{
  float cpi_cpu_bound ;
  float gbs_cpu_bound;
  float cpi_busy_waiting;
  float gbs_busy_waiting;
  float gflops_busy_waiting;
  float io_th;
  float mpi_th;
  float cpi_mem_bound;
  float gbs_mem_bound;
  float mpi_bound;
}ear_classify_t;

state_t classify_init(topology_t *tp_in);
void get_classify_limits(ear_classify_t *limits);
state_t classify(uint iobound, uint mibound, uint *app_state);
state_t is_cpu_busy_waiting(signature_t *sig, uint num_cpus, uint *busy);
uint is_process_busy_waiting(ssig_t *sig);
state_t is_cpu_bound(signature_t *sig,uint num_cpus, uint *cbound);
state_t is_mem_bound(signature_t *sig,uint num_cpus, uint *mbound);
state_t is_gpu_idle(signature_t *sig, gpu_state_t *gpu_state);
state_t is_io_bound(signature_t *sig,uint num_cpus, uint *iobound);
state_t is_network_bound(signature_t *sig, uint num_cpus, uint *mpibound);
uint    low_mem_activity(signature_t *sig,uint num_cpus);


state_t must_switch_to_time_guide(float mpisec, ulong last_sig_elapsed, uint *ear_guided);


#endif // APP_CLASSIFICATION
