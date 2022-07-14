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

#ifndef METRICS_STALLS_H
#define METRICS_STALLS_H

int init_stall_metrics();

void reset_stall_metrics();

void start_stall_metrics();

void stop_stall_metrics(long long *stall_cycles);

void get_stall_metrics(long long *total_stall_cycles);

#endif //METRICS_STALLS_H