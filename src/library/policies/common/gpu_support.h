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


#ifndef _EARL_GPU_SUPPORT_H_
#define _EARL_GPU_SUPPORT_H_

#include <common/includes.h>
#include <library/policies/policy_ctx.h>

state_t policy_gpu_load(settings_conf_t *app_settings, polsym_t *psyms);
#endif

