/*
 *
 * This program is part of the EAR software.
 *
 * EAR provides a dynamic, transparent and ligth-weigth solution for
 * Energy management. It has been developed in the context of the
 * Barcelona Supercomputing Center (BSC)&Lenovo Collaboration project.
 *
 * 
 * BSC Contact   mailto:ear-support@bsc.es
 * 
 *
 * EAR is an open source software, and it is licensed under both the BSD-3 license
 * and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
 * and COPYING.EPL files.
 */

#ifndef _SHARED_AREAS_H
#define _SHARED_AREAS_H

#include <sys/stat.h>


/** Creates a shared memory region based on files and mmap.
 *
 * \param[in] path The full file name to store data.
 * \param[in] file_perms_bits The file permission bits of the to-be created file.
 * \param[in] data The data to be stored in shared memory.
 * \param[in] area_size The size in bytes of \ref data.
 * \param[in] must_clean Specifies whether erasing \ref data before storing it.
 * \param[out] shared_fd The file descriptor of the created file.
 *
 * \return NULL on error.
 * \return the shared region pointer otherwise. */
void *create_shared_area(char *path, mode_t file_perms_bits, char *data, int area_size, int *shared_fd, int must_clean);


void * attach_shared_area(char *path,int area_size,uint perm,int *shared_fd,int *s);
void dettach_shared_area(int fd);
void dispose_shared_area(char *path,int fd);
#endif
