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

#ifndef COMMON_SYSTEM_POLL
#define COMMON_SYSTEM_POLL

#include <poll.h>
#include <sys/stat.h>
#include <common/states.h>
#include <common/system/time.h>

#define AFD_MAX        8192
#define AFD_INFINITE  -1 // Infinite timeout

typedef struct afd_set_s {
    struct pollfd fds[AFD_MAX];
    nfds_t fds_count;
    int fd_max;
    int fd_min;
} afd_set_t;

void afd_set(int fd, afd_set_t *set, int flags);

int afd_isset(int fd, afd_set_t *set, int flag);

void afd_clear(int fd, afd_set_t *set);

int afd_stat(int fd, struct stat *st);

#define AFD_SET(fd, set) \
    afd_set(fd, set, POLLIN);

#define AFD_SETR(fd, set) \
    afd_set(fd, set, POLLIN);

#define AFD_SETW(fd, set) \
    afd_set(fd, set, POLLOUT);

#define AFD_ISSET(fd, set) \
    afd_isset(fd, set, POLLIN | POLLOUT)

#define AFD_ISSETR(fd, set) \
    afd_isset(fd, set, POLLIN)

#define AFD_ISSETW(fd, set) \
    afd_isset(fd, set, POLLOUT)

#define AFD_CLR(fd, set) \
    afd_clear(fd, set)

#define AFD_STAT(fd, stat) \
    afd_stat(fd, stat)

/* New select based in poll. Time values are in milliseconds. */
int aselect(afd_set_t *set, ullong timeout, ullong *time_left);

/* Version using timevals to ease the compatibility. The lefting time is saved in timeout like select(). */
int aselectv(afd_set_t *set, struct timeval *timeout);

#endif //COMMON_SYSTEM_POLL
