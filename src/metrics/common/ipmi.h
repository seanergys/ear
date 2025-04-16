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
#if !IPMI_INDEPENDENCY
#include <common/states.h>
#endif

// Delete these regions when finished
#if IPMI_INDEPENDENCY
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

//  _____ _____  __  __ _____ 
// |_   _|  __ \|  \/  |_   _|
//   | | | |__) | \  / | | |  
//   | | |  ___/| |\/| | | |  
//  _| |_| |    | |  | |_| |_ 
// |_____|_|    |_|  |_|_____|
//
// What we don't support by now?
//   - SDRs and FRUs in IMPB addresses (other than INTERFACE).
//   - Full sensors with linearization functions other than 0x00. 
//   - Compact sensors.
// These missing features can be added as we require it.

#define IPMI_MAX_DEVS 4 // Max supported devices

typedef struct ipmi_addr_s {
    struct ipmi_addr addr;
    uint addr_len;
} ipmi_addr_t;

// This structure includes all the data required to send a message. It is also
// named as Command Package.
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
    uchar        dev_no; // device (/dev/ipmi#)
    uchar        rec_no; // record
    const char  *name;
    uchar        available;
    uchar        units;
    uchar        rate;
    const char  *units_str;
    uchar        instance;
} ipmi_sensor_t;

state_t ipmi_open();

void ipmi_close();

// Returns the IPMI devices opened (normally /dev/impiN).
int ipmi_devs_count();

// Checks if IPMI has a board attached to its system.
int ipmi_has_hardware(char *board_name);

// You can pass %d in string to retrieve a multi numbered sensor.
int ipmi_sensors_find(char *sensor_name, ipmi_sensor_t **sensors, uint *sensors_count);

// Allocates values memory if the inner pointer is NULL.
state_t ipmi_sensors_read(ipmi_sensor_t *sensors, uint sensors_count, ipmi_reading_t **values);

// OEM

// Sets an address in the CMD Package (PKG) based on an existing address.
void ipmi_cmd_set_paddr(ipmi_cmd_t *pkg, ipmi_addr_t *addr);

// Sets an address in the PKG based on separated components.
void ipmi_cmd_set_addr(ipmi_cmd_t *pkg, int type, short channel, uchar slave_addr, uchar lun);

// Sets the message part in the PKG based on separated components.
void ipmi_cmd_set_msg(ipmi_cmd_t *pkg, uchar netfn, uchar cmd, ushort data_len);

// Sends th ecommand to the device specied by its number
state_t ipmi_cmd_send(int dev_no, ipmi_cmd_t *pkg);

#endif //METRICS_COMMON_IPMI_H
