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

#ifndef COMMON_CONFIG_SCHED_H
#define COMMON_CONFIG_SCHED_H

#if SCHED_SLURM
#include <slurm/slurm.h>
#endif
#include <common/config/config_install.h>

// These variables are SLURM named variables. In case we want to use it from
// different queue managers, we can:
// 1) Replace a manager exclusive variable by our SLURM named variable inside
//    the specific plugin for PBS, OAR...
// 2) Surround these variable definitions by the different #ifs and set the name
//    depending on the queue manager are you compiling for. This strategy limits
//    an EAR component compatibility to a specific queue manager.
//
// Commented variables are unused and can be removed.
#define NULL_STEPID             (0xfffffffe)
#define NULL_ACCOUNT            "NO_SLURM_ACCOUNT"
#define FLAG_TASK_PID           "TASK_PID"
#define SCHED_JOB_ID            "JOB_ID"
#define SCHED_STEP_ID           "STEP_ID"
#define SCHED_STEPID            "STEPID"                 // Deprecated by SLURM, maintained for backwards compatibility.
#define SCHED_JOB_ACCOUNT       "JOB_ACCOUNT"
#define SCHED_NUM_TASKS         "STEP_TASKS_PER_NODE"
#define SCHED_STEP_NUM_NODES    "STEP_NUM_NODES"
#define SCHED_IS_ERUN           "IS_ERUN"

#if SCHED_SLURM
#define BATCH_STEP              ((int) SLURM_BATCH_SCRIPT)
#endif

#if SCHED_PBS
#define BATCH_STEP              0
#endif

#if SCHED_OAR
#define BATCH_STEP              0
#endif

#endif //COMMON_CONFIG_SCHED_H