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
* This file is licensed under both the BSD-3 license for individual/non-commercial
* use and EPL-1.0 license for commercial use. Full text of both licenses can be
* found in COPYING.BSD and COPYING.EPL files.
*/

#ifndef _IMC_SUPPORT_H
#define _IMC_SUPPORT_H

#include <common/types/signature.h>

typedef struct imc_data{
	double time;
	double GBS;
	double power;
	double CPI;
	ull  	 instructions;
	ull		 cycles;
	ull 	 FLOPS[FLOPS_EVENTS];
}imc_data_t;
#define NUM_UNC_FREQ 20
#define USE_FLOPS 0
#define USE_GBS 0
#define USE_CPI	0
#define USE_GBS_CPI 1
void copy_imc_data_from_signature(imc_data_t *my_imc_data,uint cpu_pstate,uint imc_pstate,signature_t *s);
uint must_increase_imc(imc_data_t *my_imc_data,uint cpu_pstate,uint imc_pstate,uint ref_cpu_pstate,uint ref_imc_pstate,signature_t *sig,double penalty);

#define IMC_MAX 0
#define IMC_MIN 1
#define IMC_VAL 2

#endif
