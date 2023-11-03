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
* EAR is an open source software, and it is licensed under both the BSD-3 license
* and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
* and COPYING.EPL files.
*/

#include <stdio.h>
#include <common/system/version.h>

void version_set(version_t *v, uint major, uint minor)
{
    v->minor = minor;
    v->major = major;
    v->hash  = (major << 16) | (minor);
    sprintf(v->str, "%u.%u", major, minor);
}

int version_is(int symbol, version_t *v, version_t *vtc)
{
    if (symbol == VERSION_GT) { return v->hash >  vtc->hash; }
    if (symbol == VERSION_GE) { return v->hash >= vtc->hash; }
    if (symbol == VERSION_LT) { return v->hash <  vtc->hash; }
    if (symbol == VERSION_LE) { return v->hash <= vtc->hash; }
    if (symbol == VERSION_EQ) { return v->hash == vtc->hash; }
    return 0;
}