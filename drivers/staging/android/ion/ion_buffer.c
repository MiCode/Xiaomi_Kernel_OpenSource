// SPDX-License-Identifier: GPL-2.0
/*
 * ION Memory Allocator - buffer interface
 *
 * Copyright (c) 2019, Google, Inc.
 */
#include <asm/cacheflush.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/dma-map-ops.h>

#define CREATE_TRACE_POINTS
#define COHERENT    1
#include "ion_trace.h"
#include "ion_private.h"

static atomic_long_t total_heap_bytes;
static atomic_long_t ion_heap_total_size;

static void track_buffer_created(struct ion_buffer *buffer)
{
	long total = atomic_long_add_return(buffer->size, &total_heap_bytes);

	trace_ion_stat(buffer->sg_table, buffer->size, total);
}

static void track_buffer_destroyed(struct ion_buffer *buffer)
{
	long total = atomic_long_sub_return(buffer->size, &total_heap_bytes);

	trace_ion_stat(buffer->sg_table, -buffer->size, total);
}

#ifdef CONFIG_E_SHOW_MEM
/* this function should only be called while dev->lock is held */
static void ion_buffer_add(struct ion_device *dev,
			   struct ion_buffer *buffer)
{
	struct rb_node **p = &dev->buffers.rb_node;
	struct rb_node *parent = NULL;
	struct ion_buffer *entry;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_buffer, node);

		if (buffer < entry) {
			p = &(*p)->rb_left;
		} else if (buffer > entry) {
			p = &(*p)->rb_right;
		} else {
			pr_err("%s: buffer already found.", __func__);
			BUG();
		}
	}

	rb_link_node(&buffer->node, parent, p);
	rb_insert_color(&buffer->node, &dev->buffers);
}
#endif

/* this function should only be called while dev->lock is held */
static struct ion_buffer *ion_buffer_create(struct ion_heap *heap,
					    struct ion_device *dev,
					    unsigned long len,
					    unsigned long flags)
{
	struct ion_buffer *buffer;
#ifdef CONFIG_E_SHOW_MEM
	struct timespec64 ts;
#endif
	int ret;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	buffer->heap = heap;
	buffer->flags = flags;
	buffer->size = len;

	ret = heap->ops->allocate(heap, buffer, len, flags);

	if (ret) {
		if (!(heap->flags & ION_HEAP_FLAG_DEFER_FREE))
			goto err2;

		ion_heap_freelist_drain(heap, 0);
		ret = heap->ops->allocate(heap, buffer, len, flags);
		if (ret)
			goto err2;
	}

	if (!buffer->sg_table) {
		WARN_ONCE(1, "This heap needs to set the sgtable");
		ret = -EINVAL;
		goto err1;
	}

	spin_lock(&heap->stat_lock);
	heap->num_of_buffers++;
	heap->num_of_alloc_bytes += len;
	if (heap->num_of_alloc_bytes > heap->alloc_bytes_wm)
		heap->alloc_bytes_wm = heap->num_of_alloc_bytes;
	if (heap->num_of_buffers == 1) {
		/* This module reference lasts as long as at least one
		 * buffer is allocated from the heap. We are protected
		 * against ion_device_remove_heap() with dev->lock, so we can
		 * safely assume the module reference is going to* succeed.
		 */
		__module_get(heap->owner);
	}
	spin_unlock(&heap->stat_lock);

	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);
#ifdef CONFIG_E_SHOW_MEM
	mutex_lock(&dev->buffer_lock);
	ion_buffer_add(dev, buffer);
	mutex_unlock(&dev->buffer_lock);
#endif
	track_buffer_created(buffer);

	if (buffer->heap->type == ION_HEAP_TYPE_SYSTEM)
		atomic_long_add(buffer->size, &ion_heap_total_size);

#ifdef CONFIG_E_SHOW_MEM
	buffer->pid = task_pid_nr(current->group_leader);
	get_task_comm(buffer->task_name, current->group_leader);
	ktime_get_real_ts64(&ts);
	ts.tv_sec -= sys_tz.tz_minuteswest * 60;
	buffer->alloc_ts = ts;
#endif

	return buffer;

err1:
	heap->ops->free(buffer);
err2:
	kfree(buffer);
	return ERR_PTR(ret);
}

static int ion_clear_pages(struct page **pages, int num, pgprot_t pgprot)
{
	void *addr = vmap(pages, num, VM_MAP, pgprot);

	if (!addr)
		return -ENOMEM;
	memset(addr, 0, PAGE_SIZE * num);
	vunmap(addr);

	return 0;
}

static int ion_sglist_zero(struct scatterlist *sgl, unsigned int nents,
			   pgprot_t pgprot)
{
	int p = 0;
	int ret = 0;
	struct sg_page_iter piter;
	struct page *pages[32];

	for_each_sg_page(sgl, &piter, nents, 0) {
		pages[p++] = sg_page_iter_page(&piter);
		if (p == ARRAY_SIZE(pages)) {
			ret = ion_clear_pages(pages, p, pgprot);
			if (ret)
				return ret;
			p = 0;
		}
	}
	if (p)
		ret = ion_clear_pages(pages, p, pgprot);

	return ret;
}

struct ion_buffer *ion_buffer_alloc(struct ion_device *dev, size_t len,
				    unsigned int heap_id_mask,
				    unsigned int flags)
{
	struct ion_buffer *buffer = NULL;
	struct ion_heap *heap;

	if (!dev || !len) {
		return ERR_PTR(-EINVAL);
	}

	/*
	 * traverse the list of heaps available in this system in priority
	 * order.  If the heap type is supported by the client, and matches the
	 * request of the caller allocate from it.  Repeat until allocate has
	 * succeeded or all heaps have been tried
	 */
	len = PAGE_ALIGN(len);
	if (!len)
		return ERR_PTR(-EINVAL);

	down_read(&dev->lock);
	plist_for_each_entry(heap, &dev->heaps, node) {
		/* if the caller didn't specify this heap id */
		if (!((1 << heap->id) & heap_id_mask))
			continue;
		buffer = ion_buffer_create(heap, dev, len, flags);
		if (!IS_ERR(buffer))
			break;
	}
	up_read(&dev->lock);

	if (!buffer)
		return ERR_PTR(-ENODEV);

	if (IS_ERR(buffer))
		return ERR_CAST(buffer);

	return buffer;
}

int ion_buffer_zero(struct ion_buffer *buffer)
{
	struct sg_table *table;
	pgprot_t pgprot;

	if (!buffer)
		return -EINVAL;

	table = buffer->sg_table;
	if (buffer->flags & ION_FLAG_CACHED)
		pgprot = PAGE_KERNEL;
	else
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	return ion_sglist_zero(table->sgl, table->orig_nents, pgprot);
}
EXPORT_SYMBOL_GPL(ion_buffer_zero);

#ifdef CONFIG_ARM
static void ion_flush_buffer(struct page *page, size_t size, int coherent_flag)
{
	if (PageHighMem(page)) {
		phys_addr_t base = __pfn_to_phys(page_to_pfn(page));
		phys_addr_t end = base + size;

		while (size > 0) {
			void *ptr = kmap_atomic(page);

			if (coherent_flag != COHERENT)
				dmac_flush_range(ptr, ptr + PAGE_SIZE);
			kunmap_atomic(ptr);
			page++;
			size -= PAGE_SIZE;
		}
		if (coherent_flag != COHERENT)
			outer_flush_range(base, end);
	} else {
		void *ptr = page_address(page);

		if (coherent_flag != COHERENT) {
			dmac_flush_range(ptr, ptr + size);
			outer_flush_range(__pa(ptr), __pa(ptr) + size);
		}
	}
}
#endif

void ion_buffer_prep_noncached(struct ion_buffer *buffer)
{
	struct scatterlist *sg;
	struct sg_table *table;
	int i;

	if (WARN_ONCE(!buffer || !buffer->sg_table,
		      "%s needs a buffer and a sg_table", __func__) ||
	    buffer->flags & ION_FLAG_CACHED)
		return;

	table = buffer->sg_table;

	for_each_sg(table->sgl, sg, table->orig_nents, i) {
		arch_dma_prep_coherent(sg_page(sg), sg->length);
	/*
	 * For uncached buffers, we need to initially flush cpu cache, since
	 * the __GFP_ZERO on the allocation means the zeroing was done by the
	 * cpu and thus it is likely cached. Map (and implicitly flush) and
	 * unmap it now so we don't get corruption later on.arch_dma_prep_coherent()
	 * is missing for arm32 so we flush cache in our own way.
	 */
#ifdef CONFIG_ARM
	 ion_flush_buffer(sg_page(sg), sg->length, 0);
#endif
	}
}
EXPORT_SYMBOL_GPL(ion_buffer_prep_noncached);

void ion_buffer_release(struct ion_buffer *buffer)
{
	if (buffer->kmap_cnt > 0) {
		pr_warn_once("%s: buffer still mapped in the kernel\n",
			     __func__);
		ion_heap_unmap_kernel(buffer->heap, buffer);
	}
	buffer->heap->ops->free(buffer);
	//trace_android_vh_ion_buffer_release(buffer);
	spin_lock(&buffer->heap->stat_lock);
	buffer->heap->num_of_buffers--;
	buffer->heap->num_of_alloc_bytes -= buffer->size;
	if (buffer->heap->num_of_buffers == 0)
		module_put(buffer->heap->owner);
	spin_unlock(&buffer->heap->stat_lock);
	/* drop reference to the heap module */

	kfree(buffer);
}

int ion_buffer_destroy(struct ion_device *dev, struct ion_buffer *buffer)
{
	struct ion_heap *heap;

	if (!dev || !buffer) {
		pr_warn("%s: invalid argument\n", __func__);
		return -EINVAL;
	}

	heap = buffer->heap;
	track_buffer_destroyed(buffer);

#ifdef CONFIG_E_SHOW_MEM
	mutex_lock(&dev->buffer_lock);
	rb_erase(&buffer->node, &dev->buffers);
	mutex_unlock(&dev->buffer_lock);
#endif

	if (buffer->heap->type == ION_HEAP_TYPE_SYSTEM)
		atomic_long_sub(buffer->size, &ion_heap_total_size);

	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		ion_heap_freelist_add(heap, buffer);
	else
		ion_buffer_release(buffer);

	return 0;
}

void *ion_buffer_kmap_get(struct ion_buffer *buffer)
{
	void *vaddr;

	if (buffer->kmap_cnt) {
		buffer->kmap_cnt++;
		return buffer->vaddr;
	}
	vaddr = ion_heap_map_kernel(buffer->heap, buffer);
	if (WARN_ONCE(!vaddr,
		      "heap->ops->map_kernel should return ERR_PTR on error"))
		return ERR_PTR(-EINVAL);
	if (IS_ERR(vaddr))
		return vaddr;
	buffer->vaddr = vaddr;
	buffer->kmap_cnt++;
	return vaddr;
}
EXPORT_SYMBOL_GPL(ion_buffer_kmap_get);

void ion_buffer_kmap_put(struct ion_buffer *buffer)
{
	buffer->kmap_cnt--;
	if (!buffer->kmap_cnt) {
		ion_heap_unmap_kernel(buffer->heap, buffer);
		buffer->vaddr = NULL;
	}
}
EXPORT_SYMBOL_GPL(ion_buffer_kmap_put);

u64 ion_get_total_heap_bytes(void)
{
	return atomic_long_read(&total_heap_bytes);
}

long get_ion_heap_total_pages(void)
{
	return atomic_long_read(&ion_heap_total_size) >> PAGE_SHIFT;
}
