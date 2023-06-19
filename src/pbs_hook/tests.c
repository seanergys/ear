#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <pbs_hook/pbs_hook_utils.h>
#include <common/hardware/hardware_info.h>

int main(int argc, char **argv)
{
    printf("Calling pbs_util_get_ID\n");
    assert(pbs_util_get_ID(1000, 4) != 0);
    printf("pbs_util_get_ID works well\n");

    printf("Calling pbs_util_get_eard_port\n");
    char *ear_tmp = getenv("EAR_TMP");
    assert(ear_tmp != NULL);
    assert(pbs_util_get_eard_port(ear_tmp) == 50001);
    printf("pbs_util_get_eard_port works well\n");

    printf("Calling pbs_util_setaffinity\n");
    pid_t curr_pid = getpid();
    cpu_set_t set;
    int errno_val;
    assert(pbs_util_getaffinity(curr_pid, sizeof(set), &set, &errno_val) == 0);
    print_this_affinity_mask(&set, stderr);

    return 0;
}
