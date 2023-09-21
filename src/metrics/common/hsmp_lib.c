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

#include <metrics/common/hsmp_lib.h>

static const char *esmi_names[] = {
    "esmi_init",
    "esmi_exit",
    "esmi_get_err_msg",
    "esmi_smu_fw_version_get",
    "esmi_hsmp_proto_ver_get",
    "esmi_socket_power_get",
    "esmi_socket_power_cap_set",
    "esmi_socket_power_cap_get",
    "esmi_socket_power_cap_max_get",
    "esmi_core_boostlimit_set",
    "esmi_socket_boostlimit_set",
    "esmi_core_boostlimit_get",
    "esmi_prochot_status_get",
    "esmi_xgmi_width_set",
    "esmi_apb_disable",
    "esmi_apb_enable",
    "esmi_fclk_mclk_get",
    "esmi_cclk_limit_get",
    "esmi_socket_c0_residency_get",
    "esmi_socket_lclk_dpm_level_set",
    "esmi_ddr_bw_get"
};

#define ESMI_N 21

static esmi_t          esmi;
static uint            loaded;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static state_t translate(esmi_status_t s)
{
    if (s == ESMI_SUCCESS) {
        return EAR_SUCCESS;
    }
    return_msg(EAR_ERROR, esmi_get_err_msg(s));
}

static state_t hsmp_load_library()
{
    #define load_library(path) \
        debug("Openning %s", (char *) path); \
        if (state_ok(plug_open(path, (void **) &esmi, esmi_names, ESMI_N, RTLD_NOW | RTLD_LOCAL))) { \
            return EAR_SUCCESS; \
        }

    load_library(ear_getenv("HACK_HSMP_LIBRARY"));
    load_libraries("/usr/lib64/libe_smi64.so");
    load_libraries("/usr/lib64/libe_smi64.so.1");
    load_libraries("/usr/lib/libe_smi64.so");
    load_libraries("/usr/lib/libe_smi64.so.1");

    return_msg(EAR_ERROR, "Can not load libe_smi.so");
}

state_t hsmp_lib_open(topology_t *tp)
{
    state_t s = EAR_SUCCESS
    while (pthread_mutex_trylock(&lock));
    if (loaded) {
        goto ret;
    }
    if (state_ok(s = hsmp_load_library())) {
        if (state_ok(s = translate(esmi_init()))) {
            loaded = 1;
        }
    }
ret:
    pthread_mutex_unlock(&lock);
    return s;
}

state_t hsmp_lib_close()
{
    return translate(esmi.exit());
}

state_t hsmp_lib_send(int socket, uint function, uint *args, uint *reps)
{
    struct smu_fw_version smu;
    struct ddr_bw_metrics ddr;
    esmi_status_t s;

    // Switch case
    switch (function) {
        case HSMP_TEST:
            // If HSMP initialized then this test is correct
            reps[0] = args[0] + 1;
            return EAR_SUCCESS;
        case HSMP_GET_SMU_VERSION:
            return translate(esmi.smu_fw_version_get((struct smu_fw_version *) &reps[0]));
        case HSMP_GET_INTERFACE_VERSION:
            return translate(esmi.hsmp_proto_ver_get(&reps[0]));
        case HSMP_READ_SOCKET_POWER:
            return translate(esmi.socket_power_get(socket, &reps[0]));
        case HSMP_WRITE_SOCKET_POWER_LIMIT:
            return translate(esmi.esmi_socket_power_cap_set(socket, args[0]));
        case HSMP_READ_SOCKET_POWER_LIMIT:
            return translate(esmi.esmi_socket_power_cap_get(socket, &reps[0]));
        case HSMP_READ_MAX_SOCKET_POWER_LIMIT:
            return translate(esmi.socket_power_cap_max_get(socket, &reps[0]));
        case HSMP_WRITE_BOOST_LIMIT:
            return translate(esmi.core_boostlimit_set(getbits32(args[0], 31, 16), getbits32(args[0], 15, 0)));
        case HSMP_WRITE_BOOST_LIMIT_ALL_CORES:
            return translate(esmi.esmi_socket_boostlimit_set(socket, args[0]));
        case HSMP_READ_BOOST_LIMIT:
            reps[0] = 0U;
            return translate(esmi.core_boostlimit_get(args[0], &reps[0]));
        case HSMP_READ_PROCHOT_STATUS:
            return translate(esmi.prochot_status_get(socket, &reps[0]));
        case HSMP_SET_XGMI_LINK_WIDTH_RANGE:
            return translate(esmi.xgmi_width_set((uchar) getbits32(args[0], 15, 8), (uchar) getbits32(args[0], 7, 0)));
        case HSMP_APB_DISABLE:
            return translate(esmi.apb_disable(socket, (uchar) args[0]));
        case HSMP_APB_ENABLE:
            return translate(esmi.apb_enable(socket));
        case HSMP_READ_CURRENT_FCLK_MEMCLK:
            return translate(esmi.fclk_mclk_get(socket, &reps[0], &reps[1]));
        case HSMP_READ_CCLK_FREQUENCY_LIMIT:
            return translate(socket, &reps[0]);
        case HSMP_SOCKET_C0_RESIDENCY:
            return translate(esmi.socket_c0_residency_get(socket, &reps[0]));
        case HSMP_SET_LCLK_DPM_LEVEL_RANGE:
            return translate(esmi.socket_lclk_dpm_level_set(socket,
                        (uchar) getbits32(args[0], 24, 16),
                        (uchar) getbits32(args[0],  7,  0),
                        (uchar) getbits32(args[0], 15,  8)));
        case HSMP_GET_MAX_DDR_BANDIWDTH:
            memset(&ddr, 0, sizeof(struct ddr_bw_metrics));
            s = esmi.ddr_bw_get(&ddr);
            reps[0] = setbits32(reps[0], ddr.max_bw      , 31, 20);
            reps[0] = setbits32(reps[0], ddr.utilized_bw , 19,  8);
            reps[0] = setbits32(reps[0], ddr.utilized_pct,  7,  0);
            return translate(s);
    }
    return_msg(EAR_ERROR, "HSMP function not found");
}