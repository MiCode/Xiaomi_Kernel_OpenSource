// SPDX-License-Identifier: GPL-2.0
/*
 * ION Memory Allocator system heap exporter
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (c) 2011-2021, The Linux Foundation. All rights reserved.
 *
 */

#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/ion.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/seq_file.h>
#include <soc/qcom/secure_buffer.h>
#include "ion_msm_system_heap.h"
#include "ion_msm_page_pool.h"
#include "msm_ion_priv.h"
#include "ion_system_secure_heap.h"
#include "ion_secure_util.h"

static gfp_t high_order_gfp_flags = (GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN |
				     __GFP_NORETRY) & ~__GFP_RECLAIM;
static gfp_t low_order_gfp_flags  = GFP_HIGHUSER | __GFP_ZERO;

static bool pool_auto_refill_en  __read_mostly =
IS_ENABLED(CONFIG_ION_POOL_AUTO_REFILL);

static bool valid_vmids[VMID_LAST];

int order_to_index(unsigned int order)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++)
		if (order == orders[i])
			return i;
	BUG();
	return -1;
}

static inline unsigned int order_to_size(int order)
{
	return PAGE_SIZE << order;
}

static int ion_heap_is_msm_system_heap_type(enum ion_heap_type type)
{
	return type == ((enum ion_heap_type)ION_HEAP_TYPE_MSM_SYSTEM);
}

static struct page *alloc_buffer_page(struct ion_msm_system_heap *sys_heap,
				      struct ion_buffer *buffer,
				      unsigned long order,
				      bool *from_pool)
{
	int cached = (int)ion_buffer_cached(buffer);
	struct page *page;
	struct ion_msm_page_pool *pool;
	int vmid = get_secure_vmid(buffer->flags);
	struct device *dev = sys_heap->heap.dev;
	int order_ind = order_to_index(order);
	struct task_struct *worker;

	if (vmid > 0) {
		pool = sys_heap->secure_pools[vmid][order_ind];

		/*
		 * We should skip stealing pages if (1) we're focing our
		 * allocations to come from buddy; or (2) pool refilling is
		 * disabled, in which case stealing pages could deplete the
		 * uncached pools.
		 */
		if (!(*from_pool && pool_auto_refill_en))
			goto normal_alloc;

		page = ion_msm_page_pool_alloc_pool_only(pool);
		if (!IS_ERR(page))
			return page;

		pool = sys_heap->uncached_pools[order_ind];
		page = ion_msm_page_pool_alloc_pool_only(pool);
		if (IS_ERR(page)) {
			pool = sys_heap->secure_pools[vmid][order_ind];
			goto normal_alloc;
		}

		/*
		 * Here, setting `from_pool = false` indicates that the
		 * page didn't come from the secure pool, and causes
		 * the page to be hyp-assigned.
		 */
		*from_pool = false;

		if (pool_auto_refill_en && pool->order &&
		    pool_count_below_lowmark(pool)) {
			worker = sys_heap->kworker[ION_KTHREAD_UNCACHED];
			wake_up_process(worker);
		}
		return page;
	} else if (!cached) {
		pool = sys_heap->uncached_pools[order_ind];
	} else {
		pool = sys_heap->cached_pools[order_ind];
	}

normal_alloc:
	page = ion_msm_page_pool_alloc(pool, from_pool);

	if (pool_auto_refill_en && pool->order &&
	    pool_count_below_lowmark(pool) && vmid <= 0)
		wake_up_process(sys_heap->kworker[cached]);

	if (IS_ERR(page))
		return page;

	if ((MAKE_ION_ALLOC_DMA_READY && vmid <= 0) || !(*from_pool))
		ion_pages_sync_for_device(dev, page, PAGE_SIZE << order,
					  DMA_BIDIRECTIONAL);

	return page;
}

/*
 * For secure pages that need to be freed and not added back to the pool; the
 *  hyp_unassign should be called before calling this function
 */
void free_buffer_page(struct ion_msm_system_heap *heap,
		      struct ion_buffer *buffer, struct page *page,
		      unsigned int order)
{
	bool cached = ion_buffer_cached(buffer);
	int vmid = get_secure_vmid(buffer->flags);

	if (!(buffer->flags & ION_FLAG_POOL_FORCE_ALLOC)) {
		struct ion_msm_page_pool *pool;

		if (vmid > 0 && PagePrivate(page))
			pool = heap->secure_pools[vmid][order_to_index(order)];
		else if (cached)
			pool = heap->cached_pools[order_to_index(order)];
		else
			pool = heap->uncached_pools[order_to_index(order)];

		if (buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE)
			ion_msm_page_pool_free_immediate(pool, page);
		else
			ion_msm_page_pool_free(pool, page);

#ifdef CONFIG_MM_STAT_UNRECLAIMABLE_PAGES
		mod_node_page_state(page_pgdat(page), NR_UNRECLAIMABLE_PAGES,
				    -(1 << pool->order));
#endif
	} else {
		__free_pages(page, order);

#ifdef CONFIG_MM_STAT_UNRECLAIMABLE_PAGES
		mod_node_page_state(page_pgdat(page), NR_UNRECLAIMABLE_PAGES,
				    -(1 << order));
#endif

	}
}

static struct
page_info *alloc_largest_available(struct ion_msm_system_heap *heap,
				   struct ion_buffer *buffer,
				   unsigned long size,
				   unsigned int max_order)
{
	struct page *page;
	struct page_info *info;
	int i;
	bool from_pool;

	info = kmalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < NUM_ORDERS; i++) {
		if (size < order_to_size(orders[i]))
			continue;
		if (max_order < orders[i])
			continue;
		from_pool = !(buffer->flags & ION_FLAG_POOL_FORCE_ALLOC);
		page = alloc_buffer_page(heap, buffer, orders[i], &from_pool);
		if (IS_ERR(page))
			continue;

		info->page = page;
		info->order = orders[i];
		info->from_pool = from_pool;
		INIT_LIST_HEAD(&info->list);
		return info;
	}
	kfree(info);

	return ERR_PTR(-ENOMEM);
}

static struct page_info *
alloc_from_pool_preferred(struct ion_msm_system_heap *heap,
			  struct ion_buffer *buffer,
			  unsigned long size,
			  unsigned int max_order)
{
	struct page *page;
	struct page_info *info;
	int i;

	if (buffer->flags & ION_FLAG_POOL_FORCE_ALLOC)
		goto force_alloc;

	info = kmalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < NUM_ORDERS; i++) {
		if (size < order_to_size(orders[i]))
			continue;
		if (max_order < orders[i])
			continue;

		page = alloc_from_secure_pool_order(heap, buffer, orders[i]);
		if (IS_ERR(page))
			continue;

		info->page = page;
		info->order = orders[i];
		info->from_pool = true;
		INIT_LIST_HEAD(&info->list);
		return info;
	}

	page = split_page_from_secure_pool(heap, buffer);
	if (!IS_ERR(page)) {
		info->page = page;
		info->order = 0;
		info->from_pool = true;
		INIT_LIST_HEAD(&info->list);
		return info;
	}

	kfree(info);
force_alloc:
	return alloc_largest_available(heap, buffer, size, max_order);
}

static void process_info(struct page_info *info,
			 struct scatterlist *sg,
			 struct scatterlist *sg_sync)
{
	struct page *page = info->page;

	if (sg_sync) {
		sg_set_page(sg_sync, page, (1 << info->order) * PAGE_SIZE, 0);
		sg_dma_address(sg_sync) = page_to_phys(page);
	}
	sg_set_page(sg, page, (1 << info->order) * PAGE_SIZE, 0);
	/*
	 * This is not correct - sg_dma_address needs a dma_addr_t
	 * that is valid for the the targeted device, but this works
	 * on the currently targeted hardware.
	 */
	sg_dma_address(sg) = page_to_phys(page);

	list_del(&info->list);
	kfree(info);
}

static bool check_valid_vmid(int dest_vmid, struct ion_msm_system_heap *sys_heap)
{
	phys_addr_t addr;
	struct page *page;
	int ret;
	bool from_pool = true;
	u32 source_vmid = VMID_HLOS;
	u32 dest_perms = msm_secure_get_vmid_perms(dest_vmid);
	int order_ind = order_to_index(0);

	if (valid_vmids[dest_vmid])
		return true;

	page = ion_msm_page_pool_alloc(sys_heap->uncached_pools[order_ind],
				       &from_pool);
	if (IS_ERR(page))
		return false;

	if (!from_pool)
		ion_pages_sync_for_device(sys_heap->heap.dev,
					  page, PAGE_SIZE,
					  DMA_BIDIRECTIONAL);
	addr = page_to_phys(page);
	ret = hyp_assign_phys(addr, PAGE_SIZE, &source_vmid, 1,
			      &dest_vmid, &dest_perms, 1);
	if (ret) {
		ion_msm_page_pool_free(sys_heap->uncached_pools[order_ind],
				       page);
		return false;
	}
	valid_vmids[dest_vmid] = true;
	SetPagePrivate(page);
	ion_msm_page_pool_free(sys_heap->secure_pools[dest_vmid][order_ind],
			       page);
	return true;
}

static int ion_msm_system_heap_allocate(struct ion_heap *heap,
					struct ion_buffer *buffer,
					unsigned long size,
					unsigned long flags)
{
	struct ion_msm_system_heap *sys_heap = to_msm_system_heap(heap);
	struct msm_ion_buf_lock_state *lock_state;
	struct sg_table *table;
	struct sg_table table_sync = {0};
	struct scatterlist *sg;
	struct scatterlist *sg_sync;
	int ret = -ENOMEM;
	struct list_head pages;
	struct list_head pages_from_pool;
	struct page_info *info, *tmp_info;
	int i = 0;
	unsigned int nents_sync = 0;
	unsigned long size_remaining = PAGE_ALIGN(size);
	unsigned int max_order = orders[0];
	unsigned int sz;
	int vmid = get_secure_vmid(buffer->flags);

	if (size / PAGE_SIZE > totalram_pages() / 2)
		return -ENOMEM;

	if (ion_heap_is_msm_system_heap_type(buffer->heap->type) &&
	    is_secure_allocation(buffer->flags)) {
		pr_info("%s: System heap doesn't support secure allocations\n",
			__func__);
		return -EINVAL;
	}

	/*
	 * check if vmid is valid and skip this
	 * check for trusted vm vmids (i.e; for
	 * vmids > VMID_LAST) assuming vmids for
	 * trusted vm are already validated.
	 */
	if (vmid > 0 && vmid < VMID_LAST &&
	    !check_valid_vmid(vmid, sys_heap)) {
		pr_err("%s: VMID: %d not valid\n",
		       __func__, vmid);
		return -EINVAL;
	}

	INIT_LIST_HEAD(&pages);
	INIT_LIST_HEAD(&pages_from_pool);

	while (size_remaining > 0) {
		if (is_secure_vmid_valid(vmid))
			info = alloc_from_pool_preferred(sys_heap, buffer,
							 size_remaining,
							 max_order);
		else
			info = alloc_largest_available(sys_heap, buffer,
						       size_remaining,
						       max_order);

		if (IS_ERR(info)) {
			ret = PTR_ERR(info);
			goto err;
		}

		sz = (1 << info->order) * PAGE_SIZE;

#ifdef CONFIG_MM_STAT_UNRECLAIMABLE_PAGES
		mod_node_page_state(page_pgdat(info->page),
				    NR_UNRECLAIMABLE_PAGES,
				    (1 << (info->order)));
#endif

		if (info->from_pool) {
			list_add_tail(&info->list, &pages_from_pool);
		} else {
			list_add_tail(&info->list, &pages);
			++nents_sync;
		}
		size_remaining -= sz;
		max_order = info->order;
		i++;
	}

	table = kzalloc(sizeof(*table), GFP_KERNEL);
	if (!table) {
		ret = -ENOMEM;
		goto err;
	}

	ret = sg_alloc_table(table, i, GFP_KERNEL);
	if (ret)
		goto err1;

	if (nents_sync) {
		ret = sg_alloc_table(&table_sync, nents_sync, GFP_KERNEL);
		if (ret)
			goto err_free_sg;
	}

	sg = table->sgl;
	sg_sync = table_sync.sgl;

	/*
	 * We now have two separate lists. One list contains pages from the
	 * pool and the other pages from buddy. We want to merge these
	 * together while preserving the ordering of the pages (higher order
	 * first).
	 */
	do {
		info = list_first_entry_or_null(&pages, struct page_info, list);
		tmp_info = list_first_entry_or_null(&pages_from_pool,
						    struct page_info, list);
		if (info && tmp_info) {
			if (info->order >= tmp_info->order) {
				process_info(info, sg, sg_sync);
				sg_sync = sg_next(sg_sync);
			} else {
				process_info(tmp_info, sg, NULL);
			}
		} else if (info) {
			process_info(info, sg, sg_sync);
			sg_sync = sg_next(sg_sync);
		} else if (tmp_info) {
			process_info(tmp_info, sg, NULL);
		}
		sg = sg_next(sg);

	} while (sg);

	if (nents_sync) {
		if (vmid > 0) {
			ret = ion_hyp_assign_sg(&table_sync, &vmid, 1, true);
			if (ret == -EADDRNOTAVAIL)
				goto err_free_sg2;
			else if (ret < 0)
				goto err_free;
		}
	}

	buffer->sg_table = table;
	if (nents_sync)
		sg_free_table(&table_sync);

	lock_state = kzalloc(sizeof(*lock_state), GFP_KERNEL);
	if (!lock_state) {
		ret = -ENOMEM;
		goto err_free_sg2;
	}
	buffer->priv_virt = lock_state;

	ion_prepare_sgl_for_force_dma_sync(buffer->sg_table);
	return 0;

err_free_sg2:
	if (vmid > 0)
		if (ion_hyp_unassign_sg(&table_sync, &vmid, 1, true))
			goto err_free_table_sync;
err_free:
	for_each_sg(table->sgl, sg, table->nents, i) {
		if (!PagePrivate(sg_page(sg))) {
			/* Pages from buddy are not zeroed. Bypass pool */
			buffer->private_flags |= ION_PRIV_FLAG_SHRINKER_FREE;
		} else {
			buffer->private_flags &= ~ION_PRIV_FLAG_SHRINKER_FREE;
		}
		free_buffer_page(sys_heap, buffer, sg_page(sg),
				 get_order(sg->length));
	}
err_free_table_sync:
	if (nents_sync)
		sg_free_table(&table_sync);
err_free_sg:
	sg_free_table(table);
err1:
	kfree(table);
err:
	list_for_each_entry_safe(info, tmp_info, &pages, list) {
		free_buffer_page(sys_heap, buffer, info->page, info->order);
		kfree(info);
	}
	list_for_each_entry_safe(info, tmp_info, &pages_from_pool, list) {
		free_buffer_page(sys_heap, buffer, info->page, info->order);
		kfree(info);
	}
	return ret;
}

static void ion_msm_system_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_msm_system_heap *sys_heap = to_msm_system_heap(heap);
	struct msm_ion_buf_lock_state *lock_state = buffer->priv_virt;
	struct sg_table *table = buffer->sg_table;
	struct scatterlist *sg;
	int i;
	int vmid = get_secure_vmid(buffer->flags);

	if (!(buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE) &&
	    !(buffer->flags & ION_FLAG_POOL_FORCE_ALLOC)) {
		mutex_lock(&buffer->lock);
		if (hlos_accessible_buffer(buffer))
			ion_buffer_zero(buffer);

		if (lock_state && lock_state->locked)
			pr_warn("%s: buffer is locked while being freed\n",
				__func__);
		mutex_unlock(&buffer->lock);
	} else if (vmid > 0) {
		if (ion_hyp_unassign_sg(table, &vmid, 1, true))
			return;
	}

	for_each_sg(table->sgl, sg, table->nents, i)
		free_buffer_page(sys_heap, buffer, sg_page(sg),
				 get_order(sg->length));
	sg_free_table(table);
	kfree(table);
	kfree(buffer->priv_virt);
}

static int ion_msm_system_heap_shrink(struct ion_heap *heap, gfp_t gfp_mask,
				      int nr_to_scan)
{
	struct ion_msm_system_heap *sys_heap;
	int nr_total = 0;
	int i, j, nr_freed = 0;
	int only_scan = 0;
	struct ion_msm_page_pool *pool;

	sys_heap = to_msm_system_heap(heap);

	if (!nr_to_scan)
		only_scan = 1;

	/* shrink the pools starting from lower order ones */
	for (i = NUM_ORDERS - 1; i >= 0; i--) {
		nr_freed = 0;

		for (j = 0; j < VMID_LAST; j++) {
			if (is_secure_vmid_valid(j))
				nr_freed +=
					ion_secure_page_pool_shrink(sys_heap,
								    j, i,
								    nr_to_scan);
		}

		pool = sys_heap->uncached_pools[i];
		nr_freed +=
			ion_msm_page_pool_shrink(pool, gfp_mask, nr_to_scan);

		pool = sys_heap->cached_pools[i];
		nr_freed +=
			ion_msm_page_pool_shrink(pool, gfp_mask, nr_to_scan);
		nr_total += nr_freed;

		if (!only_scan) {
			nr_to_scan -= nr_freed;
			/* shrink completed */
			if (nr_to_scan <= 0)
				break;
		}
	}

	return nr_total;
}

static long ion_msm_system_heap_get_pool_size(struct ion_heap *heap)
{
	struct ion_msm_system_heap *sys_heap;
	unsigned long total_size = 0;
	int i, j;
	struct ion_msm_page_pool *pool;

	sys_heap = to_msm_system_heap(heap);
	for (i = 0; i < NUM_ORDERS; i++) {
		pool = sys_heap->uncached_pools[i];
		total_size += (1 << pool->order) *
				pool->high_count;
		total_size += (1 << pool->order) *
				pool->low_count;
	}

	for (i = 0; i < NUM_ORDERS; i++) {
		pool = sys_heap->cached_pools[i];
		total_size += (1 << pool->order) *
				pool->high_count;
		total_size += (1 << pool->order) *
				pool->low_count;
	}

	for (i = 0; i < NUM_ORDERS; i++) {
		for (j = 0; j < VMID_LAST; j++) {
			if (!is_secure_vmid_valid(j))
				continue;
			pool = sys_heap->secure_pools[j][i];
		total_size += (1 << pool->order) *
				pool->high_count;
		total_size += (1 << pool->order) *
				pool->low_count;
		}
	}

	return total_size;
}

static struct ion_heap_ops system_heap_ops = {
	.allocate = ion_msm_system_heap_allocate,
	.free = ion_msm_system_heap_free,
	.shrink = ion_msm_system_heap_shrink,
	.get_pool_size = ion_msm_system_heap_get_pool_size,
};

static int ion_msm_system_heap_debug_show(struct ion_heap *heap,
					  struct seq_file *s, void *unused)
{
	struct ion_msm_system_heap *sys_heap;
	bool use_seq = s;
	unsigned long uncached_total = 0;
	unsigned long cached_total = 0;
	unsigned long secure_total = 0;
	struct ion_msm_page_pool *pool;
	int i, j;

	sys_heap = to_msm_system_heap(heap);
	for (i = 0; i < NUM_ORDERS; i++) {
		pool = sys_heap->uncached_pools[i];
		if (use_seq) {
			seq_printf(s,
				   "%d order %u highmem pages in uncached pool = %lu total\n",
				   pool->high_count, pool->order,
				   (1 << pool->order) * PAGE_SIZE *
					pool->high_count);
			seq_printf(s,
				   "%d order %u lowmem pages in uncached pool = %lu total\n",
				   pool->low_count, pool->order,
				   (1 << pool->order) * PAGE_SIZE *
					pool->low_count);
		}

		uncached_total += (1 << pool->order) * PAGE_SIZE *
			pool->high_count;
		uncached_total += (1 << pool->order) * PAGE_SIZE *
			pool->low_count;
	}

	for (i = 0; i < NUM_ORDERS; i++) {
		pool = sys_heap->cached_pools[i];
		if (use_seq) {
			seq_printf(s,
				   "%d order %u highmem pages in cached pool = %lu total\n",
				   pool->high_count, pool->order,
				   (1 << pool->order) * PAGE_SIZE *
					pool->high_count);
			seq_printf(s,
				   "%d order %u lowmem pages in cached pool = %lu total\n",
				   pool->low_count, pool->order,
				   (1 << pool->order) * PAGE_SIZE *
					pool->low_count);
		}

		cached_total += (1 << pool->order) * PAGE_SIZE *
			pool->high_count;
		cached_total += (1 << pool->order) * PAGE_SIZE *
			pool->low_count;
	}

	for (i = 0; i < NUM_ORDERS; i++) {
		for (j = 0; j < VMID_LAST; j++) {
			if (!is_secure_vmid_valid(j))
				continue;
			pool = sys_heap->secure_pools[j][i];

			if (use_seq) {
				seq_printf(s,
					   "VMID %d: %d order %u highmem pages in secure pool = %lu total\n",
					   j, pool->high_count, pool->order,
					   (1 << pool->order) * PAGE_SIZE *
						pool->high_count);
				seq_printf(s,
					   "VMID  %d: %d order %u lowmem pages in secure pool = %lu total\n",
					   j, pool->low_count, pool->order,
					   (1 << pool->order) * PAGE_SIZE *
						pool->low_count);
			}

			secure_total += (1 << pool->order) * PAGE_SIZE *
					 pool->high_count;
			secure_total += (1 << pool->order) * PAGE_SIZE *
					 pool->low_count;
		}
	}

	if (use_seq) {
		seq_puts(s, "--------------------------------------------\n");
		seq_printf(s, "uncached pool = %lu cached pool = %lu secure pool = %lu\n",
			   uncached_total, cached_total, secure_total);
		seq_printf(s, "pool total (uncached + cached + secure) = %lu\n",
			   uncached_total + cached_total + secure_total);
		seq_puts(s, "--------------------------------------------\n");
	} else {
		pr_info("-------------------------------------------------\n");
		pr_info("uncached pool = %lu cached pool = %lu secure pool = %lu\n",
			uncached_total, cached_total, secure_total);
		pr_info("pool total (uncached + cached + secure) = %lu\n",
			uncached_total + cached_total + secure_total);
		pr_info("-------------------------------------------------\n");
	}

	return 0;
}

static struct msm_ion_heap_ops msm_system_heap_ops = {
	.debug_show = ion_msm_system_heap_debug_show,
};

static void ion_msm_system_heap_destroy_pools(struct ion_msm_page_pool **pools)
{
	int i;

	if (!pools)
		return;

	for (i = 0; i < NUM_ORDERS; i++)
		if (pools[i]) {
			ion_msm_page_pool_destroy(pools[i]);
			pools[i] = NULL;
		}
}

/**
 * ion_msm_system_heap_create_pools - Creates pools for all orders
 *
 * If this fails you don't need to destroy any pools. It's all or
 * nothing. If it succeeds you'll eventually need to use
 * ion_msm_system_heap_destroy_pools to destroy the pools.
 */
static int
ion_msm_system_heap_create_pools(struct ion_msm_system_heap *sys_heap,
				 struct ion_msm_page_pool **pools, bool cached)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		struct ion_msm_page_pool *pool;
		gfp_t gfp_flags = low_order_gfp_flags;

		if (orders[i])
			gfp_flags = high_order_gfp_flags;
		pool = ion_msm_page_pool_create(gfp_flags, orders[i], cached);
		if (!pool)
			goto err_create_pool;
		pool->heap_dev = sys_heap->heap.dev;
		pools[i] = pool;
	}

	return 0;
err_create_pool:
	ion_msm_system_heap_destroy_pools(pools);
	return -ENOMEM;
}

static int ion_msm_sys_heap_worker(void *data)
{
	struct ion_msm_page_pool **pools = (struct ion_msm_page_pool **)data;
	int i;

	for (;;) {
		for (i = 0; i < NUM_ORDERS; i++) {
			if (pool_count_below_lowmark(pools[i]))
				ion_msm_page_pool_refill(pools[i]);
		}
		set_current_state(TASK_INTERRUPTIBLE);
		if (unlikely(kthread_should_stop())) {
			set_current_state(TASK_RUNNING);
			break;
		}
		schedule();

		set_current_state(TASK_RUNNING);
	}

	return 0;
}

static struct task_struct *ion_create_kworker(struct ion_msm_page_pool **pools,
					      bool cached)
{
	struct sched_attr attr = { 0 };
	struct task_struct *thread;
	int ret;
	char *buf;

	attr.sched_nice = ION_KTHREAD_NICE_VAL;
	buf = cached ? "cached" : "uncached";

	thread = kthread_run(ion_msm_sys_heap_worker, pools,
			     "ion-pool-%s-worker", buf);
	if (IS_ERR(thread)) {
		pr_err("%s: failed to create %s worker thread: %ld\n",
		       __func__, buf, PTR_ERR(thread));
		return thread;
	}
	ret = sched_setattr(thread, &attr);
	if (ret) {
		kthread_stop(thread);
		pr_warn("%s: failed to set task priority for %s worker thread: ret = %d\n",
			__func__, buf, ret);
		return ERR_PTR(ret);
	}

	return thread;
}

struct ion_heap *ion_msm_system_heap_create(struct ion_platform_heap *data)
{
	struct ion_msm_system_heap *heap;
	int ret = -ENOMEM;
	int i;

	heap = kzalloc(sizeof(*heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->heap.dev = data->priv;
	heap->heap.msm_heap_ops = &msm_system_heap_ops;
	heap->heap.ion_heap.ops = &system_heap_ops;
	heap->heap.ion_heap.buf_ops = msm_ion_dma_buf_ops;
	heap->heap.ion_heap.type = (enum ion_heap_type)ION_HEAP_TYPE_MSM_SYSTEM;
	heap->heap.ion_heap.flags = ION_HEAP_FLAG_DEFER_FREE;

	for (i = 0; i < VMID_LAST; i++)
		if (is_secure_vmid_valid(i) &&
		    ion_msm_system_heap_create_pools(heap,
						     heap->secure_pools[i],
						     false))
			goto destroy_secure_pools;

	if (ion_msm_system_heap_create_pools(heap, heap->uncached_pools, false))
		goto destroy_secure_pools;

	if (ion_msm_system_heap_create_pools(heap, heap->cached_pools, true))
		goto destroy_uncached_pools;

	if (pool_auto_refill_en) {
		heap->kworker[ION_KTHREAD_UNCACHED] =
				ion_create_kworker(heap->uncached_pools, false);
		if (IS_ERR(heap->kworker[ION_KTHREAD_UNCACHED])) {
			ret = PTR_ERR(heap->kworker[ION_KTHREAD_UNCACHED]);
			goto destroy_pools;
		}
		heap->kworker[ION_KTHREAD_CACHED] =
				ion_create_kworker(heap->cached_pools, true);
		if (IS_ERR(heap->kworker[ION_KTHREAD_CACHED])) {
			kthread_stop(heap->kworker[ION_KTHREAD_UNCACHED]);
			ret = PTR_ERR(heap->kworker[ION_KTHREAD_CACHED]);
			goto destroy_pools;
		}
	}

	mutex_init(&heap->split_page_mutex);

	return &heap->heap.ion_heap;
destroy_pools:
	ion_msm_system_heap_destroy_pools(heap->cached_pools);
destroy_uncached_pools:
	ion_msm_system_heap_destroy_pools(heap->uncached_pools);
destroy_secure_pools:
	for (i = 0; i < VMID_LAST; i++)
		ion_msm_system_heap_destroy_pools(heap->secure_pools[i]);
	kfree(heap);
	return ERR_PTR(ret);
}

MODULE_LICENSE("GPL v2");
