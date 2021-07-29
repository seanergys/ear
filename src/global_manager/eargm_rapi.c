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

#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#include <common/states.h>
#include <common/output/verbose.h>
#include <common/messaging/msg_internals.h>

#include <global_manager/eargm_rapi.h>

// Verbosity
static int eargm_sfd;
static uint eargm_remote_connected=0;


int eargm_get_all_status(cluster_conf_t *conf, eargm_status_t **status, int eargm_idx)
{
    int i, j, rc, ret, port, num_status = 0;
    eargm_status_t *aux_status, *final_status = NULL;
    char *tmp_eargm = NULL;

    for (i = 0; i < conf->eargm.eargms[eargm_idx].num_subs; i++)
    {
        for (j = 0; j < conf->eargm.num_eargms; j++) {
            if (conf->eargm.eargms[j].id == conf->eargm.eargms[eargm_idx].subs[i]) {
                tmp_eargm = conf->eargm.eargms[j].node;
                port = conf->eargm.eargms[j].port;
            }
        }
        rc = remote_connect(tmp_eargm, port);

        ret = eargm_get_status(rc, &aux_status);
        debug("Received %d eargm status", ret);
        if (ret > 0)
        {
            num_status += ret;
            final_status = realloc(final_status, sizeof(eargm_status_t)*num_status);
            memcpy(&final_status[num_status-1], aux_status, ret*sizeof(eargm_status_t));
            free(aux_status);
        }
        tmp_eargm = NULL; //reset the var
    }

    *status = final_status;

    return num_status;

}

int eargm_get_status(int fd, eargm_status_t **status)
{
    request_t command;

    request_header_t head;
    eargm_status_t *aux_status;

    memset(&command, 0, sizeof(request_t));

    //set command
    command.req = EARGM_STATUS;

    //send command
    send_command(&command);

    //receive data
    head = receive_data(fd, (void **)&aux_status);
    debug("received header with size %u and type %d", head.size, head.type);
    debug("size of eargm_status_t %lu", sizeof(eargm_status_t));
    if (head.size < sizeof(eargm_status_t) || head.type != EAR_TYPE_EARGM_STATUS)
    {
        debug("Error getting eargm status");
        if (head.size > 0) free(status);
        return 0;
    }

    *status = aux_status;

    int num_status = head.size/sizeof(eargm_status_t);

    return num_status;
}

int eargm_new_job(uint num_nodes)
{
    request_t command;
    memset(&command, 0, sizeof(request_t));
    command.req=EARGM_NEW_JOB;
    command.my_req.eargm_data.num_nodes=num_nodes;
    verbose(2,"command %u num_nodes %u\n",command.req,command.my_req.eargm_data.num_nodes);
    return send_command(&command);
}

int eargm_end_job(uint num_nodes)
{
    request_t command;
    memset(&command, 0, sizeof(request_t));
    command.req=EARGM_END_JOB;
    command.my_req.eargm_data.num_nodes=num_nodes;
    verbose(2,"command %u num_nodes %u\n",command.req,command.my_req.eargm_data.num_nodes);
    return send_command(&command);
}

int eargm_disconnect()
{
    eargm_remote_connected=0;
    close(eargm_sfd);
    return EAR_SUCCESS;
}
