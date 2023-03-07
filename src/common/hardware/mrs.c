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

#define mrs_function(type, name, reg) \
type mrs_ ##name () { \
    type var; \
    mrs_asm(reg) \
    return var; \
}

mrs_function(ullong, midr  , MIDR_EL1  );
mrs_function(ullong, cntfrq, CNTFRQ_EL0);

