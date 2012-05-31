/*
 * drivers/gpu/ion/ion_carveout_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
#include <asm/cacheflush.h>

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
	unsigned int has_outer_cache;
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
		      carveout_heap->allocated_bytes) >= size)
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

struct sg_table *ion_carveout_heap_map_dma(struct ion_heap *heap,
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

void ion_carveout_heap_unmap_dma(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	if (buffer->sg_table)
		sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
	buffer->sg_table = 0;
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
				   struct ion_buffer *buffer)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);
	void *ret_value;

	if (ion_carveout_request_region(carveout_heap))
		return NULL;

	if (ION_IS_CACHED(buffer->flags))
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

	__arm_iounmap(buffer->vaddr);
	buffer->vaddr = NULL;

	ion_carveout_release_region(carveout_heap);
	return;
}

int ion_carveout_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
			       struct vm_area_struct *vma)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);
	int ret_value = 0;

	if (ion_carveout_request_region(carveout_heap))
		return -EINVAL;

	if (!ION_IS_CACHED(buffer->flags))
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
	void (*outer_cache_op)(phys_addr_t, phys_addr_t);
	struct ion_carveout_heap *carveout_heap =
	     container_of(heap, struct  ion_carveout_heap, heap);

	switch (cmd) {
	case ION_IOC_CLEAN_CACHES:
		dmac_clean_range(vaddr, vaddr + length);
		outer_cache_op = outer_clean_range;
		break;
	case ION_IOC_INV_CACHES:
		dmac_inv_range(vaddr, vaddr + length);
		outer_cache_op = outer_inv_range;
		break;
	case ION_IOC_CLEAN_INV_CACHES:
		dmac_flush_range(vaddr, vaddr + length);
		outer_cache_op = outer_flush_range;
		break;
	default:
		return -EINVAL;
	}

	if (carveout_heap->has_outer_cache) {
		unsigned long pstart = buffer->priv_phys + offset;
		outer_cache_op(pstart, pstart + length);
	}
	return 0;
}

static int ion_carveout_print_debug(struct ion_heap *heap, struct seq_file *s,
				    const struct rb_root *mem_map)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);

	seq_printf(s, "total bytes currently allocated: %lx\n",
		carveout_heap->allocated_bytes);
	seq_printf(s, "total heap size: %lx\n", carveout_heap->total_size);

	if (mem_map) {
		unsigned long base = carveout_heap->base;
		unsigned long size = carveout_heap->total_size;
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
				seq_printf(s, "%16.s %14lx %14lx %14lu (%lx)\n",
					   "FREE", last_end, data->addr-1,
					   data->addr-last_end,
					   data->addr-last_end);
			}

			if (data->client_name)
				client_name = data->client_name;

			seq_printf(s, "%16.s %14lx %14lx %14lu (%lx)\n",
				   client_name, data->addr,
				   data->addr_end,
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

int ion_carveout_heap_map_iommu(struct ion_buffer *buffer,
					struct ion_iommu_map *data,
					unsigned int domain_num,
					unsigned int partition_num,
					unsigned long align,
					unsigned long iova_length,
					unsigned long flags)
{
	struct iommu_domain *domain;
	int ret = 0;
	unsigned long extra;
	struct scatterlist *sglist = 0;
	int prot = IOMMU_WRITE | IOMMU_READ;
	prot |= ION_IS_CACHED(flags) ? IOMMU_CACHE : 0;

	data->mapped_size = iova_length;

	if (!msm_use_iommu()) {
		data->iova_addr = buffer->priv_phys;
		return 0;
	}

	extra = iova_length - buffer->size;

	ret = msm_allocate_iova_address(domain_num, partition_num,
						data->mapped_size, align,
						&data->iova_addr);

	if (ret)
		goto out;

	domain = msm_get_iommu_domain(domain_num);

	if (!domain) {
		ret = -ENOMEM;
		goto out1;
	}

	sglist = vmalloc(sizeof(*sglist));
	if (!sglist)
		goto out1;

	sg_init_table(sglist, 1);
	sglist->length = buffer->size;
	sglist->offset = 0;
	sglist->dma_address = buffer->priv_phys;

	ret = iommu_map_range(domain, data->iova_addr, sglist,
			      buffer->size, prot);
	if (ret) {
		pr_err("%s: could not map %lx in domain %p\n",
			__func__, data->iova_addr, domain);
		goto out1;
	}

	if (extra) {
		unsigned long extra_iova_addr = data->iova_addr + buffer->size;
		ret = msm_iommu_map_extra(domain, extra_iova_addr, extra,
					  SZ_4K, prot);
		if (ret)
			goto out2;
	}
	vfree(sglist);
	return ret;

out2:
	iommu_unmap_range(domain, data->iova_addr, buffer->size);
out1:
	vfree(sglist);
	msm_free_iova_address(data->iova_addr, domain_num, partition_num,
				data->mapped_size);

out:

	return ret;
}

void ion_carveout_heap_unmap_iommu(struct ion_iommu_map *data)
{
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

	iommu_unmap_range(domain, data->iova_addr, data->mapped_size);
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
	carveout_heap->has_outer_cache = heap_data->has_outer_cache;

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
