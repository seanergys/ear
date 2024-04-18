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


#include <metrics/gpu/gpuprof.h>

#include <stdlib.h>
#include <stdio.h>
#include <metrics/common/apis.h>


#define EVENT_SET_CNT 3 // The number of sets you expect to test.


static apinfo_t             info;

static gpuprof_t            *m1;
static gpuprof_t            *m2;
static gpuprof_t            *mD;

// static const gpuprof_evs_t *evs;
// static uint                 evs_count;


int main(int argc, char *argv[])
{
	gpuprof_load(0);
	gpuprof_get_info(&info);
	apinfo_tostr(&info);
	printf("LOADED: %s %s %s %d\n", info.layer, info.api_str, info.granularity_str, info.devs_count);

	gpuprof_data_alloc(&m1);
	gpuprof_data_alloc(&m2);
	gpuprof_data_alloc(&mD);
	printf("ALLOCATED DATA\n");

	/*
	gpuprof_events_get(&evs, &evs_count);
	for (i = 0; i < evs_count; ++i) {
		printf("EVENT_%d: id.%02u %s\n", i, evs[i].id, evs[i].name);
	}
	*/

	char sets[EVENT_SET_CNT][512];

	for (int set_idx = 0; set_idx < EVENT_SET_CNT; set_idx++)
	{
		printf("Enter a comma-separated list of EVENTS set %d: ", set_idx);
		scanf("%s", sets[set_idx]);

		if (!set_idx)
		{
			gpuprof_events_set(0, sets[set_idx]);
			printf("EVENTS SET %d: %s\n", set_idx, sets[set_idx]);
		}
	}

	gpuprof_read(m1);
	sleep(2);

	int curr_set = 0;
	int count = 0;
	while(1)
	{
		gpuprof_read_copy(m2, m1, mD);
		for (int d = 0; d < info.devs_count; ++d) {
			for (int m = 0; m < mD[d].values_count; ++m) {
				printf("METRIC_D%d_M%d: %lf\n", d, m, mD[d].values[m]);
			}
		}

		count++;
		if (count % EVENT_SET_CNT == 0)
		{
			gpuprof_events_unset(0);

			curr_set = (curr_set + 1) % EVENT_SET_CNT;
			gpuprof_events_set(0, sets[curr_set]);

			printf("EVENTS SET %d: %s\n", curr_set, sets[curr_set]);
		}
		sleep(2);
	}

	return 0;
}
