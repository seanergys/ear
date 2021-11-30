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

#define _GNU_SOURCE
#include <dlfcn.h>
#include <limits.h>
#include <common/types.h>
#include <common/states.h>
#include <common/utils/dtools.h>
#include <common/output/verbose.h>

#if DTOOLS_MALLOC
static void *(*next_malloc) (size_t);
#endif
static void  (*next_free)   (void *);

static long address;

static void dtools_init()
{
    if ((next_free = dlsym(RTLD_NEXT, "free")) == NULL) {
        sserror("when dlsym 'free': %s\n", dlerror());
    }
    #if DTOOLS_MALLOC
    if ((next_malloc = dlsym(RTLD_NEXT, "malloc")) == NULL) {
        sserror("when dlsym 'malloc': %s\n", dlerror());
    }
    #endif
}

void dtools_set_address(void *address_in)
{
   address = (long) address_in; 
}

void free(void *ptr)
{
    static ulong diff_min = ULONG_MAX;
    if(next_free == NULL) {
        verbose(0, "INITIALIZING DTOOLS.");
        dtools_init();
    }
    long diff = address - (long) ptr;
    if (diff < 0L) {
        diff = -diff;
    } 
    if (diff < diff_min) {
        diff_min = diff;
        verbose(0, "FOUND ADDRESS %p!!!!!!!!!!!!", ptr);
    }
    next_free(ptr);
}
