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


#ifndef EAR_DLB_TALP_LIB_H_
#define EAR_DLB_TALP_LIB_H_


#include <common/states.h>
#include <common/system/time.h>


typedef struct earl_talp_s
{
	timestamp_t timestamp;	
  float   parallel_efficiency;
  float   load_balance;
} earl_talp_t;


/** Module's init function.
 * It currently starts the internal monitor to collect DLB node metrics.
 * It must be called by all processes. */
state_t earl_dlb_talp_init();


/** Module's ending-point function. */
state_t earl_dlb_talp_dispose();


state_t earl_dlb_talp_read(earl_talp_t *earl_talp_data);


#endif  // EAR_DLB_TALP_LIB_H_
