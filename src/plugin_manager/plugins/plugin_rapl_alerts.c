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

#include <upm/plugin_rapl.h>
#include <upm/plugin_rapl_alerts.h>

static rapl_alerts_t  a;
static rapl_t        *r;

UPM_F_INIT()
{
    return EAR_SUCCESS;
}

UPM_F_GET_INFO ()
{
    *tag = "rapl-alerts";
}

UPM_F_WORK()
{
    if (strcmp(tag, "rapl") == 0) {
        r = (rapl_t *) data;
        a.alert = r->power > 20LLU;
    }
    return EAR_SUCCESS;
}

UPM_F_DATA_ALLOC()
{
    *data = &a;
}