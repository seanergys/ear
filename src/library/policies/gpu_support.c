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
//#define SHOW_DEBUGS 1
#include <common/includes.h>
#include <common/config/config_env.h>
#include <common/system/symplug.h>
#include <library/policies/policy_ctx.h>
#include <library/common/externs.h>
#include <library/common/verbose_lib.h>
#if USE_GPUS
#include <library/metrics/gpu.h>
#endif


extern const char     *polsyms_nam[];
state_t policy_gpu_load(settings_conf_t *app_settings,polsym_t *psyms)
{
	char *obj_path;
	int polsyms_n = 14;
	void    *polsyms_obj = NULL;
  char basic_gpu_path[SZ_PATH_INCOMPLETE];
  char pgpu_name[64];
  conf_install_t *data=&app_settings->installation;
  state_t ret;

  char *ins_path = getenv(SCHED_EARL_INSTALL_PATH);
  char use_path[SZ_PATH_INCOMPLETE];


	if (masters_info.my_master_rank >= 0){
#if USE_GPUS
  	obj_path = getenv(SCHED_EAR_GPU_POWER_POLICY);
#if SHOW_DEBUGS
    if (obj_path!=NULL){ 
    	debug("%s = %s",SCHED_EAR_GPU_POWER_POLICY,obj_path);
    }else{
      debug("%s undefined",SCHED_EAR_GPU_POWER_POLICY);
    }
#endif
#if GPU_OPT
    sprintf(pgpu_name,"gpu_%s.so",app_settings->policy_name);
#else
    sprintf(pgpu_name,"gpu_monitoring.so");
#endif
    if (((obj_path==NULL) && (ins_path == NULL)) || (app_settings->user_type != AUTHORIZED)){
    	sprintf(basic_gpu_path,"%s/policies/%s",data->dir_plug,pgpu_name);
      obj_path=basic_gpu_path;
    }else{
      if (obj_path == NULL){
      	sprintf(basic_gpu_path,"%s/plugins/policies/%s",ins_path,pgpu_name);
      	obj_path=basic_gpu_path;
      }
    }
    verbose_master(2,"Loading GPU policy %s",obj_path);
		if (symplug_open(obj_path, (void **)psyms, polsyms_nam, polsyms_n) != EAR_SUCCESS){
			verbose_master(2,"Error loading file %s",obj_path);
			return EAR_ERROR;
		}
#endif
  }
	return EAR_SUCCESS;
}

state_t policy_gpu_init_ctx(polctx_t *c)
{
	uint policy_gpu_model;
	#if USE_GPUS
	c->gpu_mgt_ctx_on=0;
	if (gpu_lib_load(c->app) != EAR_SUCCESS){
    error("gpu_lib_load in init_power_policy");
		return EAR_ERROR;
  }else{
  	if (gpu_lib_init(&c->gpu_mgt_ctx) == EAR_SUCCESS){
    	c->gpu_mgt_ctx_on=1;
      verbose_master(2,"gpu_lib_init success");
      gpu_lib_model(&c->gpu_mgt_ctx,&policy_gpu_model);
      if (policy_gpu_model == MODEL_DUMMY){ 
      	debug("Setting policy num gpus to 0 because DUMMY");
      	c->num_gpus = 0;
      }else{
      	gpu_lib_count(&c->num_gpus);
      }
      verbose_master(2,"Num gpus detected in policy_load %u",c->num_gpus);	
    }else{
      error("gpu_lib_init");
			return EAR_ERROR;
    }
  }
	#endif
	return EAR_SUCCESS;
}

