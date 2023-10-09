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

//#define SHOW_DEBUGS 1

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <common/output/debug.h>
#include <common/system/symplug.h>
#include <common/config/config_env.h>
#include <metrics/common/oneapi.h>

// Provisional
#define HACK_ONEAPI_FILE  "HACK_ONEAPI_FILE"

#ifndef ONEAPI_BASE
#define ZE_PATH          ""
#else
#define ZE_PATH          ONEAPI_BASE
#endif
#define ZE_N             34
#define ZE_LIB           "libze_loader.so"

static const char *ze_names[] = {
    // Core
    "zeInit",
    "zeDriverGet",
    "zeDriverGetApiVersion",
    "zeDriverGetProperties",
    "zeDriverGetExtensionProperties",
    "zeDeviceGet",
    "zeDeviceGetProperties",
    "zeDeviceGetSubDevices",
    "zeDeviceGetMemoryProperties",
    "zeDriverGetLastErrorDescription",
    // Sysman
    "zesDeviceGetProperties",
    "sDeviceProcessesGetState",
    "zesDeviceEnumPowerDomains",
    "zesDeviceGetCardPowerDomain",
    "zesPowerGetProperties",
    "zesPowerGetLimits",
    "zesPowerSetLimits",
    "zesPowerGetEnergyCounter",
    "zesPowerGetLimitsExt",
    "zesPowerSetLimitsExt",
    "zesDeviceEnumEngineGroups",
    "zesEngineGetProperties",
    "zesEngineGetActivity",
    "zesDeviceEnumFrequencyDomains",
    "zesFrequencyGetProperties",
    "zesFrequencyGetState",
    "zesFrequencyGetAvailableClocks",
    "zesFrequencyGetRange",
    "zesFrequencySetRange",
    "zesDeviceEnumTemperatureSensors",
    "zesTemperatureGetProperties",
    "zesTemperatureGetState",
    "zesDeviceEnumPsus",
    "zesPsuGetState",
};

static pthread_mutex_t      lock = PTHREAD_MUTEX_INITIALIZER;
static ze_handlers_t       *hs;
static uint                 hs_count;
static ze_t                 ze;
static uint                 ok;

static state_t static_open()
{
    #define _open_test(path) \
        debug("openning %s", path); \
        if (state_ok(plug_open(path, (void **) &ze, ze_names, ZE_N, RTLD_NOW | RTLD_LOCAL))) { \
            return EAR_SUCCESS; \
        }
    #define open_test(path) \
        _open_test(path ZE_LIB); \
        _open_test(path ZE_LIB ".1");

    // Looking for level zero library in tipical paths.
    _open_test(ear_getenv(HACK_ONEAPI_FILE));
    open_test(ZE_PATH "/targets/x86_64-linux/lib/");
    open_test(ZE_PATH "/lib64/");
    open_test(ZE_PATH "/lib/");
    open_test("/usr/lib64/");
    open_test("/usr/lib/x86_64-linux-gnu/");
    open_test("/usr/lib32/");
    open_test("/usr/lib/");

    return_msg(EAR_ERROR, "Can not load ");
}

#define reterr return EAR_ERROR;
#define if_fail(function, action) \
    if ((z = function) != ZE_RESULT_SUCCESS) { \
        debug("Failed " #function ": %s (0x%x)", oneapi_strerror(z), z); \
        action; \
    }

static void print_driver_info(ze_driver_handle_t driver, int dr)
{
    ze_driver_properties_t           props;
    ze_api_version_t                 version;
    ze_result_t                      z;

    // Driver info
    if_fail(ze.DriverGetProperties(driver, &props), return);
    if_fail(ze.DriverGetApiVersion(driver, &version), return);
    debug("DRIVER%d version: %d (API %d) ", dr, props.driverVersion, version);
}

static void print_device_info(int h)
{
    //debug("----------------------------------------");
    debug("name         : '%s'", hs[h].device_props.name);
    debug("modelName    : '%s' (0x%04x 0x%04x)", hs[h].sdevice_props.modelName, hs[h].device_props.vendorId, hs[h].device_props.deviceId);
    debug("type         : %s (%d)", ((hs[h].device_props.type == ZE_DEVICE_TYPE_GPU) ? "GPU" : "OTHER"), hs[h].device_props.type);
    debug("uuid         : '%llu' (%u length)", hs[h].uuid, ZE_MAX_DEVICE_UUID_SIZE);
    debug("serials      : '%s' '%s'", hs[h].sdevice_props.serialNumber, hs[h].sdevice_props.boardNumber);
    debug("coreClockRate: @%u MHz", hs[h].device_props.coreClockRate);
    debug("numSubdevices: %u", hs[h].sdevice_props.numSubdevices);
    debug("#domains PFET: %u/%u/%u/%u", hs[hs_count].spowers_count, hs[hs_count].sfreqs_count, hs[hs_count].sengines_count, hs[hs_count].stemps_count);
}

// Fowler-Noll-Vo hash function
static ullong fnv1a(char c)
{
    ullong hash0 = 0LLU;
    ullong hash1 = 0LLU;
    hash0 = 16777619LLU ^ (ullong) c;
    hash1 += (hash0 <<  1);
    hash1 += (hash0 <<  4);
    hash1 += (hash0 <<  7);
    hash1 += (hash0 <<  8);
    hash1 += (hash0 << 24);
    return hash1 % 2147483647LLU;
}

static ullong hash(char *string)
{
    uint length = ZE_MAX_DEVICE_UUID_SIZE;
    uint hash = 0;
    uint i = 0;

    while (i++ < ZE_MAX_DEVICE_UUID_SIZE) {
        hash += fnv1a(string[i]);
    }
    return hash;
}

static state_t static_init()
{
    ze_driver_handle_t    *drivers;
    ze_device_handle_t    *devices;
    uint                   drivers_count = 0;
    uint                   devices_count = 0;
    uint                   uuid_length;
    ze_result_t            z;
    int                    dr;
    int                    dv;
    int                    dp;

    // Enabling SYSMAN
    setenv("ZES_ENABLE_SYSMAN", "1", 1);
    // This functions is protected from above. Lock is not required.
    if_fail(ze.Init(0), reterr);
    // Discovering all drivers
    if_fail(ze.DriverGet(&drivers_count, NULL), reterr);
    drivers = calloc(drivers_count, sizeof(ze_driver_handle_t));
    if_fail(ze.DriverGet(&drivers_count, drivers), reterr);
    debug("#drivers %u", drivers_count);

    // Counting to allocate
    for (dr = 0; dr < drivers_count; ++dr) {
        // Printing driver information
        print_driver_info(drivers[dr], dr);
        // Counting devices per driver
        if_fail(ze.DeviceGet(drivers[dr], &devices_count, NULL), reterr);
        hs_count += devices_count;
    }
    // No GPUs
    if (hs_count == 0) {
        return_msg(EAR_ERROR, "No GPUs detected");
    }
    // Allocating all the devices
    debug("#devices %u", hs_count);
    devices  = calloc(hs_count, sizeof(ze_device_handle_t));
    hs       = calloc(hs_count, sizeof(ze_handlers_t));
    hs_count = 0;
    // Filling
    for (dr = 0; dr < drivers_count; ++dr) {
        devices_count = 0;
        // Discovering all devices
        if_fail(ze.DeviceGet(drivers[dr], &devices_count, NULL), reterr);
        if_fail(ze.DeviceGet(drivers[dr], &devices_count, devices), reterr);

        for (dv = 0; dv < devices_count; ++dv) {
            debug("-- DEVICE%d -----------------------------", dv);
            // Getting properties to see if is a GPU device
            if_fail(ze.DeviceGetProperties(devices[dv], &hs[dv].device_props), reterr);
            if_fail(ze.sDeviceGetProperties(devices[dv], &hs[dv].sdevice_props), reterr);
            // If not a GPU
            if(ZE_DEVICE_TYPE_GPU != hs[dv].device_props.type) {
                continue;
            }
            // Copying main (single) domains
            memcpy(&hs[hs_count].driver , &drivers[dr], sizeof(ze_driver_handle_t));
            memcpy(&hs[hs_count].device , &devices[dv], sizeof(ze_device_handle_t));
            memcpy(&hs[hs_count].sdevice, &devices[dv], sizeof(zes_device_handle_t));
            // Counting specific domains
            if_fail(ze.sDeviceEnumPowerDomains      (devices[dv], &hs[hs_count].spowers_count , NULL), );
            if_fail(ze.sDeviceEnumFrequencyDomains  (devices[dv], &hs[hs_count].sfreqs_count  , NULL), );
            if_fail(ze.sDeviceEnumEngineGroups      (devices[dv], &hs[hs_count].sengines_count, NULL), );
            if_fail(ze.sDeviceEnumTemperatureSensors(devices[dv], &hs[hs_count].stemps_count  , NULL), );
            if_fail(ze.sDeviceEnumPsus              (devices[dv], &hs[hs_count].spsus_count   , NULL), );
            // Allocating specific domains
            hs[hs_count].spowers  = calloc(hs[hs_count].spowers_count , sizeof(zes_pwr_handle_t));
            hs[hs_count].sfreqs   = calloc(hs[hs_count].sfreqs_count  , sizeof(zes_freq_handle_t));
            hs[hs_count].sengines = calloc(hs[hs_count].sengines_count, sizeof(zes_engine_handle_t));
            hs[hs_count].stemps   = calloc(hs[hs_count].stemps_count  , sizeof(zes_temp_handle_t));
            hs[hs_count].spsus    = calloc(hs[hs_count].spsus_count   , sizeof(zes_psu_handle_t));
            // Getting specific domains
            if_fail(ze.sDeviceEnumPowerDomains      (devices[dv], &hs[hs_count].spowers_count , hs[hs_count].spowers ), );
            if_fail(ze.sDeviceEnumFrequencyDomains  (devices[dv], &hs[hs_count].sfreqs_count  , hs[hs_count].sfreqs  ), );
            if_fail(ze.sDeviceEnumEngineGroups      (devices[dv], &hs[hs_count].sengines_count, hs[hs_count].sengines), );
            if_fail(ze.sDeviceEnumTemperatureSensors(devices[dv], &hs[hs_count].stemps_count  , hs[hs_count].stemps  ), );
            if_fail(ze.sDeviceEnumPsus              (devices[dv], &hs[hs_count].spsus_count   , hs[hs_count].spsus   ), );
            // Allocating properties
            hs[hs_count].spowers_props  = calloc(hs[hs_count].spowers_count , sizeof(zes_power_properties_t ));
            hs[hs_count].sfreqs_props   = calloc(hs[hs_count].sfreqs_count  , sizeof(zes_freq_properties_t  ));
            hs[hs_count].sengines_props = calloc(hs[hs_count].sengines_count, sizeof(zes_engine_properties_t));
            hs[hs_count].stemps_props   = calloc(hs[hs_count].stemps_count  , sizeof(zes_temp_properties_t  ));
            // Getting properties
            for (dp = 0; dp < hs[hs_count].spowers_count ; ++dp) if_fail(ze.sPowerGetProperties      (hs[hs_count].spowers[dp] , &hs[hs_count].spowers_props[dp] ), );
            for (dp = 0; dp < hs[hs_count].sfreqs_count  ; ++dp) if_fail(ze.sFrequencyGetProperties  (hs[hs_count].sfreqs[dp]  , &hs[hs_count].sfreqs_props[dp]  ), );
            for (dp = 0; dp < hs[hs_count].sengines_count; ++dp) if_fail(ze.sEngineGetProperties     (hs[hs_count].sengines[dp], &hs[hs_count].sengines_props[dp]), );
            for (dp = 0; dp < hs[hs_count].stemps_count  ; ++dp) if_fail(ze.sTemperatureGetProperties(hs[hs_count].stemps[dp]  , &hs[hs_count].stemps_props[dp]  ), );
            // Card power domain
            ze.sDeviceGetCardPowerDomain(devices[dv], &hs[hs_count].scard);
            // Getting the serial
            hs[hs_count].uuid = hash(hs[dv].device_props.uuid.id);
            // Last printings
            print_device_info(dv);

            hs_count++;
        }
    }
    debug("----------------------------------------");
    free(devices);
    free(drivers);
    return EAR_SUCCESS;
}

state_t oneapi_open(ze_t *ze_in)
{
    state_t s = EAR_SUCCESS;
    #ifndef ONEAPI_BASE
    return EAR_ERROR;
    #endif
    while (pthread_mutex_trylock(&lock));
    if (ok) {
        goto fini;
    }
    if (state_ok(s = static_open())) {
        if (state_ok(s = static_init())) {
            ok = 1;
        }
    }
fini:
    pthread_mutex_unlock(&lock);
    if (ze_in != NULL) {
        memcpy(ze_in, &ze, sizeof(ze_t));
    }
    return s;
}

state_t oneapi_close()
{
    return EAR_SUCCESS;
}

state_t oneapi_get_devices(ze_handlers_t **devs, uint *devs_count)
{
    if (devs != NULL) {
        *devs = calloc(hs_count, sizeof(ze_handlers_t));
        memcpy(*devs, hs, sizeof(ze_handlers_t) * hs_count);
    }
    if (devs_count != NULL) {
        *devs_count = hs_count;
    }
    return EAR_SUCCESS;
}

int oneapi_is_privileged()
{
    if (!ok) {
        return 0;
    }
    // Provisional
    return (getuid() == 0);
}

char *oneapi_strerror(ze_result_t z)
{
    //ze.StrError(devs[dv].driver, (const char **) &pointer);
    if (z == 0x78000001) return "uninitialized driver";
    if (z == 0x78000003) return "unsupported feature";
    if (z == 0x78000004) return "invalid argument";
    if (z == 0x78000005) return "invalid handler";
    return "unknown error";
}
