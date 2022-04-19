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

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <common/output/debug.h>
#include <common/system/monitor.h>
#include <common/system/symplug.h>
#include <metrics/common/cupti.h>
#include <metrics/gpu/archs/cupti.h>

#define CTX_N        32 // Context storage
#define GRP_N        8  // Events in a group
#define DEV_N        8  // Supported devices

typedef struct CUextra_s {
    CUpti_EventID          events_ids[GRP_N];
    ullong                 events_counters[GRP_N];
    char                   events_names[GRP_N][128];
    uint                   instances_count;
    uint                   events_count;
} CUextra;

typedef struct CUmeta_s {
    CUpti_MetricID         metric_id;
    int                    meta_id;
    CUdevice               device;
    CUcontext              context;
    CUpti_EventGroupSets  *passes;
    CUpti_EventGroupSet   *set;
    CUpti_EventGroup      *groups;
    CUextra                extras[GRP_N];
    uint                   groups_count;
    uint                   events_count;
    uint                   init;
    uint                   reset;
    uint                   read;
} CUmeta;

typedef struct CUroot_s {
    CUpti_SubscriberHandle subscriber;
    char                  *metric_name;
    char                  *metric2_name;
    uint                   devs_count;
    uint                   ctxs_count;
    uint                   protected;
} CUroot;

typedef struct CUpool_s {
    ullong                 values[GRP_N];
} CUpool;

static pthread_mutex_t  lock = PTHREAD_MUTEX_INITIALIZER;
static CUroot   root;
static CUmeta   meta[CTX_N];
static CUpool   pool[DEV_N];
static cupti_t  cupti;
static cuda_t   cu;
static uint     ok;

#define cc(call)                                                                \
    do {                                                                        \
        CUptiResult _status = call;                                             \
        if (_status != CUPTI_SUCCESS) {                                         \
            const char *errstr;                                                 \
            cupti.GetResultString(_status, &errstr);                            \
            debug(#call ": FAILED, %s", errstr);                                \
            context_destroy(m);                                                  \
            return 0;                                                           \
        }                                                                       \
    } while (0)

static void callback(void *user_data, CUpti_CallbackDomain domain, CUpti_CallbackId id, CUpti_CallbackData *data);

void gpu_cupti_load(gpu_ops_t *ops)
{
    if ((root.metric_name = getenv("CUPTI_METRIC")) == NULL) {
        debug("CUPTI_METRIC environent variable doesn't found");
        return;
    }
    if (state_fail(cupti_open(&cupti, &cu))) {
        return;
    }
    apis_add(ops, init,    gpu_cupti_init);
    apis_add(ops, dispose, gpu_cupti_dispose);
    apis_add(ops, read,    gpu_cupti_read);
}

static state_t intern_init()
{
    CUptiResult s;
    char *str;

    if ((s = cupti.Subscribe(&root.subscriber, (CUpti_CallbackFunc) callback, NULL)) != CUPTI_SUCCESS) {
        cupti.GetResultString(s, (const char **) &str);
        return_msg(EAR_ERROR, str);
    }
    if ((s = cupti.EnableCallback(1, root.subscriber, CUPTI_CB_DOMAIN_DRIVER_API, CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel)) != CUPTI_SUCCESS) {
        cupti.GetResultString(s, (const char **) &str);
        return_msg(EAR_ERROR, str);
    }

    return EAR_SUCCESS;
}

state_t gpu_cupti_init(ctx_t *c)
{
    suscription_t *sus;
    while (pthread_mutex_trylock(&lock));
    if (ok) {
        goto fini;
    }
    // Setting callbacks
    intern_init();
    // Setting suscriptions
    sus = suscription();
    sus->call_main  = gpu_cupti_pool;
    sus->time_relax = 1500;
    sus->time_burst = 1500;
    sus->suscribe(sus);
    ok = 1;
    fini:
    pthread_mutex_unlock(&lock);
    return EAR_SUCCESS;
}

static void context_destroy(CUmeta *m)
{
    cupti.EventGroupSetsDestroy(m->passes);
    memset(m, 0, sizeof(CUmeta));
}

static int metric_print(CUmeta *m)
{
    #if SHOW_DEBUGS
    CUpti_EventID events_ids[GRP_N];
    char event_name[128];
    uint events_count;
    uint p, g, e;
    size_t size;

    cc(cupti.MetricGetNumEvents(m->metric_id, &events_count));

    debug("-- [CUPTI] ----------------------");
    debug("#passes: %d", m->passes->numSets);
    debug("#events: %u", events_count);

    // Sets
    for (p = 0; p < m->passes->numSets; ++p) {
        CUpti_EventGroupSet *set = &m->passes->sets[p];
        debug("-- [PASS %d/%d] -------------------", p+1, m->passes->numSets);
        debug("#groups: %d", set->numEventGroups);

        // Groups
        for (g = 0; g < m->passes->sets[p].numEventGroups; ++g) {
            CUpti_EventGroup* group = set->eventGroups[g];

            size = sizeof(events_count);
            cc(cupti.EventGroupGetAttribute(group, CUPTI_EVENT_GROUP_ATTR_NUM_EVENTS, &size, &events_count));
            size = sizeof(events_ids);
            cc(cupti.EventGroupGetAttribute(group, CUPTI_EVENT_GROUP_ATTR_EVENTS, &size, events_ids));

            // Events
            for (e = 0; e < events_count; ++e) {
                size = sizeof(event_name);
                cc(cupti.EventGetAttribute(events_ids[e], CUPTI_EVENT_ATTR_NAME, &size, event_name));
                debug("%s", event_name);
            }
        }
        debug("---------------------------------");
    }
    #endif
    return 1;
}

static int metric_enable(CUmeta *m)
{
    size_t size;
    int g, e;
    int aux;

    // We don't support more than 1 pass
    if (m->passes->numSets > 1) {
        debug("More than 1 passes are not supported.");
        return 0;
    }

    // We are interested just in one pass, so we save the first set
    m->set = m->passes->sets;
    m->groups = m->set->eventGroups;
    m->groups_count = m->set->numEventGroups;

    // Sets/set/groups/events
    for (g = 0; g < m->groups_count; ++g) {
        // Counting events per group
        size = sizeof(m->extras[g].events_count);
        cc(cupti.EventGroupGetAttribute(m->groups[g], CUPTI_EVENT_GROUP_ATTR_NUM_EVENTS, &size, &m->extras[g].events_count));

        // Counting and retrieving event IDs
        size = sizeof(m->extras[g].events_ids);
        cc(cupti.EventGroupGetAttribute(m->groups[g], CUPTI_EVENT_GROUP_ATTR_EVENTS, &size, m->extras[g].events_ids));

        // Counting instances
        size = sizeof(m->extras[g].instances_count);
        cc(cupti.EventGroupGetAttribute(m->groups[g], CUPTI_EVENT_GROUP_ATTR_INSTANCE_COUNT, &size, &m->extras[g].instances_count));

        // Enabling all domains
        cc(cupti.EventGroupSetAttribute(m->groups[g], CUPTI_EVENT_GROUP_ATTR_PROFILE_ALL_DOMAIN_INSTANCES, sizeof(aux), &aux));
        cc(cupti.EventGroupEnable(m->groups[g]));

        for (e = 0; e < m->extras[g].events_count; ++e) {
            size = sizeof(m->extras[g].events_names[e]);
            cc(cupti.EventGetAttribute(m->extras[g].events_ids[e], CUPTI_EVENT_ATTR_NAME, &size, m->extras[g].events_names[e]));
        }
    }
    return 1;
}

static int metric_create(CUmeta *m)
{
    debug("Resetting...");
    cc(cupti.SetEventCollectionMode(m->context, CUPTI_EVENT_COLLECTION_MODE_CONTINUOUS));
    cc(cupti.MetricGetIdFromName(m->device, root.metric_name, &m->metric_id));

    // When returned by cuptiMetricCreateEventGroupSets, indicates the number
    // of passes required to collect all the events, and the event groups that
    // should be collected during each pass.
    //
    // CUpti_EventGroupSets: a struct that contains an array of sets (passes)
    // CUpti_EventGroupSet: a struct that contains an array of CUpti_EventGroup
    // CUpti_EventGroup: an array of events
    cc(cupti.MetricCreateEventGroupSets(m->context, sizeof(m->metric_id), &m->metric_id, &m->passes));

    // Print (or debugging)
    if (!metric_print(m)) {
        return 0;
    }
    // Breakdown
    if (!metric_enable(m)) {
        return 0;
    }
    // State change
    m->reset = 0;
    m->init  = 1;
    m->read  = 1;

    return 1;
}

static int metric_read_group(CUmeta *m, uint g)
{
    // The NVidia 3080 has 8704 stream processors.
    static ullong buffer[16384];
    uint events_count;
    size_t events_read;
    size_t size1;
    size_t size2;
    int e, i;

    // Reading events
    size1 = sizeof(buffer);
    size2 = sizeof(m->extras[g].events_ids);
    cc(cupti.EventGroupReadAllEvents(m->groups[g], CUPTI_EVENT_READ_FLAG_NONE, &size1, buffer, &size2, m->extras[g].events_ids, &events_read));

    events_count = m->extras[g].events_count;
    debug("Readed %lu/%d events (%p)", events_read, m->extras[g].events_count, m->context);

    for (e = 0; e < events_count; ++e) {
        for (i = 0; i < m->extras[g].instances_count; ++i) {
            m->extras[g].events_counters[e] += buffer[(i * events_count) + e];
        }
        debug("D%d, %s: %llu", m->device, m->extras[g].events_names[e], m->extras[g].events_counters[e]);
    }
    return 1;
}

static void metric_read(CUmeta *m)
{
    int i;

    debug("Reading... %p", m->context);
    for (i = 0; i < m->set->numEventGroups; ++i) {
        if (!metric_read_group(m, i)) {
            return;
        }
    }
}

static int context_find(CUcontext pctx, int meta_id, uint *index)
{
    uint found  = 0;
    int i;

    for (i = CTX_N-1; i >= 0 && !found; --i) {
        if (!meta[i].init && !meta[i].reset) {
            *index = i;
        }
        found = (meta[i].context == pctx) && (meta[i].meta_id == meta_id);
    }
    if (found) {
        *index = i+1;
    }
    debug("Finding... %d (%u)", found, *index);
    return found;
}

static void context_pop(CUcontext pctx, int meta_id)
{
    uint i;

    if(!context_find(pctx, meta_id, &i)) {
        return;
    }
    meta[i].read = 0;
}

static void context_push(CUcontext pctx, int meta_id)
{
    uint i;

    if(!context_find(pctx, meta_id, &i))
    {
        debug("Pushing... %d (%d)", i, meta_id);
        cu.CtxGetDevice(&meta[i].device);
        meta[i].context = pctx;
        meta[i].meta_id = meta_id;
        meta[i].reset   = 1;
    }
}

static void callback(void *user_data, CUpti_CallbackDomain domain, CUpti_CallbackId id, CUpti_CallbackData *data)
{
    debug("Callback ... %s %p", data->functionName, data->context);
    uint enter = (data->callbackSite != CUPTI_API_EXIT);

    if (data->context == NULL) {
        return;
    }
    // If kernel launch
    if (id == CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel) {
        if (enter) {
            context_push(data->context, 1);
        } else {
            context_pop(data->context, 1);
        }
    }
}

state_t gpu_cupti_pool(void *p)
{
    uint d;
    debug("Pooling...");
    for (d = 0; d < CTX_N; ++d) {
        if (meta[d].reset) {
            metric_create(&meta[d]);
        }
        if (meta[d].read) {
            metric_read(&meta[d]);
        }
    }
    return EAR_SUCCESS;
}

state_t gpu_cupti_read(ctx_t *c, gpu_t *data)
{
    uint d;
    for (d = 0; d < root.devs_count; ++d) {
        // Nothing by now
        debug("%llu", pool[d].values[0]);
    }
    return EAR_SUCCESS;
}
