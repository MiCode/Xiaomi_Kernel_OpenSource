/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#ifndef IOMMU_IOVA_DEBUG_H
#define IOMMU_IOVA_DEBUG_H

#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/iova.h>

struct iova_info {
	u32 tab_id;
	u32 dom_id;
	struct device *dev;
	struct iova_domain *iovad;
	dma_addr_t iova;
	size_t size;
	struct list_head list_node;
};

struct iova_buf_list {
	atomic_t init_flag;
	struct list_head head;
	spinlock_t lock;
};

#endif
