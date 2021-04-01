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

#include <database_cache/eardbd.h>
#include <database_cache/eardbd_body.h>
#include <database_cache/eardbd_sync.h>
#include <database_cache/eardbd_signals.h>
#include <database_cache/eardbd_storage.h>

// Buffers
extern packet_header_t input_header;
extern char input_buffer[SZ_BUFFER];
extern char extra_buffer[SZ_BUFFER];

//
extern int master_iam; // Master is who speaks
extern int server_iam;
extern int mirror_iam;

// Sockets
extern socket_t *smets_srv;
extern socket_t *smets_mir;
extern socket_t *ssync_srv;
extern socket_t *ssync_mir;

// Synchronization
extern packet_header_t header_answer;
extern packet_header_t header_question;
extern sync_qst_t data_question;
extern sync_ans_t data_answer;

// Descriptors
extern struct sockaddr_storage addr_cli;
extern fd_set fds_incoming;
extern fd_set fds_active;
extern int fd_cli;
extern int fd_min;
extern int fd_max;
// Descriptors storage
extern long fd_hosts[FD_SETSIZE];
//
extern struct timeval timeout_insr;
extern struct timeval timeout_aggr;
extern struct timeval timeout_slct;
extern time_t time_insr;
extern time_t time_aggr;
extern time_t time_slct;
//
extern time_t time_recv1[EDB_NTYPES];
extern time_t time_recv2[EDB_NTYPES];
extern time_t time_insert1[EDB_NTYPES];
extern time_t time_insert2[EDB_NTYPES];
extern size_t type_sizeof[EDB_NTYPES];
extern uint   sample_index[EDB_NTYPES];
extern char  *type_name[EDB_NTYPES];
extern ulong  type_alloc_len[EDB_NTYPES];
extern uint   sample_recv[EDB_NTYPES];
//
extern uint   sockets_accepted;
extern uint   sockets_online;
extern uint   sockets_disconnected;
extern uint   sockets_unrecognized;
extern uint   sockets_timeout;
// Strings
extern char *str_who[2];
extern int verbosity;

/*
 *
 * Time
 *
 */

void time_substract_timeouts()
{
	timeout_insr.tv_sec -= time_slct;
	timeout_aggr.tv_sec -= time_slct;
}

void time_reset_timeout_insr(time_t offset_insr)
{
	// Refresh insert time
	timeout_insr.tv_sec  = time_insr + offset_insr;
	timeout_insr.tv_usec = 0L;
}

void time_reset_timeout_aggr()
{
	// Refresh aggregation timeout
	timeout_aggr.tv_sec  = time_aggr;
	timeout_aggr.tv_usec = 0L;
}

void time_reset_timeout_slct()
{
	// Refresh select time
	time_slct = timeout_aggr.tv_sec;

	if (timeout_insr.tv_sec < timeout_aggr.tv_sec) {
		time_slct = timeout_insr.tv_sec;
	}

	timeout_slct.tv_sec  = time_slct;
	timeout_slct.tv_usec = 0L;
}

/*
 *
 * Net
 *
 */

int sync_fd_is_new(int fd)
{
	int nc;

	nc  = !mirror_iam && (fd == smets_srv->fd || fd == ssync_srv->fd);
	nc |=  mirror_iam && (fd == smets_mir->fd);

	return nc;
}

int sync_fd_is_mirror(int fd_lst)
{
	return !mirror_iam && (fd_lst == ssync_srv->fd);
}

int sync_fd_exists(long ip, int *fd_old)
{
	int i;

	if (ip == 0) {
		return 0;
	}
	for (i = 0; i < (fd_max + 1); ++i) {
		if (FD_ISSET(i, &fds_active) && fd_hosts[i] == ip) {
			*fd_old = i;
			return 1;
		}
	}

	return 0;
}

void sync_fd_add(int fd, long ip)
{
	fd_hosts[fd] = ip;

	FD_SET(fd, &fds_active);

	if (fd > fd_max) {
		fd_max = fd;
	}
	if (fd < fd_min) {
		fd_min = fd;
	}
	// Metrics
	sockets_accepted += 1;
	sockets_online += 1;
}

void sync_fd_get_ip(int fd, long *ip)
{
	*ip = fd_hosts[fd];
}

void sync_fd_disconnect(int fd)
{
	fd_hosts[fd] = 0;
	sockets_close_fd(fd);
	FD_CLR(fd, &fds_active);
	// Metrics
	sockets_online -= 1;
}

/*
 *
 * Synchronization main/mirror
 *
 */

int sync_question(uint sync_option, int veteran, sync_ans_t *answer)
{
	time_t timeout_old;
	state_t s;

	verb_who("synchronization started: asking the question to %s (%d)",
			ssync_mir->host_dst, sync_option);

	data_question.sync_option = sync_option;
	data_question.veteran = veteran;

	// Preparing packet
	sockets_header_update(&header_question);

	// Synchronization pipeline
	s = sockets_socket(ssync_mir);

	if (state_fail(s)) {
		verb_who("failed to create client socket (%d, %s)", s, intern_error_str);
		return EAR_ERROR;
	}

	s = sockets_connect(ssync_mir);

	if (state_fail(s)) {
		verb_who("failed to connect (%d, %s)", s, intern_error_str);
		return EAR_ERROR;
	}

	s = __sockets_send(ssync_mir, &header_question, (char *) &data_question);

	if (state_fail(s)) {
		verb_who("failed to send (%d, %s)", s, intern_error_str);
		return EAR_ERROR;
	}

	// Setting new timeout
	sockets_timeout_get(ssync_mir->fd, &timeout_old);
	s = sockets_timeout_set(ssync_mir->fd, 10);

	// Waiting
	s = __sockets_recv(ssync_mir->fd, &header_answer,
		(char *) &data_answer, sizeof(sync_ans_t), 1);

	// Recovering old timeout
	sockets_timeout_set(ssync_mir->fd, timeout_old);

	if (state_fail(s)) {
		verb_who("failed to receive (%d, %s)", s, intern_error_str);
		return EAR_ERROR;
	}

	s = sockets_close(ssync_mir);

	if (verbosity) {
		verb_who_noarg("synchronization completed correctly");
	}

	if (answer != NULL) {
		memcpy (answer, &data_answer, sizeof(sync_ans_t));
	}

	return EAR_SUCCESS;
}

int sync_answer(int fd, int veteran)
{
	socket_t sync_ans_socket;
	state_t s;

	if (verbosity) {
		verb_who_noarg("synchronization started: answering the question");
	}

	// Socket
	sockets_clean(&sync_ans_socket);
	sync_ans_socket.protocol = TCP;
	sync_ans_socket.fd = fd;

	// Header
	sockets_header_update(&header_answer);
	data_answer.veteran = veteran;

	s = __sockets_send(&sync_ans_socket, &header_answer, (char *) &data_answer);

	if (state_fail(s)) {
		verb_who("Failed to send to MIRROR (%d, %s)", s, intern_error_str);
		return EAR_IGNORE;
	}

	if (verbosity) {
		verb_who_noarg("synchronization completed correctly");
	}

	return EAR_SUCCESS;
}
