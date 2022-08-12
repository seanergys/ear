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
* EAR is an open source software, and it is licensed under both the BSD-3 license
* and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
* and COPYING.EPL files.
*/

#define SHOW_DEBUGS 1

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <common/output/debug.h>
#include <common/hardware/bithack.h>
#include <common/hardware/topology.h>
#include <management/cpufreq/priority.h>

#define PM_CMD1                        0x95 //WRITE_PM_CONFIG
#define PM_CMD2_SET_CONFIG             0x03 //PM_FEATURE 
#define PM_ARG2_SET_CONFIG            (0x01 << 16) 
#define CP_CMD1                        0xD0 //CONFIG_CLOS
#define CP_CMD2_GET_CONFIG             0x02 //CLOS_PM_QOS_CONFIG
#define CP_CMD2_SET_CONFIG             CP_CMD2_GET_CONFIG
#define CP_CMD2_GET_CLOS_CONFIG(clos) (0x08 + ((clos & 0x03) * 4)) //PM_CLOS_OFFSET
#define CP_CMD2_GET_CPU_ASSOC(cpu)    (0x20 + ((cpu  & 0xFF) * 4)) //PQR_ASSOC_OFFSET
#define CP_ARG1_SET_CONFIG            (0x01 << 0x08) //MBOX_CMD_WRITE_BIT
#define CP_ARG2_SET_CONFIG(en, prio)  ((prio << 2) | (en << 1))
#define TF_CMD1                        0x7F //CONFIG_TDP
#define TF_CMD2_SET_CONFIG             0x02 //CONFIG_TDP_SET_TDP_CONTROL         
#define TF_ARG2_SET_CONFIG(en)        ((en << 16))
//#define TF_ARG2_SET_CONFIG(en)        ((en << 17) | (en << 16))

#define ISST_IF_MAGIC      0xFE
#define ISST_IF_PHYS_CMD  _IOWR(ISST_IF_MAGIC, 1, struct isst_if_cpu_map *)
#define ISST_IF_INFO_CMD  _IOR (ISST_IF_MAGIC, 0, struct isst_if_platform_info *)
#define ISST_IF_MMIO_CMD  _IOW (ISST_IF_MAGIC, 2, struct isst_if_io_regs *)
#define ISST_IF_MBOX_CMD  _IOWR(ISST_IF_MAGIC, 3, struct isst_if_mbox_cmds *)
#define ISST_IF_MSR_CMD   _IOWR(ISST_IF_MAGIC, 4, struct isst_if_msr_cmds *)

struct isst_if_msr_cmd {
    uint read_write;
    uint logical_cpu;
    ullong msr;
    ullong data;
};

struct isst_if_msr_cmds {
    uint cmd_count;
    struct isst_if_msr_cmd msr_cmd[1];
};

struct isst_if_cpu_map {
    uint logical_cpu;
    uint physical_cpu;
};

struct isst_if_cpu_maps {
    uint cmd_count;
    struct isst_if_cpu_map cpu_map[1];
};

struct isst_if_platform_info {
    ushort api_version;
    ushort driver_version;
    ushort max_cmds_per_ioctl;
    uchar  mbox_supported;
    uchar  mmio_supported;
};

struct isst_if_io_reg {
    uint read_write;
    uint logical_cpu;
    uint reg;
    uint value;
};

struct isst_if_io_regs {
    uint req_count;
    struct isst_if_io_reg io_reg[1];
};

struct isst_if_mbox_cmd {
    uint   logical_cpu;
    uint   parameter;
    uint   req_data;
    uint   resp_data;
    ushort command;
    ushort sub_command;
    uint   reserved;
};

struct isst_if_mbox_cmds {
    uint cmd_count;
    struct isst_if_mbox_cmd mbox_cmd[1];
};

static char      *driver_path = "/dev/isst_interface";
static int        driver_fd   = -1;
static char      *strprio[2]  = { "high", "low" };
static cpuprio_t  clos[4];
static uint       cpus_count;
static uint      *punit_cpus;
static topology_t tp;

/*
 *
 * SST API
 *
 */

static int send_msr_command(uint cpu, ullong msr, int write, ullong value, ullong *answer)
{
    struct isst_if_msr_cmds msr_cmds;
    int read = !write;

    *answer = 0;

    msr_cmds.cmd_count              = 1;
    msr_cmds.msr_cmd[0].logical_cpu = cpu;
    msr_cmds.msr_cmd[0].msr         = msr;
    msr_cmds.msr_cmd[0].data        = value * write;
    msr_cmds.msr_cmd[0].read_write  = write;
    
    if (ioctl(driver_fd, ISST_IF_MSR_CMD, &msr_cmds) == -1) {
    } else {
        if (read) {
            *answer = msr_cmds.msr_cmd[0].data;
        }
    }

    return 0;
}

static int send_mmio_command(uint cpu, uint cmd1, uint cmd2, int write, uint value, uint *answer)
{
    struct isst_if_io_regs io_regs = { 0 };
    int read = !write;

    *answer = 0;

    io_regs.req_count             = 1;
    io_regs.io_reg[0].logical_cpu = cpu;
    io_regs.io_reg[0].reg         = cmd2;
    io_regs.io_reg[0].read_write  = write;
    io_regs.io_reg[0].value       = write * value;
    debug("MMIO_CMD: cpu %d, reg 0x%X, read_write %d, value 0x%X",
        io_regs.io_reg[0].logical_cpu, io_regs.io_reg[0].reg, io_regs.io_reg[0].read_write, io_regs.io_reg[0].value);
 
    if (ioctl(driver_fd, ISST_IF_MMIO_CMD, &io_regs) == -1) {
        debug("ioctl failed: %s", strerror(errno))
    } else {
        if (read) {
            *answer = io_regs.io_reg[0].value;
        }
        //debug("Answer: %u", *answer);
    }

    return 0;
}

static void send_mbox_command(uint cpu, uint cmd1, uint cmd2, uint param, uint req_data, uint *answer)
{
    struct isst_if_mbox_cmds cmds = { 0 };
    uint retrys = 3;
    
    *answer = 0;
    // Command fill
    cmds.cmd_count               = 1;
    cmds.mbox_cmd[0].logical_cpu = cpu;
    cmds.mbox_cmd[0].command     = cmd1;
    cmds.mbox_cmd[0].sub_command = cmd2;
    cmds.mbox_cmd[0].parameter   = param;
    cmds.mbox_cmd[0].req_data    = req_data;
    // Do and retry
    do {
        if (ioctl(driver_fd, ISST_IF_MBOX_CMD, &cmds) == -1) {
            debug("ioctl failed: %u retrys remaining (%d, %s)", retrys, errno, strerror(errno));
            --retrys;
        } else {
            *answer = cmds.mbox_cmd[0].resp_data;
            break;
        }
    } while (retrys);

    if (!retrys) {
        debug("Failed MBOX command");
    } else {
        debug("Answer: %u", *answer);
    }
}

// Command         | Count         | Optimizable
// ----------------|---------------| ------------
// driver info     | 1             | no
// cp status       | 1             | no
// map cpus        | cpus_count    | yes
// get clos config | 4             | yes
// get clos assoc  | cpus_count    | yes
// set clos assoc  | cpus_count    | yes
// enable cp       | sockets_count | no
// enable tf       | sockets_count | no

static state_t sst_map_cpus()
{
    struct isst_if_platform_info info = { 0 };
    struct isst_if_cpu_maps map;
    uint answer;
    int i;

    for (i = 0; i < cpus_count; ++i)
    {
        map.cmd_count = 1;
        map.cpu_map[0].logical_cpu = i;
 
        if (ioctl(driver_fd, ISST_IF_PHYS_CMD, &map) == -1) {
           debug("ioctl failed in cpu %d: %s", i, strerror(errno));
            continue;
        }
        punit_cpus[i] = (map.cpu_map[0].physical_cpu >> 1);
    }
    #if SHOW_DEBUGS
    for (i = 0; i < cpus_count; ++i) {
        if ((i != 0) && (i % 8) == 0) dprintf(debug_channel, "\n");
        dprintf(debug_channel, "[%03d,%03d] ", i, punit_cpus[i]);
    }
    dprintf(debug_channel, "\n");
    #endif
    if (ioctl(driver_fd, ISST_IF_INFO_CMD, &info) == -1) {
        debug("ioctl failed: %s", strerror(errno));
        return 0;
    }

    debug("API version   : %d", info.api_version);
    debug("Driver version: %d", info.driver_version);
    debug("MBOX supported: %d", info.mbox_supported);
    debug("MMIO supported: %d", info.mmio_supported);

    return EAR_SUCCESS;
}

state_t sst_init(topology_t *_tp)
{
    // Opening driver
    if ((driver_fd = open(driver_path, O_RDWR)) < 0) {
        return EAR_ERROR;
    }
    debug("Opened %s with FD %d", driver_path, driver_fd);
    
    // Counting CPUs 
    cpus_count = _tp->cpu_count;
    // Allocating SST CPU mapping
    punit_cpus = calloc(cpus_count, sizeof(uint));
    // Getting the list of sockets
    topology_select(_tp, &tp, TPSelect.socket, TPGroup.merge, 0);
    // Mapping the CPUs SST style
    sst_map_cpus();

    return EAR_SUCCESS;
}

state_t sst_enable(uint core_power, uint turbo_freq)
{
    ullong ans2;
    uint ans1;
    uint i;

    for (i = 0; core_power && i < tp.cpu_count; ++i)
    {
        // Enabling PM (¿What is PM?)
        send_mbox_command(tp.cpus[i].id, PM_CMD1, PM_CMD2_SET_CONFIG, 0, PM_ARG2_SET_CONFIG, &ans1);
        // Get current core-power status (prio 0: proportional, prio 1: ordered)
        send_mbox_command(tp.cpus[i].id, CP_CMD1, CP_CMD2_GET_CONFIG, 0, 0, &ans1);
        debug("CP enabled : %d", getbits32(ans1, 1, 1));
        debug("CP priority: %d", getbits32(ans1, 2, 2));
        // Enabling CP, append the current core-power status enabling it with orderder priority 
        send_mbox_command(tp.cpus[i].id, CP_CMD1, CP_CMD2_SET_CONFIG, CP_ARG1_SET_CONFIG,
            ans1 | CP_ARG2_SET_CONFIG(1, 1), &ans1);

    }
    for (i = 0; i < tp.cpu_count; ++i) {
        // Enabling TF
        send_mbox_command(tp.cpus[i].id, TF_CMD1, TF_CMD2_SET_CONFIG, 0, TF_ARG2_SET_CONFIG(1), &ans1);
        // Comentar a Julita. Encontrada la opcion turbo-mode en el código de la aplicación,
        // pero no en la aplicación del Ice Lake. ¿Posible función para llegar a MAX BOOST
        // sin necesidad de turbo-freq?
        send_msr_command(tp.cpus[i].id, 0x1AD, 1, 0xFFFFFFFFFFFFFFFFULL, &ans2);
    }

    return EAR_SUCCESS;
}

static void sst_clos_print(cpuprio_t *clos, int fd)
{
    int c;

    for (c = 0; c < 4; ++c)
    {
        if (clos[c].max_khz == 255*1000) {
            dprintf(fd, "CLOS%d: MAX - %llu KHz (%s)\n", c, 
               clos[c].min_khz, strprio[clos[c].low]);
        } else {
            dprintf(fd, "CLOS%d: %llu KHz - %llu KHz (%s)\n", c, 
               clos[c].max_khz, clos[c].min_khz, strprio[clos[c].low]);
        }
    } 
}

state_t sst_clos_get_available_list(cpuprio_t *clos)
{
    uint answer;
    int c;

    for (c = 0; c < 4; ++c)
    {
        send_mmio_command(0, CP_CMD1, CP_CMD2_GET_CLOS_CONFIG(c), 0, 0, &answer);
        
        clos[c].idx     = c;
        clos[c].low     = c > 1;
        clos[c].high    = c < 2;
        clos[c].max_khz = ((answer >> 16) & 0xff) * 1000;
        clos[c].min_khz = ((answer >>  8) & 0xff) * 1000;
    }
    #if SHOW_DEBUGS
    sst_clos_print(clos, verb_channel);
    #endif

    return EAR_SUCCESS;
}

state_t sst_clos_get_assoc_list(uint *clos_list)
{
    uint answer;
    int cpu;
   
    for (cpu = 0; cpu < cpus_count; ++cpu) {
        send_mmio_command(cpu, CP_CMD1, CP_CMD2_GET_CPU_ASSOC(punit_cpus[cpu]), 0, 0, &answer);
        clos_list[cpu] = (answer >> 16) & 0x03;
        //debug("CPU%00d: CLOS%d", cpu, clos_list[cpu]);
    }
    #if SHOW_DEBUGS
    for (cpu = 0; cpu < cpus_count; ++cpu) {
        if ((cpu != 0) && (cpu % 8) == 0) dprintf(debug_channel, "\n");
        dprintf(debug_channel, "[%03d,%d] ", cpu, clos_list[cpu]);
    }
    dprintf(debug_channel, "\n");
    #endif
    
    return EAR_SUCCESS;
}

state_t sst_clos_set_assoc(uint cpu, uint clos)
{
    uint answer;
    
    send_mmio_command(cpu, CP_CMD1, CP_CMD2_GET_CPU_ASSOC(punit_cpus[cpu]), 1, ((clos & 0x03) << 16), &answer);
    
    return EAR_SUCCESS;
}

state_t sst_clos_set_assoc_list(uint *clos_list)
{
    uint cpu;

    for (cpu = 0; cpu < cpus_count; ++cpu) {
        sst_clos_set_assoc(cpu, clos_list[cpu]);
    }

    return EAR_SUCCESS;
}


/*
 *
 * MANAGEMENT API
 *
 */


void mgt_cpufreq_prio_load(topology_t *_tp)
{
    if (state_fail(sst_init(_tp))) {
        return;
    }
}

void mgt_cpufreq_prio_init()
{
    // Enabling CP and TF
    if (state_fail(sst_enable(1, 1))) {
        return;
    }
}

void mgt_cpufreq_prio_dispose()
{
    close(driver_fd);
}

state_t mgt_cpufreq_prio_get_available_list(cpuprio_t *clos)
{
    return sst_clos_get_available_list(clos);
}

state_t mgt_cpufreq_prio_get_current_list(uint *clos)
{
    return sst_clos_get_assoc_list(clos);
}

state_t mgt_cpufreq_prio_set_current_list(uint *clos)
{
    return sst_clos_set_assoc_list(clos);
}

state_t mgt_cpufreq_prio_set_current(uint cpu, uint clos)
{
    return sst_clos_set_assoc(cpu, clos);
}

int main(int argc, char *argv[])
{
    cpuprio_t clos1[4];
    uint clos2[256];
    topology_t tp;

    topology_init(&tp);

    mgt_cpufreq_prio_load(&tp);
//    mgt_cpufreq_prio_init();   

    mgt_cpufreq_prio_get_available_list(clos1);
    mgt_cpufreq_prio_get_current_list(clos2);
 
    mgt_cpufreq_prio_dispose();

    return 0;
}
