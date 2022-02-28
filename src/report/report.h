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

/** \defgroup report_api Report API
 * @{
 * This API is designed to load plugin libraries to report some information to EARD, to EARDBD,
 * to different databases and what the programmer wants, such as text files. You have to pass an
 * installation path and a list of library files sepparated by ':'. If installation path is NULL,
 * it will use the current path. If not, it will search in INSTALL_PATH/lib/plugins/report/ folder.
 * The library files have to include the '.so' extension.
 *
 * Props:
 *     - Thread safe: yes.
 *     - User mode: yes, if the plugin does user operations.
 */

#define EARGM_WARNINGS        1
#define PERIODIC_AGGREGATIONS 2

#include <unistd.h>

/** Short name for const char. */
typedef const char cchar;

/** This structure is used to pass information about the process calling your plugin function. 
 * The API functions receive a parameter of this type with the calling process information. */
typedef struct report_id_s
{
    int   local_rank;  /*!< Process node local rank. */
    int   global_rank; /*!< Process application rank. */
    int   master_rank; /*!< Indicates whether this process is the master on its node.  */
	pid_t pid;         /*!< The process PID. */
} report_id_t;

/** This structure is used to internally define the report API. It defines the available plugins symbols. */
typedef struct report_ops_s
{
	state_t (*report_init)             (report_id_t *id, cluster_conf_t *cconf);
	state_t (*report_dispose)          (report_id_t *id);
	state_t (*report_applications)     (report_id_t *id, application_t *apps, uint count);
	state_t (*report_loops)            (report_id_t *id, loop_t *loops, uint count);
	state_t (*report_events)           (report_id_t *id, ear_event_t *eves, uint count);
	state_t (*report_periodic_metrics) (report_id_t *id, periodic_metric_t *mets, uint count);
	state_t (*report_misc)             (report_id_t *id, uint type, cchar *data, uint count);
} report_ops_t;

/** Fills a report_id_t with process information. This function is used internally by EAR in the Library to fill process information before triggering report functions. */
state_t report_create_id(report_id_t *id, int local_rank, int global_rank, int master_rank);

/** Loads all report plugins located at \p `install_path`, defined in \p `libs`, which is a string with plugin
 * names separated with `:`. This functions is used internally by EAR to load the report plugin API. */
state_t report_load(const char *install_path, const char *libs);


/** The first API function called once the report plugin is loaded.
 * This function is designed to store report configuration (e.g., report filenames).
 * \param id The calling process information.
 * \param cconf The cluster configuration. this structure contains information about EAR installation. */
state_t report_init(report_id_t *id, cluster_conf_t *cconf);

/** This function is called by the EARD at the end of a job/step.
 * \param id The calling process information. */
state_t report_dispose(report_id_t *id);

/** This function is called at the end of a job/step, once the application metrics are computed.
 * \param id The calling process information.
 * \param apps An array of application signatures. For this EAR version, expect to receive only one.
 * \param count The number of elements in \p `apps` parameter. For this EAR version, expect to receive a 1. */
state_t report_applications(report_id_t *id, application_t *apps, uint count);

/** This function is reported for each loop signature once it is computed. It is called by the master process
 * with per node average metrics.
 * \param id The master process of the current node information.
 * \param loops An array of loops signatures in the current node of the job. For this EAR version, expect to receive only one.
 * \param count The number of elements in \p `loops` parameter. For this EAR version, expect to receive a 1.*/
state_t report_loops(report_id_t *id, loop_t *loops, uint count);

state_t report_events(report_id_t *id, ear_event_t *eves, uint count);

state_t report_periodic_metrics(report_id_t *id, periodic_metric_t *mets, uint count);

state_t report_misc(report_id_t *id, uint type, const char *data, uint count);

/**
 * @}
 */

#endif // REPORT_H
