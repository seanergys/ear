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

//#define SHOW_DEBUGS 1

#include <stdlib.h>
#include <common/output/debug.h>
#include <common/math_operations.h>
#include <metrics/common/msr.h>
#include <metrics/bandwidth/archs/amd19.h>

static topology_t  tp;
static uint        fun; //Address
static uint        is_pooling;
static bwidth_t    *pool;
static timestamp_t pool_time;


BWIDTH_F_LOAD(bwidth_amd19_load)
{
    if (tpo->vendor != VENDOR_AMD || tpo->family < FAMILY_ZEN){
        return_msg(, Generr.api_incompatible);
    }
    if (state_fail(hsmp_scan(tpo))) {
        return;
    }
    if (state_fail(topology_select(tpo, &tp, TPSelect.socket, TPGroup.merge, 0))) {
        return;
    }
    // ZEN3
    fun = 0x14;
    //
    pool = calloc(tp.cpu_count+1, sizeof(ullong));

    apis_put(ops->get_info,  bwidth_amd19_get_info);
    apis_put(ops->init,      bwidth_amd19_init);
    apis_put(ops->dispose,   bwidth_amd19_dispose);
    apis_put(ops->read,      bwidth_amd19_read);
    apis_put(ops->data_diff, bwidth_amd19_data_diff);
}

BWIDTH_F_GET_INFO(bwidth_amd19_get_info)
{
    info->api         = API_AMD19;
    info->scope       = SCOPE_NODE;
    info->granularity = GRANULARITY_SOCKET;
    info->devs_count  = tp.cpu_count+1;
}

static state_t multiread(bwidth_t *bws)
{
    uint args[1] = { -1 };
    uint reps[2] = { 0, -1 };

    timestamp_get(&bws[tp.cpu_count].time);
    for (sock = 0; sock < tp.cpu_count; ++sock) {
        hsmp_send(sock, fun, args, reps);
        bws[sock].cas = getbits32(reps[0], 19, 8);
    }
    return EAR_SUCCESS;
}

static state_t multiplex(void *something)
{
    // Call multiread passing pool
}

BWIDTH_F_INIT(bwidth_amd19_init)
{
    if ((is_pooling = monitor_is_initialized())) {
        // Multiplexing suscription
        sus             = suscription();
        sus->call_init  = NULL;
        sus->call_main  = multiplex;
        sus->time_relax = 5000;
        sus->time_burst = 5000;
        return sus->suscribe(sus);
    }
    return EAR_SUCCESS;
}

BWIDTH_F_DISPOSE(bwidth_amd19_dispose)
{
    return EAR_SUCCESS;
}

BWIDTH_F_READ(bwidth_amd19_read)
{
    memset(bws, 0, (tp.cpu_count+1)*sizeof(bwidth_t));
    memcpy(...);
}

BWIDTH_F_DATA_DIFF(bwidth_amd19_data_diff)
{
    //     
}
