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
    else if (tp->vendor == VENDOR_ARM) { // && ARMv8a or greater) {
        // FIXED values works for float, double, but also for SIMD NEON. Which
        // is a pity because we wanted to get fine grained results. Its is
        // divided to match with the tests iterations.
        set_flops(0x80c3, PERF_TYPE_RAW, 0,                             1.0, "FP_HP_FIXED_OPS_SPEC");
        set_flops(0x80c5, PERF_TYPE_RAW, 0,                             1.0, "FP_SP_FIXED_OPS_SPEC");
        set_flops(0x80c7, PERF_TYPE_RAW, 1,                             1.0, "FP_DP_FIXED_OPS_SPEC");
        set_flops(0x80c2, PERF_TYPE_RAW, 6, ((double) tp->sve_bits) / 128.0, "FP_HP_SCALE_OPS_SPEC");
        set_flops(0x80c4, PERF_TYPE_RAW, 6, ((double) tp->sve_bits) / 128.0, "FP_SP_SCALE_OPS_SPEC");
        set_flops(0x80c6, PERF_TYPE_RAW, 7, ((double) tp->sve_bits) / 128.0, "FP_DP_SCALE_OPS_SPEC");

//#define MODEL_NEOVERSE_N1       0x41D0C
//#define MODEL_NEOVERSE_N2       0x41D4A
//#define MODEL_NEOVERSE_V1       0x41D40
//#define MODEL_FX1000            0x46001
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
    int i;

    // Cleaning flops_t structure
    memset(fl, 0, sizeof(flops_t));
    // Getting time
    timestamp_get(&fl->time);
    // Reading (and adding if we are joining two events in the same variable)
    for (i = 0; i < counter; ++i) {
        value = 0LU;
        perf_read(&meta[i].perf, (long long *) &value);
        debug("values[%d]: %llu", meta[i].offset, value);
        p[meta[i].offset] += value;
    }
    return EAR_SUCCESS;
}

static double compute_gflops(flops_t *flD)
{
    ullong *v = (ullong *) flD;
    double gfs = 0.0;
    int i;
    // Iterating over all values
    for (i = 0; i < counter; ++i) {
        gfs += ((double) v[i]) * meta[i].weight;
        debug("FLOPS[%d] += %llu * %0.2lf", meta[i].offset, v[i], meta[i].weight);
    }
    gfs = (gfs / flD->secs) / ((double) 1E9);
    debug("Total GFLOPS: %0.2lf", gfs);
    return gfs;
}

void flops_perf_data_diff(flops_t *fl2, flops_t *fl1, flops_t *flD, double *gfs)
{
    ullong time = 0LLU;
    flops_t flA; // flAux

    // Cleaning
    if (gfs != NULL) { *gfs = 0.0; }
    if (flD != NULL) { memset(flD, 0, sizeof(flops_t)); }
    // If is not looking for the difference
    if (fl2 == NULL && fl1 == NULL && flD != NULL && gfs != NULL) {
        *gfs = compute_gflops(flD);
        return;
    }
    // Reading milliseconds and converting to seconds for decimals
    if ((time = timestamp_diff(&fl2->time, &fl1->time, TIME_MSECS)) == 0LLU) {
        // If no time passed, then flops difference an Gigaflops are zero.
        return;
    }
    flA.secs = (double) time;
    flA.secs = flA.secs / 1000.0;
    // Instruction differences (PERF registers are 64 bits long)
    flA.f64  = overflow_magic_u64(fl2->f64 , fl1->f64 , MAXBITS64);
    flA.d64  = overflow_magic_u64(fl2->d64 , fl1->d64 , MAXBITS64);
    flA.f128 = overflow_magic_u64(fl2->f128, fl1->f128, MAXBITS64);
    flA.d128 = overflow_magic_u64(fl2->d128, fl1->d128, MAXBITS64);
    flA.f256 = overflow_magic_u64(fl2->f256, fl1->f256, MAXBITS64);
    flA.d256 = overflow_magic_u64(fl2->d256, fl1->d256, MAXBITS64);
    flA.f512 = overflow_magic_u64(fl2->f512, fl1->f512, MAXBITS64);
    flA.d512 = overflow_magic_u64(fl2->d512, fl1->d512, MAXBITS64);
    debug("Total f64  insts: %llu = %llu - %llu", flA.f64 , fl2->f64 , fl1->f64 );
	debug("Total d64  insts: %llu = %llu - %llu", flA.d64 , fl2->d64 , fl1->d64 );
	debug("Total f128 insts: %llu = %llu - %llu", flA.f128, fl2->f128, fl1->f128);
	debug("Total d128 insts: %llu = %llu - %llu", flA.d128, fl2->d128, fl1->d128);
	debug("Total f256 insts: %llu = %llu - %llu", flA.f256, fl2->f256, fl1->f256);
	debug("Total d256 insts: %llu = %llu - %llu", flA.d256, fl2->d256, fl1->d256);
	debug("Total f512 insts: %llu = %llu - %llu", flA.f512, fl2->f512, fl1->f512);
	debug("Total d512 insts: %llu = %llu - %llu", flA.d512, fl2->d512, fl1->d512);
    // Copying data
    if (gfs != NULL) { *gfs = compute_gflops(&flA); }
    if (flD != NULL) { memcpy(flD, &flA, sizeof(flops_t)); }
}