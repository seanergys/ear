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

#define APP_COMP_BOUND 	1
#define APP_MPI_BOUND 	2
#define APP_IO_BOUND		3

state_t classify(ullong total_rw,uint total_mpi, float rwsec,float mpisec,uint dyn_on,uint dynais_state,uint periodic_mode,uint earl_state,uint *app_state);

#endif
