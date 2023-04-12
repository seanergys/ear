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

#define SHOW_DEBUGS 1

#include <math.h>
#include <stdlib.h>
#include <common/system/time.h>
#include <common/output/debug.h>
#include <common/utils/keeper.h>
#include <common/hardware/bithack.h>
#include <metrics/common/msr.h>
#include <metrics/common/apis.h>
#include <metrics/common/hsmp.h>
#include <management/cpupow/cpupow.h>
#include <management/cpupow/archs/amd17.h>

#define NO_REG                         0x00
#define HSMP_PKG_POWER_STATUS          0x04 //ReadSocketPower
#define HSMP_PKG_POWER_LIMIT_READ      0x06 //ReadSocketPowerLimit
#define HSMP_PKG_POWER_LIMIT_WRITE     0x05 //WriteSocketPowerLimit
#define HSMP_PKG_POWER_INFO            0x07 //ReadMaxSocketPowerLimit
#define HSMP_DRAM_POWER_STATUS         NO_REG
#define HSMP_DRAM_POWER_LIMIT_READ     NO_REG
#define HSMP_DRAM_POWER_LIMIT_WRITE    NO_REG
#define HSMP_DRAM_POWER_INFO           NO_REG
#define HSMP_PP0_POWER_STATUS          NO_REG
#define HSMP_PP0_POWER_LIMIT_READ      NO_REG
#define HSMP_PP0_POWER_LIMIT_WRITE     NO_REG
#define HSMP_PP0_POWER_INFO            NO_REG
#define HSMP_PP1_POWER_STATUS          NO_REG
#define HSMP_PP1_POWER_LIMIT_READ      NO_REG
#define HSMP_PP1_POWER_LIMIT_WRITE     NO_REG
#define HSMP_PP1_POWER_INFO            NO_REG
#define MSR_RAPL_POWER_UNIT            0xc0010299 //RAPL_PWR_UNIT
#define MSR_PKG_ENERGY_STATUS          0xc001029B //PKG_ENERGY_STAT
#define MSR_DRAM_ENERGY_STATUS         NO_REG
#define MSR_PP0_ENERGY_STATUS          0xc001029A //CORE_ENERGY_STAT
#define MSR_PP1_ENERGY_STATUS          NO_REG
#define CPUID_APM_INFO                 0x80000007 //ApmInfoEdx

#if SHOW_DEBUGS
#define tprintf3 tprintf2
#else
#define tprintf3
#endif

typedef struct msr_power_unit_s {
    ullong reg;
    double mult_power;
    double mult_energy;
    double mult_time;
} msr_power_unit_t;

typedef struct hsmp_power_limit_s {
    uint reg;
    uint power;
} hsmp_power_limit_t;

typedef struct hsmp_power_info_s {
    uint tdp;
} hsmp_power_info_t;

typedef struct msr_energy_status_s {
    ullong reg;
    uint energy;
} msr_energy_status_t;

typedef struct hsmp_power_status_s {
    uint power;
} hsmp_power_status_t;

struct domain_s {
    int                  set;
    topology_t          *tp;
    const char          *name;
    int                  capable;
    hsmp_power_limit_t  *pl;
    uint                 pl_address_read;
    uint                 pl_address_write;
    const char          *pl_keeper_name;
    msr_energy_status_t *es;
    off_t                es_address;
    timestamp_t          es_time;
    hsmp_power_status_t *ps;
    uint                 ps_address;
    hsmp_power_info_t   *pi;
    uint                 pi_address;
};

static topology_t        tp_cores;
static topology_t        tp_sockets;
static msr_power_unit_t *pu;
static strtable_t        t;

#define DOMAIN_SET(_tp, dom, cp, pl_kn) { \
    .set = 1, \
	.tp = _tp, \
    .name = #dom, \
    .capable = cp, \
	.pl_address_read = HSMP_ ## dom ## _POWER_LIMIT_READ, \
	.pl_address_write = HSMP_ ## dom ## _POWER_LIMIT_WRITE, \
    .pl_keeper_name = pl_kn, \
    .es_address = MSR_ ## dom ## _ENERGY_STATUS, \
    .ps_address = HSMP_ ## dom ## _POWER_STATUS, \
	.pi_address = HSMP_ ## dom ## _POWER_INFO \
}

static struct domain_s domains[] = {
    DOMAIN_SET(&tp_sockets, PKG,  1, "MsrPkgPowerLimit"),
    DOMAIN_SET(&tp_sockets, DRAM, 0, ""),
    DOMAIN_SET(&tp_cores  , PP0,  0, ""),
    DOMAIN_SET(&tp_sockets, PP1,  0, ""),
    { 0, NULL },
};

static int domain_init(int domain)
{
    struct domain_s *d = &domains[domain];
    uint reps[2] = {  0, -1 };
    uint args[1] = { -1 };
    char buffer[64];
    int i;

    // Allocation
    d->pl = calloc(d->tp->cpu_count    , sizeof(hsmp_power_limit_t));
    d->es = calloc(d->tp->cpu_count    , sizeof(msr_energy_status_t));
    d->ps = calloc(d->tp->cpu_count    , sizeof(hsmp_power_status_t));
    d->pi = calloc(d->tp->cpu_count    , sizeof(hsmp_power_info_t));
    //
    for (i = 0; i < d->tp->cpu_count; ++i) {
        // Reading HSMP_*_POWER_INFO
        if (d->pi_address != NO_REG) {
            reps[0] = 0;
            if (state_fail(hsmp_send(d->tp->cpus[i].socket_id, d->pi_address, args, reps))) {
                debug("Failed while reading HSMP_%s_POWER_INFO: %s", d->name, state_msg);
                return 0;
            }
            d->pi[i].tdp = reps[0] / 1000U;
            tprintf3(&t, "%s[%02d]||tdp||%u||W||[%4u / 1000]", d->name, d->tp->cpus[i].id, d->pi[i].tdp, reps[0]);
        }
        // Reading HSMP_*_POWER_LIMIT
        if (d->pl_address_read != NO_REG) {
            reps[0] = 0;
            if (state_fail(hsmp_send(d->tp->cpus[i].socket_id, d->pl_address_read, args, reps))) {
                debug("Failed while reading HSMP_%s_POWER_INFO: %s", d->name, state_msg);
                return 0;
            }
            d->pl[i].reg   = reps[0];
            d->pl[i].power = reps[0] / 1000U;
            tprintf3(&t, "%s[%02d]||power_limit ||%u||W||[%4u / 1000]", d->name, d->tp->cpus[i].id, d->pl[i].power, d->pl[i].reg);
            // Recovering MSR_*_POWER_LIMIT registers
            sprintf(buffer, "%s%d", d->pl_keeper_name, i);
            if (!keeper_load_uint32(buffer, &d->pl[i].reg)) {
                keeper_save_uint32(buffer, d->pl[i].reg);
            } else {
                d->pl[i].power = d->pl[i].reg / 1000U;
                tprintf3(&t, "%s[%02d] (k)||power_limit ||%u||W||[%4u / 1000]", d->name, d->tp->cpus[i].id, d->pl[i].power, d->pl[i].reg);
            }
        }
    }

    return 1;
}

static int rapl_init()
{
    int i;

    tprintf_init2(&t, verb_channel, STR_MODE_DEF, "13 16 14 6 14");
    tprintf3(&t, "Domain||Variable||Value||Unit||Formula");
    tprintf3(&t, "------||--------||-----||----||-------");
    // Allocation
    pu = calloc(tp_sockets.cpu_count, sizeof(msr_power_unit_t));
    // Reading MSR_RAPL_POWER_UNIT (is per socket)
    for (i = 0; i < tp_sockets.cpu_count; ++i) {
        if (state_fail(msr_read(tp_sockets.cpus[i].id, &pu[i].reg, sizeof(ullong), MSR_RAPL_POWER_UNIT))) {
            debug("Failed while reading MSR_RAPL_POWER_UNIT: %s", state_msg)
            return 0;
        }
        pu[i].mult_power  = pow(0.5, (double) getbits64(pu[i].reg,  3,  0));
        pu[i].mult_energy = pow(0.5, (double) getbits64(pu[i].reg, 12,  8));
        pu[i].mult_time   = pow(0.5, (double) getbits64(pu[i].reg, 19, 16));
        tprintf3(&t, "RAPL[%02d]||mult_power ||%lf||W||1/(2^%u)", tp_sockets.cpus[i].id, pu[i].mult_power  , (uint) getbits64(pu[i].reg,  3,  0));
        tprintf3(&t, "RAPL[%02d]||mult_energy||%lf||W||1/(2^%u)", tp_sockets.cpus[i].id, pu[i].mult_energy , (uint) getbits64(pu[i].reg, 12,  8));
        tprintf3(&t, "RAPL[%02d]||mult_time  ||%lf||W||1/(2^%u)", tp_sockets.cpus[i].id, pu[i].mult_time   , (uint) getbits64(pu[i].reg, 19, 16));
    }
    return 1;
}

void mgt_cpupow_amd17_load(topology_t *tp_in, mgt_cpupow_ops_t *ops)
{
    state_t s;
    int i;

    if (tp_in->vendor == VENDOR_AMD && tp_in->family >= FAMILY_ZEN) {
    } else {
        // API incompatible
        debug("api incompatible");
        return;
    }
    // TODO: CPUID
    if (state_fail(msr_test(tp_in, MSR_WR))) {
        debug("failed msr test: %s", state_msg);
        return;
    }
    topology_select(tp_in, &tp_cores  , TPSelect.core  , TPGroup.merge, 0);
    topology_select(tp_in, &tp_sockets, TPSelect.socket, TPGroup.merge, 0);
    // Opening MSRs
    for (i = 0; i < tp_cores.cpu_count; ++i) {
        if (state_fail(msr_open(tp_cores.cpus[i].id, MSR_WR))) {
            debug("msr_open: %s", state_msg);
            return;
        }
    }
    // Opening HSMP
    if (state_fail(s = hsmp_scan(tp_in))) {
        serror("Failed during HSMP open");
        return;
    }
    // Initializing RAPL
    if (!rapl_init()) {
        return;
    }
    // Initializing domains
    for(i = 0; domains[i].set; ++i) {
        if (!domain_init(i)) {
            domains[i].capable = 0;
        }
    }
    #if SHOW_DEBUGS
    verbose(0, "-------------------------------------------------------------");
    #endif
    // At least PKG domain
    if (!domains[CPUPOW_SOCKET].capable) {
        return;
    }
    // Ok
    apis_put(ops->get_api            , mgt_cpupow_amd17_get_api);
    apis_put(ops->count_devices      , mgt_cpupow_amd17_count_devices);
    apis_put(ops->is_powercap_capable, mgt_cpupow_amd17_is_powercap_capable);
    apis_put(ops->powercap_is_enabled, mgt_cpupow_amd17_powercap_is_enabled);
    apis_put(ops->powercap_get       , mgt_cpupow_amd17_powercap_get);
    apis_put(ops->powercap_set       , mgt_cpupow_amd17_powercap_set);
    apis_put(ops->powercap_reset     , mgt_cpupow_amd17_powercap_reset);
    apis_put(ops->tdp_get            , mgt_cpupow_amd17_tdp_get);
}

void mgt_cpupow_amd17_get_api(uint *api)
{
    *api = API_AMD17;
}

int mgt_cpupow_amd17_count_devices(int domain)
{
    return domains[domain].tp->cpu_count;
}

int mgt_cpupow_amd17_is_powercap_capable(int domain)
{
    return domains[domain].capable;
}

state_t mgt_cpupow_amd17_powercap_is_enabled(int domain, uint *enabled)
{
    // TODO: this can't be done without testing
    return EAR_SUCCESS;
}

state_t mgt_cpupow_amd17_powercap_get(int domain, uint *watts)
{
    struct domain_s *d = &domains[domain];
    uint reps[2] = { 0, -1 };
    uint args[1] = { -1 };
    int i;
    // If powercap is not capable, why would you know the current powercap?
    if (!d->capable) {
        return EAR_ERROR;
    }
    // Cleaning
    if (watts != NULL) {
        memset(watts, 0, sizeof(uint)*d->tp->cpu_count);
    }
    // Iterating
    for (i = 0; i < d->tp->cpu_count; ++i) {
        if (state_fail(hsmp_send(d->tp->cpus[i].socket_id, d->pl_address_read, args, reps))) {
            debug("Failed while sending HSMP_%s_POWER_LIMIT: %s", d->name, state_msg);
        }
        watts[i] = reps[0] / 1000U;
        debug("%s[%02d]: %u W of current powercap", d->name, d->tp->cpus[i].id, watts[i]);
    }
    return EAR_SUCCESS;
}

state_t mgt_cpupow_amd17_powercap_set(int domain, uint *watts)
{
    struct domain_s *d = &domains[domain];
    uint args[2] = {  0, -1 };
    uint reps[1] = { -1 };
    int i;
    // If powercap is not capable...
    if (!d->capable) {
        return EAR_ERROR;
    }
    // Iterating
    for (i = 0; i < d->tp->cpu_count; ++i) {
        if (watts[i] == POWERCAP_DISABLE) {
            args[0] = d->pi[i].tdp * 1000U;
        } else {
            args[0] = watts[i] * 1000U;
        }
        debug("%s[%02d]: setting %u mW of powercap", d->name, d->tp->cpus[i].id, args[0]);
        if (state_fail(hsmp_send(d->tp->cpus[i].socket_id, d->pl_address_write, args, reps))) {
            debug("Failed while sending HSMP_%s_POWER_LIMIT: %s", d->name, state_msg);
        }
    }
    return EAR_SUCCESS;
}

state_t mgt_cpupow_amd17_powercap_reset(int domain)
{
    struct domain_s *d = &domains[domain];
    uint args[2] = {  0, -1 };
    uint reps[1] = { -1 };
    int i;
    // If powercap is not capable...
    if (!d->capable) {
        return EAR_ERROR;
    }
    // Iterating
    for (i = 0; i < d->tp->cpu_count; ++i) {
        args[0] = d->pl[i].reg;
        if (state_fail(hsmp_send(d->tp->cpus[i].socket_id, d->pl_address_write, args, reps))) {
            debug("Failed while sending HSMP_%s_POWER_LIMIT: %s", d->name, state_msg);
        }
    }
    return EAR_SUCCESS;
}

state_t mgt_cpupow_amd17_tdp_get(int domain, uint *watts)
{
    struct domain_s *d = &domains[domain];
    int i;
    // Cleaning
    memset(watts, 0, sizeof(uint)*d->tp->cpu_count);
    // Iterating over sockets
    for (i = 0; i < d->tp->cpu_count; ++i) {
        watts[i] = d->pi[i].tdp;
        debug("read tdp: %u", watts[i]);
    }
    return EAR_SUCCESS;
}
