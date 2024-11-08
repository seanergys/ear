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

#define _GNU_SOURCE
#include <stdio.h>
#include <sched.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <common/config.h>
#include <common/output/debug.h>
#include <common/utils/string.h>
#include <common/config/config_env.h>
#include <library/loader/loader.h>
#include <library/loader/module_mpi.h>
#include <library/loader/module_common.h>

static int init()
{
    char *verb;
    char *task;
    pid_t pid1 = 1;
    pid_t pid2 = 0;

    // Activating verbose
    if ((verb = ear_getenv(FLAG_LOADER_VERBOSE)) != NULL) {
        VERB_SET_EN(1);
        VERB_SET_LV(atoi(verb));
    }
    pid1 = getpid();

    if (SCHED_IS(SCHED_SLURM)) {
        // SLURM prolog/epilog control
        if ((task = ear_getenv(FLAG_TASK_PID)) == NULL) {
            return 0;
        }
        pid2 = (pid_t) atoi(task);
        verbose(4, "LOADER: Loader pids: %d/%d", pid1, pid2);
    } else {
        verbose(4, "LOADER: Loader pid: %d", pid1);
    }
    return 1;
    // return pid1 == pid2;
}

int is_not_an_invalid_program()
{
    if ((strstr(program_invocation_name, "bash") == NULL) &&
        (strstr(program_invocation_name, "hydra") == NULL)) {
        verbose(2, "LOADER: Program %s loaded with EAR", program_invocation_name);
        return 1;
    }
    verbose(2, "LOADER: Program %s loaded without EAR", program_invocation_name);
    return 0;
}

int forced_load()
{
    char  *envar      = NULL;
    char **list       = NULL;
    uint   list_count = 0;
    int    l          = 0;

    if ((envar = ear_getenv(FLAG_LOADER_APPLICATION)) == NULL) {
        return 0;
    }
    verbose(2, "LOADER: Using %s = %s as special cases", FLAG_LOADER_APPLICATION, envar);
    if (strtoa((const char *) envar, ',', &list, &list_count) == NULL) {
        return 0;
    }
    for (l = 0; l < list_count; ++l) {
        if (strstr(program_invocation_name, list[l]) != NULL) {
            strtoa_free(list);
            return 1;
        }
    }
    strtoa_free(list);
    return 0;
}

void __attribute__ ((constructor)) loader()
{
    char *syms_openmp[] = {"omp_get_max_threads", "mkl_get_max_threads", NULL};
    char *syms_cuda[]   = {"cudaRuntimeGetVersion", NULL};
    char *libs_oneapi[] = {"libsycl.so", NULL};

    // The old load_no_parallel function is not required any more because the
    // new loader is compatible with Python and other scripting languages. But
    // the non-MPI version of the library turns incompatible with this loader
    // since it is loaded before the scripting languages load the MPI library
    // and call MPI_Init, which is done during the execution, and in the
    // constructor these symbols are not present yet.
    //
    // To solve this, we define FLAG_LOAD_MPI environment variable with any
    // value to tell the loader we want to use the MPI module. When that flag
    // is defined, the MPI module returns 1 even if MPI is not loaded yet,
    // preventing other modules to load the EAR library.
    if (!init()) {
        return;
    }
    if (is_not_an_invalid_program()) {
        if (load_libear_mpi()) { return; }
        if (load_libear_nonspec(OPENMP_EXT, syms_openmp, NULL       )) { return; }
        if (load_libear_nonspec(  CUDA_EXT, syms_cuda  , NULL       )) { return; }
        if (load_libear_nonspec(ONEAPI_EXT, NULL       , libs_oneapi)) { return; }
        if (!forced_load()) { return; }
        if (load_libear_nonspec(   DEF_EXT, NULL       , NULL       )) { return; }
    }
    return;
}
