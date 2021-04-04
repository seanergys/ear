/*
*
* EAR provides a dynamic, transparent and ligth-weigth solution for
* Energy management. It has been developed in the context of the
* Barcelona Supercomputing Center (BSC)&Lenovo Collaboration project.
*
* Copyright © 2017-present BSC-Lenovo
* BSC Contact   mailto:ear-support@bsc.es
* Lenovo contact  mailto:hpchelp@lenovo.com
*
* This file is licensed under both the BSD-3 license for individual/non-commercial
* use and EPL-1.0 license for commercial use. Full text of both licenses can be
* found in COPYING.BSD and COPYING.EPL files.
*/

#ifndef MPI_SUPPORT_H
#define MPI_SUPPORT_H
#include <library/api/mpi_support.h>
state_t mpi_app_init(polctx_t *c);
state_t mpi_app_end(polctx_t *c);
state_t mpi_call_init(polctx_t *c,mpi_call call_type);
state_t mpi_call_end(polctx_t *c,mpi_call call_type);
#endif
