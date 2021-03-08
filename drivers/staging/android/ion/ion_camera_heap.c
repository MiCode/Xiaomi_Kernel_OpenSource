/*
 * Copyright (c) 2019. Xiaomi Technology Co.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * All Rights Reserved.
 *
 * Confidential and Proprietary - Xiaomi Technology Co.
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
#include <linux/sched/types.h>
#include <linux/sched.h>
#include <soc/qcom/secure_buffer.h>
#include "ion_camera_heap.h"
#include "ion.h"
#include "ion_secure_util.h"

static gfp_t high_order_gfp_flags = (GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN |
				     __GFP_NORETRY) & ~__GFP_RECLAIM;
static gfp_t low_order_gfp_flags  = GFP_HIGHUSER | __GFP_ZERO;

static bool pool_auto_refill_en  __read_mostly =
		IS_ENABLED(CONFIG_ION_POOL_AUTO_REFILL);

int camera_order_to_index(unsigned int order)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++)
		if (order == orders[i])
			return i;
	BUG();
	return -1;
}

int reserved_pool_page_count(struct ion_camera_heap *heap, unsigned int order)
{
	int index = camera_order_to_index(order);
	struct ion_page_pool *reserved_pool = heap->reserved_pools[index];
	return reserved_pool->high_count + reserved_pool->low_count;
}

bool reserved_pools_full(struct ion_camera_heap *heap,unsigned int order) {
	int index = camera_order_to_index(order);
	struct ion_page_pool *reserved_pool = heap->reserved_pools[index];
	unsigned long reserved_pool_count = reserved_pool->high_count +
		reserved_pool->low_count;
	if(reserved_pool_count > 0 )
		return false;
	return true;
}

static inline unsigned int order_to_size(int order)
{
	return PAGE_SIZE << order;
}

struct pages_mem {
	struct page **pages;
	u32 size;
};

int ion_heap_is_camera_heap_type(enum ion_heap_type type)
{
	return type == ((enum ion_heap_type)ION_HEAP_TYPE_CAMERA);
}

static struct page *alloc_buffer_page(struct ion_camera_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long order,
				      bool *from_pool)
{
	int cached = (int)ion_buffer_cached(buffer);
	struct page *page;
	struct ion_page_pool *pool;
	int vmid = get_secure_vmid(buffer->flags);
	struct device *dev = heap->heap.priv;

	if (vmid > 0)
		pool = heap->secure_pools[vmid][camera_order_to_index(order)];
	else if (reserved_pool_page_count(heap, order) > 0)
		pool = heap->reserved_pools[camera_order_to_index(order)];
	else if (!cached)
		pool = heap->uncached_pools[camera_order_to_index(order)];
	else
		pool = heap->cached_pools[camera_order_to_index(order)];

	page = ion_page_pool_alloc(pool, from_pool);

	if (pool_auto_refill_en && pool->order &&
	    pool_count_below_lowmark(pool) && vmid <= 0) {
		wake_up_process(heap->kworker[cached]);
	}

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
void free_camera_buffer_page(struct ion_camera_heap *heap,
		      struct ion_buffer *buffer, struct page *page,
		      unsigned int order)
{
	bool cached = ion_buffer_cached(buffer);
	int vmid = get_secure_vmid(buffer->flags);

	if (!(buffer->flags & ION_FLAG_POOL_FORCE_ALLOC)) {
		struct ion_page_pool *pool;
		if (vmid > 0)
			pool = heap->secure_pools[vmid][camera_order_to_index(order)];
		else if (reserved_pool_page_count(heap, order) < cam_reserved_counts[camera_order_to_index(order)])
			pool = heap->reserved_pools[camera_order_to_index(order)];
		else if (cached)
			pool = heap->cached_pools[camera_order_to_index(order)];
		else
			pool = heap->uncached_pools[camera_order_to_index(order)];

		if (buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE)
			ion_page_pool_free_immediate(pool, page);
		else
			ion_page_pool_free(pool, page);

		mod_node_page_state(page_pgdat(page), NR_UNRECLAIMABLE_PAGES,
				    -(1 << pool->order));
	} else {
		__free_pages(page, order);
		mod_node_page_state(page_pgdat(page), NR_UNRECLAIMABLE_PAGES,
				    -(1 << order));
	}
}

static struct page_info *alloc_largest_available(struct ion_camera_heap *heap,
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

static unsigned int process_info(struct page_info *info,
				 struct scatterlist *sg,
				 struct scatterlist *sg_sync,
				 struct pages_mem *data, unsigned int i)
{
	struct page *page = info->page;
	unsigned int j;

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
	if (data) {
		for (j = 0; j < (1 << info->order); ++j)
			data->pages[i++] = nth_page(page, j);
	}
	list_del(&info->list);
	kfree(info);
	return i;
}

static int ion_heap_alloc_pages_mem(struct pages_mem *pages_mem)
{
	struct page **pages;
	unsigned int page_tbl_size;

	page_tbl_size = sizeof(struct page *) * (pages_mem->size >> PAGE_SHIFT);
	if (page_tbl_size > SZ_8K) {
		/*
		 * Do fallback to ensure we have a balance between
		 * performance and availability.
		 */
		pages = kmalloc(page_tbl_size,
				__GFP_COMP | __GFP_NORETRY |
				__GFP_NOWARN);
		if (!pages)
			pages = vmalloc(page_tbl_size);
	} else {
		pages = kmalloc(page_tbl_size, GFP_KERNEL);
	}

	if (!pages)
		return -ENOMEM;

	pages_mem->pages = pages;
	return 0;
}

static void ion_heap_free_pages_mem(struct pages_mem *pages_mem)
{
	kvfree(pages_mem->pages);
}

static int ion_camera_heap_allocate(struct ion_heap *heap,
				    struct ion_buffer *buffer,
				    unsigned long size,
				    unsigned long flags)
{
	struct ion_camera_heap *sys_heap = container_of(heap,
							struct ion_camera_heap,
							heap);
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
	struct pages_mem data;
	unsigned int sz;
	int vmid = get_secure_vmid(buffer->flags);

	if (size / PAGE_SIZE > totalram_pages / 2)
		return -ENOMEM;

	if (ion_heap_is_camera_heap_type(buffer->heap->type) &&
	    is_secure_vmid_valid(vmid)) {
		pr_info("%s: camera heap doesn't support secure allocations\n",
			__func__);
		return -EINVAL;
	}

	data.size = 0;
	INIT_LIST_HEAD(&pages);
	INIT_LIST_HEAD(&pages_from_pool);

	while (size_remaining > 0) {

			info = alloc_largest_available(
					sys_heap, buffer, size_remaining,
					max_order);

		if (IS_ERR(info)) {
			ret = PTR_ERR(info);
			goto err;
		}

		sz = (1 << info->order) * PAGE_SIZE;

		mod_node_page_state(
				page_pgdat(info->page), NR_UNRECLAIMABLE_PAGES,
				(1 << (info->order)));

		if (info->from_pool) {
			list_add_tail(&info->list, &pages_from_pool);
		} else {
			list_add_tail(&info->list, &pages);
			data.size += sz;
			++nents_sync;
		}
		size_remaining -= sz;
		max_order = info->order;
		i++;
	}

	ret = ion_heap_alloc_pages_mem(&data);

	if (ret)
		goto err;

	table = kzalloc(sizeof(*table), GFP_KERNEL);
	if (!table) {
		ret = -ENOMEM;
		goto err_free_data_pages;
	}

	ret = sg_alloc_table(table, i, GFP_KERNEL);
	if (ret)
		goto err1;

	if (nents_sync) {
		ret = sg_alloc_table(&table_sync, nents_sync, GFP_KERNEL);
		if (ret)
			goto err_free_sg;
	}

	i = 0;
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
				i = process_info(info, sg, sg_sync, &data, i);
				sg_sync = sg_next(sg_sync);
			} else {
				i = process_info(tmp_info, sg, 0, 0, i);
			}
		} else if (info) {
			i = process_info(info, sg, sg_sync, &data, i);
			sg_sync = sg_next(sg_sync);
		} else if (tmp_info) {
			i = process_info(tmp_info, sg, 0, 0, i);
		}
		sg = sg_next(sg);

	} while (sg);

	if (nents_sync) {
		if (vmid > 0) {
			ret = ion_hyp_assign_sg(&table_sync, &vmid, 1, true);
			if (ret)
				goto err_free_sg2;
		}
	}

	buffer->sg_table = table;
	if (nents_sync)
		sg_free_table(&table_sync);
	ion_heap_free_pages_mem(&data);
	return 0;

err_free_sg2:
	/* We failed to zero buffers. Bypass pool */
	buffer->private_flags |= ION_PRIV_FLAG_SHRINKER_FREE;

	if (vmid > 0)
		if (ion_hyp_unassign_sg(table, &vmid, 1, true, false))
			goto err_free_table_sync;

	for_each_sg(table->sgl, sg, table->nents, i)
		free_camera_buffer_page(sys_heap, buffer, sg_page(sg),
				 get_order(sg->length));
err_free_table_sync:
	if (nents_sync)
		sg_free_table(&table_sync);
err_free_sg:
	sg_free_table(table);
err1:
	kfree(table);
err_free_data_pages:
	ion_heap_free_pages_mem(&data);
err:
	list_for_each_entry_safe(info, tmp_info, &pages, list) {
		free_camera_buffer_page(sys_heap, buffer, info->page, info->order);
		kfree(info);
	}
	list_for_each_entry_safe(info, tmp_info, &pages_from_pool, list) {
		free_camera_buffer_page(sys_heap, buffer, info->page, info->order);
		kfree(info);
	}
	return ret;
}

void ion_camera_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_camera_heap *sys_heap = container_of(heap,
							struct ion_camera_heap,
							heap);
	struct sg_table *table = buffer->sg_table;
	struct scatterlist *sg;
	int i;
	int vmid = get_secure_vmid(buffer->flags);

	if (!(buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE) &&
	    !(buffer->flags & ION_FLAG_POOL_FORCE_ALLOC)) {
		if (vmid < 0)
			ion_heap_buffer_zero(buffer);
	} else if (vmid > 0) {
		if (ion_hyp_unassign_sg(table, &vmid, 1, true, false))
			return;
	}

	for_each_sg(table->sgl, sg, table->nents, i)
		free_camera_buffer_page(sys_heap, buffer, sg_page(sg),
				 get_order(sg->length));
	sg_free_table(table);
	kfree(table);
}

static int ion_camera_heap_shrink(struct ion_heap *heap, gfp_t gfp_mask,
				 int nr_to_scan)
{
	struct ion_camera_heap *sys_heap;
	int nr_total = 0;
	int i, nr_freed = 0;
	int only_scan = 0;
	struct ion_page_pool *pool;

	sys_heap = container_of(heap, struct ion_camera_heap, heap);

	if (!nr_to_scan)
		only_scan = 1;

	/* shrink the pools starting from lower order ones */
	for (i = NUM_ORDERS - 1; i >= 0; i--) {
		nr_freed = 0;

		pool = sys_heap->uncached_pools[i];
		nr_freed += ion_page_pool_shrink(pool, gfp_mask, nr_to_scan);

		pool = sys_heap->cached_pools[i];
		nr_freed += ion_page_pool_shrink(pool, gfp_mask, nr_to_scan);
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

static struct ion_heap_ops camera_heap_ops = {
	.allocate = ion_camera_heap_allocate,
	.free = ion_camera_heap_free,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_heap_map_user,
	.shrink = ion_camera_heap_shrink,
};

static int ion_camera_heap_debug_show(struct ion_heap *heap, struct seq_file *s,
				      void *unused)
{

	struct ion_camera_heap *sys_heap = container_of(
					heap, struct ion_camera_heap, heap);
	bool use_seq = s;
	unsigned long reserved_total = 0;
	unsigned long uncached_total = 0;
	unsigned long cached_total = 0;
	unsigned long secure_total = 0;
	struct ion_page_pool *pool;
	int i, j;

	for (i = 0; i < NUM_ORDERS; i++) {
		pool = sys_heap->reserved_pools[i];
		if (use_seq) {
			seq_printf(s,
				   "%d order %u highmem pages in reserved pool = %lu total\n",
				   pool->high_count, pool->order,
				   (1 << pool->order) * PAGE_SIZE *
					pool->high_count);
			seq_printf(s,
				   "%d order %u lowmem pages in reserved pool = %lu total\n",
				   pool->low_count, pool->order,
				   (1 << pool->order) * PAGE_SIZE *
					pool->low_count);
		}

		reserved_total += (1 << pool->order) * PAGE_SIZE *
			pool->high_count;
		reserved_total += (1 << pool->order) * PAGE_SIZE *
			pool->low_count;
	}

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
		seq_printf(s, "reserved pool = %lu\n uncached pool = %lu cached pool = %lu secure pool = %lu\n",
			   reserved_total, uncached_total, cached_total, secure_total);
		seq_printf(s, "pool total (reserved + uncached + cached + secure) = %lu\n",
			   reserved_total + uncached_total + cached_total + secure_total);
		seq_puts(s, "--------------------------------------------\n");
	} else {
		pr_info("-------------------------------------------------\n");
		pr_info("reserved pool = %lu\n uncached pool = %lu cached pool = %lu secure pool = %lu\n",
			   reserved_total, uncached_total, cached_total, secure_total);
		pr_info("pool total (reserved + uncached + cached + secure) = %lu\n",
			   reserved_total + uncached_total + cached_total + secure_total);
		pr_info("-------------------------------------------------\n");
	}

	return 0;
}

static void ion_camera_heap_destroy_pools(struct ion_page_pool **pools)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++)
		if (pools[i]) {
			ion_page_pool_destroy(pools[i]);
			pools[i] = NULL;
		}
}

/**
 * ion_camera_heap_create_pools - Creates pools for all orders
 *
 * If this fails you don't need to destroy any pools. It's all or
 * nothing. If it succeeds you'll eventually need to use
 * ion_camera_heap_destroy_pools to destroy the pools.
 */
static int ion_camera_heap_create_pools(struct ion_camera_heap *sys_heap, struct ion_page_pool **pools,
					bool cached)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		struct ion_page_pool *pool;
		gfp_t gfp_flags = low_order_gfp_flags;

		if (orders[i])
			gfp_flags = high_order_gfp_flags;
		pool = ion_page_pool_create(gfp_flags, orders[i], cached);
		if (!pool)
			goto err_create_pool;
		pool->dev = sys_heap->heap.priv;
		pools[i] = pool;
	}
	return 0;
err_create_pool:
	ion_camera_heap_destroy_pools(pools);
	return -ENOMEM;
}

static int ion_sys_heap_worker(void *data)
{
	struct ion_page_pool **pools = (struct ion_page_pool **)data;
	int i;

	for (;;) {
		for (i = 0; i < NUM_ORDERS; i++) {
			if (pool_count_below_lowmark(pools[i]))
				ion_page_pool_refill(pools[i]);
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

static struct task_struct *ion_create_kworker(struct ion_page_pool **pools,
					      bool cached)
{
	struct sched_attr attr = { 0 };
	struct task_struct *thread;
	int ret;
	char *buf;

	attr.sched_nice = ION_KTHREAD_NICE_VAL;
	buf = cached ? "cached" : "uncached";

	thread = kthread_run(ion_sys_heap_worker, pools,
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

struct ion_heap *ion_camera_heap_create(struct ion_platform_heap *data)
{
	struct ion_camera_heap *heap;
	int ret = -ENOMEM;
	int i;

	heap = kzalloc(sizeof(*heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->heap.ops = &camera_heap_ops;
	heap->heap.type = (enum ion_heap_type)ION_HEAP_TYPE_CAMERA;
	heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;
	heap->heap.priv = data->priv;

	for (i = 0; i < VMID_LAST; i++)
		if (is_secure_vmid_valid(i))
			if (ion_camera_heap_create_pools(heap,
					heap->secure_pools[i], false))
				goto destroy_secure_pools;

	if (ion_camera_heap_create_pools(heap, heap->uncached_pools, false))
		goto destroy_secure_pools;

	if (ion_camera_heap_create_pools(heap, heap->cached_pools, true))
		goto destroy_uncached_pools;

	if (ion_camera_heap_create_pools(heap, heap->reserved_pools, false))
		goto err_create_reserved_pools;

	for (i = 0; i < NUM_ORDERS; i++)
		ion_page_pool_prealloc(heap->reserved_pools[i], cam_reserved_counts[i]);

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

	heap->heap.debug_show = ion_camera_heap_debug_show;
	return &heap->heap;
destroy_pools:
	ion_camera_heap_destroy_pools(heap->reserved_pools);
err_create_reserved_pools:
	ion_camera_heap_destroy_pools(heap->cached_pools);
destroy_uncached_pools:
	ion_camera_heap_destroy_pools(heap->uncached_pools);
destroy_secure_pools:
	for (i = 0; i < VMID_LAST; i++) {
		if (heap->secure_pools[i])
			ion_camera_heap_destroy_pools(heap->secure_pools[i]);
	}
	kfree(heap);
	return ERR_PTR(ret);
}
