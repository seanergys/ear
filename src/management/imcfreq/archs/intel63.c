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

#include <stdlib.h>
#include <metrics/common/msr.h>
#include <management/imcfreq/archs/intel63.h>

#define U_MSR_UNCORE_RATIO_LIMIT	0x000620
#define U_MSR_UNCORE_RL_MASK_MAX	0x00007F
#define U_MSR_UNCORE_RL_MASK_MIN	0x007F00
#define HUNDRED_MHZ                 100000LLU

static topology_t tp_static;
static pstate_t  *available_copy;
static pstate_t  *available_list;
static uint       available_count;
static ullong     max_khz;
static ullong     min_khz;

static state_t intel63_read(uint cpu, ullong *max, ullong *min)
{
	off_t address = U_MSR_UNCORE_RATIO_LIMIT;
	ullong aux = 0;
	state_t s;

	*max = 0;
	*min = 0;
	if ((s = msr_read(cpu, &aux, sizeof(uint64_t), address)) != EAR_SUCCESS) {
		return s;
	}
	*max = (aux & U_MSR_UNCORE_RL_MASK_MAX) >> 0;
	*min = (aux & U_MSR_UNCORE_RL_MASK_MIN) >> 8;
	debug("CPU%d: %llu - %llu (read)", cpu, *max, *min);
	*max = *max * HUNDRED_MHZ;
	*min = *min * HUNDRED_MHZ;

	return EAR_SUCCESS;
}

static state_t mgt_imcfreq_intel63_build_list()
{
	ullong max_khz_aux = 0;
	ullong min_khz_aux = 0;
	ullong aux = 0;
	state_t s;
	int i;

	// First scan, getting the maximum and the minimum.
	for (i = 0; i < tp_static.cpu_count; ++i) {
		// Opening
		if (state_fail(s = msr_open(tp_static.cpus[i].id))) {
			return s;
		}
		// Reading
		if (state_fail(s = intel63_read(tp_static.cpus[i].id, &max_khz_aux, &min_khz_aux))) {
			debug("Failed during IMC read: %s (%d)", state_msg, s);
		}
		// Saving general maximum and minimum
		if (max_khz == 0 || max_khz_aux > max_khz) {
			max_khz = max_khz_aux;
		}
		if ((min_khz == 0 || min_khz_aux < min_khz) && min_khz_aux != 0) {
			min_khz = min_khz_aux;
		}
	}
	if (max_khz == 0 || min_khz == 0) {
		return_msg(EAR_ERROR, "Can't build a frequency list");
	}
	// Hardcoding the values
	max_khz = 24 * HUNDRED_MHZ;
	min_khz = 12 * HUNDRED_MHZ;
	// Building the list
	available_count = (max_khz / HUNDRED_MHZ) - (min_khz / HUNDRED_MHZ) + 1;
	available_list = calloc(available_count, sizeof(pstate_t));
	available_copy = calloc(available_count, sizeof(pstate_t));

	for (i = 0, aux = 0LLU; i < available_count; ++i, aux += HUNDRED_MHZ) {
		available_list[i].idx = i;
		available_list[i].khz = max_khz - aux;
		debug("IMCFREQ %d: %llu", i, available_list[i].khz);
	}

	return EAR_SUCCESS;
}

state_t mgt_imcfreq_intel63_load(topology_t *tp, imcfreq_ops_t *ops)
{
	state_t s;
	if (max_khz != 0) {
		return EAR_SUCCESS;
	}
	if (tp->vendor != VENDOR_INTEL || tp->model < MODEL_HASWELL_X) {
		return_msg(EAR_ERROR, Generr.api_incompatible);
	}
	if (state_fail(s = msr_test(tp))) {
		return s;
	}
	if (state_fail(s = topology_select(tp, &tp_static, TPSelect.socket, TPGroup.merge, 0))) {
		return s;
	}
	if (state_fail(s = mgt_imcfreq_intel63_build_list())) {
		return s;
	}
	replace_ops(ops->init,               mgt_imcfreq_intel63_init);
	replace_ops(ops->dispose,            mgt_imcfreq_intel63_dispose);
	replace_ops(ops->get_available_list, mgt_imcfreq_intel63_get_available_list);
	replace_ops(ops->get_current_list,   mgt_imcfreq_intel63_get_current_list);
	replace_ops(ops->set_current_list,   mgt_imcfreq_intel63_set_current_list);
	replace_ops(ops->set_current,        mgt_imcfreq_intel63_set_current);
	replace_ops(ops->set_auto,           mgt_imcfreq_intel63_set_auto);
	replace_ops(ops->get_current_min_list, mgt_imcfreq_intel63_get_current_min_list);
	replace_ops(ops->set_current_min_list, mgt_imcfreq_intel63_set_current_min_list);
	return s;
}

state_t mgt_imcfreq_intel63_init(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t mgt_imcfreq_intel63_dispose(ctx_t *c)
{
    c->context = NULL;
    return EAR_SUCCESS;
}

state_t mgt_imcfreq_intel63_get_available_list(ctx_t *c, const pstate_t **list, uint *count)
{
	uint i;
	for (i = 0; i < available_count; ++i) {
		available_copy[i].khz = available_list[i].khz;
		available_copy[i].idx = i;
	}
	*list = available_copy;
	if (count != NULL) {
		*count = available_count;
	}
	return EAR_SUCCESS;
}

static uint khz_to_idx(ullong khz)
{
	int i;
	for (i = 0; i < available_count; ++i) {
		if (available_list[i].khz <= khz) {
			return i;
		}
	}
	return 0;
}

state_t get_current(ctx_t *c, pstate_t *max_list, pstate_t *min_list)
{
	ullong max_khz_aux = 0;
	ullong min_khz_aux = 0;
	state_t s;
	int i;

	for (i = 0; i < tp_static.cpu_count; ++i) {
		if (state_fail(s = intel63_read(tp_static.cpus[i].id, &max_khz_aux, &min_khz_aux))) {
			return s;
		}
		if (max_list != NULL) {	
			max_list[i].idx = khz_to_idx(max_khz_aux);
			max_list[i].khz = available_list[max_list[i].idx].khz;
		}
		if (min_list != NULL) {
			min_list[i].idx = khz_to_idx(min_khz_aux);
			min_list[i].khz = available_list[min_list[i].idx].khz;
		}
	}
	return EAR_SUCCESS;
}

state_t mgt_imcfreq_intel63_get_current_list(ctx_t *c, pstate_t *pstate_list)
{
	return get_current(c, pstate_list, NULL);
}

static state_t set_current(ctx_t *c, uint *max_list, uint *min_list)
{
	off_t address = U_MSR_UNCORE_RATIO_LIMIT;
	ullong set0 = 0;
	ullong set1 = 0;
	ullong aux_max;
	ullong aux_min;
	state_t r;
	int i;

	for (i = 0; i < tp_static.cpu_count; ++i) {
		debug("INVALID CPU%d: %d - %d (write)", tp_static.cpus[i].id, max_list[i], min_list[i]);
		aux_max = max_khz / HUNDRED_MHZ;
		aux_min = min_khz / HUNDRED_MHZ;
		// If NULL we are writting the max default value
		if (max_list != NULL) {
			aux_max = available_list[max_list[i]].khz / HUNDRED_MHZ;
		}
		if (min_list != NULL) {
			aux_min = available_list[min_list[i]].khz / HUNDRED_MHZ;
		}
		debug("CPU%d: %llu - %llu (write)", tp_static.cpus[i].id, aux_max, aux_min);
		set0 = (aux_min << 8) & U_MSR_UNCORE_RL_MASK_MIN;
		set1 = (aux_max << 0) & U_MSR_UNCORE_RL_MASK_MAX;
		set0 = set0 | set1;
		//
		if ((r = msr_write(tp_static.cpus[i].id, &set0, sizeof(ullong), address)) != EAR_SUCCESS) {
			return r;
		}
	}

	return EAR_SUCCESS;
}

state_t mgt_imcfreq_intel63_set_current_list(ctx_t *c, uint *index_list)
{
	return set_current(c, index_list, NULL);
}

state_t mgt_imcfreq_intel63_set_current(ctx_t *c, uint pstate_index, int socket)
{
    uint buffer[128];
	int i;
	for (i = 0; i < tp_static.cpu_count; ++i) {
		buffer[i] = ps_nothing;
		if (i != socket && socket != all_sockets) {
			continue;
		}
		buffer[i] = pstate_index;
	}
	return set_current(c, buffer, NULL);
}

state_t mgt_imcfreq_intel63_set_auto(ctx_t *c)
{
	return mgt_imcfreq_intel63_set_current(c, ps_auto, all_sockets);
}

state_t mgt_imcfreq_intel63_get_current_min_list(ctx_t *c, pstate_t *ps_max_list, pstate_t *ps_min_list)
{
	return get_current(c, ps_max_list, ps_min_list);
}

state_t mgt_imcfreq_intel63_set_current_min_list(ctx_t *c, uint *id_max_list, uint *id_min_list)
{
	return set_current(c, id_max_list, id_min_list);
}
