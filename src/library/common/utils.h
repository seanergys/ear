/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#ifndef _LIB_UTILS_H
#define _LIB_UTILS_H

#include <common/states.h>
#include <daemon/shared_configuration.h>

/** Fills the string \p result_path with the must-to-be-used EARL plug-ins path.
 * The output string will be either the \p install_path or \p custom_path
 *	with "/plugins" string appended. The latter is selected if \ref authorized_user is true.
 *
 * \return EAR_ERROR An input argument is NULL.
 * \return EAR_SUCCESS Otherwise.
 */
state_t utils_create_plugin_path(char *result_path, char *install_path, char *custom_path, int authorized_user);

/** This function builds a complete plug-in path and checks for its existance.
 * If the calling user is authorized the function honors the HACK_EARL_INSTALL_PATH
 *	environment variable. If the wanted plug-in is not a regular file it tries to find
 * it on the official EARL install path, as if the hack was not set.
 *
 * \param[out] result_path The destination buffer.
 * \param[in] result_path_s The size of the buffer \p result_path points to.
 * \param[in] plug_name The shared object name of the plug-in.
 * \param[in] sconf A struct containing both the user type and the official EARL install path.
 *
 * \return EAR_ERROR Some input argument is NULL or the plug-in is not a regular file.
 */
state_t utils_complete_plugin_path(char *result_path, size_t result_path_s, char *plug_name, settings_conf_t *sconf);

/** Returns the maximum value between the batch scheduler provided number of nodes
 * of the active job (SLURM tested) and 1. It formally calls SCHED_STEP_NUM_NODES env var.
 */
int utils_get_sched_num_nodes();
#endif
