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

#include <fcntl.h>
#include <stddef.h>
#include <sys/types.h>

#include <common/types/types.h>
#include <common/system/shared_areas.h>
#include <common/external/ear_external.h>


static int fd_resched = -1;
static char path[256];

int get_resched_path(char *tmp, uint ID, char *path)
{
	if ((tmp==NULL) || (path==NULL)) return EAR_ERROR;
	sprintf(path,"%s/%u/.ear_resched",tmp, ID);
	return EAR_SUCCESS;	
}

state_t ear_connect(ear_mgt_t *ear) 
{

    //create resched area
    ear_mgt_t mgt;
    char *tmp = ""; // how do we get the tmp? env_var?

    //get the path
    
    if (get_resched_path(tmp, 0, path) != EAR_SUCCESS) return EAR_ERROR;

    create_shared_area(path, (char *)&mgt, sizeof(ear_mgt_t), &fd_resched, 1);
    attach_shared_area(path, sizeof(ear_mgt_t), O_RDWR, &fd_resched, NULL);

    return EAR_SUCCESS;
}


state_t ear_disconnect()
{
    dettach_shared_area(fd_resched);

    //is this necessary??
    dispose_shared_area(path, fd_resched);

    return EAR_SUCCESS;
}

