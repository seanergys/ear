/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#ifndef METRICS_COMMON_IPMI_H
#define METRICS_COMMON_IPMI_H

#include <linux/ipmi.h>
//#include <common/states.h>

#if 1
    #define debug(...) { \
        printf(__VA_ARGS__); \
        printf("\n"); \
    }

    typedef unsigned char uchar;
    typedef unsigned short ushort;
    typedef unsigned int uint;
    typedef int state_t;

    char *state_msg __attribute__((weak));

    #define state_ok(state) \
        ((state) == EAR_SUCCESS)

    #define state_fail(state) \
        ((state) != EAR_SUCCESS)

    #define return_msg(no, msg) { \
        state_msg = msg; \
        return no; \
        }

    #define EAR_SUCCESS  0
    #define EAR_ERROR   -1
#endif

typedef struct ipmi_addr_s {
    struct ipmi_addr addr;
    uint addr_len;
} ipmi_addr_t;

typedef struct ipmi_cmd_s {
    ipmi_addr_t      addr;
    struct ipmi_msg  msg_send;
    uchar            msg_send_data[64];
    uchar            msg_recv_data[256];
    uint             msg_recv_data_len;
} ipmi_cmd_t;

typedef struct ipmi_reading_s {
    double value;
    uchar raw[5];
} ipmi_reading_t;

typedef struct ipmi_sensor_s {
    uchar        dev_no; // /dev/ipmi#
    uchar        sen_no;
    const char  *name;
    uchar        available;
    uchar        units;
    const char  *units_str;
    uchar        instance;
} ipmi_sensor_t;

state_t ipmi_open();

void ipmi_close();

int ipmi_devs_count();

int ipmi_has_hardware(char *string);

int ipmi_sensors_find(char *string, ipmi_sensor_t **sensors, uint *sensors_count);

state_t ipmi_sensors_read(ipmi_sensor_t *sensors, uint sensors_count, ipmi_reading_t *values);

// OEM
void ipmi_cmd_set_paddr(ipmi_cmd_t *pkg, ipmi_addr_t *addr);

void ipmi_cmd_set_addr(ipmi_cmd_t *pkg, int type, short channel, uchar slave_addr, uchar lun);

void ipmi_cmd_set_msg(ipmi_cmd_t *pkg, uchar netfn, uchar cmd, ushort data_len);

state_t ipmi_cmd_send(int dev_no, ipmi_cmd_t *pkg);

#endif //METRICS_COMMON_IPMI_H