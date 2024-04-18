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


/**
 * \file shared_configuration.h
 * \brief This file defines the API to create/attach/dettach/release the shared memory area between the EARD and the EARL.
 * It is used to communicate the EARLib updates remotelly requested.
*/


#ifndef _SHARED_PMON_H
#define _SHARED_PMON_H

#include <daemon/power_monitor_app.h>



/******************** NODE JOB LIST **********************/

/* This is the job of jobs the EARD is aware of */

state_t get_joblist_path(char *tmp, char *path);
uint  * create_joblist_shared_area(char *path, int *fd, uint *joblist, int joblist_elems);
uint  * attach_joblist_shared_area(char * path, int *fd, int *size);
void dettach_joblib_shared_area(int fd);
void joblist_shared_area_dispose(char *path, uint  *mem, int joblist_elems, int fd);

/* This is the pmapp are for a given job context */
state_t get_jobmon_path(char *tmp, uint ID, char *path);
powermon_app_t  * create_jobmon_shared_area(char *path, powermon_app_t  * pmapp, int *fd);
powermon_app_t  * attach_jobmon_shared_area(char * path, int *fd);
void dettach_jobmon_shared_area(int fd);
void jobmon_shared_area_dispose(char *path, powermon_app_t  *mem, int fd);


#endif
