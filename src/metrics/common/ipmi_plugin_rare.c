/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

//#define SHOW_DEBUGS 1

#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <common/output/verbose.h>
#include <common/system/monitor.h>
#include <common/system/lock.h>
#include <common/states.h> //clean
#include <common/system/poll.h>
#include <common/math_operations.h>
#include <metrics/energy/node/ipmi_sensor.h>
#include <metrics/energy/node/energy_node.h>

#define SENSOR_POWER_PERIOD 2000
#define VIPMI	2

static pthread_mutex_t ipmi_sensor_power_lock = PTHREAD_MUTEX_INITIALIZER;
static uint monitor_done = 0;
static state_t ipmi_sensor_thread_main(void *p);
static state_t ipmi_sensor_thread_init(void *p);
static uint ipmi_sensor_power_initialized = 0;
static struct ipmi_intf ipmi_sensor_context_for_pool;
static struct ipmi_intf ipmi_sensor_context_for_main;
static timestamp_t  last_timestamp;
static suscription_t *ipmi_sensor_power_sus;
static ulong accum_ipmi_sensor_energy = 0;



static int opendev(struct ipmi_intf *intf)
{
    intf->fd = open("/dev/ipmi0", O_RDWR);
    if (intf->fd < 0) {
        intf->fd = open("/dev/ipmi/0", O_RDWR);
        if (intf->fd < 0) {
            intf->fd = open("/dev/ipmidev/0", O_RDWR);
        };
    };
    return intf->fd;
};

static void closedev(struct ipmi_intf *intf)
{
    if (intf->fd > 0) {
        close(intf->fd);
        intf->fd = -1;
    };
};

static struct ipmi_rs *sendcmd(struct ipmi_intf *intf, struct ipmi_rq *req)
{
    struct ipmi_req _req;
    struct ipmi_recv recv;
    struct ipmi_addr addr;
    struct ipmi_system_interface_addr bmc_addr = {
            .addr_type =    IPMI_SYSTEM_INTERFACE_ADDR_TYPE,
            .channel =    IPMI_BMC_CHANNEL,
    };
    struct ipmi_ipmb_addr ipmb_addr = {
            .addr_type =    IPMI_IPMB_ADDR_TYPE,
            .channel =    intf->channel & 0x0f,
    };
    static struct ipmi_rs rsp;
    uint8_t *data = NULL;
    static int curr_seq = 0;
    afd_set_t rset;

    if (intf == NULL || req == NULL)
        return NULL;
    memset(&_req, 0, sizeof(struct ipmi_req));

    if (intf->addr != 0) {
        ipmb_addr.slave_addr = intf->addr;
        ipmb_addr.lun = req->msg.lun;
        _req.addr = (unsigned char *) &ipmb_addr;
        _req.addr_len = sizeof(ipmb_addr);
        debug("IPMB channel %x", ipmb_addr.channel);

    } else {
        bmc_addr.lun = req->msg.lun;
        _req.addr = (unsigned char *) &bmc_addr;
        _req.addr_len = sizeof(bmc_addr);
        debug("BMC channel %x", bmc_addr.channel);
    };
    _req.msgid = curr_seq++;

    _req.msg.data = req->msg.data;
    _req.msg.data_len = req->msg.data_len;
    _req.msg.netfn = req->msg.netfn;
    _req.msg.cmd = req->msg.cmd;

    debug("Channel %p, bcm %p ipmb %p", _req.addr, &bmc_addr, &ipmb_addr);


    if (ioctl(intf->fd, IPMICTL_SEND_COMMAND, &_req) < 0) {
        debug("Unable to send command\n");
        if (data != NULL)
            free(data);
        return NULL;
    };

    AFD_ZERO(&rset);
    AFD_SET(intf->fd, &rset);

    if (aselectv(&rset, NULL) < 0) {
        debug("I/O Error\n");
        if (data != NULL)
            free(data);
        return NULL;
    };
    if (AFD_ISSET(intf->fd, &rset) == 0) {
        debug("No data available\n");
        if (data != NULL)
            free(data);
        return NULL;
    };

    recv.addr = (unsigned char *) &addr;
    recv.addr_len = sizeof(addr);
    recv.msg.data = rsp.data;
    recv.msg.data_len = sizeof(rsp.data);
    if (ioctl(intf->fd, IPMICTL_RECEIVE_MSG_TRUNC, &recv) < 0) {
        debug("Error receiving message\n");
        if (errno != EMSGSIZE) {
            if (data != NULL)
                free(data);
            return NULL;
        };
    };


    /* save completion code */
    rsp.ccode = recv.msg.data[0];
    rsp.data_len = recv.msg.data_len - 1;

    if (recv.msg.data[0] == 0) {

        /* save response data for caller */
        if (rsp.ccode == 0 && rsp.data_len > 0) {
            memmove(rsp.data, rsp.data + 1, rsp.data_len);
            rsp.data[recv.msg.data_len] = 0;
        };

        if (data != NULL)
            free(data);
        return &rsp;
    };
    rsp.ccode = recv.msg.data[0];
    rsp.data_len = recv.msg.data_len - 1;
    return &rsp;
} //sendcmd


static ulong from_raw_to_decimal(ulong raw, ulong M, ulong B, ulong R)
{
    /*
     * CPU sensor
     Sensor Number: 120 = 0x78
     Record Type: 0x01
     Conversion factors:
     M = 10.000000
     B = 0.000000
     R exp = 0

     To convert a raw reading (x):
     y = (10.000000 × x) + (0.000000 × 10^0)

     GPU sensor
     Sensor Record ID: 0x1400
     Sensor Number: 34
     Record Type: 0x01
     Conversion factors:
     M = 15.000000
     B = 150.000000
     R exp = 0

     To convert a raw reading (x):
     y = (15.000000 × x) + (150.000000 × 10^0)
     y = (15.000000 × x) + 150.000000

*/
    ulong decimal;
    decimal   =  (M * raw) + (ulong)(B * exp10f(R));
    debug("IPMI sensor: from raw to decimal %lu = (%lu * %lu) + %lu * exp10 %lu", decimal, M, raw, B, R);
    return decimal;
}


#define SENSOR_NETFN 		0x04
#define SENSOR_CMD		0x2D
#define GPU_SENSOR 		0x22 // CWG_LM5066I_PIN
#define CPU_SENSOR		0x78 // LM5066_PIN
#define CPU_SENSOR_BYTE         0
#define GPU_SENSOR_BYTE         0
#define M_CPU			10
#define B_CPU			0
#define R_CPU			0
#define M_GPU			15
#define B_GPU			150
#define R_GPU			0

/* This function assumes the lock is already acquired */
static state_t ipmi_sensor_reading(struct ipmi_intf *intf, struct ipmi_data *out, uint8_t sensor_id, ulong *raw_value, uint RAW_POS)
{
    struct ipmi_rs *rsp;
    struct ipmi_rq req;
    uint8_t msg_data[1];
    ulong  sensor[1];
    uint8_t *bytes_rs;
    uint8_t *sensor_8 = (uint8_t *)sensor;

    int i;




    memset(&req, 0, sizeof(req));
    req.msg.netfn = SENSOR_NETFN;
    req.msg.cmd = SENSOR_CMD;
    msg_data[0]=(uint8_t)sensor_id;
    req.msg.data = msg_data;
    req.msg.data_len = sizeof(msg_data);

    rsp = sendcmd(intf, &req);
    if (rsp == NULL) {
        out->mode=-1;
        return EAR_ERROR;
    };
    if (rsp->ccode > 0) {
        out->mode=-1;
        return EAR_ERROR;
    };

    out->data_len=rsp->data_len;
    for (i=0;i<rsp->data_len; i++) {
        out->data[i]=rsp->data[i];
        debug("IPMI sensor recv data[%d]=%x ",i,out->data[i]);
    }
    bytes_rs 	= out->data;
    sensor[0] 	= 0;
    sensor_8[0] 	= bytes_rs[RAW_POS];
    *raw_value 	= sensor[0];
    return EAR_SUCCESS;
}

/*
 * MONITOR
 */
static state_t ipmi_sensor_thread_main(void *p)
{
    struct ipmi_data out;
    ulong raw_value;
    ulong current_elapsed,current_energy = 0;
    state_t st;
    timestamp_t 	curr_time;
    ulong last_cpu_power = 0, last_gpu_power = 0;

    verbose(VIPMI,"IPMI sensor power monitor");
    if (!ipmi_sensor_power_initialized) return EAR_ERROR;

    debug("%sIPMI sensor: ipmi_sensor_thread_main%s", COL_RED, COL_CLR);

    if( ear_trylock(&ipmi_sensor_power_lock) != EAR_SUCCESS){
        debug("IPMI sensor: Lock not available");
        return EAR_SUCCESS;
    }
    timestamp_get(&curr_time);
    current_elapsed = timestamp_diff(&curr_time, &last_timestamp, TIME_SECS);
    /* CPU sensor */
    st = ipmi_sensor_reading(&ipmi_sensor_context_for_pool,&out, CPU_SENSOR, &raw_value, CPU_SENSOR_BYTE);
    if (st == EAR_SUCCESS){
        debug("IPMI sensor : CPU sensor reading num byes %d", out.data_len);
        last_cpu_power = from_raw_to_decimal(raw_value, M_CPU, B_CPU, R_CPU);
        current_energy = last_cpu_power * current_elapsed;
        verbose(VIPMI,"IPMI sensor : CPU power %lu", last_cpu_power);
    }
#if GPU_SENSOR
    /* GPU sensor */
    st = ipmi_sensor_reading(&ipmi_sensor_context_for_pool,&out, GPU_SENSOR, &raw_value, GPU_SENSOR_BYTE);
    if (st == EAR_SUCCESS){
        debug("IPMI sensor : GPU sensor reading num byes %d", out.data_len);
        last_gpu_power = from_raw_to_decimal(raw_value, M_GPU, B_GPU, R_GPU);
        current_energy += last_gpu_power * current_elapsed;
        verbose(VIPMI,"IPMI sensor : GPU power %lu", last_gpu_power);
    }
#endif
    /* Energy is reported in MJ */
    accum_ipmi_sensor_energy += (current_energy*1000);
    last_timestamp = curr_time;
    debug("IPMI sensor: CPU power %lu GPU power %lu, elapsed %lu. Accum energy %lu", last_cpu_power, last_gpu_power, current_elapsed, accum_ipmi_sensor_energy);
    ear_unlock(&ipmi_sensor_power_lock);

    return EAR_SUCCESS;
}

static state_t ipmi_sensor_thread_init(void *p)
{
    int ret;

    debug("%sIPMI sensor ipmi_sensor_power_thread_init%s", COL_GRE, COL_CLR);

    timestamp_get(&last_timestamp);
    ear_lock(&ipmi_sensor_power_lock);
    ret= opendev(&ipmi_sensor_context_for_pool);
    if (ret<0){
        debug("IPMI sensor: opendev failed");
        ear_unlock(&ipmi_sensor_power_lock);
        return EAR_ERROR;
    }

    ear_unlock(&ipmi_sensor_power_lock);

    return EAR_SUCCESS;
}





/*
 * MAIN FUNCTIONS
 */

state_t energy_init(void **c)
{
    int ret;

    if (c == NULL) {
        return EAR_ERROR;
    }

    *c = (struct ipmi_intf *) malloc(sizeof(struct ipmi_intf));
    if (*c == NULL) {
        return EAR_ERROR;
    }

    ear_lock(&ipmi_sensor_power_lock);
    if (ipmi_sensor_power_initialized){
        memcpy((struct ipmi_intf *)*c, &ipmi_sensor_context_for_main, sizeof(struct ipmi_intf));
        ear_unlock(&ipmi_sensor_power_lock);
        return EAR_SUCCESS;
    }

    /* Open a static dev for main queries */
    ret= opendev((struct ipmi_intf *)&ipmi_sensor_context_for_main);
    if (ret<0){
        debug("IPMPI sensor: opendev failed");
        ear_unlock(&ipmi_sensor_power_lock);
        return EAR_ERROR;
    }
    memcpy((struct ipmi_intf *)*c, &ipmi_sensor_context_for_main, sizeof(struct ipmi_intf));

    if (!monitor_is_initialized()){
        debug("IPMI sensor initializing monitor");
        monitor_init();
    }
    if (!monitor_done){
        ipmi_sensor_power_sus = suscription();
        ipmi_sensor_power_sus->call_main = ipmi_sensor_thread_main;
        ipmi_sensor_power_sus->call_init = ipmi_sensor_thread_init;
        ipmi_sensor_power_sus->time_relax = SENSOR_POWER_PERIOD;
        ipmi_sensor_power_sus->time_burst = SENSOR_POWER_PERIOD;
        ipmi_sensor_power_initialized = 1;
        monitor_done = 1;
        ear_unlock(&ipmi_sensor_power_lock);
        monitor_register(ipmi_sensor_power_sus);
        debug("IPMI sensor: energy plugin suscription initialized with timeframe %u ms",SENSOR_POWER_PERIOD);

    }else ear_unlock(&ipmi_sensor_power_lock);


    return EAR_SUCCESS;
}

state_t energy_dispose(void **c) {
    if ((c == NULL) || (*c == NULL)) return EAR_ERROR;
    ear_lock(&ipmi_sensor_power_lock);
    closedev((struct ipmi_intf *) *c);
    free(*c);
    ear_unlock(&ipmi_sensor_power_lock);
    return EAR_SUCCESS;
}
state_t energy_datasize(size_t *size)
{
    debug("energy_datasize %lu\n",sizeof(unsigned long));
    *size=sizeof(unsigned long);
    return EAR_SUCCESS;
}

state_t energy_frequency(ulong *freq_us) {
    *freq_us = 1000000;
    return EAR_SUCCESS;
}

state_t energy_dc_read(void *c, edata_t energy_mj) {
    struct ipmi_data out;
    ulong raw_value;
    ulong current_elapsed,current_energy = 0;
    state_t st;
    timestamp_t 	curr_time;
    ulong * energy_p = (ulong *)energy_mj;
    ulong last_cpu_power = 0, last_gpu_power = 0;

    *energy_p = 0;
    if (!ipmi_sensor_power_initialized) return EAR_ERROR;

    debug("%sIPMI sensor: energy_dc_read%s", COL_RED, COL_CLR);

    if( ear_trylock(&ipmi_sensor_power_lock) != EAR_SUCCESS){
        debug("IPMI sensor: Lock not available");
        return EAR_SUCCESS;
    }
    timestamp_get(&curr_time);
    current_elapsed = timestamp_diff(&curr_time, &last_timestamp, TIME_SECS);
    /* CPU sensor */
    st = ipmi_sensor_reading(&ipmi_sensor_context_for_pool,&out, CPU_SENSOR, &raw_value, CPU_SENSOR_BYTE);
    if (st == EAR_SUCCESS){
        debug("IPMI sensor : CPU sensor reading num byes %d", out.data_len);
        last_cpu_power = from_raw_to_decimal(raw_value, M_CPU, B_CPU, R_CPU);
        current_energy = last_cpu_power * current_elapsed;
    }
#if GPU_SENSOR
    /* GPU sensor */
    st = ipmi_sensor_reading(&ipmi_sensor_context_for_pool,&out, GPU_SENSOR, &raw_value, GPU_SENSOR_BYTE);
    if (st == EAR_SUCCESS){
        debug("IPMI sensor : GPU sensor reading num byes %d", out.data_len);
        last_gpu_power = from_raw_to_decimal(raw_value, M_GPU, B_GPU, R_GPU);
        current_energy += last_gpu_power * current_elapsed;
    }
#endif
    /* Energy is reported in MJ */
    accum_ipmi_sensor_energy += (current_energy*1000);
    last_timestamp = curr_time;
    debug("IPMI sensor: CPU power %lu GPU power %lu, elapsed %lu. Accum energy %lu", last_cpu_power, last_gpu_power, current_elapsed, accum_ipmi_sensor_energy);
    ear_unlock(&ipmi_sensor_power_lock);
    *energy_p = accum_ipmi_sensor_energy;

    return EAR_SUCCESS;
}



state_t energy_dc_time_read(void *c, edata_t energy_mj, ulong *time_ms) {
    timestamp_t curr_time;
    /* This converts current timestamp in msec */
    *time_ms = (ulong)timestamp_convert(&curr_time, TIME_MSECS);

    return energy_dc_read(c, energy_mj);
}

state_t energy_ac_read(void *c, edata_t energy_mj) {
    ulong *penergy_mj = (ulong *) energy_mj;
    *penergy_mj = 0;
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

state_t energy_to_str(char *str, edata_t e) {
    ulong *pe = (ulong *) e;
    sprintf(str, "%lu", *pe);
    return EAR_SUCCESS;
}

uint energy_data_is_null(edata_t e)
{
    ulong *pe=(ulong *)e;
    return (*pe == 0);

}