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

#include <report/report.h>
#include <library/common/library_shared_data.h>

static char csv_log_file[64];
static char nodename[128];

static uint report_file_created = 0;

static state_t append_data(report_id_t *id, mpi_information_t *data);

state_t report_init(report_id_t *id, cluster_conf_t *cconf)
{
    debug("report mpitrace init %d", id->local_rank);

    gethostname(nodename, sizeof(nodename));
    strtok(nodename, ".");

    xsnprintf(csv_log_file, sizeof(csv_log_file), "mpitrace.%s.%d.csv", nodename, id->local_rank);

    return EAR_SUCCESS;
}

state_t report_misc(report_id_t *id, uint type, cchar *data, uint count)
{
    mpi_information_t *mpi_data = (mpi_information_t *) data;
    if (type == MPITRACE) {
        return append_data(id, mpi_data);
    }
    return EAR_SUCCESS;
}

static state_t append_data(report_id_t *id, mpi_information_t *data)
{

    debug("report mpitrace print %d", id->local_rank);

    FILE *fd;

    if (csv_log_file == NULL) {
        return EAR_ERROR;
    }

    fd = fopen(csv_log_file, "a");
    if (fd == NULL) {
        return EAR_ERROR;
    }
    
    if (!report_file_created) {
        char header[] = "NODE_ID;LOCAL_RANK;GLOBAL_RANK;TOTAL_MPI_CALLS;EXEC_TIME;MPI_TIME";
        if (fprintf(fd, "%s\n", header) < 0) {
            return EAR_ERROR;
        }
        report_file_created = 1;
    }

    fprintf(fd, "%s;%d;%d;%u;%llu;%llu\n", nodename, id->local_rank, id->global_rank, data->total_mpi_calls,
            data->exec_time, data->mpi_time);

    fclose(fd);

    return EAR_SUCCESS;
}
