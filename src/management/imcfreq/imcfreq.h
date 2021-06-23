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

#ifndef MANAGEMENT_IMCFREQ_H
#define MANAGEMENT_IMCFREQ_H

#include <common/types.h>
#include <common/states.h>
#include <common/plugins.h>
#include <common/hardware/topology.h>
#include <management/cpufreq/cpufreq.h>

// Delete when merge
#if 1
#define ps_nothing (uint) -1
#define ps_auto    (uint) -2

#define replace_ops(p1, p2) \
	if (p1 == NULL) { \
		p1 = p2; \
	}

// APIs IDs
#define API_NONE    0
#define API_DUMMY   1
#define API_EARD    2
#define API_DEFAULT 3
#define API_AMD17   4
#define API_NVML    5
// EARD API load or not
#define NO_EARD     0
#define EARD        1
#endif

#define all_sockets all_cpus
#define all_devices all_sockets

typedef struct imcfreq_ops_s
{
	state_t (*init)               (ctx_t *c);
	state_t (*dispose)            (ctx_t *c);
	state_t (*count_devices)      (ctx_t *c, uint *devs_count);
	state_t (*get_available_list) (ctx_t *c, const pstate_t **pstate_list, uint *pstate_count);
	state_t (*get_current_list)   (ctx_t *c, pstate_t *pstate_list);
	state_t (*set_current_list)   (ctx_t *c, uint *index_list);
	state_t (*set_current)        (ctx_t *c, uint pstate_index, int socket);
	state_t (*set_auto)           (ctx_t *c);
	state_t (*get_current_min_list) (ctx_t *c, pstate_t *ps_max_list, pstate_t *ps_min_list);
	state_t (*set_current_min_list) (ctx_t *c, uint *id_max_list, uint *id_min_list);
	state_t (*data_alloc)         (pstate_t **pstate_list, uint **index_list);
	state_t (*data_print)         (pstate_t *pstate_list, uint pstate_count, int fd);
	state_t (*data_tostr)         (pstate_t *pstate_list, uint pstate_count, char *buffer, uint length);
} imcfreq_ops_t;

/** */
state_t mgt_imcfreq_load(topology_t *c, int eard);

/** Returns the loaded API. */
state_t mgt_imcfreq_get_api(uint *api);

/** */
state_t mgt_imcfreq_init(ctx_t *c);

/** */
state_t mgt_imcfreq_dispose(ctx_t *c);

/** Returns the number of devices (normally the number of sockets). */
state_t mgt_imcfreq_count_devices(ctx_t *c, uint *devs_count);

/** Returns de list of available frequencies. */
state_t mgt_imcfreq_get_available_list(ctx_t *c, const pstate_t **pstate_list, uint *pstate_count);

/** Returns the current frequencies. */
state_t mgt_imcfreq_get_current_list(ctx_t *c, pstate_t *pstate_list);

/** Sets a PSTATE by a list per device. */
state_t mgt_imcfreq_set_current_list(ctx_t *c, uint *index_list);

/** Sets a PSTATE per device (you can use all_devices). */
state_t mgt_imcfreq_set_current(ctx_t *c, uint pstate_index, int device);

/** Sets all devices in AUTO mode if available. */
state_t mgt_imcfreq_set_auto(ctx_t *c);

/** Gets the current PSTATE including a minimum frequency if its available. */
state_t mgt_imcfreq_get_current_ranged_list(ctx_t *c, pstate_t *ps_max_list, pstate_t *ps_min_list);

/** Sets the current PSTATE including a minimum frequency if its available. */
state_t mgt_imcfreq_set_current_ranged_list(ctx_t *c, uint *id_max_list, uint *id_min_list);

/** Allocates a list of PSTATEs and indexes of a length of the number of sockets. */
state_t mgt_imcfreq_data_alloc(pstate_t **pstate_list, uint **index_list);

/** Outputs a PSTATE list in a file descriptor. */
state_t mgt_imcfreq_data_print(pstate_t *pstate_list, uint pstate_count, int fd);

/** Outputs a PSTATE list in a buffer. */
state_t mgt_imcfreq_data_tostr(pstate_t *pstate_list, uint pstate_count, char *buffer, uint length);

#endif //MANAGEMENT_IMCFREQ_H
