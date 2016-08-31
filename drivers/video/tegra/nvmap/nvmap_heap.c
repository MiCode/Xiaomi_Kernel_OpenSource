/*
 * drivers/video/tegra/nvmap/nvmap_heap.c
 *
 * GPU heap allocator.
 *
 * Copyright (c) 2011-2013, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/bug.h>
#include <linux/stat.h>
#include <linux/debugfs.h>

#include <linux/nvmap.h>
#include "nvmap_priv.h"
#include "nvmap_heap.h"

#include <asm/tlbflush.h>
#include <asm/cacheflush.h>

#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>

#ifdef CONFIG_TRUSTED_LITTLE_KERNEL
#include <linux/ote_protocol.h>
#endif

/*
 * "carveouts" are platform-defined regions of physically contiguous memory
 * which are not managed by the OS. A platform may specify multiple carveouts,
 * for either small special-purpose memory regions (like IRAM on Tegra SoCs)
 * or reserved regions of main system memory.
 *
 * The carveout allocator returns allocations which are physically contiguous.
 */

struct list_block {
	struct nvmap_heap_block block;
	struct list_head all_list;
	unsigned int mem_prot;
	phys_addr_t orig_addr;
	size_t size;
	size_t align;
	struct nvmap_heap *heap;
	struct list_head free_list;
};

struct nvmap_heap {
	struct list_head all_list;
	struct list_head free_list;
	struct mutex lock;
	const char *name;
	void *arg;
	/* number of devices pointed by devs */
	unsigned int num_devs;
	/* indices for start and end device for resize support */
	unsigned int dev_start;
	unsigned int dev_end;
	/* devs to manage cma/coherent memory allocs, if resize allowed */
	struct device *devs;
	struct device dev;
	/* device to allocate memory from cma */
	struct device *cma_dev;
	/* flag that indicates whether heap can resize :shrink/grow */
	bool can_resize;
	/* lock to synchronise heap resizing */
	struct mutex resize_lock;
	/* CMA chunk size if resize supported */
	size_t cma_chunk_size;
	/* heap base */
	phys_addr_t base;
	/* heap size */
	size_t len;
	phys_addr_t cma_base;
	size_t cma_len;
	bool is_vpr;
};

static struct kmem_cache *heap_block_cache;

void nvmap_heap_debugfs_init(struct dentry *heap_root, struct nvmap_heap *heap)
{
	debugfs_create_x32("base", S_IRUGO,
		heap_root, (u32 *)&heap->base);
	debugfs_create_x32("size", S_IRUGO,
		heap_root, (u32 *)&heap->len);
	if (heap->can_resize) {
		debugfs_create_x32("cma_base", S_IRUGO,
			heap_root, (u32 *)&heap->cma_base);
		debugfs_create_x32("cma_size", S_IRUGO,
			heap_root, (u32 *)&heap->cma_len);
		debugfs_create_x32("cma_chunk_size", S_IRUGO,
			heap_root, (u32 *)&heap->cma_chunk_size);
		debugfs_create_x32("num_cma_chunks", S_IRUGO,
			heap_root, (u32 *)&heap->num_devs);
	}
}

static void nvmap_config_vpr(struct nvmap_heap *h)
{
#ifdef CONFIG_TRUSTED_LITTLE_KERNEL
	if (h->is_vpr)
		/* Config VPR_BOM/_SIZE in MC */
		te_set_vpr_params((void *)(uintptr_t)h->base, h->len);
	/* FIXME: trigger GPU to refetch VPR base and size. */
#endif
}

static phys_addr_t nvmap_alloc_from_contiguous(
				struct nvmap_heap *h,
				phys_addr_t base, size_t len)
{
	size_t count;
	struct page *page;
	unsigned long order;

	order = get_order(len);
	count = PAGE_ALIGN(len) >> PAGE_SHIFT;
	page = dma_alloc_from_contiguous(h->cma_dev, count, order);
	if (!page) {
		dev_err(h->cma_dev, "failed to alloc dma contiguous mem\n");
		goto dma_alloc_err;
	}
	base = page_to_phys(page);
	dev_dbg(h->cma_dev, "dma contiguous mem base (0x%pa) size (%zu)\n",
		&base, len);
	BUG_ON(base < h->cma_base ||
		base - h->cma_base + len > h->cma_len);
	return base;

dma_alloc_err:
	return DMA_ERROR_CODE;
}

static void nvmap_release_from_contiguous(
				struct nvmap_heap *h,
				phys_addr_t base, size_t len)
{
	struct page *page = phys_to_page(base);
	size_t count = PAGE_ALIGN(len) >> PAGE_SHIFT;

	dma_release_from_contiguous(h->cma_dev, page, count);
}

static int nvmap_declare_coherent(struct device *dev,
				  phys_addr_t base, size_t len)
{
	int err;

	BUG_ON(dev->dma_mem);
	dma_set_coherent_mask(dev,  DMA_BIT_MASK(64));
	err = dma_declare_coherent_memory(dev, 0, base, len,
			DMA_MEMORY_NOMAP | DMA_MEMORY_EXCLUSIVE);
	if (err & DMA_MEMORY_NOMAP) {
		dev_dbg(dev, "dma coherent mem base (0x%pa) size (%zu)\n",
			&base, len);
		return 0;
	}
	dev_err(dev, "failed to declare dma coherent_mem (0x%pa)\n",
		&base);
	return -ENOMEM;
}

static void nvmap_release_coherent(struct device *dev)
{
	dma_release_declared_memory(dev);
}

/* Call with resize_lock held
 * returns 0 on success.
 * returns 1 on failure.
 */
static int nvmap_heap_resize_locked(struct nvmap_heap *h)
{
	int idx;
	phys_addr_t base;
	bool at_bottom = false;
	DEFINE_DMA_ATTRS(attrs);

	dma_set_attr(DMA_ATTR_ALLOC_EXACT_SIZE, &attrs);
	base = nvmap_alloc_from_contiguous(h, 0, h->cma_chunk_size);
	if (dma_mapping_error(h->cma_dev, base))
		return 1;

	idx = div_u64(base - h->cma_base, h->cma_chunk_size);
	if (!h->len || base == h->base - h->cma_chunk_size)
		/* new chunk can be added at bottom. */
		at_bottom = true;
	else if (base != h->base + h->len)
		/* new chunk can't be added at top */
		goto fail_non_contig;

	BUG_ON(h->dev_start - 1 != idx && h->dev_end + 1 != idx && h->len);
	if (nvmap_declare_coherent(&h->devs[idx], base, h->cma_chunk_size))
		goto fail_declare;
	dev_dbg(&h->devs[idx],
		"Resize VPR base from=0x%pa to=0x%pa, len from=%zu to=%zu\n",
		&h->base, &base, h->len, h->len + h->cma_chunk_size);
	if (at_bottom) {
		h->base = base;
		h->dev_start = idx;
		if (!h->len)
			h->dev_end = h->dev_start;
	} else {
		h->dev_end = idx;
	}
	h->len += h->cma_chunk_size;
	nvmap_config_vpr(h);
	return 0;

fail_non_contig:
	dev_dbg(&h->devs[idx], "Found Non-Contiguous block(0x%pa)\n", &base);
fail_declare:
	nvmap_release_from_contiguous(h, base, h->cma_chunk_size);
	return 1;
}

static phys_addr_t nvmap_alloc_mem(struct nvmap_heap *h, size_t len)
{
	phys_addr_t pa;
	DEFINE_DMA_ATTRS(attrs);

	dma_set_attr(DMA_ATTR_ALLOC_EXACT_SIZE, &attrs);

	if (h->can_resize) {
		int idx = 0;
		struct device *dev;

		mutex_lock(&h->resize_lock);
retry_alloc:
		/* Try allocation from already existing CMA chunks */
		for (idx = h->dev_start; idx <= h->dev_end && h->len; idx++) {
			dev = &h->devs[idx];
			(void)dma_alloc_attrs(dev, len, &pa,
				DMA_MEMORY_NOMAP, &attrs);
			if (!dma_mapping_error(dev, pa)) {
				dev_dbg(dev,
					"Allocated addr (0x%pa) len(%zu)\n",
					&pa, len);
				mutex_unlock(&h->resize_lock);
				goto out;
			}
		}

		/* Check if a heap can be expanded */
		if (h->dev_end - h->dev_start + 1 < h->num_devs || !h->len) {
			if (!nvmap_heap_resize_locked(h))
				goto retry_alloc;
		}
		mutex_unlock(&h->resize_lock);
	} else if (h->cma_dev) {
		unsigned long order = get_order(len);
		int count = PAGE_ALIGN(len) >> PAGE_SHIFT;
		struct page *page = dma_alloc_from_contiguous(
					h->cma_dev, count, order);

		if (page) {
			pa = page_to_phys(page);
			dev_dbg(h->cma_dev,
			"dma contig mem addr (0x%pa) size (%zu)\n", &pa, len);
		} else {
			pa =  DMA_ERROR_CODE;
			dev_err(h->cma_dev,
			"failed to alloc dma contig mem size(%zu)\n", len);
		}
	} else {
		dev_dbg(&h->dev, "non-cma alloc addr (0x%pa) len(%zu)\n",
			&pa, len);
		/* handle non-CMA block allocation */
		(void)dma_alloc_attrs(&h->dev, len, &pa,
			DMA_MEMORY_NOMAP, &attrs);
	}
out:
	return pa;
}

static void nvmap_free_mem(struct nvmap_heap *h, phys_addr_t base,
				size_t len)
{
	int idx = 0;
	dma_addr_t dev_base;
	DEFINE_DMA_ATTRS(attrs);

	dma_set_attr(DMA_ATTR_ALLOC_EXACT_SIZE, &attrs);

	if (h->can_resize) {
		idx = div_u64(base - h->cma_base, h->cma_chunk_size);
		dev_dbg(&h->devs[idx], "dma free base (0x%pa) size (%zu)\n",
			&base, len);
		dma_free_attrs(&h->devs[idx], len, (void *)(uintptr_t)base,
			(dma_addr_t)base, &attrs);

		mutex_lock(&h->resize_lock);
check_next_chunk:
		/* Check if heap can be shrinked */
		if ((idx == h->dev_start || idx == h->dev_end) && h->len) {
			/* check if entire chunk is free */
			dma_alloc_attrs(&h->devs[idx], h->cma_chunk_size,
				&dev_base, DMA_MEMORY_NOMAP, &attrs);
			if (dma_mapping_error(&h->devs[idx], dev_base))
				goto out_unlock;
			dma_free_attrs(&h->devs[idx], h->cma_chunk_size,
				(void *)(uintptr_t)dev_base,
				(dma_addr_t)dev_base, &attrs);
			nvmap_release_coherent(&h->devs[idx]);
			BUG_ON(h->devs[idx].dma_mem != NULL);
			h->len -= h->cma_chunk_size;

			if ((idx == h->dev_start)) {
				h->base += h->cma_chunk_size;
				h->dev_start++;
				dev_dbg(&h->devs[idx],
					"Release Chunk at bottom\n");
				idx++;
			} else {
				h->dev_end--;
				dev_dbg(&h->devs[idx],
					"Release Chunk at top\n");
				idx--;
			}
			nvmap_config_vpr(h);
			nvmap_release_from_contiguous(h,
				dev_base, h->cma_chunk_size);
			goto check_next_chunk;
		}
out_unlock:
		mutex_unlock(&h->resize_lock);
	} else if (h->cma_dev) {
		dev_dbg(h->cma_dev, "dma free base (0x%pa) size (%zu)\n",
			&base, len);
		dma_release_from_contiguous(h->cma_dev, phys_to_page(base),
			PAGE_ALIGN(len) >> PAGE_SHIFT);
	} else {
		/* handle non-CMA block release */
		dev_dbg(&h->dev,
			"Non-Cma release base (0x%pa) len(%zu)\n",
			&base, len);
		dma_free_attrs(&h->dev, len,
			(void *)(uintptr_t)base, (dma_addr_t)base, &attrs);
	}
}

/*
 * base_max limits position of allocated chunk in memory.
 * if base_max is 0 then there is no such limitation.
 */
static struct nvmap_heap_block *do_heap_alloc(struct nvmap_heap *heap,
					      size_t len, size_t align,
					      unsigned int mem_prot,
					      phys_addr_t base_max)
{
	struct list_block *heap_block = NULL;
	dma_addr_t dev_base;
	struct device *dev = heap->can_resize ? &heap->devs[0] : &heap->dev;

	/* since pages are only mappable with one cache attribute,
	 * and most allocations from carveout heaps are DMA coherent
	 * (i.e., non-cacheable), round cacheable allocations up to
	 * a page boundary to ensure that the physical pages will
	 * only be mapped one way. */
	if (mem_prot == NVMAP_HANDLE_CACHEABLE ||
	    mem_prot == NVMAP_HANDLE_INNER_CACHEABLE) {
		align = max_t(size_t, align, PAGE_SIZE);
		len = PAGE_ALIGN(len);
	}

	heap_block = kmem_cache_zalloc(heap_block_cache, GFP_KERNEL);
	if (!heap_block) {
		dev_err(dev, "%s: failed to alloc heap block %s\n",
			__func__, dev_name(dev));
		goto fail_heap_block_alloc;
	}

	dev_base = nvmap_alloc_mem(heap, len);
	if (dma_mapping_error(dev, dev_base)) {
		dev_err(dev, "failed to alloc mem of size (%zu)\n",
			len);
		goto fail_dma_alloc;
	}

	heap_block->block.base = dev_base;
	heap_block->orig_addr = dev_base;
	heap_block->size = len;

	list_add_tail(&heap_block->all_list, &heap->all_list);
	heap_block->heap = heap;
	heap_block->mem_prot = mem_prot;
	heap_block->align = align;
	return &heap_block->block;

fail_dma_alloc:
	kmem_cache_free(heap_block_cache, heap_block);
fail_heap_block_alloc:
	return NULL;
}

#ifdef DEBUG_FREE_LIST
static void freelist_debug(struct nvmap_heap *heap, const char *title,
			   struct list_block *token)
{
	int i;
	struct list_block *n;
	struct device *dev = heap->can_resize ? &heap->devs[0] : &heap->dev;

	dev_debug(dev, "%s\n", title);
	i = 0;
	list_for_each_entry(n, &heap->free_list, free_list) {
		dev_debug(dev, "\t%d [%p..%p]%s\n",
				i, (void *)n->orig_addr,
			  (void *)(n->orig_addr + n->size),
			  (n == token) ? "<--" : "");
		i++;
	}
}
#else
#define freelist_debug(_heap, _title, _token)	do { } while (0)
#endif

static struct list_block *do_heap_free(struct nvmap_heap_block *block)
{
	struct list_block *b = container_of(block, struct list_block, block);
	struct nvmap_heap *heap = b->heap;

	list_del(&b->all_list);

	nvmap_free_mem(heap, block->base, b->size);
	kmem_cache_free(heap_block_cache, b);

	return b;
}

/* nvmap_heap_alloc: allocates a block of memory of len bytes, aligned to
 * align bytes. */
struct nvmap_heap_block *nvmap_heap_alloc(struct nvmap_heap *h,
					  struct nvmap_handle *handle)
{
	struct nvmap_heap_block *b;
	size_t len        = handle->size;
	size_t align      = handle->align;
	unsigned int prot = handle->flags;

	mutex_lock(&h->lock);

	align = max_t(size_t, align, L1_CACHE_BYTES);
	b = do_heap_alloc(h, len, align, prot, 0);

	if (b) {
		b->handle = handle;
		handle->carveout = b;
	}
	mutex_unlock(&h->lock);
	return b;
}

struct nvmap_heap *nvmap_block_to_heap(struct nvmap_heap_block *b)
{
	struct list_block *lb;
	lb = container_of(b, struct list_block, block);
	return lb->heap;
}

/* nvmap_heap_free: frees block b*/
void nvmap_heap_free(struct nvmap_heap_block *b)
{
	struct nvmap_heap *h = nvmap_block_to_heap(b);
	struct list_block *lb;

	mutex_lock(&h->lock);

	lb = container_of(b, struct list_block, block);
	nvmap_flush_heap_block(NULL, b, lb->size, lb->mem_prot);
	do_heap_free(b);

	mutex_unlock(&h->lock);
}

static struct device *nvmap_create_dma_devs(const char *name, int num_devs)
{
	int idx = 0;
	struct device *devs;

	devs = kzalloc(num_devs * sizeof(*devs), GFP_KERNEL);
	if (!devs)
		return NULL;
	for (idx = 0; idx < num_devs; idx++)
		dev_set_name(&devs[idx], "heap-%s-%d", name, idx);
	return devs;
}

static int nvmap_cma_heap_create(struct nvmap_heap *h,
	const struct nvmap_platform_carveout *co)
{
	struct dma_contiguous_stats stats;

	h->can_resize = co->resize;
	h->cma_dev = co->cma_dev;
	dma_get_contiguous_stats(h->cma_dev, &stats);
	dev_set_name(h->cma_dev, "cma-heap-%s", h->name);
	h->cma_base = stats.base;
	h->cma_len = stats.size;

	if (h->can_resize) {
		if (h->cma_len < co->cma_chunk_size) {
			dev_err(h->cma_dev, "error cma_len < cma_chunk_size");
			return -ENOMEM;
		}
		if (h->cma_len % co->cma_chunk_size) {
			dev_err(h->cma_dev,
				"size is not multiple of cma_chunk_size(%zu)\n"
				"size truncated from %zu to %zu\n",
				co->cma_chunk_size, h->cma_len,
				round_down(h->cma_len, co->cma_chunk_size));
			h->cma_len = round_down(h->cma_len, co->cma_chunk_size);
		}

		mutex_init(&h->resize_lock);
		h->cma_chunk_size = co->cma_chunk_size;
		h->num_devs = div_u64(h->cma_len, h->cma_chunk_size);

		h->devs = nvmap_create_dma_devs(h->name, h->num_devs);
		if (!h->devs) {
			pr_err("failed to alloc devices\n");
			return -ENOMEM;
		}
		/* FIXME: get this from board. */
		h->is_vpr = true;
	} else {
		/* allocation/free are performed using CMA directly. */
		dev_set_name(&h->dev, "heap-%s", h->name);
	}
	return 0;
}

/* nvmap_heap_create: create a heap object of len bytes, starting from
 * address base.
 */
struct nvmap_heap *nvmap_heap_create(struct device *parent,
				     const struct nvmap_platform_carveout *co,
				     phys_addr_t base, size_t len, void *arg)
{
	struct nvmap_heap *h;
	DEFINE_DMA_ATTRS(attrs);

	h = kzalloc(sizeof(*h), GFP_KERNEL);
	if (!h) {
		dev_err(parent, "%s: out of memory\n", __func__);
		return NULL;
	}

	h->name = co->name;
	h->arg = arg;

	if (co->cma_dev) {
		if (nvmap_cma_heap_create(h, co))
			goto fail_cma_heap_create;
		base = h->cma_base;
		len = h->cma_len;
	} else {
		/* declare Non-CMA heap */
		if (nvmap_declare_coherent(&h->dev, base, len))
			goto fail_dma_declare;
		h->base = base;
		h->len = len;
	}

	INIT_LIST_HEAD(&h->free_list);
	INIT_LIST_HEAD(&h->all_list);
	mutex_init(&h->lock);
	inner_flush_cache_all();
	outer_flush_range(base, base + len);
	wmb();

	dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
	dma_set_attr(DMA_ATTR_SKIP_IOVA_GAP, &attrs);
#ifdef CONFIG_PLATFORM_ENABLE_IOMMU
	dma_map_linear_attrs(parent->parent, base, len, DMA_TO_DEVICE,
				&attrs);
#endif
	dev_info(parent, "created heap %s base 0x%p size (%zuKiB)\n",
		co->name, (void *)(uintptr_t)base, len/1024);
	return h;

fail_cma_heap_create:
fail_dma_declare:
	kfree(h);
	return NULL;
}

void *nvmap_heap_to_arg(struct nvmap_heap *heap)
{
	return heap->arg;
}

/* nvmap_heap_destroy: frees all resources in heap */
void nvmap_heap_destroy(struct nvmap_heap *heap)
{
	WARN_ON(!list_is_singular(&heap->all_list));
	while (!list_empty(&heap->all_list)) {
		struct list_block *l;
		l = list_first_entry(&heap->all_list, struct list_block,
				     all_list);
		list_del(&l->all_list);
		kmem_cache_free(heap_block_cache, l);
	}
	kfree(heap);
}

int nvmap_heap_init(void)
{
	heap_block_cache = KMEM_CACHE(list_block, 0);
	if (!heap_block_cache) {
		pr_err("%s: unable to create heap block cache\n", __func__);
		return -ENOMEM;
	}
	pr_info("%s: created heap block cache\n", __func__);
	return 0;
}

void nvmap_heap_deinit(void)
{
	if (heap_block_cache)
		kmem_cache_destroy(heap_block_cache);

	heap_block_cache = NULL;
}
