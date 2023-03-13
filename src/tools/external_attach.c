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
#include <common/external/ear_external.h>
#include <common/types/configuration/cluster_conf.h>

int usage(char *app)
{
    printf("Usage: %s [jobid] [stepid]\n", app);
    printf("EAR_TMP must be set for the application to work\n");
    return 0;
}

int main(int argc, char *argv[])
{

    if (argc < 3 || getenv("EAR_TMP") == NULL) return usage(argv[0]);
    

    setenv("SLURM_JOB_ID", argv[1], 0);
    setenv("SLURM_STEP_ID", argv[2], 0);

    ear_mgt_t *mgt = NULL;
    if ((mgt = ear_connect()) == NULL) {
        printf("Error connecting with the shared region area\n");
        return 1;
    }

    mgt->new_mask = 1;
    // what here?
    //
    if (state_fail(ear_disconnect())) {
        printf("Error deattaching from the shared region area\n"); 
    }


    return 0;
}

