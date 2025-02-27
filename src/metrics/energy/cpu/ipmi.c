#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/ipmi.h>
//#include <common/output/debug.h>

#define SHOW_DEBUGS 1

#define IPMI_MAX_DEVS 4
#define IPMI_SDR_RECORD_FULL 0x01

typedef struct ipmi_msg ipmi_msg_t;
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;

typedef struct ipmi_addr_s {
    struct ipmi_addr addr;
    uint addr_len;
} ipmi_addr_t;

typedef struct sdr_s {
    ushort id;
    uchar type;
    char  name[16];
    uchar addr; // Or i2c
    uchar addr_ipmb;
    uchar addr_lun;
    uchar addr_channel;
    uchar addr_bus;
    uchar entity_id;
    uchar entity_instance;
    uchar entity_logical;
    uchar sensor_no;
    uchar sensor_type;
    uchar reading_type;
    uchar reading_complement;
    uchar reading_rate;
    uchar reading_modifier;
    uchar reading_percentage;
    uchar reading_units;
    uchar reading_modifier_units;
    uchar reading_linearization;
    uchar reading_tolerance;
    uchar reading_accuracy;
    uchar reading_accuracy_exp;
    int   reading_m; // 10 bits, 2-complement
    int   reading_b; // 10 bits, 2-complement
    int   reading_r_exp; // 4 bits, 2-complement
    int   reading_b_exp; // 4 bits, 2-complement
    uchar reading_raw; // y = Mx+B
    uchar reading_raw_max; // y = Mx+B
    uchar reading_raw_min; // y = Mx+B
} sdr_t;

typedef struct ipmi_dev_s {
    int fd;
    char path[32];
    uchar id;
    uchar is_sdr;
    uchar is_fru;
    uchar is_chassis;
    uchar is_bridge;
    uchar is_sensors;
    sdr_t *records;
    uint records_count;
} ipmi_dev_t;

typedef struct ipmi_devs_s {
    ipmi_dev_t devs[IPMI_MAX_DEVS];
    int count;
} ipmi_devs_t;

const char *sensor_type_str[] = {
        "reserved", "Temperature", "Voltage", "Current", "Fan",
        "Physical Security", "Platform Security", "Processor",
        "Power Supply", "Power Unit", "Cooling Device", "Other",
        "Memory", "Drive Slot / Bay", "POST Memory Resize",
        "System Firmwares", "Event Logging Disabled", "Watchdog1",
        "System Event", "Critical Interrupt", "Button", "Module / Board",
        "Microcontroller", "Add-in Card", "Chassis", "Chip Set",
        "Other FRU", "Cable / Interconnect", "Terminator",
        "System Boot Initiated", "Boot Error", "OS Boot",
        "OS Critical Stop", "Slot / Connector", "System ACPI Power State",
        "Watchdog2", "Platform Alert", "Entity Presence", "Monitor ASIC",
        "LAN", "Management Subsys Health", "Battery", "Session Audit",
        "Version Change", "FRU State", NULL
};

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

uchar getbits8(uchar reg, uchar left_bit, uchar right_bit)
{
    uchar digits = left_bit - right_bit;
    uchar mask   = (((0x01 << digits) - 1) << 1) + 1;
    return ((reg >> right_bit) & mask);
}

static ipmi_addr_t *ipmi_addr(int type, short channel, uchar slave_addr, uchar lun)
{
    static ipmi_addr_t addr;
    memset(&addr, 0, sizeof(ipmi_addr_t));
    addr.addr.addr_type = type;
    addr.addr.channel   = channel;
    if (type == IPMI_SYSTEM_INTERFACE_ADDR_TYPE) {
        ((struct ipmi_system_interface_addr *) &addr.addr)->lun = lun;
        addr.addr_len = sizeof(struct ipmi_system_interface_addr);
    } else if (type == IPMI_IPMB_ADDR_TYPE) {
        ((struct ipmi_ipmb_addr *) &addr.addr)->slave_addr = slave_addr;
        ((struct ipmi_ipmb_addr *) &addr.addr)->lun = lun;
        addr.addr_len = sizeof(struct ipmi_ipmb_addr);
    }
    return &addr;
}

static ipmi_addr_t *addr_def()
{
    return ipmi_addr(IPMI_SYSTEM_INTERFACE_ADDR_TYPE, IPMI_BMC_CHANNEL, 0, 0);
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

ipmi_msg_t *ipmi_driver_send_command(int fd, ipmi_msg_t *msg_send, ipmi_addr_t *addr)
{
    static struct ipmi_recv recv;
    static struct ipmi_req reqs;
    static uchar data[256];
    int tries = 3;

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
    //       - 0x06: general BMC (aka APP).
    //       - 0x08: firmware.
    //       - 0x0a: storage like FRU and SDR.
    //       - 0x0c: transport like LAN.
    //       - 0x30-03F: OEM (Original Equipment Manufacturer)
    //     We are particularly interested in 0x04, 0x06 and 0x0a.
    reqs.addr         = (unsigned char *) &addr->addr;
    reqs.addr_len     = addr->addr_len;
    reqs.msgid        = 0x1234;
    reqs.msg.netfn    = msg_send->netfn;
    reqs.msg.cmd      = msg_send->cmd;
    reqs.msg.data     = msg_send->data;
    reqs.msg.data_len = msg_send->data_len;

    #if 0
    debug("FD%d Sending %u bytes from IPMI [ADDR 0x%X, CHAN 0x%X, NETFN 0x%X, CMD 0x%x]:", fd,
          reqs.msg.data_len, addr->addr.addr_type, addr->addr.channel, reqs.msg.netfn, reqs.msg.cmd);
    debug_msg(&reqs.msg);
    #endif

    if (ioctl(fd, IPMICTL_SEND_COMMAND, &reqs) < 0) {
        debug("IPMI command ioctl error: %s", strerror(errno));
        return NULL;
    }
    recv.addr         = (unsigned char *) &addr->addr;
    recv.addr_len     = addr->addr_len;
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
        // Tiemout control
        if (tries-- == 0) {
            return NULL;
        }
        usleep(100000);
    }
#endif
    #if SHOW_DEBUGS && 0
    debug("FD%d Read %u bytes from IPMI [ ADDR TYPE 0x%X, CHAN 0x%X, NETFN 0x%X, CMD 0x%x ]:", fd,
          recv.msg.data_len, addr->addr.addr_type, addr->addr.channel, reqs.msg.netfn, reqs.msg.cmd);
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
    ipmi_msg_t  *msg_send = ipmi_msg(IPMI_NETFN_APP_REQUEST, IPMI_GET_DEVICE_ID_CMD, 0);
    ipmi_msg_t  *msg_recv = NULL;

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
        if (!(msg_recv = ipmi_driver_send_command(d->devs[d->count].fd, msg_send, addr_def()))) {
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
        debug("FD%d Detected BMC [ ID0x%X %s%s%s%s%s]", d->devs[d->count].fd, d->devs[d->count].id,
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

// TODO: Non-linearization
// TODO: Apply L(x) function
// TODO: 1's complement
// TODO: Apply tolerance
double sensor_value(sdr_t *record, uchar reading_raw)
{
    // Linearization formula:
    //   y = L((Mx + (B * 10^K1)) * 10^K2) units
    double m = (double) record->reading_m;
    double b = (double) record->reading_b;
    double x = (double) reading_raw;
    double y = 0.0;
    // Operations
    b = b * pow(10.0, record->reading_b_exp);
    y = (m * x) + b;
    y = y * pow(10.0, record->reading_r_exp);
    return y;
}

double ipmi_driver_sensor_read(void *sensor)
{
    sdr_t       *record   = (sdr_t *) sensor;
    ipmi_msg_t  *msg_send = ipmi_msg(IPMI_NETFN_SENSOR_EVENT_REQUEST, 0x2d, 1);
    ipmi_msg_t  *msg_recv = NULL;

    msg_send->data[0] = record->sensor_no;
    debug("asking sensor %u", record->sensor_no);
    if (!(msg_recv = ipmi_driver_send_command(devs.devs[0].fd, msg_send, addr_def()))) {
        perror("Error");
        return 0.0;
    }
    debug("VALUE %02.2lf", sensor_value(record, msg_recv->data[1]));
}

char *sdr_parse(ipmi_msg_t *msg_recv, sdr_t *record)
{
    #define B(n) 3+n-1
    static char buffer[256];

    record->type = msg_recv->data[B(4)];

    switch(record->type) {
        case 0x01:
            record->addr                   = getbits8(msg_recv->data[B(6)], 7, 1);
            record->addr_ipmb              = getbits8(msg_recv->data[B(6)], 0, 0);
            record->addr_channel           = getbits8(msg_recv->data[B(7)], 7, 4);
            record->addr_lun               = getbits8(msg_recv->data[B(7)], 1, 0);
            record->entity_id              = msg_recv->data[B(9)];
            record->entity_instance        = getbits8(msg_recv->data[B(10)], 6, 0);
            record->entity_logical         = getbits8(msg_recv->data[B(10)], 7, 7);
            record->sensor_no              = msg_recv->data[B(8)];
            record->sensor_type            = msg_recv->data[B(13)];
            record->reading_type           = msg_recv->data[B(13)];
            record->reading_complement     = getbits8(msg_recv->data[B(21)], 7, 6);
            record->reading_rate           = getbits8(msg_recv->data[B(21)], 5, 3);
            record->reading_modifier       = getbits8(msg_recv->data[B(21)], 2, 1);
            record->reading_percentage     = getbits8(msg_recv->data[B(21)], 0, 0);
            record->reading_units          = msg_recv->data[B(22)];
            record->reading_modifier_units = msg_recv->data[B(23)];
            record->reading_linearization  = getbits8(msg_recv->data[B(24)], 6, 0);
            record->reading_tolerance      = getbits8(msg_recv->data[B(26)], 5, 0);
            record->reading_accuracy       = getbits8(msg_recv->data[B(29)], 7, 4) << 8 | getbits8(msg_recv->data[B(28)], 5, 0);
            record->reading_accuracy_exp   = getbits8(msg_recv->data[B(29)], 3, 2);
            record->reading_m              = getbits8(msg_recv->data[B(26)], 7, 6) << 8 | msg_recv->data[B(25)];
            record->reading_b              = getbits8(msg_recv->data[B(28)], 7, 6) << 8 | msg_recv->data[B(27)];
            record->reading_m              = getbits32(record->reading_m, 9, 9) ? 0xfffffc00 | record->reading_m : record->reading_m;
            record->reading_b              = getbits32(record->reading_b, 9, 9) ? 0xfffffc00 | record->reading_b : record->reading_b;
            record->reading_r_exp          = getbits8(msg_recv->data[B(30)], 7, 4);
            record->reading_b_exp          = getbits8(msg_recv->data[B(30)], 3, 0);
            record->reading_r_exp          = getbits32(record->reading_r_exp, 3, 3) ? 0xfffffff0 | record->reading_r_exp : record->reading_r_exp;
            record->reading_b_exp          = getbits32(record->reading_b_exp, 3, 3) ? 0xfffffff0 | record->reading_b_exp : record->reading_b_exp;
            record->reading_raw            = msg_recv->data[B(32)];
            record->reading_raw_max        = msg_recv->data[B(33)];
            record->reading_raw_min        = msg_recv->data[B(34)];
            strncpy(record->name, (const char *) &msg_recv->data[B(49)], getbits8(msg_recv->data[B(48)], 4, 0));
            sprintf(buffer, "FULL SEN_0x%02X ENT_0x%02X/%02X ADDR_0x%X/%X CHAN_0x%X LUN_0x%X %s %s "
                            "%x (%uca2 %ur %um %upcnt) (%uru %umu) %ulin (%utol %uacc %uacce) (%dm %db) (%dre %dbe) (%u %u %u) %02.2lf",
                record->sensor_no, record->entity_id, record->entity_instance,
                record->addr, record->addr_ipmb, record->addr_channel, record->addr_lun,
                record->name, sensor_type_str[record->sensor_type],
                record->reading_type,
                record->reading_complement, record->reading_rate, record->reading_modifier, record->reading_percentage,
                record->reading_units, record->reading_modifier_units,
                record->reading_linearization,
                record->reading_tolerance, record->reading_accuracy, record->reading_accuracy_exp,
                record->reading_m, record->reading_b,
                record->reading_r_exp, record->reading_b_exp,
                record->reading_raw, record->reading_raw_max, record->reading_raw_min,
                sensor_value(record, record->reading_raw));
            return buffer;
        case 0x02:
            record->addr            = getbits8(msg_recv->data[B(6)], 7, 1);
            record->addr_ipmb       = getbits8(msg_recv->data[B(6)], 0, 0);
            record->addr_channel    = getbits8(msg_recv->data[B(7)], 7, 4);
            record->addr_lun        = getbits8(msg_recv->data[B(7)], 1, 0);
            record->entity_id       = msg_recv->data[B(9)];
            record->entity_instance = getbits8(msg_recv->data[B(10)], 6, 0);
            record->entity_logical  = getbits8(msg_recv->data[B(10)], 7, 7);
            record->sensor_no       = msg_recv->data[B(8)];
            record->sensor_type     = msg_recv->data[B(13)];
            strncpy(record->name, (const char *) &msg_recv->data[B(33)], getbits8(msg_recv->data[B(32)], 4, 0));
            sprintf(buffer, "COMPACT SEN_0x%02X ADDR_0x%X-%X CHAN_0x%X LUN_0x%X %s %s", record->sensor_no,
                    record->addr, record->addr_ipmb, record->addr_channel, record->addr_lun,
                    record->name, sensor_type_str[record->sensor_type]);
            return buffer;
        case 0x03: return "EVENT-BASED";
        case 0x08: return "ENTITY-ASSOC";
        case 0x09: return "DEV-RELATIVE";
        case 0x10: return "DEV-LOCATOR";
        case 0x11:
            // Table 38-1, FRU Device Locator Field Usage
            record->addr            = getbits8(msg_recv->data[B(6)], 7, 1);
            record->id              = getbits8(msg_recv->data[B(7)], 7, 0);
            record->addr_lun        = getbits8(msg_recv->data[B(8)], 4, 3);
            record->addr_channel    = getbits8(msg_recv->data[B(9)], 7, 4);
            record->entity_logical  = getbits8(msg_recv->data[B(8)], 7, 7);
            record->addr_bus        = getbits8(msg_recv->data[B(8)], 2, 0);
            sprintf(buffer, "FRU-LOCATOR ID_0x%x ADDR_0x%X CHAN_0x%x LUN_0x%x %s BUS_0x%x",
                record->id, record->addr, record->addr_channel, record->addr_lun,
                (record->entity_logical) ? "LOGICAL": "PHYSICAL", record->addr_bus);
            return buffer;
        case 0x12:
            record->addr         = getbits8(msg_recv->data[B(6)], 7, 1);
            record->addr_channel = getbits8(msg_recv->data[B(7)], 3, 0);
            strncpy(record->name, (const char *) &msg_recv->data[B(17)], getbits8(msg_recv->data[B(16)], 4, 0));
            sprintf(buffer, "BMC-LOCATOR ADDR_0x%X CHAN_0x%x %s %s%s%s%s%s",
                    record->addr, record->addr_channel, record->name,
                    getbits8(msg_recv->data[B(9)], 1, 1) ? "SDR"    : "",
                    getbits8(msg_recv->data[B(9)], 3, 3) ? "FRU"    : "",
                    getbits8(msg_recv->data[B(9)], 7, 7) ? "Chassis": "",
                    getbits8(msg_recv->data[B(9)], 0, 0) ? "Sensors": "",
                    getbits8(msg_recv->data[B(9)], 6, 6) ? "Bridge" : "");
            return buffer;
        case 0x13: return "MCC-RECORD";
        case 0x14: return "MSG-CHANNEL";
    }
    if (record->type >= 0x0a && record->type <= 0x0f) return "RESERVED";
    if (record->type >= 0xc0) return "OEM"; // Original Equipment Manufacturer
    return "?";
}

int ipmi_driver_discover_sdr(ipmi_dev_t *d)
{
    ushort record_id = 0x0000;

    d->records = realloc(NULL, 16*sizeof(sdr_t));
    memset(&d->records[0], 0, 16*sizeof(sdr_t));

    //
    while (record_id != 0xFFFF)
    {
        // - The SDR reservation allows the SDR doesn't change while reading
        // registers. But we are going to assume it won't happen.
        ipmi_msg_t  *msg_send = ipmi_msg(IPMI_NETFN_STORAGE_REQUEST, 0x23, 6);
        ipmi_msg_t  *msg_recv = NULL;

        msg_send->data[2] = (record_id     ) & 0xFF;
        msg_send->data[3] = (record_id >> 8) & 0xFF;
        msg_send->data[4] = 0x00; // Offset
        msg_send->data[5] = 0xFF; // Length
        if (!(msg_recv = ipmi_driver_send_command(d->fd, msg_send, addr_def()))) {
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
        debug("FD%d SDR Record %02d [ %s ]", d->fd, record_id,
              sdr_parse(msg_recv, &d->records[d->records_count]));
        // Getting next ID
        record_id = msg_recv->data[1] | (msg_recv->data[2] << 8);
        // Allocating new space if required
        if (++d->records_count > 0 && (d->records_count % 16) == 0) {
            d->records = realloc(d->records, (d->records_count+16)*sizeof(sdr_t));
            memset(&d->records[d->records_count], 0, 16*sizeof(sdr_t));
        }
        // The message is fully-readed, the new space is allocated, so we can
        // now do recursion without peril
        //     if type == MC_LOCATOR then call GetDeviceId()
        if (record_id == 96-1) {
            ipmi_driver_sensor_read(&d->records[d->records_count-1]);
        }
    }
    return 0;
}

int ipmi_driver_discover_fru(ipmi_devs_t *d)
{
    typedef struct fru_header_s {
        uchar version;
        union {
            struct {
                uchar internal;
                uchar chassis;
                uchar board;
                uchar product;
                uchar multi;
            } offset;
            uchar offsets[5];
        };
        uchar pad;
        uchar checksum;
    } fru_header_t;

    fru_header_t header;
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
    ipmi_msg_t *msg_recv = NULL;
    ipmi_msg_t *msg_send = NULL;

    for (i = 0; i < d->count; ++i) {
        debug("FD%d", d->devs[i].fd);
        for (f = 0; f < 0xff; ++f) {
            msg_send = ipmi_msg(IPMI_NETFN_STORAGE_REQUEST, 0x10, 1);
            msg_send->data[0] = f; // FRU id

            if (!(msg_recv = ipmi_driver_send_command(d->devs[i].fd, msg_send, addr_def()))) {
                break;
            }
            // Completion codes:
            //   0x00 : OK
            //   0xc7 : Request data length invalid.
            //   0xca : Cannot return number of requested data bytes.
            //   0xcb : Requested Sensor, data, or record not present.
            debug("%u", msg_recv->data[0]);
            if (msg_recv->data[0] != 0x00) {
                break;
            }
            ushort inventory_size = (msg_recv->data[2] << 8) | msg_recv->data[1];
            ushort words = getbits8(msg_recv->data[3], 0, 0);
            debug("Detected FRU%x of size %u words%u", f, inventory_size, words);

            msg_send = ipmi_msg(IPMI_NETFN_STORAGE_REQUEST, 0x11, 4);
            msg_send->data[0] = f; // FRU id
            msg_send->data[1] = 0x0;
            msg_send->data[2] = 0x0;
            msg_send->data[3] = 0x8;
            if (!(msg_recv = ipmi_driver_send_command(d->devs[i].fd, msg_send, addr_def()))) {
                break;
            }
            debug("%u %u %u %u %u %u %u %u", msg_recv->data[0], msg_recv->data[1],
                msg_recv->data[2], msg_recv->data[3], msg_recv->data[4],
                msg_recv->data[5], msg_recv->data[6], msg_recv->data[7]);
            memcpy(&header, msg_recv->data + 1, 8);
            debug("fru.header.version:         0x%x", header.version);
            debug("fru.header.offset.internal: 0x%x", header.offset.internal * 8);
            debug("fru.header.offset.chassis:  0x%x", header.offset.chassis * 8);
            debug("fru.header.offset.board:    0x%x", header.offset.board * 8);
            debug("fru.header.offset.product:  0x%x", header.offset.product * 8);
            debug("fru.header.offset.multi:    0x%x", header.offset.multi * 8);
        }
    }
    return 0;
}

int main(int argc, char *argv)
{
    if (!ipmi_driver_open(&devs)) {
        return 0;
    }
    //ipmi_driver_discover_sdr(&devs.devs[0]);
    ipmi_driver_discover_fru(&devs);
    return 0;
}