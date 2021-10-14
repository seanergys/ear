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

#include <metrics/energy/energy_node.h>
#include <common/system/monitor.h>

//gcc -I ../../ -o test energy_node_test.c ../libmetrics.a ../../common/libcommon.a -ldl && sudo ./test $PWD/.. energy_nm.so

int main(int argc, char *argv[])
{
	cluster_conf_t conf;
	edata_t energy_mj_init, energy_mj_end;
	ulong energy_mj_diff;
	ehandler_t eh;
	state_t s;
	size_t node_size;
	uint   node_units;

	char epath[256];

	if (argc < 3) {
		return 0;
	}

	strcpy(conf.install.dir_plug, argv[1]);
	strcpy(conf.install.obj_ener, argv[2]);

	monitor_init();

	s = energy_init(&conf, &eh);
	printf("energy_init %d\n", s);

	energy_units(&eh, &node_units);
  energy_datasize(&eh, &node_size);

	printf("Energy units %u Energy size %u\n", node_units, node_size);
	
	energy_mj_init = (edata_t *) malloc(node_size);
	energy_mj_end  = (edata_t *) malloc(node_size);
	energy_mj_diff = (edata_t *) malloc(node_size);

	//
	s = energy_dc_read(&eh, energy_mj_init);
	printf("energy_dc_read %d\n", s);

	sleep(10);

	s = energy_dc_read(&eh, energy_mj_end);
	printf("energy_dc_read %d\n", s);

	energy_accumulated(NULL, &energy_mj_diff, energy_mj_init, energy_mj_end);

	printf("Total energy %lu mj. Avg Power %lf W\n", energy_mj_diff, ((double)energy_mj_diff/(double)(node_units*5)));

	//
	s = energy_dispose(&eh);
	printf("energy_dispose %d\n", s);

	return 0;
}
