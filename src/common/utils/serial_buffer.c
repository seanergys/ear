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

#include <stdlib.h>
#include <string.h>
#include <common/utils/serial_buffer.h>

#define psize_alloc(b) ((size_t *) &b->data[0]);
#define psize_taken(b) ((size_t *) &b->data[sizeof(size_t)]);
#define pelem(b)       ((size_t *) &b->data[sizeof(size_t)*2]);
#define size_meta      sizeof(size_t)*3

//  0: size alloc
//  8: size taken
// 16: elem (pointer)
// 24: data (data size + data)

static void serial_init(wide_buffer_t *b, size_t size)
{
	size_t *size_alloc;
	size_t *size_taken;
	size_t *elem;

	if (b->data != NULL) {
		return;
	}
	// Size meta + size first data size + size data
	if ((size = size_meta + sizeof(size_t) + size) < 8192) {
		size = 8192;
	}
	b->data = calloc(1, size);
	// Taking pointers from first bytes
	size_taken = psize_taken(b);
	size_alloc = psize_alloc(b);
	elem = pelem(b);
	// Default values
	*size_taken = size_meta;
	*size_alloc = size;
	*elem = size_meta;
}

void serial_alloc(wide_buffer_t *b, size_t size)
{
	b->data = NULL;
	serial_init(b, size);
}

void serial_resize(wide_buffer_t *b, size_t size)
{
	size_t *size_alloc;
	// Init if required
	serial_init(b, size);
	size_alloc = psize_alloc(b);
	// Size data + size data size + size taken
	if ((size = size_meta + sizeof(size_t) + size) > *size_alloc) {
		b->data = realloc(b->data, size);
		// Recovering size_alloc
		size_alloc = psize_alloc(b);
		// New size
		*size_alloc = size;
	}
}

void serial_reset(wide_buffer_t *b)
{
	size_t *size_taken;
	size_t *elem;
	
	if (b->data == NULL) {
		serial_init(b, 0);
		return;
	}
	size_taken = psize_taken(b);
	elem = pelem(b);
	*size_taken = size_meta;
	*elem = size_meta;	
}

void serial_free(wide_buffer_t *b)
{
	free(b->data);
	b->data = NULL;
}

void serial_add_elem(wide_buffer_t *b, char *data, size_t size)
{
	size_t *size_alloc;
	size_t *size_taken;

	// Error handling
	serial_resize(b, size);
	// Taking pointers
	size_alloc = psize_alloc(b);
	size_taken = psize_taken(b);
	//
	memcpy(&b->data[*size_taken], (char *) &size, sizeof(size_t));
	*size_taken += sizeof(size_t);
	memcpy(&b->data[*size_taken], data, size);
	*size_taken += size;
}

char *serial_copy_elem(wide_buffer_t *b, char *data, size_t *size)
{
	size_t *size_alloc;
	size_t *size_taken;
	size_t *size_elem;
	size_t *elem;
	char   *p;

	//
	size_alloc = psize_alloc(b);
	size_taken = psize_taken(b);
	elem = pelem(b);
	// It is the end, try to rewind
	if (*elem == *size_taken) {
		return NULL;
	}
	// Copying the content data
	size_elem = (size_t *) &b->data[*elem];
	if (data == NULL) {
		p = calloc(1, *size_elem);
	} else {
		p = data;
	}
	// Elem points directly to the data
	*elem += sizeof(size_t);
	// Copying
	p = memcpy(p, &b->data[*elem], *size_elem);
	// Elem points to next element
	*elem += *size_elem;

	return p;
}

size_t serial_size(wide_buffer_t *b)
{
	size_t *size_taken = psize_taken(b);
	return *size_taken;
}
