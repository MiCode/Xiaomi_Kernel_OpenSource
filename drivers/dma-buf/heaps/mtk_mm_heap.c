// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF mtk_mm heap exporter
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2019, 2020 Linaro Ltd.
 * Copyright (C) 2021 MediaTek Inc.
 *
 * Portions based off of Andrew Davis' SRAM heap:
 * Copyright (C) 2019 Texas Instruments Incorporated - http://www.ti.com/
 *	Andrew F. Davis <afd@ti.com>
 */

#define pr_fmt(fmt) "dma_heap: mtk_mm "fmt

#include <linux/module.h>

#ifdef __MTK_MM_HEAP_FILE_ENABLE__
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "page_pool.h"
#include "deferred-free-helper.h"

#include <linux/iommu.h>
#include <dt-bindings/memory/mtk-memory-port.h>
#include "mtk_heap_priv.h"
#include "mtk_heap.h"

static struct dma_heap *mtk_mm_heap;
static struct dma_heap *mtk_mm_uncached_heap;

struct mtk_mm_heap_buffer {
	struct dma_heap *heap;
	struct list_head attachments;
	struct mutex lock;
	unsigned long len;
	struct sg_table sg_table;
	int vmap_cnt;
	void *vaddr;
	struct deferred_freelist_item deferred_free;

	bool uncached;

	void *priv;
	bool                    mapped[BUF_PRIV_MAX_CNT];
	struct mtk_heap_dev_info dev_info[BUF_PRIV_MAX_CNT];
	struct sg_table         *mapped_table[BUF_PRIV_MAX_CNT];
	struct mutex            map_lock; /* map iova lock */
	pid_t                   pid;
	pid_t                   tid;
	char                    pid_name[TASK_COMM_LEN];
	char                    tid_name[TASK_COMM_LEN];
};

#define HIGH_ORDER_GFP  (((GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN \
				| __GFP_NORETRY) & ~__GFP_RECLAIM) \
				| __GFP_COMP)
#define LOW_ORDER_GFP (GFP_HIGHUSER | __GFP_ZERO | __GFP_COMP)
static gfp_t order_flags[] = {HIGH_ORDER_GFP, LOW_ORDER_GFP, LOW_ORDER_GFP};
/*
 * The selection of the orders used for allocation (1MB, 64K, 4K) is designed
 * to match with the sizes often found in IOMMUs. Using order 4 pages instead
 * of order 0 pages can significantly improve the performance of many IOMMUs
 * by reducing TLB pressure and time spent updating page tables.
 */
static const unsigned int orders[] = {8, 4, 0};
#define NUM_ORDERS ARRAY_SIZE(orders)
struct dmabuf_page_pool *pools[NUM_ORDERS];

static struct sg_table *dup_sg_table(struct sg_table *table)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg, *new_sg;

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, table->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	new_sg = new_table->sgl;
	for_each_sgtable_sg(table, sg, i) {
		sg_set_page(new_sg, sg_page(sg), sg->length, sg->offset);
		new_sg = sg_next(new_sg);
	}

	return new_table;
}

/* source copy to dest */
static int copy_sg_table(struct sg_table *source, struct sg_table *dest)
{
	int i;
	struct scatterlist *sgl, *dest_sgl;

	if (source->orig_nents != dest->orig_nents) {
		pr_info("nents not match %d-%d\n",
			source->orig_nents, dest->orig_nents);

		return -EINVAL;
	}

	/* copy mapped nents */
	dest->nents = source->nents;

	dest_sgl = dest->sgl;
	for_each_sg(source->sgl, sgl, source->orig_nents, i) {
		memcpy(dest_sgl, sgl, sizeof(*sgl));
		dest_sgl = sg_next(dest_sgl);
	}

	return 0;
};

/*
 * must check domain info before call fill_buffer_info
 * @Return 0: pass
 */
static int fill_buffer_info(struct mtk_mm_heap_buffer *buffer,
                           struct sg_table *table,
                           struct dma_buf_attachment *a,
                           enum dma_data_direction dir,
                           int iommu_dom_id) {
	struct sg_table *new_table = NULL;
	int ret = 0;

	/*
	 * devices without iommus attribute,
	 * use common flow, skip set buf_info
	 */
	if (iommu_dom_id >= BUF_PRIV_MAX_CNT)
		return 0;

	if (buffer->mapped[iommu_dom_id]) {
		pr_err("%s err: already mapped, no need fill again\n", __func__);
		return -EINVAL;
	}

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return -ENOMEM;

	ret = sg_alloc_table(new_table, table->orig_nents, GFP_KERNEL);
	if (ret) {
		pr_err("%s err: sg_alloc_table failed\n", __func__);
		kfree(new_table);
		return -ENOMEM;
	}

	ret = copy_sg_table(table, new_table);
	if (ret)
		return ret;

	buffer->mapped_table[iommu_dom_id] = new_table;
	buffer->mapped[iommu_dom_id] = true;
	buffer->dev_info[iommu_dom_id].dev = a->dev;
	buffer->dev_info[iommu_dom_id].direction = dir;
	/* TODO: check map_attrs affect??? */
	buffer->dev_info[iommu_dom_id].map_attrs = a->dma_map_attrs;

	return 0;
}


static int mtk_mm_heap_attach(struct dma_buf *dmabuf,
			      struct dma_buf_attachment *attachment)
{
	struct mtk_mm_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;
	struct sg_table *table;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	table = dup_sg_table(&buffer->sg_table);
	if (IS_ERR(table)) {
		kfree(a);
		return -ENOMEM;
	}

	a->table = table;
	a->dev = attachment->dev;
	INIT_LIST_HEAD(&a->list);
	a->mapped = false;
	a->uncached = buffer->uncached;
	attachment->priv = a;

	mutex_lock(&buffer->lock);
	list_add(&a->list, &buffer->attachments);
	mutex_unlock(&buffer->lock);

	return 0;
}

static void mtk_mm_heap_detach(struct dma_buf *dmabuf,
			       struct dma_buf_attachment *attachment)
{
	struct mtk_mm_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a = attachment->priv;

	mutex_lock(&buffer->lock);
	list_del(&a->list);
	mutex_unlock(&buffer->lock);

	sg_free_table(a->table);
	kfree(a->table);
	kfree(a);
}

static struct sg_table *mtk_mm_heap_map_dma_buf(struct dma_buf_attachment *attachment,
						enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	struct sg_table *table = a->table;
	int attr = attachment->dma_map_attrs;
	int ret;

	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(attachment->dev);
	int dom_id = BUF_PRIV_MAX_CNT;
	struct mtk_mm_heap_buffer *buffer = attachment->dmabuf->priv;

	if (a->uncached)
		attr |= DMA_ATTR_SKIP_CPU_SYNC;

	mutex_lock(&buffer->map_lock);

	if (fwspec)
		dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);

	/* device with iommus attribute AND mapped before */
	if (fwspec && buffer->mapped[dom_id]) {
		/* mapped before, return saved table */
		ret = copy_sg_table(buffer->mapped_table[dom_id], table);
		mutex_unlock(&buffer->map_lock);
		if (ret)
			return ERR_PTR(-EINVAL);

		a->mapped = true;
		return table;
	}

	/* first map OR device without iommus attribute */
	if (dma_map_sgtable(attachment->dev, table, direction, attr)) {
		pr_info("%s map fail dom:%d, dev:%s\n",
			__func__, dom_id, dev_name(attachment->dev));
		mutex_unlock(&buffer->map_lock);
		return ERR_PTR(-ENOMEM);
	}

	ret = fill_buffer_info(buffer, table,
			       attachment, direction, dom_id);
	if (ret) {
		mutex_unlock(&buffer->map_lock);
		return ERR_PTR(-ENOMEM);
	}

	a->mapped = true;
	mutex_unlock(&buffer->map_lock);

	return table;
}

static void mtk_mm_heap_unmap_dma_buf(struct dma_buf_attachment *attachment,
				      struct sg_table *table,
				      enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	int attr = attachment->dma_map_attrs;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(attachment->dev);

	if (a->uncached)
		attr |= DMA_ATTR_SKIP_CPU_SYNC;

	if (!fwspec)
		dma_unmap_sgtable(attachment->dev, table, direction, attr);

	a->mapped = false;
	/*
	 * for devices with iommus attribute,
	 * unmap iova when release dma-buf
	 */
}

static int mtk_mm_heap_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
						enum dma_data_direction direction)
{
	struct mtk_mm_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;

	mutex_lock(&buffer->lock);

	if (buffer->vmap_cnt)
		invalidate_kernel_vmap_range(buffer->vaddr, buffer->len);

	if (!buffer->uncached) {
		list_for_each_entry(a, &buffer->attachments, list) {
			if (!a->mapped)
				continue;
			dma_sync_sgtable_for_cpu(a->dev, a->table, direction);
		}
	}
	mutex_unlock(&buffer->lock);

	return 0;
}

static int mtk_mm_heap_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					      enum dma_data_direction direction)
{
	struct mtk_mm_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;

	mutex_lock(&buffer->lock);

	if (buffer->vmap_cnt)
		flush_kernel_vmap_range(buffer->vaddr, buffer->len);

	if (!buffer->uncached) {
		list_for_each_entry(a, &buffer->attachments, list) {
			if (!a->mapped)
				continue;
			dma_sync_sgtable_for_device(a->dev, a->table, direction);
		}
	}
	mutex_unlock(&buffer->lock);

	return 0;
}

static int mtk_mm_heap_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct mtk_mm_heap_buffer *buffer = dmabuf->priv;
	struct sg_table *table = &buffer->sg_table;
	unsigned long addr = vma->vm_start;
	struct sg_page_iter piter;
	int ret;

	if (buffer->uncached)
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	for_each_sgtable_page(table, &piter, vma->vm_pgoff) {
		struct page *page = sg_page_iter_page(&piter);

		ret = remap_pfn_range(vma, addr, page_to_pfn(page), PAGE_SIZE,
				      vma->vm_page_prot);
		if (ret)
			return ret;
		addr += PAGE_SIZE;
		if (addr >= vma->vm_end)
			return 0;
	}
	return 0;
}

static void *mtk_mm_heap_do_vmap(struct mtk_mm_heap_buffer *buffer)
{
	struct sg_table *table = &buffer->sg_table;
	int npages = PAGE_ALIGN(buffer->len) / PAGE_SIZE;
	struct page **pages = vmalloc(sizeof(struct page *) * npages);
	struct page **tmp = pages;
	struct sg_page_iter piter;
	pgprot_t pgprot = PAGE_KERNEL;
	void *vaddr;

	if (!pages)
		return ERR_PTR(-ENOMEM);

	if (buffer->uncached)
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	for_each_sgtable_page(table, &piter, 0) {
		WARN_ON(tmp - pages >= npages);
		*tmp++ = sg_page_iter_page(&piter);
	}

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	vfree(pages);

	if (!vaddr)
		return ERR_PTR(-ENOMEM);

	return vaddr;
}

static void *mtk_mm_heap_vmap(struct dma_buf *dmabuf)
{
	struct mtk_mm_heap_buffer *buffer = dmabuf->priv;
	void *vaddr;

	mutex_lock(&buffer->lock);
	if (buffer->vmap_cnt) {
		buffer->vmap_cnt++;
		vaddr = buffer->vaddr;
		goto out;
	}

	vaddr = mtk_mm_heap_do_vmap(buffer);
	if (IS_ERR(vaddr))
		goto out;

	buffer->vaddr = vaddr;
	buffer->vmap_cnt++;
out:
	mutex_unlock(&buffer->lock);

	return vaddr;
}

static void mtk_mm_heap_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
	struct mtk_mm_heap_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->lock);
	if (!--buffer->vmap_cnt) {
		vunmap(buffer->vaddr);
		buffer->vaddr = NULL;
	}
	mutex_unlock(&buffer->lock);
}

static int mtk_mm_heap_zero_buffer(struct mtk_mm_heap_buffer *buffer)
{
	struct sg_table *sgt = &buffer->sg_table;
	struct sg_page_iter piter;
	struct page *p;
	void *vaddr;
	int ret = 0;

	for_each_sgtable_page(sgt, &piter, 0) {
		p = sg_page_iter_page(&piter);
		vaddr = kmap_atomic(p);
		memset(vaddr, 0, PAGE_SIZE);
		kunmap_atomic(vaddr);
	}

	return ret;
}

static void mtk_mm_heap_buf_free(struct deferred_freelist_item *item,
				 enum df_reason reason)
{
	struct mtk_mm_heap_buffer *buffer;
	struct sg_table *table;
	struct scatterlist *sg;
	int i, j;

	buffer = container_of(item, struct mtk_mm_heap_buffer, deferred_free);
	/* Zero the buffer pages before adding back to the pool */
	if (reason == DF_NORMAL)
		if (mtk_mm_heap_zero_buffer(buffer))
			reason = DF_UNDER_PRESSURE; // On failure, just free

	table = &buffer->sg_table;
	for_each_sg(table->sgl, sg, table->nents, i) {
		struct page *page = sg_page(sg);

		if (reason == DF_UNDER_PRESSURE) {
			__free_pages(page, compound_order(page));
		} else {
			for (j = 0; j < NUM_ORDERS; j++) {
				if (compound_order(page) == orders[j])
					break;
			}
			dmabuf_page_pool_free(pools[j], page);
		}
	}
	sg_free_table(table);
	kfree(buffer);
}

static void mtk_mm_heap_dma_buf_release(struct dma_buf *dmabuf)
{
	struct mtk_mm_heap_buffer *buffer = dmabuf->priv;
	int npages = PAGE_ALIGN(buffer->len) / PAGE_SIZE;
	int i;

	pr_debug("%s: addr:%p, size:%lu, name:%s\n",
		 __func__, dmabuf, buffer->len, dmabuf->name);
	/* unmap all domains' iova */
	for (i = 0; i < BUF_PRIV_MAX_CNT; i++) {
		struct sg_table *table = buffer->mapped_table[i];
		struct mtk_heap_dev_info dev_info = buffer->dev_info[i];
		unsigned long attrs = dev_info.map_attrs;

		if(buffer->uncached)
			attrs |= DMA_ATTR_SKIP_CPU_SYNC;

		if (!buffer->mapped[i])
			continue;

		dma_unmap_sgtable(dev_info.dev, table, dev_info.direction, attrs);
		buffer->mapped[i] = false;
		sg_free_table(table);
		kfree(table);
	}

	/* free buffer memory */
	deferred_free(&buffer->deferred_free, mtk_mm_heap_buf_free, npages);
}

static int mtk_mm_heap_dma_buf_get_flags(struct dma_buf *dmabuf, unsigned long *flags)
{
	struct mtk_mm_heap_buffer *buffer = dmabuf->priv;

	*flags = buffer->uncached;

	return 0;
}

static const struct dma_buf_ops mtk_mm_heap_buf_ops = {
	/* one attachment can only map once */
	.cache_sgt_mapping = 1,
	.attach = mtk_mm_heap_attach,
	.detach = mtk_mm_heap_detach,
	.map_dma_buf = mtk_mm_heap_map_dma_buf,
	.unmap_dma_buf = mtk_mm_heap_unmap_dma_buf,
	.begin_cpu_access = mtk_mm_heap_dma_buf_begin_cpu_access,
	.end_cpu_access = mtk_mm_heap_dma_buf_end_cpu_access,
	.mmap = mtk_mm_heap_mmap,
	.vmap = mtk_mm_heap_vmap,
	.vunmap = mtk_mm_heap_vunmap,
	.release = mtk_mm_heap_dma_buf_release,
	.get_flags = mtk_mm_heap_dma_buf_get_flags,
};

static struct page *alloc_largest_available(unsigned long size,
					    unsigned int max_order)
{
	struct page *page;
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		if (size <  (PAGE_SIZE << orders[i]))
			continue;
		if (max_order < orders[i])
			continue;
		page = dmabuf_page_pool_alloc(pools[i]);
		if (!page)
			continue;
		return page;
	}
	return NULL;
}

static struct dma_buf *mtk_mm_heap_do_allocate(struct dma_heap *heap,
					       unsigned long len,
					       unsigned long fd_flags,
					       unsigned long heap_flags,
					       bool uncached)
{
	struct mtk_mm_heap_buffer *buffer;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	unsigned long size_remaining = len;
	unsigned int max_order = orders[0];
	struct dma_buf *dmabuf;
	struct sg_table *table;
	struct scatterlist *sg;
	struct list_head pages;
	struct page *page, *tmp_page;
	int i, ret = -ENOMEM;
	struct task_struct *task = current->group_leader;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);
	buffer->heap = heap;
	buffer->len = len;
	buffer->uncached = uncached;

	INIT_LIST_HEAD(&pages);
	i = 0;
	while (size_remaining > 0) {
		/*
		 * Avoid trying to allocate memory if the process
		 * has been killed by SIGKILL
		 */
		if (fatal_signal_pending(current))
			goto free_buffer;

		page = alloc_largest_available(size_remaining, max_order);
		if (!page)
			goto free_buffer;

		list_add_tail(&page->lru, &pages);
		size_remaining -= page_size(page);
		max_order = compound_order(page);
		i++;
	}

	table = &buffer->sg_table;
	if (sg_alloc_table(table, i, GFP_KERNEL))
		goto free_buffer;

	sg = table->sgl;
	list_for_each_entry_safe(page, tmp_page, &pages, lru) {
		sg_set_page(sg, page, page_size(page), 0);
		sg = sg_next(sg);
		list_del(&page->lru);
	}

	mutex_init(&buffer->map_lock);
	/* add alloc pid & tid info */
	get_task_comm(buffer->pid_name, task);
	get_task_comm(buffer->tid_name, current);
	buffer->pid = task_pid_nr(task);
	buffer->tid = task_pid_nr(current);

	/* create the dmabuf */
	exp_info.exp_name = dma_heap_get_name(heap);
	exp_info.ops = &mtk_mm_heap_buf_ops;
	exp_info.size = buffer->len;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer;
	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto free_pages;
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

	return dmabuf;

free_pages:
	for_each_sgtable_sg(table, sg, i) {
		struct page *p = sg_page(sg);

		__free_pages(p, compound_order(p));
	}
	sg_free_table(table);
free_buffer:
	list_for_each_entry_safe(page, tmp_page, &pages, lru)
		__free_pages(page, compound_order(page));
	kfree(buffer);

	return ERR_PTR(ret);
}

static struct dma_buf *mtk_mm_heap_allocate(struct dma_heap *heap,
					    unsigned long len,
					    unsigned long fd_flags,
					    unsigned long heap_flags)
{
	return mtk_mm_heap_do_allocate(heap, len, fd_flags, heap_flags, false);
}

static long mtk_mm_heap_get_pool_size(struct dma_heap *heap) {
	int i, j;
	long page_cnt = 0;
	struct dmabuf_page_pool *pool;
	int count = 0;

	/*
	 * "mtk_mm" & "mtk_mm-uncached" use same page pool
	 * here we return 0 for uncached pool
	 */
	if (heap != mtk_mm_heap)
		return 0;

	for (i = 0; i < NUM_ORDERS; i++) {
		pool = pools[i];
		count = 0;

		mutex_lock(&pool->mutex);
		for (j = 0; j < POOL_TYPE_SIZE; j++) {
			count += pool->count[j];
		}
		page_cnt += (count << pool->order);
		mutex_unlock(&pool->mutex);
	}

	return page_cnt * PAGE_SIZE;
}


static const struct dma_heap_ops mtk_mm_heap_ops = {
	.allocate = mtk_mm_heap_allocate,
	.get_pool_size = mtk_mm_heap_get_pool_size,
};

static struct dma_buf *mtk_mm_uncached_heap_allocate(struct dma_heap *heap,
						     unsigned long len,
						     unsigned long fd_flags,
						     unsigned long heap_flags)
{
	return mtk_mm_heap_do_allocate(heap, len, fd_flags, heap_flags, true);
}

/* Dummy function to be used until we can call coerce_mask_and_coherent */
static struct dma_buf *mtk_mm_uncached_heap_not_initialized(struct dma_heap *heap,
							    unsigned long len,
							    unsigned long fd_flags,
							    unsigned long heap_flags)
{
	return ERR_PTR(-EBUSY);
}

static struct dma_heap_ops mtk_mm_uncached_heap_ops = {
	/* After mtk_mm_heap_create is complete, we will swap this */
	.allocate = mtk_mm_uncached_heap_not_initialized,
};

/**
 * return none-zero value means dump fail.
 *       maybe the input dmabuf isn't this heap buffer, no need dump
 *
 * return 0 means dump pass
 */
static int mm_heap_buf_priv_dump(const struct dma_buf *dmabuf,
				 struct dma_heap *heap,
				 void *priv)
{
	struct mtk_mm_heap_buffer *buf = dmabuf->priv;
	struct seq_file *s = priv;
	int i = 0;

	/* buffer check */
	if (!is_mtk_mm_heap_dmabuf(dmabuf))
		return -EINVAL;

	if (heap != buf->heap)
		return -EINVAL;

	dmabuf_dump(s, "\t\tbuf_priv: uncache:%d alloc-pid:%d[%s]-tid:%d[%s]\n",
		    !!buf->uncached,
		    buf->pid, buf->pid_name,
		    buf->tid, buf->tid_name);

	for (i = 0; i < BUF_PRIV_MAX_CNT; i++) {
		bool mapped = buf->mapped[i];
		struct device *dev = buf->dev_info[i].dev;
		struct sg_table *sgt = buf->mapped_table[i];

		if (!sgt || !dev || !dev_iommu_fwspec_get(dev))
			continue;

		dmabuf_dump(s,
			    "\t\tbuf_priv: dom:%-2d map:%d iova:0x%-12lx attr:0x%-4lx dir:%-2d dev:%s\n",
			    i, mapped,
			    sg_dma_address(sgt->sgl),
			    buf->dev_info[i].map_attrs,
			    buf->dev_info[i].direction,
			    dev_name(dev));
	}

	return 0;
}

static struct mtk_heap_priv_info mtk_mm_heap_priv = {
	.buf_priv_dump = mm_heap_buf_priv_dump,
};

static int mtk_mm_heap_create(void)
{
	struct dma_heap_export_info exp_info;
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		pools[i] = dmabuf_page_pool_create(order_flags[i], orders[i]);

		if (!pools[i]) {
			int j;

			pr_err("%s: page pool creation failed!\n", __func__);
			for (j = 0; j < i; j++)
				dmabuf_page_pool_destroy(pools[j]);
			return -ENOMEM;
		}
	}

	exp_info.name = "mtk_mm";
	exp_info.ops = &mtk_mm_heap_ops;
	exp_info.priv = (void *)&mtk_mm_heap_priv;

	mtk_mm_heap = dma_heap_add(&exp_info);
	if (IS_ERR(mtk_mm_heap))
		return PTR_ERR(mtk_mm_heap);
	pr_info("%s add heap[%s] success\n", __func__, exp_info.name);

	exp_info.name = "mtk_mm-uncached";
	exp_info.ops = &mtk_mm_uncached_heap_ops;
	exp_info.priv = (void *)&mtk_mm_heap_priv;

	mtk_mm_uncached_heap = dma_heap_add(&exp_info);
	if (IS_ERR(mtk_mm_uncached_heap))
		return PTR_ERR(mtk_mm_uncached_heap);

	dma_coerce_mask_and_coherent(dma_heap_get_dev(mtk_mm_uncached_heap), DMA_BIT_MASK(64));
	mb(); /* make sure we only set allocate after dma_mask is set */
	mtk_mm_uncached_heap_ops.allocate = mtk_mm_uncached_heap_allocate;
	pr_info("%s add heap[%s] success\n", __func__, exp_info.name);

	return 0;
}

int is_mtk_mm_heap_dmabuf(const struct dma_buf *dmabuf)
{
	if (dmabuf && dmabuf->ops == &mtk_mm_heap_buf_ops)
		return 1;
	return 0;
}
EXPORT_SYMBOL_GPL(is_mtk_mm_heap_dmabuf);
#else
static int mtk_mm_heap_create(void)
{
	pr_info("%s init!\n", __func__);

	return 0;
}
#endif

module_init(mtk_mm_heap_create);
MODULE_LICENSE("GPL v2");
