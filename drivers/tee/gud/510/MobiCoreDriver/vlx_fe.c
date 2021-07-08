// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2020 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifdef CONFIG_VLX_HYP

#include <linux/irq.h>

#include "mci/mciiwp.h"		/* struct interworld_session */
#include "main.h"
#include "client.h"
#include "iwp.h"
#include "mcp.h"
#include "vlx_common.h"
#include "vlx_fe.h"
#include "vrpc.h"
#include "mmu_internal.h"

static struct {
	int				(*start)(void);
	struct tee_vlx_fe		*vlx_fe;
#ifdef MC_USE_VLX_PMEM
	int				pmem_free_pages[MC_VLX_PMEM_PAGES];
	/* Current index into the pmem_free_pages tab */
	int				pmem_index;
#endif /* MC_USE_VLX_PMEM */
} l_ctx;

struct vlx_fe_map {
	struct protocol_fe_map		map;
#ifdef MC_USE_VLX_PMEM
	/* PMEM: PMD and PTE for FE */
	/* Contains phys @ of ptes tables */
	union mmu_table			pmd_table;
	/* Array of pages that hold buffer ptes*/
	union mmu_table			pte_tables[PMD_ENTRIES_MAX];
	/* Actual number of ptes tables */
	size_t				nr_pmd_entries;
#else /* MC_USE_VLX_PMEM */
	int				dom_id;
#endif /* MC_USE_VLX_PMEM */
};

#ifdef MC_USE_VLX_PMEM

static int pmem_init(struct tee_vlx_fe *vlx_fe)
{
	NkPhAddr plink = 0;
	/* PMEM Physical Base Address */
	NkPhAddr paddr;
	int i = 0;
	int ret = -ENXIO;

	plink = nkops.nk_vlink_lookup("vtrustonic", plink);
	if (!plink) {
		ret = -ENOMEM;
		mc_dev_err(ret, "nk_vlink_lookup failed");
		return ret;
	}
	mc_dev_devel("Link paddr %lx", plink);

	paddr = nkops.nk_pmem_alloc(plink, vlx_fe->vm_info->no,
				    MC_VLX_PMEM_PAGES * PAGE_SIZE);
	if (!paddr) {
		ret = -ENOMEM;
		mc_dev_err(ret, "nk_pmem_alloc failed");
		return ret;
	}
	mc_dev_devel("PMEM paddr %lx", paddr);

	vlx_fe->pmem_vaddr = nkops.nk_mem_map(paddr,
					      MC_VLX_PMEM_PAGES * PAGE_SIZE);
	if (!vlx_fe->pmem_vaddr) {
		ret = -ENOMEM;
		mc_dev_err(ret, "nk_mem_map failed");
		return ret;
	}
	mc_dev_devel("PMEM vaddr %p", vlx_fe->pmem_vaddr);

	/* Initialize the memory manager */
	for (i = 0; i < MC_VLX_PMEM_PAGES; i++)
		l_ctx.pmem_free_pages[i] = i;
	l_ctx.pmem_index = 0;

	return 0;
}

/*
 * Memory Manager:
 * Return the virtual address of the page into the PMEM
 */
static void *pmem_alloc_page(void)
{
	/* PMEM Virtual Base Address */
	char *base = (char *)l_ctx.vlx_fe->pmem_vaddr;
	/* Page Number of the current page */
	int page_no = l_ctx.pmem_free_pages[l_ctx.pmem_index];
	/* Virtual Address of the page */
	void *virt_addr = base + (page_no * PAGE_SIZE);

	/* Check the index */
	if (l_ctx.pmem_index >= MC_VLX_PMEM_PAGES)
		return NULL;

	l_ctx.pmem_index++;

	/* Cleanup the pointed page */
	memset(virt_addr, 0x0, PAGE_SIZE);

	return virt_addr;
}

/*
 * Memory Manager:
 * Free the page at the virtual address in arg
 */
static void pmem_free_page(void *virt_addr)
{
	/* PMEM Virtual Base Address */
	char *base = (char *)l_ctx.vlx_fe->pmem_vaddr;
	/* Page Number of the current page */
	int page_no = ((char *)virt_addr - base) / PAGE_SIZE;

	/* Check the index */
	WARN_ON(!l_ctx.pmem_index);

	l_ctx.pmem_index--;

	l_ctx.pmem_free_pages[l_ctx.pmem_index] = page_no;
}

/*
 * Memory Manager:
 * Convert the virtual address in arg to an offset
 */
static u64 pmem_virt_to_offset(void *virt_addr)
{
	char *v_addr = (char *)virt_addr;
	char *base = (char *)l_ctx.vlx_fe->pmem_vaddr;

	return v_addr - base;
}

#else /* MC_USE_VLX_PMEM */

static inline int pmem_init(struct tee_vlx_fe *vlx_fe)
{
	return 0;
}

static int vlx_vm_mem_grant(int dom_id, unsigned long phys_addr)
{
	int ret = -1;
	unsigned int flags =
		HYP_VM_MEM_GRANT_FLAGS_READ | HYP_VM_MEM_GRANT_FLAGS_CPU;
	NkMemMap rgn[1];

	rgn[0].addr = phys_addr;
	rgn[0].size = PAGE_SIZE;

	ret = hyp_call_vm_mem_grant(dom_id, flags, 1, &rgn[0]);
	if (ret < 0)
		mc_dev_err(ret, "hyp_call_vm_mem_grant failed");

	return ret;
}

static int vlx_vm_mem_release(int dom_id, unsigned long phys_addr)
{
	int ret = -1;
	unsigned int flags =
		HYP_VM_MEM_GRANT_FLAGS_READ | HYP_VM_MEM_GRANT_FLAGS_CPU;
	NkMemMap rgn = {
		.addr = phys_addr,
		.size = PAGE_SIZE,
	};

	ret = hyp_call_vm_mem_deny(dom_id, flags, 1, &rgn);
	if (ret < 0)
		mc_dev_err(ret, "hyp_call_vm_mem_deny failed");

	return ret;
}

#endif /* MC_USE_VLX_PMEM */

/* Maps create, release and delete */
static void vlx_fe_map_release_pmd_ptes(
	struct protocol_fe_map *map,
	const struct tee_protocol_buffer *buffer)
{
	int i;
	struct vlx_fe_map *vlx_map = container_of(map, struct vlx_fe_map, map);
	struct mcp_buffer_map b_map;
	/* Contains physical addresses of PTEs table */
	union mmu_table *pmd_table;
	/* Contains physical addresses of pages */
	union mmu_table *pte_tables;
	/* Number of PTEs */
	unsigned long nr_pte_tables;

	/* Get pmd_table and pte_tables from the mmu */
	pmd_table = &vlx_map->map.mmu->pmd_table;
	pte_tables = vlx_map->map.mmu->pte_tables;

	if (!vlx_map->map.refs_shared)
		return;

	/* Convert vlx_map->map.mmu (tee_mmu) into b_map (mcp_buffer_map) */
	tee_mmu_buffer(vlx_map->map.mmu, &b_map);

#ifdef MC_USE_VLX_PMEM
	pmem_free_page(vlx_map->pmd_table.addr);
	mc_dev_devel("released PMEM PMD ipa %p", vlx_map->pmd_table.addr);
	vlx_map->pmd_table.addr = NULL;
#else /* MC_USE_VLX_PMEM */
	/* Release PMD and decrease the count of shared pages */
	vlx_vm_mem_release(vlx_map->dom_id, b_map.addr);
	mc_dev_devel("released PMD ipa %llx", b_map.addr);
#endif /* MC_USE_VLX_PMEM */

	vlx_map->map.refs_shared--;

	/* Deduce the number of PTEs */
	nr_pte_tables = (b_map.mmu->nr_pages + PTE_ENTRIES_MAX - 1) /
		PTE_ENTRIES_MAX;

	/* Loop over the number of PTEs */
	for (i = 0; i < nr_pte_tables; i++) {
		if (!vlx_map->map.refs_shared)
			break;

#ifdef MC_USE_VLX_PMEM
		pmem_free_page(vlx_map->pte_tables[i].addr);
		mc_dev_devel("released PMEM PMD ipa %p",
			     vlx_map->pte_tables[i].addr);
		vlx_map->pte_tables[i].addr = NULL;
#else /* MC_USE_VLX_PMEM */
		/* Release PTE and decrease the count of shared pages */
		vlx_vm_mem_release(vlx_map->dom_id, pmd_table->entries[i]);
		mc_dev_devel("released PTE %d ipa %llx",
			     i, pmd_table->entries[i]);
#endif /* MC_USE_VLX_PMEM */

		vlx_map->map.refs_shared--;
	}
}

static void vlx_fe_map_release(struct protocol_fe_map *map)
{
	/* Check memory leak */
	if (map->refs_shared) {
		mc_dev_err(-EUCLEAN, "leak detected: still shared %d",
			   map->refs_shared);
		/* Do not free the map, so the error can be spotted */
		return;
	}

	mc_dev_devel("freed map %p: nr_pages %lu, nr_pte_tables %ld",
		     map, map->mmu->nr_pages, map->mmu->nr_pmd_entries);
	kfree(map);
	atomic_dec(&g_ctx.c_vm_maps);
}

static void vlx_fe_map_delete(void *arg)
{
	struct protocol_fe_map *map = arg;

	vlx_fe_map_release(map);
}

static struct protocol_fe_map *vlx_fe_map_create(
	struct tee_protocol_buffer *buffer,
	const struct mcp_buffer_map *b_map,
	struct protocol_fe *pfe)
{
	struct vlx_fe_map *vlx_map;
	/* Contains physical addresses of PTEs table */
	union mmu_table *pmd_table;
	/* Contains physical addresses of pages */
	union mmu_table *pte_tables;
	/* Deduce the number of PTEs and the number of pages */
	unsigned long nr_pte_refs =
		(b_map->mmu->nr_pages + PTE_ENTRIES_MAX - 1) / PTE_ENTRIES_MAX;
	int ret, i;

	vlx_map = kzalloc(sizeof(*vlx_map), GFP_KERNEL);
	if (!vlx_map)
		return ERR_PTR(-ENOMEM);
	atomic_inc(&g_ctx.c_vm_maps);

	mc_dev_devel("number of PTEs %lx", nr_pte_refs);
	mc_dev_devel("number of pages %lx", b_map->mmu->nr_pages);

	/* b_map describes the PMD which contains pointers to PTE tables */
	pmd_table = &b_map->mmu->pmd_table;
	pte_tables = b_map->mmu->pte_tables;

	/* Update vlx_map (protocol_fe_map) */
	vlx_map->map.mmu = b_map->mmu;

#ifdef MC_USE_VLX_PMEM
	vlx_map->pmd_table.addr = pmem_alloc_page();
	if (!vlx_map->pmd_table.addr) {
		ret = -ENOMEM;
		goto err_mem_grant;
	}
	mc_dev_devel("allocated PMEM PMD ipa %p", vlx_map->pmd_table.addr);
#else /* MC_USE_VLX_PMEM */
	vlx_map->dom_id = vrpc_peer_id(
		container_of(pfe, struct tee_vlx_fe, pfe)->fe2be_vrpc);

	/* Make the PMD page accessible to the back-end */
	ret = vlx_vm_mem_grant(vlx_map->dom_id, b_map->addr);
	if (ret < 0) {
		mc_dev_err(ret, "vlx_vm_mem_grant failed");
		goto err_mem_grant;
	}
	mc_dev_devel("granted PMD ipa %llx", b_map->addr);
#endif /* MC_USE_VLX_PMEM */
	/* Increment the count of shared pages */
	vlx_map->map.refs_shared++;

	/* Loop over the number of PTEs */
	for (i = 0; i < nr_pte_refs; i++) {
#ifdef MC_USE_VLX_PMEM
		vlx_map->pte_tables[i].addr = pmem_alloc_page();
		if (!vlx_map->pte_tables[i].addr) {
			ret = -ENOMEM;
			goto err_mem_grant;
		}

		memcpy(vlx_map->pte_tables[i].addr, pte_tables[i].addr,
		       PAGE_SIZE);
		vlx_map->pmd_table.entries[i] =
			pmem_virt_to_offset(vlx_map->pte_tables[i].addr);
		mc_dev_devel("allocated PMEM PTE ipa %p",
			     vlx_map->pte_tables[i].addr);
#else /* MC_USE_VLX_PMEM */
		/*
		 * Make the PTE page accessible to the back-end, release
		 * eveything on failure
		 */
		ret = vlx_vm_mem_grant(vlx_map->dom_id, pmd_table->entries[i]);
		if (ret < 0) {
			mc_dev_err(ret, "vlx_vm_mem_grant failed");
			goto err_mem_grant;
		}
		mc_dev_devel("granted PTE %d ipa %llx",
			     i, pmd_table->entries[i]);
#endif /* MC_USE_VLX_PMEM */

		/* Increment the count of shared pages */
		vlx_map->map.refs_shared++;
	}

	/* Update the buffer (tee_protocol_buffer) with: */
#ifdef MC_USE_VLX_PMEM
	buffer->pmd_ref = pmem_virt_to_offset(vlx_map->pmd_table.addr);
#else /* MC_USE_VLX_PMEM */
	buffer->pmd_ref = b_map->addr;		/* PMD address */
#endif/* MC_USE_VLX_PMEM */
	buffer->addr = (uintptr_t)b_map->mmu;	/* MMU */
	buffer->offset = b_map->offset;
	buffer->length = b_map->length;
	buffer->flags = b_map->flags;

	/* Auto-delete */
	vlx_map->map.deleter.object = &vlx_map->map;
	vlx_map->map.deleter.delete = vlx_fe_map_delete;
	tee_mmu_set_deleter(b_map->mmu, &vlx_map->map.deleter);

	mc_dev_devel("granted map %p: nr pages %lu, nr pte %ld",
		     &vlx_map->map, vlx_map->map.mmu->nr_pages,
		     vlx_map->map.mmu->nr_pmd_entries);

	return &vlx_map->map;

err_mem_grant:
	vlx_fe_map_release(&vlx_map->map);
	return ERR_PTR(-ENOMEM);
}

/* Call the BE server */
static int vlx_call_be(struct protocol_fe *pfe)
{
	struct tee_vlx_fe *vlx_fe = container_of(pfe, struct tee_vlx_fe, pfe);
	struct vrpc_t *vrpc = vlx_fe->fe2be_vrpc;
	struct fe2be_data *data = vlx_fe->pfe.fe2be_data;
	vrpc_size_t size;
	int ret;

	/* Try to re-open client if closed */
	if (!vlx_fe->client_is_open) {
		ret = vrpc_client_open(vrpc, NULL, NULL);
		if (ret) {
			mc_dev_err(ret, "Failed to open BE connection");
			return ret;
		}

		vlx_fe->client_is_open = true;
		mc_dev_info("Connection to BE re-open");
	}

	vlx_fe->pfe.fe2be_data->id = vlx_fe->pfe.fe_cmd_id;
	data->cmd = vlx_fe->pfe.fe2be_data->cmd;
	size = sizeof(*data);
	ret = vrpc_call_non_interruptible(vrpc, &size);
	if (ret) {
		mc_dev_err(ret, "Connection to BE broken, close");
		vrpc_close(vrpc);
		vlx_fe->client_is_open = false;
		return ret;
	}

	vlx_fe->pfe.fe2be_data->cmd = TEE_FE_NONE;
	vlx_fe->pfe.fe2be_data->id = 0;
	return 0;
}

/* Callback of the BE client */

static vrpc_size_t vlx_fe_callback(void *cookie, vrpc_size_t size)
{
	struct tee_vlx_fe *vlx_fe = cookie;

	if (size != sizeof(*vlx_fe->pfe.be2fe_data))
		return 0;

	mc_dev_devel("BE -> FE command %u id %u", vlx_fe->pfe.be2fe_data->cmd,
		     vlx_fe->pfe.be2fe_data->id);

	vlx_fe->pfe.be2fe_data->cmd_ret = protocol_fe_dispatch(&vlx_fe->pfe);

	if (vlx_fe->pfe.be2fe_data->cmd_ret)
		mc_dev_err(vlx_fe->pfe.be2fe_data->cmd_ret,
			   "BE -> FE result %u id %u",
			   vlx_fe->pfe.be2fe_data->cmd,
			   vlx_fe->pfe.be2fe_data->id);
	else
		mc_dev_devel("BE -> FE result %u id %u ret %d",
			     vlx_fe->pfe.be2fe_data->cmd,
			     vlx_fe->pfe.be2fe_data->id,
			     vlx_fe->pfe.be2fe_data->cmd_ret);

	return sizeof(*vlx_fe->pfe.be2fe_data);
}

/* Device */

/* Will be called when/if the connection is established (BE is ready) */
static void vlx_fe_start(void *cookie)
{
	struct tee_vlx_fe *vlx_fe = cookie;
	int ret;

	mc_dev_info("Connection to BE open");
	vlx_fe->client_is_open = true;

	/* Mutex is not locked because vlx_fe is only local here,
	 * but to pass the WARN_ON check in the call function,
	 * protocol_busy is set before the call.
	 */
	vlx_fe->pfe.protocol_busy = true;

	/* Set the cmd for the BE with the FE's name*/
	vlx_fe->pfe.fe2be_data->cmd = TEE_CONNECT;
	strlcpy(vlx_fe->pfe.fe2be_data->server_name,
		vlx_fe->vm_info->callback_channel_name,
		sizeof(vlx_fe->pfe.fe2be_data->server_name));
	vlx_fe->pfe.fe2be_data->server_name[
		sizeof(vlx_fe->pfe.fe2be_data->server_name) - 1] = '\0';

	/* Call the BE Server with the FE name to create the BE Client */
	ret = vlx_call_be(&vlx_fe->pfe);
	if (ret)
		return;

	vlx_fe->pfe.protocol_busy = false;

	/* Communication is in place, proceed with start */
	l_ctx.vlx_fe = vlx_fe;
	if (l_ctx.start())
		return;

	mc_dev_devel("FE client initialized");
}

static int vlx_fe_early_init(int (*probe)(void), int (*start)(void))
{
	l_ctx.start = start;
	return 0;
}

static int vlx_fe_probe(void)
{
	struct tee_vlx_fe *vlx_fe;
	/* To create the server */
	struct vrpc_t *vrpc_fe;
	/* To create the client */
	struct vrpc_t *vrpc_be;
	int ret;

	/* Create vlx_fe */
	vlx_fe = tee_vlx_fe_create(tee_vlx_find_vm_info(nkops.nk_id_get()));
	if (IS_ERR(vlx_fe)) {
		ret = PTR_ERR(vlx_fe);
		mc_dev_err(ret, "Failed to create vlx_fe");
		return ret;
	}

	/* Create FE server for the BE client */
	vrpc_fe = vrpc_server_lookup(vlx_fe->vm_info->callback_channel_name,
				     NULL);
	if (!vrpc_fe) {
		ret = -ENODEV;
		mc_dev_err(ret, "Failed to lookup callback channel name: %s",
			   vlx_fe->vm_info->callback_channel_name);
		return ret;
	}

	vlx_fe->be2fe_vrpc = vrpc_fe;
	vlx_fe->pfe.be2fe_data = vrpc_data(vrpc_fe);

	if (vrpc_maxsize(vrpc_fe) < sizeof(*vlx_fe->pfe.be2fe_data)) {
		ret = -ENOMEM;
		mc_dev_err(ret, "Not enough VRPC FE server shared memory");
		goto err;
	}

	ret = vrpc_server_open(vrpc_fe, vlx_fe_callback, vlx_fe, 0);
	if (ret) {
		mc_dev_err(ret, "VRPC FE server open has failed!");
		goto err;
	}

	mc_dev_devel("FE server initialized");

	/* Create FE client for the BE server */
	vrpc_be = vrpc_client_lookup(vlx_fe->vm_info->call_channel_name, 0);
	if (!vrpc_be) {
		ret = -ENODEV;
		mc_dev_err(ret, "No vrpc (client) link!");
		goto err;
	}

	vlx_fe->fe2be_vrpc = vrpc_be;
	vlx_fe->pfe.fe2be_data = vrpc_data(vrpc_be);

	if (vrpc_maxsize(vrpc_be) < sizeof(*vlx_fe->pfe.fe2be_data)) {
		ret = -ENOMEM;
		mc_dev_err(ret, "Not enough VRPC FE server shared memory");
		goto err;
	}

	ret = vrpc_client_open(vrpc_be, vlx_fe_start, vlx_fe);
	if (ret) {
		mc_dev_err(ret, "VRPC BE client open has failed!");
		goto err;
	}

	ret = pmem_init(vlx_fe);
	if (ret)
		goto err;

	return 0;

err:
	tee_vlx_fe_put(vlx_fe);
	return ret;
}

static void vlx_fe_exit(void)
{
	tee_vlx_fe_put(l_ctx.vlx_fe);
}

static struct tee_protocol_fe_call_ops fe_call_ops = {
	.call_be = vlx_call_be,
	.fe_map_create = vlx_fe_map_create,
	.fe_map_release_pmd_ptes = vlx_fe_map_release_pmd_ptes,
};

static struct tee_protocol_ops protocol_ops = {
	.name = "VLX FE",
	.early_init = vlx_fe_early_init,
	.init = vlx_fe_probe,
	.exit = vlx_fe_exit,
	.fe_call_ops = &fe_call_ops,
};

struct tee_protocol_ops *vlx_fe_check(void)
{
	const struct tee_vlx_vm_info *vm_info;

	/* Get the vm number to detect if this is a VLX FE */
	vm_info = tee_vlx_find_vm_info(nkops.nk_id_get());
	if (!vm_info || !vm_info->call_channel_name)
		return NULL;

	return &protocol_ops;
}

#endif /* CONFIG_VLX_HYP */
