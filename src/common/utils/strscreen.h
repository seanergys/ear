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

#ifndef COMMON_UTILS_STRSCREEN_H
#define COMMON_UTILS_STRSCREEN_H

#include <common/types.h>

typedef struct screen_s {
    void       *divs;
    cchar      *title;
    int         divs_count;
    char       *buffer; //unprocessed buffer
    char       *buffer_bench; //working bench buffer
    char       *buffer_final; //buffer ready to print
    char      **matrix;
    char        bckchr;
    int         height;
    int         width;
    int         fd;
    int         y;
} strscreen_t;

void scprintf_init(strscreen_t *screen, int height, int width, int fd, char bckchr);

void scprintf_divide(strscreen_t *screen, int p1[2], int p2[2], int *id, cchar *title);

// Writes the content of a buffer in the 'id' division.
void scsprintf(strscreen_t *s, int id, int append, int set_title, char *buffer);

// Returns and writes in the 'fd' output the whole formatted screen.
char *scprintf(strscreen_t *s);

#endif //COMMON_UTILS_STRSCREEN_H
