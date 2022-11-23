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

#include <common/system/file.h>
#include <slurm_plugin/slurm_plugin.h>
#include <slurm_plugin/slurm_plugin_environment.h>
#include <slurm_plugin/slurm_plugin_serialization.h>

// Externs
extern int _sp;
// Buffers
static char path_master[SZ_PATH];
static char path_step[SZ_PATH];
static char path_job[SZ_PATH];
static char command[SZ_PATH];
//
static int fd_master = -1;

#define PERMS000(T) 0
#define PERMS100(T) S_IR ## T
#define PERMS010(T) S_IW ## T
#define PERMS001(T) S_IX ## T
#define PERMS110(T) (PERMS100(T)) | (PERMS010(T))
#define PERMS111(T) (PERMS100(T)) | (PERMS010(T)) | (PERMS001(T))
#define PEXPA(...) __VA_ARGS__
#define PERMS(u, g, o) PEXPA(PERMS ## u)(USR) | PEXPA(PERMS ## g)(GRP) | PEXPA(PERMS ## o)(OTH)

int lock_master(char *path_tmp, int job_id)
{
    plug_verbose(_sp, 3, "function lock_master");

    // Creating temp folder
    if ((access(path_tmp, W_OK) != 0) && (errno == ENOENT)) {
        mkdir(path_job, PERMS(111,000,000)); 
    }
    // Creating erun folder
    sprintf(path_job, "%s/erun", path_tmp);
    mkdir(path_job, PERMS(111,110,110)); 
    // Creating job folder
    sprintf(path_job, "%s/erun/%d", path_tmp, job_id);
    mkdir(path_job, PERMS(111,110,110));
    // Getting the master lock
    sprintf(path_master, "%s/erun/%d/master.lock", path_tmp, job_id);
	plug_verbose(_sp, 3, "Trying to get the lock %s", path_master);
    if ((fd_master = file_lock_master(path_master)) < 0) {
	    plug_error(_sp, "while taking lock_master %s", strerror(errno));
    }
    // Returning FD means it got the unique master lock
    return fd_master >= 0;
}

int lock_job(char *path_tmp, int job_id, int step_id)
{
	plug_verbose(_sp, 3, "function lock_step (%d,%d)", job_id, step_id);
	// Forming erun.%job_id.lock path
	sprintf(path_job, "%s/erun/%d/job.lock", path_tmp, job_id);
    // Locking in node processes:
    // 1. Try to take master.lock
    //      2. If taken master.lock
    //          2.1. Test job.lock
    //                SLURM_STEP_ID -> Set step_id to SLURM_STEP_ID
    //               !SLURM_STEP_ID
    //                   job.lock -> Read the %step_id + 1
    //                  !job.lock -> Set %step_id to 0
    //          2.2. Remove job.lock
    //          2.3. Create job.lock with the %step_id
    //          2.4. Create step.lock
    //          2.5. Write %step_id in step.lock
    //          2.6. Execute the application
    //          2.7. Remove master.lock
    //          2.8. Remove step.lock
    //          2.9. Launch a cleanning command
    //          2.10. Create job.lock with the %step_id
    //      3. If not taken master.lock
    //          3.1 Spin while can't access step.lock
    //          3.2 Read the %step_id in step.lock
	if (step_id == 0) {
		if (state_ok(ear_file_read(path_job, (char *) &step_id, sizeof(int)))) {
        	    	plug_verbose(_sp, 2, "read %d step_id in the file '%s'", step_id, path_job);
			step_id += 1;
		} else {
			// In case the reading fails, force step_id to 0
			step_id = 0;
		}
	}
	return step_id;
}

int unlock_step(char *path_tmp, int job_id, int step_id)
{
    plug_verbose(_sp, 3, "function unlock_step");
    // 1. Form the step.lock path
    sprintf(path_step, "%s/erun/%d/step.lock", path_tmp, job_id);
    // 2. Create and write step.lock
    ear_file_write(path_step, (char *) &step_id, sizeof(int));
    return step_id;
}

int spinlock_step(char *path_tmp, int job_id, int step_id)
{
    plug_verbose(_sp, 3, "function spinlock_step");
    // 1. Form the step.lock path
    sprintf(path_step, "%s/erun/%d/step.lock", path_tmp, job_id);
    // 2. While !step.lock file, spinlock
    plug_verbose(_sp, 3," Waiting for step lock file %s", path_step);
    while (access(path_step, F_OK) != 0);
    // 3. Read the %step_id contained in step.lock
    ear_file_read(path_step, (char *) &step_id, sizeof(int));
    // 4. Return %step_id
    plug_verbose(_sp, 2, "read %d step_id in the file '%s'", step_id, path_job);
    return step_id;
}

int lock_clean(char *path_tmp, int job_id, int step_id)
{
	plug_verbose(_sp, 3, "function lock_clean");
	// 1. Creating the cleaning command
	sprintf(command, "rm -f %s/erun/%d/*.lock 2> /dev/null", path_tmp, job_id);
	// 2. Remove master.lock
	file_unlock_master(fd_master, path_master);
	// 3. Remove step.lock
	ear_file_clean(path_step);
	// 5. Remove job.lock
	ear_file_clean(path_job);
	// 6. Launch cleaning command to remove other files.
	if (system(command)) {
        //
    }
	// 5. Write again job.lock 
	ear_file_write(path_job, (char *) &step_id, sizeof(int));
	return 0;
}

int folder_clean(char *path_tmp, int job_id)
{
	// 1. Creating the cleaning command
	sprintf(command, "rm -rf %s/erun/%d 2> /dev/null", path_tmp, job_id);
    if (system(command)) {
        //
    }
    return 0;
}

int all_clean(char *path_tmp)
{
    xsprintf(command, "rm -rf %s/erun &> /dev/null", path_tmp);
		plug_verbose(_sp, 3, "Executing %s", command);
    if (system(command)) {
        //
    }
    return 0;
}
