/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

/* clang-format off */
#include <stdio.h>
#include <string.h>
#include <common/utils/args.h>
#include <common/utils/string.h>

#define PTR_FOUND_VALUE ((void *)(intptr_t)-1)

static char *_args_get(int argc, char *argv[], const char *flag_in, char *buffer)
{
    char *equal      = NULL; // If found the symbol '='
    char *no_space   = NULL; // When -fValue
    char *p          = NULL;
    int argv_length  = 0;
    int flag_length  = 0;
    int is_mandatory = 0; // Its true only if the flag has a value
    int is_multiple  = 0; // Capable to return a list of arguments separated by ' '

    // Base case
    if (argc == 0 || argv[0] == NULL) {
        if (buffer != NULL) buffer[0] = '\0'; // Cleaning the buffer
        return NULL;
    }
    if ((p = _args_get(argc-1, &argv[1], flag_in, buffer)) != NULL && p != PTR_FOUND_VALUE) {
        return p;
    }
    //if (buffer != NULL) printf("looking %d'%s', buffer is '%s'\n", argc, flag_in, buffer);
    argv_length  = strlen(argv[0]);
    flag_length  = strlen(flag_in);
    is_multiple  = (flag_in[flag_length-1] == '*');
    is_mandatory = (flag_in[flag_length-1] == ':') || is_multiple;
    flag_length  = flag_length - is_mandatory; // Removing the last ':' or '*'
    // If is flag and is a long flag (--flag)
    if (strncmp(argv[0], "--", 2) == 0) {
        // If argument is --args and flag is a, this is not the place
        if (flag_length <= 1) {
            return NULL;
        }
        // If contains equal symbol
        equal = strchr(argv[0], '=');
        if (strncmp(&argv[0][2], flag_in, flag_length) == 0 && equal != NULL) {
            if (buffer != NULL) sprintf(buffer, "%s", ++equal);
            return strlen(equal)? equal: NULL;
        }
        // If don't has equal symbol
        if (strncmp(&argv[0][2], flag_in, flag_length) == 0) {
            // If found value in the next argument, it is perfect
            if (p == PTR_FOUND_VALUE) {
                return argv[1];
            // But if no value neither is mandatory, we return the argument itself
            } else if (!is_mandatory) {
                return argv[0];
            }
        }
        // Is not the flag, cleaning
        if (buffer != NULL) buffer[0] = '\0';
        return NULL;
    // If is flag and is short (-f)
    } else if (argv[0][0] == '-') {
        // If argument is -a and flag is --something, this is not the place
        if (flag_length > 1) {
            return NULL;
        }
        // It can not be a flag and be only '-'
        if (argv_length <= 1) {
            return NULL;
        }
        // Found the 'f' in -efg
        if ((no_space = strchr(&argv[0][1], flag_in[0])) != NULL) {
            // If the following is a value, we return the value
            if (p == PTR_FOUND_VALUE) {
                return argv[1];
            }
            ++no_space; // Increasing
            if (buffer != NULL) sprintf(buffer, "%s", no_space);
            // If is mandatory but there is nothing to interpret as a value
            return (is_mandatory && !strlen(no_space))? NULL: no_space;
        }
        // Is not the flag, cleaning
        if (buffer != NULL) buffer[0] = '\0';
    // Is is value
    } else {
        // If buffer exists
        if (buffer != NULL) {
            if (strlen(buffer) > 0 && is_multiple) {
                // If is multiple, we add the new value to the list
                memmove(buffer + argv_length + 1, buffer, strlen(buffer) + 1);
                memcpy(buffer, argv[0], argv_length);
                buffer[argv_length] = ' ';
            } else {
                sprintf(buffer, "%s", argv[0]);
            }
        }
        return PTR_FOUND_VALUE;  // Returning the number that means the following argument is a value
    }
    return NULL;
}

char *args_get(int argc, char *argv[], const char *flag_in, char *buffer)
{
    char *p = NULL;
    if (argc <= 1 || argv == NULL || argv[0] == NULL || argv[1] == NULL || flag_in == NULL) {
        return NULL;
    }
    // In case of something strange like -flag, return NULL
    p = _args_get(argc-1, &argv[1], flag_in, buffer);
    return (p != PTR_FOUND_VALUE)? p: NULL;
}