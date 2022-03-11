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

//#define SHOW_DEBUGS 1

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <common/sizes.h>
#include <common/output/verbose.h>
#include <metrics/common/msr.h>

#define MSR_MAX	4096

static pthread_mutex_t lock_gen = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lock_cpu[MSR_MAX];
static int counters[MSR_MAX];
static int init[MSR_MAX];
static int fds[MSR_MAX];

static struct error_s {
	char *lock;
	char *cpu_invalid;
	char *cpu_uninitialized;
} Error = {
	.lock        = "error during pthread_mutex",
	.cpu_invalid = "cpu number is out of range",
	.cpu_uninitialized = "cpu MSR is not initialized",
};

#define return_unlock_msg(s, message, lock) \
	pthread_mutex_unlock(lock); \
	return_msg(s, message);

#define return_unlock(s, lock) \
	pthread_mutex_unlock(lock); \
	return s;

state_t msr_test(topology_t *tp, mode_t mode)
{
	state_t s = EAR_SUCCESS;
	int cpu, b;
    int fd;

    if (!tp->initialized) {
		return_msg(EAR_ERROR, Generr.input_uninitialized);
	}
	for (cpu = 0; cpu < tp->cpu_count; ++cpu) {
        fd = static_open(cpu, mode);
        close(fd);

        if (fd < 0) {
            if (errno == EACCES) {
                return_msg(EAR_NO_PERMISSIONS, strerror(errno));
            }
            return_msg(EAR_ERROR, strerror(errno));
        }
	}
	return EAR_SUCCESS;
}

state_t msr_scan(topology_t *tp, off_t *regs, uint regs_count, ullong andval, off_t *offs)
{
	ullong content;
	off_t zero;
	uint c, r;
	state_t s;

	for (c = 0; c < tp->cpu_count; ++c) {
		if (state_fail(msr_open(tp->cpus[c].id))) {
			break;
		}
		// Initializing
		offs[c] = 0;
		zero    = 0;
		// Reading registers
		for (r = 0; r < regs_count; ++r) {
			if (state_fail(s = msr_read(tp->cpus[c].id, &content, sizeof(content), regs[r]))) {
				return s;
			}
			// If a register is using the same event, use that
			if (content & andval) {
				offs[c] = regs[r];
				continue;
			}
			if (!content && !zero) {
				zero = regs[r];
			}
		}
		// If free register is not found, use the first in the lis
		if (!offs[c] && !zero) {
			offs[c] = regs[0];
		}
		// If a free register is found, use that
		if (!offs[c] && zero) {
			offs[c] = zero;
		}

		msr_close(msr_open(tp->cpus[c].id));
	}
}

int static_open(uint cpu, mode_t mode)
{
    char msr_file_name[SZ_PATH_KERNEL];
    int fd;
    #if MSR_SAFE
    sprintf(msr_file_name, "/dev/cpu/%d/msr_safe", cpu);
    #else
    sprintf(msr_file_name, "/dev/cpu/%d/msr", cpu);
    #endif
    fd = open(msr_file_name, mode);
    if (mode == MSR_WR) {
        debug("Opened MSR %s (MSR_WR): fd %d (%s)", msr_file_name, fd, strerror(errno));
    } else {
        debug("Opened MSR %s (MSR_RD): fd %d (%s)", msr_file_name, fd, strerror(errno));
    }
    return fd;
}

state_t msr_open(uint cpu)
{
	if (cpu >= MSR_MAX) {
		return_msg(EAR_BAD_ARGUMENT, Error.cpu_invalid);
	}
	// General exclusion
	while (pthread_mutex_trylock(&lock_gen));
	if (init[cpu] == 0) {
		if (pthread_mutex_init(&lock_cpu[cpu], NULL) != 0) {
			return_unlock_msg(EAR_ERROR, Error.lock, &lock_gen);
		}
	}
	init[cpu] = 1;
	pthread_mutex_unlock(&lock_gen);
	// CPU exclusion
	while (pthread_mutex_trylock(&lock_cpu[cpu]));
	if (counters[cpu] == 0) {
        if ((fds[cpu] = static_open(cpu, MSR_WR)) < 0) {
            fds[cpu] = static_open(cpu, MSR_RD);
        }
        if (fds[cpu] < 0) {
            if (errno == EACCES) {
                return_unlock_msg(EAR_NO_PERMISSIONS, strerror(errno), &lock_cpu[cpu]);
            }
            return_unlock_msg(EAR_ERROR, strerror(errno), &lock_cpu[cpu]);
        }
	}
	counters[cpu] += 1;

	return_unlock(EAR_SUCCESS, &lock_cpu[cpu]);
}

state_t msr_close(uint cpu)
{
	if (cpu >= MSR_MAX) {
		return_msg(EAR_BAD_ARGUMENT, Error.cpu_invalid);
	}
	if (counters[cpu] == 0) {
		return_msg(EAR_NOT_INITIALIZED, Error.cpu_uninitialized);
	}
	while (pthread_mutex_trylock(&lock_cpu[cpu]));
	if (counters[cpu] == 0) {
		return_unlock_msg(EAR_NOT_INITIALIZED, Error.cpu_uninitialized, &lock_cpu[cpu]);
	}
	counters[cpu] -= 1;
	if (counters[cpu] == 0) {
		close(fds[cpu]);
		fds[cpu] = -1;
	}
	return_unlock(EAR_SUCCESS, &lock_cpu[cpu]);
}

state_t msr_read(uint cpu, void *buffer, size_t size, off_t offset)
{
	if (cpu >= MSR_MAX) {
		return_msg(EAR_BAD_ARGUMENT, Error.cpu_invalid);
	}
	if (counters[cpu] == 0 || fds[cpu] < 0) {
		return_msg(EAR_NOT_INITIALIZED, Error.cpu_uninitialized);
	}
	#ifdef MSR_LOCK
	while (pthread_mutex_trylock(&lock_cpu[cpu]));
    #endif
    debug("Reading MSR of CPU%d (fd %d), address %p", cpu, fds[cpu], offset);
	if (pread(fds[cpu], buffer, size, offset) != size) {
		#ifdef MSR_LOCK
		return_unlock_msg(EAR_ERROR, strerror(errno), &lock_cpu[cpu]);
		#else
		return_msg(EAR_ERROR, strerror(errno));
		#endif
	}
	#ifdef MSR_LOCK
	return_unlock(EAR_SUCCESS, &lock_cpu[cpu]);
	#else
	return EAR_SUCCESS;
	#endif
}

state_t msr_write(uint cpu, const void *buffer, size_t size, off_t offset)
{
	if (cpu >= MSR_MAX) {
		return_msg(EAR_BAD_ARGUMENT, Error.cpu_invalid);
	}
	if (counters[cpu] == 0 || fds[cpu] < 0) {
		return_msg(EAR_NOT_INITIALIZED, Error.cpu_uninitialized);
	}
	#ifdef MSR_LOCK
	while (pthread_mutex_trylock(&lock_cpu[cpu]));
	#endif
	if (pwrite(fds[cpu], buffer, size, offset) != size) {
		#ifdef MSR_LOCK
		return_unlock_msg(EAR_ERROR, strerror(errno), &lock_cpu[cpu]);
		#else
		return_msg(EAR_ERROR, strerror(errno));
		#endif
	}
	#ifdef MSR_LOCK
	return_unlock(EAR_SUCCESS, &lock_cpu[cpu]);
	#else
	return EAR_SUCCESS;
	#endif
}

state_t msr_print(topology_t *tp, off_t offset)
{
	ulong value_c;
	ulong value_t;
	state_t s_c;
	state_t s_t;
	int id_c;
	int id_t;
	int i;

	for (i = 0; i < tp->cpu_count; ++i)
	{
		if (tp->cpus[i].is_thread) {
			continue;
		}
		id_c = tp->cpus[i].id;
		id_t = tp->cpus[i].sibling_id;
		value_c = 0LU;
		value_t = 0LU;
		// Open both core and thread
		s_c = msr_open(id_c);
		s_t = msr_open(id_t);
		// Reading both values
		if (state_ok(s_c)) msr_read(id_c, &value_c, sizeof(ulong), offset);
		if (state_ok(s_t)) msr_read(id_t, &value_t, sizeof(ulong), offset);
		// Printing
		verbose(0, "%d/%d: %lu %lu", id_c, id_t, value_c, value_t);
		// Closing
		if (state_ok(s_c)) msr_close(id_c);
		if (state_ok(s_t)) msr_close(id_t);
	}

	return EAR_SUCCESS;
}

void msr_inspect(topology_t *tp, int cpu, off_t *offsets, int fd)
{
	int max_cpu = 0;
	int min_cpu = 0;
	ullong value;
	int c, o;
	
	if (cpu == all_cpus) {
		max_cpu = tp->cpu_count;
	} else if (cpu == all_cores) {
		max_cpu = tp->core_count;
	} else {
		max_cpu = cpu+1;
		min_cpu = cpu;
	}

	for (c = min_cpu; c < max_cpu; ++c)
	{
		if (init[c] == 0) {
			msr_open(c);
		}
		// Through all the offsets
		o = 0;

		while (offsets[o] != 0LLU) {
			value = 0LLU;
			msr_read(c, &value, sizeof(ullong), offsets[o]);
			dprintf(fd, "%09llx ", value);
			++o;
		}
		dprintf(fd, "\n");
	}
}
