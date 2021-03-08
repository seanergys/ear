#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <common/states.h>
#include <database_cache/eardbd_api.h>

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

	return 0;
}

