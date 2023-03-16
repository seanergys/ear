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
* EAR is an open source software, and it is licensed under both the BSD-3 license
* and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
* and COPYING.EPL files.
*/

#define SHOW_DEBUGS 1

#include <common/system/time.h>
#include <common/output/debug.h>
#include <common/math_operations.h>
#include <metrics/common/perf.h>
#include <metrics/flops/archs/perf.h>

typedef struct fmeta_s {
    perf_t perf;
    int    offset;
    double weight;
    int    enabled;
    cchar  *desc;
} fmeta_t;

static fmeta_t meta[8];
static uint    counter;
static uint    started;

void set_flops(ullong event, uint type, int offset, double weight, cchar *desc)
{
    meta[counter].offset      = offset;
    meta[counter].weight      = weight;
    meta[counter].desc        = desc;
    meta[counter].enabled     = 1;
    // Opening perf event (some events might be accepted but don't work)
    if (state_fail(perf_open(&meta[counter].perf, NULL, 0, type, (ulong) event))) {
        debug("perf_open returned: %s", state_msg);
        meta[counter].enabled = 0;
    }
    counter++;
}

void flops_perf_load(topology_t *tp, flops_ops_t *ops)
{
    if (tp->vendor == VENDOR_INTEL && tp->model >= MODEL_HASWELL_X) {
        set_flops(0x02c7, PERF_TYPE_RAW, 0,  1.0, "FP_ARITH_INST_RETIRED.SCALAR_SINGLE");
        set_flops(0x01c7, PERF_TYPE_RAW, 1,  1.0, "FP_ARITH_INST_RETIRED.SCALAR_DOUBLE");
        set_flops(0x08c7, PERF_TYPE_RAW, 2,  4.0, "FP_ARITH_INST_RETIRED.128B_PACKED_SINGLE");
        set_flops(0x04c7, PERF_TYPE_RAW, 3,  2.0, "FP_ARITH_INST_RETIRED.128B_PACKED_DOUBLE");
	set_flops(0x20c7, PERF_TYPE_RAW, 4,  8.0, "FP_ARITH_INST_RETIRED.256B_PACKED_SINGLE");
        set_flops(0x10c7, PERF_TYPE_RAW, 5,  4.0, "FP_ARITH_INST_RETIRED.256B_PACKED_DOUBLE");
	set_flops(0x80c7, PERF_TYPE_RAW, 6, 16.0, "FP_ARITH_INST_RETIRED.512B_PACKED_SINGLE");
	set_flops(0x40c7, PERF_TYPE_RAW, 7,  8.0, "FP_ARITH_INST_RETIRED.512B_PACKED_DOUBLE");
    }
    else if (tp->vendor == VENDOR_AMD && tp->family >= FAMILY_ZEN) {
        // In theory, this event counts 128 and 256 bits operations from SSE and
        // AVX FLOPs. But it can not distinguish between types. We are saving
        // the register readings in 256f by pure arbitrariness.
        set_flops(0xff03, PERF_TYPE_RAW, 4, 1.0, "FpRetSseAvxOps");
    }
    else if (tp->vendor == VENDOR_ARM && tp->model == MODEL_FX1000) {
        // We can mix these offsets because share the same weight. FIXED events
        // combines floats, doubles and also SIMD NEON.
        set_flops(0x80c3, PERF_TYPE_RAW, 2,                             1.0, "FP_HP_FIXED_OPS_SPEC");
        set_flops(0x80c5, PERF_TYPE_RAW, 2,                             1.0, "FP_SP_FIXED_OPS_SPEC");
        set_flops(0x80c7, PERF_TYPE_RAW, 3,                             1.0, "FP_DP_FIXED_OPS_SPEC");
        set_flops(0x80c2, PERF_TYPE_RAW, 6, ((double) tp->sve_bits) / 128.0, "FP_HP_SCALE_OPS_SPEC");
        set_flops(0x80c4, PERF_TYPE_RAW, 6, ((double) tp->sve_bits) / 128.0, "FP_SP_SCALE_OPS_SPEC");
        set_flops(0x80c6, PERF_TYPE_RAW, 7, ((double) tp->sve_bits) / 128.0, "FP_DP_SCALE_OPS_SPEC");
    }
    else if (tp->vendor == VENDOR_ARM && tp->sve) {
        // FIXED also counts VFP_SPEC operations.
        set_flops(0x80c3, PERF_TYPE_RAW, 2,                             1.0, "FP_FIXED_OPS_SPEC");
        set_flops(0x80c2, PERF_TYPE_RAW, 6, ((double) tp->sve_bits) / 128.0, "FP_SCALE_OPS_SPEC");
    }
    else if (tp->vendor == VENDOR_ARM) {
        // ASE_SPEC is multiplied by x4 because we are counting float32x4_t, the
        // mean value of float16x8_t, float32x4_t and float64x2_t.
        set_flops(0x0075, PERF_TYPE_RAW, 0,  1.0, "VFP_SPEC");
        set_flops(0x0074, PERF_TYPE_RAW, 2,  4.0, "ASE_SPEC");
    } else {
	debug("%s", Generr.api_incompatible);
        return;
    }
    if (!counter) {
        return;
    }
    apis_put(ops->get_info,        flops_perf_get_info);
    apis_put(ops->init,            flops_perf_init);
    apis_put(ops->dispose,         flops_perf_dispose);
    apis_put(ops->read,            flops_perf_read);
    apis_put(ops->data_diff,       flops_perf_data_diff);
    apis_put(ops->internals_tostr, flops_perf_internals_tostr);
    debug("Loaded PERF");
}

void flops_perf_get_info(apinfo_t *info)
{
    info->api         = API_PERF;
    info->scope       = SCOPE_PROCESS;
    info->granularity = GRANULARITY_PROCESS;
    info->devs_count  = 1;
    info->bits        = 64;
}

state_t flops_perf_init(ctx_t *c)
{
	uint i;
	if (!started) {
		for (i = 0; i < counter; ++i) {
            if (meta[i].enabled) {
			    perf_start(&meta[i].perf);
            }
		}
		++started;
	}
	return EAR_SUCCESS;
}

state_t flops_perf_dispose(ctx_t *c)
{
    return EAR_SUCCESS;
}

state_t flops_perf_read(flops_t *fl)
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
    for (i = 0; i < counter; ++i) {
        if (meta[i].enabled) {
            p[i] = value = 0LLU;
            if (state_ok(s = perf_read(&meta[i].perf, (long long *) &value))) {
                p[i] = value;
            }
            debug("values[%d]: %llu (state %d)", meta[i].offset, value, s);
        }
    }
    return EAR_SUCCESS;
}

static void compute_gflops(flops_t *flD, double *gfs, int old_style)
{
    ullong *v = (ullong *) flD;
    double agfs = 0.0;
    int i;

    // Iterating over all values
    for (i = 0; i < counter; ++i) {
        // Normal style
        if (!old_style) {
            agfs += ((double) v[i]) * meta[i].weight;
            debug("FLOPS[%d->%d] += %llu * %0.2lf", i, meta[i].offset, v[i], meta[i].weight);
        } else {
            // In the old style they are asking you for the flops,
            // but also the individual ops are computed and saved.
            agfs += ((double) v[meta[i].offset]) * meta[i].weight;
            v[meta[i].offset] = v[meta[i].offset] * meta[i].weight; 
        }
    }
    agfs = (agfs / flD->secs) / ((double) 1E9);
    debug("Total GFLOPS: %0.2lf", agfs);
    if (gfs != NULL) {
        *gfs = agfs;
    }
}

void flops_perf_data_diff(flops_t *fl2, flops_t *fl1, flops_t *flD, double *gfs)
{
    flops_t flA; // flAux
    flops_t flB; // flAux
    ullong *pA = (ullong *) &flA;
    ullong *pB = (ullong *) &flB;
    ullong *pD = (ullong *)  flD;
    ullong *p2 = (ullong *)  fl2;
    ullong *p1 = (ullong *)  fl1;
    ullong time = 0LLU;
    int i;

    // If is not looking for the difference, just to compute de gflops,
    // and the data is already sorted. The old style.
    if (fl2 == NULL && fl1 == NULL && flD != NULL) {
        return compute_gflops(flD, gfs, 1);
    }
    // Cleaning
    if (gfs != NULL) { *gfs = 0.0; }
    if (flD != NULL) { memset(flD, 0, sizeof(flops_t)); }
    memset(&flA, 0, sizeof(flops_t));
    memset(&flB, 0, sizeof(flops_t));
    // Reading milliseconds and converting to seconds for decimals
    if ((time = timestamp_diff(&fl2->time, &fl1->time, TIME_MSECS)) == 0LLU) {
        // If no time passed, then flops difference an Gigaflops are zero.
        return;
    }
    flA.secs = flB.secs = (double) time;
    flA.secs = flB.secs = flA.secs / 1000.0;
    // Instruction differences (PERF registers are 64 bits long)
    for (i = 0; i < counter; ++i) {
        if (meta[i].enabled) {
            // Is overflow_zero because sometimes values at time X are under
            // values al time Y, being X greater than Y. This is because perf
            // multiplexing process.
            pA[i] = overflow_zeros_u64(p2[i], p1[i]);
            debug("values[%d->%d]: %llu = %llu - %llu", i, meta[i].offset, pA[i], p2[i], p1[i]);
        }
    }
    // Computing GFLOPs
    if (gfs != NULL) {
        compute_gflops(&flA, gfs, 0);
    }
    // Sorting values at the end, and adding values of the same offset
    for (i = 0; i < counter; ++i) {
       pB[meta[i].offset] += pA[i]; 
    }
    debug("Total f64  insts: %llu", flB.f64 );
    debug("Total d64  insts: %llu", flB.d64 );
    debug("Total f128 insts: %llu", flB.f128);
    debug("Total d128 insts: %llu", flB.d128);
    debug("Total f256 insts: %llu", flB.f256);
    debug("Total d256 insts: %llu", flB.d256);
    debug("Total f512 insts: %llu", flB.f512);
    debug("Total d512 insts: %llu (%0.2lf s)", flB.d512, flB.secs);
    // Copying data
    if (flD != NULL) { memcpy(flD, &flB, sizeof(flops_t)); }
}

void flops_perf_internals_tostr(char *buffer, int length)
{
    cchar *offstr[] = { "F64 ", "D64 ", "F128", "D128", "F256", "D256", "F512", "D512" };
    cchar *state[] = { "failed", "loaded" };
    int i, c;
    for (i = c = 0; i < counter; ++i) {
        c += sprintf(&buffer[c], "FLOPS %s: %s event %s * %-2.2lf\n", offstr[meta[i].offset],
                     state[meta[i].enabled], meta[i].desc, meta[i].weight);
    }
}
