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

#ifndef METRICS_COMMON_POPEN_H
#define METRICS_COMMON_POPEN_H

#include <common/states.h>

// Use example:
//  float f1, f2;
//  popen_t p;
//  int gpu;
//
//  popen_open("for i in {1..100}; do echo GPU $i 10.0 20.0 && sleep 1; done", 0, &p);
//  while(popen_read(&p, "aiff", &gpu, &f1, &f2)) {
//      printf("RETURNED: %d %f %f\n", gpu, f1, f2);
//  }
//  popen_close(&p); // You can close, or wait and try again to read
//
//  Format options:
//  - a: avoid (pass to next read argument)
//  - s: return integer
//  - i: return integer
//  - d: return double
//  - D: return double vector

typedef struct popen_s {
    char  table[512][16][128];
    int   rows_escape;
    int   rows_index;
    int   rows_count;
    int   cols_count;
    int   one_shot;
    int   opened;
    FILE *file;
    int   eof;
    int   fd;
} popen_t;

// Tests if a executable (binary or script) exists, before popen_open.
state_t popen_test(char *exec);

// Opens a process given a command.
//     @param escape_lines: avoids reading the first N lines. It can be useful
//         to escape the typical header returned by the command.
//     @param one_shot: used when the command process prints something and
//         immediately closes and is not kept in the background. Under the hood
//         blocks both read functions ensuring you have something to read.
state_t popen_open(char *command, int escape_lines, int one_shot, popen_t *p);

void popen_close(popen_t *p);

// Return 1 if there are lines pending to read.

// Reads a single line of the command output. It is a variadic function which
// converts the white spaced separated line of string format into a set of
// variables of different type.
//     @param fmt: it is the format and admits the following letters:
//         - a: avoid (pass to next read argument)
//         - s: return string
//         - i: return integer
//         - I: return integer vector
//         - d: return double
//         - D: return double vector
//         Take into the account that vector letters are used as last letters
//         in format because the function doesn't know in advance how many
//         elements the caller wants to read.
int popen_read(popen_t *p, const char* fmt, ...);

// This function is the same than popen_read() but returns the elements in
// string format given a separator.
int popen_read2(popen_t *p, char separator, char **elems, uint *elems_count);

// Counts the number of current lines kept in the buffer from last reading.
uint popen_count_read(popen_t *p);

// Count the number of lines pending.
uint popen_count_pending(popen_t *p);

#endif //METRICS_COMMON_POPEN_H