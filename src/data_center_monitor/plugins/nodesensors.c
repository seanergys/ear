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

//#define SHOW_DEBUGS 1

#include <string.h>
#include <stdlib.h>
#include <common/output/debug.h>
#include <common/system/plugin_manager.h>
#include <common/config.h>
#include <common/system/popen.h>
#include <data_center_monitor/plugins/nodesensors.h>

// Separator character used by nodesensors command
static char sep=',';

// Use the absolute path because otherwise popen fails
static char *command="/opt/confluent/bin/nodesensors -c ";
//
// The order of sensors is hardcoded, we should implement a detection method
static char *sensor_name="\'Internal Humidity\',\'Internal Temperature\',\'Total Real Power\'";

// Hardcoded for testing, a discovery method must be implemented
static char *nodelist="p01r01a1pdu,p01r01b1pdu,p01r01e1pdu,p01r01f1pdu";
static popen_t nodesensor_p;
static char query[4096];



// Hardcoded for testing in four PDUs
#define NUM_PDUs    4


static cluster_sensors_t edcmon_info;

declr_up_get_tag ()
{
    *tag = "nodesensors";
    *tags_deps = NULL;
}

static void static_init()
{
    // Create command query
    snprintf(query,sizeof(query),"%s %s %s", command, nodelist, sensor_name);
    debug("Command to be executed: \'%s\'", query);
}

declr_up_action_init(_nodesensors)
{
    // We expose this information
    *data_alloc = &edcmon_info;

    debug("nodesensors init");
    static_init();
    return rsprintf("nodesensors action init(_nodesensors): received tag %s", tag);
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

static state_t static_read_power(uint *num_rows, uint expected_items)
{
	// Execute the query
    char *node;
    char *power, *temperature, *humidity;
    uint i = 0;

    uint no_header = 1;
    uint one_shot  = 0;
    if (popen_open(query, no_header, one_shot, &nodesensor_p) != EAR_SUCCESS){
       debug("popen failed\n");
       return EAR_ERROR;
    }
    // Parse data: An external loop is needed since popen returns if there is no data, then
    // in case the command has not reported yet the information, the inner loop will finish with
    // no data. It is a busy waiting loop assuming the command will not last for too much time
    while(i < expected_items){
    while(popen_read2(&nodesensor_p, sep,  "ssss", &node,&humidity,&temperature, &power)) {
            debug("RETURNED: node %s humidity %s temperature %s power %s\n", node, humidity, temperature,power);
	    strncpy(edcmon_info.edcmon_data[i].nodename, node, sizeof(edcmon_info.edcmon_data[i].nodename));

	    // Sensors units are still hardcoded. We could implement a discovery method
	    convert_double(&edcmon_info.edcmon_data[i].humidity, humidity);
	    convert_double(&edcmon_info.edcmon_data[i].temperature, temperature);
	    convert_double(&edcmon_info.edcmon_data[i].power, power);
	    edcmon_info.edcmon_data[i].timestamp  = time(NULL);
	    debug("nodesensors[%u] time %u node %s power %lf W, temperature %lf F , humidity %lf ", i, (uint)edcmon_info.edcmon_data[i].timestamp, edcmon_info.edcmon_data[i].nodename, edcmon_info.edcmon_data[i].power, edcmon_info.edcmon_data[i].temperature, edcmon_info.edcmon_data[i].humidity);
	    i++;
    }
    }
    *num_rows = i;
    // Close
    popen_close(&nodesensor_p);

}

declr_up_action_periodic(_nodesensors)
{
    	debug("nodesensors action");

    	// Get instanteneous power
    	static_read_power(&edcmon_info.num_items, NUM_PDUs);
    	debug("%u items detected", edcmon_info.num_items);

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
