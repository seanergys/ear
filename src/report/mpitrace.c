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

#define SHOW_DEBUGS 1

#include <common/includes.h>
#include <common/config.h>
#include <library/common/library_shared_data.h>
#include <report/report.h>

static char nodename[128];
static char csv_log_file[64];
static char jobid[32];

static uint report_file_created = 0;

static ullong my_time = 0;

static state_t append_data(report_id_t *id, mpitrace_t *data);

state_t report_init(report_id_t *id, cluster_conf_t *cconf)
{
    debug("report mpitrace init %d", id->local_rank);

    gethostname(nodename, sizeof(nodename));
    strtok(nodename, ".");

    char *job_id = getenv(SCHED_JOB_ID);
    if (job_id == NULL) {
        verbose(3, "%sWARNING%s %s could not be read.", COL_YLW, COL_CLR, SCHED_JOB_ID);
    } else {
        memcpy(jobid, job_id, sizeof(jobid));
    }

    xsnprintf(csv_log_file, sizeof(csv_log_file), "mpitrace.%s.%s.%d.csv", jobid, nodename, id->local_rank);

    return EAR_SUCCESS;
}

state_t report_misc(report_id_t *id, uint type, cchar *data, uint count)
{
    if (type == MPITRACE) {
        mpitrace_t *mpitrace = (mpitrace_t *) data;
        return append_data(id, mpitrace);
    }
    return EAR_SUCCESS;
}

static state_t append_data(report_id_t *id, mpitrace_t *data)
{

    debug("report mpitrace print %d at %s", id->local_rank, nodename);

    FILE *fd;

    if (csv_log_file == NULL) {
        return EAR_ERROR;
    }

    fd = fopen(csv_log_file, "a");
    if (fd == NULL) {
        return EAR_ERROR;
    }
    
    if (!report_file_created) {
        char header[] =
            "JOB_ID;STEP_ID;NODE_ID;LOCAL_RANK;GLOBAL_RANK;TOTAL_MPI_CALLS;EXEC_TIME;"
            "MPI_TIME;BLOCK;T_BLOCK;SYNC;T_SYNC;COLLEC;T_COLLEC;TIMESTAMP";
        if (fprintf(fd, "%s\n", header) < 0) {
            return EAR_ERROR;
        }
        report_file_created = 1;
    }

    mpi_information_t *mpi_info = data->mpi_info;
    mpi_calls_types_t *mpi_types = data->mpi_types_info;

    fprintf(fd, "%s;%s;%s;%d;%d;%u;%llu;%llu;%lu;%lu;%lu;%lu;%lu;%lu\n",
            jobid, getenv(SCHED_STEP_ID), nodename, id->local_rank, id->global_rank,
            mpi_info->total_mpi_calls, mpi_info->exec_time, mpi_info->mpi_time,
            mpi_types->blocking, mpi_types->time_blocking,
            mpi_types->sync, mpi_types->time_sync, mpi_types->collec, mpi_types->time_collec);

    fclose(fd);

    return EAR_SUCCESS;
}
