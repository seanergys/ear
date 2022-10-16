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
#include <common/system/time.h>
#include <common/system/monitor.h>
#include <common/system/symplug.h>
#include <metrics/common/cupti.h>
#include <metrics/gpu/archs/cupti.h>

#define CTX_N        32 // Context storage
#define GRP_N        8  // Events in a group
#define DEV_N        8  // Supported devices


#define DEFAULT_CUPTI_METRIC "ipc"

typedef struct CUextra_s {
    CUpti_EventID          events_ids[GRP_N];
    ullong                 events_counters[GRP_N];
    ullong                 events_normals[GRP_N];
    char                   events_names[GRP_N][128];
    uint                   instances_count;
    uint                   total_instances_count;
    uint                   events_count;
} CUextra;

typedef struct CUmeta_s {
    CUpti_MetricID         metric_id;
    CUpti_MetricValueKind  metric_kind;
    CUpti_MetricValue      metric_value;
    timestamp_t            time;
    int                    meta_id;
    CUdevice               device;
    CUcontext              context;
    CUpti_EventGroupSets  *passes;
    CUpti_EventGroupSet   *set;
    CUpti_EventGroup      *groups;
    CUextra                extras[GRP_N];
    uint                   groups_count;
    uint                   events_count;
    uint                   init;  // Is already initialized
    uint                   reset; // Is set to reset
    uint                   read;  // Is set to read
    uint                   ban;   // Is banned
    uint                   ban_count;
} CUmeta;

typedef struct CUroot_s {
    CUpti_SubscriberHandle subscriber;
    char                  *metric_name;
    int                    metric_num;
    uint                   devs_count;
    uint                   ctxs_count;
    uint                   protected;
} CUroot;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static CUroot    root;
static uint      meta_count;
static CUmeta    meta[CTX_N]; //Context queue
static gpuproc_t pool[DEV_N];
static cupti_t   cupti;
static cuda_t    cu;
static uint      ok;

#define METRIC_CPI       1
#define METRIC_FLOPS_SP  2
#define METRIC_FLOPS_DP  3

#define cc(call)                                                                \
    do {                                                                        \
        CUptiResult _status = call;                                             \
        if (_status != CUPTI_SUCCESS) {                                         \
            const char *errstr;                                                 \
            cupti.GetResultString(_status, &errstr);                            \
            debug(#call ": FAILED, %s (context %p)", errstr, m->context);       \
            context_ban(m);                                                     \
            return 0;                                                           \
        }                                                                       \
    } while (0)

static void callback(void *user_data, CUpti_CallbackDomain domain, CUpti_CallbackId id, CUpti_CallbackData *data);

static int enum_metric()
{
    if (!strcmp(root.metric_name, "ipc")) {
        return METRIC_CPI;
    }
    if (!strcmp(root.metric_name, "flop_count_sp")) {
        return METRIC_FLOPS_SP;
    }
    if (!strcmp(root.metric_name, "flop_count_dp")) {
        return METRIC_FLOPS_DP;
    }
    return 0;
}

void gpu_cupti_load(gpuproc_ops_t *ops)
{
    if ((root.metric_name = getenv("CUPTI_METRIC")) == NULL) {
        debug("CUPTI_METRIC environent variable doesn't found using default %s", DEFAULT_CUPTI_METRIC);
        root.metric_name = malloc(strlen(DEFAULT_CUPTI_METRIC) +1 );
        strcpy(root.metric_name, DEFAULT_CUPTI_METRIC);
    }
    debug("Using CUPTI metric %s", root.metric_name);
    root.metric_num = enum_metric();
    //
    if (state_fail(cupti_open(&cupti, &cu))) {
        debug("Can not open CUPTI: %s", state_msg);
        return;
    }
    cupti_count_devices(&root.devs_count); 
    //
    apis_set(ops->get_api,       gpu_cupti_get_api);
    apis_set(ops->init,          gpu_cupti_init);
    apis_set(ops->dispose,       gpu_cupti_dispose);
    apis_set(ops->read,          gpu_cupti_read);
    apis_set(ops->count_devices, gpu_cupti_count_devices);
    
    debug("Loaded CUPTI (%d devices)", root.devs_count);
}

void gpu_cupti_get_api(uint *api)
{
    *api = API_CUPTI;
}

state_t gpu_cupti_dispose(ctx_t *c)
{
    return cupti_close();
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
    state_t s;
    suscription_t *sus;
    while (pthread_mutex_trylock(&lock));
    debug("Initializing CUPTI");
    if (ok) {
        goto fini;
    }
    // Setting callbacks
    if (state_fail(s = intern_init())) {
        debug("Error while initializing CUPTI: %s", state_msg);
        goto fini;
    }
    // Setting suscriptions
    sus = suscription();
    sus->call_main  = gpu_cupti_pool;
    sus->time_relax = 2000;
    sus->time_burst = 2000;
    sus->suscribe(sus);
    ok = 1;
    debug("Initialized CUPTI");
    fini:
    pthread_mutex_unlock(&lock);
    return EAR_SUCCESS;
}

state_t gpu_cupti_count_devices(ctx_t *c, uint *devs_count)
{
    *devs_count = root.devs_count;
    return EAR_SUCCESS;
}

#if 0
static void context_destroy(CUmeta *m)
{
    cupti.EventGroupSetsDestroy(m->passes);
    memset(m, 0, sizeof(CUmeta));
}
#endif

static void context_ban(CUmeta *m)
{
    ++m->ban_count;
    m->ban = (m->ban_count >= 5);
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
    debug("context: %p", m->context);
    debug("met. id: 0x%X", m->metric_id);
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
                if (!strcmp(event_name, "event_name")) {
                    debug("unknown (id 0x%X)", events_ids[e]);
                } else {
                    debug("%s (id 0x%X)", event_name, events_ids[e]);
                }
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

        // Counting total instances
        //size = sizeof(m->extras[g].total_instances_count);
        //cc(cupti.DeviceGetEventDomainAttribute(metricData->device, groupDomain, CUPTI_EVENT_DOMAIN_ATTR_TOTAL_INSTANCE_COUNT, &size, &numTotalInstances));

        // Enabling all domains
        cc(cupti.EventGroupSetAttribute(m->groups[g], CUPTI_EVENT_GROUP_ATTR_PROFILE_ALL_DOMAIN_INSTANCES, sizeof(aux), &aux));
        cc(cupti.EventGroupEnable(m->groups[g]));

        for (e = 0; e < m->extras[g].events_count; ++e) {
            size = sizeof(m->extras[g].events_names[e]);
            cc(cupti.EventGetAttribute(m->extras[g].events_ids[e], CUPTI_EVENT_ATTR_NAME, &size, m->extras[g].events_names[e]));

            if (!strcmp(m->extras[g].events_names[e], "event_name")) {
                sprintf(m->extras[g].events_names[e], "ev%X", m->extras[g].events_ids[e]); 
            }
        }
        
        size = sizeof(CUpti_MetricValueKind);
        cc(cupti.MetricGetAttribute(m->metric_id, CUPTI_METRIC_ATTR_VALUE_KIND, &size, &m->metric_kind));
    }
    return 1;
}

static int metric_create(CUmeta *m)
{
    debug("Resetting...");
    m->reset = 0;
  
    #if 0 
    char *metric_name;
    if (m->meta_id == 1) {
        metric_name = "ipc";
    } else {
        metric_name = "flop_count_sp";
        metric_name = "flop_count_dp";
        metric_name = "dram_utilization";
    }
        metric_name = "ipc";
    #endif
 
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
    //cc(cupti.EventGroupAddEvent(m->passes->sets->eventGroups, (CUpti_EventID) 0x16000090));

    // Print (or debugging)
    if (!metric_print(m)) {
        return 0;
    }
    // Breakdown
    if (!metric_enable(m)) {
        return 0;
    }
    // Getting the timestamp
    timestamp_get(&m->time);
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
    debug("Pooled %lu/%d events (ctx %p)", events_read, m->extras[g].events_count, m->context);

    for (e = 0; e < events_count; ++e) {
        for (i = 0; i < m->extras[g].instances_count; ++i) {
            m->extras[g].events_counters[e] = buffer[(i * events_count) + e];
        }
        // Future: normalize these values
        debug("D%d, %s: %llu", m->device, m->extras[g].events_names[e], m->extras[g].events_counters[e]);
    }
    // Kind: Events
    // --------------------
    // CPI     : DOUBLE (0)
    // FLOPS_SP: UINT64 (1): fadd + fmul + ffma (OPS)
    // FLOPS_DP: UINT64 (1): fadd (0x16000090) + fmul (0x16000091) + ffma (0x16000092)
    //
    
    #if 0
    // This doesn't works well
    cc(cupti.MetricGetValue(m->device, m->metric_id,
        m->events_count * sizeof(CUpti_EventID), m->extras[g].events_ids,
        m->events_count * sizeof(uint64_t), (uint64_t *) m->extras[g].events_counters,
        (uint64_t) timestamp_diffnow(&m->time, TIME_NSECS), &m->metric_value));
    #endif
    while (pthread_mutex_trylock(&lock));
    if (root.metric_num == METRIC_CPI) {
        pool[m->device].insts  += (ullong) m->extras[g].events_counters[0];
        pool[m->device].cycles += (ullong) m->extras[g].events_counters[1];
    }
    if (root.metric_num == METRIC_FLOPS_SP) {
        pool[m->device].flops_sp += (ullong) m->extras[g].events_counters[0];
        pool[m->device].flops_sp += (ullong) m->extras[g].events_counters[1];
        pool[m->device].flops_sp += (ullong) m->extras[g].events_counters[2] * 2LLU;
    }
    if (root.metric_num == METRIC_FLOPS_DP) {
        pool[m->device].flops_dp += (ullong) m->extras[g].events_counters[0];
        pool[m->device].flops_dp += (ullong) m->extras[g].events_counters[1];
        pool[m->device].flops_dp += (ullong) m->extras[g].events_counters[2] * 2LLU;     
    }
    //gpuproc_data_print(pool, fderr);
    pthread_mutex_unlock(&lock);
    // Saving metadata
    pool[m->device].time     = m->time;
    pool[m->device].samples += 1;
    // Getting new time
    timestamp_get(&m->time);
    
    return 1;
}

static void metric_read(CUmeta *m)
{
    int i;

    for (i = 0; i < m->set->numEventGroups; ++i) {
        if (!metric_read_group(m, i)) {
            //return;
        }
    }
}

static int context_find(CUcontext pctx, int meta_id, uint *index)
{
    uint found  = 0;
    int i;

    for (i = CTX_N-1; i >= 0 && !found; --i) {
        if (!meta[i].init && !meta[i].reset && !meta[i].ban) {
        //if (!meta[i].init && !meta[i].reset) {
            *index = i;
        }
        found = (meta[i].context == pctx) && (meta[i].meta_id == meta_id);
    }
    if (found) {
        *index = i+1;
    }
    #if 0
    if (found) {
        debug("Found context %p in index %d", pctx, *index);
    } else {
        debug("Doen't found context %p, free index %d", pctx, *index);
    }
    #endif
    //debug("Finding... %d (%u)", found, *index);
    return found;
}

#if 0
static void context_pop(CUcontext pctx, int meta_id)
{
    uint i;

    if(!context_find(pctx, meta_id, &i)) {
        return;
    }
    meta[i].read = 0;
}
#endif

static void context_push(CUcontext pctx, int meta_id)
{
    uint i;

    if (meta_count >= CTX_N) {
        return; 
    }
    if(!context_find(pctx, meta_id, &i))
    {
        debug("Adding new context to queue... %p, id %d, meta %d", pctx, i, meta_id);
        cu.CtxGetDevice(&meta[i].device);
        meta[i].context = pctx;
        meta[i].meta_id = meta_id;
        meta[i].reset   = 1;
        meta_count     += 1;
    } 
    // If not banned and not initted.
    else if (!meta[i].ban && !meta[i].init) {
        meta[i].reset = 1;
    }
}

static void callback(void *user_data, CUpti_CallbackDomain domain, CUpti_CallbackId id, CUpti_CallbackData *data)
{
    //debug("Callback ... %s %p", data->functionName, data->context);
    uint enter = (data->callbackSite != CUPTI_API_EXIT);

    if (data->context == NULL) {
        return;
    }
    // If kernel launch
    if (id == CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel) {
        if (enter) {
            context_push(data->context, 1);
            //context_push(data->context, 2);
        } else {
            //context_pop(data->context, 1);
        }
    }
}

state_t gpu_cupti_pool(void *p)
{
    uint d;
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

state_t gpu_cupti_read(ctx_t *c, gpuproc_t *data)
{
    while (pthread_mutex_trylock(&lock));
    memcpy(data, pool, root.devs_count * sizeof(gpuproc_t));
    pthread_mutex_unlock(&lock);

    return EAR_SUCCESS;
}
