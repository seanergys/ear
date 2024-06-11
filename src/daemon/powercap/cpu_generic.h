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

#ifndef CPU_GENERIC_POWERCAP_H
#define CPU_GENERIC_POWERCAP_H
#include <common/states.h>
#include <common/system/monitor.h>


state_t cpu_pc_disable_powercap_policy(uint pid);
state_t cpu_pc_disable_powercap_policies();
state_t cpu_pc_enable_powercap_policies();
state_t cpu_pc_set_powercap_value(uint pid,uint domain,uint limit);
state_t cpu_pc_get_powercap_value(uint pid,uint *powercap);
uint cpu_pc_is_powercap_policy_enabled(uint pid);
void cpu_pc_print_powercap_value(int fd);
void cpu_pc_powercap_to_str(char *b);


state_t cpu_pc_enable();
state_t cpu_pc_disable();


void cpu_pc_set_status(uint status);
uint cpu_pc_get_powercap_stragetgy();
void cpu_pc_set_pc_mode(uint mode);



#endif
