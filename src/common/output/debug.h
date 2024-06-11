/*
*
* This program is part of the EAR software.
*
* EAR provides a dynamic, transparent and ligth-weigth solution for
* Energy management. It has been developed in the context of the
* Barcelona Supercomputing Center (BSC)&Lenovo Collaboration project.
*
* 
* BSC Contact   mailto:ear-support@bsc.es
* 
*
* EAR is an open source software, and it is licensed under both the BSD-3 license
* and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
* and COPYING.EPL files.
*/

#ifndef EAR_DEBUG_H
#define EAR_DEBUG_H

#include <stdio.h>
#include <string.h>

#define DEBUG_ENABLED     1
int  debug_channel	       __attribute__((weak)) = 2;
int adebug_enabled         __attribute__((weak)) = 0;

#define ENABLE_DEBUG     0
#define DEBUG_SET_FD(fd) debug_channel = fd;

// Set
#define  DEBUG_SET_FD(fd)  debug_channel = fd;
#define ADEBUG_SET_EN(en) adebug_enabled = en;

// Long format
#if 1
#define _debug(...) \
    do { \
      dprintf(debug_channel, "%s:%s:%d: ", __FILE__, __FUNCTION__, __LINE__); \
      dprintf(debug_channel, __VA_ARGS__); \
      dprintf(debug_channel, "\n"); \
    } while(0);
#else
// Short format
#define _debug(...) \
    dprintf(debug_channel, __VA_ARGS__); \
    dprintf(debug_channel, "\n");
#endif

// Traditional debug
#if SHOW_DEBUGS && DEBUG_ENABLED
#define debug(...) \
{ \
    _debug(__VA_ARGS__); \
}
#else
#define debug(...)
#endif

// Active debug
#define adebug(...) \
{ \
    if (adebug_enabled) { \
        dprintf(debug_channel, __VA_ARGS__); \
        dprintf(debug_channel, "\n"); \
    } \
}

#endif //EAR_DEBUG_H
