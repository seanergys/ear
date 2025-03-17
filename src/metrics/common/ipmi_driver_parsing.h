/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#ifndef METRICS_COMMON_IPMI_DRIVER_PARSING_H
#define METRICS_COMMON_IPMI_DRIVER_PARSING_H

#if IPMI_INDEPENDENCY
#include <ipmi_driver.h>
#else
#include <metrics/common/ipmi_driver.h>
#endif

void ipmi_driver_parse_device_id(uchar *data, ipmi_dev_t *dev);

// Returns type
uchar ipmi_driver_parse_sdr(uchar *data, sdr_t *rec, ipmi_addr_t *addr);

void ipmi_driver_parse_fru_inventory(uchar *data);

void ipmi_driver_parse_sensor_reading(uchar raw, sdr_t *rec, ipmi_reading_t *val);

#endif //METRICS_COMMON_IPMI_DRIVER_PARSING_H