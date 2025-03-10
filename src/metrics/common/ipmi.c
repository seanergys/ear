/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#include <pthread.h>
//#include <metrics/common/ipmi.h>
//#include <metrics/common/ipmi_driver.h>
#include <ipmi.h>
#include <ipmi_driver.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

state_t ipmi_open()
{
    state_t s;
    while (pthread_mutex_trylock(&lock));
    s = ipmi_driver_open();
    pthread_mutex_unlock(&lock);
    return s;
}

void ipmi_close()
{
    while (pthread_mutex_trylock(&lock));
    ipmi_driver_close();
    pthread_mutex_unlock(&lock);
}

int ipmi_sensors_find(char *string, ipmi_sensor_t **sensors, uint *sensors_count)
{
    return ipmi_driver_sensors_find(string, sensors, sensors_count);
}

state_t ipmi_sensors_read(ipmi_sensor_t *sensors, uint sensors_count, ipmi_reading_t **values)
{
    state_t s;
    while (pthread_mutex_trylock(&lock));
    s = ipmi_driver_sensors_read(sensors, sensors_count, values);
    pthread_mutex_unlock(&lock);
    return s;
}

// OEM
void ipmi_cmd_set_msg(ipmi_cmd_t *pkg, uchar netfn, uchar cmd, ushort data_len)
{
    ipmi_driver_cmd_set_msg(pkg, netfn, cmd, data_len);
}

state_t ipmi_cmd_send(int dev_no, ipmi_cmd_t *pkg)
{
    state_t s;
    while (pthread_mutex_trylock(&lock));
    s = ipmi_driver_cmd_send(dev_no, pkg);
    pthread_mutex_unlock(&lock);
    return s;
}

static ipmi_reading_t *readings1, *readings2;
static ipmi_sensor_t *sensors;
static uint sensors_count;

int main(int argc, char *argv)
{
    ipmi_open();
    ipmi_driver_sensors_find("dimm", &sensors, &sensors_count);
    ipmi_driver_sensors_read(sensors, sensors_count, &readings1);
    sleep(2);
    ipmi_driver_sensors_read(sensors, sensors_count, &readings2);
    return 0;
}