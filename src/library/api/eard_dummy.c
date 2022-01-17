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
#include <library/common/verbose_lib.h>
#include <library/common/global_comm.h>
#include <common/output/debug.h>
#include <common/states.h>
#include <common/types/configuration/cluster_conf.h>
#include <common/types/configuration/cluster_conf_generic.h>


#include <daemon/shared_configuration.h>

static char path[GENERIC_NAME];
static cluster_conf_t dummy_cc;
extern masters_info_t masters_info;


state_t eard_dummy_cluster_conf(char *ear_tmp, uint ID)
{
				char *dummy_ser_cc,  *local_dummy_ser_cc;
				size_t dummy_ser_cc_size;

				/* Init dummy_cc */
				if (get_ear_conf_path(path) != EAR_ERROR){
					read_cluster_conf(path, &dummy_cc);
				}else{
					memset(&dummy_cc, 0 , sizeof(cluster_conf_t));
					set_ear_conf_default(&dummy_cc);
				}

				/* serialization */
				serialize_cluster_conf(&dummy_cc, &dummy_ser_cc, &dummy_ser_cc_size);
				get_ser_cluster_conf_path(ear_tmp, path);
				local_dummy_ser_cc = create_ser_cluster_conf_shared_area(path, dummy_ser_cc, dummy_ser_cc_size);


				verbose_master(2, "Dummy cluster_conf done");

				return EAR_SUCCESS;
}

state_t eard_dummy_earl_settings(char *ear_tmp, uint ID)
{
				settings_conf_t * dummy_sett;
				resched_t       * dummy_resch;

				get_settings_conf_path(ear_tmp, ID, path);
				dummy_sett = create_settings_conf_shared_area(path);
				

				verbose_master(2, "Dummy earl settings done");

				get_resched_path(ear_tmp, ID, path);
				dummy_resch = create_resched_shared_area(path);

				verbose_master(2, "Dummy resched are done");

				return EAR_SUCCESS;

}

state_t eard_dummy_app_mgt(char *ear_tmp, uint ID)
{
				app_mgt_t *dummy;

				get_app_mgt_path(ear_tmp, ID, path);
				dummy = create_app_mgt_shared_area(path);

				verbose_master(2, "Dummy app_mgt area done");

				return EAR_SUCCESS;
}

state_t eard_dummy_pc_app_info(char *ear_tmp, uint ID)
{
				pc_app_info_t *dummy;

				get_pc_app_info_path(ear_tmp, ID, path);	
				dummy = create_pc_app_info_shared_area(path);

				verbose_master(2, "Dummy powercap are done");
	
				return EAR_SUCCESS;
}

state_t eard_dummy_nodemgr(char *ear_tmp, uint ID)
{

				verbose_master(2, "Dummy node mgr area done");
				return EAR_SUCCESS;
}

state_t eard_dummy_coefficients(char *ear_tmp, uint ID)
{
				coefficient_t *coeffs;

				/* How many coefficients ? Depends on frequencies */
				int size = sizeof(coefficient_t);
				coeffs = calloc(1, size);
				get_coeffs_default_path(ear_tmp, path);
				create_coeffs_default_shared_area(path, coeffs, size);

				verbose_master(2, "Dummy coefficients area done");
				return EAR_SUCCESS;
}




/* This function is called when metrics are not yet initialized */
state_t create_eard_dummy_shared_regions(char *ear_tmp, uint ID)
{
				eard_dummy_cluster_conf(ear_tmp, ID);
				eard_dummy_earl_settings(ear_tmp, ID);
				eard_dummy_nodemgr(ear_tmp, ID);
//				eard_dummy_frequency_list(ear_tmp, ID);
				eard_dummy_app_mgt(ear_tmp, ID);
				eard_dummy_pc_app_info(ear_tmp, ID);
				eard_dummy_coefficients(ear_tmp, ID);

				verbose_master(2,"All the dummy regions ready");
				return EAR_SUCCESS;
}


