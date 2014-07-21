/*
 * SWIOTLB-based DMA API implementation
 *
 * Copyright (C) 2012 ARM Ltd.
 * Author: Catalin Marinas <catalin.marinas@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/gfp.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/vmalloc.h>
#include <linux/swiotlb.h>
#include <linux/sched.h>
#include <linux/io.h>

#include <asm/cacheflush.h>
#include <asm/tlbflush.h>

struct dma_map_ops *dma_ops;
EXPORT_SYMBOL(dma_ops);

#define DEFAULT_DMA_COHERENT_POOL_SIZE  SZ_256K
#define NO_KERNEL_MAPPING_DUMMY 0x2222

struct dma_pool {
	size_t size;
	spinlock_t lock;
	void *vaddr;
	unsigned long *bitmap;
	unsigned long nr_pages;
	struct page **pages;
};

static struct dma_pool atomic_pool = {
	.size = DEFAULT_DMA_COHERENT_POOL_SIZE,
};

static int __init early_coherent_pool(char *p)
{
	atomic_pool.size = memparse(p, &p);
	return 0;
}
early_param("coherent_pool", early_coherent_pool);

static void *__alloc_from_pool(size_t size, struct page **ret_page)
{
	struct dma_pool *pool = &atomic_pool;
	unsigned int count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	unsigned int pageno;
	unsigned long flags;
	void *ptr = NULL;
	unsigned long align_mask;

	if (!pool->vaddr) {
		WARN(1, "coherent pool not initialised!\n");
		return NULL;
	}

	/*
	 * Align the region allocation - allocations from pool are rather
	 * small, so align them to their order in pages, minimum is a page
	 * size. This helps reduce fragmentation of the DMA space.
	 */
	align_mask = (1 << get_order(size)) - 1;

	spin_lock_irqsave(&pool->lock, flags);
	pageno = bitmap_find_next_zero_area(pool->bitmap, pool->nr_pages,
					    0, count, align_mask);
	if (pageno < pool->nr_pages) {
		bitmap_set(pool->bitmap, pageno, count);
		ptr = pool->vaddr + PAGE_SIZE * pageno;
		*ret_page = pool->pages[pageno];
	} else {
		pr_err_once("ERROR: %u KiB atomic DMA coherent pool is too small!\n"
			    "Please increase it with coherent_pool= kernel parameter!\n",
				(unsigned)pool->size / 1024);
	}
	spin_unlock_irqrestore(&pool->lock, flags);

	return ptr;
}

static bool __in_atomic_pool(void *start, size_t size)
{
	struct dma_pool *pool = &atomic_pool;
	void *end = start + size;
	void *pool_start = pool->vaddr;
	void *pool_end = pool->vaddr + pool->size;

	if (start < pool_start || start >= pool_end)
		return false;

	if (end <= pool_end)
		return true;

	WARN(1, "Wrong coherent size(%p-%p) from atomic pool(%p-%p)\n",
		start, end - 1, pool_start, pool_end - 1);

	return false;
}

static int __free_from_pool(void *start, size_t size)
{
	struct dma_pool *pool = &atomic_pool;
	unsigned long pageno, count;
	unsigned long flags;

	if (!__in_atomic_pool(start, size))
		return 0;

	pageno = (start - pool->vaddr) >> PAGE_SHIFT;
	count = size >> PAGE_SHIFT;

	spin_lock_irqsave(&pool->lock, flags);
	bitmap_clear(pool->bitmap, pageno, count);
	spin_unlock_irqrestore(&pool->lock, flags);

	return 1;
}

static int __dma_update_pte(pte_t *pte, pgtable_t token, unsigned long addr,
			    void *data)
{
	struct page *page = virt_to_page(addr);
	pgprot_t prot = *(pgprot_t *)data;

	set_pte(pte, mk_pte(page, prot));
	return 0;
}

static int __dma_clear_pte(pte_t *pte, pgtable_t token, unsigned long addr,
			    void *data)
{
	pte_clear(&init_mm, addr, pte);
	return 0;
}

static void __dma_remap(struct page *page, size_t size, pgprot_t prot,
			bool no_kernel_map)
{
	unsigned long start = (unsigned long) page_address(page);
	unsigned end = start + size;
	int (*func)(pte_t *pte, pgtable_t token, unsigned long addr,
			    void *data);

	if (no_kernel_map)
		func = __dma_clear_pte;
	else
		func = __dma_update_pte;

	apply_to_page_range(&init_mm, start, size, func, &prot);
	mb();
	flush_tlb_kernel_range(start, end);
}



static void *arm64_swiotlb_alloc_coherent(struct device *dev, size_t size,
					  dma_addr_t *dma_handle, gfp_t flags,
					  struct dma_attrs *attrs)
{
	if (dev == NULL) {
		WARN_ONCE(1, "Use an actual device structure for DMA allocation\n");
		return NULL;
	}

	if (IS_ENABLED(CONFIG_ZONE_DMA32) &&
	    dev->coherent_dma_mask <= DMA_BIT_MASK(32))
		flags |= GFP_DMA32;

	if (!(flags & __GFP_WAIT)) {
		struct page *page = NULL;
		void *addr = __alloc_from_pool(size, &page);

		if (addr)
			*dma_handle = phys_to_dma(dev, page_to_phys(page));

		return addr;
	} else if (IS_ENABLED(CONFIG_CMA)) {
		unsigned long pfn;
		struct page *page;
		void *addr;

		size = PAGE_ALIGN(size);
		pfn = dma_alloc_from_contiguous(dev, size >> PAGE_SHIFT,
							get_order(size));
		if (!pfn)
			return NULL;

		page = pfn_to_page(pfn);
		addr = page_address(page);

		if (dma_get_attr(DMA_ATTR_NO_KERNEL_MAPPING, attrs)) {
			/*
			 * flush the caches here because we can't do it later
			 */
			__dma_flush_range(addr, addr + size);
			__dma_remap(page, size, 0, true);
		}

		*dma_handle = phys_to_dma(dev, __pfn_to_phys(pfn));
		return addr;
	} else {
		return swiotlb_alloc_coherent(dev, size, dma_handle, flags);
	}
}

static void arm64_swiotlb_free_coherent(struct device *dev, size_t size,
					void *vaddr, dma_addr_t dma_handle,
					struct dma_attrs *attrs)
{
	if (dev == NULL) {
		WARN_ONCE(1, "Use an actual device structure for DMA allocation\n");
		return;
	}

	size = PAGE_ALIGN(size);

	if (__free_from_pool(vaddr, size)) {
		return;
	} else if (IS_ENABLED(CONFIG_CMA)) {
		phys_addr_t paddr = dma_to_phys(dev, dma_handle);

		if (dma_get_attr(DMA_ATTR_NO_KERNEL_MAPPING, attrs))
			__dma_remap(phys_to_page(paddr), size, PAGE_KERNEL,
					false);

		dma_release_from_contiguous(dev,
					__phys_to_pfn(paddr),
					size >> PAGE_SHIFT);
	} else {
		swiotlb_free_coherent(dev, size, vaddr, dma_handle);
	}
}

static pgprot_t __get_dma_pgprot(pgprot_t prot, struct dma_attrs *attrs)
{
	if (dma_get_attr(DMA_ATTR_WRITE_COMBINE, attrs))
		prot = pgprot_writecombine(prot);
	else if (dma_get_attr(DMA_ATTR_STRONGLY_ORDERED, attrs))
		prot = pgprot_noncached(prot);
	/* if non-consistent just pass back what was given */
	else if (!dma_get_attr(DMA_ATTR_NON_CONSISTENT, attrs))
		prot = pgprot_dmacoherent(prot);

	return prot;
}

static void *arm64_swiotlb_alloc_noncoherent(struct device *dev, size_t size,
					     dma_addr_t *dma_handle, gfp_t flags,
					     struct dma_attrs *attrs)
{
	struct page *page, **map;
	void *ptr, *coherent_ptr;
	int order, i;
	pgprot_t prot = __get_dma_pgprot(pgprot_default, attrs);

	size = PAGE_ALIGN(size);
	order = get_order(size);

	ptr = arm64_swiotlb_alloc_coherent(dev, size, dma_handle, flags, attrs);
	if (!ptr)
		goto no_mem;

	if (!(flags & __GFP_WAIT))
		return ptr;

	if (dma_get_attr(DMA_ATTR_NO_KERNEL_MAPPING, attrs)) {
		coherent_ptr = (void *)NO_KERNEL_MAPPING_DUMMY;

	} else {
		/* remove any dirty cache lines on the kernel alias */
		__dma_flush_range(ptr, ptr + size);

		map = kmalloc(sizeof(struct page *) << order, flags & ~GFP_DMA);
		if (!map)
			goto no_map;

		/* create a coherent mapping */
		page = virt_to_page(ptr);
		for (i = 0; i < (size >> PAGE_SHIFT); i++)
			map[i] = page + i;
		coherent_ptr = vmap(map, size >> PAGE_SHIFT, VM_MAP, prot);
		kfree(map);
		if (!coherent_ptr)
			goto no_map;
	}

	return coherent_ptr;

no_map:
	swiotlb_free_coherent(dev, size, ptr, *dma_handle);
no_mem:
	*dma_handle = ~0;
	return NULL;
}

static void arm64_swiotlb_free_noncoherent(struct device *dev, size_t size,
					   void *vaddr, dma_addr_t dma_handle,
					   struct dma_attrs *attrs)
{
	void *swiotlb_addr = phys_to_virt(dma_to_phys(dev, dma_handle));

	size = PAGE_ALIGN(size);

	if (__free_from_pool(vaddr, size))
		return;
	if (!dma_get_attr(DMA_ATTR_NO_KERNEL_MAPPING, attrs))
		vunmap(vaddr);
	arm64_swiotlb_free_coherent(dev, size, swiotlb_addr, dma_handle, attrs);
}

static dma_addr_t arm64_swiotlb_map_page(struct device *dev,
					 struct page *page,
					 unsigned long offset, size_t size,
					 enum dma_data_direction dir,
					 struct dma_attrs *attrs)
{
	dma_addr_t dev_addr;

	dev_addr = swiotlb_map_page(dev, page, offset, size, dir, attrs);
	__dma_map_area(phys_to_virt(dma_to_phys(dev, dev_addr)), size, dir);

	return dev_addr;
}


static void arm64_swiotlb_unmap_page(struct device *dev, dma_addr_t dev_addr,
				     size_t size, enum dma_data_direction dir,
				     struct dma_attrs *attrs)
{
	__dma_unmap_area(phys_to_virt(dma_to_phys(dev, dev_addr)), size, dir);
	swiotlb_unmap_page(dev, dev_addr, size, dir, attrs);
}

static int arm64_swiotlb_map_sg_attrs(struct device *dev,
				      struct scatterlist *sgl, int nelems,
				      enum dma_data_direction dir,
				      struct dma_attrs *attrs)
{
	struct scatterlist *sg;
	int i, ret;

	ret = swiotlb_map_sg_attrs(dev, sgl, nelems, dir, attrs);
	for_each_sg(sgl, sg, ret, i)
		__dma_map_area(phys_to_virt(dma_to_phys(dev, sg->dma_address)),
			       sg->length, dir);

	return ret;
}

static void arm64_swiotlb_unmap_sg_attrs(struct device *dev,
					 struct scatterlist *sgl, int nelems,
					 enum dma_data_direction dir,
					 struct dma_attrs *attrs)
{
	struct scatterlist *sg;
	int i;

	for_each_sg(sgl, sg, nelems, i)
		__dma_unmap_area(phys_to_virt(dma_to_phys(dev, sg->dma_address)),
				 sg->length, dir);
	swiotlb_unmap_sg_attrs(dev, sgl, nelems, dir, attrs);
}

static void arm64_swiotlb_sync_single_for_cpu(struct device *dev,
					      dma_addr_t dev_addr,
					      size_t size,
					      enum dma_data_direction dir)
{
	__dma_unmap_area(phys_to_virt(dma_to_phys(dev, dev_addr)), size, dir);
	swiotlb_sync_single_for_cpu(dev, dev_addr, size, dir);
}

static void arm64_swiotlb_sync_single_for_device(struct device *dev,
						 dma_addr_t dev_addr,
						 size_t size,
						 enum dma_data_direction dir)
{
	swiotlb_sync_single_for_device(dev, dev_addr, size, dir);
	__dma_map_area(phys_to_virt(dma_to_phys(dev, dev_addr)), size, dir);
}

static void arm64_swiotlb_sync_sg_for_cpu(struct device *dev,
					  struct scatterlist *sgl, int nelems,
					  enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	for_each_sg(sgl, sg, nelems, i)
		__dma_unmap_area(phys_to_virt(dma_to_phys(dev, sg->dma_address)),
				 sg->length, dir);
	swiotlb_sync_sg_for_cpu(dev, sgl, nelems, dir);
}

static void arm64_swiotlb_sync_sg_for_device(struct device *dev,
					     struct scatterlist *sgl,
					     int nelems,
					     enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	swiotlb_sync_sg_for_device(dev, sgl, nelems, dir);
	for_each_sg(sgl, sg, nelems, i)
		__dma_map_area(phys_to_virt(dma_to_phys(dev, sg->dma_address)),
			       sg->length, dir);
}

int arm64_swiotlb_mmap(struct device *dev, struct vm_area_struct *vma,
		 void *cpu_addr, dma_addr_t dma_addr, size_t size,
		 struct dma_attrs *attrs)
{
	int ret = -ENXIO;
	unsigned long nr_vma_pages = (vma->vm_end - vma->vm_start) >>
					PAGE_SHIFT;
	unsigned long nr_pages = PAGE_ALIGN(size) >> PAGE_SHIFT;
	unsigned long pfn = dma_to_phys(dev, dma_addr) >> PAGE_SHIFT;
	unsigned long off = vma->vm_pgoff;

	vma->vm_page_prot = __get_dma_pgprot(vma->vm_page_prot, attrs);

	if (dma_mmap_from_coherent(dev, vma, cpu_addr, size, &ret))
		return ret;

	if (off < nr_pages && nr_vma_pages <= (nr_pages - off)) {
		ret = remap_pfn_range(vma, vma->vm_start,
				      pfn + off,
				      vma->vm_end - vma->vm_start,
				      vma->vm_page_prot);
	}

	return ret;
}

static void *arm64_dma_remap(struct device *dev, void *cpu_addr,
			dma_addr_t handle, size_t size,
			struct dma_attrs *attrs)
{
	struct page *page = phys_to_page(dma_to_phys(dev, handle));
	pgprot_t prot = __get_dma_pgprot(PAGE_KERNEL, attrs);
	unsigned long offset = handle & ~PAGE_MASK;
	struct vm_struct *area;
	unsigned long addr;

	size = PAGE_ALIGN(size + offset);

	/*
	 * DMA allocation can be mapped to user space, so lets
	 * set VM_USERMAP flags too.
	 */
	area = get_vm_area(size, VM_USERMAP);
	if (!area)
		return NULL;

	addr = (unsigned long)area->addr;
	area->phys_addr = __pfn_to_phys(page_to_pfn(page));

	if (ioremap_page_range(addr, addr + size, area->phys_addr, prot)) {
		vunmap((void *)addr);
		return NULL;
	}
	return (void *)addr + offset;
}

static void arm64_dma_unremap(struct device *dev, void *remapped_addr,
				size_t size)
{
	struct vm_struct *area;

	remapped_addr = (void *)((unsigned long)remapped_addr & PAGE_MASK);

	area = find_vm_area(remapped_addr);
	if (!area) {
		WARN(1, "trying to free invalid coherent area: %p\n",
			remapped_addr);
		return;
	}
	vunmap(remapped_addr);
}

struct dma_map_ops noncoherent_swiotlb_dma_ops = {
	.alloc = arm64_swiotlb_alloc_noncoherent,
	.free = arm64_swiotlb_free_noncoherent,
	.mmap = arm64_swiotlb_mmap,
	.map_page = arm64_swiotlb_map_page,
	.unmap_page = arm64_swiotlb_unmap_page,
	.map_sg = arm64_swiotlb_map_sg_attrs,
	.unmap_sg = arm64_swiotlb_unmap_sg_attrs,
	.sync_single_for_cpu = arm64_swiotlb_sync_single_for_cpu,
	.sync_single_for_device = arm64_swiotlb_sync_single_for_device,
	.sync_sg_for_cpu = arm64_swiotlb_sync_sg_for_cpu,
	.sync_sg_for_device = arm64_swiotlb_sync_sg_for_device,
	.dma_supported = swiotlb_dma_supported,
	.mapping_error = swiotlb_dma_mapping_error,
	.remap = arm64_dma_remap,
	.unremap = arm64_dma_unremap,
};
EXPORT_SYMBOL(noncoherent_swiotlb_dma_ops);

struct dma_map_ops coherent_swiotlb_dma_ops = {
	.alloc = arm64_swiotlb_alloc_coherent,
	.free = arm64_swiotlb_free_coherent,
	.map_page = swiotlb_map_page,
	.unmap_page = swiotlb_unmap_page,
	.map_sg = swiotlb_map_sg_attrs,
	.unmap_sg = swiotlb_unmap_sg_attrs,
	.sync_single_for_cpu = swiotlb_sync_single_for_cpu,
	.sync_single_for_device = swiotlb_sync_single_for_device,
	.sync_sg_for_cpu = swiotlb_sync_sg_for_cpu,
	.sync_sg_for_device = swiotlb_sync_sg_for_device,
	.dma_supported = swiotlb_dma_supported,
	.mapping_error = swiotlb_dma_mapping_error,
	.remap = arm64_dma_remap,
	.unremap = arm64_dma_unremap,
};
EXPORT_SYMBOL(coherent_swiotlb_dma_ops);

static int __init atomic_pool_init(void)
{
	struct dma_pool *pool = &atomic_pool;
	pgprot_t prot = pgprot_dmacoherent(PAGE_KERNEL);
	unsigned long nr_pages = pool->size >> PAGE_SHIFT;
	unsigned long *bitmap;
	unsigned long pfn = 0;
	struct page *page;
	struct page **pages;
	int bitmap_size = BITS_TO_LONGS(nr_pages) * sizeof(long);


	if (!IS_ENABLED(CONFIG_CMA))
		return 0;

	bitmap = kzalloc(bitmap_size, GFP_KERNEL);
	if (!bitmap)
		goto no_bitmap;

	pages = kzalloc(nr_pages * sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		goto no_pages;

	if (IS_ENABLED(CONFIG_CMA))
		pfn = dma_alloc_from_contiguous(NULL, nr_pages,
					get_order(pool->size));

	if (pfn) {
		int i;
		page = pfn_to_page(pfn);

		for (i = 0; i < nr_pages; i++)
			pages[i] = page + i;

		spin_lock_init(&pool->lock);
		pool->pages = pages;
		pool->vaddr = vmap(pages, nr_pages, VM_MAP, prot);
		pool->bitmap = bitmap;
		pool->nr_pages = nr_pages;
		pr_info("DMA: preallocated %u KiB pool for atomic allocations\n",
			(unsigned)pool->size / 1024);
		return 0;
	}

	kfree(pages);
no_pages:
	kfree(bitmap);
no_bitmap:
	pr_err("DMA: failed to allocate %u KiB pool for atomic coherent allocation\n",
		(unsigned)pool->size / 1024);
	return -ENOMEM;
}
postcore_initcall(atomic_pool_init);

void __init arm64_swiotlb_init(void)
{
	dma_ops = &noncoherent_swiotlb_dma_ops;
	swiotlb_init(1);
}

#define PREALLOC_DMA_DEBUG_ENTRIES	4096

static int __init dma_debug_do_init(void)
{
	dma_debug_init(PREALLOC_DMA_DEBUG_ENTRIES);
	return 0;
}
fs_initcall(dma_debug_do_init);
