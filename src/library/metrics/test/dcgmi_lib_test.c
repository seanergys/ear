#include <common/output/verbose.h>
#include <common/states.h>
#include <library/metrics/dcgmi_lib.h>
#include <metrics/gpu/gpu.h>


int main(int argc, char **argv)
{
	gpu_load(0); // 0 means no forcing any API
	gpu_init(no_ctx);

	if (state_fail(dcgmi_lib_load()))
	{
		verb(0, "Error loading dcgmi_lib module: %s", state_msg);
		return -1;
	}

	while(1)
	{
		dcgmi_lib_report_total_metrics();

		sleep(4);
	}
}
