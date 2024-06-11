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

#ifndef COMMON_SYSTEM_VERSION_H
#define COMMON_SYSTEM_VERSION_H

#include <common/types.h>

typedef struct version_s {
    char  str[32];
    uint  minor;
    uint  major;
    uint  hash;
} version_t;

#define VERSION_GT 0 //Greater-than
#define VERSION_GE 1 //Greater-or-equal
#define VERSION_LT 2 //Less-than
#define VERSION_LE 3 //Less-or-equal
#define VERSION_EQ 4 //Equal

// Initializes the version structure
void version_set(version_t *v, uint major, uint minor);

// Useful to compare between versions (ie: VERSION_GT means v > vtc)
int version_is(int symbol, version_t *v, version_t *vtc);

#endif //COMMON_SYSTEM_VERSION_H