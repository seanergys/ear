/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#ifndef _SCHED_SUPPORT_H
#define _SCHED_SUPPORT_H

#include <common/types/generic.h>

/* Checks if a given jobid stepid is still valid in the scheduler. Only SLURM is supported for now */
uint is_job_step_valid(job_id jid, job_id sid);
int get_my_sched_node_rank();
int get_my_sched_group_node_size();
int get_my_sched_global_rank();

void sched_local_barrier(char *file,int N);
void remove_local_barrier(char *file);
#endif


