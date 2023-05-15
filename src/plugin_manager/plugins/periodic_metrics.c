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

#include <plugin_manager/plugins/metrics.h>


static periodic_metric_t  cs; // Current sample
static my_node_conf_t    *nc; // Node conf
static timestamp_t        ts1;
static timestamp_t        ts2;
static double             seconds;

declr_up_get_tag ()
{
    *tag = "periodic-metrics";
}

declr_up_init()
{
    init_periodic_metric(&cs); //eard_power_monitoring()
}

declr_up_callback()
{
    if (nc == NULL) {
        return;
    }
    timestamp_getfast(&ts2);
    seconds = timestamp_fdiff(&ts2, &ts1, TIME_SECS, TIME_MSECS);

    ts1 = ts2;

    // eard_power_monitoring() -> update_historic_info
    // update_historic_info() -> current_sample.avg_f = (ulong) get_nm_cpufreq(&my_nm_id, nm);
    // update_historic_info() -> if (powercap_elapsed_last_powercap() >= f_monitoring) powercap_set_power_per_domain(&pdomain,mpi,current_sample.avg_f);
    // update_historic_info() -> current_sample.start_time = pmon_info->begin; (time())
    // update_historic_info() -> current_sample.end_time   = pmon_info->end;
    // update_historic_info() -> current_sample.DRAM_energy = rapl_dram * time_consumed;
    // update_historic_info() -> current_sample.PCK_energy  = rapl_pck  * time_consumed;
    // update_historic_info() -> #if USE_GPUS current_sample.GPU_energy = gpu_power * time_consumed; #endif
    // update_historic_info() -> current_sample.DC_energy = corrected_power * time_consumed;
    // update_historic_info() -> current_sample.temp = (ulong) get_nm_temp(&my_nm_id, nm);
    // update_historic_info() -> if (current_sample.temp > my_node_conf->max_temp){
    // update_historic_info() ->     syslog(LOG_DAEMON|LOG_ERR,"Node %s reports %lu degress (max %lu)\n",nodename,current_sample.temp,my_node_conf->max_temp);
    // update_historic_info() ->     log_report_eard_rt_error(&rid, jid, sid, TEMP_ERROR, (llong) current_sample.temp);
    // update_historic_info() -> }
    // update_historic_info() -> if ((rapl_dram + rapl_pck) == 0) {
    // update_historic_info() ->     log_report_eard_rt_error(&rid,jid,sid,RAPL_ERROR,0);
    // update_historic_info() -> }
    // update_historic_info() -> if ((current_sample.avg_f == 0) || ((current_sample.avg_f > frequency_get_nominal_freq()) && (mpi))){
    // update_historic_info() ->     log_report_eard_rt_error(&rid, jid, sid, FREQ_ERROR, (llong) current_sample.avg_f);
    // update_historic_info() -> }
    // if (report) {
    //     report_connection_status = report_periodic_metrics(&rid, &current_sample, 1);
    //     last_power_reported      = corrected_power;
    // }
}

declr_up_post()
{
    if (strcmp(msg, "node_conf") == 0) {
        nc = (my_node_conf_t *) data;
    }
    if (strcmp(msg, "new_job") == 0) {
        new_job_for_period(&cs, ((ulong *) data)[0], ((ulong *) data)[1]);
    }
    if (strcmp(msg, "end_job") == 0) {
        end_job_for_period(&cs);
    }
}

declr_up_data_alloc()
{
    *data = &cs;
}