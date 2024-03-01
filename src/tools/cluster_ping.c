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
#include <common/types/configuration/cluster_conf.h>
#include <common/messaging/msg_internals.h>
#include <daemon/remote_api/eard_rapi_internals.h>
#include <daemon/remote_api/eard_rapi.h>


void seq_ear_cluster_ping(cluster_conf_t *my_cluster_conf);

int main(int argc,char *argv[])
{
  cluster_conf_t my_cluster;
  char ear_path[256];
  if (get_ear_conf_path(ear_path)==EAR_ERROR){
    printf("Error getting ear.conf path\n");
    exit(0);
  }
  read_cluster_conf(ear_path,&my_cluster);
  seq_ear_cluster_ping(&my_cluster);
}

void seq_ear_cluster_ping(cluster_conf_t *my_cluster_conf)
{
  int i, j, k, rc;
  char node_name[256];
  printf("Sendind ping to all nodes");
  //it is always secuential as it is only used for debugging purposes
  for (i=0;i< my_cluster_conf->num_islands;i++){
    for (j = 0; j < my_cluster_conf->islands[i].num_ranges; j++)
    {
      for (k = my_cluster_conf->islands[i].ranges[j].start; k <= my_cluster_conf->islands[i].ranges[j].end; k++)
      {
        if (k == -1)
          sprintf(node_name, "%s", my_cluster_conf->islands[i].ranges[j].prefix);
        else if (my_cluster_conf->islands[i].ranges[j].end == my_cluster_conf->islands[i].ranges[j].start)
          sprintf(node_name, "%s%u", my_cluster_conf->islands[i].ranges[j].prefix, k);
        else {
          if (k < 10 && my_cluster_conf->islands[i].ranges[j].end > 10)
            sprintf(node_name, "%s0%u", my_cluster_conf->islands[i].ranges[j].prefix, k);
          else
            sprintf(node_name, "%s%u", my_cluster_conf->islands[i].ranges[j].prefix, k);
        }
        if (strlen(my_cluster_conf->net_ext) > 0) {
          strcat(node_name, my_cluster_conf->net_ext);
        }
        rc=remote_connect(node_name,my_cluster_conf->eard.port);
        if (rc<0){
          error("Error connecting with node %s", node_name);
        }else{
          printf("Node %s ping!\n", node_name);
          if (!ear_node_ping()) error("Error doing ping for node %s", node_name);
          remote_disconnect();
        }
      }
    }
  }
}


