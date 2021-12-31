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

#include <common/system/time.h>
#include <common/output/debug.h>
#include <common/math_operations.h>
#include <metrics/common/perf.h>
#include <metrics/flops/archs/dummy.h>

state_t flops_dummy_load(topology_t *tp, flops_ops_t *ops)
{
	replace_ops(ops->init,       flops_dummy_init);
	replace_ops(ops->dispose,    flops_dummy_dispose);
	replace_ops(ops->read,       flops_dummy_read);
	replace_ops(ops->data_diff,  flops_dummy_data_diff);
	replace_ops(ops->data_copy,  flops_dummy_data_copy);
	replace_ops(ops->data_print, flops_dummy_data_print);
	replace_ops(ops->data_tostr, flops_dummy_data_tostr);
	return EAR_SUCCESS;
}

state_t flops_dummy_init(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t flops_dummy_dispose(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t flops_dummy_read(ctx_t *c, flops_t *fl)
{
	memset(fl, 0, sizeof(flops_t));
	return EAR_SUCCESS;
}

// Helpers
state_t flops_dummy_data_diff(flops_t *fl2, flops_t *fl1, flops_t *flD, double *gfs)
{
	static ullong weights[8] = { 1, 1, 4, 2, 8, 4, 16, 8 };
	ullong diff = 0LLU;
	ullong time = 0LLU;
	double secs = 0.0;
	int i;

	*gfs = 0.0;
	memset(flD, 0, sizeof(flops_t));
	// Ms to seconds (for decimals)
	if ((time = timestamp_diff(&fl2->time, &fl1->time, TIME_MSECS)) == 0LLU) {
		return EAR_SUCCESS;
	}
	secs = (double) time;
	secs = secs / 1000.0;
	// Instruction differences
	flD->f64  = overflow_zeros_u64(fl2->f64 , fl1->f64);
	flD->d64  = overflow_zeros_u64(fl2->d64 , fl1->d64);
	flD->f128 = overflow_zeros_u64(fl2->f128, fl1->f128);
	flD->d128 = overflow_zeros_u64(fl2->d128, fl1->d128);
	flD->f256 = overflow_zeros_u64(fl2->f256, fl1->f256);
	flD->d256 = overflow_zeros_u64(fl2->d256, fl1->d256);
	flD->f512 = overflow_zeros_u64(fl2->f512, fl1->f512);
	flD->d512 = overflow_zeros_u64(fl2->d512, fl1->d512);
	// Giga floating point instructions per second
	*gfs += (double) (flD->f64  * weights[0]);
	*gfs += (double) (flD->d64  * weights[1]);
	*gfs += (double) (flD->f128 * weights[2]);
	*gfs += (double) (flD->d128 * weights[3]);
	*gfs += (double) (flD->f256 * weights[4]);
	*gfs += (double) (flD->d256 * weights[5]);
	*gfs += (double) (flD->f512 * weights[6]);
	*gfs += (double) (flD->d512 * weights[7]);
	*gfs = (*gfs / secs);
	*gfs = (*gfs / B_TO_GB);
	//
	debug("Total f64  insts: %llu", flD->f64);
	debug("Total d64  insts: %llu", flD->d64);
	debug("Total f128 insts: %llu", flD->f128);
	debug("Total d128 insts: %llu", flD->d128);
	debug("Total f256 insts: %llu", flD->f256);
	debug("Total d256 insts: %llu", flD->d256);
	debug("Total f512 insts: %llu", flD->f512);
	debug("Total d512 insts: %llu", flD->d512);
	debug("Total GFLOPS: %0.2lf", *gfs);
	return EAR_SUCCESS;
}

state_t flops_dummy_data_copy(flops_t *dst, flops_t *src)
{
	memcpy(dst, src, sizeof(flops_t));
	return EAR_SUCCESS;
}

state_t flops_dummy_data_print(flops_t *flD, double gfs, int fd)
{
	char buffer[SZ_BUFFER];
	if (state_ok(flops_dummy_data_tostr(flD, gfs, buffer, SZ_BUFFER))) {
		dprintf(fd, "%s", buffer);
	}
	return EAR_SUCCESS;
}

state_t flops_dummy_data_tostr(flops_t *flD, double gfs, char *buffer, size_t length)
{
	int added = 0;
	added += sprintf(&buffer[added], "Total  64 insts f-d: %llu %llu\n", flD->f64 , flD->d64);
	added += sprintf(&buffer[added], "Total 128 insts f-d: %llu %llu\n", flD->f128, flD->d128);
	added += sprintf(&buffer[added], "Total 256 insts f-d: %llu %llu\n", flD->f256, flD->d256);
	added += sprintf(&buffer[added], "Total 512 insts f-d: %llu %llu\n", flD->f512, flD->d512);
	added += sprintf(&buffer[added], "Total GFLOPS: %0.2lf\n", gfs);
	return EAR_SUCCESS;
}
