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

#if MPI
#include <mpi.h>
#endif
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
// #define SHOW_DEBUGS 1
#if !SHOW_DEBUGS
#define NDEBUG // Activate assert calls only in the debug mode.
#endif
#include <assert.h>

#include <common/config.h>
#include <common/states.h>
#include <common/output/verbose.h>
#include <common/colors.h>
#include <common/math_operations.h>
#include <common/types/log.h>
#include <common/types/application.h>
#include <common/environment.h>

#include <management/cpufreq/frequency.h>

#include <daemon/local_api/eard_api.h>

#include <report/report.h>

#include <library/common/externs.h>
#include <library/common/global_comm.h>
#include <library/common/verbose_lib.h>
#include <library/tracer/tracer.h>
#include <library/metrics/metrics.h>
#include <library/policies/policy.h>
#include <library/policies/policy_state.h>
#include <library/policies/common/cpu_support.h>
#include <library/states/states_comm.h>
#include <library/states/states.h>

#define NEW_CPUFREQ_API 1 // Enables the usage of the new metrics API,
                          // instead of the deprecated frequency API.
#define VERB_SAV_PEN    2 // Verbose level for saving and penalty estimation.
#define DYNAIS_CUTOFF	1 // Enables the dynamic DynAIS turning off. 
#define MAX_SIG_TIME    10000000 // 10 s. It is a time limit for signature computation.

extern masters_info_t masters_info; // TODO: Where declared ?
extern float          ratio_PPN;    // TODO: Where declared ?

static application_t *signatures; // TODO: usage
static uint          *sig_ready;  // TODO: usage

/* Defined at ear.c */
extern report_id_t    rep_id;

extern cluster_conf_t cconf;

// static application_t last_signature; TODO: remove if not needed

// TODO: usage
static llong          comp_N_begin;
static llong          comp_N_end;
static llong          comp_N_time;

// TODO: usage
static uint           begin_iter;
static uint           N_iter;

// TODO: usage
static uint           tries_current_loop           = 0;
static uint           tries_current_loop_same_freq = 0;

// TODO: usage
static uint           perf_count_period            = 100;
static uint           loop_perf_count_period;
static uint           perf_count_period_10p;

static ulong          policy_cpufreq_khz;

       ulong          perf_accuracy_min_time       = 1000000; // Default value for this setting. TODO: what means?

static uint           EAR_STATE                    = NO_PERIOD; // State machine controller
// uint current_earl_state = NO_PERIOD; Commented because it doesn't seems this variable is used.

static uint           EAR_POLICY_STATE             = EAR_POLICY_NO_STATE; // Policy state ("state machine")

static int            current_loop_id; // TODO: This variable does not seems to be used.

static int            MAX_POLICY_TRIES;

static state_t        policy_state_ret; // Policy state (state returned by functions)

static uint           total_threads_cnt; // Total number of processes being executed at the node.

static uint           signatures_stables           = 0;

static signature_t    policy_sel_signature; // Signature sent for policy selection. TODO: Check if this usage is true

static node_freqs_t   node_freqs;

static float          MAX_DYNAIS_OVERHEAD_DYN      = MAX_DYNAIS_OVERHEAD; // Maximum DynAIS overhead permitted.
       uint           waiting_for_node_signatures  = 0;

#if 0
/* Aux to print signatures. Commented because not used. */
static char sig_buff[1024];
#endif

#if EAR_OVERHEAD_CONTROL // at common/config/config_def.h set to 1 by default.
extern uint check_periodic_mode;
#endif

#if DYNAIS_CUTOFF
/** This function checks whether this application is going to be affected or not (an estimation based on CPI and GBS) by DynIAS. */
static state_t check_dynais_on(signature_t *A, signature_t *B)
{
    assert(A != NULL && B != NULL);
    if (!equal_with_th(A->CPI, B->CPI, EAR_ACCEPTED_TH * 2) || !equal_with_th(A->GBS, B->GBS, EAR_ACCEPTED_TH * 2)) {
        return EAR_SUCCESS;
    }
    return EAR_ERROR;
}
#endif

#if EAR_OVERHEAD_CONTROL
/** This function checks if there is too much overhead from DynAIS and sets it to OFF. */
static void check_dynais_off(ulong mpi_calls_iter, uint period, uint level, ulong event)
{
    if (loop_signature.signature.time > 0) {
        ulong dynais_overhead_usec = mpi_calls_iter;
        float dynais_overhead_perc = ((float) dynais_overhead_usec / 1000000.0) * 100.0 / loop_signature.signature.time;
        if (dynais_overhead_perc > MAX_DYNAIS_OVERHEAD_DYN) {
            // Disable dynais: TODO: API is still pending.
#if DYNAIS_CUTOFF
            dynais_enabled = DYNAIS_DISABLED;
#endif
            if (masters_info.my_master_rank >= 0) log_report_dynais_off(application.job.id, application.job.step_id);
        }
        if (dynais_enabled == DYNAIS_DISABLED){
            verbose_master(3, "DYNAIS_DISABLED: Total time %lf s dynais overhead %lu usec in %lu mpi calls(%lf percent), event=%lu min_time=%lu",
                    loop_signature.signature.time, dynais_overhead_usec, mpi_calls_iter, dynais_overhead_perc, event, perf_accuracy_min_time);
        }
    }

    last_first_event    = event;          // TODO: where it comes from? usage
    last_calls_in_loop  = mpi_calls_iter; // TODO: where it comes from? usage
    last_loop_size      = period;         // TODO: where it comes from? usage
    last_loop_level     = level;          // TODO: where it comes from? usage
}
#endif // EAR_OVERHEAD_CONTROL

static uint policy_had_effect(signature_t *A, signature_t *B)
{
    assert(A != NULL && B != NULL);
    if (equal_with_th(A->CPI, B->CPI, EAR_ACCEPTED_TH) &&
            equal_with_th(A->GBS, B->GBS, EAR_ACCEPTED_TH))
    {
        return 1;
    }
    return 0;
}

#if 0
static void print_loop_signature(char *title, signature_t *loop)
{
    float avg_f = (float) loop->avg_f / 1000000.0;

    debug("(%s) Avg. freq: %.2lf (GHz), CPI/TPI: %0.3lf/%0.3lf, GBs: %0.3lf, DC power: %0.3lf, time: %0.3lf, GFLOPS: %0.3lf",
            title, avg_f, loop->CPI, loop->TPI, loop->GBS, loop->DC_power, loop->time, loop->Gflops);
}
#endif

// void states_new_iteration(int my_id, uint period, uint iterations, uint level, ulong event, ulong mpi_calls_iter, uint dynais_used);

timestamp_t time_last_signature;
uint signature_reported = 0;

static void report_loop_signature(uint iterations, loop_t *my_loop, job_t *job)
{
    my_loop->total_iterations = iterations;
    my_loop->jid              = job->id;
    my_loop->step_id          = job->step_id;

	// EAR uses this timestamp to switch to periodic mode (or not).
    timestamp_getfast(&time_last_signature);
    signature_reported = 1;

    if (masters_info.my_master_rank >= 0) {
        clean_db_loop(my_loop, system_conf->max_sig_power);
        application.power_sig.max_DC_power = ear_max(application.power_sig.max_DC_power, my_loop->signature.DC_power);
        if (state_fail(report_loops(&rep_id, my_loop, 1))) {
            verbose_master(1, "%sError%s Reporting Loop", COL_RED, COL_CLR);
        }
    }
}

static void accumulate_energy_savings(signature_t *loops, signature_t *apps)
{
    static char verb_savs_pens_hdr[128] =
        "    --- Savings and penalties ---\n"
        "Scope E.save  P.save  T.pen   T.elapsed\n"
        "----- ------  ------  -----   ---------"; // Header for savings and penalties verbose

	sig_ext_t *loop_sig_ext = loops->sig_ext;	
	sig_ext_t *app_sig_ext;

	if (loop_sig_ext == NULL) return;
	if (apps->sig_ext == NULL) {
		apps->sig_ext = (void *) calloc(1, sizeof(sig_ext_t));
	}
	app_sig_ext = apps->sig_ext;

	if (loop_sig_ext->saving == 0) return;

	app_sig_ext->saving   += loop_sig_ext->saving;
	app_sig_ext->psaving  += loop_sig_ext->psaving;
	app_sig_ext->telapsed += loop_sig_ext->elapsed;
	app_sig_ext->tpenalty += loop_sig_ext->tpenalty;

#ifdef VERB_SAV_PEN
    verbose_master(VERB_SAV_PEN, "%s\n Loop %-7.2lf %-7.2f %-7.2f %-9.2lf\n  App %-7.2lf %-7.2f %-7.2f %-9.2lf\n", verb_savs_pens_hdr,
            loop_sig_ext->saving, loop_sig_ext->psaving, loop_sig_ext->tpenalty, loop_sig_ext->elapsed,
            app_sig_ext->saving, app_sig_ext->psaving, app_sig_ext->tpenalty, app_sig_ext->telapsed);
#endif
}

/* --- End auxiliary functions --- */

/** Executed at application start. Reads EAR configuration environment variables. */
void states_begin_job(int my_id, char *app_name)
{
    char *max_dyn_ov = getenv("EAR_MAX_DYNAIS_OVERHEAD");
    if (max_dyn_ov != NULL) {
        MAX_DYNAIS_OVERHEAD_DYN = atof(max_dyn_ov);
    }

    node_freqs_alloc(&node_freqs);

    // The number of available pstates
#if NEW_CPUFREQ_API
    uint pstate_cnt = metrics_get(MGT_CPUFREQ)->avail_count;
#else
    uint pstate_cnt = frequency_get_num_pstates();
#endif

    signatures = (application_t *) calloc(pstate_cnt, sizeof(application_t));
    sig_ready  =          (uint *) calloc(pstate_cnt, sizeof(uint));

    for (uint i = 0; i < pstate_cnt; i++) { 
        init_application(&signatures[i]);
        sig_ready[i] = 0;
    }

#if ONLY_MASTER
    if (my_id) return;
#endif

    /* LOOP WAS CREATED HERE BEFORE */

    perf_accuracy_min_time = get_ear_performance_accuracy();
	verbose_master(2, "perf_accuracy_min_time default: %lu", perf_accuracy_min_time);

    ulong architecture_min_perf_accuracy_time;
    if (masters_info.my_master_rank >= 0 && eards_connected()) {
        architecture_min_perf_accuracy_time = eards_node_energy_frequency();
        if (architecture_min_perf_accuracy_time == (ulong) EAR_ERROR) {
            architecture_min_perf_accuracy_time = perf_accuracy_min_time;
        }
        verbose_master(2, "eard perf_accuracy_min_time default %lu", architecture_min_perf_accuracy_time);
    } else {
        architecture_min_perf_accuracy_time = 1000000;
    }
    if ((architecture_min_perf_accuracy_time > perf_accuracy_min_time) && (architecture_min_perf_accuracy_time < MAX_SIG_TIME)) {
        perf_accuracy_min_time = architecture_min_perf_accuracy_time;
    }
    verbose_master(2, "perf_accuracy_min_time %lu", perf_accuracy_min_time);

    EAR_STATE = NO_PERIOD;

    policy_cpufreq_khz = EAR_default_frequency;

    init_log();

    policy_state_ret = policy_max_tries(&MAX_POLICY_TRIES);

    total_threads_cnt =  get_total_resources();
    debug("Total threads: %u", total_threads_cnt);

}

/** This function is executed at application end. */
void states_end_job(int my_id,  char *app_name)
{
    debug("EAR(%s) Ends execution.", app_name);
    node_freqs_free(&node_freqs);
    free(signatures);
    free(sig_ready);
    end_log();
}

int states_ear_state()
{
    return EAR_STATE;
}

/** This function is executed at each loop init */
void states_begin_period(int my_id, ulong event, ulong size, ulong level)
{
    EAR_STATE = TEST_LOOP;

    tries_current_loop           = 0;
    tries_current_loop_same_freq = 0;

    if (state_fail(loop_init(&loop, &loop_signature.job, event, size, level))) {
        verbose_master(1, "%sError%s Creating loop.", COL_RED, COL_CLR);
    }

    policy_loop_init(&loop.id);

    comp_N_begin = metrics_time();

    if (masters_info.my_master_rank >= 0) {
        traces_new_period(ear_my_rank, my_id, event);
    }

    loop_with_signature = 0;
#if NEW_CPUFREQ_API
    uint pstate_cnt = metrics_get(MGT_CPUFREQ)->avail_count;
#else
    uint pstate_cnt = frequency_get_num_pstates();
#endif
    assert(pstate_cnt != 0);
    for (uint i = 0; i < pstate_cnt; i++){
        init_application(&signatures[i]);
        sig_ready[i]=0;
    }
}


/** This function is executed at each loop end */
void states_end_period(uint iterations)
{
    if (loop_with_signature)
    {
        loop.total_iterations = iterations;
        //append_loop_text_file(loop_summary_path, &loop,&loop_signature.job);
        if (system_conf->report_loops){
            if (masters_info.my_master_rank>=0){
                clean_db_loop(&loop, system_conf->max_sig_power);
                if (report_loops(&rep_id,&loop,1) != EAR_SUCCESS){
                    verbose_master(0,"Error reporting loop");
                }
            }
        }
    }

    loop_with_signature = 0;
    policy_loop_end(&loop.id);
}

/** This is the main function when used DynAIS. It applies
 * the EAR state diagram that drives the internal behaviour. */
void states_new_iteration(int my_id, uint period, uint iterations,
        uint level, ulong event, ulong mpi_calls_iter,uint dynais_used)
{
    ulong prev_cpufreq_khz, lavg;
    int ready;
    int result;
    int pok;
    int curr_pstate,def_pstate;
    signature_t *l_sig;
    ulong policy_def_freq;
    
    char use_case[SHORT_GENERIC_NAME]; // 16 bytes allocated. Used for verbose signature.

    /***************************************************************************************************/
    /**** This function can potentially include data sharing between masters, depends on the policy ****/
    /***************************************************************************************************/

    sig_shared_region[my_node_id].iterations++;
    signature_t check_signature;	
    if (state_ok(metrics_new_iteration(&check_signature))) {
        policy_state_ret = policy_new_iteration(&check_signature);
        if (state_fail(policy_state_ret)) {
            verbose_master(2, "%sNew Phase detected%s", COL_BLU, COL_CLR);
            policy_set_default_freq(NULL);
            comp_N_begin = metrics_time();
            EAR_STATE = FIRST_ITERATION;
            return;
        }
    }

    prev_cpufreq_khz = ear_frequency; // TODO: Why khz are prev, but pstate is curr?
    curr_pstate      = frequency_closest_pstate(ear_frequency);

    policy_state_ret = policy_get_default_freq(&node_freqs); // Slaves don't fill node_freqs
    policy_def_freq  = node_freqs.cpu_freq[0];	
    def_pstate       = frequency_closest_pstate(policy_def_freq);

    /* A rescheduling is a global event generated by the EARGM */
    if (system_conf != NULL) {
        if ((masters_info.my_master_rank >= 0) && (resched_conf->force_rescheduling)) {

            traces_reconfiguration(ear_my_rank, my_id);

            resched_conf->force_rescheduling=0;

            verbose_master(2, "--- [EAR] Re-scheduling forced by EARD ---\n%-20s: %u\n%-20s: %lu\n%-20s: %lu\n%-20s: %lf\n",
                    "Curr. state", EAR_STATE, "Max CPUfreq", system_conf->max_freq, "Def. CPUfreq",
                    system_conf->def_freq, "Policy th", system_conf->settings[0]);


            comp_N_begin = metrics_time();

            EAR_STATE = SIGNATURE_HAS_CHANGED;
            verbose_master(2,"New state by reschedduling: %u", EAR_STATE);
            state_report_traces_state(masters_info.my_master_rank, ear_my_rank, my_id, EAR_STATE);

            tries_current_loop_same_freq = 0;
            tries_current_loop           = 0;

            return;
        }
    }

    /* Based on the EAR internal state... */
    switch (EAR_STATE)
    {
        /* This is a new state to minimize overhead, we just check the loop has, at least, some granularity */
        case TEST_LOOP:
            comp_N_end = metrics_time();
            comp_N_time = metrics_usecs_diff(comp_N_end, comp_N_begin);
            if (comp_N_time > (perf_accuracy_min_time * 0.1)) {
                debug("Going to FIRST_ITERATION after %d iterations", iterations);
                comp_N_begin = comp_N_end;
                EAR_STATE = FIRST_ITERATION;
                if (masters_info.my_master_rank >= 0) traces_start();
            }
            break;

        /* FIRST_ITERATION computes the iteration duration and how many
         * iterations needs to be considered to compute a signature */
        case FIRST_ITERATION:
            comp_N_end = metrics_time();
            comp_N_time = metrics_usecs_diff(comp_N_end, comp_N_begin);

            if (comp_N_time == 0) comp_N_time = 1;

            /* We include a dynamic configurarion of EAR */
            if (comp_N_time < (llong) perf_accuracy_min_time) {
                perf_count_period = (perf_accuracy_min_time / comp_N_time) + 1;
            } else {
                perf_count_period = 1;
            }

            loop_perf_count_period = perf_count_period;

            /* Included to accelerate the signature computation */
            perf_count_period_10p = perf_count_period / 10;
            if (perf_count_period_10p == 0) {
                perf_count_period_10p = 1;
            }

            /* Once min iterations are computed for performance accuracy we start computing application signature */
            EAR_STATE = EVALUATING_LOCAL_SIGNATURE;
            state_report_traces_state(masters_info.my_master_rank, ear_my_rank, my_id, EAR_STATE);
            metrics_compute_signature_begin();
            begin_iter = iterations;

            loop.id.event = event;
            loop.id.level = level;
            loop.id.size = period;
            break;

        /* If the signature (basically CPI and GBS) changes, we must recompute the number of iterations */
        case SIGNATURE_HAS_CHANGED: /********* SIGNATURE_HAS_CHANGED *************/
            comp_N_end = metrics_time();
            comp_N_time = metrics_usecs_diff(comp_N_end, comp_N_begin);

            if (comp_N_time < (long long) perf_accuracy_min_time) {
                perf_count_period = (perf_accuracy_min_time / comp_N_time) + 1;
            } else {
                perf_count_period = 1;
            }                                        
            /* Included to accelerate the signature computation */
            perf_count_period_10p=perf_count_period/10;
            if (perf_count_period_10p==0) perf_count_period_10p=1;
            /**/
            loop_perf_count_period=perf_count_period;
            EAR_STATE = EVALUATING_LOCAL_SIGNATURE;
            break;

            /* Again, we recompute the number of iterations to be considered but in this case because we have changed
             * the frequency. TODO: This state could be removed */
        case RECOMPUTING_N: /************ RECOMPUTING_N *************/

            comp_N_time = metrics_usecs_diff(comp_N_end, comp_N_begin);
            if (comp_N_time == 0) {
                error( "EAR(%s): PANIC comp_N_time must be > 0", ear_app_name);
            }

            /* We include a dynamic configurarion of DPD behaviour */
            if (comp_N_time < (llong) perf_accuracy_min_time) {
                perf_count_period = (perf_accuracy_min_time / comp_N_time) + 1;
            } else {
                perf_count_period = 1;
            }

            /* Included to accelerate the signature computation */
            perf_count_period_10p = perf_count_period / 10;
            if (perf_count_period_10p == 0) {
                perf_count_period_10p = 1;
            }

            loop_perf_count_period = perf_count_period;

            EAR_STATE = SIGNATURE_STABLE;

            break;

            /* After N iterations of being in this state the signature will be computed and the policy will be
             * applied, next state will depend on the policy itself. See the REAMDE file to see the different
             * alternatives */
        case EVALUATING_LOCAL_SIGNATURE: /********* EVALUATING_LOCAL_SIGNATURE **********/
            /* We check from time to time if the signature is ready */
            /* Included to accelerate the signature computation */
            verbose_master(4, "%s--- EVALUATING LOCAL SIGNATURE ---%s", COL_YLW, COL_CLR);

            if (!lib_shared_region->master_ready) {
                if (!dynais_used && ((iterations % mpi_calls_iter) != 0)) return;

                /* Only when using dynais we consider the 'N' */
                if (dynais_used) {
                    if ((iterations % perf_count_period_10p) == 0) {
                        if (time_ready_signature(perf_accuracy_min_time)){	
                            perf_count_period=iterations - 1;
                            if (perf_count_period==0) perf_count_period=1;
                        }
                    }
                    if (((iterations - 1) % perf_count_period) || (iterations == 1)) return;
                }
            }

#if SHOW_DEBUGS
            if (lib_shared_region->master_ready){
                if (masters_info.my_master_rank >= 0){
                    debug(" Master: master ready waiting for slaves evaluating");
                }else{
                    debug("Slave: going to compute signature because master is ready evaluating");
                }
            }
#endif

            N_iter = iterations - begin_iter;

            /* Signature computation */
            result = metrics_compute_signature_finish(&loop_signature.signature, N_iter, perf_accuracy_min_time, total_threads_cnt);	
            if (result == EAR_NOT_READY)
            {
                if (!lib_shared_region->master_ready) perf_count_period++;
                return;
            }

			/* At this point, signatures for all the process in the node are ready.
             * lib_shared_region->job_signature includes the job signature. */ 
			compute_per_process_and_job_metrics(&loop_signature.signature);

            /* Included for dynais test */
#ifdef SHOW_DEBUGS
            if (loop_with_signature == 0) {
                time_t curr_time;
                double time_from_mpi_init;
                time(&curr_time);    
                time_from_mpi_init = difftime(curr_time, application.job.start_time);
                debug("Number of seconds since the application start_time at which signature is computed %lf",
                        time_from_mpi_init);
            }
#endif

            loop_with_signature = 1;

#if EAR_OVERHEAD_CONTROL
            if (dynais_used) {
                check_periodic_mode = 0;
            }
            /* Computing dynais overhead. Change dynais_enabled to ear_tracing_status=DYNAIS_ON/DYNAIS_OFF. */
            if (dynais_used && (dynais_enabled==DYNAIS_ENABLED)) {
                check_dynais_off(mpi_calls_iter, period, level, event);
            }
#endif
            current_loop_id = event;
            //begin_iter = iterations;

            memcpy(&signatures[curr_pstate], &loop_signature, sizeof(application_t));
            sig_ready[curr_pstate] = 1;

    		/* This function takes into account if we are using or not the whole node. */
            signature_t app_signature;	
            adapt_signature_to_node(&app_signature, &loop_signature.signature, ratio_PPN);

            /* This function executes the energy policy. app_signature is the local process signature,
             * the policy will compute the average if needed */
            policy_state_ret = policy_node_apply(&app_signature, &policy_cpufreq_khz, &ready);

            /* TODO: For no masters, ready will be 0, pending */
            EAR_POLICY_STATE = ready;

			/* Now we compute the JOB signature for reporting */
			if (masters_info.my_master_rank >= 0) {
		    	metrics_node_signature(&lib_shared_region->job_signature, &loop.signature);
				signature_copy(&lib_shared_region->job_signature, &loop.signature);
            }
            /* At this point loop and job_signature are the JOB signature
             * with accumulated per process and global metrics. */

            /* State check */
            if (EAR_POLICY_STATE == EAR_POLICY_READY){
                /* When the policy is ready to be evaluated, we go to the next state */
                verbose_master(2, "Policy ready after evaluating local signature");

                signatures_stables = 0;
                if ((policy_cpufreq_khz != prev_cpufreq_khz) && (dynais_used)) {
                    tries_current_loop++;
                    comp_N_begin = metrics_time();
                    EAR_STATE = RECOMPUTING_N;
                } else {
                    EAR_STATE = SIGNATURE_STABLE; 
                }
                signature_copy(&policy_sel_signature, &lib_shared_region->job_signature);
            } else if (EAR_POLICY_STATE == EAR_POLICY_GLOBAL_EV) {
                EAR_STATE = EVALUATING_GLOBAL_SIGNATURE;
            }

            begin_iter = iterations;

            /* TODO: To be removed */
			signature_copy(&loop.signature, &lib_shared_region->job_signature);

            report_loop_signature(iterations, &loop, &loop_signature.job); // report plugin

            policy_get_current_freq(&node_freqs);
            lavg = node_freqs_avgcpufreq(node_freqs.cpu_freq);	

            // TODO: Bad usage of def_f storing a per-process average.
            if (lavg > 0) {
                loop_signature.signature.def_f = lavg;
            }

            /* VERBOSE */
            state_report_traces(masters_info.my_master_rank, ear_my_rank, my_id, &loop, policy_cpufreq_khz, EAR_STATE); // tracer plugin

            // The reported signature will show whether the Library is using DynAIS
            if (dynais_used) {
                sprintf(use_case, "D");
            } else {
                sprintf(use_case, "P");
            }
            
            // Verbose loop signature
            state_verbose_signature(&loop, masters_info.my_master_rank, show_signatures,
                    ear_app_name, application.node_id, iterations, prev_cpufreq_khz, policy_cpufreq_khz, use_case);

            break; // EVALUATING_LOCAL_SIGNATURE

        case EVALUATING_GLOBAL_SIGNATURE:

            policy_app_apply(&policy_cpufreq_khz, &ready);

            // Select the next state
            if (ready == EAR_POLICY_READY){
                EAR_STATE = EVALUATING_LOCAL_SIGNATURE;
            }

            break;

        /* If the policy is ok, we are in stable state and we increase
         * the number of iterations to compute the signature. */
        case SIGNATURE_STABLE:

            if (!lib_shared_region->master_ready) {
                if (!dynais_used && ((iterations % mpi_calls_iter) != 0)) {
                    return;
                }

                // I have executed N iterations more with a new frequency, we must check the signature
                if (dynais_used && ((iterations - 1) % perf_count_period)) {
                    return;
                }
            }

#if SHOW_DEBUGS
            if (lib_shared_region->master_ready) {
                if (masters_info.my_master_rank >= 0){
                    debug("(Stable) Master ready and waiting for slaves");
                } else {
                    debug("(Stable) Slave going to compute signature because master is ready");
                }
            }
#endif
            /* We can compute the signature */
            N_iter = iterations - begin_iter;

            result = metrics_compute_signature_finish(&loop_signature.signature, N_iter, perf_accuracy_min_time, total_threads_cnt);
            if (result == EAR_NOT_READY)
            {
                if (!lib_shared_region->master_ready) {
                    perf_count_period++;
                }
                return;
            }

			/* At this point, signatures are ready */

            /* This function uses cpu power model and accumulates global metrics (GBS, Power etc) */
            compute_per_process_and_job_metrics(&loop.signature);

			/* This function accumulates per procecss metrics in loop.signature,
             * so loop.signature is the job signature */	 	
            if (masters_info.my_master_rank >= 0) {
                metrics_node_signature(&lib_shared_region->job_signature, &loop.signature);
                signature_copy(&loop_signature.signature, &loop.signature);
                signature_copy(&lib_shared_region->job_signature, &loop.signature);
			}
            /* At this point loop, loop_signature and job_signature are the
             * JOB signature with accumulated per process and global metrics */
            report_loop_signature(iterations, &loop, &loop_signature.job);

            /* VERBOSE */
            if (dynais_used) {
                sprintf(use_case, "D-Stable");
            } else {
                sprintf(use_case, "P-Stable");
            }
            state_report_traces(masters_info.my_master_rank, ear_my_rank, my_id, &loop, prev_cpufreq_khz, EAR_STATE);
            state_verbose_signature(&loop, masters_info.my_master_rank, show_signatures, ear_app_name, application.node_id, iterations, prev_cpufreq_khz,0,use_case);
            /* END VERBOSE */

            begin_iter = iterations;
            if (sig_ready[curr_pstate] == 0){
                memcpy(&signatures[curr_pstate], &loop_signature, sizeof(application_t));
                sig_ready[curr_pstate]=1;
            }

            /* We must evaluate policy decissions */
            signatures_stables++;
            l_sig = &policy_sel_signature;
            if (sig_ready[def_pstate] == 0){
                verbose_master(2,"Signature at default freq not available, assuming policy ok");
                /* If default is not available, that means a dynamic configuration has been decided, we assume we are ok */
                if (sig_ready[curr_pstate] == 0){
                    memcpy(&signatures[curr_pstate], &loop_signature, sizeof(application_t));
                    sig_ready[curr_pstate] = 1;
                }
                return;
            }
            policy_state_ret = policy_ok(&loop_signature.signature, l_sig,&pok);
						
			/* Accumulate Energy savings */
			accumulate_energy_savings(&loop_signature.signature, &application.signature);
						
			// accumulate_energy_savings(&lib_shared_region->job_signature, &application.signature);

            char *ok_clr;
            ok_clr = COL_GRE;
            if (pok){
                /* When collecting traces, we maintain the period */
                if ((masters_info.my_master_rank>=0) && (traces_are_on()==0)){
                    perf_count_period = perf_count_period * 2;
                }
                tries_current_loop = 0;
            } else {
                /* Reset */
                policy_set_default_freq(&loop_signature.signature);
                EAR_STATE = EVALUATING_LOCAL_SIGNATURE;
                ok_clr = COL_RED;
            }
            verbose_master(2,"%sPolicy ok=%u%s current (%.2lf/%.2lf/%.2lf) last/pol_sel (%.2lf/%.2lf/%.2lf)/(%.2lf/%.2lf/%.2lf)",ok_clr,pok,COL_CLR,
                    loop_signature.signature.CPI,loop_signature.signature.GBS,loop_signature.signature.DC_power,l_sig->CPI,l_sig->GBS,l_sig->DC_power,
                    policy_sel_signature.CPI,policy_sel_signature.GBS,policy_sel_signature.DC_power);
            if (pok) return;

            // We can give the library a second change in case we are at def freq
            if (((tries_current_loop<MAX_POLICY_TRIES) && (curr_pstate==def_pstate)) || (!dynais_used))
            {
                EAR_STATE = EVALUATING_LOCAL_SIGNATURE;
                break;
            }

            /* If we cannot select an optimal frequency, We must report a problem and go to the default configuration*/
            if (tries_current_loop>=MAX_POLICY_TRIES){
                EAR_STATE = PROJECTION_ERROR;
                policy_state_ret=policy_get_default_freq(&node_freqs);
                policy_cpufreq_khz = node_freqs.cpu_freq[0];
                if (masters_info.my_master_rank >= 0) { 
                    log_report_max_tries(application.job.id,application.job.step_id, application.job.def_f);
                    traces_frequency(ear_my_rank, my_id, policy_cpufreq_khz);
                }
                break;
            }

            /* We are not going better. If signature is different, we consider as a new loop case. */
            if (!policy_had_effect(&loop_signature.signature, l_sig))
            {
                EAR_STATE = SIGNATURE_HAS_CHANGED;
                comp_N_begin = metrics_time();
                policy_loop_init(&loop.id);
#if DYNAIS_CUTOFF
                if (state_ok(check_dynais_on(&loop_signature.signature, l_sig))) {
                    debug("Dynais ON");
                    dynais_enabled = DYNAIS_ENABLED;
                }
#endif
            } else {
                EAR_STATE = EVALUATING_LOCAL_SIGNATURE;
            }
            break;
        case PROJECTION_ERROR:
            verbose(2, "%s--- PROJECTION ERROR ---%s", COL_YLW, COL_CLR);
            if (masters_info.my_master_rank>=0 && traces_are_on()){
                /* We compute the signature just in case EAR_GUI is on TODO: wtf is EAR_GUI */ 
                if (((iterations - 1) % perf_count_period) ) return;
                /* We can compute the signature */
                N_iter = iterations - begin_iter;
                result = metrics_compute_signature_finish(&loop_signature.signature, N_iter, perf_accuracy_min_time, total_threads_cnt);
                if (result == EAR_NOT_READY) {
                    perf_count_period++;
                    break;
                }
                /* VERBOSE */
                signature_copy(&loop.signature, &loop_signature.signature);
                state_report_traces(masters_info.my_master_rank,ear_my_rank, my_id,&loop,prev_cpufreq_khz,EAR_STATE);
                state_verbose_signature(&loop,masters_info.my_master_rank,show_signatures,ear_app_name,application.node_id,iterations,prev_cpufreq_khz,0,"D-PE");
            }
            /* We run here at default freq */
            break;
        default: break;
    }
    // current_earl_state = EAR_STATE;
    state_report_traces_state(masters_info.my_master_rank,ear_my_rank, my_id,EAR_STATE);
    debug("End states_end_iteration");
}
