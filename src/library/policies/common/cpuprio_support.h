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


#ifndef _EARL_CPUPRIO_SUPPORT_H
#define _EARL_CPUPRIO_SUPPORT_H


/** Encapsulates priority setup. */
void policy_setup_priority();

/** This function resets priority configuration for CPUs used by the job
 * in the case priority system is enabled by the user. */
void policy_reset_priority();


#endif
