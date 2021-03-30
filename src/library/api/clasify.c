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

state_t classify(ullong total_rw,uint total_mpi, float rwsec,float mpisec,uint dyn_on,uint dynais_state,uint periodic_mode,uint earl_state,uint *app_state)
{
	*app_state = APP_COMP_BOUND;
	if ((dyn_on == DYNAIS_ENABLED) && (dynais_state == NO_LOOP) && (earl_state == NO_PERIOD)){
		if (rwsec > IO_TH) *app_state = APP_IO_BOUND;
		return EAR_SUCCESS;
	}
	if ((rwsec > IO_TH) && (mpisec < MPI_TH)){
		*app_state = APP_IO_BOUND;
		return EAR_SUCCESS;
	}
	if (mpisec > MPI_TH){
		*app_state = APP_MPI_BOUND;
		return EAR_SUCCESS;
	}
	return EAR_SUCCESS;
}
