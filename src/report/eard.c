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

#include <common/states.h>
#include <common/types/types.h>
#include <common/types/configuration/cluster_conf.h>
#include <common/output/verbose.h>
#include <daemon/local_api/eard_api.h>
#include <report/report.h>

static uint must_report;
state_t report_init(report_id_t *id,cluster_conf_t *cconf)
{
	debug("eard report_init");
	if (id->master_rank >= 0) must_report = 1;
	return EAR_SUCCESS;
}

state_t report_applications(report_id_t *id,application_t *apps, uint count)
{
	int i;
	debug("eard report_applications");
	if (!must_report) return EAR_SUCCESS;
	if (!eards_connected()) return EAR_ERROR;
	if ((apps == NULL) || (count == 0)) return EAR_SUCCESS;
	for (i=0;i<count;i++){
		eards_write_app_signature(&apps[i]);
	}
	return EAR_SUCCESS;
}

state_t report_loops(report_id_t *id,loop_t *loops, uint count)
{
	int i;
	debug("eard report_loops");
	if (!must_report) return EAR_SUCCESS;
	if (!eards_connected()) return EAR_ERROR;
	if ((loops == NULL) || (count == 0)) return EAR_SUCCESS;
	for (i=0;i<count;i++){
		eards_write_loop_signature(&loops[i]);
	}
	return EAR_SUCCESS;
}
