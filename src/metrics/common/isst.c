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
#include <metrics/common/isst.h>

#define PM_GET_CONFIG                  0x094,0x03,0x00,0x00 //READ_PM_CONFIG,PM_FEATURE
#define CP_GET_CONFIG                  0x0D0,0x02,0x00,0x00 //CONFIG_CLOS,CLOS_PM_QOS_CONFIG
#define TF_GET_CONFIG                  0x07F,0x01,0x00,0x00 //CONFIG_TDP,CONFIG_TDP_GET_TDP_CONTROL
#define TR_GET_CONFIG                  0x1AD,0x00,0x00      //Turbo Ratio Limit
#define CP_GET_CLOS(c)                 (0x08 + ((c & 0x03) * 0x04)),0x00,0x00 //PM_CLOS_OFFSET
#define CP_GET_ASSOC(c)                (0x20 + ((c & 0xFF) * 0x04)),0x00,0x00 //PQR_ASSOC_OFFSET
#define ISST_IF_MAGIC                  0xFE
#define ISST_IF_PHYS_CMD              _IOWR(ISST_IF_MAGIC, 1, struct isst_if_cpu_map *)
#define ISST_IF_INFO_CMD              _IOR (ISST_IF_MAGIC, 0, struct isst_if_platform_info *)
#define ISST_IF_MMIO_CMD              _IOW (ISST_IF_MAGIC, 2, struct isst_if_io_regs *)
#define ISST_IF_MBOX_CMD              _IOWR(ISST_IF_MAGIC, 3, struct isst_if_mbox_cmds *)
#define ISST_IF_MSR_CMD               _IOWR(ISST_IF_MAGIC, 4, struct isst_if_msr_cmds *)
#define CMDS_MAX                       64 //ISST_IF_CMD_LIMIT
#define CMDS_SPLIT(i, total)          (((i + CMDS_MAX) <= total) * CMDS_MAX) + (((i + CMDS_MAX) > total) * (total % CMDS_MAX))

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
    struct isst_if_cpu_map cpu_map[CMDS_MAX];
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
    struct isst_if_io_reg io_reg[CMDS_MAX];
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
    struct isst_if_mbox_cmd mbox_cmd[CMDS_MAX];
};

/*
 *
 * My types
 *
 */

typedef struct isst_if_io_regs   cmds_mmio_t;
typedef struct isst_if_mbox_cmds cmds_mbox_t;

typedef struct envals_s {
    uint    cp_support;
    uint    tf_support;
    uint    bf_support;
    uint    cp_enabled;
    uint    tf_enabled;
    uint    bf_enabled;
} envals_t;

typedef struct invals_s {
    envals_t envals;
    clos_t   clos[4];
    ullong   trl;
} invals_t;

static char      *strtype[2]  = { "proportional", "ordered" };
static char      *strprio[2]  = { "high", "low" };
static char      *driver_path = "/dev/isst_interface";
static int        driver_fd   = -1;
static uint       cpus_count; // Total CPUs count
static uint      *punit_cpus;
static invals_t   invals;
static topology_t tp; // Socketed topology

static state_t commands_mmsr_send_single(uint cpu, ullong msr, ullong data, int write, ullong *answer)
{
    struct isst_if_msr_cmds msr_cmds;
    int read = !write;

    if (answer != NULL) {
        *answer = 0;
    }

    msr_cmds.cmd_count              = 1;
    msr_cmds.msr_cmd[0].logical_cpu = cpu;
    msr_cmds.msr_cmd[0].msr         = msr;
    msr_cmds.msr_cmd[0].data        = data * write;
    msr_cmds.msr_cmd[0].read_write  = write;

    if (ioctl(driver_fd, ISST_IF_MSR_CMD, &msr_cmds) == -1) {
            debug("ioctl failed for MSR command 0x%llX (%d, %s)",
                msr, errno, strerror(errno));
    } else {
        if (read) {
            *answer = msr_cmds.msr_cmd[0].data;
            debug("Answer for MMSR command 0x%llX: 0x%llX", msr, *answer);
        }
    }
    return EAR_SUCCESS;
}

static state_t commands_mmio_send(cmds_mmio_t *cmds, uint *answers)
{
    int i;

    if (cmds->req_count == 0) {
        return EAR_SUCCESS;
    }
    #if 1
    // Extreme debugging
    for (i = 0; i < cmds->req_count; ++i) {
        debug("MMIO%d: cpu %d, reg 0x%02X, value 0x%02X, read_write %d", i,
            cmds->io_reg[i].logical_cpu, cmds->io_reg[i].reg, cmds->io_reg[i].value, cmds->io_reg[i].read_write);
    }
    #endif 
    // Cleaning answers
    for(i = 0; answers != NULL && i < cmds->req_count; ++i) {
        answers[i] = 0;
    }
    // Sending command 
    if (ioctl(driver_fd, ISST_IF_MMIO_CMD, cmds) == -1) {
        debug("ioctl failed to send %d MMIO commands 0x%X: %d, %s",
            cmds->req_count, cmds->io_reg[0].reg, errno, strerror(errno));
        return_msg(EAR_ERROR, strerror(errno));
    }
    // Saving answers 
    for(i = 0; answers != NULL && i < cmds->req_count; ++i) {
        answers[i] = cmds->io_reg[i].value;
    }
    debug("Answer for the %d MMIO commands 0x%02X: 0x%02X",
        cmds->req_count, cmds->io_reg[0].reg, cmds->io_reg[0].value);

    return EAR_SUCCESS;
}

static int commands_mmio_fill(cmds_mmio_t *cmds, uint cpu, uint reg, uint value, int write, uint *answers, int send)
{
    cmds->io_reg[cmds->req_count].logical_cpu = cpu;
    cmds->io_reg[cmds->req_count].reg         = reg;
    cmds->io_reg[cmds->req_count].value       = value;
    cmds->io_reg[cmds->req_count].read_write  = write;

    if (++(cmds->req_count) == CMDS_MAX || send) {
        // Sending
        commands_mmio_send(cmds, answers);
        // Cleaning command array
        if (cmds->req_count == CMDS_MAX) {
            memset(cmds, 0, sizeof(cmds_mmio_t));
        }
        return 1;
    }
    return 0;
}

static state_t commands_mmio_send_single(uint cpu, uint reg, uint value, int write, uint *answer)
{
    cmds_mmio_t cmds = { 0 };
    commands_mmio_fill(&cmds, cpu, reg, value, write, NULL, 0); 
    return commands_mmio_send(&cmds, answer);
}

static state_t commands_mbox_send(cmds_mbox_t *cmds, uint *answers)
{
    uint retrys = 3;
    int i;  
 
    if (cmds->cmd_count == 0) {
        return EAR_SUCCESS;
    } 
    // Cleaning answers
    for(i = 0; answers != NULL && i < cmds->cmd_count; ++i) {
        answers[i] = 0;
    }
    // Sending command 
    do {
        if (ioctl(driver_fd, ISST_IF_MBOX_CMD, cmds) == -1) {
            debug("ioctl failed to send %d MBOX commands 0x%X_0x%X: %u retrys remaining (%d, %s)",
                cmds->cmd_count, cmds->mbox_cmd[0].command, cmds->mbox_cmd[0].sub_command, retrys, errno, strerror(errno));
            --retrys;
        } else {
            break;
        }
    } while (retrys);
    //
    if (!retrys) {
        return_msg(EAR_ERROR, strerror(errno));
    }
    // Saving answers 
    for(i = 0; answers != NULL && i < cmds->cmd_count; ++i) {
        answers[i] = cmds->mbox_cmd[i].resp_data;
    }
    debug("Answer for MBOX command 0x%X_0x%X: 0x%X",
        cmds->mbox_cmd[0].command, cmds->mbox_cmd[0].sub_command, cmds->mbox_cmd[0].resp_data);
    return EAR_SUCCESS;
}

static int commands_mbox_fill(cmds_mbox_t *cmds, uint cpu, uint cmd1, uint cmd2, uint param, uint req_data, uint *answers, int send)
{
    cmds->mbox_cmd[cmds->cmd_count].logical_cpu = cpu;
    cmds->mbox_cmd[cmds->cmd_count].command     = cmd1;
    cmds->mbox_cmd[cmds->cmd_count].sub_command = cmd2;
    cmds->mbox_cmd[cmds->cmd_count].parameter   = param;
    cmds->mbox_cmd[cmds->cmd_count].req_data    = req_data;

    if (++cmds->cmd_count == CMDS_MAX || send) {
        // Sending
        commands_mbox_send(cmds, answers);
        // Cleaning command array
        if (cmds->cmd_count == CMDS_MAX) {
            memset(&cmds, 0, sizeof(cmds_mbox_t));
        }
        return 1;
    }
    return 0;
}

static state_t commands_mbox_send_single(uint cpu, uint cmd1, uint cmd2, uint param, uint req_data, uint *answer)
{
    cmds_mbox_t cmds = { 0 };
    commands_mbox_fill(&cmds, cpu, cmd1, cmd2, param, req_data, NULL, 0);
    return commands_mbox_send(&cmds, answer);
}

/*
 *
 * Not commands
 *
 */

static void clos_print(clos_t *clos, int fd)
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

static state_t clos_get(clos_t *clos)
{
    cmds_mmio_t cmds = { 0 };
    uint answers[4];
    int c;

    for (c = 0; c < 4; ++c) {
        commands_mmio_fill(&cmds, 0, CP_GET_CLOS(c), answers, c == 3);
    }
    for (c = 0; c < 4; ++c) {
        clos[c].idx     = c;
        clos[c].low     = c > 1;
        clos[c].high    = c < 2;
        clos[c].max_khz = ((answers[c] >> 16) & 0xff) * 1000;
        clos[c].min_khz = ((answers[c] >>  8) & 0xff) * 1000;
    }
    #if SHOW_DEBUGS
    clos_print(clos, verb_channel);
    #endif
    return EAR_SUCCESS; 
}

static state_t assoc_get(uint *clos)
{
    cmds_mmio_t cmds = { 0 };
    int cpu, last;

    for (cpu = last = 0; cpu < cpus_count; ++cpu) {
        if (commands_mmio_fill(&cmds, cpu, CP_GET_ASSOC(punit_cpus[cpu]), &clos[last], cpu == (cpus_count - 1))) {
            last = cpu + 1;
        }
    }
    for (cpu = 0; cpu < cpus_count; ++cpu) {
        clos[cpu] = (clos[cpu] >> 16) & 0x03;
    }
    #if SHOW_DEBUGS
    for (cpu = 0; cpu < cpus_count; ++cpu) {
        if ((cpu != 0) && (cpu % 8) == 0) dprintf(debug_channel, "\n");
        dprintf(debug_channel, "[%03d,%d] ", cpu, clos[cpu]);
    }
    dprintf(debug_channel, "\n");
    #endif

    return EAR_SUCCESS;
}

static state_t status_get(envals_t *en)
{
    ullong ans_tr;
    uint ans_pm;
    uint ans_cp;
    uint ans_tf;
    state_t s;
    uint i;

    // 1 Socket is enough
    for (i = 0; i < 1; ++i) {
        // Enabling PM (¿What is PM?)
        if (state_fail(s = commands_mbox_send_single(tp.cpus[i].id, PM_GET_CONFIG, &ans_pm))) {
            return s;
        }
        // Get current core-power status (prio 0: proportional, prio 1: ordered)
        if (state_fail(s = commands_mbox_send_single(tp.cpus[i].id, CP_GET_CONFIG, &ans_cp))) {
            return s;
        }
        // Get current turbo-freq status
        if (state_fail(s = commands_mbox_send_single(tp.cpus[i].id, TF_GET_CONFIG, &ans_tf))) {
            return s;
        }
        // Get current turbo-ratio-limit status
        if (state_fail(s = commands_mmsr_send_single(tp.cpus[i].id, TR_GET_CONFIG, &ans_tr))) {
            return s;
        }
 
        en->cp_support = getbits32(ans_cp,  0,  0);
        en->tf_support = getbits32(ans_tf,  0,  0);
        en->bf_support = getbits32(ans_tf,  1,  1);
        en->cp_enabled = getbits32(ans_cp,  1,  1);
        en->tf_enabled = getbits32(ans_tf, 16, 16);
        en->bf_enabled = getbits32(ans_tf, 17, 17);
        // Printing details
        debug("CP support : %d", en->cp_support);
        debug("TF support : %d", en->tf_support);
        debug("BF support : %d", en->bf_support);;
        debug("CP enabled : %d (%s)", en->cp_enabled, strtype[en->cp_enabled]);
        debug("TF enabled : %d", en->tf_enabled);
        debug("BF enabled : %d", en->cp_enabled);
        // Failing
        if (!en->cp_support || !en->tf_support) {
            return_msg(EAR_ERROR, "CP/TF not supported");
        }
    }
    return EAR_SUCCESS;
}

static state_t cpus_map()
{
    struct isst_if_cpu_maps map;
    int i, j;

    for (i = 0; i < cpus_count; i += CMDS_MAX) {
        map.cmd_count = CMDS_SPLIT(i, cpus_count);
        for (j = 0; j < map.cmd_count; ++j) {
            map.cpu_map[j].logical_cpu = i+j;
        }
        debug("Mapping [%3d - %3d] (#%d)", i, i+CMDS_MAX, map.cmd_count);
        if (ioctl(driver_fd, ISST_IF_PHYS_CMD, &map) == -1) {
            debug("ioctl failed in MAP CMD: %s", strerror(errno));
            return_msg(EAR_ERROR, strerror(errno));
        }
        for (j = 0; j < map.cmd_count; ++j) {
            punit_cpus[i+j] = (map.cpu_map[j].physical_cpu >> 1);
        }
    }
    #if SHOW_DEBUGS
    for (i = 0; i < cpus_count; ++i) {
        if ((i != 0) && (i % 8) == 0) dprintf(debug_channel, "\n");
        dprintf(debug_channel, "[%03d,%03d] ", i, punit_cpus[i]);
    }
    dprintf(debug_channel, "\n");
    #endif
    return EAR_SUCCESS;
}

static state_t init()
{
    struct isst_if_platform_info info = { 0 };
    state_t s;

    if (ioctl(driver_fd, ISST_IF_INFO_CMD, &info) == -1) {
        debug("ioctl failed: %s", strerror(errno));
        return 0;
    }
    debug("API version   : %d", info.api_version);
    debug("Driver version: %d", info.driver_version);
    debug("MBOX supported: %d", info.mbox_supported);
    debug("MMIO supported: %d", info.mmio_supported);

    if (!info.mbox_supported || !info.mmio_supported) {
        return_msg(EAR_ERROR, "MBOX/MMIO not supported");
    }
    // Mapping CPUs
    if (state_fail(s = cpus_map())) {
        return s;
    }
    // Saving initial status
    if (state_fail(s = status_get(&invals.envals))) {
        return s;
    }
    // Saving initial CLOS
    clos_get(invals.clos);
    uint assocs[1024];
    assoc_get(assocs);

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
    return init();
}

int main(int argc, char *argv[])
{
    topology_t tp;

    topology_init(&tp);

    if (state_fail(sst_init(&tp))) {
        serror("while initializing SST");
    }
    
    return 0;
}
