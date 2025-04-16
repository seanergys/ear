#define SHOW_DEBUGS 1

#define _GNU_SOURCE
#include <stdlib.h>
#include <pthread.h>
#include <common/output/debug.h>
#include <metrics/energy/node/energy_inm_power.c>

void *energy_alloc(void **data)
{
    size_t size;
    energy_datasize(&size);
    *data = calloc(1, size);
}

int main(int argc, char *argv[])
{
    void *context = NULL;
    void *energy1 = NULL;
    void *energy2 = NULL;
    ulong energyD = 0LU;

    if (state_fail(energy_init(&context))) {
        return 0;
    }
    energy_alloc(&energy1);
    energy_alloc(&energy2);
    energy_dc_read(context, energy1);
    sleep(2);
    energy_dc_read(context, energy2);
    energy_accumulated(&energyD, energy1, energy2);
    printf("%lu\n", energyD); 
    return 0;
}
