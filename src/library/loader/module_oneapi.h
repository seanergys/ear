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

#ifndef LIBRARY_LOADER_ONEAPI_H
#define LIBRARY_LOADER_ONEAPI_H

int module_oneapi_is();
int module_constructor_oneapi(char *path_lib_so, char *libhack);
void module_destructor_oneapi();
int is_oneapi_enabled();
void ear_oneapi_enabled();

#endif
