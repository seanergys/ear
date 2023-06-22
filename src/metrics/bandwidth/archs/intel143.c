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

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <metrics/common/pci.h>
#include <common/output/debug.h>
#include <metrics/bandwidth/archs/intel143.h>

static ushort sap_ids[2] = { 0x3251, 0x00 };
static char  *sap_dfs[2] = { "00.1", NULL };
//
static pci_t *pcis;
static uint   pcis_count;
static uint   imc_maps_count;
static uint   imc_ctrs_count;
static void **imc_maps;
static uint  *imc_ctrs;

static state_t load_icelake()
{
	addr_t addr_hi;
	addr_t addr_lo;
	addr_t addr_fi;
	uint p, i, c;
	state_t s;

	imc_maps_count = 4 * pcis_count; // 4 IMCs * #PCIs (or sockets)
	imc_ctrs_count = imc_maps_count * 2; // #MAPs * 2 counters (RD and WR)
	imc_maps = calloc(imc_maps_count, sizeof(void *));
	imc_ctrs = calloc(imc_ctrs_count, sizeof(uint));
    debug("#IMC maps: %u", imc_maps_count);
    debug("#IMC ctrs: %u", imc_ctrs_count);

	for (p = c = 0; p < pcis_count; ++p) {
		addr_hi = 0x00;
        // MMIO_BASE
		if (state_fail(s = pci_read(&pcis[p], &addr_hi, sizeof(uint), 0xD0))) {
			// Clean PCIs
			return s;
		}
		addr_hi = (addr_hi & 0x1FFFFFFF) << 23;
		debug("PCI%u base address %llx", p, addr_hi);
        // 4 IMCs
		for (i = 0; i < 4; ++i) {
			addr_lo = 0x00;
            // MEMx_BAR (0xD8, 0xDC, 0xE0 and 0xE4 in the document, which is the same as 0xD8+(i*0x04).
	    	if (state_fail(s = pci_read(&pcis[p], &addr_lo, sizeof(uint), 0xD8+(i*0x04)))) {
				// Clean PCIs
				return s;
			}
			addr_lo = (addr_lo & 0x7FF) << 12;
            // Free running counters base address
			addr_fi = (addr_hi | addr_lo) + 0x2318;
			if (state_ok(s = pci_mmio_map(addr_fi, &imc_maps[(p*4)+i]))) {
                debug("IMC%u address physical = 0x%llx (0x%llx | 0x%llx)", (p*4)+i, addr_fi, addr_hi, addr_lo);
                debug("IMC%u address virtual  = 0x%p", (p*4)+i, imc_maps[(p*4)+i]);
                // Filling RD and WR counters
                imc_ctrs[c++] = 0x00;
                imc_ctrs[c++] = 0x08;
			}
		}
	}
	return EAR_SUCCESS;
}

state_t bwidth_intel143_load(topology_t *tp, bwidth_ops_t *ops)
{
	state_t s;
	// If AMD or model previous to Intel Haswell
	if (tp->vendor == VENDOR_AMD || tp->model != MODEL_SAPPHIRE_RAPIDS) {
		return_msg(EAR_ERROR, Generr.api_incompatible);
	}
	debug("detected Intel SAPPHIRE RAPIDS");
	if (state_fail(s = pci_scan(0x8086, sap_ids, sap_dfs, O_RDONLY, &pcis, &pcis_count))) {
		serror("pci_scan");
		return s;
	}
	// It is shared by all Intel's architecture from Haswell to Skylake
	debug("PCIs count: %d", pcis_count);
	
    if (state_fail(s = load_icelake())) {
		// serror("load_icelake");
		return s;
	}
	// In the future it can be initialized in read only mode
	apis_put(ops->get_info, bwidth_intel143_get_info);
	apis_put(ops->init,     bwidth_intel143_init);
	apis_put(ops->dispose,  bwidth_intel143_dispose);
	apis_put(ops->read,     bwidth_intel143_read);

	return EAR_SUCCESS;
}

BWIDTH_F_GET_INFO(bwidth_intel143_get_info)
{
    info->api         = API_INTEL106;
    info->scope       = SCOPE_NODE;
    info->granularity = GRANULARITY_IMC;
    info->devs_count  = imc_ctrs_count+1;
}

state_t bwidth_intel143_init(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t bwidth_intel143_dispose(ctx_t *c)
{
	return EAR_SUCCESS;
}

state_t bwidth_intel143_count_devices(ctx_t *c, uint *devs_count_in)
{
	*devs_count_in = imc_ctrs_count+1;
	return EAR_SUCCESS;
}

state_t bwidth_intel143_read(ctx_t *c, bwidth_t *bw)
{
    addr_t addr0;
    addr_t addr1;
    int i, j;
	
	timestamp_get(&bw[imc_ctrs_count].time);
	for (i = j = 0; i < imc_maps_count; ++i, j+=2) {
		debug("IMC%d: RD DDR addr 0x%p", i, imc_maps[i]);
        addr0 = ((addr_t) imc_maps[i]) + ((addr_t) imc_ctrs[j+0]);
        addr1 = ((addr_t) imc_maps[i]) + ((addr_t) imc_ctrs[j+1]);
		bw[j+0].cas = (ullong) *((ullong *) (addr0));
        bw[j+1].cas = (ullong) *((ullong *) (addr1));
		bw[j+0].cas = bw[j+0].cas & 0x0000ffffffffffff;
        bw[j+1].cas = bw[j+1].cas & 0x0000ffffffffffff;
        debug("IMC%d: %llu %llu cas", i, bw[j+0].cas, bw[j+1].cas);
	}
	return EAR_SUCCESS;
}