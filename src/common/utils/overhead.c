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

#include <unistd.h>
#include <common/system/time.h>
#include <common/utils/overhead.h>
#include <common/string_enhanced.h>

#define N_QUEUE 256

typedef struct ohdata_s {
    const char *description;
    timestamp_t time_start;
    timestamp_t time_stop;
    ullong accum_us;
    uint active;
} ohdata_t;

static uint        init;
static timestamp_t global_time_start;
static ohdata_t    queue[N_QUEUE];
static uint        queue_last;

void overhead_suscribe(const char *description, uint *id)
{
    if (!init) {
        timestamp_get(&global_time_start);
        init = 1;
    }

    *id = queue_last;
    queue[*id].description = description;
    queue[*id].active = 1;
    queue_last++;
}

void overhead_start(uint id)
{
    timestamp_get(&queue[id].time_start);
}

void overhead_stop(uint id)
{
    timestamp_get(&queue[id].time_stop);
    queue[id].accum_us += timestamp_diff(&queue[id].time_stop, &queue[id].time_start, TIME_USECS);
}

void overhead_report()
{
    double global_accum_s;
    double accum_s;
    int id;

    tprintf_init(fderr, STR_MODE_DEF, "14 14 32");
    tprintf("Accum. secs.||%% from start||Description");
    tprintf("------------||------------ ||-----------");

    global_accum_s = ((double) timestamp_diffnow(&global_time_start, TIME_USECS)) / 1000000.0;

    for(id = 0; id < queue_last; ++id) {
        accum_s = ((double) queue[id].accum_us) / 1000000.0;
        tprintf("%0.2lf||%0.2lf||%s", accum_s, (accum_s / global_accum_s)*100.0, queue[id].description);
    }
}

int main(int argc, char *argv[])
{
    uint id1;
    uint id2;
    uint id3;
    int i;

    overhead_suscribe("CPUFreq read", &id1);
    overhead_suscribe("PIPE communication", &id2);
    overhead_suscribe("GPU read", &id3);

    for (i = 0; i < 5; ++i) {
        overhead_start(id1);
        sleep(1);
        overhead_stop(id1);
        overhead_start(id2);
        sleep(2);
        overhead_stop(id2);
        overhead_start(id3);
        sleep(1);
        overhead_stop(id3);
    }

    overhead_report();

    return 0;
}