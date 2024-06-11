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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#include <common/config.h>
#include <common/output/verbose.h>
#include <common/output/debug.h>
#include <common/output/error.h>
#include <common/types/generic.h>
#include <common/system/popen.h>
#include <common/system/file.h>

// if enabled, we will support use cases such as
// srun -n X app (where app is not mpi)
// app is supossed to work with 1 master as in mpi 
#define APP_MULTIPROCESS_NO_MPI_SUPPORT 1

uint is_job_step_valid(job_id jid, job_id sid)
{
#if SCHED_SLURM
  char command[1024];
  popen_t sched_test;
  snprintf(command, sizeof(command),"/usr/bin/sacct -j %lu.%lu -o State", jid, sid);

  uint no_header = 2;
	// 'one_shot' creates a non-blocking pipe, so we then need
	// to nest to while loops. We could test by setting one_shot
	// to 1, so the popen module itself will block.
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
#endif
  /* Should we check more cases ? */
  return 1;



}

/* From SLURM srun doc
 * SLURM_STEP_TASKS_PER_NODE Number of processes per node within the step.
 * SLURM_PROCID The MPI rank (or relative process ID) of the current process.
 * SLURM_LOCALID Node local task ID for the process within a job.
*/

int get_my_sched_node_rank()
{
#if APP_MULTIPROCESS_NO_MPI_SUPPORT
#if SCHED_SLURM
    if (ear_getenv("SLURM_LOCALID") != NULL) return atoi(ear_getenv("SLURM_LOCALID"));
#endif
#if SCHED_PBS
		// WARNING: Not tested!
    if (ear_getenv("PBS_TASKNUM") != NULL) return atoi(ear_getenv("SLURM_LOCALID"));
#endif
#endif
    return 0;
}

int get_my_sched_global_rank()
{
	#if SCHED_SLURM
	#if APP_MULTIPROCESS_NO_MPI_SUPPORT
    if (ear_getenv("SLURM_PROCID") != NULL) return atoi(ear_getenv("SLURM_PROCID"));
	#endif
	#endif
	return 0;
}


int get_my_sched_group_node_size()
{
  #if SCHED_SLURM
	#if APP_MULTIPROCESS_NO_MPI_SUPPORT
    if (ear_getenv("SLURM_STEP_TASKS_PER_NODE") != NULL) return atoi(ear_getenv("SLURM_STEP_TASKS_PER_NODE"));
	#endif
  #endif
    return 1;
}

void sched_local_barrier(char *file,int N)
{
	debug("EARL[%d] waiting in %s for %d processes", getpid(), file, N);
	char lock_file[1024];
	int fd, fd_lock;
	
	snprintf(lock_file,sizeof(lock_file),"%s.lock", file); // Implements the exclusive acess
	// Create the file and accum 1 element
	while((fd_lock = file_lock_master(lock_file)) < 0); // lock
	fd = open(file, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
		// Update
		if (fd >= 0){
			int curr = 0, leido;
			leido = read(fd, &curr, sizeof(int));
			if (leido == sizeof(int)){ // Already have data
				curr++;
				lseek(fd,0, SEEK_SET);
			}else{										 // First process
				curr = 1;
			}
			write(fd,&curr, sizeof(int));
			close(fd);
		}else{
			error("sched_local_barrier timeout");
		}
	file_unlock_master(fd_lock, lock_file);


	debug("Waiting for processes");
	// Wait until the value is equal to N
	int out_tries = 0;
	int max_tries = 1000000;
	int value = -1;
	do{ // Out
		fd = open(file, O_RDONLY);
		// Read
		if (fd >= 0){
			read(fd, &value, sizeof(int));
			close(fd);
		}else{
			error("That should not happen! (%s)", strerror(errno));
		}
		// Compare
		if (value == N) return;
		out_tries++;
		usleep(100000);
	}while(out_tries < max_tries);
	error("sched_local_barrier timeout");
	return;	
}

void remove_local_barrier(char *file)
{
	unlink(file);
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
