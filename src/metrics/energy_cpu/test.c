// Energy cpu
#include <stdio.h>
#include <metrics/energy_cpu/energy_cpu.h>

int main(int argc, char *argv[])
{
    topology_t tp;
    ctx_t      ctx;
    uint       api;
    ullong    *energy1 = NULL;
    ullong    *energy2 = NULL;
    ullong    *energyD = NULL;
    uint       energy_count;
    int        e;

    topology_init(&tp);
    if (state_fail(energy_cpu_load(&tp, API_FREE))) {
        dprintf(STDERR_FILENO, "energy_cpu_load() failed: %s\n", state_msg);
        return 0;
    }
    if (state_fail(energy_cpu_init(&ctx))) {
        dprintf(STDERR_FILENO, "energy_cpu_init() failed: %s\n", state_msg);
        return 0;
    }
    energy_cpu_get_api(&api);
    dprintf(STDERR_FILENO, "API: %d\n", api);

    energy_cpu_data_alloc(&ctx, &energy1, &energy_count);
    energy_cpu_data_alloc(&ctx, &energy2, &energy_count);
    energy_cpu_data_alloc(&ctx, &energyD, &energy_count);
    sleep(1);
    energy_cpu_read(&ctx, energy1);
    while (1) {
        sleep(4);
        energy_cpu_read_copy(&ctx, energy1, energy2, energyD);
        dprintf(STDERR_FILENO, "FINAL ENERGY READINGS: ");
        for(e = 0; e < energy_count; ++e) {
            dprintf(STDERR_FILENO, "%llu ", energyD[e] / (4LU*1000000LU));
        }
        dprintf(STDERR_FILENO, "\n");
    }
    return 0;
}

