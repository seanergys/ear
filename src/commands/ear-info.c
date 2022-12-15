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

#include <stdio.h>
#include <stdlib.h>
#include <common/config.h>
#include <common/states.h>
#include <common/types/version.h>
#include <common/types/configuration/cluster_conf.h>
#include <common/system/user.h>

#define DEFAULT_EARL 1

void Usage(char *cprog)
{
  printf("Usage: %s [options]\n"\
    "--node-conf[=nodename]\n"\
    "--help\n", cprog);
 exit(0);
}

int main(int argc,char *argv[])
{
	cluster_conf_t my_cluster;
	char ver[128];
	char ear_path[256];
  char *nodename = NULL;


  int option_idx = 0;
  static struct option long_options[] = {
  {"node-conf",      required_argument, 0, 'n'},
  {"help",          no_argument, 0, 'h'},
  };
  int c;
  c = getopt_long(argc, argv, "n:h", long_options, &option_idx);

  if (c == -1){
    Usage(argv[0]);
  }

	switch(c)
	{
			case 'n': 
        if (optarg){ 
          nodename = optarg; 
          printf("nodeconf with %s\n", nodename);
        }
        break;
      case 'h': Usage(argv[0]);break;
  }


	if (get_ear_conf_path(ear_path)==EAR_ERROR){
			printf("Error getting ear.conf path, load the ear module\n");
			exit(0);
	}
	read_cluster_conf(ear_path,&my_cluster);
	version_to_str(ver);
	printf("EAR version %s\n",ver);
	#if USE_GPUS
	printf("EAR installed with GPU support\n");
	#else
	printf("EAR installed without GPU support\n");
	#endif
	if (my_cluster.eard.force_frequencies){
		printf("EAR controls node CPU frequencies");	
	}
	printf("Default cluster policy is %s\n",my_cluster.power_policies[my_cluster.default_policy].name);
	printf("EAR optimization  by default set to %d\n",DEFAULT_EARL);		


  if (!is_privileged_command(&my_cluster)) return 0;

  if (nodename != NULL){
    my_node_conf_t *my_node_conf = NULL;
    my_node_conf = get_my_node_conf(&my_cluster, nodename); 
    if (my_node_conf == NULL){
      printf("Error, Node %s not found\n", nodename);
      exit(0);
    }
    printf("\n\nInformation for node %s..................\n", nodename);
    print_my_node_conf(my_node_conf);
    printf("............................................\n");
  }


	return 0;
}

