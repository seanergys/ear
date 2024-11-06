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

#include <pthread.h>
#include <sys/types.h>
#include <common/output/debug.h>
#include <metrics/common/hsmp.h>
#include <metrics/common/hsmp_pci.h>
#include <metrics/common/hsmp_esmi.h>
#include <metrics/common/hsmp_driver.h>

#define HSMP_BY_NOTHING   0
#define HSMP_BY_DRIVER    1
#define HSMP_BY_ESMI      2
#define HSMP_BY_PCI       3

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int             by;

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