/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#ifndef METRICS_COMMON_HSMP_H
#define METRICS_COMMON_HSMP_H

#include <metrics/common/hsmp_driver.h>

// It opens HSMP by various methods: contacting amd_hsmp driver, loading e_smi library
// or opening PCIs directly (in that order).
state_t hsmp_open(topology_t *tp);

state_t hsmp_close();

state_t hsmp_send(int socket, uint function, uint *args, uint *reps);

#endif
