/*
 * drivers/gpu/ion/ion_system_heap.c
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

#include <linux/err.h>
#include <linux/ion.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/iommu.h>
#include <linux/seq_file.h>
#include <mach/iommu_domains.h>
#include "ion_priv.h"
#include <mach/memory.h>
#include <asm/cacheflush.h>

static atomic_t system_heap_allocated;
static atomic_t system_contig_heap_allocated;
static unsigned int system_heap_has_outer_cache;
static unsigned int system_heap_contig_has_outer_cache;

static int ion_system_heap_allocate(struct ion_heap *heap,
				     struct ion_buffer *buffer,
				     unsigned long size, unsigned long align,
				     unsigned long flags)
{
	struct sg_table *table;
	struct scatterlist *sg;
	int i, j;
	int npages = PAGE_ALIGN(size) / PAGE_SIZE;

	table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;
	i = sg_alloc_table(table, npages, GFP_KERNEL);
	if (i)
		goto err0;
	for_each_sg(table->sgl, sg, table->nents, i) {
		struct page *page;
		page = alloc_page(GFP_KERNEL|__GFP_ZERO);
		if (!page)
			goto err1;
		sg_set_page(sg, page, PAGE_SIZE, 0);
	}
	buffer->priv_virt = table;
	atomic_add(size, &system_heap_allocated);
	return 0;
err1:
	for_each_sg(table->sgl, sg, i, j)
		__free_page(sg_page(sg));
	sg_free_table(table);
err0:
	kfree(table);
	return -ENOMEM;
}

void ion_system_heap_free(struct ion_buffer *buffer)
{
	int i;
	struct scatterlist *sg;
	struct sg_table *table = buffer->priv_virt;

	for_each_sg(table->sgl, sg, table->nents, i)
		__free_page(sg_page(sg));
	if (buffer->sg_table)
		sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
	atomic_sub(buffer->size, &system_heap_allocated);
}

struct sg_table *ion_system_heap_map_dma(struct ion_heap *heap,
					 struct ion_buffer *buffer)
{
	return buffer->priv_virt;
}

void ion_system_heap_unmap_dma(struct ion_heap *heap,
			       struct ion_buffer *buffer)
{
	return;
}

void *ion_system_heap_map_kernel(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	if (!ION_IS_CACHED(buffer->flags)) {
		pr_err("%s: cannot map system heap uncached\n", __func__);
		return ERR_PTR(-EINVAL);
	} else {
		struct scatterlist *sg;
		int i;
		void *vaddr;
		struct sg_table *table = buffer->priv_virt;
		struct page **pages = kmalloc(
					sizeof(struct page *) * table->nents,
					GFP_KERNEL);

		for_each_sg(table->sgl, sg, table->nents, i)
			pages[i] = sg_page(sg);
		vaddr = vmap(pages, table->nents, VM_MAP, PAGE_KERNEL);
		kfree(pages);

		return vaddr;
	}
}

void ion_system_heap_unmap_kernel(struct ion_heap *heap,
				  struct ion_buffer *buffer)
{
	vunmap(buffer->vaddr);
}

void ion_system_heap_unmap_iommu(struct ion_iommu_map *data)
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

int ion_system_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
			     struct vm_area_struct *vma)
{
	if (!ION_IS_CACHED(buffer->flags)) {
		pr_err("%s: cannot map system heap uncached\n", __func__);
		return -EINVAL;
	} else {
		struct sg_table *table = buffer->priv_virt;
		unsigned long addr = vma->vm_start;
		unsigned long offset = vma->vm_pgoff;
		struct scatterlist *sg;
		int i;

		for_each_sg(table->sgl, sg, table->nents, i) {
			if (offset) {
				offset--;
				continue;
			}
			vm_insert_page(vma, addr, sg_page(sg));
			addr += PAGE_SIZE;
		}
		return 0;
	}
}

int ion_system_heap_cache_ops(struct ion_heap *heap, struct ion_buffer *buffer,
			void *vaddr, unsigned int offset, unsigned int length,
			unsigned int cmd)
{
	void (*outer_cache_op)(phys_addr_t, phys_addr_t);

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

	if (system_heap_has_outer_cache) {
		unsigned long pstart;
		struct sg_table *table = buffer->priv_virt;
		struct scatterlist *sg;
		int i;
		for_each_sg(table->sgl, sg, table->nents, i) {
			struct page *page = sg_page(sg);
			pstart = page_to_phys(page);
			/*
			 * If page -> phys is returning NULL, something
			 * has really gone wrong...
			 */
			if (!pstart) {
				WARN(1, "Could not translate virtual address to physical address\n");
				return -EINVAL;
			}
			outer_cache_op(pstart, pstart + PAGE_SIZE);
		}
	}
	return 0;
}

static int ion_system_print_debug(struct ion_heap *heap, struct seq_file *s,
				  const struct rb_root *unused)
{
	seq_printf(s, "total bytes currently allocated: %lx\n",
			(unsigned long) atomic_read(&system_heap_allocated));

	return 0;
}

int ion_system_heap_map_iommu(struct ion_buffer *buffer,
				struct ion_iommu_map *data,
				unsigned int domain_num,
				unsigned int partition_num,
				unsigned long align,
				unsigned long iova_length,
				unsigned long flags)
{
	int ret = 0;
	struct iommu_domain *domain;
	unsigned long extra;
	unsigned long extra_iova_addr;
	struct sg_table *table = buffer->priv_virt;
	int prot = IOMMU_WRITE | IOMMU_READ;
	prot |= ION_IS_CACHED(flags) ? IOMMU_CACHE : 0;

	if (!ION_IS_CACHED(flags))
		return -EINVAL;

	if (!msm_use_iommu())
		return -EINVAL;

	data->mapped_size = iova_length;
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

	ret = iommu_map_range(domain, data->iova_addr, table->sgl,
			      buffer->size, prot);

	if (ret) {
		pr_err("%s: could not map %lx in domain %p\n",
			__func__, data->iova_addr, domain);
		goto out1;
	}

	extra_iova_addr = data->iova_addr + buffer->size;
	if (extra) {
		ret = msm_iommu_map_extra(domain, extra_iova_addr, extra, SZ_4K,
					  prot);
		if (ret)
			goto out2;
	}
	return ret;

out2:
	iommu_unmap_range(domain, data->iova_addr, buffer->size);
out1:
	msm_free_iova_address(data->iova_addr, domain_num, partition_num,
				data->mapped_size);
out:
	return ret;
}

static struct ion_heap_ops vmalloc_ops = {
	.allocate = ion_system_heap_allocate,
	.free = ion_system_heap_free,
	.map_dma = ion_system_heap_map_dma,
	.unmap_dma = ion_system_heap_unmap_dma,
	.map_kernel = ion_system_heap_map_kernel,
	.unmap_kernel = ion_system_heap_unmap_kernel,
	.map_user = ion_system_heap_map_user,
	.cache_op = ion_system_heap_cache_ops,
	.print_debug = ion_system_print_debug,
	.map_iommu = ion_system_heap_map_iommu,
	.unmap_iommu = ion_system_heap_unmap_iommu,
};

struct ion_heap *ion_system_heap_create(struct ion_platform_heap *pheap)
{
	struct ion_heap *heap;

	heap = kzalloc(sizeof(struct ion_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->ops = &vmalloc_ops;
	heap->type = ION_HEAP_TYPE_SYSTEM;
	system_heap_has_outer_cache = pheap->has_outer_cache;
	return heap;
}

void ion_system_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}

static int ion_system_contig_heap_allocate(struct ion_heap *heap,
					   struct ion_buffer *buffer,
					   unsigned long len,
					   unsigned long align,
					   unsigned long flags)
{
	buffer->priv_virt = kzalloc(len, GFP_KERNEL);
	if (!buffer->priv_virt)
		return -ENOMEM;
	atomic_add(len, &system_contig_heap_allocated);
	return 0;
}

void ion_system_contig_heap_free(struct ion_buffer *buffer)
{
	kfree(buffer->priv_virt);
	atomic_sub(buffer->size, &system_contig_heap_allocated);
}

static int ion_system_contig_heap_phys(struct ion_heap *heap,
				       struct ion_buffer *buffer,
				       ion_phys_addr_t *addr, size_t *len)
{
	*addr = virt_to_phys(buffer->priv_virt);
	*len = buffer->size;
	return 0;
}

struct sg_table *ion_system_contig_heap_map_dma(struct ion_heap *heap,
						   struct ion_buffer *buffer)
{
	struct sg_table *table;
	int ret;

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		kfree(table);
		return ERR_PTR(ret);
	}
	sg_set_page(table->sgl, virt_to_page(buffer->priv_virt), buffer->size,
		    0);
	return table;
}

int ion_system_contig_heap_map_user(struct ion_heap *heap,
				    struct ion_buffer *buffer,
				    struct vm_area_struct *vma)
{
	unsigned long pfn = __phys_to_pfn(virt_to_phys(buffer->priv_virt));

	if (ION_IS_CACHED(buffer->flags))
		return remap_pfn_range(vma, vma->vm_start, pfn + vma->vm_pgoff,
			       vma->vm_end - vma->vm_start,
			       vma->vm_page_prot);
	else {
		pr_err("%s: cannot map system heap uncached\n", __func__);
		return -EINVAL;
	}
}

int ion_system_contig_heap_cache_ops(struct ion_heap *heap,
			struct ion_buffer *buffer, void *vaddr,
			unsigned int offset, unsigned int length,
			unsigned int cmd)
{
	void (*outer_cache_op)(phys_addr_t, phys_addr_t);

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

	if (system_heap_contig_has_outer_cache) {
		unsigned long pstart;

		pstart = virt_to_phys(buffer->priv_virt) + offset;
		if (!pstart) {
			WARN(1, "Could not do virt to phys translation on %p\n",
				buffer->priv_virt);
			return -EINVAL;
		}

		outer_cache_op(pstart, pstart + PAGE_SIZE);
	}

	return 0;
}

static int ion_system_contig_print_debug(struct ion_heap *heap,
					 struct seq_file *s,
					 const struct rb_root *unused)
{
	seq_printf(s, "total bytes currently allocated: %lx\n",
		(unsigned long) atomic_read(&system_contig_heap_allocated));

	return 0;
}

int ion_system_contig_heap_map_iommu(struct ion_buffer *buffer,
				struct ion_iommu_map *data,
				unsigned int domain_num,
				unsigned int partition_num,
				unsigned long align,
				unsigned long iova_length,
				unsigned long flags)
{
	int ret = 0;
	struct iommu_domain *domain;
	unsigned long extra;
	struct scatterlist *sglist = 0;
	struct page *page = 0;
	int prot = IOMMU_WRITE | IOMMU_READ;
	prot |= ION_IS_CACHED(flags) ? IOMMU_CACHE : 0;

	if (!ION_IS_CACHED(flags))
		return -EINVAL;

	if (!msm_use_iommu()) {
		data->iova_addr = virt_to_phys(buffer->vaddr);
		return 0;
	}

	data->mapped_size = iova_length;
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
	page = virt_to_page(buffer->vaddr);

	sglist = vmalloc(sizeof(*sglist));
	if (!sglist)
		goto out1;

	sg_init_table(sglist, 1);
	sg_set_page(sglist, page, buffer->size, 0);

	ret = iommu_map_range(domain, data->iova_addr, sglist,
			      buffer->size, prot);
	if (ret) {
		pr_err("%s: could not map %lx in domain %p\n",
			__func__, data->iova_addr, domain);
		goto out1;
	}

	if (extra) {
		unsigned long extra_iova_addr = data->iova_addr + buffer->size;
		ret = msm_iommu_map_extra(domain, extra_iova_addr, extra, SZ_4K,
					  prot);
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

static struct ion_heap_ops kmalloc_ops = {
	.allocate = ion_system_contig_heap_allocate,
	.free = ion_system_contig_heap_free,
	.phys = ion_system_contig_heap_phys,
	.map_dma = ion_system_contig_heap_map_dma,
	.unmap_dma = ion_system_heap_unmap_dma,
	.map_kernel = ion_system_heap_map_kernel,
	.unmap_kernel = ion_system_heap_unmap_kernel,
	.map_user = ion_system_contig_heap_map_user,
	.cache_op = ion_system_contig_heap_cache_ops,
	.print_debug = ion_system_contig_print_debug,
	.map_iommu = ion_system_contig_heap_map_iommu,
	.unmap_iommu = ion_system_heap_unmap_iommu,
};

struct ion_heap *ion_system_contig_heap_create(struct ion_platform_heap *pheap)
{
	struct ion_heap *heap;

	heap = kzalloc(sizeof(struct ion_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->ops = &kmalloc_ops;
	heap->type = ION_HEAP_TYPE_SYSTEM_CONTIG;
	system_heap_contig_has_outer_cache = pheap->has_outer_cache;
	return heap;
}

void ion_system_contig_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}

