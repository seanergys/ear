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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>


#include <common/config.h>
#include <common/system/popen.h>

uint is_job_step_valid(job_id jid, job_id sid)
{
  char command[1024];
  popen_t sched_test;
  snprintf(command, sizeof(command),"/usr/bin/sacct -j %lu.%lu -o State", jid, sid);

  uint no_header = 2;
  uint one_shot  = 0;
  char sep = ' ';
  char *job_sched_state;

	printf("Executing: %s\n", command);

  if (popen_open(command, no_header, one_shot, &sched_test) != EAR_SUCCESS){
       debug("popen for scheduler test failedfailed\n");
       return 0;
  }
  uint expected_items = 1;
  uint i = 0;
  while(i < expected_items){
  while(popen_read2(&sched_test, sep,  "s", &job_sched_state)) {
		printf("Detected state '%s'\n", job_sched_state);
    i++;
  }
  }

  popen_close(&sched_test);

  if (strcmp(job_sched_state, "RUNNING") == 0) return 1;
  if (strcmp(job_sched_state, "COMPLETED") == 0) return 0;
  if (strcmp(job_sched_state, "CANCELLED") == 0) return 0;
  if (strcmp(job_sched_state, "FAILED") == 0) return 0;
  if (strcmp(job_sched_state, "EXITED") == 0) return 0;
  if (strcmp(job_sched_state, "TIMEOUT") == 0) return 0;

  /* Should we check more cases ? */
  return 1;



}

#if 0

// These functions were used by EARD , not longer used
static int is_running(char *status_text)
{
  if (strlen(status_text) == 0) return 0;
  if (strstr(status_text, "COMPLETED") != NULL) return 0;
  if (strstr(status_text, "FAILED")    != NULL) return 0;
  if (strstr(status_text, "EXITED")    != NULL) return 0;
  if (strstr(status_text, "TIMEOUT")   != NULL) return 0;
  if (strstr(status_text, "CANCELLED") != NULL) return 0;
  return 1;
}


static int read_word(int fd, char * buff, int limit)
{
  int t = 0, r = 0;
  while(((r = read(fd, &buff[t], (limit-t))) > 0) && (t < limit)){
    t += r;
  }
  return t;
}

static int is_job_running(char *tmp, int jobid)
{
  /* sacct -j 1512221.4294967291 -o state -n*/
  char path_file[GENERIC_NAME], pid_str[128], status[128];
  int fd, size;

  sprintf(path_file,"%s/jobid_status_file", tmp);
  fd = open(path_file, O_RDWR|O_CREAT|O_TRUNC,S_IWUSR|S_IRUSR);
  pid_t status_pid = fork();
  if (status_pid == 0 ){
    dup2(fd,1);
    sprintf(pid_str,"%d.batch", jobid);
    debug("job id: %d", jobid);
    if (execlp("sacct", "sacct", "-j", pid_str,"-o","state", "-n", NULL) < 0) error( "Error execlp (%s)", strerror(errno)); // perror ("error execlp");
    exit(1);
  }
  if (waitpid(status_pid,  NULL, 0) < 0 ) error("Error waitpid (%s)", strerror(errno));
  if (lseek(fd, 0 , SEEK_SET) < 0 ) error("Error lseek (%s)", strerror(errno));
  size = read_word(fd, status, 128);
  status[size] = '\0';
  return is_running(status);
}

#endif
