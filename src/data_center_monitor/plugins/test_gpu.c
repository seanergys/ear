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

//#define SHOW_DEBUGS 1

#include <string.h>
#include <common/output/debug.h>
#include <data_center_monitor/plugins/metrics.h>
#include <data_center_monitor/plugins/management.h>

// Metrics
static metrics_read_t mr1;
static metrics_read_t mr2;
static metrics_diff_t mrD;
// Information
static int            devs_count;
static int            sms_count;
static const ulong  **freq_list; // KHz
static const uint    *freq_list_count;
static ulong         *pcap_list_max; // W
static ulong         *pcap_list_min;
static int            cfg_dev;
static char           buffer_test[512];

declr_up_get_tag()
{
    *tag = "test_gpu";
    *tags_deps = "<!metrics,!management";
}

declr_up_action_init(_metrics)
{
    metrics_data_alloc(&mr1, &mr2, &mrD);
    return NULL;
}

declr_up_action_init(_test_gpu)
{
    int i, j;
    // Configuration
    if (getenv("GPU_TEST_DEV")) {
        cfg_dev = atoi(getenv("GPU_TEST_DEV")); // 0 by default
    }
    mgt_gpu_data_alloc(&pcap_list_min);
    mgt_gpu_data_alloc(&pcap_list_max);
    mgt_gpu_freq_get_available(no_ctx, &freq_list, &freq_list_count);
    mgt_gpu_power_cap_get_rank(no_ctx, pcap_list_min, pcap_list_max);
    mgt_gpu_count_devices(no_ctx, &devs_count);
    //
    for (i = 0; i < devs_count && i < 1; ++i) {
        fprintf(stderr, "GPU%d: %lu W to %lu W\n", i, pcap_list_min[i], pcap_list_max[i]);
        for (j = 0; j < freq_list_count[i]; ++j) {
            fprintf(stderr, "GPU%d: PS%d %lu KHz\n", i, j, freq_list[i][j]);
        }
    }
    return rsprintf("%d GPUs detected", devs_count);
}

static int test_powercap(int dev)
{
    static long p = 0;
    long pcap;

    pcap = (long) pcap_list_max[dev]-p; 
    if (pcap <= pcap_list_min[dev]) {
        mgt_gpu_power_cap_reset_dev(no_ctx, dev);
        sprintf(buffer_test, "(resetting)");
        p = 0;
        return 0;
    }
    mgt_gpu_power_cap_set_dev(no_ctx, pcap, dev);
    sprintf(buffer_test, "(setting power cap to %ld W)", pcap);
    p += 20;
    return 1;
}

static int test_frequency(int dev)
{
    static int f = 0;
    if (f >= freq_list_count[dev]) {
        mgt_gpu_freq_limit_reset_dev(no_ctx, dev);
        sprintf(buffer_test, "(resetting)");
        f = 0;
        return 0;
    }
    mgt_gpu_freq_limit_set_dev(no_ctx, freq_list[dev][f], dev);
    sprintf(buffer_test, "(setting frequency to %lu KHz)", freq_list[dev][f]);
    f += 1;
    return 1;
}

char *test_select()
{
    static int flip_flop = 0;
    if (!flip_flop) {
        flip_flop = !test_frequency(cfg_dev);
    } else {
        flip_flop = test_powercap(cfg_dev);
    }
    return NULL;
}

declr_up_action_periodic(_metrics)
{
    static char buffer[1024];
    mets_t *mets = (mets_t *) data;
    if (mr1.samples == 0) {
        metrics_data_copy(&mr1, &mets->mr);
        return NULL;
    }
    metrics_data_copy(&mr2, &mets->mr);
    metrics_data_diff(&mr2, &mr1, &mrD);
    metrics_data_copy(&mr1, &mr2);
    sprintf(buffer, "GPU%d, %lu KHz, %0.2lf W %s", cfg_dev,
        mrD.gpu_diff[cfg_dev].freq_gpu, mrD.gpu_diff[cfg_dev].power_w, buffer_test);
    buffer_test[0] = '\0';
    return buffer;
}

declr_up_action_periodic(_test_gpu)
{
    return test_select();
}