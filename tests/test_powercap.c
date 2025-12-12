#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Include config first
#include "common/config.h"
// Mocking functions and variables from other modules that are used in powercap.c
#include "common/states.h"
#include "common/types.h"
#include "daemon/powercap/powercap.h"
#include "daemon/powercap/powercap_mgt.h"
#include "common/system/monitor.h"

// Define macros if needed
#define POWERCAP_MON 1

// Mock implementations
uint32_t mock_power_unlimited = POWER_CAP_UNLIMITED;
uint mock_cpu_devices         = 2;
uint mock_dram_devices        = 1;
uint mock_gpu_devices         = 0;
// uint current_mode             = PC_MODE_AUTO; // Defined in powercap.c

// Globals
int *ips = NULL;
int ips_storage[] = {0};
int self_id = 0;
volatile int init_ips_ready = 1;
int eard_must_exit = 0;
int max_context_created = 0;
int num_contexts = 0;

// Note: current_mode is defined in powercap.c, so we don't define it here.
extern uint32_t current_mode;

// We need struct definition for powermon_app_t, which is in power_monitor.h
#include "daemon/power_monitor.h"
powermon_app_t *current_ear_app[MAX_NESTED_LEVELS];

// Mock struct for suscription (since we don't link monitor.c)
suscription_t mock_sus;
// suscription_t *sus_powercap_monitor; // Defined in powercap.c 

suscription_t *suscription() {
    mock_sus.time_relax = 0;
    mock_sus.time_burst = 0;
    mock_sus.suscribe = NULL; // Will be set by powercap code?
    return &mock_sus;
}
state_t monitor_register(suscription_t *s) { return EAR_SUCCESS; }
state_t monitor_unregister(suscription_t *s) { return EAR_SUCCESS; }
int monitor_is_initialized() { return 1; }

// Power monitor mocks
uint powermon_get_max_powercap_def() { return 1000; }
uint powermon_get_powercap_def() { return 500; }
uint powermon_current_power() { return 100; }
void powermon_report_event(uint event, llong value) {}

uint pmgt_get_cpu_devices()
{
    return mock_cpu_devices;
}

uint pmgt_get_dram_devices()
{
    return mock_dram_devices;
}

uint pmgt_get_gpu_devices()
{
    return mock_gpu_devices;
}

state_t pmgt_get_powercap_value_per_device(uint domain, uint32_t *powercaps)
{
    for (int i = 0;
         i < (domain == DOMAIN_CPU ? mock_cpu_devices : (domain == DOMAIN_DRAM ? mock_dram_devices : mock_gpu_devices));
         ++i) {
        powercaps[i] = mock_power_unlimited;
    }
    return EAR_SUCCESS;
}

// Check signature match with powercap_mgt.h
state_t pmgt_set_powercap_value(pwr_mgt_t *p, uint pid, uint domain, uint32_t limit) { return EAR_SUCCESS; }

// We mock other pmgt functions that powercap.c calls
state_t pmgt_init() { return EAR_SUCCESS; }
state_t pmgt_handler_alloc(pwr_mgt_t **p) { return EAR_SUCCESS; }
state_t pmgt_enable(pwr_mgt_t *p) { return EAR_SUCCESS; }
state_t pmgt_disable(pwr_mgt_t *p) { return EAR_SUCCESS; }
uint pmgt_get_powercap_cpu_strategy(pwr_mgt_t *p) { return 0; }
// Missing mocks added below
uint pmgt_get_powercap_gpu_strategy(pwr_mgt_t *p) { return 0; }
void pmgt_set_pc_mode(pwr_mgt_t *p, uint mode) {}
void pmgt_set_status(pwr_mgt_t *p, uint status) {}
void pmgt_idle_to_run(pwr_mgt_t *p) {}
void pmgt_run_to_idle(pwr_mgt_t *p) {}
void pmgt_set_app_req_freq(pwr_mgt_t *p) {}
void pmgt_set_power_per_domain(pwr_mgt_t *p, dom_power_t *cp, uint st) {}
void pmgt_get_status(pmgt_status_t *status) {}
void pmgt_new_job(pwr_mgt_t *p) {}
void pmgt_end_job(pwr_mgt_t *p) {}

// Note: pmgt_process_message is below, matching signature with char* level
void pmgt_process_message(char *level, int32_t num_values, int32_t values[num_values])
{
    // Mock implementation: just print the received values
    printf("pmgt_process_message called with: level=%s, num_values=%d, values[0]=%d\n", level, num_values, values[0]);
}

// Undefine macros that might conflict (if any)
#undef verbose
#undef debug
#undef error
#define verbose(...) 
#define debug(...) 
#define error(...) 

#include "../src/daemon/powercap/powercap.c"

void powercap_init_device_storage();
void powercap_cleanup_device_storage();
extern state_t powercap_set_stored_device_value(uint domain, uint device_id, uint32_t powercap_value);
extern uint32_t powercap_get_stored_device_value(uint domain, uint device_id);

// Dummy main for the test
void test_powercap_storage()
{
    printf("Running test_powercap_storage...");

    // Test initialization
    powercap_init_device_storage();
    assert(powercap_get_stored_device_value(DOMAIN_CPU, 0) == mock_power_unlimited);
    assert(powercap_get_stored_device_value(DOMAIN_CPU, 1) == mock_power_unlimited);
    assert(powercap_get_stored_device_value(DOMAIN_DRAM, 0) == mock_power_unlimited);

    // Test setting and getting values
    powercap_set_stored_device_value(DOMAIN_CPU, 0, 100);
    assert(powercap_get_stored_device_value(DOMAIN_CPU, 0) == 100);

    powercap_set_stored_device_value(DOMAIN_DRAM, 0, 50);
    assert(powercap_get_stored_device_value(DOMAIN_DRAM, 0) == 50);

    // Test updating all devices in a domain
    powercap_update_all_device_storage(DOMAIN_CPU, 120);
    assert(powercap_get_stored_device_value(DOMAIN_CPU, 0) == 120);
    assert(powercap_get_stored_device_value(DOMAIN_CPU, 1) == 120);

    // Test cleanup
    powercap_cleanup_device_storage();
    assert(powercap_get_stored_device_value(DOMAIN_CPU, 0) == 0); // Should be 0 after cleanup
    assert(powercap_get_stored_device_value(DOMAIN_DRAM, 0) == 0);

    printf("test_powercap_storage passed.\n");
}

void test_powercap_verification()
{
    printf("Running test_powercap_verification...\n");

    powercap_init_device_storage();
    // Enable powercap for verification to run
    my_pc_opt.current_pc = 100; 
    
    powercap_set_stored_device_value(DOMAIN_CPU, 0, mock_power_unlimited);

    // Mocking the hardware value to be the same as the stored value
    assert(powercap_verify_device_values(DOMAIN_CPU) == EAR_SUCCESS);

    // Mocking a mismatch
    powercap_set_stored_device_value(DOMAIN_CPU, 0, 100);
    assert(powercap_verify_device_values(DOMAIN_CPU) == EAR_WARNING);

    powercap_cleanup_device_storage();

    printf("test_powercap_verification passed.\n");
}

void test_powercap_process_message()
{
    printf("Running test_powercap_process_message...\n");

    int32_t values[] = {200};

    // Test case 1: Set manual mode
    powercap_process_message(NULL, "manual", NULL, 0, NULL);
    assert(current_mode == PC_MODE_MANUAL);

    // Test case 2: Deactivate manual mode
    powercap_process_message("deactivate", "manual", NULL, 0, NULL);
    assert(current_mode == PC_MODE_AUTO);

    // Test case 3: Set node level (auto mode)
    powercap_process_message(NULL, NULL, "node", 0, NULL);
    assert(current_mode == PC_MODE_AUTO);

    // Test case 4: Set cpu level (manual mode)
    powercap_process_message(NULL, NULL, "cpu", 0, NULL);
    assert(current_mode == PC_MODE_MANUAL);

    // Test case 5: Set action
    powercap_process_message("set", NULL, "cpu", 1, values);
    // Here we can't directly check the result of pmgt_process_message,
    // but we can see the print from the mock function.

    printf("test_powercap_process_message passed.\n");
}

int main()
{
    test_powercap_storage();
    test_powercap_verification();
    test_powercap_process_message();
    return 0;
}
