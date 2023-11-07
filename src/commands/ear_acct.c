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

#define _XOPEN_SOURCE 700 //to get rid of the warning

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <common/config.h>
#include <common/config/config_sched.h>
#include <common/system/user.h>
#include <common/output/verbose.h>
#include <common/types/loop.h>
#include <common/types/version.h>
#include <common/types/application.h>
#include <common/types/log.h>
#include <common/types/event_type.h>
#include <common/colors.h>
#include <common/types/classification_limits.h>

#include <commands/query_helpers.h>

#if USE_DB
#include <common/states.h>
#include <common/database/db_helper.h>
#include <common/database/mysql_io_functions.h>
#include <common/database/postgresql_io_functions.h>
#include <common/types/configuration/cluster_conf.h>
cluster_conf_t my_conf;
#endif

#define IMPROVEMENTS 1

#define COLORS 1

#define STANDARD_NODENAME_LENGTH	25
#define APP_TEXT_FILE_FIELDS		22

int full_length = 0;
int verbose = 0;
int all_mpi = 0;
int avx = 0;
int print_gpus = 1;
int loop_extended = 0;
char csv_path[256] = "";

#if COLORS
static void print_colors_legend()
{
  printf("%s CPU busy waiting %s memory bound %s cpu bound %s IO bound%s\n", COL_CYA, COL_GRE, COL_RED, COL_MGT, COL_CLR);
}
static void select_color_by_phase(double CPI, double GBS, double IO, double GFLOPS)
{
  char *col = NULL;
  printf("%s", COL_CLR);
  fflush(stdout);
  /* Memory bound */
  if ((CPI > CPI_MEM_BOUND_DEF) && (GBS > GBS_MEM_BOUND_DEF))                                       col = COL_GRE;
  /* CPU bound */
  if (!col && (CPI < CPI_CPU_BOUND_DEF) && (GBS < GBS_CPU_BOUND_DEF) && (GFLOPS > GFLOPS_BUSY_WAITING_DEF)) col = COL_RED;
  /* Busy waiting */
  if (!col && (CPI < CPI_BUSY_WAITING_DEF) && (GBS < GBS_BUSY_WAITING_DEF) && (GFLOPS < GFLOPS_BUSY_WAITING_DEF))   col = COL_CYA;
  /* IO */
  if (!col && (IO > IO_TH_DEF))      col = COL_MGT;
  if (col) printf("%s",col);
  fflush(stdout);
  
}
#else
#define print_colors_legend()
#define select_color_by_phase(A,B,C,D)
#endif

void usage(char *app)
{
#if USE_DB 
	printf("Usage: %s [Optional parameters]\n"\
			"\tOptional parameters: \n" \
			"\t\t-h\tdisplays this message\n"\
			"\t\t-v\tdisplays current EAR version\n" \
			"\t\t-b\tverbose mode for debugging purposes\n" \
			"\t\t-u\tspecifies the user whose applications will be retrieved. Only available to privileged users. [default: all users]\n" \
			"\t\t-j\tspecifies the job id and step id to retrieve with the format [jobid.stepid] or the format [jobid1,jobid2,...,jobid_n].\n" \
			"\t\t\t\tA user can only retrieve its own jobs unless said user is privileged. [default: all jobs]\n"\
			"\t\t-a\tspecifies the application names that will be retrieved. [default: all app_ids]\n" \
			"\t\t-c\tspecifies the file where the output will be stored in CSV format. If the argument is \"no_file\" the output will be printed to STDOUT [default: off]\n" \
			"\t\t-t\tspecifies the energy_tag of the jobs that will be retrieved. [default: all tags].\n" \
			"\t\t-s\tspecifies the minimum start time of the jobs that will be retrieved in YYYY-MM-DD. [default: no filter].\n" \
			"\t\t-e\tspecifies the maximum end time of the jobs that will be retrieved in YYYY-MM-DD. [default: no filter].\n" \
			"\t\t-l\tshows the information for each node for each job instead of the global statistics for said job.\n" \
			"\t\t-x\tshows the last EAR events. Users, start and end times, job ids, and step ids can be specified as if were showing job information.\n" \
			"\t\t-m\tprints power signatures regardless of whether mpi signatures are available or not.\n" \
			"\t\t-r\tshows the EAR loop signatures. Users, job ids, and step ids can be specified as if were showing job information.\n" \
			"\t\t-o\tmodifies the -r option to also show the corresponding jobs. Should be used with -j.\n", app);
	printf("\t\t-n\tspecifies the number of jobs to be shown, starting from the most recent one. [default: 20][to get all jobs use -n all]\n" );
	printf("\t\t-f\tspecifies the file where the user-database can be found. If this option is used, the information will be read from the file and not the database.\n");
#endif
	exit(0);
}

void print_values(int fd, char ***values, int columns, int rows)
{
	int i, j;
	for (i = 0; i < rows; i++) {
		dprintf(fd, "%s", values[i][0]);
		for (j = 1; j < columns; j++) {
			dprintf(fd, ";%s", values[i][j]);
		}
		dprintf(fd, "\n");
	}
}

void print_header(int fd, char ***values, int columns, int rows)
{
	int i = 0;
	dprintf(fd, "%s", values[i][0]);
	for (i = 1; i < rows; i++) {
		dprintf(fd, ";%s", values[i][0]);
	}
}

struct avg_apps {
    application_t app;
    int num_nodes;
};

void average_applications(application_t *apps, int num_apps, struct avg_apps **sorted_apps, int *num_sorted_apps)
{
    if (num_apps < 1 || apps == NULL) return; //for safety


    int i, current = 0;
    struct avg_apps *avg_apps = calloc(num_apps, sizeof(struct avg_apps));

    //copy the frist one manually to simplify the for loop
    memcpy(&avg_apps[current].app, &apps[0], sizeof(application_t)); 
    avg_apps[current].num_nodes++; 


    //since the first application has been copied, we start at 1
    for (i = 1; i < num_apps; i++) {
        if (avg_apps[current].app.job.id == apps[i].job.id && 
                avg_apps[current].app.job.step_id == apps[i].job.step_id ) { //existing application

            acum_sig_metrics(&avg_apps[current].app.signature, &apps[i].signature);
            avg_apps[current].num_nodes++; //either way, we get a +1 to the nodes counter
        } else { //new application
            current++; 
            memcpy(&avg_apps[current].app, &apps[i], sizeof(application_t)); //we copy the signature directly
            avg_apps[current].num_nodes++; //either way, we get a +1 to the nodes counter
        }
    }
    current++;

    //empty the free space
    avg_apps = realloc(avg_apps, sizeof(struct avg_apps)*current);

    for (i = 0; i < current; i++) {
        compute_avg_sig(&avg_apps[i].app.signature, &avg_apps[i].app.signature, avg_apps[i].num_nodes);
    }
    *sorted_apps = avg_apps;
    *num_sorted_apps = current;
}


#if IMPROVEMENTS
bool print_power_signature_format(application_t *app, char f)
{ 
    switch(f) {
        case 'f': //frequency
            printf("%-8.2lf ", app->power_sig.avg_f/1000000.0);
            break;
        case 't': //time
            printf("%-10.0lf ", app->power_sig.time);
            break;
        case 'i': //imc freq
            printf("%-8s ", "---");
            break;
        case 'g': //GBS
            printf("%-10s ", "---");
            break;
        case 'm': //MPI perc
            printf("%-5s ", "---");
            break;
        case 'o': //IO_MBGS
            printf("%-7s ", "---");
            break;
        case 'e': //energy
            printf("%-10.2lf ", app->power_sig.DC_power*app->power_sig.time);
            break;
        case 'p': //DC power
            printf("%-10.2lf ", app->power_sig.DC_power);
            break;
        case 'F': //FLOPS
            for (int i = 0; i < FLOPS_EVENTS; i++)
                printf("%-10s ", "---");
        case 'D': //DRAM power
            printf("%-10.2lf ", app->power_sig.DRAM_power);
            break;
        case 'P': //PCK power
            printf("%-10.2lf ", app->power_sig.PCK_power);
            break;
        case 'G': //GPUs
            printf(" %-7.2s/%-7.2s %-7.3s %-s%%/%s%% ", "---","---","---","---","---");
            break;
        default:
            return false; //return false if there isn't any field corresponding to this format
            break;

    }
    return true;
}

bool print_signature_format(signature_t *sig, char f)
{
    switch(f) {
        case 'p': //DC power
           printf("%-10.2lf ", sig->DC_power);
            break;
        case 'D': //DRAM power
            printf("%-10.2lf ", sig->DRAM_power);
            break;
        case 'P': //PCK power
            printf("%-10.2lf ", sig->PCK_power);
            break;
        case 'g': //GBS
            printf("%-10.2lf ", sig->GBS);
            break;
        case 'T': //TPI
            printf("%-10.2lf ", sig->TPI);
            break;
        case 'C': //CPI
            printf("%-10.2lf ", sig->CPI);
            break;
        case 's': //gflops
            printf("%-10.2lf ", sig->Gflops/sig->DC_power);
            break;
        case 't': //time
            printf("%-10.0lf ", sig->time);
            break;
        case 'F': //FLOPS
            for (int i = 0; i < FLOPS_EVENTS; i++)
                printf("%-10llu ", sig->FLOPS[i]);
            break;
        case 'L': //L1/2/3 misses
            printf("%-10llu ", sig->L1_misses);
            printf("%-10llu ", sig->L2_misses);
            printf("%-10llu ", sig->L3_misses);
            break;
        case 'I': //instructions
            printf("%-10llu ", sig->instructions);
            break;
        case 'y': //cycles
            printf("%-10llu ", sig->cycles);
            break;
        case 'r': // all three frequencies
            printf("%-3.2lf/", sig->avg_f/1000000.0);
            printf("%-3.2lf/", sig->def_f/1000000.0);
            printf("%-8.2lf ", sig->avg_imc_f/1000000.0);
            break;
        case 'f': //frequency
            printf("%-8.2lf ", sig->avg_f/1000000.0);
            break;
        case 'i': //img freq
            printf("%-8.2lf ", sig->avg_imc_f/1000000.0);
            break;
        case 'd': //def_f
            printf("%-8.2lf ", sig->def_f/1000000.0);
            break;
        case 'm': //MPI perc
            printf("%-5.1lf ", sig->perc_MPI);
            break;
        case 'o': //IO_MBGS
            printf("%-7.1lf ", sig->IO_MBS);
            break;
        case 'v': //vpi
            {
                double vpi;
                compute_sig_vpi(&vpi, sig); 
                printf("%-7.2lf ", vpi*100);
            }
            break;
        case 'e': //energy
            printf("%-10.2lf ", sig->DC_power*sig->time);
            break;
        case 'G': //GPUs
            { // declare a custom scope to avoid compiler complains
                gpu_signature_t *ptr = &sig->gpu_sig;
                double gpu_power = 0, gpu_total_power = 0;
                ulong gpu_freq = 0;
                ulong gpu_util = 0;
                ulong gpu_mem_util = 0;;
                int num_gpus = 0;

                if (ptr->num_gpus > 0) {
                    for (int i = 0; i < ptr->num_gpus; i++) {
                        gpu_total_power += ptr->gpu_data[i].GPU_power;
                        gpu_freq += ptr->gpu_data[i].GPU_freq;
                        if (ptr->gpu_data[i].GPU_util > 0) {
                            gpu_power += ptr->gpu_data[i].GPU_power;
                            gpu_util += ptr->gpu_data[i].GPU_util;
                            gpu_mem_util += ptr->gpu_data[i].GPU_mem_util;
                            num_gpus ++;
                        }
                    }

                    if (num_gpus > 0) {
                        gpu_util /= num_gpus;
                        gpu_mem_util /= num_gpus;
                        printf(" %-7.2lf/%-7.2lf %-7.3lf %-7lu%%/%6lu%% ", gpu_total_power,
                                gpu_power, (double)gpu_freq/1000000, gpu_util, gpu_mem_util);
                    }
                    else {
                        printf(" %-7.2lf/%-7.2s %-7.3s %-s%%/%s%% ", gpu_total_power, "---", "---", "---", "---"); 
                    }
                }
            }
            break;
        default:
            return false; //return false if there isn't any field corresponding to this format
            break;
    }
    return true;
}

void print_app_format(application_t *app, char *fmt, int num_nodes)
{
    int i;
    char is_sbatch;
    bool is_consumed;

    //print the color legend if possible
    if (app->is_mpi){ select_color_by_phase(app->signature.CPI, app->signature.GBS, app->signature.IO_MBS, app->signature.Gflops);}


    for (i = 0; i < strlen(fmt); i++) { //this is preferable to a while loop because of the use of continues
        if (app->is_mpi) is_consumed = print_signature_format(&app->signature, fmt[i]);
        else is_consumed = print_power_signature_format(app, fmt[i]);

        if (is_consumed) continue;

        switch(fmt[i]) {
            case 'a': //application id
                if (strlen(app->job.app_id) > 30)
                    if (strchr(app->job.app_id, '/') != NULL)
                        strcpy(app->job.app_id, strrchr(app->job.app_id, '/')+1);
                printf("%-10s ", app->job.app_id);
                break;
            case 'j': //jobid
                is_sbatch = ((uint)app->job.step_id == BATCH_STEP) ? 1 : 0;
                if (is_sbatch) 
                    printf("%8lu-%-4s ", app->job.id, "sb");
                else
                    printf("%8lu-%-4lu ", app->job.id, app->job.step_id);
                break;
            case 'u': //user id
                printf("%-10s ", app->job.user_id);
                break;
            case 'n': //node id
                printf("%-10s ", app->node_id);
                break;
            case 'N': //num nodes
                printf("%-10d ", num_nodes);
                break;
            default:
                printf("unknown format char %c\n", fmt[i]);
                break;
        }
    }
    printf("\n");

    //reset the color
    printf("%s", COL_CLR);
    fflush(stdout);

}

void print_header_format(char *fmt)
{ 
#if 0
    if (avx)
        dprintf(fd, header_format,
                "JOB", "STEP", "USER", "APPLICATION", "POLICY", "NODES", "AVG","DEF", "IMC(GHz)", "TIME(s)",
                "POWER(W)", "GBS", "CPI", "ENERGY(J)", "GFLOPS/W", "IO(MBs)", "MPI%", "VPI(%)");
   strcpy(header_format, "%7s-%-4s %-10s %-16s %-6s %-5s %-3s/%-3s/%-8s %-10s %-8s %-7s %-5s %-12s %-8s %-7s %-5s %-14s");
#endif
    while (*fmt) {
        switch(*fmt) {
            case 'a': //application id
                printf("%-16s ", "APPLICATION");
                break;
            case 'j': //jobid
                printf("%7s-%-4s ", "JOB", "STEP");
                break;
            case 'u': //user id
                printf("%-10s ", "USER");
                break;
            case 'n': //node id
                printf("%-10s ", "NODE ID");
                break;
            case 'N': //node id
                printf("%-10s ", "NODES");
                break;
            case 'p': //DC power
                printf("%-8s ", "POWER(W)");
                break;
            case 'G': //GPUs
                break;
            case 'D': //DRAM power
                printf("%-10s ", "DRAM POW(W)");
                break;
            case 'P': //PCK power
                printf("%-10s ", "PKG POW(W)");
                break;
            case 'g': //GBS
                printf("%-7s ", "GBS");
                break;
            case 'T': //TPI
                printf("%-5s ", "TPI");
                break;
            case 'C': //CPI
                printf("%-5s ", "CPI");
                break;
            case 's': //gflops
                printf("%-12s ", "GFLOPS/W");
                break;
            case 't': //time
                printf("%-10s ", "TIME(s)");
                break;
            case 'F': //FLOPS
                for (int i = 0; i < FLOPS_EVENTS; i++) 
                    printf("%-5s%-5d ", "FLOPS", i+1);
                break;
            case 'L': //L1/2/3 misses
                printf("%-10s ", "L1_misses");
                printf("%-10s ", "L2_misses");
                printf("%-10s ", "L3_misses");
                break;
            case 'I': //instructions
                printf("%-10s ", "INSTRUCT");
                break;
            case 'y': //cycles
                printf("%-10s ", "CYCLES");
                break;
            case 'r': //all three freqs in format
                printf("%-3s/%-3s/%-8s ", "AVG", "DEF", "IMC(GHz)");
                break;
            case 'f': //frequency
                printf("%-8s ", "AVG(GHz)");
                break;
            case 'i': //img freq
                printf("%-8s ", "IMC(GHz)");
                break;
            case 'd': //def_f
                printf("%-8s ", "DEF(GHz)");
                break;
            case 'm': //MPI perc
                printf("%-5s ", "MPI(%)");
                break;
            case 'o': //IO_MBGS
                printf("%-7s ", "IO(MBs)");
                break;
            case 'v': //vpi
                printf("%-14s ", "VPI(%)");
                break;
            case 'e': //energy
                printf("%-12s ", "ENERGY(J)");
                break;
            default:
                break;
        }
        fmt++;
    }
    printf("\n");
}

void print_applications(struct avg_apps *apps, int num_apps, char *fmt)
{
    int i;
    for (i = 0; i < num_apps; i++) {
        print_app_format(&apps[i].app, fmt, apps[i].num_nodes);
    }
}

void print_full_apps(application_t *apps, int num_apps, bool is_csv)
{
    if (is_csv) {
        if (!strcmp(csv_path, "no_file")) {
            for (int i = 0; i < num_apps; i++)
                print_application_fd(STDOUT_FILENO, &apps[i], my_conf.database.report_sig_detail, 1, 0);
        } 
        else {
            for (int i = 0; i < num_apps; i++) {
                append_application_text_file(csv_path, &apps[i], my_conf.database.report_sig_detail, 1, 0);
            }
        }
    }
    else {
        for (int i = 0; i < num_apps; i++) {
            print_app_format(&apps[i], "format", 1);
        }
    }
}

void print_short_apps(application_t *apps, int num_apps, bool is_csv)
{
    if (num_apps == 0) 
        printf("No applications retrieved\n");
    struct avg_apps *sorted_apps = NULL;
    int new_num_apps = 0;
    average_applications(apps, num_apps, &sorted_apps, &new_num_apps);
    if (is_csv) {
        if (!strcmp(csv_path, "no_file")) {
            for (int i = 0; i < new_num_apps; i++)
                print_application_fd(STDOUT_FILENO, &sorted_apps[i].app, my_conf.database.report_sig_detail, 1, 0);
        } 
        else {
            for (int i = 0; i < new_num_apps; i++) {
                append_application_text_file(csv_path, &sorted_apps[i].app, my_conf.database.report_sig_detail, 1, 0);
            }
        }
    } else {
        print_applications(sorted_apps, new_num_apps, "janft");
    }
    free(sorted_apps);
}
#endif 

void process_and_print_applications(application_t *apps, int num_apps, bool is_csv, bool aggregate) 
{
    if (aggregate)
        print_short_apps(apps, num_apps, is_csv);
    else
        print_full_apps(apps, num_apps, is_csv);

}

void print_event_type(int type, int fd)
{
    char str[64];
    ear_event_t aux;
    aux.event = type;
    event_type_to_str(&aux, str, sizeof(str));
    dprintf(fd, "%20s ", str);
}

#if DB_MYSQL
void mysql_print_events(MYSQL_RES *result, int fd)
{
    if (fd <  0) {
        fd = 1; // The standard output
    }

    int i;
    int num_fields = mysql_num_fields(result);

    MYSQL_ROW row;
    int has_records = 0;
    while ((row = mysql_fetch_row(result)) != NULL) 
    {
        if (!has_records)
        {
            dprintf(fd, "%12s %20s %20s %8s %8s %20s %12s\n",
                    "Event_ID", "Timestamp", "Event_type", "Job_id", "Step_id", "Value", "node_id");
            has_records = 1;
        }

        int possible_negative = 0; // This variable checks whether the event value can be negative.
        for(i = 0; i < num_fields; i++) {
            if (i == 2) {
                int event_type_int = strtol(row[i], NULL, 10);

                print_event_type(event_type_int, fd);  

                if (event_type_int == ENERGY_SAVING || event_type_int == POWER_SAVING ||
                        event_type_int == POWER_SAVING_AVG || event_type_int == ENERGY_SAVING_AVG) {
                    possible_negative = 1;
                }
            }
            else if (i == 1)
                dprintf(fd, "%20s ", row[i] ? row[i] : "NULL");
            else if (i == 4 || i == 3)
                dprintf(fd, "%8s ", row[i] ? row[i] : "NULL");
            else if (i == 5) {

                if (row[i] != NULL) {

                    if (possible_negative) {
                        long int value = atoi(row[i]);
                        dprintf(fd, "%20ld ", *(long *) &value);
                    } else {
                        dprintf(fd, "%20s ", row[i]);
                    } // Column 5 not possible negative
                } else {
                    dprintf(fd, "%20s ", "NULL");
                } // Column 5 NULL
            } else {
                dprintf(fd, "%12s ", row[i] ? row[i] : "NULL");
            } // Column 0
        }
        dprintf(fd, "\n");
    }
    if (!has_records)
    {
        printf("There are no events with the specified properties.\n\n");
    }
}
#elif DB_PSQL
void postgresql_print_events(PGresult *res, int fd)
{
    if (fd < 0) {
        fd = 1; // Standard output
    }

    int i, j, num_fields, has_records = 0;
    num_fields = PQnfields(res);

    int possible_negative = 0; // This variable checks whether the event value can be negative.
    for (i = 0; i < PQntuples(res); i++)
    {
        if (!has_records)
        {
            dprintf(fd, "%12s %22s %20s %8s %8s %20s %12s\n",
                    "Event_ID", "Timestamp", "Event_type", "Job_id", "Step_id", "Value", "node_id");
            has_records = 1;
        }

        for (j = 0; j < num_fields; j++) {
            if (j == 2) {
                int event_type_int = strtol(PQgetvalue(res, i, j), NULL, 10);

                print_event_type(event_type_int, fd);

                if (event_type_int == ENERGY_SAVING || event_type_int == POWER_SAVING) {
                    possible_negative = 1;
                }
            }
            else if (i == 1)
                dprintf(fd, "%22s ", PQgetvalue(res, i, j) ? PQgetvalue(res, i, j) : "NULL");
            else if (i == 4 || i == 3)
                dprintf(fd, "%8s ", PQgetvalue(res, i, j) ? PQgetvalue(res, i, j) : "NULL");
            else if (i == 5) {

                if (PQgetvalue(res, i, j) != NULL) {

                    if (possible_negative) {
                        long int value = atoi(PQgetvalue(res, i, j));
                        dprintf(fd, "%20ld ", *(long *) &value);
                    } else {
                        dprintf(fd, "%20s ", PQgetvalue(res, i, j));
                    } // Column 5 not possible negative
                } else {
                    dprintf(fd, "%20s ", "NULL");
                } // Column 5 NULL

            }
            else
                dprintf(fd, "%12s ", PQgetvalue(res, i, j) ? PQgetvalue(res, i, j) : "NULL");
        }

        dprintf(fd, "\n");
    }

    if (!has_records)
    {
        printf("There are no events with the specified properties.\n\n");
    }
}
#endif

#if DB_MYSQL
#define EVENTS_QUERY "SELECT id, FROM_UNIXTIME(timestamp), event_type, job_id, step_id, value, node_id FROM Events"
#define EVENTS_QUERY_RAW "SELECT id, timestamp, event_type, job_id, step_id, value, node_id FROM Events"
#elif DB_PSQL
#define EVENTS_QUERY "SELECT id, to_timestamp(timestamp), event_type, job_id, step_id, value, node_id FROM Events"
#define EVENTS_QUERY_RAW "SELECT id, timestamp, event_type, job_id, step_id, value, node_id FROM Events"
#endif

void read_events(char *user, query_adds_t *q_a) 
{
    char query[512];
    char subquery[128];

    if (strlen(my_conf.database.user_commands) < 1) 
    {
        fprintf(stderr, "Warning: commands' user is not defined in ear.conf\n");
    }
    else
    {
        strcpy(my_conf.database.user, my_conf.database.user_commands);
        strcpy(my_conf.database.pass, my_conf.database.pass_commands);
    }
    init_db_helper(&my_conf.database);


    int fd = -1;
    if (strlen(csv_path) > 0 && strcmp(csv_path, "no_file")) {
        fd = open(csv_path, O_WRONLY | O_CREAT | O_TRUNC , S_IRUSR|S_IWUSR|S_IRGRP);
    }

    if (fd > 0) {
        strcpy(query, EVENTS_QUERY_RAW);
    } else {
        strcpy(query, EVENTS_QUERY);
    }

    add_int_comp_filter(query, "event_type", 100, 0);
    if (q_a->job_id >= 0)
        add_int_filter(query, "job_id", q_a->job_id);
    else if (strlen(q_a->job_ids) > 0)
        add_int_list_filter(query, "job_id", q_a->job_ids);
    if (q_a->step_id >= 0)
        add_int_filter(query, "step_id", q_a->step_id);
    if (strlen(q_a->step_ids) > 0)
        add_string_filter_no_quotes(query, "step_id", q_a->step_ids);
    if (user != NULL)
        add_string_filter(query, "node_id", user);
    if (q_a->start_time != 0)
        add_int_comp_filter(query, "timestamp", q_a->start_time, 1);
    if (q_a->end_time != 0)
        add_int_comp_filter(query, "timestamp", q_a->end_time, 0);

    if (q_a->limit > 0)
    {
        sprintf(subquery, " ORDER BY timestamp desc LIMIT %d", q_a->limit);
        strcat(query, subquery);
    }

    if (verbose) printf("QUERY: %s\n", query);


#if DB_MYSQL
    MYSQL_RES *result = db_run_query_result(query);
#elif DB_PSQL
    PGresult *result = db_run_query_result(query);
#endif

    if (result == NULL) 
    {
        printf("Database error\n");
        return;
    }

#if DB_MYSQL
    mysql_print_events(result, fd);
#elif DB_PSQL
    postgresql_print_events(result, fd);
#endif
}

void print_loops(loop_t *loops, int num_loops)
{
    int i;
    char line[256];
    signature_t sig;
    char date_iter[128];
#if REPORT_TIMESTAMP
    struct tm *date_info;
#endif

#if REPORT_TIMESTAMP
    strcpy(line, "%6s-%-4s\t %-10s %-9s %-8s %-7s %-8s %-8s %-8s %-8s %-5s %-7s %-5s ");
    printf(line, "JOB", "STEP", "NODE ID", "DATE", "POWER(W)", "GBS/TPI", "CPI", "GFLOPS/W", "TIME(s)", "AVG_F/F", "IMC_F", "IO(MBS)", "MPI%");
#else
    strcpy(line, "%6s-%-4s\t %-10s %-6s %-8s %-8s %-8s %-8s %-8s %-8s %-5s %-7s %-5s ");
    printf(line, "JOB", "STEP", "NODE ID", "ITER.", "POWER(W)", "GBS", "CPI", "GFLOPS/W", "TIME(s)", "AVG_F/F", "IMC_F", "IO(MBS)", "MPI%");
#endif
#if USE_GPUS
    //GPU variable declaration
    int s;
    char gpu_line[256], tmp[16];
    double gpup = 0, gpupu = 0;
    ulong  gpuf = 0, gpuu = 0, gpuused = 0, gpu_mem_util = 0;
    strcpy(line, "%-13s %-8s %-13s");
    if (print_gpus)
        printf(line, "G-POWER(T/U)","G-FREQ","G-UTIL(G/MEM)");
    //prepare gpu_line format
    strcpy(gpu_line, "%-6.1lf/%6.1lf  %-8.2lf %-13s");
#endif
    printf("\n");

    strcpy(line, "%6u-%-4u\t %-10s %-9s %-8.1lf %-3.0lf/%-3.0lf %-8.3lf %-8.3lf %-8.3lf %-4.2lf/%-3.1lf %-5.2lf %-7.1lf %-5.1lf ");
    for (i = 0; i < num_loops; i++)
    {
        signature_copy(&sig, &loops[i].signature);
        select_color_by_phase(sig.CPI, sig.GBS, sig.IO_MBS, sig.Gflops);

#if USE_GPUS
        if (print_gpus)
        {				
            for (s=0; s < sig.gpu_sig.num_gpus; s++){
                gpup += sig.gpu_sig.gpu_data[s].GPU_power;
                gpuf += sig.gpu_sig.gpu_data[s].GPU_freq;
                if (sig.gpu_sig.gpu_data[s].GPU_util){
                    gpupu += sig.gpu_sig.gpu_data[s].GPU_power;
                    gpuu += sig.gpu_sig.gpu_data[s].GPU_util;
                    gpu_mem_util += sig.gpu_sig.gpu_data[s].GPU_mem_util;
                    gpuused++;
                }
            }
            if (sig.gpu_sig.num_gpus)
                gpuf /= sig.gpu_sig.num_gpus;

            if (gpuused > 0)
            {
                gpuu /= gpuused;
                gpu_mem_util /= gpuused;
            }
        }
#endif
#if REPORT_TIMESTAMP
        date_info = localtime( (time_t *)&loops[i].total_iterations);
        strftime(date_iter, sizeof(date_iter), "%H:%M:%S", date_info );
#else
        sprintf(date_iter,"%lu", loops[i].total_iterations);
#endif
        printf(line, loops[i].jid, loops[i].step_id, loops[i].node_id, date_iter,
                sig.DC_power, sig.GBS, sig.TPI,sig.CPI, sig.Gflops/sig.DC_power, sig.time, (double)(sig.avg_f)/1000000,(double)(sig.def_f)/1000000, (double)(sig.avg_imc_f)/1000000, sig.IO_MBS, sig.perc_MPI);
#if USE_GPUS
        sprintf(tmp, "%lu%%/%lu%%", gpuu, gpu_mem_util);
        if (print_gpus) printf(gpu_line, gpup, gpupu, (double)gpuf/1000000.0, tmp);
        gpu_mem_util = 0;
        gpuused = 0;
        gpupu = 0;
        gpuf = 0;
        gpuu = 0;
        gpup = 0;
#endif
        printf("\n");
    }
}

#define LOOPS_QUERY "SELECT * FROM Loops "
#define LOOPS_FILTER_QUERY "SELECT Loops.* from Loops INNER JOIN Jobs ON Jobs.id = job_id AND Jobs.step_id = Loops.step_id "

void format_loop_query(char *user, char *query, char *base_query, query_adds_t *q_a)
{
    char subquery[128];

    strcpy(query, base_query);
    if (user != NULL) add_string_filter(query, "user_id", user);

    if (q_a->job_id >= 0)
        add_int_filter(query, "job_id", q_a->job_id);
    else if (strlen(q_a->job_ids) > 0)
        add_int_list_filter(query, "job_id", q_a->job_ids);
    if (q_a->step_id >= 0)
        add_int_filter(query, "Loops.step_id", q_a->step_id);
    if (strlen(q_a->step_ids) > 0)
        add_string_filter_no_quotes(query, "step_id", q_a->step_ids);

    if (q_a->limit > 0 && q_a->job_id < 0)
    {
        sprintf(subquery, " ORDER BY job_id desc LIMIT %d", q_a->limit);
        strcat(query, subquery);
    }
    else strcat(query, " ORDER BY job_id desc");

    if (verbose) printf("QUERY: %s\n", query);

}

void read_jobs_from_loops(query_adds_t *q_a)
{
    char query[256], subquery[256], tmp_path[512];
    char ***values;
    int columns, rows;
    int fd = STDOUT_FILENO, ret;

    if (strlen(csv_path) > 0 && strcmp(csv_path, "no_file")) {
        sprintf(tmp_path, "out_jobs.%s", csv_path);
        fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC , S_IRUSR|S_IWUSR|S_IRGRP);
    }

    ret = db_run_query_string_results("DESCRIBE Jobs", &values, &columns, &rows);
    if (verbose) printf("running query DESCRIBE Jobs\n");
    if (ret != EAR_SUCCESS) {
        printf("Error reading Loop description from database.\n");
        return;
    }
    print_header(fd, values, columns, rows);
    dprintf(fd, "\n"); //print_header does not print a new_line
    db_free_results(values, columns, rows);

    // reset filters
    reset_query_filters();
    strcpy(query, "SELECT * FROM Jobs ");

    if (q_a->job_id >= 0)
        add_int_filter(query, "id", q_a->job_id);
    else if (strlen(q_a->job_ids) > 0)
        add_int_list_filter(query, "id", q_a->job_ids);
    else printf("WARNING: -o option is meant to be used with a -j specification\n\n");
    if (q_a->step_id >= 0)
        add_int_filter(query, "step_id", q_a->step_id);
    if (q_a->start_time > 0)
        add_int_filter(query, "start_time", q_a->step_id);
    if (q_a->end_time > 0)
        add_int_filter(query, "end_time", q_a->step_id);

    if (q_a->limit > 0 && q_a->job_id < 0)
    {
        sprintf(subquery, " ORDER BY id desc LIMIT %d", q_a->limit);
        strcat(query, subquery);
    }
    else strcat(query, " ORDER BY id desc");

    if (verbose) printf("\nQUERY: %s\n", query);
    ret = db_run_query_string_results(query, &values, &columns, &rows);
    if (ret != EAR_SUCCESS) {
        printf("Error reading Jobs from database\n");
        return;
    }

    print_values(fd, values, columns, rows);
    db_free_results(values, columns, rows);

}

void read_loops(char *user, query_adds_t *q_a) 
{
    char query[1024];

    if (strlen(my_conf.database.user_commands) < 1) 
    {
        fprintf(stderr, "Warning: commands' user is not defined in ear.conf\n");
    }
    else
    {
        strcpy(my_conf.database.user, my_conf.database.user_commands);
        strcpy(my_conf.database.pass, my_conf.database.pass_commands);
    }
    init_db_helper(&my_conf.database);

#if 0
    if (loop_extended) {
        format_loop_query(user, query, LOOPS_EXTENDED_QUERY, q_a);
        return extended_loop_print(query);
    }
#endif

    if (user != NULL) {
        format_loop_query(user, query, LOOPS_FILTER_QUERY, q_a);
    } else {
        format_loop_query(user, query, LOOPS_QUERY, q_a);
    }


    loop_t *loops = NULL;
    int num_loops;

    num_loops = db_read_loops_query(&loops, query);

    if (num_loops < 1)
    {
        printf("No loops retrieved\n");
        return;
    }

    if (verbose) printf("retrieved %d loops\n", num_loops);
    if (strlen(csv_path) > 0)
    {
        int i;
        if (!strcmp(csv_path, "no_file"))
        {
            for (i = 0; i < num_loops; i++)
                print_loop_fd(STDOUT_FILENO, &loops[i]);
        }
        else {
            for (i = 0; i < num_loops; i++)
                append_loop_text_file_no_job(csv_path, &loops[i], 1, 0, ' ');
            printf("appended %d loops to %s\n", num_loops, csv_path);
        }
    } else {
        print_loops(loops, num_loops);
    }
    if (loops != NULL) free(loops);
    if (loop_extended) read_jobs_from_loops(q_a);

}


//select Applications.* from Applications join Jobs on job_id = id where Jobs.end_time in (select end_time from (select end_time from Jobs where user_id = "xjcorbalan" and id = 284360 order by end_time desc limit 25) as t1) order by Jobs.end_time desc;
//select Applications.* from Applications join Jobs on job_id=id where Jobs.user_id = "xjcorbalan" group by job_id order by Jobs.end_time desc limit 5;
#if USE_DB
void read_from_database(char *user, query_adds_t *q_a) 
{
    int num_apps = 0;
    char subquery[256];
    char query[512];

    if (strlen(my_conf.database.user_commands) < 1) {
        fprintf(stderr, "Warning: commands' user is not defined in ear.conf\n");
    }
    else {
        strcpy(my_conf.database.user, my_conf.database.user_commands);
        strcpy(my_conf.database.pass, my_conf.database.pass_commands);
    }
    init_db_helper(&my_conf.database);


    if (verbose) {
        printf("Preparing query statement\n");
    }

    sprintf(query, "SELECT Applications.* FROM Applications join Jobs on job_id=id and Applications.step_id = Jobs.step_id where Jobs.id in (select id from (select id, end_time from Jobs" );
    application_t *apps;
    if (q_a->job_id >= 0)
        add_int_filter(query, "id", q_a->job_id);
    else if (strlen(q_a->job_ids) > 0)
        add_int_list_filter(query, "id", q_a->job_ids);
    if (q_a->step_id >= 0)
        add_int_filter(query, "step_id", q_a->step_id);
    else if (strlen(q_a->step_ids) > 0)
        add_string_filter_no_quotes(query, "step_id", q_a->step_ids);
    if (user != NULL)
        add_string_filter(query, "user_id", user);
    if (strlen(q_a->e_tag) > 0)
        add_string_filter(query, "e_tag", q_a->e_tag);
    if (strlen(q_a->app_id) > 0)
        add_string_filter(query, "app_id", q_a->app_id);
    if (q_a->start_time != 0)
        add_int_comp_filter(query, "start_time", q_a->start_time, 1);
    if (q_a->end_time != 0)
        add_int_comp_filter(query, "end_time", q_a->end_time, 0);

    if (q_a->limit > 0) {
        sprintf(subquery, " ORDER BY Jobs.end_time desc LIMIT %d", q_a->limit);
        strcat(query, subquery);
    }
    strcat(query, ") as t1");

    reset_query_filters();
    if (q_a->job_id >= 0)
        add_int_filter(query, "id", q_a->job_id);
    else if (strlen(q_a->job_ids) > 0)
        add_int_list_filter(query, "id", q_a->job_ids);
    if (q_a->step_id >= 0)
        add_int_filter(query, "Jobs.step_id", q_a->step_id);
    else if (strlen(q_a->step_ids) > 0)
        add_string_filter_no_quotes(query, "Jobs.step_id", q_a->step_ids);
    if (user != NULL)
        add_string_filter(query, "user_id", user);

    strcat(query, ") order by Jobs.id desc, Jobs.step_id desc, Jobs.end_time desc");

    if (verbose) {
        printf("Retrieving applications\n");
    }

    if (verbose) {
        printf("QUERY: %s\n", query);
    }

    num_apps = db_read_applications_query(&apps, query);

    if (verbose) {
        printf("Finalized retrieving applications\n");
    }

    if (num_apps == EAR_MYSQL_ERROR) {
        exit(1); //error
    }

    if (num_apps < 1) {
        printf("No jobs found.\n");
        return;
    }


    if (q_a->limit == 20 && strlen(csv_path) < 1)
        printf("\nBy default only the first 20 jobs are retrieved.\n\n");

    bool is_csv = (strlen(csv_path) < 1);
    bool aggregate = !full_length;
    process_and_print_applications(apps, num_apps, is_csv, aggregate);

    free(apps);

}
#endif

void read_from_files(char *path, char *user, query_adds_t *q_a)
{
    application_t *apps;
    int num_apps = read_application_text_file(path, &apps, 0);
    if (full_length) print_full_apps(apps, num_apps, false);
    else print_short_apps(apps, num_apps, false);
}

int main(int argc, char *argv[])
{
    int c;

    query_adds_t query_adds;
    memset(&query_adds, 0, sizeof(query_adds_t));

    query_adds.limit = 20;
    query_adds.job_id = -1;
    query_adds.step_id = -1;

    char is_loops = 0;
    char is_events = 0;

    char path_name[256];
    char *file_name = NULL;

    struct tm tinfo = {0};

    verb_level = -1;
    verb_enabled = 0;
    if (get_ear_conf_path(path_name)==EAR_ERROR){
        fprintf(stderr, "Error getting ear.conf path\n");
        exit(1);
    }

    if (read_cluster_conf(path_name, &my_conf) != EAR_SUCCESS) {
        fprintf(stderr, "ERROR reading cluster configuration\n");
    }

    user_t user_info;
    if (user_all_ids_get(&user_info) != EAR_SUCCESS)
    {
        fprintf(stderr, "Failed to retrieve user data\n");
        exit(1);
    }

    char *user = user_info.ruid_name;
    if (getuid() == 0 || is_privileged_command(&my_conf))
    {
        user = NULL; //by default, privilegd users or root will query all user jobs
    }

    char *token;
    int option_idx;
    static struct option long_options [] = {
        {"help",       no_argument, 0, 'h'},
        {"version",    no_argument, 0, 'v'},
        {"no-mpi",     no_argument, 0, 'm'},
        {"avx",        no_argument, 0, 'p'},
        {"verbose",    no_argument, 0, 'b'},
        {"show-gpus",  no_argument, 0, 'g'},
        {"long-apps",  no_argument, 0, 'l'},
        {"loops",      no_argument, 0, 'r'},
        {"ext_loops",  no_argument, 0, 'o'},
        {"help",       no_argument, 0, 'h'},
        {"limit",      required_argument, 0, 'n'},
        {"user",       required_argument, 0, 'u'},
        {"jobs",       required_argument, 0, 'j'},
        {"events",     required_argument, 0, 'x'},
        {"csv",        required_argument, 0, 'c'},
        {"tag",        required_argument, 0, 't'},
        {"app-id",     required_argument, 0, 'a'},
        {"start-time", required_argument, 0, 's'},
        {"end-time",   required_argument, 0, 'e'},
    };

#if COLORS
    print_colors_legend();
#endif

    while (1)
    {
        c = getopt_long(argc, argv, "n:u:j:f:t:voma:pbglrs:e:c:hx::", long_options, &option_idx);

        if (c == -1) break;

        switch (c)
        {
            case 'n':
                if (!strcmp(optarg, "all")) query_adds.limit = -1;
                else query_adds.limit = atoi(optarg);
                break;
            case 'r':
                is_loops = 1;
                break;
            case 'u':
                if (user != NULL) break;
                user = optarg;
                break;
            case 'j':
                query_adds.limit = query_adds.limit == 20 ? -1 : query_adds.limit; //if the limit is still the default
                if (strchr(optarg, ',')) {
                    strcpy(query_adds.job_ids, optarg);
                }
                else {
                    query_adds.job_id = atoi(strtok(optarg, "."));
                    token = strtok(NULL, ".");
                    if (token != NULL) {
                        if (!strcmp(token, "sbatch") || !strcmp(token, "sb")) {
                            sprintf(query_adds.step_ids, "%u", BATCH_STEP);
                        }
                        else strcpy(query_adds.step_ids, token);
                    }
                    //if (token != NULL) query_adds.step_id = atoi(token);
                }
                break;
            case 'x':
                is_events = 1;
                if (optind < argc && strchr(argv[optind], '-') == NULL) {
                    if (strchr(argv[optind], ',')) {
                        strcpy(query_adds.job_ids, argv[optind]);
                    }
                    else {
                        query_adds.job_id = atoi(strtok(argv[optind], "."));
                        token = strtok(NULL, ".");
                        if (token != NULL) {
                            if (!strcmp(token, "sbatch") || !strcmp(token, "sb")) {
                                sprintf(query_adds.step_ids, "%u", BATCH_STEP);
                            }
                            else strcpy(query_adds.step_ids, token);
                        }
                        //if (token != NULL) query_adds.step_id = atoi(token);
                    }
                }
                else if (verbose) printf("No argument for -x\n");
                break;
            case 'o':
                loop_extended = 1;
                break;
            case 'g':
                print_gpus = 1;
                break;
            case 'f':
                file_name = optarg;
                break;
            case 'l':
                full_length = 1;
                break;
            case 'b':
                verbose = 1;
                break;
            case 'v':
                free_cluster_conf(&my_conf);
                print_version();
                exit(0);
                break;
            case 'm':
                all_mpi = 1;
                break;
            case 'c':
                strcpy(csv_path, optarg);
                break;
            case 't':
                strcpy(query_adds.e_tag, optarg);
                break;
            case 'p':
                avx = 1;
                break;
            case 'a':
                strcpy(query_adds.app_id, optarg);
                break;
            case 's':
                if (strptime(optarg, "%Y-%m-%e", &tinfo) == NULL) {
                    printf("Incorrect time format. Supported format is YYYY-MM-DD\n"); //error
                    free_cluster_conf(&my_conf);
                    exit(1);
                }
                query_adds.start_time = mktime(&tinfo);
                break;
            case 'e':
                if (strptime(optarg, "%Y-%m-%e", &tinfo) == NULL) {
                    printf("Incorrect time format. Supported format is YYYY-MM-DD\n"); //error
                    free_cluster_conf(&my_conf);
                    exit(1);
                }
                query_adds.end_time = mktime(&tinfo);
                break;
            case 'h':
                free_cluster_conf(&my_conf);
                usage(argv[0]);
                break;
        }
    }

    if (verbose) printf("Limit set to %d\n", query_adds.limit);

    if (file_name != NULL) read_from_files(file_name, user, &query_adds);
    else if (is_events) read_events(user, &query_adds);
    else if (is_loops) read_loops(user, &query_adds);
    else read_from_database(user, &query_adds); 

#if COLORS
    printf("%s",COL_CLR);
#endif

    free_cluster_conf(&my_conf);
    exit(0);
}
