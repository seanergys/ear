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

#define _XOPEN_SOURCE 700 //to get rid of the warning
#define _GNU_SOURCE 

#define SHOW_DEBUGS 1

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <common/states.h>
#include <common/config.h>
#include <common/output/verbose.h>
#include <common/types/generic.h>
#include <common/messaging/msg_internals.h>

#include <global_manager/eargm_rapi.h>


extern uint total_nodes;
extern uint my_port;
cluster_conf_t *conf = NULL;

void *meta_eargm(void *config)
{
    int num_status, i, eargm_idx = -1;
    eargm_def_t *e_def;
    eargm_status_t *status;
    char host[GENERIC_NAME];

    conf = (cluster_conf_t *) config;

    gethostname(host, sizeof(host));
    strtok(host, ".");
    e_def = get_eargm_conf(conf, host);
    if (e_def == NULL) {
        error("Could not get configuration for current node's meta_EARGM, exiting thread");
        pthread_exit(0);
    }

    if (pthread_setname_np(pthread_self(), "meta_eargm")) verbose(0, "Setting name for meta_eargm thread %s", strerror(errno));
    debug("created meta_eargm thread");

    for (i = 0; i < conf->eargm.num_eargms; i++) {
        if (conf->eargm.eargms[i].id == e_def->id) eargm_idx = i;
    }
    if (eargm_idx < 0) {
        error("EARGM is not defined in the configuration file (ear.conf), closing meta_eargm");
        pthread_exit(0);
    }

    while(1)
    {
        debug("waiting period of time before asking nodes");
        sleep(conf->eargm.t1_power*2);

        num_status = eargm_get_all_status(conf, &status, eargm_idx);
        debug("Received %d status in meta thread", num_status);
        if (num_status > 0) 
        {
            for (i = 0; i < num_status; i++)
            {
                debug("id %d power %lu energy %lu", status[i].id, status[i].current_power, status[i].energy);
            }
            free(status);
        }
    }

    
    verbose(VGM, "exiting meta_eargm thread");
    pthread_exit(0);
}
