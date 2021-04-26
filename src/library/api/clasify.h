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

#ifndef APP_CLASSIFICATION
#define APP_CLASSIFICATION
#include <stdio.h>
#include <stdlib.h>
#include <common/config.h>
#include <common/types/signature.h>


#define APP_COMP_BOUND 	1
#define APP_MPI_BOUND 	2
#define APP_IO_BOUND		3
#define APP_BUSY_WAITING	4


#define DYNAIS_GUIDED 0
#define TIME_GUIDED 	1

#define USEFUL_COMP 0
#define BUSY_WAITING 1
// #define CPI_CPU_BOUND 0.4
#define CPI_CPU_BOUND 0.6
#define GBS_CPU_BOUND 2.0
#define FLOPS_CPU_BOUND 5000.0
//#define FLOPS_CPU_BOUND 100.0

state_t classify(ullong total_rw,uint total_mpi, float rwsec,float mpisec,uint dyn_on,uint dynais_state,uint periodic_mode,uint earl_state,uint *app_state,uint *ear_guided);
state_t is_cpu_busy_waiting(signature_t *sig,uint *busy);


#endif
