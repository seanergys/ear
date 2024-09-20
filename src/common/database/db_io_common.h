/*
*
* This program is part of the EAR software.
*
* EAR provides a dynamic, transparent and ligth-weigth solution for
* Energy management. It has been developed in the context of the
* Barcelona Supercomputing Center (BSC)&Lenovo Collaboration project.
*
* 
* BSC Contact   mailto:ear-support@bsc.es
* 
*
* EAR is an open source software, and it is licensed under both the BSD-3 license
* and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
* and COPYING.EPL files.
*/

#ifndef DB_IO_COMMON_H
#define DB_IO_COMMON_H
#include <common/config.h>

//
#define EAR_TYPE_APPLICATION    1
#define EAR_TYPE_LOOP           2

#define PERIODIC_AGGREGATION_ARGS   4
#define EAR_EVENTS_ARGS             6
#define POWER_SIGNATURE_ARGS        9
#define APPLICATION_ARGS            6
#define LOOP_ARGS                   9
#define JOB_ARGS                    17



#if USE_GPUS 

//Signatures
#define FULL_SIGNATURE_ARGS              29
#define SIMPLE_SIGNATURE_ARGS            16

//Periodic_metrics
#define FULL_PERIODIC_METRIC_ARGS        11
#define SIMPLE_PERIODIC_METRIC_ARGS       6

#else //no USE_GPU
      
//Signatures
#define FULL_SIGNATURE_ARGS              27
#define SIMPLE_SIGNATURE_ARGS            14

//Periodic_metrics
#define FULL_PERIODIC_METRIC_ARGS        10
#define SIMPLE_PERIODIC_METRIC_ARGS       6

#endif


#define FULL_AVG_SIGNATURE_ARGS          25
#define SIMPLE_AVG_SIGNATURE_ARGS        15

#if USE_GPUS
#define GPU_SIGNATURE_ARGS 6
#endif

#endif
