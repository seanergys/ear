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
#include <common/states.h>

typedef struct afd_set_s {
    struct pollfd fds[8192];
    nfds fds_count;
    int timeout;
} afd_set;

#define AFD_SET(fd, set) \
    void afd_set(fd, set);

#define AFD_ISSET(fd, set) \
    int afd_isset(fd, set);

#define AFD_CLEAR() \
    void afd_clear(fd, set);

int aselect(afd_set set);

#endif //COMMON_SYSTEM_POLL