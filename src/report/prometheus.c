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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <microhttpd.h>

#include <common/states.h>
#include <common/types/types.h>
#include <common/output/verbose.h>

#include <report/report.h>

#define NUM_LINES 30 

static struct MHD_Daemon * d;
static char text_buffers[NUM_LINES][258];
static char text[NUM_LINES*258];
static int next_buffer = 0;

static enum MHD_Result
    ahc_echo(void * cls,
        struct MHD_Connection * connection,
        const char * url,
        const char * method,
        const char * version,
        const char * upload_data,
        size_t * upload_data_size,
        void ** ptr) {
    static int dummy;
    const char * page = cls;
    struct MHD_Response * response;
    int ret;

    if (0 != strcmp(method, "GET"))
        return MHD_NO; /* unexpected method */
    if (&dummy != *ptr)
    {
        /* The first time only the headers are valid,
           do not respond in the first round... */
        *ptr = &dummy;
        return MHD_YES;
    }
    if (0 != *upload_data_size)
        return MHD_NO; /* upload data in a GET!? */
    *ptr = NULL; /* clear context pointer */
    response = MHD_create_response_from_buffer (strlen(page),
            (void*) page,
            MHD_RESPMEM_PERSISTENT);
    ret = MHD_queue_response(connection,
            MHD_HTTP_OK,
            response);
    MHD_destroy_response(response);
    return ret;
}


void rebuild_buffer()
{
    int i;
    strcpy(text, "");
    for (i = 0; i < NUM_LINES; i++) {
        strcat(text, text_buffers[i]);
    }

}

state_t report_init(report_id_t *id, cluster_conf_t *cconf)
{

    int i;
    for (i = 0; i < NUM_LINES; i++) {
        strcpy(text_buffers[i], "");
    }
    strcpy(text, "");
    d = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
            9011,
            NULL,
            NULL,
            &ahc_echo,
            text,
            MHD_OPTION_END);

    if (d == NULL) {
        warning("Couldn't open server for Prometheus to scrape, returning");
        return EAR_ERROR;
    }
    verbose(0, "prometheus report_init");
    return EAR_SUCCESS;
}

state_t report_dispose(report_id_t *id)
{
    MHD_stop_daemon(d);
    return EAR_SUCCESS;
}

state_t report_periodic_metrics(report_id_t *id, periodic_metric_t *mets, uint count)
{

    sprintf(text_buffers[0], "periodic_metric_DC_energy_Joules{node=\"%s\"} %lu %lu\n", 
            mets[0].node_id, mets[0].DC_energy, mets[0].end_time*1000);
    sprintf(text_buffers[1], "periodic_metric_avg_freq_kHz{node=\"%s\"} %lu %lu\n", 
            mets[0].node_id, mets[0].avg_f, mets[0].end_time*1000);
    sprintf(text_buffers[2], "periodic_metric_temp_Celsius{node=\"%s\"} %lu %lu\n", 
            mets[0].node_id, mets[0].temp, mets[0].end_time*1000);
    //next_buffer = (next_buffer+1) >= NUM_LINES ? 0 : next_buffer + 1 ;
    rebuild_buffer();
    return EAR_SUCCESS;
}

state_t report_applications(report_id_t *id, application_t *apps, uint count)
{
    verbose(0, "dummy report_applications");
    return EAR_SUCCESS;
}

state_t report_loops(report_id_t *id, loop_t *loops, uint count)
{
    verbose(0, "dummy report_loops");
    return EAR_SUCCESS;
}
