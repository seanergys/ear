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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#define _GNU_SOURCE             
#include <sched.h>

// #define SHOW_DEBUGS 1
#include <common/config.h>
#include <common/states.h>
#include <common/output/verbose.h>
#include <common/types/generic.h>
#include <common/types/application.h>
#include <daemon/local_api/eard_api.h>

#define MAX_PIPE_TRIES (MAX_SOCKET_COMM_TRIES*10)
#define MAX_TRIES             1
#define USE_NON_BLOCKING_IO 1

/* These pipes are used for local communication between ear library and eard */

char ear_commreq_global[SZ_PATH]; 		/* This pipe connects with EARD, the same for all the jobs, then a specific per job-step-localid is created */ 
char ear_commreq[SZ_PATH];
char ear_commack[SZ_PATH];
int ear_fd_req_global = -1;
int ear_fd_req        = -1;
int ear_fd_ack        = -1;
static uint8_t app_connected = 0;
static app_id_t local_con_info;
static int my_step_id;
static int my_job_id;
static char *ear_tmp;


// Metrics: Needed for old API where data size must be requested in advance
static ulong uncore_size;
static ulong energy_size;
static ulong rapl_size;

#define my_read(...)          eards_read(__VA_ARGS__, WAIT_DATA)
#define my_write(...)         eards_write(__VA_ARGS__)

#define CLOSE_LOCAL_COMM()    \
  close(ear_fd_req_global); \
  ear_fd_req_global = -1;   \
  close(ear_fd_ack);        \
  ear_fd_ack = -1;          \
  unlink(ear_commack);      \
  unlink(ear_commreq);


int eards_read(int fd,char *buff,int size, uint wait_mode)
{
#if USE_NON_BLOCKING_IO
  int ret; 
  int tries=0;
  uint to_recv,received=0;
  uint must_abort=0;
  to_recv=size;
  do
  {
    ret = read(fd, buff+received,to_recv);
    if (ret > 0){
      received+=ret;
      to_recv-=ret;
    }else if (ret < 0){
      if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) tries++;
      else{
      	debug("Error receiving data from eard %s,%d",strerror(errno),errno);
        must_abort = 1;
      }
    }else if (ret == 0){
			if (wait_mode == WAIT_DATA) tries++;
			else 												must_abort = 1;
    }
  }while((tries<MAX_PIPE_TRIES) && (to_recv>0) && (must_abort==0));
	if (tries >= MAX_PIPE_TRIES) {
		error("Error reading data, max number of tries reached. tries %d received %u (last ret %d errno %d)",tries, received, ret,errno);
	}
	if (ret<0) return ret;
	return received;

#else
  return read(fd,buff,size);
#endif
}

int eards_write(int fd,char *buff,int size)
{
#if USE_NON_BLOCKING_IO
  int ret;
  int tries=0;
  uint to_send,sended=0;
  uint must_abort=0;


  to_send=size;
  do
  {
    ret=write(fd, buff+sended,size);
    if (ret>0){
      sended+=ret;
      to_send-=ret;
    }else if (ret<0){
      if ((errno==EWOULDBLOCK) || (errno==EAGAIN)) tries++;
      else{
        must_abort=1;
        debug("Error sending command to eard %s,%d",strerror(errno),errno);
      }
    }else if (ret==0){
      must_abort=1;
    }
  }while((tries<MAX_PIPE_TRIES) && (to_send>0) && (must_abort==0));
  /* If there are bytes left to send, we return a 0 */
	if (ret<0) return ret;
	return sended;

#else
  return write(fd,buff,size);
#endif
}


void eards_signal_handler(int s)
{
	debug("EARD has been disconnected");
	mark_as_eard_disconnected(my_job_id,my_step_id,getpid());
	app_connected=0;
}

void eards_signal_catcher()
{
    struct sigaction action;
    sigset_t earl_mask;
    
    sigfillset(&earl_mask);
    sigdelset(&earl_mask,SIGPIPE);
    sigprocmask(SIG_UNBLOCK,&earl_mask,NULL);
    sigemptyset(&action.sa_mask);
    action.sa_handler = eards_signal_handler;
    action.sa_flags = 0;
    
    if (sigaction(SIGPIPE, &action, NULL) < 0) {
    	error( "sigaction error on signal s=%d (%s)", SIGPIPE, strerror(errno));
    }
}    


// If this approach is selected, we will substitute the function call by the key
ulong create_sec_tag()
{
		// You make define a constant at makefile time called SEC_KEY to use that option
    #if SEC_KEY
    return (ulong)SEC_KEY;
    #else
    return (ulong)0;
    #endif

}


uint warning_api(int return_value, int expected, char *msg)
{
	if (return_value != expected){ 
		app_connected=0;
		debug("eards: %s (returned %d expected %d)", msg,return_value,expected);
	}
	if (return_value < 0){ 
		app_connected=0;
		debug("eards: %s (%s)", msg,strerror(errno));
	}
	return (return_value != expected);
}

int eards_connected()
{
	return app_connected;
}

/* 
 * This is an anonymoous connection
 */
state_t eards_connection()
{
  application_t my_dummy_app;
  my_dummy_app.job.id = 0;
  my_dummy_app.job.step_id = 0;
  return eards_connect(&my_dummy_app,getpid());
}


int eards_connect(application_t *my_app, ulong lid)
{
  char nodename[256];
  ulong ack;
  struct daemon_req req;
  int tries=0,connected=0;
  int i;
  int my_id;
  int ret1;
  if (app_connected) return EAR_SUCCESS;

  // These files connect EAR with EAR_COMM
  ear_tmp=getenv("EAR_TMP");

  if (ear_tmp == NULL)
  {
    ear_tmp=getenv("TMP");

    if (ear_tmp == NULL) {
      ear_tmp=getenv("HOME");
    }
  }

  if (gethostname(nodename,sizeof(nodename)) < 0){
    error("ERROR while getting the node name (%s)", strerror(errno));
    return EAR_ERROR;
  }


  copy_application(&req.req_data.app,my_app);

  // We create a single ID
  my_job_id 	= my_app->job.id;
  my_step_id	= my_app->job.step_id;
  local_con_info.jid = my_job_id;
  local_con_info.sid = my_step_id;
  local_con_info.lid = lid;
  memcpy(&req.con_id,&local_con_info,sizeof(local_con_info));
  /* my_id must be extended to include the PID */
  my_id = create_ID(my_app->job.id,my_app->job.step_id);
  debug("Connecting with daemon job_id=%lu step_id%lu pid %d\n",my_app->job.id,my_app->job.step_id,getpid());
  i = 0;
	/* There are three channels: One global for acceptance , and the ones for each connection */
  sprintf(ear_commreq_global,"%s/.ear_comm.globalreq",ear_tmp);
  sprintf(ear_commreq,"%s/%u/.ear_comm.req_%d.%d.%lu",ear_tmp,(uint)my_id, i, my_id,lid);
  sprintf(ear_commack,"%s/%u/.ear_comm.ack_%d.%d.%lu",ear_tmp,(uint)my_id, i, my_id,lid);
  debug("comreq_global %s comreq %s comack %s",ear_commreq_global,ear_commreq,ear_commack);
  /* Private pipes are created before the connection request is sent */
	verbose(2,"Creating %s",ear_commack);
  if (mknod(ear_commack, S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0) < 0) {
     error("Error when creating ear communicator for ack %s", strerror(errno));
  }
  chmod(ear_commack, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	verbose(2,"Creating %s",ear_commreq);
  if (mknod(ear_commreq, S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0) < 0) {
     error("Error when creating ear communicator for requests %s", strerror(errno));
  }
  chmod(ear_commreq, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  /* Pipes are created */
  /* We give some opportunities to connect */
  do{
        verbose(2,"Connecting with EARD using file %s\n",ear_commreq_global);
        if ((ear_fd_req_global = open(ear_commreq_global,O_WRONLY|O_NONBLOCK))<0) tries++;
        else connected=1;
        if ((MAX_TRIES>1) && (!connected)) sleep(1);
  }while ((tries<MAX_TRIES) && !connected);
  if (!connected) {
    // Not possible to connect with ear_daemon
    return_xmsg(EAR_ERROR, "while opening the communicator for requests %s (%d attempts) (%s)",
      ear_commreq_global, tries, strerror(errno));
  }
	debug("Connected with EARD");
  #if USE_NON_BLOCKING_IO
  ret1=fcntl(ear_fd_req_global, F_SETFL, O_NONBLOCK);
  if (ret1<0){
        error("Setting O_NONBLOCK for eard global req communicator %s",strerror(errno));
        return EAR_ERROR;
  }
  #endif

  /* We now connect with the EARD, the tag CONNECT_EARD_NODE_SERVICES is used as the first service requested (for backward compatibility ) */
  req.req_service = CONNECT_EARD_NODE_SERVICES;
  req.sec					= create_sec_tag();
  if (ear_fd_req_global >= 0)
  {
      debug("Sending connection request to EARD\n");
      if (warning_api(write(ear_fd_req_global, &req, sizeof(req)), sizeof(req),
          "writting req_service in ear_daemon_client_connect"))
      {
        ear_fd_req_global = -1;
        return EAR_ERROR;
      }
    }
    /* At this point we have sent the data for connecting with EARD */
    if (ear_fd_req_global >= 0) {
      /* EARD sends an ack when ack pipe for specific service is created */
			verbose(2,"Open %s",ear_commack);
      ear_fd_ack = open(ear_commack,O_RDONLY);
      if (ear_fd_ack < 0){
        error("Error opening local ack communicator with EARD (%s)",strerror(errno));
        close(ear_fd_req_global);
        ear_fd_req_global = -1;
        return EAR_ERROR;
      }
      #if USE_NON_BLOCKING_IO
      ret1 = fcntl(ear_fd_ack, F_SETFL, O_NONBLOCK);
      if (ret1<0){
        error("Setting O_NONBLOCK for eard ack communicator %s",strerror(errno));
        return EAR_ERROR;
      }
      #endif

      debug( "ear_client: waiting for ear_daemon ok");
      if (warning_api(read(ear_fd_ack,&ack,sizeof(ulong)),sizeof(ulong),
        "ERROR while reading ack ")) {
        CLOSE_LOCAL_COMM();
        return EAR_ERROR;
      }
      /* ACK received */
      debug("ACK received from EARD ,opening REQ");
			verbose(2,"Open %s",ear_commreq);
      if ((ear_fd_req = open(ear_commreq,O_WRONLY)) < 0){
        error("Error opening local req communicator, closing the EARD connection");
        CLOSE_LOCAL_COMM();
        return EAR_ERROR;
      }
      #if USE_NON_BLOCKING_IO
      ret1 = fcntl(ear_fd_req, F_SETFL, O_NONBLOCK);
      if (ret1<0){
        error("Setting O_NONBLOCK for eard req communicator %s",strerror(errno));
        return EAR_ERROR;
      }
      #endif
      debug("EARD-EARL in %s connected", nodename);
      close(ear_fd_req_global);
  }
  app_connected = 1;
  eards_signal_catcher();
  debug("Connected");
  return EAR_SUCCESS;
}
void eards_disconnect()
{
  struct daemon_req req;
  req.req_service = DISCONNECT_EARD_NODE_SERVICES;
  req.sec=create_sec_tag();

  debug( "Disconnecting");
  if (!app_connected) return;
  if (ear_fd_req >= 0) {
    warning_api(my_write(ear_fd_req, (char *)&req,sizeof(req)), sizeof(req),
        "witting req in ear_daemon_client_disconnect");
  }
  CLOSE_LOCAL_COMM();
  app_connected=0;

}

//////////////// SYSTEM REQUESTS
ulong eards_write_app_signature(application_t *app_signature)
{
  struct daemon_req req;
  ulong ack=EAR_SUCCESS;

  if (!app_connected) return EAR_SUCCESS;
  debug( "asking the daemon to write the whole application signature (DB)");

  req.req_service = WRITE_APP_SIGNATURE;
  req.sec=create_sec_tag();
  memcpy(&req.req_data.app, app_signature, sizeof(application_t));

  if (ear_fd_req >= 0)
  {
    debug( "writing request...");
    if (warning_api(my_write(ear_fd_req, (char *)&req, sizeof(req)), sizeof(req),
      "ERROR writing request for app signature")) return EAR_ERROR;

    debug( "reading ack...");
    if (warning_api(my_read(ear_fd_ack, (char *)&ack, sizeof(ulong)),sizeof(ulong),
            "ERROR reading ack for app signature")) return EAR_ERROR;
  }else{
    debug( "writting application signature (DB) service unavailable");
    ack=EAR_SUCCESS;
  }

  return ack;
}

ulong eards_write_event(ear_event_t *event)
{
    struct daemon_req req;
    ulong ack=EAR_SUCCESS;

    if (!app_connected) return EAR_SUCCESS;
    debug( "asking the daemon to write an event)");

    req.req_service = WRITE_EVENT;
    req.sec=create_sec_tag();
    memcpy(&req.req_data.event, event, sizeof(ear_event_t));

    if (ear_fd_req >= 0)
    {
        debug( "writing request...");
        if (warning_api(write(ear_fd_req, (char *)&req, sizeof(req)), sizeof(req),
            "ERROR writing request for event")) return EAR_ERROR;

        debug( "reading ack...");
        if (warning_api(my_read(ear_fd_ack, (char *)&ack, sizeof(ulong)),sizeof(ulong),
                "ERROR reading ack event")) return EAR_ERROR;
    }else{
        debug( "eards_write_event service not available");
        ack=EAR_SUCCESS;
    }
    return ack;
}
ulong eards_write_loop_signature(loop_t *loop_signature)
{
    struct daemon_req req;
    ulong ack=EAR_SUCCESS;

    if (!app_connected) return EAR_SUCCESS;
    debug( "asking the daemon to write the loop signature (DB)");

    req.req_service = WRITE_LOOP_SIGNATURE;
    req.sec=create_sec_tag();
    memcpy(&req.req_data.loop, loop_signature, sizeof(loop_t));
    if (ear_fd_req >= 0)
    {
        debug( "writing request...");
        if (warning_api(my_write(ear_fd_req, (char *)&req, sizeof(req)), sizeof(req),
            "ERROR writing request for loop signature")) return EAR_ERROR;

        debug( "reading ack...");
        if (warning_api(my_read(ear_fd_ack, (char *)&ack, sizeof(ulong)),sizeof(ulong),
                "ERROR reading ack for loop signature")) return EAR_ERROR;
    }else{
        debug( "writting loop signature (DB) service unavailable");
        ack=EAR_SUCCESS;
    }

    return ack;
}

//////////////// RAPL REQUESTS
ulong eards_get_data_size_rapl() // size in bytes
{
    struct daemon_req req;
  ulong ack;
  if (!app_connected) return sizeof(ulong);

    debug( "asking for rapl data size ");
    req.req_service=DATA_SIZE_RAPL;
  req.sec=create_sec_tag();
    ack=EAR_SUCCESS;

    if (ear_fd_req >= 0)
  {
        if (warning_api(my_write(ear_fd_req,(char *)&req,sizeof(req)) , sizeof(req),
       "ERROR writing request for RAPL data size ")) return EAR_ERROR;
        if (warning_api(my_read(ear_fd_ack,(char *)&ack,sizeof(ulong)) , sizeof(ulong),
      "ERROR reading ack for RAPL data size ")) return EAR_ERROR;
    } else {
    debug( "ear_daemon_client_get_data_size_rapl service not provided");
    ack=EAR_ERROR;
  }
    rapl_size=ack;
    return ack;
}


int eards_reset_rapl()
{
  struct daemon_req req;
  ulong ack=EAR_SUCCESS;
  if (!app_connected) return EAR_SUCCESS;
  req.req_service = RESET_RAPL;
  req.sec=create_sec_tag();
  debug( "reset rapl");

  if (ear_fd_req>=0)
  {
    if (warning_api(my_write(ear_fd_req,(char *)&req,sizeof(req)) , sizeof(req),
       "ERROR writing request for resetting RAPL counters ")) return EAR_ERROR;
    if (warning_api(my_read(ear_fd_ack,(char *)&ack, sizeof(ulong)) , sizeof(ulong),
       "ERROR reading ack for resetting RAPL counters ")) return EAR_ERROR;
    debug( "RESET ACK rapl counters received from daemon ");
    return ack;
    }
  debug( "reset rapl service not provided");
  return EAR_ERROR;
}
int eards_start_rapl()
{
  struct daemon_req req;
  ulong ack;
  if (!app_connected) return EAR_SUCCESS;
  req.req_service=START_RAPL;
  req.sec=create_sec_tag();
  debug( "start rapl");

    if (ear_fd_req>=0)
  {
    if (warning_api(my_write(ear_fd_req,(char *)&req,sizeof(req)) , sizeof(req),
       "ERROR writing request for starting RAPL counters ")) return EAR_ERROR;
    if (warning_api(my_read(ear_fd_ack,(char *)&ack,sizeof(ulong)) , sizeof(ulong),
       "ERROR reading ack for starting RAPL counters ")) return EAR_ERROR;
    debug( "START ACK rapl counters received from daemon ");
    return EAR_SUCCESS;
    }
  debug( "start rapl service not provided");
  return EAR_ERROR;
}

int eards_read_rapl(unsigned long long *values)
{
  struct daemon_req req;
  ulong ack;
  if (!app_connected){ values[0]=0;return EAR_SUCCESS;}

  req.req_service=READ_RAPL;
  req.sec=create_sec_tag();
  debug( "reading rapl counters");

  if (ear_fd_req>=0)
  {
    // There is not request for uncore...only answer
    if (warning_api(my_write(ear_fd_req,(char *)&req,sizeof(req)) , sizeof(req),
       "ERROR writing request for stopping RAPL counters ")) return EAR_ERROR;
    if (warning_api(my_read(ear_fd_ack,(char *)values,rapl_size) , rapl_size,
       "ERROR reading ack for stopping RAPL counters "))
    {
      return EAR_ERROR;
    }else{
      ack = EAR_SUCCESS;
    }

  }else{
    debug( "read rapl service not provided");
    ack = EAR_ERROR;
  }
  return ack;
}
// END RAPL SERVICES
// NODE ENERGY SERVICES
ulong eards_node_energy_data_size()
{
    struct daemon_req req;
  ulong ack = EAR_SUCCESS;

  if (!app_connected) return sizeof(ulong);

    debug( "asking for node energy data size ");
    req.req_service=DATA_SIZE_ENERGY_NODE;
  req.sec=create_sec_tag();

    if (ear_fd_req>=0)
  {
        if (warning_api(my_write(ear_fd_req,(char *)&req,sizeof(req)) , sizeof(req),
       "ERROR writing request for IPMI data size ")) return EAR_ERROR;
        if (warning_api(my_read(ear_fd_ack,(char *)&ack,sizeof(ulong)) , sizeof(ulong),
       "ERROR reading ack for IPMI data size ")) return EAR_ERROR;
    } else {
    debug( "ear_daemon_client_node_energy_data_size service not provided");
    ack=EAR_ERROR;
    }
    energy_size=ack;
    return ack;
}
int eards_node_dc_energy(void *energy,ulong datasize)
{
  ulong ack=EAR_SUCCESS;
  struct daemon_req req;

  if (!app_connected){
    memset(energy,0,datasize);
    return EAR_SUCCESS;
  }

    debug( "asking for node dc energy ");
    req.req_service=READ_DC_ENERGY;
  req.sec=create_sec_tag();

  if (ear_fd_req>=0)
  {
        if (warning_api(my_write(ear_fd_req,(char *)&req,sizeof(req)) , sizeof(req),
      "ERROR writing request for dc_node_energy ")) return EAR_ERROR;
        if (warning_api(my_read(ear_fd_ack,(char *)energy,datasize) , datasize,
      "ERROR reading data for dc_node_energy ")) return EAR_ERROR;
    } else
  {
    debug( "ear_daemon_client_node_dc_energy service not provided");
    ack=EAR_ERROR;
    }
    return ack;
}
ulong eards_node_energy_frequency()
{
    ulong ack=EAR_ERROR;
    struct daemon_req req;

  if (!app_connected) return 10000000;

    debug( "asking for dc node energy frequency ");
    req.req_service=ENERGY_FREQ;
  req.sec=create_sec_tag();
    if (ear_fd_req>=0)
    {
        if (warning_api(my_write(ear_fd_req,(char *)&req,sizeof(req)) , sizeof(req),
             "ERROR writing request for energy freq of update ")) return EAR_ERROR;
        if (warning_api(my_read(ear_fd_ack,(char *)&ack,sizeof(ulong)) , sizeof(ulong),
            "ERROR reading ack for energy freq of update ")) return ack;
    } else
    {
        debug( "eards_node_energy_frequency service not provided");
        ack=EAR_ERROR;
    }
    return ack;

}


#if 0
//////////////// SYSTEM REQUESTS
ulong eards_write_app_signature(application_t *app_signature)
{
	int com_fd=system_req;
	struct daemon_req req;
	ulong ack=EAR_SUCCESS;

	if (!app_connected) return EAR_SUCCESS;
	debug( "asking the daemon to write the whole application signature (DB)");

	req.req_service = WRITE_APP_SIGNATURE;
	req.sec=create_sec_tag();
	memcpy(&req.req_data.app, app_signature, sizeof(application_t));

	if (ear_fd_req[com_fd] >= 0)
	{
		debug( "writing request...");
		if (warning_api(my_write(ear_fd_req[com_fd], (char *)&req, sizeof(req)), sizeof(req),
			"ERROR writing request for app signature")) return EAR_ERROR;
        
		debug( "reading ack...");
		if (warning_api(my_read(ear_fd_ack[com_fd], (char *)&ack, sizeof(ulong)),sizeof(ulong),
        		"ERROR reading ack for app signature")) return EAR_ERROR;
	}else{
		debug( "writting application signature (DB) service unavailable");
		ack=EAR_SUCCESS;
	}

	return ack;
}

ulong eards_write_event(ear_event_t *event)
{
	int com_fd=system_req;
    struct daemon_req req;
    ulong ack=EAR_SUCCESS;

    if (!app_connected) return EAR_SUCCESS;
    debug( "asking the daemon to write an event)");

    req.req_service = WRITE_EVENT;
	req.sec=create_sec_tag();
    memcpy(&req.req_data.event, event, sizeof(ear_event_t));

    if (ear_fd_req[com_fd] >= 0)
    {
        debug( "writing request...");
        if (warning_api(write(ear_fd_req[com_fd], (char *)&req, sizeof(req)), sizeof(req),
            "ERROR writing request for event")) return EAR_ERROR;

        debug( "reading ack...");
        if (warning_api(my_read(ear_fd_ack[com_fd], (char *)&ack, sizeof(ulong)),sizeof(ulong),
                "ERROR reading ack event")) return EAR_ERROR;
    }else{
        debug( "eards_write_event service not available");
        ack=EAR_SUCCESS;
    }

    return ack;
	
}
ulong eards_write_loop_signature(loop_t *loop_signature)
{
    int com_fd=system_req;
    struct daemon_req req;
    ulong ack=EAR_SUCCESS;

    if (!app_connected) return EAR_SUCCESS;
    debug( "asking the daemon to write the loop signature (DB)");

    req.req_service = WRITE_LOOP_SIGNATURE;
	req.sec=create_sec_tag();
    memcpy(&req.req_data.loop, loop_signature, sizeof(loop_t));

	

    if (ear_fd_req[com_fd] >= 0)
    {
        debug( "writing request...");
        if (warning_api(my_write(ear_fd_req[com_fd], (char *)&req, sizeof(req)), sizeof(req),
            "ERROR writing request for loop signature")) return EAR_ERROR;

        debug( "reading ack...");
        if (warning_api(my_read(ear_fd_ack[com_fd], (char *)&ack, sizeof(ulong)),sizeof(ulong),
                "ERROR reading ack for loop signature")) return EAR_ERROR;
    }else{
        debug( "writting loop signature (DB) service unavailable");
        ack=EAR_SUCCESS;
    }

    return ack;
}

void eards_begin_compute_turbo_freq()
{
	struct daemon_req req;
	ulong ack=EAR_SUCCESS;
	if (!app_connected) return;
	debug( "start getting turbo freq ");
    req.req_service=START_GET_FREQ;
	req.sec=create_sec_tag();
    if (ear_fd_req[freq_req]>=0)
	{
		if (warning_api(my_write(ear_fd_req[freq_req],(char *)&req,sizeof(req)) , sizeof(req),
			"ERROR writing request for starting frequency ")) return ;
		if (warning_api(my_read(ear_fd_ack[freq_req],(char *)&ack, sizeof(ulong)) , sizeof(ulong),
			 "ERROR reading ack for starting frequency ")) return ;
		return;
   }   
   debug( "ear_daemon_client_begin_compute_turbo_freq service not provided");
   return;
}

ulong eards_end_compute_turbo_freq()
{
	struct daemon_req req;
	ulong ack=EAR_SUCCESS;

	if (!app_connected) return 0;

	debug( "end getting turbo freq ");
    req.req_service = END_GET_FREQ;
	req.sec=create_sec_tag();
	if (ear_fd_req[freq_req] >= 0)
	{
		if (warning_api(my_write(ear_fd_req[freq_req],(char *)&req, sizeof(req)) ,sizeof(req),
			 "ERROR writing request for finishing frequency")) return EAR_ERROR;
		if (warning_api(my_read(ear_fd_ack[freq_req],(char *)&ack, sizeof(ulong)) , sizeof(ulong),
				"ERROR reading ack for finishing frequency ")) return EAR_ERROR;

		debug("TURBO freq computed as  %lu",  ack);
	} else {
		debug( "ear_daemon_client_end_compute_turbo_freq service not provided");
	}
    return ack;
}
void eards_begin_app_compute_turbo_freq()
{
    struct daemon_req req;
    ulong ack = EAR_SUCCESS;
	if (!app_connected) return;

    debug( "start getting turbo freq");
    req.req_service = START_APP_COMP_FREQ;
	req.sec=create_sec_tag();

	if (ear_fd_req[freq_req]>=0)
	{
		if (warning_api(my_write(ear_fd_req[freq_req],(char *)&req, sizeof(req)) , sizeof(req),
			"ERROR writing request for starting frequency for app")) return;
		if (warning_api(my_read(ear_fd_ack[freq_req],(char *)&ack,sizeof(ulong)) , sizeof(ulong),
			"ERROR reading ack for starting frequency")) return;
		return;
   }
   debug( "ear_daemon_client_begin_app_compute_turbo_freq service not provided");
   return;
}
ulong eards_end_app_compute_turbo_freq()
{
	struct daemon_req req;
	ulong ack=EAR_SUCCESS;
	if (!app_connected) return 0;
	debug( "end getting turbo freq");
	req.req_service = END_APP_COMP_FREQ;
	req.sec=create_sec_tag();

	if (ear_fd_req[freq_req]>=0)
	{
		if (warning_api(my_write(ear_fd_req[freq_req],(char *)&req,sizeof(req)) , sizeof(req),
			 "ERROR writing request for finishing frequency for app")) return EAR_ERROR;
		if (warning_api(my_read(ear_fd_ack[freq_req],(char *)&ack,sizeof(ulong)) , sizeof(ulong),
			"ERROR writing ack for finishing frequency for app ")) return EAR_ERROR;

		debug("TURBO freq computed as  %lu", ack);
	} else {
		debug( "ear_daemon_client_end_app_compute_turbo_freq service not provided");
	}
	return ack;
}

// END FREQUENCY SERVICES
#endif
//////////////// UNCORE REQUESTS
ulong eards_get_data_size_uncore()
{
	struct daemon_req req;
	ulong ack = EAR_SUCCESS;
	if (!app_connected) return sizeof(ulong);

	debug( "asking for uncore data size ");
  req.req_service=DATA_SIZE_UNCORE;
	req.sec=create_sec_tag();

	if (ear_fd_req >= 0) {
		if (warning_api(my_write(ear_fd_req,(char *)&req, sizeof(req)) , sizeof(req),
			 "ERROR writing request for uncore data size ")) return EAR_ERROR;
		if (warning_api(my_read(ear_fd_ack,(char *)&ack, sizeof(ulong)) , sizeof(ulong),
			 "ERROR reading ack for uncore data size ")) return EAR_ERROR;
	} else {
		debug( "ear_daemon_client_get_data_size_uncore service not provided");
		ack=EAR_ERROR;
	}
    uncore_size=ack;
    return ack;
}


int eards_reset_uncore()
{
	ulong ack = EAR_SUCCESS;
  struct daemon_req req;
	if (!app_connected) return EAR_SUCCESS;

	req.req_service = RESET_UNCORE;
	req.sec=create_sec_tag();
	debug( "reset uncore");

	if (ear_fd_req >= 0)
	{
		if (warning_api(my_write(ear_fd_req,(char *)&req,sizeof(req)), sizeof(req),
			 "ERROR writing request for resetting uncores ")) return EAR_ERROR;
		if (warning_api(my_read(ear_fd_ack,(char *)&ack,sizeof(ulong)) , sizeof(ulong),
			 "ERROR reading ack for resetting uncores ")) return EAR_ERROR;
		debug( "RESET ACK Uncore counters received from daemon ");
	    return EAR_SUCCESS;
    }
	debug( "reset uncore service not provided ");
	return EAR_ERROR;
}

int eards_start_uncore()
{
	ulong ack;
  struct daemon_req req;
	if (!app_connected) return EAR_SUCCESS;
  req.req_service=START_UNCORE;
	req.sec=create_sec_tag();
	debug( "start uncore");

	if (ear_fd_req >= 0)
	{
		if (warning_api(my_write(ear_fd_req,(char *)&req, sizeof(req)) , sizeof(req),
			 "ERROR writing request for starting uncores ")) return EAR_ERROR;
		if (warning_api(my_read(ear_fd_ack,(char *)&ack,sizeof(ulong)) , sizeof(ulong),
			 "ERROR reading ack for starting uncores ")) return EAR_ERROR;
		debug( "START ACK Uncore counters received from daemon ");
		return EAR_SUCCESS;
    }
	debug( "start uncore service not provided ");
	return EAR_ERROR;
}
int eards_stop_uncore(unsigned long long *values)
{
  ulong ack;
  struct daemon_req req;

    if (!app_connected){ values[0]=0;return EAR_SUCCESS;}

    req.req_service=STOP_UNCORE;
    req.sec=create_sec_tag();
    debug( "stopping uncore counters");

    if (ear_fd_req >= 0)
    {
        /* There is not request for uncore...only answer */
        if (warning_api(my_write(ear_fd_req, (char *)&req, sizeof(req)) , sizeof(req),
            "ERROR writing request for reading uncores ")) return EAR_ERROR;
        if (warning_api(my_read(ear_fd_ack,(char *)values,uncore_size) , uncore_size, 
             "ERROR reading ack for reading uncores ")){ 
            return EAR_ERROR;
        } else {
            ack = EAR_SUCCESS;
        }

    }else{
        ack=EAR_ERROR;
        debug( "read uncore service not provided");
    }
    return ack;
}

int eards_read_uncore(unsigned long long *values)
{
	ulong ack;
  struct daemon_req req;

	if (!app_connected){ values[0]=0;return EAR_SUCCESS;}

	req.req_service=READ_UNCORE;
	req.sec=create_sec_tag();
	debug( "reading uncore counters");

	if (ear_fd_req>=0)
	{
		/* There is not request for uncore...only answer */
		if (warning_api(my_write(ear_fd_req, (char *)&req, sizeof(req)) , sizeof(req),
			"ERROR writing request for reading uncores ")) return EAR_ERROR;
		if (warning_api(my_read(ear_fd_ack,(char *)values,uncore_size) , uncore_size, 
			 "ERROR reading ack for reading uncores ")){ 
			return EAR_ERROR;
		} else {
			ack = EAR_SUCCESS;
		}

	}else{
		ack=EAR_ERROR;
		debug( "read uncore service not provided");
	}
	return ack;
}
#if 0
//////////////// RAPL REQUESTS
ulong eards_get_data_size_rapl() // size in bytes
{
    int com_fd=rapl_req;
    struct daemon_req req;
	ulong ack;
	if (!app_connected) return sizeof(ulong);

    debug( "asking for rapl data size ");
    req.req_service=DATA_SIZE_RAPL;
	req.sec=create_sec_tag();
    ack=EAR_SUCCESS;

    if (ear_fd_req[com_fd] >= 0)
	{
        if (warning_api(my_write(ear_fd_req[com_fd],(char *)&req,sizeof(req)) , sizeof(req),
			 "ERROR writing request for RAPL data size ")) return EAR_ERROR;
        if (warning_api(my_read(ear_fd_ack[com_fd],(char *)&ack,sizeof(ulong)) , sizeof(ulong),
			"ERROR reading ack for RAPL data size ")) return EAR_ERROR;
    } else {
		debug( "ear_daemon_client_get_data_size_rapl service not provided");
		ack=EAR_ERROR;
	}
    rapl_size=ack;
    return ack;
}


int eards_reset_rapl()
{
	struct daemon_req req;
	ulong ack=EAR_SUCCESS;
	if (!app_connected) return EAR_SUCCESS;
	req.req_service = RESET_RAPL;
	req.sec=create_sec_tag();
	debug( "reset rapl");

	if (ear_fd_req[rapl_req]>=0)
	{
		if (warning_api(my_write(ear_fd_req[rapl_req],(char *)&req,sizeof(req)) , sizeof(req),
			 "ERROR writing request for resetting RAPL counters ")) return EAR_ERROR;
		if (warning_api(my_read(ear_fd_ack[rapl_req],(char *)&ack, sizeof(ulong)) , sizeof(ulong),
			 "ERROR reading ack for resetting RAPL counters ")) return EAR_ERROR;
		debug( "RESET ACK rapl counters received from daemon ");
		return ack;
    }
	debug( "reset rapl service not provided");
	return EAR_ERROR;
}

int eards_start_rapl()
{
	struct daemon_req req;
	ulong ack;
	if (!app_connected) return EAR_SUCCESS;
	req.req_service=START_RAPL;
	req.sec=create_sec_tag();
	debug( "start rapl");

    if (ear_fd_req[rapl_req]>=0)
	{
		if (warning_api(my_write(ear_fd_req[rapl_req],(char *)&req,sizeof(req)) , sizeof(req),
			 "ERROR writing request for starting RAPL counters ")) return EAR_ERROR;
		if (warning_api(my_read(ear_fd_ack[rapl_req],(char *)&ack,sizeof(ulong)) , sizeof(ulong),
			 "ERROR reading ack for starting RAPL counters ")) return EAR_ERROR;
		debug( "START ACK rapl counters received from daemon ");
		return EAR_SUCCESS;
    }
	debug( "start rapl service not provided");
	return EAR_ERROR;
}

int eards_read_rapl(unsigned long long *values)
{
	struct daemon_req req;
	ulong ack;
	if (!app_connected){ values[0]=0;return EAR_SUCCESS;}

	req.req_service=READ_RAPL;
	req.sec=create_sec_tag();
	debug( "reading rapl counters");

	if (ear_fd_req[rapl_req]>=0)
	{
		// There is not request for uncore...only answer
		if (warning_api(my_write(ear_fd_req[rapl_req],(char *)&req,sizeof(req)) , sizeof(req),
			 "ERROR writing request for stopping RAPL counters ")) return EAR_ERROR;
		if (warning_api(my_read(ear_fd_ack[rapl_req],(char *)values,rapl_size) , rapl_size,
			 "ERROR reading ack for stopping RAPL counters "))
		{ 
			return EAR_ERROR;
		}else{
			ack = EAR_SUCCESS;
		}

	}else{
		debug( "read rapl service not provided");
		ack = EAR_ERROR;
	}
	return ack;
}
// END RAPL SERVICES
// NODE ENERGY SERVICES
ulong eards_node_energy_data_size()
{
    int com_fd =node_energy_req;
    struct daemon_req req;
	ulong ack = EAR_SUCCESS;
	
	if (!app_connected) return sizeof(ulong);

    debug( "asking for node energy data size ");
    req.req_service=DATA_SIZE_ENERGY_NODE;
	req.sec=create_sec_tag();

    if (ear_fd_req[com_fd]>=0)
	{
        if (warning_api(my_write(ear_fd_req[com_fd],(char *)&req,sizeof(req)) , sizeof(req),
			 "ERROR writing request for IPMI data size ")) return EAR_ERROR;
        if (warning_api(my_read(ear_fd_ack[com_fd],(char *)&ack,sizeof(ulong)) , sizeof(ulong),
			 "ERROR reading ack for IPMI data size ")) return EAR_ERROR;
    } else {
		debug( "ear_daemon_client_node_energy_data_size service not provided");
		ack=EAR_ERROR;
    }   
    energy_size=ack;
    return ack;
}
int eards_node_dc_energy(void *energy,ulong datasize)
{
    int com_fd = node_energy_req;
	ulong ack=EAR_SUCCESS;
	struct daemon_req req;

	if (!app_connected){
		memset(energy,0,datasize);
		return EAR_SUCCESS;
	}

    debug( "asking for node dc energy ");
    req.req_service=READ_DC_ENERGY;
	req.sec=create_sec_tag();

	if (ear_fd_req[com_fd]>=0)
	{
        if (warning_api(my_write(ear_fd_req[com_fd],(char *)&req,sizeof(req)) , sizeof(req),
			"ERROR writing request for dc_node_energy ")) return EAR_ERROR;
        if (warning_api(my_read(ear_fd_ack[com_fd],(char *)energy,datasize) , datasize,
			"ERROR reading data for dc_node_energy ")) return EAR_ERROR;
    } else
	{
		debug( "ear_daemon_client_node_dc_energy service not provided");
		ack=EAR_ERROR;
    }   
    return ack;
}
ulong eards_node_energy_frequency()
{
    int com_fd = node_energy_req;
    ulong ack=EAR_ERROR;
    struct daemon_req req;

	if (!app_connected) return 10000000;

    debug( "asking for dc node energy frequency ");
    req.req_service=ENERGY_FREQ;
	req.sec=create_sec_tag();
    if (ear_fd_req[com_fd]>=0)
    {
        if (warning_api(my_write(ear_fd_req[com_fd],(char *)&req,sizeof(req)) , sizeof(req),
             "ERROR writing request for energy freq of update ")) return EAR_ERROR;
        if (warning_api(my_read(ear_fd_ack[com_fd],(char *)&ack,sizeof(ulong)) , sizeof(ulong),
            "ERROR reading ack for energy freq of update ")) return ack;
    } else
    {
        debug( "eards_node_energy_frequency service not provided");
        ack=EAR_ERROR;
    }
    return ack;

}
#endif

/********** GPU *********/
int eards_gpu_model(uint *gpu_model)
{
  ulong ack=EAR_SUCCESS;
  struct daemon_req req;
  
	*gpu_model=0;
  if (!app_connected){
    return EAR_SUCCESS;
  }
  debug( "asking for gpu_model ");
  req.req_service = GPU_MODEL;
  req.sec = create_sec_tag();
  if (ear_fd_req >= 0)
  {      
      if (warning_api(my_write(ear_fd_req,(char *)&req,sizeof(req)) , sizeof(req),
      "ERROR writing request for gpu model ")) return EAR_ERROR;
      if (warning_api(my_read(ear_fd_ack,(char *)gpu_model,sizeof(uint)) , sizeof(uint),
      "ERROR reading data for gpu model")) return EAR_ERROR;
      ack = EAR_SUCCESS;
  } else
  { 
    debug( "gpu_model service not provided");
    ack=EAR_ERROR;
  }   
  return ack;
}

int eards_gpu_dev_count(uint *gpu_dev_count)
{
  ulong ack=EAR_SUCCESS;
  struct daemon_req req;
 
	*gpu_dev_count=0; 
  if (!app_connected){
    return EAR_SUCCESS;
  }
  debug( "asking for gpu_dev_count ");
  req.req_service = GPU_DEV_COUNT;
  req.sec = create_sec_tag();
  if (ear_fd_req >= 0)
  {      
      if (warning_api(my_write(ear_fd_req,(char *)&req,sizeof(req)) , sizeof(req),
      "ERROR writing request for gpu dev_coubt ")) return EAR_ERROR;
      if (warning_api(my_read(ear_fd_ack,(char *)gpu_dev_count,sizeof(uint)) , sizeof(uint),
      "ERROR reading data for gpu dev_count")) return EAR_ERROR;
      ack = EAR_SUCCESS;
  } else
  { 
    debug( "gpu_dev_count service not provided");
    ack=EAR_ERROR;
  }   
  return ack;
}

int eards_gpu_data_read(gpu_t *gpu_info,uint num_dev)
{
  ulong ack=EAR_SUCCESS;
  struct daemon_req req;
  
	memset(gpu_info,0,sizeof(gpu_t)*num_dev);
  if (!app_connected){
    return EAR_SUCCESS;
  }
  debug( "asking for gpu_data ");
  req.req_service = GPU_DATA_READ;
  req.sec = create_sec_tag();
  if (ear_fd_req >= 0)
  {     
      if (warning_api(my_write(ear_fd_req,(char *)&req,sizeof(req)) , sizeof(req),
      "ERROR writing request for gpu data read ")) return EAR_ERROR;
			debug("gpu_data_read, reading %lu bytes",sizeof(gpu_t)*num_dev);
      if (warning_api(my_read(ear_fd_ack,(char *)gpu_info,sizeof(gpu_t)*num_dev) , sizeof(gpu_t)*num_dev,
      "ERROR reading data for gpu data read ")) return EAR_ERROR;
			ack = EAR_SUCCESS;
  } else
  { 
    debug( "gpu_data_read service not provided");
    ack=EAR_ERROR;
  }   
	return ack;
}

int eards_gpu_get_info(gpu_info_t *info, uint num_dev)
{
  ulong ack=EAR_SUCCESS;
  struct daemon_req req;
  
	memset(info,0,sizeof(gpu_info_t)*num_dev);
  if (!app_connected){
    return EAR_SUCCESS;
  }
  debug("asking for gpu_data ");
  req.req_service = GPU_GET_INFO;
  req.sec = create_sec_tag();
  if (ear_fd_req >= 0)
  {     
      if (warning_api(my_write(ear_fd_req,(char *)&req,sizeof(req)) , sizeof(req),
      "ERROR writing request for gpu data read ")) return EAR_ERROR;
			debug("gpu_data_read, reading %lu bytes",sizeof(gpu_info_t)*num_dev);
      if (warning_api(my_read(ear_fd_ack,(char *)info,sizeof(gpu_info_t)*num_dev) , sizeof(gpu_info_t)*num_dev,
      "ERROR reading data for gpu data read ")) return EAR_ERROR;
			ack = EAR_SUCCESS;
  } else
  { 
    debug( "gpu_data_read service not provided");
    ack=EAR_ERROR;
  }   
	return ack;
}

int eards_gpu_set_freq(uint num_dev,ulong *freqs)
{
  ulong ack=EAR_SUCCESS;
  struct daemon_req req;
	int i;
  
  if (!app_connected){
    return EAR_SUCCESS;
  }
  debug( "setting GPU freqs ");
	for (i=0;i<num_dev;i++) debug("GPU %d: Freq requested %lu",i,freqs[i]);
  req.req_service = GPU_SET_FREQ;
  req.sec=create_sec_tag();
	req.req_data.gpu_freq.num_dev=num_dev;
	if (num_dev>MAX_GPUS_SUPPORTED){
		error("Using more GPUS than supported: Requested %u Max %u",num_dev,MAX_GPUS_SUPPORTED);
		num_dev=MAX_GPUS_SUPPORTED;
	}
	memcpy(req.req_data.gpu_freq.gpu_freqs,freqs,sizeof(ulong)*num_dev);

  if (ear_fd_req >= 0)
  {      
      if (warning_api(my_write(ear_fd_req,(char *)&req,sizeof(req)) , sizeof(req),
      "ERROR writing request for gpu set freq ")) return EAR_ERROR;
      debug("eards_gpu_set_freq, reading %lu bytes",sizeof(ack));
      if (warning_api(my_read(ear_fd_ack,(char *)&ack,sizeof(ack)) , sizeof(ack),
      "ERROR reading data for eards_gpu_set_freq")) return EAR_ERROR;
      ack = EAR_SUCCESS;
  } else
  { 
    debug( "eards_gpu_set_freq service not provided");
    ack=EAR_ERROR;
  }   
  return ack;
}

