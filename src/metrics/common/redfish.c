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

#ifdef REDFISH_BASE
#include <redfish.h>
#endif

#include <stdlib.h>
#include <pthread.h>
#include <common/output/debug.h>
#include <common/system/symplug.h>
#include <metrics/common/redfish.h>

typedef const char cchar;

#ifndef REDFISH_BASE
typedef uint enumeratorAuthentication;
typedef uint redfishService;
typedef uint redfishPayload;
#define REDFISH_PATH ""
#else
#define REDFISH_PATH REDFISH_BASE
#endif

#define REDFISH_N 8

static struct redfish_s
{
    redfishService *(*createService)    (cchar *host, cchar *uri, enumeratorAuthentication* auth, uint flags);
    redfishPayload *(*getPayload)       (redfishService* service, cchar *path);
    char *          (*payloadToString)  (redfishPayload* payload, int prettyPrint);
    int             (*payloadToInt)     (redfishPayload* payload);
    size_t          (*getCollectionSize)(redfishPayload* payload);
    size_t          (*getArraySize)     (redfishPayload* payload);
    void            (*cleanupPayload)   (redfishPayload* payload);
    void            (*cleanupService)   (redfishService* service);
} redfish;

static const char *redfish_names[] =
{
    "createServiceEnumerator",
    "getPayloadByPath",
    "payloadToString",
    "getPayloadIntValue",
    "getCollectionSize",
    "getArraySize",
    "cleanupPayload",
    "cleanupServiceEnumerator",
};

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static redfishService *service;
static uint            loaded;

static int load(char *path)
{
    int flags = RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND;
    debug("loading: %s", path);
    if (state_fail(plug_load(path, (void **) &redfish, redfish_names, REDFISH_N, flags))) {
        debug("failed while loading %s: %s", path, state_msg);
        return 0;
    }
    return 1;
}

static state_t static_load()
{
    state_t s = EAR_SUCCESS;
    if (load(getenv("HACK_FILE_REDFISH"))) return s;
    if (load(REDFISH_PATH "/lib64/libredfish.so")) return s;
    if (load(REDFISH_PATH "/lib/libredfish.so")) return s;
    return_msg(EAR_ERROR, "Can't load libredfish.so");
}

static state_t static_init(char *host)
{
    // host, uri, auth, flags
    // uri, if null is /redfish
    service = redfish.createService(host, NULL, NULL, 1);
    return EAR_SUCCESS;
}

state_t redfish_init(char *host)
{
    state_t s = EAR_SUCCESS;
    #ifndef REDFISH_BASE
    return_msg(EAR_ERROR, "path to libredfish.so installation not set");
    #endif
    if (host == NULL) {
        return_msg(EAR_ERROR, Generr.input_null);
    }
    if (loaded) {
        return s;
    }
    while (pthread_mutex_trylock(&lock));
    if (!loaded) {
        if (state_ok(s = static_load())) {
            if (state_ok(s = static_init(host))) {
                loaded = 1;
            }
        }
    }
    pthread_mutex_unlock(&lock);
    return s;
}

state_t redfish_dispose()
{
    redfish.cleanupService(service);
    return EAR_SUCCESS;
}

//"/Chassis[%d]/Sensors[Id=TotalPower]/Reading"
//"/Chassis/Members[%d]/Sensors[Id=TotalPower]/Reading (int)

static int count_iterators(char *field)
{
    uint count = 0;
    int i;
    for(i = 0; i < strlen(field); ++i) {
        count += (field[i] == '%');
    }
    debug("Interators: %d", count)
    return count;
}

static state_t read_single(char *field, void *content, void *accum, uint *count, int type_flag)
{
    redfishPayload *payload = redfish.getPayload(service, field);

    debug("Looking in field: %s", field);
    if (payload == NULL) {
        redfish.cleanupPayload(payload);
        return_msg(EAR_ERROR, "No content.");
    }
    if (count != NULL) {
        *count += 1;
    }
    if (type_flag == REDTYPE_STR) {
        if (content != NULL) {
            char *p = (char *) content;
            sprintf(p, "%s", redfish.payloadToString(payload, 1));
            debug("STR: %s", p);
        }
    }
    else if (type_flag == REDTYPE_I32) {
        int v  = redfish.payloadToInt(payload);
        if (content != NULL) {
            int *p = (int *) content;
            *p = v;
            debug("I32v: %d", *p);
        }
        if (accum != NULL) {
            int *p = (int *) accum;
            *p += v;
            debug("I32a: %d", *p);
        }
    }
    redfish.cleanupPayload(payload);
    return EAR_SUCCESS;
}

static state_t read_multiple(char *field, void *content, void *accum, uint *count, int type_flag)
{
    char *p = (char *) content;
    char buffer[4096];
    int c1, c2, i1, i2;
    int bytes = 4;
    state_t s;

    if (count != NULL) {
        *count = 0;
    }
    // c1: counts the '%d' occurrences
    // c2: counts the number of read values
    if ((c1 = count_iterators(field)) == 0) {
        return read_single(field, content, accum, count, type_flag);
    }
    if (type_flag == REDTYPE_I64 || type_flag == REDTYPE_F64) {
        bytes = 8;
    }
    // i1: exterior iterator
    // i2: interior iterator
    i1 = -1;
    c2 =  0;

    if (c1 == 1) {
        do {
            ++i1;
            sprintf(buffer, field, i1);
            s = read_single(buffer, &p[c2*bytes], accum, count, type_flag);
            c2++;
        } while(state_ok(s));
    }
    if (c1 == 2) {
        do {
            ++i1;
            i2 = -1;
            do {
                ++i2;
                sprintf(buffer, field, i1, i2);
                s = read_single(buffer, &p[c2*bytes], accum, count, type_flag);
                c2++;
            } while(state_ok(s));
        } while(i2 > 0);
    }
    if (c2 == 1) {
        return_msg(EAR_ERROR, "Nothing found with that path.")
    }
    return EAR_SUCCESS;
}

state_t redfish_read(char *field, void *content, void *accum, uint *count, int type_flag)
{
    return read_multiple(field, content, accum, count, type_flag);
}

state_t redfish_count_members(char *field, uint *count)
{
    redfishPayload* payload = redfish.getPayload(service, field);
    if (payload == NULL) {
        redfish.cleanupPayload(payload);
        return_msg(EAR_ERROR, "No content.");
    }
    size_t size = redfish.getArraySize(payload);
    *count = (uint) size;
    debug("Counter: %u", *count);
    redfish.cleanupPayload(payload);
    return EAR_SUCCESS;
}

#if 1
// /opt/rh/devtoolset-7/root/bin/gcc -g -I ../../ -I/hpc/opt/libredfish/include -DREDFISH_BASE=\"/hpc/opt/libredfish\" -o test redfish.c ../libmetrics.a ../../common/libcommon.a -ldl -lpthread

int main (int argc, char *argv[])
{
    uint watts[16];
    uint accum = 0;
    uint count = 0;

    if (argc <= 2) {
        debug("insuficient arguments")
        return 0;
    }
    if (state_fail(redfish_init(argv[1]))) {
        debug("redfish_init failed: %s", state_msg);
        return 0;
    }
    if (state_fail(redfish_read(argv[2], (void *) watts, (void *) &accum, &count, REDTYPE_I32))) {
        debug("redfish_read failed: %s", state_msg);
        return 0;
    }

    return 0;
}
#endif
