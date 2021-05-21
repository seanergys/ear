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
#include <daemon/local_api/eard_api.h>
#include <library/policies/policy_api.h>
#include <common/math_operations.h>
#include <library/common/externs.h>
#include <common/system/time.h>
#include <daemon/powercap/powercap_status.h>
#include <library/policies/policy_state.h>
#include <library/policies/gpu_support.h>
#include <library/policies/imc_policy_support.h>
#include <library/policies/mpi_stats_support.h>
#include <library/policies/compute_cpu_freq.h>
#include <library/common/verbose_lib.h>
#include <library/policies/cpu_support.h>
#include <library/metrics/gpu.h>
#include <library/api/clasify.h>


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
extern ulong imc_max_khz, imc_min_khz;
extern ulong def_imc_max_khz, def_imc_min_khz;
extern int my_globalrank;
extern uint imc_set_by_hw;
extern double imc_extra_th;
ulong avg_to_khz(ulong freq_khz);

#define IMC_STEP        100000
#define IMC_RANGE       100000
#define SELECT_CPUFREQ  100
#define SELECT_IMCFREQ  101
#define COMP_IMCREF     102

#define LB_TH       15.0
#define EXTRA_TH    0.10

static imc_data_t *imc_data;
static uint min_time_state = SELECT_CPUFREQ;
// static uint last_cpu_pstate,last_imc_pstate, ref_imc_pstate;
static uint ref_imc_pstate;

static int nump;
static uint enable_load_balance = 1;
static uint num_processes;
#endif

// static timestamp pol_time_init; not used
static ulong req_f;
static signature_t *my_app;
static mpi_information_t *my_node_mpi_calls, *last_my_node_mpi_calls, *last_node_mpi_calls;
static mpi_calls_types_t *my_node_mpi_types_calls, *last_node_mpi_types_calls;
static polsym_t gpus;
static ulong *last_cpufreq_sel;
static const ulong **gpuf_pol_list;
static const uint *gpuf_pol_list_items;
static int min_pstate;
static double *percs_mpi, *last_percs_mpi;
static uint first_time = 1;
static ulong nominal_node;
// static double *sorted_percs_mpi;
static uint use_turbo_for_cp = 0;
static uint *critical_path;
static int master_sig_without_sig = 0;
static signature_t cpufreq_signature;

/*  wrap function which calculates intructions, flops and cycles. */
state_t compute_total_node_metrics();
/*  Assigns 'freq_val' to 'len' positions of array 'freqs'. */
state_t set_all_cores(ulong *freqs, int len, ulong freq_val);
/*  Copy current information to use it in the next iteration as last information. */
state_t copy_cpufreq_sel(ulong *to, ulong *from, size_t size);

state_t policy_init(polctx_t *c)
{
    // char *cuse_imc_ref; Not used
    char *cenable_load_balance = getenv(SCHED_ENABLE_LOAD_BALANCE);
    char *cuse_turbo_for_cp = getenv(SCHED_USE_TURBO_FOR_CP);
    int i;

    if (c!=NULL){ 
#if NEW_IMCFREQ
        imc_data = calloc(c->num_pstates*NUM_UNC_FREQ,sizeof(imc_data_t));
#endif
        nominal_node = frequency_get_nominal_freq();
        verbose_master(2, "Nominal frequency is %lu", nominal_node);
        if (is_mpi_enabled()) {
            mpi_app_init(c);
        }
        /*  Config from env variables */
        if (cenable_load_balance != NULL) enable_load_balance = atoi(cenable_load_balance);
        verbose_master(2,"Load balance enabled ? %u", enable_load_balance);
        if(cuse_turbo_for_cp != NULL) use_turbo_for_cp = atoi(cuse_turbo_for_cp);
        verbose_master(2, "Using turbo for CP when load balance enables ? %u", use_turbo_for_cp);

        my_app = calloc(1,sizeof(signature_t));
        my_app->sig_ext = calloc(1, sizeof(io_data_t));

        num_processes = lib_shared_region->num_processes;

        my_node_mpi_calls = calloc(num_processes, sizeof(mpi_information_t));
        last_node_mpi_calls = calloc(num_processes, sizeof(mpi_information_t));
        last_my_node_mpi_calls = calloc(num_processes, sizeof(mpi_information_t));
        percs_mpi = calloc(num_processes, sizeof(double));
        last_percs_mpi = calloc(num_processes, sizeof(double));
        // sorted_percs_mpi = calloc(num_processes, sizeof(double));

        my_node_mpi_types_calls = calloc(num_processes, sizeof(mpi_calls_types_t));
        last_node_mpi_types_calls = calloc(num_processes, sizeof(mpi_calls_types_t));

        last_cpufreq_sel = calloc(MAX_CPUS_SUPPORTED, sizeof(ulong));

        critical_path = calloc(num_processes, sizeof(uint));

        for (i = 0; i < num_processes; i++) {
            last_cpufreq_sel[i] = *(c->ear_frequency);
        }
#if USE_GPUS
        if (c->num_gpus){
            if (policy_gpu_load(c->app,&gpus) != EAR_SUCCESS){
                verbose_master(2,"Error loading GPU policy");
            }
            if (gpus.init != NULL) gpus.init(c);
            gpu_lib_freq_list(&gpuf_pol_list, &gpuf_pol_list_items);
        }
#endif
        return EAR_SUCCESS;
    }else return EAR_ERROR;
}

state_t policy_end(polctx_t *c)
{
    if (c != NULL) return  mpi_app_end(c);
    else return EAR_ERROR;
}

state_t policy_loop_init(polctx_t *c,loop_id_t *loop_id)
{
    if (c != NULL){ 
        projection_reset(c->num_pstates);
        return EAR_SUCCESS;
    }else{
        return EAR_ERROR;
    }
}

state_t policy_loop_end(polctx_t *c,loop_id_t *loop_id)
{
    if ((c!=NULL) && (c->reset_freq_opt))
    {
        // Use configuration when available
        *(c->ear_frequency) = eards_change_freq(FREQ_DEF(c->app->def_freq));
    }
    return EAR_SUCCESS;
}

// This is the main function in this file, it implements power policy
state_t policy_apply(polctx_t *c,signature_t *sig,node_freqs_t *freqs,int *ready)
{
    ulong *new_freq=freqs->cpu_freq;
    int i;
    uint ref;
    double power_proj, time_proj;
    double power_ref, time_ref, time_current;
    double freq_gain, perf_gain, min_eff_gain;
    ulong best_freq, best_pstate, freq_ref;
    ulong curr_freq, nominal;
    ulong curr_pstate, def_pstate, def_freq, imc_pstate;
    state_t st;
    double sig_vpi, node_sig_vpi, max_vpi;
    int gready;
    uint sid = 0;
    int max_mpi, min_mpi;
    uint signature_is_unbalance = 0;
    uint local_process_with_high_vpi;
    double local_eff_gain, base_eff_gain;
    int freq_per_core = 0;
    double mpi_stats_perc_mpi_sd, mpi_stats_perc_mpi_mean;
    uint cbound, mbound;

#if NEW_IMCFREQ
    ulong curr_imc_freq;
#endif

    if (c == NULL || c->app==NULL) {
        *ready = EAR_POLICY_CONTINUE;
        return EAR_ERROR;
    }
    verbose_master(2,"%s---MIN TIME---%s",COL_GRE,COL_CLR);

    if (gpus.node_policy_apply != NULL){
        /* Basic support for GPUS */
        gpus.node_policy_apply(c,sig,freqs,&gready);
    }

    if (is_mpi_enabled()){
        read_diff_node_mpi_type_info(lib_shared_region, sig_shared_region, my_node_mpi_types_calls, last_node_mpi_types_calls);
        verbose_mpi_types(2, my_node_mpi_types_calls);
        read_diff_node_mpi_data(lib_shared_region, sig_shared_region, my_node_mpi_calls, last_node_mpi_calls);
    }

    if (!are_signatures_ready(lib_shared_region,sig_shared_region)){
        copy_cpufreq_sel(new_freq, last_cpufreq_sel, sizeof(ulong) * MAX_CPUS_SUPPORTED);
        *ready = EAR_POLICY_TRY_AGAIN;
        verbose_master(2, "%sNode signature not ready%s, selecting %lu CPU f", COL_RED, COL_CLR, *new_freq);
        master_sig_without_sig++;
        if (master_sig_without_sig > 3) print_sig_readiness(lib_shared_region, sig_shared_region);
        return EAR_SUCCESS;
    }
    master_sig_without_sig = 0;
    verbose_master(2, "%s-----MR[%d] Node signatures ready-----%s", COL_GRE, my_globalrank, COL_CLR);
    for (int p = 0; p < num_processes; p++){
        verbosen_master(3, "Process[%d] = (CPI %lf FLOPS %lf) ",
                p, sig_shared_region[p].sig.CPI, sig_shared_region[p].sig.Gflops);
    }
    verbose_master(3, "");
    min_eff_gain=c->app->settings[0];
    base_eff_gain = min_eff_gain;

    if (is_mpi_enabled() && min_time_state == SELECT_CPUFREQ && enable_load_balance) {
        /*  Even though it's compute bound, we check if we must redistribute power */
        evaluate_cp(num_processes, my_node_mpi_calls, &max_mpi, &min_mpi);
        // opportunity to check similarity to avoid this code
        memset(critical_path, 0, sizeof(uint)*num_processes);

        mpi_stats_get_only_percs_mpi(my_node_mpi_calls, num_processes, percs_mpi);
        mpi_stats_evaluate_mean_sd(num_processes, percs_mpi, &mpi_stats_perc_mpi_mean, &mpi_stats_perc_mpi_sd);
        signature_is_unbalance = (mpi_stats_perc_mpi_sd > LB_TH);
        uint unbalance = enable_load_balance && signature_is_unbalance;
        verbose_master(2,"%sNode load unbalance%s: LH_TH %lf max perc mpi %lf min perc mpi %lf mean %lf standard deviation %f unbalance ? %u",
                COL_MGT, COL_CLR, LB_TH, percs_mpi[max_mpi],
                percs_mpi[min_mpi], mpi_stats_perc_mpi_mean, mpi_stats_perc_mpi_sd, unbalance);

        if (!first_time){
            mpi_stats_get_only_percs_mpi(last_my_node_mpi_calls, num_processes, last_percs_mpi);
            double similarity;
            mpi_stats_evaluate_similarity(percs_mpi, last_percs_mpi, num_processes, &similarity);
            verbose(2, "Similarity between last and current: %lf", similarity);
        }
        else{
            first_time = 0;
        }
        memcpy(last_my_node_mpi_calls, my_node_mpi_calls, num_processes);

        if (unbalance){
            verbose_master(2, "%sPer-core CPU freq. selection%s", COL_RED, COL_CLR);
            freq_per_core = 1;
            nump = num_processes;
            // opportunity to check similarity to avoid below code
            for (i=0; i < nump; i++){
                /*  Decides whether process i is part of the critical path */
                if (abs(percs_mpi[i] - percs_mpi[min_mpi]) < abs(percs_mpi[i] - mpi_stats_perc_mpi_mean)){
                    verbose_master(2, "Process %d selected as part of the critical path", i);
                    critical_path[i] = 1;
                }
            }
        }else nump = 1;
    }
    else nump = 1;

    if (c->use_turbo) min_pstate = 0;
    else min_pstate = frequency_closest_pstate(c->app->max_freq);

    nominal = frequency_pstate_to_freq(min_pstate);

    // Default values

    def_freq=FREQ_DEF(c->app->def_freq);
    def_pstate=frequency_closest_pstate(def_freq);

    // This is the frequency at which we were running
    signature_copy(my_app, sig);
#ifdef POWERCAP
    if (c->pc_limit > 0){
        curr_freq=frequency_closest_high_freq(my_app->avg_f,1);
    }else{
        curr_freq=*(c->ear_frequency);
    }
#else
    curr_freq=*(c->ear_frequency);
#endif

    // TODO below code does not exist in min_energy
#if POWERCAP
    if (is_powercap_set(&c->app->pc_opt)){ 
        verbose(3,"Powercap is set to %uWatts",get_powercapopt_value(&c->app->pc_opt));
    }
#endif
    curr_pstate = frequency_closest_pstate(curr_freq);

#if NEW_IMCFREQ
    if (dyn_unc){
        curr_imc_freq = avg_to_khz(my_app->sig_ext->imc_freq);
        imc_pstate = (def_imc_max_khz - curr_imc_freq) / IMC_STEP;
        verbose_master(2, "CPU pstate %lu CPU freq %lu IMC freq %lu IMC pstate %lu", curr_pstate, curr_freq, my_app->sig_ext->imc_freq, imc_pstate);
        copy_imc_data_from_signature(imc_data, curr_pstate, imc_pstate, my_app);
    }
    if ((min_time_state == SELECT_CPUFREQ) || (dyn_unc == 0)){
        /*  SELECT CPUFREQ */
        if (dyn_unc){
            *ready = EAR_POLICY_TRY_AGAIN;
        }else{
            *ready = EAR_POLICY_READY;
        }
        verbose_master(2, "%sSELECT_CPUFREQ%s state dyn_unc %u", COL_YLW, COL_CLR, dyn_unc);
#else
        *ready = EAR_POLICY_READY;
#endif

        // last_cpu_pstate = curr_pstate;
        // last_imc_pstate = imc_pstate;
        is_cpu_bound(my_app, &cbound);
        is_mem_bound(my_app, &mbound);
        verbose_master(2, "App CPU bound %u MEM bound %u", cbound, mbound);

        // If is not the default P_STATE selected in the environment, a projection
        // is made for the reference P_STATE in case the projections were available.
        i = 0;
        while ((i < num_processes) && (last_cpufreq_sel[i] == def_freq)) i++;
        // TODO: min energy contains a condition and code wich has no sense
        if (i < num_processes) {
            verbose_master(3, "Not the default P_STATE selected in the environemnt!\nsetting default cpu freq for %d processes..", num_processes);
            *ready = EAR_POLICY_TRY_AGAIN;
            /*  TODO: Here is an opportunity to check similarity (?) */
            set_all_cores(new_freq, num_processes, def_freq);
#if NEW_IMCFREQ
            if (dyn_unc) {
                verbose_master(2, "setting default imc range freq...");
                freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = imc_max_khz;
                freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = imc_min_khz;
            }
#endif
            copy_cpufreq_sel(last_cpufreq_sel, new_freq, sizeof(ulong) * MAX_CPUS_SUPPORTED);
            verbose_master(2, "Setting default conf for MT");
            return EAR_SUCCESS;
        }
        for (i = 0; i < nump; i++) {
            verbose_master(2, "Processing proc %d freq per core %d", i, freq_per_core);
            if (!freq_per_core) {
                compute_total_node_metrics();
                compute_vpi(&sig_vpi, sig);
                compute_vpi(&node_sig_vpi,my_app);
                local_process_with_high_vpi = compute_max_vpi(lib_shared_region, sig_shared_region, &max_vpi);
                my_app->CPI = (double)my_app->cycles/(double)my_app->instructions;
                verbose_master(2, "AVGNodeSig MR[%d] SIGNATURE CPI %.3lf VPI %.3lf/Node CPI %.3lf VPI %3lf/MAX VPI %.3lf", 
                        my_globalrank, sig->CPI, sig_vpi, my_app->CPI, node_sig_vpi, max_vpi);
                /*  With this two lines, we force the VPI of the signature to the most loaded in the node */
                my_app->instructions = sig_shared_region[local_process_with_high_vpi].sig.instructions;
                memcpy(my_app->FLOPS, sig_shared_region[local_process_with_high_vpi].sig.FLOPS, sizeof(ull) * FLOPS_EVENTS);
                local_eff_gain = base_eff_gain;
            }else{
                ulong eff_p_cpuf, process_avgcpuf;
                process_avg_cpu_freq(&sig_shared_region[i].cpu_mask, cpufreq_data_per_core, &process_avgcpuf);
                from_minis_to_sig(my_app, &sig_shared_region[i].sig);
                verbose_master(2, "Per-processing MR[%d][%d] SIGNATURE CPI %.3lf AVGCPUF %f (%lf, %lf)", my_globalrank, i, 
                        my_app->CPI, (float)process_avgcpuf/1000000.0, percs_mpi[i], percs_mpi[min_mpi]);
                if (critical_path[i]){
                    if (!mbound){
                        eff_p_cpuf = frequency_closest_high_freq(process_avgcpuf, 0);
                        // Always will be >= def_freq
                        if (use_turbo_for_cp && process_avgcpuf >= def_freq){
                            new_freq[i] = frequency_pstate_to_freq(0);
                        }else{
                            new_freq[i] = ear_min(eff_p_cpuf, frequency_pstate_to_freq(1));
                        }
                        verbose_master(2, "CPU freq to CP process %d is %lu (CPU_BOUND)", i, new_freq[i]);
                        continue;
                    }else{
                        local_eff_gain = base_eff_gain + (1 - base_eff_gain)/2;
                    }
                }
                else {
                    local_eff_gain = 
                        min_eff_gain - EXTRA_TH * ear_math_exp(5 * ((percs_mpi[i] - percs_mpi[min_mpi]) / 100.0));
                }
                verbose_master(2, "Efficiency %f", local_eff_gain);
            }
            compute_reference(c, my_app, &curr_freq, &def_freq, &best_freq, &time_ref, &power_ref);
            verbose_master(2, "Time ref %lf Power ref %lf Freq ref %lu", time_ref, power_ref, best_freq);
            best_pstate = frequency_closest_pstate(best_freq);
            compute_cpu_freq_min_time(my_app, min_pstate, time_ref, local_eff_gain, curr_pstate, 
                    best_pstate, best_freq, def_freq, &new_freq[i]);
            verbose_master(2, "CPU freq for process %d is %lu", i, new_freq[i]);
            /* 
             * #if LET_TURBO
             *  if (cpu_supp_try_boost_cpu_freq(freq_per_core, i, min_mpi, percs_mpi, mpi_stats_perc_mpi_mean, new_freq, c->use_turbo)){
             *      verbose_master(2, "Nominal freq. boosted for process %d: %lu", i, new_freq[i]);
             *  }
             * #endif
             */
        }
        if (!freq_per_core) {
            set_all_cores(new_freq, num_processes, new_freq[0]);
        }
        copy_cpufreq_sel(last_cpufreq_sel, new_freq, sizeof(ulong) * num_processes);
        /*  We will use this signature to detect changes in the sig compared when the one used to compute the CPU freq */
        signature_copy(&cpufreq_signature, sig);
#if NEW_IMCFREQ
        if (dyn_unc){
            if (!imc_set_by_hw){
                /*  Fix the imc to the first selected by hardware */
                freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = avg_to_khz(my_app->sig_ext->imc_freq);
                freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = freqs->imc_freq[sid*IMC_VAL+IMC_MAX] - IMC_RANGE;
            }
            else {
                freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = imc_max_khz;
                freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = imc_min_khz;
            }
            i = 0;
            while ((i < num_processes) && (last_cpufreq_sel[i] == def_freq)) i++;
            if (i < num_processes) {
                min_time_state = COMP_IMCREF;
            } else {
                min_time_state = SELECT_IMCFREQ;
                ref_imc_pstate = imc_pstate;
                verbose_master(2, "new_freq == curr_freq. ref_imc_pstate set to %u", ref_imc_pstate);
            }
        } else {
            min_time_state = SELECT_CPUFREQ;
            *ready = EAR_POLICY_READY;
        }
#endif
#if NEW_IMCFREQ
    }else if (min_time_state == COMP_IMCREF){
        // curr_imc_freq = avg_to_khz(my_app->sig_ext->imc_freq); This variable is already initialized
        // ulong curr_imc_pstate = (def_imc_max_khz - curr_imc_freq)/IMC_STEP; this variable is already initialized
        copy_cpufreq_sel(new_freq, last_cpufreq_sel, sizeof(ulong) * MAX_CPUS_SUPPORTED);
        ref_imc_pstate = imc_pstate;
        // freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = curr_imc_freq;
        verbose_master(2,"%sCOMP_IMCREF%s | cpufreq %lu | imc %lu-%lu | imcfreq %lu | ref imc_pstate %u",
                COL_YLW, COL_CLR, *new_freq, freqs->imc_freq[sid*IMC_VAL+IMC_MAX], freqs->imc_freq[sid*IMC_VAL+IMC_MIN], curr_imc_freq, ref_imc_pstate);
        min_time_state = SELECT_IMCFREQ;
        *ready = EAR_POLICY_TRY_AGAIN;
    }
    else if ((min_time_state == SELECT_IMCFREQ) && (dyn_unc)){
        verbose_master(2,"%sSELECT_IMCFREQ%s state: nominal %d -- current CPU %lu, IMC %lu -- ref IMC pstate %d",
                COL_YLW, COL_CLR, min_pstate, curr_pstate, imc_pstate, ref_imc_pstate);
        copy_cpufreq_sel(new_freq, last_cpufreq_sel, sizeof(ulong) * MAX_CPUS_SUPPORTED);
        /* 
            uint increase_imc = must_increase_imc(imc_data, curr_pstate, imc_pstate, curr_pstate,
            ref_imc_pstate, my_app, imc_extra_th);
        // If we are over the limit, we increase the IMC 
        if (increase_imc){
        verbose(2,"%sWarning, passing the limit", COL_RED);
        freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = ear_min(freqs->imc_freq[sid*IMC_VAL+IMC_MAX] + IMC_STEP, imc_max_khz);	
        freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = imc_min_khz;
        verbose(2,"Selecting %lu - %lu and policy ready%s",
        freqs->imc_freq[sid*IMC_VAL+IMC_MAX], freqs->imc_freq[sid*IMC_VAL+IMC_MIN], COL_CLR);
         *ready = EAR_POLICY_READY;
         min_time_state = SELECT_CPUFREQ;
         }else{
         freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = ear_max(freqs->imc_freq[sid*IMC_VAL+IMC_MAX] - IMC_STEP,
         imc_min_khz-IMC_RANGE);
         freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = imc_min_khz;
         verbose(2, "%sTrying a lower IMC freq %lu-%lu%s", COL_GRE, freqs->imc_freq[sid*IMC_VAL+IMC_MAX],
         freqs->imc_freq[sid*IMC_VAL+IMC_MIN], COL_CLR);
         if (freqs->imc_freq[sid*IMC_VAL+IMC_MAX] > (imc_min_khz + IMC_RANGE)) *ready = EAR_POLICY_TRY_AGAIN;
         else{ 
         *ready = EAR_POLICY_READY;
         min_time_state = SELECT_CPUFREQ;
         }
         }
         */
        uint decrease_imc = 0;
        if (frequency_pstate_to_freq(ref_imc_pstate) != def_imc_max_khz){
            verbose(2, "ref IMC pstate (%d) not at maximum. Checking for increase opportunity.")
            decrease_imc = must_decrease_imc(imc_data, curr_pstate, imc_pstate, curr_pstate, ref_imc_pstate, my_app, imc_extra_th);
        }
        
        /*  If we are over the limit, we increase the IMC */
        if (decrease_imc) {
            verbose_master(2,"%sWarning, passing the limit", COL_RED);
            freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = ear_max(freqs->imc_freq[sid*IMC_VAL+IMC_MAX] - IMC_STEP,
                    imc_min_khz - IMC_RANGE  );
            freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = imc_min_khz;
            verbose_master(2,"Selecting %lu - %lu and policy ready%s",
                    freqs->imc_freq[sid*IMC_VAL+IMC_MAX], freqs->imc_freq[sid*IMC_VAL+IMC_MIN], COL_CLR);
            *ready = EAR_POLICY_READY;
            min_time_state = SELECT_CPUFREQ;
        }else {
            freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = ear_min(freqs->imc_freq[sid*IMC_VAL+IMC_MAX] + IMC_STEP, imc_max_khz);	
            freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = imc_min_khz;
            verbose_master(2, "%sTrying an upper IMC freq %lu-%lu%s", COL_GRE, freqs->imc_freq[sid*IMC_VAL+IMC_MAX],
                    freqs->imc_freq[sid*IMC_VAL+IMC_MIN], COL_CLR);
            if (freqs->imc_freq[sid*IMC_VAL+IMC_MAX] < (imc_max_khz + IMC_RANGE)) *ready = EAR_POLICY_TRY_AGAIN;
            else{ 
                *ready = EAR_POLICY_READY;
                min_time_state = SELECT_CPUFREQ;
            }
        }
    }
#endif
    verbose_master(2,"Next min_time state %u next policy state %u",min_time_state,*ready);
    return EAR_SUCCESS;
}

state_t policy_ok(polctx_t *c,signature_t *curr_sig,signature_t *last_sig,int *ok)
{
    state_t st=EAR_SUCCESS;
    ulong eff_f;
    uint power_status;
    uint next_status;

    if ((c==NULL) || (curr_sig==NULL) || (last_sig==NULL)) return EAR_ERROR;

    if (curr_sig->def_f==last_sig->def_f) *ok=1;

    // Check that efficiency is enough
    if (curr_sig->time < last_sig->time) *ok=1;

    // If time is similar and it was same freq it is ok	
    if (equal_with_th(curr_sig->time , last_sig->time, 0.1) && (curr_sig->def_f == last_sig->def_f)) *ok=1;

    // If we run at a higher freq but we are slower, we are not ok	
    if ((curr_sig->time > last_sig->time) && (curr_sig->def_f > last_sig->def_f)) *ok=0;

    // else *ok=0;

    return st;

}

state_t policy_get_default_freq(polctx_t *c, ulong *f)
{
    if ((c!=NULL) && (c->app!=NULL)){
        // Just in case the bestPstate was the frequency at which the application was running
        *f=FREQ_DEF(c->app->def_freq);
    }
    return EAR_SUCCESS;
}

state_t policy_max_tries(polctx_t *c,int *intents)
{
    *intents=2;
    return EAR_SUCCESS;
}

state_t policy_mpi_init(polctx_t *c,mpi_call call_type)
{
    if (c != NULL) return mpi_call_init(c,call_type);
    else return EAR_ERROR;
}
state_t policy_mpi_end(polctx_t *c,mpi_call call_type)
{
    if (c != NULL) return mpi_call_end(c,call_type);
    else return EAR_ERROR;
}

state_t policy_domain(polctx_t *c,node_freq_domain_t *domain)
{
    domain->cpu = POL_GRAIN_CORE;
    if (dyn_unc) 	domain->mem = POL_GRAIN_NODE;
    else 			domain->mem = POL_NOT_SUPPORTED;
#if USE_GPUS
    if (c->num_gpus) domain->gpu = POL_GRAIN_CORE;
    else domain->gpu = POL_NOT_SUPPORTED;
#else
    domain->gpu = POL_NOT_SUPPORTED;
#endif
    domain->gpumem = POL_NOT_SUPPORTED;
    return EAR_SUCCESS;
}

state_t policy_io_settings(polctx_t *c,signature_t *my_sig,node_freqs_t *freqs)
{
    int i;
    uint sock_id;
    if (c == NULL) return EAR_ERROR;
    for (i=0;i<num_processes;i++) freqs->cpu_freq[i] = frequency_pstate_to_freq(c->num_pstates - 1);
    copy_cpufreq_sel(last_cpufreq_sel,freqs->cpu_freq,sizeof(ulong)*MAX_CPUS_SUPPORTED);
    if (dyn_unc){
        for (sock_id=0;sock_id<arch_desc.top.socket_count;sock_id++){
            freqs->imc_freq[sock_id*IMC_VAL+IMC_MAX] = imc_min_khz;
            freqs->imc_freq[sock_id*IMC_VAL+IMC_MIN] = imc_min_khz;
        }
    }
#if USE_GPUS
    for (uint gpuid=0;gpuid<c->num_gpus;gpuid++){
        freqs->gpu_freq[gpuid] = gpuf_pol_list[gpuid][gpuf_pol_list_items[gpuid]-1];
    }
#endif
    /* GPU mem is pending */

    return EAR_SUCCESS;
}
state_t policy_busy_wait_settings(polctx_t *c,signature_t *my_sig,node_freqs_t *freqs)
{
    int i;
    uint sock_id;
    if (c == NULL) return EAR_ERROR;
    for (i=0;i<num_processes;i++) freqs->cpu_freq[i] = frequency_pstate_to_freq(c->num_pstates - 1);
    if (dyn_unc){
        copy_cpufreq_sel(last_cpufreq_sel, freqs->cpu_freq, sizeof(ulong)*MAX_CPUS_SUPPORTED);
        for (uint sock_id=0;sock_id<arch_desc.top.socket_count;sock_id++){
            freqs->imc_freq[sock_id*IMC_VAL+IMC_MAX] = imc_min_khz;
            freqs->imc_freq[sock_id*IMC_VAL+IMC_MIN] = imc_min_khz;
        }
    }
#if USE_GPUS
    for (uint gpuid=0;gpuid<c->num_gpus;gpuid++){
        freqs->gpu_freq[gpuid] = gpuf_pol_list[gpuid][0];
    }
#endif
    /* GPU mem is pending */

    return EAR_SUCCESS;
}
state_t policy_restore_settings(polctx_t *c,signature_t *my_sig,node_freqs_t *freqs)
{
    int i;
    uint sock_id;
    if (c == NULL) return EAR_ERROR;
    for (i=0;i<num_processes;i++) freqs->cpu_freq[i] = c->app->max_freq;
    copy_cpufreq_sel(last_cpufreq_sel, freqs->cpu_freq, sizeof(ulong)*MAX_CPUS_SUPPORTED);
    if (dyn_unc){
        for (uint sock_id=0;sock_id<arch_desc.top.socket_count;sock_id++){
            freqs->imc_freq[sock_id*IMC_VAL+IMC_MAX] = imc_max_khz;
            freqs->imc_freq[sock_id*IMC_VAL+IMC_MIN] = imc_min_khz;
        }
    }
#if USE_GPUS
    for (uint gpuid=0;gpuid<c->num_gpus;gpuid++){
        freqs->gpu_freq[gpuid] = gpuf_pol_list[gpuid][0];
    }
#endif
    /* GPU mem is pending */

    return EAR_SUCCESS;
}

state_t compute_total_node_metrics(){
    compute_total_node_instructions(lib_shared_region,sig_shared_region,&my_app->instructions);	
    compute_total_node_flops(lib_shared_region,sig_shared_region,my_app->FLOPS);
    compute_total_node_cycles(lib_shared_region,sig_shared_region,&my_app->cycles);
    return EAR_SUCCESS;
}

state_t set_all_cores(ulong *freqs, int len, ulong freq_val) {
    verbose_master(3, "Setting %lu freq. value to %d cores...", freq_val, len);
    for (int i = 0; i < len; i++) {
        freqs[i] = freq_val;
    }
    return EAR_SUCCESS;
}

state_t copy_cpufreq_sel(ulong *to, ulong *from, size_t size) {
    memcpy(to, from, size);
    return EAR_SUCCESS;
}

