#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/ipmi.h>
//#include <common/output/debug.h>

#define SHOW_DEBUGS 1

#define IPMI_MAX_DEVS 4

typedef struct ipmi_msg ipmi_msg_t;
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;

typedef struct ipmi_dev_s {
    int fd;
    char path[32];
    uchar id;
    int is_sdr;
    int is_fru;
    int is_chassis;
    int is_bridge;
    int is_sensors;
    uchar *sdr_raw;
    uchar *fru_raw;
} ipmi_dev_t;

typedef struct ipmi_devs_s {
    ipmi_dev_t devs[IPMI_MAX_DEVS];
    int count;
} ipmi_devs_t;

#define debug(...) { \
    printf(__VA_ARGS__); \
    printf("\n"); \
}

static ipmi_devs_t devs;

uint getbits32(uint reg, uint left_bit, uint right_bit)
{
    uint digits = left_bit - right_bit;
    uint mask   = (((0x01 << digits) - 1) << 1) + 1;
    return ((reg >> right_bit) & mask);
}

ipmi_msg_t *ipmi_msg(uchar netfn, uchar cmd, ushort data_len)
{
    static ipmi_msg_t msg;
    static uchar data[512];
    memset(data, 0, 512);
    msg.netfn    = netfn;
    msg.cmd      = cmd;
    msg.data     = (data_len > 0) ? data : NULL;
    msg.data_len = data_len;
    return &msg;
}

static void debug_msg(ipmi_msg_t *msg)
{
    char buffer_debug[256*3];
    int i, c;

    c = sprintf(buffer_debug, "    ");
    for (i = 0; i < msg->data_len; i++) {
        if (i > 0 && (i % 32) == 0) c += sprintf(&buffer_debug[c], "\n    ");
        c += sprintf(&buffer_debug[c], "%02x ", (int) msg->data[i]);
    }
    debug("%s", buffer_debug);
}

ipmi_msg_t *ipmi_driver_send_command(int fd, ipmi_msg_t *msg_send, uchar lun)
{
    static struct ipmi_system_interface_addr addr;
    static struct ipmi_recv recv;
    static struct ipmi_req reqs;
    static uchar data[256];

    memset(&addr, 0, sizeof(addr));
    memset(&recv, 0, sizeof(recv));
    memset(&reqs, 0, sizeof(reqs));
    memset( data, 0, sizeof(data));
    // Addresses:
    // - IPMI_SYSTEM_INTERFACE_ADDR_TYPE: local BMC address. The driver uses
    //   KCS, BT or SMIC underlying.
    // - IPMI_IPMB_ADDR_TYPE: to communicate to a device not directly connected
    //   to the main BMC, but connected by an i2c IMPB bus to the main BMC like
    //   a satellite controller (other BMC).
    // Channels:
    //   - Are interfaces to group sets of devices (0-15, 4 bits).
    //   - There is a command to retrieve information of a channel.
    //   - Channels are predefined: 0 to communicate with KCS, BT or SMIC
    //      devices, 1 IPMB, 2-3 LAN and 4-15 hardware custom.
    // LUN (logical unit number):
    //   - Is used to identify logic subcomponents within a physical device.
    //     That allows multi-function devices to ask different messages
    //     depending on LUN.
    //   - Is a number of 2 bits, allowing 4 subcomponents (0-3).
    //   - In general LUN 0 is the default for most devices, and 1-3 are
    //     specialized.
    // NetFn:
    //   - Is a number to categorize commands (0-63, 6 bits)
    //   - 0 least significant bit means request, 1 is response.
    //   - Pre-definitions:
    //       - 0x00: chassis commands like turn on/off, reset...
    //       - 0x02: bridge between buses.
    //       - 0x04: sensors and events, like get sensor reading...
    //       - 0x06: general BMC.
    //       - 0x08: firmware.
    //       - 0x0a: storage like FRU and SDR.
    //       - 0x0c: transport like LAN.
    //       - 0x30-03F: OEM (Original Equipment Manufacturer)
    //     We are particularly interested in 0x04, 0x06 and 0x0a.
    addr.addr_type    = IPMI_SYSTEM_INTERFACE_ADDR_TYPE;
    addr.channel      = IPMI_BMC_CHANNEL;
    addr.lun          = lun;
    reqs.addr         = (unsigned char *) &addr;
    reqs.addr_len     = sizeof(addr);
    reqs.msgid        = 0x1234;
    reqs.msg.netfn    = msg_send->netfn;
    reqs.msg.cmd      = msg_send->cmd;
    reqs.msg.data     = msg_send->data;
    reqs.msg.data_len = msg_send->data_len;

    #if 0
    debug("FD%d Sending %u bytes from IPMI [ADDR 0x%X, CHAN 0x%X, LUN 0x%X, NETFN 0x%X, CMD 0x%x]:", fd,
          recv.msg.data_len, addr.addr_type, addr.channel, lun, reqs.msg.netfn, reqs.msg.cmd);
    debug_msg(&reqs.msg);
    #endif

    if (ioctl(fd, IPMICTL_SEND_COMMAND, &reqs) < 0) {
        printf("IPMI command ioctl error: %s\n", strerror(errno));
        return NULL;
    }
    recv.addr         = (unsigned char *) &addr;
    recv.addr_len     = sizeof(addr);
    recv.msg.data     = data;
    recv.msg.data_len = sizeof(data);
#if 0
    // poll
    if (ioctl(fd, IPMICTL_RECEIVE_MSG_TRUNC, &recv) < 0) {
        debug("IPMI receive message ioctl error: %s", strerror(errno));
        return NULL;
    }
#else
    while (ioctl(fd, IPMICTL_RECEIVE_MSG_TRUNC, &recv) < 0) {
        if (errno != EAGAIN) {
            debug("IPMI receive message ioctl error: %s", strerror(errno));
        }
        usleep(100000);
    }
#endif
    #if SHOW_DEBUGS && 0
    debug("FD%d Read %u bytes from IPMI [ ADDR 0x%X, CHAN 0x%X, LUN 0x%X, NETFN 0x%X, CMD 0x%x ]:", fd,
          recv.msg.data_len, addr.addr_type, addr.channel, lun, reqs.msg.netfn, reqs.msg.cmd);
    debug_msg(&recv.msg);
    #endif
    return &recv.msg;
}

int ipmi_driver_open(ipmi_devs_t *d)
{
    ipmi_msg_t msg;
    int i;
    // Cleaning
    memset(d->devs, 0, sizeof(ipmi_dev_t)*IPMI_MAX_DEVS);
    d->count = 0;
    // Opening devices
    // Check if the BMCs answer
    ipmi_msg_t *msg_send = ipmi_msg(IPMI_NETFN_APP_REQUEST, IPMI_GET_DEVICE_ID_CMD, 0);
    ipmi_msg_t *msg_recv = NULL;

    for (i = 0; i < IPMI_MAX_DEVS; ++i) {
        d->devs[d->count].fd = -1;
        sprintf(d->devs[d->count].path, "/dev/ipmi%d", i);
        if ((d->devs[d->count].fd = open(d->devs[d->count].path, O_RDWR)) < 0) {
            sprintf(d->devs[d->count].path, "/dev/ipmi/%d", i);
            if ((d->devs[d->count].fd = open(d->devs[d->count].path, O_RDWR)) < 0) {
                sprintf(d->devs[d->count].path, "/dev/ipmidev%d", i);
                if ((d->devs[d->count].fd = open(d->devs[d->count].path, O_RDWR)) < 0) {
                }}} // Ifs
        if (d->devs[d->count].fd < 0) {
            if (!d->count) {
                return 0;
            }
            break;
        }
        debug("FD%d Opened '%s' file", d->devs[d->count].fd, d->devs[d->count].path);
        // Testing IPMI's BMCs
        if (!(msg_recv = ipmi_driver_send_command(d->devs[d->count].fd, msg_send, 0))) {
            close(d->devs[d->count].fd);
            d->devs[d->count].fd      = -1;
            d->devs[d->count].path[0] = '\0';
            continue;
        }
        // Getting BMC data
        d->devs[d->count].id         = msg_recv->data[1];
        d->devs[d->count].is_sdr     = getbits32((uint) msg_recv->data[2], 7, 7);
        d->devs[d->count].is_fru     = getbits32((uint) msg_recv->data[6], 3, 3);
        d->devs[d->count].is_bridge  = getbits32((uint) msg_recv->data[6], 6, 6);
        d->devs[d->count].is_chassis = getbits32((uint) msg_recv->data[6], 7, 7);
        d->devs[d->count].is_sensors = getbits32((uint) msg_recv->data[6], 0, 0);
        debug("FD%d Detected device 0x%X [ %s%s%s%s%s]", d->devs[d->count].fd, d->devs[d->count].id,
              (d->devs[d->count].is_sdr    ) ? "SDR "    : "",
              (d->devs[d->count].is_fru    ) ? "FRU "    : "",
              (d->devs[d->count].is_chassis) ? "Chassis ": "",
              (d->devs[d->count].is_sensors) ? "Sensors ": "",
              (d->devs[d->count].is_bridge ) ? "Bridge " : ""
        );
        ++d->count;
    }
    return d->count;
}

int ipmi_driver_discover_frus(ipmi_devs_t *d)
{
    int i;
    int f;
    // SDR (Sensor Data Records)
    //   - A database stored in the BMC that has detailed information about all
    //     sensors in the system: type (temp, volt, fan...), place (cpu, power
    //     supply...), units, values rank, access method (IPMB, KCS...).
    // FRU inventory (Field Replaceable Unit)
    //   - A database stored in an EEPROM that has information about components
    //     attached to the system like cards, modules, power supplies... It
    //     includes manufacturer, serial and part numbers, name, OEM info...
    //   - There are 255 possible FRUs (0-254) per LUN (0-3)
    ipmi_msg_t *msg_send = ipmi_msg(IPMI_NETFN_STORAGE_REQUEST, 0x10, 1);
    ipmi_msg_t *msg_recv = NULL;

    for (i = 0; i < d->count; ++i) {
        for (f = 0; f < 0xff; ++f) {
            msg_send->data[0] = f; // FRU id
            if (!(msg_recv = ipmi_driver_send_command(d->devs[i].fd, msg_send, 0))) {
            }
            // Completion codes:
            //   0x00 : OK
            //   0xc7 : Request data length invalid.
            //   0xca : Cannot return number of requested data bytes.
            //   0xcb : Requested Sensor, data, or record not present.
            if (msg_recv->data[0] != 0x00) {
                break;
            }
        }
    }
    return 0;
}

char *sdr_type(uchar type)
{
    switch(type) {
        case 0x01: return "FULL";
        case 0x02: return "COMPACT";
        case 0x03: return "EVENT-BASED";
        case 0x08: return "ENTITY-ASSOC";
        case 0x09: return "DEV-RELATIVE";
        case 0x10: return "DEV-LOCATOR";
        case 0x11: return "FRU-LOCATOR";
        case 0x12: return "BMC-LOCATOR";
        case 0x13: return "MCC-RECORD";
        case 0x14: return "MSG-CHANNEL";
    }
    if (type >= 0x0a && type <= 0x0f) return "RESERVED";
    if (type >= 0xc0) return "OEM"; // Original Equipment Manufacturer
    return "?";
}

int ipmi_driver_discover_sdr(ipmi_devs_t *d)
{
    ushort record_id = 0x0000;
    // - The SDR reservation allows the SDR doesn't change while reading
    // registers. But we are going to assume it won't happen.
    ipmi_msg_t *msg_send = ipmi_msg(IPMI_NETFN_STORAGE_REQUEST, 0x23, 6);
    ipmi_msg_t *msg_recv = NULL;

    while (record_id != 0xFFFF)
    {
        msg_send->data[2] = (record_id     ) & 0xFF;
        msg_send->data[3] = (record_id >> 8) & 0xFF;
        msg_send->data[4] = 0x00; // Offset
        msg_send->data[5] = 0xFF; // Length
        if (!(msg_recv = ipmi_driver_send_command(d->devs[0].fd, msg_send, 0))) {
            break;
        }
        // Response:
        //    0: Completion Code
        //    1: Record ID for next record, LS Byte
        //    2: Record ID for next record, MS Byte
        //    3: Record Data (the 5 first bytes are the header):
        //          0-1: Record ID
        //          2  : SDR Version (0x51 is IPMI 2.0)
        //          3  : Record Type (0x1 full, 0x2 compact, 0x8 FRU, 0x11 other BMC...)
        //          4  : Record Length
        //          5-x: Device information.
        if (msg_recv->data[0] != 0x00) {
            break;
        }
        int slave = msg_recv->data[3+5] & 0x01;
        debug("FD%d Detected Sensor Data Record %02d [ %s %s ]", d->devs[0].fd, record_id,
              sdr_type(msg_recv->data[3+3]), (slave) ? "SLAVE": "");
        record_id = msg_recv->data[1] | (msg_recv->data[2] << 8);  // Obtener el siguiente ID
    }
    return 0;
}

int main(int argc, char *argv)
{
    if (!ipmi_driver_open(&devs)) {
        return 0;
    }
    //ipmi_driver_discover_frus(&devs);
    ipmi_driver_discover_sdr(&devs);
    return 0;
}