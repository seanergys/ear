/*
 *
 * This program is part of the EAR software.
 *
 * EAR provides a dynamic, transparent and ligth-weigth solution for
 * Energy management. It has been developed in the context of the
 * Barcelona Supercomputing Center (BSC)&Lenovo Collaboration project.
 *
 * 
 * BSC Contact   mailto:ear-support@bsc.es
 * 
 *
 * EAR is an open source software, and it is licensed under both the BSD-3 license
 * and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
 * and COPYING.EPL files.
 */

#ifndef _LIB_UTILS_H
#define _LIB_UTILS_H

#include <common/states.h>

/** Fills the string \ref result_path with the must-to-be-used EARL plug-ins path.
 * The output string will be either the \ref install_path or \ref custom_path with "/plugins" string appended. The latter is selected if \ref authorized_user is true.
 * \return EAR_ERROR An input argument is NULL.
 * \return EAR_SUCCESS Otherwise. */
state_t utils_create_plugin_path(char *result_path, char *install_path, char *custom_path, int authorized_user);

/** Returns the maximum value between the batch scheduler provided number of nodes of the active job (SLURM tested) and 1. It formally calls SCHED_STEP_NUM_NODES env var.*/
int utils_get_sched_num_nodes();
#endif
