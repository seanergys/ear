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
* This file is licensed under both the BSD-3 license for individual/non-commercial
* use and EPL-1.0 license for commercial use. Full text of both licenses can be
* found in COPYING.BSD and COPYING.EPL files.
*/

#include <stdlib.h>
#include <metrics/frequency/imc/dummy.h>

state_t ifreq_dummy_init(topology_t *tp)
{
	return EAR_SUCCESS;
}

state_t ifreq_dummy_dispose()
{
	return EAR_SUCCESS;
}

state_t ifreq_dummy_read(freq_imc_t *ef)
{
	if ((ef != NULL)  && (ef->data != NULL)) ef->data = 0;
	return EAR_SUCCESS;
}

state_t ifreq_dummy_read_diff(freq_imc_t *ef2, freq_imc_t *ef1, ulong *freqs, ulong *average)
{
	if (freqs   != NULL) *freqs   = 0;
	if (average != NULL) *average = 0;
	return EAR_SUCCESS;
}

state_t ifreq_dummy_read_copy(freq_imc_t *ef2, freq_imc_t *ef1, ulong *freqs, ulong *average)
{
	return ifreq_dummy_read_diff(ef2, ef1, freqs, average);
}

state_t ifreq_dummy_data_alloc(freq_imc_t *ef, ulong *freqs[], ulong *freqs_count)
{
	if (ef != NULL) ef->data = calloc(1,sizeof(ulong));
	return EAR_SUCCESS;
}

state_t ifreq_dummy_data_count(uint *count)
{
	if (count != NULL) *count = sizeof(ulong);
	return EAR_SUCCESS;
}

state_t ifreq_dummy_data_copy(freq_imc_t *ef_dst, freq_imc_t *ef_src)
{
	return EAR_SUCCESS;
}

state_t ifreq_dummy_data_free(freq_imc_t *ef, ulong *freqs[])
{
	return EAR_SUCCESS;
}

state_t ifreq_dummy_data_diff(freq_imc_t *ef2, freq_imc_t *ef1, ulong *freqs, ulong *average)
{
	return ifreq_dummy_read_diff(ef2, ef1, freqs, average);
}

state_t ifreq_dummy_data_print(ulong *freqs, ulong *average)
{
	return EAR_SUCCESS;
}
