/*
*
* This program is part of the EAR software.
*
* EAR provides a dynamic, transparent and ligth-weigth solution for
* Energy management. It has been developed in the context of the
* Barcelona Supercomputing Center (BSC)&Lenovo Collaboration project.
*
* Copyright © 2017-present BSC-Lenovo
* BSC Contact   mailto:ear-support@bsc.es
* Lenovo contact  mailto:hpchelp@lenovo.com
*
* This file is licensed under both the BSD-3 license for individual/non-commercial
* use and EPL-1.0 license for commercial use. Full text of both licenses can be
* found in COPYING.BSD and COPYING.EPL files.
*/

//#define SHOW_DEBUGS 1

#include <errno.h>
#include <stdlib.h>
#include <daemon/local_api/eard_api_rpc.h>

__thread char *rpc_buffer = NULL;
__thread size_t rpc_size  = 0;

extern int ear_fd_req;
extern int ear_fd_ack;

ulong create_sec_tag();

static state_t eard_send(int fd, uint call, state_t state, char *data, size_t size, char *error)
{
	char  *send_data = data;
	size_t send_size = size;
	size_t head_szof;
	eard_head_t head;
	debug("Preparing to send RPC %u of size %lu through FD %d", call, size, fd);
	// If we are sending an error or content
	if (state_fail(state)) {
		send_size = 0;
		if (error != NULL) {
			send_size = strlen(error);
			send_data = error;
		}
	}
	// Preparing the header
	head_szof         = sizeof(eard_head_t);
	head.req_service  = call;
	head.size         = send_size;
	head.sec          = create_sec_tag();
	head.state        = state;
	// Sending
	if (eards_write(fd, (char *) &head, head_szof) != head_szof) {
		return_print(EAR_ERROR, "Problem when writing EARD pipe (%s)", strerror(errno));
	}
	if (head.size > 0) {
		if (eards_write(fd, send_data, head.size) != head.size) {
			return_print(EAR_ERROR, "Problem when writing EARD pipe (%s)", strerror(errno));
		}
	}
	return EAR_SUCCESS;
}

static void rpc_buffer_test(size_t size)
{
	if (rpc_buffer == NULL) {
		rpc_buffer = calloc(1, 8192);
		rpc_size = 8192;
	}
	if (size > rpc_size) {
		rpc_buffer = realloc(rpc_buffer, size);
		rpc_size = size;
	}
}

static state_t eard_recv(int fd, uint call, char *data, size_t *size, size_t expc_size)
{
	eard_head_t head;
	size_t head_szof;
	//
	head_szof  = sizeof(eard_head_t);
	debug("Waiting RPC %u of expected size %lu through FD %d", call, expc_size, fd);
	// Receiving
	if (eards_read(fd, (char *) &head, head_szof) != head_szof) {
		return_print(EAR_ERROR, "Problem when reading EARD pipe (%s)", strerror(errno));
	}
	if (head.req_service != call) {
		return_print(EAR_ERROR, "Expected header of service %u, received %lu",
			call, head.req_service);
	}
	if (head.size > 0) {
		if (data == rpc_buffer) {
			rpc_buffer_test(head.size);
		}
		if (state_fail(head.state)) {
			data = state_buffer;
		}
		if (eards_read(fd, data, head.size) != head.size) {
			return_print(EAR_ERROR, "Problem when reading EARD pipe (%s)", strerror(errno));
		}
		if (state_fail(head.state)) {
			return_msg(head.state, state_buffer);
		}
		if (expc_size != 0 && head.size != expc_size) {
			return_msg(EAR_ERROR, "The read and expected size from the data pipe differs");
		}
	}
	if (size != NULL) {
		*size = head.size;
	}
	return EAR_SUCCESS;
}

static state_t data_check(char *data, size_t size)
{
	if ((size == 0 && data != NULL) || (size > 0 && data == NULL)) {
		return_msg(EAR_ERROR, Generr.input_null);
	}
	return EAR_SUCCESS;
}

static state_t static_rpc(uint call, char *data, size_t size, char *recv_data, size_t *recv_size, size_t expc_size)
{
	state_t s;
	// Testing data dissonances
	if (state_fail(s = data_check(data, size))) {
		return s;
	}
	// Sending header+data
	if (state_fail(s = eard_send(ear_fd_req, call, EAR_SUCCESS, data, size, NULL))) {
		return s;
	}
	// Receiving the data and returning the RPC state
	return eard_recv(ear_fd_ack, call, recv_data, recv_size, expc_size);
}

state_t eard_rpc(uint call, char *data, size_t size, char *recv_data, size_t expc_size)
{
	return static_rpc(call, data, size, recv_data, NULL, expc_size);
}

state_t eard_rpc_buffered(uint call, char *data, size_t size, char **buffer, size_t *recv_size)
{
	if (buffer == NULL) {
		return_msg(EAR_ERROR, "passed buffer can't be NULL");
	}
	//
	rpc_buffer_test(size);
	*buffer = rpc_buffer;
	//
	return static_rpc(call, data, size, rpc_buffer, recv_size, 0);
}

state_t eard_rpc_answer(int fd, uint call, state_t s, char *data, size_t size, char *error)
{
	state_t s2;
	// Testing data dissonances
	if (state_fail(s2 = data_check(data, size))) {
		return s2;
	}
	return eard_send(fd, call, s, data, size, error);
}

state_t eard_rpc_read_pending(int fd, char *recv_data, size_t read_size, size_t expc_size)
{
	state_t s;
	// Testing data dissonances
	if (xtate_fail(s, data_check(recv_data, read_size))) {
		return s;
	}
	if (eards_read(fd, recv_data, read_size) != read_size) {
		return_print(EAR_ERROR, "problem when reading EARD pipe (%s)", strerror(errno));
	}
	if (expc_size != 0 && read_size != expc_size) {
		return_msg(EAR_ERROR, "the read and expected size from the data pipe differs");
	}
	return EAR_SUCCESS;
}

state_t eard_rpc_clean(int fd, size_t size)
{
	//
	rpc_buffer_test(size);
	//
	if (eards_read(fd, rpc_buffer, size) != size) {
		return_print(EAR_ERROR, "Problem when reading EARD pipe (%s)", strerror(errno));
	}
	return EAR_SUCCESS;	
}

