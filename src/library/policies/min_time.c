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
#include <common/math_operations.h>
#include <library/common/externs.h>
#include <library/common/verbose_lib.h>
#include <library/api/clasify.h>
#include <library/metrics/metrics.h>
#include <management/cpufreq/frequency.h>
#include <management/imcfreq/imcfreq.h>
#include <common/types/projection.h>
#include <daemon/local_api/eard_api.h>
#include <daemon/powercap/powercap_status.h>
#include <library/policies/policy_api.h>
#include <library/policies/policy_state.h>
#include <library/policies/imc_policy_support.h>
#include <library/policies/gpu_support.h>
#include <library/policies/cpu_support.h>
#include <library/policies/mpi_stats_support.h>
#include <library/metrics/gpu.h>

#ifdef EARL_RESEARCH
extern unsigned long ext_def_freq;
#define FREQ_DEF(f) (!ext_def_freq?f:ext_def_freq)
#else
#define FREQ_DEF(f) f
#endif

/*  IMC management */
extern uint dyn_unc;
static uint imc_set_by_hw = 0;
extern double imc_extra_th;
extern uint *imc_max_pstate,*imc_min_pstate;
extern uint imc_num_pstates, imc_devices;

static int min_pstate;
static int ref_imc_pstate = -1;

static imc_data_t *imc_data;

static uint first_imc_try = 1;

/*  Utility */
ulong avg_to_khz(ulong freq_khz);

/* Frequency management */
static uint last_cpu_pstate;
static node_freqs_t min_time_def_freqs, last_nodefreq_sel;

static uint num_processes;
static ulong nominal_node;

/* Load balance management */
extern int my_globalrank;
static int nump;
static int freq_per_core = 0;
static uint signature_is_unbalance = 0;

static mpi_information_t *my_node_mpi_calls, *last_my_node_mpi_calls, *last_node_mpi_calls;
static mpi_calls_types_t *my_node_mpi_types_calls, *last_node_mpi_types_calls;
static double *percs_mpi, *last_percs_mpi;
static int max_mpi, min_mpi;
static double last_mpi_stats_perc_mpi_sd, last_mpi_stats_perc_mpi_mean,
              last_mpi_stats_perc_mpi_mag, last_mpi_stats_perc_mpi_median;

static uint *critical_path, *last_critical_path;

/*  Config */
extern uint enable_load_balance;
extern uint use_turbo_for_cp;
static uint network_use_imc = 0;
extern uint try_turbo_enabled;

#define SELECT_CPUFREQ      100
#define SELECT_IMCFREQ      101
#define COMP_IMCREF         102
#define TRY_TURBO_CPUFREQ   103

/* Policy/Other */
static uint min_time_state = SELECT_CPUFREQ;
static signature_t *my_app, cpufreq_signature;
extern uint cpu_ready, gpu_ready, gidle;

static uint first_time = 1;

static uint block_type;

static polsym_t gpus;
static const ulong **gpuf_pol_list;
static const uint *gpuf_pol_list_items;

static state_t int_policy_ok(signature_t *curr_sig,signature_t *last_sig,int *ok);

/*  wrap function which calculates intructions, flops and cycles. */
state_t compute_total_node_metrics();

state_t policy_init(polctx_t *c)
{
    char *cnetwork_use_imc = getenv(SCHED_NETWORK_USE_IMC);
    char *cimc_set_by_hw = getenv(SCHED_LET_HW_CTRL_IMC);
    // TODO: min energy uses more environment variables:
    //  - cuse_imc_ref: -
    //  - cmin_cpufreq: Avoid lowering cpu freq below some value

    int i, sid = 0;

    if (c!=NULL){
        imc_data = calloc(c->num_pstates*NUM_UNC_FREQ,sizeof(imc_data_t));
        nominal_node = frequency_get_nominal_freq();
        verbose_master(2, "Nominal frequency is %lu", nominal_node);
        if (is_mpi_enabled()) {
            mpi_app_init(c);
            is_blocking_busy_waiting(&block_type);
            verbose_master(1,"Busy_waiting MPI set to %u",block_type == BUSY_WAITING_BLOCK);
        }
        /*  Config from env variables */
        if(cnetwork_use_imc != NULL) network_use_imc = atoi(cnetwork_use_imc);
        verbose_master(2, "Network uses IMC set to %u", network_use_imc);

        if (cimc_set_by_hw != NULL) imc_set_by_hw = atoi(cimc_set_by_hw);
        verbose_master(2,"IMC set by HW set to %u",imc_set_by_hw);

        my_app = calloc(1,sizeof(signature_t));
        my_app->sig_ext = calloc(1, sizeof(io_data_t));

        num_processes = lib_shared_region->num_processes;

        my_node_mpi_calls = calloc(num_processes, sizeof(mpi_information_t));
        last_node_mpi_calls = calloc(num_processes, sizeof(mpi_information_t));
        last_my_node_mpi_calls = calloc(num_processes, sizeof(mpi_information_t));
        percs_mpi = calloc(num_processes, sizeof(double));
        last_percs_mpi = calloc(num_processes, sizeof(double));

        my_node_mpi_types_calls = calloc(num_processes, sizeof(mpi_calls_types_t));
        last_node_mpi_types_calls = calloc(num_processes, sizeof(mpi_calls_types_t));

        critical_path = calloc(num_processes, sizeof(uint));
        last_critical_path = calloc(num_processes, sizeof(uint));

        node_freqs_alloc(&min_time_def_freqs);
        node_freqs_alloc(&last_nodefreq_sel);

        last_mpi_stats_perc_mpi_sd = last_mpi_stats_perc_mpi_mean =
            last_mpi_stats_perc_mpi_mag = last_mpi_stats_perc_mpi_median = 0;

        /* Configuring default freqs */
        for (i = 0; i < num_processes; i++) {
            min_time_def_freqs.cpu_freq[i] = *(c->ear_frequency);
        }

        for (sid=0; sid < imc_devices; sid++){
            min_time_def_freqs.imc_freq[sid*IMC_VAL+IMC_MAX] = imc_min_pstate[sid];
            min_time_def_freqs.imc_freq[sid*IMC_VAL+IMC_MIN] = imc_max_pstate[sid];
        }

#if USE_GPUS
        if (c->num_gpus){
            if (policy_gpu_load(c->app,&gpus) != EAR_SUCCESS){
                verbose_master(2,"Error loading GPU policy");
            }
            verbose_master(2, "Initializing GPU policy part");
            if (gpus.init != NULL) gpus.init(c);
            if (gpu_lib_freq_list(&gpuf_pol_list, &gpuf_pol_list_items) != EAR_SUCCESS){
                verbose_master(2, "Error asking for gpu freq list");
            }
            for (i = 0; i<c->num_gpus; i++){
                min_time_def_freqs.gpu_freq[i] = gpuf_pol_list[i][0];
            }
        }
        verbose_node_freqs(2, &min_time_def_freqs);
#endif
        node_freqs_copy(&last_nodefreq_sel, &min_time_def_freqs);
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
        *(c->ear_frequency) = eards_change_freq(FREQ_DEF(c->app->def_freq));
    }
    return EAR_SUCCESS;
}


// This is the main function in this file, it implements power policy
state_t policy_apply(polctx_t *c,signature_t *sig,node_freqs_t *freqs,int *ready)
{
    /*  Selected freqs */
    ulong *new_freq=freqs->cpu_freq;

    /*  Other */
    int i;
    uint ref;
    int gready;
    uint sid = 0;
    state_t st;

    /*  Used by policy */
    double power_proj, time_proj;
    double power_ref, time_ref, time_current;
    double freq_gain, perf_gain, min_eff_gain;

    ulong best_freq, best_pstate, freq_ref;
    ulong curr_freq, nominal;
    ulong curr_pstate, def_pstate, def_freq, imc_pstate;

    double local_eff_gain, base_eff_gain;

    double sig_vpi, node_sig_vpi, max_vpi;
    uint local_process_with_high_vpi;

    double lvpi, lvpi2;
    uint cbound, mbound, turbo_set = 0;
    ulong process_avgcpuf;

    ulong curr_imc_freq;
    uint eUFS = dyn_unc;
    uint curr_imc_pstate;

    // TODO: Can we assign this values calling some function who return values based on current architecture?
    ulong max_cpufreq_sel = 0, min_cpufreq_sel = 10000000;

    if (min_time_state == SELECT_CPUFREQ){
        ref_imc_pstate = -1;
    }

    if (c == NULL || c->app==NULL) {
        *ready = EAR_POLICY_CONTINUE;
        return EAR_ERROR;
    }
    verbose_master(2,"%s---MIN TIME---%s",COL_GRE,COL_CLR);

    if (gpus.node_policy_apply != NULL){
        /* Basic support for GPUS */
        gpus.node_policy_apply(c,sig,freqs,&gready);
        gpu_ready = gready;
    }else{
        gpu_ready = EAR_POLICY_READY;
    }

    if (cpu_ready == EAR_POLICY_READY){
        return EAR_SUCCESS;
    }

    min_eff_gain=c->app->settings[0];
    base_eff_gain = min_eff_gain;

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
        verbose_master(2, "PC set to %u", c->pc_limit);
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
        verbose(2,"Powercap is set to %uWatts",get_powercapopt_value(&c->app->pc_opt));
    }
#endif
    curr_pstate = frequency_closest_pstate(curr_freq);

    /*  We save data in imc data structure for policy comparision. We are computing the imc_pstate based on the avg_imc_freq,
     *  no looking at selection */
    if (eUFS){
        pstate_t tmp_state;
        curr_imc_freq = avg_to_khz(my_app->sig_ext->imc_freq);
        if (avgfreq_to_pstate((pstate_t *)imc_pstates,imc_num_pstates,curr_imc_freq,&tmp_state) == EAR_ERROR){
            verbose_master(2,"Current AVG IMC freq cannot be translated to IMC pstate %lu",curr_imc_freq);
        }
        curr_imc_pstate = tmp_state.idx;
        verbose_master(2,"CPU pstate %lu IMC freq %lu IMC pstate %u",curr_pstate,my_app->sig_ext->imc_freq,curr_imc_pstate);

        /*  Min time has more stable IMC freq. Below code avoids overriding the first IMC ref data*/
        if (ref_imc_pstate == -1 || curr_imc_pstate != ref_imc_pstate){
            copy_imc_data_from_signature(imc_data, curr_pstate, curr_imc_pstate, my_app);
        }
    }

    int policy_stable;
    int_policy_ok(sig, &cpufreq_signature, &policy_stable);
    verbose_master(2, "Policy ok returns %d", policy_stable);

    /*  If we are not stable, we start again */
    if (min_time_state != SELECT_CPUFREQ && !policy_stable){
        verbose_master(2, "%spolicy not stable%s", COL_RED, COL_CLR);
        min_time_state = SELECT_CPUFREQ;
    }

    if ((min_time_state == SELECT_CPUFREQ) || (eUFS == 0)){
        /*  SELECT CPUFREQ */
        if (eUFS){
            *ready = EAR_POLICY_TRY_AGAIN;
        }else{
            *ready = EAR_POLICY_READY;
        }
        verbose_master(2, "%sSELECT_CPUFREQ%s state dyn_unc %u", COL_YLW, COL_CLR, dyn_unc);

        last_cpu_pstate = curr_pstate;
        is_cpu_bound(my_app, &cbound);
        is_mem_bound(my_app, &mbound);
        verbose_master(2, "App CPU bound %u MEM bound %u", cbound, mbound);

        // If is not the default P_STATE selected in the environment, a projection
        // is made for the reference P_STATE in case the projections were available.
        if (!are_default_settings(&last_nodefreq_sel, &min_time_def_freqs)){
            verbose_node_freqs(2, &last_nodefreq_sel);
            verbose_node_freqs(2, &min_time_def_freqs);

            set_default_settings(freqs, &min_time_def_freqs);
            node_freqs_copy(&last_nodefreq_sel, freqs);

            verbose_master(2, "Setting default conf for MT");
            *ready = EAR_POLICY_TRY_AGAIN;
            return EAR_SUCCESS;
        }

        /*  Here we are running at the default freqs */
        for (i = 0; i < nump; i++){
            verbose_master(3, "Processing proc %d freq per core ? %d", i, freq_per_core);
            local_eff_gain = base_eff_gain;
            if (!freq_per_core){
                compute_total_node_metrics();
                compute_vpi(&sig_vpi, sig);
                compute_vpi(&node_sig_vpi, my_app);
                local_process_with_high_vpi = compute_max_vpi(lib_shared_region, sig_shared_region, &max_vpi);
                my_app->CPI = (double)my_app->cycles/(double)my_app->instructions;
                verbose_master(2, "AVGNodeSig MR[%d] SIGNATURE CPI %.3lf VPI %.3lf/Node CPI %.3lf VPI %3lf/MAX VPI %.3lf",
                        my_globalrank, sig->CPI, sig_vpi, my_app->CPI, node_sig_vpi, max_vpi);

                /*  With this two lines, we force the VPI of the signature to the most loaded in the node */
                my_app->instructions = sig_shared_region[local_process_with_high_vpi].sig.instructions;
                memcpy(my_app->FLOPS, sig_shared_region[local_process_with_high_vpi].sig.FLOPS, sizeof(ull) * FLOPS_EVENTS);
            }else{
                compute_ssig_vpi(&lvpi, &sig_shared_region[i].sig);
                ulong eff_p_cpuf, process_avgcpuf;
                process_avg_cpu_freq(&sig_shared_region[i].cpu_mask, cpufreq_data_per_core, &process_avgcpuf);
                from_minis_to_sig(my_app, &sig_shared_region[i].sig);

                /*
                 * verbose_master(2, "Per-processing MR[%d][%d] SIGNATURE CPI %.3lf avgCPUf %f (%lf, %lf) LVPI=%.3lf CP=%u",
                 * my_globalrank, i, my_app->CPI, (float)process_avgcpuf/1000000.0, percs_mpi[i], percs_mpi[min_mpi],
                 * lvpi, critical_path[i]);
                 */

                double factor = ear_math_exp(-5 * ear_math_scale(percs_mpi[min_mpi],
                            percs_mpi[max_mpi], percs_mpi[i])) * cbound * (1 - mbound) * critical_path[i];
                if (factor < 0.01){
                    factor = 0.0;
                }
                local_eff_gain = base_eff_gain * (1 - factor);
            }
            verbose_master(2, "Local efficiency gain for process %d: %lf", i, local_eff_gain);
            compute_reference(c, my_app, &curr_freq, &def_freq, &best_freq, &time_ref, &power_ref);
            verbose_master(3, "Time ref %lf Power ref %lf Freq ref %lu", time_ref, power_ref, best_freq);
            best_pstate = frequency_closest_pstate(best_freq);
            compute_cpu_freq_min_time(my_app, min_pstate, time_ref, local_eff_gain, curr_pstate,
                    best_pstate, best_freq, def_freq, &new_freq[i]);
            verbose_master(2, "CPU freq for process %d is %lu", i, new_freq[i]);
            if (use_turbo_for_cp){
                verbose_master(3, "checking to try to boost. freq per core ? %u already using turbo ? %u min_mpi ? %u",
                        freq_per_core, c->use_turbo, min_mpi);
                if (freq_per_core && !c->use_turbo && i == min_mpi){
                    if (cpu_supp_try_boost_cpu_freq(i, critical_path, new_freq, min_pstate)){
                        verbose_master(3, "Nominal freq. boosted for process %d: %lu", i, new_freq[i]);
                    }
                }
            }
            max_cpufreq_sel = ear_max(max_cpufreq_sel, new_freq[i]);
            min_cpufreq_sel = ear_min(min_cpufreq_sel, new_freq[i]);
        }
        if (!freq_per_core){
            set_all_cores(new_freq, num_processes, new_freq[0]);
        }

        /* We check if turbo is an option */
        if ((min_cpufreq_sel == nominal_node) && cbound && try_turbo_enabled){
            turbo_set = 1;
            verbose_master(2, "Using turbo because it is cbound and turbo enabled");
            set_all_cores(new_freq, num_processes, frequency_pstate_to_freq(0));
        }

        copy_cpufreq_sel(last_nodefreq_sel.cpu_freq, new_freq, sizeof(ulong) * num_processes);
        /*  We will use this signature to detect changes in the sig compared when the one used to compute the CPU freq */
        verbose_master(3,"Updating cpufreq_signature");
        signature_copy(&cpufreq_signature, sig);

        /*  IMC freq selection */
        uint app_mpi_bound = earl_phase_classification == APP_MPI_BOUND;
        if (eUFS && !(app_mpi_bound && network_use_imc)){
            if (!imc_set_by_hw){
                /*  Fix the imc to the first selected by hardware */
                for (sid=0; sid < imc_devices; sid++){
                    freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = ps_nothing; // avg_to_khz(my_app->sig_ext->imc_freq);
                }
            }
            else {
                for (sid=0; sid < imc_devices; sid++){
                    freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = imc_max_pstate[sid];
                }
            }
            for (sid=0; sid < imc_devices; sid++){
                freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = imc_min_pstate[sid];
            }
            sid = 0;
            verbose_master(2, "%sIMC freq selection%s %lu-%lu", COL_GRE, COL_CLR,
                    imc_pstates[freqs->imc_freq[sid*IMC_VAL+IMC_MAX]].khz,
                    imc_pstates[freqs->imc_freq[sid*IMC_VAL+IMC_MIN]].khz);
        }
        else{
            verbose_master(2, "%sNot IMC freq selection%s dyn_unc ? %u MPI_BOUND ? %u Network_IMC ? %u",
                    COL_RED, COL_CLR, eUFS, app_mpi_bound, network_use_imc);
            /* If application is network bound, we don't reduce the uncore frequency */
            if (eUFS && app_mpi_bound && network_use_imc){
                eUFS = 0;
                for (sid=0; sid < imc_devices; sid++){
                    // TODO: check if we should put min or max pstate
                    freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = imc_min_pstate[sid];
                    freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = imc_min_pstate[sid] + 1;
                }
                sid = 0;
            }
        }

        /*  Next state selection */
        if (turbo_set){
            min_time_state = TRY_TURBO_CPUFREQ;
            *ready = EAR_POLICY_TRY_AGAIN;
        }
        else {
            if (!eUFS){
                *ready = EAR_POLICY_READY;
                min_time_state = SELECT_CPUFREQ;
            }else{
                i = 0;
                while ((i < num_processes) && (last_nodefreq_sel.cpu_freq[i] == def_freq)) i++;
                if (i < num_processes || cbound){
                    min_time_state = COMP_IMCREF;
                }else{
                    ref_imc_pstate = curr_imc_pstate;
                    min_time_state = SELECT_IMCFREQ;
                }
            }
        }
        memcpy(last_nodefreq_sel.imc_freq, freqs->imc_freq, imc_devices*IMC_VAL * sizeof(ulong));
    }
    else if (min_time_state == COMP_IMCREF || min_time_state == TRY_TURBO_CPUFREQ){
        ref_imc_pstate = curr_imc_pstate;
        verbose_master(2,"%sCOMP_IMCREF%s | cpufreq %lu | imc %lu-%lu | imcfreq %lu | ref imc_pstate %u",
                COL_YLW, COL_CLR, *new_freq, imc_pstates[freqs->imc_freq[sid*IMC_VAL+IMC_MAX]].khz,
                imc_pstates[freqs->imc_freq[sid*IMC_VAL+IMC_MIN]].khz, curr_imc_freq, ref_imc_pstate);

        min_time_state = SELECT_IMCFREQ;
        *ready = EAR_POLICY_TRY_AGAIN;

        memcpy(freqs->cpu_freq, last_nodefreq_sel.cpu_freq, sizeof(ulong) * num_processes);
        memcpy(last_nodefreq_sel.imc_freq, freqs->imc_freq, imc_devices * IMC_VAL * sizeof(ulong));
    }
    else if ((min_time_state == SELECT_IMCFREQ) && eUFS){
        verbose_master(2,"%sSELECT_IMCFREQ%s state: nominal %d -- current CPU %lu, IMC %u -- ref IMC pstate %u",
                COL_YLW, COL_CLR, min_pstate, curr_pstate, curr_imc_pstate, ref_imc_pstate);
        /*  SELECT IMCFREQ */
        uint decrease_imc;
        if (ref_imc_pstate != imc_min_pstate[sid]){
            verbose(3, "ref IMC pstate (%d) not at maximum. Checking for increase opportunity.", ref_imc_pstate);
            decrease_imc = must_decrease_imc(imc_data, curr_pstate, curr_imc_pstate, curr_pstate,
                    ref_imc_pstate, my_app, imc_extra_th);

            /*  If we are over the limit after previous IMC update, we decrease the IMC */
            if (decrease_imc && !first_imc_try) {
                uint must_start_again = must_start(imc_data, curr_pstate, curr_imc_pstate,
                        curr_pstate, ref_imc_pstate, my_app);
                verbose(2,"%sWarning, passing the limit: start_again %u",COL_RED,must_start_again);
                for (sid = 0; sid < imc_devices; sid++){
                    if (freqs->imc_freq[sid*IMC_VAL+IMC_MAX] < imc_max_pstate[sid]){
                        freqs->imc_freq[sid*IMC_VAL+IMC_MAX] + 1;
                    }
                    freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = ps_nothing;
                }
                sid = 0;
                verbose_master(2,"Selecting %lu - %lu%s",
                        imc_pstates[freqs->imc_freq[sid*IMC_VAL+IMC_MAX]].khz,
                        imc_pstates[freqs->imc_freq[sid*IMC_VAL+IMC_MIN]].khz, COL_CLR);
                if (!must_start_again)  *ready = EAR_POLICY_READY;
                else *ready = EAR_POLICY_TRY_AGAIN;

                min_time_state = SELECT_CPUFREQ;

            }else {
                if (first_imc_try){
                    first_imc_try = 0;
                }

                for (sid = 0; i < imc_devices; i++){
                    freqs->imc_freq[sid*IMC_VAL+IMC_MAX] = ear_min(freqs->imc_freq[sid*IMC_VAL+IMC_MAX] - 1,
                            imc_min_pstate[sid]);
                    freqs->imc_freq[sid*IMC_VAL+IMC_MIN] = ps_nothing;
                }
                sid = 0;
                verbose_master(2, "%sTrying an upper IMC freq %lu-%lu%s", COL_GRE,
                        imc_pstates[freqs->imc_freq[sid*IMC_VAL+IMC_MAX]].khz,
                        imc_pstates[freqs->imc_freq[sid*IMC_VAL+IMC_MIN]].khz, COL_CLR);

                /*  We are assuming all sockets use the same frequency range */
                if (freqs->imc_freq[sid*IMC_VAL+IMC_MAX] < imc_max_pstate[sid]) *ready = EAR_POLICY_TRY_AGAIN;
                else{
                    *ready = EAR_POLICY_READY;
                    min_time_state = SELECT_CPUFREQ;
                }
            }
        }
        memcpy(freqs->cpu_freq, last_nodefreq_sel.cpu_freq, sizeof(ulong) * num_processes);
        memcpy(last_nodefreq_sel.imc_freq, freqs->imc_freq, imc_devices * IMC_VAL * sizeof(ulong));
    }
    verbose_master(2,"Next min_time state %u next policy state %u",min_time_state,*ready);
    return EAR_SUCCESS;
}

static state_t int_policy_ok(signature_t *curr_sig,signature_t *last_sig,int *ok)
{
    state_t st=EAR_SUCCESS;
    ulong eff_f;
    uint power_status;
    uint next_status;
    int i;

    verbose_master(2, "%sPOLICY OK EVALUATION%s", COL_YLW, COL_CLR);
    *ok=1;

    /*  Check for load balance data */
    if (is_mpi_enabled() && enable_load_balance){

        read_diff_node_mpi_type_info(lib_shared_region, sig_shared_region, my_node_mpi_types_calls, last_node_mpi_types_calls);
        read_diff_node_mpi_data(lib_shared_region, sig_shared_region, my_node_mpi_calls, last_node_mpi_calls);
        verbose_mpi_types(2,my_node_mpi_types_calls);

        uint unbalance;
        double mean, sd, mag;
        mpi_support_evaluate_lb(my_node_mpi_calls, num_processes, percs_mpi, &mean, &sd, &mag, &unbalance);

        verbose_master(2,"%sNode load unbalance%s: LB_TH %lf mean %lf standard deviation %lf magnitude %lf unbalance ? %u",
                COL_MGT, COL_CLR, LB_TH, mean, sd, mag, unbalance);

        /*  Check whether application has changed from unbalanced to balanced and viceversa */
        if (unbalance != signature_is_unbalance){
            if (!unbalance){
                verbose_master(2, "%sApplication becomes balanced!%s", COL_GRE, COL_CLR);
                signature_is_unbalance = 0;
            }else{
                verbose_master(2, "%sApplication becomes unbalanced%s", COL_RED, COL_CLR);
                signature_is_unbalance = 1;
                *ok=0;
                first_time=1;
            }
        }


        if (unbalance){
            freq_per_core = 1;
            nump = num_processes;

            double median;
            /*  Select critical path */
            mpi_support_select_critical_path(critical_path, percs_mpi, num_processes, mean, &median, &max_mpi, &min_mpi);

            /* We check if policy ok because if it is 0, it means that app became unbalanced from balanced
             * state and we don't need to check variation between policy iterations  */
            if (!first_time && *ok==1){

                mpi_support_verbose_perc_mpi_stats(2, last_percs_mpi, num_processes, last_mpi_stats_perc_mpi_mean,
                        last_mpi_stats_perc_mpi_median, last_mpi_stats_perc_mpi_sd, last_mpi_stats_perc_mpi_mag);

                /*  This code has not been encapsulated because is (by now) only for verbose purposes */
                double similarity;
                mpi_stats_evaluate_similarity(percs_mpi, last_percs_mpi, num_processes, &similarity);
                verbose_master(2, "(Orientation) Similarity between last and current: %lf", similarity);
                /* *************************************************************************** */

                uint mpi_changed;
                mpi_support_mpi_changed(mag, last_mpi_stats_perc_mpi_mag, critical_path, last_critical_path,
                        num_processes, &similarity, &mpi_changed);

                if (mpi_changed){
                    *ok = 0;
                }

            }else if (*ok){
                first_time = 0;
            }
            verbose_master(3, "Copying data...");

            /*  Save current stats and critical path to be used later */
            memcpy(last_percs_mpi, percs_mpi, sizeof(double) * num_processes);
            memcpy(last_critical_path, critical_path, sizeof(uint) * num_processes);
            last_mpi_stats_perc_mpi_sd = sd;
            last_mpi_stats_perc_mpi_mean = mean;
            last_mpi_stats_perc_mpi_mag = mag;
            last_mpi_stats_perc_mpi_median = median;
        }else nump = 1;
    }else{
        nump = 1;
    }

    if (*ok == 1){
        /*  If signature has changed, we must start again */
        if (signatures_different(last_sig, curr_sig, 0.2)){
            verbose_master(2, "%sSignature is too different from the one used for CPU freq. starting again%s",
                    COL_RED, COL_CLR);
            *ok=0;
        }
    }

    /*  Reset comparisions */
    if (*ok == 0){
        first_time = 1;
        first_imc_try = 1;
    }
    return EAR_SUCCESS;
}

state_t policy_ok(polctx_t *c,signature_t *curr_sig,signature_t *last_sig,int *ok)
{
    *ok = 0;
    if ((c==NULL) || (curr_sig==NULL) || (last_sig==NULL)) return EAR_ERROR;
    return int_policy_ok(curr_sig,last_sig,ok);
}

state_t policy_get_default_freq(polctx_t *c, node_freqs_t *f)
{
    if ((c!=NULL) && (c->app!=NULL)){
        node_freqs_copy(f, &min_time_def_freqs);
    }else return EAR_ERROR;
    return EAR_SUCCESS;
}

state_t policy_max_tries(polctx_t *c,int *intents)
{
    *intents=1;
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
    cpu_ready = EAR_POLICY_READY;
    for (i=0;i<num_processes;i++) freqs->cpu_freq[i] = frequency_pstate_to_freq(c->num_pstates - 1);
    memcpy(last_nodefreq_sel.cpu_freq,freqs->cpu_freq,MAX_CPUS_SUPPORTED*sizeof(ulong));
    if (dyn_unc){
        for (sock_id=0;sock_id<arch_desc.top.socket_count;sock_id++){
            freqs->imc_freq[sock_id*IMC_VAL+IMC_MAX] = imc_max_pstate[sock_id];
            freqs->imc_freq[sock_id*IMC_VAL+IMC_MIN] = imc_max_pstate[sock_id];
        }
        memcpy(last_nodefreq_sel.imc_freq,freqs->imc_freq,imc_devices*IMC_VAL*sizeof(ulong));
    }
#if USE_GPUS
    gpu_ready = EAR_POLICY_READY;
    return EAR_SUCCESS;
    if (gidle){
        gpu_ready = EAR_POLICY_READY;
        for (uint gpuid=0;gpuid<c->num_gpus;gpuid++){
            freqs->gpu_freq[gpuid] = gpuf_pol_list[gpuid][gpuf_pol_list_items[gpuid]-1];
        }
        memcpy(last_nodefreq_sel.gpu_freq,freqs->gpu_freq,c->num_gpus*sizeof(ulong));
    }
#else
    gpu_ready = EAR_POLICY_READY;
#endif
    /* GPU mem is pending */

    return EAR_SUCCESS;
}

state_t policy_busy_wait_settings(polctx_t *c,signature_t *my_sig,node_freqs_t *freqs)
{
    int i;
    uint sock_id;
    if (c == NULL) return EAR_ERROR;
    cpu_ready = EAR_POLICY_READY;
    for (i=0;i<num_processes;i++) set_all_cores(freqs->cpu_freq, num_processes, frequency_pstate_to_freq(c->num_pstates - 1));
    copy_cpufreq_sel(last_nodefreq_sel.cpu_freq, freqs->cpu_freq, sizeof(ulong)*MAX_CPUS_SUPPORTED);
    if (dyn_unc){
        for (uint sock_id=0;sock_id<arch_desc.top.socket_count;sock_id++){
            freqs->imc_freq[sock_id*IMC_VAL+IMC_MAX] = imc_max_pstate[sock_id];
            freqs->imc_freq[sock_id*IMC_VAL+IMC_MIN] = imc_max_pstate[sock_id];
        }
        memcpy(last_nodefreq_sel.imc_freq, freqs->imc_freq, imc_devices * IMC_VAL * sizeof(ulong));
    }
#if USE_GPUS
    gpu_ready = EAR_POLICY_READY;
    return EAR_SUCCESS;
    if (gidle){
        gpu_ready = EAR_POLICY_READY;
        for (uint gpuid=0;gpuid<c->num_gpus;gpuid++){
            freqs->gpu_freq[gpuid] = gpuf_pol_list[gpuid][0];
        }
        memcpy(last_nodefreq_sel.gpu_freq,freqs->gpu_freq,c->num_gpus*sizeof(ulong));
    }
#else
    gpu_ready = EAR_POLICY_READY;
#endif
    /* GPU mem is pending */

    return EAR_SUCCESS;
}

state_t policy_restore_settings(polctx_t *c,signature_t *my_sig,node_freqs_t *freqs)
{
    int i;
    uint sock_id;
    if (c == NULL) return EAR_ERROR;

    node_freqs_copy(freqs, &min_time_def_freqs);
    node_freqs_copy(&last_nodefreq_sel,freqs);

    return EAR_SUCCESS;
}

state_t compute_total_node_metrics(){
    compute_total_node_instructions(lib_shared_region,sig_shared_region,&my_app->instructions);
    compute_total_node_flops(lib_shared_region,sig_shared_region,my_app->FLOPS);
    compute_total_node_cycles(lib_shared_region,sig_shared_region,&my_app->cycles);
    return EAR_SUCCESS;
}
