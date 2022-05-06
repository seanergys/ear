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
#include <common/system/time.h>
#include <common/output/debug.h>

void afd_set(int fd, afd_set_t *set)
{
    int i;
    for (i = 0; i < set->fds_count; ++i) {
        if (set->fds[i].fd == -1) {
            break;
        }
    }
    if (i == set->fds_count) {
        set->fds_count += 1;
    }
    set->fds[i].fd      = fd;
    set->fds[i].events  = POLLIN;
    set->fds[i].revents = 0;
    debug("FD_SET(%d): position %d of %d", fd, i, set->fds_count);
}

int afd_isset(int fd, afd_set_t *set)
{
    int r;
    int i;
    for (i = 0; i < set->fds_count; ++i) {
        if (fd == set->fds[i].fd) {
            r = (set->fds[i].revents & POLLIN) || (set->fds[i].revents & POLLOUT);
            debug("FD_ISSET(%d): position %d of %d, returns %d", fd, i, set->fds_count, r);
            return r;
        }
    }
    return 0;
}

void afd_clear(int fd, afd_set_t *set)
{
    int i, j;
    for (i = 0; i < set->fds_count; ++i) {
        if (fd == set->fds[i].fd) {
            set->fds[i].fd      = -1;
            set->fds[i].events  =  0;
            set->fds[i].revents =  0;
            break;
        }
    }
    debug("FD_CLR(%d): position %d of %d", fd, i, set->fds_count);
    for (i = 0; i < set->fds_count; ++i) {
        if (set->fds[i].fd >= 0) {
            j = i;
        }
    }
    set->fds_count = j+1;
}

int aselect(afd_set_t *set, int timeout, int *_time_passed)
{
    timestamp_t ts;
    int time_passed;
    int res;

    timestamp_get(&ts);
    res = poll(set->fds, set->fds_count, timeout);
    time_passed = (int) timestamp_diffnow(&ts, TIME_MSECS);
    debug("Poll with timeout %d ms returned after %d ms", timeout, time_passed);
    if (_time_passed != NULL) {
        *_time_passed = time_passed;
    }
    return res;
}
