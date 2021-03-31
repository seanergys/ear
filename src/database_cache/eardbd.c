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

#include <sys/stat.h>
//#define SHOW_DEBUGS 1
#include <sys/types.h>
#include <database_cache/eardbd.h>
#include <database_cache/eardbd_body.h>
#include <database_cache/eardbd_sync.h>
#include <database_cache/eardbd_signals.h>
#include <database_cache/eardbd_storage.h>
#if !EDB_OFFLINE
#include <common/config.h>
#include <common/database/db_helper.h>
#endif

// Configuration
cluster_conf_t conf_clus;

// Input buffers
packet_header_t input_header;
char input_buffer[SZ_BUFFER];
char extra_buffer[SZ_BUFFER];

// Sockets
static socket_t sockets[4];
socket_t *smets_srv = &sockets[0];
socket_t *smets_mir = &sockets[1];
socket_t *ssync_srv = &sockets[2];
socket_t *ssync_mir = &sockets[3];

// Descriptors
struct sockaddr_storage addr_cli;
fd_set fds_incoming;
fd_set fds_active;
int fd_cli;
int fd_min;
int fd_max;

//
long fd_hosts[FD_SETSIZE];

// Nomenclature:
// 	- Server: main buffer of the gathered metrics. Inserts buffered metrics in
//	the database.
// 	- Mirror: the secondary buffer of the gather metrics. Inserts the buffered
//	metrics in the database if the server is offline.
//	- Master: if in a node there is a server and a mirror, the master is the
//	server. If the server is not enabled because the admin wants the node to
//	be just a mirror, the mirror is the master.

// Mirroring
char master_host[SZ_NAME_MEDIUM]; // This node name
char master_alia[SZ_NAME_MEDIUM];
char master_name[SZ_NAME_MEDIUM];
char server_host[SZ_NAME_MEDIUM]; // If i'm mirror, which is the server?
static int server_port;
static int mirror_port;
static int synchr_port;
int server_too;
int mirror_too;
int master_iam; // Master is who speaks
int server_iam;
int mirror_iam;

// PID
char *path_pid, path_tmp_pid[128];
process_data_t proc_data_srv;
process_data_t proc_data_mir;
pid_t server_pid;
pid_t mirror_pid;
pid_t others_pid;

// Synchronization
packet_header_t header_question;
packet_header_t header_answer;
sync_qst_t data_question;
sync_ans_t data_answer;

// Data
time_t time_recv1[EDB_NTYPES];   // Time between first and last sample start
time_t time_recv2[EDB_NTYPES];   // Time between first and last sample stop
time_t time_insert1[EDB_NTYPES]; // Insert time start
time_t time_insert2[EDB_NTYPES]; // Insert time stop
size_t type_sizeof[EDB_NTYPES];  // Type sizeof
char  *type_name[EDB_NTYPES];    // Sample name
char  *type_alloc[EDB_NTYPES];   // Type batch allocation
float  type_alloc_mbs[EDB_NTYPES]; // Type allocation in megabytes
uint   type_alloc_100[EDB_NTYPES]; // Type allocation in percentage
ulong  type_alloc_len[EDB_NTYPES]; // Type allocation in vector length
ulong  sample_index[EDB_NTYPES]; // Pointer to the next empty space
uint   sample_recv[EDB_NTYPES];  // Samples received counter

// Sockets info
uint sockets_accepted;
uint sockets_online;
uint sockets_disconnected;
uint sockets_unrecognized;
uint sockets_timeout;

// Status
eardbd_status_t status;

// Times (alarms)
struct timeval timeout_insr;
struct timeval timeout_aggr;
struct timeval timeout_slct;
time_t time_insr;
time_t time_aggr;
time_t time_slct;

// State machine from the main loop
int reconfiguring;
int listening;
int releasing;
int exitting;
int updating;
int dreaming;
int veteran;
int forked;

// Extras
static float alloc;
sigset_t sigset;

// Strings
char *str_who[2] = { "server", "mirror" };
int verbosity = 0;

/*
 *
 * Init
 *
 */

static void init_general_configuration(int argc, char **argv, cluster_conf_t *conf_clus)
{
	int mode;
	char *p;

	// Cleaning 0 (who am I? Ok I'm the server, which is also the master for now)
	mirror_iam = 0;
	mirror_too = 0;
	server_iam = 1;
	server_too = 0;
	master_iam = 1;
	forked     = 0;
	server_pid = getpid();
	mirror_pid = 0;
	others_pid = 0;

	log_open("eardbd");

	// Getting host name and host alias
	gethostname(master_host, SZ_NAME_MEDIUM);
	gethostname(master_alia, SZ_NAME_MEDIUM);
	
	// Finding a possible short form
	if ((p = strchr(master_alia, '.')) != NULL) {
		p[0] = '\0';
	}

	// Configuration
	#if !EDB_OFFLINE
	if (get_ear_conf_path(extra_buffer) == EAR_ERROR) {
		_error("while getting ear.conf path");
	}

	verb_master("reading '%s' configuration file", extra_buffer);
	read_cluster_conf(extra_buffer, conf_clus);

	#ifdef USE_EARDBD_CONF
	// Database configuration
	if (get_eardbd_conf_path(extra_buffer) == EAR_ERROR){
		_error("while getting eardbd.conf path");
	}

	verb_master("reading '%s' database configuration file", extra_buffer);
	read_eardbd_conf(extra_buffer, conf_clus->database.user, conf_clus->database.pass);
	#endif

	// Output redirection
	if (conf_clus->db_manager.use_log) {
		verb_master("redirecting output to '%s'", conf_clus->install.dir_temp);
	}
	// Database
	init_db_helper(&conf_clus->database);

	debug("dbcon info: '%s@%s' access", conf_clus->database.database, conf_clus->database.user);
	debug("dbcon info: '%s:%d' socket", conf_clus->database.ip, conf_clus->database.port);

	// Mirror finding
	mode = get_node_server_mirror(conf_clus, master_host, server_host);

	server_too = (mode & 0x01);
	mirror_too = (mode & 0x02) > 0;

	#else

	conf_clus->db_manager.tcp_port      = 8811;
	conf_clus->db_manager.sec_tcp_port  = 8812;
	conf_clus->db_manager.sync_tcp_port = 8813;
	conf_clus->db_manager.mem_size      = 20;
	conf_clus->db_manager.aggr_time     = 60;
	conf_clus->db_manager.insr_time     = atoi(argv[4]);

	server_too = atoi(argv[1]);
	mirror_too = atoi(argv[2]);

	strcpy(server_host, argv[3]);
	strcpy(conf_clus->install.dir_temp, argv[5]);
	#endif

	// Ports
	server_port = conf_clus->db_manager.tcp_port;
	mirror_port = conf_clus->db_manager.sec_tcp_port;
	synchr_port = conf_clus->db_manager.sync_tcp_port;

	// Allocation
	alloc = (float) conf_clus->db_manager.mem_size;

	// PID protection
	pid_t other_server_pid;
	pid_t other_mirror_pid;
	sprintf(path_tmp_pid,"%s/%d",conf_clus->install.dir_temp,getpid());
	if (mkdir(path_tmp_pid,S_IRUSR|S_IWUSR)<0){
		verb_master("Error creating pid path");
	}
	path_pid = path_tmp_pid;

	// Neither server nor mirror
	if (!server_too && !mirror_too) {
		_error("this node is not configured as a server nor mirror");
	}

	// PID test
	process_data_initialize(&proc_data_srv, "eardbd_test", path_pid);

	if (state_fail(process_pid_file_save(&proc_data_srv))) {
		_error("while testing the temporal directory '%s' (%s)",
			   conf_clus->install.dir_temp, state_msg);
	}

	process_pid_file_clean(&proc_data_srv);
	process_data_initialize(&proc_data_srv, "eardbd_server", path_pid);
	process_data_initialize(&proc_data_mir, "eardbd_mirror", path_pid);

	int server_xst = process_exists(&proc_data_srv, "eardbd", &other_server_pid);
	int mirror_xst = process_exists(&proc_data_mir, "eardbd", &other_mirror_pid);

	server_too = server_too && !server_xst;
	mirror_too = mirror_too && !mirror_xst;

	if (!server_xst) process_pid_file_clean(&proc_data_srv);
	if (!mirror_xst) process_pid_file_clean(&proc_data_mir);
	if ( server_xst) server_pid = other_server_pid;
	if ( mirror_xst) mirror_pid = other_mirror_pid;

	if (!server_too && !mirror_too) {
		_error("another EARDBD process is currently running");
	}

	// Server & mirro verbosity
	verb_master("enabled cache server: %s",                  server_too ? "OK": "NO");
	verb_master("enabled cache mirror: %s (of server '%s')", mirror_too ? "OK" : "NO", server_host);
}

static void init_time_configuration(int argc, char **argv, cluster_conf_t *conf_clus)
{
	// Times
	time_insr = (time_t) conf_clus->db_manager.insr_time;
	time_aggr = (time_t) conf_clus->db_manager.aggr_time;

	if (time_insr == 0) {
		verb_master("insert time can't be 0, using 300 seconds (default)");
		time_insr = 300;
	}
	if (time_aggr == 0) {
		verb_master("aggregation time can't be 0, using 60 seconds (default)");
		time_aggr = 60;
	}

	// Looking for multiple value
	time_reset_timeout_insr(0);
	time_reset_timeout_aggr();
	time_reset_timeout_slct();

	// Times verbosity
	verb_master("insertion time:   %lu seconds", time_insr);
	verb_master("aggregation time: %lu seconds", time_aggr);
}

static int init_sockets_single(socket_t *socket, char *host, int port, int bind)
{
	sockets_clean(socket);

	sockets_init(socket, host, port, TCP);

	//
	if (!bind) {
		return 0;
	}
	if (state_fail(sockets_socket(socket))) {
		return -1;
	}
	if (state_fail(sockets_bind(socket, 60))) {
		return -1;
	}
	if (state_fail(sockets_listen(socket))) {
		return -1;
	}

	sockets_nonblock_set(socket->fd);

	return 0;
}

static void init_sockets(int argc, char **argv, cluster_conf_t *conf_clus)
{
	//
	int errno1 = init_sockets_single(smets_srv, NULL, server_port, 1);
	int errno2 = init_sockets_single(smets_mir, NULL, mirror_port, mirror_too);
	int errno3 = init_sockets_single(ssync_srv, NULL, synchr_port, 1);
	int errno4 = init_sockets_single(ssync_mir, server_host, synchr_port, 0);

	if (errno1 != 0 || errno2 != 0 || errno3 != 0 || errno4 != 0) {
		_error("while binding sockets (%d %d %d %d)", errno1, errno2, errno3, errno4);
	}

	// Verbosity
	char *str_sta[3] = { "listen", "closed", "error" };

	int fd1 = smets_srv->fd;
	int fd2 = smets_mir->fd;
	int fd3 = ssync_srv->fd;
	int fd4 = ssync_mir->fd;
	int st1 = (fd1 == -1) + ((fd1 == -1) * (errno1 != 0));
	int st2 = (fd2 == -1) + ((fd2 == -1) * (errno2 != 0));
	int st3 = (fd3 == -1) + ((fd3 == -1) * (errno3 != 0));
	int st4 = (fd4 == -1) + ((fd4 == -1) * (errno4 != 0));

	// Summary
	tprintf_init(fderr, STR_MODE_DEF, "18 8 7 10 8 8");

	tprintf("type||port||prot||stat||fd||err");
	tprintf("----||----||----||----||--||---");
	tprintf("server metrics||%d||TCP||%s||%d||%d", smets_srv->port, str_sta[st1], fd1, errno1);
	tprintf("mirror metrics||%d||TCP||%s||%d||%d", smets_mir->port, str_sta[st2], fd2, errno2);
	tprintf("server sync||%d||TCP||%s||%d||%d",    ssync_srv->port, str_sta[st3], fd3, errno3);
	tprintf("mirror sync||%d||TCP||%s||%d||%d",    ssync_mir->port, str_sta[st4], fd4, errno4);
	verb_master("TIP! mirror sync socket opens and closes intermittently");
	verb_master("maximum #connections per process: %u", EDB_MAX_CONNECTIONS);
}

static void init_fork(int argc, char **argv, cluster_conf_t *conf_clus)
{
	// Fork
	if (mirror_too)
	{
		mirror_pid = fork();

		if (mirror_pid == 0)
		{
			// Reconfiguring
			mirror_iam = 1;
			server_iam = 0;

			mirror_pid = getpid();
			others_pid = server_pid;
			sleep(2);
		}
		else if (mirror_pid > 0) {
			others_pid = mirror_pid;
		} else {
			_error("error while forking, terminating the program");
		}
	}

	forked = 1;
	master_iam = (server_iam && server_too) || (mirror_iam && !server_too);

	// Verbosity
	char *str_sta[2] = { "(just sleeps)", "" };

	verb_master("cache server pid: %d %s", server_pid, str_sta[server_too]);
	verb_master("cache mirror pid: %d", mirror_pid);
}

static void init_signals()
{
	struct sigaction action;

	// Blocking all signals except USR1, USR2, PIPE, TERM, CHLD, INT and HUP
	sigfillset(&sigset);
	sigdelset(&sigset, SIGUSR1);
	sigdelset(&sigset, SIGUSR2);
	sigdelset(&sigset, SIGTERM);
	sigdelset(&sigset, SIGCHLD);
	sigdelset(&sigset, SIGINT);
	sigdelset(&sigset, SIGHUP);

	sigprocmask(SIG_SETMASK, &sigset, NULL);

	// Editing signals individually
	sigfillset(&action.sa_mask);
	//sigemptyset(&action.sa_mask);
	//sigaddset(&action.sa_mask, SIGCHLD);

	action.sa_handler = NULL;
	action.sa_sigaction = signal_handler;
	action.sa_flags = SA_SIGINFO;

	if (sigaction(SIGUSR1, &action, NULL) < 0) { 
        	verb_who("sigaction error on signal %d (%s)", SIGUSR1, strerror(errno));
	}
	if (sigaction(SIGUSR2, &action, NULL) < 0) {
		verb_who("sigaction error on signal %d (%s)", SIGUSR1, strerror(errno));
	}
	if (sigaction(SIGCHLD, &action, NULL) < 0) {
		verb_who("sigaction error on signal %d (%s)", SIGUSR1, strerror(errno));
	}
	if (sigaction(SIGTERM, &action, NULL) < 0) {
		verb_who("sigaction error on signal %d (%s)", SIGTERM, strerror(errno));
	}
	if (sigaction(SIGINT, &action, NULL) < 0) {
		verb_who("sigaction error on signal %d (%s)", SIGINT, strerror(errno));
	}
	if (sigaction(SIGHUP, &action, NULL) < 0) {
		verb_who("sigaction error on signal %d (%s)", SIGHUP, strerror(errno));
	}
}

static void init_types_single(int i, char *name, size_t size, uint percent)
{
	float batch_100;
	float single_mb;
	float batch_mb;
	ulong batch_b;

	// Obtaining the percentage in floats
	type_name[i]   = name;
	type_sizeof[i] = size;
	batch_100      = ((float) percent) / 100.0;
	batch_mb       = ceilf(alloc * batch_100);
	single_mb      = ((float) type_sizeof[i]) / 1000000.0;
	type_alloc_len[i] = (ulong) (batch_mb / single_mb);
	// If there are less than 100, allocate at least 100 positions
	if (type_alloc_len[i] < 100) {
		type_alloc_len[i] = 100;
	}
	// Allocation and set
	batch_b = type_sizeof[i] * type_alloc_len[i];
	type_alloc_mbs[i] = (double) (batch_b) / 1000000.0;
	//
	type_alloc[i] = malloc(batch_b);
	memset(type_alloc[i], 0, batch_b);

	// Verbosity
	tprintf("%s||%0.2f MBs||%lu Bs||%lu||%0.2f",
			type_name[i], type_alloc_mbs[i], type_sizeof[i], type_alloc_len[i], batch_100);
}

static void init_process_configuration(int argc, char **argv, cluster_conf_t *conf_clus)
{
	uint percent = 0;
	uint invalid = 0;
	uint count = 0;
	int i;

	// Single process configuration
	listening = (server_iam && server_too) || (mirror_iam);
	updating  = 0;

	// is not a listening socket?
	dreaming  = (server_iam && !listening);
	releasing = dreaming;

	// Socket closing
	if (mirror_iam) {
		// Destroying main sockets
		sockets_dispose(smets_srv);
		sockets_dispose(ssync_srv);

		// Keep track of the biggest file descriptor
		fd_max = smets_mir->fd;
		fd_min = smets_mir->fd;

		FD_SET(smets_mir->fd, &fds_active);
	} else {
		// Destroying mirror soockets
		sockets_dispose(smets_mir);

		// Keep track of the biggest file descriptor
		fd_max = ssync_srv->fd;
		fd_min = ssync_srv->fd;

		if (smets_srv->fd > fd_max) {
			fd_max = smets_srv->fd;
		}
		if (smets_srv->fd < fd_min) {
			fd_min = smets_srv->fd;
		}

		FD_SET(smets_srv->fd, &fds_active);
		FD_SET(ssync_srv->fd, &fds_active);
	}

	// Ok, all here is done
	if (dreaming) {
		return;
	}

	// Setting master name
	if (server_iam) sprintf(master_name, "%s-s", master_alia);
	else            sprintf(master_name, "%s-m", master_alia);

	// Types allocation counting
	for (i = 0; i < EARDBD_TYPES; ++i) {
		count = conf_clus->db_manager.mem_size_types[i];
		invalid = invalid & (count > 100 || count == 0);
		percent += count;
	}

	type_alloc_100[i_appsm] = ALLOC_PERCENT_APPS_MPI;
	type_alloc_100[i_appsn] = ALLOC_PERCENT_APPS_SEQUENTIAL;
	type_alloc_100[i_appsl] = ALLOC_PERCENT_APPS_LEARNING;
	type_alloc_100[i_loops] = ALLOC_PERCENT_LOOPS;
	type_alloc_100[i_enrgy] = ALLOC_PERCENT_ENERGY_REPS;
	type_alloc_100[i_aggrs] = ALLOC_PERCENT_ENERGY_AGGRS;
	type_alloc_100[i_evnts] = ALLOC_PERCENT_EVENTS;

	if (!invalid && percent > 90 && percent < 110) {
		type_alloc_100[i_appsm] = conf_clus->db_manager.mem_size_types[0];
		type_alloc_100[i_appsn] = conf_clus->db_manager.mem_size_types[1];
		type_alloc_100[i_appsl] = conf_clus->db_manager.mem_size_types[2];
		type_alloc_100[i_loops] = conf_clus->db_manager.mem_size_types[3];
		type_alloc_100[i_enrgy] = conf_clus->db_manager.mem_size_types[4];
		type_alloc_100[i_aggrs] = conf_clus->db_manager.mem_size_types[5];
		type_alloc_100[i_evnts] = conf_clus->db_manager.mem_size_types[6];
	}

	// Verbose
	tprintf_init(fderr, STR_MODE_DEF, "15 15 11 9 8");

	if(!master_iam) {
		tprintf_close();
	}

	// Summary
	tprintf("type||memory||sample||elems||%%");
	tprintf("----||------||------||-----||----");

	init_types_single(i_appsm, "apps mpi",     sizeof(application_t),          type_alloc_100[i_appsm]);
	init_types_single(i_appsn, "apps non-mpi", sizeof(application_t),          type_alloc_100[i_appsn]);
	init_types_single(i_appsl, "apps learn.",  sizeof(application_t),          type_alloc_100[i_appsl]);
	init_types_single(i_loops, "loops",        sizeof(loop_t),                 type_alloc_100[i_loops]);
	init_types_single(i_evnts, "events",       sizeof(ear_event_t),            type_alloc_100[i_evnts]);
	init_types_single(i_enrgy, "energy reps",  sizeof(periodic_metric_t),      type_alloc_100[i_enrgy]);
	init_types_single(i_aggrs, "energy aggrs", sizeof(periodic_aggregation_t), type_alloc_100[i_aggrs]);

	float mb_total = type_alloc_mbs[i_appsm] + type_alloc_mbs[i_appsn] +
				type_alloc_mbs[i_appsl] + type_alloc_mbs[i_loops] +
				type_alloc_mbs[i_evnts] + type_alloc_mbs[i_enrgy] +
				type_alloc_mbs[i_aggrs];
	tprintf("TOTAL||%0.2f MBs", mb_total);
	verb_master("TIP! this allocated space is per process server/mirror");
	// Synchronization headers
	sockets_header_clean(&header_answer);
	sockets_header_clean(&header_question);

	header_answer.content_type = EDB_TYPE_SYNC_ANSWER;
	header_answer.content_size = sizeof(sync_ans_t);
	header_question.content_type = EDB_TYPE_SYNC_QUESTION;
	header_question.content_size = sizeof(sync_qst_t);
}

static void init_pid_files(int argc, char **argv)
{
	// Process PID save file
	if (server_iam && server_too) {
		process_update_pid(&proc_data_mir);

		if (process_pid_file_save(&proc_data_srv)) {
			if (mirror_too) {
				_error("while creating server PID file, waiting the mirror");
			} else {
				_error("while creating server PID file");
			}
		}
	} else if (mirror_iam && mirror_too)
	{
		process_update_pid(&proc_data_mir);

		if (process_pid_file_save(&proc_data_mir)) {
			_error("while creating mirror PID file");
		}
	}
}

static void init_output_redirection(int argc, char **argv, cluster_conf_t *conf_clus)
{
	int fd_output = -1;

	if (listening) {
		verb_master_line("phase 7: listening (processing every %lu s)", time_insr);
	}

	if (conf_clus->db_manager.use_log) {
		if (server_iam) fd_output = create_log(conf_clus->install.dir_temp, "eardbd.server");
		else fd_output = create_log(conf_clus->install.dir_temp, "eardbd.mirror");
	}
	if (fd_output < 0) {
		fd_output = verb_channel;
	}

	VERB_SET_FD(fd_output);
	WARN_SET_FD(fd_output);
	ERROR_SET_FD(fd_output);
	DEBUG_SET_FD(fd_output);
	TIMESTAMP_SET_EN(conf_clus->db_manager.use_log);
}

/*
 *
 *
 *
 */

void usage(int argc, char **argv) {}

int main(int argc, char **argv)
{
	//
	usage(argc, argv);

	while(!exitting)
	{
		//
		verb_master_line("phase 1: general configuration");
		init_general_configuration(argc, argv, &conf_clus);

		//
		verb_master_line("phase 2: time configuration");
		init_time_configuration(argc, argv, &conf_clus);

		//
		verb_master_line("phase 3: sockets initialization");
		init_sockets(argc, argv, &conf_clus);

		//
		verb_master_line("phase 4: processes fork");
		init_fork(argc, argv, &conf_clus);

		/*
		 * Post fork
		 */

		// Initializing signal handler
		init_signals();

		//
		verb_master_line("phase 6: process configuration & allocation");
		init_process_configuration(argc, argv, &conf_clus);

		// Creating PID files
		init_pid_files(argc, argv);

		// Output file
		init_output_redirection(argc, argv, &conf_clus);

		/*
 		 * Running
 		 */

		//
		body();

		//
		release();

		//
		dream();
	}

	verb_master_line("Bye");

	return 0;
}
