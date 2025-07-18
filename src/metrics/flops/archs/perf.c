/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

// #define SHOW_DEBUGS 1

#include <common/system/time.h>
#include <common/output/debug.h>
#include <common/math_operations.h>
#include <metrics/common/perf.h>
#include <metrics/flops/archs/perf.h>

typedef struct fperf_s {
    perf_t perf;
    int    offset;
    double weight;
    int    enabled;
    cchar *desc;
} fperf_t;

static fperf_t fperf[8];
static uint    fperf_count;
static uint    enableds_count;

static void set_flops(ullong event, uint type, int offset, double weight, cchar *desc)
{
    fperf[fperf_count].offset  = offset;
    fperf[fperf_count].weight  = weight;
    fperf[fperf_count].desc    = desc;
    fperf[fperf_count].enabled = 1;
    // Opening perf event (some events might be accepted but don't work)
    if (state_fail(perf_open(&fperf[fperf_count].perf, NULL, 0, type, (ulong) event))) {
        debug("perf_open returned: %s", state_msg);
        fperf[fperf_count].enabled = 0;
    } else {
        enableds_count++;
    }
    fperf_count++;
}

FLOPS_F_LOAD(perf)
{
    int i;

    if (tp->vendor == VENDOR_INTEL && tp->model >= MODEL_HASWELL_X) {
        set_flops(0x02c7, PERF_TYPE_RAW, 0,  1.0, "FP_ARITH_INST_RETIRED.SCALAR_SINGLE     ");
        set_flops(0x01c7, PERF_TYPE_RAW, 1,  1.0, "FP_ARITH_INST_RETIRED.SCALAR_DOUBLE     ");
        set_flops(0x08c7, PERF_TYPE_RAW, 2,  4.0, "FP_ARITH_INST_RETIRED.128B_PACKED_SINGLE");
        set_flops(0x04c7, PERF_TYPE_RAW, 3,  2.0, "FP_ARITH_INST_RETIRED.128B_PACKED_DOUBLE");
        set_flops(0x20c7, PERF_TYPE_RAW, 4,  8.0, "FP_ARITH_INST_RETIRED.256B_PACKED_SINGLE");
        set_flops(0x10c7, PERF_TYPE_RAW, 5,  4.0, "FP_ARITH_INST_RETIRED.256B_PACKED_DOUBLE");
        set_flops(0x80c7, PERF_TYPE_RAW, 6, 16.0, "FP_ARITH_INST_RETIRED.512B_PACKED_SINGLE");
        set_flops(0x40c7, PERF_TYPE_RAW, 7,  8.0, "FP_ARITH_INST_RETIRED.512B_PACKED_DOUBLE");
    } else if (tp->vendor == VENDOR_AMD && tp->family >= FAMILY_ZEN) {
        if (tp->family >= FAMILY_ZEN5) {
            // This ZEN5 register contains type selection (float and double). 0h
            // means all types, but this can be reworked next to Retired_FP_uOps
            // to count 512, 256 and 128 length uops.
            set_flops(0x0f03, PERF_TYPE_RAW, 4, 1.0, "Retired_SSE_AVX_FLOPs");
        } else {
            // In theory, this event counts 128 and 256 bits operations from SSE and
            // AVX FLOPs. But it can not distinguish between types. We are saving
            // the register readings in 256f by pure arbitrariness.
            set_flops(0xff03, PERF_TYPE_RAW, 4, 1.0, "FpRetSseAvxOps");
        }
    } else if (tp->vendor == VENDOR_ARM && tp->model == MODEL_FX1000) {
        // We can mix these offsets because share the same weight. FIXED events
        // combines floats, doubles and also SIMD NEON.
        set_flops(0x80c3, PERF_TYPE_RAW, 2, 1.0, "FP_HP_FIXED_OPS_SPEC");
        set_flops(0x80c5, PERF_TYPE_RAW, 2, 1.0, "FP_SP_FIXED_OPS_SPEC");
        set_flops(0x80c7, PERF_TYPE_RAW, 3, 1.0, "FP_DP_FIXED_OPS_SPEC");
        set_flops(0x80c2, PERF_TYPE_RAW, 6, ((double) tp->sve_bits) / 128.0, "FP_HP_SCALE_OPS_SPEC");
        set_flops(0x80c4, PERF_TYPE_RAW, 6, ((double) tp->sve_bits) / 128.0, "FP_SP_SCALE_OPS_SPEC");
        set_flops(0x80c6, PERF_TYPE_RAW, 7, ((double) tp->sve_bits) / 128.0, "FP_DP_SCALE_OPS_SPEC");
    } else if (tp->vendor == VENDOR_ARM && tp->sve && tp->model == MODEL_NEOVERSE_V2) {
        // FIXED also counts VFP_SPEC operations.
        set_flops(0x80C1, PERF_TYPE_RAW, 2, 1.0, "FP_FIXED_OPS_SPEC");
        set_flops(0x80c0, PERF_TYPE_RAW, 6, ((double) tp->sve_bits) / 128.0, "FP_SCALE_OPS_SPEC");
    } else if (tp->vendor == VENDOR_ARM && tp->sve && tp->model != MODEL_NEOVERSE_V2) {
        // FIXED also counts VFP_SPEC operations.
        set_flops(0x80c3, PERF_TYPE_RAW, 2, 1.0, "FP_FIXED_OPS_SPEC");
        set_flops(0x80c2, PERF_TYPE_RAW, 6, ((double) tp->sve_bits) / 128.0, "FP_SCALE_OPS_SPEC");
    } else if (tp->vendor == VENDOR_ARM) {
        // ASE_SPEC is multiplied by x4 because we are counting float32x4_t, the
        // mean value of float16x8_t, float32x4_t and float64x2_t.
        set_flops(0x0075, PERF_TYPE_RAW, 0, 1.0, "VFP_SPEC");
        set_flops(0x0074, PERF_TYPE_RAW, 2, 4.0, "ASE_SPEC");
    } else {
        debug("%s", Generr.api_incompatible);
        return;
    }
    if (!enableds_count) {
        return;
    }
    // Old init
    for (i = 0; i < fperf_count; ++i) {
        if (fperf[i].enabled) {
            perf_start(&fperf[i].perf);
        }
    }
    apis_put(ops->unload   , flops_perf_unload);
    apis_put(ops->get_info , flops_perf_get_info);
    apis_put(ops->read     , flops_perf_read);
    apis_put(ops->data_diff, flops_perf_data_diff);
    apis_put(ops->internals_tostr, flops_perf_internals_tostr);
    debug("Loaded PERF");
}

FLOPS_F_UNLOAD(perf)
{
    uint i;
    for (i = 0; i < fperf_count; ++i) {
        perf_close(&fperf[i].perf);
    }
    fperf_count = 0;
    enableds_count = 0;
}

FLOPS_F_GET_INFO(perf)
{
    info->api         = API_PERF;
    info->scope       = SCOPE_PROCESS;
    info->granularity = GRANULARITY_PROCESS;
    info->devs_count  = 1;
    info->bits        = 64;
}

FLOPS_F_READ(perf)
{
    ullong *p = (ullong *) fl;
    ullong value;
    state_t s;
    int i;

    // Cleaning flops_t structure
    memset(fl, 0, sizeof(flops_t));
    // Getting time
    timestamp_get(&fl->time);
    // Reading
    for (i = 0; i < fperf_count; ++i) {
        if (fperf[i].enabled) {
            p[i] = value = 0LLU;
            if (state_ok(s = perf_read(&fperf[i].perf, (long long *) &value))) {
                p[i] = value;
            }
            debug("values[%d]: %llu (state %d)", fperf[i].offset, value, s);
        }
    }
    return EAR_SUCCESS;
}

FLOPS_F_DATA_DIFF(perf)
{
    flops_t flA; // flAux (holds the temporary flops_t)
    flops_t flB; // flAux (holds the final flops_t)
    ullong *pA             = (ullong *) &flA;
    ullong *pB             = (ullong *) &flB;
    ATTR_UNUSED ullong *pD = (ullong *) flD;
    ullong *p2             = (ullong *) fl2;
    ullong *p1             = (ullong *) fl1;
    ullong time            = 0LLU;
    int i;

    // Cleaning
    memset(&flA, 0, sizeof(flops_t));
    memset(&flB, 0, sizeof(flops_t));
    if (gfs != NULL) {
        *gfs = 0.0;
    }
    if (flD != NULL) {
        memset(flD, 0, sizeof(flops_t));
    }
    // Reading milliseconds and converting to seconds for decimals
    if ((time = timestamp_diff(&fl2->time, &fl1->time, TIME_MSECS)) == 0LLU) {
        // If no time passed, then flops difference an Gigaflops are zero.
        return;
    }
    flB.secs = (double) time;
    flB.secs = flB.secs / 1000.0;
    // Instruction differences (PERF registers are 64 bits long)
    for (i = 0; i < fperf_count; ++i) {
        if (fperf[i].enabled) {
            // Is overflow_zero because sometimes values at time X are under
            // values al time Y, being X greater than Y. This is because of
            // perf's multiplexing process.
            pA[i] = overflow_zeros_u64(p2[i], p1[i]);
            debug("values[%d->%d]: %llu - %llu = %llu",
                i, fperf[i].offset, p2[i], p1[i], pA[i]);
        }
    }
    // Sorting and computing GFLOPs
    for (i = 0; i < fperf_count; ++i) {
        pB[fperf[i].offset] += pA[i] * ((ullong) fperf[i].weight);
        debug("FLOPS[%d->%d] += %llu * %0.2lf", i, fperf[i].offset, pA[i], fperf[i].weight);
        flB.gflops += ((double) pA[i]) * fperf[i].weight;
    }
    flB.gflops = (flB.gflops / flB.secs) / ((double) 1E9);
    debug("Total GFLOPS: %0.2lf", flB.gflops);
    // Debugging
    debug("Total f64  insts: %llu", flB.f64);
    debug("Total d64  insts: %llu", flB.d64);
    debug("Total f128 insts: %llu", flB.f128);
    debug("Total d128 insts: %llu", flB.d128);
    debug("Total f256 insts: %llu", flB.f256);
    debug("Total d256 insts: %llu", flB.d256);
    debug("Total f512 insts: %llu", flB.f512);
    debug("Total d512 insts: %llu (%0.2lf s)", flB.d512, flB.secs);
    // Copying data
    if (flD != NULL) {
        memcpy(flD, &flB, sizeof(flops_t));
    }
    if (gfs != NULL) {
        *gfs = flB.gflops;
    }
}

FLOPS_F_INTERNALS_TOSTR(perf)
{
    cchar *offstr[] = {"F64 ", "D64 ", "F128", "D128", "F256", "D256", "F512", "D512"};
    cchar *state[]  = {"failed", "loaded"};
    int i, c;
    for (i = c = 0; i < fperf_count; ++i) {
        c += sprintf(&buffer[c], "FLOPS %s: %s event %s * %-2.2lf\n",
            offstr[fperf[i].offset], state[fperf[i].enabled], fperf[i].desc, fperf[i].weight);
    }
}