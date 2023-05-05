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

#ifndef UMP_H
#define UMP_H

#include <common/types.h>
#include <common/hardware/topology.h>

// Universal Plugin
#define UPM_F_GET_INFO()   void    up_get_info   (cchar **tag)
#define UPM_F_INIT()       state_t up_init       ()
#define UPM_F_WORK()       state_t up_work       (cchar *tag, void *data)
#define UPM_F_DATA_ALLOC() void    up_data_alloc (void **data)
#define UPM_F_DATA_TOSTR() char   *up_data_tostr (void *data)

#define UPM_DEFINES() \
UPM_F_GET_INFO  (); \
UPM_F_INIT      (); \
UPM_F_WORK      (); \
UPM_F_DATA_ALLOC(); \
UPM_F_DATA_TOSTR();

#if 0
// Long version
#define UPM_F_GET_INFO  () void up_get_info   (cchar *tag)
#define UPM_F_INIT      () state_t up_init    ()
#define UPM_F_READ      () state_t up_read    (void *data)
#define UPM_F_REPORT    () state_t up_report  (void *data)
#define UPM_F_ALERT     () state_t up_alert   (void *data)
#define UPM_F_DATA_ALLOC() void up_data_alloc (void **data)
#define UPM_F_DATA_DIFF () void up_data_diff  (void *data2, void *data1, void *dataD)
#define UPM_F_DATA_TOSTR() void up_data_tostr (void *data)

#define UPM_DEFINES(name) \
UPM_F_GET_INFO  (); \
UPM_F_INIT      (); \
UPM_F_READ      (); \
UPM_F_REPORT    (); \
UPM_F_ALERT     (); \
UPM_F_DATA_ALLOC(); \
UPM_F_DATA_DIFF (); \
UPM_F_DATA_TOSTR();
#endif

#endif //UMP_H