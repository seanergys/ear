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

/**
*    \file projection.h
*    \brief Projections are used by time&power models.
*
*/

#ifndef _EAR_TYPES_PROJECTION
#define _EAR_TYPES_PROJECTION

#include <common/types/signature.h>
#include <common/types/coefficient.h>


/* Basic for compatibility and tools */
double basic_project_time(signature_t *sign,coefficient_t *coeff);
double basic_project_cpi(signature_t *sign,coefficient_t *coeff);
double basic_project_power(signature_t *sign,coefficient_t *coeff);


#endif
