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

#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#include "main.h"
#include "admin.h"		/* tee_object* */
#include "client.h"		/* Consider other VMs as clients */
#include "mmu.h"
#include "mcp.h"		/* mcp_get_version */
#include "nq.h"
#include "vlx_common.h"
#include "vlx_be.h"
#include "vrpc.h"
#include "mmu_internal.h"

static struct {
	int			(*probe)(void);
	int			(*start)(void);
	struct list_head	vlx_fes;
} l_ctx;

#ifdef MC_USE_VLX_PMEM

static int vlx_vm_mem_alloc(struct tee_vlx_fe *vlx_fe)
{
	NkPhAddr plink = 0;
	NkDevVlink *vlink;
	NkPhAddr paddr;
	int ret = -ENXIO;

	/* Loop for all vlinks named 'vtrustonic' detected */
	while ((plink = nkops.nk_vlink_lookup("vtrustonic", plink)) != 0) {
		if (!plink) {
			mc_dev_err(ret, " nk_vlink_lookup failed");
			return ret;
		}

		vlink = nkops.nk_ptov(plink);

		/* Check if the link found is for the current VM */
		if (vlink->c_id == vlx_fe->vm_info->no) {
			mc_dev_devel("VM %d Link paddr %lx    vaddr %p",
				     vlink->c_id, plink, vlink);

			paddr = nkops.nk_pmem_alloc(
				plink, vlx_fe->vm_info->no,
				MC_VLX_PMEM_PAGES * PAGE_SIZE);
			if (!paddr) {
				ret = -ENOMEM;
				mc_dev_err(ret, "nk_pmem_alloc failed");
				return ret;
			}
			mc_dev_devel("VM %d PMEM paddr %lx",
				     vlink->c_id, paddr);

			vlx_fe->pmem_vaddr = nkops.nk_mem_map(
				paddr, MC_VLX_PMEM_PAGES * PAGE_SIZE);
			if (!vlx_fe->pmem_vaddr) {
				ret = -ENOMEM;
				mc_dev_err(ret, "nk_mem_map failed");
				return ret;
			}
			mc_dev_devel("VM %d PMEM vaddr %p",
				     vlink->c_id, vlx_fe->pmem_vaddr);

			break;
		}
	}
	return 0;
}

static void vlx_vm_mem_read(struct tee_vlx_fe *vlx_fe, void *dest, u64 ref)
{
	char *pmem_vaddr = (char *)vlx_fe->pmem_vaddr;

	/* ref is the offset in our pmem table of pages */
	memcpy(dest, pmem_vaddr + ref, PAGE_SIZE);
}

#else /* MC_USE_VLX_PMEM */

static int vlx_vm_mem_verify(int dom_id, unsigned long phys_addr)
{
	int ret = 0;
	NkMemMap rgn[1];

	rgn[0].addr = phys_addr;
	rgn[0].size = PAGE_SIZE;

	ret = hyp_call_vm_mem_verify(dom_id, 1, &rgn[0]);
	if (ret < 0)
		mc_dev_err(ret, "hyp_call_vm_mem_verify failed");

	return ret;
}

static void vlx_vm_mem_read(struct tee_vlx_fe *vlx_fe, void *dest, u64 ref)
{
	/* ref is the IPA of the page in the client VM */
	NkPhAddr paddr = (NkPhAddr)ref;
	void *vaddr;

	/* Map page */
	vaddr = nkops.nk_mem_map(paddr, PAGE_SIZE);

	/* Copy page content */
	memcpy(dest, vaddr, PAGE_SIZE);

	/* Unmap page */
	nkops.nk_mem_unmap(vaddr, paddr, PAGE_SIZE);
}

#endif /* MC_USE_VLX_PMEM */

/* Maps create and delete */

static void vlx_be_map_delete(struct protocol_be_map *map)
{
	mc_dev_devel("freed vlx map %p", map);
	kfree(map);
	atomic_dec(&g_ctx.c_vm_maps);
}

static struct protocol_be_map *vlx_be_map_create(
	struct tee_protocol_buffer *buffer,
	struct protocol_fe *pfe)
{
	struct protocol_be_map *map;
	struct tee_vlx_fe *vlx_fe = container_of(pfe, struct tee_vlx_fe, pfe);
	/* Contains physical addresses of PTEs table */
	union mmu_table *pmd_table;
	/* Contains physical addresses of pages */
	union mmu_table *pte_tables;
	/* Number of pages and PTEs */
	unsigned long nr_pages = PAGE_ALIGN(buffer->offset + buffer->length) /
		PAGE_SIZE;
	unsigned long nr_pte_refs;
#ifndef MC_USE_VLX_PMEM
	unsigned long nr_pages_left = nr_pages;
	int dom_id = vrpc_peer_id(vlx_fe->fe2be_vrpc);
#endif /* MC_USE_VLX_PMEM */
	int ret = 0, i;

	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (!map)
		return ERR_PTR(-ENOMEM);

	/* Increment debug counter */
	atomic_inc(&g_ctx.c_vm_maps);

	/* Allocate the mmu and increment debug counter */
	map->mmu = tee_mmu_create_and_init();
	if (IS_ERR(map->mmu)) {
		long ptr_err = PTR_ERR(map->mmu);

		kfree(map);
		atomic_dec(&g_ctx.c_vm_maps);
		return ERR_PTR(ptr_err);
	}

	/* Set references to pmd_table and pte_tables */
	pmd_table = &map->mmu->pmd_table;
	pte_tables = map->mmu->pte_tables;

	/* Deduce the number of PTEs and the number of pages */
	nr_pte_refs = (nr_pages + PTE_ENTRIES_MAX - 1) / PTE_ENTRIES_MAX;

	mc_dev_devel("number of PTEs %lx", nr_pte_refs);
	mc_dev_devel("number of pages %lx", nr_pages);

#ifndef MC_USE_VLX_PMEM
	/* Verify the PMD page reference */
	ret = vlx_vm_mem_verify(dom_id, buffer->pmd_ref);
	if (ret < 0) {
		mc_dev_err(ret, "vlx_vm_mem_verify failed");
		goto out;
	}
	mc_dev_devel("verified PMD ipa %llx", buffer->pmd_ref);

#endif /* MC_USE_VLX_PMEM */

	/* Create PMD table that contains physical PTE addresses */
	pmd_table->page = get_zeroed_page(GFP_KERNEL);
	if (!pmd_table->page) {
		ret = -ENOMEM;
		mc_dev_err(ret, "get_zeroed_page of pmd_table failed");
		goto out;
	}
	map->mmu->pages_created++;

	/*
	 * Read the shared PMD page into pmd_table that contains physical PTE
	 * addresses
	 */
	vlx_vm_mem_read(vlx_fe, pmd_table->addr, buffer->pmd_ref);

	/* Loop over the number of PTEs */
	for (i = 0; i < nr_pte_refs; i++) {
#ifndef MC_USE_VLX_PMEM
		unsigned long nr_pages = nr_pages_left;
		int j;

		if (nr_pages > PTE_ENTRIES_MAX)
			nr_pages = PTE_ENTRIES_MAX;

		/* Verify the PTE page */
		ret = vlx_vm_mem_verify(dom_id, pmd_table->entries[i]);
		if (ret < 0) {
			mc_dev_err(ret, "vlx_vm_mem_verify failed");
			goto out;
		}
		mc_dev_devel("verified PTE %d ipa %llx",
			     i, pmd_table->entries[i]);
#endif /* MC_USE_VLX_PMEM */

		/* Create PTE tables that contains physical page addresses */
		pte_tables[i].page = get_zeroed_page(GFP_KERNEL);
		if (!pte_tables[i].page) {
			mc_dev_err(ret,
				   "get_zeroed_page of pte_tables failed");
			goto out;
		}
		map->mmu->pages_created++;
		map->mmu->nr_pmd_entries++;

		/*
		 * Read the shared PTE page into pte_tables that contains
		 * physical page addresses
		 */
		vlx_vm_mem_read(vlx_fe, pte_tables[i].addr,
				pmd_table->entries[i]);

		/* Store local pte_tables */
		pmd_table->entries[i] = virt_to_phys(pte_tables[i].addr);

#ifndef MC_USE_VLX_PMEM
		/* Loop over the number of pages */
		for (j = 0; j < nr_pages; j++) {
			/* Verify the page */
			ret = vlx_vm_mem_verify(dom_id,
						pte_tables[i].entries[j]);
			if (ret < 0) {
				mc_dev_err(ret, "vlx_vm_mem_verify failed");
				goto out;
			}

			nr_pages_left--;
			mc_dev_devel(
				"verified PTE %d PAGE %d ipa %llx, left %ld",
				i, j, pte_tables[i].entries[j], nr_pages_left);
		}
#endif /* MC_USE_VLX_PMEM */
	}
	/* Update the map */
	map->mmu->offset = buffer->offset;
	map->mmu->length = buffer->length;
	map->mmu->flags = buffer->flags;

	/* Auto-delete */
	map->deleter.object = map;
	map->deleter.delete = (void(*)(void *))vlx_be_map_delete;
	map->mmu->deleter = &map->deleter;

	mc_dev_devel("verified map %p: nr pages %lu, nr pte %ld",
		     map, map->mmu->nr_pages, map->mmu->nr_pmd_entries);

	return map;
out:
	vlx_be_map_delete(map);
	return NULL;
}

/* Ops to set the mmu */

struct tee_mmu *vlx_be_set_mmu(struct protocol_be_map *map,
			       struct mcp_buffer_map b_map)
{
	return map->mmu;
}

/* Call the FE server */

static void vlx_call_fe(struct protocol_fe *pfe, atomic_t call_vm_instance_no)
{
	struct tee_vlx_fe *vlx_fe = container_of(pfe, struct tee_vlx_fe, pfe);
	struct vrpc_t *vrpc = vlx_fe->be2fe_vrpc;
	struct be2fe_data *data = vlx_fe->pfe.be2fe_data;
	vrpc_size_t size;
	int ret;

	for (;;) {
		/* Detect VM restarts */
		if (atomic_read(&pfe->vm_instance_no) !=
		    atomic_read(&call_vm_instance_no)) {
			mc_dev_info("VRPC FE instance changed, cancel call");
			return;
		}

		data->cmd = vlx_fe->pfe.be2fe_data->cmd;

		size = sizeof(*data);
		if (!vrpc_call(vrpc, &size))
			break;

		mc_dev_err(-ENODEV, "Lost VRPC FE server. Reopen.");
		vrpc_close(vrpc);
		ret = vrpc_client_open(vrpc, NULL, NULL);
		if (ret) {
			mc_dev_err(ret, "VRPC FE client open has failed!");
			break;
		}
		mc_dev_devel("Re-establish VRPC FE link.");
	}

	vlx_fe->pfe.be2fe_data->cmd = TEE_BE_NONE;
	vlx_fe->pfe.be2fe_data->id = 0;
}

/* Create the BE client */

static const char *vm_id(NkOsId vm_no)
{
	const struct tee_vlx_vm_info *vm_info = tee_vlx_find_vm_info(vm_no);

	if (!vm_info)
		return NULL;

	return vm_info->id;
}

/* Runs in atomic context */
static void vrpc_off(NkOsId peer)
{
	struct tee_vlx_fe *vlx_fe = NULL, *candidate;
	const struct tee_vlx_vm_info *vm_info;

	vm_info = tee_vlx_find_vm_info(peer);
	if (!vm_info)
		return;

	/* Find FE in list (created once and for all at start time) */
	list_for_each_entry(candidate, &l_ctx.vlx_fes, list) {
		if (!strcmp(candidate->vm_info->id, vm_info->id)) {
			vlx_fe = candidate;
			break;
		}
	}

	if (!vlx_fe)
		return;

	mc_dev_info("peer %s instance %d is gone",
		    client_vm_id(vlx_fe->pfe.client),
		    atomic_read(&vlx_fe->pfe.vm_instance_no));
	atomic_inc(&vlx_fe->pfe.vm_instance_no);
}

static int vlx_create_be_client(struct protocol_fe *pfe)
{
	struct tee_vlx_fe *vlx_fe = container_of(pfe, struct tee_vlx_fe, pfe);
	char *server_name = vlx_fe->pfe.fe2be_data->server_name;
	const char *name = NULL;
	struct vrpc_t *vrpc_fe;
	int ret;

	if (vlx_fe->client_is_open) {
		tee_vlx_fe_cleanup(vlx_fe);
		vlx_fe->client_is_open = false;
	}

	/* Create BE client for the FE server */
	vrpc_fe = vrpc_client_lookup(server_name, 0);
	if (!vrpc_fe) {
		ret = -ENODEV;
		mc_dev_err(ret, "No vrpc (client) link!");
		goto err_client_lookup;
	}

	vlx_fe->be2fe_vrpc = vrpc_fe;
	vlx_fe->pfe.be2fe_data = vrpc_data(vrpc_fe);

	if (vrpc_maxsize(vrpc_fe) < sizeof(*vlx_fe->pfe.be2fe_data)) {
		ret = -ENOMEM;
		mc_dev_err(ret, "Not enough VRPC FE client shared memory");
		goto err_vlx_fe_size;
	}

	ret = vrpc_client_open(vrpc_fe, NULL, NULL);
	if (ret) {
		mc_dev_err(ret, "VRPC FE client open has failed!");
		goto err_client_open;
	}
	mc_dev_devel("BE client initialized");

	name = vm_id(vrpc_peer_id(vlx_fe->be2fe_vrpc));
	if (IS_ERR(name)) {
		ret = PTR_ERR(name);
		goto err_vm_id_name;
	}

	/* Create the client */
	vlx_fe->pfe.client = client_create(true, name);
	if (!vlx_fe->pfe.client) {
		ret = -ENODEV;
		mc_dev_err(ret, "Failed to create FE client");
		goto err_client_create;
	}

	/* We shall be informed without delay of the VM death */
	vrpc_off_notifier(vrpc_fe, vrpc_off, 0);
	vlx_fe->client_is_open = true;
	mc_dev_info("peer %s instance %d appeared", name,
		    atomic_read(&vlx_fe->pfe.vm_instance_no));
	return 0;

err_client_create:
err_vm_id_name:
err_client_open:
err_vlx_fe_size:
	vrpc_release(vrpc_fe);
err_client_lookup:
	return ret;
}

/* Callback of the FE client */

static vrpc_size_t vlx_be_callback(void *cookie, vrpc_size_t size)
{
	struct tee_vlx_fe *vlx_fe = cookie;

	if (size != sizeof(*vlx_fe->pfe.fe2be_data))
		return 0;

	mc_dev_devel("FE -> BE command %u id %u", vlx_fe->pfe.fe2be_data->cmd,
		     vlx_fe->pfe.fe2be_data->id);

	vlx_fe->pfe.fe2be_data->otherend_ret =
		protocol_be_dispatch(&vlx_fe->pfe);

	mc_dev_devel("FE -> BE result %u id %u ret %d",
		     vlx_fe->pfe.fe2be_data->cmd, vlx_fe->pfe.fe2be_data->id,
		     vlx_fe->pfe.fe2be_data->otherend_ret);

	return sizeof(*vlx_fe->pfe.fe2be_data);
}

static int vlx_be_start_server(struct tee_vlx_fe *vlx_fe)
{
	struct vrpc_t *vrpc_fe2be;
	int ret;

	/* Create BE server for the FE client */
	vrpc_fe2be = vrpc_server_lookup(vlx_fe->vm_info->call_channel_name,
					NULL);
	if (!vrpc_fe2be) {
		ret = -ENODEV;
		mc_dev_err(ret, "Failed to lookup call channel name: %s",
			   vlx_fe->vm_info->call_channel_name);
		return ret;
	}

	vlx_fe->fe2be_vrpc = vrpc_fe2be;
	vlx_fe->pfe.fe2be_data = vrpc_data(vrpc_fe2be);

	if (vrpc_maxsize(vrpc_fe2be) < sizeof(*vlx_fe->pfe.fe2be_data)) {
		ret = -ENOMEM;
		mc_dev_err(ret, "Not enough VRPC BE server shared memory");
		return ret;
	}

	/* A thread shall be started to handle the callback */
	ret = vrpc_server_open(vrpc_fe2be, vlx_be_callback, vlx_fe, 0);
	if (ret) {
		mc_dev_err(ret, "VRPC BE server open has failed!");
		return ret;
	}

	mc_dev_info("Started server for FE %s", vlx_fe->vm_info->id);
	return 0;
}

int vlx_be_start(void)
{
	const struct tee_vlx_vm_info *vm_info;
	int ret = -ENODEV;

	INIT_LIST_HEAD(&l_ctx.vlx_fes);

	/* Gather all front-ends into a list */
	vm_info = &tee_vlx_vms_info[0];
	while (vm_info->id) {
		struct tee_vlx_fe *vlx_fe;

		if (!vm_info->call_channel_name) {
			/* Back-end */
			vm_info++;
			continue;
		}

		mc_dev_info("Found FE %s", vm_info->id);
		vlx_fe = tee_vlx_fe_create(vm_info);
		if (IS_ERR(vlx_fe)) {
			ret = PTR_ERR(vlx_fe);
			mc_dev_err(ret, "failed to create vlx_fe");
			return ret;
		}

#ifdef MC_USE_VLX_PMEM
		/* Allocate PMEM for FE in vm number 'vm_info->no' */
		ret = vlx_vm_mem_alloc(vlx_fe);
		if (ret) {
			/* Don't fail for all VMs in case of error */
			mc_dev_err(ret, "failed to allocate PMEM");
			tee_vlx_fe_put(vlx_fe);
			continue;
		}
#endif
		ret = vlx_be_start_server(vlx_fe);
		if (ret) {
			tee_vlx_fe_put(vlx_fe);
			/* Don't fail for all if DTB is not up-to-date */
			vm_info++;
			continue;
		}

		list_add_tail(&vlx_fe->list, &l_ctx.vlx_fes);
		vm_info++;
	}

	mc_dev_devel("BE server initialized");

	return 0;
}

static struct tee_protocol_be_call_ops be_call_ops = {
	.call_fe = vlx_call_fe,
	.be_map_create = vlx_be_map_create,
	.be_map_delete = vlx_be_map_delete,
	.be_set_mmu = vlx_be_set_mmu,
	.be_create_client = vlx_create_be_client,
};

static struct tee_protocol_ops protocol_ops = {
	.name = "VLX BE",
	.start = vlx_be_start,
	.be_call_ops = &be_call_ops,
};

struct tee_protocol_ops *vlx_be_check(void)
{
	const struct tee_vlx_vm_info *vm_info;

	/* Get the vm number to detect if this is a VLX BE */
	vm_info = tee_vlx_find_vm_info(nkops.nk_id_get());
	if (!vm_info || vm_info->call_channel_name)
		return NULL;

	protocol_ops.vm_id = vm_info->id;
	return &protocol_ops;
}

#endif /* CONFIG_VLX_HYP */
