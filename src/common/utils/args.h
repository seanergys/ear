/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#ifndef COMMON_UTILS_ARGS_H
#define COMMON_UTILS_ARGS_H
/* clang-format off */

/* Search specific FLAG into ARGC and ARGV, and returns a pointer to the
   the value, to the flag if the value does not exists but is not mandatory,
   or NULL when the flag is unrecognized. The values are also stored in
   BUFFER.

   Allowed flag types:
 	-a, -abc, -aVal
 	--flag, --flag=val, --flag val, --flag val1 val2 val3
   Option sufixes:
 	: value is mandatory.
	* return multiple values in case they are.
   Exceptions:
        - If an argument is '-abc' and you search for '-a', the 'bc' is
	  considered a value. 'getopt()' has the same limitation. */
char *args_get(int argc, char *argv[], const char *flag, char *buffer);

/* clang-format on */
#endif // COMMON_UTILS_ARGS_H
