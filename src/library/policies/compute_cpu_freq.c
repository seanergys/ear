#include <library/policies/compute_cpu_freq.h>
#include <library/common/verbose_lib.h>
#include <common/types/projection.h>
#include <management/cpufreq/frequency.h>

state_t compute_cpu_freq_min_time(signature_t *my_app, int min_pstate, double time_ref,
        double min_eff_gain, ulong curr_pstate, ulong best_pstate, ulong best_freq, ulong def_freq, ulong *newf){
    int i;
    uint try_next;
    double time_current, power_proj, time_proj, freq_gain, perf_gain, vpi;
    state_t st;
    ulong freq_ref;

    compute_vpi(&vpi,my_app);
    // error al compilar con verbose_master :( error: identifier "masters_info" is undefined
    verbose(3, "CPUfreq algorithm for min_time. timeref %lf; pstate %lu; F %lu; deffreq %lu; VPI %lf",
            time_ref, curr_pstate, best_freq, def_freq, vpi);

    try_next = 1;
    i = best_pstate - 1;
    time_current = time_ref;
    while(try_next && (i >= min_pstate))
    {
        if (projection_available(curr_pstate,i)==EAR_SUCCESS)
        {
            verbose(3, "Looking for pstate %d",i);
            st = project_power(my_app, curr_pstate, i, &power_proj);
            st = project_time(my_app, curr_pstate, i, &time_proj);
            projection_set(i, time_proj, power_proj);
            freq_ref = frequency_pstate_to_freq(i);
            freq_gain = min_eff_gain * (double)(freq_ref - best_freq) / (double)best_freq;
            perf_gain = (time_current - time_proj) / time_current;
            verbose(3, "%lu to %d time %lf proj time %lf freq gain %lf perf_gain %lf",curr_pstate,i,my_app->time, time_proj,freq_gain,perf_gain);
            // OK
            if (perf_gain>=freq_gain)
            {
                verbose(3, "New best solution found: Best freq: %lu; Best pstate: %d; Current time ref: %lf",
                        freq_ref, i, time_proj);
                best_freq=freq_ref;
                best_pstate=i;
                time_current = time_proj;
                i--;
            }
            else
            {
                try_next = 0;
            }
        } // Projections available
        else{
            try_next=0;
        }
    }
    /* Controlar la freq por power cap, si capado poner GREEDY, gestionar req-f */
    if (best_freq<def_freq) best_freq=def_freq;
    *newf=best_freq;
    verbose(3, "*new_freq=%lu", *newf);

    return EAR_SUCCESS;
}
