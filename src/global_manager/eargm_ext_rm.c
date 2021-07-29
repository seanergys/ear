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
//#define SHOW_DEBUGS 0
#include <common/output/verbose.h>
#include <common/types/generic.h>
#include <common/messaging/msg_internals.h>
#include <daemon/remote_api/eard_rapi.h>
#include <global_manager/cluster_powercap.h>


extern uint total_nodes;
extern uint my_port;
extern uint eargm_id;

extern shared_powercap_data_t ext_powercap_data;
extern pthread_mutex_t ext_mutex;

void eargm_return_status(int clientfd, long ack)
{
    eargm_status_t status;
    send_answer(clientfd, &ack);

    memset(&status, 0, sizeof(eargm_status_t));
    status.id = eargm_id;

    /* Critical section */
    pthread_mutex_lock(&ext_mutex);
    status.current_power = ext_powercap_data.current_power;
    status.def_power = ext_powercap_data.def_power;
    status.extra_power = ext_powercap_data.extra_power;
    status.requested = ext_powercap_data.requested;
    status.freeable_power = ext_powercap_data.available_power;
    pthread_mutex_unlock(&ext_mutex);
    /* End of critical section */

    debug("sending eargm_status with power %lu and energy %lu\n", status.current_power, status.energy);
    send_data(clientfd, sizeof(eargm_status_t), (char *)&status, EAR_TYPE_EARGM_STATUS);
}

/*
 *
 *	THREAD attending external commands. 
 *
 */
void process_remote_requests(int clientfd)
{
    request_t command;
    memset(&command, 0, sizeof(request_t));
    uint req;
    long ack=EAR_SUCCESS;
    debug("connection received");
    req=read_command(clientfd,&command);
    switch (req){
        case EARGM_NEW_JOB:
            // Computes the total number of nodes in use
            debug("new_job command received %d num_nodes %u",command.req,command.num_nodes);
            total_nodes+=command.my_req.eargm_data.num_nodes;
            break;
        case EARGM_END_JOB:
            // Computes the total number of nodes in use
            debug("end_job command received %d num_nodes %u",command.req,command.num_nodes);
            total_nodes-=command.my_req.eargm_data.num_nodes;
            break;
        case EARGM_STATUS:
            // Responds with the current EARGM stats
            debug("received eargm status request");
            eargm_return_status(clientfd, ack);
            return;
        default:
            error("Invalid remote command");
    }  
    send_answer(clientfd,&ack);
}


void *eargm_server_api(void *p)
{
    int eargm_fd,eargm_client;
    struct sockaddr_in eargm_con_client;

    if (pthread_setname_np(pthread_self(), "eargm_server")) error("Setting name for eargm_server thread %s", strerror(errno));

    debug("Creating scoket for remote commands,using port %u",my_port);
    eargm_fd=create_server_socket(my_port);
    if (eargm_fd<0){
        error("Error creating socket");
        pthread_exit(0);
    }
    do{
        debug("waiting for remote commands port=%u",my_port);
        eargm_client=wait_for_client(eargm_fd,&eargm_con_client);
        if (eargm_client<0){
            error(" wait_for_client returns error");
        }else{
            process_remote_requests(eargm_client);
            close(eargm_client);
        }
    }while(1);
    verbose(VGM,"exiting");
    close_server_socket(eargm_fd);
    pthread_exit(0);
}




