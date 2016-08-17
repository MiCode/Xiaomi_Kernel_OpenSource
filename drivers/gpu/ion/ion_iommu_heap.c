/*
 * drivers/gpu/ion/ion_iommu_heap.c
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define pr_fmt(fmt)	"%s(): " fmt, __func__

#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/ion.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/iommu.h>
#include <linux/highmem.h>
#include <linux/platform_device.h>

#include <asm/cacheflush.h>

#include "ion_priv.h"

#define NUM_PAGES(buf)	(PAGE_ALIGN((buf)->size) >> PAGE_SHIFT)

#define GFP_ION		(GFP_KERNEL | __GFP_HIGHMEM | __GFP_NOWARN)

struct ion_iommu_heap {
	struct ion_heap		heap;
	struct gen_pool		*pool;
	struct iommu_domain	*domain;
	struct device		*dev;
};

static struct scatterlist *iommu_heap_map_dma(struct ion_heap *heap,
					      struct ion_buffer *buf)
{
	struct ion_iommu_heap *h =
		container_of(heap, struct ion_iommu_heap, heap);
	int err, npages = NUM_PAGES(buf);
	unsigned int i;
	struct scatterlist *sg;
	unsigned long da = (unsigned long)buf->priv_virt;

	for_each_sg(buf->sglist, sg, npages, i) {
		phys_addr_t pa;

		pa = sg_phys(sg);
		BUG_ON(!IS_ALIGNED(sg->length, PAGE_SIZE));
		err = iommu_map(h->domain, da, pa, PAGE_SIZE, 0);
		if (err)
			goto err_out;

		sg->dma_address = da;
		da += PAGE_SIZE;
	}

	pr_debug("da:%p pa:%08x va:%p\n",
		 buf->priv_virt, sg_phys(buf->sglist), buf->vaddr);

	return buf->sglist;

err_out:
	if (i-- > 0) {
		unsigned int j;
		for_each_sg(buf->sglist, sg, i, j)
			iommu_unmap(h->domain, sg_dma_address(sg), 0);
	}
	return ERR_PTR(err);
}

static void iommu_heap_unmap_dma(struct ion_heap *heap, struct ion_buffer *buf)
{
	struct ion_iommu_heap *h =
		container_of(heap, struct ion_iommu_heap, heap);
	unsigned int i;
	struct scatterlist *sg;
	int npages = NUM_PAGES(buf);

	for_each_sg(buf->sglist, sg, npages, i)
		iommu_unmap(h->domain, sg_dma_address(sg), 0);

	pr_debug("da:%p\n", buf->priv_virt);
}

struct scatterlist *iommu_heap_remap_dma(struct ion_heap *heap,
					      struct ion_buffer *buf,
					      unsigned long addr)
{
	struct ion_iommu_heap *h =
		container_of(heap, struct ion_iommu_heap, heap);
	int err;
	unsigned int i;
	unsigned long da, da_to_free = (unsigned long)buf->priv_virt;
	int npages = NUM_PAGES(buf);

	BUG_ON(!buf->priv_virt);

	da = gen_pool_alloc_addr(h->pool, buf->size, addr);
	if (da == 0) {
		pr_err("dma address alloc failed, addr=0x%lx", addr);
		return ERR_PTR(-ENOMEM);
	} else {
		pr_err("iommu_heap_remap_dma passed, addr=0x%lx",
			addr);
		iommu_heap_unmap_dma(heap, buf);
		gen_pool_free(h->pool, da_to_free, buf->size);
		buf->priv_virt = (void *)da;
	}
	for (i = 0; i < npages; i++) {
		phys_addr_t pa;

		pa = page_to_phys(buf->pages[i]);
		err = iommu_map(h->domain, da, pa, 0, 0);
		if (err)
			goto err_out;
		da += PAGE_SIZE;
	}

	pr_debug("da:%p pa:%08x va:%p\n",
		 buf->priv_virt, page_to_phys(buf->pages[0]), buf->vaddr);

	return (struct scatterlist *)buf->pages;

err_out:
	if (i-- > 0) {
		da = (unsigned long)buf->priv_virt;
		iommu_unmap(h->domain, da + (i << PAGE_SHIFT), 0);
	}
	return ERR_PTR(err);
}

static int ion_buffer_allocate(struct ion_buffer *buf)
{
	int i, npages = NUM_PAGES(buf);

	buf->pages = kmalloc(npages * sizeof(*buf->pages), GFP_KERNEL);
	if (!buf->pages)
		goto err_pages;

	buf->sglist = vzalloc(npages * sizeof(*buf->sglist));
	if (!buf->sglist)
		goto err_sgl;

	sg_init_table(buf->sglist, npages);

	for (i = 0; i < npages; i++) {
		struct page *page;
		phys_addr_t pa;

		page = alloc_page(GFP_ION);
		if (!page)
			goto err_pgalloc;
		pa = page_to_phys(page);

		sg_set_page(&buf->sglist[i], page, PAGE_SIZE, 0);

		flush_dcache_page(page);
		outer_flush_range(pa, pa + PAGE_SIZE);

		buf->pages[i] = page;

		pr_debug_once("pa:%08x\n", pa);
	}
	return 0;

err_pgalloc:
	while (i-- > 0)
		__free_page(buf->pages[i]);
	vfree(buf->sglist);
err_sgl:
	kfree(buf->pages);
err_pages:
	return -ENOMEM;
}

static void ion_buffer_free(struct ion_buffer *buf)
{
	int i, npages = NUM_PAGES(buf);

	for (i = 0; i < npages; i++)
		__free_page(buf->pages[i]);
	vfree(buf->sglist);
	kfree(buf->pages);
}

static int iommu_heap_allocate(struct ion_heap *heap, struct ion_buffer *buf,
			       unsigned long len, unsigned long align,
			       unsigned long flags)
{
	int err;
	struct ion_iommu_heap *h =
		container_of(heap, struct ion_iommu_heap, heap);
	unsigned long da;
	struct scatterlist *sgl;

	len = round_up(len, PAGE_SIZE);

	da = gen_pool_alloc(h->pool, len);
	if (!da)
		return -ENOMEM;

	buf->priv_virt = (void *)da;
	buf->size = len;

	WARN_ON(!IS_ALIGNED(da, PAGE_SIZE));

	err = ion_buffer_allocate(buf);
	if (err)
		goto err_alloc_buf;

	sgl = iommu_heap_map_dma(heap, buf);
	if (IS_ERR_OR_NULL(sgl))
		goto err_heap_map_dma;
	buf->vaddr = 0;
	return 0;

err_heap_map_dma:
	ion_buffer_free(buf);
err_alloc_buf:
	gen_pool_free(h->pool, da, len);
	buf->size = 0;
	buf->pages = NULL;
	buf->priv_virt = NULL;
	return err;
}

static void iommu_heap_free(struct ion_buffer *buf)
{
	struct ion_heap *heap = buf->heap;
	struct ion_iommu_heap *h =
		container_of(heap, struct ion_iommu_heap, heap);
	void *da = buf->priv_virt;

	iommu_heap_unmap_dma(heap, buf);
	ion_buffer_free(buf);
	gen_pool_free(h->pool, (unsigned long)da, buf->size);

	buf->pages = NULL;
	buf->priv_virt = NULL;
	pr_debug("da:%p\n", da);
}

static int iommu_heap_phys(struct ion_heap *heap, struct ion_buffer *buf,
			   ion_phys_addr_t *addr, size_t *len)
{
	*addr = (unsigned long)buf->priv_virt;
	*len = buf->size;
	pr_debug("da:%08lx(%x)\n", *addr, *len);
	return 0;
}

static void *iommu_heap_map_kernel(struct ion_heap *heap,
				   struct ion_buffer *buf)
{
	int npages = NUM_PAGES(buf);

	BUG_ON(!buf->pages);
	buf->vaddr = vm_map_ram(buf->pages, npages, -1,
				pgprot_noncached(pgprot_kernel));
	pr_debug("va:%p\n", buf->vaddr);
	WARN_ON(!buf->vaddr);
	return buf->vaddr;
}

static void iommu_heap_unmap_kernel(struct ion_heap *heap,
				    struct ion_buffer *buf)
{
	int npages = NUM_PAGES(buf);

	BUG_ON(!buf->pages);
	WARN_ON(!buf->vaddr);
	vm_unmap_ram(buf->vaddr, npages);
	buf->vaddr = NULL;
	pr_debug("va:%p\n", buf->vaddr);
}

static int iommu_heap_map_user(struct ion_heap *mapper,
			       struct ion_buffer *buf,
			       struct vm_area_struct *vma)
{
	int i = vma->vm_pgoff >> PAGE_SHIFT;
	unsigned long uaddr = vma->vm_start;
	unsigned long usize = vma->vm_end - vma->vm_start;

	pr_debug("vma:%08lx-%08lx\n", vma->vm_start, vma->vm_end);
	BUG_ON(!buf->pages);

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	do {
		int ret;
		struct page *page = buf->pages[i++];

		ret = vm_insert_page(vma, uaddr, page);
		if (ret)
			return ret;

		uaddr += PAGE_SIZE;
		usize -= PAGE_SIZE;
	} while (usize > 0);

	return 0;
}

static struct ion_heap_ops iommu_heap_ops = {
	.allocate	= iommu_heap_allocate,
	.free		= iommu_heap_free,
	.phys		= iommu_heap_phys,
	.map_dma	= iommu_heap_map_dma,
	.unmap_dma	= iommu_heap_unmap_dma,
	.map_kernel	= iommu_heap_map_kernel,
	.unmap_kernel	= iommu_heap_unmap_kernel,
	.map_user	= iommu_heap_map_user,
};

struct ion_heap *ion_iommu_heap_create(struct ion_platform_heap *data)
{
	struct ion_iommu_heap *h;
	int err;

	h = kzalloc(sizeof(*h), GFP_KERNEL);
	if (!h) {
		err = -ENOMEM;
		goto err_heap;
	}

	h->pool = gen_pool_create(12, -1);
	if (!h->pool) {
		err = -ENOMEM;
		goto err_genpool;
	}
	gen_pool_add(h->pool, data->base, data->size, -1);

	h->heap.ops = &iommu_heap_ops;
	h->domain = iommu_domain_alloc(&platform_bus_type);
	h->dev = data->priv;
	if (!h->domain) {
		err = -ENOMEM;
		goto err_iommu_alloc;
	}

	err = iommu_attach_device(h->domain, h->dev);
	if (err)
		goto err_iommu_attach;

	return &h->heap;

err_iommu_attach:
	iommu_domain_free(h->domain);
err_iommu_alloc:
	gen_pool_destroy(h->pool);
err_genpool:
	kfree(h);
err_heap:
	return ERR_PTR(err);
}

void ion_iommu_heap_destroy(struct ion_heap *heap)
{
	struct ion_iommu_heap *h =
		container_of(heap, struct  ion_iommu_heap, heap);

	iommu_detach_device(h->domain, h->dev);
	gen_pool_destroy(h->pool);
	iommu_domain_free(h->domain);
	kfree(h);
}
