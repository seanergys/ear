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
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <common/sizes.h>
#include <common/output/debug.h>
#include <common/system/folder.h>
#include <metrics/common/pci.h>

// Total of 65.536 bytes
#define N_BUSES     256
#define N_DEVICES   32
#define N_FUNCTIONS 8
#define N_DFS       256
#define N_PCIS      65536
#define N_ALLOCS    32 // Allocated PCIs (is hardcoded avoid multiple scannings)
// 65.536 * (64 + 2 + 2 + 1) = 4.259.840 bytes (4MB)
static struct aux_dfs_s {
    char    path[64];
    ushort  vendor;
    ushort  id;
    ubyte   exists;
} aux_dfs[N_PCIS];

static struct aux_buses_s {
    ubyte exists;
} aux_buses[N_BUSES];

static uint traveled;

static char *strdfs(uint dfs, char *string)
{
    uint device = dfs / 8;
    uint function = dfs % 8;
    sprintf(string, "%.2x.%u", device, function);
    return string;
}

static void pci_travel()
{
    folder_t folder;
    int fds_count = 0;
    char path[64];
    char aux[8];
    int b, d, f;
    int fd;

    for (b = 0; b < N_BUSES; ++b)
    {
        sprintf(path, "/proc/bus/pci/%.2x", b);
        // Discarding bus
        if (state_fail(folder_open(&folder, path))) {
            fds_count += N_DFS;
            continue;
        }
        folder_close(&folder);
        // OK, bus exists
        aux_buses[b].exists = 1;
        // Let's travel into DXF (devices X functions)
        for (d = 0; d < N_DEVICES; ++d) {
        for (f = 0; f < N_FUNCTIONS; ++f)
        {
            sprintf(aux_dfs[fds_count].path, "%s/%.2x.%u", path, d, f);
            //
            debug("Trying to open PCI %s", path);
            if ((fd = open(aux_dfs[fds_count].path, mode)) < 0) {
                if (errno == EACCES) {
                    debug("Exists '%s', but no permissions", path);
                } else {
                    debug("Exists '%s', but error %s", path, strerror(errno));
                }
                fds_count += 1;
                continue;
            }
            aux_dfs[fds_count].exists = 1;
            aux_dfs[fds_count].index  = (d * N_FUNCTIONS) + f;
            // Reading vendor and device id
            pread(fd, &aux_dfs[fds_count].vendor, sizeof(ushort), 0);
            pread(fd, &aux_dfs[fds_count].id,     sizeof(ushort), 2);
            // 1 by 1
            fds_count += 1;
            close(fd);
        }} //DXF
    }
}

static state_t pci_scan_all()
{
    return EAR_SUCCESS;
}

static state_t pci_scan_dfs(ushort vendor, ushort *ids, char **dfs, mode_t mode, pci_t **pcis, uint *pcis_count)
{
    uint ids_count = 0;
    uint dfs_count = 0;
    int b, d, f, i, fd;
    uint idx_dfs[N_DFS];

    if (!traveled) {
        pci_travel();
        traveled = 1;
    }
    // Allocating memory
    *pcis = calloc(N_ALLOCS, sizeof(pci_t));
    *pcis_count = 0;
    // Counting IDs
    for (i = 0; ids[i] != 0x00; ++i) {
        ids_count++;
    }
    // Counting DFS
    for (i = 0; dfs[i] != NULL; ++i) {
        dfs_count++;
    }
    // Converting DFs in indexes

    // Travelling again
    for (b = 0; b < N_BUSES; ++b) {
        if (!aux_buses[b].exists) {
            fds_count += N_DFS;
            continue;
        }
        for (d = 0; d < dfs_count; ++d) {
            if (aux_dfs[idx_dfs[d]].vendor != vendor) {
                continue;
            }
            for (i = 0; i < ids_count; ++i) {
                if (aux_dfs[idx_dfs[d]].id == ids[i]) {
                    break;
                }
            }
        }
        fds_count += N_DFS;
    }
    if (*pcis_count == 0) {
        return_msg(EAR_ERROR, "No PCIs found");
    }
    return EAR_SUCCESS;
}

state_t pci_scan_close(pci_t **pcis, uint *pcis_count)
{
	int p;
	for (p = 0; p < *pcis_count; ++p) {
		debug("%s closed with FD %d", (*pcis)[p].path, (*pcis)[p].fd);
		close((*pcis)[p].fd);
		(*pcis)[p].mode = 0;
		(*pcis)[p].fd = -1;
	}
	*pcis_count = 0;
	return EAR_SUCCESS;
}

state_t pci_read(pci_t *pci, void *buffer, size_t size, off_t addr)
{
	ssize_t psize;
	errno = 0;
	if ((psize = pread(pci->fd, buffer, size, addr)) != size) {
		debug("%s read %ld bytes but expected %lu (errno %d)", pci->path, psize, size, errno);
		if (errno == 0) {
			return_msg(EAR_ERROR, "undefined error");
		} else {
			return_msg(EAR_ERROR, strerror(errno));
		}
	}
	return EAR_SUCCESS;
}

state_t pci_write(pci_t *pci, const void *buffer, size_t size, off_t addr)
{
	if (pwrite(pci->fd, buffer, size, addr) != size) {
		return_msg(EAR_ERROR, strerror(errno));
	}
	return EAR_SUCCESS;
}

state_t pci_mwrite32(pci_t *pcis, uint pcis_count, const uint *buffer, off_t *addrs, uint addrs_count)
{
	int p, a;
	for (p = 0; p < pcis_count; ++p) {
		for (a = 0; a < addrs_count; ++a) {
#if SHOW_DEBUGS
			uint written = pwrite(pcis[p].fd, &buffer[a], sizeof(uint), addrs[a]);
			debug("PCI%d (fd %d), in address %x written value 0x%x (%ld bytes)",
				p, pcis[p].fd, addrs[a], buffer[a], written);
#else
            pwrite(pcis[p].fd, &buffer[a], sizeof(uint), addrs[a]);
#endif
		}
	}
	return EAR_SUCCESS;
}

state_t pci_mmio_map(addr_t addr, void **p)
{
    void *maddr;
	int fd;

	if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
		return_print(EAR_ERROR, "while opening /dev/mem: %s", strerror(errno));
	}
	if ((maddr = mmap(0, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, PAGE_MASK(addr))) == MAP_FAILED) {
		return_print(EAR_ERROR, "while mapping /dev/mem: %s", strerror(errno));
	}
    *p = (void *) (((addr_t) maddr) + (addr - PAGE_MASK(addr)));
    debug("Mapped at 0x%lx and size 0x%x", addr, PAGE_SIZE);
    #if 0
	if ((*p = ioremap(addr, size)) == NULL) {
		return_msg(EAR_ERROR, strerror(errno));
	}
	#endif
	return EAR_SUCCESS;
}

state_t pci_mmio_unmap(void *p)
{
	//iounmap(p);
	return EAR_SUCCESS;
}
