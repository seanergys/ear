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
#include <global_manager/meta_eargm.h>

static ulong total_power, total_requested, total_freeable, total_extra;
static uint num_ask_def, num_greedy, num_greedy_no_extra;
extern uint total_nodes;
extern uint my_port;
cluster_conf_t *conf = NULL;


int eargm_table_init(eargm_table_t *egm_table, cluster_conf_t *conf, int egm_idx)
{
    int i, j;

    egm_table->num_eargms   = conf->eargm.eargms[egm_idx].num_subs;
    egm_table->eargm_ids    = calloc(egm_table->num_eargms, sizeof(int));
    egm_table->eargm_ips    = calloc(egm_table->num_eargms, sizeof(int));
    egm_table->eargm_ports  = calloc(egm_table->num_eargms, sizeof(int));
    egm_table->action       = calloc(egm_table->num_eargms, sizeof(char));
    egm_table->action_value = calloc(egm_table->num_eargms, sizeof(uint));
    
    for (i = 0; i < conf->eargm.eargms[egm_idx].num_subs; i++)
    {
        for (j = 0; j < conf->eargm.num_eargms; j++) {
            if (conf->eargm.eargms[j].id == conf->eargm.eargms[egm_idx].subs[i]) {
                egm_table->eargm_ids[i]   = conf->eargm.eargms[j].id;
                egm_table->eargm_ports[i] = conf->eargm.eargms[j].port;
                egm_table->eargm_ips[i] = get_ip(conf->eargm.eargms[j].node, conf);
            }
        }
    }
}

void meta_aggregate_data(eargm_status_t *status, uint num_status)
{
    int i;

    //reset values
    total_power = total_requested = total_freeable = total_extra = 0;
    num_ask_def = num_greedy = num_greedy_no_extra = 0;

    //aggregate data
    for (i = 0; i < num_status; i++)
    {
        total_power += status[i].current_power;
        total_requested += status[i].requested;
        total_freeable += status[i].freeable_power;
        total_extra += status[i].extra_power;

        if (status[i].requested) {
            if ((status[i].current_powercap < status[i].def_power)) {
                num_ask_def ++;
            } else if (status[i].requested) {
                if (status[i].extra_power) num_greedy++;
                else num_greedy_no_extra++;
            }
        }
    }

}

void meta_reallocate_power(eargm_status_t *status, uint num_status)
{
    meta_aggregate_data(status, num_status);
    //algorithm goes here
    //
    //
}

void *meta_eargm(void *config)
{
    int i, num_status, eargm_idx = -1;
    eargm_def_t *e_def;
    eargm_status_t *status;
    eargm_table_t egm_table;
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
        if (conf->eargm.eargms[i].id == e_def->id) {
            eargm_idx = i;
            break;
        }
    }

    if (eargm_idx < 0) {
        error("EARGM is not defined in the configuration file (ear.conf), closing meta_eargm");
        pthread_exit(0);
    }

    if (eargm_table_init(&egm_table, conf, eargm_idx) != EAR_SUCCESS)
    {
        error("Error initializing meta_eargm node tables, closing meta_eargm");
        pthread_exit(0);
    }

    while(1)
    {
        debug("waiting period of time before asking nodes");
        //sleep(conf->eargm.t1_power*2);
        sleep(10);

        num_status = eargm_get_all_status(conf, &status, eargm_idx);
        debug("Received %d status in meta thread", num_status);
        if (num_status > 0) 
        {
            for (i = 0; i < num_status; i++) {
                verbose(VGM, "id %d power %lu energy %lu", status[i].id, status[i].current_power, status[i].energy);
            }
            meta_reallocate_power(status, num_status);

            free(status);
        }
    }


    verbose(VGM, "exiting meta_eargm thread");
    pthread_exit(0);
}
