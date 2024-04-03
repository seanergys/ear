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

#ifndef _NODESENSORS_H_
#define _NODESENSORS_H_

#include <time.h>
#include <sys/types.h>

#define NODESENSORS_TYPE 0

typedef enum {
  POWER_W      = 0,
  HUMIDITY_PERC,
  TEMPERATURE_F,
}esensor_t;

// MAX_SOURCES must be the maximum number of PDUs, IPs etc.
#define MAX_SOURCES 128


typedef struct nodesesor{
        time_t    timestamp;
        char      nodename[MAX_PATH_SIZE];
        double    power;
	double    humidity;
	double    temperature;
}nodesensor_t;

typedef struct cluster_sensors{
	nodesensor_t    edcmon_data[MAX_SOURCES];
	uint 		num_items;
}cluster_sensors_t;

#endif
