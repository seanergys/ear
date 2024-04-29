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
#include <stdlib.h>
#include <unistd.h>
#include <common/output/debug.h>
#include <common/system/plugin_manager.h>
#include <common/config.h>
#include <common/system/popen.h>
#include <data_center_monitor/plugins/conf.h>
#include <data_center_monitor/plugins/nodesensors.h>

// Separator character used by nodesensors command
static char sep=',';

// EDCMON configuration : list of pdus, sensors, etc
static conf_t                   *conf;
static edcmon_t * edcmons = NULL;
static uint       num_edcmons = 0;
static uint*      enabled;
static uint       total_enabled = 0;

// Use the absolute path because otherwise popen fails
static char *command="/opt/confluent/bin/nodesensors -c ";
//
// The order of sensors is hardcoded, we should implement a detection method
static char *sensor_name_old="\'Internal Humidity\',\'Internal Temperature\',\'Total Real Power\'";
static char **sensor_name;

// Hardcoded for testing, a discovery method must be implemented
static char *nodelist_old="p01r01a1pdu,p01r01b1pdu,p01r01e1pdu,p01r01f1pdu";
static char **nodelist;
static popen_t nodesensor_p;
static pdu_t *type;
static uint  *num_pdus;


#define QUERY_SIZE 4096
static char **query;
static char my_hostname[256];





static cluster_sensors_t edcmon_info;

declr_up_get_tag ()
{
    *tag = "nodesensors";
    *tags_deps = "!conf";
}

static void static_init(uint i)
{
		debug("Preparing query [%u]........", i);
    // Create command query
    snprintf(query[i],QUERY_SIZE,"%s %s %s", command, nodelist_old, sensor_name_old);
    debug("OLD Command to be executed: \'%s\'", query[i]);
    snprintf(query[i],QUERY_SIZE,"%s %s %s", command, nodelist[i], sensor_name[i]);
    debug("Current Command to be executed: \'%s\'", query[i]);

}

declr_up_action_init(_nodesensors)
{
    // We expose this information
    *data_alloc = &edcmon_info;

    debug("nodesensors init");
    for (uint i = 0; i < total_enabled; i++) static_init(i);
    return rsprintf("nodesensors action init(_nodesensors): received tag %s", tag);
}


static void compute_num_puds(char *list, uint *elems)
{
	char *next;
	*elems = 0;

	debug("Parsing list %s", list);
	if (list == NULL) 			return;
	if (strlen(list) == 0) 	return;

	next = strtok(list, ",");
	*elems = 1;
	while(next != NULL){
		*elems = *elems + 1;
		next = strtok(NULL, ",");
	}
	debug("%u pdus found in %s", *elems, list);
}

/* We use the configuration from conf plugin (ear.conf) */
declr_up_action_init(_conf)
{
    conf = (conf_t *) data;

		// We get my name to look for my queries
		if (gethostname(my_hostname, sizeof(my_hostname)) < 0) {
    	error("Error getting node name (%s)", strerror(errno));
  	}

  	strtok(my_hostname, ".");

		edcmons     = conf->cluster.edcmon_tags;
		num_edcmons = conf->cluster.num_edcmons_tags;	
	
		// server is the edcmon hostname	
		strncpy(edcmon_info.server, my_hostname, sizeof(edcmon_info.server));

		if (num_edcmons == 0){
			return rsprintf("[D] No EDCMONS sensors configured, plugin disabled");
		}


		debug("Nodesensors: %u configurations detected", num_edcmons);
		enabled = calloc(num_edcmons, sizeof(uint));
		for (uint i = 0; i < num_edcmons; i++){
			// This version only support 1 tag
			enabled[i] = ((strcmp(edcmons[i].host, my_hostname) == 0) || (strcmp(edcmons[i].host, "any") == 0));
			debug("EDMON[%u] host %s my host %s, enabled %u", i, edcmons[i].host, my_hostname, enabled[i]);
			total_enabled++;
		}

		// This version only supports 1 set of query per node
		if (!total_enabled){
			return rsprintf("[D] Number of actions 0 or not supported %u, plugin disabled", total_enabled);
		}

		query = (char **)calloc(total_enabled, sizeof(char *));
		if (!query) return rsprintf("[D] Not enough memory, disabled");
		num_pdus    = (uint *)calloc(total_enabled, sizeof(uint));
		if (!num_pdus) return rsprintf("[D] Not enough memory, disabled");


		for (uint i = 0; i < total_enabled; i++) query[i] = (char *)calloc(QUERY_SIZE, sizeof(char));
		// Error control

		sensor_name = (char **)calloc(total_enabled, sizeof(char *));
		nodelist    = (char **)calloc(total_enabled, sizeof(char *));
		type        = (pdu_t *)calloc(total_enabled, sizeof(pdu_t *));
		// Error control
		for (uint i = 0; i < num_edcmons; i++) if (enabled[i]) sensor_name[i] = edcmons[i].sensors_list;
		for (uint i = 0; i < num_edcmons; i++) if (enabled[i]) nodelist[i]    = edcmons[i].pdu_list;
		for (uint i = 0; i < num_edcmons; i++) if (enabled[i]) type[i]        = edcmons[i].type;

		for (uint i = 0; i < total_enabled; i++)  compute_num_puds(nodelist[i], &num_pdus[i]);

		uint total_sources = 0;
	 	for (uint i = 0; i < total_enabled; i++)
		{
			total_sources += num_pdus[i];
			verbose(0,"[%u] Sensor list %s pdus %s type %u", i, sensor_name[i] , nodelist[i], type[i]);
		}	
		if (total_sources > MAX_SOURCES){
			verbose(0,"[D] ERROR, max sources exceeded (curr %u limit %u)", total_sources,MAX_SOURCES);		
		}


    return rsprintf("nodesensors action init(_nodesensors): received tag %s, %u groups enabled", tag, total_enabled);
}


static void convert_double(double *info, char *data_in)
{
	if (strcmp(data_in, "N/A") == 0){
		// 0 means not available. Should we use another value? Can be the same for all the sensors? 
		*info = 0;
	}else{
		*info = strtod(data_in, NULL);
	}
}

static state_t static_read_power(uint i, uint *num_rows, uint expected_items)
{
	// Execute the query
    char *node;
    char *power, *temperature, *humidity;
    uint it = 0;
		uint idx = edcmon_info.num_items;

    uint no_header = 1;
    uint one_shot  = 0;

		#if TEST
		return EAR_SUCCESS;	
		#endif
    if (popen_open(query[i], no_header, one_shot, &nodesensor_p) != EAR_SUCCESS){
       debug("popen failed\n");
       return EAR_ERROR;
    }
    // Parse data: An external loop is needed since popen returns if there is no data, then
    // in case the command has not reported yet the information, the inner loop will finish with
    // no data. It is a busy waiting loop assuming the command will not last for too much time
    while(it < expected_items){
    while(popen_read2(&nodesensor_p, sep,  "ssss", &node,&humidity,&temperature, &power)) {
            debug("RETURNED: node %s humidity %s temperature %s power %s\n", node, humidity, temperature,power);
	    strncpy(edcmon_info.edcmon_data[it+idx].nodename, node, sizeof(edcmon_info.edcmon_data[it+idx].nodename));

	    // Sensors units are still hardcoded. We could implement a discovery method
	    convert_double(&edcmon_info.edcmon_data[it+idx].humidity, humidity);
	    convert_double(&edcmon_info.edcmon_data[it+idx].temperature, temperature);
	    convert_double(&edcmon_info.edcmon_data[it+idx].power, power);
	    edcmon_info.edcmon_data[it+idx].timestamp  = time(NULL);
	    debug("nodesensors[%u] time %u node %s power %lf W, temperature %lf F , humidity %lf ", it+idx, (uint)edcmon_info.edcmon_data[it+idx].timestamp, edcmon_info.edcmon_data[it+idx].nodename, edcmon_info.edcmon_data[it+idx].power, edcmon_info.edcmon_data[it+idx].temperature, edcmon_info.edcmon_data[it+idx].humidity);
	    it++;
    }
    }
    *num_rows = it;
    // Close
    popen_close(&nodesensor_p);
		return EAR_SUCCESS;
}

declr_up_action_periodic(_nodesensors)
{
    	debug("nodesensors action");
			uint curr_items;

			edcmon_info.num_items = 0;
    	// Get instanteneous metrics
    	for (uint i = 0; i < total_enabled; i++){
    		static_read_power(i, &curr_items, num_pdus[i]);
    		debug("TYPE[%u] %u items detected", i, curr_items);
				edcmon_info.num_items += curr_items;
			}

    	return rsprintf("nodesensors action periodic(_nodesensors): received tag %s", tag);
}


#if TEST
void main(int argc, char *argv[])
{
	uint num_items;
	static_init();
	static_read_power(&num_items, NUM_PDUs);
	printf("%u items detected", num_items);
}
#endif
