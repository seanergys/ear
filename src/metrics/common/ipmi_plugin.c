#include <metrics/common/ipmi.h>
#include <metrics/energy/energy_node.h>

int bmc_channel = IPMI_BMC_CHANNEL;

void *energy_alloc(void **data)
{
    size_t size = 0;
    energy_datasize(NULL, &size);
    *data = calloc(1, size);
}

int main(int argc, char *argv[])
{
    ehandler_t ctx = {0};
    void *energy1 = NULL;
    void *energy2 = NULL;
    ulong energyD = 0LU;

    if (argc == 1) {
        return 0;
    }
    if (state_fail(energy_load(argv[1]))) {
        printf("energy_load() failed: %s\n", state_msg);
        return 0;
    }
    if (state_fail(energy_init(&ctx))) {
        printf("energy_init() failed: %s\n", state_msg);
        return 0;
    }
    energy_alloc(&energy1);
    energy_alloc(&energy2);
    sleep(1);
    energy_dc_read(&ctx, energy1);
    while (1) {
        sleep(3);
        energy_dc_read(&ctx, energy2);
        energy_accumulated(&ctx, &energyD, energy1, energy2);
        printf("energy: %lu\n", energyD);
    }
    return 0;
}
