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

// #define SHOW_DEBUGS 1

#include <library/policies/common/generic.h>
#include <common/states.h>

extern volatile uint ear_periodic_mode;

double metric_ratio(double metric_ref, double metric_curr, uint type) {
    if (type) {
        return (metric_curr - metric_ref) / metric_ref;
    }
    else {
        return (metric_ref - metric_curr) / metric_ref;
    }
}

uint above_max_penalty(double time_ref, double time_curr, double cpi_ref, double cpi_curr,
        double gbs_ref, double gbs_curr, double penalty_th){
    double metric_max, metric_min;
    if (ear_periodic_mode == PERIODIC_MODE_OFF){
        /* We use TIME as our reference metric */
        metric_max = time_ref * (1 + penalty_th);
        return (time_curr > metric_max);
    }else{
        /* We use CPI and GBS our reference metrics */
        metric_max = cpi_ref * (1 + penalty_th);
        metric_min = gbs_ref * (1 - penalty_th);
        return ((cpi_curr > metric_max) || (gbs_curr < metric_min));
    }
}

uint below_perf_min_benefit(double time_ref, double time_curr, double cpi_ref, double cpi_curr,
        double gbs_ref, double gbs_curr, double penalty_th){
    return 1;
}
