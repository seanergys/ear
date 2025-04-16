#define SHOW_DEBUGS 1

#define _GNU_SOURCE
#include <stdlib.h>
#include <pthread.h>
#include <common/output/debug.h>
#include <metrics/energy/node/energy_sd650.c>

void energy_load(void *conf)
{
    energy_plugin_load(conf);
    energy_hwmon_load();
    energy_ipmi_oem_load();
    energy_ipmi_auto_load();
}

state_t energy_init()
{
    if (state_ok(ops.init)) {
        if (ops.pool != NULL) {
            sus = suscription();
		    sus->call_init  = NULL;
		    sus->call_main  = ops.pool();
		    sus->time_relax = ops.pooling_time_relax();
		    sus->time_burst = ops.pooling_time_burst();
        }
    }
}

void energy_dispose()
{
    monitor_unregister(&sus);
    ops.dipose();
}

state_t energy_read(void *data)
{
    ops.read();
}

void energy_diff(void *data2, void *data1, energy_t *energy)
{
    ops.diff(data2, data1, energy);
}

void *energy_alloc(void **data)
{
}
