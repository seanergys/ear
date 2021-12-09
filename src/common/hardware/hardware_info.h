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

#ifndef _HW_INFO_H_
#define _HW_INFO_H_

#define _GNU_SOURCE
#include <sched.h>
#include <common/sizes.h>
#include <common/states.h>
#include <common/system/file.h>
#include <common/types/generic.h>
#include <common/hardware/cpuid.h>
#include <common/hardware/topology.h>

/** Returns 1 if the CPU is APERF/MPERF compatible. */
int is_aperf_compatible();

/** Returns an EAR architecture index (top). */
int get_model();

/** Returns true if turbo is enabled */
int is_cpu_boost_enabled();

/** Sets all bits of `mask` to one. */
state_t set_mask_all_ones(cpu_set_t *mask);

/** Returns the number of packages detected*/
int detect_packages(int **package_map);

/** Computes the number of cpus in a given mask */
uint num_cpus_in_mask(cpu_set_t *my_mask);

/* Add cpus set in src to dst */
void aggregate_cpus_to_mask(cpu_set_t *dst, cpu_set_t *src);

/* Removes CPUs set in src from dst */
void remove_cpus_from_mask(cpu_set_t *dst, cpu_set_t *src);


/* Sets in dst CPUs not in src */
void cpus_not_in_mask(cpu_set_t *dst, cpu_set_t *src);

/** Prints the affinity mask of the current process */
void print_affinity_mask(topology_t *topo);

/** Prints the affinity mask passed by argument. */
state_t print_this_affinity_mask(cpu_set_t *mask);

/** Checks if process with pid=pid has some cpu forbiden to run , then is_set is set to 1 */
state_t is_affinity_set(topology_t *topo,int pid,int *is_set,cpu_set_t *my_mask);

/** dest results from doing the OR between dest and src. cpu_count is used */
state_t add_affinity(topology_t *topo, cpu_set_t *dest,cpu_set_t *src);


#endif
