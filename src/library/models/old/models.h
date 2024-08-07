/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#ifndef EAR_MODELS_H_
#define EAR_MODELS_H_
#warning Deprecated header file. You must use the new Energy model API at library/models/energy_model.h

#include <common/environment.h>
#include <daemon/shared_configuration.h>
#include <common/hardware/architecture.h>

state_t init_power_models(uint user_type,conf_install_t *data,architecture_t *myarch);


#endif
