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

//#define SHOW_DEBUGS 1

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <common/config.h>
#include <common/states.h>
#include <common/output/verbose.h>
#include <management/cpufreq/frequency.h>
#include <library/common/externs.h>
#include <library/policies/imc_policy_support.h>


extern volatile uint ear_periodic_mode;

uint must_increase_imc(imc_data_t *my_imc_data,uint cpu_pstate,uint imc_pstate,uint ref_cpu_pstate,uint ref_imc_pstate,signature_t *sig,double penalty)
{
	double metric,metric_ref;
	double metric_max,metric_min;
	double metric_var,power_var;
	int i;
	ull tflops = 0;
	ull cflops = 0;
	double tflopss,cflopss;
	uint ref_pstate;
	ref_pstate = ref_cpu_pstate;
	verbose(2,"Validating IMC freq selection: cpu_pstate %u imc_pstate %u",ref_pstate,ref_imc_pstate);
	power_var = (my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].power - sig->DC_power)/my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].power; 
	if (ear_periodic_mode == PERIODIC_MODE_OFF){
		metric_max = my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].time + (penalty* my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].time);
		metric = sig->time;
		metric_var = (sig->time-my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].time)/my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].time;
		verbose(2,"Using time as reference : Current %.3lf Max %.3lf energy_saving %.2lf (%.2lf - %.2lf)",metric,metric_max,(power_var-metric_var)*100,power_var,metric_var);
		return (metric > metric_max);
	}else{
		/* We were using ref_pstate = min_pstate */
		#if USE_CPI
		double CPI;
		CPI = (double)my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].cycles/(double)my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].instructions;
		metric_max = CPI + CPI*penalty;
		metric = sig->CPI;
		metric_var = (CPI-metric)/CPI;
		verbose(2,"Using CPI as reference : Current %.3lf Max %.3lf energy_saving %.2lf (%.2lf - %.2lf)",metric,metric_max,(power_var-metric_var)*100,power_var,metric_var);
		return (metric > metric_max);	
		#endif
		#if USE_GBS
		metric_min = my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].GBS - (penalty* my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].GBS);
		metric = sig->GBS;
		metric_var = (my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].GBS-sig->GBS)/my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].GBS;
		verbose(2,"Using GBS as reference : Current %.3lf Min %.3lf energy_saving %.2lf (%.2lf - %.2lf)",metric,metric_min,(power_var-metric_var)*100,power_var,metric_var);
		return (metric < metric_min);
		#endif
		#if USE_FLOPS
		for (i=0;i<FLOPS_EVENTS;i++) tflops += my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].FLOPS[i];	
		for (i=0;i<FLOPS_EVENTS;i++) cflops += sig->FLOPS[i];	
		tflopss = (double)tflops / (my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].time*1000000.0);
		cflopss = (double)cflops / (sig->time*1000000.0);
		metric_min =  tflopss - (penalty* tflopss);
		metric = cflopss;
		metric_var = (tflopss-cflopss)/tflopss;
		verbose(2,"Using FLOPS as reference : Current %.3lf Min %.3lf energy_saving %.2lf (%.2lf - %.2lf)",metric,metric_min,(power_var-metric_var)*100,power_var,metric_var);
		return (metric < metric_min);
		#endif
	}
}

void copy_imc_data_from_signature(imc_data_t *my_imc_data,uint cpu_pstate,uint imc_pstate,signature_t *s)
{
	int i;
	ullong tfops = 0;
	float flops = 0;
  my_imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].time = s->time;
  my_imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].power = s->DC_power;
  my_imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].GBS = s->GBS;
  my_imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].instructions = s->instructions;
  my_imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].cycles = s->cycles;
  memcpy(my_imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].FLOPS , s->FLOPS,sizeof(ull)*FLOPS_EVENTS);
	for (i=0;i<FLOPS_EVENTS;i++) tfops += s->FLOPS[i];
	flops = (float)tfops/(s->time*1000000000.0);
	verbose(2,"IMC data: time %.3lf power %2.lf GBS %.2lf I/s %.2f flops %.2f",my_imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].time,my_imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].power,my_imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].GBS,s->instructions/(s->time*1000000000.0),flops);
}

