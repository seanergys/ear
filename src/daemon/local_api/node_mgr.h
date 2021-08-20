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

#ifndef _LIB_NODE_MGR_H
#define _LIB_NODE_MGR_H

#include <common/states.h>

#include <common/types/generic.h>
#include <library/common/library_shared_data.h>

#define LOCK_NODE_MGR_LCK	  ".node_mgr_lck"
#define LOCK_NODE_MGR_INFO	".node_mgr_data"

typedef struct ear_njob{
	job_id jid;
	job_id sid;
	lib_shared_data_t *job_data;
	shsignature_t		*job_signatures;		
}ear_njob_t;

/** Initialices the lock file and attaches the shared memory region. nodelist is internally allocated. Must be used by earl */
state_t nodemgr_job_init(char *tmp, ear_njob_t **nodelist );

/* Created the lock and the shared region. To be used by eard */
state_t nodemgr_server_init(char *tmp, ear_njob_t **nodelist);

/* Computes the number of jobs attached */
state_t nodemgr_get_num_jobs_attached(uint *num_jobs);

/* Attaches a new job . To be used by earl */
state_t nodemgr_attach_job(ear_njob_t *my_job, uint *my_index);

/* Marks a job entry as free. To be used by earl before job end */
state_t nodemgr_job_end(uint index);

/* Cleans a given job entry. Tobe used by eard to guarantee the job is not there */
state_t nodemgr_clean_job(job_id jid,job_id sid);

/* Removes the shared region. To be used by eard at eard exit */
state_t nodemgr_server_end();



#endif
