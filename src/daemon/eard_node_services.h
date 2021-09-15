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

#ifndef _EARD_NODE_SERVICES_H_
#define _EARD_NODE_SERVICES_H_

#include <common/states.h>
#define CLOSE_EARD -100
#define CLOSE_PIPE -101

void clean_global_connector();
void eard_close_comm(int req_fd,int ack_fd);
state_t eard_local_api();
state_t service_close_by_id(ulong jid,ulong sid);



#else
#endif
