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

#ifndef _EVENT_H
#define _EVENT_H

#include <time.h>
#include <common/config.h>
#include <common/types/generic.h>
#include <common/types/job.h>

typedef struct ear_event {
	job_id jid;
    job_id step_id;
	char node_id[GENERIC_NAME];
	uint event;
	ulong freq;
	time_t timestamp;
} ear_event_t;


#define ENERGY_POLICY_NEW_FREQ  0
#define GLOBAL_ENERGY_POLICY  1
#define ENERGY_POLICY_FAILS   2
#define DYNAIS_OFF        3
#define ENERGY_SAVING     9
#define POWER_SAVING      10
#define PERF_PENALTY      11

#define EARL_STATE              4
#define EARL_PHASE              5
#define EARL_POLICY_PHASE       6
#define EARL_MPI_LOAD_BALANCE   7
#define EARL_OPT_ACCURACY       8


#define OPT_NOT_READY  0
#define OPT_OK         1
#define OPT_NOT_OK     2
#define OPT_TRY_AGAIN  3

#define ENERGY_POLICY_NEW_FREQ_TXT "new_frequency"
#define GLOBAL_ENERGY_POLICY_TXT   "cluster_policy"
#define ENERGY_POLICY_FAILS_TXT    "policy_error"
#define DYNAIS_OFF_TXT             "dynais_off"
#define EARL_STATE_TXT 						 "earl_state"
#define EARL_PHASE_TXT 						 "earl_phase"
#define EARL_POLICY_PHASE_TXT 		 "policy_phase"
#define EARL_MPI_LOAD_BALANCE_TXT  "mpi_load_balance"
#define EARL_OPT_ACCURACY_TXT      "optim_accuracy"


#define OPT_NOT_READY_TXT         "optim_not_ready"
#define OPT_OK_TXT                "optim_ok"
#define OPT_NOT_OK_TXT            "optim_not_ok"
#define OPT_TRY_AGAIN_TXT         "optim_try_again"


/* EARD Init events */

#define PM_CREATION_ERROR       100
#define APP_API_CREATION_ERROR  101
#define DYN_CREATION_ERROR      102
#define UNCORE_INIT_ERROR       103
#define RAPL_INIT_ERROR         104
#define ENERGY_INIT_ERROR       105
#define CONNECTOR_INIT_ERROR    106
#define RCONNECTOR_INIT_ERROR   107



/* EARD runtime events */
#define DC_POWER_ERROR		300
#define TEMP_ERROR			301
#define FREQ_ERROR			302
#define RAPL_ERROR			303
#define GBS_ERROR			304
#define CPI_ERROR			305

/* EARD powercap events */
#define POWERCAP_VALUE  500
#define RESET_POWERCAP  501
#define INC_POWERCAP    502
#define RED_POWERCAP    503
#define SET_POWERCAP    504
#define SET_ASK_DEF     505
#define RELEASE_POWER   506


#define EARD_RT_ERRORS  6
#define FIRST_RT_ERROR  300


/* EARGM events */
#define CLUSTER_POWER   600
#define NODE_POWERCAP   601
#define POWER_UNLIMITED 602


void event_type_to_str(ear_event_t *ev, char *str, size_t max);
void event_value_to_str(ear_event_t *ev, char *str, size_t max);



#endif
