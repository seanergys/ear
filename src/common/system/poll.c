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

#define SHOW_DEBUGS 1

#include <common/system/poll.h>
#include <common/output/debug.h>

static void afd_init(afd_set_t *set)
{
    int i;
    for (i = 0; i < AFD_MAX; ++i) {
        set->fds[i].fd      = -1;
        set->fds[i].events  =  0;
        set->fds[i].revents =  0;
    }
    set->fd_min = AFD_MAX;
    set->fd_max = 0;
}

void afd_set(int fd, afd_set_t *set, int flags)
{
    if (set->fds_count == 0) {
        afd_init(set);
    }
    if (fd >= AFD_MAX || set->fds[fd].fd != -1) {
        return;
    }

    set->fds[fd].fd      = fd;
    set->fds[fd].events  = flags;
    set->fds[fd].revents = 0;

    if (fd > set->fd_max) {
        set->fd_max = fd;
    }
    if (fd < set->fd_min) {
        set->fd_min = fd;
    }
    // Counting
    set->fds_count = set->fd_max - set->fd_min + 1;
    debug("FD_SET(%d) [%d,%d]", fd, set->fd_min, set->fd_max);
}

int afd_isset(int fd, afd_set_t *set, int flag)
{
    int r = (set->fds[fd].revents & flag);
    debug("FD_ISSET(%d): ret %d", fd, r);
    return r;
}

void afd_clear(int fd, afd_set_t *set)
{
    int i;    

    set->fds[fd].fd      = -1;
    set->fds[fd].events  =  0;
    set->fds[fd].revents =  0;

    for (i = set->fd_min; i < AFD_MAX; ++i) {
        if (set->fds[i].fd != -1) {
            set->fd_min = set->fds[i].fd;
            break;
        }
    }
    for (i = set->fd_max; i >= 0; --i) {
        if (set->fds[i].fd != -1) {
            set->fd_max = set->fds[i].fd;
            break;
        }
    }
    // Counting
    set->fds_count = set->fd_max - set->fd_min + 1;
    debug("FD_CLR(%d) [%d,%d]", fd, set->fd_min, set->fd_max);
}

int afd_stat(int fd, struct stat *s)
{
    struct stat *p = s;
    struct stat aux;

    if (s == NULL) {
        p = &aux;
    }
    if (fstat(fd, p) < 0) {
        return 0;
    }
    return 1;
}

int aselect(afd_set_t *set, ullong timeout, ullong *time_left)
{
    ullong time_passed, r;
    timestamp_t ts;
    // Polling and calculating time
    timestamp_get(&ts);
    r = poll(&set->fds[set->fd_min], set->fds_count, (int) timeout);
    time_passed = timestamp_diffnow(&ts, TIME_MSECS);
    debug("%d = poll(fds_count: %d, timeout: %llu, time_passed: %llu)", r, set->fds_count, timeout, time_passed);
    // Calculating the lefting time
    if (time_left != NULL) {
        *time_left = 0;
        if (time_passed < timeout) {
            *time_left = timeout - time_passed;
        }
    }
    return r;
}

int aselectv(afd_set_t *set, struct timeval *_timeout)
{
    ullong timeout = AFD_INFINITE;
    ullong time_left;
    int r;
    // If timeout is NULL, then the timeout is infinite.
    if (_timeout != NULL) {
        timeout = timeval_convert(_timeout, TIME_MSECS);
    }
    // Converting timeval to single time value.
    r = aselect(set, timeout, &time_left);
    // Converting lefting time to timeval.
    if (_timeout != NULL) {
        *_timeout = timeval_create(time_left, TIME_MSECS);
    }
    return r;
}
