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

#define SHOW_DEBUGS 1

#include <stdio.h>
#include <pthread.h>
#include <metrics/common/msr.h>
#include <common/output/debug.h>
#include <management/imcfreq/imcfreq.h>
#include <management/imcfreq/archs/eard.h>
#include <management/imcfreq/archs/dummy.h>
#include <management/imcfreq/archs/amd17.h>
#include <management/imcfreq/archs/intel63.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static imcfreq_ops_t ops;
static uint sockets_count;
static uint api;

state_t mgt_imcfreq_load(topology_t *tp, int eard)
{
	state_t s;
	while (pthread_mutex_trylock(&lock));
	if (api != API_NONE) {
		pthread_mutex_unlock(&lock);
		return EAR_SUCCESS;
	}
	sockets_count = tp->socket_count;
	// API load
	api = API_DUMMY;
	if (state_ok(s = mgt_imcfreq_amd17_load(tp, &ops))) {
		api = API_AMD17;
		debug("Loaded AMD17");
	}
	if (state_ok(s = mgt_imcfreq_intel63_load(tp, &ops))) {
		api = API_DEFAULT;
		debug("Loaded DEFAULT");
	}
	if (state_ok(s = mgt_imcfreq_eard_load(tp, &ops, eard))) {
		api = API_EARD;
		debug("Loaded EARD");
	}
	mgt_imcfreq_dummy_load(tp, &ops);
	pthread_mutex_unlock(&lock);
	return EAR_SUCCESS;
}

state_t mgt_imcfreq_get_api(uint *api_in)
{
	*api_in = api;
	return EAR_SUCCESS;
}

state_t mgt_imcfreq_init(ctx_t *c)
{
	preturn (ops.init, c);
}

state_t mgt_imcfreq_dispose(ctx_t *c)
{
	preturn (ops.dispose, c);
}

state_t mgt_imcfreq_count_devices(ctx_t *c, uint *devs_count)
{
	preturn (ops.count_devices, c, devs_count);
}

state_t mgt_imcfreq_get_available_list(ctx_t *c, const pstate_t **pstate_list, uint *pstate_count)
{
	preturn (ops.get_available_list, c, pstate_list, pstate_count);
}

state_t mgt_imcfreq_get_current_list(ctx_t *c, pstate_t *pstate_list)
{
	preturn (ops.get_current_list, c, pstate_list);
}

state_t mgt_imcfreq_set_current_list(ctx_t *c, uint *index_list)
{
	preturn (ops.set_current_list, c, index_list);
}

state_t mgt_imcfreq_set_current(ctx_t *c, uint pstate_index, int socket)
{
	preturn (ops.set_current, c, pstate_index, socket);
}

state_t mgt_imcfreq_set_auto(ctx_t *c)
{
	preturn (ops.set_auto, c);
}

/** */
state_t mgt_imcfreq_get_current_min_list(ctx_t *c, pstate_t *ps_max_list, pstate_t *ps_min_list)
{
	preturn (ops.get_current_min_list, c, ps_max_list, ps_min_list);
}

state_t mgt_imcfreq_set_current_min_list(ctx_t *c, uint *id_max_list, uint *id_min_list)
{
	preturn (ops.set_current_min_list, c, id_max_list, id_min_list);
}

/** */
state_t mgt_imcfreq_data_alloc(pstate_t **pstate_list, uint **index_list)
{
	preturn (ops.data_alloc, pstate_list, index_list);
}

state_t mgt_imcfreq_data_print(pstate_t *pstate_list, uint pstate_count, int fd)
{
	preturn (ops.data_print, pstate_list, pstate_count, fd);
}

#if TEST
#include <daemon/local_api/eard_api.h>

state_t eards_connection()
{
	application_t my_dummy_app;
	my_dummy_app.job.id = 0;
	my_dummy_app.job.step_id = 0;
	return eards_connect(&my_dummy_app);
}

int main(int argc, char *arg[])
{
	pstate_t current_max[1024];
	pstate_t current_min[1024];
	uint setnew_max[1024];
	uint setnew_min[1024];
	uint devices;
	topology_t tp;
	state_t s;

	topology_init(&tp);

	eards_connection();

	if(state_fail(s = mgt_imcfreq_load(&tp, EARD))) {
		debug("Error during mgt_imcfreq_load: %s", state_msg);
	}

	return 0;
	
	uint api;
	mgt_imcfreq_get_api(&api);
	debug("API %d", api); 

	mgt_imcfreq_init(NULL);	
	
	mgt_imcfreq_count_devices(NULL, &devices);

	mgt_imcfreq_get_current_min_list(NULL, current_max, current_min);
	mgt_imcfreq_data_print(current_max, devices, fderr);
	mgt_imcfreq_data_print(current_min, devices, fderr);
	dprintf(fderr, "\n");

	setnew_max[0] = 2;
	setnew_max[1] = 2;
	setnew_min[0] = 10;
	setnew_min[1] = 10;

	mgt_imcfreq_set_current_min_list(NULL, setnew_max, setnew_min);

	sleep(1);
	
	mgt_imcfreq_get_current_min_list(NULL, current_max, current_min);
	mgt_imcfreq_data_print(current_max, devices, fderr);
	mgt_imcfreq_data_print(current_min, devices, fderr);
	dprintf(fderr, "\n");
	
	setnew_max[0] = 0;
	setnew_max[1] = 0;
	setnew_min[0] = 12;
	setnew_min[1] = 12;
	
	mgt_imcfreq_set_current_min_list(NULL, setnew_max, setnew_min);
	
	sleep(1);
	
	mgt_imcfreq_get_current_min_list(NULL, current_max, current_min);
	mgt_imcfreq_data_print(current_max, devices, fderr);
	mgt_imcfreq_data_print(current_min, devices, fderr);
	dprintf(fderr, "\n");

	return 0;
}
#endif
