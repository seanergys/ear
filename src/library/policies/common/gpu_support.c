/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/


#define SHOW_DEBUGS 1


#define _GNU_SOURCE
#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <common/includes.h>
#include <common/config.h>
#include <common/system/symplug.h>
#include <common/sizes.h>
#include <library/policies/policy_ctx.h>
#include <library/common/externs.h>
#include <library/common/verbose_lib.h>
#include <library/common/utils.h>
#include <library/metrics/metrics.h>


#ifndef GPU_OPT
#define GPU_OPT 0
#endif

extern const int     polsyms_n;
extern const char     *polsyms_nam[];


static void set_gpu_monitoring_plugin(char **path, size_t path_size);


state_t policy_gpu_load(settings_conf_t *app_settings, polsym_t *psyms)
{
	if (masters_info.my_master_rank < 0)
	{
		return EAR_SUCCESS;
	}
#if USE_GPUS
	pid_t pid = getpid();
	pid_t ppid = getppid();

	debug("PID: %d. PPID: %d", pid, ppid);

	int read_env = (app_settings->user_type == AUTHORIZED || USER_TEST);
	// Hack env var overwrites the entire path
	char *hack_gpu_policy_plugin = (read_env) ? ear_getenv(HACK_GPU_POWER_POLICY) : NULL;

	size_t gpupolicy_objc_size = SZ_PATH;
	char *gpupolicy_objc = (char *) malloc(gpupolicy_objc_size);
	memset(gpupolicy_objc, 0, gpupolicy_objc_size);

	// If hack, then the energy plugin is an entire path, otherwise is a shared object file.
	if (hack_gpu_policy_plugin)
	{
		debug("GPU policy hacked via path: %s", hack_gpu_policy_plugin);
		strncpy(gpupolicy_objc, hack_gpu_policy_plugin, gpupolicy_objc_size - 1);
		debug("GPU policy object: %s", gpupolicy_objc);
	} else if (app_settings->policy_name)
	{
		char gpupolicy_name[SZ_FILENAME];

		verbose_info2_master("GPU optimization %s. GPU policy plug-in base name: %s",
				(GPU_OPT) ? "enabled" : "disabled", (GPU_OPT) ? app_settings->policy_name : "monitoring");

		xsnprintf(gpupolicy_name, sizeof(gpupolicy_name), "gpu_%s.so",
				(GPU_OPT) ? app_settings->policy_name : "monitoring");

		char my_plug_path[SZ_PATH_INCOMPLETE];
		if (state_fail(utils_create_plugin_path(my_plug_path, app_settings->installation.dir_plug,
																						ear_getenv(HACK_EARL_INSTALL_PATH), app_settings->user_type)))
		{
			free(gpupolicy_objc);
				return_msg(EAR_ERROR, "Plugins path can not be built.");
		}

		xsnprintf(gpupolicy_objc, gpupolicy_objc_size - 1, "%s/policies/%s", my_plug_path, gpupolicy_name);
	} else
	{
	 	free(gpupolicy_objc);
			return_msg(EAR_NOT_FOUND, "There is no gpu policy plugin configured.");
	}

	// Replace the policy plug-in name ny gpu_monitoring in case FLAG_GPU_MASTER_OPTIMIZE
	// is defined to 0. This is implemented to avoid multiprocess applications where all processes
	// are masters an all of them see all GPU devices.

	char *gpu_master_opt_str = ear_getenv(FLAG_GPU_MASTER_OPTIMIZE);
	if (gpu_master_opt_str)
	{
		debug("GPU Master optimize defined: %s", gpu_master_opt_str);
		long gpu_master_opt = strtol(gpu_master_opt_str, NULL, 10);
		if (!gpu_master_opt)
		{
			debug("[%d] Forcing GPU monitoring policy...", pid);
			set_gpu_monitoring_plugin(&gpupolicy_objc, gpupolicy_objc_size);
		}
	} else
	{
		debug("GPU Master optimize not defined.");
	}

	verbose_info2_master("[%d] Loading GPU policy plug-in: %s", pid, gpupolicy_objc);
	if (state_fail(symplug_open(gpupolicy_objc, (void **) psyms, polsyms_nam, polsyms_n)))
	{
		verbose_error_master("Loading file %s (%s)", gpupolicy_objc, state_msg);
		free(gpupolicy_objc);
		return EAR_ERROR;
	}
	free(gpupolicy_objc);
#endif
	return EAR_SUCCESS;
}


static void set_gpu_monitoring_plugin(char **path, size_t path_size)
{
	uint elem_cnt = 1;
	debug("Original path: %s", *path);
	char **policy_path = strtoa(*path, '/', NULL, &elem_cnt);

	// If we are already loading gpu_monitoring.so, we can skip below code
	if (strcmp(policy_path[elem_cnt - 1], "gpu_monitoring.so")) {

		char *aux_path = (char *) malloc(path_size);
		memset(aux_path, 0, path_size);
		snprintf(aux_path, path_size, "/");

		// Concat all path entries, except the last one, i.e., the policy plugin
		for (int i = 0; i < elem_cnt - 1; i++) {
			char path_part[SZ_PATH_INCOMPLETE];
			snprintf(path_part, sizeof(path_part), "%s/", policy_path[i]);

			debug("Concatenating %s to %s", path_part, aux_path);
			strncat(aux_path, path_part, path_size - strlen(aux_path) - 1);
		}

		// Finally we concat gpu_monitoring.so plug-in name
		strncat(aux_path, "gpu_monitoring.so", path_size - strlen(aux_path) - 1);

		// Overwrite the original policy plug-in path
		memcpy(*path, aux_path, path_size);

		// Free the auxiliar policy plug-in path
		free(aux_path);
	}

	strtoa_free(policy_path);
}
