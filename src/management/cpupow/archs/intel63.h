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

#ifndef MANAGEMENT_CPUPOW_INTEL63_H
#define MANAGEMENT_CPUPOW_INTEL63_H

#include <management/cpupow/cpupow.h>

void mgt_cpupow_intel63_load(topology_t *tp, mgt_cpupow_ops_t *ops);

void mgt_cpupow_intel63_get_api(uint *api);

state_t mgt_cpupow_intel63_count_devices(int type, uint *devs_count);

state_t mgt_cpupow_intel63_get_powercap(int type, uint *watts);

state_t mgt_cpupow_intel63_set_powercap(int type, uint *watts);

state_t mgt_cpupow_intel63_get_tdp(uint *watts);

#endif
