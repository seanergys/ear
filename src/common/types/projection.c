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

//#define SHOW_DEBUGS 1
#include <common/types/projection.h>


double basic_project_cpi(signature_t *sign, coefficient_t *coeff)
{
  return (coeff->D * sign->CPI) +
    (coeff->E * sign->TPI) +
    (coeff->F);
}


double basic_project_time(signature_t *sign,coefficient_t *coeff)
{
  double ptime=0;
  if (coeff->available){
    double proj_cpi = basic_project_cpi(sign, coeff);
    double freq_src = (double) coeff->pstate_ref;
    double freq_dst = (double) coeff->pstate;

    ptime= ((sign->time * proj_cpi) / sign->CPI) *
      (freq_src / freq_dst);
  }
  return ptime;
}


double basic_project_power(signature_t *sign, coefficient_t *coeff)
{
  double ppower=0;
  if (coeff->available){
    ppower= (coeff->A * sign->DC_power) +
      (coeff->B * sign->TPI) +
      (coeff->C);
  }
  return ppower;
}
