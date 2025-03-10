/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#include <math.h>
#include <stdio.h>
#include <ctype.h>
#include <stddef.h>
#include <string.h>
#include <ipmi_driver_parsing.h>

static const char *unit_str[] = {
    "unspecified", "degrees C", "degrees F", "degrees K", "Volts", "Amps",
    "Watts", "Joules", "Coulombs", "VA", "Nits", "lumen", "lux", "Candela",
    "kPa", "PSI", "Newton", "CFM", "RPM", "Hz", "microsecond", "millisecond",
    "second", "minute", "hour", "day", "week", "mil", "inches", "feet", "cu in",
    "cu feet", "mm", "cm", "m", "cu cm", "cu m", "liters", "fluid ounce",
    "radians", "steradians", "revolutions", "cycles", "gravities", "ounce",
    "pound", "ft-lb", "oz-in", "gauss", "gilberts", "henry", "millihenry",
    "farad", "microfarad", "ohms", "siemens", "mole", "becquerel", "PPM",
    "reserved", "Decibels", "DbA", "DbC", "gray", "sievert", "color temp deg K",
    "bit", "kilobit", "megabit", "gigabit", "byte", "kilobyte", "megabyte",
    "gigabyte", "word", "dword", "qword", "line", "hit", "miss", "retry",
    "reset", "overflow", "underrun", "collision", "packets", "messages",
    "characters", "error", "correctable error", "uncorrectable error",
    "fatal error", "grams", NULL
};
const char *sensor_type_str[] = {

    "reserved", "Temperature", "Voltage", "Current", "Fan", "Physical Security",
    "Platform Security", "Processor", "Power Supply", "Power Unit",
    "Cooling Device", "Other", "Memory", "Drive Slot / Bay",
    "POST Memory Resize", "System Firmwares", "Event Logging Disabled",
    "Watchdog1", "System Event", "Critical Interrupt", "Button",
    "Module / Board", "Microcontroller", "Add-in Card", "Chassis", "Chip Set",
    "Other FRU", "Cable / Interconnect", "Terminator", "System Boot Initiated",
    "Boot Error", "OS Boot", "OS Critical Stop", "Slot / Connector",
    "System ACPI Power State", "Watchdog2", "Platform Alert", "Entity Presence",
    "Monitor ASIC", "LAN", "Management Subsys Health", "Battery",
    "Session Audit", "Version Change", "FRU State", NULL
};

#if 1
static uint getbits32(uint reg, uint left_bit, uint right_bit)
{
    uint digits = left_bit - right_bit;
    uint mask   = (((0x01 << digits) - 1) << 1) + 1;
    return ((reg >> right_bit) & mask);
}

static uchar getbits8(uchar reg, uchar left_bit, uchar right_bit)
{
    uchar digits = left_bit - right_bit;
    uchar mask   = (((0x01 << digits) - 1) << 1) + 1;
    return ((reg >> right_bit) & mask);
}

char *strtolow(char *string)
{
    char *p = string;
    while (*string != '\0') {
        *string = tolower((unsigned char) *string);
        string++;
    }
    return p;
}
#endif

void ipmi_driver_parse_device_id(uchar *data, ipmi_dev_t *dev)
{
    // Getting BMC data
    dev->id      = data[1];
    dev->has_sdr = getbits32((uint) data[2], 7, 7);
    dev->has_fru = getbits32((uint) data[6], 3, 3);
    debug("FD%d Detected BMC [ ID0x%X %s%s]", dev->fd, dev->id,
          (dev->has_sdr    ) ? "SDR "    : "",
          (dev->has_fru    ) ? "FRU "    : ""
    );
}

uchar ipmi_driver_parse_sdr(uchar *data, sdr_t *rec, ipmi_addr_t *addr)
{
    #define B(n) 3+n-1
    static char buffer[256];

    switch((rec->type = data[B(4)]))
    {
        case 0x01: // FULL
            rec->entity_instance         = getbits8(data[B(10)], 6, 0);
            rec->entity_logical          = getbits8(data[B(10)], 7, 7);
            rec->sensor_no               = data[B(8)];
            rec->sensor_type             = data[B(13)];
            rec->sensor_type_str         = (char *) sensor_type_str[rec->sensor_type];
            rec->formula_complement      = getbits8(data[B(21)], 7, 6);
            rec->formula_rate_unit       = getbits8(data[B(21)], 5, 3);
            rec->formula_modifier        = getbits8(data[B(21)], 2, 1);
            rec->formula_percentage      = getbits8(data[B(21)], 0, 0);
            rec->formula_result_unit     = data[B(22)];
            rec->formula_result_unit_str = (char *) unit_str[rec->formula_result_unit];
            rec->formula_modifier_unit   = data[B(23)];
            rec->formula_lin_function    = getbits8(data[B(24)], 6, 0);
            rec->formula_m               = getbits8(data[B(26)], 7, 6) << 8 | data[B(25)];
            rec->formula_b               = getbits8(data[B(28)], 7, 6) << 8 | data[B(27)];
            rec->formula_m               = getbits32(rec->formula_m, 9, 9) ? 0xfffffc00 | rec->formula_m : rec->formula_m; // Converting to ca2
            rec->formula_b               = getbits32(rec->formula_b, 9, 9) ? 0xfffffc00 | rec->formula_b : rec->formula_b; // Converting to ca2
            rec->formula_r_exp           = getbits8(data[B(30)], 7, 4);
            rec->formula_b_exp           = getbits8(data[B(30)], 3, 0);
            rec->formula_r_exp           = getbits32(rec->formula_r_exp, 3, 3) ? 0xfffffff0 | rec->formula_r_exp : rec->formula_r_exp; // Converting to ca2
            rec->formula_b_exp           = getbits32(rec->formula_b_exp, 3, 3) ? 0xfffffff0 | rec->formula_b_exp : rec->formula_b_exp; // Converting to ca2
            strncpy(rec->name      , (const char *) &data[B(49)], getbits8(data[B(48)], 4, 0));
            strncpy(rec->name_lower, (const char *) &data[B(49)], getbits8(data[B(48)], 4, 0));
            strtolow(rec->name_lower);
            debug("FULL    SEN#%03d INS#%02d [%s %s] "
                   "(%uca2 %ur %um %upcnt) (%uru %umu) %ulin (%dm %db) (%dre %dbe)",
                    rec->sensor_no, rec->entity_instance,
                    rec->name, sensor_type_str[rec->sensor_type],
                    rec->formula_complement, rec->formula_rate_unit,
                    rec->formula_modifier, rec->formula_percentage,
                    rec->formula_result_unit, rec->formula_modifier_unit,
                    rec->formula_lin_function, rec->formula_m, rec->formula_b,
                    rec->formula_r_exp, rec->formula_b_exp);
            break;
        case 0x02: // COMPACT
            rec->entity_instance        = getbits8(data[B(10)], 6, 0);
            rec->entity_logical         = getbits8(data[B(10)], 7, 7);
            rec->sensor_no              = data[B(8)];
            rec->sensor_type            = data[B(13)];
            rec->formula_rate_unit      = getbits8(data[B(21)], 5, 3);
            rec->formula_modifier       = getbits8(data[B(21)], 2, 1);
            rec->formula_percentage     = getbits8(data[B(21)], 0, 0);
            rec->formula_result_unit    = data[B(22)];
            rec->formula_modifier_unit  = data[B(23)];
            strncpy(rec->name      , (const char *) &data[B(33)], getbits8(data[B(32)], 4, 0));
            strncpy(rec->name_lower, (const char *) &data[B(33)], getbits8(data[B(32)], 4, 0));
            strtolow(rec->name_lower);
            debug("COMPACT SEN#%03d INS#%02d [%s %s] "
                  "(%ur %um %upcnt) (%uru %umu)",
                  rec->sensor_no, rec->entity_instance,
                  rec->name, sensor_type_str[rec->sensor_type],
                  rec->formula_rate_unit, rec->formula_modifier,
                  rec->formula_percentage, rec->formula_result_unit,
                  rec->formula_modifier_unit);
            break;
        case 0x03: // EVENT-BASED
            debug("EVENT-BASED");
            break;
        case 0x08: // ENTITY-ASSOC
            debug("ENTITY-ASSOC");
            break;
        case 0x09: // DEV-RELATIVE
            debug("DEV-RELATIVE");
            break;
        case 0x10: // DEV-LOCATOR
            debug("DEV-LOCATOR");
            break;
        case 0x11: // FRU-LOCATOR
            debug("FRU-LOCATOR");
            break;
        case 0x12: // BMC-LOCATOR
            rec->new_addr_slave   = getbits8(data[B(6)], 7, 1);
            rec->new_addr_channel = getbits8(data[B(7)], 3, 0);
            strncpy(rec->name, (const char *) &data[B(17)], getbits8(data[B(16)], 4, 0));
            debug("BMC-LOCATOR ADDR_0x%X CHAN_0x%x %s %s%s",
                    rec->new_addr_slave, rec->new_addr_channel, rec->name,
                    getbits8(data[B(9)], 1, 1) ? "SDR ": "",
                    getbits8(data[B(9)], 3, 3) ? "FRU": "");
            break;
        case 0x13: // MCC-RECORD
            debug("MCC-RECORD");
            break;
        case 0x14: // MSG-CHANNEL
            debug("MSG-CHANNEL");
            break;
        default:
            if (rec->type >= 0x0a && rec->type <= 0x0f) {} //RESERVED
            if (rec->type >= 0xc0) {} // Original Equipment Manufacturer (OEM)
    }
    rec->addr = addr;
    return rec->type;
}

void ipmi_driver_parse_sensor_reading(uchar raw, sdr_t *rec, ipmi_reading_t *val)
{
    // Linearization formula:
    //   y = lf((mx + (b * 10^(b_exp))) * 10^(r_exp)) units
    //
    //   where:
    //   - x Raw reading
    //   - y Converted reading
    //   - lf() Linearization function specified by ‘lin_type’.
    //     This function is ‘null’ (y = f(x) = x) if the sensor is linear.
    //   - m Signed integer constant multiplier
    //   - b Signed additive ‘offset’
    //   - b_exp Signed Exponent. Sets ‘decimal point’ location for b.
    //   - r_exp Signed Result Exponent. Sets ‘decimal point’ location for
    //     the result before the linearization function is applied.
    double b_exp = (double) rec->formula_b_exp;
    double r_exp = (double) rec->formula_r_exp;
    double m = (double) rec->formula_m;
    double b = (double) rec->formula_b;
    double y = 0.0;
    long   x = raw; // Unsigned

    // Complement processing
    if (rec->formula_complement == 0x02) { //Two's complement
        x = (long) ((char) raw);
    } else { // Ones' complement
        x = (((char) raw) >= 0) ? (long) ((uchar) raw): (long) ((char) raw+1);
    }
    // Operations
    b = b * pow(10.0, b_exp);
    y = (m * ((double) x)) + b;
    y = y * pow(10.0, r_exp);
    debug("%s #%u => %lfy = lf((%lfm*%ldx + (%lfb * 10^(%lfb_exp))) * 10^(%lfr_exp))",
          rec->name, rec->sensor_no, y, m, x, b, b_exp, r_exp);
    val->value = y;
}