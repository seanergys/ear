/***************************************************************************
 * Copyright (c) 2026 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#include <common/config.h>
#include <common/system/monitor.h>
#include <metrics/energy/energy_node.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include implementation to access split_plugin_args (static) within this TU.
#define TEST 0
#include "../../src/metrics/energy/energy_node.c"

static int test_split_plugin_args()
{
    const char *input         = "/root/ipmi_sensors.so:Sys Power:1:1000000";
    const char *expected_name = "/root/ipmi_sensors.so";
    const char *expected_args = "Sys Power:1:1000000";
    char name_no_args[MAX_PATH_SIZE];
    const char *env_args;

    unsetenv("EAR_ENERGY_ARGS");
    memset(name_no_args, 0, sizeof(name_no_args));

    split_plugin_args(input, name_no_args);

    if (strcmp(name_no_args, expected_name) != 0) {
        fprintf(stderr, "[split_plugin_args] expected '%s' got '%s'\n", expected_name, name_no_args);
        return 1;
    }

    env_args = getenv("EAR_ENERGY_ARGS");
    if (env_args == NULL || strcmp(env_args, expected_args) != 0) {
        fprintf(stderr, "[split_plugin_args] expected EAR_ENERGY_ARGS '%s' got '%s'\n", expected_args,
                env_args == NULL ? "(null)" : env_args);
        return 1;
    }

    printf("[split_plugin_args] ok\n");
    return 0;
}

static int test_no_paths()
{
    unsetenv("EAR_INSTALL_PATH");
    cluster_conf_t conf;
    memset(&conf, 0, sizeof(conf));

    state_t ret = energy_load(API_FREE, &conf, NULL);
    if (state_ok(ret)) {
        fprintf(stderr, "[test_no_paths] Expected failure with no paths\n");
        return 1;
    }

    printf("[test_no_paths] ok\n");
    return 0;
}

static int test_with_dummy()
{
    cluster_conf_t conf;
    memset(&conf, 0, sizeof(conf));

    if (read_cluster_conf("../../etc/conf/ear.conf.template", &conf) != EAR_SUCCESS) {
        fprintf(stderr, "[test_with_dummy] Failed to read ../etc/conf/ear.conf.template\n");
        return 1;
    }

    strcpy(conf.install.obj_ener, "energy_dummy.so");

    state_t ret = energy_load(API_FREE, &conf, NULL);
    if (state_fail(ret)) {
        fprintf(stderr, "[test_with_dummy] Failed to load dummy plugin: %s\n", state_msg);
        return 1;
    }

    energy_unload();
    printf("[test_with_dummy] ok\n");
    return 0;
}

static int test_with_plugin(char *plugin_name)
{
    cluster_conf_t conf;
    edata_t energy_init = NULL;
    edata_t energy_end  = NULL;
    size_t node_size    = 0;
    uint node_units     = 0;
    ulong energy_diff   = 0;

    memset(&conf, 0, sizeof(conf));

    fprintf(stdout, "[test_with_plugin] Loading plugin %s\n", plugin_name);

    if (read_cluster_conf("../../etc/conf/ear.conf.template", &conf) != EAR_SUCCESS) {
        fprintf(stderr, "[test_with_plugin] Failed to read ../etc/conf/ear.conf.template\n");
        return 1;
    }

    strcpy(conf.install.obj_ener, plugin_name);

    if (state_fail(energy_load(API_FREE, &conf, NULL))) {
        fprintf(stderr, "[test_with_plugin] Warning: Failed to load %s: %s\n", plugin_name, state_msg);
        return 0; // Do not fail the test if plugin not available
    }

    if (state_fail(energy_units(&node_units)) || node_units == 0) {
        fprintf(stderr, "[test_with_plugin] energy_units failed or returned 0\n");
        energy_unload();
        return 1;
    }

    if (state_fail(energy_datasize(&node_size)) || node_size == 0) {
        fprintf(stderr, "[test_with_plugin] energy_datasize failed or returned 0\n");
        energy_unload();
        return 1;
    }

    energy_init = calloc(1, node_size);
    energy_end  = calloc(1, node_size);
    if (energy_init == NULL || energy_end == NULL) {
        fprintf(stderr, "[test_with_plugin] allocation failed\n");
        energy_unload();
        return 1;
    }

    if (state_fail(energy_dc_read(energy_init))) {
        fprintf(stderr, "[test_with_plugin] first energy_dc_read failed\n");
        energy_unload();
        free(energy_init);
        free(energy_end);
        return 1;
    }

    sleep(1);

    if (state_fail(energy_dc_read(energy_end))) {
        fprintf(stderr, "[test_with_plugin] second energy_dc_read failed\n");
        energy_unload();
        free(energy_init);
        free(energy_end);
        return 1;
    }

    if (state_fail(energy_accumulated(&energy_diff, energy_init, energy_end))) {
        fprintf(stderr, "[test_with_plugin] energy_accumulated failed\n");
        energy_unload();
        free(energy_init);
        free(energy_end);
        return 1;
    }

    printf("[test_with_plugin] ok (diff=%lu, units=%u)\n", energy_diff, node_units);

    // Test additional functions
    ulong freq;
    if (state_ok(energy_frequency(&freq))) {
        printf("[test_with_plugin] frequency: %lu\n", freq);
    } else {
        printf("[test_with_plugin] frequency not supported\n");
    }

    ulong tms;
    if (state_ok(energy_dc_time_read(energy_end, &tms))) {
        printf("[test_with_plugin] dc_time_read ok (time=%lu)\n", tms);
    } else {
        printf("[test_with_plugin] dc_time_read not supported\n");
    }

    if (state_ok(energy_ac_read(energy_end))) {
        printf("[test_with_plugin] ac_read ok\n");
    } else {
        printf("[test_with_plugin] ac_read not supported\n");
    }

    char buf[1024];
    if (state_ok(energy_to_str(buf, energy_end))) {
        printf("[test_with_plugin] to_str: %s\n", buf);
    } else {
        printf("[test_with_plugin] to_str not supported\n");
    }

    int is_null = energy_data_is_null(energy_end);
    printf("[test_with_plugin] data_is_null: %d\n", is_null);

    energy_unload();
    free(energy_init);
    free(energy_end);

    return 0;
}

int main(int argc, char *argv[])
{
    int failures = 0;

    monitor_init();

    failures += test_split_plugin_args();
    failures += test_no_paths();
    failures += test_with_dummy();
    failures += test_with_plugin("energy_cpu_gpu.so");
    failures += test_with_plugin("energy_bat.so");

    if (failures == 0) {
        printf("ALL TESTS PASSED\n");
        return EXIT_SUCCESS;
    }

    fprintf(stderr, "%d test(s) failed\n", failures);
    return EXIT_FAILURE;
}
