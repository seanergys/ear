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

#ifndef EAR_CONFIG_DEV_H
#define EAR_CONFIG_DEV_H
#include <common/config/config_install.h>


/***** DO NOT MODIFY THESE VARIABLES *******/
/* Sets to 0 MI statistics after signature computation*/
#define RESET_STATISTICS_AT_SIGNATURE 1

/**** These variables modifies features under development ****/

/*** EARL ***/
/* To be removed and set fixed in the code */
#define EARL_RESEARCH 1
/* Still under development */

/**** Deprecated. To be removed ****/
#define EAR_CPUPOWER	1
/* When set to 1 , creates a thread in EARD to support application queries apart
 *  *  * from EARL, do not set to 0 except for debug purposes */
#define APP_API_THREAD            1
/* When set to 1 , creates a thread in EARD for powermonitoring, do not set to 0
 *  *  * except for debug purposes */
#define POWERMON_THREAD           1
/* When set to 1 , creates a thread in EARD for external commands, do not set to
 *  *  * 0 except for debug purposes */
#define EXTERNAL_COMMANDS_THREAD        1

#define USE_LEARNING_APPS 1

/* For EAR validation */
//#define FAKE_ERROR_USE_DUMMY 1
//define FAKE_ERROR_EARD_NOT_CONNECTED 1
//#define FAKE_ERROR_ERROR_PATH 1


#endif //EAR_CONFIG_DEV_H
