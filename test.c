#include <stdlib.h>
#include <metrics/common/perf.h>

static perf_t *perfs;
static uint    perfs_count;
static ullong  beats1[32];
static ullong  beats2[32];
static int     i;

int main(int argc, char *argv[])
{
    //if (state_fail(perf_open_files(&perfs, "cpu", "cpu-cycles,instructions", &perfs_count))) {
    //if (state_fail(perf_open_files(&perfs, "uncore_imc_free_running_%d", "data_total,instructions", &perfs_count))) {
    //if (state_fail(perf_open_files(&perfs, "nvidia_scf_pmu_%d", "cmem_rd_data,cmem_wr_data", &perfs_count))) {
    if (state_fail(perf_open_files(&perfs, "nvidia_scf_pmu_%d", "cmem_rd_data,cmem_wr_total_bytes", &perfs_count))) {
		printf("Failed: %s\n", state_msg);
	}
    printf("Found %u perfs\n", perfs_count);
    for (i = 0; i < perfs_count; ++i) {
        if (state_ok(perf_read(&perfs[i], &beats1[i]))) {
        }
    }
    system("perf bench mem memcpy --size 10GB");
    for (i = 0; i < perfs_count; ++i) {
        if (state_ok(perf_read(&perfs[i], &beats2[i]))) {
            printf("CAS: %llu\n", beats2[i] - beats1[i]);
        }
    }

	return 0;
}

