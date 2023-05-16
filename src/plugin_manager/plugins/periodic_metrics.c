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

#define SHOW_DEBUGS 1

#include <string.h>
#include <report/report.h>
#include <common/output/debug.h>
#include <plugin_manager/plugins/metrics.h>

static periodic_metric_t  cs; // Current sample
static my_node_conf_t    *nc; // Node conf
static metrics_read_t     mr1;
static metrics_read_t     mr2;
static metrics_diff_t     mrD;
static cchar             *self_tag = "periodic-metrics";
// Reporting things
static report_id_t       *rid;
static ulong              jid;
static ulong              sid;
static ulong              mpi;

declr_up_get_tag ()
{
    *tag = self_tag;
}

declr_up_init()
{
    init_periodic_metric(&cs); //eard_power_monitoring()
    metrics_data_alloc(&mr1, &mr2, &mrD);
}

declr_up_callback()
{
    mets_t *mets = (mets_t *) data;

    debug("'%s': received from '%s' data '%p'", self_tag, tag, data);
    if (strcmp(tag, "metrics") == 0) {
        metrics_data_copy(&mr2, &mets->mr);
    }
    if (strcmp(tag, "periodic-metrics") == 0) {
        metrics_data_diff(&mr2, &mr1, &mrD);
        // Fill the current sample
        cs.start_time  = mr1.time.tv_sec;
        cs.end_time    = mr2.time.tv_sec;
        cs.avg_f       = mrD.cpu_avrg;
        cs.temp        = mrD.tmp_avrg;
        cs.DRAM_energy = mrD.pow_dram;
        cs.PCK_energy  = mrD.pow_pack;
        cs.DC_energy   = mrD.pow_pack;
        #if SHOW_DEBUGS
        periodic_metrict_print_channel(stderr, &cs);
        #endif
        // Copying mr2 data in mr1
        metrics_data_copy(&mr1, &mr2);
        // Reporting and alert-system
        if (rid != NULL) {
            if (nc != NULL && cs.temp > nc->max_temp) {
                // log_report_eard_rt_error(&rid, jid, sid, TEMP_ERROR, (llong) cs.temp);
            }
            if ((cs.DRAM_energy + cs.PCK_energy) == 0) {
                // log_report_eard_rt_error(&rid, jid, sid, RAPL_ERROR, 0);
            }
            if ((cs.avg_f == 0) || ((cs.avg_f > frequency_get_nominal_freq()) && (mpi))) {
                // log_report_eard_rt_error(&rid, jid, sid, FREQ_ERROR, (llong) cs.avg_f);
            }
            report_periodic_metrics(rid, &cs, 1);
        }
    }
}

declr_up_post()
{
    if (strcmp(msg, "node_conf") == 0) {
        nc = (my_node_conf_t *) data;
    }
    if (strcmp(msg, "report_id") == 0) {
        rid = (report_id_t *) data;
    }
    if (strcmp(msg, "new_job") == 0) {
        new_job_for_period(&cs, ((ulong *) data)[0], ((ulong *) data)[1]);
        debug("'%s': posted with message '%s' jid '%lu.%lu'", self_tag, msg, cs.job_id, cs.step_id);
    }
    if (strcmp(msg, "end_job") == 0) {
        end_job_for_period(&cs);
        debug("'%s': posted with message '%s' jid '%lu.%lu'", self_tag, msg, cs.job_id, cs.step_id);
    }
}

declr_up_data_alloc()
{
    *data = &cs;
}