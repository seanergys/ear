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

//#define SHOW_DEBUGS 1

#include <stdio.h>
#include <stdlib.h>
#include <common/sizes.h>
#include <common/utils/string.h>
#include <common/output/debug.h>

size_t xsnsize(size_t size)
{
	return size;
}

char *xsnbuffer(char *buffer)
{
    return buffer;
}

int appenv(const char *var, const char *string)
{
	char buffer[SZ_PATH];
	char *p = getenv(var);

	if (p != NULL) {
		sprintf(buffer, "%s:%s", string, p);
	} else {
		sprintf(buffer, "%s", string);
	}
	return setenv(var, buffer, 1);
}

char **strtoa(const char *string, char separator, char ***rows, uint *rows_count)
{
    uint str_length = strlen(string);
    uint char_count = 0;
    char *pstring;
    char **prows;
    uint i;

    if (strlen(string) <= 1) {
        return NULL;
    }
    // Allocating space for the new separated string (and copying)
    pstring = calloc(str_length+1, sizeof(char));
    strcpy(pstring, string);
    // Counting
    for (i = 0; i < strlen(pstring)+1; ++i) {
        debug("%c", pstring[i]);
        if (pstring[i] == separator || pstring[i] == '\0') {
            *rows_count += (char_count > 0);
            char_count = 0;
        } else {
            char_count += 1;
        }
    }
    debug("Rows detected %d", *rows_count);
    if (rows_count == 0) {
        return NULL;
    }
    // Allocating space for the rows
    prows = calloc(*rows_count, sizeof(char *));
    // Setting new references
    for (*rows_count = i = 0; i < str_length+1; ++i) {
        if (pstring[i] == separator || pstring[i] == '\0') {
            if (char_count > 0) {
                prows[*rows_count] = &pstring[i-char_count];
                debug("R%d: %s", *rows_count, prows[*rows_count]);
                // Passing to next row
                *rows_count += 1;
            }
            // Setting end char of the row
            pstring[i] = '\0';
            char_count = 0; 
        } else {
            char_count += 1;
        }
    }
    if (rows != NULL) {
        *rows = prows;
    }
    return prows;
}

char **strtoa_free(char **rows)
{
    free(rows[0]);
    free(rows);
}

