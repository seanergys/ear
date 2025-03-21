#include <common/output/debug.h>
#include <metrics/common/hsmp.h>
#include <common/hardware/bithack.h>

static topology_t tp;

int main(int argc, char *argv[])
{
    topology_init(&tp);
    uint args[3] = { -1, -1, -1 };
    uint reps[3] = { -1, -1, -1 };

    #define HSMP(function) \
        if (state_fail(function)) { \
            debug(#function ": %s\n", state_msg); \
            return 0; \
        }
        
    #define HSMP_CLEAN() \
    args[0] = args[1] = reps[0] = reps[1] = -1;

    HSMP(hsmp_open(&tp, O_RDONLY));
#if 1
    reps[0] = 0;
    HSMP(hsmp_send(0, HSMP_GET_SMU_VER, args, reps));
    printf("HSMP_GET_SMU_VER: %u\n", reps[0]);
    HSMP_CLEAN();
    
    reps[0] = 0;
    HSMP(hsmp_send(0, HSMP_GET_SOCKET_POWER, args, reps));
    printf("HSMP_GET_SOCKET_POWER: %u W\n", reps[0] / 1000);
    HSMP_CLEAN();
    
    reps[0] = 0;
    HSMP(hsmp_send(0, HSMP_GET_SOCKET_POWER_LIMIT, args, reps));
    printf("HSMP_GET_SOCKET_POWER_LIMIT: %u W\n", reps[0] / 1000);
    HSMP_CLEAN();
    
    reps[0] = 0;
    HSMP(hsmp_send(0, HSMP_GET_SOCKET_POWER_LIMIT_MAX, args, reps));
    printf("HSMP_READ_MAX_SOCKET_POWER_LIMIT: %u W\n", reps[0] / 1000);
    HSMP_CLEAN();
    
    args[0] = 0;
    reps[0] = 0;
    HSMP(hsmp_send(0, HSMP_GET_BOOST_LIMIT, args, reps));
    printf("HSMP_GET_BOOST_LIMIT: %u MHz\n", reps[0]);
    HSMP_CLEAN();

    reps[0] = 0;
    reps[1] = 0;
    HSMP(hsmp_send(0, HSMP_GET_FCLK_MCLK, args, reps));
    printf("HSMP_GET_FCLK_MCLK: %u %u MHz (DataFabric freq and ¿DIMMS? freq)\n", reps[0], reps[1]);
    HSMP_CLEAN();
    
    reps[0] = 0;
    HSMP(hsmp_send(0, HSMP_GET_CCLK_THROTTLE_LIMIT, args, reps));
    printf("HSMP_GET_CCLK_THROTTLE_LIMIT: %u MHz (Core throttle limit)\n", reps[0]);
    HSMP_CLEAN();
    
    reps[0] = 0;
    HSMP(hsmp_send(0, HSMP_GET_C0_PERCENT, args, reps));
    printf("HSMP_GET_C0_PERCENT: %u\n", reps[0]);
    HSMP_CLEAN();
   
    reps[0] = 0;
    HSMP(hsmp_send(0, HSMP_GET_DDR_BANDWIDTH, args, reps));
    printf("HSMP_GET_DDR_BANDWIDTH: %u Gbps\n", getbits32(reps[0], 19, 8));
    HSMP_CLEAN();
    
    reps[0] = 0;
    HSMP(hsmp_send(0, HSMP_GET_DDR_BANDWIDTH, args, reps));
    printf("HSMP_GET_DDR_BANDWIDTH: %u Gbps\n", getbits32(reps[0], 19, 8));
    HSMP_CLEAN();
    
    HSMP(hsmp_send(0, HSMP_SET_AUTO_DF_PSTATE, args, reps));
    printf("HSMP_SET_AUTO_DF_PSTATE: -\n");
    HSMP_CLEAN();
#endif    
    return 0;
}
