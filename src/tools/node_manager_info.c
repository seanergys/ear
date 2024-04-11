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

#include <stdio.h>
#include <stdlib.h>

#include <common/config.h>
#include <common/states.h>
#include <daemon/local_api/node_mgr.h>
#include <common/utils/sched_support.h>


int main(int argc, char *argv[])
{
	ear_njob_t *nlist;
	char *tmp = "/var/ear";

	if (nodemgr_job_init(tmp, &nlist) != EAR_SUCCESS){
		printf("Error in nodemgr_job_init\n");
		exit(0);
	}

	for (uint j = 0;j < MAX_CPUS_SUPPORTED; j++){
		printf("Node:mgr[%u] jid %lu sid %lu creation_time %lu\n", j, nlist[j].jid, nlist[j].sid, nlist[j].creation_time);
		if (nlist[j].jid != -1){
			if (!is_job_step_valid(nlist[j].jid, nlist[j].sid)){
				printf("Error, JOB %lu.%lu not longer valid\n", nlist[j].jid, nlist[j].sid);
			}
		}
	}

	return 0;	
	
}
