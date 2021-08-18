// SPDX-License-Identifier: GPL-2.0
/*
 * MTK carveout heap for partly continuous physical memory
 *
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "public/mtee_regions.h"
#include "private/tmem_error.h"

struct tmem_carveout_heap {
	struct gen_pool *pool;
	phys_addr_t heap_base;
	size_t heap_size;
};

/*
 * enum MTEE_MCHUNKS_ID {
 *
 *	MTEE_MCHUNKS_PROT = 0,
 *	MTEE_MCHUNKS_HAPP = 1,
 *	MTEE_MCHUNKS_HAPP_EXTRA = 2,
 *	MTEE_MCHUNKS_SDSP = 3,
 *	MTEE_MCHUNKS_SDSP_SHARED_VPU_TEE = 4,
 *	MTEE_MCHUNKS_SDSP_SHARED_MTEE_TEE = 5,
 *	MTEE_MCHUNKS_SDSP_SHARED_VPU_MTEE_TEE = 6,
 *	MTEE_MCHUNKS_CELLINFO = 7,
 *	MTEE_MCHUNKS_SVP = 8,
 *	MTEE_MCHUNKS_WFD = 9,
 *	MTEE_MCHUNKS_SAPU_DATA_SHM = 10,
 *	MTEE_MCHUNKS_SAPU_ENGINE_SHM = 11,
 * }
 */
static struct tmem_carveout_heap *tmem_carveout_heap[MTEE_MCHUNKS_MAX_ID];

struct tmem_block {
	struct list_head head;
	unsigned long start;
	int size;
	u32 handle;
};

static spinlock_t tmem_carveout_lock;
static LIST_HEAD(tmem_block_list);

#define ION_CARVEOUT_SHIFT	6

static unsigned long tmem_pa_to_handle(unsigned long paddr)
{
	return paddr >> ION_CARVEOUT_SHIFT;
}

static unsigned long tmem_handle_to_pa(unsigned long handle)
{
	return handle << ION_CARVEOUT_SHIFT;
}

int tmem_carveout_heap_alloc(enum MTEE_MCHUNKS_ID mchunk_id,
								unsigned long size,
								u32 *handle)
{
	unsigned long paddr;
	unsigned long lock_flags;
	struct tmem_block *entry;
	unsigned long pool_idx = mchunk_id;

	spin_lock_irqsave(&tmem_carveout_lock, lock_flags);
	paddr = gen_pool_alloc(tmem_carveout_heap[pool_idx]->pool, size);
	if (!paddr)
		goto out2;

	entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
	if (!entry)
		goto out1;

	*handle = tmem_pa_to_handle(paddr);
	entry->start = paddr;
	entry->size = size;
	list_add(&entry->head, &tmem_block_list);
	spin_unlock_irqrestore(&tmem_carveout_lock, lock_flags);

	return TMEM_OK;

out1:
	gen_pool_free(tmem_carveout_heap[pool_idx]->pool, paddr, size);
out2:
	spin_unlock_irqrestore(&tmem_carveout_lock, lock_flags);
	pr_info("%s fail: size=0x%lx, gen_pool_avail=0x%lx, pool_idx=%d\n",
			__func__, size,
			gen_pool_avail(tmem_carveout_heap[pool_idx]->pool), pool_idx);

	return -ENOMEM;
}

int tmem_carveout_heap_free(enum MTEE_MCHUNKS_ID mchunk_id, u32 handle)
{
	unsigned long lock_flags;
	int size = 0;
	struct tmem_block *tmp;
	unsigned long pool_idx = mchunk_id;

	spin_lock_irqsave(&tmem_carveout_lock, lock_flags);
	list_for_each_entry(tmp, &tmem_block_list, head) {
		if (tmp->start == tmem_handle_to_pa(handle)) {
			size = tmp->size;
			list_del(&tmp->head);
			kfree(tmp);
			gen_pool_free(tmem_carveout_heap[pool_idx]->pool,
					tmem_handle_to_pa(handle), size);
			spin_unlock_irqrestore(&tmem_carveout_lock, lock_flags);
			return TMEM_OK;
		}
	}
	spin_unlock_irqrestore(&tmem_carveout_lock, lock_flags);

	pr_info("%s fail: handle=0x%lx, idx=0x%lx\n",
			__func__, handle, pool_idx);

	return TMEM_KPOOL_FREE_CHUNK_FAILED;
}

int tmem_carveout_create(int idx, phys_addr_t heap_base, size_t heap_size)
{
	if (tmem_carveout_heap[idx] != NULL) {
		pr_info("%s:%d: tmem_carveout_heap already created\n", __func__, __LINE__);
		return TMEM_KPOOL_HEAP_ALREADY_CREATED;
	}

	tmem_carveout_heap[idx] = kmalloc(sizeof(struct tmem_carveout_heap), GFP_KERNEL);
	if (!tmem_carveout_heap[idx])
		return -ENOMEM;

	tmem_carveout_heap[idx]->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!tmem_carveout_heap[idx]->pool) {
		pr_info("%s:%d: gen_pool_create() fail\n", __func__, __LINE__);
		kfree(tmem_carveout_heap[idx]);
		return -ENOMEM;
	}

	tmem_carveout_heap[idx]->heap_base = heap_base;
	tmem_carveout_heap[idx]->heap_size = heap_size;
	gen_pool_add(tmem_carveout_heap[idx]->pool, heap_base, heap_size, -1);

	return TMEM_OK;
}

int tmem_carveout_destroy(int idx)
{
	unsigned long lock_flags;
	struct tmem_block *tmp;

	if (tmem_carveout_heap[idx] == NULL) {
		pr_info("%s:%d: tmem_carveout_heap is NULL\n", __func__, __LINE__);
		return TMEM_KPOOL_HEAP_IS_NULL;
	}

	spin_lock_irqsave(&tmem_carveout_lock, lock_flags);
	list_for_each_entry(tmp, &tmem_block_list, head) {
		gen_pool_free(tmem_carveout_heap[idx]->pool, tmp->start, tmp->size);
		list_del(&tmp->head);
		kfree(tmp);
	}

	spin_unlock_irqrestore(&tmem_carveout_lock, lock_flags);
	gen_pool_destroy(tmem_carveout_heap[idx]->pool);

	kfree(tmem_carveout_heap[idx]);
	tmem_carveout_heap[idx] = NULL;

	return TMEM_OK;
}

int tmem_carveout_init(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	spin_lock_init(&tmem_carveout_lock);

	return TMEM_OK;
}

phys_addr_t get_mem_pool_pa(u32 mchunk_id)
{
	return tmem_carveout_heap[mchunk_id]->heap_base;
}

unsigned long get_mem_pool_size(u32 mchunk_id)
{
	return gen_pool_size(tmem_carveout_heap[mchunk_id]->pool);
}
