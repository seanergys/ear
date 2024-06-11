/*
*
* This program is part of the EAR software.
*
* EAR provides a dynamic, transparent and ligth-weigth solution for
* Energy management. It has been developed in the context of the
* Barcelona Supercomputing Center (BSC)&Lenovo Collaboration project.
*
* 
* BSC Contact   mailto:ear-support@bsc.es
* 
*
* EAR is an open source software, and it is licensed under both the BSD-3 license
* and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
* and COPYING.EPL files.
*/

#ifndef _EDCMONTAG_CONF_H
#define _EDCMONTAG_CONF_H

#include <common/config.h>
#include <common/states.h>
#include <common/types/generic.h>

typedef enum {
    storage     = 0,
    network     = 1,
    management  = 2,
    others      = 3,
} pdu_t;

#define pdu_type_to_str(t) ((t == storage)?"storage":((t == network)?"network":((t == management)?"management":"others")))

typedef struct edcmon
{
    char  id[GENERIC_NAME];
		char  host[GENERIC_NAME];
    pdu_t type;
	  char  sensors_list[1024];
	  char  pdu_list[1024];
} edcmon_t;

state_t EDCMON_token(char *token);
state_t EDCMON_parse_token(edcmon_t **edcmon_i, unsigned int *num_edcmon_i, char *line);
void print_edcmon_tags_conf(edcmon_t *edcmon_tag, uint i);



#endif
