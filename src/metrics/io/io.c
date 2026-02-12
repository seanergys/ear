/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

// clang-format off
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <metrics/io/io.h>
#include <metrics/io/archs/dummy.h>
#include <metrics/io/archs/proc_file.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static apinfo_t        info;
static io_ops_t        ops;

void io_load(topology_t *tp, int options)
{
    while (pthread_mutex_trylock(&lock));
    if (info.api != API_NONE) {
        goto done;
    }
    if (API_IS(options, API_DUMMY)) {
        goto dummy;
    }
    io_proc_file_load(tp, &ops, options);
dummy:
    io_dummy_load(tp, &ops, options);
    io_get_info(&info);
done:
    pthread_mutex_unlock(&lock);
}

void io_unload()
{
    while (pthread_mutex_trylock(&lock));
    if (ops.unload != NULL) {
        ops.unload();
        memset(&ops, 0, sizeof(io_ops_t));
        memset(&info, 0, sizeof(apinfo_t));
    }
    pthread_mutex_unlock(&lock);
}

state_t io_update(uint option, void *value)
{
    return ops.update(option, value);
}

void io_get_info(apinfo_t *info)
{
    memset(info, 0, sizeof(apinfo_t));
    info->layer = "IO";
    if (ops.get_info != NULL) {
        ops.get_info(info);
    }
}

state_t io_read(io_t *io)
{
    if (io == NULL) {
        return_msg(EAR_ERROR, Generr.input_null);
    }
    return ops.read(io);
}

state_t io_read_diff(io_t *io2, io_t *io1, io_t *io_diff)
{
    state_t s;
    if (state_fail(s = io_read(io2))) {
        return s;
    }
    io_data_diff(io2, io1, io_diff);
    return s;
}

state_t io_read_copy(io_t *io2, io_t *io1, io_t *io_diff)
{
    state_t s;
    if (state_fail(s = io_read_diff(io2, io1, io_diff))) {
        return s;
    }
    io_data_copy(io1, io2);
    return s;
}

void io_data_diff(io_t *io2, io_t *io1, io_t *io_diff)
{
    int i;
    for (i = 0; i < info.devs_count; ++i) {
        io_diff[i].pid   = io2[i].pid;
        io_diff[i].rchar = io2[i].rchar - io1[i].rchar;
        io_diff[i].wchar = io2[i].wchar - io1[i].wchar;
        io_diff[i].syscr = io2[i].syscr - io1[i].syscr;
        io_diff[i].syscw = io2[i].syscw - io1[i].syscw;
        io_diff[i].rstor = io2[i].rstor - io1[i].rstor;
        io_diff[i].wstor = io2[i].wstor - io1[i].wstor;
        io_diff[i].cancelled = io2[i].cancelled - io1[i].cancelled;
    }
}

void io_data_alloc(io_t **io)
{
    if (io == NULL) {
        return;
    }
    *io = (io_t *) calloc(info.devs_count, sizeof(io_t));
}

void io_data_free(io_t **io)
{
    if (io != NULL || *io != NULL) {
        return;
    }
    free(*io);
    *io = NULL;
}

void io_data_copy(io_t *io_dst, io_t *io_src)
{
    memcpy(io_dst, io_src, sizeof(io_t) * info.devs_count);
}

void io_data_print(io_t *io_diff, int fd)
{
    char buffer[1024];
    io_data_tostr(io_diff, buffer, sizeof(buffer));
    dprintf(fd, "%s", buffer);
}

char *io_data_tostr(io_t *io_diff, char *buffer, size_t length)
{
    int i, n;
    for (i = n = 0; i < info.devs_count; ++i) {
        if (io_diff[i].pid == 0) {
            continue;
        }
        n += snprintf(&buffer[n], length-n,
            "dev%d "
            "rchar %011llu wchar %011llu, "
            "syscr %011llu syscw %011llu, "
            "rstor %012llu wstor %012llu\n", i,
            io_diff[i].rchar, io_diff[i].wchar,
            io_diff[i].syscr, io_diff[i].syscw,
            io_diff[i].rstor, io_diff[i].wstor);
    }
    return buffer;
}