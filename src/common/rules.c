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

#include <common/config.h>
#include <common/rules.h>
#include <common/output/verbose.h>
#include <common/types/generic.h>
#include <common/types/configuration/cluster_conf.h>


static int     num_rules = 0;
static rule_t  *ruleset = NULL;


state_t rule_add(rule_t *rule)
{
    ruleset = realloc(ruleset, sizeof(rule_t)*num_rules + 1);
    
    memcpy(&ruleset[num_rules], rule, sizeof(rule_t));

    num_rules++;

    return EAR_SUCCESS;
}

state_t rule_add_list()
{
    return EAR_SUCCESS;
}

state_t rule_filter(char *filter, enum domain domain)
{
    int i;
    for (i = 0; i < num_rules; i++) {
        //if the rule is of the same type and name, keep it
        if (domain == ruleset[i].domain) {
            if (!strcmp(filter, ruleset[i].domain_id)) {
                continue;
            }
        }
        num_rules --;
        ruleset[i] = ruleset[num_rules]; //move the last one in this position 
        ruleset = realloc(ruleset, sizeof(rule_t)*num_rules); //remove the extra element

        i--; //re-do this position (which is now the last one)
    }

    return EAR_SUCCESS;
}

state_t rule_check(time_t timestamp, uint64_t power)
{
    return EAR_SUCCESS;
}
