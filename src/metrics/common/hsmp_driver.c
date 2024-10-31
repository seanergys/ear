/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

//#define SHOW_DEBUGS 1

#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <common/output/debug.h>
#include <metrics/common/hsmp.h>
#include <metrics/common/hsmp_pci.h>
#include <metrics/common/hsmp_esmi.h>

#define HSMP_BY_NOTHING   0
#define HSMP_BY_DRIVER    1
#define HSMP_BY_ESMI      2
#define HSMP_BY_PCI       3

static uint            sockets_count;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int             fd   = -1;
static int             by;

static state_t hsmp_driver_send(int socket, uint function, uint *args, uint *reps)
{
    struct hsmp_message msg;
    int ret;

    memset(&msg, 0, sizeof(struct hsmp_message));
    msg.msg_id      = (__u32) function;
    msg.sock_ind    = (__u16) socket;
    debug("msg.msg_id     : %u", msg.msg_id);
    debug("msg.sock_ind   : %u", msg.sock_ind);
    // Counting args and reps
    while(args[msg.num_args] != -1) {
        msg.args[msg.num_args] = (__u32) args[msg.num_args];
        debug("msg.args[%d]    : %u", msg.num_args, msg.args[msg.num_args]);
        msg.num_args++;
    }
    while(reps[msg.response_sz] != -1) {
        msg.response_sz++;
    }
    debug("msg.num_args   : %u", msg.num_args);
    debug("msg.response_sz: %u", msg.response_sz);
    // Contacting the driver
    if ((ret = ioctl(fd, HSMP_IOCTL_CMD, &msg)) == -1) {
        debug("ioctl failed: %s", strerror(errno));
        return_print(EAR_ERROR, "Error while sending a command to HSMP driver: %s", strerror(errno));
    }
    // Recovering responses
    while(msg.response_sz > 0) {
        reps[msg.response_sz-1] = (uint) msg.args[msg.response_sz-1];
        msg.response_sz--;
    }
    return EAR_SUCCESS;
}

static state_t hsmp_driver_close()
{
    if (fd != -1) {
        close(fd);
    }
    fd = -1;
	return EAR_SUCCESS;
}

static state_t hsmp_driver_open(topology_t *tp)
{
    state_t s;
    if (tp->vendor == VENDOR_INTEL || tp->family < FAMILY_ZEN) {
		return_msg(EAR_ERROR, Generr.api_incompatible);
	}
    if (fd != -1) {
        goto leave;
    }
    if ((fd = open("/dev/hsmp", O_RDWR)) < 0) {
        if ((fd = open("/dev/hsmp", O_RDONLY)) < 0) {
            return_print(EAR_ERROR, "Error while opening /dev/hsmp driver file: %s", strerror(errno))
        }
        debug("HSMP is in user mode");
        // Test ping
        uint args[2] = { 68, -1 };
        uint reps[2] = {  0, -1 };
        if (state_fail(s = hsmp_driver_send(0, HSMP_TEST, args, reps))) {
            hsmp_driver_close();
            return_print(EAR_ERROR, "HSMP test function failed: %s", state_msg);
        }
        if (reps[0] != 69) {
            hsmp_driver_close();
            return_print(EAR_ERROR, "HSMP not enabled (incorrect ping value, expected 69, received %u)", reps[0]);
        }
    }
	sockets_count = tp->socket_count;
leave:
    return EAR_SUCCESS;
}

state_t hsmp_open(topology_t *tp)
{
    state_t s = EAR_SUCCESS;
    while (pthread_mutex_trylock(&lock));
    if (by != HSMP_BY_NOTHING) {
        goto leave;
    }
    by = HSMP_BY_DRIVER;
    if (state_fail(s = hsmp_driver_open(tp))) {
        debug("HSMP driver not loaded");
        by = HSMP_BY_ESMI;
        if (state_fail(s = hsmp_esmi_open(tp))) {
            debug("HSMP esmi not loaded");
            by = HSMP_BY_PCI;
            if (state_fail(s = hsmp_pci_open(tp))) {
                debug("HSMP pci not loaded");
                by = HSMP_BY_NOTHING;
            }
        }
    }
leave:
    pthread_mutex_unlock(&lock);
    return s;
}

state_t hsmp_close()
{
    state_t s = EAR_ERROR;
    state_msg = Generr.api_uninitialized;
    while (pthread_mutex_trylock(&lock));
    if (by == HSMP_BY_DRIVER) s = hsmp_driver_close();
    if (by == HSMP_BY_ESMI  ) s = hsmp_esmi_close();
    if (by == HSMP_BY_PCI   ) s = hsmp_pci_close();
    by = HSMP_BY_NOTHING;
    pthread_mutex_unlock(&lock);
    return s;
}

static state_t unlock(state_t s)
{
    pthread_mutex_unlock(&lock);
    return s;
}

state_t hsmp_send(int socket, uint function, uint *args, uint *reps)
{
    while (pthread_mutex_trylock(&lock));
    if (by == HSMP_BY_DRIVER) return unlock(hsmp_driver_send(socket, function, args, reps));
    if (by == HSMP_BY_ESMI  ) return unlock(hsmp_esmi_send(socket, function, args, reps));
    if (by == HSMP_BY_PCI   ) return unlock(hsmp_pci_send(socket, function, args, reps));
    pthread_mutex_unlock(&lock);
    return_msg(EAR_ERROR, Generr.api_uninitialized);
}
