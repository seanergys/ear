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
#include <common/types/projection.h>
#include <library/policies/policy_api.h>
#include <daemon/local_api/eard_api.h>
#include <daemon/powercap/powercap_status.h>
#include <library/policies/policy_state.h>

typedef unsigned long ulong;
#ifdef EARL_RESEARCH
extern unsigned long ext_def_freq;
#define FREQ_DEF(f) (!ext_def_freq?f:ext_def_freq)
#else
#define FREQ_DEF(f) f
#endif

#if NEW_IMCFREQ
extern ulong policy_max_imcfreq, policy_min_imcfreq;
extern uint dyn_unc;
extern ulong imc_max_khz,imc_min_khz;
extern ulong def_imc_max_khz, def_imc_min_khz;
extern uint dynais_enabled;
extern uint ear_periodic_mode;
ulong avg_to_khz(ulong freq_khz);
static uint imc_freq_selected = 0;
static uint last_cpu_pstate,last_imc_pstate;
static int min_pstate;

struct imc_data_t{
    double time;
    double GBS;
    double power;
    double l3_mpkc; // l3 misses per 1K cycles
    double l3_rate;
    double Gflops;
};

#define NUM_UNC_FREQ 20
static struct imc_data_t *imc_data;
#define IMC_STEP 100000
#define IMC_RANGE 100000
#define SELECT_CPUFREQ 100
#define SELECT_IMCFREQ 101
static uint min_energy_state = SELECT_CPUFREQ;
static uint imc_dynamic = 0;
static uint min_imc_pstate = 0;
#endif

static ulong req_f;

static uint must_increase_imc(signature_t *sig,double penalty)
{
    double metric,metric_ref;
    double metric_max;
    if (ear_periodic_mode == PERIODIC_MODE_OFF){
        metric_max = imc_data[min_pstate*NUM_UNC_FREQ+0].time + (penalty* imc_data[min_pstate*NUM_UNC_FREQ+0].time);
        metric = sig->time;
        verbose(2,"Using time as reference : Current %.3lf Max %.3lf",metric,metric_max);
        return (metric > metric_max);
    }else{
        metric_max = imc_data[min_pstate*NUM_UNC_FREQ+0].GBS - (penalty* imc_data[min_pstate*NUM_UNC_FREQ+0].GBS);
        metric = sig->GBS;
        verbose(2,"Using GBS as reference : Current %.3lf Max %.3lf",metric,metric_max);
        return (metric < metric_max);
    }
}

state_t policy_init(polctx_t *c)
{
    if (c!=NULL){ 
#if NEW_IMCFREQ
        imc_data = calloc(c->num_pstates*NUM_UNC_FREQ,sizeof(struct imc_data_t));
#endif
        return EAR_SUCCESS;
    } else return EAR_ERROR;
}

state_t policy_loop_init(polctx_t *c,loop_id_t *l)
{
    if (c!=NULL){
        projection_reset(c->num_pstates);
        return EAR_SUCCESS;
    }else{
        return EAR_ERROR;
    }
}

state_t policy_loop_end(polctx_t *c,loop_id_t *l)
{
    if ((c!=NULL) && (c->reset_freq_opt))
    {
        *(c->ear_frequency) = eards_change_freq(FREQ_DEF(c->app->def_freq));
    }
    return EAR_SUCCESS;
}


state_t policy_apply(polctx_t *c,signature_t *sig,node_freqs_t *freqs,int *ready)
{
    ulong *new_freq = freqs->cpu_freq;
    signature_t *my_app = sig;
    int i;
    unsigned int ref;
    double power_proj,time_proj,energy_proj,best_solution,energy_ref;
    double power_ref,time_ref,max_penalty,time_max;
    ulong imc_pstate;

    ulong best_freq,best_pstate,freq_ref,eff_f;
    ulong curr_freq,nominal;
    ulong curr_pstate,def_pstate,def_freq;
    state_t st;
    uint power_status;

    if ((c == NULL) || (c->app == NULL)){
        *ready=EAR_POLICY_CONTINUE;
        return EAR_ERROR;
    }

    if (c->use_turbo) min_pstate=0;
    else min_pstate=frequency_closest_pstate(c->app->max_freq);

    nominal=frequency_pstate_to_freq(min_pstate);

    // Default values

    max_penalty=c->app->settings[0];
    def_freq=FREQ_DEF(c->app->def_freq);
    def_pstate=frequency_closest_pstate(def_freq);

    // This is the frequency at which we were running
#ifdef POWERCAP
    if (c->pc_limit > 0){
        curr_freq=frequency_closest_high_freq(my_app->avg_f,1);
    }else{
        curr_freq=*(c->ear_frequency);
    }
#else
    curr_freq=*(c->ear_frequency);
#endif

    curr_pstate = frequency_closest_pstate(curr_freq);
    eff_f=frequency_closest_high_freq(my_app->avg_f,1);

#if NEW_IMCFREQ
    if (dyn_unc){
        imc_pstate = (def_imc_max_khz - avg_to_khz(my_app->sig_ext->imc_freq))/IMC_STEP;
        verbose(2,"CPU pstate %lu IMC freq %lu IMC pstate %lu",curr_pstate,my_app->sig_ext->imc_freq,imc_pstate);
        imc_data[curr_pstate*NUM_UNC_FREQ+imc_pstate].time = my_app->time;
        imc_data[curr_pstate*NUM_UNC_FREQ+imc_pstate].power = my_app->DC_power;
        imc_data[curr_pstate*NUM_UNC_FREQ+imc_pstate].GBS = my_app->GBS;
        imc_data[curr_pstate*NUM_UNC_FREQ+imc_pstate].l3_mpkc = ((double) my_app->L3_misses * 1000.0) / (double) my_app->cycles;
        imc_data[curr_pstate*NUM_UNC_FREQ+imc_pstate].l3_rate = ((double) my_app->L3_misses) / my_app->time;
        imc_data[curr_pstate*NUM_UNC_FREQ+imc_pstate].Gflops = my_app->Gflops;
        verbose(2,"IMC data: time %.3lf power %2.lf GBS %.2lf MPKC %.2lf L3_MISS_RATE %.2lf",
                imc_data[curr_pstate*NUM_UNC_FREQ+imc_pstate].time,
                imc_data[curr_pstate*NUM_UNC_FREQ+imc_pstate].power,
                imc_data[curr_pstate*NUM_UNC_FREQ+imc_pstate].GBS,
                imc_data[curr_pstate*NUM_UNC_FREQ+imc_pstate].l3_mpkc,
                imc_data[curr_pstate*NUM_UNC_FREQ+imc_pstate].l3_rate);
    }
    if ((min_energy_state == SELECT_CPUFREQ) || (dyn_unc == 0)){
        if (dyn_unc){
            min_energy_state = SELECT_IMCFREQ;
            *ready = EAR_POLICY_TRY_AGAIN;
        }else *ready=EAR_POLICY_READY;
        verbose(2,"SELECT_CPUFREQ state");
#else
        *ready=EAR_POLICY_READY;
#endif
        last_cpu_pstate = curr_pstate;
        last_imc_pstate = imc_pstate;


        // If is not the default P_STATE selected in the environment, a projection
        // is made for the reference P_STATE in case the coefficents were available.
        // 
        if (curr_freq != def_freq) // Use configuration when available
        {
            if (projection_available(curr_pstate,def_pstate)==EAR_SUCCESS)
            {
                st=project_power(my_app,curr_pstate,def_pstate,&power_ref);
                st=project_time(my_app,curr_pstate,def_pstate,&time_ref);
                best_freq=def_freq;
                debug("projecting from %lu to %lu\t time: %.2lf\t power: %.2lf", curr_pstate, def_pstate, time_ref, power_ref);
            }
            else
            {
                time_ref=my_app->time;
                power_ref=my_app->DC_power;
                best_freq=curr_freq;
            }
        }
        // If it is the default P_STATE selected in the environment, then a projection
        // is not needed, so the signature will be enough as a reference. 
        else
        { // we are running at default frequency , signature is our reference
            if (curr_freq == nominal){
                /* And we are the nominal freq */
                time_ref=my_app->time;
                power_ref=my_app->DC_power;
                best_freq=curr_freq;
            }else{
                /* Nominal is the reference , if projections are ready, we project time and power */
                debug("current_freq is not the nominal");

                if (projection_available(curr_pstate,min_pstate) == EAR_SUCCESS){
                    project_power(my_app,curr_pstate,min_pstate,&power_ref);
                    project_time(my_app,curr_pstate,min_pstate,&time_ref);
                    best_freq=nominal;
                    debug("projecting to nominal\t time: %.2lf\t power: %.2lf", time_ref, power_ref);
                }else{
                    time_ref=my_app->time;
                    power_ref=my_app->DC_power;
                    best_freq=curr_freq;
                }
            }

        }
        energy_ref=power_ref*time_ref;
        best_solution=energy_ref;

        // We compute the maximum performance loss
        time_max = time_ref + (time_ref * max_penalty);

        debug("Max_freq set to %lu min_pstate = %d nominal %lu curr_frequency %lu curr_pstate %lu time_max: %.2lf",
                c->app->max_freq,min_pstate,nominal,curr_freq,curr_pstate,time_max);

        debug("Policy Signature (CPI=%lf GBS=%lf Power=%lf Time=%lf TPI=%lf)",
                my_app->CPI,my_app->GBS,my_app->DC_power,my_app->time,my_app->TPI);

        // MIN_ENERGY_TO_SOLUTION ALGORITHM
        // Calcular el min_pstate que este dentro del limite
        for (i = min_pstate; i < c->num_pstates;i++)
        {
            debug("testing projection between P%d and P%d", curr_pstate, i);
            if (projection_available(curr_pstate,i)==EAR_SUCCESS)
            {
                debug("ok");
                st=project_power(my_app,curr_pstate,i,&power_proj);
                st=project_time(my_app,curr_pstate,i,&time_proj);
                projection_set(i,time_proj,power_proj);
                energy_proj=power_proj*time_proj;

                debug("projected from %lu to %d\t time: %.2lf\t power: %.2lf energy: %.2lf",
                        curr_pstate, i, time_proj, power_proj, energy_proj);

                if ((energy_proj < best_solution) && (time_proj < time_max)) {
                    debug("new best solution found");
                    best_freq = frequency_pstate_to_freq(i);
                    best_solution = energy_proj;
                    best_pstate=i;
                }
            }

            policy_max_imcfreq = 2400000;
            policy_min_imcfreq = 2300000;
        }
        *new_freq=best_freq;
#if NEW_IMCFREQ
    }
    else if ((min_energy_state == SELECT_IMCFREQ) && (dyn_unc)){
        verbose(2,"SELECT_IMCFREQ state: nominal %d - last CPU %u,IMC %u -- current CPU %lu, IMC %lu",
                min_pstate,last_cpu_pstate,last_imc_pstate,curr_pstate,imc_pstate);
        *new_freq = frequency_pstate_to_freq(curr_pstate);
        // time_max = imc_data[min_pstate*NUM_UNC_FREQ+last_imc_pstate].time + (max_penalty* imc_data[min_pstate*NUM_UNC_FREQ+last_imc_pstate].time);
        // refactor the above equation
        time_max = imc_data[min_pstate * NUM_UNC_FREQ + last_imc_pstate].time * (1 + max_penalty);
        /* If we are over the limit, we increase the IMC */
        if (my_app->time > time_max){
            verbose(2,"%sWarning, passing the limit: limit %.2lf current %.2lf",COL_RED,time_max,my_app->time);
            policy_max_imcfreq = ear_min(avg_to_khz(my_app->sig_ext->imc_freq) + 100000,2400000);	
            policy_min_imcfreq = policy_max_imcfreq - 100000;
            verbose(2,"Selecting %lu - %lu and policy ready%s",policy_max_imcfreq,policy_min_imcfreq,COL_CLR);
            *ready = EAR_POLICY_READY;
            min_energy_state = SELECT_CPUFREQ;
        }else{
            policy_max_imcfreq = ear_min(avg_to_khz(my_app->sig_ext->imc_freq),2400000);
            policy_max_imcfreq = policy_max_imcfreq - 100000;
            policy_min_imcfreq = policy_max_imcfreq - 100000;
            verbose(2,"%sTrying a lower IMC freq %lu-%lu%s",COL_GRE,policy_max_imcfreq,policy_min_imcfreq,COL_CLR);
            *ready = EAR_POLICY_TRY_AGAIN;
        }
    }
#endif
    verbose(2,"Next min_energy state %u next policy state %u",min_energy_state,*ready);
    return EAR_SUCCESS;
}


state_t policy_ok(polctx_t *c,signature_t *curr_sig,signature_t *last_sig,int *ok)
{
    double energy_last, energy_curr;
    uint power_status,next_status;
    ulong eff_f;

    if ((c==NULL) || (curr_sig==NULL) || (last_sig==NULL)) return EAR_ERROR;

    /* This part is generic */

    if (curr_sig->def_f==last_sig->def_f) *ok=1;
    energy_last = last_sig->time*last_sig->DC_power;
    energy_curr = curr_sig->time * curr_sig->DC_power;

    if ((energy_curr<energy_last)&&(curr_sig->time<(last_sig->time*c->app->settings[0]))) *ok=1;
    else *ok=0;
    return EAR_SUCCESS;
}

state_t  policy_get_default_freq(polctx_t *c, node_freqs_t *freqs)
{
    ulong *freq_set = freqs->cpu_freq;
    if (c!=NULL){
        *freq_set=c->app->max_freq;
    }else EAR_ERROR;
    return EAR_SUCCESS;
}

state_t policy_max_tries(polctx_t *c,int *intents)
{
    *intents = 1;
    return EAR_SUCCESS;
}

