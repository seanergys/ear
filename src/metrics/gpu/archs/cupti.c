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
#include <common/output/debug.h>
#include <common/system/monitor.h>
#include <common/system/symplug.h>
#include <metrics/common/cupti.h>
#include <metrics/gpu/archs/cupti.h>

typedef struct CUmeta_s {
	CUdevice              device;
	CUcontext             context;
	CUpti_MetricID        metric_id;
	CUpti_EventGroupSets *s;
	CUpti_EventGroup      group;
	CUpti_EventID         event_ids[8];
	char                 *event_names[8];
	uint                  events_count;
	uint                  instances_count;
} CUmeta;

typedef struct CUstate_s {
	uint reset;
	uint init;
	uint read;
	uint stop;
	uint ban;
} CUstate;

typedef struct CUpool_s {
	ullong instructions;
	ullong cycles;
} CUpool;

#define N_CTX   32

static CUpti_SubscriberHandle subscriber;
static CUmeta   meta[N_CTX];
static CUstate  stat[N_CTX];
static CUpool   pool[N_CTX];
static uint     devs_count;

#define clean_call(call)                                                    \
do {                                                                        \
    CUptiResult _status = call;                                             \
    if (_status != CUPTI_SUCCESS) {                                         \
		const char *errstr;                                                 \
		cupti.GetResultString(_status, &errstr);                            \
		debug(#call ": FAILED, %s", errstr);                                \
        s->ban = 1;                                                         \
        return;                                                             \
    }                                                                       \
} while (0)

static void intern_reset(CUmeta *m, CUstate *s)
{
    size_t size;
    int ret;

    clean_call(cupti.MetricGetIdFromName(m->device, "ipc", &m->metric_id));
    clean_call(cupti.MetricGetNumEvents(m->metric_id, &m->events_count));

    // CUpti_EventGroupSets: a struct that contains an array of CUpti_EventGroupSet
    // CUpti_EventGroupSet: a struct that contains an array of CUpti_EventGroup
    // CUpti_EventGroup: an array of events
    clean_call(cupti.MetricCreateEventGroupSets(m->context, sizeof(m->metric_id), &m->metric_id, &m->s));
    clean_call(cupti.SetEventCollectionMode(m->context, CUPTI_EVENT_COLLECTION_MODE_CONTINUOUS));

    // flop_count_dp just have 1 GroupSet set with 1 Group of Events.
    m->group = m->s->sets->eventGroups[0];

    // Enabling all domains
    clean_call(cupti.EventGroupSetAttribute(m->group, CUPTI_EVENT_GROUP_ATTR_PROFILE_ALL_DOMAIN_INSTANCES, sizeof(ret), &ret));
    clean_call(cupti.EventGroupEnable(m->group));

    // Attributes: group instances
    size = sizeof(m->instances_count);
    clean_call(cupti.EventGroupGetAttribute(m->group, CUPTI_EVENT_GROUP_ATTR_INSTANCE_COUNT, &size, &m->instances_count));

    // Attributes: event ids
    size = m->events_count * sizeof(CUpti_EventID);
    clean_call(cupti.EventGroupGetAttribute(m->group, CUPTI_EVENT_GROUP_ATTR_EVENTS, &size, m->event_ids));

    //
    s->reset = 0;
    s->read = 1;
}

static void intern_read(CUmeta *m, CUstate *s)
{
	static ulong values[16384];
    size_t size2 = sizeof(CUpti_EventID) * 8;
    size_t size1 = sizeof(ullong) * 1024;
    int v0, v1, v;
    size_t ret;

    memset(values, 0, 16384*sizeof(ulong));
    clean_call(cupti.EventGroupReadAllEvents(m->group, CUPTI_EVENT_READ_FLAG_NONE, &size1, values, &size2, m->event_ids, &ret));

    for (v = v0 = 0, v1 = 1; v < m->instances_count; v += 1, v0 += 2, v1 += 2) {
		#if SHOW_DEBUGS && 0
		if (values[v0] != 0 || values[v1] != 0) {
			debug("read ins/cyc: %lu/%lu", values[v0], values[v1]);
		}
		#endif
        pool[(int) m->device].instructions += values[v0];
        pool[(int) m->device].cycles       += values[v1];
    }
}

static int intern_find(CUcontext pctx, uint *index)
{
    uint found  = 0;
    int i;

    for (i = N_CTX-1; i >= 0 && !found; --i) {
        if (!stat[i].init) {
            *index = i;
        }
        found = (meta[i].context == pctx);
    }
    if (found) {
        *index = i+1;
    }
    return found;
}

static void intern_pop(CUcontext pctx)
{
    uint i;

    if(!intern_find(pctx, &i)) {
        return;
    }
    stat[i].stop = 1;
}

static void intern_push(CUcontext pctx)
{
    uint i;

    if(!intern_find(pctx, &i))
	{
    	cuda.CtxGetDevice(&meta[i].device);
    	meta[i].context = pctx;
    	stat[i].reset   = 1;
    	stat[i].init    = 1;
    }
}

void intern_callback(void *user_data, CUpti_CallbackDomain domain, CUpti_CallbackId id, CUpti_CallbackData *data)
{
	uint enter = (data->callbackSite != CUPTI_API_EXIT);

	if (data->context == NULL) {
		return;
	}
	// If kernel launch
	if (id == CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel) {
		if (enter) {
			intern_push(data->context);
		} else {
			intern_pop(data->context);
		}
	}
}

static state_t intern_init()
{
	CUptiResult s;
	char *str;

    if ((s = cupti.Subscribe(&subscriber, (CUpti_CallbackFunc) intern_callback, NULL)) != CUPTI_SUCCESS) {
		cupti.GetResultString(s, (const char **) &str);
		return_msg(EAR_ERROR, str);
	}
    if ((s = cupti.EnableCallback(1, subscriber, CUPTI_CB_DOMAIN_DRIVER_API, CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel)) != CUPTI_SUCCESS) {
		cupti.GetResultString(s, (const char **) &str);
		return_msg(EAR_ERROR, str);
	}

	return EAR_SUCCESS;
}
#endif

/*
 *
 * Public
 * Functions
 *
 */
static uint ok_load;
static uint ok_init;

state_t cupti_load()
{
}

state_t cupti_init(ctx_t *c)
{
	suscription_t *sus;

	if (!ok_load) {
		return EAR_ERROR;
	}
	if (ok_init) {
		return EAR_SUCCESS;
	}

	sus = suscription();
	sus->call_main  = cupti_pool;
	sus->time_relax = 1500;
	sus->time_burst = 1500;
	sus->suscribe(sus);
	
	sus = suscription();
	sus->call_main  = cupti_read;
	sus->time_relax = 5000;
	sus->time_burst = 5000;
	sus->suscribe(sus);

	ok_init = 1;
	
	return EAR_SUCCESS;
}

state_t cupti_dispose(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t cupti_pool(void *p)
{
	uint d;

	for (d = 0; d < N_CTX; ++d) {
		if (stat[d].reset && !stat[d].ban) {
			intern_reset(&meta[d], &stat[d]);
		}
		if (stat[d].read && !stat[d].ban) {
			intern_read(&meta[d], &stat[d]);
		}
	}
	return EAR_SUCCESS;
}

state_t cupti_read(ctx_t *c, gpu_t *data)
{
	uint d;

	for (d = 0; d < devs_count; ++d) 
	{
		debug("D%d: %llu %llu", d, pool[d].instructions, pool[d].cycles); 
	}
	return EAR_SUCCESS;
}
