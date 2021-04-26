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
#include <common/config.h>
#include <common/states.h>
#include <library/common/verbose_lib.h>
#include <library/common/externs.h>
#include <library/dynais/dynais.h>
#include <library/api/clasify.h>
#include <library/states/states_comm.h>


#define IO_TH 	10.0
#define MPI_TH 10000
#define MIN_MPI_TH 200

state_t classify(ullong total_rw,uint total_mpi, float rwsec,float mpisec,uint dyn_on,uint dynais_state,uint periodic_mode,uint earl_state,uint *app_state,uint *ear_guided)
{
	*app_state = APP_COMP_BOUND;
	if ((dyn_on == DYNAIS_ENABLED) && (dynais_state == NO_LOOP) && (earl_state == NO_PERIOD)){
		if (rwsec > IO_TH) *app_state = APP_IO_BOUND;
	}else if ((rwsec > IO_TH) && (mpisec < MPI_TH)){
		*app_state = APP_IO_BOUND;
	}else if (mpisec > MPI_TH){
		*app_state = APP_MPI_BOUND;
	}
	if (mpisec < MIN_MPI_TH){
		*ear_guided = TIME_GUIDED;
	}
	return EAR_SUCCESS;
}

state_t is_cpu_busy_waiting(signature_t *sig,uint *busy)
{
	ull flop = 0;
	double flops;
	uint i;
	uint c1, c2, c3;
	*busy = USEFUL_COMP;
	for (i=0;i<FLOPS_EVENTS;i++) flop+=sig->FLOPS[i];
	flops = (double)flop/sig->time;
	c1 = (sig->CPI < CPI_CPU_BOUND);
	c2 = (sig->GBS < GBS_CPU_BOUND);
	c3 = (flops < FLOPS_CPU_BOUND);
	if (c1 || c2 || c3){
		verbose_master(2,"CPI (%.3lf/%.3lf) GBS(%.3lf/%.3lf) FLOPS (%.3lf/%.3lf)", sig->CPI,CPI_CPU_BOUND,sig->GBS,GBS_CPU_BOUND,flops,FLOPS_CPU_BOUND);
	}
	if (c1 && c2 && c3) *busy = BUSY_WAITING;
	return EAR_SUCCESS;
}
