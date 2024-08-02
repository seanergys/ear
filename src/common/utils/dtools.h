/*
*
* This program is part of the EAR software.
*
* EAR provides a dynamic, transparent and ligth-weigth solution for
* Energy management. It has been developed in the context of the
* Barcelona Supercomputing Center (BSC)&Lenovo Collaboration project.
*
* 
* BSC Contact   mailto:ear-support@bsc.es
* 
*
* EAR is an open source software, and it is licensed under both the BSD-3 license
* and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
* and COPYING.EPL files.
*/

#ifndef COMMON_UTILS_DTOOLS_H
#define COMMON_UTILS_DTOOLS_H

// Given an address and a breakpoint in dtools_break() function, when the
// address or a near address is detected in malloc or free, dtools_break() is
// called, prompting the breakpoint in GDB.

// Set the address to control by dtools.
void dtools_set_address(void *address);

char *dtools_get_backtrace_library(char *buffer, int calls_count);

#endif
