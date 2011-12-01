/*
 * drivers/gpu/ion/ion_carveout_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/spinlock.h>

#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/ion.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "ion_priv.h"

#include <asm/mach/map.h>

struct ion_carveout_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	ion_phys_addr_t base;
	unsigned long allocated_bytes;
	unsigned long total_size;
	void (*request_region)(void *);
	void (*release_region)(void *);
	atomic_t map_count;
	void *bus_id;
};

ion_phys_addr_t ion_carveout_allocate(struct ion_heap *heap,
				      unsigned long size,
				      unsigned long align)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);
	unsigned long offset = gen_pool_alloc_aligned(carveout_heap->pool,
							size, ilog2(align));

	if (!offset) {
		if ((carveout_heap->total_size -
		      carveout_heap->allocated_bytes) > size)
			pr_debug("%s: heap %s has enough memory (%lx) but"
				" the allocation of size %lx still failed."
				" Memory is probably fragmented.",
				__func__, heap->name,
				carveout_heap->total_size -
				carveout_heap->allocated_bytes, size);
		return ION_CARVEOUT_ALLOCATE_FAIL;
	}

	carveout_heap->allocated_bytes += size;
	return offset;
}

void ion_carveout_free(struct ion_heap *heap, ion_phys_addr_t addr,
		       unsigned long size)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);

	if (addr == ION_CARVEOUT_ALLOCATE_FAIL)
		return;
	gen_pool_free(carveout_heap->pool, addr, size);
	carveout_heap->allocated_bytes -= size;
}

static int ion_carveout_heap_phys(struct ion_heap *heap,
				  struct ion_buffer *buffer,
				  ion_phys_addr_t *addr, size_t *len)
{
	*addr = buffer->priv_phys;
	*len = buffer->size;
	return 0;
}

static int ion_carveout_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size, unsigned long align,
				      unsigned long flags)
{
	buffer->priv_phys = ion_carveout_allocate(heap, size, align);
	return buffer->priv_phys == ION_CARVEOUT_ALLOCATE_FAIL ? -ENOMEM : 0;
}

static void ion_carveout_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;

	ion_carveout_free(heap, buffer->priv_phys, buffer->size);
	buffer->priv_phys = ION_CARVEOUT_ALLOCATE_FAIL;
}

struct scatterlist *ion_carveout_heap_map_dma(struct ion_heap *heap,
					      struct ion_buffer *buffer)
{
	struct scatterlist *sglist;
	struct page *page = phys_to_page(buffer->priv_phys);

	if (page == NULL)
		return NULL;

	sglist = vmalloc(sizeof(struct scatterlist));
	if (!sglist)
		return ERR_PTR(-ENOMEM);

	sg_init_table(sglist, 1);
	sg_set_page(sglist, page, buffer->size, 0);

	return sglist;
}

void ion_carveout_heap_unmap_dma(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	if (buffer->sglist)
		vfree(buffer->sglist);
}

void *ion_carveout_heap_map_kernel(struct ion_heap *heap,
				   struct ion_buffer *buffer,
				   unsigned long flags)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);

	if (atomic_inc_return(&carveout_heap->map_count) == 1)
		carveout_heap->request_region(carveout_heap->bus_id);

	if (ION_IS_CACHED(flags))
		return ioremap_cached(buffer->priv_phys, buffer->size);
	else
		return ioremap(buffer->priv_phys, buffer->size);
}

void ion_carveout_heap_unmap_kernel(struct ion_heap *heap,
				    struct ion_buffer *buffer)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);

	__arch_iounmap(buffer->vaddr);
	buffer->vaddr = NULL;

	if (atomic_dec_and_test(&carveout_heap->map_count))
		carveout_heap->release_region(carveout_heap->bus_id);

	return;
}

int ion_carveout_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
			       struct vm_area_struct *vma, unsigned long flags)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);

	if (atomic_inc_return(&carveout_heap->map_count) == 1)
		carveout_heap->request_region(carveout_heap->bus_id);

	if (ION_IS_CACHED(flags))
		return remap_pfn_range(vma, vma->vm_start,
			       __phys_to_pfn(buffer->priv_phys) + vma->vm_pgoff,
			       vma->vm_end - vma->vm_start,
			       vma->vm_page_prot);
	else
		return remap_pfn_range(vma, vma->vm_start,
			       __phys_to_pfn(buffer->priv_phys) + vma->vm_pgoff,
					vma->vm_end - vma->vm_start,
					pgprot_noncached(vma->vm_page_prot));
}

void ion_carveout_heap_unmap_user(struct ion_heap *heap,
				    struct ion_buffer *buffer)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);

	if (atomic_dec_and_test(&carveout_heap->map_count))
		carveout_heap->release_region(carveout_heap->bus_id);
}

int ion_carveout_cache_ops(struct ion_heap *heap, struct ion_buffer *buffer,
			void *vaddr, unsigned int offset, unsigned int length,
			unsigned int cmd)
{
	unsigned long vstart, pstart;

	pstart = buffer->priv_phys + offset;
	vstart = (unsigned long)vaddr;

	switch (cmd) {
	case ION_IOC_CLEAN_CACHES:
		clean_caches(vstart, length, pstart);
		break;
	case ION_IOC_INV_CACHES:
		invalidate_caches(vstart, length, pstart);
		break;
	case ION_IOC_CLEAN_INV_CACHES:
		clean_and_invalidate_caches(vstart, length, pstart);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static unsigned long ion_carveout_get_allocated(struct ion_heap *heap)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);

	return carveout_heap->allocated_bytes;
}

static unsigned long ion_carveout_get_total(struct ion_heap *heap)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);

	return carveout_heap->total_size;
}

static struct ion_heap_ops carveout_heap_ops = {
	.allocate = ion_carveout_heap_allocate,
	.free = ion_carveout_heap_free,
	.phys = ion_carveout_heap_phys,
	.map_user = ion_carveout_heap_map_user,
	.map_kernel = ion_carveout_heap_map_kernel,
	.unmap_user = ion_carveout_heap_unmap_user,
	.unmap_kernel = ion_carveout_heap_unmap_kernel,
	.map_dma = ion_carveout_heap_map_dma,
	.unmap_dma = ion_carveout_heap_unmap_dma,
	.cache_op = ion_carveout_cache_ops,
	.get_allocated = ion_carveout_get_allocated,
	.get_total = ion_carveout_get_total,
};

struct ion_heap *ion_carveout_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_carveout_heap *carveout_heap;
	int ret;

	carveout_heap = kzalloc(sizeof(struct ion_carveout_heap), GFP_KERNEL);
	if (!carveout_heap)
		return ERR_PTR(-ENOMEM);

	carveout_heap->pool = gen_pool_create(12, -1);
	if (!carveout_heap->pool) {
		kfree(carveout_heap);
		return ERR_PTR(-ENOMEM);
	}
	carveout_heap->base = heap_data->base;
	ret = gen_pool_add(carveout_heap->pool, carveout_heap->base,
			heap_data->size, -1);
	if (ret < 0) {
		kfree(carveout_heap);
		return ERR_PTR(-EINVAL);
	}
	carveout_heap->heap.ops = &carveout_heap_ops;
	carveout_heap->heap.type = ION_HEAP_TYPE_CARVEOUT;
	carveout_heap->allocated_bytes = 0;
	carveout_heap->total_size = heap_data->size;
	carveout_heap->bus_id = heap_data->setup_region();
	carveout_heap->request_region = heap_data->request_region;
	carveout_heap->release_region = heap_data->release_region;

	return &carveout_heap->heap;
}

void ion_carveout_heap_destroy(struct ion_heap *heap)
{
	struct ion_carveout_heap *carveout_heap =
	     container_of(heap, struct  ion_carveout_heap, heap);

	gen_pool_destroy(carveout_heap->pool);
	kfree(carveout_heap);
	carveout_heap = NULL;
}
