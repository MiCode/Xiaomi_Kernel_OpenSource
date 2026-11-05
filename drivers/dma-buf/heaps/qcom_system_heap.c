// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF System heap exporter
 * Originally copied from: drivers/dma-buf/heaps/system_heap.c as of commit
 * 263e38f82cbb ("dma-buf: heaps: Remove redundant heap identifier from system
 * heap name")
 *
 * Additions taken from modifications to drivers/dma-buf/heaps/system-heap.c,
 * from patches submitted, are listed below:
 *
 * Addition that modifies dma_buf ops to use SG tables taken from
 * drivers/dma-buf/heaps/system-heap.c in:
 * https://lore.kernel.org/lkml/20201017013255.43568-2-john.stultz@linaro.org/
 *
 * Addition that skips unneeded syncs in the dma_buf ops taken from
 * https://lore.kernel.org/lkml/20201017013255.43568-5-john.stultz@linaro.org/
 *
 * Addition that tries to allocate higher order pages taken from
 * https://lore.kernel.org/lkml/20201017013255.43568-6-john.stultz@linaro.org/
 *
 * Addition that implements an uncached heap taken from
 * https://lore.kernel.org/lkml/20201017013255.43568-8-john.stultz@linaro.org/,
 * with our own modificaitons made to account for core kernel changes that are
 * a part of the patch series.
 *
 * Pooling functionality taken from:
 * Git-repo: https://git.linaro.org/people/john.stultz/android-dev.git
 * Branch: dma-buf-heap-perf
 * Git-commit: 6f080eb67dce63c6efa57ef564ca4cd762ccebb0
 * Git-commit: 6fb9593b928c4cb485bef4e88c59c6b9fdf11352
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2019, 2020 Linaro Ltd.
 *
 * Portions based off of Andrew Davis' SRAM heap:
 * Copyright (C) 2019 Texas Instruments Incorporated - http://www.ti.com/
 *	Andrew F. Davis <afd@ti.com>
 *
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/qcom_dma_heap.h>
#include <uapi/linux/sched/types.h>

#include "qcom_dma_heap_secure_utils.h"
#include "qcom_dynamic_page_pool.h"
#include "qcom_sg_ops.h"
#include "qcom_system_heap.h"
#include "qcom_system_movable_heap.h"
#include "../../../mm/internal.h"

#if IS_ENABLED(CONFIG_MI_LOW_LAT_MEM)
#include <trace/events/mi_low_lat_mem.h>

int mi_smart_refill_id = 0;
EXPORT_TRACEPOINT_SYMBOL_GPL(mi_smart_refill_alloc);
EXPORT_TRACEPOINT_SYMBOL_GPL(mi_smart_refill_deferred_free);
EXPORT_TRACEPOINT_SYMBOL_GPL(mi_smart_refill_sg_free);
EXPORT_TRACEPOINT_SYMBOL_GPL(mi_alloc_pre_compact_page);
#endif

#if IS_ENABLED(CONFIG_QCOM_DMABUF_HEAPS_PAGE_POOL_REFILL)
#define DYNAMIC_POOL_FILL_MARK (100 * SZ_1M)
#define DYNAMIC_POOL_LOW_MARK_PERCENT 40UL
#define DYNAMIC_POOL_LOW_MARK ((DYNAMIC_POOL_FILL_MARK * DYNAMIC_POOL_LOW_MARK_PERCENT) / 100)

#define DYNAMIC_POOL_REFILL_DEFER_WINDOW_MS 10
#define DYNAMIC_POOL_KTHREAD_NICE_VAL 10

static int get_dynamic_pool_fillmark(struct dynamic_page_pool *pool)
{
	return DYNAMIC_POOL_FILL_MARK / (PAGE_SIZE << pool->order);
}

static bool dynamic_pool_fillmark_reached(struct dynamic_page_pool *pool)
{
	return atomic_read(&pool->count) >= get_dynamic_pool_fillmark(pool);
}

static int get_dynamic_pool_lowmark(struct dynamic_page_pool *pool)
{
	return DYNAMIC_POOL_LOW_MARK / (PAGE_SIZE << pool->order);
}

static bool dynamic_pool_count_below_lowmark(struct dynamic_page_pool *pool)
{
	return atomic_read(&pool->count) < get_dynamic_pool_lowmark(pool);
}

/* Based on gfp_zone() in mm/mmzone.c since it is not exported. */
enum zone_type dynamic_pool_gfp_zone(gfp_t flags)
{
	enum zone_type z;
	gfp_t local_flags = flags;
	int bit;

	bit = (__force int) ((local_flags) & GFP_ZONEMASK);

	z = (GFP_ZONE_TABLE >> (bit * GFP_ZONES_SHIFT)) &
					 ((1 << GFP_ZONES_SHIFT) - 1);
	VM_BUG_ON((GFP_ZONE_BAD >> bit) & 1);
	return z;
}

/*
 * Based on __zone_watermark_ok() in mm/page_alloc.c since it is not exported.
 *
 * Return true if free base pages are above 'mark'. For high-order checks it
 * will return true of the order-0 watermark is reached and there is at least
 * one free page of a suitable size. Checking now avoids taking the zone lock
 * to check in the allocation paths if no pages are free.
 */
static bool __dynamic_pool_zone_watermark_ok(struct zone *z, unsigned int order, unsigned long mark,
					     int highest_zoneidx, long free_pages)
{
	long min = mark;
	long unusable_free;
	int o;

	/*
	 * Access to high atomic reserves is not required, and CMA should not be
	 * used, since these allocations are non-movable.
	 */
	unusable_free = ((1 << order) - 1) + z->nr_reserved_highatomic;
#ifdef CONFIG_CMA
	unusable_free += zone_page_state(z, NR_FREE_CMA_PAGES);
#endif

	/* free_pages may go negative - that's OK */
	free_pages -= unusable_free;

	/*
	 * Check watermarks for an order-0 allocation request. If these
	 * are not met, then a high-order request also cannot go ahead
	 * even if a suitable page happened to be free.
	 *
	 * 'min' can be taken as 'mark' since we do not expect these allocations
	 * to require disruptive actions (such as running the OOM killer) or
	 * a lot of effort.
	 */
	if (free_pages <= min + z->lowmem_reserve[highest_zoneidx])
		return false;

	/* If this is an order-0 request then the watermark is fine */
	if (!order)
		return true;

	/* For a high-order request, check at least one suitable page is free */
	for (o = order; o < MAX_ORDER; o++) {
		struct free_area *area = &z->free_area[o];
		int mt;

		if (!area->nr_free)
			continue;

		for (mt = 0; mt < MIGRATE_PCPTYPES; mt++) {
#ifdef CONFIG_CMA
			/*
			 * Note that this check is needed only
			 * when MIGRATE_CMA < MIGRATE_PCPTYPES.
			 */
			if (mt == MIGRATE_CMA)
				continue;
#endif
			if (!free_area_empty(area, mt))
				return true;
		}
	}

	return false;
}

/* Based on zone_watermark_ok_safe from mm/page_alloc.c since it is not exported. */
bool dynamic_pool_zone_watermark_ok_safe(struct zone *z, unsigned int order,
						unsigned long mark, int highest_zoneidx)
{
	long free_pages = zone_page_state(z, NR_FREE_PAGES);

	if (z->percpu_drift_mark && free_pages < z->percpu_drift_mark)
		free_pages = zone_page_state_snapshot(z, NR_FREE_PAGES);

	return __dynamic_pool_zone_watermark_ok(z, order, mark, highest_zoneidx, free_pages);
}

/* do a simple check to see if we are in any low memory situation */
static bool dynamic_pool_refill_ok(struct dynamic_page_pool *pool)
{
	struct zone *zone;
	int i, mark;
	enum zone_type classzone_idx = dynamic_pool_gfp_zone(pool->gfp_mask);
	s64 delta;

	/* check if we are within the refill defer window */
	delta = ktime_ms_delta(ktime_get(), pool->last_low_watermark_ktime);
	if (delta < DYNAMIC_POOL_REFILL_DEFER_WINDOW_MS)
		return false;

	/*
	 * make sure that if we allocate a pool->order page from buddy,
	 * we don't put the zone watermarks below the high threshold.
	 * This makes sure there's no unwanted repetitive refilling and
	 * reclaiming of buddy pages on the pool.
	 */
	for (i = classzone_idx; i >= 0; i--) {
		zone = &NODE_DATA(numa_node_id())->node_zones[i];

		if (!strcmp(zone->name, "DMA32"))
			continue;

		mark = high_wmark_pages(zone);
		mark += 1 << pool->order;
		if (!dynamic_pool_zone_watermark_ok_safe(zone, pool->order, mark, classzone_idx)) {
			pool->last_low_watermark_ktime = ktime_get();
			return false;
		}
	}

	return true;
}

static void dynamic_page_pool_refill(struct dynamic_page_pool *pool)
{
	struct page *page;
	gfp_t gfp_refill = (pool->gfp_mask | __GFP_RECLAIM) & ~__GFP_NORETRY;

	/* skip refilling order 0 pools */
	if (!pool->order)
		return;

	while (!dynamic_pool_fillmark_reached(pool) && dynamic_pool_refill_ok(pool)) {
		page = alloc_pages(gfp_refill, pool->order);
		if (!page)
			break;

		dynamic_page_pool_add(pool, page);
	}
}

#if IS_ENABLED(CONFIG_MI_LOW_LAT_MEM)
#define SR_ORDER_9 9
#define SR_ORDER_4 4
#define SR_ORDER_9_THRESHOLD 50
#define SR_ORDER_4_THRESHOLD 500
void mi_smart_refill_dynamic_page_pool_refill(int id, int count)
{
	struct page *page;
	struct dynamic_page_pool *pool;
	struct mi_smart_refill_data *data;
	int count_l = 0;
	int count_org = 0;
	int pre_count = 0;
	int threshold = 0;
	gfp_t gfp_refill;
	bool refill_ok = true;

	pool = (struct dynamic_page_pool *)mi_smart_refill_find_pool(id);
	if (IS_ERR_OR_NULL(pool))
		return;
	count_org = atomic_read(&pool->count);
	refill_ok = dynamic_pool_refill_ok(pool);
	data = (struct mi_smart_refill_data *)(pool->sr_data);
	if (IS_ERR_OR_NULL(data) || !data->active)
		return;
	if (!pool->order)
		return;
	if (pool->order == SR_ORDER_9)
		threshold = SR_ORDER_9_THRESHOLD;
	else if (pool->order == SR_ORDER_4)
		threshold = SR_ORDER_4_THRESHOLD;
	if (threshold > 0) {
		int need = max(count, threshold);

		if (need > count_org)
			count_l = need - count_org;
	}
	gfp_refill = (pool->gfp_mask | __GFP_RECLAIM) & ~__GFP_NORETRY;
	while (count_l--) {
		page = NULL;
		trace_mi_alloc_pre_compact_page(pool->gfp_mask,
				pool->order, &page);
		if (IS_ERR_OR_NULL(page)) {
			if (refill_ok)
				page = alloc_pages(gfp_refill,
						pool->order);
		} else
			pre_count++;
		if (IS_ERR_OR_NULL(page))
			break;
		dynamic_page_pool_add(pool, page);
	}
	pr_debug("low_lat_mem: %s count:%d pre_count:%d order:%u (p_cnt-b_cnt:%llu-%llu)\n",
			__func__, count, pre_count, pool->order,
			data->dmabuf_alloc_from_pool_cnt,
			data->dmabuf_alloc_from_buddy_cnt);
}
EXPORT_SYMBOL_GPL(mi_smart_refill_dynamic_page_pool_refill);
#endif

static bool dynamic_pool_needs_refill(struct dynamic_page_pool *pool)
{
	return pool->order && dynamic_pool_count_below_lowmark(pool);
}

static int system_heap_refill_worker(void *data)
{
	struct dynamic_page_pool **pool_list = data;
	int i;

	for (;;) {
		for (i = 0; i < NUM_ORDERS; i++) {
			if (dynamic_pool_count_below_lowmark(pool_list[i]))
				dynamic_page_pool_refill(pool_list[i]);
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

static int system_heap_create_refill_worker(struct qcom_system_heap *sys_heap, const char *name)
{
	struct task_struct *refill_worker;
	struct sched_attr attr = { .sched_nice = DYNAMIC_POOL_KTHREAD_NICE_VAL };
	int ret;
	int i;

	refill_worker = kthread_run(system_heap_refill_worker, sys_heap->pool_list,
				    "%s-pool-refill-thread", name);
	if (IS_ERR(refill_worker)) {
		pr_err("%s: failed to create %s-pool-refill-thread: %ld\n",
			__func__, name, PTR_ERR(refill_worker));
		return PTR_ERR(refill_worker);
	}

	ret = sched_setattr(refill_worker, &attr);
	if (ret) {
		pr_warn("%s: failed to set task priority for %s-pool-refill-thread: ret = %d\n",
			__func__, name, ret);
		kthread_stop(refill_worker);
		return ret;
	}

	for (i = 0; i < NUM_ORDERS; i++)
		sys_heap->pool_list[i]->refill_worker = refill_worker;

	return ret;
}

static void system_heap_destroy_refill_worker(struct qcom_system_heap *sys_heap)
{
	kthread_stop(sys_heap->pool_list[0]->refill_worker);
}
#else
static bool dynamic_pool_needs_refill(struct dynamic_page_pool *pool)
{
	return false;
}

static int system_heap_create_refill_worker(struct qcom_system_heap *sys_heap, const char *name)
{
	return 0;
}

static void system_heap_destroy_refill_worker(struct qcom_system_heap *sys_heap)
{
}
#endif

static int system_heap_clear_pages(struct page **pages, int num, pgprot_t pgprot)
{
	void *addr = vmap(pages, num, VM_MAP, pgprot);

	if (!addr)
		return -ENOMEM;
	memset(addr, 0, PAGE_SIZE * num);
	vunmap(addr);
	return 0;
}

static int system_heap_zero_buffer(struct qcom_sg_buffer *buffer)
{
	struct sg_table *sgt = &buffer->sg_table;
	struct sg_page_iter piter;
	struct page *pages[32];
	int p = 0;
	int ret = 0;

	for_each_sgtable_page(sgt, &piter, 0) {
		pages[p++] = sg_page_iter_page(&piter);
		if (p == ARRAY_SIZE(pages)) {
			ret = system_heap_clear_pages(pages, p, PAGE_KERNEL);
			if (ret)
				return ret;
			p = 0;
		}
	}
	if (p)
		ret = system_heap_clear_pages(pages, p, PAGE_KERNEL);

	return ret;
}

#if IS_ENABLED(CONFIG_MI_LOW_LAT_MEM)
static void mi_smart_refill_data_init(struct dynamic_page_pool **pool_list,
		void **data)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++)
		data[i] = pool_list[i]->sr_data;
}
#endif

static void system_heap_deferred_free(struct deferred_freelist_item *item,
				 enum df_reason reason)
{
	struct qcom_system_heap *sys_heap;
	struct qcom_sg_buffer *buffer;
	struct sg_table *table;
	struct scatterlist *sg;
	int i, j;
#if IS_ENABLED(CONFIG_MI_LOW_LAT_MEM)
	void *data[NUM_ORDERS];
#endif

	buffer = container_of(item, struct qcom_sg_buffer, deferred_free);
	sys_heap = dma_heap_get_drvdata(buffer->heap);
#if IS_ENABLED(CONFIG_MI_LOW_LAT_MEM)
	mi_smart_refill_data_init(sys_heap->pool_list, &(data[0]));
#endif
	/* Zero the buffer pages before adding back to the pool */
	if (reason == DF_NORMAL)
		if (system_heap_zero_buffer(buffer))
			reason = DF_UNDER_PRESSURE; // On failure, just free

	table = &buffer->sg_table;
	for_each_sg(table->sgl, sg, table->nents, i) {
		struct page *page = sg_page(sg);
#if IS_ENABLED(CONFIG_MI_LOW_LAT_MEM)
		trace_mi_smart_refill_deferred_free(&(data[0]), NUM_ORDERS, page);
#endif
		/* Do not keep page in the pool if it is a zone movable page */
		if (is_zone_movable_page(page)) {
			/* Matches get_page() in qcom_movable_heap_alloc_pages() */
			put_page(page);
			__free_pages(page, compound_order(page));
		} else if (reason == DF_UNDER_PRESSURE) {
			__free_pages(page, compound_order(page));
		} else {
			for (j = 0; j < NUM_ORDERS; j++) {
				if (compound_order(page) == orders[j])
					break;
			}
				dynamic_page_pool_free(sys_heap->pool_list[j], page);
		}
	}
	sg_free_table(table);
	kfree(buffer);
}

void qcom_system_heap_free(struct qcom_sg_buffer *buffer)
{
	deferred_free(&buffer->deferred_free, system_heap_deferred_free,
			PAGE_ALIGN(buffer->len) / PAGE_SIZE);
}

struct page *qcom_sys_heap_alloc_largest_available(struct dynamic_page_pool **pools,
						   unsigned long size,
						   unsigned int max_order,
						   bool movable)
{
	struct page *page = NULL;
	int i;
#if IS_ENABLED(CONFIG_MI_LOW_LAT_MEM)
	int dmabuf_alloc_from_buddy = 0;
#endif

	for (i = 0; i < NUM_ORDERS; i++) {
		unsigned long flags;
#if IS_ENABLED(CONFIG_MI_LOW_LAT_MEM)
		dmabuf_alloc_from_buddy = 0;
#endif

		if (size <  (PAGE_SIZE << orders[i]))
			continue;
		if (max_order < orders[i])
			continue;

		spin_lock_irqsave(&pools[i]->lock, flags);
		if (pools[i]->high_count)
			page = dynamic_page_pool_remove(pools[i], true);
		else if (pools[i]->low_count)
			page = dynamic_page_pool_remove(pools[i], false);
		spin_unlock_irqrestore(&pools[i]->lock, flags);

		if (!page && movable)
			page = qcom_movable_heap_alloc_pages(pools[i]);
		if (!page)
			page = alloc_pages(pools[i]->gfp_mask, pools[i]->order);
#if IS_ENABLED(CONFIG_MI_LOW_LAT_MEM)
		if (page)
			dmabuf_alloc_from_buddy = 1;
#endif
		if (!page)
			continue;

		if (dynamic_pool_needs_refill(pools[i]))
			wake_up_process(pools[i]->refill_worker);
#if IS_ENABLED(CONFIG_MI_LOW_LAT_MEM)
		trace_mi_smart_refill_alloc(pools[i]->sr_data, page, dmabuf_alloc_from_buddy);
#endif
		return page;
	}
	return NULL;
}

int system_qcom_sg_buffer_alloc(struct dma_heap *heap,
				struct qcom_sg_buffer *buffer,
				unsigned long len,
				bool movable)
{
	struct qcom_system_heap *sys_heap;
	unsigned long size_remaining = len;
	unsigned int max_order = orders[0];
	struct sg_table *table;
	struct scatterlist *sg;
	struct list_head pages;
	struct page *page, *tmp_page;
	int i, ret = -ENOMEM;
#if IS_ENABLED(CONFIG_MI_LOW_LAT_MEM)
	void *data[NUM_ORDERS];
#endif

	sys_heap = dma_heap_get_drvdata(heap);
#if IS_ENABLED(CONFIG_MI_LOW_LAT_MEM)
	mi_smart_refill_data_init(sys_heap->pool_list, &(data[0]));
#endif

	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);
	buffer->heap = heap;
	buffer->len = len;
	buffer->uncached = sys_heap->uncached;
	buffer->free = qcom_system_heap_free;

	INIT_LIST_HEAD(&pages);
	i = 0;
	while (size_remaining > 0) {
		/*
		 * Avoid trying to allocate memory if the process
		 * has been killed by SIGKILL
		 */
		if (fatal_signal_pending(current))
			goto free_mem;

		page = qcom_sys_heap_alloc_largest_available(sys_heap->pool_list,
							     size_remaining,
							     max_order,
							     movable);
		if (!page)
			goto free_mem;

		list_add_tail(&page->lru, &pages);
		size_remaining -= page_size(page);
		max_order = compound_order(page);
		i++;
	}

	table = &buffer->sg_table;
	if (sg_alloc_table(table, i, GFP_KERNEL))
		goto free_mem;

	sg = table->sgl;
	list_for_each_entry_safe(page, tmp_page, &pages, lru) {
		sg_set_page(sg, page, page_size(page), 0);
		sg = sg_next(sg);
		list_del(&page->lru);
	}

	/*
	 * For uncached buffers, we need to initially flush cpu cache, since
	 * the __GFP_ZERO on the allocation means the zeroing was done by the
	 * cpu and thus it is likely cached. Map (and implicitly flush) and
	 * unmap it now so we don't get corruption later on.
	 */
	if (buffer->uncached) {
		dma_map_sgtable(dma_heap_get_dev(heap), table, DMA_BIDIRECTIONAL, 0);
		dma_unmap_sgtable(dma_heap_get_dev(heap), table, DMA_BIDIRECTIONAL, 0);
	}

	return 0;

free_mem:
	list_for_each_entry_safe(page, tmp_page, &pages, lru) {
#if IS_ENABLED(CONFIG_MI_LOW_LAT_MEM)
		trace_mi_smart_refill_sg_free(&(data[0]), NUM_ORDERS, page);
#endif
		/* Unpin the memory first if it was borrowed from movable zone */
		if (is_zone_movable_page(page))
			put_page(page);
		__free_pages(page, compound_order(page));
	}

	return ret;
}

static struct dma_buf *system_heap_allocate(struct dma_heap *heap,
					    unsigned long len,
					    u32 fd_flags,
					    u64 heap_flags)
{
	struct qcom_sg_buffer *buffer;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *dmabuf;
	int ret;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	ret = system_qcom_sg_buffer_alloc(heap, buffer, len, false);
	if (ret)
		goto free_buf_struct;

	buffer->vmperm = mem_buf_vmperm_alloc(&buffer->sg_table);
	if (IS_ERR(buffer->vmperm)) {
		ret = PTR_ERR(buffer->vmperm);
		goto free_sys_heap_mem;
	}

	/* create the dmabuf */
	exp_info.exp_name = dma_heap_get_name(heap);
	exp_info.size = buffer->len;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer;
	dmabuf = qcom_dma_buf_export(&exp_info, &qcom_sg_buf_ops);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto free_vmperm;
	}

	return dmabuf;

free_vmperm:
	mem_buf_vmperm_release(buffer->vmperm);
free_sys_heap_mem:
	qcom_system_heap_free(buffer);
	return ERR_PTR(ret);
free_buf_struct:
	kfree(buffer);

	return ERR_PTR(ret);
}

static long get_pool_size_bytes(struct dma_heap *heap)
{
	long total_size = 0;
	int i;
	struct qcom_system_heap *sys_heap = dma_heap_get_drvdata(heap);

	for (i = 0; i < NUM_ORDERS; i++)
		total_size += dynamic_page_pool_total(sys_heap->pool_list[i], true);

	return total_size << PAGE_SHIFT;
}

static const struct dma_heap_ops system_heap_ops = {
	.allocate = system_heap_allocate,
	.get_pool_size = get_pool_size_bytes,
};

#if IS_ENABLED(CONFIG_MI_LOW_LAT_MEM)
void mi_smart_refill_heap_active(struct dynamic_page_pool **pool_list,
		int num_order)
{
	int i;
	struct mi_smart_refill_data *data;

	for (i = 0; i < num_order; i++) {
		data = (struct mi_smart_refill_data *)(pool_list[i]->sr_data);
		data->active = 1;
		data->id = mi_smart_refill_id++;
	}
}
#endif

void qcom_system_heap_create(const char *name, const char *system_alias, bool uncached)
{
	struct dma_heap_export_info exp_info;
	struct dma_heap *heap;
	struct qcom_system_heap *sys_heap;
	int ret;

	ret = dynamic_page_pool_init_shrinker();
	if (ret)
		goto out;

	sys_heap = kzalloc(sizeof(*sys_heap), GFP_KERNEL);
	if (!sys_heap) {
		ret = -ENOMEM;
		goto out;
	}

	exp_info.name = name;
	exp_info.ops = &system_heap_ops;
	exp_info.priv = sys_heap;

	sys_heap->uncached = uncached;

	sys_heap->pool_list = dynamic_page_pool_create_pools(0, NULL);
	if (IS_ERR(sys_heap->pool_list)) {
		ret = PTR_ERR(sys_heap->pool_list);
		goto free_heap;
	}

#if IS_ENABLED(CONFIG_MI_LOW_LAT_MEM)
	mi_smart_refill_heap_active(sys_heap->pool_list, NUM_ORDERS);
#endif

	ret = system_heap_create_refill_worker(sys_heap, name);
	if (ret)
		goto free_pools;

	heap = dma_heap_add(&exp_info);
	if (IS_ERR(heap)) {
		ret = PTR_ERR(heap);
		goto stop_worker;
	}

	if (uncached)
		dma_coerce_mask_and_coherent(dma_heap_get_dev(heap),
					     DMA_BIT_MASK(64));

	pr_info("%s: DMA-BUF Heap: Created '%s'\n", __func__, name);

	if (system_alias != NULL) {
		exp_info.name = system_alias;

		heap = dma_heap_add(&exp_info);
		if (IS_ERR(heap)) {
			pr_err("%s: Failed to create '%s', error is %ld\n", __func__,
			       system_alias, PTR_ERR(heap));
			return;
		}

		dma_coerce_mask_and_coherent(dma_heap_get_dev(heap), DMA_BIT_MASK(64));

		pr_info("%s: DMA-BUF Heap: Created '%s'\n", __func__, system_alias);
	}

	return;

stop_worker:
	system_heap_destroy_refill_worker(sys_heap);

free_pools:
	dynamic_page_pool_release_pools(sys_heap->pool_list);

free_heap:
	kfree(sys_heap);

out:
	pr_err("%s: Failed to create '%s', error is %d\n", __func__, name, ret);
}
