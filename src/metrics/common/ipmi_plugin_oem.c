#define SHOW_DEBUGS 1

#include <stdlib.h>
#include <pthread.h>
#include <common/system/time.h>
#include <common/output/debug.h>
#include <common/math_operations.h>
#include <metrics/common/ipmi.h>

typedef struct consumption_s {
    uint64_t    energy; // mJ
    uint64_t    power_current; // mW
    uint64_t    power_minimum;
    uint64_t    power_maximum;
    uint64_t    power_average;
    uint64_t    timeframe;
    timestamp_t timestamp; // We use our timestamp because DCMI fails
} consumption_t;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static uint           devs_count;
static consumption_t *power_data;
static char           msg_sd650e[10] = { 1, 0x3a, 0x32, 5, 0x04, 0x02, 0x00, 0x00, 0x00, 0x00 }; // Lenovo SD650 (energy)
static char           msg_sd650p[10] = { 2, 0x3a, 0x32, 5, 0x04, 0x02, 0x00, 0x00, 0x00, 0x00 }; // Lenovo SD650 (energy)
static char           msg_dcmi  [10] = { 3, 0x2c, 0x02, 4, 0xdc, 0x01, 0x00, 0x00, 0x00, 0x00 }; // Data Center Manageability Interface 
static char           msg_inm   [10] = { 4, 0x2e, 0xc8, 6, 0x57, 0x01, 0x00, 0x01, 0x00, 0x00 }; // Intel Node Manager, example MPMPPB0302WBQ
static char          *msg           = NULL;

static void parse(char id, char *data, consumption_t *reading)
{
    // Getting timestamp
    timestamp_get(&reading->timestamp);
    //
    switch(id) {
    case 1: // Lenovo SD650
        reading->energy = ((uint64_t) *((uint32_t *) &data[3])) * 1000LLU +
                          ((uint64_t) *((uint16_t *) &data[7]));
        reading->timestamp.tv_sec  = ((uint64_t) *((uint32_t *) &data[9]));
        reading->timestamp.tv_nsec = ((uint64_t) *((uint16_t *) &data[13])) * 1000000LLU;
        break;
    case 3: // Data Center Manageability Interface (DCMI)
        reading->power_current     = (uint64_t) *((uint16_t *) &data[2]);
        reading->power_minimum     = (uint64_t) *((uint16_t *) &data[4]);
        reading->power_maximum     = (uint64_t) *((uint16_t *) &data[6]);
        reading->power_average     = (uint64_t) *((uint16_t *) &data[8]);
        reading->timestamp.tv_sec  = (uint64_t) *((uint32_t *) &data[10]);
        reading->timestamp.tv_nsec = 0;
        reading->timeframe         = (uint64_t) *((uint32_t *) &data[14]);
        break;
    case 4: // Intel Node Manager (INM)
        reading->power_current = (uint64_t) *((uint16_t *) &data[4]);
        reading->power_minimum = (uint64_t) *((uint16_t *) &data[6]);
        reading->power_maximum = (uint64_t) *((uint16_t *) &data[8]);
        reading->power_average = (uint64_t) *((uint16_t *) &data[10]);
        //reading->timestamp   = (uint64_t) *((uint32_t *) &data[12]);
        reading->timeframe     = (uint64_t) *((uint32_t *) &data[16]);
        break;
    }
}

static int send_oem_message(int d, ipmi_cmd_t *pkg)
{
    state_t s;
    // Addresses and messages are shared among all devices
    ipmi_cmd_set_addr(pkg, IPMI_SYSTEM_INTERFACE_ADDR_TYPE, IPMI_BMC_CHANNEL, 0x00, 0x00);
    ipmi_cmd_set_msg(pkg, msg[1], msg[2], msg[3]);
    memcpy(pkg->msg_send.data, &msg[4], 6);

    if (state_fail(s = ipmi_cmd_send(d, pkg))) {
        return 0;
    }
    if (pkg->msg_recv_data[0] != 0) {
        return_msg(0, "invalid completion code")
    }
    #if SHOW_DEBUGS
    int i;
    for (i = 0; i < 16; ++i) {
        debug("data[%d]: 0x%x", i, pkg->msg_recv_data[i]);
    }
    #endif
    return 1;
}

state_t energy_init(void **x)
{
    state_t s = EAR_SUCCESS;
    while (pthread_mutex_trylock(&lock));
    if (devs_count) {
        goto leave;
    }
    if (state_fail(s = ipmi_open())) {
        goto leave;
    }
    // Detecting sensors depending on hardware
    if (0 && ipmi_has_hardware("SD650")) msg = msg_sd650;
    else if (ipmi_has_hardware("ThinkSystem")) msg = msg_dcmi;
    // If no sensors, error
    if (msg == NULL) {
        s = EAR_ERROR;
        goto leave;
    }
    devs_count = ipmi_devs_count();
leave:
    pthread_mutex_unlock(&lock);
    return s;
}

state_t energy_dispose(void **x)
{
    return EAR_SUCCESS;
}

state_t energy_dc_read(void *x, void *data)
{
    consumption_t *readings = (consumption_t *) data;
    timestamp_t time = {0};
    ipmi_cmd_t pkg = {0};
    int d;

    timestamp_get(&time);
    for (d = 0; d < devs_count; ++d) {
        if (!send_oem_message(d, &pkg)) {
            return EAR_ERROR;
        }
        parse(msg[0], pkg.msg_recv_data, &readings[d]);
        debug("energy: %llu mJ", readings[d].energy);
        debug("power : %lu/%lu/%lu/%lu mW (cur/min/max/avg)", readings[d].power_current,
            readings[d].power_minimum, readings[d].power_maximum, readings[d].power_average);
        debug("tstamp: %ld s, %ld ns\n", readings[d].timestamp.tv_sec, readings[d].timestamp.tv_nsec);
        debug("tframe: %lu", readings[d].timeframe);
    }
    return EAR_SUCCESS;
}

void *energy_alloc(void **data)
{
    *data = calloc(devs_count, sizeof(consumption_t));
}

state_t energy_accumulated(ulong *energy_mj, void *data1, void *data2)
{
    consumption_t *readings2 = (consumption_t *) data2;
    consumption_t *readings1 = (consumption_t *) data1;
    
    if (readings2->energy != 0LLU || readings1->energy != 0LLU) {
        *energy_mj = overflow_zeros_u64(readings2->energy, readings1->energy);
    } else if (readings2->power_average != 0LLU || readings1->energy != 0LLU) {
        *energy_mj = readings2->power_average * 1000LLU;
    }
    return EAR_SUCCESS;
}

int main(int argc, char *argv[])
{
    void *energy1 = NULL;
    void *energy2 = NULL;
    ulong energyD = 0LU;

    if (state_fail(energy_init(NULL))) {
        return 0;
    }
    energy_alloc(&energy1);
    energy_alloc(&energy2);
    energy_dc_read(NULL, energy1);
    sleep(2);
    energy_dc_read(NULL, energy2);
    energy_accumulated(&energyD, energy1, energy2);
    printf("%lu mJ\n", energyD);
    return 0;
}
