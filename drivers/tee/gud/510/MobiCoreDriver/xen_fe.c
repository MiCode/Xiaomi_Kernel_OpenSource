// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017-2020 TRUSTONIC LIMITED
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
#include <linux/kernel.h>	/* container_of */

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
	/* Last back-end state,
	 * to overcome an issue in some Xen implementations
	 */
	int			last_be_state;
} l_ctx;

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

	mc_dev_devel("Dom0 -> DomU command %u id %u cmd ret %d",
		     xfe->ring->be2fe_data.cmd, xfe->ring->be2fe_data.id,
		     xfe->ring->be2fe_data.cmd_ret);

	xfe->ring->be2fe_data.cmd_ret = protocol_fe_dispatch(&xfe->pfe);
	if (xfe->ring->be2fe_data.cmd_ret)
		mc_dev_err(xfe->ring->be2fe_data.cmd_ret,
			   "Dom0 -> DomU result %u id %u",
			   xfe->ring->be2fe_data.cmd, xfe->ring->be2fe_data.id);
	else
		mc_dev_devel("Dom0 -> DomU result %u id %u ret %d",
			     xfe->ring->be2fe_data.cmd,
			     xfe->ring->be2fe_data.id,
			     xfe->ring->be2fe_data.cmd_ret);

	notify_remote_via_evtchn(xfe->evtchn_dom0);
}

/* Buffer management */

static void xen_fe_map_release_pmd_ptes(
	struct protocol_fe_map *map,
	const struct tee_protocol_buffer *buffer)
{
	int i;

	gnttab_end_foreign_access(buffer->pmd_ref, true, 0);
	map->refs_shared--;
	mc_dev_devel("unmapped PMD ref %llu", buffer->pmd_ref);

	for (i = 0; i < map->nr_pte_tables; i++) {
		gnttab_end_foreign_access(
			map->mmu->pmd_table.entries[i], true, 0);
		map->refs_shared--;
		mc_dev_devel("unmapped PTE %d ref %llu",
			     i, map->mmu->pmd_table.entries[i]);
	}
}

static void xen_fe_map_release(struct protocol_fe_map *map)
{
	int nr_pages_left = map->nr_pages;
	int i;

	/* Unshare buffer pages */
	for (i = 0; i < map->nr_pte_tables; i++) {
		int j, nr_pages = nr_pages_left;

		if (nr_pages > PTE_ENTRIES_MAX)
			nr_pages = PTE_ENTRIES_MAX;

		for (j = 0; j < nr_pages; j++) {
			gnttab_end_foreign_access(
				map->mmu->pte_tables[i].entries[j],
				map->readonly, 0);
			map->refs_shared--;
			nr_pages_left--;
			mc_dev_devel("unmapped [%d, %d] ref %llu, left %d",
				     i, j, map->mmu->pte_tables[i].entries[j],
				     nr_pages_left);
		}
	}

	/* Check memory leak */
	if (map->refs_shared) {
		mc_dev_err(-EUCLEAN, "leak detected: still shared %d",
			   map->refs_shared);
		/* Do not free the map, so the error can be spotted */
		return;
	}

	mc_dev_devel("freed map %p: nr_pages %u, nr_pte_tables %d",
		     map, map->nr_pages, map->nr_pte_tables);
	tee_mmu_put(map->mmu);
	kfree(map);
	atomic_dec(&g_ctx.c_vm_maps);
}

static void xen_fe_map_delete(void *arg)
{
	struct protocol_fe_map *map = arg;

	xen_fe_map_release(map);
}

static struct protocol_fe_map *xen_fe_map_create(
	struct tee_protocol_buffer *buffer,
	const struct mcp_buffer_map *b_map,
	struct protocol_fe *pfe)
{
	struct tee_xfe *xfe = container_of(pfe, struct tee_xfe, pfe);
	int dom_id = xfe->xdev->otherend_id;
	/* b_map describes the PMD which contains pointers to PTE tables */
	uintptr_t *pte_tables = (uintptr_t *)(uintptr_t)b_map->addr;
	struct protocol_fe_map *map;
	unsigned long nr_pte_tables =
		(b_map->mmu->nr_pages + PTE_ENTRIES_MAX - 1) / PTE_ENTRIES_MAX;
	unsigned long nr_pages_left = b_map->mmu->nr_pages;
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

	atomic_inc(&g_ctx.c_vm_maps);
	map->readonly = readonly;

	/* Allocate the mmu to share with Dom0 */
	map->mmu = tee_mmu_create_and_init();
	if (IS_ERR(map->mmu)) {
		ret = PTR_ERR(map->mmu);
		goto err_mmu_create;
	}

	map->mmu->pmd_table.page = get_zeroed_page(GFP_KERNEL);
	if (!map->mmu->pmd_table.page) {
		ret = -ENOMEM;
		goto err_pmd_create;
	}

	map->mmu->pages_created++;

	/* Create ref for this PMD table */
	ret = gnttab_grant_foreign_access(dom_id,
					  virt_to_gfn(map->mmu->pmd_table.addr),
					  true);
	if (ret < 0) {
		mc_dev_err(ret, "gnttab_grant_foreign_access failed for PMD");
		goto err_pmd_grant;
	}

	map->refs_shared++;
	buffer->pmd_ref = ret;
	mc_dev_devel("mapped PMD ref %llu", buffer->pmd_ref);

	for (i = 0; i < nr_pte_tables; i++) {
		/* As expected, PTE tables contain pointers to buffer pages */
		struct page **pages = (struct page **)pte_tables[i];
		unsigned long nr_pages = nr_pages_left;
		int j;

		map->mmu->pte_tables[i].page = get_zeroed_page(GFP_KERNEL);
		if (!map->mmu->pte_tables[i].page) {
			ret = -ENOMEM;
			goto err;
		}

		map->mmu->pages_created++;
		map->nr_pte_tables++;
		map->mmu->nr_pmd_entries++;

		if (nr_pages > PTE_ENTRIES_MAX)
			nr_pages = PTE_ENTRIES_MAX;

		/* Create ref for this PTE table */
		ret = gnttab_grant_foreign_access(
			dom_id,
			virt_to_gfn(map->mmu->pte_tables[i].addr),
			true);
		if (ret < 0) {
			mc_dev_err(
				ret,
				"gnttab_grant_foreign_access failed:\t"
				"PTE table %d", i);
			goto err;
		}

		map->refs_shared++;
		map->mmu->pmd_table.entries[i] = ret;
		mc_dev_devel("mapped PTE %d ref %llu for %lu pages",
			     i, map->mmu->pmd_table.entries[i], nr_pages);

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

			map->refs_shared++;
			map->mmu->pte_tables[i].entries[j] = ret;
			map->nr_pages++;
			nr_pages_left--;
			mc_dev_devel("mapped [%d, %d] ref %llu, left %lu",
				     i, j, map->mmu->pte_tables[i].entries[j],
				     nr_pages_left);
		}
	}

	buffer->addr = (uintptr_t)b_map->mmu;
	buffer->offset = b_map->offset;
	buffer->length = b_map->length;
	buffer->flags = b_map->flags;

	/* Auto-delete */
	map->deleter.object = map;
	map->deleter.delete = xen_fe_map_delete;
	tee_mmu_set_deleter(b_map->mmu, &map->deleter);

	mc_dev_devel("created map %p: refs=%u nr_pte_tables=%d",
		     map, map->nr_pages, map->nr_pte_tables);
	return map;

err:
	xen_fe_map_release_pmd_ptes(map, buffer);
	xen_fe_map_release(map);
err_pmd_grant:
	free_page(map->mmu->pmd_table.page);
err_pmd_create:
	tee_mmu_put(map->mmu);
err_mmu_create:
	kfree(map);
	atomic_dec(&g_ctx.c_vm_maps);
	return ERR_PTR(ret);
}

/* DomU call to Dom0 */

/* Must be called under xfe->pfe.protocol_mutex */
static inline int call_dom0(struct protocol_fe *pfe)
{
	struct tee_xfe *xfe = container_of(pfe, struct tee_xfe, pfe);

	/* Call */
	notify_remote_via_evtchn(xfe->evtchn_domu);
	wait_for_completion(&xfe->ring_completion);
	return 0;
}

/* Will be called back under xfe->pfe.protocol_mutex */
static irqreturn_t xen_fe_irq_handler_domu_th(int intr, void *arg)
{
	struct tee_xfe *xfe = arg;

	WARN_ON(!xfe->pfe.protocol_busy);

	/* Response to a domU command, our side of ring locked by us */
	mc_dev_devel("DomU -> Dom0 response %u id %u ret %d",
		     xfe->ring->fe2be_data.cmd, xfe->ring->fe2be_data.id,
		     xfe->ring->fe2be_data.otherend_ret);
	xfe->ring->fe2be_data.cmd = TEE_FE_NONE;
	xfe->ring->fe2be_data.id = 0;
	complete(&xfe->ring_completion);

	return IRQ_HANDLED;
}

/* Device */

static inline void xfe_release(struct tee_xfe *xfe)
{
	if (xfe->irq_domu >= 0)
		unbind_from_irqhandler(xfe->irq_domu, xfe);

	if (xfe->irq_dom0 >= 0)
		unbind_from_irqhandler(xfe->irq_dom0, xfe);

	if (xfe->evtchn_domu >= 0)
		xenbus_free_evtchn(xfe->xdev, xfe->evtchn_domu);

	if (xfe->evtchn_dom0 >= 0)
		xenbus_free_evtchn(xfe->xdev, xfe->evtchn_dom0);

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
	grant_ref_t ref;
	int ret = -ENOMEM;

	/* Alloc */
	xfe = tee_xfe_create(xdev);
	if (IS_ERR(xfe))
		return xfe;

	/* Create shared information buffer */
	xfe->ring_ul = get_zeroed_page(GFP_KERNEL);
	if (!xfe->ring_ul)
		goto err;

	/* Share both communication channels with parent */
	xfe->pfe.fe2be_data = &xfe->ring->fe2be_data;
	xfe->pfe.be2fe_data = &xfe->ring->be2fe_data;

	/* Connect */
	ret = xenbus_grant_ring(xfe->xdev, xfe->ring, 1, &ref);
	if (ret < 0)
		goto err;

	xfe->ring_ref = ref;

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
				    "ring-ref", "%llu", xfe->ring_ref);
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
	struct tee_xfe *xfe;
	int ret;

	ret = l_ctx.probe();
	if (ret)
		return ret;

	xfe = xfe_create(xdev);
	if (IS_ERR(xfe))
		return PTR_ERR(xfe);

	l_ctx.xfe = xfe;
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
	.call_be = call_dom0,
	.fe_map_create = xen_fe_map_create,
	.fe_map_release_pmd_ptes = xen_fe_map_release_pmd_ptes,
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
