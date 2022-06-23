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

// Fake functions
#define main(argc, argv) \
    __main(argc, argv)

int isst_daemon(int debug_mode, int poll_interval, int no_daemon)
{
    return 0;
}

// Including source files
#include "isst-config.c"
#include "isst-core.c"
#include "isst-display.c"

int isst_main(int argc, char *argv[])
{
    dump_clos_info(0);
    return 0;
    //return __main(argc, argv);
}