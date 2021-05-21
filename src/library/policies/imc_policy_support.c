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
extern ulong def_imc_max_khz;
#define IMC_STEP 100000

double metric_diff(double metric_ref, double metric_curr, uint type) {
    if (type) {
        return (metric_curr - metric_ref) / metric_ref;
    }
    else {
        return (metric_ref - metric_curr) / metric_ref;
    }
}
#define DIFF_SIG 0.15
uint must_start(imc_data_t *my_imc_data,uint cpu_pstate, uint imc_pstate,
        uint ref_cpu_pstate, uint ref_imc_pstate, signature_t *sig)
{
	uint ref_pstate;
	double CPI,GBS,CPI_ref,GBS_ref, rCPI, rGBS;
	ref_pstate = ref_cpu_pstate;
	CPI_ref = (double)my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].cycles
            / (double)my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].instructions;
	GBS_ref = my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].GBS;
	CPI     = sig->CPI;
	GBS	    = sig->GBS;
	if (CPI > CPI_ref) rCPI = (CPI - CPI_ref)/CPI;
	else               rCPI = (CPI_ref - CPI)/CPI_ref;
	if (GBS > GBS_ref) rGBS = (GBS - GBS_ref)/GBS;
	else               rGBS = (GBS_ref - GBS)/GBS_ref;
	if ((rCPI > DIFF_SIG) || (rGBS > DIFF_SIG)) return 1;
	else return 0;
}

uint must_increase_imc(imc_data_t *my_imc_data,uint cpu_pstate, uint imc_pstate,
        uint ref_cpu_pstate, uint ref_imc_pstate, signature_t *sig, double penalty)
{
    double metric,metric_ref,metric2; // ,metric2_min,metric2_ref;
    double CPI;
    double metric_max,metric_min;
    double metric_var,power_var;
    int i;
    ull tflops = 0;
    ull cflops = 0;
    double tflopss,cflopss;
    uint ref_pstate;

    ref_pstate = ref_cpu_pstate;
    verbose(2,"Validating IMC freq selection: cpu_pstate %u imc_pstate %u",ref_pstate,ref_imc_pstate);

    double power_ref = my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].power;
    power_var = metric_diff(power_ref, sig->DC_power, 0); // (my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].power
            // - sig->DC_power)/my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].power;

    if (ear_periodic_mode == PERIODIC_MODE_OFF){
        metric_ref = my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].time;
        metric_max = metric_ref * (1 + penalty);
        metric = sig->time;
        // metric_var = (metric-metric_ref)/metric_ref;
        metric_var = metric_diff(metric_ref, metric, 1);
        verbose(2,"Using time as reference : Current %.3lf Ref %3.lf  Max %.3lf energy_saving %.2lf (%.2lf - %.2lf)",
                metric,metric_ref,metric_max,(power_var-metric_var)*100,power_var,metric_var);
        return (metric > metric_max);
    }else{
        /* We were using ref_pstate = min_pstate */
#if USE_CPI
        CPI = (double)my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].cycles
            / (double)my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].instructions;
        metric_max = CPI * (1 + penalty); // CPI + CPI*penalty;
        metric = sig->CPI;
        // metric_var = (CPI-metric)/CPI;
        metric_var = metric_diff(CPI, metric, 0);
        verbose(2,"Using CPI as reference : Current %.3lf Max %.3lf energy_saving %.2lf (%.2lf - %.2lf)",
                metric,metric_max,(power_var-metric_var)*100,power_var,metric_var);
        return (metric > metric_max);
#endif
#if USE_GBS
        metric_ref = my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].GBS;
        metric_min = metric_ref * (1 - penalty);
        metric = sig->GBS;
        // metric_var = (metric_ref - sig->GBS) / metric_ref;
        metric_var = metric_diff(metric_ref, metric, 0);
        verbose(2,"Using GBS as reference : Current %.3lf Ref %.3lf Min %.3lf energy_saving %.2lf (%.2lf - %.2lf)",
                metric,metric_ref,metric_min,(power_var-metric_var)*100,power_var,metric_var);
        return (metric < metric_min);
#endif
#if USE_FLOPS
        for (i=0;i<FLOPS_EVENTS;i++) tflops += my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].FLOPS[i];
        for (i=0;i<FLOPS_EVENTS;i++) cflops += sig->FLOPS[i];
        tflopss = (double)tflops / (my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].time*1000000.0);
        cflopss = (double)cflops / (sig->time*1000000.0);
        metric_min =  tflopss - (penalty* tflopss);
        metric = cflopss;
        // metric_var = (tflopss-cflopss)/tflopss;
        metric_var = metric_diff(tflopss, cflopss, 0);
        verbose(2,"Using FLOPS as reference : Current %.3lf Min %.3lf energy_saving %.2lf (%.2lf - %.2lf)",
                metric,metric_min,(power_var-metric_var)*100,power_var,metric_var);
        return (metric < metric_min);
#endif
#if USE_GBS_CPI
        metric_ref = my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].GBS;
        metric_min = metric_ref * (1 - penalty);
        metric     = sig->GBS;
        CPI         = my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].CPI;
        metric2     = sig->CPI;
        metric_max  = CPI + CPI*penalty;
        verbose(2,"Using GBS&CPI as reference : Current (%.2lf,%.2lf) Ref (%.2lf,%.2lf) Limits (%.2lf,%.2lf)",
                metric,metric2,metric_ref,CPI,metric_min,metric_max);
        return ((metric < metric_min) || (metric2>metric_max));
#endif
    }
}

uint must_decrease_imc(imc_data_t *my_imc_data,uint cpu_pstate,uint imc_pstate,
        uint ref_cpu_pstate,uint ref_imc_pstate,signature_t *sig, double min_eff_gain) {
    double metric,metric_ref,metric2;
    double CPI;
    double metric_max, metric_min, metric_gain, metric_gain_2;
    double imc_gain;
    double metric_var,power_var;
    int i;
    ull tflops = 0;
    ull cflops = 0;
    double tflopss, cflopss;
    uint ref_pstate;

    ref_pstate = ref_cpu_pstate;
    verbose(2,"Validating IMC freq selection: cpu_pstate %u imc_pstate %u refs %lu %lu", cpu_pstate, imc_pstate, ref_pstate, ref_imc_pstate);

    double power_ref = my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].power;
    power_var = metric_diff(power_ref, sig->DC_power, 0);

    ulong curr_imc_freq = sig->sig_ext->imc_freq;
    ulong ref_imc_freq = frequency_pstate_to_freq(ref_imc_pstate);
    imc_gain = (curr_imc_freq - ref_imc_freq) / ref_imc_freq;
    double perf_min_benefit = imc_gain * min_eff_gain;

    if (ear_periodic_mode == PERIODIC_MODE_OFF){
        metric_ref  = my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].time;
        metric      = sig->time;
        // metric_gain = (metric_ref - metric) / metric_ref;
        metric_gain = metric_diff(metric_ref, metric, 0);
        verbose(2,"Using time as reference: Current %.3lf Ref %3.lf Gain %.3lf IMC gain: %.3lf",
                metric, metric_ref, metric_gain, imc_gain);
        return (metric_gain < perf_min_benefit);
    }else{
        /* We were using ref_pstate = min_pstate */
#if USE_CPI
        CPI = (double)my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].cycles
            / (double)my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].instructions;
        metric      = sig->CPI;
        // metric_gain = (CPI - metric) / CPI;
        metric_gain = metric_diff(CPI, metric, 0);
        verbose(2,"Using CPI as reference: Current %.3lf Ref %3.lf Gain %.3lf IMC gain: %.3lf",
                metric, CPI, metric_gain, imc_gain);
        return (metric_gain < perf_min_benefit);
#endif
#if USE_GBS
        metric_ref  = my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].GBS;
        metric      = sig->GBS;
        // metric_gain = (metric - metric_ref) / metric_ref;
        metric_gain = metric_diff(metric_ref, metric, 1);
        verbose(2,"Using GBS as reference: Current %.3lf Ref %3.lf Gain %.3lf IMC gain: %.3lf",
                metric, metric_ref, metric_gain, imc_gain);
        return (metric_gain < perf_min_benefit);
#endif
#if USE_FLOPS
        for (i=0;i<FLOPS_EVENTS;i++) tflops += my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].FLOPS[i];
        for (i=0;i<FLOPS_EVENTS;i++) cflops += sig->FLOPS[i];
        tflopss     = (double)tflops / (my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].time*1000000.0);
        cflopss     = (double)cflops / (sig->time*1000000.0);
        metric      = cflopss;
        // metric_gain = (metric - tflopss) / tflopss;
        metric_gain = metric_diff(tflopss, metric, 1);
        verbose(2,"Using FLOPS as reference: Current %.3lf Ref %3.lf Gain %.3lf IMC gain: %.3lf",
                metric, tflopss, metric_gain, imc_gain);
        return (metric_gain < perf_min_benefit);
#endif
#if USE_GBS_CPI
        metric_ref  = my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].GBS;
        metric      = sig->GBS;
        metric_gain = metric_diff(metric_ref, metric, 1);
        CPI         = my_imc_data[ref_pstate*NUM_UNC_FREQ+ref_imc_pstate].CPI;
        metric2     = sig->CPI;
        metric_gain_2 = metric_diff(CPI, metric, 0);
        verbose(2,"Using GBS&CPI as reference: Current (%.2lf,%.2lf) Ref (%.2lf,%.2lf) Gain (%.3lf, %.3lf) IMC gain: %.3lf",
                metric, metric2,metric_ref,CPI,metric_gain, metric_gain_2, imc_gain);
        return ((metric_gain < perf_min_benefit) || (metric_gain_2 < perf_min_benefit));
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
    my_imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].CPI = s->CPI;
    my_imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].instructions = s->instructions;
    my_imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].cycles = s->cycles;
    memcpy(my_imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].FLOPS , s->FLOPS,sizeof(ull)*FLOPS_EVENTS);
    for (i=0;i<FLOPS_EVENTS;i++) tfops += s->FLOPS[i];
    flops = (float)tfops/(s->time*1000000000.0);
    verbose(2,"IMC data: time %.3lf power %2.lf GBS %.2lf I/s %.2f flops %.2f",
            my_imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].time,
            my_imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].power,
            my_imc_data[cpu_pstate*NUM_UNC_FREQ+imc_pstate].GBS,s->instructions/(s->time*1000000000.0),
            flops);
}

ulong select_imc_freq(ulong max,ulong min,float p)
{
	ulong new;
	new = max-(float)(max-min)*p;
	return new;
}
