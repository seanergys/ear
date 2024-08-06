/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/


#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common/config.h>
#include <common/states.h>
#include <common/output/verbose.h>
#include <common/hardware/architecture.h>
#include <common/types/projection.h>
#include <common/types/application.h>
#include <library/common/externs.h>
#include <library/models/models.h>

/* This variable enables/disables the utilization of energy models in power policies */
extern uint use_energy_models;

state_t init_power_models(uint user_type,conf_install_t *data,architecture_t *arch_desc)
{
	state_t st;
	st = projections_init(user_type,data,arch_desc);
	debug("Projections for %d pstates",arch_desc->pstates);
	projection_create(arch_desc->pstates);
	projection_reset(arch_desc->pstates);
	if (projections_available() == EAR_ERROR) use_energy_models = 0;
	return st;
}



