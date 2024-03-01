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

#ifndef COMMON_UTILS_SERIAL_BUFFER_H
#define COMMON_UTILS_SERIAL_BUFFER_H

// This is a buffer to keep data divided in elements. Its intern metadata
// control the allocated data, the current used data and also a pointer to the
// current element (for iterations). The current data element is the one to be
// returned when using functions like serial_copy_elem().
//
//	1) Serial buffer initialization
//
//		serial_buffer_t b;
//		// Initializing 16KB of memory.
//		serial_alloc(&b, 16384);
//
//	2) Example of packing data
//
//		// Cleans the buffer and metadata
//		serial_clean(&b);
//		// Adding new elements
//		serial_add_elem(&b, (char *) elem1, sizeof(elem1_t)*elem1_length);
//		serial_add_elem(&b, (char *) elem2, sizeof(elem2_t)*elem2_length);
//
//	3) Example of sending data
//
//		char  *data = w.data;
//		size_t size = serial_size(&b, 0);
//		some_sending_function(fd, data, size)));
//
//	4) Example of receiving data
//
// 		serial_buffer_t w;
//		// Initializing 16KB of memory.
//		serial_alloc(&b, 16384);
//		// You can use this because the first parameter in the buffer
//		// structure is the allocated space.
//		some_receiving_function(call, (char **) &b);
//
//	5) Example of unpacking data
//
//		// Rewind is necessary to set the internal pointer to the first
//		// element again. The first time is not required.
//		serial_rewind(&b);
//		serial_copy_elem(&b, (char *) elem1, NULL);
//		serial_copy_elem(&b, (char *) elem2, NULL);
//

typedef struct serial_buffer_t {
	char *data;
	size_t size; //Allocated size
} serial_buffer_t;

typedef serial_buffer_t wide_buffer_t; // Old name

#define SIZE_1KB 1024
#define SIZE_2KB 2048
#define SIZE_4KB 4096
#define SIZE_8KB 8192

void serial_alloc(serial_buffer_t *b, size_t size);

void serial_free(serial_buffer_t *b);

/* Allocates more space if needed. */
void serial_resize(serial_buffer_t *b, size_t size);

/* Marks the data buffer as 0 bytes taken and rewinds the element pointer. */
void serial_clean(serial_buffer_t *b);

/* Resize + clean. */
void serial_reset(serial_buffer_t *b, size_t size);

/* Back to the first the element pointer. */
void serial_rewind(serial_buffer_t *b);

/* Adds a new element to the buffer. */
void serial_add_elem(serial_buffer_t *b, char *data, size_t size);

/* Copies the next element inside the buffer. */
char *serial_copy_elem(serial_buffer_t *b, char *data, size_t *size);

/* Returns the pointer to the data (avoid the metadata). */
char *serial_data(serial_buffer_t *b);

/* Returns the size of the whole buffer or just the data (if 1). */
size_t serial_size(serial_buffer_t *b);

// Serial params is a small extension of serial buffer. It saves struct
// parameters/fields/variables in a serial buffer, adding a tag to properly
// identify each introduced parameter. Pack and unpack functions are self
// describing. You can use the auto macros for code cleaning.
//
// Sender example:
//     serial_params_pack_auto(&b, appS->job.id);
//     serial_params_pack_auto(&b, appS->job.step_id);
//
// Receiver example:
//     serial_params_pack_auto(&b, appR->job.id);
//     serial_params_pack_auto(&b, appR->job.step_id);

#define serial_params_pack_auto(sp, parameter) \
    serial_params_pack(sp, #parameter, (void *) &parameter, sizeof(parameter));

#define serial_params_unpack_auto(sp, parameter) \
    serial_params_unpack(sp, #parameter, (void *) &parameter, sizeof(parameter));

void serial_params_pack(serial_buffer_t *b, char *tag, void *param, size_t param_size);

// This function pops parameters.
int serial_params_unpack(serial_buffer_t *b, char *tag, void *param, size_t expected_size);

void serial_params_print(serial_buffer_t *b);

#endif