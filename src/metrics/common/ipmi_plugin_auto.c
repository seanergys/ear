#define SHOW_DEBUGS 1

#include <pthread.h>
#include <common/output/debug.h>
#include <metrics/common/ipmi.h>

static pthread_mutex_t  lock = PTHREAD_MUTEX_INITIALIZER;
static ipmi_sensor_t   *sensors;
static uint             sensors_count;
static ipmi_reading_t  *sensors_readings;
static uint             devs_count;

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
    ipmi_sensors_find("Sys Power", &sensors, &sensors_count);
    // If no sensors, error
    if (sensors_count == 0) {
        s = EAR_ERROR;
        goto leave;
    }
    devs_count = ipmi_devs_count();
leave:
    pthread_mutex_unlock(&lock);
    return s;
}

state_t energy_dispose(void **x)
{
    return EAR_SUCCESS;
}

state_t energy_dc_read(void *x, void *data)
{
    int i;
    ipmi_sensors_read(sensors, sensors_count, &sensors_readings);
    for (i = 0; i < sensors_count; ++i) {
        debug("SEN%d: %0.02lf", i, sensors_readings[i]);
    }
    return EAR_SUCCESS;
}

state_t energy_datasize(size_t *size)
{
	*size = sizeof(double);
	return EAR_SUCCESS;
}

state_t energy_units(uint *units)
{
    *units = 1000; // mJ
    return EAR_SUCCESS;
}

int main(int argc, char *argv[])
{
    energy_init(NULL);
    energy_dc_read(NULL, NULL);
    sleep(2);
    energy_dc_read(NULL, NULL);
    return 0;
}
