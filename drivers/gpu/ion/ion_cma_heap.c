/*
 * drivers/gpu/ion/ion_cma_heap.c
 *
 * Copyright (C) Linaro 2012
 * Author: <benjamin.gaignard@linaro.org> for ST-Ericsson.
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

#include <linux/device.h>
#include <linux/ion.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/dma-mapping.h>
#include <linux/msm_ion.h>
#include <mach/iommu_domains.h>

#include <asm/cacheflush.h>

/* for ion_heap_ops structure */
#include "ion_priv.h"

#define ION_CMA_ALLOCATE_FAILED -1

struct ion_cma_buffer_info {
	void *cpu_addr;
	dma_addr_t handle;
	struct sg_table *table;
	bool is_cached;
};

static int cma_heap_has_outer_cache;
/*
 * Create scatter-list for the already allocated DMA buffer.
 * This function could be replace by dma_common_get_sgtable
 * as soon as it will avalaible.
 */
int ion_cma_get_sgtable(struct device *dev, struct sg_table *sgt,
			void *cpu_addr, dma_addr_t handle, size_t size)
{
	struct page *page = phys_to_page(handle);
	int ret;

	ret = sg_alloc_table(sgt, 1, GFP_KERNEL);
	if (unlikely(ret))
		return ret;

	sg_set_page(sgt->sgl, page, PAGE_ALIGN(size), 0);
	return 0;
}

/* ION CMA heap operations functions */
static int ion_cma_allocate(struct ion_heap *heap, struct ion_buffer *buffer,
			    unsigned long len, unsigned long align,
			    unsigned long flags)
{
	struct device *dev = heap->priv;
	struct ion_cma_buffer_info *info;

	dev_dbg(dev, "Request buffer allocation len %ld\n", len);

	info = kzalloc(sizeof(struct ion_cma_buffer_info), GFP_KERNEL);
	if (!info) {
		dev_err(dev, "Can't allocate buffer info\n");
		return ION_CMA_ALLOCATE_FAILED;
	}

	if (!ION_IS_CACHED(flags))
		info->cpu_addr = dma_alloc_writecombine(dev, len,
					&(info->handle), 0);
	else
		info->cpu_addr = dma_alloc_nonconsistent(dev, len,
					&(info->handle), 0);

	if (!info->cpu_addr) {
		dev_err(dev, "Fail to allocate buffer\n");
		goto err;
	}

	info->table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!info->table) {
		dev_err(dev, "Fail to allocate sg table\n");
		goto err;
	}

	info->is_cached = ION_IS_CACHED(flags);

	ion_cma_get_sgtable(dev,
			info->table, info->cpu_addr, info->handle, len);

	/* keep this for memory release */
	buffer->priv_virt = info;
	dev_dbg(dev, "Allocate buffer %p\n", buffer);
	return 0;

err:
	kfree(info);
	return ION_CMA_ALLOCATE_FAILED;
}

static void ion_cma_free(struct ion_buffer *buffer)
{
	struct device *dev = buffer->heap->priv;
	struct ion_cma_buffer_info *info = buffer->priv_virt;

	dev_dbg(dev, "Release buffer %p\n", buffer);
	/* release memory */
	dma_free_coherent(dev, buffer->size, info->cpu_addr, info->handle);
	sg_free_table(info->table);
	/* release sg table */
	kfree(info->table);
	kfree(info);
}

/* return physical address in addr */
static int ion_cma_phys(struct ion_heap *heap, struct ion_buffer *buffer,
			ion_phys_addr_t *addr, size_t *len)
{
	struct device *dev = heap->priv;
	struct ion_cma_buffer_info *info = buffer->priv_virt;

	dev_dbg(dev, "Return buffer %p physical address 0x%pa\n", buffer,
		&info->handle);

	*addr = info->handle;
	*len = buffer->size;

	return 0;
}

struct sg_table *ion_cma_heap_map_dma(struct ion_heap *heap,
					 struct ion_buffer *buffer)
{
	struct ion_cma_buffer_info *info = buffer->priv_virt;

	return info->table;
}

void ion_cma_heap_unmap_dma(struct ion_heap *heap,
			       struct ion_buffer *buffer)
{
	return;
}

static int ion_cma_mmap(struct ion_heap *mapper, struct ion_buffer *buffer,
			struct vm_area_struct *vma)
{
	struct device *dev = buffer->heap->priv;
	struct ion_cma_buffer_info *info = buffer->priv_virt;

	if (info->is_cached)
		return dma_mmap_nonconsistent(dev, vma, info->cpu_addr,
				info->handle, buffer->size);
	else
		return dma_mmap_writecombine(dev, vma, info->cpu_addr,
				info->handle, buffer->size);
}

static void *ion_cma_map_kernel(struct ion_heap *heap,
				struct ion_buffer *buffer)
{
	struct ion_cma_buffer_info *info = buffer->priv_virt;

	return info->cpu_addr;
}

static void ion_cma_unmap_kernel(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	return;
}

static int ion_cma_print_debug(struct ion_heap *heap, struct seq_file *s,
			const struct rb_root *mem_map)
{
	if (mem_map) {
		struct rb_node *n;

		seq_printf(s, "\nMemory Map\n");
		seq_printf(s, "%16.s %14.s %14.s %14.s\n",
			   "client", "start address", "end address",
			   "size (hex)");

		for (n = rb_first(mem_map); n; n = rb_next(n)) {
			struct mem_map_data *data =
					rb_entry(n, struct mem_map_data, node);
			const char *client_name = "(null)";


			if (data->client_name)
				client_name = data->client_name;

			seq_printf(s, "%16.s %14pa %14pa %14lu (%lx)\n",
				   client_name, &data->addr,
				   &data->addr_end,
				   data->size, data->size);
		}
	}
	return 0;
}

static struct ion_heap_ops ion_cma_ops = {
	.allocate = ion_cma_allocate,
	.free = ion_cma_free,
	.map_dma = ion_cma_heap_map_dma,
	.unmap_dma = ion_cma_heap_unmap_dma,
	.phys = ion_cma_phys,
	.map_user = ion_cma_mmap,
	.map_kernel = ion_cma_map_kernel,
	.unmap_kernel = ion_cma_unmap_kernel,
	.print_debug = ion_cma_print_debug,
};

struct ion_heap *ion_cma_heap_create(struct ion_platform_heap *data)
{
	struct ion_heap *heap;

	heap = kzalloc(sizeof(struct ion_heap), GFP_KERNEL);

	if (!heap)
		return ERR_PTR(-ENOMEM);

	heap->ops = &ion_cma_ops;
	/* set device as private heaps data, later it will be
	 * used to make the link with reserved CMA memory */
	heap->priv = data->priv;
	heap->type = ION_HEAP_TYPE_DMA;
	cma_heap_has_outer_cache = data->has_outer_cache;
	return heap;
}

void ion_cma_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}
