#include <common/system/monitor.h>
//#include <metrics/gpu/archs/rsmi.h>
#include <metrics/gpu/gpu.h>

int main(int argc, char *argv[])
{
    gpu_t data[2];
    state_t s;

    monitor_init();

    #if 0
    gpu_ops_t ops;

    gpu_rsmi_load(&ops, 0);

    if (ops.init == NULL) {
        return 0;
    }

    gpu_rsmi_init(NULL);

    gpu_rsmi_read(NULL, data);
    #endif
    gpu_load(0);
    gpu_init(NULL);
    gpu_read(data1);
    sleep(4);
    sleep(25);

    return 0;
}

