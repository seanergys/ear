/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#ifndef DEBUG_CBS_H
#define DEBUG_CBS_H

typedef void (*debug_cbs_f) (char *fmt, ...);

#define rdebug(...) if (dcb != NULL) { dcb(__VA_ARGS__); }

void basic_print(char *fmt, ...);

#endif // DEBUG_CBS_H