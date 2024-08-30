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

#ifndef LIBRARY_LOADER_MPI_H
#define LIBRARY_LOADER_MPI_H

int module_mpi(char *path_lib_so, char *libhack);

int load_mpi_and_ear(char *libmpi_path);

void module_mpi_destructor();

// It returns if MPI is detected.
int module_mpi_is_enabled();

// It returns if the MPI version is OpenMPI.
int module_mpi_is_open();

#endif //LIBRARY_LOADER_MPI_H