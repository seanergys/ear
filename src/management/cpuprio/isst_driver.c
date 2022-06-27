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

#include <common/output/debug.h>
#include <management/cpuprio/isst_if.h>

const char *driver_path = "/dev/isst_interface";
const int   driver_fd   = -1;

// Take a look to isst_send_mbox_command in isst-config.c
static void send_mbox_command()
{

}

//
static void send_mmio_command(uint cpu, uint cmd1, uint cmd2, uint data, uint req_ans)
{
    struct isst_if_mbox_cmds cmds = { 0 };
    uint retrys = 3;
    uint answer;

    // Command fill
    cmds.cmd_count               = 1;
    cmds.mbox_cmd[0].logical_cpu = cpu;
    cmds.mbox_cmd[0].command     = cmd1;
    cmds.mbox_cmd[0].sub_command = cmd2;
    cmds.mbox_cmd[0].parameter   = data;
    cmds.mbox_cmd[0].req_data    = req_ans;
    // Do and retry
    do {
        if (ioctl(fd, ISST_IF_MBOX_COMMAND, &cmds) == -1) {
            debug("ioctl failed: %u retrys remaining", retrys)
            --retrys;
        } else {
            answer = cmds.mbox_cmd[0].resp_data;
            break;
        }
    } while (retrys);

    debug("Answer: %u", answer);
}

#define CMD1_CONFIG          0xD0 //CONFIG_CLOS
#define CMD2_CLOS_GET_ASSOC  0x00 //CLOS_PM_QOS_CONFIG

void isst_init()
{
    if ((driver_fd = open(driver_path, O_RDWR)) < 0) {
        return;
    }

    send_mbox_command(0, CMD1_CONFIG, CMD2_CLOS_GET_ASSOC, 0, 0);

    close(driver_fd);
}

int main(int argc, char *argv[])
{
    isst_init();
    return 0;
}