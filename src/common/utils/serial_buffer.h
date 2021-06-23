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
* This file is licensed under both the BSD-3 license for individual/non-commercial
* use and EPL-1.0 license for commercial use. Full text of both licenses can be
* found in COPYING.BSD and COPYING.EPL files.
*/

#ifndef COMMON_UTILS_SERIAL_BUFFER_H
#define COMMON_UTILS_SERIAL_BUFFER_H

#if 1
typedef struct wide_buffer_s {
	char *data;
} wide_buffer_t;
#else
typedef struct wide_buffer_s {
	size_t size_alloc;
	size_t size;
	size_t elem;
	char *data;
} wide_buffer_t;
#endif

void serial_alloc(wide_buffer_t *b, size_t size);

void serial_resize(wide_buffer_t *b, size_t size);

void serial_reset(wide_buffer_t *b);

void serial_free(wide_buffer_t *b);

void serial_add_elem(wide_buffer_t *b, char *data, size_t size);

char *serial_copy_elem(wide_buffer_t *b, char *data, size_t *size);

size_t serial_size(wide_buffer_t *b);

#endif
