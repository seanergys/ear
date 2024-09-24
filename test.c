#include <metrics/common/perf.h>

static perf_t *perfs;
static uint    perfs_count;

int main(int argc, char *argv[])
{
    //if (state_fail(perf_open_files(&perfs, "cpu", "cpu-cycles,instructions", &perfs_count))) {
    if (state_fail(perf_open_files(&perfs, "uncore_imc_free_running_%d", "data_total,instructions", &perfs_count))) {
		printf("Failed: %s\n", state_msg);
	}
    printf("Found %u perfs\n", perfs_count);

	return 0;
}

