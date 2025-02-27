/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#ifndef METRICS_COMMON_IPMI_DRIVER_H
#define METRICS_COMMON_IPMI_DRIVER_H

//#include <metrics/common/ipmi.h>
#include <ipmi.h>

typedef struct sdr_s {
    ipmi_addr_t *addr;
    ushort       id;
    uchar        type;
    char         name[16];
    char         name_lower[16]; // Name in lower-case
    uchar        entity_instance;
    uchar        entity_logical;
    uchar        sensor_no;
    uchar        sensor_type;
    uchar        new_addr_slave;
    uchar        new_addr_lun;
    uchar        new_addr_channel;
    uchar        formula_complement;
    uchar        formula_rate_unit;
    uchar        formula_modifier;
    uchar        formula_percentage;
    uchar        formula_result_unit;
    char        *formula_result_unit_str;
    uchar        formula_modifier_unit;
    uchar        formula_lin_function;
    int          formula_m;
    int          formula_b;
    int          formula_r_exp;
    int          formula_b_exp;
} sdr_t;

typedef struct ipmi_dev_s {
    int          fd;
    int          no; //number in the array
    char         path[32];
    uchar        id;
    uchar        has_sdr;
    uchar        has_fru;
    sdr_t       *records;
    uint         records_count;
    ipmi_addr_t  addrs[16];
    uint         addrs_count;
} ipmi_dev_t;

state_t ipmi_driver_open();

void ipmi_driver_close();

int ipmi_driver_sensors_find(char *string, ipmi_sensor_t **sensors, uint *sensors_count);

state_t ipmi_driver_sensors_read(ipmi_sensor_t *sensors, uint sensors_count, ipmi_reading_t *values);

// OEM
void ipmi_driver_cmd_set_paddr(ipmi_cmd_t *pkg, ipmi_addr_t *addr);

void ipmi_driver_cmd_set_msg(ipmi_cmd_t *pkg, uchar netfn, uchar cmd, ushort data_len);

state_t ipmi_driver_cmd_send(int dev_no, ipmi_cmd_t *pkg);

#endif //METRICS_COMMON_IPMI_DRIVER_H