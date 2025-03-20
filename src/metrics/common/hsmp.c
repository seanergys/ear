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

#define HSMP_API_FAILED   -1
#define HSMP_API_NONE      0
#define HSMP_API_DRIVER    1
#define HSMP_API_ESMI      2
#define HSMP_API_PCI       3

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int             api;
static mode_t          api_mode;

static state_t __hsmp_open(topology_t *tp, mode_t mode)
{
    state_t s;
    api_mode = mode;
    if (state_ok(s = hsmp_driver_open(tp, mode))) {
        debug("HSMP driver loaded");
        api = HSMP_API_DRIVER;
        return s;
    }
    if (state_ok(s = hsmp_esmi_open(tp, mode))) {
        debug("HSMP esmi loaded");
        api = HSMP_API_ESMI;
        return s;
    }
    if ((mode == O_RDWR) && state_ok(s = hsmp_pci_open(tp))) {
        debug("HSMP pci not loaded");
        api = HSMP_API_PCI;
        return s;
    }
    return EAR_ERROR;
}

state_t hsmp_open(topology_t *tp, mode_t mode)
{
    while (pthread_mutex_trylock(&lock));
    if (api == HSMP_API_NONE) {
        if (state_fail(__hsmp_open(tp, O_RDWR))) {
            if (state_fail(__hsmp_open(tp, O_RDONLY))) {
                api = HSMP_API_FAILED;
            }
        }
    }
    pthread_mutex_unlock(&lock);
    // Once opened or failed
    if (api == HSMP_API_FAILED) {
        return_msg(EAR_ERROR, "API failed previously");
    } else if (mode == O_RDWR && api_mode != O_RDWR) {
        return_msg(EAR_ERROR, Generr.no_permissions);
    }
    return EAR_SUCCESS;
}

state_t hsmp_close()
{
    state_t s = EAR_ERROR;
    state_msg = Generr.api_uninitialized;
    while (pthread_mutex_trylock(&lock));
    if (api == HSMP_API_DRIVER) s = hsmp_driver_close();
    if (api == HSMP_API_ESMI  ) s = hsmp_esmi_close();
    if (api == HSMP_API_PCI   ) s = hsmp_pci_close();
    api = HSMP_API_NONE;
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
    if (api == HSMP_API_DRIVER) return unlock(hsmp_driver_send(socket, function, args, reps));
    if (api == HSMP_API_ESMI  ) return unlock(hsmp_esmi_send(socket, function, args, reps));
    if (api == HSMP_API_PCI   ) return unlock(hsmp_pci_send(socket, function, args, reps));
    pthread_mutex_unlock(&lock);
    return_msg(EAR_ERROR, Generr.api_uninitialized);
}
