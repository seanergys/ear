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

#include <common/output/debug.h>
#include <common/hardware/mrs.h>
#include <common/hardware/defines.h>

#if __ARCH_ARM
#define mrs_asm(reg) __asm__ volatile("mrs %0, " #reg : "=r" (var));
#else
#define mrs_asm(reg)
#endif

// Type formats
#define tf_ullong "%llu"

#define mrs_file(type, name, path) \
type mrsf_  ##name () { \
    FILE *file; \
    type var; \
    if (!(file = fopen(path, "r"))) { \
        return 0; \
    } \
    fscanf(file, tf_##type, &var); \
    fclose(file); \
    return var; \
}

#define mrs_function(type, name, reg, fcall) \
type mrs_ ##name () { \
    type var; \
    if ((var = fcall)) { \
        return var; \
    } \
    mrs_asm(reg) \
    return var; \
}

// File functions are hidden
mrs_file(ullong, midr,   "/sys/devices/system/cpu/cpu0/regs/identification/midr_el1"  );
mrs_file(ullong, revidr, "/sys/devices/system/cpu/cpu0/regs/identification/revidr_el1");
// Introduce new register reading functions as required
mrs_function(ullong, midr     , MIDR_EL1       , mrsf_midr());
mrs_function(ullong, cntfrq   , CNTFRQ_EL0     , 0);
mrs_function(ullong, dfr0     , ID_AA64DFR0_EL1, 0);
mrs_function(ullong, pmccntr  , PMCCNTR_EL0    , 0);
mrs_function(ullong, pmccfiltr, PMCCFILTR_EL0  , 0);