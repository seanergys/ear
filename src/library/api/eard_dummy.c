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
#include <daemon/local_api/node_mgr.h>


#include <daemon/shared_configuration.h>

static char path[GENERIC_NAME];
static cluster_conf_t dummy_cc;
extern masters_info_t masters_info;
static my_node_conf_t *dummy_node_conf;
static char nodename[MAX_PATH_SIZE];
static ear_njob_t *dummy_nodemgr;

#define check_null(p, str, arg) \
	if (p == NULL){ \
		verbose_master(2, str, arg);\
		return EAR_ERROR;\
	}

void create_dummy_path_lock(char *ear_tmp, uint ID, char *dummy_areas_path, uint size)
{
	snprintf(dummy_areas_path, size, "%s/.dummy_areas_lock.%u", ear_tmp, ID);
}

state_t eard_dummy_cluster_conf(char *ear_tmp, uint ID)
{
				char *dummy_ser_cc,  *local_dummy_ser_cc;
				size_t dummy_ser_cc_size;

				/* Init dummy_cc */
				if (get_ear_conf_path(path) != EAR_ERROR){
					verbose_master(2, "Reading cluster_conf");
					read_cluster_conf(path, &dummy_cc);
				}else{
					memset(&dummy_cc, 0 , sizeof(cluster_conf_t));
					set_ear_conf_default(&dummy_cc);
				}

				/* serialization */
				serialize_cluster_conf(&dummy_cc, &dummy_ser_cc, &dummy_ser_cc_size);
				get_ser_cluster_conf_path(ear_tmp, path);
				local_dummy_ser_cc = create_ser_cluster_conf_shared_area(path, dummy_ser_cc, dummy_ser_cc_size);

				check_null(local_dummy_ser_cc, "Error serializing cluster_conf in path %s", " ");

				if (gethostname(nodename, sizeof(nodename)) < 0) {
        	verbose_master(0, "Error getting node name (%s)", strerror(errno));
        }
        strtok(nodename, ".");
				verbose_master(2, "Looking for node conf. Nodename %s", nodename);
				dummy_node_conf = get_my_node_conf(&dummy_cc, nodename);
				check_null(dummy_node_conf, "Invalid configuration file for node %s", nodename);

				verbose_master(2, "Dummy cluster_conf done");

				return EAR_SUCCESS;
}

state_t eard_dummy_earl_settings(char *ear_tmp, uint ID)
{
				settings_conf_t * dummy_sett;
				resched_t       * dummy_resch;
  			policy_conf_t   * my_policy;

				verbose_master(2, "looking for policy %d", dummy_cc.default_policy);	
  			my_policy = get_my_policy_conf(dummy_node_conf,dummy_cc.default_policy);
				check_null(my_policy, "My policy returns NULL for policy %d", dummy_cc.default_policy);

				get_settings_conf_path(ear_tmp, ID, path);
				dummy_sett = create_settings_conf_shared_area(path);
				check_null(dummy_sett, "create_settings_conf_shared_area returns NULL for path %s", path);

				memset(dummy_sett, 0, sizeof(settings_conf_t));

  			dummy_sett->id             = ID;
				dummy_sett->user_type	  	 = AUTHORIZED;
				dummy_sett->lib_enabled = 1;
				dummy_sett->policy           = dummy_cc.default_policy;
				dummy_sett->def_freq         = 2600000;
  			dummy_sett->def_p_state      = my_policy->p_state;
  			dummy_sett->max_freq         = 2600000;
  			strncpy(dummy_sett->policy_name, my_policy->name, 64);
  			memcpy(dummy_sett->settings, my_policy->settings, sizeof(double)*MAX_POLICY_SETTINGS);
  			dummy_sett->min_sig_power    = dummy_node_conf->min_sig_power;
  			dummy_sett->max_sig_power    = dummy_node_conf->max_sig_power;
  			dummy_sett->max_power_cap    = dummy_node_conf->powercap;
  			dummy_sett->report_loops     = dummy_cc.database.report_loops;
  			dummy_sett->max_avx512_freq  = dummy_node_conf->max_avx512_freq;
  			dummy_sett->max_avx2_freq    = dummy_node_conf->max_avx2_freq;
				dummy_sett->pc_opt.current_pc     = POWER_CAP_UNLIMITED;
  			memcpy(&dummy_sett->installation , &dummy_cc.install,sizeof(conf_install_t));
  			copy_ear_lib_conf(&dummy_sett->lib_info , &dummy_cc.earlib);
	

				verbose_master(2, "Dummy earl settings done");

				get_resched_path(ear_tmp, ID, path);
				dummy_resch = create_resched_shared_area(path);

				check_null(dummy_resch, "Resched are returns NULL for path %s", path);

				dummy_resch->force_rescheduling = 0;

				verbose_master(2, "Dummy resched are done");

				return EAR_SUCCESS;

}

state_t eard_dummy_app_mgt(char *ear_tmp, uint ID)
{
				app_mgt_t *dummy;

				get_app_mgt_path(ear_tmp, ID, path);
				dummy = create_app_mgt_shared_area(path);
				check_null(dummy_nodemgr, "Error creating app_mgr in path %s ", path);

				verbose_master(2, "Dummy app_mgt area done");

				return EAR_SUCCESS;
}

state_t eard_dummy_pc_app_info(char *ear_tmp, uint ID)
{
				pc_app_info_t *dummy;

				get_pc_app_info_path(ear_tmp, ID, path);	
				dummy = create_pc_app_info_shared_area(path);
				check_null(dummy, "Error creating pc area in path %s", path);

				memset(dummy, 0 , sizeof(pc_app_info_t));
				dummy->cpu_mode = PC_POWER;
				#if USE_GPUS
				dummy->gpu_mode = PC_POWER;
				#endif

				verbose_master(2, "Dummy powercap are done");
	
				return EAR_SUCCESS;
}

state_t eard_dummy_nodemgr(char *ear_tmp, uint ID)
{
				nodemgr_server_init(ear_tmp, &dummy_nodemgr);
				check_null(dummy_nodemgr, "Error creating dummy node_mgr %s", " ");				

				verbose_master(2, "Dummy node mgr area done");
				return EAR_SUCCESS;
}

state_t eard_dummy_coefficients(char *ear_tmp, uint ID)
{
				coefficient_t *coeffs, *dummy_coeff;

				/* How many coefficients ? Depends on frequencies */
				int size = sizeof(coefficient_t);
				coeffs = calloc(1, size);
				check_null(coeffs, "Error allocating memory for coefficients %s", " ");

				get_coeffs_default_path(ear_tmp, path);
				dummy_coeff = create_coeffs_default_shared_area(path, coeffs, size);
				check_null(dummy_coeff, "Error creating coefficients for path %s", path);

				verbose_master(2, "Dummy coefficients area done");
				return EAR_SUCCESS;
}

state_t eard_dummy_frequency_list(char *ear_tmp, uint ID)
{
				ulong * frequencies, *dummy_frequencies;
				int ps, i;
				ps = 1;
				int size = ps * sizeof(ulong);
				frequencies = (ulong *) malloc(size);
				check_null(frequencies, "Error allocating memory for frequencies %s", " ");	
				frequencies[0] = 2600000;

				get_frequencies_path(ear_tmp, path);
				dummy_frequencies = create_frequencies_shared_area(path, frequencies, size);
				check_null(dummy_frequencies, "Error creating frequencies area at path %s", path);	

				return EAR_SUCCESS;
	
}


/* This function is called when metrics are not yet initialized */
state_t create_eard_dummy_shared_regions(char *ear_tmp, uint ID)
{
				verbose(2,"Creating dummy areas in %s and ID %u", ear_tmp, ID);
				eard_dummy_cluster_conf(ear_tmp, ID);
				eard_dummy_earl_settings(ear_tmp, ID);
				eard_dummy_nodemgr(ear_tmp, ID);
				eard_dummy_frequency_list(ear_tmp, ID);
				eard_dummy_app_mgt(ear_tmp, ID);
				eard_dummy_pc_app_info(ear_tmp, ID);
				eard_dummy_coefficients(ear_tmp, ID);

				verbose_master(2,"All the dummy regions ready");
				return EAR_SUCCESS;
}


