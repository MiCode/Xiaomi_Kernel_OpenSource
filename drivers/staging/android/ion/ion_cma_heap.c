/*
 * drivers/staging/android/ion/ion_cma_heap.c
 *
 * Copyright (C) Linaro 2012
 * Author: <benjamin.gaignard@linaro.org> for ST-Ericsson.
 *
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/dma-mapping.h>
#include <linux/msm_ion.h>
#include <linux/of.h>

#include <asm/cacheflush.h>
#include <soc/qcom/secure_buffer.h>

#include "ion.h"
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
static int ion_cma_get_sgtable(struct device *dev, struct sg_table *sgt,
			       void *cpu_addr, dma_addr_t handle, size_t size)
{
	struct page *page = pfn_to_page(PFN_DOWN(handle));
	int ret;

	ret = sg_alloc_table(sgt, 1, GFP_KERNEL);
	if (unlikely(ret))
		return ret;

	sg_set_page(sgt->sgl, page, PAGE_ALIGN(size), 0);
	sg_dma_address(sgt->sgl) = sg_phys(sgt->sgl);
	return 0;
}

static bool ion_cma_has_kernel_mapping(struct ion_heap *heap)
{
	struct device *dev = heap->priv;
	struct device_node *mem_region;

	mem_region = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (IS_ERR(mem_region))
		return false;

	return !of_property_read_bool(mem_region, "no-map");
}

/* ION CMA heap operations functions */
static int ion_cma_allocate(struct ion_heap *heap, struct ion_buffer *buffer,
			    unsigned long len, unsigned long align,
			    unsigned long flags)
{
	struct device *dev = heap->priv;
	struct ion_cma_buffer_info *info;

	info = kzalloc(sizeof(struct ion_cma_buffer_info), GFP_KERNEL);
	if (!info)
		return ION_CMA_ALLOCATE_FAILED;

	/* Override flags if cached-mappings are not supported */
	if (!ion_cma_has_kernel_mapping(heap)) {
		flags &= ~((unsigned long)ION_FLAG_CACHED);
		buffer->flags = flags;
	}

	if (!ION_IS_CACHED(flags))
		info->cpu_addr = dma_alloc_writecombine(dev, len,
							&info->handle,
							GFP_KERNEL);
	else
		info->cpu_addr = dma_alloc_attrs(dev, len, &info->handle,
						GFP_KERNEL,
						DMA_ATTR_FORCE_COHERENT);

	if (!info->cpu_addr) {
		dev_err(dev, "Fail to allocate buffer\n");
		goto err;
	}

	info->table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!info->table)
		goto free_mem;

	info->is_cached = ION_IS_CACHED(flags);

	ion_cma_get_sgtable(dev,
			    info->table, info->cpu_addr, info->handle, len);

	/* Ensure memory is dma-ready - refer to ion_buffer_create() */
	if (info->is_cached)
		dma_sync_sg_for_device(dev, info->table->sgl,
				       info->table->nents, DMA_BIDIRECTIONAL);

	/* keep this for memory release */
	buffer->priv_virt = info;
	return 0;

free_mem:
	if (!ION_IS_CACHED(flags))
		dma_free_writecombine(dev, len, info->cpu_addr, info->handle);
	else
		dma_free_attrs(dev, len, info->cpu_addr, info->handle,
			DMA_ATTR_FORCE_COHERENT);

err:
	kfree(info);
	return ION_CMA_ALLOCATE_FAILED;
}

static void ion_cma_free(struct ion_buffer *buffer)
{
	struct device *dev = buffer->heap->priv;
	struct ion_cma_buffer_info *info = buffer->priv_virt;
	unsigned long attrs = 0;

	/* release memory */
	if (info->is_cached)
		attrs |= DMA_ATTR_FORCE_COHERENT;
	dma_free_attrs(dev, buffer->size, info->cpu_addr, info->handle, attrs);
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

	dev_dbg(dev, "Return buffer %pK physical address %pa\n", buffer,
		&info->handle);

	*addr = info->handle;
	*len = buffer->size;

	return 0;
}

static struct sg_table *ion_cma_heap_map_dma(struct ion_heap *heap,
					     struct ion_buffer *buffer)
{
	struct ion_cma_buffer_info *info = buffer->priv_virt;

	return info->table;
}

static void ion_cma_heap_unmap_dma(struct ion_heap *heap,
				   struct ion_buffer *buffer)
{
}

static int ion_cma_mmap(struct ion_heap *mapper, struct ion_buffer *buffer,
			struct vm_area_struct *vma)
{
	struct device *dev = buffer->heap->priv;
	struct ion_cma_buffer_info *info = buffer->priv_virt;

	if (info->is_cached)
		return dma_mmap_attrs(dev, vma, info->cpu_addr,
				info->handle, buffer->size,
				DMA_ATTR_FORCE_COHERENT);
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
}

static int ion_cma_print_debug(struct ion_heap *heap, struct seq_file *s,
			       const struct list_head *mem_map)
{
	if (mem_map) {
		struct mem_map_data *data;

		seq_puts(s, "\nMemory Map\n");
		seq_printf(s, "%16.s %14.s %14.s %14.s\n",
			   "client", "start address", "end address",
			   "size");

		list_for_each_entry(data, mem_map, node) {
			const char *client_name = "(null)";

			if (data->client_name)
				client_name = data->client_name;

			seq_printf(s, "%16.s 0x%14pa 0x%14pa %14lu (0x%lx)\n",
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

	heap = kzalloc(sizeof(*heap), GFP_KERNEL);

	if (!heap)
		return ERR_PTR(-ENOMEM);

	heap->ops = &ion_cma_ops;
	/*
	 * set device as private heaps data, later it will be
	 * used to make the link with reserved CMA memory
	 */
	heap->priv = data->priv;
	heap->type = (enum ion_heap_type)ION_HEAP_TYPE_DMA;
	cma_heap_has_outer_cache = data->has_outer_cache;
	return heap;
}

void ion_cma_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}

static void ion_secure_cma_free(struct ion_buffer *buffer)
{
	int i, ret = 0;
	int *source_vm_list;
	int source_nelems;
	int dest_vmid;
	int dest_perms;
	struct sg_table *sgt;
	struct scatterlist *sg;
	struct ion_cma_buffer_info *info = buffer->priv_virt;

	source_nelems = count_set_bits(buffer->flags & ION_FLAGS_CP_MASK);
	if (!source_nelems)
		return;
	source_vm_list = kcalloc(source_nelems, sizeof(*source_vm_list),
				 GFP_KERNEL);
	if (!source_vm_list)
		return;
	ret = populate_vm_list(buffer->flags, source_vm_list, source_nelems);
	if (ret) {
		pr_err("%s: Failed to get secure vmids\n", __func__);
		goto out_free_source;
	}

	dest_vmid = VMID_HLOS;
	dest_perms = PERM_READ | PERM_WRITE | PERM_EXEC;

	sgt = info->table;
	ret = hyp_assign_table(sgt, source_vm_list, source_nelems,
			       &dest_vmid, &dest_perms, 1);
	if (ret) {
		pr_err("%s: Not freeing memory since assign failed\n",
		       __func__);
		goto out_free_source;
	}

	for_each_sg(sgt->sgl, sg, sgt->nents, i)
		ClearPagePrivate(sg_page(sg));

	ion_cma_free(buffer);
out_free_source:
	kfree(source_vm_list);
}

static int ion_secure_cma_allocate(
			struct ion_heap *heap,
			struct ion_buffer *buffer, unsigned long len,
			unsigned long align, unsigned long flags)
{
	int i, ret = 0;
	int count;
	int source_vm;
	int *dest_vm_list = NULL;
	int *dest_perms = NULL;
	int dest_nelems;
	struct ion_cma_buffer_info *info;
	struct sg_table *sgt;
	struct scatterlist *sg;

	source_vm = VMID_HLOS;

	dest_nelems = count_set_bits(flags & ION_FLAGS_CP_MASK);
	if (!dest_nelems) {
		ret = -EINVAL;
		goto out;
	}
	dest_vm_list = kcalloc(dest_nelems, sizeof(*dest_vm_list), GFP_KERNEL);
	if (!dest_vm_list) {
		ret = -ENOMEM;
		goto out;
	}
	dest_perms = kcalloc(dest_nelems, sizeof(*dest_perms), GFP_KERNEL);
	if (!dest_perms) {
		ret = -ENOMEM;
		goto out_free_dest_vm;
	}
	ret = populate_vm_list(flags, dest_vm_list, dest_nelems);
	if (ret) {
		pr_err("%s: Failed to get secure vmid(s)\n", __func__);
		goto out_free_dest;
	}

	for (count = 0; count < dest_nelems; count++) {
		if (dest_vm_list[count] == VMID_CP_SEC_DISPLAY)
			dest_perms[count] = PERM_READ;
		else
			dest_perms[count] = PERM_READ | PERM_WRITE;
	}

	ret = ion_cma_allocate(heap, buffer, len, align, flags);
	if (ret) {
		dev_err(heap->priv, "Unable to allocate cma buffer");
		goto out_free_dest;
	}

	info = buffer->priv_virt;
	sgt = info->table;
	ret = hyp_assign_table(sgt, &source_vm, 1, dest_vm_list, dest_perms,
			       dest_nelems);
	if (ret) {
		pr_err("%s: Assign call failed\n", __func__);
		goto err;
	}

	/* Set the private bit to indicate that we've secured this */
	for_each_sg(sgt->sgl, sg, sgt->nents, i)
		SetPagePrivate(sg_page(sg));

	kfree(dest_vm_list);
	kfree(dest_perms);
	return ret;

err:
	ion_secure_cma_free(buffer);
out_free_dest:
	kfree(dest_perms);
out_free_dest_vm:
	kfree(dest_vm_list);
out:
	return ret;
}

static void *ion_secure_cma_map_kernel(struct ion_heap *heap,
				       struct ion_buffer *buffer)
{
	if (!is_buffer_hlos_assigned(buffer)) {
		pr_info("%s: Mapping non-HLOS accessible buffer disallowed\n",
			__func__);
		return NULL;
	}
	return ion_cma_map_kernel(heap, buffer);
}

static int ion_secure_cma_map_user(struct ion_heap *mapper,
				   struct ion_buffer *buffer,
				   struct vm_area_struct *vma)
{
	if (!is_buffer_hlos_assigned(buffer)) {
		pr_info("%s: Mapping non-HLOS accessible buffer disallowed\n",
			__func__);
		return -EINVAL;
	}
	return ion_cma_mmap(mapper, buffer, vma);
}

static struct ion_heap_ops ion_secure_cma_ops = {
	.allocate = ion_secure_cma_allocate,
	.free = ion_secure_cma_free,
	.map_dma = ion_cma_heap_map_dma,
	.unmap_dma = ion_cma_heap_unmap_dma,
	.phys = ion_cma_phys,
	.map_user = ion_secure_cma_map_user,
	.map_kernel = ion_secure_cma_map_kernel,
	.unmap_kernel = ion_cma_unmap_kernel,
	.print_debug = ion_cma_print_debug,
};

struct ion_heap *ion_cma_secure_heap_create(struct ion_platform_heap *data)
{
	struct ion_heap *heap;

	heap = kzalloc(sizeof(*heap), GFP_KERNEL);

	if (!heap)
		return ERR_PTR(-ENOMEM);

	heap->ops = &ion_secure_cma_ops;
	/*
	 *  set device as private heaps data, later it will be
	 * used to make the link with reserved CMA memory
	 */
	heap->priv = data->priv;
	heap->type = (enum ion_heap_type)ION_HEAP_TYPE_HYP_CMA;
	cma_heap_has_outer_cache = data->has_outer_cache;
	return heap;
}

void ion_cma_secure_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}
