/*
 * Copyright (c) 2017-2018 TRUSTONIC LIMITED
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

#ifdef CONFIG_XEN

#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#include "platform.h"		/* MC_XENBUS_MAP_RING_VALLOC_4_1 */
#include "main.h"
#include "admin.h"		/* tee_object* */
#include "client.h"		/* Consider other VMs as clients */
#include "mmu.h"
#include "mcp.h"		/* mcp_get_version */
#include "nq.h"
#include "xen_common.h"
#include "xen_be.h"

#define vaddr(page) ((unsigned long)pfn_to_kaddr(page_to_pfn(page)))

static struct {
	struct list_head	xfes;
	struct mutex		xfes_mutex;	/* Protect the above */
} l_ctx;

/* Maps */

struct xen_be_map {
	struct page		**pages;
	grant_handle_t		*handles;
	unsigned long		nr_pages;
	u32			flags;
	bool			pages_allocd;
	bool			refs_mapped;
	/* To auto-delete */
	struct tee_deleter	deleter;
};

static void xen_be_map_delete(struct xen_be_map *map)
{
	int i;

	if (map->refs_mapped) {
		struct gnttab_unmap_grant_ref *unmaps;

		unmaps = kcalloc(map->nr_pages, sizeof(*unmaps), GFP_KERNEL);
		if (!unmaps)
			/* Cannot go on */
			return;

		for (i = 0; i < map->nr_pages; i++)
			gnttab_set_unmap_op(&unmaps[i], vaddr(map->pages[i]),
					    map->flags, map->handles[i]);

		if (gnttab_unmap_refs(unmaps, NULL, map->pages, map->nr_pages))
			/* Cannot go on */
			return;

		for (i = 0; i < map->nr_pages; i++)
			put_page(map->pages[i]);

		kfree(unmaps);
	}

	if (map->pages_allocd)
		gnttab_free_pages(map->nr_pages, map->pages);

	kfree(map->handles);
	kfree(map->pages);
	kfree(map);
	mc_dev_devel("freed xen map %p", map);
	atomic_dec(&g_ctx.c_xen_maps);
}

static struct xen_be_map *be_map_create(const struct xen_be_map *pte_map,
					grant_ref_t *refs, int nr_refs,
					int pte_entries_max, int dom_id,
					bool readonly)
{
	struct xen_be_map *map;
	struct gnttab_map_grant_ref *maps = NULL;
	int i, ret = -ENOMEM;

	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (!map)
		return ERR_PTR(-ENOMEM);

	atomic_inc(&g_ctx.c_xen_maps);
	map->flags = GNTMAP_host_map;
	if (readonly)
		map->flags |= GNTMAP_readonly;

	map->nr_pages = nr_refs;
	map->pages = kcalloc(map->nr_pages, sizeof(*map->pages), GFP_KERNEL);
	if (!map->pages)
		goto out;

	map->handles = kcalloc(map->nr_pages, sizeof(*map->handles),
			       GFP_KERNEL);
	if (!map->handles)
		goto out;

	if (gnttab_alloc_pages(map->nr_pages, map->pages))
		goto out;

	map->pages_allocd = true;
	maps = kcalloc(map->nr_pages, sizeof(*maps), GFP_KERNEL);
	if (!maps)
		goto out;

	if (pte_map) {
		int k = 0, nr_refs_left = nr_refs;

		for (i = 0; i < pte_map->nr_pages; i++) {
			int j, nr_refs = nr_refs_left;
			grant_ref_t *refs = (void *)vaddr(pte_map->pages[i]);

			if (nr_refs > pte_entries_max)
				nr_refs = pte_entries_max;

			for (j = 0;  j < nr_refs; j++) {
				mc_dev_devel("map [%d, %d] -> %d ref %u",
					     i, j, k, refs[j]);
#ifdef DEBUG
				/* Relax serial interface to not kill the USB */
				usleep_range(100, 200);
#endif
				gnttab_set_map_op(
					&maps[k], vaddr(map->pages[k]),
					map->flags, refs[j], dom_id);
				nr_refs_left--;
				k++;
			}
		}
	} else {
		for (i = 0;  i < map->nr_pages; i++) {
			mc_dev_devel("map table %d ref %u", i, refs[i]);
			gnttab_set_map_op(&maps[i], vaddr(map->pages[i]),
					  map->flags, refs[i], dom_id);
		}
	}

	ret = gnttab_map_refs(maps, NULL, map->pages, map->nr_pages);
	if (ret)
		goto out;

	map->refs_mapped = true;
	/* Pin pages */
	for (i = 0;  i < map->nr_pages; i++) {
		get_page(map->pages[i]);
		map->handles[i] = maps[i].handle;
	}

out:
	kfree(maps);

	if (ret) {
		xen_be_map_delete(map);
		return ERR_PTR(-ret);
	}

	mc_dev_devel("created %s xen map %p", pte_map ? "buffer" : "ptes", map);
	return map;
}

static struct xen_be_map *xen_be_map_create(struct tee_xen_buffer *buffer,
					    int pte_entries_max, int dom_id)
{
	struct xen_be_map *map;
	struct xen_be_map *pte_map;
	int nr_pte_refs =
		(buffer->info->nr_refs + pte_entries_max - 1) / pte_entries_max;

	/* First map the PTE pages */
	pte_map = be_map_create(NULL, buffer->data.refs, nr_pte_refs,
				pte_entries_max, dom_id, true);
	if (IS_ERR(pte_map))
		return pte_map;

	/* Now map the pages */
	map = be_map_create(pte_map, NULL, buffer->info->nr_refs,
			    pte_entries_max, dom_id,
			    buffer->info->flags == MC_IO_MAP_INPUT);
	/* PTE pages mapping not needed any more */
	xen_be_map_delete(pte_map);
	if (!IS_ERR(map)) {
		/* Auto-delete */
		map->deleter.object = map;
		map->deleter.delete = (void(*)(void *))xen_be_map_delete;
	}

	return map;
}

/* Dom0 call to DomU */

/* Must be called under xfe->ring_mutex */
static inline void call_domu(struct tee_xfe *xfe, enum tee_xen_dom0_cmd cmd,
			     u32 id, int ret)
{
	WARN_ON(!xfe->ring_busy);

	/* Set command and ID */
	xfe->ring->dom0.cmd = cmd;
	xfe->ring->dom0.id = id;
	xfe->ring->dom0.cmd_ret = ret;
	mc_dev_devel("Dom0 -> DomU request %u id %u ret %d",
		     xfe->ring->dom0.cmd, xfe->ring->dom0.id,
		     xfe->ring->dom0.cmd_ret);
	/* Call */
	notify_remote_via_irq(xfe->irq_dom0);
	wait_for_completion(&xfe->ring_completion);
}

/* Will be called back under xfe->ring_mutex */
static irqreturn_t xen_be_irq_handler_dom0_th(int intr, void *arg)
{
	struct tee_xfe *xfe = arg;

	if (!xfe->ring->dom0.cmd) {
		mc_dev_devel("Ignore IRQ with no command (on DomU connect)");
		return IRQ_HANDLED;
	}

	WARN_ON(!xfe->ring_busy);

	/* Response to a dom0 command, our side of ring locked by us */
	mc_dev_devel("Dom0 -> DomU response %u id %u ret %d",
		     xfe->ring->dom0.cmd, xfe->ring->dom0.id,
		     xfe->ring->dom0.cmd_ret);
	xfe->ring->dom0.cmd = TEE_XEN_DOM0_NONE;
	xfe->ring->dom0.id = 0;
	xfe->ring->dom0.cmd_ret = -EPERM;
	complete(&xfe->ring_completion);

	return IRQ_HANDLED;
}

/* MC protocol interface */

static inline int xen_be_get_version(struct tee_xfe *xfe)
{
	struct mc_version_info version_info;
	int ret;

	ret = mcp_get_version(&version_info);
	if (ret)
		return ret;

	xfe->ring->domu.version_info = version_info;
	return 0;
}

static inline int xen_be_mc_has_sessions(struct tee_xfe *xfe)
{
	return client_has_sessions(xfe->client) ? -EBUSY : 0;
}

static inline int xen_be_mc_open_session(struct tee_xfe *xfe)
{
	struct tee_xen_buffer *tci_buffer = &xfe->buffers[0];
	struct mcp_open_info info = {
		.type = TEE_MC_UUID,
		.uuid = &xfe->ring->domu.uuid,
	};
	int ret;

	if (tci_buffer->info->flags) {
		struct xen_be_map *map;
		struct mcp_buffer_map b_map = {
			.offset = tci_buffer->info->offset,
			.length = tci_buffer->info->length,
			.flags = tci_buffer->info->flags,
		};

		map = xen_be_map_create(tci_buffer, xfe->pte_entries_max,
					xfe->xdev->otherend_id);
		if (IS_ERR(map)) {
			ret = PTR_ERR(map);
			goto out;
		}

		/* Shall be freed by session */
		b_map.nr_pages = map->nr_pages;
		info.tci_mmu = tee_mmu_wrap(&map->deleter, map->pages,
					    &b_map);
		if (IS_ERR(info.tci_mmu)) {
			ret = PTR_ERR(info.tci_mmu);
			info.tci_mmu = NULL;
			goto out;
		}
	}

	/* Open session */
	ret = client_mc_open_common(xfe->client, &info,
				    &xfe->ring->domu.session_id);

out:
	if (info.tci_mmu)
		tee_mmu_put(info.tci_mmu);

	mc_dev_devel("session %x, exit with %d",
		     xfe->ring->domu.session_id, ret);
	return ret;
}

static inline int xen_be_mc_open_trustlet(struct tee_xfe *xfe)
{
	struct tee_xen_buffer *ta_buffer = &xfe->buffers[1];
	struct tee_xen_buffer *tci_buffer = &xfe->buffers[0];
	struct mcp_open_info info = {
		.type = TEE_MC_TA,
	};
	struct xen_be_map *ta_map;
	void *addr = NULL;
	int ret = -ENOMEM;

	ta_map = xen_be_map_create(ta_buffer, xfe->pte_entries_max,
				   xfe->xdev->otherend_id);
	if (IS_ERR(ta_map))
		return PTR_ERR(ta_map);

	info.spid = xfe->ring->domu.spid;
	addr = vmap(ta_map->pages, ta_map->nr_pages,
		    VM_MAP | VM_IOREMAP | VM_USERMAP, PAGE_KERNEL);
	if (!addr)
		goto out;

	info.va = (uintptr_t)addr + ta_buffer->info->offset;
	info.len = ta_buffer->info->length;

	if (tci_buffer->info->flags) {
		struct xen_be_map *map;
		struct mcp_buffer_map b_map = {
			.offset = tci_buffer->info->offset,
			.length = tci_buffer->info->length,
			.flags = tci_buffer->info->flags,
		};

		map = xen_be_map_create(tci_buffer, xfe->pte_entries_max,
					xfe->xdev->otherend_id);
		if (IS_ERR(map)) {
			ret = PTR_ERR(map);
			goto out;
		}

		/* Shall be freed by session */
		b_map.nr_pages = map->nr_pages;
		info.tci_mmu = tee_mmu_wrap(&map->deleter, map->pages, &b_map);
		if (IS_ERR(info.tci_mmu)) {
			ret = PTR_ERR(info.tci_mmu);
			info.tci_mmu = NULL;
			goto out;
		}
	}

	/* Open session */
	ret = client_mc_open_common(xfe->client, &info,
				    &xfe->ring->domu.session_id);

out:
	if (info.tci_mmu)
		tee_mmu_put(info.tci_mmu);

	if (addr)
		vunmap(addr);

	xen_be_map_delete(ta_map);

	mc_dev_devel("session %x, exit with %d",
		     xfe->ring->domu.session_id, ret);
	return ret;
}

static inline int xen_be_mc_close_session(struct tee_xfe *xfe)
{
	return client_remove_session(xfe->client, xfe->ring->domu.session_id);
}

static inline int xen_be_mc_notify(struct tee_xfe *xfe)
{
	return client_notify_session(xfe->client, xfe->ring->domu.session_id);
}

/* mc_wait cannot keep the ring busy while waiting, so we use a worker */
struct mc_wait_work {
	struct work_struct	work;
	struct tee_xfe		*xfe;
	u32			session_id;
	s32			timeout;
	u32			id;
};

static void xen_be_mc_wait_worker(struct work_struct *work)
{
	struct mc_wait_work *wait_work =
		container_of(work, struct mc_wait_work, work);
	struct tee_xfe *xfe = wait_work->xfe;
	int ret;

	ret = client_waitnotif_session(wait_work->xfe->client,
				       wait_work->session_id,
				       wait_work->timeout, false);

	/* Send return code */
	mc_dev_devel("MC wait session done %x, ret %d",
		     wait_work->session_id, ret);
	ring_get(xfe);
	/* In */
	xfe->ring->dom0.session_id = wait_work->session_id;
	/* Call */
	call_domu(xfe, TEE_XEN_MC_WAIT_DONE, wait_work->id, ret);
	/* Out */
	ring_put(xfe);
	kfree(wait_work);
	tee_xfe_put(xfe);
}

static inline int xen_be_mc_wait(struct tee_xfe *xfe)
{
	struct mc_wait_work *wait_work;

	/* Wait in a separate thread to release the communication ring */
	wait_work = kzalloc(sizeof(*wait_work), GFP_KERNEL);
	if (!wait_work)
		return -ENOMEM;

	tee_xfe_get(xfe);
	wait_work->xfe = xfe;
	wait_work->session_id = xfe->ring->domu.session_id;
	wait_work->timeout = xfe->ring->domu.timeout;
	wait_work->id = xfe->ring->domu.id;
	INIT_WORK(&wait_work->work, xen_be_mc_wait_worker);
	schedule_work(&wait_work->work);
	return 0;
}

static inline int xen_be_mc_map(struct tee_xfe *xfe)
{
	struct tee_xen_buffer *buffer = &xfe->buffers[0];
	struct xen_be_map *map;
	struct tee_mmu *mmu = NULL;
	struct mc_ioctl_buffer buf;
	struct mcp_buffer_map b_map = {
		.offset = buffer->info->offset,
		.length = buffer->info->length,
		.flags = buffer->info->flags,
	};
	int ret;

	map = xen_be_map_create(buffer, xfe->pte_entries_max,
				xfe->xdev->otherend_id);
	if (IS_ERR(map)) {
		ret = PTR_ERR(map);
		return ret;
	}

	/* Shall be freed by session */
	b_map.nr_pages = map->nr_pages;
	mmu = tee_mmu_wrap(&map->deleter, map->pages, &b_map);
	if (IS_ERR(mmu)) {
		xen_be_map_delete(map);
		return PTR_ERR(mmu);
	}

	ret = client_mc_map(xfe->client, xfe->ring->domu.session_id, mmu, &buf);
	/* Releasing the MMU shall also clear the map */
	tee_mmu_put(mmu);
	if (!ret)
		buffer->info->sva = buf.sva;

	mc_dev_devel("session %x, exit with %d",
		     xfe->ring->domu.session_id, ret);
	return ret;
}

static inline int xen_be_mc_unmap(struct tee_xfe *xfe)
{
	struct tee_xen_buffer *buffer = &xfe->buffers[0];
	struct mc_ioctl_buffer buf = {
		.len = buffer->info->length,
		.sva = buffer->info->sva,
	};
	int ret;

	ret = client_mc_unmap(xfe->client, xfe->ring->domu.session_id, &buf);

	mc_dev_devel("session %x, exit with %d",
		     xfe->ring->domu.session_id, ret);
	return ret;
}

static inline int xen_be_mc_get_err(struct tee_xfe *xfe)
{
	int ret;

	ret = client_get_session_exitcode(xfe->client,
					  xfe->ring->domu.session_id,
					  &xfe->ring->domu.err);
	mc_dev_devel("session %x err %d, exit with %d",
		     xfe->ring->domu.session_id, xfe->ring->domu.err, ret);
	return ret;
}

/* GP protocol interface */

static inline int xen_be_gp_register_shared_mem(struct tee_xfe *xfe)
{
	struct tee_xen_buffer *buffer = &xfe->buffers[0];
	struct xen_be_map *map;
	struct tee_mmu *mmu = NULL;
	struct gp_shared_memory memref = {
		.buffer = buffer->info->addr,
		.size = buffer->info->length,
		.flags = buffer->info->flags,
	};
	struct mcp_buffer_map b_map = {
		.offset = buffer->info->offset,
		.length = buffer->info->length,
		.flags = buffer->info->flags,
	};
	int ret;

	map = xen_be_map_create(buffer, xfe->pte_entries_max,
				xfe->xdev->otherend_id);
	if (IS_ERR(map)) {
		ret = PTR_ERR(map);
		return ret;
	}

	/* Shall be freed by session */
	b_map.nr_pages = map->nr_pages;
	mmu = tee_mmu_wrap(&map->deleter, map->pages, &b_map);
	if (IS_ERR(mmu)) {
		xen_be_map_delete(map);
		return PTR_ERR(mmu);
	}

	ret = client_gp_register_shared_mem(xfe->client, mmu,
					    &buffer->info->sva, &memref,
					    &xfe->ring->domu.gp_ret);
	/* Releasing the MMU shall also clear the map */
	tee_mmu_put(mmu);
	mc_dev_devel("session %x, exit with %d",
		     xfe->ring->domu.session_id, ret);
	return ret;
}

static inline int xen_be_gp_release_shared_mem(struct tee_xfe *xfe)
{
	struct tee_xen_buffer *buffer = &xfe->buffers[0];
	struct gp_shared_memory memref = {
		.buffer = buffer->info->addr,
		.size = buffer->info->length,
		.flags = buffer->info->flags,
	};
	int ret;

	ret = client_gp_release_shared_mem(xfe->client, &memref);

	mc_dev_devel("exit with %d", ret);
	return ret;
}

/* GP functions cannot keep the ring busy while waiting, so we use a worker */
struct gp_work {
	struct work_struct		work;
	struct tee_xfe			*xfe;
	u64				operation_id;
	struct interworld_session	iws;
	struct tee_mmu			*mmus[4];
	struct mc_uuid_t		uuid;
	u32				session_id;
	u32				id;
};

static void xen_be_gp_open_session_worker(struct work_struct *work)
{
	struct gp_work *gp_work = container_of(work, struct gp_work, work);
	struct tee_xfe *xfe = gp_work->xfe;
	struct gp_return gp_ret;
	int i, ret;

	ret = client_gp_open_session_domu(xfe->client, &gp_work->uuid,
					  gp_work->operation_id, &gp_work->iws,
					  gp_work->mmus, &gp_ret);
	mc_dev_devel("GP open session done, ret %d", ret);
	for (i = 0; i < TEE_BUFFERS; i++)
		if (gp_work->mmus[i])
			tee_mmu_put(gp_work->mmus[i]);

	/* Send return code */
	ring_get(xfe);
	/* In */
	xfe->ring->dom0.operation_id = gp_work->operation_id;
	xfe->ring->dom0.iws = gp_work->iws;
	xfe->ring->dom0.gp_ret = gp_ret;
	/* Call */
	call_domu(xfe, TEE_XEN_GP_OPEN_SESSION_DONE, gp_work->id, ret);
	/* Out */
	ring_put(xfe);
	kfree(gp_work);
	tee_xfe_put(xfe);
}

static inline int xen_be_gp_open_session(struct tee_xfe *xfe)
{
	struct gp_work *gp_work;
	int i, ret = 0;

	gp_work = kzalloc(sizeof(*gp_work), GFP_KERNEL);
	if (!gp_work)
		return -ENOMEM;

	/* Map tmpref buffers */
	for (i = 0; i < TEE_BUFFERS; i++) {
		struct tee_xen_buffer *buffer = &xfe->buffers[i];
		struct xen_be_map *map;
		struct mcp_buffer_map b_map = {
			.offset = buffer->info->offset,
			.length = buffer->info->length,
			.flags = buffer->info->flags,
		};

		if (!buffer->info->flags)
			continue;

		map = xen_be_map_create(buffer, xfe->pte_entries_max,
					xfe->xdev->otherend_id);
		if (IS_ERR(map)) {
			ret = PTR_ERR(map);
			goto err_map;
		}

		/* Shall be freed by session */
		b_map.nr_pages = map->nr_pages;
		gp_work->mmus[i] = tee_mmu_wrap(&map->deleter, map->pages,
						&b_map);
		if (IS_ERR(gp_work->mmus[i])) {
			xen_be_map_delete(map);
			ret = PTR_ERR(gp_work->mmus[i]);
			goto err_mmus;
		}
	}

	tee_xfe_get(xfe);
	gp_work->xfe = xfe;
	gp_work->operation_id = xfe->ring->domu.operation_id;
	gp_work->iws = xfe->ring->domu.iws;
	gp_work->uuid = xfe->ring->domu.uuid;
	gp_work->id = xfe->ring->domu.id;
	INIT_WORK(&gp_work->work, xen_be_gp_open_session_worker);
	schedule_work(&gp_work->work);
	return 0;

err_mmus:
	for (i = 0; i < TEE_BUFFERS; i++)
		if (!IS_ERR_OR_NULL(gp_work->mmus[i]))
			tee_mmu_put(gp_work->mmus[i]);
err_map:
	kfree(gp_work);
	return ret;
}

static void xen_be_gp_close_session_worker(struct work_struct *work)
{
	struct gp_work *gp_work = container_of(work, struct gp_work, work);
	struct tee_xfe *xfe = gp_work->xfe;
	int ret;

	ret = client_gp_close_session(xfe->client, gp_work->session_id);
	mc_dev_devel("GP close session done, ret %d", ret);

	/* Send return code */
	ring_get(xfe);
	/* In */
	xfe->ring->dom0.operation_id = gp_work->operation_id;
	/* Call */
	call_domu(xfe, TEE_XEN_GP_CLOSE_SESSION_DONE, gp_work->id, ret);
	/* Out */
	ring_put(xfe);
	kfree(gp_work);
	tee_xfe_put(xfe);
}

static inline int xen_be_gp_close_session(struct tee_xfe *xfe)
{
	struct gp_work *gp_work;

	gp_work = kzalloc(sizeof(*gp_work), GFP_KERNEL);
	if (!gp_work)
		return -ENOMEM;

	tee_xfe_get(xfe);
	gp_work->xfe = xfe;
	gp_work->operation_id = xfe->ring->domu.operation_id;
	gp_work->session_id = xfe->ring->domu.session_id;
	gp_work->id = xfe->ring->domu.id;
	INIT_WORK(&gp_work->work, xen_be_gp_close_session_worker);
	schedule_work(&gp_work->work);
	return 0;
}

static void xen_be_gp_invoke_command_worker(struct work_struct *work)
{
	struct gp_work *gp_work = container_of(work, struct gp_work, work);
	struct tee_xfe *xfe = gp_work->xfe;
	struct gp_return gp_ret;
	int i, ret;

	ret = client_gp_invoke_command_domu(xfe->client, gp_work->session_id,
					    gp_work->operation_id,
					    &gp_work->iws, gp_work->mmus,
					    &gp_ret);
	mc_dev_devel("GP invoke command done, ret %d", ret);
	for (i = 0; i < TEE_BUFFERS; i++)
		if (gp_work->mmus[i])
			tee_mmu_put(gp_work->mmus[i]);

	/* Send return code */
	ring_get(xfe);
	/* In */
	xfe->ring->dom0.operation_id = gp_work->operation_id;
	xfe->ring->dom0.iws = gp_work->iws;
	xfe->ring->dom0.gp_ret = gp_ret;
	/* Call */
	call_domu(xfe, TEE_XEN_GP_INVOKE_COMMAND_DONE, gp_work->id, ret);
	/* Out */
	ring_put(xfe);
	kfree(gp_work);
	tee_xfe_put(xfe);
}

static inline int xen_be_gp_invoke_command(struct tee_xfe *xfe)
{
	struct gp_work *gp_work;
	int i, ret = 0;

	gp_work = kzalloc(sizeof(*gp_work), GFP_KERNEL);
	if (!gp_work)
		return -ENOMEM;

	/* Map tmpref buffers */
	for (i = 0; i < TEE_BUFFERS; i++) {
		struct tee_xen_buffer *buffer = &xfe->buffers[i];
		struct xen_be_map *map;
		struct mcp_buffer_map b_map = {
			.offset = buffer->info->offset,
			.length = buffer->info->length,
			.flags = buffer->info->flags,
		};

		if (!buffer->info->flags)
			continue;

		map = xen_be_map_create(buffer, xfe->pte_entries_max,
					xfe->xdev->otherend_id);
		if (IS_ERR(map)) {
			ret = PTR_ERR(map);
			goto err_map;
		}

		/* Shall be freed by session */
		b_map.nr_pages = map->nr_pages;
		gp_work->mmus[i] = tee_mmu_wrap(&map->deleter, map->pages,
						&b_map);
		if (IS_ERR(gp_work->mmus[i])) {
			xen_be_map_delete(map);
			ret = PTR_ERR(gp_work->mmus[i]);
			goto err_mmus;
		}
	}

	tee_xfe_get(xfe);
	gp_work->xfe = xfe;
	gp_work->operation_id = xfe->ring->domu.operation_id;
	gp_work->iws = xfe->ring->domu.iws;
	gp_work->session_id = xfe->ring->domu.session_id;
	gp_work->id = xfe->ring->domu.id;
	INIT_WORK(&gp_work->work, xen_be_gp_invoke_command_worker);
	schedule_work(&gp_work->work);
	return 0;

err_mmus:
	for (i = 0; i < TEE_BUFFERS; i++)
		if (!IS_ERR_OR_NULL(gp_work->mmus[i]))
			tee_mmu_put(gp_work->mmus[i]);
err_map:
	kfree(gp_work);
	return ret;
}

static inline int xen_be_gp_request_cancellation(struct tee_xfe *xfe)
{
	client_gp_request_cancellation(xfe->client,
				       xfe->ring->domu.operation_id);
	return 0;
}

static irqreturn_t xen_be_irq_handler_domu_th(int intr, void *arg)
{
	struct tee_xfe *xfe = arg;

	if (!xfe->ring->domu.cmd) {
		mc_dev_devel("Ignore IRQ with no command (on DomU connect)");
		return IRQ_HANDLED;
	}

	/* DomU event, their side of ring locked by them */
	schedule_work(&xfe->work);

	return IRQ_HANDLED;
}

static void xen_be_irq_handler_domu_bh(struct work_struct *data)
{
	struct tee_xfe *xfe = container_of(data, struct tee_xfe, work);

	xfe->ring->domu.otherend_ret = -EINVAL;
	mc_dev_devel("DomU -> Dom0 command %u id %u",
		     xfe->ring->domu.cmd, xfe->ring->domu.id);
	switch (xfe->ring->domu.cmd) {
	case TEE_XEN_DOMU_NONE:
		return;
	/* MC */
	case TEE_XEN_MC_HAS_SESSIONS:
		xfe->ring->domu.otherend_ret = xen_be_mc_has_sessions(xfe);
		break;
	case TEE_XEN_GET_VERSION:
		xfe->ring->domu.otherend_ret = xen_be_get_version(xfe);
		break;
	case TEE_XEN_MC_OPEN_SESSION:
		xfe->ring->domu.otherend_ret = xen_be_mc_open_session(xfe);
		break;
	case TEE_XEN_MC_OPEN_TRUSTLET:
		xfe->ring->domu.otherend_ret = xen_be_mc_open_trustlet(xfe);
		break;
	case TEE_XEN_MC_CLOSE_SESSION:
		xfe->ring->domu.otherend_ret = xen_be_mc_close_session(xfe);
		break;
	case TEE_XEN_MC_NOTIFY:
		xfe->ring->domu.otherend_ret = xen_be_mc_notify(xfe);
		break;
	case TEE_XEN_MC_WAIT:
		xfe->ring->domu.otherend_ret = xen_be_mc_wait(xfe);
		break;
	case TEE_XEN_MC_MAP:
		xfe->ring->domu.otherend_ret = xen_be_mc_map(xfe);
		break;
	case TEE_XEN_MC_UNMAP:
		xfe->ring->domu.otherend_ret = xen_be_mc_unmap(xfe);
		break;
	case TEE_XEN_MC_GET_ERR:
		xfe->ring->domu.otherend_ret = xen_be_mc_get_err(xfe);
		break;
	/* GP */
	case TEE_XEN_GP_REGISTER_SHARED_MEM:
		xfe->ring->domu.otherend_ret =
			xen_be_gp_register_shared_mem(xfe);
		break;
	case TEE_XEN_GP_RELEASE_SHARED_MEM:
		xfe->ring->domu.otherend_ret =
			xen_be_gp_release_shared_mem(xfe);
		break;
	case TEE_XEN_GP_OPEN_SESSION:
		xfe->ring->domu.otherend_ret = xen_be_gp_open_session(xfe);
		break;
	case TEE_XEN_GP_CLOSE_SESSION:
		xfe->ring->domu.otherend_ret = xen_be_gp_close_session(xfe);
		break;
	case TEE_XEN_GP_INVOKE_COMMAND:
		xfe->ring->domu.otherend_ret = xen_be_gp_invoke_command(xfe);
		break;
	case TEE_XEN_GP_REQUEST_CANCELLATION:
		xfe->ring->domu.otherend_ret =
			xen_be_gp_request_cancellation(xfe);
		break;
	}

	mc_dev_devel("DomU -> Dom0 result %u id %u ret %d",
		     xfe->ring->domu.cmd, xfe->ring->domu.id,
		     xfe->ring->domu.otherend_ret);
	notify_remote_via_irq(xfe->irq_domu);
}

/* Device */

static const struct xenbus_device_id xen_be_ids[] = {
	{ "tee_xen" },
	{ "" }
};

/* Called when a front-end is created */
static int xen_be_probe(struct xenbus_device *xdev,
			const struct xenbus_device_id *id)
{
	struct tee_xfe *xfe;
	int ret = 0;

	ret = xenbus_switch_state(xdev, XenbusStateInitWait);
	if (ret) {
		xenbus_dev_fatal(xdev, ret,
				 "failed to change state to initwait");
		return ret;
	}

	xfe = tee_xfe_create(xdev);
	if (!xfe) {
		ret = -ENOMEM;
		xenbus_dev_fatal(xdev, ret, "failed to create FE struct");
		goto err_xfe_create;
	}

	xfe->client = client_create(true);
	if (!xfe->client) {
		ret = -ENOMEM;
		xenbus_dev_fatal(xdev, ret, "failed to create FE client");
		goto err_client_create;
	}

	INIT_WORK(&xfe->work, xen_be_irq_handler_domu_bh);

	mutex_lock(&l_ctx.xfes_mutex);
	list_add_tail(&xfe->list, &l_ctx.xfes);
	mutex_unlock(&l_ctx.xfes_mutex);

	ret = xenbus_switch_state(xdev, XenbusStateInitialised);
	if (ret) {
		xenbus_dev_fatal(xdev, ret,
				 "failed to change state to initialised");
		goto err_switch_state;
	}

	return 0;

err_switch_state:
	mutex_lock(&l_ctx.xfes_mutex);
	list_del(&xfe->list);
	mutex_unlock(&l_ctx.xfes_mutex);
err_client_create:
	tee_xfe_put(xfe);
err_xfe_create:
	return ret;
}

/* Called when device is unregistered */
static int xen_be_remove(struct xenbus_device *xdev)
{
	struct tee_xfe *xfe = dev_get_drvdata(&xdev->dev);

	xenbus_switch_state(xdev, XenbusStateClosed);

	mutex_lock(&l_ctx.xfes_mutex);
	list_del(&xfe->list);
	mutex_unlock(&l_ctx.xfes_mutex);

	tee_xfe_put(xfe);
	return 0;
}

static inline int xen_be_map_ring_valloc(struct xenbus_device *dev,
					 grant_ref_t ref, void **vaddr)
{
	return xenbus_map_ring_valloc(dev, &ref, 1, vaddr);
}

static inline void frontend_attach(struct tee_xfe *xfe)
{
	int domu_version;
	int ret;
	int i;

	if (xenbus_read_driver_state(xfe->xdev->nodename) !=
			XenbusStateInitialised)
		return;

	ret = xenbus_gather(XBT_NIL, xfe->xdev->otherend,
			    "ring-ref", "%u", &xfe->ring_ref,
			    "pte-entries-max", "%u", &xfe->pte_entries_max,
			    "event-channel-domu", "%u", &xfe->evtchn_domu,
			    "event-channel-dom0", "%u", &xfe->evtchn_dom0,
			    "domu-version", "%u", &domu_version, NULL);
	if (ret) {
		xenbus_dev_fatal(xfe->xdev, ret,
				 "failed to gather other domain info");
		return;
	}

	mc_dev_devel("ring ref %u evtchn domu=%u dom0=%u version=%u",
		     xfe->ring_ref, xfe->evtchn_domu, xfe->evtchn_dom0,
		     domu_version);

	if (domu_version != TEE_XEN_VERSION) {
		xenbus_dev_fatal(
			xfe->xdev, ret,
			"front- and back-end versions do not match: %d vs %d",
			domu_version, TEE_XEN_VERSION);
		return;
	}

	ret = xen_be_map_ring_valloc(xfe->xdev, xfe->ring_ref, &xfe->ring_p);
	if (ret < 0) {
		xenbus_dev_fatal(xfe->xdev, ret, "failed to map ring");
		return;
	}
	mc_dev_devel("mapped ring %p", xfe->ring_p);

	/* Map buffers individually */
	for (i = 0; i < TEE_BUFFERS; i++) {
		ret = xen_be_map_ring_valloc(xfe->xdev,
					     xfe->ring->domu.buffers[i].pmd_ref,
					     &xfe->buffers[i].data.addr);
		if (ret < 0) {
			xenbus_dev_fatal(xfe->xdev, ret,
					 "failed to map buffer page");
			return;
		}

		xfe->buffers[i].info = &xfe->ring->domu.buffers[i];
	}

	ret = bind_interdomain_evtchn_to_irqhandler(
		xfe->xdev->otherend_id, xfe->evtchn_domu,
		xen_be_irq_handler_domu_th, 0, "tee_be_domu", xfe);
	if (ret < 0) {
		xenbus_dev_fatal(xfe->xdev, ret,
				 "failed to bind event channel to DomU IRQ");
		return;
	}

	xfe->irq_domu = ret;
	mc_dev_devel("bound DomU IRQ %d", xfe->irq_domu);

	ret = bind_interdomain_evtchn_to_irqhandler(
		xfe->xdev->otherend_id, xfe->evtchn_dom0,
		xen_be_irq_handler_dom0_th, 0, "tee_be_dom0", xfe);
	if (ret < 0) {
		xenbus_dev_fatal(xfe->xdev, ret,
				 "failed to bind event channel to Dom0 IRQ");
		return;
	}

	xfe->irq_dom0 = ret;
	mc_dev_devel("bound Dom0 IRQ %d", xfe->irq_dom0);

	ret = xenbus_switch_state(xfe->xdev, XenbusStateConnected);
	if (ret) {
		xenbus_dev_fatal(xfe->xdev, ret,
				 "failed to change state to connected");
		return;
	}
}

static inline void frontend_detach(struct tee_xfe *xfe)
{
	int i;

	xenbus_switch_state(xfe->xdev, XenbusStateClosing);
	if (xfe->irq_domu >= 0)
		unbind_from_irqhandler(xfe->irq_domu, xfe);

	if (xfe->irq_dom0 >= 0)
		unbind_from_irqhandler(xfe->irq_dom0, xfe);

	for (i = 0; i < TEE_BUFFERS; i++)
		xenbus_unmap_ring_vfree(xfe->xdev, xfe->buffers[i].data.addr);

	if (xfe->ring_p)
		xenbus_unmap_ring_vfree(xfe->xdev, xfe->ring_p);
}

static void xen_be_frontend_changed(struct xenbus_device *xdev,
				    enum xenbus_state fe_state)
{
	struct tee_xfe *xfe = dev_get_drvdata(&xdev->dev);

	mc_dev_devel("fe state changed to %d", fe_state);
	switch (fe_state) {
	case XenbusStateInitialising:
	case XenbusStateInitWait:
		break;
	case XenbusStateInitialised:
		frontend_attach(xfe);
		break;
	case XenbusStateConnected:
		break;
	case XenbusStateClosing:
		frontend_detach(xfe);
		break;
	case XenbusStateUnknown:
	case XenbusStateClosed:
		device_unregister(&xfe->xdev->dev);
		break;
	case XenbusStateReconfiguring:
	case XenbusStateReconfigured:
		break;
	}
}

static struct xenbus_driver xen_be_driver = {
	.ids  = xen_be_ids,
	.probe = xen_be_probe,
	.remove = xen_be_remove,
	.otherend_changed = xen_be_frontend_changed,
};

int xen_be_init(void)
{
	INIT_LIST_HEAD(&l_ctx.xfes);
	mutex_init(&l_ctx.xfes_mutex);
	return xenbus_register_backend(&xen_be_driver);
}

void xen_be_exit(void)
{
	xenbus_unregister_driver(&xen_be_driver);
}

#endif
