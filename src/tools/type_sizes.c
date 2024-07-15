/*  */


// gcc -Isrc -o types_tests type_tests.c
// gcc -I.. -DWF_SUPPORT=1 -o type_sizes type_sizes.c
// gcc -I.. -DWF_SUPPORT=0 -o type_sizes type_sizes.c
#include <stdio.h>
#include <stdlib.h>
#include <common/types/job.h>
#include <common/types/application.h>
#include <common/types/loop.h>



void main(int argc,char *argv[])
{
	printf("Loop size %u\n", sizeof(loop_t));
	printf("Application size %u\n", sizeof(application_t));
	printf("Job size %u\n", sizeof(job_t));
}
