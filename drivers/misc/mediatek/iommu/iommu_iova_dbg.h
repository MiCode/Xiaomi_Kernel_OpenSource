/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt)    "[iommu_debug] " fmt

#ifndef IOMMU_IOVA_DEBUG_H
#define IOMMU_IOVA_DEBUG_H

#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/atomic.h>
#include <linux/iommu.h>
#include <dt-bindings/memory/mtk-smi-larb-port.h>

struct iova_info {
	u32 dom_id;
	struct device *dev;
	dma_addr_t iova;
	size_t size;
	struct list_head list_node;
};

struct iova_buf_list {
	atomic_t init_flag;
	struct list_head head;
	struct mutex lock;
};

static struct iova_buf_list iova_list = {.init_flag = ATOMIC_INIT(0)};

void mtk_iova_dbg_alloc(struct device *dev, dma_addr_t iova, size_t size)
{
	u32 dom_id;
	struct iova_info *iova_buf;

	if (!atomic_cmpxchg(&iova_list.init_flag, 0, 1)) {
		pr_info("iommu debug info init\n");
		mutex_init(&iova_list.lock);
		INIT_LIST_HEAD(&iova_list.head);
	}
	iova_buf = kzalloc(sizeof(*iova_buf), GFP_KERNEL);
	if (!iova_buf)
		return;

	iova_buf->dom_id = (iova >> 32);
	iova_buf->dev = dev;
	iova_buf->iova = iova;
	iova_buf->size = size;
	mutex_lock(&iova_list.lock);
	list_add(&iova_buf->list_node, &iova_list.head);
	mutex_unlock(&iova_list.lock);
}

void mtk_iova_dbg_free(dma_addr_t iova, size_t size)
{
	struct iova_info *plist;
	struct iova_info *tmp_plist;

	mutex_lock(&iova_list.lock);
	list_for_each_entry_safe(plist, tmp_plist,
				 &iova_list.head, list_node) {
		if (plist->iova == iova && plist->size == size) {
			list_del(&plist->list_node);
			kfree(plist);
			break;
		}
	}
	mutex_unlock(&iova_list.lock);
}

void mtk_iova_dbg_dump(struct device *dev)
{
	u32 id = 0;
	struct iommu_fwspec *fwspec;
	struct iova_info *plist = NULL;
	struct iova_info *n = NULL;

	mutex_lock(&iova_list.lock);

	if (!dev)
		goto dump;

	fwspec = dev_iommu_fwspec_get(dev);
	id = MTK_M4U_TO_DOM(fwspec->ids[0]);

dump:
	pr_info("%6s %18s %8s %18s\n", "dom_id", "iova", "size", "dev");
	list_for_each_entry_safe(plist, n, &iova_list.head,
				 list_node)
		if (!dev)
			pr_info("%6u %18pa %8zx %18s\n",
				plist->dom_id,
				&plist->iova,
				plist->size,
				dev_name(plist->dev));
		else if (plist->dom_id == id)
			pr_info("%6u %18pa %8zx %18s\n",
				plist->dom_id,
				&plist->iova,
				plist->size,
				dev_name(plist->dev));
	mutex_unlock(&iova_list.lock);
}
#endif
