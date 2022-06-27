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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <common/output/debug.h>
#include <common/hardware/bithack.h>
#include <management/cpuprio/isst_if.h>
#include <management/cpuprio/isst_driver.h>

#define CMD_CP_GET_INFO   0    // CLOS_PM_QOS_CONFIG (MBOX)
#define CMD_CP_GET_ASSOC  1    // CLOS_PQR_ASSOC & PQR_ASSOC_OFFSET (MMIO)
#define CMD_CP_SET_ASSOC  2    // CLOS_PQR_ASSOC & PQR_ASSOC_OFFSET (MMIO)
#define CMD_CP_GET_CLOS   3    // CLOS_PM_CLOS & PM_CLOS_OFFSET (MMIO)
#define CMD_CP_SET_CLOS   4    // CLOS_PM_CLOS & PM_CLOS_OFFSET (MMIO)

static char *cp_str_enabled[] = { "disabled", "enabled" };
static char *cp_str_type[]    = { "proportional", "ordered" };
static char *driver_path = "/dev/isst_interface";
static int   driver_fd   = -1;
static uint  cmds[]      = { 0x0D002, 0x0D020, 0x1D020, 0x0D008, 0x1D008 };
static uint *driver_cpus = NULL;
static uint  cpus_count  = 0;

// Take a look to isst_send_mbox_command in isst-config.c
static int send_mmio_command(uint cpu, uint c, uint data, uint offset, uint *answer)
{
    struct isst_if_io_regs regs;
    int status;

    *answer = 0;
    //
    regs.req_count             = 1;
    regs.io_reg[0].logical_cpu = cpu;
    regs.io_reg[0].reg         = getbits32(cmds[c],  7,  0) + offset * 4;
    regs.io_reg[0].read_write  = getbits32(cmds[c], 16, 16);
    regs.io_reg[0].value       = data;

    if ((status = ioctl(driver_fd, ISST_IF_IO_CMD, &regs)) < 0) {
        if (errno == ENOTTY) {
            debug("isst_if_mmio file not found");
        } else {
            debug("ioctl failed (%d, %s)", status, strerror(errno));
        }
        return 0;
    } else {
        if (!regs.io_reg[0].read_write) {
            *answer = regs.io_reg[0].value;
        }
        //debug("ioctl answered: %u", *answer);
    } 

    return 1;
}

//
static int send_mbox_command(uint cpu, uint c, uint data, uint req_answer, uint *answer)
{
    struct isst_if_mbox_cmds cmd = { 0 };
    uint retrys = 3;
    int status;

    *answer = 0;
    // Command fill
    cmd.cmd_count               = 1;
    cmd.mbox_cmd[0].logical_cpu = cpu;
    cmd.mbox_cmd[0].command     = getbits32(cmds[c], 15, 8);
    cmd.mbox_cmd[0].sub_command = getbits32(cmds[c],  7, 0);
    cmd.mbox_cmd[0].parameter   = data;
    cmd.mbox_cmd[0].req_data    = req_answer;
    // Do and retry
    do {
        if ((status = ioctl(driver_fd, ISST_IF_MBOX_COMMAND, &cmd)) == -1) {
            debug("ioctl failed: %u retrys remaining (%d, %s)", retrys, status, strerror(errno))
            --retrys;
        } else {
            *answer = cmd.mbox_cmd[0].resp_data;
            //debug("ioctl answered: %u", *answer);
            break;
        }
    } while (retrys);

    if (!retrys) {
        debug("unable to perform ioctl");
        return 0;
    }
    return 1;
}

static int corepower_get_info()
{
    uint answer;

    if (!send_mbox_command(0, CMD_CP_GET_INFO, 0, 0, &answer)) {
        return 0;
    }
    int enabled = getbits32(answer, 1, 1); 
    int type    = getbits32(answer, 2, 2); 
    debug("corepower_get_info: cpu %d, %s, type %s", 0, cp_str_enabled[enabled], cp_str_type[type]);
    return 1;
}

static int corepower_get_assoc()
{
    uint clos;
    int cpu;

    for (cpu = 0; cpu < cpus_count; ++cpu) {
        uint offset = driver_cpus[cpu] & 0xFF; 
        if (!send_mmio_command(cpu, CMD_CP_GET_ASSOC, 0, offset, &clos)) {
            return 0;
        }
        clos = (clos >> 16) & 0x03;
        debug("CPU%d: CLOS%u", cpu, clos);
    }
    
    return 1;
}

static int send_phys_command(uint cpu, uint *answer)
{
    struct isst_if_cpu_maps map;
    int status;

    map.cmd_count              = 1;
    map.cpu_map[0].logical_cpu = cpu;

    if ((status = ioctl(driver_fd, ISST_IF_GET_PHY_ID, &map)) < 0) {
        debug("ioctl failed (%d, %s)", status, strerror(errno));
        return 0;
    }
    *answer = map.cpu_map[0].physical_cpu;
    return 1;
}

static int driver_get_cpus()
{
    int cpu;

    driver_cpus = calloc(cpus_count, sizeof(uint));
    //
    for (cpu = 0; cpu < cpus_count; ++cpu) {
        if (!send_phys_command(cpu, &driver_cpus[cpu])) {
            return 0;
        }
        debug("mapped CPU%d: %d", cpu, driver_cpus[cpu]); 
        driver_cpus[cpu] = (driver_cpus[cpu] >> 1);
    }
    return 1;
}

void isst_load(topology_t *tp)
{
    if (tp->cpu_count > 0) {
        cpus_count = tp->cpu_count;
    }
    if ((driver_fd = open(driver_path, O_RDWR)) < 0) {
        debug("Failed to open %s (%s)", driver_path, strerror(errno));
        return;
    }
    debug("Opened %s (fd: %d)", driver_path, driver_fd);

    if (!driver_get_cpus()) {
        return; 
    } 
    if (!corepower_get_info()) {
        return;
    }
    if (!corepower_get_assoc()) {
        return;
    }

    close(driver_fd);

    // Set CPUPRIO functions
}

int main(int argc, char *argv[])
{
    topology_t tp;
    topology_init(&tp);
    isst_load(&tp);
    return 0;
}
