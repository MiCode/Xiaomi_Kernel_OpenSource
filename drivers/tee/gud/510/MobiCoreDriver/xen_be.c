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

#ifdef CONFIG_XEN

#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#if KERNEL_VERSION(4, 0, 0) > LINUX_VERSION_CODE
#include <xen/balloon.h>
#endif

#include "platform.h"		/* MC_XENBUS_MAP_RING_VALLOC_4_1 */
#include "main.h"
#include "admin.h"		/* tee_object* */
#include "client.h"		/* Consider other VMs as clients */
#include "mcp.h"		/* mcp_get_version */
#include "nq.h"
#include "xen_common.h"
#include "xen_be.h"

#define vaddr(page) ((unsigned long)pfn_to_kaddr(page_to_pfn(page)))

static struct {
	char			vm_id[16];
	struct list_head	xfes;
	struct mutex		xfes_mutex;	/* Protect the above */
} l_ctx;

/* Maps */

struct xen_be_local_map {
	struct page		**pages;
	unsigned int		*handles;
	int			nr_pages;
};

struct xen_be_map {
	struct protocol_be_map	protocol_map;
	/* Xen specific */
	struct xen_be_local_map	buffer_map;
};

static inline int xen_be_alloc_pages(struct xen_be_local_map *map)
{
	map->pages = kcalloc(map->nr_pages, sizeof(*map->pages), GFP_KERNEL);
	if (!map->pages)
		return -ENOMEM;

#if KERNEL_VERSION(4, 0, 0) <= LINUX_VERSION_CODE
	return gnttab_alloc_pages(map->nr_pages, map->pages);
#else
	return alloc_xenballooned_pages(map->nr_pages, map->pages, false);
#endif
}

static inline void xen_be_free_pages(struct xen_be_local_map *map)
{
#if KERNEL_VERSION(4, 0, 0) <= LINUX_VERSION_CODE
	gnttab_free_pages(map->nr_pages, map->pages);
#else
	free_xenballooned_pages(map->nr_pages, map->pages, false);
#endif
	kfree(map->pages);
}

static inline int xen_be_map_pages(int dom_id, u64 *refs,
				   struct xen_be_local_map *map)
{
	struct gnttab_map_grant_ref *maps;
	int ret = -ENOMEM, i;

	map->handles = kcalloc(map->nr_pages, sizeof(*map->handles),
			       GFP_KERNEL);
	if (!map->handles)
		goto err_handles;

	maps = kcalloc(map->nr_pages, sizeof(*maps), GFP_KERNEL);
	if (!maps)
		goto err_maps;

	for (i = 0;  i < map->nr_pages; i++) {
		mc_dev_devel("map page #%d of %d ref %llu",
			     i, map->nr_pages, refs[i]);
		gnttab_set_map_op(&maps[i], vaddr(map->pages[i]),
				  GNTMAP_host_map | GNTMAP_readonly,
				  refs[i], dom_id);
	}

	ret = gnttab_map_refs(maps, NULL, map->pages, map->nr_pages);
	if (ret)
		goto err_map_refs;

	/* Pin pages */
	for (i = 0;  i < map->nr_pages; i++) {
		get_page(map->pages[i]);
		map->handles[i] = maps[i].handle;
	}

	kfree(maps);
	return 0;

err_map_refs:
	kfree(maps);
err_maps:
	kfree(map->handles);
err_handles:
	return ret;
}

static inline void xen_be_unmap_pages(struct xen_be_local_map *map)
{
	struct gnttab_unmap_grant_ref *unmaps;
	int i;

	for (i = 0; i < map->nr_pages; i++)
		put_page(map->pages[i]);

	unmaps = kcalloc(map->nr_pages, sizeof(*unmaps), GFP_KERNEL);
	if (!unmaps)
		return;

	for (i = 0; i < map->nr_pages; i++) {
		mc_dev_devel("unmap page #%d of %d", i, map->nr_pages);
		gnttab_set_unmap_op(&unmaps[i], vaddr(map->pages[i]),
				    GNTMAP_host_map | GNTMAP_readonly,
				    map->handles[i]);
	}

	gnttab_unmap_refs(unmaps, NULL, map->pages, map->nr_pages);
	kfree(unmaps);
	kfree(map->handles);
	map->handles = NULL;
}

static void xen_be_map_delete(struct protocol_be_map *map)
{
	struct xen_be_map *xen_map = container_of(map,
						  struct xen_be_map,
						  protocol_map);

	mc_dev_devel("unmap buffer");
	xen_be_unmap_pages(&xen_map->buffer_map);
	xen_be_free_pages(&xen_map->buffer_map);
	kfree(map);
	mc_dev_devel("freed xen map %p", map);
	atomic_dec(&g_ctx.c_vm_maps);
}

static struct protocol_be_map *xen_be_map_create(
	struct tee_protocol_buffer *buffer,
	struct protocol_fe *pfe)
{
	struct tee_xfe *xfe = container_of(pfe, struct tee_xfe, pfe);
	struct xen_be_map *map = NULL;
	u64 *buffer_refs;
	union tee_protocol_mmu_table pmd_table;
	unsigned long nr_pages =
		PAGE_ALIGN(buffer->offset + buffer->length) / PAGE_SIZE;
	int dom_id = xfe->xdev->otherend_id;
	int i, ret;

	/* PTEs mapping */
	struct xen_be_local_map ptes_map = {
		.nr_pages = (nr_pages + PTE_ENTRIES_MAX - 1) / PTE_ENTRIES_MAX,
	};

	/* PMD mapping */
	struct xen_be_local_map pmd_map = {
		.nr_pages = 1,
	};

	/* Map the PMD page */
	mc_dev_devel("map PMD");
	ret = xen_be_alloc_pages(&pmd_map);
	if (ret < 0)
		goto err_pmd_pages;

	ret = xen_be_map_pages(dom_id, &buffer->pmd_ref, &pmd_map);
	if (ret)
		goto err_pmd_map;

	pmd_table.page = vaddr(pmd_map.pages[0]);

	/* Map the PTE pages */
	mc_dev_devel("map %d PTEs", ptes_map.nr_pages);
	ret = xen_be_alloc_pages(&ptes_map);
	if (ret < 0)
		goto err_ptes_pages;

	ret = xen_be_map_pages(dom_id, pmd_table.refs, &ptes_map);
	if (ret)
		goto err_ptes_map;

	/* Create map */
	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (!map)
		goto err_map_create;

	atomic_inc(&g_ctx.c_vm_maps);
	map->buffer_map.nr_pages = nr_pages;

	/* Auto-delete */
	map->protocol_map.deleter.object = map;
	map->protocol_map.deleter.delete = (void(*)(void *))xen_be_map_delete;

	/* Map the buffer pages */
	mc_dev_devel("map %d buffer pages", map->buffer_map.nr_pages);
	ret = xen_be_alloc_pages(&map->buffer_map);
	if (ret < 0)
		goto err_buffer_pages;

	buffer_refs = kcalloc(nr_pages, sizeof(*buffer_refs), GFP_KERNEL);
	if (!buffer_refs) {
		ret = -ENOMEM;
		goto err_alloc_refs;
	}

	/* Gather all buffer refs */
	for (i = 0; i < ptes_map.nr_pages; i++) {
		int j, nr_pages_in_pte = nr_pages;
		u64 *refs = (void *)vaddr(ptes_map.pages[i]);

		if (nr_pages_in_pte > PTE_ENTRIES_MAX)
			nr_pages_in_pte = PTE_ENTRIES_MAX;

		for (j = 0; j < nr_pages_in_pte; j++)
			buffer_refs[i * PTE_ENTRIES_MAX + j] = refs[j];

		nr_pages -= nr_pages_in_pte;
	}

	ret = xen_be_map_pages(dom_id, buffer_refs, &map->buffer_map);
	if (ret)
		goto err_buffer_map;

	/* Success */
	mc_dev_devel("map done");

err_buffer_map:
	kfree(buffer_refs);
err_alloc_refs:
	if (ret < 0)
		xen_be_free_pages(&map->buffer_map);
err_buffer_pages:
	if (ret < 0) {
		kfree(map);
		atomic_dec(&g_ctx.c_vm_maps);
	}
err_map_create:
	mc_dev_devel("unmap PTEs");
	xen_be_unmap_pages(&ptes_map);
err_ptes_map:
	xen_be_free_pages(&ptes_map);
err_ptes_pages:
	mc_dev_devel("unmap PMD");
	xen_be_unmap_pages(&pmd_map);
err_pmd_map:
	xen_be_free_pages(&pmd_map);
err_pmd_pages:
	mc_dev_devel("unmap done");
	if (ret < 0)
		return ERR_PTR(ret);

	return &map->protocol_map;
}

static struct tee_mmu *xen_be_set_mmu(struct protocol_be_map *map,
				      struct mcp_buffer_map b_map)
{
	struct xen_be_map *xen_map = container_of(map,
						  struct xen_be_map,
						  protocol_map);

	return tee_mmu_wrap(&map->deleter, xen_map->buffer_map.pages,
			    xen_map->buffer_map.nr_pages, &b_map);
}

/* Dom0 call to DomU */

/* Must be called under xfe->pfe.protocol_mutex */
static void call_domu(struct protocol_fe *pfe, atomic_t call_vm_instance_no)
{
	struct tee_xfe *xfe = container_of(pfe, struct tee_xfe, pfe);

	/* Call */
	notify_remote_via_irq(xfe->irq_dom0);
	wait_for_completion(&xfe->ring_completion);
}

/* Will be called back under xfe->pfe.protocol_mutex */
static irqreturn_t xen_be_irq_handler_dom0_th(int intr, void *arg)
{
	struct tee_xfe *xfe = arg;

	if (!xfe->ring->be2fe_data.cmd) {
		mc_dev_devel("Ignore IRQ with no command (on DomU connect)");
		return IRQ_HANDLED;
	}

	WARN_ON(!xfe->pfe.protocol_busy);

	/* Response to a dom0 command, our side of ring locked by us */
	mc_dev_devel("Dom0 -> DomU response %u id %u ret %d",
		     xfe->ring->be2fe_data.cmd, xfe->ring->be2fe_data.id,
		     xfe->ring->be2fe_data.cmd_ret);
	xfe->ring->be2fe_data.cmd = TEE_BE_NONE;
	xfe->ring->be2fe_data.id = 0;
	xfe->ring->be2fe_data.cmd_ret = -EPERM;
	complete(&xfe->ring_completion);

	return IRQ_HANDLED;
}

static irqreturn_t xen_be_irq_handler_domu_th(int intr, void *arg)
{
	struct tee_xfe *xfe = arg;

	if (!xfe->ring->fe2be_data.cmd) {
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

	mc_dev_devel("DomU -> Dom0 command %u id %u",
		     xfe->ring->be2fe_data.cmd, xfe->ring->be2fe_data.id);

	xfe->ring->fe2be_data.otherend_ret = protocol_be_dispatch(&xfe->pfe);

	mc_dev_devel("DomU -> Dom0 result %u id %u ret %d",
		     xfe->ring->be2fe_data.cmd, xfe->ring->be2fe_data.id,
		     xfe->ring->fe2be_data.otherend_ret);
	notify_remote_via_irq(xfe->irq_domu);
}

/* Device */

static const struct xenbus_device_id xen_be_ids[] = {
	{ "tee_xen" },
	{ "" }
};

static inline char *vm_id(int domain_id)
{
	char dir[32];

	snprintf(dir, sizeof(dir), "/local/domain/%d", domain_id);
	return xenbus_read(XBT_NIL, dir, "name", NULL);
}

/* Called when a front-end is created */
static int xen_be_probe(struct xenbus_device *xdev,
			const struct xenbus_device_id *id)
{
	struct tee_xfe *xfe;
	char *name = NULL;
	int ret = 0;

	ret = xenbus_switch_state(xdev, XenbusStateInitWait);
	if (ret) {
		xenbus_dev_fatal(xdev, ret,
				 "failed to change state to initwait");
		return ret;
	}

	name = vm_id(xdev->otherend_id);
	if (IS_ERR(name)) {
		ret = PTR_ERR(name);
		xenbus_dev_fatal(xdev, ret, "failed to get front-end name");
		return ret;
	}

	xfe = tee_xfe_create(xdev);
	if (IS_ERR(xfe)) {
		ret = PTR_ERR(xfe);
		xenbus_dev_fatal(xdev, ret, "failed to create FE struct");
		goto err_xfe_create;
	}

	xfe->pfe.client = client_create(true, name);
	if (!xfe->pfe.client) {
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
	kfree(name);
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
					 u64 ref, void **vaddr)
{
	grant_ref_t grant_ref = (grant_ref_t)ref;

#if KERNEL_VERSION(4, 1, 0) <= LINUX_VERSION_CODE || \
		defined(MC_XENBUS_MAP_RING_VALLOC_4_1)
	return xenbus_map_ring_valloc(dev, &grant_ref, 1, vaddr);
#else
	return xenbus_map_ring_valloc(dev, grant_ref, vaddr);
#endif
}

static inline void frontend_attach(struct tee_xfe *xfe)
{
	int domu_version;
	int ret;

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

	mc_dev_devel("ring ref %llu evtchn domu=%u dom0=%u version=%u",
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

	/* Share both communication channels with parent */
	xfe->pfe.fe2be_data = &xfe->ring->fe2be_data;
	xfe->pfe.be2fe_data = &xfe->ring->be2fe_data;

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
	ret = bind_interdomain_evtchn_to_irqhandler_lateeoi(
		xfe->xdev->otherend_id, xfe->evtchn_domu,
		xen_be_irq_handler_domu_th, 0, "tee_be_domu", xfe);
#else
	ret = bind_interdomain_evtchn_to_irqhandler(
		xfe->xdev->otherend_id, xfe->evtchn_domu,
		xen_be_irq_handler_domu_th, 0, "tee_be_domu", xfe);
#endif
	if (ret < 0) {
		xenbus_dev_fatal(xfe->xdev, ret,
				 "failed to bind event channel to DomU IRQ");
		return;
	}

	xfe->irq_domu = ret;
	mc_dev_devel("bound DomU IRQ %d", xfe->irq_domu);

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
	ret = bind_interdomain_evtchn_to_irqhandler_lateeoi(
		xfe->xdev->otherend_id, xfe->evtchn_dom0,
		xen_be_irq_handler_dom0_th, 0, "tee_be_dom0", xfe);
#else
	ret = bind_interdomain_evtchn_to_irqhandler(
		xfe->xdev->otherend_id, xfe->evtchn_dom0,
		xen_be_irq_handler_dom0_th, 0, "tee_be_dom0", xfe);
#endif
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
	xenbus_switch_state(xfe->xdev, XenbusStateClosing);
	if (xfe->irq_domu >= 0)
		unbind_from_irqhandler(xfe->irq_domu, xfe);

	if (xfe->irq_dom0 >= 0)
		unbind_from_irqhandler(xfe->irq_dom0, xfe);

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

static int xen_be_early_init(int (*probe)(void), int (*start)(void))
{
	char *be_vm_id;

	/* Determine VM ID */
	be_vm_id = vm_id(0);
	if (IS_ERR(be_vm_id))
		return PTR_ERR(be_vm_id);

	strlcpy(l_ctx.vm_id, be_vm_id, sizeof(l_ctx.vm_id));
	kfree(be_vm_id);
	return 0;
};

static int xen_be_start(void)
{
	/* Initialise */
	INIT_LIST_HEAD(&l_ctx.xfes);
	mutex_init(&l_ctx.xfes_mutex);
	return xenbus_register_backend(&xen_be_driver);
}

static struct tee_protocol_be_call_ops be_call_ops = {
	.call_fe = call_domu,
	.be_map_create = xen_be_map_create,
	.be_map_delete = xen_be_map_delete,
	.be_set_mmu = xen_be_set_mmu,
};

static void xen_be_stop(void)
{
	xenbus_unregister_driver(&xen_be_driver);
}

static struct tee_protocol_ops protocol_ops = {
	.name = "XEN BE",
	.early_init = xen_be_early_init,
	.start = xen_be_start,
	.stop = xen_be_stop,
	.vm_id = l_ctx.vm_id,
	.be_call_ops = &be_call_ops,
};

struct tee_protocol_ops *xen_be_check(void)
{
	if (!xen_domain() || !xen_initial_domain())
		return NULL;

	return &protocol_ops;
}

#endif
