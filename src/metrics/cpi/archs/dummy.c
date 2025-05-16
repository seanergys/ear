/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

// #define SHOW_DEBUGS 1

#include <common/output/debug.h>
#include <metrics/cpi/archs/dummy.h>

void cpi_dummy_load(topology_t *tp, debug_cbs_f cb, cpi_ops_t *ops)
{
    apis_put(ops->unload  , cpi_dummy_unload  );
    apis_put(ops->get_info, cpi_dummy_get_info);
    apis_put(ops->read    , cpi_dummy_read    );
}

void cpi_dummy_unload()
{
}

void cpi_dummy_get_info(apinfo_t *info)
{
    info->api = API_DUMMY;
}

state_t cpi_dummy_read(cpi_t *ci)
{
    memset(ci, 0, sizeof(cpi_t));
    return EAR_SUCCESS;
}