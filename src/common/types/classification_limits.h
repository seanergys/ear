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
* EAR is an open source software, and it is licensed under both the BSD-3 license
* and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
* and COPYING.EPL files.
*/

<<<<<<< HEAD:src/plugin_manager/plugins/metrics.h
#ifndef UP_METRICS_H
#define UP_METRICS_H

#include <metrics/metrics.h>
#include <plugin_manager/plugin_manager.h>

typedef struct mets_s {
    metrics_info_t mi;
    metrics_read_t mr;
} mets_t;

#endif //UP_METRICS_H
=======
#ifndef APP_CLASSIFICATION_LIMITS
#define APP_CLASSIFICATION_LIMITS


#define CPI_CPU_BOUND_DEF         0.6
#define GBS_CPU_BOUND_DEF         30.0
#define CPI_BUSY_WAITING_DEF      0.4
#define GBS_BUSY_WAITING_DEF      3.0
#define GFLOPS_BUSY_WAITING_DEF   0.1
#define IO_TH_DEF                 10.0
#define CPI_MEM_BOUND_DEF         0.8
#define GBS_MEM_BOUND_DEF         100.0

#endif // APP_CLASSIFICATION_LIMITS
>>>>>>> cddce1b4719ecd093db49fb073d5ada84b1ebc1d:src/common/types/classification_limits.h
