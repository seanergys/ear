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
#include <string.h>

#include <common/config.h>
#include <metrics/common/popen.h>

uint is_job_step_valid(job_id jid, job_id sid)
{
  char command[1024];
  popen_t sched_test;
  snprintf(command, sizeof(command),"sacct -j %lu.%lu -o State -n", jid, sid);

  uint no_header = 1;
  uint one_shot  = 0;
  char sep = ' ';
  char *job_sched_state;

  if (popen_open(command, no_header, one_shot, &sched_test) != EAR_SUCCESS){
       debug("popen for scheduler test failedfailed\n");
       return EAR_ERROR;
  }
  uint expected_items = 1;
  uint i = 0;
  while(i < expected_items){
  while(popen_read2(&sched_test, sep,  "s", &job_sched_state)) {
    i++;
  }
  }

  popen_close(&sched_test);

  if (strcmp(job_sched_state, "RUNNING") == 0) return 1;
  if (strcmp(job_sched_state, "COMPLETED") == 0) return 0;
  if (strcmp(job_sched_state, "CANCELED") == 0) return 0;

  /* Should we check more cases ? */
  return 1;



}


