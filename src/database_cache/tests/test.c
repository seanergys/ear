#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <common/states.h>
#include <database_cache/eardbd_api.h>

void fake_application(char *host, uint port)
{
	static cluster_conf_t conf;
	static my_node_conf_t node;
	static application_t app;
	
	// Faking configuration
	conf.db_manager.tcp_port    = port;
	conf.db_manager.sec_tcp_port = port+1;
	sprintf(node.db_ip, host);
	sprintf(node.db_sec_ip, host);

	if (state_fail(eardbd_connect(&conf, &node))) {
		error("Something went wrong while connecting: %s", state_msg);
		return;
	}

	// Faking app
	uint base   = 7020;
	app.job.id  = 1;
	app.is_mpi  = 1;
	app.signature.DC_power = 777;	
	sprintf(app.node_id, "cmp2545");
	
	// Different
	app.job.step_id = base + 1;
	if (state_fail(eardbd_send_application(&app))) { error("Something went wrong sending an application: %s", state_msg); }
	app.job.step_id = app.job.step_id + 1;
	if (state_fail(eardbd_send_application(&app))) { error("Something went wrong sending an application: %s", state_msg); }
	app.job.step_id = app.job.step_id + 1;
	if (state_fail(eardbd_send_application(&app))) { error("Something went wrong sending an application: %s", state_msg); }
	app.job.step_id = app.job.step_id + 1;
	if (state_fail(eardbd_send_application(&app))) { error("Something went wrong sending an application: %s", state_msg); }
	app.job.step_id = app.job.step_id + 1;
	if (state_fail(eardbd_send_application(&app))) { error("Something went wrong sending an application: %s", state_msg); }

	eardbd_disconnect();
}

int main(int argc, char *argv[])
{
	eardbd_status_t status;
	state_t s;

	if (argc < 3) {
		printf("Usage: %s node port\n", argv[0]);
		printf("\tExample of use: %s node001 8787\n", argv[0]);
		return 0;
	}
	
	if (state_fail((s = eardbd_status(argv[1], atoi(argv[2]), &status)))) {
		error("Error when asking for EARDBD status: %s", state_msg);
		return 0;
	}
	printf("Node %s:%s returned state %d, it has %u online nodes\n",
		argv[1], argv[2], s, status.sockets_online);
	printf("Samples status: [%d,%u] [%d,%u] [%d,%u] [%d,%u] [%d,%u] [%d,%u] [%d,%u]\n", 
		status.insert_states[0], status.samples_recv[0],
		status.insert_states[1], status.samples_recv[1],
		status.insert_states[2], status.samples_recv[2],
		status.insert_states[3], status.samples_recv[3],
		status.insert_states[4], status.samples_recv[4],
		status.insert_states[5], status.samples_recv[5],
		status.insert_states[6], status.samples_recv[6]);

	if (argc == 4) {
		fake_application(argv[1], atoi(argv[2]));
	}

	return 0;
}

