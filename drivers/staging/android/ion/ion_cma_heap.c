/*
 * drivers/staging/android/ion/ion_cma_heap.c
 *
 * Copyright (C) Linaro 2012
 * Author: <benjamin.gaignard@linaro.org> for ST-Ericsson.
 *
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#include <linux/cma.h>
#include <linux/scatterlist.h>
#include <soc/qcom/secure_buffer.h>

#include "ion.h"

struct ion_cma_heap {
	struct ion_heap heap;
	struct cma *cma;
};

#define to_cma_heap(x) container_of(x, struct ion_cma_heap, heap)


/* ION CMA heap operations functions */
static int ion_cma_allocate(struct ion_heap *heap, struct ion_buffer *buffer,
			    unsigned long len,
			    unsigned long flags)
{
	struct ion_cma_heap *cma_heap = to_cma_heap(heap);
	struct sg_table *table;
	struct page *pages;
	int ret;
	void *addr;
	struct device *dev = heap->priv;

	len = PAGE_ALIGN(len);
	pages = cma_alloc(cma_heap->cma, len / PAGE_SIZE, 0, GFP_KERNEL);
	if (!pages)
		return -ENOMEM;

	if (!(flags & ION_FLAG_SECURE)) {
		addr = page_address(pages);
		memset(addr, 0, len);
	}

	if (MAKE_ION_ALLOC_DMA_READY ||
	    (flags & ION_FLAG_SECURE) ||
	    !ion_buffer_cached(buffer))
		ion_pages_sync_for_device(dev, pages, len, DMA_BIDIRECTIONAL);

	table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		goto err;

	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto free_mem;

	sg_set_page(table->sgl, pages, len, 0);

	buffer->priv_virt = pages;
	buffer->sg_table = table;
	return 0;

free_mem:
	kfree(table);
err:
	cma_release(cma_heap->cma, pages, buffer->size);
	return -ENOMEM;
}

static void ion_cma_free(struct ion_buffer *buffer)
{
	struct ion_cma_heap *cma_heap = to_cma_heap(buffer->heap);
	struct page *pages = buffer->priv_virt;
	unsigned int count = PAGE_ALIGN(buffer->size) / PAGE_SIZE;

	/* release memory */
	cma_release(cma_heap->cma, pages, count);
	/* release sg table */
	sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
}

static struct ion_heap_ops ion_cma_ops = {
	.allocate = ion_cma_allocate,
	.free = ion_cma_free,
	.map_user = ion_heap_map_user,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
};

struct ion_heap *ion_cma_heap_create(struct ion_platform_heap *data)
{
	struct ion_cma_heap *cma_heap;
	struct device *dev = (struct device *)data->priv;

	if (!dev->cma_area)
		return ERR_PTR(-EINVAL);

	cma_heap = kzalloc(sizeof(*cma_heap), GFP_KERNEL);

	if (!cma_heap)
		return ERR_PTR(-ENOMEM);

	cma_heap->heap.ops = &ion_cma_ops;
	/*
	 * get device from private heaps data, later it will be
	 * used to make the link with reserved CMA memory
	 */
	cma_heap->cma = dev->cma_area;
	cma_heap->heap.type = ION_HEAP_TYPE_DMA;
	return &cma_heap->heap;
}

void ion_cma_heap_destroy(struct ion_heap *heap)
{
	struct ion_cma_heap *cma_heap = to_cma_heap(heap);

	kfree(cma_heap);
}

static int ion_secure_cma_unassign_buffer(struct ion_buffer *buffer)
{
	int ret = 0;
	int *source_vm_list;
	int source_nelems;
	int dest_vmid;
	int dest_perms;

	source_nelems = count_set_bits(buffer->flags & ION_FLAGS_CP_MASK);
	source_vm_list = kcalloc(source_nelems, sizeof(*source_vm_list),
				 GFP_KERNEL);
	if (!source_vm_list)
		return -ENOMEM;
	ret = populate_vm_list(buffer->flags, source_vm_list, source_nelems);
	if (ret) {
		pr_err("%s: Failed to get secure vmids\n", __func__);
		goto out_free_source;
	}

	dest_vmid = VMID_HLOS;
	dest_perms = PERM_READ | PERM_WRITE | PERM_EXEC;

	ret = hyp_assign_table(buffer->sg_table, source_vm_list, source_nelems,
			       &dest_vmid, &dest_perms, 1);
	if (ret) {
		pr_err("%s: Not freeing memory since assign failed\n",
		       __func__);
	}

out_free_source:
	kfree(source_vm_list);
	return ret;
}

static void ion_secure_cma_free(struct ion_buffer *buffer)
{
	if (ion_secure_cma_unassign_buffer(buffer))
		return;

	ion_cma_free(buffer);
}

static int ion_secure_cma_assign_buffer(
			struct ion_buffer *buffer,
			unsigned long flags)
{
	int ret = 0;
	int count;
	int source_vm;
	int *dest_vm_list = NULL;
	int *dest_perms = NULL;
	int dest_nelems;

	source_vm = VMID_HLOS;

	dest_nelems = count_set_bits(flags & ION_FLAGS_CP_MASK);
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

	ret = hyp_assign_table(buffer->sg_table, &source_vm, 1,
			       dest_vm_list, dest_perms, dest_nelems);
	if (ret) {
		pr_err("%s: Assign call failed\n", __func__);
		goto out_free_dest;
	}

	kfree(dest_vm_list);
	kfree(dest_perms);
	return ret;

out_free_dest:
	kfree(dest_perms);
out_free_dest_vm:
	kfree(dest_vm_list);
out:
	return ret;
}

static int ion_secure_cma_allocate(
			struct ion_heap *heap,
			struct ion_buffer *buffer, unsigned long len,
			unsigned long flags)
{
	int ret;

	ret = ion_cma_allocate(heap, buffer, len, flags);
	if (ret) {
		dev_err(heap->priv, "Unable to allocate cma buffer");
		goto out;
	}

	ret = ion_secure_cma_assign_buffer(buffer, flags);
	if (ret)
		goto out_free_buf;

	return ret;

out_free_buf:
	ion_secure_cma_free(buffer);
out:
	return ret;
}

static struct ion_heap_ops ion_secure_cma_ops = {
	.allocate = ion_secure_cma_allocate,
	.free = ion_secure_cma_free,
	.map_user = ion_heap_map_user,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
};

struct ion_heap *ion_cma_secure_heap_create(struct ion_platform_heap *data)
{
	struct ion_cma_heap *cma_heap;
	struct device *dev = (struct device *)data->priv;

	if (!dev->cma_area)
		return ERR_PTR(-EINVAL);

	cma_heap = kzalloc(sizeof(*cma_heap), GFP_KERNEL);

	if (!cma_heap)
		return ERR_PTR(-ENOMEM);

	cma_heap->heap.ops = &ion_secure_cma_ops;
	/*
	 * get device from private heaps data, later it will be
	 * used to make the link with reserved CMA memory
	 */
	cma_heap->cma = dev->cma_area;
	cma_heap->heap.type = (enum ion_heap_type)ION_HEAP_TYPE_HYP_CMA;
	return &cma_heap->heap;
}

void ion_cma_secure_heap_destroy(struct ion_heap *heap)
{
	struct ion_cma_heap *cma_heap = to_cma_heap(heap);

	kfree(cma_heap);
}
