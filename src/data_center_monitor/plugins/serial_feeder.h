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

#ifndef UP_SERIAL_FEEDER_H
#define UP_SERIAL_FEEDER_H

#include <common/system/plugin_manager.h>

typedef struct signature
{
    double          DC_power;
    double          DRAM_power;
    double          PCK_power;
    double          EDP;
    double          GBS;
    double          IO_MBS;
    double          TPI;
    double          CPI;
    double          Gflops;
    double          time;
    ullong          FLOPS[8];
    ullong          L1_misses;
    ullong          L2_misses;
    ullong          L3_misses;
    ullong          instructions;
    ullong          cycles;
    ulong           avg_f;
    ulong           avg_imc_f;
    ulong           def_f;
    double          perc_MPI;
} signature_t;

typedef struct serial_params_s {
    char   *data;
    size_t  data_size;
} serial_params_t;

#define serial_params_add_auto(sp, parameter) \
    serial_params_add(sp, #parameter, (void *) &parameter, sizeof(parameter));

#endif