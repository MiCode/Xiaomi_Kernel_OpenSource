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

static atomic_t system_heap_allocated;
static atomic_t system_contig_heap_allocated;

static int ion_system_heap_allocate(struct ion_heap *heap,
				     struct ion_buffer *buffer,
				     unsigned long size, unsigned long align,
				     unsigned long flags)
{
	buffer->priv_virt = vmalloc_user(size);
	if (!buffer->priv_virt)
		return -ENOMEM;

	atomic_add(size, &system_heap_allocated);
	return 0;
}

void ion_system_heap_free(struct ion_buffer *buffer)
{
	vfree(buffer->priv_virt);
	atomic_sub(buffer->size, &system_heap_allocated);
}

struct scatterlist *ion_system_heap_map_dma(struct ion_heap *heap,
					    struct ion_buffer *buffer)
{
	struct scatterlist *sglist;
	struct page *page;
	int i;
	int npages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;
	void *vaddr = buffer->priv_virt;

	sglist = vmalloc(npages * sizeof(struct scatterlist));
	if (!sglist)
		return ERR_PTR(-ENOMEM);
	memset(sglist, 0, npages * sizeof(struct scatterlist));
	sg_init_table(sglist, npages);
	for (i = 0; i < npages; i++) {
		page = vmalloc_to_page(vaddr);
		if (!page)
			goto end;
		sg_set_page(&sglist[i], page, PAGE_SIZE, 0);
		vaddr += PAGE_SIZE;
	}
	/* XXX do cache maintenance for dma? */
	return sglist;
end:
	vfree(sglist);
	return NULL;
}

void ion_system_heap_unmap_dma(struct ion_heap *heap,
			       struct ion_buffer *buffer)
{
	/* XXX undo cache maintenance for dma? */
	if (buffer->sglist)
		vfree(buffer->sglist);
}

void *ion_system_heap_map_kernel(struct ion_heap *heap,
				 struct ion_buffer *buffer,
				 unsigned long flags)
{
	if (ION_IS_CACHED(flags))
		return buffer->priv_virt;
	else {
		pr_err("%s: cannot map system heap uncached\n", __func__);
		return ERR_PTR(-EINVAL);
	}
}

void ion_system_heap_unmap_kernel(struct ion_heap *heap,
				  struct ion_buffer *buffer)
{
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
			     struct vm_area_struct *vma, unsigned long flags)
{
	if (ION_IS_CACHED(flags))
		return remap_vmalloc_range(vma, buffer->priv_virt,
						vma->vm_pgoff);
	else {
		pr_err("%s: cannot map system heap uncached\n", __func__);
		return -EINVAL;
	}
}

int ion_system_heap_cache_ops(struct ion_heap *heap, struct ion_buffer *buffer,
			void *vaddr, unsigned int offset, unsigned int length,
			unsigned int cmd)
{
	unsigned long vstart, pstart;
	void *vtemp;
	unsigned long ln = 0;
	void (*op)(unsigned long, unsigned long, unsigned long);

	switch (cmd) {
	case ION_IOC_CLEAN_CACHES:
		op = clean_caches;
		break;
	case ION_IOC_INV_CACHES:
		op = invalidate_caches;
		break;
	case ION_IOC_CLEAN_INV_CACHES:
		op = clean_and_invalidate_caches;
		break;
	default:
		return -EINVAL;
	}

	for (vtemp = buffer->priv_virt + offset,
	     vstart = (unsigned long) vaddr;
			ln < length;
			vtemp += PAGE_SIZE, ln += PAGE_SIZE,
			vstart += PAGE_SIZE) {
		pstart = page_to_phys(vmalloc_to_page(vtemp));
		/*
		 * If vmalloc -> page -> phys is returning NULL, something
		 * has really gone wrong...
		 */
		if (!pstart) {
			WARN(1, "Could not translate %p to physical address\n",
				vtemp);
			return -EINVAL;
		}

		op(vstart, PAGE_SIZE, pstart);
	}

	return 0;
}

static int ion_system_print_debug(struct ion_heap *heap, struct seq_file *s)
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
	int ret = 0, i;
	struct iommu_domain *domain;
	unsigned long extra;
	unsigned long extra_iova_addr;
	struct page *page;
	int npages = buffer->size >> PAGE_SHIFT;
	void *vaddr = buffer->priv_virt;
	struct scatterlist *sglist = 0;
	int prot = ION_IS_CACHED(flags) ? 1 : 0;

	if (!ION_IS_CACHED(flags))
		return -EINVAL;

	if (!msm_use_iommu())
		return -EINVAL;

	data->mapped_size = iova_length;
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


	sglist = vmalloc(sizeof(*sglist) * npages);
	if (!sglist) {
		ret = -ENOMEM;
		goto out1;
	}

	sg_init_table(sglist, npages);
	for (i = 0; i < npages; i++) {
		page = vmalloc_to_page(vaddr);
		if (!page)
			goto out1;
		sg_set_page(&sglist[i], page, PAGE_SIZE, 0);
		vaddr += PAGE_SIZE;
	}

	ret = iommu_map_range(domain, data->iova_addr, sglist,
			      buffer->size, prot);

	if (ret) {
		pr_err("%s: could not map %lx in domain %p\n",
			__func__, data->iova_addr, domain);
		goto out1;
	}

	extra_iova_addr = data->iova_addr + buffer->size;
	if (extra) {
		ret = msm_iommu_map_extra(domain, extra_iova_addr, extra, prot);
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

struct ion_heap *ion_system_heap_create(struct ion_platform_heap *unused)
{
	struct ion_heap *heap;

	heap = kzalloc(sizeof(struct ion_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->ops = &vmalloc_ops;
	heap->type = ION_HEAP_TYPE_SYSTEM;
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

struct scatterlist *ion_system_contig_heap_map_dma(struct ion_heap *heap,
						   struct ion_buffer *buffer)
{
	struct scatterlist *sglist;

	sglist = vmalloc(sizeof(struct scatterlist));
	if (!sglist)
		return ERR_PTR(-ENOMEM);
	sg_init_table(sglist, 1);
	sg_set_page(sglist, virt_to_page(buffer->priv_virt), buffer->size, 0);
	return sglist;
}

int ion_system_contig_heap_map_user(struct ion_heap *heap,
				    struct ion_buffer *buffer,
				    struct vm_area_struct *vma,
				    unsigned long flags)
{
	unsigned long pfn = __phys_to_pfn(virt_to_phys(buffer->priv_virt));

	if (ION_IS_CACHED(flags))
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
	unsigned long vstart, pstart;

	pstart = virt_to_phys(buffer->priv_virt) + offset;
	if (!pstart) {
		WARN(1, "Could not do virt to phys translation on %p\n",
			buffer->priv_virt);
		return -EINVAL;
	}

	vstart = (unsigned long) vaddr;

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

static int ion_system_contig_print_debug(struct ion_heap *heap,
					 struct seq_file *s)
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
	int prot = ION_IS_CACHED(flags) ? 1 : 0;
	struct scatterlist *sglist = 0;
	struct page *page = 0;

	if (!ION_IS_CACHED(flags))
		return -EINVAL;

	if (!msm_use_iommu()) {
		data->iova_addr = virt_to_phys(buffer->vaddr);
		return 0;
	}

	data->mapped_size = iova_length;
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
		ret = msm_iommu_map_extra(domain, extra_iova_addr, extra, prot);
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

struct ion_heap *ion_system_contig_heap_create(struct ion_platform_heap *unused)
{
	struct ion_heap *heap;

	heap = kzalloc(sizeof(struct ion_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->ops = &kmalloc_ops;
	heap->type = ION_HEAP_TYPE_SYSTEM_CONTIG;
	return heap;
}

void ion_system_contig_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}

