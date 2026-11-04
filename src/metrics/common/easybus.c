/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

/* clang-format off */
#define SHOW_DEBUGS 1
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <common/output/debug.h>
#include <common/system/symplug.h>
#include <metrics/common/easybus.h>

#ifndef EASYBUS_PREFIX
#define EASYBUS_PREFIX ""
#else
#endif
#define EASYBUS_LIB "libaccess_easybus.so"
#define EASYBUS_N   9

static const char *easybus_names[] = {
    "easybus_connect",
    "easybus_disconnect",
    "easybus_is_connected",
    "easybus_get_available_metrics",
    "easybus_search_available_metrics",
    "easybus_request_get",
    "easybus_request_set",
    "easybus_request_set_imm",
    "easybus_metrics_alloc",
};

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static easybus_t  easybus;
static void      *handler;
static int        counter;

static int library_load()
{
    #define _open_test(path) \
    debug("Openning %s", path); \
    if ((handler = plug_open2(path, (void **) &easybus, easybus_names, EASYBUS_N, RTLD_NOW | RTLD_LOCAL)) != NULL) { \
        return 1; \
    } else { \
        debug("Failed: %s", state_msg); \
    }
    #define open_test(path) \
    _open_test(path EASYBUS_LIB);

    open_test(EASYBUS_PREFIX "/lib/accessd/");
    open_test("/usr/lib/targets/x86_64-linux/lib/accessd/");
    open_test("/usr/lib64/accessd/");
    open_test("/usr/lib/accessd/");
    return_msg(0, "library not found");
}

int easybus_open(easybus_t *_easybus, char *address)
{
    #define return_errno(no) { errno = no; return 0; }
    #ifndef EASYBUS_PREFIX
    debug("No EASYBUS_PREFIX path provided");
    return 0;
    #endif
    if (_easybus == NULL) {
        return_errno(EINVAL);
    }
    while (pthread_mutex_trylock(&lock));
    if (counter == 0) {
        if (!library_load()) {
            debug("library not found");
            goto leave;
        }
        if (!easybus.connect(address)) {
            debug("EasyBus can not connect: %s", strerror(errno));
            dlclose(handler);
            handler = NULL;
            goto leave;
        }
    }
    memcpy(_easybus, &easybus, sizeof(easybus_t));
    ++counter;
leave:
    pthread_mutex_unlock(&lock);
    return counter > 0;
}

void easybus_close()
{
    while (pthread_mutex_trylock(&lock));
    if (counter > 0) {
        if (counter == 1) {
            easybus.disconnect();
            memset(&easybus, 0, sizeof(easybus_t));
            dlclose(handler);
            handler = NULL;
        }
        --counter;
    }
    pthread_mutex_unlock(&lock);
}