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

#ifndef REPORT_H
#define REPORT_H

#include <common/states.h>
#include <common/types/types.h>
#include <common/types/configuration/cluster_conf.h>

// The API
//
// This API is designed to load plugin libraries to report some information to
// EARD, to EARDBD, to different databases and what the programmer wants, such
// as text files. You have to pass an installation path and a list of library
// files sepparated by ':'. If installation path is NULL, it will use the
// current path. If not, it will search in INSTALL_PATH/lib/plugins/report/
// folder. The library files have to include the '.so' extension.
//
// Props:
//	- Thread safe: yes.
//	- User mode: yes, if the plugin does user operations.
//------------------------------------------------------------------------------

#define EARGM_WARNINGS 1

typedef const char cchar;
typedef struct report_id_s
{
    int local_rank;
    int global_rank;
    int master_rank;
} report_id_t;

typedef struct report_ops_s
{
	state_t (*report_init)             (report_id_t *id,cluster_conf_t *cconf);
	state_t (*report_dispose)          (report_id_t *id);
	state_t (*report_applications)     (report_id_t *id,application_t *apps, uint count);
	state_t (*report_loops)            (report_id_t *id,loop_t *loops, uint count);
	state_t (*report_events)           (report_id_t *id,ear_event_t *eves, uint count);
	state_t (*report_periodic_metrics) (report_id_t *id,periodic_metric_t *mets, uint count);
	state_t (*report_misc)             (report_id_t *id,uint type, cchar *data, uint count);
} report_ops_t;

state_t report_load(const char *install_path, const char *libs);

state_t report_init(report_id_t *id,cluster_conf_t *cconf);
state_t report_dispose(report_id_t *id);
state_t report_applications(report_id_t *id,application_t *apps, uint count);

state_t report_loops(report_id_t *id,loop_t *loops, uint count);

state_t report_events(report_id_t *id,ear_event_t *eves, uint count);

state_t report_periodic_metrics(report_id_t *id,periodic_metric_t *mets, uint count);

state_t report_misc(report_id_t *id,uint type, const char *data, uint count);

#endif //REPORT_H
