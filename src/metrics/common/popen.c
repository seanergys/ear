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

//#define SHOW_DEBUGS 1

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <common/output/debug.h>
#include <metrics/common/popen.h>

#define MAX_ROWS          512
#define MAX_COLS          16
#define MAX_BUFFER        32768

static char no_string = '\0';
static char buffer[MAX_BUFFER];

state_t popen_test(char *exec)
{
    char command[1024];
    // This is not fully portable. There are too many
    // systems, and too many different shells.
    sprintf(command, "type %s > /dev/null 2>&1", exec);
    if (system(command)) {
        return_msg(EAR_ERROR, "command not found");
    }
    return EAR_SUCCESS;
}

state_t popen_open(char *command, int escape_lines, int one_shot, popen_t *p)
{
    // Cleaning
    memset(p, 0, sizeof(popen_t));
    // System
    if ((p->file = popen(command, "r")) == NULL) {
        debug("popen error: %s", strerror(errno));
        return_msg(EAR_ERROR, strerror(errno));
    }
    p->opened = 1;
    p->fd = fileno(p->file);
    p->rows_escape = escape_lines;
    p->one_shot = one_shot;
    if (!one_shot) {
        fcntl(p->fd, F_SETFL, O_NONBLOCK);
    }
    return EAR_SUCCESS;
}

void popen_close(popen_t *p)
{
    p->opened = 0;
    pclose(p->file);
    memset(p, 0, sizeof(popen_t));
}

static void static_flush(popen_t *p, char *buffer)
{
    while (fread(buffer, sizeof(char), MAX_BUFFER, p->file) > 0);
}

static int static_read(popen_t *p, char *buffer)
{
    size_t len_acum = 0;
    size_t len_read = 0;

    if (p->one_shot) {
        do {
            // If one shot, we are reading one by one and spaces ' ' will be compressed
            buffer[len_acum] = (char) getc(p->file);
            len_acum = len_acum + 1;
            if (len_acum > 1) {
                if (buffer[len_acum-1] == ' ' && buffer[len_acum-2] == ' ') {
                    len_acum = len_acum - 1;
                }
            }
        } while(buffer[len_acum-1] != EOF);
    } else {
        do {
            // If not one shot, we are reading a bulk of bytes (less overhead)
            len_read = fread(&buffer[len_acum], sizeof(char), MAX_BUFFER-len_acum, p->file);
            len_acum = len_acum + len_read;
        } while(len_read > 0);
    }
    debug("read %lu bytes", len_acum);
    // Too much data
    if (len_acum == MAX_BUFFER) {
        static_flush(p, buffer);
        return_msg(0, "Flushing PIPE due excess of data");
    }
    // End of string
    buffer[len_acum] = '\0';
    // Not enough data
    if (len_acum <= 0) {
        return_msg(0, "0 bytes read");
    }
    return 1;
}

static void static_parse(popen_t *p, char *buffer, char separator)
{
    int char_count = 0;
    int word_count = 0;
    int c;

    p->cols_count = 0;
    p->rows_count = 0;
    p->rows_index = 0;
    // Cleaning table
    memset(p->table, 0, sizeof(p->table));
    for (c = 0; c < strlen(buffer)+1; ++c) {
        if (buffer[c] == separator || buffer[c] == '\t' || buffer[c] == '\n' || buffer[c] == '\0') {
            word_count += (char_count > 0);
            char_count = 0;
        } else {
            p->table[p->rows_count][word_count][char_count+0] = buffer[c];
            p->table[p->rows_count][word_count][char_count+1] = '\0';
            char_count += 1;
        }
        if (buffer[c] == '\n') {
            if (word_count > p->cols_count) {
                p->cols_count = word_count;
            }
            p->rows_count++;
            if (p->rows_escape) {
                p->rows_escape--;
                p->rows_count--;
            }
            word_count = 0;
            char_count = 0;
        }
        if (p->rows_count >= MAX_ROWS) {
            break;
        }
    }
    #if SHOW_DEBUGS
    int r;
    for (r = 0; r < p->rows_count; ++r) {
        for (c = 0; c < p->cols_count; ++c) {
            if (strlen(p->table[r][c])) {
                debug("TABLE[%d][%d]: '%s'", r, c, p->table[r][c]);
            }
        }
    }
    #endif
}

static int popen_pending(popen_t *p)
{
    return p->rows_index < p->rows_count;
}

static int popen_eof(popen_t *p)
{
    return p->eof;
}

int popen_read2(popen_t *p, char separator, const char* f, ...)
{
    va_list args;
    int was_pending;
    int c, r;

    if (!p->opened) {
        return 0;
    }
    // If there is no pending data to return
    if (!popen_pending(p)) {
        // If not end of file
        if (!popen_eof(p)) {
            // Then read and parse
            if ((static_read(p, buffer))) {
                static_parse(p, buffer, separator);
            }
        }
    }
    // Disabling end of file in next
    if (popen_eof(p)) {
        p->eof = 0;
    }
    // Checking again if there is new pending data
    was_pending = popen_pending(p);
    // Indexes
    r = p->rows_index;
    c = 0;

    // Variadic part
    va_start(args, f);
    while (*f != '\0')
    {
        if (*f == 'a') {
            // Avoid, do nothing
        }
        // If string
        if (*f == 's') {
            char **s = va_arg(args, char **);
            s[0] = &no_string;
            if (was_pending) {
                s[0] = p->table[r][c];
            }
        }
        // If string vector
        if (*f == 'S') {
            char **S = va_arg(args, char **);
            while(c < p->cols_count) {
                *S = &no_string;
                if (was_pending) {
                    *S = p->table[r][c];
                }
                ++S;
                ++c;
            }
        }
        // If signed integer
        if (*f == 'i') {
            int *i = va_arg(args, int *);
            if (was_pending) {
                *i = atoi(p->table[r][c]);
            } else {
                *i = 0;
            }
        }
        // If signed integer vector
        if (*f == 'I') {
            int *I = va_arg(args, int *);
            while(c < p->cols_count) {
                if (was_pending) {
                    *I = atoi(p->table[r][c]);
                } else {
                    *I = 0;
                }
                ++I;
                ++c;
            }
            break;
        }
        // If double
        if (*f == 'd') {
            double *d = va_arg(args, double *);
            if (was_pending) {
                *d = atof(p->table[r][c]);
            } else {
                *d = 0.0;
            }
        }
        // If double vector
        if (*f == 'D') {
            double *D = va_arg(args, double *);
            while(c < p->cols_count) {
                if (was_pending) {
                    *D = atof(p->table[r][c]);
                } else {
                    *D = 0.0;
                }
                ++D;
                ++c;
            }
            break;
        }
        ++f;
        ++c;
    }
    va_end(args);

    if (was_pending) {
        p->rows_index++;
        // Next time return o to indicate EOF (until next read)
        if (!popen_pending(p)) {
            p->eof = 1;
        }
    }
    return was_pending;
}

uint popen_count_read(popen_t *p)
{
    return (uint) p->rows_count;
}

uint popen_count_pending(popen_t *p)
{
    return (uint) (p->rows_count - p->rows_index);
}

#if TEST1 || TEST2
int main(int argc, char *argv[])
{
    popen_t p;
    #if TEST1
    double f1, f2;
    char *s1;
    int gpu;

    popen_open("for i in 0 1 2 3; do echo GPU $i 10.0 20.0 some_text && sleep 1; done", 0, 0, &p);
    while (1) {
        while(popen_read2(&p, ' ', "aidds", &gpu, &f1, &f2, &s1)) {
            printf("%d %f %f %s\n", gpu, f1, f2, s1);
            if (gpu == 3) {
                return 0;
            }
        }
    }
    #else
    char *s1[3];
    int i1;

    popen_open("for i in 0 1 2 3; do echo $i text1 text2 text3 && sleep 1; done", 0, 0, &p);
    while (1) {
        while(popen_read2(&p, ' ', "iS", &i1, s1)) {
            printf("%d %s %s %s\n", i1, s1[0], s1[1], s1[2]);
            if (i1 == 3) {
                return 0;
            }
        }
    }
    #endif
    return 0;
}
#endif