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
* EAR is an open source software, and it is licensed under both the BSD-3 license
* and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
* and COPYING.EPL files.
*/




#ifndef _SHARED_TYPES_H
#define _SHARED_TYPES_H


#include <common/config.h>
#include <common/types/generic.h>
#include <common/types/configuration/cluster_conf.h>
#include <common/types/coefficient.h>

#include <daemon/powercap/powercap.h>
#include <daemon/app_mgt.h>
#include <common/types/pc_app_info.h>



typedef struct services_conf
{
	eard_conf_t		eard;
	eargm_conf_t	eargmd;
	db_conf_t			db;
	eardb_conf_t	eardbd;
	char					net_ext[ID_SIZE];
} services_conf_t;

typedef struct settings_conf
{
	uint								id;
	uint 	            	user_type;
	uint 	            	learning;
	uint 	            	lib_enabled;
	uint 	            	policy;
	ulong 	          	max_freq;
	ulong	            	def_freq;
	uint	            	def_p_state;
	double 	            settings[MAX_POLICY_SETTINGS];
  char                policy_name[64];
	earlib_conf_t       lib_info;
	double              min_sig_power;
	double              max_sig_power;
	double              max_power_cap;
	uint                report_loops;
	conf_install_t      installation;
	ulong               max_avx512_freq;
  ulong               max_avx2_freq;
	node_powercap_opt_t pc_opt;
	uint                island;   
	char                tag[GENERIC_NAME];
  int                 cpu_max_pstate;
  int                 imc_max_pstate;
} settings_conf_t;

typedef struct resched
{
	int	force_rescheduling;
} resched_t;


#include <daemon/power_monitor_app.h>

#endif
