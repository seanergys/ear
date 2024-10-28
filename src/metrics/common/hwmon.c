/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#define SHOW_DEBUGS 1

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <common/sizes.h>
#include <common/output/debug.h>
#include <metrics/common/file.h>
#include <metrics/common/hwmon.h>

#define SET_NULL_DEVICE(dev)  dev.input_fd = -2
#define  IS_NULL_DEVICE(dev) (dev.input_fd == -2)

static int file_read(char *path, int *fd, int close_on_read, char *buffer, size_t buffer_length)
{
    int i  =  0;
    int r  =  0;
    if (*fd == -1) {
        if ((*fd = open(path, O_RDONLY)) < 0) {
            //debug("Opening '%s' failed: %s", path, strerror(errno));
            return 0;
        }
    }
    lseek(*fd, 0, SEEK_SET);
    while (r < buffer_length) {
        if ((r = read(*fd, &buffer[r], buffer_length)) <= 0) {
            break;
        }
        buffer_length -= r;
        i += r;
    }
    buffer[i] = '\0';
    if (buffer[strlen(buffer)-1] == '\n') {
        buffer[strlen(buffer)-1] =  '\0';
    }
    debug("HWMON read from '%s': %s", path, buffer);
    if (close_on_read) {
        close(*fd);
        *fd = -1;
    }
    return 1;
}

static int test_chip_folder(char *path)
{
    return filemagic_exists(path);
}

static int test_chip_name(char *chip_mem, char *folder_path, char *name_file)
{
    char path[SZ_PATH];
    char data[128];
    int fd = -1;

    sprintf(path, "%s/%s", folder_path, name_file);
    if (!file_read(path, &fd, 1, data, 128)) {
        return 0;
    }
    if (strcmp(chip_mem, data) != 0) {
        return -1;
    }
    return 1;
}

static int read_device_item(char *type_path, char *item_name, ullong address, int close)
{
    char path[SZ_PATH];
    int *item_toint;
    int *item_fd;
    char *item;

    item_fd    = (int  *) (address);
    item       = (char *) (address + sizeof(int));
    item_toint = (int  *) (address + sizeof(int) + 32);
    // Getting full item path
    if (type_path != NULL) {
        sprintf(path, "%s_%s", type_path, item_name);
    } else {
        sprintf(path, "fd %d", *item_fd);
    }
    // Reading file
    if (!file_read(path, item_fd, close, item, 32)) {
        return 0;
    }
    *item_toint = atoi(item);
    return 1;
}

static void open_devices(char *folder_path, char *devs_name, hwmon_t **devs, uint *devs_count)
{
    char type_path[SZ_PATH];
    int completed;
    int d = 0;

    *devs = NULL;
    *devs_count = 0;
    *devs = realloc(*devs, sizeof(hwmon_t));
    do {
        completed = 0;
        sprintf(type_path, "%s/%s%d", folder_path, devs_name, d+1);
        //debug("HWMON testing type %s", type_path);
        // Cleaning
        memset(&(*devs)[d], 0, sizeof(hwmon_t));
        (*devs)[d].input_fd            = -1;
        (*devs)[d].label_fd            = -1;
        (*devs)[d].max_fd              = -1;
        (*devs)[d].average_fd          = -1;
        (*devs)[d].average_interval_fd = -1;
        //
        #define offset(var) (ullong) &((*devs)[d].var)
        completed += read_device_item(type_path, "input"  , offset(input_fd)  , 0);
        completed += read_device_item(type_path, "label"  , offset(label_fd)  , 1);
        completed += read_device_item(type_path, "max"    , offset(max_fd)    , 1);
        completed += read_device_item(type_path, "average", offset(average_fd), 0);
        completed += read_device_item(type_path, "average_interval", offset(average_interval_fd), 1);
        if (completed) {
            // Allocating more space for the next device and its items (+2 because it NULL terminated)
            *devs = realloc(*devs, sizeof(hwmon_t)*((*devs_count)+2));
            (*devs_count)++;
            d++;
        } else {
            break;
        }
    } while(1);
    // Null-terminated list of devices
    SET_NULL_DEVICE((*devs)[d]);
}

state_t hwmon_open(char *chip_name, uint *chips_count, char *devs_name, hwmon_t ***devs, uint **devs_count)
{
    char folder_path[SZ_PATH];
    int i = 0;
    int test;

    *devs = NULL;
    *chips_count = 0;
    // [x] -> [a]
    // [a] -> [] [] []
    // [b] -> [] [] []
    // [c] -> [] [] []
    do {
        sprintf(folder_path, "/sys/class/hwmon/hwmon%d", i);
        //debug("HWMON testing chip %s", folder_path);
        // If folder does not exists, there are no more hwmon folders
        if (!test_chip_folder(folder_path)) {
            break;
        }
        // If file name matches
        if ((test = test_chip_name(chip_name, folder_path, "name")) != 0 ||
            (test = test_chip_name(chip_name, folder_path, "device/name")) != 0 ||
            (test = test_chip_name(chip_name, folder_path, "device/power1_oem_info")) != 0) { //Rare case
            // The is a chip but it has different name
            if (test == -1) {
                goto next;
            }
            debug("HWMON new chip found! We found %d already", (*chips_count)+1);
            // Allocating more space for the devices of the newly found chip (+2 because it NULL terminated)
            *devs = realloc(*devs, sizeof(hwmon_t *)*((*chips_count)+2));
            *devs_count = realloc(*devs_count, sizeof(uint)*((*chips_count)+1));
            // Opening the devices of this chip
            open_devices(folder_path, devs_name, &(*devs)[*chips_count], &(*devs_count)[*chips_count]);
            // Increasing the number of devices found
            (*chips_count)++;
        }
    next:
        ++i;
    } while(1);
    if (chips_count == 0) {
        return_print(EAR_ERROR, "HWMON failed to find %s chip name", chip_name);
    }
    // Null-terminating
    (*devs)[*chips_count] = NULL;
    // Extreme debugging
    #if 0
    int j;
    debug("List of opened files:")
    for (i = 0; i < *chips_count; ++i) {
        debug(" - CHIP%d '%s'", i, chip_name);
        for (j = 0; j < (*devs_count)[i]; ++j) {
            debug("   - DEV%d %s%d_", j, devs_name, j+1);
        }
    }
    #endif
    return EAR_SUCCESS;
}

state_t hwmon_read(hwmon_t **devs)
{
    int c = 0; // c of chip
    int d = 0; // d of device
    while (devs[c] != NULL) {
        while (!IS_NULL_DEVICE(devs[c][d])) {
            read_device_item(NULL, NULL, offset(input_fd)  , 0);
            read_device_item(NULL, NULL, offset(average_fd), 0);
            ++d;
        }
        // If fd = -1 do nothing
        ++c;
    }
    return EAR_SUCCESS;
}

int main(int argc, char *argv[])
{
    uint chips_count = 0;
    uint *devs_count = 0;
    hwmon_t **devs   = NULL;

    if (state_fail(hwmon_open("coretemp", &chips_count, "temp", &devs, &devs_count))) {
        debug("Failed: %s", state_msg);
    }
    hwmon_read(devs);
    return 0;
}