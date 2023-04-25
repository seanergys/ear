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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <common/utils/strscreen.h>

#define Y 0
#define X 1
#define clrscr()     printf("\e[1;1H\e[2J")
#define debug(...)   printf(__VA_ARGS__)
#define buffer(y, x) s->buffer[(y)*s->width+(x)]

static void clean_buffer_background(strscreen_t *s)
{
    int y, x;
    for (y = 0; y < s->height; ++y) {
        for (x = 0; x < s->width; ++x) {
            buffer(y,x) = s->bckchr;
        }
        buffer(y,x-1) = '\n';
    }
    buffer(y-1,x) = '\0';
}

void wprintf_init(strscreen_t *s, int height, int width, int fd, char bckchr)
{
    s->buffer = calloc((width+1)*height, sizeof(char));
    s->divs   = calloc(8, sizeof(strscreen_t)); //8 subdivisions is fairly enough
    s->height = height;
    s->width  = width;
    s->fd     = fd;
    s->bckchr = bckchr;
    clean_buffer_background(s);
}

static void clean_matrix_background(strscreen_t *s)
{
    int x, y;
    for (y = 0; y < s->height; ++y)
        for (x = 0; x < s->width-1; ++x) {
            s->matrix[y][x] = s->bckchr;
        }
}

void wprintf_divide(strscreen_t *s, int p1[2], int p2[2], int div_index)
{
    strscreen_t *div = &(((strscreen_t *) s->divs)[div_index]);
    int y;
    // X: #######\n <- (n-2 = '#') (n-1 = '\n')
    // Y: #######\0 <- (n-1 = '#')
    if (p2[X] >= s->width ) p2[X] = s->width-3;
    if (p2[Y] >= s->height) p2[Y] = s->height-2;
    div->buffer = s->buffer;
    div->width  = p2[X] - p1[X];
    div->height = p2[Y] - p1[Y];
    div->bckchr = ' ';
    div->matrix = calloc(div->height, sizeof(char *));
    // Setting division pointers
    for (y = 0; y < div->height; ++y) {
        div->matrix[y] = &div->buffer[(p1[Y]+y)*s->width+p1[X]];
    }
    s->divs_count++;
    clean_matrix_background(div);
}

void wsprintf(strscreen_t *s, int div_index, int append, char *buffer)
{
    strscreen_t *div = &(((strscreen_t *) s->divs)[div_index]);
    size_t length = strlen(buffer);
    int c, x;

    // Cleaning
    if (!append) {
        clean_matrix_background(div);
        div->y = 0;
    }
    // Copying characters
    for (c = x = 0; c < length && div->y < div->height; ++c) {
        debug("'%c'\n", buffer[c]);
        if (x == 0) {
            div->matrix[div->y][x] = ' ';
            ++x;
        }
        if (x == (div->width-2)) {
            while (buffer[c] != '\n' && buffer[c] != '\0') {
                c++;
            }
        }
        if (buffer[c] == '\0') {
            continue;
        }
        if (buffer[c] == '\n') {
            x = -1;
            div->y++;
        } else {
            div->matrix[div->y][x] = buffer[c];
        }
        x++;
    }
}

void wprintf(strscreen_t *s)
{
    clrscr();
    printf("%s", s->buffer);
}