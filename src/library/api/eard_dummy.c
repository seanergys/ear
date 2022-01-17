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

#define _GNU_SOURCE
#include <sched.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
//#define SHOW_DEBUGS 1
#include <common/config.h>
#include <common/config/config_env.h>
#include <common/output/verbose.h>
#include <common/output/debug.h>
#include <common/states.h>


#include <daemon/shared_configuration.h>

state_t eard_dummy_cluster_conf(char *ear_tmp, uint ID)
{
				char ser_cc_path[GENERIC_NAME];

				get_ser_cluster_conf_path(ear_tmp, ser_cc_path);
				return EAR_SUCCESS;
}

state_t eard_dummy_earl_settings(char *ear_tmp, uint ID)
{
				char system_conf_path[GENERIC_NAME];
				char resched_conf_path[GENERIC_NAME];

				get_settings_conf_path(ear_tmp, ID, system_conf_path);
				get_resched_path(ear_tmp, ID, resched_conf_path);
				return EAR_SUCCESS;

}

state_t eard_dummy_frequency_list(char *ear_tmp, uint ID)
{
				int ps, i;
				char frequencies_path[GENERIC_NAME];

				/*
					 ps = frequency_get_num_pstates();
					 int size = ps * sizeof(ulong);
					 frequencies = (ulong *) malloc(size);
					 memcpy(frequencies, frequency_get_freq_rank_list(), size);
					 */
				get_frequencies_path(ear_tmp, frequencies_path);
				/*
					 shared_frequencies = create_frequencies_shared_area(frequencies_path, frequencies, size);
					 for (i = 0; i < ps; i++) {
					 verbose(VEARD_INIT, "shared_frequencies[%d]=%lu", i, shared_frequencies[i]);
					 }
					 */
				return EAR_SUCCESS;
}


state_t eard_dummy_app_mgt(char *ear_tmp, uint ID)
{
				char app_path[GENERIC_NAME];

				get_app_mgt_path(ear_tmp, ID, app_path);
				return EAR_SUCCESS;
}

state_t eard_dummy_pc_app_info(char *ear_tmp, uint ID)
{
				char pc_path[GENERIC_NAME];

				get_pc_app_info_path(ear_tmp, ID, pc_path);	
				return EAR_SUCCESS;
}

state_t eard_dummy_nodemgr(char *ear_tmp, uint ID)
{
				return EAR_SUCCESS;
}

state_t eard_dummy_coefficients(char *ear_tmp, uint ID)
{
				char coeffs_path[GENERIC_NAME];

				get_coeffs_default_path(ear_tmp, coeffs_path);
				return EAR_SUCCESS;
}




/* This function is called when metrics are not yet initialized */
state_t create_eard_dummy_shared_regions(char *ear_tmp, uint ID)
{
				eard_dummy_earl_settings(ear_tmp, ID);
				eard_dummy_cluster_conf(ear_tmp, ID);
				eard_dummy_nodemgr(ear_tmp, ID);
				eard_dummy_frequency_list(ear_tmp, ID);
				eard_dummy_app_mgt(ear_tmp, ID);
				eard_dummy_pc_app_info(ear_tmp, ID);
				eard_dummy_coefficients(ear_tmp, ID);
				return EAR_SUCCESS;
}


