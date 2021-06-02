// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017-2019 TRUSTONIC LIMITED
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

#include <linux/irq.h>

#include "mci/mciiwp.h"		/* struct interworld_session */

#include "main.h"

#ifdef CONFIG_XEN

#include "client.h"
#include "iwp.h"
#include "mcp.h"
#include "xen_common.h"
#include "xen_fe.h"

#define page_to_gfn(p) (pfn_to_gfn(page_to_phys(p) >> XEN_PAGE_SHIFT))

static struct {
	int			(*probe)(void);
	int			(*start)(void);
	struct tee_xfe		*xfe;
	/* MC sessions */
	struct mutex		mc_sessions_lock;
	struct list_head	mc_sessions;
	/* GP operations */
	struct mutex		gp_operations_lock;
	struct list_head	gp_operations;
	/* Last back-end state,
	 * to overcome an issue in some Xen implementations
	 */
	int			last_be_state;
} l_ctx;

struct xen_fe_mc_session {
	struct list_head		list;
	struct completion		completion;
	int				ret;
	struct mcp_session		*session;
};

struct xen_fe_gp_operation {
	struct list_head		list;
	struct completion		completion;
	int				ret;
	u64				slot;
	struct gp_return		*gp_ret;
	struct interworld_session	*iws;
};

static inline struct xen_fe_mc_session *find_mc_session(u32 session_id)
{
	struct xen_fe_mc_session *session = ERR_PTR(-ENXIO), *candidate;

	mutex_lock(&l_ctx.mc_sessions_lock);
	list_for_each_entry(candidate, &l_ctx.mc_sessions, list) {
		struct mcp_session *mcp_session = candidate->session;

		if (mcp_session->sid == session_id) {
			session = candidate;
			break;
		}
	}
	mutex_unlock(&l_ctx.mc_sessions_lock);

	WARN(IS_ERR(session), "MC session not found for ID %u", session_id);
	return session;
}

static inline int xen_fe_mc_wait_done(struct tee_xfe *xfe)
{
	struct xen_fe_mc_session *session;

	mc_dev_devel("received response to mc_wait for session %x: %d",
		     xfe->ring->dom0.session_id, xfe->ring->dom0.cmd_ret);
	session = find_mc_session(xfe->ring->dom0.session_id);
	if (IS_ERR(session))
		return PTR_ERR(session);

	session->ret = xfe->ring->dom0.cmd_ret;
	complete(&session->completion);
	return 0;
}

static struct xen_fe_gp_operation *find_gp_operation(u64 operation_id)
{
	struct xen_fe_gp_operation *operation = ERR_PTR(-ENXIO), *candidate;

	mutex_lock(&l_ctx.gp_operations_lock);
	list_for_each_entry(candidate, &l_ctx.gp_operations, list) {
		if (candidate->slot == operation_id) {
			operation = candidate;
			list_del(&operation->list);
			break;
		}
	}
	mutex_unlock(&l_ctx.gp_operations_lock);

	WARN(IS_ERR(operation), "GP operation not found for op id %llx",
	     operation_id);
	return operation;
}

static inline int xen_fe_gp_open_session_done(struct tee_xfe *xfe)
{
	struct xen_fe_gp_operation *operation;

	mc_dev_devel("received response to gp_open_session for op id %llx",
		     xfe->ring->dom0.operation_id);
	operation = find_gp_operation(xfe->ring->dom0.operation_id);
	if (IS_ERR(operation))
		return PTR_ERR(operation);

	operation->ret = xfe->ring->dom0.cmd_ret;
	*operation->iws = xfe->ring->dom0.iws;
	*operation->gp_ret = xfe->ring->dom0.gp_ret;
	complete(&operation->completion);
	return 0;
}

static inline int xen_fe_gp_close_session_done(struct tee_xfe *xfe)
{
	struct xen_fe_gp_operation *operation;

	mc_dev_devel("received response to gp_close_session for op id %llx",
		     xfe->ring->dom0.operation_id);
	operation = find_gp_operation(xfe->ring->dom0.operation_id);
	if (IS_ERR(operation))
		return PTR_ERR(operation);

	operation->ret = xfe->ring->dom0.cmd_ret;
	complete(&operation->completion);
	return 0;
}

static inline int xen_fe_gp_invoke_command_done(struct tee_xfe *xfe)
{
	struct xen_fe_gp_operation *operation;

	mc_dev_devel("received response to gp_invoke_command for op id %llx",
		     xfe->ring->dom0.operation_id);
	operation = find_gp_operation(xfe->ring->dom0.operation_id);
	if (IS_ERR(operation))
		return PTR_ERR(operation);

	operation->ret = xfe->ring->dom0.cmd_ret;
	*operation->iws = xfe->ring->dom0.iws;
	*operation->gp_ret = xfe->ring->dom0.gp_ret;
	complete(&operation->completion);
	return 0;
}

static irqreturn_t xen_fe_irq_handler_dom0_th(int intr, void *arg)
{
	struct tee_xfe *xfe = arg;

	/* Dom0 event, their side of ring locked by them */
	schedule_work(&xfe->work);

	return IRQ_HANDLED;
}

static void xen_fe_irq_handler_dom0_bh(struct work_struct *data)
{
	struct tee_xfe *xfe = container_of(data, struct tee_xfe, work);
	int ret = -EINVAL;

	mc_dev_devel("Dom0 -> DomU command %u id %u cmd ret %d",
		     xfe->ring->dom0.cmd, xfe->ring->dom0.id,
		     xfe->ring->dom0.cmd_ret);
	switch (xfe->ring->dom0.cmd) {
	case TEE_XEN_DOM0_NONE:
		return;
	case TEE_XEN_MC_WAIT_DONE:
		ret = xen_fe_mc_wait_done(xfe);
		break;
	case TEE_XEN_GP_OPEN_SESSION_DONE:
		ret = xen_fe_gp_open_session_done(xfe);
		break;
	case TEE_XEN_GP_CLOSE_SESSION_DONE:
		ret = xen_fe_gp_close_session_done(xfe);
		break;
	case TEE_XEN_GP_INVOKE_COMMAND_DONE:
		ret = xen_fe_gp_invoke_command_done(xfe);
		break;
	}

	if (ret)
		mc_dev_err(ret, "Dom0 -> DomU result %u id %u",
			   xfe->ring->dom0.cmd, xfe->ring->dom0.id);
	else
		mc_dev_devel("Dom0 -> DomU result %u id %u",
			     xfe->ring->dom0.cmd, xfe->ring->dom0.id);

	notify_remote_via_evtchn(xfe->evtchn_dom0);
}

/* Buffer management */

struct xen_fe_map {
	/* Array of PTE tables, so we can release the associated buffer refs */
	union tee_xen_mmu_table	*pte_tables;
	int			nr_pte_tables;
	int			nr_refs;
	bool			readonly;
	int			pages_created;	/* Leak check */
	int			refs_granted;	/* Leak check */
	/* To auto-delete */
	struct tee_deleter deleter;
};

static void xen_fe_map_release_pmd(struct xen_fe_map *map,
				   const struct tee_xen_buffer *buffer)
{
	int i;

	if (IS_ERR_OR_NULL(map))
		return;

	for (i = 0; i < map->nr_pte_tables; i++) {
		gnttab_end_foreign_access(buffer->data.refs[i], true, 0);
		map->refs_granted--;
		mc_dev_devel("unmapped table %d ref %u",
			     i, buffer->data.refs[i]);
	}
}

static void xen_fe_map_release(struct xen_fe_map *map,
			       const struct tee_xen_buffer *buffer)
{
	int nr_refs_left = map->nr_refs;
	int i;

	if (buffer)
		xen_fe_map_release_pmd(map, buffer);

	for (i = 0; i < map->nr_pte_tables; i++) {
		int j, nr_refs = nr_refs_left;

		if (nr_refs > PTE_ENTRIES_MAX)
			nr_refs = PTE_ENTRIES_MAX;

		for (j = 0; j < nr_refs; j++) {
			gnttab_end_foreign_access(map->pte_tables[i].refs[j],
						  map->readonly, 0);
			map->refs_granted--;
			nr_refs_left--;
			mc_dev_devel("unmapped [%d, %d] ref %u, left %d",
				     i, j, map->pte_tables[i].refs[j],
				     nr_refs_left);
		}

		free_page(map->pte_tables[i].page);
		map->pages_created--;
	}

	kfree(map->pte_tables);
	if (map->pages_created || map->refs_granted)
		mc_dev_err(-EUCLEAN,
			   "leak detected: still in use %d, still ref'd %d",
			   map->pages_created, map->refs_granted);

	kfree(map);
	atomic_dec(&g_ctx.c_xen_maps);
	mc_dev_devel("freed map %p: refs=%u nr_pte_tables=%d",
		     map, map->nr_refs, map->nr_pte_tables);
}

static void xen_fe_map_delete(void *arg)
{
	struct xen_fe_map *map = arg;

	xen_fe_map_release(map, NULL);
}

static struct xen_fe_map *xen_fe_map_create(struct tee_xen_buffer *buffer,
					    const struct mcp_buffer_map *b_map,
					    int dom_id)
{
	/* b_map describes the PMD which contains pointers to PTE tables */
	uintptr_t *pte_tables = (uintptr_t *)(uintptr_t)b_map->addr;
	struct xen_fe_map *map;
	unsigned long nr_pte_tables =
		(b_map->nr_pages + PTE_ENTRIES_MAX - 1) / PTE_ENTRIES_MAX;
	unsigned long nr_pages_left = b_map->nr_pages;
	int readonly = !(b_map->flags & MC_IO_MAP_OUTPUT);
	int ret, i;

	/*
	 * We always map the same way, to simplify:
	 * * the buffer contains references to PTE pages
	 * * PTE pages contain references to the buffer pages
	 */
	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (!map)
		return ERR_PTR(-ENOMEM);

	atomic_inc(&g_ctx.c_xen_maps);
	map->readonly = readonly;

	map->pte_tables = kcalloc(nr_pte_tables,
				  sizeof(union tee_xen_mmu_table), GFP_KERNEL);
	if (!map->pte_tables) {
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < nr_pte_tables; i++) {
		/* As expected, PTE tables contain pointers to buffer pages */
		struct page **pages = (struct page **)pte_tables[i];
		unsigned long nr_pages = nr_pages_left;
		int j;

		map->pte_tables[i].page = get_zeroed_page(GFP_KERNEL);
		if (!map->pte_tables[i].page) {
			ret = -ENOMEM;
			goto err;
		}
		map->pages_created++;
		map->nr_pte_tables++;

		if (nr_pages > PTE_ENTRIES_MAX)
			nr_pages = PTE_ENTRIES_MAX;

		/* Create ref for this PTE table */
		ret = gnttab_grant_foreign_access(
			dom_id, virt_to_gfn(map->pte_tables[i].addr), true);
		if (ret < 0) {
			mc_dev_err(
				ret,
				"gnttab_grant_foreign_access failed:\t"
				"PTE table %d", i);
			goto err;
		}

		map->refs_granted++;
		buffer->data.refs[i] = ret;
		mc_dev_devel("mapped table %d ref %u for %lu pages",
			     i, buffer->data.refs[i], nr_pages);

		/* Create refs for pages */
		for (j = 0; j < nr_pages; j++) {
			ret = gnttab_grant_foreign_access(
				dom_id, page_to_gfn(pages[j]), readonly);
			if (ret < 0) {
				mc_dev_err(
					ret,
					"gnttab_grant_foreign_access failed:\t"
					"PTE %d pg %d", i, j);
				goto err;
			}

			map->refs_granted++;
			map->pte_tables[i].refs[j] = ret;
			map->nr_refs++;
			nr_pages_left--;
			mc_dev_devel("mapped [%d, %d] ref %u, left %lu",
				     i, j, map->pte_tables[i].refs[j],
				     nr_pages_left);
		}
	}

	buffer->info->nr_refs = map->nr_refs;
	buffer->info->addr = (uintptr_t)b_map->mmu;
	buffer->info->offset = b_map->offset;
	buffer->info->length = b_map->length;
	buffer->info->flags = b_map->flags;

	/* Auto-delete */
	map->deleter.object = map;
	map->deleter.delete = xen_fe_map_delete;
	tee_mmu_set_deleter(b_map->mmu, &map->deleter);

	mc_dev_devel("created map %p: refs=%u nr_pte_tables=%d",
		     map, map->nr_refs, map->nr_pte_tables);
	return map;

err:
	xen_fe_map_release(map, buffer);
	return ERR_PTR(ret);
}

/* DomU call to Dom0 */

/* Must be called under xfe->ring_mutex */
static inline void call_dom0(struct tee_xfe *xfe, enum tee_xen_domu_cmd cmd)
{
	WARN_ON(!xfe->ring_busy);

	xfe->domu_cmd_id++;
	if (!xfe->domu_cmd_id)
		xfe->domu_cmd_id++;

	/* Set command and ID */
	xfe->ring->domu.cmd = cmd;
	xfe->ring->domu.id = xfe->domu_cmd_id;
	mc_dev_devel("DomU -> Dom0 request %u id %u pid %d",
		     xfe->ring->domu.cmd, xfe->ring->domu.id, current->pid);
	/* Call */
	notify_remote_via_evtchn(xfe->evtchn_domu);
	wait_for_completion(&xfe->ring_completion);
}

/* Will be called back under xfe->ring_mutex */
static irqreturn_t xen_fe_irq_handler_domu_th(int intr, void *arg)
{
	struct tee_xfe *xfe = arg;

	WARN_ON(!xfe->ring_busy);

	/* Response to a domU command, our side of ring locked by us */
	mc_dev_devel("DomU -> Dom0 response %u id %u ret %d",
		     xfe->ring->domu.cmd, xfe->ring->domu.id,
		     xfe->ring->domu.otherend_ret);
	xfe->ring->domu.cmd = TEE_XEN_DOMU_NONE;
	xfe->ring->domu.id = 0;
	complete(&xfe->ring_completion);

	return IRQ_HANDLED;
}

/* MC protocol interface */

static int xen_mc_get_version(struct mc_version_info *version_info)
{
	struct tee_xfe *xfe = l_ctx.xfe;

	ring_get(xfe);
	/* Call */
	call_dom0(xfe, TEE_XEN_GET_VERSION);
	/* Out */
	memcpy(version_info, &xfe->ring->domu.version_info,
	       sizeof(*version_info));
	ring_put(xfe);
	return xfe->ring->domu.otherend_ret;
}

static int xen_mc_open_session(struct mcp_session *session,
			       struct mcp_open_info *info)
{
	struct tee_xfe *xfe = l_ctx.xfe;
	struct xen_fe_mc_session *fe_mc_session;
	struct tee_xen_buffer *ta_buffer = &xfe->buffers[1];
	struct tee_xen_buffer *tci_buffer = &xfe->buffers[0];
	struct xen_fe_map *ta_map = NULL;
	struct xen_fe_map *tci_map = NULL;
	enum tee_xen_domu_cmd cmd;
	int ret;

	fe_mc_session = kzalloc(sizeof(*fe_mc_session), GFP_KERNEL);
	if (!fe_mc_session)
		return -ENOMEM;

	INIT_LIST_HEAD(&fe_mc_session->list);
	init_completion(&fe_mc_session->completion);
	fe_mc_session->session = session;

	ring_get(xfe);
	/* In */
	xfe->ring->domu.uuid = *info->uuid;
	if (info->type == TEE_MC_UUID) {
		cmd = TEE_XEN_MC_OPEN_SESSION;
	} else {
		struct mcp_buffer_map b_map;

		cmd = TEE_XEN_MC_OPEN_TRUSTLET;
		tee_mmu_buffer(info->ta_mmu, &b_map);
		ta_map = xen_fe_map_create(ta_buffer, &b_map,
					   xfe->xdev->otherend_id);
		if (IS_ERR(ta_map)) {
			ret = PTR_ERR(ta_map);
			goto out;
		}
	}

	/* Convert IPAs to grant references in-place */
	if (info->tci_mmu) {
		struct mcp_buffer_map b_map;

		tee_mmu_buffer(info->tci_mmu, &b_map);
		tci_map = xen_fe_map_create(tci_buffer, &b_map,
					    xfe->xdev->otherend_id);
		if (IS_ERR(tci_map)) {
			ret = PTR_ERR(tci_map);
			goto out;
		}
	} else {
		tci_buffer->info->flags = 0;
	}

	/* Call */
	call_dom0(xfe, cmd);
	/* Out */
	ret = xfe->ring->domu.otherend_ret;
	if (!ret)
		session->sid = xfe->ring->domu.session_id;

out:
	if (!ret) {
		mutex_lock(&l_ctx.mc_sessions_lock);
		list_add_tail(&fe_mc_session->list, &l_ctx.mc_sessions);
		mutex_unlock(&l_ctx.mc_sessions_lock);
	} else {
		kfree(fe_mc_session);
	}

	/* Release the PMD and PTEs, but not the pages so they remain pinned */
	xen_fe_map_release_pmd(ta_map, ta_buffer);
	xen_fe_map_release_pmd(tci_map, tci_buffer);

	ring_put(xfe);
	return ret;
}

static int xen_mc_close_session(struct mcp_session *session)
{
	struct tee_xfe *xfe = l_ctx.xfe;
	struct xen_fe_mc_session *fe_mc_session;
	int ret;

	fe_mc_session = find_mc_session(session->sid);
	if (!fe_mc_session)
		return -ENXIO;

	ring_get(xfe);
	/* In */
	xfe->ring->domu.session_id = session->sid;
	/* Call */
	call_dom0(xfe, TEE_XEN_MC_CLOSE_SESSION);
	/* Out */
	ret = xfe->ring->domu.otherend_ret;
	if (!ret) {
		mutex_lock(&l_ctx.mc_sessions_lock);
		session->state = MCP_SESSION_CLOSED;
		list_del(&fe_mc_session->list);
		mutex_unlock(&l_ctx.mc_sessions_lock);
		kfree(fe_mc_session);
	}

	ring_put(xfe);
	return ret;
}

static int xen_mc_notify(struct mcp_session *session)
{
	struct tee_xfe *xfe = l_ctx.xfe;
	int ret;

	mc_dev_devel("MC notify session %x", session->sid);
	ring_get(xfe);
	/* In */
	xfe->ring->domu.session_id = session->sid;
	/* Call */
	call_dom0(xfe, TEE_XEN_MC_NOTIFY);
	/* Out */
	ret = xfe->ring->domu.otherend_ret;
	ring_put(xfe);
	return ret;
}

static int xen_mc_wait(struct mcp_session *session, s32 timeout)
{
	struct tee_xfe *xfe = l_ctx.xfe;
	struct xen_fe_mc_session *fe_mc_session;
	int ret;

	/* Locked by caller so no two waits can happen on one session */
	fe_mc_session = find_mc_session(session->sid);
	if (!fe_mc_session)
		return -ENXIO;

	fe_mc_session->ret = 0;

	mc_dev_devel("MC wait session %x", session->sid);
	ring_get(xfe);
	/* In */
	xfe->ring->domu.session_id = session->sid;
	xfe->ring->domu.timeout = timeout;
	/* Call */
	call_dom0(xfe, TEE_XEN_MC_WAIT);
	/* Out */
	ret = xfe->ring->domu.otherend_ret;
	ring_put(xfe);

	if (ret)
		return ret;

	/* Now wait for notification from Dom0 */
	ret = wait_for_completion_interruptible(&fe_mc_session->completion);
	if (!ret)
		ret = fe_mc_session->ret;

	return ret;
}

static int xen_mc_map(u32 session_id, struct tee_mmu *mmu, u32 *sva)
{
	struct tee_xfe *xfe = l_ctx.xfe;
	struct tee_xen_buffer *buffer = &xfe->buffers[0];
	struct mcp_buffer_map b_map;
	struct xen_fe_map *map = NULL;
	int ret;

	ring_get(xfe);
	/* In */
	xfe->ring->domu.session_id = session_id;
	tee_mmu_buffer(mmu, &b_map);
	map = xen_fe_map_create(buffer, &b_map, xfe->xdev->otherend_id);
	if (IS_ERR(map)) {
		ret = PTR_ERR(map);
		goto out;
	}

	/* Call */
	call_dom0(xfe, TEE_XEN_MC_MAP);
	/* Out */
	ret = xfe->ring->domu.otherend_ret;
	if (!ret) {
		*sva = buffer->info->sva;
		atomic_inc(&g_ctx.c_maps);
	}

out:
	xen_fe_map_release_pmd(map, buffer);
	ring_put(xfe);
	return ret;
}

static int xen_mc_unmap(u32 session_id, const struct mcp_buffer_map *map)
{
	struct tee_xfe *xfe = l_ctx.xfe;
	struct tee_xen_buffer *buffer = &xfe->buffers[0];
	int ret;

	ring_get(xfe);
	/* In */
	xfe->ring->domu.session_id = session_id;
	buffer->info->length = map->length;
	buffer->info->sva = map->secure_va;
	/* Call */
	call_dom0(xfe, TEE_XEN_MC_UNMAP);
	/* Out */
	ret = xfe->ring->domu.otherend_ret;
	if (!ret)
		atomic_dec(&g_ctx.c_maps);

	ring_put(xfe);
	return ret;
}

static int xen_mc_get_err(struct mcp_session *session, s32 *err)
{
	struct tee_xfe *xfe = l_ctx.xfe;
	int ret;

	ring_get(xfe);
	/* In */
	xfe->ring->domu.session_id = session->sid;
	/* Call */
	call_dom0(xfe, TEE_XEN_MC_GET_ERR);
	/* Out */
	ret = xfe->ring->domu.otherend_ret;
	if (!ret)
		*err = xfe->ring->domu.err;

	mc_dev_devel("MC get_err session %x err %d", session->sid, *err);
	ring_put(xfe);
	return ret;
}

/* GP protocol interface */

static int xen_gp_register_shared_mem(struct tee_mmu *mmu, u32 *sva,
				      struct gp_return *gp_ret)
{
	struct tee_xfe *xfe = l_ctx.xfe;
	struct tee_xen_buffer *buffer = &xfe->buffers[0];
	struct mcp_buffer_map b_map;
	struct xen_fe_map *map = NULL;
	int ret;

	ring_get(xfe);
	/* In */
	tee_mmu_buffer(mmu, &b_map);
	map = xen_fe_map_create(buffer, &b_map, xfe->xdev->otherend_id);
	if (IS_ERR(map)) {
		ret = PTR_ERR(map);
		goto out;
	}

	/* Call */
	call_dom0(xfe, TEE_XEN_GP_REGISTER_SHARED_MEM);
	/* Out */
	ret = xfe->ring->domu.otherend_ret;
	if (!ret) {
		*sva = buffer->info->sva;
		atomic_inc(&g_ctx.c_maps);
	}

	if (xfe->ring->domu.gp_ret.origin)
		*gp_ret = xfe->ring->domu.gp_ret;

out:
	xen_fe_map_release_pmd(map, buffer);
	ring_put(xfe);
	return ret;
}

static int xen_gp_release_shared_mem(struct mcp_buffer_map *map)
{
	struct tee_xfe *xfe = l_ctx.xfe;
	struct tee_xen_buffer *buffer = &xfe->buffers[0];
	int ret;

	ring_get(xfe);
	/* In */
	buffer->info->addr = (uintptr_t)map->mmu;
	buffer->info->length = map->length;
	buffer->info->flags = map->flags;
	buffer->info->sva = map->secure_va;
	/* Call */
	call_dom0(xfe, TEE_XEN_GP_RELEASE_SHARED_MEM);
	/* Out */
	ret = xfe->ring->domu.otherend_ret;
	if (!ret)
		atomic_dec(&g_ctx.c_maps);

	ring_put(xfe);
	return ret;
}

static int xen_gp_open_session(struct iwp_session *session,
			       const struct mc_uuid_t *uuid,
			       const struct iwp_buffer_map *b_maps,
			       struct interworld_session *iws,
			       struct interworld_session *op_iws,
			       struct gp_return *gp_ret)
{
	struct tee_xfe *xfe = l_ctx.xfe;
	struct xen_fe_gp_operation operation = { .ret = 0 };
	struct xen_fe_map *maps[4] = { NULL, NULL, NULL, NULL };
	int i, ret;

	/* Prepare operation first not to be racey */
	INIT_LIST_HEAD(&operation.list);
	init_completion(&operation.completion);
	/* Note: slot is a unique identifier for a session/operation */
	operation.slot = session->slot;
	operation.gp_ret = gp_ret;
	operation.iws = iws;
	mutex_lock(&l_ctx.gp_operations_lock);
	list_add_tail(&operation.list, &l_ctx.gp_operations);
	mutex_unlock(&l_ctx.gp_operations_lock);

	ring_get(xfe);
	/* The operation may contain tmpref's to map */
	for (i = 0; i < TEE_BUFFERS; i++) {
		if (!b_maps[i].map.addr) {
			xfe->buffers[i].info->flags = 0;
			continue;
		}

		maps[i] = xen_fe_map_create(&xfe->buffers[i], &b_maps[i].map,
					    xfe->xdev->otherend_id);
		if (IS_ERR(maps[i])) {
			ret = PTR_ERR(maps[i]);
			goto err;
		}
	}

	/* In */
	xfe->ring->domu.uuid = *uuid;
	xfe->ring->domu.operation_id = session->slot;
	xfe->ring->domu.iws = *op_iws;
	/* Call */
	call_dom0(xfe, TEE_XEN_GP_OPEN_SESSION);
	/* Out */
	ret = xfe->ring->domu.otherend_ret;
err:
	for (i = 0; i < TEE_BUFFERS; i++)
		xen_fe_map_release_pmd(maps[i], &xfe->buffers[i]);

	ring_put(xfe);
	if (ret) {
		mutex_lock(&l_ctx.gp_operations_lock);
		list_del(&operation.list);
		mutex_unlock(&l_ctx.gp_operations_lock);
		return ret;
	}

	/* Now wait for notification from Dom0 */
	wait_for_completion(&operation.completion);
	/* FIXME origins? */
	return operation.ret;
}

static int xen_gp_close_session(struct iwp_session *session)
{
	struct tee_xfe *xfe = l_ctx.xfe;
	struct xen_fe_gp_operation operation = { .ret = 0 };
	int ret;

	/* Prepare operation first not to be racey */
	INIT_LIST_HEAD(&operation.list);
	init_completion(&operation.completion);
	/* Note: slot is a unique identifier for a session/operation */
	operation.slot = session->slot;
	mutex_lock(&l_ctx.gp_operations_lock);
	list_add_tail(&operation.list, &l_ctx.gp_operations);
	mutex_unlock(&l_ctx.gp_operations_lock);

	ring_get(xfe);
	/* In */
	xfe->ring->domu.session_id = session->sid;
	xfe->ring->domu.operation_id = session->slot;
	/* Call */
	call_dom0(xfe, TEE_XEN_GP_CLOSE_SESSION);
	/* Out */
	ret = xfe->ring->domu.otherend_ret;
	ring_put(xfe);
	if (ret) {
		mutex_lock(&l_ctx.gp_operations_lock);
		list_del(&operation.list);
		mutex_unlock(&l_ctx.gp_operations_lock);
		return ret;
	}

	/* Now wait for notification from Dom0 */
	wait_for_completion(&operation.completion);
	return operation.ret;
}

static int xen_gp_invoke_command(struct iwp_session *session,
				 const struct iwp_buffer_map *b_maps,
				 struct interworld_session *iws,
				 struct gp_return *gp_ret)
{
	struct tee_xfe *xfe = l_ctx.xfe;
	struct xen_fe_gp_operation operation = { .ret = 0 };
	struct xen_fe_map *maps[4] = { NULL, NULL, NULL, NULL };
	int i, ret;

	/* Prepare operation first not to be racey */
	INIT_LIST_HEAD(&operation.list);
	init_completion(&operation.completion);
	/* Note: slot is a unique identifier for a session/operation */
	operation.slot = session->slot;
	operation.gp_ret = gp_ret;
	operation.iws = iws;
	mutex_lock(&l_ctx.gp_operations_lock);
	list_add_tail(&operation.list, &l_ctx.gp_operations);
	mutex_unlock(&l_ctx.gp_operations_lock);

	ring_get(xfe);
	/* The operation is in op_iws and may contain tmpref's to map */
	for (i = 0; i < TEE_BUFFERS; i++) {
		if (!b_maps[i].map.addr) {
			xfe->buffers[i].info->flags = 0;
			continue;
		}

		maps[i] = xen_fe_map_create(&xfe->buffers[i], &b_maps[i].map,
					    xfe->xdev->otherend_id);
		if (IS_ERR(maps[i])) {
			ret = PTR_ERR(maps[i]);
			goto err;
		}
	}

	/* In */
	xfe->ring->domu.session_id = session->sid;
	xfe->ring->domu.operation_id = session->slot;
	xfe->ring->domu.iws = *iws;
	/* Call */
	call_dom0(xfe, TEE_XEN_GP_INVOKE_COMMAND);
	/* Out */
	ret = xfe->ring->domu.otherend_ret;
err:
	for (i = 0; i < TEE_BUFFERS; i++)
		xen_fe_map_release_pmd(maps[i], &xfe->buffers[i]);

	ring_put(xfe);
	if (ret) {
		mutex_lock(&l_ctx.gp_operations_lock);
		list_del(&operation.list);
		mutex_unlock(&l_ctx.gp_operations_lock);
		return ret;
	}

	/* Now wait for notification from Dom0 */
	wait_for_completion(&operation.completion);
	/* FIXME origins? */
	return operation.ret;
}

static int xen_gp_request_cancellation(u64 slot)
{
	struct tee_xfe *xfe = l_ctx.xfe;
	int ret;

	ring_get(xfe);
	/* In */
	xfe->ring->domu.operation_id = slot;
	/* Call */
	call_dom0(xfe, TEE_XEN_GP_REQUEST_CANCELLATION);
	/* Out */
	ret = xfe->ring->domu.otherend_ret;
	ring_put(xfe);
	return ret;
}

/* Device */

static inline void xfe_release(struct tee_xfe *xfe)
{
	int i;

	if (xfe->irq_domu >= 0)
		unbind_from_irqhandler(xfe->irq_domu, xfe);

	if (xfe->irq_dom0 >= 0)
		unbind_from_irqhandler(xfe->irq_dom0, xfe);

	if (xfe->evtchn_domu >= 0)
		xenbus_free_evtchn(xfe->xdev, xfe->evtchn_domu);

	if (xfe->evtchn_dom0 >= 0)
		xenbus_free_evtchn(xfe->xdev, xfe->evtchn_dom0);

	for (i = 0; i < TEE_BUFFERS; i++) {
		if (!xfe->buffers[i].data.page)
			break;

		gnttab_end_foreign_access(xfe->ring->domu.buffers[i].pmd_ref, 0,
					  xfe->buffers[i].data.page);
		free_page(xfe->buffers[i].data.page);
	}

	if (xfe->ring_ul) {
		gnttab_end_foreign_access(xfe->ring_ref, 0, xfe->ring_ul);
		free_page(xfe->ring_ul);
	}

	kfree(xfe);
}

static inline struct tee_xfe *xfe_create(struct xenbus_device *xdev)
{
	struct tee_xfe *xfe;
	struct xenbus_transaction trans;
	int i, ret = -ENOMEM;

	/* Alloc */
	xfe = tee_xfe_create(xdev);
	if (!xfe)
		return ERR_PTR(-ENOMEM);

	/* Create shared information buffer */
	xfe->ring_ul = get_zeroed_page(GFP_KERNEL);
	if (!xfe->ring_ul)
		goto err;

	/* Connect */
	ret = xenbus_grant_ring(xfe->xdev, xfe->ring, 1, &xfe->ring_ref);
	if (ret < 0)
		goto err;

	for (i = 0; i < TEE_BUFFERS; i++) {
		xfe->buffers[i].data.page = get_zeroed_page(GFP_KERNEL);
		if (!xfe->buffers[i].data.page)
			goto err;

		ret = xenbus_grant_ring(xfe->xdev, xfe->buffers[i].data.addr, 1,
					&xfe->ring->domu.buffers[i].pmd_ref);
		if (ret < 0)
			goto err;

		xfe->buffers[i].info = &xfe->ring->domu.buffers[i];
	}

	ret = xenbus_alloc_evtchn(xfe->xdev, &xfe->evtchn_domu);
	if (ret)
		goto err;

	ret = xenbus_alloc_evtchn(xfe->xdev, &xfe->evtchn_dom0);
	if (ret)
		goto err;

	ret = bind_evtchn_to_irqhandler(xfe->evtchn_domu,
					xen_fe_irq_handler_domu_th, 0,
					"tee_fe_domu", xfe);
	if (ret < 0)
		goto err;

	xfe->irq_domu = ret;

	ret = bind_evtchn_to_irqhandler(xfe->evtchn_dom0,
					xen_fe_irq_handler_dom0_th, 0,
					"tee_fe_dom0", xfe);
	if (ret < 0)
		goto err;

	xfe->irq_dom0 = ret;

	/* Publish */
	do {
		ret = xenbus_transaction_start(&trans);
		if (ret) {
			xenbus_dev_fatal(xfe->xdev, ret,
					 "failed to start transaction");
			goto err_transaction;
		}

		/* Ring is one page to support older kernels */
		ret = xenbus_printf(trans, xfe->xdev->nodename,
				    "ring-ref", "%u", xfe->ring_ref);
		if (ret) {
			xenbus_dev_fatal(xfe->xdev, ret,
					 "failed to write ring ref");
			goto err_transaction;
		}

		ret = xenbus_printf(trans, xfe->xdev->nodename,
				    "pte-entries-max", "%u",
				    PTE_ENTRIES_MAX);
		if (ret) {
			xenbus_dev_fatal(xfe->xdev, ret,
					 "failed to write PTE entries max");
			goto err_transaction;
		}

		ret = xenbus_printf(trans, xfe->xdev->nodename,
				    "event-channel-domu", "%u",
				    xfe->evtchn_domu);
		if (ret) {
			xenbus_dev_fatal(xfe->xdev, ret,
					 "failed to write event channel domu");
			goto err_transaction;
		}

		ret = xenbus_printf(trans, xfe->xdev->nodename,
				    "event-channel-dom0", "%u",
				    xfe->evtchn_dom0);
		if (ret) {
			xenbus_dev_fatal(xfe->xdev, ret,
					 "failed to write event channel dom0");
			goto err_transaction;
		}

		ret = xenbus_printf(trans, xfe->xdev->nodename,
				    "domu-version", "%u", TEE_XEN_VERSION);
		if (ret) {
			xenbus_dev_fatal(xfe->xdev, ret,
					 "failed to write version");
			goto err_transaction;
		}

		ret = xenbus_transaction_end(trans, 0);
		if (ret) {
			if (ret == -EAGAIN)
				mc_dev_devel("retry");
			else
				xenbus_dev_fatal(xfe->xdev, ret,
						 "failed to end transaction");
		}
	} while (ret == -EAGAIN);

	mc_dev_devel("evtchn domu=%u dom0=%u version=%u",
		     xfe->evtchn_domu, xfe->evtchn_dom0, TEE_XEN_VERSION);
	xenbus_switch_state(xfe->xdev, XenbusStateInitialised);
	return xfe;

err_transaction:
err:
	xenbus_switch_state(xfe->xdev, XenbusStateClosed);
	xfe_release(xfe);
	return ERR_PTR(ret);
}

static const struct xenbus_device_id xen_fe_ids[] = {
	{ "tee_xen" },
	{ "" }
};

static int xen_fe_probe(struct xenbus_device *xdev,
			const struct xenbus_device_id *id)
{
	int ret;

	ret = l_ctx.probe();
	if (ret)
		return ret;

	l_ctx.xfe = xfe_create(xdev);
	if (IS_ERR(l_ctx.xfe))
		return PTR_ERR(l_ctx.xfe);

	INIT_WORK(&l_ctx.xfe->work, xen_fe_irq_handler_dom0_bh);

	return 0;
}

static int xen_fe_remove(struct xenbus_device *dev)
{
	struct tee_xfe *xfe = l_ctx.xfe;

	tee_xfe_put(xfe);
	return 0;
}

static void xen_fe_backend_changed(struct xenbus_device *xdev,
				   enum xenbus_state be_state)
{
	struct tee_xfe *xfe = l_ctx.xfe;

	mc_dev_devel("be state changed to %d", be_state);

	if (be_state == l_ctx.last_be_state) {
		/* Protection against duplicated notifications (TBUG-1387) */
		mc_dev_devel("be state (%d) already set... ignoring", be_state);
		return;
	}

	switch (be_state) {
	case XenbusStateUnknown:
	case XenbusStateInitialising:
	case XenbusStateInitWait:
	case XenbusStateInitialised:
		break;
	case XenbusStateConnected:
		if (l_ctx.start())
			xenbus_switch_state(xfe->xdev, XenbusStateClosing);
		else
			xenbus_switch_state(xfe->xdev, XenbusStateConnected);
		break;
	case XenbusStateClosing:
	case XenbusStateClosed:
	case XenbusStateReconfiguring:
	case XenbusStateReconfigured:
		break;
	}

	/* Refresh last back-end state */
	l_ctx.last_be_state = be_state;
}

static struct tee_protocol_fe_call_ops fe_call_ops = {
	/* MC protocol interface */
	.mc_get_version = xen_mc_get_version,
	.mc_open_session = xen_mc_open_session,
	.mc_close_session = xen_mc_close_session,
	.mc_map = xen_mc_map,
	.mc_unmap = xen_mc_unmap,
	.mc_notify = xen_mc_notify,
	.mc_wait = xen_mc_wait,
	.mc_get_err = xen_mc_get_err,
	/* GP protocol interface */
	.gp_register_shared_mem = xen_gp_register_shared_mem,
	.gp_release_shared_mem = xen_gp_release_shared_mem,
	.gp_open_session = xen_gp_open_session,
	.gp_close_session = xen_gp_close_session,
	.gp_invoke_command = xen_gp_invoke_command,
	.gp_request_cancellation = xen_gp_request_cancellation,
};

static struct xenbus_driver xen_fe_driver = {
	.ids  = xen_fe_ids,
	.probe = xen_fe_probe,
	.remove = xen_fe_remove,
	.otherend_changed = xen_fe_backend_changed,
};

static int xen_fe_early_init(int (*probe)(void), int (*start)(void))
{
	int ret;

	l_ctx.probe = probe;
	l_ctx.start = start;
	mutex_init(&l_ctx.mc_sessions_lock);
	INIT_LIST_HEAD(&l_ctx.mc_sessions);
	mutex_init(&l_ctx.gp_operations_lock);
	INIT_LIST_HEAD(&l_ctx.gp_operations);
	ret = xenbus_register_frontend(&xen_fe_driver);
	if (ret)
		return ret;

	/* Stop init as we have our own probe/start mechanism */
	return 1;
}

static void xen_fe_exit(void)
{
	xenbus_unregister_driver(&xen_fe_driver);
}

static struct tee_protocol_ops protocol_ops = {
	.name = "XEN FE",
	.early_init = xen_fe_early_init,
	.exit = xen_fe_exit,
	.fe_call_ops = &fe_call_ops,
	.fe_uses_pages_and_vas = true,
};

struct tee_protocol_ops *xen_fe_check(void)
{
	if (!xen_domain() || xen_initial_domain())
		return NULL;

	return &protocol_ops;
}

#endif /* CONFIG_XEN */
