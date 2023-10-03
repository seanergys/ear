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

// Config install first because by now is defining the SCHED_* macros.
#include <common/config/config_install.h>

// Including this because NULL_STEPID
#include <common/config/config_env.h>

#if SCHED_SLURM

#include <slurm/slurm.h>

#define BATCH_STEP    ((int) SLURM_BATCH_SCRIPT)

// See EAR's issue #121
#if SLURM_VERSION_NUMBER >= SLURM_VERSION_NUM(22,11,0)
#define INTERACT_STEP ((int) SLURM_INTERACTIVE_STEP)
#else 
#define INTERACT_STEP -6 // Mimic from slurm.h value
#endif // SLURM_VERSION_NUMBER

#endif // SCHED_SLURM

#if SCHED_PBS
#define BATCH_STEP     0
#define INTERACT_STEP -6 // Mimic from slurm.h value
#endif

#if SCHED_OAR
#define BATCH_STEP     0
#define INTERACT_STEP -6 // Mimic from slurm.h value
#endif

#if !SCHED_SLURM && !SCHED_PBS && !SCHED_OAR
#define BATCH_STEP     NULL_STEPID
#define INTERACT_STEP -6
#endif

#endif //COMMON_CONFIG_SCHED_H
