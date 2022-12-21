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

#ifndef METRICS_COMMON_REDFISH_H
#define METRICS_COMMON_REDFISH_H

#include <common/types/generic.h>

#define REDTYPE_STR 0
#define REDTYPE_I32 1
#define REDTYPE_I64 2
#define REDTYPE_F32 3
#define REDTYPE_F64 4

state_t redfish_init(char *host);

state_t redfish_read(char *field, void *content, void *accum, uint *count, int type_flag);

state_t redfish_count_members(char *field, uint *count);

#endif //METRICS_COMMON_REDFISH_H
