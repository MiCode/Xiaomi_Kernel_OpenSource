/*
 * drivers/gpu/ion/ion_removed_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include <linux/seq_file.h>
#include "ion_priv.h"

#include <asm/mach/map.h>
#include <asm/cacheflush.h>
#include <linux/msm_ion.h>

struct ion_removed_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	ion_phys_addr_t base;
	unsigned long allocated_bytes;
	unsigned long total_size;
	int (*request_region)(void *);
	int (*release_region)(void *);
	atomic_t map_count;
	void *bus_id;
};

ion_phys_addr_t ion_removed_allocate(struct ion_heap *heap,
				      unsigned long size,
				      unsigned long align)
{
	struct ion_removed_heap *removed_heap =
		container_of(heap, struct ion_removed_heap, heap);
	unsigned long offset = gen_pool_alloc_aligned(removed_heap->pool,
							size, ilog2(align));

	if (!offset) {
		if ((removed_heap->total_size -
		      removed_heap->allocated_bytes) >= size)
			pr_debug("%s: heap %s has enough memory (%lx) but the allocation of size %lx still failed. Memory is probably fragmented.",
				__func__, heap->name,
				removed_heap->total_size -
				removed_heap->allocated_bytes, size);
		return ION_CARVEOUT_ALLOCATE_FAIL;
	}

	removed_heap->allocated_bytes += size;
	return offset;
}

void ion_removed_free(struct ion_heap *heap, ion_phys_addr_t addr,
		       unsigned long size)
{
	struct ion_removed_heap *removed_heap =
		container_of(heap, struct ion_removed_heap, heap);

	if (addr == ION_CARVEOUT_ALLOCATE_FAIL)
		return;
	gen_pool_free(removed_heap->pool, addr, size);
	removed_heap->allocated_bytes -= size;
}

static int ion_removed_heap_phys(struct ion_heap *heap,
				  struct ion_buffer *buffer,
				  ion_phys_addr_t *addr, size_t *len)
{
	*addr = buffer->priv_phys;
	*len = buffer->size;
	return 0;
}

static int ion_removed_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size, unsigned long align,
				      unsigned long flags)
{
	buffer->priv_phys = ion_removed_allocate(heap, size, align);
	return buffer->priv_phys == ION_CARVEOUT_ALLOCATE_FAIL ? -ENOMEM : 0;
}

static void ion_removed_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;

	ion_removed_free(heap, buffer->priv_phys, buffer->size);
	buffer->priv_phys = ION_CARVEOUT_ALLOCATE_FAIL;
}

struct sg_table *ion_removed_heap_map_dma(struct ion_heap *heap,
					      struct ion_buffer *buffer)
{
	struct sg_table *table;
	int ret;

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto err0;

	table->sgl->length = buffer->size;
	table->sgl->offset = 0;
	table->sgl->dma_address = buffer->priv_phys;

	return table;

err0:
	kfree(table);
	return ERR_PTR(ret);
}

void ion_removed_heap_unmap_dma(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	if (buffer->sg_table)
		sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
	buffer->sg_table = 0;
}

static int ion_removed_request_region(struct ion_removed_heap *removed_heap)
{
	int ret_value = 0;
	if (atomic_inc_return(&removed_heap->map_count) == 1) {
		if (removed_heap->request_region) {
			ret_value = removed_heap->request_region(
						removed_heap->bus_id);
			if (ret_value) {
				pr_err("Unable to request SMI region");
				atomic_dec(&removed_heap->map_count);
			}
		}
	}
	return ret_value;
}

static int ion_removed_release_region(struct ion_removed_heap *removed_heap)
{
	int ret_value = 0;
	if (atomic_dec_and_test(&removed_heap->map_count)) {
		if (removed_heap->release_region) {
			ret_value = removed_heap->release_region(
						removed_heap->bus_id);
			if (ret_value)
				pr_err("Unable to release SMI region");
		}
	}
	return ret_value;
}

void *ion_removed_heap_map_kernel(struct ion_heap *heap,
				   struct ion_buffer *buffer)
{
	struct ion_removed_heap *removed_heap =
		container_of(heap, struct ion_removed_heap, heap);
	void *ret_value;

	if (ion_removed_request_region(removed_heap))
		return NULL;

	if (ION_IS_CACHED(buffer->flags))
		ret_value = ioremap_cached(buffer->priv_phys, buffer->size);
	else
		ret_value = ioremap(buffer->priv_phys, buffer->size);

	if (!ret_value)
		ion_removed_release_region(removed_heap);
	return ret_value;
}

void ion_removed_heap_unmap_kernel(struct ion_heap *heap,
				    struct ion_buffer *buffer)
{
	struct ion_removed_heap *removed_heap =
		container_of(heap, struct ion_removed_heap, heap);

	__arm_iounmap(buffer->vaddr);
	buffer->vaddr = NULL;

	ion_removed_release_region(removed_heap);
	return;
}

int ion_removed_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
			       struct vm_area_struct *vma)
{
	struct ion_removed_heap *removed_heap =
		container_of(heap, struct ion_removed_heap, heap);
	int ret_value = 0;

	if (ion_removed_request_region(removed_heap))
		return -EINVAL;

	if (!ION_IS_CACHED(buffer->flags))
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	ret_value =  remap_pfn_range(vma, vma->vm_start,
			__phys_to_pfn(buffer->priv_phys) + vma->vm_pgoff,
			vma->vm_end - vma->vm_start,
			vma->vm_page_prot);

	if (ret_value)
		ion_removed_release_region(removed_heap);
	return ret_value;
}

void ion_removed_heap_unmap_user(struct ion_heap *heap,
				    struct ion_buffer *buffer)
{
	struct ion_removed_heap *removed_heap =
		container_of(heap, struct ion_removed_heap, heap);
	ion_removed_release_region(removed_heap);
}

static int ion_removed_print_debug(struct ion_heap *heap, struct seq_file *s,
				    const struct rb_root *mem_map)
{
	struct ion_removed_heap *removed_heap =
		container_of(heap, struct ion_removed_heap, heap);

	seq_printf(s, "total bytes currently allocated: %lx\n",
		removed_heap->allocated_bytes);
	seq_printf(s, "total heap size: %lx\n", removed_heap->total_size);

	if (mem_map) {
		unsigned long base = removed_heap->base;
		unsigned long size = removed_heap->total_size;
		unsigned long end = base+size;
		unsigned long last_end = base;
		struct rb_node *n;

		seq_printf(s, "\nMemory Map\n");
		seq_printf(s, "%16.s %14.s %14.s %14.s\n",
			   "client", "start address", "end address",
			   "size (hex)");

		for (n = rb_first(mem_map); n; n = rb_next(n)) {
			struct mem_map_data *data =
					rb_entry(n, struct mem_map_data, node);
			const char *client_name = "(null)";

			if (last_end < data->addr) {
				phys_addr_t da;

				da = data->addr-1;
				seq_printf(s, "%16.s %14pa %14pa %14lu (%lx)\n",
					   "FREE", &last_end, &da,
					   (unsigned long)data->addr-last_end,
					   (unsigned long)data->addr-last_end);
			}

			if (data->client_name)
				client_name = data->client_name;

			seq_printf(s, "%16.s %14pa %14pa %14lu (%lx)\n",
				   client_name, &data->addr,
				   &data->addr_end,
				   data->size, data->size);
			last_end = data->addr_end+1;
		}
		if (last_end < end) {
			seq_printf(s, "%16.s %14lx %14lx %14lu (%lx)\n", "FREE",
				last_end, end-1, end-last_end, end-last_end);
		}
	}
	return 0;
}

static struct ion_heap_ops removed_heap_ops = {
	.allocate = ion_removed_heap_allocate,
	.free = ion_removed_heap_free,
	.phys = ion_removed_heap_phys,
	.map_user = ion_removed_heap_map_user,
	.map_kernel = ion_removed_heap_map_kernel,
	.unmap_user = ion_removed_heap_unmap_user,
	.unmap_kernel = ion_removed_heap_unmap_kernel,
	.map_dma = ion_removed_heap_map_dma,
	.unmap_dma = ion_removed_heap_unmap_dma,
	.print_debug = ion_removed_print_debug,
};

struct ion_heap *ion_removed_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_removed_heap *removed_heap;
	int ret;

	removed_heap = kzalloc(sizeof(struct ion_removed_heap), GFP_KERNEL);
	if (!removed_heap)
		return ERR_PTR(-ENOMEM);

	removed_heap->pool = gen_pool_create(12, -1);
	if (!removed_heap->pool) {
		kfree(removed_heap);
		return ERR_PTR(-ENOMEM);
	}
	removed_heap->base = heap_data->base;
	ret = gen_pool_add(removed_heap->pool, removed_heap->base,
			heap_data->size, -1);
	if (ret < 0) {
		gen_pool_destroy(removed_heap->pool);
		kfree(removed_heap);
		return ERR_PTR(-EINVAL);
	}
	removed_heap->heap.ops = &removed_heap_ops;
	removed_heap->heap.type = ION_HEAP_TYPE_REMOVED;
	removed_heap->allocated_bytes = 0;
	removed_heap->total_size = heap_data->size;

	if (heap_data->extra_data) {
		struct ion_co_heap_pdata *extra_data =
				heap_data->extra_data;

		if (extra_data->setup_region)
			removed_heap->bus_id = extra_data->setup_region();
		if (extra_data->request_region)
			removed_heap->request_region =
					extra_data->request_region;
		if (extra_data->release_region)
			removed_heap->release_region =
					extra_data->release_region;
	}
	return &removed_heap->heap;
}

void ion_removed_heap_destroy(struct ion_heap *heap)
{
	struct ion_removed_heap *removed_heap =
	     container_of(heap, struct  ion_removed_heap, heap);

	gen_pool_destroy(removed_heap->pool);
	kfree(removed_heap);
	removed_heap = NULL;
}
