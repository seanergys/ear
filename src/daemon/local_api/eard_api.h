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

#ifndef EARD_LOCAL_API_FUNCTIONS_H
#define EARD_LOCAL_API_FUNCTIONS_H

#include <common/types/log.h>
#include <common/types/generic.h>
#include <common/system/version.h>
#include <daemon/local_api/eard_api_rpc.h>
#include <daemon/local_api/eard_api_conf.h>

// This class is intended to provide easy-to-read functions instead RPCs to
// communicate between binaries and EAR Daemon (EARD).

typedef struct eard_state_s {
    version_t version;
    uint gpus_supported_count;
		uint cpus_supported_count;
		uint sock_supported_count;
		uint gpus_support_enabled;
    pid_t pid;
} eard_state_t;

state_t eards_get_state(eard_state_t *state);

// Legacy functions

// Request to read the RAPL counters. Returns -1 if there's an error, and 0 on
// success and fills the array with the counter values.
int eards_read_rapl(ullong *values);

// Requests the rapl data size, but returns -1 if there's an error. */
ulong eards_get_data_size_rapl();

// Request to write an application signature. Returns 0 on success, -1 on error.
ulong eards_write_app_signature(application_t *app_signature);

// Request write a loop signature into the DB. Returns 0 on success, -1 on error.
ulong eards_write_loop_signature(loop_t *loop_signature);

// Reports event. Returns 0 on success, -1 on error.
ulong eards_write_event(ear_event_t *event);

// Requests the DC energy of the node. Returns 0 on success, -1 on error.
int eards_node_dc_energy(void *energy, ulong datasize);

// Requests the DC energy node frequency. Returns -1 or 10000000 if not connected.
ulong eards_node_energy_frequency();

// Reports a loop signature to EARD to be reported in the DB.
ulong eards_write_loop_signature(loop_t *loop_signature);

// Returns the frequency at which the node energy frequency is refreshed.
ulong eards_node_energy_frequency();

// Returns if GPU is supported by the EARD.
int eards_gpu_support();

// Returns basic EARD compile configured limits & options for verification
state_t eards_get_state(eard_state_t *state);

// Legacy non-RPC services
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

#endif //EARD_LOCAL_API_FUNCTIONS_H
