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

#ifndef LIBRARY_LOADER_MPI_H
#define LIBRARY_LOADER_MPI_H

#if MPICC_FLAGS
int  module_mpi(char *path_lib_so, char *libhack);
void module_mpi_destructor();
int  module_mpi_is_enabled();
int  module_mpi_is_intel();
int  module_mpi_is_open();
int  module_mpi_is_detected();
void module_mpi_set_forced();
#else
#define module_mpi(p, l)         0
#define module_mpi_destructor()
#define module_mpi_is_enabled()  0
#define module_mpi_is_intel()    0
#define module_mpi_is_open()     0
#define module_mpi_is_detected() 0
#define module_mpi_set_forced()
#endif

#endif //LIBRARY_LOADER_MPI_H