/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <mmprofile.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include "mtk/mtk_ion.h"
#include "ion_profile.h"
#include "ion_drv_priv.h"
#include "ion_fb_heap.h"
#include "ion_priv.h"
#include "mtk/ion_drv.h"
#include <m4u.h>
#include "ion_sec_heap.h"

typedef struct {
	int eModuleID;
	unsigned int security;
	unsigned int coherent;
	void *pVA;
	unsigned int MVA;
	ion_mm_buf_debug_info_t dbg_info;
	ion_mm_sf_buf_info_t sf_buf_info;
	ion_mm_buf_destroy_callback_t *destroy_fn;
} ion_mm_buffer_info;

#define ION_FUNC_ENTER  /* MMProfileLogMetaString(MMP_ION_DEBUG, MMProfileFlagStart, __func__); */
#define ION_FUNC_LEAVE  /* MMProfileLogMetaString(MMP_ION_DEBUG, MMProfileFlagEnd, __func__); */

#define ION_PRINT_LOG_OR_SEQ(seq_file, fmt, args...) \
	do {\
		if (seq_file)\
			seq_printf(seq_file, fmt, ##args);\
		else\
			pr_err(fmt, ##args);\
	} while (0)

static unsigned int high_order_gfp_flags = (GFP_HIGHUSER | __GFP_ZERO
		| __GFP_NOWARN | __GFP_NORETRY | __GFP_NO_KSWAPD) & ~__GFP_WAIT;
static unsigned int low_order_gfp_flags = (GFP_HIGHUSER | __GFP_ZERO
		| __GFP_NOWARN);
static const unsigned int orders[] = { 1, 0 };
/* static const unsigned int orders[] = {8, 4, 0}; */
static const int num_orders = ARRAY_SIZE(orders);
static int order_to_index(unsigned int order)
{
	int i;

	for (i = 0; i < num_orders; i++)
		if (order == orders[i])
			return i;
	BUG();
	return -1;
}

static unsigned int order_to_size(int order)
{
	return PAGE_SIZE << order;
}

struct ion_system_heap {
	struct ion_heap heap;
	struct ion_page_pool **pools;
	struct ion_page_pool **cached_pools;
};

struct page_info {
	struct page *page;
	unsigned int order;
	struct list_head list;
};

static size_t mm_heap_total_memory;

static struct page *alloc_buffer_page(struct ion_system_heap *heap,
		struct ion_buffer *buffer, unsigned long order) {
	bool cached = ion_buffer_cached(buffer);
	bool split_pages = ion_buffer_fault_user_mappings(buffer);
	struct ion_page_pool *pool;
	struct page *page;

	if (!cached)
		pool = heap->pools[order_to_index(order)];
	else
		pool = heap->cached_pools[order_to_index(order)];

	page = ion_page_pool_alloc(pool);

	if (!page) {
		pr_err_ratelimited("[ion_dbg] alloc_pages order=%lu cache=%d\n", order, cached);
		return NULL;
	}

	if (split_pages)
		split_page(page, order);

	return page;
}

static void free_buffer_page(struct ion_system_heap *heap,
		struct ion_buffer *buffer, struct page *page, unsigned int order) {
	bool cached = ion_buffer_cached(buffer);

	if (!cached && !(buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE)) {
		struct ion_page_pool *pool = heap->pools[order_to_index(order)];

		ion_page_pool_free(pool, page);
	} else if (cached
			&& !(buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE)) {
		struct ion_page_pool *pool = heap->cached_pools[order_to_index(order)];

		ion_page_pool_free(pool, page);
	} else {
		__free_pages(page, order);
	}
}

static struct page_info *alloc_largest_available(struct ion_system_heap *heap,
		struct ion_buffer *buffer, unsigned long size, unsigned int max_order) {
	struct page *page;
	struct page_info *info;
	int i;

	info = kmalloc(sizeof(struct page_info), GFP_KERNEL);
	if (!info) {
		IONMSG("%s kmalloc failed info is null.\n", __func__);
		return NULL;
	}

	for (i = 0; i < num_orders; i++) {
		if (size < order_to_size(orders[i]))
			continue;
		if (max_order < orders[i])
			continue;

		page = alloc_buffer_page(heap, buffer, orders[i]);
		if (!page)
			continue;

		info->page = page;
		info->order = orders[i];
		INIT_LIST_HEAD(&info->list);
		return info;
	}
	kfree(info);

	return NULL;
}

static int ion_mm_heap_allocate(struct ion_heap *heap,
		struct ion_buffer *buffer, unsigned long size, unsigned long align,
		unsigned long flags) {
	struct ion_system_heap
	*sys_heap = container_of(heap,
			struct ion_system_heap,
			heap);
	struct sg_table *table;
	struct scatterlist *sg;
	int ret;
	struct list_head pages;
	struct page_info *info, *tmp_info;
	int i = 0;
	unsigned long size_remaining = PAGE_ALIGN(size);
	unsigned int max_order = orders[0];
	ion_mm_buffer_info *pBufferInfo = NULL;
	unsigned long long start, end;

	if (align > PAGE_SIZE) {
		IONMSG("%s align %lu is larger than PAGE_SIZE.\n", __func__, align);
		return -EINVAL;
	}

	if (size / PAGE_SIZE > totalram_pages / 2) {
		IONMSG("%s size %lu is larger than totalram_pages.\n", __func__, size);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&pages);
	start = sched_clock();
	while (size_remaining > 0) {
		info = alloc_largest_available(sys_heap, buffer, size_remaining,
				max_order);
		if (!info) {
			IONMSG("%s alloc largest available failed info is null.\n", __func__);
			goto err;
		}
		list_add_tail(&info->list, &pages);
		size_remaining -= (1 << info->order) * PAGE_SIZE;
		max_order = info->order;
		i++;
	}
	end = sched_clock();

	if (end - start > 10000000ULL)	{ /* unit is ns, 10ms */
		trace_printk("warn: ion mm heap allocate buffer size: %lu time: %lld ns --%d\n",
			     size, end - start, heap->id);
		IONMSG("warn: ion mm heap allocate buffer size: %lu time: %lld ns --%d\n", size,
		       end - start, heap->id);
	}
	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table) {
		IONMSG("%s kzalloc failed table is null.\n", __func__);
		goto err;
	}

	ret = sg_alloc_table(table, i, GFP_KERNEL);
	if (ret) {
		IONMSG("%s sg alloc table failed %d.\n", __func__, ret);
		goto err1;
	}

	sg = table->sgl;
	list_for_each_entry_safe(info, tmp_info, &pages, list) {
		struct page *page = info->page;

		sg_set_page(sg, page, (1 << info->order) * PAGE_SIZE, 0);
		sg = sg_next(sg);
		list_del(&info->list);
		kfree(info);
	}

	/* create MM buffer info for it */
	pBufferInfo = kzalloc(sizeof(ion_mm_buffer_info), GFP_KERNEL);
	if (IS_ERR_OR_NULL(pBufferInfo)) {
		IONMSG("[ion_mm_heap_allocate]: Error. Allocate ion_buffer failed.\n");
		goto err1;
	}

	buffer->sg_table = table;
	pBufferInfo->pVA = 0;
	pBufferInfo->MVA = 0;
	pBufferInfo->eModuleID = -1;
	pBufferInfo->dbg_info.value1 = 0;
	pBufferInfo->dbg_info.value2 = 0;
	pBufferInfo->dbg_info.value3 = 0;
	pBufferInfo->dbg_info.value4 = 0;
	strncpy((pBufferInfo->dbg_info.dbg_name), "nothing", ION_MM_DBG_NAME_LEN);

	buffer->priv_virt = pBufferInfo;

	mm_heap_total_memory += size;

	return 0;
err1:
	kfree(table);
	IONMSG("error: alloc for sg_table fail\n");
err:
	list_for_each_entry_safe(info, tmp_info, &pages, list) {
		free_buffer_page(sys_heap, buffer, info->page, info->order);
		kfree(info);
	}
	IONMSG("error: mm_alloc fail: size=%lu, flag=%lu.\n", size, flags);
	return -ENOMEM;
}

int ion_mm_heap_register_buf_destroy_callback(struct ion_buffer *buffer,
		ion_mm_buf_destroy_callback_t *fn) {
	ion_mm_buffer_info *pBufferInfo = (ion_mm_buffer_info *) buffer->priv_virt;

	if (pBufferInfo) {
		pBufferInfo->destroy_fn = fn;
	}
	return 0;
}

void ion_mm_heap_free_bufferInfo(struct ion_buffer *buffer)
{
	struct sg_table *table = buffer->sg_table;
	ion_mm_buffer_info *pBufferInfo = (ion_mm_buffer_info *) buffer->priv_virt;

	buffer->priv_virt = NULL;

	if (pBufferInfo) {
		if ((pBufferInfo->destroy_fn) && (pBufferInfo->MVA))
			pBufferInfo->destroy_fn(buffer, pBufferInfo->MVA);

		if ((pBufferInfo->eModuleID != -1) && (pBufferInfo->MVA))
			m4u_dealloc_mva_sg(pBufferInfo->eModuleID, table, buffer->size, pBufferInfo->MVA);

		kfree(pBufferInfo);
	}
}

void ion_mm_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_system_heap
	*sys_heap = container_of(heap, struct ion_system_heap, heap);
	struct sg_table *table = buffer->sg_table;
	struct scatterlist *sg;
	LIST_HEAD(pages);
	int i;

	mm_heap_total_memory -= buffer->size;

	/* uncached pages come from the page pools, zero them before returning
	 for security purposes (other allocations are zerod at alloc time */
	if (!(buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE))
		ion_heap_buffer_zero(buffer);

	ion_mm_heap_free_bufferInfo(buffer);

	for_each_sg(table->sgl, sg, table->nents, i)
		free_buffer_page(sys_heap, buffer, sg_page(sg), get_order(sg->length));
	sg_free_table(table);
	kfree(table);
}

struct sg_table *ion_mm_heap_map_dma(struct ion_heap *heap,
		struct ion_buffer *buffer) {
	return buffer->sg_table;
}

void ion_mm_heap_unmap_dma(struct ion_heap *heap, struct ion_buffer *buffer)
{
}

static int ion_mm_heap_shrink(struct ion_heap *heap, gfp_t gfp_mask, int nr_to_scan)
{
	struct ion_system_heap *sys_heap;
	int nr_total = 0;
	int i;

	sys_heap = container_of(heap, struct ion_system_heap, heap);

	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];

		nr_total += ion_page_pool_shrink(pool, gfp_mask, nr_to_scan);
		/* shrink cached pool */
		nr_total += ion_page_pool_shrink(sys_heap->cached_pools[i], gfp_mask, nr_to_scan);
	}

	return nr_total;
}

static int ion_mm_heap_phys(struct ion_heap *heap, struct ion_buffer *buffer,
		ion_phys_addr_t *addr, size_t *len) {
	ion_mm_buffer_info *pBufferInfo = (ion_mm_buffer_info *) buffer->priv_virt;

	if (!pBufferInfo) {
		IONMSG("[ion_mm_heap_phys]: Error. Invalid buffer.\n");
		return -EFAULT; /* Invalid buffer */
	}
	if (pBufferInfo->eModuleID == -1) {
		IONMSG("[ion_mm_heap_phys]: Error. Buffer not configured.\n");
		return -EFAULT; /* Buffer not configured. */
	}
	/* Allocate MVA */

	if (pBufferInfo->MVA == 0) {
		int ret = m4u_alloc_mva_sg(pBufferInfo->eModuleID, buffer->sg_table,
				buffer->size, pBufferInfo->security, pBufferInfo->coherent,
				&pBufferInfo->MVA);

		if (ret < 0) {
			pBufferInfo->MVA = 0;
			IONMSG("[ion_mm_heap_phys]: Error. Allocate MVA failed.\n");
			return -EFAULT;
		}
	}
	*(unsigned int *) addr = pBufferInfo->MVA; /* MVA address */
	*len = buffer->size;

	return 0;
}

int ion_mm_heap_pool_total(struct ion_heap *heap)
{
	struct ion_system_heap *sys_heap;
	int total = 0;
	int i;

	sys_heap = container_of(heap, struct ion_system_heap, heap);

	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];

		total += (pool->high_count + pool->low_count) * (1 << pool->order);
		pool = sys_heap->cached_pools[i];
		total += (pool->high_count + pool->low_count) * (1 << pool->order);
	}

	return total;
}

static struct ion_heap_ops system_heap_ops = {
		.allocate = ion_mm_heap_allocate,
		.free = ion_mm_heap_free,
		.map_dma = ion_mm_heap_map_dma,
		.unmap_dma = ion_mm_heap_unmap_dma,
		.map_kernel = ion_heap_map_kernel,
		.unmap_kernel = ion_heap_unmap_kernel,
		.map_user = ion_heap_map_user,
		.phys = ion_mm_heap_phys,
		.shrink = ion_mm_heap_shrink,
		.page_pool_total = ion_mm_heap_pool_total,
};

static int ion_mm_heap_debug_show(struct ion_heap *heap, struct seq_file *s, void *unused)
{
	struct ion_system_heap
	*sys_heap = container_of(heap, struct ion_system_heap, heap);
	struct ion_device *dev = heap->dev;
	struct rb_node *n;
	int i;

	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];

		ION_PRINT_LOG_OR_SEQ(s,
				"%d order %u highmem pages in pool = %lu total, dev, 0x%p, heap id: %d\n",
				pool->high_count, pool->order, (1 << pool->order) * PAGE_SIZE * pool->high_count,
				dev, heap->id);
		ION_PRINT_LOG_OR_SEQ(s,
				"%d order %u lowmem pages in pool = %lu total\n",
				pool->low_count, pool->order, (1 << pool->order) * PAGE_SIZE * pool->low_count);
		pool = sys_heap->cached_pools[i];
		ION_PRINT_LOG_OR_SEQ(s,
				"%d order %u highmem pages in cached_pool = %lu total\n",
				pool->high_count, pool->order, (1 << pool->order) * PAGE_SIZE * pool->high_count);
		ION_PRINT_LOG_OR_SEQ(s,
				"%d order %u lowmem pages in cached_pool = %lu total\n",
				pool->low_count, pool->order, (1 << pool->order) * PAGE_SIZE * pool->low_count);
	}
	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		ION_PRINT_LOG_OR_SEQ(s, "mm_heap_freelist total_size=0x%zu\n", ion_heap_freelist_size(heap));
	else
		ION_PRINT_LOG_OR_SEQ(s, "mm_heap defer free disabled\n");

	{
		ion_mm_buffer_info *pBufInfo;
		ion_mm_buf_debug_info_t *pDbg;

		ION_PRINT_LOG_OR_SEQ(s, "----------------------------------------------------\n");
		ION_PRINT_LOG_OR_SEQ(s,
				"%8.s %8.s %4.s %3.s %3.s %3.s %8.s %3.s %4.s %3.s %8.s %4.s %4.s %4.s %4.s %s\n",
				"buffer", "size", "kmap", "ref", "hdl", "mod", "mva", "sec",
				"flag", "pid", "comm(client)", "v1", "v2", "v3", "v4", "dbg_name");

		mutex_lock(&dev->buffer_lock);
		for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
			struct ion_buffer
			*buffer = rb_entry(n, struct ion_buffer, node);
			if (buffer->heap->type != heap->type)
				continue;
			pBufInfo = (ion_mm_buffer_info *) buffer->priv_virt;
			pDbg = &(pBufInfo->dbg_info);
			if ((heap->id == ION_HEAP_TYPE_MULTIMEDIA_FOR_CAMERA)
				&& (buffer->heap->id != ION_HEAP_TYPE_MULTIMEDIA_FOR_CAMERA))
					continue;
			ION_PRINT_LOG_OR_SEQ(s,
					"0x%p %8zu %3d %3d %3d %3d %8x %3u %3lu %3d %s 0x%x 0x%x 0x%x 0x%x %s",
					buffer, buffer->size, buffer->kmap_cnt, atomic_read(&buffer->ref.refcount),
					buffer->handle_count, pBufInfo->eModuleID, pBufInfo->MVA, pBufInfo->security,
					buffer->flags, buffer->pid, buffer->task_comm, pDbg->value1, pDbg->value2,
					pDbg->value3, pDbg->value4, pDbg->dbg_name);
			ION_PRINT_LOG_OR_SEQ(s, " sf_info(");
			for (i = 0; i < ION_MM_SF_BUF_INFO_LEN; i++)
				ION_PRINT_LOG_OR_SEQ(s, "%d ", pBufInfo->sf_buf_info.info[i]);
			ION_PRINT_LOG_OR_SEQ(s, ")\n");
		}
		mutex_unlock(&dev->buffer_lock);

		ION_PRINT_LOG_OR_SEQ(s, "----------------------------------------------------\n");

	}

	/* dump all handle's backtrace */
	down_read(&dev->lock);
	for (n = rb_first(&dev->clients); n; n = rb_next(n)) {
		struct ion_client
		*client = rb_entry(n, struct ion_client, node);

		if (client->task) {
			char task_comm[TASK_COMM_LEN];

			get_task_comm(task_comm, client->task);
			ION_PRINT_LOG_OR_SEQ(s,
					"client(0x%p) %s (%s) pid(%u) ================>\n",
					client, task_comm, client->dbg_name, client->pid);
		} else {
			ION_PRINT_LOG_OR_SEQ(s,
					"client(0x%p) %s (from_kernel) pid(%u) ================>\n",
					client, client->name, client->pid);
		}

		{
			struct rb_node *m;

			mutex_lock(&client->lock);
			for (m = rb_first(&client->handles); m; m = rb_next(m)) {
				struct ion_handle
				*handle = rb_entry(m, struct ion_handle, node);
#if ION_RUNTIME_DEBUGGER
				int i;
#endif
				if ((heap->id == ION_HEAP_TYPE_MULTIMEDIA_FOR_CAMERA)
					&& (handle->buffer->heap->id != ION_HEAP_TYPE_MULTIMEDIA_FOR_CAMERA))
					continue;

				ION_PRINT_LOG_OR_SEQ(s,
						"\thandle=0x%p, buffer=0x%p, heap=%d, backtrace is:\n",
						handle, handle->buffer, handle->buffer->heap->id);

#if ION_RUNTIME_DEBUGGER
				for (i = 0; i < handle->dbg.backtrace_num; i++)
					ION_PRINT_LOG_OR_SEQ(s, "\t\tbt: 0x%x\n", handle->dbg.backtrace[i]);
#endif

			}
			mutex_unlock(&client->lock);
		}

	}
	up_read(&dev->lock);

	return 0;
}

int ion_mm_heap_for_each_pool(int (*fn)(int high, int order, int cache,
		size_t size)) {
	struct ion_heap *heap = ion_drv_get_heap(g_ion_device, ION_HEAP_TYPE_MULTIMEDIA, 1);
	struct ion_system_heap
	*sys_heap = container_of(heap, struct ion_system_heap, heap);
	int i;

	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];

		fn(1, pool->order, 0, (1 << pool->order) * PAGE_SIZE * pool->high_count);
		fn(0, pool->order, 0, (1 << pool->order) * PAGE_SIZE * pool->low_count);

		pool = sys_heap->cached_pools[i];
		fn(1, pool->order, 1, (1 << pool->order) * PAGE_SIZE * pool->high_count);
		fn(0, pool->order, 1, (1 << pool->order) * PAGE_SIZE * pool->low_count);
	}
	return 0;
}

/*static int write_mm_page_pool(int high, int order, int cache, size_t size)
{
	if (cache)
		IONMSG("%s order_%u in cached_pool = %zu total\n", high ? "high" : "low", order, size);
	else
		IONMSG("%s order_%u in pool = %zu total\n", high ? "high" : "low", order, size);

	return 0;
}*/

static size_t ion_debug_mm_heap_total(struct ion_client *client, unsigned int id)
{
	size_t size = 0;
	struct rb_node *n;

	if (mutex_trylock(&client->lock)) {
		/* mutex_lock(&client->lock); */
		for (n = rb_first(&client->handles); n; n = rb_next(n)) {
			struct ion_handle
			*handle = rb_entry(n, struct ion_handle, node);
			if (handle->buffer->heap->id == ION_HEAP_TYPE_MULTIMEDIA)
				size += handle->buffer->size;
		}
		mutex_unlock(&client->lock);
	}
	return size;
}

void ion_mm_heap_memory_detail(void)
{
	struct ion_device *dev = g_ion_device;
	/* struct ion_heap *heap = NULL; */
	size_t total_size = 0;
	size_t total_orphaned_size = 0;
	struct rb_node *n;

	/* ion_mm_heap_for_each_pool(write_mm_page_pool); */

	ION_PRINT_LOG_OR_SEQ(NULL, "%16.s(%16.s) %16.s %16.s %s\n",
			"client", "dbg_name", "pid", "size", "address");
	ION_PRINT_LOG_OR_SEQ(NULL, "----------------------------------------------------\n");

	if (!down_read_trylock(&dev->lock))
		return;
	for (n = rb_first(&dev->clients); n; n = rb_next(n)) {
		struct ion_client
		*client = rb_entry(n, struct ion_client, node);
		size_t size = ion_debug_mm_heap_total(client, ION_HEAP_TYPE_MULTIMEDIA);

		if (!size)
			continue;
		if (client->task) {
			char task_comm[TASK_COMM_LEN];

			get_task_comm(task_comm, client->task);
			ION_PRINT_LOG_OR_SEQ(NULL, "%16.s(%16.s) %16u %16zu 0x%p\n",
					task_comm, client->dbg_name, client->pid, size, client);
		} else {
			ION_PRINT_LOG_OR_SEQ(NULL, "%16.s(%16.s) %16u %16zu 0x%p\n",
					client->name, "from_kernel", client->pid, size, client);
		}
	}
	up_read(&dev->lock);
	ION_PRINT_LOG_OR_SEQ(NULL, "----------------------------------------------------\n");
	ION_PRINT_LOG_OR_SEQ(NULL, "orphaned allocations (info is from last known client):\n");

	if (mutex_trylock(&dev->buffer_lock)) {
		for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
			struct ion_buffer
			*buffer = rb_entry(n, struct ion_buffer, node);

			if ((1 << buffer->heap->id) & ION_HEAP_MULTIMEDIA_MASK) {
				/* heap = buffer->heap; */
				total_size += buffer->size;
				if (!buffer->handle_count) {
					ION_PRINT_LOG_OR_SEQ(NULL, "%16.s %16u %16zu %d %d\n",
							buffer->task_comm, buffer->pid, buffer->size, buffer->kmap_cnt,
							atomic_read(&buffer->ref.refcount));
					total_orphaned_size += buffer->size;
				}
			}
		}
		mutex_unlock(&dev->buffer_lock);

		ION_PRINT_LOG_OR_SEQ(NULL, "----------------------------------------------------\n");
		ION_PRINT_LOG_OR_SEQ(NULL, "%16.s %16zu\n", "total orphaned", total_orphaned_size);
		ION_PRINT_LOG_OR_SEQ(NULL, "%16.s %16zu\n", "total ", total_size);
		ION_PRINT_LOG_OR_SEQ(NULL, "----------------------------------------------------\n");
	} else {
		ION_PRINT_LOG_OR_SEQ(NULL, "ion mm heap total memory: %16zu\n", mm_heap_total_memory);
	}
}

size_t ion_mm_heap_total_memory(void)
{
	return mm_heap_total_memory;
}

struct ion_heap *ion_mm_heap_create(struct ion_platform_heap *unused)
{
	struct ion_system_heap *heap;
	int i;

	heap = kzalloc(sizeof(struct ion_system_heap), GFP_KERNEL);
	if (!heap) {
		IONMSG("%s kzalloc failed heap is null.\n", __func__);
		return ERR_PTR(-ENOMEM);
	}
	heap->heap.ops = &system_heap_ops;
	heap->heap.type = ION_HEAP_TYPE_MULTIMEDIA;
	heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;
	heap->pools = kcalloc(num_orders, sizeof(struct ion_page_pool *), GFP_KERNEL);
	if (!heap->pools)
		goto err_alloc_pools;
	heap->cached_pools = kcalloc(num_orders, sizeof(struct ion_page_pool *), GFP_KERNEL);
	if (!heap->cached_pools) {
		kfree(heap->pools);
		goto err_alloc_pools;
	}

	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool;
		gfp_t gfp_flags = low_order_gfp_flags;

		if (orders[i] > 0)
			gfp_flags = high_order_gfp_flags;

		if (unused->id == ION_HEAP_TYPE_MULTIMEDIA_FOR_CAMERA)
			gfp_flags |= (__GFP_HIGHMEM | __GFP_CMA);


		pool = ion_page_pool_create(gfp_flags, orders[i]);
		if (!pool)
			goto err_create_pool;
		heap->pools[i] = pool;

		pool = ion_page_pool_create(gfp_flags, orders[i]);
		if (!pool)
			goto err_create_pool;
		heap->cached_pools[i] = pool;
	}

	heap->heap.debug_show = ion_mm_heap_debug_show;
	return &heap->heap;

err_create_pool:
	IONMSG("[ion_mm_heap]: error to create pool\n");
	for (i = 0; i < num_orders; i++) {
		if (heap->pools[i])
			ion_page_pool_destroy(heap->pools[i]);
		if (heap->cached_pools[i])
			ion_page_pool_destroy(heap->cached_pools[i]);
	}
	kfree(heap->pools);
	kfree(heap->cached_pools);

err_alloc_pools:
	IONMSG("[ion_mm_heap]: error to allocate pool\n");
	kfree(heap);
	return ERR_PTR(-ENOMEM);
}

void ion_mm_heap_destroy(struct ion_heap *heap)
{
	struct ion_system_heap
	*sys_heap = container_of(heap, struct ion_system_heap, heap);
	int i;

	for (i = 0; i < num_orders; i++)
		ion_page_pool_destroy(sys_heap->pools[i]);
	kfree(sys_heap->pools);
	kfree(sys_heap);
}

int ion_mm_copy_dbg_info(ion_mm_buf_debug_info_t *src, ion_mm_buf_debug_info_t *dst)
{
	int i;

	dst->handle = src->handle;
	for (i = 0; i < ION_MM_DBG_NAME_LEN; i++)
		dst->dbg_name[i] = src->dbg_name[i];

	dst->dbg_name[ION_MM_DBG_NAME_LEN - 1] = '\0';
	dst->value1 = src->value1;
	dst->value2 = src->value2;
	dst->value3 = src->value3;
	dst->value4 = src->value4;

	return 0;

}
int ion_mm_copy_sf_buf_info(ion_mm_sf_buf_info_t *src, ion_mm_sf_buf_info_t *dst)
{
	int i;

	dst->handle = src->handle;
	for (i = 0; i < ION_MM_SF_BUF_INFO_LEN; i++)
		dst->info[i] = src->info[i];

	return 0;
}

long ion_mm_ioctl(struct ion_client *client, unsigned int cmd, unsigned long arg, int from_kernel)
{
	ion_mm_data_t Param;
	long ret = 0;
	/* char dbgstr[256]; */
	unsigned long ret_copy;
	unsigned int  buffer_sec = 0;
	enum ion_heap_type buffer_type = 0

	ION_FUNC_ENTER;
	if (from_kernel)
		Param = *(ion_mm_data_t *) arg;
	else
		ret_copy = copy_from_user(&Param, (void __user *)arg, sizeof(ion_mm_data_t));

	switch (Param.mm_cmd) {
	case ION_MM_CONFIG_BUFFER:
		if (Param.config_buffer_param.handle) {
			struct ion_buffer *buffer;
			struct ion_handle *kernel_handle;

			kernel_handle = ion_drv_get_handle(client, Param.config_buffer_param.handle,
					Param.config_buffer_param.kernel_handle, from_kernel);
			if (IS_ERR(kernel_handle)) {
				IONMSG("ion config buffer fail! port=%d.\n", Param.config_buffer_param.eModuleID);
				ret = -EINVAL;
				break;
			}

			buffer = ion_handle_buffer(kernel_handle);
			buffer_type = buffer->heap->type;
			if ((int) buffer->heap->type == ION_HEAP_TYPE_MULTIMEDIA) {
				ion_mm_buffer_info *pBufferInfo = buffer->priv_virt;

				buffer_sec = pBufferInfo->security;
				if (pBufferInfo->MVA == 0) {
					pBufferInfo->eModuleID = Param.config_buffer_param.eModuleID;
					pBufferInfo->security = Param.config_buffer_param.security;
					pBufferInfo->coherent = Param.config_buffer_param.coherent;
				} else {
					if (pBufferInfo->security
							!= Param.config_buffer_param.security
							|| pBufferInfo->coherent
									!= Param.config_buffer_param.coherent) {
						IONMSG("[ion_heap]: Warning. config buffer param error from %c heap\n",
								buffer->heap->type);
						IONMSG(" sec:%d(%d), coherent: %d(%d)\n",
								pBufferInfo->security,
								Param.config_buffer_param.security,
								pBufferInfo->coherent,
								Param.config_buffer_param.coherent);
						ret = -ION_ERROR_CONFIG_LOCKED;
					}
				}
			} else if ((int) buffer->heap->type == ION_HEAP_TYPE_FB) {
				ion_fb_buffer_info *pBufferInfo = buffer->priv_virt;

				buffer_sec = pBufferInfo->security;
				if (pBufferInfo->MVA == 0) {
					pBufferInfo->eModuleID = Param.config_buffer_param.eModuleID;
					pBufferInfo->security = Param.config_buffer_param.security;
					pBufferInfo->coherent = Param.config_buffer_param.coherent;
				} else {
					if (pBufferInfo->security
							!= Param.config_buffer_param.security
							|| pBufferInfo->coherent
									!= Param.config_buffer_param.coherent) {
						IONMSG("[ion_heap]: Warning. config buffer param error from %c heap\n",
								buffer->heap->type);
						IONMSG(" sec:%d(%d), coherent: %d(%d)\n",
								pBufferInfo->security,
								Param.config_buffer_param.security,
								pBufferInfo->coherent,
								Param.config_buffer_param.coherent);
						ret = -ION_ERROR_CONFIG_LOCKED;
					}
				}

			} else if ((int) buffer->heap->type == ION_HEAP_TYPE_MULTIMEDIA_SEC) {
				ion_sec_buffer_info *pBufferInfo = buffer->priv_virt;

				buffer_sec = pBufferInfo->security;
				if (pBufferInfo->MVA == 0) {
					pBufferInfo->eModuleID = Param.config_buffer_param.eModuleID;
					pBufferInfo->security = Param.config_buffer_param.security;
					pBufferInfo->coherent = Param.config_buffer_param.coherent;
				} else {
					if (pBufferInfo->security
							!= Param.config_buffer_param.security
							|| pBufferInfo->coherent
									!= Param.config_buffer_param.coherent) {
						IONMSG("[ion_heap]: Warning. config buffer param error from %c heap\n",
								buffer->heap->type);
						IONMSG(" sec:%d(%d), coherent: %d(%d)\n",
								pBufferInfo->security,
								Param.config_buffer_param.security,
								pBufferInfo->coherent,
								Param.config_buffer_param.coherent);
						ret = -ION_ERROR_CONFIG_LOCKED;
					}
				}
			} else {
				IONMSG("[ion_heap]: Error. Cannot configure buffer that is not from %c heap.\n",
						buffer->heap->type);
				ret = 0;
			}
			ion_drv_put_kernel_handle(kernel_handle);
		} else {
			IONMSG("[ion_heap]: Error config buf with invalid handle.\n");
			ret = -EFAULT;
		}
		break;
	case ION_MM_SET_DEBUG_INFO:
	{
		struct ion_buffer *buffer;

		if (Param.buf_debug_info_param.handle) {
			struct ion_handle *kernel_handle;

			kernel_handle = ion_drv_get_handle(client, Param.buf_debug_info_param.handle,
					Param.buf_debug_info_param.kernel_handle, from_kernel);
			if (IS_ERR(kernel_handle)) {
				IONMSG("ion set debug info fail! kernel_handle=0x%p.\n", kernel_handle);
				ret = -EINVAL;
				break;
			}

			buffer = ion_handle_buffer(kernel_handle);
			buffer_type = buffer->heap->type;
			if ((int) buffer->heap->type == ION_HEAP_TYPE_MULTIMEDIA) {
				ion_mm_buffer_info *pBufferInfo = buffer->priv_virt;

				buffer_sec = pBufferInfo->security;
				ion_mm_copy_dbg_info(&(Param.buf_debug_info_param), &(pBufferInfo->dbg_info));
			} else if ((int) buffer->heap->type == ION_HEAP_TYPE_FB) {
				ion_fb_buffer_info *pBufferInfo = buffer->priv_virt;

				buffer_sec = pBufferInfo->security;
				ion_mm_copy_dbg_info(&(Param.buf_debug_info_param), &(pBufferInfo->dbg_info));
			} else if ((int) buffer->heap->type == ION_HEAP_TYPE_MULTIMEDIA_SEC) {
				ion_sec_buffer_info *pBufferInfo = buffer->priv_virt;

				buffer_sec = pBufferInfo->security;
				ion_mm_copy_dbg_info(&(Param.buf_debug_info_param), &(pBufferInfo->dbg_info));
			} else {
				IONMSG("[ion_heap]: Error. Cannot set dbg buffer that is not from %c heap.\n",
						buffer->heap->type);
				ret = -EFAULT;
			}
			ion_drv_put_kernel_handle(kernel_handle);
		} else {
			IONMSG("[ion_heap]: Error. set dbg buffer with invalid handle.\n");
			ret = -EFAULT;
		}
	}
	break;
	case ION_MM_GET_DEBUG_INFO:
	{
		struct ion_buffer *buffer;

		if (Param.buf_debug_info_param.handle) {
			struct ion_handle *kernel_handle;

			kernel_handle = ion_drv_get_handle(client, Param.buf_debug_info_param.handle,
					Param.buf_debug_info_param.kernel_handle, from_kernel);
			if (IS_ERR(kernel_handle)) {
				IONMSG("ion get debug info fail! kernel_handle=0x%p.\n", kernel_handle);
				ret = -EINVAL;
				break;
			}
			buffer = ion_handle_buffer(kernel_handle);
			buffer_type = buffer->heap->type;
			if ((int) buffer->heap->type == ION_HEAP_TYPE_MULTIMEDIA) {
				ion_mm_buffer_info *pBufferInfo = buffer->priv_virt;

				buffer_sec = pBufferInfo->security;
				ion_mm_copy_dbg_info(&(pBufferInfo->dbg_info), &(Param.buf_debug_info_param));
			} else if ((int) buffer->heap->type == ION_HEAP_TYPE_FB) {
				ion_fb_buffer_info *pBufferInfo = buffer->priv_virt;

				buffer_sec = pBufferInfo->security;
				ion_mm_copy_dbg_info(&(pBufferInfo->dbg_info), &(Param.buf_debug_info_param));
			} else if ((int) buffer->heap->type == ION_HEAP_TYPE_MULTIMEDIA_SEC) {
				ion_sec_buffer_info *pBufferInfo = buffer->priv_virt;

				buffer_sec = pBufferInfo->security;
				ion_mm_copy_dbg_info(&(pBufferInfo->dbg_info), &(Param.buf_debug_info_param));
			} else {
				IONMSG("[ion_heap]: Error. Cannot get dbg buffer that is not from %c heap.\n",
						buffer->heap->type);
				ret = -EFAULT;
			}
			ion_drv_put_kernel_handle(kernel_handle);
		} else {
			IONMSG("[ion_heap]: Error. get dbg buffer with invalid handle.\n");
			ret = -EFAULT;
		}
	}
	break;
	case ION_MM_SET_SF_BUF_INFO:
	{
		struct ion_buffer *buffer;

		if (Param.sf_buf_info_param.handle) {
			struct ion_handle *kernel_handle;

			kernel_handle = ion_drv_get_handle(client, Param.sf_buf_info_param.handle,
					Param.sf_buf_info_param.kernel_handle, from_kernel);
			if (IS_ERR(kernel_handle)) {
				IONMSG("ion set sf buf info fail! kernel_handle=0x%p.\n", kernel_handle);
				ret = -EINVAL;
				break;
			}

			buffer = ion_handle_buffer(kernel_handle);
			buffer_type = buffer->heap->type;
			if ((int) buffer->heap->type == ION_HEAP_TYPE_MULTIMEDIA) {
				ion_mm_buffer_info *pBufferInfo = buffer->priv_virt;

				buffer_sec = pBufferInfo->security;
				ion_mm_copy_sf_buf_info(&(Param.sf_buf_info_param), &(pBufferInfo->sf_buf_info));
			} else if ((int) buffer->heap->type == ION_HEAP_TYPE_FB) {
				ion_fb_buffer_info *pBufferInfo = buffer->priv_virt;

				buffer_sec = pBufferInfo->security;
				ion_mm_copy_sf_buf_info(&(Param.sf_buf_info_param), &(pBufferInfo->sf_buf_info));
			} else if ((int) buffer->heap->type == ION_HEAP_TYPE_MULTIMEDIA_SEC) {
				ion_sec_buffer_info *pBufferInfo = buffer->priv_virt;

				buffer_sec = pBufferInfo->security;
				ion_mm_copy_sf_buf_info(&(Param.sf_buf_info_param), &(pBufferInfo->sf_buf_info));
			} else {
				IONMSG("[ion_heap]: Error. Cannot set sf_buf_info buffer that is not from %c heap.\n",
						buffer->heap->type);
				ret = -EFAULT;
			}
			ion_drv_put_kernel_handle(kernel_handle);
		} else {
			IONMSG("[ion_heap]: Error. set sf_buf_info buffer with invalid handle.\n");
			ret = -EFAULT;
		}
	}
	break;
	case ION_MM_GET_SF_BUF_INFO:
	{
		struct ion_buffer *buffer;

		if (Param.sf_buf_info_param.handle) {
			struct ion_handle *kernel_handle;

			kernel_handle = ion_drv_get_handle(client, Param.sf_buf_info_param.handle,
					Param.sf_buf_info_param.kernel_handle, from_kernel);
			if (IS_ERR(kernel_handle)) {
				IONMSG("ion get sf buf info fail! kernel_handle=0x%p.\n", kernel_handle);
				ret = -EINVAL;
				break;
			}
			buffer = ion_handle_buffer(kernel_handle);
			buffer_type = buffer->heap->type;
			if ((int) buffer->heap->type == ION_HEAP_TYPE_MULTIMEDIA) {
				ion_mm_buffer_info *pBufferInfo = buffer->priv_virt;

				buffer_sec = pBufferInfo->security;
				ion_mm_copy_sf_buf_info(&(pBufferInfo->sf_buf_info), &(Param.sf_buf_info_param));
			} else if ((int) buffer->heap->type == ION_HEAP_TYPE_FB) {
				ion_fb_buffer_info *pBufferInfo = buffer->priv_virt;

				buffer_sec = pBufferInfo->security;
				ion_mm_copy_sf_buf_info(&(pBufferInfo->sf_buf_info), &(Param.sf_buf_info_param));
			} else if ((int) buffer->heap->type == ION_HEAP_TYPE_MULTIMEDIA_SEC) {
				ion_sec_buffer_info *pBufferInfo = buffer->priv_virt;

				buffer_sec = pBufferInfo->security;
				ion_mm_copy_sf_buf_info(&(pBufferInfo->sf_buf_info), &(Param.sf_buf_info_param));
			} else {
				IONMSG("[ion_heap]: Error. Cannot get sf_buf_info buffer that is not from %c heap.\n",
						buffer->heap->type);
				ret = -EFAULT;
			}
			ion_drv_put_kernel_handle(kernel_handle);
		} else {
			IONMSG("[ion_heap]: Error. get sf_buf_info buffer with invalid handle.\n");
			ret = -EFAULT;
		}
	}
	break;
	default:
		IONMSG("[ion_heap]: Error. Invalid command.\n");
		ret = -EFAULT;
	}

#if 0
	if (((buffer_sec != 0) && ((int)buffer_type != ION_HEAP_TYPE_MULTIMEDIA_SEC)) ||
	    ((buffer_sec == 0) && ((int)buffer_type == ION_HEAP_TYPE_MULTIMEDIA_SEC)))
		IONMSG("[ion_heap]: Warning. CMD(%d) buffer para error from %d heap sec:%d(%d).!!!!!!\n",
								Param.mm_cmd,
								buffer_type,
								buffer_sec,
								Param.config_buffer_param.security);
#endif
	if (from_kernel)
		*(ion_mm_data_t *)arg = Param;
	else
		ret_copy = copy_to_user((void __user *)arg, &Param, sizeof(ion_mm_data_t));
	ION_FUNC_LEAVE;
	return ret;
}

#ifdef CONFIG_PM
void shrink_ion_by_scenario(void)
{
	int nr_to_reclaim, nr_reclaimed;
	int nr_to_try = 3;

	struct ion_heap *movable_ion_heap = ion_drv_get_heap(g_ion_device, ION_HEAP_TYPE_MULTIMEDIA_FOR_CAMERA, 1);

	do {
		nr_to_reclaim = ion_mm_heap_shrink(movable_ion_heap, __GFP_HIGHMEM | __GFP_CMA, 0);
		nr_reclaimed = ion_mm_heap_shrink(movable_ion_heap,
				 __GFP_HIGHMEM | __GFP_CMA, nr_to_reclaim);

		if (nr_to_reclaim == nr_reclaimed)
			break;
	} while (--nr_to_try != 0);

	if (nr_to_reclaim != nr_reclaimed)
		IONMSG("%s: remaining (%d)\n", __func__, nr_to_reclaim - nr_reclaimed);
}
#endif
