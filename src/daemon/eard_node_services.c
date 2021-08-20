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

#include <signal.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <linux/limits.h>
#include <strings.h>
#include <string.h>

//#define SHOW_DEBUGS 1
#include <common/includes.h>
#include <common/types/generic.h>
#include <common/environment.h>
#include <common/types/log_eard.h>
#include <common/system/fd_sets.h>
#include <common/types/pc_app_info.h>
#include <common/hardware/hardware_info.h>
#include <common/system/monitor.h>

#include <management/cpufreq/frequency.h>
#include <management/imcfreq/imcfreq.h>

#include <metrics/common/msr.h>
#include <metrics/gpu/gpu.h>
#include <metrics/energy/cpu.h>
#include <metrics/energy/energy_node.h>
#include <metrics/bandwidth/bandwidth.h>
#include <metrics/frequency/cpu.h>
#include <metrics/imcfreq/imcfreq.h>

#include <daemon/eard_node_services.h>
#include <daemon/local_api/eard_api_conf.h>
#include <daemon/local_api/eard_api_rpc.h>
#include <daemon/local_api/node_mgr.h>
#include <daemon/power_monitor.h>
#include <daemon/shared_configuration.h>
#include <daemon/remote_api/dynamic_configuration.h>

#if USE_DB
#include <database_cache/eardbd_api.h>
#include <common/database/db_helper.h>
#endif
#include <daemon/eard.h>
#if USE_GPUS
#include <daemon/gpu/gpu_mgt.h>
#endif


#define MIN_INTERVAL_RT_ERROR 3600


#if APP_API_THREAD
pthread_t app_eard_api_th;
#endif

#if USE_GPUS
static ctx_t eard_main_gpu_ctx;
static uint eard_gpu_initialized=0;
#endif
static ulong node_energy_datasize;
static edata_t node_energy_data;
static int num_packs = 0;
static unsigned long long *values_rapl;
static cpufreq_t freq_app_req;
static uint size_cpufreqs,count_cpufreqs;
static imcfreq_t *unc_data;
static uint unc_count;
static int global_req_fd;
static char ear_commreq_global[MAX_PATH_SIZE * 2];
static state_t s;
static ulong      energy_freq;
static int RAPL_counting = 0;
/* This var is used in all the eard services, no thread-safe */
static struct daemon_req req;

/* Defined in eard.c */
extern ctx_t imc_eard_ctx;
extern topology_t node_desc;
extern   ehandler_t eard_handler_energy,handler_energy;
extern int fd_rapl[MAX_PACKAGES];
extern char ear_tmp[MAX_PATH_SIZE];
extern char nodename[MAX_PATH_SIZE];
extern int eard_must_exit;
extern cluster_conf_t my_cluster_conf;
extern my_node_conf_t *my_node_conf;
extern ulong eard_max_freq;
extern int num_uncore_counters;


#define NUM_PRIVILEGED_SERVICES 11
uint privileged_services[NUM_PRIVILEGED_SERVICES]={SET_FREQ,SET_TURBO,SET_FREQ_WITH_MASK,SET_NODE_FREQ_WITH_LIST,UNC_SET_LIMITS,START_RAPL,RESET_RAPL,WRITE_APP_SIGNATURE,WRITE_EVENT,WRITE_LOOP_SIGNATURE,GPU_SET_FREQ};

/* FDs for local connections */
int numfds_req = 0;
static fd_set rfds;
static fd_set rfds_basic;
int max_fd = -1;
int total_local_fd=0;

#define NUM_LOCAL_FDS 2
typedef struct local_connection{
  ulong jid,sid,lid,anonymous;
  int fds[NUM_LOCAL_FDS];
}local_connection_t;
static local_connection_t *eard_local_conn;
static uint num_local_con=0;
#define REQ_LOCAL_FD 0
#define ACK_LOCAL_FD 1

/* End data for local connections */
/********************* These functions links local fds with eard job context. The jobid.stepid must be already created **/
uint is_anonymous(ulong jid,ulong sid);
state_t init_local_connections(local_connection_t **my_con,int num_con)
{
  int i;
  local_connection_t *lcon;
  *my_con = calloc(sizeof(local_connection_t),num_con);
  lcon = *my_con;
  for (i=0;i<num_con;i++) lcon[i].jid = ULONG_MAX;
  num_local_con = num_con;
  return EAR_SUCCESS;
}
state_t add_local_fd(local_connection_t *my_con,int fd, int pos)
{
  if (pos >= NUM_LOCAL_FDS){
    state_return_msg(EAR_ERROR,EAR_NO_RESOURCES,"No more FDs in local connection");
  }
  my_con->fds[pos] = fd;
  return EAR_SUCCESS;
}
int is_new_job(local_connection_t *my_con,ulong jid,ulong sid)
{
  int i;
  i = 0;
  while((i < num_local_con) && !((my_con[i].jid == jid) && (my_con[i].sid == sid))) i++;
  if (i<num_local_con) return 1;
  return 0;
}
int add_new_local_connection(local_connection_t *my_con,ulong jid,ulong sid,ulong lid)
{
  int i=0;
  while ((my_con[i].jid != ULONG_MAX) && (i< num_local_con)) i++;
  if (i == num_local_con){
    error("Maximum number of local connections reached");
    return -1;
  }
  my_con[i].jid = jid;
  my_con[i].sid = sid;
  my_con[i].lid = lid;
  my_con[i].anonymous = is_anonymous(jid,sid);
  return i;
}

int get_local_connection_ack(local_connection_t *my_con,int reqfd)
{
  int i=0;
  while ((my_con[i].fds[REQ_LOCAL_FD] != reqfd) && (i<num_local_con)){
      //debug("Check local conn req=%d in pos %d req=%d ack=%d",reqfd,i,my_con[i].fds[REQ_LOCAL_FD],my_con[i].fds[ACK_LOCAL_FD]);
      i++;
  }
  if (i == num_local_con){
    state_return_msg(EAR_ERROR,EAR_NOT_FOUND,"Local connection not found");
  }
  //debug("ACK for %d found at pos %d = %d",reqfd,i,my_con[i].fds[ACK_LOCAL_FD]);
  return my_con[i].fds[ACK_LOCAL_FD];
}
state_t del_local_connection(local_connection_t *my_con,job_id jid,job_id sid)
{
  int i=0;
  while (!((my_con[i].jid == jid) && (my_con[i].sid == sid)) && (i<num_local_con)) i++;
  if (i == num_local_con){
    state_return_msg(EAR_ERROR,EAR_NOT_FOUND,"Local connection not found");
  }
  my_con[i].jid = ULONG_MAX;
  my_con[i].sid = 0;
  my_con[i].lid = 0;
  my_con->anonymous = 0;
  return EAR_SUCCESS;
}
int get_local_connection_by_fd(local_connection_t *my_con,int reqfd,int ackfd)
{
  int i=0;
  while (!((my_con[i].fds[REQ_LOCAL_FD] == reqfd) && (my_con[i].fds[ACK_LOCAL_FD] == ackfd)) && (i<num_local_con)) i++;
  if (i == num_local_con){
    state_return_msg(EAR_ERROR,EAR_NOT_FOUND,"Local connection not found");
  }
  return i;
}
int get_local_connection_by_req(local_connection_t *my_con,int reqfd)
{
  int i=0;
  while ((my_con[i].fds[REQ_LOCAL_FD] != reqfd) && (i<num_local_con)) i++;
  if (i == num_local_con){
    state_return_msg(EAR_ERROR,EAR_NOT_FOUND,"Local connection not found");
  }
  return i;
}

void clean_local_connection(local_connection_t *my_con)
{
  my_con->jid = ULONG_MAX;
  my_con->sid = 0;
  my_con->lid = 0;
  my_con->anonymous = 0;
  my_con->fds[REQ_LOCAL_FD] = -1;
  my_con->fds[ACK_LOCAL_FD] = -1;
}

/**********************/

uint is_privileged_service(uint service)
{
  int i = 0;
  while((i < NUM_PRIVILEGED_SERVICES) && (privileged_services[i] != service)) i++;
  if (i<NUM_PRIVILEGED_SERVICES){
    return 1;
  }else{
    return 0;
  }

}


uint is_anonymous(ulong jid,ulong sid)
{
  return ((jid == 0) && (sid == 0));
}


int is_valid_sec_tag(ulong tag) {
#if 1
	// You make define a constant at makefile time called SEC_KEY to use that option
	return ((ulong) SEC_KEY == tag);
#endif
}

#if 0

/* To report data */

int eard_system(eard_head_t *head, int req_fd, int ack_fd) {
	unsigned long ack;
	int ret1;

	if (must_read) {
		if (read(ear_fd_req[system_req], &req, sizeof(req)) != sizeof(req))
			error("error reading info at eard_system(%s)\n", strerror(errno));
	}

	switch (req.req_service) {
		case CONNECT_SYSTEM:
			connect_service(system_req, &req.req_data.app);
			break;
		case WRITE_APP_SIGNATURE:
			powermon_mpi_signature(&req.req_data.app);
			ack = EAR_COM_OK;
			if (write(ear_fd_ack[system_req], &ack, sizeof(ulong)) != sizeof(ulong)) {
				error("ERROR while writing system service ack, closing connection...");
				eard_close_comm();
			}

			break;
		case WRITE_EVENT:
			ack = EAR_COM_OK;
			ret1 = EAR_SUCCESS;
#if USE_DB
		if (my_cluster_conf.eard.use_mysql)
		{
			if (!my_cluster_conf.eard.use_eardbd) {
				ret1 = db_insert_ear_event(&req.req_data.event);
			} else {
				state_t state = eardbd_send_event(&req.req_data.event);
				if (state_fail(state)) {
					error("Error sending event to EARDBD: %s", state_msg);
					eardbd_reconnect(&my_cluster_conf, my_node_conf);
					ret1 = EAR_ERROR;
				}
			}
		}
#endif

			if (ret1 == EAR_SUCCESS) ack = EAR_COM_OK;
			else ack = EAR_COM_ERROR;

			write(ear_fd_ack[system_req], &ack, sizeof(ulong));
			break;
		case WRITE_LOOP_SIGNATURE:
			ack = EAR_COM_OK;
			ret1 = EAR_SUCCESS;
			copy_loop(&current_loop_data,&req.req_data.loop);
			// print_loop_fd(1,&req.req_data.loop);
			if (my_cluster_conf.database.report_loops) {
#if USE_DB
				if (my_cluster_conf.eard.use_mysql)
				{
					if (!my_cluster_conf.eard.use_eardbd){
						ret1 = db_insert_loop (&req.req_data.loop);
					} else {
						state_t state = eardbd_send_loop(&req.req_data.loop);
						if (state_fail(state)){
							error("Error sending loop to EARDBD: %s", state_msg);
							eardbd_reconnect(&my_cluster_conf, my_node_conf);
							ret1 = EAR_ERROR;
						}
					}
				}
#endif
			}

			if (ret1 == EAR_SUCCESS) ack = EAR_COM_OK;
			else ack = EAR_COM_ERROR;

			write(ear_fd_ack[system_req], &ack, sizeof(ulong));
			break;
		default:
			return 0;
	}
	return 1;
}

/************************ FREQUENCY FUNCTIONALLITY: max_freq is the limit *******************/
void eard_set_freq(unsigned long new_freq, unsigned long max_freq) {
	unsigned long ear_ok, freq;
	verbose(VCONF, "setting node frequency . requested %lu, max %lu\n", new_freq, max_freq);
	if (new_freq <= max_freq) {
		freq = new_freq;
	} else {
		warning(" warning, maximum freq is limited to %lu\n", max_freq);
		freq = max_freq;
	}
	ear_ok = frequency_set_all_cpus(freq);
	current_node_freq = freq;
	if (ear_ok != freq) warning(" warning, frequency is not correctly changed\n");
	write(ear_fd_ack[freq_req], &ear_ok, sizeof(unsigned long));
}

int eard_freq(eard_head_t *head, int req_fd, int ack_fd)
{
	char buffer[4096];
	size_t size;
	ulong ack;
	state_t s;
	uint *min;
	uint *max;
	uint *aux;
	ulong *f;

	if (must_read) {
		if (read(ear_fd_req[freq_req], &req, sizeof(req)) != sizeof(req))
			error("error when reading info at eard_freq (%s)\n", strerror(errno));
	}

	switch (req.req_service) {
		case CONNECT_FREQ:
			// HIGHLIGHT: LIBRARY INIT
			connect_service(freq_req, &req.req_data.app);
			break;
		case SET_FREQ:
			eard_set_freq(req.req_data.req_value, min(eard_max_freq, max_dyn_freq()));
			break;
		case SET_FREQ_WITH_MASK:
			ack=frequency_set_with_mask(&req.req_data.f_mask.mask,req.req_data.f_mask.f);
			write(ear_fd_ack[freq_req], &ack, sizeof(unsigned long));
			break;
		case SET_NODE_FREQ_WITH_LIST:
			ack=frequency_set_with_list(req.req_data.cpu_freq.num_cpus,req.req_data.cpu_freq.cpu_freqs);
			write(ear_fd_ack[freq_req], &ack, sizeof(unsigned long));
			break;
		case GET_CPUFREQ:
			ack = frequency_get_cpu_freq(req.req_data.req_value);
			write(ear_fd_ack[freq_req], &ack, sizeof(unsigned long));
			break;
		case GET_CPUFREQ_LIST:
			f=calloc(sizeof(ulong),node_desc.cpu_count);
			ack = frequency_get_cpufreq_list(req.req_data.req_value,f);
			write(ear_fd_ack[freq_req], f,sizeof(ulong)*req.req_data.req_value);
			free(f);
			break;
		case SET_TURBO:
			frequency_set_performance_governor_all_cpus();
			write(ear_fd_ack[freq_req], &ack, sizeof(unsigned long));
			break;
		case END_COMM:
			// HIGHLIGHT: LIBRARY DISPOSE (1/2)
			eard_close_comm();
			break;
		case DATA_SIZE_FREQ:
			ack = sizeof(unsigned long);
			write(ear_fd_ack[freq_req], &ack, sizeof(unsigned long));
			break;
		case READ_CPUFREQ:
			debug("READ_CPUFREQ");
			if (xtate_fail(s, cpufreq_read(&freq_app_req))) {
				error("when reading CPU global frequency requeste by app (%d, %s)", s, state_msg);
			}
			write(ear_fd_ack[freq_req],freq_app_req.context,size_cpufreqs);
			break;
    /* These functions are new for UNC freq management */
		case UNC_SIZE:
			aux = (uint *) buffer;
			aux[1] = unc_count;
			imcfreq_get_api(aux);
			write(ear_fd_ack[freq_req], aux, sizeof(uint)*2);
			break;
		case UNC_READ:
			debug("UNC_READ");
			memset(unc_data, 0, unc_count);
			if (state_fail(s = imcfreq_read(no_ctx, unc_data))){
					error("WHen reading unc frequency requested by app (%d, %s)", s, state_msg);
			}
			write(ear_fd_ack[freq_req], unc_data, unc_count * sizeof(imcfreq_t));
			break;
		case UNC_INIT_DATA:
			memset(&buffer[0], 0, 4096);
			char *p;
			/* This code sends in a single request the API, the num of pstates and the list of pstates */
			/* It's a kind of serialization */
			mgt_imcfreq_get_api((uint *) &buffer[0]);
			mgt_imcfreq_get_available_list(&imc_eard_ctx, (const pstate_t **) &p, (uint *) &buffer[sizeof(uint)]);
			memcpy(&buffer[sizeof(uint)*2], p, ((uint) buffer[sizeof(uint)])*sizeof(pstate_t));
			write(ear_fd_ack[freq_req], buffer, 4096);
		break;
		case UNC_GET_LIMITS:
			memset(&buffer[0], 0, 4096);
			if (state_fail(s = mgt_imcfreq_get_current_list(&imc_eard_ctx, (pstate_t *) &buffer[0]))){
				error("When reading IMC current frequency (%d, %s)", s, state_msg);
			}
			write(ear_fd_ack[freq_req], buffer, node_desc.socket_count*sizeof(pstate_t));
		break;
		case UNC_SET_LIMITS:
			ack = EAR_SUCCESS;
			if (state_fail(s = mgt_imcfreq_set_current_list(&imc_eard_ctx, req.req_data.unc_freq.index_list))){
				error("When setting imc limits req by app (%d, %s)", s, state_msg);
				ack = (ulong) s;
			}
			write(ear_fd_ack[freq_req], &ack, sizeof(ulong));
		break;
		case UNC_GET_LIMITS_MIN:
			size = node_desc.socket_count * sizeof(pstate_t);
			memset(&buffer[0], 0, size*2);
			if (state_fail(s = mgt_imcfreq_get_current_ranged_list(&imc_eard_ctx, (pstate_t *) &buffer[0], (pstate_t *) &buffer[size]))){
				error("When reading IMC current frequency (%d, %s)", s, state_msg);
			}
			write(ear_fd_ack[freq_req], buffer, size*2);
		break;
		case UNC_SET_LIMITS_MIN:
			ack = EAR_SUCCESS;
			max = &req.req_data.unc_freq.index_list[0];
			min = &req.req_data.unc_freq.index_list[node_desc.socket_count];
			if (state_fail(s = mgt_imcfreq_set_current_ranged_list(&imc_eard_ctx, max, min))){
				error("When setting imc limits req by app (%d, %s)", s, state_msg);
				ack = (ulong) s;
			}
			write(ear_fd_ack[freq_req], &ack, sizeof(ulong));
		break;

		default:
			return 0;
	}
	return 1;
}
#endif

int eard_uncore(eard_head_t *head, int req_fd, int ack_fd) {

	unsigned long ack = EAR_SUCCESS;
	unsigned long long values[num_uncore_counters];


	switch (head->req_service) {
		case START_UNCORE:
			start_uncores();
			write(ack_fd, &ack, sizeof(ack));
			break;
		case RESET_UNCORE:
			reset_uncores();
			write(ack_fd, &ack, sizeof(ack));
			break;
		case READ_UNCORE: {
			unsigned long demon_cas = 0;
			int i;
			read_uncores(values);
			write(ack_fd, values, sizeof(unsigned long long) * num_uncore_counters);
			for (i = 0; i < num_uncore_counters; i++) demon_cas += values[i];
			break;
		}
		case STOP_UNCORE: {
			unsigned long demon_cas = 0;
			int i;
			stop_uncores(values);
			write(ack_fd, values, sizeof(unsigned long long) * num_uncore_counters);
			for (i = 0; i < num_uncore_counters; i++) demon_cas += values[i];
			break;
		}

		case DATA_SIZE_UNCORE:
			ack = sizeof(unsigned long long) * num_uncore_counters;
			verbose(1,"Uncore size %lu",ack);
			write(ack_fd, &ack, sizeof(unsigned long));
			break;
		default:
			return 0;

	}
	return 1;
}

/***************** GPU SERVICES ****************/
int eard_gpu(eard_head_t *head, int req_fd, int ack_fd) 
{
	unsigned long ack = 0;
	#if USE_GPUS
	gpu_info_t *info;
	gpu_info_t fake;

	switch (head->req_service) {
		case GPU_MODEL:
			debug("GPU_MODEL %d",eard_gpu_model);
			write(ack_fd,&eard_gpu_model,sizeof(eard_gpu_model));	
		break;
		case GPU_DEV_COUNT:
			debug("GPU_DEV_COUNT %d",eard_num_gpus);
			write(ack_fd,&eard_num_gpus,sizeof(eard_num_gpus));	
			break;
		case GPU_GET_INFO:
			debug("GPU_GET_INFO %d", eard_num_gpus);
			if (eard_gpu_initialized) {
				gpu_get_info(&eard_main_gpu_ctx, (const gpu_info_t **) &info, NULL);
			} else {
				memset(&fake, 0, sizeof(gpu_info_t));
				*info = fake;
			}
			write(ack_fd, info, sizeof(gpu_info_t)*eard_num_gpus);	
			break;
		case GPU_DATA_READ:
			if (eard_gpu_initialized){
				gpu_read(&eard_main_gpu_ctx,eard_read_gpu);
			}else{
				memset(eard_read_gpu,0,sizeof(gpu_t));
			}
			debug("Sending %lu bytes en gpu_data_read",sizeof(gpu_t)*eard_num_gpus);
			gpu_data_tostr(eard_read_gpu,gpu_str,sizeof(gpu_str));
			debug("gpu_data_read info: %s",gpu_str);
			write(ack_fd,eard_read_gpu,sizeof(gpu_t)*eard_num_gpus);
			break;
		case GPU_SET_FREQ:
			if (eard_gpu_initialized){
				debug("Setting the GPU freq");
				gpu_mgr_set_freq(req.req_data.gpu_freq.num_dev,req.req_data.gpu_freq.gpu_freqs);
			}else{
				error("GPU not initialized and GPU_SET_FREQ requested");
			}
			ack=EAR_SUCCESS;
			write(ack_fd,&ack,sizeof(ack));
			break;
	  default:
			error("Invalid GPU command");
      return 0;
  }
	#else
	switch (head->req_service) {
    case GPU_MODEL:
		case GPU_DEV_COUNT:
		case GPU_DATA_READ:
		case GPU_SET_FREQ:
		case GPU_GET_INFO:
			ack=EAR_ERROR;
			write(ack_fd,&ack,sizeof(ack));
			error("GPUS not supported");
			break;
		default: break;
		}
	return 0;
	#endif
  return 1;
}

void clean_global_connector()
{
  close(global_req_fd);
  unlink(ear_commreq_global);
}
// Creates a pipe to receive requests for applications. eard creates 1 pipe (per node) and service to receive requests
void create_global_connector(char *ear_tmp, char *nodename) {
  sprintf(ear_commreq_global, "%s/.ear_comm.globalreq", ear_tmp);
	verbose(1,"Global connector placed at %s",ear_commreq_global);
  // ear_comreq files will be used to send requests from the library to the eard
  if (mknod(ear_commreq_global, S_IFIFO | S_IRUSR | S_IRGRP | S_IWUSR | S_IWGRP | S_IROTH | S_IWOTH, 0) < 0) {
    if (errno != EEXIST) {
      error("Error creating ear global communicator for requests %s\n", strerror(errno));
    }
  }
  if (chmod(ear_commreq_global, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)<0){
    error("Error setting privileges for eard-earl connection");
  }
  global_req_fd = open(ear_commreq_global,O_RDWR);
  if (global_req_fd < 0){
    error("Error creating global communicator for connections");
    _exit(0);
  }
  new_fd(&rfds_basic,global_req_fd,&numfds_req,&total_local_fd);
}
void connect_service(struct daemon_req *new_req)
{
  char ear_commack[MAX_PATH_SIZE * 2];
  char ear_commreq[MAX_PATH_SIZE * 2];
  application_t *new_app = &new_req->req_data.app;
  job_t *new_job = &new_app->job;
  unsigned long ack;
  int ear_req_fd,ear_ack_fd;
  int alive;
  int newc,is_new;
  ulong lid = new_req->con_id.lid;
  int pid = create_ID(new_job->id, new_job->step_id);
	uint ID = (uint)pid;
  // Let's check if there is another application
  verbose(1, "request for connection at service (%lu,%lu)",new_job->id,new_job->step_id);


  /* Creates 1 pipe (per node),  jobid and step id to send acks. pid is a combination of jobid and stepid */
  verbose(VEARD + 1, "New local connection job_id=%lu step_id=%lu local_id %lu\n", new_job->id, new_job->step_id,new_req->con_id.lid);
  /* This pipe is to send data, 1 per connection */
  sprintf(ear_commack, "%s/%u/.ear_comm.ack_0.%d.%lu", ear_tmp, ID, pid,lid);
  sprintf(ear_commreq, "%s/%u/.ear_comm.req_0.%d.%lu", ear_tmp, ID, pid,lid);
  debug("Comm channels %s and %s",ear_commack,ear_commreq);
  /* We must create a new local connection, for now, 1 connection per job is allowed  */
  if ((newc = add_new_local_connection(eard_local_conn,new_job->id,new_job->step_id,new_req->con_id.lid)) <0){
      error("Error opening slot for new local connection");
  }
  debug("application %d.%lu asks for new connection ", pid,lid);
	verbose(1,"Opening %s",ear_commack);
  ear_ack_fd = open(ear_commack, O_WRONLY);
  if (ear_ack_fd < 0) {
        error("ERROR while opening ack_private pipe %s (%s)", ear_commack, strerror(errno));
  }
  /* We must associate the ack pipe with th local connection */
  if (add_local_fd(&eard_local_conn[newc],ear_ack_fd,ACK_LOCAL_FD) != EAR_SUCCESS){
        error("Opening adding local ack newc");
  }
  //new_fd(&rfds_basic,ear_ack_fd,&numfds_req,&total_local_fd);
  /* we send and ack to the earl */
  if (write(ear_ack_fd, &ack, sizeof(ack)) != sizeof(ack)) {
        warning("WARNING while sending ack to  %d", pid);
    eard_close_comm(-1,ear_ack_fd);
  }
  /* we open the local req pipe */
	verbose(1,"Opening %s", ear_commreq);
  ear_req_fd = open(ear_commreq, O_RDONLY);
  if (ear_req_fd < 0){
        error("ERROR while opening req_private pipe %s (%s)", ear_commreq, strerror(errno));
        eard_close_comm(-1,ear_ack_fd);
  }
  if (add_local_fd(&eard_local_conn[newc],ear_req_fd,REQ_LOCAL_FD) != EAR_SUCCESS){
        error("Opening adding local ack newc");
  }
  debug("New communication FDs created for application FD[%d]=%d,%d",newc,ear_ack_fd,ear_req_fd);
  new_fd(&rfds_basic,ear_req_fd,&numfds_req,&total_local_fd);
  /* This function notifies the local connection */
  if (!eard_local_conn[newc].anonymous) powermon_mpi_init(&handler_energy, new_app);
  debug("Application %d connected with local API (anonymous %lu)",pid,eard_local_conn[newc].anonymous);
} 

// HIGHLIGHT: LIBRARY DISPOSE (2/2)
void eard_close_comm(int req_fd,int ack_fd)
{
  int i, ret;
  int id;
  ulong jid,sid;
  ulong was_anonymous = 1;
  char ear_commack[MAX_PATH_SIZE * 2];
  char ear_commreq[MAX_PATH_SIZE * 2];

  /* This is a special case where one of the fds has been closed */
  if ((req_fd == -1) && (ack_fd == -1)){
    for (i=0;i<numfds_req;i++){
			/* What about was_anonymous ? */
      if (check_fd(&rfds_basic,i) == EAR_ERROR){
        id = get_local_connection_by_req(eard_local_conn,i);
        close(i);
        close(eard_local_conn[id].fds[ACK_LOCAL_FD]);
        clean_fd(&rfds_basic,i,&numfds_req,&total_local_fd);
        clean_fd(&rfds_basic,eard_local_conn[id].fds[ACK_LOCAL_FD],&numfds_req,&total_local_fd);
        clean_local_connection(&eard_local_conn[id]);
      }
    }
  }else if (req_fd == CLOSE_PIPE){
		/* SIGPIPE received  in eard */
		verbose(1,"SIGPIPE received in EARD: PENDING");
	}else if (req_fd == CLOSE_EARD){
		/* End eard */
		verbose(1,"Releasing all the files: PENDING");
	}else{ /* this is the normal use case */
    id = get_local_connection_by_fd(eard_local_conn,req_fd,ack_fd);
    if (id < 0){
      error("We cannot find (%d,%d) in the list of local connections",req_fd,ack_fd);
      return;
    }
    debug("Closing communication for (%lu,%lu)",eard_local_conn[id].jid,eard_local_conn[id].sid);
    jid = eard_local_conn[id].jid;
    sid = eard_local_conn[id].sid;
		ulong lid = eard_local_conn[id].lid;
		uint ID = create_ID(jid, sid);
  	sprintf(ear_commack, "%s/%u/.ear_comm.ack_0.%u.%lu", ear_tmp, ID, ID,lid);
  	sprintf(ear_commreq, "%s/%u/.ear_comm.req_0.%u.%lu", ear_tmp, ID, ID,lid);
		verbose(1,"Cleaning files for job %lu/%lu/%lu",jid,sid,lid);
		verbose(1,"Deleting %s",ear_commack);
		ret = unlink(ear_commack);
		if (ret != ENOENT) verbose(1,"Error deleting file %s",strerror(errno));
		verbose(1,"Deleting %s",ear_commreq);
		ret = unlink(ear_commreq);
		if (ret != ENOENT) verbose(1,"Error deleting file %s",strerror(errno));
    was_anonymous = eard_local_conn[id].anonymous;
    if (req_fd != -1){
      close(req_fd);
      clean_fd(&rfds_basic,req_fd,&numfds_req,&total_local_fd);
    }
    if (ack_fd != -1){
      close(ack_fd);
      clean_fd(&rfds_basic,ack_fd,&numfds_req,&total_local_fd);
    }
    debug("application (%lu,%lu) disconnected (anonymous %lu)", jid,sid,was_anonymous);
    clean_local_connection(&eard_local_conn[id]);
    // This debug was set outside, but it requires a valid id to prevent segmentation faults.
  }
  if (!was_anonymous) {
    powermon_mpi_finalize(&handler_energy,jid,sid);
  }
}
/************************* Node_energy services ******************/
int eard_node_energy(eard_head_t *head,int req_fd,int ack_fd) {
  unsigned long ack;
  switch (head->req_service) {
    case READ_DC_ENERGY:
      energy_dc_read(&handler_energy, node_energy_data);
      write(ack_fd, node_energy_data, node_energy_datasize);
      break;
    case DATA_SIZE_ENERGY_NODE:
      energy_datasize(&handler_energy, &ack);
      write(ack_fd, &ack, sizeof(unsigned long));
      break;
    case ENERGY_FREQ:
      ack = (ulong) energy_freq;
      write(ack_fd, &ack, sizeof(unsigned long));
      break;
/* Add new_services for RAPL here. Use req_fd to read pending data and ack_fd to send data*/
    default:
      return 0;
  }
  return 1;

}
/********* SYSTEM services ***************/
int eard_system(eard_head_t *head,int req_fd,int ack_fd)
{
  unsigned long ack;
  int ret1;
  int con_id;
  con_id = get_local_connection_by_fd(eard_local_conn,req_fd,ack_fd);

  switch (head->req_service) {
    case WRITE_APP_SIGNATURE:
			verbose(1,"App signature received: Anonymous %lu", eard_local_conn[con_id].anonymous);
      if (!eard_local_conn[con_id].anonymous) powermon_mpi_signature(&req.req_data.app);
      ack = EAR_COM_OK;
      if (write(ack_fd, &ack, sizeof(ulong)) != sizeof(ulong)) {
        error("ERROR while writing system service ack, closing connection...");
        eard_close_comm(req_fd,ack_fd);
      }

      break;
    case WRITE_EVENT:
      ack = EAR_COM_OK;
      ret1 = EAR_SUCCESS;
#if USE_DB
    if (my_cluster_conf.eard.use_mysql)
    {
      if (!my_cluster_conf.eard.use_eardbd) {
        ret1 = db_insert_ear_event(&req.req_data.event);
      } else {
        state_t state = eardbd_send_event(&req.req_data.event);
        if (state_fail(state)) {
          error("Error sending event to EARDBD: %s", state_msg);
          eardbd_reconnect(&my_cluster_conf, my_node_conf);
          ret1 = EAR_ERROR;
        }
      }
    }
#endif
      if (ret1 == EAR_SUCCESS) ack = EAR_COM_OK;
      else ack = EAR_COM_ERROR;

      write(ack_fd, &ack, sizeof(ulong));
      break;
    case WRITE_LOOP_SIGNATURE:
      ack = EAR_COM_OK;
      ret1 = EAR_SUCCESS;
      powermon_loop_signature(eard_local_conn[con_id].jid,eard_local_conn[con_id].sid,&req.req_data.loop);
      // print_loop_fd(1,&req.req_data.loop);
      if (my_cluster_conf.database.report_loops) {
#if USE_DB
        if (my_cluster_conf.eard.use_mysql)
        {
          if (!my_cluster_conf.eard.use_eardbd){
            ret1 = db_insert_loop (&req.req_data.loop);
          } else {
            state_t state = eardbd_send_loop(&req.req_data.loop);
            if (state_fail(state)){
              error("Error sending loop to EARDBD: %s", state_msg);
              eardbd_reconnect(&my_cluster_conf, my_node_conf);
              ret1 = EAR_ERROR;
            }
          }
        }
#endif
      }

      if (ret1 == EAR_SUCCESS) ack = EAR_COM_OK;
      else ack = EAR_COM_ERROR;

      write(ack_fd, &ack, sizeof(ulong));
      break;
/* Add new_services for RAPL here. Use req_fd to read pending data and ack_fd to send data*/
    default:
      return 0;
  }
  return 1;
}
/************************ FREQUENCY FUNCTIONALLITY: max_freq is the limit *******************/
void eard_set_freq(int ack_fd,unsigned long new_freq, unsigned long max_freq) {
  unsigned long ear_ok, freq;
  verbose(VCONF, "setting node frequency . requested %lu, max %lu\n", new_freq, max_freq);
  if (new_freq <= max_freq) {
    freq = new_freq;
  } else {
    warning(" warning, maximum freq is limited to %lu\n", max_freq);
    freq = max_freq;
  }
  ear_ok = frequency_set_all_cpus(freq);
  if (ear_ok != freq) warning(" warning, frequency is not correctly changed\n");
  write(ack_fd, &ear_ok, sizeof(unsigned long));
}
int eard_freq(eard_head_t *head, int req_fd, int ack_fd)
{
  long aux_long[2];
  size_t size;
  state_t s;
  ulong ack;
  ulong *f;

  switch (head->req_service) {
		//case CONNECT_FREQ:
    case CONNECT_EARD_NODE_SERVICES:
      connect_service(&req);
    break;
    case SET_FREQ:
      eard_set_freq(ack_fd,req.req_data.req_value, ear_min(eard_max_freq, max_dyn_freq()));
      break;
		case SET_TURBO:
			frequency_set_performance_governor_all_cpus();
			write(ack_fd, &ack, sizeof(unsigned long));
			break;
    case SET_FREQ_WITH_MASK:
      ack=frequency_set_with_mask(&req.req_data.f_mask.mask,req.req_data.f_mask.f);
      write(ack_fd, &ack, sizeof(unsigned long));
      break;
    case SET_NODE_FREQ_WITH_LIST:
      ack=frequency_set_with_list(req.req_data.cpu_freq.num_cpus,req.req_data.cpu_freq.cpu_freqs);
      write(ack_fd, &ack, sizeof(unsigned long));
      break;
    case GET_CPUFREQ:
      ack = frequency_get_cpu_freq(req.req_data.req_value);
      write(ack_fd, &ack, sizeof(unsigned long));
      break;
    case GET_CPUFREQ_LIST:
      f=calloc(sizeof(ulong),node_desc.cpu_count);
      ack = frequency_get_cpufreq_list(req.req_data.req_value,f);
      write(ack_fd, f,sizeof(ulong)*req.req_data.req_value);
      free(f);
      break;
		//case END_COMM:
    case DISCONNECT_EARD_NODE_SERVICES:
      // HIGHLIGHT: LIBRARY DISPOSE (1/2)
      eard_close_comm(req_fd,ack_fd);
      break;
    case DATA_SIZE_FREQ:
      ack = sizeof(unsigned long);
      write(ack_fd, &ack, sizeof(unsigned long));
      break;
    case READ_CPUFREQ:
      //if (xtate_fail(s, cpufreq_read(&cpu_c, freq_app_req))) {
      if (xtate_fail(s, cpufreq_read(&freq_app_req))) {
        error("when reading CPU global frequency requeste by app (%d, %s)", s, state_msg);
      }
      write(ack_fd, freq_app_req.context, size_cpufreqs);
      break;
    default:
      return 0;
  }
  return 1;
}

int eard_imc(eard_head_t *head,int req_fd,int ack_fd) {
  unsigned long ack = 0;
  int ret;
  uint *min;
  uint *max;
  char buffer[4096];
  size_t size;
  uint *aux;


  switch (head->req_service) {
			case UNC_SIZE:
			aux = (uint *) buffer;
			aux[1] = unc_count;
			imcfreq_get_api(aux);
			write(ack_fd, aux, sizeof(uint)*2);
			break;
		case UNC_READ:
			memset(unc_data, 0, unc_count);
			if (state_fail(s = imcfreq_read(no_ctx, unc_data))){
					error("WHen reading unc frequency requested by app (%d, %s)", s, state_msg);
			}
			write(ack_fd, unc_data, unc_count * sizeof(imcfreq_t));
			break;
		case UNC_INIT_DATA:
			memset(&buffer[0], 0, 4096);
			char *p;
			/* This code sends in a single request the API, the num of pstates and the list of pstates */
			/* It's a kind of serialization */
			mgt_imcfreq_get_api((uint *) &buffer[0]);
			mgt_imcfreq_get_available_list(&imc_eard_ctx, (const pstate_t **) &p, (uint *) &buffer[sizeof(uint)]);
			memcpy(&buffer[sizeof(uint)*2], p, ((uint) buffer[sizeof(uint)])*sizeof(pstate_t));
			write(ack_fd, buffer, 4096);
		break;
		case UNC_GET_LIMITS:
			memset(&buffer[0], 0, 4096);
			if (state_fail(s = mgt_imcfreq_get_current_list(&imc_eard_ctx, (pstate_t *) &buffer[0]))){
				error("When reading IMC current frequency (%d, %s)", s, state_msg);
			}
			write(ack_fd, buffer, node_desc.socket_count*sizeof(pstate_t));
		break;
		case UNC_SET_LIMITS:
			ack = EAR_SUCCESS;
			if (state_fail(s = mgt_imcfreq_set_current_list(&imc_eard_ctx, req.req_data.unc_freq.index_list))){
				error("When setting imc limits req by app (%d, %s)", s, state_msg);
				ack = (ulong) s;
			}
			write(ack_fd, &ack, sizeof(ulong));
		break;
		case UNC_GET_LIMITS_MIN:
			size = node_desc.socket_count * sizeof(pstate_t);
			memset(&buffer[0], 0, size*2);
			if (state_fail(s = mgt_imcfreq_get_current_ranged_list(&imc_eard_ctx, (pstate_t *) &buffer[0], (pstate_t *) &buffer[size]))){
				error("When reading IMC current frequency (%d, %s)", s, state_msg);
			}
			write(ack_fd, buffer, size*2);
		break;
		case UNC_SET_LIMITS_MIN:
			ack = EAR_SUCCESS;
			max = &req.req_data.unc_freq.index_list[0];
			min = &req.req_data.unc_freq.index_list[node_desc.socket_count];
			if (state_fail(s = mgt_imcfreq_set_current_ranged_list(&imc_eard_ctx, max, min))){
				error("When setting imc limits req by app (%d, %s)", s, state_msg);
				ack = (ulong) s;
			}
			write(ack_fd, &ack, sizeof(ulong));
		break;
	default:
      return 0;
	}
	return 1;
}
/*******************+ RAPL SERVICES *******************/
int eard_rapl(eard_head_t *head,int req_fd,int ack_fd) {
  unsigned long ack = 0;
  int ret;
  switch (head->req_service) {
    case START_RAPL:
      error("starting rapl msr not supported");
      RAPL_counting = 1;
      write(ack_fd, &ack, sizeof(ack));
      break;
    case RESET_RAPL:
      error("Reset RAPL counters not provided");
      write(ack_fd, &ack, sizeof(ack));
      break;
    case READ_RAPL:
      if (read_rapl_msr(fd_rapl,values_rapl)<0) error("Error reading rapl msr counters");
      write(ack_fd, values_rapl, sizeof(unsigned long long) * RAPL_POWER_EVS*num_packs);
      break;
    case DATA_SIZE_RAPL:
      ack = sizeof(unsigned long long) * RAPL_POWER_EVS*num_packs;
      write(ack_fd, &ack, sizeof(unsigned long));
      break;
    /* Add new_services for RAPL here. Use req_fd to read pending data and ack_fd to send data*/
    default:
      return 0;
  }
  return 1;
}




/* New services, to be included one by one */
#if 0
int service_cpufreq(eard_head_t *head, int req_fd, int ack_fd)
{
	uint call = head->req_service;
	state_t s = EAR_SUCCESS;
	char *data = NULL;
	uint aux_int[2];
	ulong aux_long;
	size_t size = 0;

	switch (call) {
	case RPC_MGT_CPUFREQ_GET_API:
		sfail(s = mgt_cpufreq_get_api((uint *) &aux_int[0])) {
			serror("Could not get management cpufreq API");
		}
		data = (char *) &aux_int[0];
		size = sizeof(uint);
	break;
	case RPC_MGT_CPUFREQ_GET_AVAILABLE:
		data = (char *) mcpu_ps_ava;
		size = mcpu_ps_ava_count * sizeof(pstate_t);
	break;
	case RPC_MGT_CPUFREQ_GET_CURRENT:
		sfail(s = mgt_cpufreq_get_current_list(&mcpu_c, mcpu_ps_cur)) {
			serror("Could not get the current management frequency");
		}
		data = (char *) mcpu_ps_cur;
		size = mcpu_dev_count * sizeof(pstate_t);
	break;
	case RPC_MGT_CPUFREQ_GET_NOMINAL:
		sfail(s = mgt_cpufreq_get_nominal(&mcpu_c, &aux_int[0])) {
			serror("Could not get the current nominal frequency");
		}
		data = (char *) &aux_int[0];
		size = sizeof(uint);
	break;
	case RPC_MGT_CPUFREQ_GET_GOVERNOR:
		sfail(s = mgt_cpufreq_get_governor(&mcpu_c, &aux_int[0])) {
			serror("Could not get the current governor");
		}
		data = (char *) &aux_int[0];
		size = sizeof(uint);
	break;
	case RPC_MGT_CPUFREQ_SET_CURRENT_LIST:
		aux_long = mcpu_dev_count * sizeof(uint);
		if (state_fail(s = eard_rpc_read_pending(req_fd, (char *) mcpu_ps_idx, head->size, aux_long))) {
			serror(Rpcerr.pending);
		}
		if (state_ok(s)) {
			if (state_fail(s = mgt_cpufreq_set_current_list(&mcpu_c, mcpu_ps_idx))) {
				serror("Error when setting a CPU frequency list");
			}
		}
	break;
	case RPC_MGT_CPUFREQ_SET_CURRENT:
		aux_long = sizeof(uint) + sizeof(int);
		sfail(s = eard_rpc_read_pending(req_fd, (char *) aux_int, head->size, aux_long)) {
			serror(Rpcerr.pending);
		}
		if (state_ok(s)) {
			sfail(s = mgt_cpufreq_set_current(&mcpu_c, aux_int[0], (int) aux_int[1])) {
				serror("Error when setting a CPU frequency");
			}
		}
	break;
	case RPC_MGT_CPUFREQ_SET_GOVERNOR:
		sfail(s = eard_rpc_read_pending(req_fd, (char *) &aux_int[0], head->size, sizeof(uint))) {
			serror(Rpcerr.pending);
		}
		if (state_ok(s)) {
			sfail(s = mgt_cpufreq_set_governor(&mcpu_c, aux_int[0])) {
				serror(Rpcerr.answer);
			}
		}
	break;
	case RPC_MET_CPUFREQ_GET_API:
		sfail(s = cpufreq_get_api((uint *) &aux_int[0])) {
			serror("Could not get metrics cpufreq API");
		}
		data = (char *) &aux_int[0];
		size = sizeof(uint);
	break;
	default:
		return 0;
	}
	// Always answering
	sfail(eard_rpc_answer(ack_fd, call, s, data, size, state_msg)) {
		return 0;
	}
	return 1;
}

int service_imcfreq(eard_head_t *head, int req_fd, int ack_fd)
{
	uint call = head->req_service;
	state_t s = EAR_SUCCESS;
	char *data = NULL;
	size_t size = 0;
	uint aux_int;

	switch (call) {
	case RPC_MGT_IMCFREQ_GET_API:
		if (state_fail(s = mgt_imcfreq_get_api((uint *) &aux_int))) {
			serror("Could not get management imcfreq API");
		}
		data = (char *) &aux_int;
		size = sizeof(uint);
	break;
	case RPC_MGT_IMCFREQ_GET_AVAILABLE_PS:
		serial_clean(&wide);
		serial_add_elem(&wide, (char *) &mimc_ps_ava_count, sizeof(uint));
		serial_add_elem(&wide, (char *) mimc_ps_ava, sizeof(pstate_t)*mimc_ps_ava_count);
		size = serial_size(&wide, 0);
		data = wide.data;
	break;
	case RPC_MGT_IMCFREQ_GET_CURRENT_PS:
		if (state_fail(s = mgt_imcfreq_get_current_list(&mimc_c, mimc_ps_cur1))) {
			serror("Could not get IMC current frequency");
		}
		data = (char *) mimc_ps_cur1;
		size = sizeof(pstate_t) * mimc_dev_count;
	break;
	case RPC_MGT_IMCFREQ_SET_CURRENT_PS:
		if (state_fail(s = eard_rpc_read_pending(req_fd, (char *) mimc_ps_idx1, head->size, 0))) {
			serror(Rpcerr.pending);
		}
		if (state_fail(s = mgt_imcfreq_set_current_list(&mimc_c, mimc_ps_idx1))) {
			serror("Could not set IMC current frequency");
		}
	break;
	case RPC_MGT_IMCFREQ_GET_CURRENT_PS_MIN:
		serial_clean(&wide);
		if (state_fail(s = mgt_imcfreq_get_current_ranged_list(&mimc_c, mimc_ps_cur1, mimc_ps_cur2))) {
			serror("Could not get IMC current ranged frequency");
		} else {
			serial_add_elem(&wide, (char *) mimc_ps_cur1, sizeof(pstate_t)*mimc_dev_count);
			serial_add_elem(&wide, (char *) mimc_ps_cur2, sizeof(pstate_t)*mimc_dev_count);
			size = serial_size(&wide, 0);
			data = wide.data;
		}
	break;
	case RPC_MGT_IMCFREQ_SET_CURRENT_PS_MIN:
		serial_assign(&wide, head->size);
		if (state_fail(s = eard_rpc_read_pending(req_fd, serial_data(&wide), head->size, 0))) {
			serror(Rpcerr.pending);
		} else {
			serial_copy_elem(&wide, (char *) mimc_ps_idx1, NULL);
			serial_copy_elem(&wide, (char *) mimc_ps_idx2, NULL);
			s = mgt_imcfreq_set_current_ranged_list(&mimc_c, mimc_ps_idx1, mimc_ps_idx2);
		}
	break;
	case RPC_MET_IMCFREQ_GET_API:
		imcfreq_get_api((uint *) &aux_int);
		data = (char *) &aux_int;
		size = sizeof(uint);
	break;
	default:
		return 0;
	}
	// Always answering
	if (state_fail(eard_rpc_answer(ack_fd, call, s, data, size, state_msg))) {
		return 0;
	}
	return 1;
}

int service_bwidth(eard_head_t *head,int req_fd,int ack_fd) {

	uint call = head->req_service;
	state_t s = EAR_SUCCESS;
	char *data = NULL;
	size_t size = 0;
	uint aux;

	switch (req.req_service) {
		case RPC_MET_BWIDTH_GET_API:
			if (state_fail(s = bwidth_get_api((uint *) &aux))) {
				serror("Could not get metrics bandwidth API");
			}
			data = (char *) &aux;
			size = sizeof(uint);
		break;
		case RPC_MET_BWIDTH_COUNT_DEVICES:
			data = (char *) &ram_dev_count;
			size = sizeof(uint);
		break;
		case RPC_MET_BWIDTH_READ:
			if (state_fail(s = bwidth_read(&ram_c, ram_mets))) {
				serror("Could not get metrics bandwidth API");
			}
			data = (char *) ram_mets;
			size = sizeof(bwidth_t) * ram_dev_count;
		break;
		default:
			return 0;
	}
	if (state_fail(eard_rpc_answer(ack_fd, call, s, data, size, state_msg))) {
		return 0;
	}
	return 1;
}

int service_gpu(eard_head_t *head, int req_fd, int ack_fd)
{
	uint call = head->req_service;
	state_t s = EAR_SUCCESS;
	char *data = NULL;
	size_t size = 0;
	uint aux_int;
	int i;
		
	switch (call) {
	case RPC_MGT_GPU_GET_API:
 		sfail(s = mgt_gpu_get_api((uint *) &aux_int)) {
	        serror("Could not get management cpufreq API");
        }
		data = (char *) &aux_int;
		size = sizeof(uint);
	break;
	case RPC_MGT_GPU_COUNT_DEVICES:
		data = (char *) &mgpu_dev_count;
		size = sizeof(uint);
	break;
	case RPC_MGT_GPU_GET_FREQ_LIMIT_DEFAULT:
		data = (char *) mgpu_ps_def;
		size = sizeof(ulong)*mgpu_dev_count;
	break;
	case RPC_MGT_GPU_GET_FREQ_LIMIT_MAX:
		data = (char *) mgpu_ps_max;
		size = sizeof(ulong)*mgpu_dev_count;
	break;
	case RPC_MGT_GPU_GET_FREQ_LIMIT_CURRENT:
		sfail (s = mgt_gpu_freq_limit_get_current(&mgpu_c, mgpu_ps_cur)) {
	        serror("Could not get current GPU frequency limit");
		}
		data = (char *) mgpu_ps_cur;
		size = sizeof(ulong)*mgpu_dev_count;
	break;
	case RPC_MGT_GPU_GET_FREQS_AVAILABLE:
		serial_clean(&wide);
		serial_add_elem(&wide, (char *) mgpu_ps_ava_count, sizeof(uint)*mgpu_dev_count);
		for (i = 0; i < mgpu_dev_count; ++i) {
				serial_add_elem(&wide, (char *) mgpu_ps_ava[i], sizeof(ulong)*mgpu_ps_ava_count[i]);
		}
		size = serial_size(&wide, 0);
		data = wide.data;
	break;
	case RPC_MGT_GPU_RESET_FREQ_LIMIT:
		s = mgt_gpu_freq_limit_reset(&mgpu_c);
	break;
	case RPC_MGT_GPU_SET_FREQ_LIMIT:
		sfail(s = eard_rpc_read_pending(req_fd, (char *) mgpu_ps_cur, head->size, 0)) {
			serror(Rpcerr.pending);
		}
		if (state_ok(s)) {
			sfail(s = mgt_gpu_freq_limit_set(&mgpu_c, mgpu_ps_cur)) {
				serror("Error when setting a GPU frequency limit");
			}
		}
	break;
	case RPC_MGT_GPU_GET_POWER_CAP_DEFAULT:
		data = (char *) mgpu_pow_def;
		size = sizeof(ulong)*mgpu_dev_count;
	break;
	case RPC_MGT_GPU_GET_POWER_CAP_MAX:
		serial_clean(&wide);
		serial_add_elem(&wide, (char *) mgpu_pow_max, sizeof(ulong)*mgpu_dev_count);
		serial_add_elem(&wide, (char *) mgpu_pow_min, sizeof(ulong)*mgpu_dev_count);
		size = serial_size(&wide, 0);
		data = wide.data;
	break;
	case RPC_MGT_GPU_GET_POWER_CAP_CURRENT:
		sfail (s = mgt_gpu_power_cap_get_current(&mgpu_c, mgpu_pow_cur)) {
	        serror("Could not get current GPU power limit");
		}
		data = (char *) mgpu_pow_cur;
		size = sizeof(ulong)*mgpu_dev_count;
	break;
	case RPC_MGT_GPU_RESET_POWER_CAP:
		s = mgt_gpu_power_cap_reset(&mgpu_c);
	break;
	case RPC_MGT_GPU_SET_POWER_CAP:
        sfail(s = eard_rpc_read_pending(req_fd, (char *) mgpu_pow_cur, head->size, 0)) {
            serror(Rpcerr.pending);
        }
        if (state_ok(s)) {
            sfail(s = mgt_gpu_power_cap_set(&mgpu_c, mgpu_pow_cur)) {
                serror("Error when setting a GPU power cap");
            }
        }
	break;
	case RPC_MET_GPU_GET_API:
		sfail(s = gpu_get_api((uint *) &aux_int)) {
		serror("Could not get management cpufreq API");
		}
		data = (char *) &aux_int;
		size = sizeof(uint);
	break;
	case RPC_MET_GPU_COUNT_DEVICES:
		data = (char *) &gpu_dev_count;
		size = sizeof(uint);
	break;
	case RPC_MET_GPU_GET_METRICS:
		sfail(gpu_read(&gpu_c, gpu_mets)) {
			serror("Could not get management GPU metrics");
		}
		data = (char *) gpu_mets;
		size = sizeof(gpu_t) * gpu_dev_count;
	break;
	case RPC_MET_GPU_GET_METRICS_RAW:
		sfail(gpu_read_raw(&gpu_c, gpu_mets)) {
			serror("Could not get management GPU metrics");
		}
		data = (char *) gpu_mets;
		size = sizeof(gpu_t) * gpu_dev_count;
	break;
	default:
		return 0;
	}
	// Always answering
	sfail(eard_rpc_answer(ack_fd, call, s, data, size, state_msg)) {
		return 0;
	}
	return 1;
}

#endif
#if 0
void select_service(int fd) {
	if (read(ear_fd_req[freq_req], &req, sizeof(req)) != sizeof(req))
		error("error when reading info at select_service\n");
	if (!is_valid_sec_tag(req.sec)) {
		error("Invalid connection with eard %lu", req.sec);
		return;
	}
	if (eard_freq(0)) {
		return;
	}
	if (eard_uncore(0)) {
		return;
	}
	if (eard_rapl(0)) {
		return;
	}
	if (eard_system(0)) {
		return;
	}
	if (eard_node_energy(0)) {
		return;
	}
	if (eard_gpu(0)){
		return;
	}
	error(" Error, request received not supported\n");
	eard_close_comm();
}
#endif

state_t service_close(int req_fd, int ack_fd)
{
	ulong job_id, step_id;
	ulong was_anonymous;
	int id;
	
	debug("Trying to close FDs (req %d, ack %d)", req_fd, ack_fd);
	if (req_fd == -1 || ack_fd == -1) {
		return EAR_ERROR;
	}
	if ((id = get_local_connection_by_fd(eard_local_conn, req_fd, ack_fd)) < 0){
		error("FDs (%d, %d) not found in list of local connections", req_fd, ack_fd);
		return EAR_ERROR;
	}
	debug("Closing FDS (req %d, ack %d) communication (%lu, %lu)",
		  req_fd, ack_fd, eard_local_conn[id].jid, eard_local_conn[id].sid);
	was_anonymous = eard_local_conn[id].anonymous;
	job_id  = eard_local_conn[id].jid;
	step_id = eard_local_conn[id].sid;
	// Remove from fd_set and close
	clean_fd(&rfds_basic, req_fd, &numfds_req, &total_local_fd);
	clean_fd(&rfds_basic, ack_fd, &numfds_req, &total_local_fd);
	clean_local_connection(&eard_local_conn[id]);
	close(req_fd);
	close(ack_fd);
	// End job
	if (!was_anonymous) {
		powermon_mpi_finalize(&handler_energy, job_id, step_id);
	}

	return EAR_SUCCESS;
}


state_t service_listen(int req_fd, int ack_fd, struct daemon_req *req)
{ 
  size_t size;
  uint rpc;
  
  // Reading the header.
  if ((size = eards_read(req_fd, (char *) req, sizeof(eard_head_t))) != sizeof(eard_head_t)) {
    error("Error when reading header during service respond: expected %lu bytes and received %lu",
      sizeof(eard_head_t), size);
    return EAR_ERROR;
  }
  // Reading the union part
  size = sizeof(union daemon_req_opt);
  rpc  = (req->req_service > NEW_API_SERVICES);
  // If is not RPC, complete the daemon_req.
  if (!rpc && (eards_read(req_fd, (char *) &req->req_data, size) != size)) {
    error("Error when reading union daemon_req during service respond: %s", strerror(errno));
    return EAR_ERROR;
  }
  // Checking if its privileged service
  if (!is_privileged_service(req->req_service)) {
    return EAR_SUCCESS;
  }
  if (!is_valid_sec_tag(req->sec)) {
    if (rpc) {
      // If is not valid, clean the rest of the requirement.
      sfail(eard_rpc_clean(req_fd, req->size)) {
        serror("Error when cleaning RPC during service respond");
      }
      // And answer the error.
      sfail(eard_rpc_answer(ack_fd, req->req_service, EAR_ERROR, NULL, 0, "Invalid security key")) {
        serror("Error during RPC answer during service respond");
      }
    }
    return EAR_ERROR;
  }
  return EAR_SUCCESS;
}

state_t service_answer(int req_fd, int ack_fd, uint call, state_t s, char *data, size_t size, char *error)
{ 
  if (state_fail(eard_rpc_answer(ack_fd, call, s, data, size, error))) {
    serror("Error during RPC answer");
    if (state_fail(service_close(req_fd, ack_fd))) {
      serror("Error during service close");
    }
  }
  return EAR_SUCCESS;
}

state_t service_answer_old(int req_fd, int ack_fd, char *data, size_t size)
{ 
  if (eards_write(ack_fd, data, size) != size) {
    error("Error during answer old: %s", strerror(errno));
    if (state_fail(service_close(req_fd, ack_fd))) {
      serror("Error during service close");
    }
  }
  return EAR_SUCCESS;
}

void service_select(int req_fd, int ack_fd)
{ 
  eard_head_t *local_req = (eard_head_t *) &req;
  
  // Hanging up
  if (state_fail (service_listen(req_fd, ack_fd, &req))) {
    service_close(req_fd, ack_fd);
  }
	/* Add one by one */
	#if 0
  // Services list
  if (service_cpufreq(local_req, req_fd, ack_fd)) {
    return;
  }
  if (service_imcfreq(local_req, req_fd, ack_fd)) {
    return;
  }
  if (service_bwidth(local_req, req_fd, ack_fd)) {
    return;
  }
  if (service_gpu(local_req, req_fd, ack_fd)) {
    return;
  }
	#endif
  // Old services
  if (eard_freq(local_req,req_fd,ack_fd)) {
    return;
  }
	if (eard_imc(local_req,req_fd,ack_fd)) {
    return;
  }
  if (eard_rapl(local_req,req_fd,ack_fd)) {
    return;
  }
  if (eard_system(local_req,req_fd,ack_fd)) {
    return;
  }
  if (eard_node_energy(local_req,req_fd,ack_fd)) {
    return;
  }
	if (eard_uncore(local_req,req_fd,ack_fd)) {
    return;
  }
  if (eard_gpu(local_req,req_fd,ack_fd)){
    return;
  }
  error("Request received '%lu' not supported, answering error", local_req->req_service);
  eard_rpc_answer(ack_fd, local_req->req_service, EAR_ERROR, NULL, 0, "EARD RPC does not exist");
  // Closing, it is not necessary to control the error.
  service_close(req_fd,ack_fd);
}


state_t eard_local_api()
{
	int ack_fd;
	int numfds_ready;
	num_packs = node_desc.socket_count;
	struct timeval *my_to;
	ear_njob_t *eard_jobs_list;

	verbose(1,"Creating node_mgr data");
	if (nodemgr_server_init(ear_tmp, &eard_jobs_list) != EAR_SUCCESS){
		verbose(1,"Error creating shared memory region for node_mgr");
		exit(1);
	}


	// CPU Frequency metrics
	state_assert(s, cpufreq_data_alloc(&freq_app_req,    NULL), _exit(0));
	state_assert(s, cpufreq_data_count(&size_cpufreqs,&count_cpufreqs), _exit(0));

	// IMC frequency metrics
  state_assert(s, imcfreq_count_devices(no_ctx, &unc_count), _exit(0));
  state_assert(s, imcfreq_data_alloc(&unc_data, NULL),       _exit(0));

	verbose(1,"CPUfreq and IMC freq data ok for node_services devices %u", unc_count);

	// Energy node metrics
	energy_datasize(&handler_energy,&node_energy_datasize);
	verbose(1,"EARD main thread: node_energy_datasize %lu",node_energy_datasize);
	node_energy_data=(edata_t)malloc(node_energy_datasize);
  energy_frequency(&handler_energy, &energy_freq);
	verbose(VCONF, "energy: power performance accuracy %lu usec", energy_freq);

	// RAPL metrics
  values_rapl=(unsigned long long*)calloc(num_packs*RAPL_POWER_EVS,sizeof(unsigned long long));
  if (values_rapl==NULL) error("values_rapl returns NULL in eard initialization");

	verbose(1,"RAPL data allocated in node_serices");

	// HW initialized HERE...creating communication channels
	verbose(1, "Creating comm in %s for node %s", ear_tmp, nodename);

	#if USE_GPUS
	/* GPU Initialization for app requests */
	ret=gpu_init(&eard_main_gpu_ctx);
	if (ret!=EAR_SUCCESS){
		error("EARD error when initializing GPU context for EARD API queries");
		eard_gpu_initialized=0;	
	}else{
		eard_gpu_initialized=1;
	}
	gpu_data_alloc(&eard_read_gpu);
	gpu_count(&eard_main_gpu_ctx,&eard_num_gpus);
	debug("gpu_count %u",eard_num_gpus);
	eard_gpu_model=gpu_model();
	debug("gpu_model %u",eard_gpu_model);
	#endif

  clean_fd_set(&rfds_basic,&total_local_fd);
  /* This function creates the pipe for new local connections */
  verbose(0,"Initializing local connections");
  if (init_local_connections(&eard_local_conn,node_desc.cpu_count) != EAR_SUCCESS){
    error("Error initializing local connections");
    _exit(0);
  }
	verbose(1,"Creating global connector");
  create_global_connector(ear_tmp, nodename);
  rfds = rfds_basic;

	verbose(1, "Communicator for %s ON", nodename);
	// we wait until EAR daemon receives a request
	// We support requests realted to frequency and to uncore counters
	// rapl support is pending to be supported
	// EAR daemon main loop
	my_to = NULL;
	/*
	*
	*	MAIN LOOP 
	*
	*/
  while (((numfds_ready = select(numfds_req, &rfds, NULL, NULL, my_to)) >= 0) ||
       ((numfds_ready < 0) && (errno == EINTR)))
  {
    if (eard_must_exit) {
      verbose(0, "eard exiting");
      eard_exit(2);
    }
    if (numfds_ready >= 0) {
      if (numfds_ready > 0) {
        for (uint i = 0; i < numfds_req; i++) {
          if (FD_ISSET(i, &rfds)) {
            if (i == global_req_fd){
              /* New connection */
            }else{
              /* We need to find the local req and ack fds */
              ack_fd = get_local_connection_ack(eard_local_conn,i);
            }
            if (ack_fd >=0) service_select(i,ack_fd);
            else _exit(0);
          }    // IF FD_ISSET
        } //for
        // We have to check if there is something else
      }
    } else {// Signal received
      my_to = NULL;
      debug("signal received");
    }
    rfds = rfds_basic;
    //debug("eard waiting.....\n");
  }//while
	verbose(1,"Releasing node_mgr data ");
	nodemgr_server_end();

	return EAR_SUCCESS;
}
