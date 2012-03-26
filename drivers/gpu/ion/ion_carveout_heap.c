/*
 * drivers/gpu/ion/ion_carveout_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/iommu.h>
#include <linux/seq_file.h>
#include "ion_priv.h"

#include <mach/iommu_domains.h>
#include <asm/mach/map.h>

struct ion_carveout_heap {
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

	sglist = vmalloc(sizeof(struct scatterlist));
	if (!sglist)
		return ERR_PTR(-ENOMEM);

	sg_init_table(sglist, 1);
	sglist->length = buffer->size;
	sglist->offset = 0;
	sglist->dma_address = buffer->priv_phys;

	return sglist;
}

void ion_carveout_heap_unmap_dma(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	if (buffer->sglist)
		vfree(buffer->sglist);
}

static int ion_carveout_request_region(struct ion_carveout_heap *carveout_heap)
{
	int ret_value = 0;
	if (atomic_inc_return(&carveout_heap->map_count) == 1) {
		if (carveout_heap->request_region) {
			ret_value = carveout_heap->request_region(
						carveout_heap->bus_id);
			if (ret_value) {
				pr_err("Unable to request SMI region");
				atomic_dec(&carveout_heap->map_count);
			}
		}
	}
	return ret_value;
}

static int ion_carveout_release_region(struct ion_carveout_heap *carveout_heap)
{
	int ret_value = 0;
	if (atomic_dec_and_test(&carveout_heap->map_count)) {
		if (carveout_heap->release_region) {
			ret_value = carveout_heap->release_region(
						carveout_heap->bus_id);
			if (ret_value)
				pr_err("Unable to release SMI region");
		}
	}
	return ret_value;
}

void *ion_carveout_heap_map_kernel(struct ion_heap *heap,
				   struct ion_buffer *buffer,
				   unsigned long flags)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);
	void *ret_value;

	if (ion_carveout_request_region(carveout_heap))
		return NULL;

	if (ION_IS_CACHED(flags))
		ret_value = ioremap_cached(buffer->priv_phys, buffer->size);
	else
		ret_value = ioremap(buffer->priv_phys, buffer->size);

	if (!ret_value)
		ion_carveout_release_region(carveout_heap);
	return ret_value;
}

void ion_carveout_heap_unmap_kernel(struct ion_heap *heap,
				    struct ion_buffer *buffer)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);

	__arch_iounmap(buffer->vaddr);
	buffer->vaddr = NULL;

	ion_carveout_release_region(carveout_heap);
	return;
}

int ion_carveout_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
			       struct vm_area_struct *vma, unsigned long flags)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);
	int ret_value = 0;

	if (ion_carveout_request_region(carveout_heap))
		return -EINVAL;

	if (!ION_IS_CACHED(flags))
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	ret_value =  remap_pfn_range(vma, vma->vm_start,
			__phys_to_pfn(buffer->priv_phys) + vma->vm_pgoff,
			vma->vm_end - vma->vm_start,
			vma->vm_page_prot);

	if (ret_value)
		ion_carveout_release_region(carveout_heap);
	return ret_value;
}

void ion_carveout_heap_unmap_user(struct ion_heap *heap,
				    struct ion_buffer *buffer)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);
	ion_carveout_release_region(carveout_heap);
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

static int ion_carveout_print_debug(struct ion_heap *heap, struct seq_file *s)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);

	seq_printf(s, "total bytes currently allocated: %lx\n",
		carveout_heap->allocated_bytes);
	seq_printf(s, "total heap size: %lx\n", carveout_heap->total_size);

	return 0;
}

int ion_carveout_heap_map_iommu(struct ion_buffer *buffer,
					struct ion_iommu_map *data,
					unsigned int domain_num,
					unsigned int partition_num,
					unsigned long align,
					unsigned long iova_length,
					unsigned long flags)
{
	unsigned long temp_phys, temp_iova;
	struct iommu_domain *domain;
	int i, ret = 0;
	unsigned long extra;

	data->mapped_size = iova_length;

	if (!msm_use_iommu()) {
		data->iova_addr = buffer->priv_phys;
		return 0;
	}

	extra = iova_length - buffer->size;

	data->iova_addr = msm_allocate_iova_address(domain_num, partition_num,
						data->mapped_size, align);

	if (!data->iova_addr) {
		ret = -ENOMEM;
		goto out;
	}

	domain = msm_get_iommu_domain(domain_num);

	if (!domain) {
		ret = -ENOMEM;
		goto out1;
	}

	temp_iova = data->iova_addr;
	temp_phys = buffer->priv_phys;
	for (i = buffer->size; i > 0; i -= SZ_4K, temp_iova += SZ_4K,
						  temp_phys += SZ_4K) {
		ret = iommu_map(domain, temp_iova, temp_phys,
				get_order(SZ_4K),
				ION_IS_CACHED(flags) ? 1 : 0);

		if (ret) {
			pr_err("%s: could not map %lx to %lx in domain %p\n",
				__func__, temp_iova, temp_phys, domain);
			goto out2;
		}
	}

	if (extra && (msm_iommu_map_extra(domain, temp_iova, extra, flags) < 0))
		goto out2;

	return 0;


out2:
	for ( ; i < buffer->size; i += SZ_4K, temp_iova -= SZ_4K)
		iommu_unmap(domain, temp_iova, get_order(SZ_4K));

out1:
	msm_free_iova_address(data->iova_addr, domain_num, partition_num,
				data->mapped_size);

out:

	return ret;
}

void ion_carveout_heap_unmap_iommu(struct ion_iommu_map *data)
{
	int i;
	unsigned long temp_iova;
	unsigned int domain_num;
	unsigned int partition_num;
	struct iommu_domain *domain;

	if (!msm_use_iommu())
		return;

	domain_num = iommu_map_domain(data);
	partition_num = iommu_map_partition(data);

	domain = msm_get_iommu_domain(domain_num);

	if (!domain) {
		WARN(1, "Could not get domain %d. Corruption?\n", domain_num);
		return;
	}

	temp_iova = data->iova_addr;
	for (i = data->mapped_size; i > 0; i -= SZ_4K, temp_iova += SZ_4K)
		iommu_unmap(domain, temp_iova, get_order(SZ_4K));

	msm_free_iova_address(data->iova_addr, domain_num, partition_num,
				data->mapped_size);

	return;
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
	.print_debug = ion_carveout_print_debug,
	.map_iommu = ion_carveout_heap_map_iommu,
	.unmap_iommu = ion_carveout_heap_unmap_iommu,
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
		gen_pool_destroy(carveout_heap->pool);
		kfree(carveout_heap);
		return ERR_PTR(-EINVAL);
	}
	carveout_heap->heap.ops = &carveout_heap_ops;
	carveout_heap->heap.type = ION_HEAP_TYPE_CARVEOUT;
	carveout_heap->allocated_bytes = 0;
	carveout_heap->total_size = heap_data->size;

	if (heap_data->extra_data) {
		struct ion_co_heap_pdata *extra_data =
				heap_data->extra_data;

		if (extra_data->setup_region)
			carveout_heap->bus_id = extra_data->setup_region();
		if (extra_data->request_region)
			carveout_heap->request_region =
					extra_data->request_region;
		if (extra_data->release_region)
			carveout_heap->release_region =
					extra_data->release_region;
	}
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
