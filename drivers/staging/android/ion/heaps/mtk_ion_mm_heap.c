// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "[ION] " fmt

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
#include <linux/iommu.h>
#include <dt-bindings/memory/mtk-memory-port.h>
#include "ion_page_pool.h"

#define NUM_ORDERS ARRAY_SIZE(orders)

static gfp_t high_order_gfp_flags = (GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN |
				     __GFP_NORETRY) & ~__GFP_RECLAIM;
static gfp_t low_order_gfp_flags  = GFP_HIGHUSER | __GFP_ZERO;
static const unsigned int orders[] = {8, 4, 0};

struct dev_info {
	struct device		*dev;
	enum dma_data_direction	direction;
	unsigned long		map_attrs;
};

struct ion_mm_buf_info {
	bool			mapped[MTK_M4U_DOM_NR_MAX];
	struct dev_info		dev_class[MTK_M4U_DOM_NR_MAX];
	struct sg_table		*table[MTK_M4U_DOM_NR_MAX];
	struct mutex		lock; /* iova mapping lock */
	pid_t			pid;
	pid_t			tid;
	char			task_comm[TASK_COMM_LEN];
	char			thread_comm[TASK_COMM_LEN];
};

struct mtk_iommu_iova_resv {
	dma_addr_t		iova_base;
	size_t			size;
};

/*
 * resv_iova include CCU iova or other HW which has dma_address
 * limitation.
 * resv_iova objective:
 * Before ION put unused buffer into freelist, resv_iova must be
 * unmapped firstly to avoid specific iova is allocated failed.
 * This is common flow !!
 */
static const struct mtk_iommu_iova_resv resv_iova[] = {
	{ .iova_base = 0x240000000ULL, .size = 0x4000000}, /* CCU0 */
	{ .iova_base = 0x244000000ULL, .size = 0x4000000}, /* CCU1 */
};

static bool iova_is_reserved(struct device *dev)
{
	int i;
	struct iommu_domain *domain = iommu_get_domain_for_dev(dev);

	for (i = 0; i < ARRAY_SIZE(resv_iova); i++) {
		if ((domain->geometry.aperture_start ==
				resv_iova[i].iova_base) &&
		    (domain->geometry.aperture_end ==
				resv_iova[i].iova_base + resv_iova[i].size - 1))
			return true;
	}

	return false;
}

static int order_to_index(unsigned int order)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++)
		if (order == orders[i])
			return i;
	/* BUG(); */
	return -1;
}

static inline unsigned int order_to_size(int order)
{
	return PAGE_SIZE << order;
}

struct ion_mtk_mm_heap {
	struct ion_heap heap;
	struct ion_page_pool *pools[NUM_ORDERS];
};

static struct page *alloc_buffer_page(struct ion_mtk_mm_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long order)
{
	struct ion_page_pool *pool = heap->pools[order_to_index(order)];

	return ion_page_pool_alloc(pool);
}

static void free_buffer_page(struct ion_mtk_mm_heap *heap,
			     struct ion_buffer *buffer, struct page *page)
{
	struct ion_page_pool *pool;
	unsigned int order = compound_order(page);

	/* go to system */
	if (buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE) {
		__free_pages(page, order);
		return;
	}

	pool = heap->pools[order_to_index(order)];

	ion_page_pool_free(pool, page);
}

static struct page *alloc_largest_available(struct ion_mtk_mm_heap *heap,
					    struct ion_buffer *buffer,
					    unsigned long size,
					    unsigned int max_order)
{
	struct page *page;
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		if (size < order_to_size(orders[i]))
			continue;
		if (max_order < orders[i])
			continue;

		page = alloc_buffer_page(heap, buffer, orders[i]);
		if (!page)
			continue;

		return page;
	}

	return NULL;
}

static int ion_mm_heap_allocate(struct ion_heap *heap,
				struct ion_buffer *buffer,
				unsigned long size,
				unsigned long flags)
{
	struct ion_mtk_mm_heap *mm_heap =
				container_of(heap,
					     struct ion_mtk_mm_heap,
					     heap);
	struct ion_mm_buf_info *buf_info;
	struct sg_table *table;
	struct scatterlist *sg;
	struct list_head pages;
	struct page *page, *tmp_page;
	struct task_struct *task = current->group_leader;
	int i = 0;
	unsigned long size_remaining = PAGE_ALIGN(size);
	unsigned int max_order = orders[0];

	if (size / PAGE_SIZE > totalram_pages() / 2)
		return -ENOMEM;

	INIT_LIST_HEAD(&pages);
	while (size_remaining > 0) {
		page = alloc_largest_available(mm_heap, buffer,
					       size_remaining,
					       max_order);
		if (!page)
			goto free_pages;
		list_add_tail(&page->lru, &pages);
		size_remaining -= page_size(page);
		max_order = compound_order(page);
		i++;
	}
	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		goto free_pages;

	if (sg_alloc_table(table, i, GFP_KERNEL))
		goto free_table;

	sg = table->sgl;
	list_for_each_entry_safe(page, tmp_page, &pages, lru) {
		sg_set_page(sg, page, page_size(page), 0);
		sg = sg_next(sg);
		list_del(&page->lru);
	}

	buffer->sg_table = table;

	ion_buffer_prep_noncached(buffer);

	buf_info = kzalloc(sizeof(*buf_info), GFP_KERNEL);
	if (!buf_info)
		goto  free_table;

	buffer->priv_virt = buf_info;
	mutex_init(&buf_info->lock);

	get_task_comm(buf_info->task_comm, task);
	buf_info->pid = task_pid_nr(task);
	buf_info->tid = task_pid_nr(current);
	get_task_comm(buf_info->thread_comm, current);

	return 0;

free_table:
	kfree(table);
free_pages:
	list_for_each_entry_safe(page, tmp_page, &pages, lru)
		free_buffer_page(mm_heap, buffer, page);
	return -ENOMEM;
}

static void ion_mm_heap_free(struct ion_buffer *buffer)
{
	struct ion_mtk_mm_heap *mm_heap =
				container_of(buffer->heap,
					     struct ion_mtk_mm_heap,
					     heap);
	struct sg_table *table = buffer->sg_table;
	struct ion_mm_buf_info *buf_info = buffer->priv_virt;
	struct scatterlist *sg;
	int i;

	/* unmap iova */
	for (i = 0; i < MTK_M4U_DOM_NR_MAX; i++) {
		struct sg_table	*table_buf = buf_info->table[i];
		struct dev_info dev_info = buf_info->dev_class[i];
		unsigned long attrs = dev_info.map_attrs | DMA_ATTR_SKIP_CPU_SYNC;

		if (!buf_info->mapped[i])
			continue;

		dma_unmap_sg_attrs(dev_info.dev, table_buf->sgl, table_buf->nents,
				   dev_info.direction, attrs);
		sg_free_table(table_buf);
		kfree(table_buf);
	}

	/* zero the buffer before goto page pool */
	if (!(buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE))
		ion_buffer_zero(buffer);

	for_each_sg(table->sgl, sg, table->nents, i)
		free_buffer_page(mm_heap, buffer, sg_page(sg));
	sg_free_table(table);
	kfree(buf_info);
	kfree(table);
}

static int ion_mm_heap_shrink(struct ion_heap *heap, gfp_t gfp_mask,
			      int nr_to_scan)
{
	struct ion_page_pool *pool;
	struct ion_mtk_mm_heap *mm_heap;
	int nr_total = 0;
	int i, nr_freed;
	int only_scan = 0;

	mm_heap = container_of(heap, struct ion_mtk_mm_heap, heap);

	if (!nr_to_scan)
		only_scan = 1;

	for (i = 0; i < NUM_ORDERS; i++) {
		pool = mm_heap->pools[i];

		if (only_scan) {
			nr_total += ion_page_pool_shrink(pool,
							 gfp_mask,
							 nr_to_scan);

		} else {
			nr_freed = ion_page_pool_shrink(pool,
							gfp_mask,
							nr_to_scan);
			nr_to_scan -= nr_freed;
			nr_total += nr_freed;
			if (nr_to_scan <= 0)
				break;
		}
	}
	return nr_total;
}

static long ion_mm_get_pool_size(struct ion_heap *heap)
{
	struct ion_mtk_mm_heap *mm_heap;
	long total_pages = 0;
	int i;

	mm_heap = container_of(heap, struct ion_mtk_mm_heap, heap);
	for (i = 0; i < NUM_ORDERS; i++)
		total_pages += ion_page_pool_nr_pages(mm_heap->pools[i]);

	return total_pages;
}

static void ion_mm_heap_destroy_pools(struct ion_page_pool **pools)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++)
		if (pools[i])
			ion_page_pool_destroy(pools[i]);
}

static int ion_mm_heap_create_pools(struct ion_page_pool **pools)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		struct ion_page_pool *pool;
		gfp_t gfp_flags = low_order_gfp_flags;

		if (orders[i] > 4)
			gfp_flags = high_order_gfp_flags;

		pool = ion_page_pool_create(gfp_flags, orders[i]);
		if (!pool)
			goto err_create_pool;
		pools[i] = pool;
	}

	return 0;

err_create_pool:
	ion_mm_heap_destroy_pools(pools);
	return -ENOMEM;
}

/* source copy to dest */
static int copy_sg_table(struct sg_table *source, struct sg_table *dest)
{
	int i;
	struct scatterlist *sg, *dest_sg;

	if (source->nents != dest->nents)
		return -EINVAL;

	dest_sg = dest->sgl;
	for_each_sg(source->sgl, sg, source->nents, i) {
		memcpy(dest_sg, sg, sizeof(*sg));
		dest_sg = sg_next(dest_sg);
	}

	return 0;
};

static int update_buffer_info(struct ion_mm_buf_info *buf_info,
			      struct sg_table *table,
			      struct dma_buf_attachment *attachment,
			      enum dma_data_direction direction,
			      int dom_id)
{
	int ret;

	buf_info->table[dom_id] = kzalloc(sizeof(*buf_info->table[dom_id]),
					  GFP_KERNEL);
	if (!buf_info->table[dom_id])
		return -ENOMEM;

	ret = sg_alloc_table(buf_info->table[dom_id], table->nents, GFP_KERNEL);
	if (ret) {
		kfree(buf_info->table[dom_id]);
		return -ENOMEM;
	}

	ret = copy_sg_table(table, buf_info->table[dom_id]);
	if (ret)
		return ret;

	buf_info->mapped[dom_id] = true;
	buf_info->dev_class[dom_id].dev = attachment->dev;
	buf_info->dev_class[dom_id].direction = direction;
	buf_info->dev_class[dom_id].map_attrs = attachment->dma_map_attrs;

	return ret;
}

static struct
sg_table *mtk_ion_map_dma_buf(struct dma_buf_attachment *attachment,
			      enum dma_data_direction direction)
{
	struct ion_buffer *buffer = attachment->dmabuf->priv;
	struct ion_mm_buf_info *buf_info = buffer->priv_virt;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(attachment->dev);
	struct ion_dma_buf_attachment *a;
	struct sg_table *table;
	unsigned long attrs = attachment->dma_map_attrs | DMA_ATTR_SKIP_CPU_SYNC;
	int ret, dom_id = MTK_M4U_DOM_NR_MAX;

	a = attachment->priv;
	table = a->table;
	if (!fwspec) {
		if (!dma_map_sg_attrs(attachment->dev, table->sgl, table->nents,
				      direction, attrs)) {
			pr_info("%s non-iommu map fail dom:%d, dev:%s\n",
				__func__, dom_id, dev_name(attachment->dev));
			return ERR_PTR(-ENOMEM);
		}
		a->mapped = true;
		return table;
	}

	dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
	mutex_lock(&buf_info->lock);
	if (buf_info->mapped[dom_id]) {
		ret = copy_sg_table(buf_info->table[dom_id], table);
		if (ret) {
			mutex_unlock(&buf_info->lock);
			return ERR_PTR(-EINVAL);
		}
		mutex_unlock(&buf_info->lock);
		return table;
	}
	if (!dma_map_sg_attrs(attachment->dev, table->sgl, table->nents,
			      direction, attrs)) {
		pr_info("%s iommu map fail dom:%d, dev:%s\n",
			__func__, dom_id, dev_name(attachment->dev));
		mutex_unlock(&buf_info->lock);
		return ERR_PTR(-ENOMEM);
	}
	ret = update_buffer_info(buf_info, table,
				 attachment, direction, dom_id);
	if (ret) {
		mutex_unlock(&buf_info->lock);
		return ERR_PTR(-ENOMEM);
	}
	a->mapped = true;
	mutex_unlock(&buf_info->lock);

	return table;
};

static void mtk_ion_unmap_dma_buf(struct dma_buf_attachment *attachment,
				  struct sg_table *table,
				  enum dma_data_direction direction)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(attachment->dev);
	struct ion_dma_buf_attachment *a = attachment->priv;
	unsigned long attrs = attachment->dma_map_attrs | DMA_ATTR_SKIP_CPU_SYNC;

	if (!fwspec) {
		dma_unmap_sg_attrs(attachment->dev, table->sgl, table->nents,
				   direction, attrs);
		a->mapped = false;
	}
	/* Do nothing for iommu-dev.
	 * For ion_mtk_mm_heap, we will unmap all the iova
	 * in dma_buf_release
	 */

};

static void mtk_ion_dma_buf_release(struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_mm_buf_info *buf_info = buffer->priv_virt;
	int i;

	/* unmap iova */
	for (i = 0; i < MTK_M4U_DOM_NR_MAX; i++) {
		struct sg_table	*table = buf_info->table[i];
		struct dev_info dev_info = buf_info->dev_class[i];
		unsigned long attrs = dev_info.map_attrs | DMA_ATTR_SKIP_CPU_SYNC;

		if (!buf_info->mapped[i] || !iova_is_reserved(dev_info.dev))
			continue;

		dma_unmap_sg_attrs(dev_info.dev, table->sgl, table->nents,
				   dev_info.direction, attrs);
		buf_info->mapped[i] = false;
		sg_free_table(table);
		kfree(table);
	}
	ion_free(buffer);
};

static const struct dma_buf_ops mtk_dma_buf_ops = {
	.map_dma_buf = mtk_ion_map_dma_buf,
	.unmap_dma_buf = mtk_ion_unmap_dma_buf,
	.release = mtk_ion_dma_buf_release,
};

static struct ion_heap_ops mm_heap_ops = {
	.allocate = ion_mm_heap_allocate,
	.free = ion_mm_heap_free,
	.shrink = ion_mm_heap_shrink,
	.get_pool_size = ion_mm_get_pool_size,
};

static struct ion_mtk_mm_heap mm_heap = {
	.heap = {
		.ops = &mm_heap_ops,
		.type = ION_HEAP_TYPE_CUSTOM,
		.flags = ION_HEAP_FLAG_DEFER_FREE,
		.name = "mtk_ion_mm_heap",
	}
};

static int __init mtk_ion_mm_heap_init(void)
{
	int ret = ion_mm_heap_create_pools(mm_heap.pools);

	if (ret)
		return ret;

	ret = ion_device_add_heap(&mm_heap.heap);
	if (ret)
		return ret;

	memcpy(&mm_heap.heap.buf_ops,
	       &mtk_dma_buf_ops, sizeof(struct dma_buf_ops));

	return ret;
}

static void __exit mtk_ion_mm_heap_exit(void)
{
	ion_device_remove_heap(&mm_heap.heap);
	ion_mm_heap_destroy_pools(mm_heap.pools);
}

module_init(mtk_ion_mm_heap_init);
module_exit(mtk_ion_mm_heap_exit);
MODULE_LICENSE("GPL v2");
