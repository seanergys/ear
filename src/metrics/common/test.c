#include <metrics/common/hsmp.h>
#include <common/hardware/bithack.h>

static topology_t tp;
static uint default_boost_limit;

int main(int argc, char *argv[])
{
    topology_init(&tp);
    uint args[3] = { -1, -1, -1 };
    uint reps[3] = { -1, -1, -1 };

    #define HSMP(function) \
        if (state_fail(function)) { \
            printf(#function ": %s\n", state_msg); \
        }
            //return 0; \

    #define HSMP_CLEAN() \
    args[0] = args[1] = reps[0] = reps[1] = -1;

    HSMP(hsmp_open(&tp, O_RDONLY));

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

    // Writing CPU frequency
    args[0] = tp.cpus[0].apicid;
    reps[0] = 0;
    HSMP(hsmp_send(0, HSMP_GET_BOOST_LIMIT, args, reps));
    printf("HSMP_GET_BOOST_LIMIT: %u MHz\n", reps[0]);
    default_boost_limit = reps[0];
    HSMP_CLEAN();

    args[0] = setbits32(0, tp.cpus[0].apicid, 31, 16) | 2000; //2GHz
    printf("Setting CPU0 frequency to %u MHz\n", 2000);
    HSMP(hsmp_send(tp.cpus[0].socket_id, HSMP_SET_BOOST_LIMIT, args, reps));
    HSMP_CLEAN();

    args[0] = tp.cpus[0].apicid;
    reps[0] = 0;
    HSMP(hsmp_send(0, HSMP_GET_BOOST_LIMIT, args, reps));
    printf("HSMP_GET_BOOST_LIMIT: %u MHz\n", reps[0]);
    HSMP_CLEAN();

    args[0] = setbits32(0, tp.cpus[0].apicid, 31, 16) | default_boost_limit; //Recovering default value
    printf("Setting CPU0 frequency to %u MHz\n", default_boost_limit);
    HSMP(hsmp_send(tp.cpus[0].socket_id, HSMP_SET_BOOST_LIMIT, args, reps));
    HSMP_CLEAN();

    args[0] = tp.cpus[0].apicid;
    reps[0] = 0;
    HSMP(hsmp_send(0, HSMP_GET_BOOST_LIMIT, args, reps));
    printf("HSMP_GET_BOOST_LIMIT: %u MHz\n", reps[0]);
    HSMP_CLEAN();

    // Writing DF frequency
    reps[0] = 0;
    reps[1] = 0;
    HSMP(hsmp_send(0, HSMP_GET_FCLK_MCLK, args, reps));
    printf("HSMP_GET_FCLK_MCLK: %u %u MHz (DataFabric freq and ¿DIMMS? freq)\n", reps[0], reps[1]);
    HSMP_CLEAN();

    args[0] = 2; // DF P_STATE 2
    printf("Setting CPU0 DF P_STATE to %u\n", 2);
    HSMP(hsmp_send(0, HSMP_SET_DF_PSTATE, args, reps));
    HSMP_CLEAN();

    reps[0] = 0;
    reps[1] = 0;
    HSMP(hsmp_send(0, HSMP_GET_FCLK_MCLK, args, reps));
    printf("HSMP_GET_FCLK_MCLK: %u %u MHz (DataFabric freq and ¿DIMMS? freq)\n", reps[0], reps[1]);
    HSMP_CLEAN();

    printf("Setting CPU0 DF P_STATE to default\n");
    HSMP(hsmp_send(0, HSMP_SET_AUTO_DF_PSTATE, args, reps));
    HSMP_CLEAN();

    reps[0] = 0;
    reps[1] = 0;
    HSMP(hsmp_send(0, HSMP_GET_FCLK_MCLK, args, reps));
    printf("HSMP_GET_FCLK_MCLK: %u %u MHz (DataFabric freq and ¿DIMMS? freq)\n", reps[0], reps[1]);
    HSMP_CLEAN();

    return 0;
}

