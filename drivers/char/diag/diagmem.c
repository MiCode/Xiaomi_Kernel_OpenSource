/* Copyright (c) 2008-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/mempool.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/kmemleak.h>
#include <linux/ratelimit.h>
#include <asm/atomic.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/kmemleak.h>

#include "diagchar.h"
#include "diagmem.h"
#include "diagfwd_bridge.h"

struct diag_mempool_t diag_mempools[NUM_MEMORY_POOLS] = {
	{
		.id = POOL_TYPE_COPY,
		.name = "POOL_COPY",
		.pool = NULL,
		.itemsize = 0,
		.poolsize = 0,
		.count = 0
	},
	{
		.id = POOL_TYPE_HDLC,
		.name = "POOL_HDLC",
		.pool = NULL,
		.itemsize = 0,
		.poolsize = 0,
		.count = 0
	},
	{
		.id = POOL_TYPE_USER,
		.name = "POOL_USER",
		.pool = NULL,
		.itemsize = 0,
		.poolsize = 0,
		.count = 0
	},
	{
		.id = POOL_TYPE_USB_APPS,
		.name = "POOL_USB_APPS",
		.pool = NULL,
		.itemsize = 0,
		.poolsize = 0,
		.count = 0
	},
	{
		.id = POOL_TYPE_USB_PERIPHERALS,
		.name = "POOL_USB_PERIPHERALS",
		.pool = NULL,
		.itemsize = 0,
		.poolsize = 0,
		.count = 0
	},
	{
		.id = POOL_TYPE_DCI,
		.name = "POOL_DCI",
		.pool = NULL,
		.itemsize = 0,
		.poolsize = 0,
		.count = 0
	},
#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
	{
		.id = POOL_TYPE_MDM,
		.name = "POOL_MDM",
		.pool = NULL,
		.itemsize = 0,
		.poolsize = 0,
		.count = 0
	},
	{
		.id = POOL_TYPE_MDM2,
		.name = "POOL_MDM2",
		.pool = NULL,
		.itemsize = 0,
		.poolsize = 0,
		.count = 0
	},
	{
		.id = POOL_TYPE_MDM_DCI,
		.name = "POOL_MDM_DCI",
		.pool = NULL,
		.itemsize = 0,
		.poolsize = 0,
		.count = 0
	},
	{
		.id = POOL_TYPE_MDM2_DCI,
		.name = "POOL_MDM2_DCI",
		.pool = NULL,
		.itemsize = 0,
		.poolsize = 0,
		.count = 0
	},
	{
		.id = POOL_TYPE_MDM_USB,
		.name = "POOL_MDM_USB",
		.pool = NULL,
		.itemsize = 0,
		.poolsize = 0,
		.count = 0
	},
	{
		.id = POOL_TYPE_MDM2_USB,
		.name = "POOL_MDM2_USB",
		.pool = NULL,
		.itemsize = 0,
		.poolsize = 0,
		.count = 0
	},
	{
		.id = POOL_TYPE_MDM_DCI_WRITE,
		.name = "POOL_MDM_DCI_WRITE",
		.pool = NULL,
		.itemsize = 0,
		.poolsize = 0,
		.count = 0
	},
	{
		.id = POOL_TYPE_MDM2_DCI_WRITE,
		.name = "POOL_MDM2_DCI_WRITE",
		.pool = NULL,
		.itemsize = 0,
		.poolsize = 0,
		.count = 0
	},
	{
		.id = POOL_TYPE_QSC_USB,
		.name = "POOL_QSC_USB",
		.pool = NULL,
		.itemsize = 0,
		.poolsize = 0,
		.count = 0
	}
#endif
};

void diagmem_setsize(int pool_idx, int itemsize, int poolsize)
{
	if (pool_idx < 0 || pool_idx >= NUM_MEMORY_POOLS) {
		pr_err("diag: Invalid pool index %d in %s\n", pool_idx,
		       __func__);
		return;
	}

	diag_mempools[pool_idx].itemsize = itemsize;
	diag_mempools[pool_idx].poolsize = poolsize;
	pr_debug("diag: Mempool %s sizes: itemsize %d poolsize %d\n",
		 diag_mempools[pool_idx].name, diag_mempools[pool_idx].itemsize,
		 diag_mempools[pool_idx].poolsize);
}

void *diagmem_alloc(struct diagchar_dev *driver, int size, int pool_type)
{
	void *buf = NULL;
	int i = 0;
	unsigned long flags;
	struct diag_mempool_t *mempool = NULL;

	if (!driver)
		return NULL;

	for (i = 0; i < NUM_MEMORY_POOLS; i++) {
		mempool = &diag_mempools[i];
		if (pool_type != mempool->id)
			continue;
		if (!mempool->pool) {
			pr_err_ratelimited("diag: %s mempool is not initialized yet\n",
					   mempool->name);
			break;
		}
		if (size == 0 || size > mempool->itemsize) {
			pr_err_ratelimited("diag: cannot alloc from mempool %s, invalid size: %d\n",
					   mempool->name, size);
			break;
		}
		spin_lock_irqsave(&mempool->lock, flags);
		if (mempool->count < mempool->poolsize) {
			atomic_add(1, (atomic_t *)&mempool->count);
			buf = mempool_alloc(mempool->pool, GFP_ATOMIC);
			kmemleak_not_leak(buf);
		}
		spin_unlock_irqrestore(&mempool->lock, flags);
		if (!buf) {
			pr_debug_ratelimited("diag: Unable to allocate buffer from memory pool %s, size: %d/%d count: %d/%d\n",
					     mempool->name,
					     size, mempool->itemsize,
					     mempool->count,
					     mempool->poolsize);
		}
		break;
	}

	return buf;
}

void diagmem_free(struct diagchar_dev *driver, void *buf, int pool_type)
{
	int i = 0;
	unsigned long flags;
	struct diag_mempool_t *mempool = NULL;

	if (!driver || !buf)
		return;

	for (i = 0; i < NUM_MEMORY_POOLS; i++) {
		mempool = &diag_mempools[i];
		if (pool_type != mempool->id)
			continue;
		if (!mempool->pool) {
			pr_err_ratelimited("diag: %s mempool is not initialized yet\n",
					   mempool->name);
			break;
		}
		spin_lock_irqsave(&mempool->lock, flags);
		if (mempool->count > 0) {
			mempool_free(buf, mempool->pool);
			atomic_add(-1, (atomic_t *)&mempool->count);
		} else {
			pr_err_ratelimited("diag: Attempting to free items from %s mempool which is already empty\n",
					   mempool->name);
		}
		spin_unlock_irqrestore(&mempool->lock, flags);
		break;
	}
}

void diagmem_init(struct diagchar_dev *driver, int index)
{
	struct diag_mempool_t *mempool = NULL;
	if (!driver)
		return;

	if (index < 0 || index >= NUM_MEMORY_POOLS) {
		pr_err("diag: In %s, Invalid index %d\n", __func__, index);
		return;
	}

	mempool = &diag_mempools[index];
	if (mempool->pool) {
		pr_debug("diag: mempool %s is already initialized\n",
			 mempool->name);
		return;
	}
	if (mempool->itemsize <= 0 || mempool->poolsize <= 0) {
		pr_err("diag: Unable to initialize %s mempool, itemsize: %d poolsize: %d\n",
		       mempool->name, mempool->itemsize,
		       mempool->poolsize);
		return;
	}

	mempool->pool = mempool_create_kmalloc_pool(mempool->poolsize,
						    mempool->itemsize);
	if (!mempool->pool)
		pr_err("diag: cannot allocate %s mempool\n", mempool->name);
	spin_lock_init(&mempool->lock);
}

void diagmem_exit(struct diagchar_dev *driver, int index)
{
	unsigned long flags;
	struct diag_mempool_t *mempool = NULL;

	if (!driver)
		return;

	if (index < 0 || index >= NUM_MEMORY_POOLS) {
		pr_err("diag: In %s, Invalid index %d\n", __func__, index);
		return;
	}

	mempool = &diag_mempools[index];
	spin_lock_irqsave(&mempool->lock, flags);
	if (mempool->count == 0) {
		mempool_destroy(mempool->pool);
		mempool->pool = NULL;
	} else {
		pr_err("diag: Unable to destory %s pool, count: %d\n",
		       mempool->name, mempool->count);
	}
	spin_unlock_irqrestore(&mempool->lock, flags);
}

