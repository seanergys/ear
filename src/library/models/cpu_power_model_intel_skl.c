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

#define SHOW_DEBUGS 1
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common/config.h>
#include <common/states.h>
#include <common/output/debug.h>
#include <common/hardware/architecture.h>
#include <common/types/configuration/cluster_conf.h>
#include <daemon/shared_configuration.h>
#include <library/common/library_shared_data.h>

#define IPC_COEFF (double)11.40897054
#define GBS_COEFF (double)0.601652592
#define VPI_COEFF (double)28.21247619
#define F_COEFF   (double)9.26892E-05
#define INTERCEPT (double)-28.85109789

typedef struct intel_skl{
	double ipc;
	double gbs;	
	double vpi;
	double f;
	double inter;
}intel_skl_t;

static intel_skl_t my_node_coeffs;
static uint arch_model_loaded = 0;

state_t cpu_power_models_init_arch(settings_conf_t *libconf, conf_install_t *conf, architecture_t *arch)
{
	debug("cpu_power_models_init_arch. Using tag %s etc %s", libconf->tag, conf->dir_conf);
	char model_path[MAX_PATH_SIZE];
	sprintf(model_path,"%s/ear/coeffs/island%d/coeffs.cpumodel.%s", conf->dir_conf,libconf->island, libconf->tag);
	debug("cpu_power_models_init_arch coeffs path %s", model_path);

	my_node_coeffs.ipc 		= IPC_COEFF;
	my_node_coeffs.gbs 		=	GBS_COEFF;
	my_node_coeffs.vpi 		=	VPI_COEFF;
	my_node_coeffs.f   		=	F_COEFF;
	my_node_coeffs.inter 	=	INTERCEPT;

	arch_model_loaded = 1;	
	
	return EAR_SUCCESS;
}

state_t cpu_power_model_status_arch()
{
	debug("cpu_power_model_status_arch");
	if (!arch_model_loaded) return EAR_ERROR;
	return EAR_SUCCESS;
}

state_t cpu_power_model_project_arch(lib_shared_data_t *data,shsignature_t *sig, node_mgr_sh_data_t *nmgr)
{
	debug("cpu_power_model_project_arch");
	if (!arch_model_loaded) return EAR_ERROR;
	return EAR_SUCCESS;
}



