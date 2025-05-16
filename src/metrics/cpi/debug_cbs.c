/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#include <stdio.h>
#include <stdarg.h>

void basic_print(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int result = vprintf(fmt, args);
    va_end(args);
}