/*
 * SWIOTLB-based DMA API implementation
 *
 * Copyright (C) 2012 ARM Ltd.
 * Author: Catalin Marinas <catalin.marinas@arm.com>
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#include <linux/acpi.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/genalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/mm.h>
#include <linux/iommu.h>
#include <linux/vmalloc.h>
#include <linux/swiotlb.h>

#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <linux/io.h>
#include <asm/dma-iommu.h>
#include <linux/dma-mapping-fast.h>
#include <linux/msm_dma_iommu_mapping.h>

#include "mm.h"


static pgprot_t __get_dma_pgprot(struct dma_attrs *attrs, pgprot_t prot,
				 bool coherent)
{
	if (dma_get_attr(DMA_ATTR_STRONGLY_ORDERED, attrs))
		return pgprot_noncached(prot);
	else if (!coherent || dma_get_attr(DMA_ATTR_WRITE_COMBINE, attrs))
		return pgprot_writecombine(prot);
	return prot;
}

static bool is_dma_coherent(struct device *dev, struct dma_attrs *attrs)
{
	bool is_coherent;

	if (dma_get_attr(DMA_ATTR_FORCE_COHERENT, attrs))
		is_coherent = true;
	else if (dma_get_attr(DMA_ATTR_FORCE_NON_COHERENT, attrs))
		is_coherent = false;
	else if (is_device_dma_coherent(dev))
		is_coherent = true;
	else
		is_coherent = false;

	return is_coherent;
}

static struct gen_pool *atomic_pool;
#define NO_KERNEL_MAPPING_DUMMY 0x2222
#define DEFAULT_DMA_COHERENT_POOL_SIZE  SZ_256K
static size_t atomic_pool_size __initdata = DEFAULT_DMA_COHERENT_POOL_SIZE;

static int __init early_coherent_pool(char *p)
{
	atomic_pool_size = memparse(p, &p);
	return 0;
}
early_param("coherent_pool", early_coherent_pool);

static void *__alloc_from_pool(size_t size, struct page **ret_page, gfp_t flags)
{
	unsigned long val;
	void *ptr = NULL;

	if (!atomic_pool) {
		WARN(1, "coherent pool not initialised!\n");
		return NULL;
	}

	val = gen_pool_alloc(atomic_pool, size);
	if (val) {
		phys_addr_t phys = gen_pool_virt_to_phys(atomic_pool, val);

		*ret_page = phys_to_page(phys);
		ptr = (void *)val;
		memset(ptr, 0, size);
	}

	return ptr;
}

static bool __in_atomic_pool(void *start, size_t size)
{
	return addr_in_gen_pool(atomic_pool, (unsigned long)start, size);
}

static int __free_from_pool(void *start, size_t size)
{
	if (!__in_atomic_pool(start, size))
		return 0;

	gen_pool_free(atomic_pool, (unsigned long)start, size);

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

static void *__dma_alloc_coherent(struct device *dev, size_t size,
				  dma_addr_t *dma_handle, gfp_t flags,
				  struct dma_attrs *attrs)
{
	void *addr;

	if (dev == NULL) {
		WARN_ONCE(1, "Use an actual device structure for DMA allocation\n");
		return NULL;
	}

	if (IS_ENABLED(CONFIG_ZONE_DMA) &&
	    dev->coherent_dma_mask <= DMA_BIT_MASK(32))
		flags |= GFP_DMA;
	if (dev_get_cma_area(dev) && gfpflags_allow_blocking(flags)) {
		struct page *page;

		page = dma_alloc_from_contiguous(dev, size >> PAGE_SHIFT,
							get_order(size));
		if (!page)
			return NULL;

		*dma_handle = phys_to_dma(dev, page_to_phys(page));
		addr = page_address(page);
		memset(addr, 0, size);
	} else {
		addr = swiotlb_alloc_coherent(dev, size, dma_handle, flags);
	}

	if (addr && (dma_get_attr(DMA_ATTR_NO_KERNEL_MAPPING, attrs) ||
		dma_get_attr(DMA_ATTR_STRONGLY_ORDERED, attrs))) {
		/*
		 * flush the caches here because we can't later
		 */
		__dma_flush_range(addr, addr + size);
		__dma_remap(virt_to_page(addr), size, 0, true);
	}

	return addr;
}

static void __dma_free_coherent(struct device *dev, size_t size,
				void *vaddr, dma_addr_t dma_handle,
				struct dma_attrs *attrs)
{
	bool freed;
	phys_addr_t paddr = dma_to_phys(dev, dma_handle);

	size = PAGE_ALIGN(size);
	if (dev == NULL) {
		WARN_ONCE(1, "Use an actual device structure for DMA allocation\n");
		return;
	}

	if (dma_get_attr(DMA_ATTR_NO_KERNEL_MAPPING, attrs) ||
	    dma_get_attr(DMA_ATTR_STRONGLY_ORDERED, attrs))
		__dma_remap(phys_to_page(paddr), size, PAGE_KERNEL, false);

	freed = dma_release_from_contiguous(dev,
					phys_to_page(paddr),
					size >> PAGE_SHIFT);
	if (!freed)
		swiotlb_free_coherent(dev, size, vaddr, dma_handle);
}

static void *__dma_alloc(struct device *dev, size_t size,
			 dma_addr_t *dma_handle, gfp_t flags,
			 struct dma_attrs *attrs)
{
	struct page *page;
	void *ptr, *coherent_ptr;
	bool coherent = is_dma_coherent(dev, attrs);

	size = PAGE_ALIGN(size);

	if (!coherent && !gfpflags_allow_blocking(flags)) {
		struct page *page = NULL;
		void *addr = __alloc_from_pool(size, &page, flags);

		if (addr)
			*dma_handle = phys_to_dma(dev, page_to_phys(page));

		return addr;
	}

	ptr = __dma_alloc_coherent(dev, size, dma_handle, flags, attrs);
	if (!ptr)
		goto no_mem;

	/* no need for non-cacheable mapping if coherent */
	if (coherent)
		return ptr;

	if (dma_get_attr(DMA_ATTR_NO_KERNEL_MAPPING, attrs)) {
		coherent_ptr = (void *)NO_KERNEL_MAPPING_DUMMY;
	} else {
		if (!dma_get_attr(DMA_ATTR_STRONGLY_ORDERED, attrs))
			/* remove any dirty cache lines on the kernel alias */
			__dma_flush_range(ptr, ptr + size);

		/* create a coherent mapping */
		page = virt_to_page(ptr);
		coherent_ptr = dma_common_contiguous_remap(page, size, VM_USERMAP,
					__get_dma_pgprot(attrs,
						__pgprot(PROT_NORMAL_NC), false),
						NULL);
		if (!coherent_ptr)
			goto no_map;
	}
	return coherent_ptr;

no_map:
	__dma_free_coherent(dev, size, ptr, *dma_handle, attrs);
no_mem:
	*dma_handle = DMA_ERROR_CODE;
	return NULL;
}

static void __dma_free(struct device *dev, size_t size,
		       void *vaddr, dma_addr_t dma_handle,
		       struct dma_attrs *attrs)
{
	void *swiotlb_addr = phys_to_virt(dma_to_phys(dev, dma_handle));

	size = PAGE_ALIGN(size);

	if (!is_dma_coherent(dev, attrs)) {
		if (__free_from_pool(vaddr, size))
			return;
		if (!dma_get_attr(DMA_ATTR_NO_KERNEL_MAPPING, attrs))
			vunmap(vaddr);
	}
	__dma_free_coherent(dev, size, swiotlb_addr, dma_handle, attrs);
}

static dma_addr_t __swiotlb_map_page(struct device *dev, struct page *page,
				     unsigned long offset, size_t size,
				     enum dma_data_direction dir,
				     struct dma_attrs *attrs)
{
	dma_addr_t dev_addr;

	dev_addr = swiotlb_map_page(dev, page, offset, size, dir, attrs);
	if (!is_dma_coherent(dev, attrs))
		__dma_map_area(phys_to_virt(dma_to_phys(dev, dev_addr)), size, dir);

	return dev_addr;
}


static void __swiotlb_unmap_page(struct device *dev, dma_addr_t dev_addr,
				 size_t size, enum dma_data_direction dir,
				 struct dma_attrs *attrs)
{
	if (!is_dma_coherent(dev, attrs))
		__dma_unmap_area(phys_to_virt(dma_to_phys(dev, dev_addr)), size, dir);
	swiotlb_unmap_page(dev, dev_addr, size, dir, attrs);
}

static int __swiotlb_map_sg_attrs(struct device *dev, struct scatterlist *sgl,
				  int nelems, enum dma_data_direction dir,
				  struct dma_attrs *attrs)
{
	struct scatterlist *sg;
	int i, ret;

	ret = swiotlb_map_sg_attrs(dev, sgl, nelems, dir, attrs);
	if (!is_dma_coherent(dev, attrs))
		for_each_sg(sgl, sg, ret, i)
			__dma_map_area(phys_to_virt(dma_to_phys(dev, sg->dma_address)),
				       sg->length, dir);

	return ret;
}

static void __swiotlb_unmap_sg_attrs(struct device *dev,
				     struct scatterlist *sgl, int nelems,
				     enum dma_data_direction dir,
				     struct dma_attrs *attrs)
{
	struct scatterlist *sg;
	int i;

	if (!is_dma_coherent(dev, attrs))
		for_each_sg(sgl, sg, nelems, i)
			__dma_unmap_area(phys_to_virt(dma_to_phys(dev, sg->dma_address)),
					 sg->length, dir);
	swiotlb_unmap_sg_attrs(dev, sgl, nelems, dir, attrs);
}

static void __swiotlb_sync_single_for_cpu(struct device *dev,
					  dma_addr_t dev_addr, size_t size,
					  enum dma_data_direction dir)
{
	if (!is_device_dma_coherent(dev))
		__dma_unmap_area(phys_to_virt(dma_to_phys(dev, dev_addr)), size, dir);
	swiotlb_sync_single_for_cpu(dev, dev_addr, size, dir);
}

static void __swiotlb_sync_single_for_device(struct device *dev,
					     dma_addr_t dev_addr, size_t size,
					     enum dma_data_direction dir)
{
	swiotlb_sync_single_for_device(dev, dev_addr, size, dir);
	if (!is_device_dma_coherent(dev))
		__dma_map_area(phys_to_virt(dma_to_phys(dev, dev_addr)), size, dir);
}

static void __swiotlb_sync_sg_for_cpu(struct device *dev,
				      struct scatterlist *sgl, int nelems,
				      enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	if (!is_device_dma_coherent(dev))
		for_each_sg(sgl, sg, nelems, i)
			__dma_unmap_area(phys_to_virt(dma_to_phys(dev, sg->dma_address)),
					 sg->length, dir);
	swiotlb_sync_sg_for_cpu(dev, sgl, nelems, dir);
}

static void __swiotlb_sync_sg_for_device(struct device *dev,
					 struct scatterlist *sgl, int nelems,
					 enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	swiotlb_sync_sg_for_device(dev, sgl, nelems, dir);
	if (!is_device_dma_coherent(dev))
		for_each_sg(sgl, sg, nelems, i)
			__dma_map_area(phys_to_virt(dma_to_phys(dev, sg->dma_address)),
				       sg->length, dir);
}

static int __swiotlb_mmap(struct device *dev,
			  struct vm_area_struct *vma,
			  void *cpu_addr, dma_addr_t dma_addr, size_t size,
			  struct dma_attrs *attrs)
{
	int ret = -ENXIO;
	unsigned long nr_vma_pages = (vma->vm_end - vma->vm_start) >>
					PAGE_SHIFT;
	unsigned long nr_pages = PAGE_ALIGN(size) >> PAGE_SHIFT;
	unsigned long pfn = dma_to_phys(dev, dma_addr) >> PAGE_SHIFT;
	unsigned long off = vma->vm_pgoff;

	vma->vm_page_prot = __get_dma_pgprot(attrs, vma->vm_page_prot,
					     is_dma_coherent(dev, attrs));

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

static int __swiotlb_get_sgtable(struct device *dev, struct sg_table *sgt,
				 void *cpu_addr, dma_addr_t handle, size_t size,
				 struct dma_attrs *attrs)
{
	int ret = sg_alloc_table(sgt, 1, GFP_KERNEL);

	if (!ret)
		sg_set_page(sgt->sgl, phys_to_page(dma_to_phys(dev, handle)),
			    PAGE_ALIGN(size), 0);

	return ret;
}

static void *arm64_dma_remap(struct device *dev, void *cpu_addr,
			dma_addr_t handle, size_t size,
			struct dma_attrs *attrs)
{
	struct page *page = phys_to_page(dma_to_phys(dev, handle));
	pgprot_t prot = __get_dma_pgprot(attrs, PAGE_KERNEL, false);
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

	size = PAGE_ALIGN(size);
	remapped_addr = (void *)((unsigned long)remapped_addr & PAGE_MASK);

	area = find_vm_area(remapped_addr);
	if (!area) {
		WARN(1, "trying to free invalid coherent area: %p\n",
			remapped_addr);
		return;
	}
	vunmap(remapped_addr);
	flush_tlb_kernel_range((unsigned long)remapped_addr,
			(unsigned long)(remapped_addr + size));
}

static struct dma_map_ops swiotlb_dma_ops = {
	.alloc = __dma_alloc,
	.free = __dma_free,
	.mmap = __swiotlb_mmap,
	.get_sgtable = __swiotlb_get_sgtable,
	.map_page = __swiotlb_map_page,
	.unmap_page = __swiotlb_unmap_page,
	.map_sg = __swiotlb_map_sg_attrs,
	.unmap_sg = __swiotlb_unmap_sg_attrs,
	.sync_single_for_cpu = __swiotlb_sync_single_for_cpu,
	.sync_single_for_device = __swiotlb_sync_single_for_device,
	.sync_sg_for_cpu = __swiotlb_sync_sg_for_cpu,
	.sync_sg_for_device = __swiotlb_sync_sg_for_device,
	.dma_supported = swiotlb_dma_supported,
	.mapping_error = swiotlb_dma_mapping_error,
	.remap = arm64_dma_remap,
	.unremap = arm64_dma_unremap,
};

static int __init atomic_pool_init(void)
{
	pgprot_t prot = __pgprot(PROT_NORMAL_NC);
	unsigned long nr_pages = atomic_pool_size >> PAGE_SHIFT;
	struct page *page;
	void *addr;
	unsigned int pool_size_order = get_order(atomic_pool_size);

	if (dev_get_cma_area(NULL))
		page = dma_alloc_from_contiguous(NULL, nr_pages,
							pool_size_order);
	else
		page = alloc_pages(GFP_DMA, pool_size_order);

	if (page) {
		int ret;
		void *page_addr = page_address(page);

		memset(page_addr, 0, atomic_pool_size);
		__dma_flush_range(page_addr, page_addr + atomic_pool_size);

		atomic_pool = gen_pool_create(PAGE_SHIFT, -1);
		if (!atomic_pool)
			goto free_page;

		addr = dma_common_contiguous_remap(page, atomic_pool_size,
					VM_USERMAP, prot, atomic_pool_init);

		if (!addr)
			goto destroy_genpool;

		ret = gen_pool_add_virt(atomic_pool, (unsigned long)addr,
					page_to_phys(page),
					atomic_pool_size, -1);
		if (ret)
			goto remove_mapping;

		gen_pool_set_algo(atomic_pool,
				  gen_pool_first_fit_order_align,
				  (void *)PAGE_SHIFT);

		pr_info("DMA: preallocated %zu KiB pool for atomic allocations\n",
			atomic_pool_size / 1024);
		return 0;
	}
	goto out;

remove_mapping:
	dma_common_free_remap(addr, atomic_pool_size, VM_USERMAP, true);
destroy_genpool:
	gen_pool_destroy(atomic_pool);
	atomic_pool = NULL;
free_page:
	if (!dma_release_from_contiguous(NULL, page, nr_pages))
		__free_pages(page, pool_size_order);
out:
	pr_err("DMA: failed to allocate %zu KiB pool for atomic coherent allocation\n",
		atomic_pool_size / 1024);
	return -ENOMEM;
}

/********************************************
 * The following APIs are for dummy DMA ops *
 ********************************************/

static void *__dummy_alloc(struct device *dev, size_t size,
			   dma_addr_t *dma_handle, gfp_t flags,
			   struct dma_attrs *attrs)
{
	WARN(1, "dma alloc failure, device may be missing a call to arch_setup_dma_ops");
	return NULL;
}

static void __dummy_free(struct device *dev, size_t size,
			 void *vaddr, dma_addr_t dma_handle,
			 struct dma_attrs *attrs)
{
}

static int __dummy_mmap(struct device *dev,
			struct vm_area_struct *vma,
			void *cpu_addr, dma_addr_t dma_addr, size_t size,
			struct dma_attrs *attrs)
{
	return -ENXIO;
}

static dma_addr_t __dummy_map_page(struct device *dev, struct page *page,
				   unsigned long offset, size_t size,
				   enum dma_data_direction dir,
				   struct dma_attrs *attrs)
{
	return DMA_ERROR_CODE;
}

static void __dummy_unmap_page(struct device *dev, dma_addr_t dev_addr,
			       size_t size, enum dma_data_direction dir,
			       struct dma_attrs *attrs)
{
}

static int __dummy_map_sg(struct device *dev, struct scatterlist *sgl,
			  int nelems, enum dma_data_direction dir,
			  struct dma_attrs *attrs)
{
	return 0;
}

static void __dummy_unmap_sg(struct device *dev,
			     struct scatterlist *sgl, int nelems,
			     enum dma_data_direction dir,
			     struct dma_attrs *attrs)
{
}

static void __dummy_sync_single(struct device *dev,
				dma_addr_t dev_addr, size_t size,
				enum dma_data_direction dir)
{
}

static void __dummy_sync_sg(struct device *dev,
			    struct scatterlist *sgl, int nelems,
			    enum dma_data_direction dir)
{
}

static int __dummy_mapping_error(struct device *hwdev, dma_addr_t dma_addr)
{
	return 1;
}

static int __dummy_dma_supported(struct device *hwdev, u64 mask)
{
	return 0;
}

struct dma_map_ops dummy_dma_ops = {
	.alloc                  = __dummy_alloc,
	.free                   = __dummy_free,
	.mmap                   = __dummy_mmap,
	.map_page               = __dummy_map_page,
	.unmap_page             = __dummy_unmap_page,
	.map_sg                 = __dummy_map_sg,
	.unmap_sg               = __dummy_unmap_sg,
	.sync_single_for_cpu    = __dummy_sync_single,
	.sync_single_for_device = __dummy_sync_single,
	.sync_sg_for_cpu        = __dummy_sync_sg,
	.sync_sg_for_device     = __dummy_sync_sg,
	.mapping_error          = __dummy_mapping_error,
	.dma_supported          = __dummy_dma_supported,
};
EXPORT_SYMBOL(dummy_dma_ops);

static int __init arm64_dma_init(void)
{
	return atomic_pool_init();
}
arch_initcall(arm64_dma_init);

#define PREALLOC_DMA_DEBUG_ENTRIES	4096

static int __init dma_debug_do_init(void)
{
	dma_debug_init(PREALLOC_DMA_DEBUG_ENTRIES);
	return 0;
}
fs_initcall(dma_debug_do_init);


#ifdef CONFIG_IOMMU_DMA
#include <linux/dma-iommu.h>
#include <linux/platform_device.h>
#include <linux/amba/bus.h>

/* Thankfully, all cache ops are by VA so we can ignore phys here */
static void flush_page(struct device *dev, const void *virt, phys_addr_t phys)
{
	__dma_flush_range(virt, virt + PAGE_SIZE);
}

static void *__iommu_alloc_attrs(struct device *dev, size_t size,
				 dma_addr_t *handle, gfp_t gfp,
				 struct dma_attrs *attrs)
{
	bool coherent = is_dma_coherent(dev, attrs);
	int ioprot = dma_direction_to_prot(DMA_BIDIRECTIONAL, coherent);
	size_t iosize = size;
	void *addr;

	if (WARN(!dev, "cannot create IOMMU mapping for unknown device\n"))
		return NULL;

	size = PAGE_ALIGN(size);

	/*
	 * Some drivers rely on this, and we probably don't want the
	 * possibility of stale kernel data being read by devices anyway.
	 */
	gfp |= __GFP_ZERO;

	if (gfpflags_allow_blocking(gfp)) {
		struct page **pages;
		pgprot_t prot = __get_dma_pgprot(attrs, PAGE_KERNEL, coherent);

		pages = iommu_dma_alloc(dev, iosize, gfp, ioprot, handle,
					flush_page);
		if (!pages)
			return NULL;

		addr = dma_common_pages_remap(pages, size, VM_USERMAP, prot,
					      __builtin_return_address(0));
		if (!addr)
			iommu_dma_free(dev, pages, iosize, handle);
	} else {
		struct page *page;
		/*
		 * In atomic context we can't remap anything, so we'll only
		 * get the virtually contiguous buffer we need by way of a
		 * physically contiguous allocation.
		 */
		if (coherent) {
			page = alloc_pages(gfp, get_order(size));
			addr = page ? page_address(page) : NULL;
		} else {
			addr = __alloc_from_pool(size, &page, gfp);
		}
		if (!addr)
			return NULL;

		*handle = iommu_dma_map_page(dev, page, 0, iosize, ioprot);
		if (iommu_dma_mapping_error(dev, *handle)) {
			if (coherent)
				__free_pages(page, get_order(size));
			else
				__free_from_pool(addr, size);
			addr = NULL;
		}
	}
	return addr;
}

static void __iommu_free_attrs(struct device *dev, size_t size, void *cpu_addr,
			       dma_addr_t handle, struct dma_attrs *attrs)
{
	size_t iosize = size;

	size = PAGE_ALIGN(size);
	/*
	 * @cpu_addr will be one of 3 things depending on how it was allocated:
	 * - A remapped array of pages from iommu_dma_alloc(), for all
	 *   non-atomic allocations.
	 * - A non-cacheable alias from the atomic pool, for atomic
	 *   allocations by non-coherent devices.
	 * - A normal lowmem address, for atomic allocations by
	 *   coherent devices.
	 * Hence how dodgy the below logic looks...
	 */
	if (__in_atomic_pool(cpu_addr, size)) {
		iommu_dma_unmap_page(dev, handle, iosize, 0, NULL);
		__free_from_pool(cpu_addr, size);
	} else if (is_vmalloc_addr(cpu_addr)){
		struct vm_struct *area = find_vm_area(cpu_addr);

		if (WARN_ON(!area || !area->pages))
			return;
		iommu_dma_free(dev, area->pages, iosize, &handle);
		dma_common_free_remap(cpu_addr, size, VM_USERMAP, true);
	} else {
		iommu_dma_unmap_page(dev, handle, iosize, 0, NULL);
		__free_pages(virt_to_page(cpu_addr), get_order(size));
	}
}

static int __iommu_mmap_attrs(struct device *dev, struct vm_area_struct *vma,
			      void *cpu_addr, dma_addr_t dma_addr, size_t size,
			      struct dma_attrs *attrs)
{
	struct vm_struct *area;
	int ret;

	vma->vm_page_prot = __get_dma_pgprot(attrs, vma->vm_page_prot,
					     is_dma_coherent(dev, attrs));

	if (dma_mmap_from_coherent(dev, vma, cpu_addr, size, &ret))
		return ret;

	area = find_vm_area(cpu_addr);
	if (WARN_ON(!area || !area->pages))
		return -ENXIO;

	return iommu_dma_mmap(area->pages, size, vma);
}

static int __iommu_get_sgtable(struct device *dev, struct sg_table *sgt,
			       void *cpu_addr, dma_addr_t dma_addr,
			       size_t size, struct dma_attrs *attrs)
{
	unsigned int count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	struct vm_struct *area = find_vm_area(cpu_addr);

	if (WARN_ON(!area || !area->pages))
		return -ENXIO;

	return sg_alloc_table_from_pages(sgt, area->pages, count, 0, size,
					 GFP_KERNEL);
}

static void __iommu_sync_single_for_cpu(struct device *dev,
					dma_addr_t dev_addr, size_t size,
					enum dma_data_direction dir)
{
	phys_addr_t phys;

	if (is_device_dma_coherent(dev))
		return;

	phys = iommu_iova_to_phys(iommu_get_domain_for_dev(dev), dev_addr);
	__dma_unmap_area(phys_to_virt(phys), size, dir);
}

static void __iommu_sync_single_for_device(struct device *dev,
					   dma_addr_t dev_addr, size_t size,
					   enum dma_data_direction dir)
{
	phys_addr_t phys;

	if (is_device_dma_coherent(dev))
		return;

	phys = iommu_iova_to_phys(iommu_get_domain_for_dev(dev), dev_addr);
	__dma_map_area(phys_to_virt(phys), size, dir);
}

static dma_addr_t __iommu_map_page(struct device *dev, struct page *page,
				   unsigned long offset, size_t size,
				   enum dma_data_direction dir,
				   struct dma_attrs *attrs)
{
	bool coherent = is_dma_coherent(dev, attrs);
	int prot = dma_direction_to_prot(dir, coherent);
	dma_addr_t dev_addr = iommu_dma_map_page(dev, page, offset, size, prot);

	if (!iommu_dma_mapping_error(dev, dev_addr) &&
	    !dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs))
		__iommu_sync_single_for_device(dev, dev_addr, size, dir);

	return dev_addr;
}

static void __iommu_unmap_page(struct device *dev, dma_addr_t dev_addr,
			       size_t size, enum dma_data_direction dir,
			       struct dma_attrs *attrs)
{
	if (!dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs))
		__iommu_sync_single_for_cpu(dev, dev_addr, size, dir);

	iommu_dma_unmap_page(dev, dev_addr, size, dir, attrs);
}

static void __iommu_sync_sg_for_cpu(struct device *dev,
				    struct scatterlist *sgl, int nelems,
				    enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	if (is_device_dma_coherent(dev))
		return;

	for_each_sg(sgl, sg, nelems, i)
		__dma_unmap_area(sg_virt(sg), sg->length, dir);
}

static void __iommu_sync_sg_for_device(struct device *dev,
				       struct scatterlist *sgl, int nelems,
				       enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	if (is_device_dma_coherent(dev))
		return;

	for_each_sg(sgl, sg, nelems, i)
		__dma_map_area(sg_virt(sg), sg->length, dir);
}

static int __iommu_map_sg_attrs(struct device *dev, struct scatterlist *sgl,
				int nelems, enum dma_data_direction dir,
				struct dma_attrs *attrs)
{
	bool coherent = is_dma_coherent(dev, attrs);

	if (!dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs))
		__iommu_sync_sg_for_device(dev, sgl, nelems, dir);

	return iommu_dma_map_sg(dev, sgl, nelems,
			dma_direction_to_prot(dir, coherent));
}

static void __iommu_unmap_sg_attrs(struct device *dev,
				   struct scatterlist *sgl, int nelems,
				   enum dma_data_direction dir,
				   struct dma_attrs *attrs)
{
	if (!dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs))
		__iommu_sync_sg_for_cpu(dev, sgl, nelems, dir);

	iommu_dma_unmap_sg(dev, sgl, nelems, dir, attrs);
}

static struct dma_map_ops iommu_dma_ops = {
	.alloc = __iommu_alloc_attrs,
	.free = __iommu_free_attrs,
	.mmap = __iommu_mmap_attrs,
	.get_sgtable = __iommu_get_sgtable,
	.map_page = __iommu_map_page,
	.unmap_page = __iommu_unmap_page,
	.map_sg = __iommu_map_sg_attrs,
	.unmap_sg = __iommu_unmap_sg_attrs,
	.sync_single_for_cpu = __iommu_sync_single_for_cpu,
	.sync_single_for_device = __iommu_sync_single_for_device,
	.sync_sg_for_cpu = __iommu_sync_sg_for_cpu,
	.sync_sg_for_device = __iommu_sync_sg_for_device,
	.mapping_error = iommu_dma_mapping_error,
};

/*
 * TODO: Right now __iommu_setup_dma_ops() gets called too early to do
 * everything it needs to - the device is only partially created and the
 * IOMMU driver hasn't seen it yet, so it can't have a group. Thus we
 * need this delayed attachment dance. Once IOMMU probe ordering is sorted
 * to move the arch_setup_dma_ops() call later, all the notifier bits below
 * become unnecessary, and will go away.
 */
struct iommu_dma_notifier_data {
	struct list_head list;
	struct device *dev;
	const struct iommu_ops *ops;
	u64 dma_base;
	u64 size;
};
static LIST_HEAD(iommu_dma_masters);
static DEFINE_MUTEX(iommu_dma_notifier_lock);

/*
 * Temporarily "borrow" a domain feature flag to to tell if we had to resort
 * to creating our own domain here, in case we need to clean it up again.
 */
#define __IOMMU_DOMAIN_FAKE_DEFAULT		(1U << 31)

static bool do_iommu_attach(struct device *dev, const struct iommu_ops *ops,
			   u64 dma_base, u64 size)
{
	struct iommu_domain *domain = iommu_get_domain_for_dev(dev);

	/*
	 * Best case: The device is either part of a group which was
	 * already attached to a domain in a previous call, or it's
	 * been put in a default DMA domain by the IOMMU core.
	 */
	if (!domain) {
		/*
		 * Urgh. The IOMMU core isn't going to do default domains
		 * for non-PCI devices anyway, until it has some means of
		 * abstracting the entirely implementation-specific
		 * sideband data/SoC topology/unicorn dust that may or
		 * may not differentiate upstream masters.
		 * So until then, HORRIBLE HACKS!
		 */
		domain = ops->domain_alloc(IOMMU_DOMAIN_DMA);
		if (!domain)
			goto out_no_domain;

		domain->ops = ops;
		domain->type = IOMMU_DOMAIN_DMA | __IOMMU_DOMAIN_FAKE_DEFAULT;

		if (iommu_attach_device(domain, dev))
			goto out_put_domain;
	}

	if (iommu_dma_init_domain(domain, dma_base, size))
		goto out_detach;

	dev->archdata.dma_ops = &iommu_dma_ops;
	return true;

out_detach:
	iommu_detach_device(domain, dev);
out_put_domain:
	if (domain->type & __IOMMU_DOMAIN_FAKE_DEFAULT)
		iommu_domain_free(domain);
out_no_domain:
	pr_warn("Failed to set up IOMMU for device %s; retaining platform DMA ops\n",
		dev_name(dev));
	return false;
}

static void queue_iommu_attach(struct device *dev, const struct iommu_ops *ops,
			      u64 dma_base, u64 size)
{
	struct iommu_dma_notifier_data *iommudata;

	iommudata = kzalloc(sizeof(*iommudata), GFP_KERNEL);
	if (!iommudata)
		return;

	iommudata->dev = dev;
	iommudata->ops = ops;
	iommudata->dma_base = dma_base;
	iommudata->size = size;

	mutex_lock(&iommu_dma_notifier_lock);
	list_add(&iommudata->list, &iommu_dma_masters);
	mutex_unlock(&iommu_dma_notifier_lock);
}

static int __iommu_attach_notifier(struct notifier_block *nb,
				   unsigned long action, void *data)
{
	struct iommu_dma_notifier_data *master, *tmp;

	if (action != BUS_NOTIFY_ADD_DEVICE)
		return 0;

	mutex_lock(&iommu_dma_notifier_lock);
	list_for_each_entry_safe(master, tmp, &iommu_dma_masters, list) {
		if (do_iommu_attach(master->dev, master->ops,
				master->dma_base, master->size)) {
			list_del(&master->list);
			kfree(master);
		}
	}
	mutex_unlock(&iommu_dma_notifier_lock);
	return 0;
}

static int __init register_iommu_dma_ops_notifier(struct bus_type *bus)
{
	struct notifier_block *nb = kzalloc(sizeof(*nb), GFP_KERNEL);
	int ret;

	if (!nb)
		return -ENOMEM;
	/*
	 * The device must be attached to a domain before the driver probe
	 * routine gets a chance to start allocating DMA buffers. However,
	 * the IOMMU driver also needs a chance to configure the iommu_group
	 * via its add_device callback first, so we need to make the attach
	 * happen between those two points. Since the IOMMU core uses a bus
	 * notifier with default priority for add_device, do the same but
	 * with a lower priority to ensure the appropriate ordering.
	 */
	nb->notifier_call = __iommu_attach_notifier;
	nb->priority = -100;

	ret = bus_register_notifier(bus, nb);
	if (ret) {
		pr_warn("Failed to register DMA domain notifier; IOMMU DMA ops unavailable on bus '%s'\n",
			bus->name);
		kfree(nb);
	}
	return ret;
}

static int __init __iommu_dma_init(void)
{
	int ret;

	ret = iommu_dma_init();
	if (!ret)
		ret = register_iommu_dma_ops_notifier(&platform_bus_type);
	if (!ret)
		ret = register_iommu_dma_ops_notifier(&amba_bustype);

	/* handle devices queued before this arch_initcall */
	if (!ret)
		__iommu_attach_notifier(NULL, BUS_NOTIFY_ADD_DEVICE, NULL);
	return ret;
}
arch_initcall(__iommu_dma_init);

static void __iommu_setup_dma_ops(struct device *dev, u64 dma_base, u64 size,
				  const struct iommu_ops *ops)
{
	struct iommu_group *group;

	if (!ops)
		return;
	/*
	 * TODO: As a concession to the future, we're ready to handle being
	 * called both early and late (i.e. after bus_add_device). Once all
	 * the platform bus code is reworked to call us late and the notifier
	 * junk above goes away, move the body of do_iommu_attach here.
	 */
	group = iommu_group_get(dev);
	if (group) {
		do_iommu_attach(dev, ops, dma_base, size);
		iommu_group_put(group);
	} else {
		queue_iommu_attach(dev, ops, dma_base, size);
	}
}

void arch_teardown_dma_ops(struct device *dev)
{
	struct iommu_domain *domain = iommu_get_domain_for_dev(dev);

	if (domain) {
		iommu_detach_device(domain, dev);
		if (domain->type & __IOMMU_DOMAIN_FAKE_DEFAULT)
			iommu_domain_free(domain);
	}

	dev->archdata.dma_ops = NULL;
}

#else

static void __iommu_setup_dma_ops(struct device *dev, u64 dma_base, u64 size,
				  struct iommu_ops *iommu)
{ }

#endif  /* CONFIG_IOMMU_DMA */

void arch_setup_dma_ops(struct device *dev, u64 dma_base, u64 size,
			struct iommu_ops *iommu, bool coherent)
{
	if (!dev->archdata.dma_ops)
		dev->archdata.dma_ops = &swiotlb_dma_ops;

	dev->archdata.dma_coherent = coherent;
	__iommu_setup_dma_ops(dev, dma_base, size, iommu);
}
EXPORT_SYMBOL(arch_setup_dma_ops);

#ifdef CONFIG_ARM64_DMA_USE_IOMMU

static int __get_iommu_pgprot(struct dma_attrs *attrs, int prot,
			      bool coherent)
{
	if (!dma_get_attr(DMA_ATTR_EXEC_MAPPING, attrs))
		prot |= IOMMU_NOEXEC;
	if (coherent)
		prot |= IOMMU_CACHE;

	return prot;
}

/*
 * Make an area consistent for devices.
 * Note: Drivers should NOT use this function directly, as it will break
 * platforms with CONFIG_DMABOUNCE.
 * Use the driver DMA support - see dma-mapping.h (dma_sync_*)
 */
static void __dma_page_cpu_to_dev(struct page *page, unsigned long off,
	size_t size, enum dma_data_direction dir)
{
	__dma_map_area(page_address(page) + off, size, dir);
}

static void __dma_page_dev_to_cpu(struct page *page, unsigned long off,
	size_t size, enum dma_data_direction dir)
{
	__dma_unmap_area(page_address(page) + off, size, dir);

	/*
	 * Mark the D-cache clean for this page to avoid extra flushing.
	 */
	if (dir != DMA_TO_DEVICE && off == 0 && size >= PAGE_SIZE)
		set_bit(PG_dcache_clean, &page->flags);
}

static int arm_dma_set_mask(struct device *dev, u64 dma_mask)
{
	if (!dev->dma_mask || !dma_supported(dev, dma_mask))
		return -EIO;

	*dev->dma_mask = dma_mask;

	return 0;
}

/* IOMMU */

static void __dma_clear_buffer(struct page *page, size_t size,
			       struct dma_attrs *attrs, bool is_coherent)
{
	/*
	 * Ensure that the allocated pages are zeroed, and that any data
	 * lurking in the kernel direct-mapped region is invalidated.
	 */
	void *ptr = page_address(page);
	if (!dma_get_attr(DMA_ATTR_SKIP_ZEROING, attrs))
		memset(ptr, 0, size);
	if (!is_coherent)
		dmac_flush_range(ptr, ptr + size);
}

static inline dma_addr_t __alloc_iova(struct dma_iommu_mapping *mapping,
				      size_t size)
{
	unsigned int order = get_order(size);
	unsigned int align = 0;
	unsigned int count, start;
	unsigned long flags;

	if (order > CONFIG_ARM64_DMA_IOMMU_ALIGNMENT)
		order = CONFIG_ARM64_DMA_IOMMU_ALIGNMENT;

	count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	align = (1 << order) - 1;

	spin_lock_irqsave(&mapping->lock, flags);
	start = bitmap_find_next_zero_area(mapping->bitmap, mapping->bits, 0,
					   count, align);
	if (start > mapping->bits) {
		spin_unlock_irqrestore(&mapping->lock, flags);
		return DMA_ERROR_CODE;
	}

	bitmap_set(mapping->bitmap, start, count);
	spin_unlock_irqrestore(&mapping->lock, flags);

	return mapping->base + (start << PAGE_SHIFT);
}

static inline void __free_iova(struct dma_iommu_mapping *mapping,
			       dma_addr_t addr, size_t size)
{
	unsigned int start = (addr - mapping->base) >> PAGE_SHIFT;
	unsigned int count = size >> PAGE_SHIFT;
	unsigned long flags;

	spin_lock_irqsave(&mapping->lock, flags);
	bitmap_clear(mapping->bitmap, start, count);
	spin_unlock_irqrestore(&mapping->lock, flags);
}

static struct page **__iommu_alloc_buffer(struct device *dev, size_t size,
					  gfp_t gfp, struct dma_attrs *attrs)
{
	struct page **pages;
	size_t count = size >> PAGE_SHIFT;
	size_t array_size = count * sizeof(struct page *);
	int i = 0;
	bool is_coherent = is_dma_coherent(dev, attrs);

	if (array_size <= PAGE_SIZE)
		pages = kzalloc(array_size, gfp);
	else
		pages = vzalloc(array_size);
	if (!pages)
		return NULL;

	if (dma_get_attr(DMA_ATTR_FORCE_CONTIGUOUS, attrs)) {
		unsigned long order = get_order(size);
		struct page *page;

		page = dma_alloc_from_contiguous(dev, count, order);
		if (!page)
			goto error;

		__dma_clear_buffer(page, size, attrs, is_coherent);

		for (i = 0; i < count; i++)
			pages[i] = page + i;

		return pages;
	}

	/*
	 * IOMMU can map any pages, so himem can also be used here
	 */
	gfp |= __GFP_NOWARN | __GFP_HIGHMEM;

	while (count) {
		int j, order = __fls(count);

		pages[i] = alloc_pages(gfp, order);
		while (!pages[i] && order)
			pages[i] = alloc_pages(gfp, --order);
		if (!pages[i])
			goto error;

		if (order) {
			split_page(pages[i], order);
			j = 1 << order;
			while (--j)
				pages[i + j] = pages[i] + j;
		}

		__dma_clear_buffer(pages[i], PAGE_SIZE << order, attrs,
				   is_coherent);
		i += 1 << order;
		count -= 1 << order;
	}

	return pages;
error:
	while (i--)
		if (pages[i])
			__free_pages(pages[i], 0);
	if (array_size <= PAGE_SIZE)
		kfree(pages);
	else
		vfree(pages);
	return NULL;
}

static int __iommu_free_buffer(struct device *dev, struct page **pages,
			       size_t size, struct dma_attrs *attrs)
{
	int count = size >> PAGE_SHIFT;
	int array_size = count * sizeof(struct page *);
	int i;

	if (dma_get_attr(DMA_ATTR_FORCE_CONTIGUOUS, attrs)) {
		dma_release_from_contiguous(dev, pages[0], count);
	} else {
		for (i = 0; i < count; i++)
			if (pages[i])
				__free_pages(pages[i], 0);
	}

	if (array_size <= PAGE_SIZE)
		kfree(pages);
	else
		vfree(pages);
	return 0;
}

/*
 * Create a CPU mapping for a specified pages
 */
static void *
__iommu_alloc_remap(struct page **pages, size_t size, gfp_t gfp, pgprot_t prot,
		    const void *caller)
{
	return dma_common_pages_remap(pages, size, VM_USERMAP, prot, caller);
}

/*
 * Create a mapping in device IO address space for specified pages
 */
static dma_addr_t __iommu_create_mapping(struct device *dev,
					struct page **pages, size_t size,
					struct dma_attrs *attrs)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;
	unsigned int count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	dma_addr_t dma_addr, iova;
	int i, ret;
	int prot = IOMMU_READ | IOMMU_WRITE;

	dma_addr = __alloc_iova(mapping, size);
	if (dma_addr == DMA_ERROR_CODE)
		return dma_addr;
	prot = __get_iommu_pgprot(attrs, prot,
				  is_dma_coherent(dev, attrs));

	iova = dma_addr;
	for (i = 0; i < count; ) {
		unsigned int next_pfn = page_to_pfn(pages[i]) + 1;
		phys_addr_t phys = page_to_phys(pages[i]);
		unsigned int len, j;

		for (j = i + 1; j < count; j++, next_pfn++)
			if (page_to_pfn(pages[j]) != next_pfn)
				break;

		len = (j - i) << PAGE_SHIFT;
		ret = iommu_map(mapping->domain, iova, phys, len, prot);
		if (ret < 0)
			goto fail;
		iova += len;
		i = j;
	}
	return dma_addr;
fail:
	iommu_unmap(mapping->domain, dma_addr, iova-dma_addr);
	__free_iova(mapping, dma_addr, size);
	return DMA_ERROR_CODE;
}

static int __iommu_remove_mapping(struct device *dev, dma_addr_t iova,
				size_t size)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;

	/*
	 * add optional in-page offset from iova to size and align
	 * result to page size
	 */
	size = PAGE_ALIGN((iova & ~PAGE_MASK) + size);
	iova &= PAGE_MASK;

	iommu_unmap(mapping->domain, iova, size);
	__free_iova(mapping, iova, size);
	return 0;
}

static struct page **__atomic_get_pages(void *addr)
{
	struct page *page;
	phys_addr_t phys;

	phys = gen_pool_virt_to_phys(atomic_pool, (unsigned long)addr);
	page = phys_to_page(phys);

	return (struct page **)page;
}

static struct page **__iommu_get_pages(void *cpu_addr, struct dma_attrs *attrs)
{
	struct vm_struct *area;

	if (__in_atomic_pool(cpu_addr, PAGE_SIZE))
		return __atomic_get_pages(cpu_addr);

	if (dma_get_attr(DMA_ATTR_NO_KERNEL_MAPPING, attrs))
		return cpu_addr;

	area = find_vm_area(cpu_addr);
	if (area)
		return area->pages;
	return NULL;
}

static void *__iommu_alloc_atomic(struct device *dev, size_t size,
				  dma_addr_t *handle, gfp_t gfp,
				struct dma_attrs *attrs)
{
	struct page *page;
	struct page **pages;
	size_t count = size >> PAGE_SHIFT;
	size_t array_size = count * sizeof(struct page *);
	int i;
	void *addr;
	bool coherent = is_dma_coherent(dev, attrs);

	if (array_size <= PAGE_SIZE)
		pages = kzalloc(array_size, gfp);
	else
		pages = vzalloc(array_size);

	if (!pages)
		return NULL;

	if (coherent) {
		page = alloc_pages(gfp, get_order(size));
		addr = page ? page_address(page) : NULL;
	} else {
		addr = __alloc_from_pool(size, &page, gfp);
	}

	if (!addr)
		goto err_free;

	for (i = 0; i < count ; i++)
		pages[i] = page + i;

	*handle = __iommu_create_mapping(dev, pages, size, attrs);
	if (*handle == DMA_ERROR_CODE)
		goto err_mapping;

	kvfree(pages);
	return addr;

err_mapping:
	if (coherent)
		__free_pages(page, get_order(size));
	else
		__free_from_pool(addr, size);
err_free:
	kvfree(pages);
	return NULL;
}

static void __iommu_free_atomic(struct device *dev, void *cpu_addr,
				dma_addr_t handle, size_t size)
{
	__iommu_remove_mapping(dev, handle, size);
	__free_from_pool(cpu_addr, size);
}

static void *arm_iommu_alloc_attrs(struct device *dev, size_t size,
	    dma_addr_t *handle, gfp_t gfp, struct dma_attrs *attrs)
{
	bool coherent = is_dma_coherent(dev, attrs);
	pgprot_t prot = __get_dma_pgprot(attrs, PAGE_KERNEL, coherent);
	struct page **pages;
	void *addr = NULL;

	*handle = DMA_ERROR_CODE;
	size = PAGE_ALIGN(size);

	if (!gfpflags_allow_blocking(gfp))
		return __iommu_alloc_atomic(dev, size, handle, gfp, attrs);

	/*
	 * Following is a work-around (a.k.a. hack) to prevent pages
	 * with __GFP_COMP being passed to split_page() which cannot
	 * handle them.  The real problem is that this flag probably
	 * should be 0 on ARM as it is not supported on this
	 * platform; see CONFIG_HUGETLBFS.
	 */
	gfp &= ~(__GFP_COMP);

	pages = __iommu_alloc_buffer(dev, size, gfp, attrs);
	if (!pages)
		return NULL;

	*handle = __iommu_create_mapping(dev, pages, size, attrs);
	if (*handle == DMA_ERROR_CODE)
		goto err_buffer;

	if (dma_get_attr(DMA_ATTR_NO_KERNEL_MAPPING, attrs))
		return pages;

	addr = __iommu_alloc_remap(pages, size, gfp, prot,
				   __builtin_return_address(0));
	if (!addr)
		goto err_mapping;

	return addr;

err_mapping:
	__iommu_remove_mapping(dev, *handle, size);
err_buffer:
	__iommu_free_buffer(dev, pages, size, attrs);
	return NULL;
}

static int arm_iommu_mmap_attrs(struct device *dev, struct vm_area_struct *vma,
		    void *cpu_addr, dma_addr_t dma_addr, size_t size,
		    struct dma_attrs *attrs)
{
	unsigned long uaddr = vma->vm_start;
	unsigned long usize = vma->vm_end - vma->vm_start;
	struct page **pages = __iommu_get_pages(cpu_addr, attrs);
	bool coherent = is_dma_coherent(dev, attrs);

	vma->vm_page_prot = __get_dma_pgprot(attrs, vma->vm_page_prot,
					     coherent);

	if (!pages)
		return -ENXIO;

	do {
		int ret = vm_insert_page(vma, uaddr, *pages++);
		if (ret) {
			pr_err("Remapping memory failed: %d\n", ret);
			return ret;
		}
		uaddr += PAGE_SIZE;
		usize -= PAGE_SIZE;
	} while (usize > 0);

	return 0;
}

/*
 * free a page as defined by the above mapping.
 * Must not be called with IRQs disabled.
 */
void arm_iommu_free_attrs(struct device *dev, size_t size, void *cpu_addr,
			  dma_addr_t handle, struct dma_attrs *attrs)
{
	struct page **pages;
	size = PAGE_ALIGN(size);

	if (__in_atomic_pool(cpu_addr, size)) {
		__iommu_free_atomic(dev, cpu_addr, handle, size);
		return;
	}

	pages = __iommu_get_pages(cpu_addr, attrs);
	if (!pages) {
		WARN(1, "trying to free invalid coherent area: %p\n", cpu_addr);
		return;
	}

	if (!dma_get_attr(DMA_ATTR_NO_KERNEL_MAPPING, attrs))
		dma_common_free_remap(cpu_addr, size, VM_USERMAP, true);

	__iommu_remove_mapping(dev, handle, size);
	__iommu_free_buffer(dev, pages, size, attrs);
}

int arm_iommu_get_sgtable(struct device *dev, struct sg_table *sgt,
				 void *cpu_addr, dma_addr_t dma_addr,
				 size_t size, struct dma_attrs *attrs)
{
	unsigned int count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	struct page **pages = __iommu_get_pages(cpu_addr, attrs);

	if (!pages)
		return -ENXIO;

	return sg_alloc_table_from_pages(sgt, pages, count, 0, size,
					 GFP_KERNEL);
}

static int __dma_direction_to_prot(enum dma_data_direction dir)
{
	int prot;

	switch (dir) {
	case DMA_BIDIRECTIONAL:
		prot = IOMMU_READ | IOMMU_WRITE;
		break;
	case DMA_TO_DEVICE:
		prot = IOMMU_READ;
		break;
	case DMA_FROM_DEVICE:
		prot = IOMMU_WRITE;
		break;
	default:
		prot = 0;
	}

	return prot;
}

/**
 * arm_iommu_map_sg - map a set of SG buffers for streaming mode DMA
 * @dev: valid struct device pointer
 * @sg: list of buffers
 * @nents: number of buffers to map
 * @dir: DMA transfer direction
 *
 * Map a set of buffers described by scatterlist in streaming mode for DMA.
 * The scatter gather list elements are merged together (if possible) and
 * tagged with the appropriate dma address and length. They are obtained via
 * sg_dma_{address,length}.
 */
int arm_iommu_map_sg(struct device *dev, struct scatterlist *sg,
		int nents, enum dma_data_direction dir, struct dma_attrs *attrs)
{
	struct scatterlist *s;
	int ret, i;
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;
	unsigned int total_length = 0, current_offset = 0;
	dma_addr_t iova;
	int prot = __dma_direction_to_prot(dir);

	for_each_sg(sg, s, nents, i)
		total_length += s->length;

	iova = __alloc_iova(mapping, total_length);
	if (iova == DMA_ERROR_CODE) {
		dev_err(dev, "Couldn't allocate iova for sg %p\n", sg);
		return 0;
	}
	prot = __get_iommu_pgprot(attrs, prot,
				  is_dma_coherent(dev, attrs));

	ret = iommu_map_sg(mapping->domain, iova, sg, nents, prot);
	if (ret != total_length) {
		__free_iova(mapping, iova, total_length);
		return 0;
	}

	for_each_sg(sg, s, nents, i) {
		s->dma_address = iova + current_offset;
		s->dma_length = total_length - current_offset;
		current_offset += s->length;
	}

	return nents;
}

/**
 * arm_iommu_unmap_sg - unmap a set of SG buffers mapped by dma_map_sg
 * @dev: valid struct device pointer
 * @sg: list of buffers
 * @nents: number of buffers to unmap (same as was passed to dma_map_sg)
 * @dir: DMA transfer direction (same as was passed to dma_map_sg)
 *
 * Unmap a set of streaming mode DMA translations.  Again, CPU access
 * rules concerning calls here are the same as for dma_unmap_single().
 */
void arm_iommu_unmap_sg(struct device *dev, struct scatterlist *sg, int nents,
			enum dma_data_direction dir, struct dma_attrs *attrs)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;
	unsigned int total_length = sg_dma_len(sg);
	dma_addr_t iova = sg_dma_address(sg);

	total_length = PAGE_ALIGN((iova & ~PAGE_MASK) + total_length);
	iova &= PAGE_MASK;

	iommu_unmap(mapping->domain, iova, total_length);
	__free_iova(mapping, iova, total_length);
}

/**
 * arm_iommu_sync_sg_for_cpu
 * @dev: valid struct device pointer
 * @sg: list of buffers
 * @nents: number of buffers to map (returned from dma_map_sg)
 * @dir: DMA transfer direction (same as was passed to dma_map_sg)
 */
void arm_iommu_sync_sg_for_cpu(struct device *dev, struct scatterlist *sg,
			int nents, enum dma_data_direction dir)
{
	struct scatterlist *s;
	int i;
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;
	dma_addr_t iova = sg_dma_address(sg);
	bool iova_coherent = iommu_is_iova_coherent(mapping->domain, iova);

	if (iova_coherent)
		return;

	for_each_sg(sg, s, nents, i)
		__dma_page_dev_to_cpu(sg_page(s), s->offset, s->length, dir);

}

/**
 * arm_iommu_sync_sg_for_device
 * @dev: valid struct device pointer
 * @sg: list of buffers
 * @nents: number of buffers to map (returned from dma_map_sg)
 * @dir: DMA transfer direction (same as was passed to dma_map_sg)
 */
void arm_iommu_sync_sg_for_device(struct device *dev, struct scatterlist *sg,
			int nents, enum dma_data_direction dir)
{
	struct scatterlist *s;
	int i;
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;
	dma_addr_t iova = sg_dma_address(sg);
	bool iova_coherent = iommu_is_iova_coherent(mapping->domain, iova);

	if (iova_coherent)
		return;

	for_each_sg(sg, s, nents, i)
		__dma_page_cpu_to_dev(sg_page(s), s->offset, s->length, dir);
}


/**
 * arm_coherent_iommu_map_page
 * @dev: valid struct device pointer
 * @page: page that buffer resides in
 * @offset: offset into page for start of buffer
 * @size: size of buffer to map
 * @dir: DMA transfer direction
 *
 * Coherent IOMMU aware version of arm_dma_map_page()
 */
static dma_addr_t arm_coherent_iommu_map_page(struct device *dev,
	     struct page *page, unsigned long offset, size_t size,
	     enum dma_data_direction dir, struct dma_attrs *attrs)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;
	dma_addr_t dma_addr;
	int ret, prot, len, start_offset, map_offset;

	map_offset = offset & ~PAGE_MASK;
	start_offset = offset & PAGE_MASK;
	len = PAGE_ALIGN(map_offset + size);

	dma_addr = __alloc_iova(mapping, len);
	if (dma_addr == DMA_ERROR_CODE)
		return dma_addr;

	prot = __dma_direction_to_prot(dir);
	prot = __get_iommu_pgprot(attrs, prot,
				  is_dma_coherent(dev, attrs));

	ret = iommu_map(mapping->domain, dma_addr, page_to_phys(page) +
			start_offset, len, prot);
	if (ret < 0)
		goto fail;

	return dma_addr + map_offset;
fail:
	__free_iova(mapping, dma_addr, len);
	return DMA_ERROR_CODE;
}

/**
 * arm_iommu_map_page
 * @dev: valid struct device pointer
 * @page: page that buffer resides in
 * @offset: offset into page for start of buffer
 * @size: size of buffer to map
 * @dir: DMA transfer direction
 *
 * IOMMU aware version of arm_dma_map_page()
 */
static dma_addr_t arm_iommu_map_page(struct device *dev, struct page *page,
	     unsigned long offset, size_t size, enum dma_data_direction dir,
	     struct dma_attrs *attrs)
{
	if (!is_dma_coherent(dev, attrs) &&
	      !dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs))
		__dma_page_cpu_to_dev(page, offset, size, dir);

	return arm_coherent_iommu_map_page(dev, page, offset, size, dir, attrs);
}

/**
 * arm_iommu_unmap_page
 * @dev: valid struct device pointer
 * @handle: DMA address of buffer
 * @size: size of buffer (same as passed to dma_map_page)
 * @dir: DMA transfer direction (same as passed to dma_map_page)
 *
 * IOMMU aware version of arm_dma_unmap_page()
 */
static void arm_iommu_unmap_page(struct device *dev, dma_addr_t handle,
		size_t size, enum dma_data_direction dir,
		struct dma_attrs *attrs)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;
	dma_addr_t iova = handle & PAGE_MASK;
	struct page *page = phys_to_page(iommu_iova_to_phys(
						mapping->domain, iova));
	int offset = handle & ~PAGE_MASK;
	int len = PAGE_ALIGN(size + offset);

	if (!(is_dma_coherent(dev, attrs) ||
	      dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs)))
		__dma_page_dev_to_cpu(page, offset, size, dir);

	iommu_unmap(mapping->domain, iova, len);
	__free_iova(mapping, iova, len);
}

static void arm_iommu_sync_single_for_cpu(struct device *dev,
		dma_addr_t handle, size_t size, enum dma_data_direction dir)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;
	dma_addr_t iova = handle & PAGE_MASK;
	struct page *page = phys_to_page(iommu_iova_to_phys(
						mapping->domain, iova));
	unsigned int offset = handle & ~PAGE_MASK;
	bool iova_coherent = iommu_is_iova_coherent(mapping->domain, handle);

	if (!iova_coherent)
		__dma_page_dev_to_cpu(page, offset, size, dir);
}

static void arm_iommu_sync_single_for_device(struct device *dev,
		dma_addr_t handle, size_t size, enum dma_data_direction dir)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;
	dma_addr_t iova = handle & PAGE_MASK;
	struct page *page = phys_to_page(iommu_iova_to_phys(
						mapping->domain, iova));
	unsigned int offset = handle & ~PAGE_MASK;
	bool iova_coherent = iommu_is_iova_coherent(mapping->domain, handle);

	if (!iova_coherent)
		__dma_page_cpu_to_dev(page, offset, size, dir);
}

static int arm_iommu_dma_supported(struct device *dev, u64 mask)
{
	struct dma_iommu_mapping *mapping = to_dma_iommu_mapping(dev);

	if (!mapping) {
		dev_warn(dev, "No IOMMU mapping for device\n");
		return 0;
	}

	return iommu_dma_supported(mapping->domain, dev, mask);
}

static int arm_iommu_mapping_error(struct device *dev,
				   dma_addr_t dma_addr)
{
	return dma_addr == DMA_ERROR_CODE;
}

const struct dma_map_ops iommu_ops = {
	.alloc		= arm_iommu_alloc_attrs,
	.free		= arm_iommu_free_attrs,
	.mmap		= arm_iommu_mmap_attrs,
	.get_sgtable	= arm_iommu_get_sgtable,

	.map_page		= arm_iommu_map_page,
	.unmap_page		= arm_iommu_unmap_page,
	.sync_single_for_cpu	= arm_iommu_sync_single_for_cpu,
	.sync_single_for_device	= arm_iommu_sync_single_for_device,

	.map_sg			= arm_iommu_map_sg,
	.unmap_sg		= arm_iommu_unmap_sg,
	.sync_sg_for_cpu	= arm_iommu_sync_sg_for_cpu,
	.sync_sg_for_device	= arm_iommu_sync_sg_for_device,

	.set_dma_mask		= arm_dma_set_mask,
	.dma_supported		= arm_iommu_dma_supported,
	.mapping_error		= arm_iommu_mapping_error,
};

/**
 * arm_iommu_create_mapping
 * @bus: pointer to the bus holding the client device (for IOMMU calls)
 * @base: start address of the valid IO address space
 * @size: maximum size of the valid IO address space
 *
 * Creates a mapping structure which holds information about used/unused
 * IO address ranges, which is required to perform memory allocation and
 * mapping with IOMMU aware functions.
 *
 * The client device need to be attached to the mapping with
 * arm_iommu_attach_device function.
 */
struct dma_iommu_mapping *
arm_iommu_create_mapping(struct bus_type *bus, dma_addr_t base, size_t size)
{
	unsigned int bits = size >> PAGE_SHIFT;
	unsigned int bitmap_size = BITS_TO_LONGS(bits) * sizeof(long);
	struct dma_iommu_mapping *mapping;
	int err = -ENOMEM;

	if (!bitmap_size)
		return ERR_PTR(-EINVAL);

	mapping = kzalloc(sizeof(struct dma_iommu_mapping), GFP_KERNEL);
	if (!mapping)
		goto err;

	mapping->bitmap = kzalloc(bitmap_size, GFP_KERNEL | __GFP_NOWARN |
							__GFP_NORETRY);
	if (!mapping->bitmap)
		mapping->bitmap = vzalloc(bitmap_size);

	if (!mapping->bitmap)
		goto err2;

	mapping->base = base;
	mapping->bits = bits;
	spin_lock_init(&mapping->lock);

	mapping->domain = iommu_domain_alloc(bus);
	if (!mapping->domain)
		goto err3;

	kref_init(&mapping->kref);
	return mapping;
err3:
	kvfree(mapping->bitmap);
err2:
	kfree(mapping);
err:
	return ERR_PTR(err);
}
EXPORT_SYMBOL(arm_iommu_create_mapping);

static void release_iommu_mapping(struct kref *kref)
{
	struct dma_iommu_mapping *mapping =
		container_of(kref, struct dma_iommu_mapping, kref);

	iommu_domain_free(mapping->domain);
	kvfree(mapping->bitmap);
	kfree(mapping);
}

void arm_iommu_release_mapping(struct dma_iommu_mapping *mapping)
{
	if (mapping)
		kref_put(&mapping->kref, release_iommu_mapping);
}
EXPORT_SYMBOL(arm_iommu_release_mapping);

/**
 * arm_iommu_attach_device
 * @dev: valid struct device pointer
 * @mapping: io address space mapping structure (returned from
 *	arm_iommu_create_mapping)
 *
 * Attaches specified io address space mapping to the provided device,
 * this replaces the dma operations (dma_map_ops pointer) with the
 * IOMMU aware version. More than one client might be attached to
 * the same io address space mapping.
 */
int arm_iommu_attach_device(struct device *dev,
			    struct dma_iommu_mapping *mapping)
{
	int err;
	int s1_bypass = 0, is_fast = 0;

	iommu_domain_get_attr(mapping->domain, DOMAIN_ATTR_FAST, &is_fast);
	if (is_fast)
		return fast_smmu_attach_device(dev, mapping);

	err = iommu_attach_device(mapping->domain, dev);
	if (err)
		return err;

	iommu_domain_get_attr(mapping->domain, DOMAIN_ATTR_S1_BYPASS,
					&s1_bypass);

	kref_get(&mapping->kref);
	dev->archdata.mapping = mapping;
	if (!s1_bypass)
		set_dma_ops(dev, &iommu_ops);

	pr_debug("Attached IOMMU controller to %s device.\n", dev_name(dev));
	return 0;
}
EXPORT_SYMBOL(arm_iommu_attach_device);

/**
 * arm_iommu_detach_device
 * @dev: valid struct device pointer
 *
 * Detaches the provided device from a previously attached map.
 * This voids the dma operations (dma_map_ops pointer)
 */
void arm_iommu_detach_device(struct device *dev)
{
	struct dma_iommu_mapping *mapping;
	int is_fast, s1_bypass = 0;

	mapping = to_dma_iommu_mapping(dev);
	if (!mapping) {
		dev_warn(dev, "Not attached\n");
		return;
	}

	iommu_domain_get_attr(mapping->domain, DOMAIN_ATTR_FAST, &is_fast);
	if (is_fast) {
		fast_smmu_detach_device(dev, mapping);
		return;
	}

	iommu_domain_get_attr(mapping->domain, DOMAIN_ATTR_S1_BYPASS,
					&s1_bypass);

	if (msm_dma_unmap_all_for_dev(dev))
		dev_warn(dev, "IOMMU detach with outstanding mappings\n");

	iommu_detach_device(mapping->domain, dev);
	kref_put(&mapping->kref, release_iommu_mapping);
	dev->archdata.mapping = NULL;
	if (!s1_bypass)
		set_dma_ops(dev, NULL);

	pr_debug("Detached IOMMU controller from %s device.\n", dev_name(dev));
}
EXPORT_SYMBOL(arm_iommu_detach_device);

#endif
