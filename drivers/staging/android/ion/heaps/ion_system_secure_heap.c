// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <soc/qcom/secure_buffer.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>

#include "ion_system_secure_heap.h"
#include "ion_msm_system_heap.h"
#include "ion_msm_page_pool.h"
#include "msm_ion_priv.h"
#include "ion_secure_util.h"

#define MAX_NR_PREFETCH_REGIONS 32
#define to_system_secure_heap(_heap) \
	container_of(to_msm_ion_heap(_heap), struct ion_system_secure_heap, \
		     heap)

struct ion_system_secure_heap {
	struct ion_heap *sys_heap;
	struct msm_ion_heap heap;

	/* Protects prefetch_list */
	spinlock_t work_lock;
	bool destroy_heap;
	struct list_head prefetch_list;
	struct delayed_work prefetch_work;
	struct workqueue_struct *prefetch_wq;
};

struct prefetch_info {
	struct list_head list;
	int vmid;
	u64 size;
	bool shrink;
};

/*
 * The video client may not hold the last reference count on the
 * ion_buffer(s). Delay for a short time after the video client sends
 * the IOC_DRAIN event to increase the chance that the reference
 * count drops to zero. Time in milliseconds.
 */
#define SHRINK_DELAY 1000

int ion_heap_is_system_secure_heap_type(enum ion_heap_type type)
{
	return type == ((enum ion_heap_type)ION_HEAP_TYPE_SYSTEM_SECURE);
}

static bool is_cp_flag_present(unsigned long flags)
{
	return flags & (ION_FLAG_CP_TOUCH |
			ION_FLAG_CP_BITSTREAM |
			ION_FLAG_CP_PIXEL |
			ION_FLAG_CP_NON_PIXEL |
			ION_FLAG_CP_CAMERA);
}

static void ion_system_secure_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_system_secure_heap *secure_heap =
		to_system_secure_heap(heap);

	buffer->heap = secure_heap->sys_heap;
	secure_heap->sys_heap->ops->free(buffer);
	/*
	 * Restore buffer's heap pointer to the system secure heap, so that
	 * the ION memory accounting code uses the system secure heap's stats
	 * instead of the system heap stats.
	 */
	buffer->heap = heap;
}

static int ion_system_secure_heap_allocate(struct ion_heap *heap,
					   struct ion_buffer *buffer,
					   unsigned long size,
					   unsigned long flags)
{
	int ret = 0;
	struct ion_system_secure_heap *secure_heap =
		to_system_secure_heap(heap);
	enum ion_heap_type type = secure_heap->heap.ion_heap.type;
	unsigned long cp_flags = buffer->flags & ION_FLAGS_CP_MASK;

	if (!ion_heap_is_system_secure_heap_type(type) ||
	    !is_cp_flag_present(flags) || (hweight_long(cp_flags) != 1)) {
		pr_info("%s: Incorrect heap type or incorrect flags\n",
			__func__);
		return -EINVAL;
	}

	ret = secure_heap->sys_heap->ops->allocate(secure_heap->sys_heap,
						buffer, size, flags);
	if (ret) {
		pr_info("%s: Failed to get allocation for %s, ret = %d\n",
			__func__, heap->name, ret);
		return ret;
	}
	return ret;
}

static void process_one_prefetch(struct ion_heap *sys_heap,
				 struct prefetch_info *info)
{
	struct ion_buffer buffer;
	int ret;
	int vmid;

	memset(&buffer, 0, sizeof(struct ion_buffer));
	buffer.heap = sys_heap;

	ret = sys_heap->ops->allocate(sys_heap, &buffer, info->size,
					buffer.flags);
	if (ret) {
		pr_debug("%s: Failed to prefetch 0x%llx, ret = %d\n",
			 __func__, info->size, ret);
		return;
	}

	vmid = get_secure_vmid(info->vmid);
	if (vmid < 0)
		goto out;

	ret = ion_hyp_assign_sg(buffer.sg_table, &vmid, 1, true);
	if (ret == -EADDRNOTAVAIL)
		goto out1;
	else if (ret < 0)
		goto out;

	/* Now free it to the secure heap */
	buffer.heap = sys_heap;
	buffer.flags = info->vmid;

out:
	sys_heap->ops->free(&buffer);
out1:
	/*
	 * The security state of the pages is unknown after a failure;
	 * They can neither be added back to the secure pool nor buddy system.
	 */
	return;
}

/*
 * Since no lock is held, results are approximate.
 */
size_t ion_system_secure_heap_page_pool_total(struct ion_heap *heap,
					      int vmid_flags)
{
	struct ion_msm_system_heap *sys_heap;
	struct ion_msm_page_pool *pool;
	size_t total = 0;
	int vmid, i;

	sys_heap = to_msm_system_heap(heap);
	vmid = get_secure_vmid(vmid_flags);
	if (vmid < 0)
		return 0;

	for (i = 0; i < NUM_ORDERS; i++) {
		pool = sys_heap->secure_pools[vmid][i];
		total += ion_msm_page_pool_total(pool, true);
	}

	return total << PAGE_SHIFT;
}

static void process_one_shrink(struct ion_system_secure_heap *secure_heap,
			       struct ion_heap *sys_heap,
			       struct prefetch_info *info)
{
	struct ion_buffer buffer;
	size_t pool_size, size;
	int ret;

	memset(&buffer, 0, sizeof(struct ion_buffer));
	buffer.heap = &secure_heap->heap.ion_heap;
	buffer.flags = info->vmid;

	pool_size = ion_system_secure_heap_page_pool_total(sys_heap,
							   info->vmid);
	size = min_t(size_t, pool_size, info->size);
	ret = sys_heap->ops->allocate(sys_heap, &buffer, size, buffer.flags);
	if (ret) {
		pr_debug("%s: Failed to shrink 0x%llx, ret = %d\n",
			 __func__, info->size, ret);
		return;
	}

	buffer.private_flags = ION_PRIV_FLAG_SHRINKER_FREE;
	buffer.heap = sys_heap;
	sys_heap->ops->free(&buffer);
}

static void ion_system_secure_heap_prefetch_work(struct work_struct *work)
{
	struct ion_system_secure_heap *secure_heap = container_of(work,
						struct ion_system_secure_heap,
						prefetch_work.work);
	struct ion_heap *sys_heap = secure_heap->sys_heap;
	struct prefetch_info *info, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&secure_heap->work_lock, flags);
	list_for_each_entry_safe(info, tmp,
				 &secure_heap->prefetch_list, list) {
		list_del(&info->list);
		spin_unlock_irqrestore(&secure_heap->work_lock, flags);

		if (info->shrink)
			process_one_shrink(secure_heap, sys_heap, info);
		else
			process_one_prefetch(sys_heap, info);

		kfree(info);
		spin_lock_irqsave(&secure_heap->work_lock, flags);
	}
	spin_unlock_irqrestore(&secure_heap->work_lock, flags);
}

static int alloc_prefetch_info(struct ion_prefetch_region *region,
			       bool shrink, struct list_head *items)
{
	struct prefetch_info *info;

	if (!is_secure_vmid_valid(get_secure_vmid(region->vmid)))
		return -EINVAL;

	info = kmalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->size = region->size;
	info->vmid = region->vmid;
	info->shrink = shrink;
	INIT_LIST_HEAD(&info->list);
	list_add_tail(&info->list, items);
	return 0;
}

static int __ion_system_secure_heap_resize(struct ion_heap *heap,
					   struct ion_prefetch_region *regions,
					   int nr_regions,
					   bool shrink)
{
	struct ion_system_secure_heap *secure_heap =
		to_system_secure_heap(heap);
	int i, ret = 0;
	struct prefetch_info *info, *tmp;
	unsigned long flags;
	LIST_HEAD(items);

	if (nr_regions > MAX_NR_PREFETCH_REGIONS)
		return -EINVAL;

	for (i = 0; i < nr_regions; i++) {
		ret = alloc_prefetch_info(&regions[i], shrink, &items);
		if (ret)
			goto out_free;
	}

	spin_lock_irqsave(&secure_heap->work_lock, flags);
	if (secure_heap->destroy_heap) {
		spin_unlock_irqrestore(&secure_heap->work_lock, flags);
		goto out_free;
	}
	list_splice_tail_init(&items, &secure_heap->prefetch_list);
	queue_delayed_work(secure_heap->prefetch_wq,
			   &secure_heap->prefetch_work,
			   shrink ?  msecs_to_jiffies(SHRINK_DELAY) : 0);
	spin_unlock_irqrestore(&secure_heap->work_lock, flags);

	return 0;

out_free:
	list_for_each_entry_safe(info, tmp, &items, list) {
		list_del(&info->list);
		kfree(info);
	}
	return ret;
}

static int ion_system_secure_heap_prefetch(struct ion_heap *heap,
					   struct ion_prefetch_region *regions,
					   int nr_regions)
{
	return __ion_system_secure_heap_resize(heap, regions, nr_regions,
					       false);
}

static int ion_system_secure_heap_drain(struct ion_heap *heap,
					struct ion_prefetch_region *regions,
					int nr_regions)
{
	return __ion_system_secure_heap_resize(heap, regions, nr_regions, true);
}

static int ion_system_secure_heap_shrink(struct ion_heap *heap, gfp_t gfp_mask,
					 int nr_to_scan)
{
	struct ion_system_secure_heap *secure_heap =
		to_system_secure_heap(heap);

	return secure_heap->sys_heap->ops->shrink(secure_heap->sys_heap,
						gfp_mask, nr_to_scan);
}

static struct ion_heap_ops system_secure_heap_ops = {
	.allocate = ion_system_secure_heap_allocate,
	.free = ion_system_secure_heap_free,
	.shrink = ion_system_secure_heap_shrink,
};

static struct msm_ion_heap_ops msm_system_secure_heap_ops = {
	.heap_prefetch = ion_system_secure_heap_prefetch,
	.heap_drain = ion_system_secure_heap_drain,
};

struct ion_heap *ion_system_secure_heap_create(struct ion_platform_heap *unused)
{
	struct ion_system_secure_heap *heap;
	struct ion_heap *sys_heap = get_ion_heap(ION_SYSTEM_HEAP_ID);

	if (!sys_heap) {
		pr_err("Sys heap missing; ensure it's specified in the DT, before the sys secure heap\n");
		return ERR_PTR(-EINVAL);
	}

	heap = kzalloc(sizeof(*heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->heap.ion_heap.ops = &system_secure_heap_ops;
	heap->heap.msm_heap_ops = &msm_system_secure_heap_ops;
	heap->heap.ion_heap.buf_ops = msm_ion_dma_buf_ops;
	heap->heap.ion_heap.type =
		(enum ion_heap_type)ION_HEAP_TYPE_SYSTEM_SECURE;
	heap->sys_heap = sys_heap;

	heap->destroy_heap = false;
	heap->work_lock = __SPIN_LOCK_UNLOCKED(heap->work_lock);
	INIT_LIST_HEAD(&heap->prefetch_list);
	INIT_DELAYED_WORK(&heap->prefetch_work,
			  ion_system_secure_heap_prefetch_work);

	heap->prefetch_wq = alloc_workqueue("system_secure_prefetch_wq",
					    WQ_UNBOUND | WQ_FREEZABLE, 0);
	if (!heap->prefetch_wq) {
		pr_err("Failed to create system secure prefetch workqueue\n");
		kfree(heap);
		return ERR_PTR(-ENOMEM);
	}

	return &heap->heap.ion_heap;
}

struct page *alloc_from_secure_pool_order(struct ion_msm_system_heap *heap,
					  struct ion_buffer *buffer,
					  unsigned long order)
{
	int vmid = get_secure_vmid(buffer->flags);
	struct ion_msm_page_pool *pool;

	if (!is_secure_vmid_valid(vmid))
		return ERR_PTR(-EINVAL);

	pool = heap->secure_pools[vmid][order_to_index(order)];
	return ion_msm_page_pool_alloc_pool_only(pool);
}

struct page *split_page_from_secure_pool(struct ion_msm_system_heap *heap,
					 struct ion_buffer *buffer)
{
	int i, j;
	struct page *page;
	unsigned int order;

	mutex_lock(&heap->split_page_mutex);

	/*
	 * Someone may have just split a page and returned the unused portion
	 * back to the pool, so try allocating from the pool one more time
	 * before splitting. We want to maintain large pages sizes when
	 * possible.
	 */
	page = alloc_from_secure_pool_order(heap, buffer, 0);
	if (!IS_ERR(page))
		goto got_page;

	for (i = NUM_ORDERS - 2; i >= 0; i--) {
		order = orders[i];
		page = alloc_from_secure_pool_order(heap, buffer, order);
		if (IS_ERR(page))
			continue;

		split_page(page, order);
		break;
	}
	/*
	 * Return the remaining order-0 pages to the pool.
	 * SetPagePrivate flag to mark memory as secure.
	 */
	if (!IS_ERR(page)) {
		for (j = 1; j < (1 << order); j++) {
			SetPagePrivate(page + j);
			free_buffer_page(heap, buffer, page + j, 0);
		}
	}
got_page:
	mutex_unlock(&heap->split_page_mutex);

	return page;
}

int ion_secure_page_pool_shrink(struct ion_msm_system_heap *sys_heap,
				int vmid, int order_idx, int nr_to_scan)
{
	int ret, freed = 0;
	int order = orders[order_idx];
	struct page *page, *tmp;
	struct sg_table sgt;
	struct scatterlist *sg;
	struct ion_msm_page_pool *pool =
		sys_heap->secure_pools[vmid][order_idx];
	LIST_HEAD(pages);

	if (nr_to_scan == 0)
		return ion_msm_page_pool_total(pool, true);

	while (freed < nr_to_scan) {
		page = ion_msm_page_pool_alloc_pool_only(pool);
		if (IS_ERR(page))
			break;
		list_add(&page->lru, &pages);
		freed += (1 << order);
	}

	if (!freed)
		return freed;

	ret = sg_alloc_table(&sgt, (freed >> order), GFP_KERNEL);
	if (ret)
		goto out1;
	sg = sgt.sgl;
	list_for_each_entry(page, &pages, lru) {
		sg_set_page(sg, page, (1 << order) * PAGE_SIZE, 0);
		sg_dma_address(sg) = page_to_phys(page);
		sg = sg_next(sg);
	}

	ret = ion_hyp_unassign_sg(&sgt, &vmid, 1, true);
	if (ret == -EADDRNOTAVAIL)
		goto out3;
	else if (ret < 0)
		goto out2;

	list_for_each_entry_safe(page, tmp, &pages, lru) {
		list_del(&page->lru);
		ion_msm_page_pool_free_immediate(pool, page);
	}

	sg_free_table(&sgt);
	return freed;

out2:
	sg_free_table(&sgt);
out1:
	/* Restore pages to secure pool */
	list_for_each_entry_safe(page, tmp, &pages, lru) {
		list_del(&page->lru);
		ion_msm_page_pool_free(pool, page);
	}
	return 0;
out3:
	/*
	 * The security state of the pages is unknown after a failure;
	 * They can neither be added back to the secure pool nor buddy system.
	 */
	sg_free_table(&sgt);
	return 0;
}
