/**************************************************************
 *    Energy Aware Runtime (EAR)
 *    This program is part of the Energy Aware Runtime (EAR).
 *
 *    EAR provides a dynamic, transparent and ligth-weigth solution for
 *    Energy management.
 *
 *        It has been developed in the context of the Barcelona Supercomputing Center (BSC)-Lenovo Collaboration project.
 *
 *       Copyright (C) 2017  
 *    BSC Contact     mailto:ear-support@bsc.es
 *    Lenovo contact     mailto:hpchelp@lenovo.com
 *
 *    EAR is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 2.1 of the License, or (at your option) any later version.
 *    
 *    EAR is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *    
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with EAR; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *    The GNU LEsser General Public License is contained in the file COPYING    
 */



#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <endian.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <pthread.h>
#include <limits.h> 
#include <pwd.h>
#include <freeipmi/freeipmi.h>

#include <common/states.h>
#include <common/output/verbose.h>
#include <common/hardware/hardware_info.h>
#include <common/system/monitor.h>


#define PACKAGE_NAME  "freeipmi"
#define FREEIPMI_CONFIG_DIRECTORY_MODE    0700
#define SDR_CACHE_DIR                     "sdr-cache"
#define MAXHOSTNAMELEN 64
#define MAXPATHLEN 4096


static ipmi_ctx_t ipmi_ctx ;
static uint8_t *bytes_rq = NULL;
static uint8_t *bytes_rs = NULL;
static unsigned int send_len;
static pthread_mutex_t node_energy_lock_nm = PTHREAD_MUTEX_INITIALIZER;
static ipmi_sdr_ctx_t sdr_ctx;
static uint8_t channel, slave, lun;
static uint found = 0;
static suscription_t *inm_sus;
static ipmi_ctx_t     ipmi_ctx
static time_t         last_timestamp;


#define SDR_CACHE_FILENAME_PREFIX         "sdr-cache"

/* ipmi_sdr_cache_iterate
 *  * - iterate through all SDR records calling callback for each one.
 *   * - if callback returns < 0, that will break iteration and return
 *    *   value is returned here.
 *     */

				static int
intelnm_sdr_callback (ipmi_sdr_ctx_t sdr_ctx,
								uint8_t record_type,
								const void *sdr_record,
								unsigned int sdr_record_len,
								void *arg)
{ 
				int ret; 


				debug("Record type %d OEMO RECORD %d\n", record_type, IPMI_SDR_FORMAT_OEM_RECORD);

				if (record_type != IPMI_SDR_FORMAT_OEM_RECORD)
								return (0);

				debug("ipmi_sdr_oem_parse_intel_node_manager\n");
				/* return (1) - is oem intel node manager, fully parsed
				 *  * return (0) - is not oem intel node manager
				 *   * return (-1) - error
				 *    */

				if ((ret = ipmi_sdr_oem_parse_intel_node_manager (sdr_ctx,
																				sdr_record,
																				sdr_record_len,
																				&slave,
																				&lun,
																				&channel,
																				NULL,
																				NULL,
																				NULL,
																				NULL)) < 0)
				{ 
								return 0;
				}

				if (ret){
								debug("Intel NM record found\n");
								found = 1;
				}
				if (found) return -1;
				return 0;
}

				static int
sdr_get_home_directory ( char *buf,
								unsigned int buflen)
{
				uid_t user_id;
				struct passwd pwd,*pwdptr;
				long int tbuf_len;
				char *tbuf = NULL;
				int ret;
				int rv = -1;


				tbuf_len = 4096;      /* XXX */

				if (!(tbuf = malloc (tbuf_len)))
				{
								debug("malloc error\n");
								return -1;
				}

				user_id = getuid ();
				debug("User id %d\n", user_id);
				memset (&pwd, '\0', sizeof (struct passwd));
				if (getpwuid_r (user_id,
																&pwd,
																tbuf,
																tbuf_len,
																&(pwdptr)) != 0)
				{
								free(tbuf);
								debug("getpwuid_r error\n");
								return -1;
				}

				debug("pw_dir %s\n", pwd.pw_dir); 
				free(tbuf);

				if (pwd.pw_dir)
				{
								if (!access (pwd.pw_dir, R_OK|W_OK|X_OK))
								{
												if (strlen (pwd.pw_dir) > (buflen - 1))
												{
																debug("internal overflow error\n");
																return -1;
												}

												strcpy (buf, pwd.pw_dir);
												return 0;
								}
				}

				if ((ret = sndebug (buf,
																				buflen,
																				"/tmp/.%s-%s",
																				PACKAGE_NAME,
																				pwd.pw_name)) < 0)
				{
								debug("sndebug");
								return -1;
				}
				if (ret >= buflen)
				{
								debug("sndebug invalid bytes written\n");
								return -1;
				}

				if (access (buf, R_OK|W_OK|X_OK) < 0)
				{
								if (errno == ENOENT)
								{
												if (mkdir (buf, FREEIPMI_CONFIG_DIRECTORY_MODE) < 0)
												{
																debug("Cannot make cache directory: %s: %s\n", buf,
																								strerror (errno));
																return -1;
												}
								}
								else
								{
												debug(    "Cannot access cache directory: %s\n", buf);
												return -1;
								}
				}
				debug("Home dir is %s\n", buf);

				return (0);
}

				static int
sdr_get_config_directory ( const char *cache_dir,
								char *buf,
								unsigned int buflen)
{
				char tbuf[MAXPATHLEN+1];
				int ret;


				memset (tbuf, '\0', MAXPATHLEN+1);
				if (!cache_dir)
				{
								if (sdr_get_home_directory ( tbuf,
																				MAXPATHLEN) < 0){
												debug("home directory error\n");
												return (-1);
								}
				}
				else
				{
								if (strlen (cache_dir) > (MAXPATHLEN - 1))
								{
												debug("internal overflow error\n");
												return (-1);
								}
								strcpy (tbuf, cache_dir);

								if (access (tbuf, R_OK|W_OK|X_OK) < 0)
								{
												if (errno == ENOENT)
												{
																if (mkdir (tbuf, FREEIPMI_CONFIG_DIRECTORY_MODE) < 0)
																{
																				debug("Cannot make cache directory: %s: %s\n",
																												tbuf,
																												strerror (errno));
																				return (-1);
																}
												}
												else
												{
																debug("Cannot access cache directory: %s\n",
																								tbuf);
																return (-1);
												}
								}
				}
				debug("home directory is %s\n", tbuf);
				if ((ret = sndebug (buf,
																				buflen,
																				"%s/.%s",
																				tbuf,
																				PACKAGE_NAME)) < 0)
				{
								debug( "sndebug error");
								return (-1);
				}

				if (ret >= buflen)
				{
								debug("sndebug invalid bytes written\n");
								return (-1);
				}
				debug("sdr_get_config_directory success\n");
				return (0);
}

				static int
sdr_cache_get_cache_directory ( const char *cache_dir,
								char *buf,
								unsigned int buflen)
{
				char tbuf[MAXPATHLEN+1];
				int ret;


				memset (tbuf, '\0', MAXPATHLEN+1);
				if (sdr_get_config_directory ( cache_dir,
																tbuf,
																MAXPATHLEN) < 0)
								return (-1);

				if ((ret = sndebug (buf,
																				buflen,
																				"%s/%s",
																				tbuf,
																				SDR_CACHE_DIR)) < 0)
				{
								debug("sndebug error\n");
								return (-1);
				}

				if (ret >= buflen)
				{
								debug("sndebug invalid bytes written\n");
								return (-1);
				}
				debug("sdr_cache_get_cache_directory success\n");

				return (0);
}
				static int
sdr_get_cache_filename ( const char *hostname,
								char *buf,
								unsigned int buflen)
{
				char sdrcachebuf[MAXPATHLEN+1];
				char *cache_directory  = 0;
				char *ptr;
				int ret;


				char hostnamebuf[MAXHOSTNAMELEN+1];

				memset (hostnamebuf, '\0', MAXHOSTNAMELEN+1);
				if (gethostname (hostnamebuf, MAXHOSTNAMELEN) < 0)
								sndebug (hostnamebuf, MAXHOSTNAMELEN, "localhost");

				/* shorten hostname if necessary */
				if ((ptr = strchr (hostnamebuf, '.')))
								*ptr = '\0';

				if (sdr_cache_get_cache_directory (cache_directory,
																sdrcachebuf,
																MAXPATHLEN) < 0)
								return (-1);

				if ((ret = sndebug (buf,
																				buflen,
																				"%s/%s-%s.%s",
																				sdrcachebuf,
																				SDR_CACHE_FILENAME_PREFIX,
																				hostnamebuf,
																				hostname ? hostname : "localhost")) < 0)

				{
								debug("sndebug error\n");
								return (-1);
				}

				if (ret >= buflen)
				{

								debug("sndebug invalid bytes written\n");
								return (-1);
				}
				debug("sdr name success %s\n", buf);
				return (0);
}





state_t energy_init(void **c)
{
				uid_t uid;
				int ret=0;
				int rs_len;
				unsigned int workaround_flags = 0;
				pthread_mutex_lock(&node_energy_lock_nm);
				char cachefilenamebuf[MAXPATHLEN+1];
				char *hostname = 0;
				int cache_create_flags = 0;

				


				//Creating the context
				//

				debug("Create context\n");
				if (!( ipmi_ctx = ipmi_ctx_create ())){
								pthread_mutex_unlock(&node_energy_lock_nm);
								error("Error in ipmi_ctx_create %s",strerror(errno));
								return EAR_ERROR;
				}
				// Checking for root
				uid = getuid ();
				if (uid != 0){ 
								energy_dispose(no_ctx);
								pthread_mutex_unlock(&node_energy_lock_nm);
								error("No root permissions");
								return EAR_ERROR;
				}
				// inband context
				debug("Inband context\n");
				if ((ret = ipmi_ctx_find_inband (ipmi_ctx, 
																				NULL, // driver_type
																				0, //disable_auto_probe
																				0, // driver_address
																				0, // register_spacing
																				NULL, // driver_device
																				workaround_flags,
																				IPMI_FLAGS_DEFAULT)) < 0) {
								error("%s",ipmi_ctx_errormsg(ipmi_ctx));
								energy_dispose(no_ctx);
								pthread_mutex_unlock(&node_energy_lock_nm);

								return EAR_ERROR;    
				}
				if (ret==0){
								error("Not inband device found %s",ipmi_ctx_errormsg(ipmi_ctx));
								energy_dispose(no_ctx);
								pthread_mutex_unlock(&node_energy_lock_nm);
								return EAR_ERROR;    
				}

				/* SDR context */
				debug( "ipmi_sdr_ctx_create");
				if (!(sdr_ctx = ipmi_sdr_ctx_create ()) ){
								error("INM SDR ctx create");
								energy_dispose(no_ctx);
								pthread_mutex_unlock(&node_energy_lock_nm);
								return EAR_ERROR;
				}
				debug("Callback and sdr process\n");

				/* Cache file name generation */

				if (sdr_get_cache_filename(hostname, cachefilenamebuf, MAXPATHLEN) < 0)
								debug("Error in sdr_get_cache_filename\n");


				if (ipmi_sdr_cache_open(sdr_ctx, *ipmi_ctx, cachefilenamebuf) < 0){
								debug("Error ipmi_sdr_cache_open, creating it\n");
								char buff[4096];
								sndebug(buff,sizeof(buff), "%s/bin/ipmi-oem intelnm get-node-manager-statistics mode=globalpower", FREEIPMI_BASE);
								if (execute(buff) != EAR_SUCCESS){
												energy_dispose(*ipmi_ctx);
												pthread_mutex_unlock(&node_energy_lock_nm);
												return EAR_ERROR;
								}

				}


				debug("ipmi_sdr_cache_iterate\n");
				if (ipmi_sdr_cache_iterate (sdr_ctx,
																intelnm_sdr_callback,
																NULL) < 0)
				{
								if (!found){
												debug("Error ipmi_sdr_cache_iterate\n");
												energy_dispose(*ipmi_ctx);
												pthread_mutex_unlock(&node_energy_lock_nm);
												return EAR_ERROR;
								}
				}


				if (!found)
				{  
								debug("ARG not fount! using default\n");
								slave = 0x2c;
								lun = 0x00;
								channel = 0x06;
								return EAR_SUCCESS;

				}

				/* From IPMI documentation */
				slave <<= 1;

				debug("after init channel %x slave %x lun %x \n", channel,  slave, lun);

				ipmi_sdr_cache_close(sdr_ctx);
				ipmi_sdr_ctx_destroy(sdr_ctx);
				sdr_ctx = NULL;
i
		uint maxp, minp, avgp, currentp, periodp;
    st = inm_power_reading(*ipmi_ctx, &maxp, &minp, &avgp, &currentp, &periodp);
    if (st != EAR_SUCCESS){
      debug("_power_reading fails");
			energy_dispose(*ipmi_ctx);
			return EAR_ERROR;
    }else{
      inm_last_power_reading = currentp;
      inm_sus = suscription();
			inm_timeframe = periodp * 2;
      inm_sus->call_main = inm_thread_main;
      inm_sus->call_init = inm_thread_init;
      inm_sus->time_relax = inm_timeframe;
      inm_sus->time_burst = inm_timeframe;
      inm_sus->suscribe(inm_sus);
      debug("inm energy plugin suscription initialized with timeframe %lu ms",inm_timeframe);
    }
  }


				return EAR_SUCCESS;    

}

state_t inm_node_power_reading(ipmi_ctx_t ipmi_ctx, uint *maxp, uint *minp, uint *avgp, uint *currentp, uint *periodp)
{
				uint8_t mode = IPMI_OEM_INTEL_NODE_MANAGER_STATISTICS_MODE_GLOBAL_POWER_STATISTICS;
				uint8_t domainid = IPMI_OEM_INTEL_NODE_MANAGER_DOMAIN_ID_ENTIRE_PLATFORM;
				uint8_t policyid = 0;
				int policyid_specified = 0;
				uint64_t val;
				fiid_obj_t obj_cmd_rs = NULL;

				if (!(obj_cmd_rs = fiid_obj_create (tmpl_cmd_oem_intel_node_manager_get_node_manager_statistics_rs)))
				{
								debug("fiid_obj_create: %s\n", strerror (errno));
								energy_dispose();
								return EAR_ERROR;
				}


				if (ipmi_cmd_oem_intel_node_manager_get_node_manager_statistics (ipmi_ctx,
																channel,
																slave,
																lun,
																mode,
																domainid,
																policyid,
																obj_cmd_rs) < 0){
								debug("Error ipmi_cmd_oem_intel_node_manager_get_node_manager_statistics\n");
								energy_dispose(ipmi_ctx);
								return EAR_ERROR;
				}

				if (FIID_OBJ_GET (obj_cmd_rs,
																"average",
																&val) < 0)
				{
								debug("FIID_OBJ_GET: 'average': %s\n",
																fiid_obj_errormsg (obj_cmd_rs));
								return EAR_ERROR;
				}
				*avgp = val; 
				//debug("Average power %d\n", val);
				if (FIID_OBJ_GET (obj_cmd_rs,
																"maximum",
																&val) < 0)
				{
								debug("FIID_OBJ_GET: 'maximum': %s\n",
																fiid_obj_errormsg (obj_cmd_rs));
								return EAR_ERROR;
				}
				*maxp = val;
				//debug("Maximum power %d\n", val);
				if (FIID_OBJ_GET (obj_cmd_rs,
																"minimum",
																&val) < 0)
				{
								debug("FIID_OBJ_GET: 'minimum': %s\n",
																fiid_obj_errormsg (obj_cmd_rs));
								return EAR_ERROR;
				}
				*minp = val;
				//debug("Minimum power %d\n", val);

				if (FIID_OBJ_GET (obj_cmd_rs,
																"current",
																&val) < 0)
				{
								debug("FIID_OBJ_GET: 'current power': %s\n",
																fiid_obj_errormsg (obj_cmd_rs));
								return EAR_ERROR;
				}
				*currentp = val;
				if (FIID_OBJ_GET (obj_cmd_rs,
																"statistics_reporting_period",
																&val) < 0)
				{
								debug("FIID_OBJ_GET: 'statistics_reporting_period': %s\n",
																fiid_obj_errormsg (obj_cmd_rs));
								return EAR_ERROR;
				}
				*periodp = val;
				//debug("Statistics reporting period %d\n", val);
				return EAR_SUCCESS;
}

/**** These functions are used to accumulate the eneergy */
state_t inm_thread_main(void *p)
{
  uint maxp, minp, avgp, currentp, periodp;
	ulong current_elapsed;
	ulong current_energy;
	time_t currt = time(NULL);

  st = inm_power_reading(ipmi_ctx, &maxp, &minp, &avgp, &currentp, &periodp);
  if (currentp > 0){
    current_elapsed = currt - last_timestamp;
		last_timestamp = currt;		

    current_energy = current_elapsed * currentp;
    /* Energy is reported in MJ */
    inm_accumulated_energy += (current_energy*1000);
    debug("AVG power in last %lu ms is %lu",current_elapsed, currentp);
  }else{
    debug("Current power is 0 in inm power reading ");
    return EAR_ERROR;
  }
  return EAR_SUCCESS;
}

state_t dcmi_thread_init(void *p)
{
	last_timestamp = time(NULL);
  return EAR_SUCCESS;
}




/* Release access to ipmi device */
state_t energy_dispose(void **c) {
			
				if (!inm_initialized) return EAR_ERROR;

				// // Close context
				ipmi_ctx_close (ipmi_ctx);
				// delete context
				ipmi_ctx_destroy (ipmi_ctx);
				ipmi_ctx = NULL;
				if (sdr_ctx == NULL){
								return EAR_SUCCESS;
				}
				ipmi_sdr_cache_close(sdr_ctx);
				ipmi_sdr_ctx_destroy(sdr_ctx);
				sdr_ctx = NULL;

				return EAR_SUCCESS;
}

state_t energy_datasize(size_t *size)
{ 
				debug("energy_datasize %lu\n",sizeof(unsigned long));
				*size=sizeof(unsigned long);
				return EAR_SUCCESS;
}

state_t energy_frequency(ulong *freq_us) {
				*freq_us = inm_timeframe;
				return EAR_SUCCESS;
}

state_t energy_to_str(char *str, edata_t e) {
				ulong *pe = (ulong *) e;
				sprintf(str, "%lu", *pe);
				return EAR_SUCCESS;
}
unsigned long diff_node_energy(ulong init,ulong end)
{ 
				ulong ret=0;
				if (end>=init){
								ret=end-init;
				} else{
								ret=ulong_diff_overflow(init,end);
				}
				return ret;
}


state_t energy_units(uint *units) {
				*units = 1000;
				return EAR_SUCCESS;
}

state_t energy_accumulated(unsigned long *e, edata_t init, edata_t end) {
				ulong *pinit = (ulong *) init, *pend = (ulong *) end;

				unsigned long total = diff_node_energy(*pinit, *pend);
				*e = total;
				return EAR_SUCCESS;
}


state_t energy_dc_read(void *c, edata_t energy_mj) {
				ulong *penergy_mj=(ulong *)energy_mj;

				debug("energy_dc_read\n");

				*penergy_mj= inm_accumulated_energy;

				return EAR_SUCCESS;
}

state_t energy_dc_time_read(void *c, edata_t energy_mj, ulong *time_ms)
{
				ulong *penergy_mj=(ulong *)energy_mj;

				debug("energy_dc_read\n");

				*penergy_mj = inm_accumulated_energy;
				*time_ms = last_timestamp*1000;

				return EAR_SUCCESS;

}
uint energy_data_is_null(edata_t e)
{
				ulong *pe=(ulong *)e;
				return (*pe == 0);

}


#if 0
void main(int argc , char *argv[])
{
				uint iters = 0;
				uint maxp, minp, avgp, periodp, currentp;
				node_energy_init();
				do{
								node_power_reading(&maxp, &minp, &avgp, &currentp, &periodp);
								debug("POWER READING: Max power %u Min power %u Avg power %u Current power %u Period %u\n", maxp, minp, avgp, currentp, periodp);
								sleep(10);
								iters++;
				}while(iters < 10);
				node_energy_dispose();

}
#endif

