#define SHOW_DEBUGS 1

#include <pthread.h>
#include <common/system/time.h>
#include <common/output/debug.h>
#include <metrics/common/ipmi.h>

typedef struct consumption_s {
    uint64_t    energy; // mJ
    timestamp_t timestamp; // We use our timestamp because DCMI fails
    uint64_t    samples;
} consumption_t;

static pthread_mutex_t  lock = PTHREAD_MUTEX_INITIALIZER;
static uint             devs_count;
static ipmi_sensor_t   *sensors;
static uint             sensors_count;
static ipmi_reading_t  *sensors_readings;
static consumption_t   *pool;
static uint             opt_sysp[11] = { 1, 1, 1000 }; // Sys Power (power)
static uint             opt_lm50[11] = { 2, 1, 1000 }; // LM5066_PIN (power)
static uint            *opt = NULL;

static state_t energy_pool(void *data);

state_t energy_init(void **x)
{
    state_t s = EAR_SUCCESS;
    while (pthread_mutex_trylock(&lock));
    if (devs_count) {
        goto leave;
    }
    if (state_fail(s = ipmi_open())) {
        goto leave;
    }
    // Detecting sensors depending on hardware
    if(ipmi_sensors_find("LM5066_PIN,CWG_LM5066I_PIN", &sensors, &sensors_count)) { opt = opt_lm50; }
    else if (ipmi_sensors_find("Sys Power", &sensors, &sensors_count)) { opt = opt_sysp; }
    // If no sensors, error
    if (sensors_count == 0) {
        s = EAR_ERROR;
        goto leave;
    }
    devs_count = ipmi_devs_count();
    pool = calloc(devs_count, sizeof(consumption_t));
    // Monitoring
    if (opt[1]) {
        sus = suscription();
        sus->call_main  = energy_pool;
        sus->time_relax = 2000;
        sus->time_burst = 2000;
    }
leave:
    pthread_mutex_unlock(&lock);
    return s;
}

state_t energy_dispose(void **x)
{
    return EAR_SUCCESS;
}

#define goto_msg(where, state, message) { \
    state; \
    state_msg = message; \
    goto where; \
    }

#define goto_state(where, state) { \
    state; \
    goto where; \
    }

#define goto_msg(where, state, message) { \
    state; \
    state_msg = message; \
    goto where; \
    }

static state_t energy_pool(void *data)
{
    state_t s = EAR_SUCCESS;
    consumption_t rd = {0};
    ipmi_cmd_t pkg = {0};
    double fpower;
    double ftime;
    int s;

    while (pthread_mutex_trylock(&lock));
    //
    if (state_fail(s = ipmi_sensors_read(sensors, sensors_count, &sensors_readings))) {
        goto_state(leave, s = EAR_ERROR);
    }
    timestamp_get(&rd.timestamp);
    // If there are no changes between timestamps, continue
    if (ftime < 0.1) {
        goto_msg(leave, s = EAR_WARNING, "Insufficient time to get new samples");
    }
    for (s = 0; s < sensors_count; ++s) {
        // Cleaning and reading
        memset(&rd, 0, sizeof(consumption_t));
        // Computing time between samples
        ftime   = (double) timestamp_fdiff(&rd.timestamp, &pool[s].timestamp, TIME_SECS, TIME_MSECS);
        fpower  = (pool[d].samples > 0)? sensors_readings[s].value * ftime: sensors_readings[s].value;
        fpower *= (double) opt[2];
        // Pooling things
        pool[d].energy    = (opt[1])? ((uint64_t) fpower) + pool[d].energy:
                                      ((uint64_t) sensors_readings[s].value);
        pool[d].timestamp = rd.timestamp;
        pool[d].samples  += 1;
        debug("pool->sensor: %0.02lf", sensors_readings[s].value);
        debug("pool->ftime : %.02lf s", ftime);
        debug("pool->tstamp: %ld s, %ld ns", pool[d].timestamp.tv_sec, pool[d].timestamp.tv_nsec);
        debug("pool->fpower: %0.02lf W", fpower);
        debug("pool->energy: %lu mJ", pool[d].energy);
    }
    leave:
    if (data != NULL) {
        memcpy(data, pool, sizeof(consumption_t)*devs_count);
    }
    pthread_mutex_unlock(&lock);
    return s;
}

state_t energy_dc_read(void *x, void *data)
{
    return energy_pool(data);
}

state_t energy_datasize(size_t *size)
{
    *size = sizeof(consumption_t)*devs_count;
    return EAR_SUCCESS;
}

state_t energy_units(uint *units)
{
    *units = 1000; // mJ
    return EAR_SUCCESS;
}

state_t energy_to_str(char *buffer, void *data)
{
    sprintf(buffer, "%lu", ((consumption_t *) data)->energy);
    return EAR_SUCCESS;
}

uint energy_data_is_null(void *data)
{
    return ((consumption_t *) data)->energy == 0;
}