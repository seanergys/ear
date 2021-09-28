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

#if PSQL
#include <stdio.h>
#include <common/config.h>
#include <common/states.h>
#include <common/types/types.h>
#include <common/output/verbose.h>
#include <common/database/db_helper.h>
#include <common/database/postgresql_io_functions.h>

static db_conf_t *db_config = NULL;

#define IS_SIG_FULL_QUERY   "SELECT COUNT(*) FROM information_schema.columns where TABLE_NAME='Signatures' AND TABLE_SCHEMA='%s'"
#define IS_NODE_FULL_QUERY  "SELECT COUNT(*) FROM information_schema.columns where TABLE_NAME='Periodic_metrics' AND TABLE_SCHEMA='%s'"

state_t report_init(cluster_conf_t *conf)
{
	debug("mysql report_init");

    db_config = &conf->database;
    char query[256];
    int num_sig_args, num_per_args;
#if 0

    sprintf(query, IS_SIG_FULL_QUERY, db_config->database);
    num_sig_args = run_query_int_result(query);

    if (num_sig_args == EAR_ERROR)
        set_signature_simple(db_config->report_sig_detail);
    else if (num_sig_args < 20)
        set_signature_simple(0);
    else 
        set_signature_simple(1);


    sprintf(query, IS_NODE_FULL_QUERY, db_config->database);
    num_per_args = run_query_int_result(query);
    if (num_per_args == EAR_ERROR) {
        set_node_detail(db_config->report_node_detail);
    } else if (num_per_args < 8) {
        set_node_detail(0);
    } else {
        set_node_detail(1);
    }

	return EAR_SUCCESS;
#endif
}

state_t report_applications(application_t *apps, uint count)
{
    int bulk_elms = _BULK_ELMS(_MMAAXX(APP_VARS, PSI_VARS, NSI_VARS, JOB_VARS));
	int bulk_sets = _BULK_SETS(count, bulk_elms);
	int e, s;

	PGconn *connection = postgresql_create_connection();

    if (connection == NULL) {
        return EAR_ERROR;
    }

    // Inserting full bulks one by one
	for (e = 0, s = 0; s < bulk_sets; e += bulk_elms, s += 1)
    {
    	if (postgresql_batch_insert_applications(connection, &apps[e], bulk_elms) < 0) {
        	verbose(VDBH, "ERROR while batch writing applications to database.");
            PQfinish(connection);
        	return EAR_ERROR;
    	}
	}
	// Inserting the lagging bulk, the incomplete last one
	if (e < count)
	{
		if (postgresql_batch_insert_applications(connection, &apps[e], count - e) < 0) {
            verbose(VDBH, "ERROR while batch writing applications to database.");
            PQfinish(connection);
            return EAR_ERROR;
        }
	}	 

	PQfinish(connection);

    return EAR_SUCCESS;

}

state_t report_loops(loop_t *loops, uint count)
{
    int bulk_elms = _BULK_ELMS(_MMAAXX(APP_VARS, PSI_VARS, NSI_VARS, JOB_VARS));
	int bulk_sets = _BULK_SETS(count, bulk_elms);
	int e, s;

	PGconn *connection = postgresql_create_connection();

    if (connection == NULL) {
        return EAR_ERROR;
    }

    // Inserting full bulks one by one
	for (e = 0, s = 0; s < bulk_sets; e += bulk_elms, s += 1)
    {
    	if (postgresql_batch_insert_loops(connection, &loops[e], bulk_elms) < 0) {
        	verbose(VDBH, "ERROR while batch writing loops to database.");
            PQfinish(connection);
        	return EAR_ERROR;
    	}
	}
	// Inserting the lagging bulk, the incomplete last one
	if (e < count)
	{
		if (postgresql_batch_insert_loops(connection, &loops[e], count - e) < 0) {
            verbose(VDBH, "ERROR while batch writing loops to database.");
            PQfinish(connection);
            return EAR_ERROR;
        }
	}	 

	PQfinish(connection);

    return EAR_SUCCESS;

}

state_t report_events(ear_event_t *eves, uint count)
{
    int bulk_elms = _BULK_ELMS(_MMAAXX(APP_VARS, PSI_VARS, NSI_VARS, JOB_VARS));
	int bulk_sets = _BULK_SETS(count, bulk_elms);
	int e, s;

	PGconn *connection = postgresql_create_connection();

    if (connection == NULL) {
        return EAR_ERROR;
    }

    // Inserting full bulks one by one
	for (e = 0, s = 0; s < bulk_sets; e += bulk_elms, s += 1)
    {
    	if (postgresql_batch_insert_ear_events(connection, &eves[e], bulk_elms) < 0) {
        	verbose(VDBH, "ERROR while batch writing events to database.");
            PQfinish(connection);
        	return EAR_ERROR;
    	}
	}
	// Inserting the lagging bulk, the incomplete last one
	if (e < count)
	{
		if (postgresql_batch_insert_ear_events(connection, &eves[e], count - e) < 0) {
            verbose(VDBH, "ERROR while batch writing events to database.");
            PQfinish(connection);
            return EAR_ERROR;
        }
	}	 

	PQfinish(connection);

    return EAR_SUCCESS;

}

state_t report_periodic_metrics(periodic_metric_t *mets, uint count)
{
    int bulk_elms = _BULK_ELMS(_MMAAXX(APP_VARS, PSI_VARS, NSI_VARS, JOB_VARS));
	int bulk_sets = _BULK_SETS(count, bulk_elms);
	int e, s;

	PGconn *connection = postgresql_create_connection();

    if (connection == NULL) {
        return EAR_ERROR;
    }

    // Inserting full bulks one by one
	for (e = 0, s = 0; s < bulk_sets; e += bulk_elms, s += 1)
    {
    	if (postgresql_batch_insert_periodic_metrics(connection, &mets[e], bulk_elms) < 0) {
        	verbose(VDBH, "ERROR while batch writing periodic_metrics to database.");
            PQfinish(connection);
        	return EAR_ERROR;
    	}
	}
	// Inserting the lagging bulk, the incomplete last one
	if (e < count)
	{
		if (postgresql_batch_insert_periodic_metrics(connection, &mets[e], count - e) < 0) {
            verbose(VDBH, "ERROR while batch writing periodic_metrics to database.");
            PQfinish(connection);
            return EAR_ERROR;
        }
	}	 

	PQfinish(connection);

    return EAR_SUCCESS;

}

state_t report_periodic_aggregations(periodic_aggregation_t *aggs, uint count)
{
    int bulk_elms = _BULK_ELMS(_MMAAXX(APP_VARS, PSI_VARS, NSI_VARS, JOB_VARS));
	int bulk_sets = _BULK_SETS(count, bulk_elms);
	int e, s;

	PGconn *connection = postgresql_create_connection();

    if (connection == NULL) {
        return EAR_ERROR;
    }

    // Inserting full bulks one by one
	for (e = 0, s = 0; s < bulk_sets; e += bulk_elms, s += 1)
    {
    	if (postgresql_batch_insert_periodic_aggregations(connection, &aggs[e], bulk_elms) < 0) {
        	verbose(VDBH, "ERROR while batch writing periodic_aggregations to database.");
            PQfinish(connection);
        	return EAR_ERROR;
    	}
	}
	// Inserting the lagging bulk, the incomplete last one
	if (e < count)
	{
		if (postgresql_batch_insert_periodic_aggregations(connection, &aggs[e], count - e) < 0) {
            verbose(VDBH, "ERROR while batch writing periodic_aggregations to database.");
            PQfinish(connection);
            return EAR_ERROR;
        }
	}	 

	PQfinish(connection);

    return EAR_SUCCESS;

}

state_t report_applications_no_mpi(application_t *apps, uint count)
{

    int bulk_elms = _BULK_ELMS(_MMAAXX(APP_VARS, PSI_VARS, NSI_VARS, JOB_VARS));
	int bulk_sets = _BULK_SETS(count, bulk_elms);
	int e, s;

	PGconn *connection = postgresql_create_connection();


    if (connection == NULL) {
        return EAR_ERROR;
    }

    // Inserting full bulks one by one
	for (e = 0, s = 0; s < bulk_sets; e += bulk_elms, s += 1)
    {
    	if (postgresql_batch_insert_applications_no_mpi(connection, &apps[e], bulk_elms) < 0) {
        	verbose(VDBH, "ERROR while batch writing non_mpi applications to database.");
            PQfinish(connection);
        	return EAR_ERROR;
    	}
	}
	// Inserting the lagging bulk, the incomplete last one
	if (e < count)
	{
		if (postgresql_batch_insert_applications_no_mpi(connection, &apps[e], count - e) < 0) {
            verbose(VDBH, "ERROR while batch writing non_mpi applications to database.");
            PQfinish(connection);
            return EAR_ERROR;
        }
	}	 

	PQfinish(connection);

    return EAR_SUCCESS;

}
#endif
