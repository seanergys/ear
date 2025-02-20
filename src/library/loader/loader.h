/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#ifndef LIBRARY_LOADER_H
#define LIBRARY_LOADER_H

#include <common/output/verbose.h>
#include <common/config/config_env.h>

// These functions are intended to be used ONLY for the loader
int is_libear_loaded();

void set_libear_loaded();

#endif //LIBRARY_LOADER_H
