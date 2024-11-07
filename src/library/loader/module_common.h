/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#ifndef _MODULE_COMMON_H
#define _MODULE_COMMON_H

void loader_get_path_libear(char **path_lib, char **hack_lib);

int load_libear_nonspec(char *lib_extension, char *syms[], char *libs[]);

// Depcrecated, clean.
int is_cuda_enabled();

#endif