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

#ifndef _EAR_DAEMON_COMMON_H
#define _EAR_DAEMON_COMMON_H

#define _GNU_SOURCE             
#include <sched.h>

#include <common/config/config_install.h>
#include <common/types/generic.h>
#include <common/types/application.h>
#include <common/types/job.h>
#include <common/types/loop.h>
#include <common/types/event_type.h>

typedef struct req_padding_s {
  uint  padding1;
  ulong padding2[MAX_CPUS_SUPPORTED];
}req_padding_t;

// Data type to send the requests
union daemon_req_opt {
    unsigned long   req_value;
    application_t   app;
	loop_t		    loop;
	ear_event_t     event;
	req_padding_t   padding; // Padding is just for compatibility
};

typedef struct app_id{
  ulong jid;
  ulong sid;
  ulong lid;
}app_id_t;

/* new_services : This section is eard request messages headers */
typedef struct eard_head_s {
  ulong    req_service;
  ulong    sec;
  size_t   size;
  state_t  state;
  app_id_t con_id;
} eard_head_t;

struct daemon_req {
/* For new_services : This section must include same size than eard_head_t */
  ulong    req_service;
  ulong    sec;
  size_t   size;
  state_t  state;
  app_id_t con_id;
  union daemon_req_opt req_data;
};

#define WAIT_DATA    0
#define NO_WAIT_DATA 1

int eards_read(int fd,char *buff,int size, uint type);
int eards_write(int fd,char *buff,int size);

// Legacy RPC services
#define CONNECT_EARD_NODE_SERVICES      5
#define DISCONNECT_EARD_NODE_SERVICES   1000
#define EAR_COM_OK                      0
#define EAR_COM_ERROR                  -1
#define READ_RAPL                       202
#define DATA_SIZE_RAPL                  203
#define WRITE_APP_SIGNATURE             300
#define WRITE_EVENT                     302
#define WRITE_LOOP_SIGNATURE            303
#define READ_DC_ENERGY                  400
#define ENERGY_FREQ                     403
#define GPU_SUPPORTED                   505
#define NEW_API_SERVICES                1000

#endif
