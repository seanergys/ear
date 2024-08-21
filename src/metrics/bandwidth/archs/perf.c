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

#define SHOW_DEBUGS 1

#include <stdlib.h>
#include <common/system/time.h>
#include <common/output/debug.h>
#include <metrics/common/perf.h>
#include <metrics/bandwidth/archs/perf.h>

static uint    started;
static perf_t *perfs;
static uint    perfs_alloc_count;
static uint    perfs_count;

static void set_alloc(uint new_count)
{
    if (new_count <= perfs_alloc_count) {
        return;
    }
    if (perfs_alloc_count != 0) {
        perfs = realloc(perfs, new_count*sizeof(perf_t));
        memset(&perfs[perfs_alloc_count], 0, (new_count-perfs_alloc_count)*sizeof(perf_t));
    } else {
        perfs = calloc(new_count, sizeof(perf_t));
    }
    perfs_alloc_count = new_count;
}

static void set_event(ullong event, uint type, cchar *event_name)
{
    if (state_fail(perf_open(&perfs[perfs_count], NULL, 0, type, event))) {
        debug("perf_open returned: %s", state_msg);
        return;
    }
    sprintf(perfs[perfs_count].event_name, "%s", event_name);
    perfs_count++;
}

state_t bwidth_perf_load(topology_t *tp, bwidth_ops_t *ops)
{
    if (tp->vendor == VENDOR_INTEL) {
        // Ice Lake type. 4 IMCs and 2 Channels per IMC. In case of two sockets,
        // you will see 4 IMCs only, and two CPUs in cpumask.
        if (state_fail(perf_open_files(&perfs, "uncore_imc_free_running_0", "data_total", &perfs_count))) {
            debug("perf_open_files returned: %s", state_msg);
        }
    } else if (tp->vendor == VENDOR_ARM) {
        set_alloc(1);
        set_event(0x19, PERF_TYPE_RAW, "BUS_ACCESS");
    }
    if (perfs_count == 0) {
        return EAR_ERROR;
    }
    apis_put(ops->get_info,        bwidth_perf_get_info);
    apis_put(ops->init,            bwidth_perf_init);
    apis_put(ops->dispose,         bwidth_perf_dispose);
    apis_put(ops->read,            bwidth_perf_read);
    debug("Loaded PERF")
    return EAR_SUCCESS;
}

BWIDTH_F_GET_INFO(bwidth_perf_get_info)
{
    info->api         = API_PERF;
    info->scope       = SCOPE_NODE;
    info->granularity = GRANULARITY_IMC;
    info->devs_count  = perfs_count+1;
}

state_t bwidth_perf_init(ctx_t *c)
{
    int i;
    if (!started) {
        for (i = 0; i < perfs_count; ++i) {
            perf_start(&perfs[i]);
        }
        ++started;
    }
    return EAR_SUCCESS;
}

state_t bwidth_perf_dispose(ctx_t *c)
{
    return EAR_SUCCESS;
}

state_t bwidth_perf_read(ctx_t *c, bwidth_t *bw)
{
    llong value;
    int i;
    // Cleaning
    memset(bw, 0, perfs_count*sizeof(bwidth_t));
    // Reading
    timestamp_get(&bw[perfs_count].time);
    for (i = 0; i < perfs_count; ++i) {
        if (state_ok(perf_read(&perfs[i], &value))) {
            bw[i].cas = value;
            printf("CAS: %llu\n", bw[i].cas);
        }
    }
    return EAR_SUCCESS;
}