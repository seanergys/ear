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


#include <common/output/verbose.h>
#include <common/states.h>
#include <library/metrics/dcgmi_lib.h>
#include <metrics/gpu/gpu.h>


int main(int argc, char **argv)
{
	verb(0, "Loading GPU API...");
	gpu_load(0); // 0 means no forcing any API
	gpu_init(no_ctx);

	VERB_SET_LV(2);
	
	verb(0, "Loading EARL DCGMI module...");
	if (state_fail(dcgmi_lib_load()))
	{
		verb(0, "Error loading dcgmi_lib module: %s", state_msg);
		return -1;
	}

	while(1)
	{
		sleep(4);
	}
}
