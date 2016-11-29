/*
 * Copyright © 2010 Daniel Vetter
 * Copyright © 2011-2014 Intel Corporation
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include <linux/seq_file.h>
#include <drm/drmP.h>
#include <drm/i915_drm.h>
#include "i915_drv.h"
#include "i915_trace.h"
#include "intel_drv.h"

static void bdw_setup_private_ppat(struct drm_i915_private *dev_priv);
static void chv_setup_private_ppat(struct drm_i915_private *dev_priv);

static int sanitize_enable_ppgtt(struct drm_device *dev, int enable_ppgtt)
{
	bool has_aliasing_ppgtt;
	bool has_full_ppgtt;

	has_aliasing_ppgtt = INTEL_INFO(dev)->gen >= 6;
	has_full_ppgtt = INTEL_INFO(dev)->gen >= 7;

	if (enable_ppgtt == 0 || !has_aliasing_ppgtt)
		return 0;

	if (enable_ppgtt == 1)
		return 1;

	if (enable_ppgtt == 2 && has_full_ppgtt)
		return 2;

#ifdef CONFIG_INTEL_IOMMU
	/* Disable ppgtt on SNB if VT-d is on. */
	if (INTEL_INFO(dev)->gen == 6 && intel_iommu_gfx_mapped) {
		DRM_INFO("Disabling PPGTT because VT-d is on\n");
		return 0;
	}
#endif

	/* VLV Android doesn't want this */
	if (IS_VALLEYVIEW(dev) && !IS_CHERRYVIEW(dev)) {
		DRM_DEBUG_DRIVER("disabling PPGTT on VLV\n");
		return 0;
	}

	return has_full_ppgtt ? 2 : has_aliasing_ppgtt ? 1 : 0;
}

static void ppgtt_bind_vma(struct i915_vma *vma,
			   enum i915_cache_level cache_level,
			   u32 flags);
static void ppgtt_unbind_vma(struct i915_vma *vma);

static inline gen8_gtt_pte_t gen8_pte_encode(dma_addr_t addr,
					     enum i915_cache_level level,
					     bool valid)
{
	gen8_gtt_pte_t pte = valid ? _PAGE_PRESENT | _PAGE_RW : 0;
	pte |= addr;

	switch (level) {
	case I915_CACHE_NONE:
		pte |= PPAT_UNCACHED_INDEX;
		break;
	case I915_CACHE_WT:
		pte |= PPAT_DISPLAY_ELLC_INDEX;
		break;
	default:
		pte |= PPAT_CACHED_INDEX;
		break;
	}

	return pte;
}

static inline gen8_ppgtt_pde_t gen8_pde_encode(struct drm_device *dev,
					     dma_addr_t addr,
					     enum i915_cache_level level)
{
	gen8_ppgtt_pde_t pde = _PAGE_PRESENT | _PAGE_RW;
	pde |= addr;
	if (level != I915_CACHE_NONE)
		pde |= PPAT_CACHED_PDE_INDEX;
	else
		pde |= PPAT_UNCACHED_INDEX;
	return pde;
}

static gen6_gtt_pte_t snb_pte_encode(dma_addr_t addr,
				     enum i915_cache_level level,
				     bool valid, u32 unused)
{
	gen6_gtt_pte_t pte = valid ? GEN6_PTE_VALID : 0;
	pte |= GEN6_PTE_ADDR_ENCODE(addr);

	switch (level) {
	case I915_CACHE_L3_LLC:
	case I915_CACHE_LLC:
		pte |= GEN6_PTE_CACHE_LLC;
		break;
	case I915_CACHE_NONE:
		pte |= GEN6_PTE_UNCACHED;
		break;
	default:
		WARN_ON(1);
	}

	return pte;
}

static gen6_gtt_pte_t ivb_pte_encode(dma_addr_t addr,
				     enum i915_cache_level level,
				     bool valid, u32 unused)
{
	gen6_gtt_pte_t pte = valid ? GEN6_PTE_VALID : 0;
	pte |= GEN6_PTE_ADDR_ENCODE(addr);

	switch (level) {
	case I915_CACHE_L3_LLC:
		pte |= GEN7_PTE_CACHE_L3_LLC;
		break;
	case I915_CACHE_LLC:
		pte |= GEN6_PTE_CACHE_LLC;
		break;
	case I915_CACHE_NONE:
		pte |= GEN6_PTE_UNCACHED;
		break;
	default:
		WARN_ON(1);
	}

	return pte;
}

static gen6_gtt_pte_t byt_pte_encode(dma_addr_t addr,
				     enum i915_cache_level level,
				     bool valid, u32 flags)
{
	gen6_gtt_pte_t pte = valid ? GEN6_PTE_VALID : 0;
	pte |= GEN6_PTE_ADDR_ENCODE(addr);

	/* Mark the page as writeable.  Other platforms don't have a
	 * setting for read-only/writable, so this matches that behavior.
	 */
	if (!(flags & PTE_READ_ONLY))
		pte |= BYT_PTE_WRITEABLE;

	if (level != I915_CACHE_NONE)
		pte |= BYT_PTE_SNOOPED_BY_CPU_CACHES;

	return pte;
}

static gen6_gtt_pte_t hsw_pte_encode(dma_addr_t addr,
				     enum i915_cache_level level,
				     bool valid, u32 unused)
{
	gen6_gtt_pte_t pte = valid ? GEN6_PTE_VALID : 0;
	pte |= HSW_PTE_ADDR_ENCODE(addr);

	if (level != I915_CACHE_NONE)
		pte |= HSW_WB_LLC_AGE3;

	return pte;
}

static gen6_gtt_pte_t iris_pte_encode(dma_addr_t addr,
				      enum i915_cache_level level,
				      bool valid, u32 unused)
{
	gen6_gtt_pte_t pte = valid ? GEN6_PTE_VALID : 0;
	pte |= HSW_PTE_ADDR_ENCODE(addr);

	switch (level) {
	case I915_CACHE_NONE:
		break;
	case I915_CACHE_WT:
		pte |= HSW_WT_ELLC_LLC_AGE3;
		break;
	default:
		pte |= HSW_WB_ELLC_LLC_AGE3;
		break;
	}

	return pte;
}

#define i915_dma_unmap_single(px, dev) \
	__i915_dma_unmap_single((px)->daddr, dev)

static inline void __i915_dma_unmap_single(dma_addr_t daddr,
					struct drm_device *dev)
{
	struct device *device = &dev->pdev->dev;

	dma_unmap_page(device, daddr, 4096, PCI_DMA_BIDIRECTIONAL);
}

/**
 * i915_dma_map_px_single() - Create a dma mapping for a page table/dir/etc.
 * @px:		Page table/dir/etc to get a DMA map for
 * @dev:	drm device
 *
 * Page table allocations are unified across all gens. They always require a
 * single 4k allocation, as well as a DMA mapping. If we keep the structs
 * symmetric here, the simple macro covers us for every page table type.
 *
 * Return: 0 if success.
 */
#define i915_dma_map_px_single(px, dev) \
	i915_dma_map_page_single((px)->page, (dev), &(px)->daddr)

static inline int i915_dma_map_page_single(struct page *page,
					   struct drm_device *dev,
					   dma_addr_t *daddr)
{
	struct device *device = &dev->pdev->dev;

	*daddr = dma_map_page(device, page, 0, 4096, PCI_DMA_BIDIRECTIONAL);
	return dma_mapping_error(device, *daddr);
}

static void unmap_and_free_pt(struct i915_page_table_entry *pt,
			       struct drm_device *dev)
{
	if (WARN_ON(!pt->page))
		return;

	i915_dma_unmap_single(pt, dev);
	__free_page(pt->page);
	kfree(pt->used_ptes);
	kfree(pt);
}

static struct i915_page_table_entry *alloc_pt_single(struct drm_device *dev)
{
	struct i915_page_table_entry *pt;
	const size_t count = INTEL_INFO(dev)->gen >= 8 ?
		GEN8_PTES_PER_PAGE : I915_PPGTT_PT_ENTRIES;
	int ret = -ENOMEM;

	pt = kzalloc(sizeof(*pt), GFP_KERNEL);
	if (!pt)
		return ERR_PTR(-ENOMEM);

	pt->used_ptes = kcalloc(BITS_TO_LONGS(count), sizeof(*pt->used_ptes),
				GFP_KERNEL);

	if (!pt->used_ptes)
		goto fail_bitmap;

	pt->page = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (!pt->page)
		goto fail_page;

	ret = i915_dma_map_px_single(pt, dev);
	if (ret)
		goto fail_dma;

	return pt;

fail_dma:
	__free_page(pt->page);
fail_page:
	kfree(pt->used_ptes);
fail_bitmap:
	kfree(pt);

	return ERR_PTR(ret);
}

static inline struct i915_page_table_entry *alloc_pt_scratch(struct drm_device *dev)
{
	struct i915_page_table_entry *pt = alloc_pt_single(dev);

	if (!IS_ERR(pt))
		pt->scratch = 1;

	return pt;
}

/**
 * alloc_pt_range() - Allocate a multiple page tables
 * @pd:		The page directory which will have at least @count entries
 *		available to point to the allocated page tables.
 * @pde:	First page directory entry for which we are allocating.
 * @count:	Number of pages to allocate.
 *
 * Allocates multiple page table pages and sets the appropriate entries in the
 * page table structure within the page directory. Function cleans up after
 * itself on any failures.
 *
 * Return: 0 if allocation succeeded.
 */
static int alloc_pt_range(struct i915_page_directory_entry *pd, uint16_t pde, size_t count,
		  struct drm_device *dev)

{
	int i, ret;

	/* 512 is the max page tables per page_directory on any platform.
	 * TODO: make WARN after patch series is done
	 */
	BUG_ON(pde + count > GEN6_PPGTT_PD_ENTRIES);

	for (i = pde; i < pde + count; i++) {
		struct i915_page_table_entry *pt = alloc_pt_single(dev);

		if (IS_ERR(pt)) {
			ret = PTR_ERR(pt);
			goto err_out;
		}
		WARN(pd->page_tables[i],
		     "Leaking page directory entry %d (%pa)\n",
		     i, pd->page_tables[i]);
		pd->page_tables[i] = pt;
	}

	return 0;

err_out:
	while (i--)
		unmap_and_free_pt(pd->page_tables[i], dev);
	return ret;
}

static void unmap_and_free_pd(struct i915_page_directory_entry *pd,
			 struct drm_device *dev)
{
	if (pd->page) {
		i915_dma_unmap_single(pd, dev);
		__free_page(pd->page);
		kfree(pd->used_pdes);
		kfree(pd);
	}
}

static struct i915_page_directory_entry *alloc_pd_single(struct drm_device *dev)
{
	struct i915_page_directory_entry *pd;
	int ret = -ENOMEM;

	pd = kzalloc(sizeof(*pd), GFP_KERNEL);
	if (!pd)
		return ERR_PTR(-ENOMEM);

	pd->used_pdes = kcalloc(BITS_TO_LONGS(GEN8_PDES_PER_PAGE),
				sizeof(*pd->used_pdes), GFP_KERNEL);
	if (!pd->used_pdes)
		goto free_pd;

	pd->page = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (!pd->page)
		goto free_bitmap;

	ret = i915_dma_map_px_single(pd, dev);
	if (ret)
		goto free_page;

	return pd;

free_page:
	__free_page(pd->page);
free_bitmap:
	kfree(pd->used_pdes);
free_pd:
	kfree(pd);

	return ERR_PTR(ret);
}

/* Broadwell Page Directory Pointer Descriptors */
static int gen8_write_pdp(struct intel_engine_cs *ring,
			unsigned entry, dma_addr_t addr,
			bool synchronous)
{
	struct drm_i915_private *dev_priv = ring->dev->dev_private;
	int ret;

	BUG_ON(entry >= 4);

	if (synchronous) {
		I915_WRITE(GEN8_RING_PDP_UDW(ring, entry), upper_32_bits(addr));
		I915_WRITE(GEN8_RING_PDP_LDW(ring, entry), lower_32_bits(addr));
		return 0;
	}

	ret = intel_ring_begin(ring, 6);
	if (ret)
		return ret;

	intel_ring_emit(ring, MI_LOAD_REGISTER_IMM(1));
	intel_ring_emit(ring, GEN8_RING_PDP_UDW(ring, entry));
	intel_ring_emit(ring, upper_32_bits(addr));
	intel_ring_emit(ring, MI_LOAD_REGISTER_IMM(1));
	intel_ring_emit(ring, GEN8_RING_PDP_LDW(ring, entry));
	intel_ring_emit(ring, lower_32_bits(addr));
	intel_ring_advance(ring);

	return 0;
}

static int gen8_mm_switch(struct i915_hw_ppgtt *ppgtt,
			  struct intel_engine_cs *ring,
			  bool synchronous)
{
	int i, ret;

	for (i = GEN8_LEGACY_PDPES - 1; i >= 0; i--) {
		struct i915_page_directory_entry *pd = ppgtt->pdp.page_directory[i];
		dma_addr_t pd_daddr = pd ? pd->daddr : ppgtt->scratch_pd->daddr;
		/* The page directory might be NULL, but we need to clear out
		 * whatever the previous context might have used. */
		ret = gen8_write_pdp(ring, i, pd_daddr, synchronous);
		if (ret)
			return ret;
	}

	return 0;
}

static void gen8_ppgtt_clear_range(struct i915_address_space *vm,
				   uint64_t start,
				   uint64_t length,
				   bool use_scratch)
{
	struct i915_hw_ppgtt *ppgtt =
		container_of(vm, struct i915_hw_ppgtt, base);
	gen8_gtt_pte_t *pt_vaddr, scratch_pte;
	unsigned pdpe = start >> GEN8_PDPE_SHIFT & GEN8_PDPE_MASK;
	unsigned pde = start >> GEN8_PDE_SHIFT & GEN8_PDE_MASK;
	unsigned pte = start >> GEN8_PTE_SHIFT & GEN8_PTE_MASK;
	unsigned num_entries = length >> PAGE_SHIFT;
	unsigned last_pte, i;

	scratch_pte = gen8_pte_encode(ppgtt->base.scratch.addr,
				      I915_CACHE_LLC, use_scratch);

	while (num_entries) {
		struct i915_page_directory_entry *pd;
		struct i915_page_table_entry *pt;
		struct page *page_table;

		if (WARN_ON(!ppgtt->pdp.page_directory[pdpe]))
			continue;

		pd = ppgtt->pdp.page_directory[pdpe];

		if (WARN_ON(!pd->page_tables[pde]))
			continue;

		pt = pd->page_tables[pde];

		if (WARN_ON(!pt->page))
			continue;

		page_table = pt->page;

		last_pte = pte + num_entries;
		if (last_pte > GEN8_PTES_PER_PAGE)
			last_pte = GEN8_PTES_PER_PAGE;

		pt_vaddr = kmap_atomic(page_table);

		for (i = pte; i < last_pte; i++) {
			pt_vaddr[i] = scratch_pte;
			num_entries--;
		}

		if (!HAS_LLC(ppgtt->base.dev))
			drm_clflush_virt_range(pt_vaddr, PAGE_SIZE);
		kunmap_atomic(pt_vaddr);

		pte = 0;
		if (++pde == GEN8_PDES_PER_PAGE) {
			pdpe++;
			pde = 0;
		}
	}
}

static void gen8_ppgtt_insert_entries(struct i915_address_space *vm,
				      struct sg_table *pages,
				      uint64_t start,
				      enum i915_cache_level cache_level,
				      u32 unused)
{
	struct i915_hw_ppgtt *ppgtt =
		container_of(vm, struct i915_hw_ppgtt, base);
	gen8_gtt_pte_t *pt_vaddr;
	unsigned pdpe = start >> GEN8_PDPE_SHIFT & GEN8_PDPE_MASK;
	unsigned pde = start >> GEN8_PDE_SHIFT & GEN8_PDE_MASK;
	unsigned pte = start >> GEN8_PTE_SHIFT & GEN8_PTE_MASK;
	struct sg_page_iter sg_iter;

	pt_vaddr = NULL;

	for_each_sg_page(pages->sgl, &sg_iter, pages->nents, 0) {
		if (WARN_ON(pdpe >= GEN8_LEGACY_PDPES))
			break;

		if (pt_vaddr == NULL) {
			struct i915_page_directory_entry *pd = ppgtt->pdp.page_directory[pdpe];
			struct i915_page_table_entry *pt = pd->page_tables[pde];
			struct page *page_table = pt->page;

			pt_vaddr = kmap_atomic(page_table);
		}

		pt_vaddr[pte] =
			gen8_pte_encode(sg_page_iter_dma_address(&sg_iter),
					cache_level, true);
		if (++pte == GEN8_PTES_PER_PAGE) {
			if (!HAS_LLC(ppgtt->base.dev))
				drm_clflush_virt_range(pt_vaddr, PAGE_SIZE);
			kunmap_atomic(pt_vaddr);
			pt_vaddr = NULL;
			if (++pde == GEN8_PDES_PER_PAGE) {
				pdpe++;
				pde = 0;
			}
			pte = 0;
		}
	}
	if (pt_vaddr) {
		if (!HAS_LLC(ppgtt->base.dev))
			drm_clflush_virt_range(pt_vaddr, PAGE_SIZE);
		kunmap_atomic(pt_vaddr);
	}
}

static void __gen8_do_map_pt(gen8_ppgtt_pde_t * const pde,
			     struct i915_page_table_entry *pt,
			     struct drm_device *dev)
{
	gen8_ppgtt_pde_t entry =
		gen8_pde_encode(dev, pt->daddr, I915_CACHE_LLC);
	*pde = entry;
}

/* It's likely we'll map more than one pagetable at a time. This function will
 * save us unnecessary kmap calls, but do no more functionally than multiple
 * calls to map_pt. */
static void gen8_map_pagetable_range(struct i915_page_directory_entry *pd,
				uint64_t start,
				uint64_t length,
				struct drm_device *dev)
{
	gen8_ppgtt_pde_t * const page_directory = kmap_atomic(pd->page);
	struct i915_page_table_entry *pt;
	uint64_t temp, pde;

	gen8_for_each_pde(pt, pd, start, length, temp, pde)
		__gen8_do_map_pt(page_directory + pde, pt, dev);

	if (!HAS_LLC(dev))
		drm_clflush_virt_range(page_directory, PAGE_SIZE);

	kunmap_atomic(page_directory);
}

static void gen8_free_page_tables(struct i915_page_directory_entry *pd, struct drm_device *dev)
{
	int i;

	if (!pd->page)
		return;

	for_each_set_bit(i, pd->used_pdes, GEN8_PDES_PER_PAGE) {
		if (WARN_ON(!pd->page_tables[i]))
			continue;

		unmap_and_free_pt(pd->page_tables[i], dev);
		pd->page_tables[i] = NULL;
	}
}

static void gen8_ppgtt_unmap_pages(struct i915_hw_ppgtt *ppgtt)
{
	struct pci_dev *hwdev = ppgtt->base.dev->pdev;
	int i, j;

	for_each_set_bit(i, ppgtt->pdp.used_pdpes, GEN8_LEGACY_PDPES) {
		struct i915_page_directory_entry *pd;

		if (WARN_ON(!ppgtt->pdp.page_directory[i]))
			continue;

		pd = ppgtt->pdp.page_directory[i];
		if (!pd->daddr)
			pci_unmap_page(hwdev, pd->daddr, PAGE_SIZE,
					PCI_DMA_BIDIRECTIONAL);

		for_each_set_bit(j, pd->used_pdes, GEN8_PDES_PER_PAGE) {
			struct i915_page_table_entry *pt;
			dma_addr_t addr;

			if (WARN_ON(!pd->page_tables[j]))
				continue;

			pt = pd->page_tables[j];
			addr = pt->daddr;

			if (addr)
				pci_unmap_page(hwdev, addr, PAGE_SIZE,
					       PCI_DMA_BIDIRECTIONAL);
		}
	}
}

static void gen8_ppgtt_free(struct i915_hw_ppgtt *ppgtt)
{
	int i;

	for_each_set_bit(i, ppgtt->pdp.used_pdpes, GEN8_LEGACY_PDPES) {
		if (WARN_ON(!ppgtt->pdp.page_directory[i]))
			continue;

		gen8_free_page_tables(ppgtt->pdp.page_directory[i], ppgtt->base.dev);
		unmap_and_free_pd(ppgtt->pdp.page_directory[i], ppgtt->base.dev);
	}

	unmap_and_free_pt(ppgtt->scratch_pd, ppgtt->base.dev);
	unmap_and_free_pt(ppgtt->scratch_pt, ppgtt->base.dev);
}

static void gen8_ppgtt_cleanup(struct i915_address_space *vm)
{
	struct i915_hw_ppgtt *ppgtt =
		container_of(vm, struct i915_hw_ppgtt, base);

	gen8_ppgtt_unmap_pages(ppgtt);
	gen8_ppgtt_free(ppgtt);
}

static void gen8_initialize_pt(struct i915_hw_ppgtt *ppgtt,
			struct i915_page_table_entry *pt)
{
	gen8_gtt_pte_t *pt_vaddr, scratch_pte;
	int i;

	pt_vaddr = kmap_atomic(pt->page);
	scratch_pte = gen8_pte_encode(ppgtt->base.scratch.addr,
			I915_CACHE_LLC, true);

	for (i = 0; i < GEN8_PTES_PER_PAGE; i++)
		pt_vaddr[i] = scratch_pte;

	if (!HAS_LLC(ppgtt->base.dev))
		drm_clflush_virt_range(pt_vaddr, PAGE_SIZE);
	kunmap_atomic(pt_vaddr);
}

static void gen8_initialize_pd(struct i915_hw_ppgtt *ppgtt,
			struct i915_page_directory_entry *pd)
{
	gen8_ppgtt_pde_t *page_directory;
	struct i915_page_table_entry *pt;
	int i;

	page_directory = kmap_atomic(pd->page);
	pt = ppgtt->scratch_pt;
	for (i = 0; i < GEN8_PDES_PER_PAGE; i++)
		/* Map the PDE to the page table */
		__gen8_do_map_pt(page_directory + i, pt, ppgtt->base.dev);

	if (!HAS_LLC(ppgtt->base.dev))
		drm_clflush_virt_range(page_directory, PAGE_SIZE);
	kunmap_atomic(page_directory);
}

/**
 * gen8_ppgtt_alloc_pagetabs() - Allocate page tables for VA range.
 * @ppgtt:	Master ppgtt structure.
 * @pd:		Page directory for this address range.
 * @start:	Starting virtual address to begin allocations.
 * @length	Size of the allocations.
 * @new_pts:	Bitmap set by function with new allocations. Likely used by the
 *		caller to free on error.
 *
 * Allocate the required number of page tables. Extremely similar to
 * gen8_ppgtt_alloc_page_directories(). The main difference is here we are limited by
 * the page directory boundary (instead of the page directory pointer). That
 * boundary is 1GB virtual. Therefore, unlike gen8_ppgtt_alloc_page_directories(), it is
 * possible, and likely that the caller will need to use multiple calls of this
 * function to achieve the appropriate allocation.
 *
 * Return: 0 if success; negative error code otherwise.
 */
static int gen8_ppgtt_alloc_pagetabs(struct i915_hw_ppgtt *ppgtt,
				     struct i915_page_directory_entry *pd,
				     uint64_t start,
				     uint64_t length,
				     unsigned long *new_pts)
{
	struct i915_page_table_entry *pt;
	uint64_t temp;
	uint32_t pde;

	gen8_for_each_pde(pt, pd, start, length, temp, pde) {
		/* Don't reallocate page tables */
		if (pt) {
			/* Scratch is never allocated this way */
			WARN_ON(pt->scratch);
			continue;
		}

		pt = alloc_pt_single(ppgtt->base.dev);
		if (IS_ERR(pt))
			goto unwind_out;

		gen8_initialize_pt(ppgtt, pt);
		pd->page_tables[pde] = pt;
		set_bit(pde, new_pts);
	}

	return 0;

unwind_out:
	for_each_set_bit(pde, new_pts, GEN8_PDES_PER_PAGE)
		unmap_and_free_pt(pd->page_tables[pde], ppgtt->base.dev);

	return -ENOMEM;
}

/**
 * gen8_ppgtt_alloc_page_directories() - Allocate page directories for VA range.
 * @ppgtt:	Master ppgtt structure.
 * @pdp:	Page directory pointer for this address range.
 * @start:	Starting virtual address to begin allocations.
 * @length	Size of the allocations.
 * @new_pds	Bitmap set by function with new allocations. Likely used by the
 *		caller to free on error.
 *
 * Allocate the required number of page directories starting at the pde index of
 * @start, and ending at the pde index @start + @length. This function will skip
 * over already allocated page directories within the range, and only allocate
 * new ones, setting the appropriate pointer within the pdp as well as the
 * correct position in the bitmap @new_pds.
 *
 * The function will only allocate the pages within the range for a give page
 * directory pointer. In other words, if @start + @length straddles a virtually
 * addressed PDP boundary (512GB for 4k pages), there will be more allocations
 * required by the caller, This is not currently possible, and the BUG in the
 * code will prevent it.
 *
 * Return: 0 if success; negative error code otherwise.
 */
static int gen8_ppgtt_alloc_page_directories(struct i915_hw_ppgtt *ppgtt,
				 struct i915_page_directory_pointer_entry *pdp,
				 uint64_t start,
				 uint64_t length,
				 unsigned long *new_pds)
{
	struct i915_page_directory_entry *pd;
	uint64_t temp;
	uint32_t pdpe;

	BUG_ON(!bitmap_empty(new_pds, GEN8_LEGACY_PDPES));

	/* FIXME: PPGTT container_of won't work for 64b */
	BUG_ON((start + length) > 0x800000000ULL);

	gen8_for_each_pdpe(pd, pdp, start, length, temp, pdpe) {
		if (pd)
			continue;

		pd = alloc_pd_single(ppgtt->base.dev);
		if (IS_ERR(pd))
			goto unwind_out;

		gen8_initialize_pd(ppgtt, pd);
		pdp->page_directory[pdpe] = pd;
		set_bit(pdpe, new_pds);
	}

	return 0;

unwind_out:
	for_each_set_bit(pdpe, new_pds, GEN8_LEGACY_PDPES)
		unmap_and_free_pd(pdp->page_directory[pdpe], ppgtt->base.dev);

	return -ENOMEM;
}

static inline void
free_gen8_temp_bitmaps(unsigned long *new_pds, unsigned long **new_pts)
{
	int i;

	for (i = 0; i < GEN8_LEGACY_PDPES; i++)
		kfree(new_pts[i]);
	kfree(new_pts);
	kfree(new_pds);
}

/* Fills in the page directory bitmap, ant the array of page tables bitmap. Both
 * of these are based on the number of PDPEs in the system.
 */
int __must_check alloc_gen8_temp_bitmaps(unsigned long **new_pds,
					 unsigned long ***new_pts)
{
	int i;
	unsigned long *pds;
	unsigned long **pts;

	pds = kcalloc(BITS_TO_LONGS(GEN8_LEGACY_PDPES), sizeof(unsigned long), GFP_KERNEL);
	if (!pds)
		return -ENOMEM;

	pts = kcalloc(GEN8_PDES_PER_PAGE, sizeof(unsigned long *), GFP_KERNEL);
	if (!pts) {
		kfree(pds);
		return -ENOMEM;
	}

	for (i = 0; i < GEN8_LEGACY_PDPES; i++) {
		pts[i] = kcalloc(BITS_TO_LONGS(GEN8_PDES_PER_PAGE),
				 sizeof(unsigned long), GFP_KERNEL);
		if (!pts[i])
			goto err_out;
	}

	*new_pds = pds;
	*new_pts = (unsigned long **)pts;

	return 0;

err_out:
	free_gen8_temp_bitmaps(pds, pts);
	return -ENOMEM;
}

static int gen8_alloc_va_range(struct i915_address_space *vm,
			       uint64_t start,
			       uint64_t length)
{
	struct i915_hw_ppgtt *ppgtt =
		container_of(vm, struct i915_hw_ppgtt, base);
	unsigned long *new_page_dirs, **new_page_tables;
	struct i915_page_directory_entry *pd;
	const uint64_t orig_start = start;
	const uint64_t orig_length = length;
	uint64_t temp;
	uint32_t pdpe;
	int ret;

#ifndef CONFIG_64BIT
	/* Disallow 64b address on 32b platforms. Nothing is wrong with doing
	 * this in hardware, but a lot of the drm code is not prepared to handle
	 * 64b offset on 32b platforms.
	 * This will be addressed when 48b PPGTT is added */
	if (start + length > 0x100000000ULL)
		return -E2BIG;
#endif

	/* Wrap is never okay since we can only represent 48b, and we don't
	 * actually use the other side of the canonical address space.
	 */
	if (WARN_ON(start + length < start))
		return -ERANGE;

	ret = alloc_gen8_temp_bitmaps(&new_page_dirs, &new_page_tables);
	if (ret)
		return ret;

	/* Do the allocations first so we can easily bail out */
	ret = gen8_ppgtt_alloc_page_directories(ppgtt, &ppgtt->pdp, start, length,
					new_page_dirs);
	if (ret) {
		free_gen8_temp_bitmaps(new_page_dirs, new_page_tables);
		return ret;
	}

	/* For every page directory referenced, allocate page tables */
	gen8_for_each_pdpe(pd, &ppgtt->pdp, start, length, temp, pdpe) {
		bitmap_zero(new_page_tables[pdpe], GEN8_PDES_PER_PAGE);
		ret = gen8_ppgtt_alloc_pagetabs(ppgtt, pd, start, length,
						new_page_tables[pdpe]);
		if (ret)
			goto err_out;
	}

	start = orig_start;
	length = orig_length;

	/* Allocations have completed successfully, so set the bitmaps, and do
	 * the mappings. */
	gen8_for_each_pdpe(pd, &ppgtt->pdp, start, length, temp, pdpe) {
		gen8_ppgtt_pde_t *const page_directory = kmap_atomic(pd->page);
		struct i915_page_table_entry *pt;
		uint64_t pd_len = gen8_clamp_pd(start, length);
		uint64_t pd_start = start;
		uint32_t pde;

		/* Every pd should be allocated, we just did that above. */
		BUG_ON(!pd);

		gen8_for_each_pde(pt, pd, pd_start, pd_len, temp, pde) {
			/* Same reasoning as pd */
			BUG_ON(!pt);
			BUG_ON(!pd_len);
			BUG_ON(!gen8_pte_count(pd_start, pd_len));

			/* Set our used ptes within the page table */
			bitmap_set(pt->used_ptes,
				   gen8_pte_index(pd_start),
				   gen8_pte_count(pd_start, pd_len));

			/* Our pde is now pointing to the pagetable, pt */
			set_bit(pde, pd->used_pdes);

			/* Map the PDE to the page table */
			__gen8_do_map_pt(page_directory + pde, pt, vm->dev);

			/* NB: We haven't yet mapped ptes to pages. At this
			 * point we're still relying on insert_entries() */
		}

		if (!HAS_LLC(vm->dev))
			drm_clflush_virt_range(page_directory, PAGE_SIZE);

		kunmap_atomic(page_directory);

		set_bit(pdpe, ppgtt->pdp.used_pdpes);
	}

	free_gen8_temp_bitmaps(new_page_dirs, new_page_tables);
	return 0;

err_out:
	gen8_ppgtt_free(ppgtt);

	for_each_set_bit(pdpe, new_page_dirs, GEN8_LEGACY_PDPES)
		unmap_and_free_pd(ppgtt->pdp.page_directory[pdpe], vm->dev);

	free_gen8_temp_bitmaps(new_page_dirs, new_page_tables);
	return ret;
}

/**
 * GEN8 legacy ppgtt programming is accomplished through a max 4 PDP registers
 * with a net effect resembling a 2-level page table in normal x86 terms. Each
 * PDP represents 1GB of memory 4 * 512 * 512 * 4096 = 4GB legacy 32b address
 * space.
 *
 */
static int gen8_ppgtt_init_common(struct i915_hw_ppgtt *ppgtt, uint64_t size)
{
	ppgtt->scratch_pd = alloc_pt_scratch(ppgtt->base.dev);
	if (IS_ERR(ppgtt->scratch_pd))
		return PTR_ERR(ppgtt->scratch_pd);

	ppgtt->scratch_pt = alloc_pt_scratch(ppgtt->base.dev);
	if (IS_ERR(ppgtt->scratch_pt))
		return PTR_ERR(ppgtt->scratch_pt);

	gen8_initialize_pt(ppgtt, ppgtt->scratch_pt);
	gen8_initialize_pd(ppgtt,
		(struct i915_page_directory_entry *)ppgtt->scratch_pd);

	ppgtt->base.start = 0;
	ppgtt->base.total = size;
	ppgtt->base.cleanup = gen8_ppgtt_cleanup;
	ppgtt->base.insert_entries = gen8_ppgtt_insert_entries;

	ppgtt->switch_mm = gen8_mm_switch;

	return 0;
}

static int gen8_aliasing_ppgtt_init(struct i915_hw_ppgtt *ppgtt)
{
	struct drm_device *dev = ppgtt->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct i915_page_directory_entry *pd;
	uint64_t temp, start = 0, size = dev_priv->gtt.base.total;
	uint32_t pdpe;
	int ret;

	ret = gen8_ppgtt_init_common(ppgtt, dev_priv->gtt.base.total);
	if (ret)
		return ret;

	/* Aliasing PPGTT has to always work and be mapped because of the way we
	 * use RESTORE_INHIBIT in the context switch. This will be fixed
	 * eventually. */
	ret = gen8_alloc_va_range(&ppgtt->base, start, size);
	if (ret) {
		unmap_and_free_pt(ppgtt->scratch_pd, ppgtt->base.dev);
		return ret;
	}

	gen8_for_each_pdpe(pd, &ppgtt->pdp, start, size, temp, pdpe)
		gen8_map_pagetable_range(pd, start, size, ppgtt->base.dev);

	ppgtt->base.allocate_va_range = NULL;
	ppgtt->base.clear_range = gen8_ppgtt_clear_range;
	ppgtt->base.clear_range(&ppgtt->base, 0, ppgtt->base.total, true);

	return 0;
}

static int gen8_ppgtt_init(struct i915_hw_ppgtt *ppgtt)
{
	int ret;

	ret = gen8_ppgtt_init_common(ppgtt, (1ULL << 32));
	if (ret)
		return ret;

	ppgtt->base.allocate_va_range = gen8_alloc_va_range;
	ppgtt->base.clear_range = gen8_ppgtt_clear_range;

	return 0;
}

static void gen6_dump_ppgtt(struct i915_hw_ppgtt *ppgtt, struct seq_file *m)
{
	struct i915_address_space *vm = &ppgtt->base;
	struct i915_page_table_entry *unused;
	gen6_gtt_pte_t scratch_pte;
	uint32_t pd_entry;
	uint32_t  pte, pde, temp;
	uint32_t start = ppgtt->base.start, length = ppgtt->base.total;

	scratch_pte = vm->pte_encode(vm->scratch.addr, I915_CACHE_LLC, true, 0);

	gen6_for_each_pde(unused, &ppgtt->pd, start, length, temp, pde) {
		u32 expected;
		gen6_gtt_pte_t *pt_vaddr;
		dma_addr_t pt_addr = ppgtt->pd.page_tables[pde]->daddr;
		pd_entry = readl(ppgtt->pd_addr + pde);
		expected = (GEN6_PDE_ADDR_ENCODE(pt_addr) | GEN6_PDE_VALID);

		if (pd_entry != expected)
			seq_printf(m, "\tPDE #%d mismatch: Actual PDE: %x Expected PDE: %x\n",
				   pde,
				   pd_entry,
				   expected);
		seq_printf(m, "\tPDE: %x\n", pd_entry);

		pt_vaddr = kmap_atomic(ppgtt->pd.page_tables[pde]->page);
		for (pte = 0; pte < I915_PPGTT_PT_ENTRIES; pte+=4) {
			unsigned long va =
				(pde * PAGE_SIZE * I915_PPGTT_PT_ENTRIES) +
				(pte * PAGE_SIZE);
			int i;
			bool found = false;
			for (i = 0; i < 4; i++)
				if (pt_vaddr[pte + i] != scratch_pte)
					found = true;
			if (!found)
				continue;

			seq_printf(m, "\t\t0x%lx [%03d,%04d]: =", va, pde, pte);
			for (i = 0; i < 4; i++) {
				if (pt_vaddr[pte + i] != scratch_pte)
					seq_printf(m, " %08x", pt_vaddr[pte + i]);
				else
					seq_puts(m, "  SCRATCH ");
			}
			seq_puts(m, "\n");
		}
		kunmap_atomic(pt_vaddr);
	}
}

/* Write pde (index) from the page directory @pd to the page table @pt */
static void gen6_write_pdes(struct i915_page_directory_entry *pd,
			    const int pde, struct i915_page_table_entry *pt)
{
	struct i915_hw_ppgtt *ppgtt =
		container_of(pd, struct i915_hw_ppgtt, pd);
	u32 pd_entry;

	pd_entry = GEN6_PDE_ADDR_ENCODE(pt->daddr);
	pd_entry |= GEN6_PDE_VALID;

	writel(pd_entry, ppgtt->pd_addr + pde);

	/* XXX: Caller needs to make sure the write completes if necessary */
}

/* Write all the page tables found in the ppgtt structure to incrementing page
 * directories. */
static void gen6_write_page_range(struct drm_i915_private *dev_priv,
				struct i915_page_directory_entry *pd, uint32_t start, uint32_t length)
{
	struct i915_page_table_entry *pt;
	uint32_t pde, temp;

	gen6_for_each_pde(pt, pd, start, length, temp, pde)
		gen6_write_pdes(pd, pde, pt);

	/* Make sure write is complete before other code can use this page
	 * table. Also require for WC mapped PTEs */
	readl(dev_priv->gtt.gsm);
}

static uint32_t get_pd_offset(struct i915_hw_ppgtt *ppgtt)
{
	BUG_ON(ppgtt->pd.pd_offset & 0x3f);

	return (ppgtt->pd.pd_offset / 64) << 16;
}

static int hsw_mm_switch(struct i915_hw_ppgtt *ppgtt,
			 struct intel_engine_cs *ring,
			 bool synchronous)
{
	struct drm_device *dev = ppgtt->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	/* If we're in reset, we can assume the GPU is sufficiently idle to
	 * manually frob these bits. Ideally we could use the ring functions,
	 * except our error handling makes it quite difficult (can't use
	 * intel_ring_begin, ring->flush, or intel_ring_advance)
	 *
	 * FIXME: We should try not to special case reset
	 */
	if (synchronous ||
	    i915_reset_in_progress(&dev_priv->gpu_error)) {
		WARN_ON(ppgtt != dev_priv->mm.aliasing_ppgtt);
		I915_WRITE(RING_PP_DIR_DCLV(ring), PP_DIR_DCLV_2G);
		I915_WRITE(RING_PP_DIR_BASE(ring), get_pd_offset(ppgtt));
		POSTING_READ(RING_PP_DIR_BASE(ring));
		return 0;
	}

	/* NB: TLBs must be flushed and invalidated before a switch */
	ret = ring->flush(ring, I915_GEM_GPU_DOMAINS, I915_GEM_GPU_DOMAINS);
	if (ret)
		return ret;

	ret = intel_ring_begin(ring, 6);
	if (ret)
		return ret;

	intel_ring_emit(ring, MI_LOAD_REGISTER_IMM(2));
	intel_ring_emit(ring, RING_PP_DIR_DCLV(ring));
	intel_ring_emit(ring, PP_DIR_DCLV_2G);
	intel_ring_emit(ring, RING_PP_DIR_BASE(ring));
	intel_ring_emit(ring, get_pd_offset(ppgtt));
	intel_ring_emit(ring, MI_NOOP);
	intel_ring_advance(ring);

	return 0;
}

static int gen7_mm_switch(struct i915_hw_ppgtt *ppgtt,
			  struct intel_engine_cs *ring,
			  bool synchronous)
{
	struct drm_device *dev = ppgtt->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	/* If we're in reset, we can assume the GPU is sufficiently idle to
	 * manually frob these bits. Ideally we could use the ring functions,
	 * except our error handling makes it quite difficult (can't use
	 * intel_ring_begin, ring->flush, or intel_ring_advance)
	 *
	 * FIXME: We should try not to special case reset
	 */
	if (synchronous ||
	    i915_reset_in_progress(&dev_priv->gpu_error)) {
		WARN_ON(ppgtt != dev_priv->mm.aliasing_ppgtt);
		I915_WRITE(RING_PP_DIR_DCLV(ring), PP_DIR_DCLV_2G);
		I915_WRITE(RING_PP_DIR_BASE(ring), get_pd_offset(ppgtt));
		POSTING_READ(RING_PP_DIR_BASE(ring));
		return 0;
	}

	/* NB: TLBs must be flushed and invalidated before a switch */
	ret = ring->flush(ring, I915_GEM_GPU_DOMAINS, I915_GEM_GPU_DOMAINS);
	if (ret)
		return ret;

	ret = intel_ring_begin(ring, 6);
	if (ret)
		return ret;

	intel_ring_emit(ring, MI_LOAD_REGISTER_IMM(2));
	intel_ring_emit(ring, RING_PP_DIR_DCLV(ring));
	intel_ring_emit(ring, PP_DIR_DCLV_2G);
	intel_ring_emit(ring, RING_PP_DIR_BASE(ring));
	intel_ring_emit(ring, get_pd_offset(ppgtt));
	intel_ring_emit(ring, MI_NOOP);
	intel_ring_advance(ring);

	/* XXX: RCS is the only one to auto invalidate the TLBs? */
	if (ring->id != RCS) {
		ret = ring->flush(ring, I915_GEM_GPU_DOMAINS, I915_GEM_GPU_DOMAINS);
		if (ret)
			return ret;
	}

	return 0;
}

static int gen6_mm_switch(struct i915_hw_ppgtt *ppgtt,
			  struct intel_engine_cs *ring,
			  bool synchronous)
{
	struct drm_device *dev = ppgtt->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (!synchronous)
		return 0;

	I915_WRITE(RING_PP_DIR_DCLV(ring), PP_DIR_DCLV_2G);
	I915_WRITE(RING_PP_DIR_BASE(ring), get_pd_offset(ppgtt));

	POSTING_READ(RING_PP_DIR_DCLV(ring));

	return 0;
}

static void gen8_ppgtt_enable(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_engine_cs *ring;
	int j;

	for_each_ring(ring, dev_priv, j) {
		I915_WRITE(RING_MODE_GEN7(ring),
			   _MASKED_BIT_ENABLE(GFX_PPGTT_ENABLE));
	}
}

static void gen7_ppgtt_enable(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_engine_cs *ring;
	uint32_t ecochk, ecobits;
	int i;

	ecobits = I915_READ(GAC_ECO_BITS);
	I915_WRITE(GAC_ECO_BITS, ecobits | ECOBITS_PPGTT_CACHE64B);

	ecochk = I915_READ(GAM_ECOCHK);
	if (IS_HASWELL(dev)) {
		ecochk |= ECOCHK_PPGTT_WB_HSW;
	} else {
		ecochk |= ECOCHK_PPGTT_LLC_IVB;
		ecochk &= ~ECOCHK_PPGTT_GFDT_IVB;
	}
	I915_WRITE(GAM_ECOCHK, ecochk);

	for_each_ring(ring, dev_priv, i) {
		/* GFX_MODE is per-ring on gen7+ */
		I915_WRITE(RING_MODE_GEN7(ring),
			   _MASKED_BIT_ENABLE(GFX_PPGTT_ENABLE));
	}
}

static void gen6_ppgtt_enable(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	uint32_t ecochk, gab_ctl, ecobits;

	ecobits = I915_READ(GAC_ECO_BITS);
	I915_WRITE(GAC_ECO_BITS, ecobits | ECOBITS_SNB_BIT |
		   ECOBITS_PPGTT_CACHE64B);

	gab_ctl = I915_READ(GAB_CTL);
	I915_WRITE(GAB_CTL, gab_ctl | GAB_CTL_CONT_AFTER_PAGEFAULT);

	ecochk = I915_READ(GAM_ECOCHK);
	I915_WRITE(GAM_ECOCHK, ecochk | ECOCHK_SNB_BIT | ECOCHK_PPGTT_CACHE64B);

	I915_WRITE(GFX_MODE, _MASKED_BIT_ENABLE(GFX_PPGTT_ENABLE));
}

/* PPGTT support for Sandybdrige/Gen6 and later */
static void gen6_ppgtt_clear_range(struct i915_address_space *vm,
				   uint64_t start,
				   uint64_t length,
				   bool use_scratch)
{
	struct i915_hw_ppgtt *ppgtt =
		container_of(vm, struct i915_hw_ppgtt, base);
	gen6_gtt_pte_t *pt_vaddr, scratch_pte;
	unsigned first_entry = start >> PAGE_SHIFT;
	unsigned num_entries = length >> PAGE_SHIFT;
	unsigned act_pt = first_entry / I915_PPGTT_PT_ENTRIES;
	unsigned first_pte = first_entry % I915_PPGTT_PT_ENTRIES;
	unsigned last_pte, i;

	scratch_pte = vm->pte_encode(vm->scratch.addr, I915_CACHE_LLC, true, 0);

	while (num_entries) {
		last_pte = first_pte + num_entries;
		if (last_pte > I915_PPGTT_PT_ENTRIES)
			last_pte = I915_PPGTT_PT_ENTRIES;

		pt_vaddr = kmap_atomic(ppgtt->pd.page_tables[act_pt]->page);

		for (i = first_pte; i < last_pte; i++)
			pt_vaddr[i] = scratch_pte;

		kunmap_atomic(pt_vaddr);

		num_entries -= last_pte - first_pte;
		first_pte = 0;
		act_pt++;
	}
}

static void gen6_ppgtt_insert_entries(struct i915_address_space *vm,
				      struct sg_table *pages,
				      uint64_t start,
				      enum i915_cache_level cache_level,
				      u32 flags)
{
	struct i915_hw_ppgtt *ppgtt =
		container_of(vm, struct i915_hw_ppgtt, base);
	gen6_gtt_pte_t *pt_vaddr;
	unsigned first_entry = start >> PAGE_SHIFT;
	unsigned act_pt = first_entry / I915_PPGTT_PT_ENTRIES;
	unsigned act_pte = first_entry % I915_PPGTT_PT_ENTRIES;
	struct sg_page_iter sg_iter;

	pt_vaddr = NULL;
	for_each_sg_page(pages->sgl, &sg_iter, pages->nents, 0) {
		if (pt_vaddr == NULL)
			pt_vaddr = kmap_atomic(ppgtt->pd.page_tables[act_pt]->page);

		pt_vaddr[act_pte] =
			vm->pte_encode(sg_page_iter_dma_address(&sg_iter),
				       cache_level, true, flags);

		if (++act_pte == I915_PPGTT_PT_ENTRIES) {
			kunmap_atomic(pt_vaddr);
			pt_vaddr = NULL;
			act_pt++;
			act_pte = 0;
		}
	}
	if (pt_vaddr)
		kunmap_atomic(pt_vaddr);
}

static void gen6_ppgtt_unmap_pages(struct i915_hw_ppgtt *ppgtt)
{
	struct i915_page_table_entry *pt;
	uint32_t pde;

	gen6_for_all_pdes(pt, ppgtt, pde) {
		if (pt != ppgtt->scratch_pt)
			pci_unmap_page(ppgtt->base.dev->pdev,
				pt->daddr,
				4096, PCI_DMA_BIDIRECTIONAL);
	}
}

/* PDE TLBs are a pain invalidate pre GEN8. It requires a context reload. If we
 * are switching between contexts with the same LRCA, we also must do a force
 * restore.
 */
static inline void mark_tlbs_dirty(struct i915_hw_ppgtt *ppgtt)
{
	/* If current vm != vm, */
	ppgtt->pd_dirty_rings = INTEL_INFO(ppgtt->base.dev)->ring_mask;
}

static int gen6_alloc_va_range(struct i915_address_space *vm,
			       uint64_t start, uint64_t length)
{
	DECLARE_BITMAP(new_page_tables, GEN6_PPGTT_PD_ENTRIES);
	struct drm_device *dev = vm->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct i915_hw_ppgtt *ppgtt =
				container_of(vm, struct i915_hw_ppgtt, base);
	struct i915_page_table_entry *pt;
	const uint32_t start_save = start, length_save = length;
	uint32_t pde, temp;
	int ret;

	BUG_ON(upper_32_bits(start));

	bitmap_zero(new_page_tables, GEN6_PPGTT_PD_ENTRIES);

	/* The allocation is done in two stages so that we can bail out with
	 * minimal amount of pain. The first stage finds new page tables that
	 * need allocation. The second stage marks use ptes within the page
	 * tables.
	 */
	gen6_for_each_pde(pt, &ppgtt->pd, start, length, temp, pde) {
		if (pt != ppgtt->scratch_pt) {
			WARN_ON(bitmap_empty(pt->used_ptes, I915_PPGTT_PT_ENTRIES));
			continue;
		}

		/* We've already allocated a page table */
		WARN_ON(!bitmap_empty(pt->used_ptes, I915_PPGTT_PT_ENTRIES));

		pt = alloc_pt_single(dev);
		if (IS_ERR(pt)) {
			ret = PTR_ERR(pt);
			goto unwind_out;
		}

		ppgtt->pd.page_tables[pde] = pt;
		set_bit(pde, new_page_tables);
		trace_i915_page_table_entry_alloc(vm, pde, start, GEN6_PDE_SHIFT);
	}

	start = start_save;
	length = length_save;

	gen6_for_each_pde(pt, &ppgtt->pd, start, length, temp, pde) {
		DECLARE_BITMAP(tmp_bitmap, I915_PPGTT_PT_ENTRIES);

		bitmap_zero(tmp_bitmap, I915_PPGTT_PT_ENTRIES);
		bitmap_set(tmp_bitmap, gen6_pte_index(start),
			   gen6_pte_count(start, length));

		if (test_and_clear_bit(pde, new_page_tables))
			gen6_write_pdes(&ppgtt->pd, pde, pt);

		trace_i915_page_table_entry_map(vm, pde, pt,
					 gen6_pte_index(start),
					 gen6_pte_count(start, length),
					 I915_PPGTT_PT_ENTRIES);
		bitmap_or(pt->used_ptes, tmp_bitmap, pt->used_ptes,
				I915_PPGTT_PT_ENTRIES);
	}

	WARN_ON(!bitmap_empty(new_page_tables, GEN6_PPGTT_PD_ENTRIES));

	/* Make sure write is complete before other code can use this page
	 * table. Also require for WC mapped PTEs */
	readl(dev_priv->gtt.gsm);

	mark_tlbs_dirty(ppgtt);
	return 0;

unwind_out:
	for_each_set_bit(pde, new_page_tables, GEN6_PPGTT_PD_ENTRIES) {
		struct i915_page_table_entry *pt = ppgtt->pd.page_tables[pde];

		ppgtt->pd.page_tables[pde] = NULL;
		unmap_and_free_pt(pt, vm->dev);
	}

	mark_tlbs_dirty(ppgtt);
	return ret;
}

static void gen6_ppgtt_free(struct i915_hw_ppgtt *ppgtt)
{
	struct i915_page_table_entry *pt;
	uint32_t pde;

	gen6_for_all_pdes(pt, ppgtt, pde) {
		if (pt != ppgtt->scratch_pt)
			unmap_and_free_pt(pt, ppgtt->base.dev);
	}

	unmap_and_free_pt(ppgtt->scratch_pt, ppgtt->base.dev);
	unmap_and_free_pd(&ppgtt->pd, ppgtt->base.dev);
}

static void gen6_ppgtt_cleanup(struct i915_address_space *vm)
{
	struct i915_hw_ppgtt *ppgtt =
		container_of(vm, struct i915_hw_ppgtt, base);

	drm_mm_remove_node(&ppgtt->node);

	gen6_ppgtt_unmap_pages(ppgtt);
	gen6_ppgtt_free(ppgtt);
}

static int gen6_ppgtt_allocate_page_directories(struct i915_hw_ppgtt *ppgtt)
{
	struct drm_device *dev = ppgtt->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	bool retried = false;
	int ret;

	/* PPGTT PDEs reside in the GGTT and consists of 512 entries. The
	 * allocator works in address space sizes, so it's multiplied by page
	 * size. We allocate at the top of the GTT to avoid fragmentation.
	 */
	BUG_ON(!drm_mm_initialized(&dev_priv->gtt.base.mm));
	ppgtt->scratch_pt = alloc_pt_scratch(ppgtt->base.dev);
	if (IS_ERR(ppgtt->scratch_pt))
		return PTR_ERR(ppgtt->scratch_pt);
alloc:
	ret = drm_mm_insert_node_in_range_generic(&dev_priv->gtt.base.mm,
						  &ppgtt->node, GEN6_PD_SIZE,
						  GEN6_PD_ALIGN, 0,
						  0, dev_priv->gtt.base.total,
						  DRM_MM_TOPDOWN);
	if (ret == -ENOSPC && !retried) {
		ret = i915_gem_evict_something(dev, &dev_priv->gtt.base,
					       GEN6_PD_SIZE, GEN6_PD_ALIGN,
					       I915_CACHE_NONE,
					       0, dev_priv->gtt.base.total,
					       0);
		if (ret)
			goto err_out;

		retried = true;
		goto alloc;
	}

	if (ret)
		goto err_out;


	if (ppgtt->node.start < dev_priv->gtt.mappable_end)
		DRM_DEBUG("Forced to use aperture for PDEs\n");

	return 0;

err_out:
	unmap_and_free_pt(ppgtt->scratch_pt, ppgtt->base.dev);
	return ret;
}

static int gen6_ppgtt_alloc(struct i915_hw_ppgtt *ppgtt)
{
	int ret;

	ret = gen6_ppgtt_allocate_page_directories(ppgtt);
	if (ret)
		return ret;

	return 0;
}

static void gen6_scratch_va_range(struct i915_hw_ppgtt *ppgtt,
				  uint64_t start, uint64_t length)
{
	struct i915_page_table_entry *unused;
	uint32_t pde, temp;

	gen6_for_each_pde(unused, &ppgtt->pd, start, length, temp, pde)
		ppgtt->pd.page_tables[pde] = ppgtt->scratch_pt;
}

static int gen6_ppgtt_init(struct i915_hw_ppgtt *ppgtt, bool aliasing)
{
	struct drm_device *dev = ppgtt->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	ppgtt->base.pte_encode = dev_priv->gtt.base.pte_encode;
	if (IS_GEN6(dev)) {
		ppgtt->switch_mm = gen6_mm_switch;
	} else if (IS_HASWELL(dev)) {
		ppgtt->switch_mm = hsw_mm_switch;
	} else if (IS_GEN7(dev)) {
		ppgtt->switch_mm = gen7_mm_switch;
	} else
		BUG();

	ret = gen6_ppgtt_alloc(ppgtt);
	if (ret)
		return ret;

	if (aliasing) {
		/* preallocate all pts */
		ret = alloc_pt_range(&ppgtt->pd, 0, GEN6_PPGTT_PD_ENTRIES,
				ppgtt->base.dev);

		if (ret) {
			unmap_and_free_pt(ppgtt->scratch_pt, ppgtt->base.dev);
			drm_mm_remove_node(&ppgtt->node);
			return ret;
		}
	}

	ppgtt->base.allocate_va_range = aliasing ? NULL : gen6_alloc_va_range;
	ppgtt->base.clear_range = gen6_ppgtt_clear_range;
	ppgtt->base.insert_entries = gen6_ppgtt_insert_entries;
	ppgtt->base.cleanup = gen6_ppgtt_cleanup;
	ppgtt->base.start = 0;
	ppgtt->base.total = GEN6_PPGTT_PD_ENTRIES * I915_PPGTT_PT_ENTRIES * PAGE_SIZE;
	ppgtt->debug_dump = gen6_dump_ppgtt;

	ppgtt->pd.pd_offset =
		ppgtt->node.start / PAGE_SIZE * sizeof(gen6_gtt_pte_t);

	ppgtt->pd_addr = (gen6_gtt_pte_t __iomem *)dev_priv->gtt.gsm +
		ppgtt->pd.pd_offset / sizeof(gen6_gtt_pte_t);

	if (aliasing)
		ppgtt->base.clear_range(&ppgtt->base, 0, ppgtt->base.total, true);
	else
		gen6_scratch_va_range(ppgtt, 0, ppgtt->base.total);

	gen6_write_page_range(dev_priv, &ppgtt->pd, 0, ppgtt->base.total);

	DRM_DEBUG_DRIVER("Allocated pde space (%ldM) at GTT entry: %lx\n",
			 ppgtt->node.size >> 20,
			 ppgtt->node.start / PAGE_SIZE);

	DRM_DEBUG("Adding PPGTT at offset %x\n",
		  ppgtt->pd.pd_offset << 10);

	return 0;
}

static int __hw_ppgtt_init(struct drm_device *dev, struct i915_hw_ppgtt *ppgtt,
		bool aliasing)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	ppgtt->base.dev = dev;
	ppgtt->base.scratch = dev_priv->gtt.base.scratch;

	if (INTEL_INFO(dev)->gen < 8)
		return gen6_ppgtt_init(ppgtt, aliasing);
	else if (aliasing)
		return gen8_aliasing_ppgtt_init(ppgtt);
	else
		return gen8_ppgtt_init(ppgtt);
}

int i915_ppgtt_init(struct drm_device *dev, struct i915_hw_ppgtt *ppgtt)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret = 0;

	ret = __hw_ppgtt_init(dev, ppgtt, false);
	if (ret == 0) {
		kref_init(&ppgtt->ref);
		drm_mm_init(&ppgtt->base.mm, ppgtt->base.start,
			    ppgtt->base.total);
		i915_init_vm(dev_priv, &ppgtt->base);

		INIT_LIST_HEAD(&ppgtt->vma_list);
	}

	return ret;
}

int i915_ppgtt_init_hw(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_engine_cs *ring;
	struct i915_hw_ppgtt *ppgtt = dev_priv->mm.aliasing_ppgtt;
	int i, ret = 0;

	/* In the case of execlists, PPGTT is enabled by the context descriptor
	 * and the PDPs are contained within the context itself.  We don't
	 * need to do anything here. */
	if (i915.enable_execlists)
		return 0;

	if (!USES_PPGTT(dev))
		return 0;

	if (IS_GEN6(dev))
		gen6_ppgtt_enable(dev);
	else if (IS_GEN7(dev))
		gen7_ppgtt_enable(dev);
	else if (INTEL_INFO(dev)->gen >= 8)
		gen8_ppgtt_enable(dev);
	else
		WARN_ON(1);

	if (ppgtt) {
		for_each_ring(ring, dev_priv, i) {
			ret = ppgtt->switch_mm(ppgtt, ring, true);
			if (ret != 0)
				return ret;

			/*
			 * Make sure the context switch (if one actually happened)
			 * gets wrapped up and finished rather than hanging around
			 * and confusing things later.
			 */
			if (ring->outstanding_lazy_request) {
				ret = i915_add_request_no_flush(ring);
				if (ret)
					return ret;
			}
		}
	}

	return ret;
}
struct i915_hw_ppgtt *
i915_ppgtt_create(struct drm_device *dev, struct drm_i915_file_private *fpriv)
{
	struct i915_hw_ppgtt *ppgtt;
	int ret;

	ppgtt = kzalloc(sizeof(*ppgtt), GFP_KERNEL);
	if (!ppgtt)
		return ERR_PTR(-ENOMEM);

	ret = i915_ppgtt_init(dev, ppgtt);
	if (ret) {
		kfree(ppgtt);
		return ERR_PTR(ret);
	}

	ppgtt->file_priv = fpriv;

	return ppgtt;
}

void  i915_ppgtt_release(struct kref *kref)
{
	struct i915_hw_ppgtt *ppgtt =
		container_of(kref, struct i915_hw_ppgtt, ref);

	/* vmas should already be unbound */
	WARN_ON(!list_empty(&ppgtt->base.active_list));
	WARN_ON(!list_empty(&ppgtt->base.inactive_list));
	WARN_ON(!list_empty(&ppgtt->vma_list));

	list_del(&ppgtt->base.global_link);
	drm_mm_takedown(&ppgtt->base.mm);

	ppgtt->base.cleanup(&ppgtt->base);

	kfree(ppgtt);
}

void
i915_ppgtt_destroy(struct i915_hw_ppgtt *ppgtt)
{
	struct i915_vma *vma, *tmp;
	struct i915_address_space *vm;
	int ret;

	if (!ppgtt)
		return;

	vm = &ppgtt->base;

	/*
	 * If this fires it means that the context reference counting went
	 * awry.
	 */
	WARN_ON(!list_empty(&ppgtt->base.active_list));

	if (!list_empty(&ppgtt->vma_list))
		list_for_each_entry_safe(vma, tmp, &ppgtt->vma_list, vm_link) {
			WARN_ON(vma->pin_count != 0);
			/*
			 * The object should be inactive at this point, thus
			 * its pin_count should be 0. We will zero it anyway
			 * make sure that the unbind call succeeds.
			 */
			vma->pin_count = 0;
			ret = i915_vma_unbind(vma);
		}

	i915_ppgtt_put(ppgtt);
}

static void
ppgtt_bind_vma(struct i915_vma *vma,
	       enum i915_cache_level cache_level,
	       u32 flags)
{
	/* Currently applicable only to VLV */
	if (vma->obj->gt_ro)
		flags |= PTE_READ_ONLY;

	vma->vm->insert_entries(vma->vm, vma->obj->pages, vma->node.start,
				cache_level, flags);
}

static void ppgtt_unbind_vma(struct i915_vma *vma)
{
	vma->vm->clear_range(vma->vm,
			     vma->node.start,
			     vma->obj->base.size,
			     true);
}

extern int intel_iommu_gfx_mapped;
/* Certain Gen5 chipsets require require idling the GPU before
 * unmapping anything from the GTT when VT-d is enabled.
 */
static inline bool needs_idle_maps(struct drm_device *dev)
{
#ifdef CONFIG_INTEL_IOMMU
	/* Query intel_iommu to see if we need the workaround. Presumably that
	 * was loaded first.
	 */
	if (IS_GEN5(dev) && IS_MOBILE(dev) && intel_iommu_gfx_mapped)
		return true;
#endif
	return false;
}

static bool do_idling(struct drm_i915_private *dev_priv)
{
	bool ret = dev_priv->mm.interruptible;

	if (unlikely(dev_priv->gtt.do_idle_maps)) {
		dev_priv->mm.interruptible = false;
		if (i915_gpu_idle(dev_priv->dev)) {
			DRM_ERROR("Couldn't idle GPU\n");
			/* Wait a bit, in hopes it avoids the hang */
			udelay(10);
		}
	}

	return ret;
}

static void undo_idling(struct drm_i915_private *dev_priv, bool interruptible)
{
	if (unlikely(dev_priv->gtt.do_idle_maps))
		dev_priv->mm.interruptible = interruptible;
}

void i915_check_and_clear_faults(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_engine_cs *ring;
	int i;

	if (INTEL_INFO(dev)->gen < 6)
		return;

	for_each_ring(ring, dev_priv, i) {
		u32 fault_reg;
		fault_reg = I915_READ(RING_FAULT_REG(ring));
		if (fault_reg & RING_FAULT_VALID) {
			DRM_DEBUG_DRIVER("Unexpected fault\n"
					 "\tAddr: 0x%08lx\\n"
					 "\tAddress space: %s\n"
					 "\tSource ID: %d\n"
					 "\tType: %d\n",
					 fault_reg & PAGE_MASK,
					 fault_reg & RING_FAULT_GTTSEL_MASK ? "GGTT" : "PPGTT",
					 RING_FAULT_SRCID(fault_reg),
					 RING_FAULT_FAULT_TYPE(fault_reg));
			I915_WRITE(RING_FAULT_REG(ring),
				   fault_reg & ~RING_FAULT_VALID);
		}
	}
	POSTING_READ(RING_FAULT_REG(&dev_priv->ring[RCS]));
}

static void i915_ggtt_flush(struct drm_i915_private *dev_priv)
{
	if (INTEL_INFO(dev_priv->dev)->gen < 6) {
		intel_gtt_chipset_flush();
	} else {
		I915_WRITE(GFX_FLSH_CNTL_GEN6, GFX_FLSH_CNTL_EN);
		POSTING_READ(GFX_FLSH_CNTL_GEN6);
	}
}

void i915_gem_suspend_gtt_mappings(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	/* Don't bother messing with faults pre GEN6 as we have little
	 * documentation supporting that it's a good idea.
	 */
	if (INTEL_INFO(dev)->gen < 6)
		return;

	i915_check_and_clear_faults(dev);

	dev_priv->gtt.base.clear_range(&dev_priv->gtt.base,
				       dev_priv->gtt.base.start,
				       dev_priv->gtt.base.total,
				       true);

	i915_ggtt_flush(dev_priv);
}

void i915_gem_restore_gtt_mappings(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj;
	struct i915_address_space *vm;

	i915_check_and_clear_faults(dev);

	/* First fill our portion of the GTT with scratch pages */
	dev_priv->gtt.base.clear_range(&dev_priv->gtt.base,
				       dev_priv->gtt.base.start,
				       dev_priv->gtt.base.total,
				       true);

	list_for_each_entry(obj, &dev_priv->mm.bound_list, global_list) {
		struct i915_vma *vma = i915_gem_obj_to_vma(obj,
							   &dev_priv->gtt.base);
		if (!vma)
			continue;

		i915_gem_clflush_object(obj, obj->pin_display);
		/* The bind_vma code tries to be smart about tracking mappings.
		 * Unfortunately above, we've just wiped out the mappings
		 * without telling our object about it. So we need to fake it.
		 */
		obj->has_global_gtt_mapping = 0;
		vma->bind_vma(vma, obj->cache_level, GLOBAL_BIND);
	}


	if (INTEL_INFO(dev)->gen >= 8) {
		if (IS_CHERRYVIEW(dev))
			chv_setup_private_ppat(dev_priv);
		else
			bdw_setup_private_ppat(dev_priv);

		return;
	}

	if (USES_PPGTT(dev)) {
		list_for_each_entry(vm, &dev_priv->vm_list, global_link) {
		/* TODO: Perhaps it shouldn't be gen6 specific */

			struct i915_hw_ppgtt *ppgtt =
				container_of(vm, struct i915_hw_ppgtt, base);

			if (i915_is_ggtt(vm))
				ppgtt = dev_priv->mm.aliasing_ppgtt;

			gen6_write_page_range(dev_priv, &ppgtt->pd, 0, GEN6_PPGTT_PD_ENTRIES);
		}
	}

	i915_ggtt_flush(dev_priv);
}

int i915_gem_gtt_prepare_object(struct drm_i915_gem_object *obj)
{
	if (obj->has_dma_mapping)
		return 0;

	if (!dma_map_sg(&obj->base.dev->pdev->dev,
			obj->pages->sgl, obj->pages->nents,
			PCI_DMA_BIDIRECTIONAL))
		return -ENOSPC;

	return 0;
}

static inline void gen8_set_pte(void __iomem *addr, gen8_gtt_pte_t pte)
{
#ifdef writeq
	writeq(pte, addr);
#else
	iowrite32((u32)pte, addr);
	iowrite32(pte >> 32, addr + 4);
#endif
}

static void gen8_ggtt_insert_entries(struct i915_address_space *vm,
				     struct sg_table *st,
				     uint64_t start,
				     enum i915_cache_level level, u32 unused)
{
	struct drm_i915_private *dev_priv = vm->dev->dev_private;
	unsigned first_entry = start >> PAGE_SHIFT;
	gen8_gtt_pte_t __iomem *gtt_entries =
		(gen8_gtt_pte_t __iomem *)dev_priv->gtt.gsm + first_entry;
	int i = 0;
	struct sg_page_iter sg_iter;
	dma_addr_t addr = 0;

	for_each_sg_page(st->sgl, &sg_iter, st->nents, 0) {
		addr = sg_dma_address(sg_iter.sg) +
			(sg_iter.sg_pgoffset << PAGE_SHIFT);
		gen8_set_pte(&gtt_entries[i],
			     gen8_pte_encode(addr, level, true));
		i++;
	}

	/*
	 * XXX: This serves as a posting read to make sure that the PTE has
	 * actually been updated. There is some concern that even though
	 * registers and PTEs are within the same BAR that they are potentially
	 * of NUMA access patterns. Therefore, even with the way we assume
	 * hardware should work, we must keep this posting read for paranoia.
	 */
	if (i != 0)
		WARN_ON(readq(&gtt_entries[i-1])
			!= gen8_pte_encode(addr, level, true));

	/* This next bit makes the above posting read even more important. We
	 * want to flush the TLBs only after we're certain all the PTE updates
	 * have finished.
	 */
	I915_WRITE(GFX_FLSH_CNTL_GEN6, GFX_FLSH_CNTL_EN);
	POSTING_READ(GFX_FLSH_CNTL_GEN6);
}

/*
 * Binds an object into the global gtt with the specified cache level. The object
 * will be accessible to the GPU via commands whose operands reference offsets
 * within the global GTT as well as accessible by the GPU through the GMADR
 * mapped BAR (dev_priv->mm.gtt->gtt).
 */
static void gen6_ggtt_insert_entries(struct i915_address_space *vm,
				     struct sg_table *st,
				     uint64_t start,
				     enum i915_cache_level level, u32 flags)
{
	struct drm_i915_private *dev_priv = vm->dev->dev_private;
	unsigned first_entry = start >> PAGE_SHIFT;
	gen6_gtt_pte_t __iomem *gtt_entries =
		(gen6_gtt_pte_t __iomem *)dev_priv->gtt.gsm + first_entry;
	int i = 0;
	struct sg_page_iter sg_iter;
	dma_addr_t addr;

	for_each_sg_page(st->sgl, &sg_iter, st->nents, 0) {
		addr = sg_page_iter_dma_address(&sg_iter);
		iowrite32(vm->pte_encode(addr, level, true, flags),
							&gtt_entries[i]);
		i++;
	}

	/* XXX: This serves as a posting read to make sure that the PTE has
	 * actually been updated. There is some concern that even though
	 * registers and PTEs are within the same BAR that they are potentially
	 * of NUMA access patterns. Therefore, even with the way we assume
	 * hardware should work, we must keep this posting read for paranoia.
	 */
	if (i != 0)
		WARN_ON(readl(&gtt_entries[i-1]) !=
			vm->pte_encode(addr, level, true, flags));

	/* This next bit makes the above posting read even more important. We
	 * want to flush the TLBs only after we're certain all the PTE updates
	 * have finished.
	 */
	I915_WRITE(GFX_FLSH_CNTL_GEN6, GFX_FLSH_CNTL_EN);
	POSTING_READ(GFX_FLSH_CNTL_GEN6);
}

static void gen8_ggtt_clear_range(struct i915_address_space *vm,
				  uint64_t start,
				  uint64_t length,
				  bool use_scratch)
{
	struct drm_i915_private *dev_priv = vm->dev->dev_private;
	unsigned first_entry = start >> PAGE_SHIFT;
	unsigned num_entries = length >> PAGE_SHIFT;
	gen8_gtt_pte_t scratch_pte, __iomem *gtt_base =
		(gen8_gtt_pte_t __iomem *) dev_priv->gtt.gsm + first_entry;
	const int max_entries = gtt_total_entries(dev_priv->gtt) - first_entry;
	int i;

	if (WARN(num_entries > max_entries,
		 "First entry = %d; Num entries = %d (max=%d)\n",
		 first_entry, num_entries, max_entries))
		num_entries = max_entries;

	scratch_pte = gen8_pte_encode(vm->scratch.addr,
				      I915_CACHE_LLC,
				      use_scratch);
	for (i = 0; i < num_entries; i++)
		gen8_set_pte(&gtt_base[i], scratch_pte);
	readl(gtt_base);
}

static void gen6_ggtt_clear_range(struct i915_address_space *vm,
				  uint64_t start,
				  uint64_t length,
				  bool use_scratch)
{
	struct drm_i915_private *dev_priv = vm->dev->dev_private;
	unsigned first_entry = start >> PAGE_SHIFT;
	unsigned num_entries = length >> PAGE_SHIFT;
	gen6_gtt_pte_t scratch_pte, __iomem *gtt_base =
		(gen6_gtt_pte_t __iomem *) dev_priv->gtt.gsm + first_entry;
	const int max_entries = gtt_total_entries(dev_priv->gtt) - first_entry;
	int i;

	if (WARN(num_entries > max_entries,
		 "First entry = %d; Num entries = %d (max=%d)\n",
		 first_entry, num_entries, max_entries))
		num_entries = max_entries;

	scratch_pte = vm->pte_encode(vm->scratch.addr, I915_CACHE_LLC,
							use_scratch, 0);

	for (i = 0; i < num_entries; i++)
		iowrite32(scratch_pte, &gtt_base[i]);
	readl(gtt_base);
}


static void i915_ggtt_bind_vma(struct i915_vma *vma,
			       enum i915_cache_level cache_level,
			       u32 unused)
{
	const unsigned long entry = vma->node.start >> PAGE_SHIFT;
	unsigned int flags = (cache_level == I915_CACHE_NONE) ?
		AGP_USER_MEMORY : AGP_USER_CACHED_MEMORY;

	BUG_ON(!i915_is_ggtt(vma->vm));
	intel_gtt_insert_sg_entries(vma->obj->pages, entry, flags);
	vma->obj->has_global_gtt_mapping = 1;
}

static void i915_ggtt_clear_range(struct i915_address_space *vm,
				  uint64_t start,
				  uint64_t length,
				  bool unused)
{
	unsigned first_entry = start >> PAGE_SHIFT;
	unsigned num_entries = length >> PAGE_SHIFT;
	intel_gtt_clear_range(first_entry, num_entries);
}

static void i915_ggtt_unbind_vma(struct i915_vma *vma)
{
	const unsigned int first = vma->node.start >> PAGE_SHIFT;
	const unsigned int size = vma->obj->base.size >> PAGE_SHIFT;

	BUG_ON(!i915_is_ggtt(vma->vm));
	vma->obj->has_global_gtt_mapping = 0;
	intel_gtt_clear_range(first, size);
}

static void ggtt_bind_vma(struct i915_vma *vma,
			  enum i915_cache_level cache_level,
			  u32 flags)
{
	struct drm_device *dev = vma->vm->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj = vma->obj;

	/* Currently applicable only to VLV */
	if (obj->gt_ro)
		flags |= PTE_READ_ONLY;

	/* If there is no aliasing PPGTT, or the caller needs a global mapping,
	 * or we have a global mapping already but the cacheability flags have
	 * changed, set the global PTEs.
	 *
	 * If there is an aliasing PPGTT it is anecdotally faster, so use that
	 * instead if none of the above hold true.
	 *
	 * NB: A global mapping should only be needed for special regions like
	 * "gtt mappable", SNB errata, or if specified via special execbuf
	 * flags. At all other times, the GPU will use the aliasing PPGTT.
	 */
	if (!dev_priv->mm.aliasing_ppgtt || flags & GLOBAL_BIND) {
		if (!obj->has_global_gtt_mapping ||
		    (cache_level != obj->cache_level)) {
			vma->vm->insert_entries(vma->vm, obj->pages,
						vma->node.start,
						cache_level, flags);
			obj->has_global_gtt_mapping = 1;
		}
	}

	if (dev_priv->mm.aliasing_ppgtt &&
	    (!obj->has_aliasing_ppgtt_mapping ||
	     (cache_level != obj->cache_level))) {
		struct i915_hw_ppgtt *appgtt = dev_priv->mm.aliasing_ppgtt;
		appgtt->base.insert_entries(&appgtt->base,
					    vma->obj->pages,
					    vma->node.start,
					    cache_level, flags);
		vma->obj->has_aliasing_ppgtt_mapping = 1;
	}
}

static void ggtt_unbind_vma(struct i915_vma *vma)
{
	struct drm_device *dev = vma->vm->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj = vma->obj;

	if (obj->has_global_gtt_mapping) {
		vma->vm->clear_range(vma->vm,
				     vma->node.start,
				     obj->base.size,
				     true);
		obj->has_global_gtt_mapping = 0;
	}

	if (obj->has_aliasing_ppgtt_mapping) {
		struct i915_hw_ppgtt *appgtt = dev_priv->mm.aliasing_ppgtt;
		appgtt->base.clear_range(&appgtt->base,
					 vma->node.start,
					 obj->base.size,
					 true);
		obj->has_aliasing_ppgtt_mapping = 0;
	}
}

void i915_gem_gtt_finish_object(struct drm_i915_gem_object *obj)
{
	struct drm_device *dev = obj->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	bool interruptible;

	interruptible = do_idling(dev_priv);

	if (!obj->has_dma_mapping)
		dma_unmap_sg(&dev->pdev->dev,
			     obj->pages->sgl, obj->pages->nents,
			     PCI_DMA_BIDIRECTIONAL);

	undo_idling(dev_priv, interruptible);
}

static void i915_gtt_color_adjust(struct drm_mm_node *node,
				  unsigned long color,
				  unsigned long *start,
				  unsigned long *end)
{
	if (node->color != color)
		*start += 4096;

	if (!list_empty(&node->node_list)) {
		node = list_entry(node->node_list.next,
				  struct drm_mm_node,
				  node_list);
		if (node->allocated && node->color != color)
			*end -= 4096;
	}
}

int i915_gem_setup_global_gtt(struct drm_device *dev,
			      unsigned long start,
			      unsigned long mappable_end,
			      unsigned long end)
{
	/* Let GEM Manage all of the aperture.
	 *
	 * However, leave one page at the end still bound to the scratch page.
	 * There are a number of places where the hardware apparently prefetches
	 * past the end of the object, and we've seen multiple hangs with the
	 * GPU head pointer stuck in a batchbuffer bound at the last page of the
	 * aperture.  One page should be enough to keep any prefetching inside
	 * of the aperture.
	 */
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct i915_address_space *ggtt_vm = &dev_priv->gtt.base;
	struct drm_mm_node *entry;
	struct drm_i915_gem_object *obj;
	unsigned long hole_start, hole_end;
	int ret;

	BUG_ON(mappable_end > end);

	/* Subtract the guard page ... */
	drm_mm_init(&ggtt_vm->mm, start, end - start - PAGE_SIZE);
	if (!HAS_LLC(dev))
		dev_priv->gtt.base.mm.color_adjust = i915_gtt_color_adjust;

	/* Mark any preallocated objects as occupied */
	list_for_each_entry(obj, &dev_priv->mm.bound_list, global_list) {
		struct i915_vma *vma = i915_gem_obj_to_vma(obj, ggtt_vm);

		DRM_DEBUG_KMS("reserving preallocated space: %lx + %zx\n",
			      i915_gem_obj_ggtt_offset(obj), obj->base.size);

		WARN_ON(i915_gem_obj_ggtt_bound(obj));
		ret = drm_mm_reserve_node(&ggtt_vm->mm, &vma->node);
		if (ret) {
			DRM_DEBUG_KMS("Reservation failed: %i\n", ret);
			return ret;
		}
		obj->has_global_gtt_mapping = 1;
	}

	dev_priv->gtt.base.start = start;
	dev_priv->gtt.base.total = end - start;

	/* Clear any non-preallocated blocks */
	drm_mm_for_each_hole(entry, &ggtt_vm->mm, hole_start, hole_end) {
		DRM_DEBUG_KMS("clearing unused GTT space: [%lx, %lx]\n",
			      hole_start, hole_end);
		ggtt_vm->clear_range(ggtt_vm, hole_start,
				     hole_end - hole_start, true);
	}

	/* And finally clear the reserved guard page */
	ggtt_vm->clear_range(ggtt_vm, end - PAGE_SIZE, PAGE_SIZE, true);

	if (USES_PPGTT(dev) && !USES_FULL_PPGTT(dev)) {
		struct i915_hw_ppgtt *ppgtt;

		ppgtt = kzalloc(sizeof(*ppgtt), GFP_KERNEL);
		if (!ppgtt)
			return -ENOMEM;

		ret = __hw_ppgtt_init(dev, ppgtt, true);
		if (ret != 0)
			return ret;

		dev_priv->mm.aliasing_ppgtt = ppgtt;
	}

	return 0;
}

void i915_gem_init_global_gtt(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	unsigned long gtt_size, mappable_size;

	gtt_size = dev_priv->gtt.base.total;
	mappable_size = dev_priv->gtt.mappable_end;

	i915_gem_setup_global_gtt(dev, 0, mappable_size, gtt_size);
}

void i915_global_gtt_cleanup(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct i915_address_space *vm = &dev_priv->gtt.base;

	if (dev_priv->mm.aliasing_ppgtt) {
		struct i915_hw_ppgtt *ppgtt = dev_priv->mm.aliasing_ppgtt;

		ppgtt->base.cleanup(&ppgtt->base);
	}

	if (drm_mm_initialized(&vm->mm)) {
		drm_mm_takedown(&vm->mm);
		list_del(&vm->global_link);
	}

	vm->cleanup(vm);
}
static int setup_scratch_page(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct page *page;
	dma_addr_t dma_addr;

	page = alloc_page(GFP_KERNEL | GFP_DMA32 | __GFP_ZERO);
	if (page == NULL)
		return -ENOMEM;
	get_page(page);
	set_pages_uc(page, 1);

#ifdef CONFIG_INTEL_IOMMU
	dma_addr = pci_map_page(dev->pdev, page, 0, PAGE_SIZE,
				PCI_DMA_BIDIRECTIONAL);
	if (pci_dma_mapping_error(dev->pdev, dma_addr))
		return -EINVAL;
#else
	dma_addr = page_to_phys(page);
#endif
	dev_priv->gtt.base.scratch.page = page;
	dev_priv->gtt.base.scratch.addr = dma_addr;

	return 0;
}

static void teardown_scratch_page(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct page *page = dev_priv->gtt.base.scratch.page;

	set_pages_wb(page, 1);
	pci_unmap_page(dev->pdev, dev_priv->gtt.base.scratch.addr,
		       PAGE_SIZE, PCI_DMA_BIDIRECTIONAL);
	put_page(page);
	__free_page(page);
}

static inline unsigned int gen6_get_total_gtt_size(u16 snb_gmch_ctl)
{
	snb_gmch_ctl >>= SNB_GMCH_GGMS_SHIFT;
	snb_gmch_ctl &= SNB_GMCH_GGMS_MASK;
	return snb_gmch_ctl << 20;
}

static inline unsigned int gen8_get_total_gtt_size(u16 bdw_gmch_ctl)
{
	bdw_gmch_ctl >>= BDW_GMCH_GGMS_SHIFT;
	bdw_gmch_ctl &= BDW_GMCH_GGMS_MASK;
	if (bdw_gmch_ctl)
		bdw_gmch_ctl = 1 << bdw_gmch_ctl;

#ifdef CONFIG_X86_32
	/* Limit 32b platforms to a 2GB GGTT: 4 << 20 / pte size * PAGE_SIZE */
	if (bdw_gmch_ctl > 4)
		bdw_gmch_ctl = 4;
#endif

	return bdw_gmch_ctl << 20;
}

static inline unsigned int chv_get_total_gtt_size(u16 gmch_ctrl)
{
	gmch_ctrl >>= SNB_GMCH_GGMS_SHIFT;
	gmch_ctrl &= SNB_GMCH_GGMS_MASK;

	if (gmch_ctrl)
		return 1 << (20 + gmch_ctrl);

	return 0;
}

static inline size_t gen6_get_stolen_size(u16 snb_gmch_ctl)
{
	snb_gmch_ctl >>= SNB_GMCH_GMS_SHIFT;
	snb_gmch_ctl &= SNB_GMCH_GMS_MASK;
	return snb_gmch_ctl << 25; /* 32 MB units */
}

static inline size_t gen8_get_stolen_size(u16 bdw_gmch_ctl)
{
	bdw_gmch_ctl >>= BDW_GMCH_GMS_SHIFT;
	bdw_gmch_ctl &= BDW_GMCH_GMS_MASK;
	return bdw_gmch_ctl << 25; /* 32 MB units */
}

static size_t chv_get_stolen_size(u16 gmch_ctrl)
{
	gmch_ctrl >>= SNB_GMCH_GMS_SHIFT;
	gmch_ctrl &= SNB_GMCH_GMS_MASK;

	/*
	 * 0x0  to 0x10: 32MB increments starting at 0MB
	 * 0x11 to 0x16: 4MB increments starting at 8MB
	 * 0x17 to 0x1d: 4MB increments start at 36MB
	 */
	if (gmch_ctrl < 0x11)
		return gmch_ctrl << 25;
	else if (gmch_ctrl < 0x17)
		return (gmch_ctrl - 0x11 + 2) << 22;
	else
		return (gmch_ctrl - 0x17 + 9) << 22;
}

static int ggtt_probe_common(struct drm_device *dev,
			     size_t gtt_size)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	phys_addr_t gtt_phys_addr;
	int ret;

	/* For Modern GENs the PTEs and register space are split in the BAR */
	gtt_phys_addr = pci_resource_start(dev->pdev, 0) +
		(pci_resource_len(dev->pdev, 0) / 2);

	if (IS_CHERRYVIEW(dev))
		dev_priv->gtt.gsm = ioremap_nocache(gtt_phys_addr, gtt_size);
	else
		dev_priv->gtt.gsm = ioremap_wc(gtt_phys_addr, gtt_size);
	if (!dev_priv->gtt.gsm) {
		DRM_ERROR("Failed to map the gtt page table\n");
		return -ENOMEM;
	}

	ret = setup_scratch_page(dev);
	if (ret) {
		DRM_ERROR("Scratch setup failed\n");
		/* iounmap will also get called at remove, but meh */
		iounmap(dev_priv->gtt.gsm);
	}

	return ret;
}

/* The GGTT and PPGTT need a private PPAT setup in order to handle cacheability
 * bits. When using advanced contexts each context stores its own PAT, but
 * writing this data shouldn't be harmful even in those cases. */
static void bdw_setup_private_ppat(struct drm_i915_private *dev_priv)
{
	uint64_t pat;

	pat = GEN8_PPAT(0, GEN8_PPAT_WB | GEN8_PPAT_LLC)     | /* for normal objects, no eLLC */
	      GEN8_PPAT(1, GEN8_PPAT_WC | GEN8_PPAT_LLCELLC) | /* for something pointing to ptes? */
	      GEN8_PPAT(2, GEN8_PPAT_WT | GEN8_PPAT_LLCELLC) | /* for scanout with eLLC */
	      GEN8_PPAT(3, GEN8_PPAT_UC)                     | /* Uncached objects, mostly for scanout */
	      GEN8_PPAT(4, GEN8_PPAT_WB | GEN8_PPAT_LLCELLC | GEN8_PPAT_AGE(0)) |
	      GEN8_PPAT(5, GEN8_PPAT_WB | GEN8_PPAT_LLCELLC | GEN8_PPAT_AGE(1)) |
	      GEN8_PPAT(6, GEN8_PPAT_WB | GEN8_PPAT_LLCELLC | GEN8_PPAT_AGE(2)) |
	      GEN8_PPAT(7, GEN8_PPAT_WB | GEN8_PPAT_LLCELLC | GEN8_PPAT_AGE(3));

	/* XXX: spec defines this as 2 distinct registers. It's unclear if a 64b
	 * write would work. */
	I915_WRITE(GEN8_PRIVATE_PAT, pat);
	I915_WRITE(GEN8_PRIVATE_PAT + 4, pat >> 32);
}

static void chv_setup_private_ppat(struct drm_i915_private *dev_priv)
{
	uint64_t pat;

	/*
	 * Map WB on BDW to snooped on CHV.
	 *
	 * Only the snoop bit has meaning for CHV, the rest is
	 * ignored.
	 *
	 * Note that the harware enforces snooping for all page
	 * table accesses. The snoop bit is actually ignored for
	 * PDEs.
	 */
	pat = GEN8_PPAT(0, CHV_PPAT_SNOOP) |
	      GEN8_PPAT(1, 0) |
	      GEN8_PPAT(2, 0) |
	      GEN8_PPAT(3, 0) |
	      GEN8_PPAT(4, CHV_PPAT_SNOOP) |
	      GEN8_PPAT(5, CHV_PPAT_SNOOP) |
	      GEN8_PPAT(6, CHV_PPAT_SNOOP) |
	      GEN8_PPAT(7, CHV_PPAT_SNOOP);

	I915_WRITE(GEN8_PRIVATE_PAT, pat);
	I915_WRITE(GEN8_PRIVATE_PAT + 4, pat >> 32);
}

static int gen8_gmch_probe(struct drm_device *dev,
			   size_t *gtt_total,
			   size_t *stolen,
			   phys_addr_t *mappable_base,
			   unsigned long *mappable_end)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	unsigned int gtt_size;
	u16 snb_gmch_ctl;
	int ret;

	/* TODO: We're not aware of mappable constraints on gen8 yet */
	*mappable_base = pci_resource_start(dev->pdev, 2);
	*mappable_end = pci_resource_len(dev->pdev, 2);

	if (!pci_set_dma_mask(dev->pdev, DMA_BIT_MASK(39)))
		pci_set_consistent_dma_mask(dev->pdev, DMA_BIT_MASK(39));

	pci_read_config_word(dev->pdev, SNB_GMCH_CTRL, &snb_gmch_ctl);

	if (IS_CHERRYVIEW(dev)) {
		*stolen = chv_get_stolen_size(snb_gmch_ctl);
		gtt_size = chv_get_total_gtt_size(snb_gmch_ctl);
	} else {
		*stolen = gen8_get_stolen_size(snb_gmch_ctl);
		gtt_size = gen8_get_total_gtt_size(snb_gmch_ctl);
	}

	*gtt_total = (gtt_size / sizeof(gen8_gtt_pte_t)) << PAGE_SHIFT;

	if (IS_CHERRYVIEW(dev))
		chv_setup_private_ppat(dev_priv);
	else
		bdw_setup_private_ppat(dev_priv);

	ret = ggtt_probe_common(dev, gtt_size);

	dev_priv->gtt.base.clear_range = gen8_ggtt_clear_range;
	dev_priv->gtt.base.insert_entries = gen8_ggtt_insert_entries;

	return ret;
}

static int gen6_gmch_probe(struct drm_device *dev,
			   size_t *gtt_total,
			   size_t *stolen,
			   phys_addr_t *mappable_base,
			   unsigned long *mappable_end)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	unsigned int gtt_size;
	u16 snb_gmch_ctl;
	int ret;

	*mappable_base = pci_resource_start(dev->pdev, 2);
	*mappable_end = pci_resource_len(dev->pdev, 2);

	/* 64/512MB is the current min/max we actually know of, but this is just
	 * a coarse sanity check.
	 */
	if ((*mappable_end < (64<<20) || (*mappable_end > (512<<20)))) {
		DRM_ERROR("Unknown GMADR size (%lx)\n",
			  dev_priv->gtt.mappable_end);
		return -ENXIO;
	}

	if (!pci_set_dma_mask(dev->pdev, DMA_BIT_MASK(40)))
		pci_set_consistent_dma_mask(dev->pdev, DMA_BIT_MASK(40));
	pci_read_config_word(dev->pdev, SNB_GMCH_CTRL, &snb_gmch_ctl);

	*stolen = gen6_get_stolen_size(snb_gmch_ctl);

	gtt_size = gen6_get_total_gtt_size(snb_gmch_ctl);
	*gtt_total = (gtt_size / sizeof(gen6_gtt_pte_t)) << PAGE_SHIFT;

	ret = ggtt_probe_common(dev, gtt_size);

	dev_priv->gtt.base.clear_range = gen6_ggtt_clear_range;
	dev_priv->gtt.base.insert_entries = gen6_ggtt_insert_entries;

	return ret;
}

static void gen6_gmch_remove(struct i915_address_space *vm)
{

	struct i915_gtt *gtt = container_of(vm, struct i915_gtt, base);

	iounmap(gtt->gsm);
	teardown_scratch_page(vm->dev);
}

static int i915_gmch_probe(struct drm_device *dev,
			   size_t *gtt_total,
			   size_t *stolen,
			   phys_addr_t *mappable_base,
			   unsigned long *mappable_end)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	ret = intel_gmch_probe(dev_priv->bridge_dev, dev_priv->dev->pdev, NULL);
	if (!ret) {
		DRM_ERROR("failed to set up gmch\n");
		return -EIO;
	}

	intel_gtt_get(gtt_total, stolen, mappable_base, mappable_end);

	dev_priv->gtt.do_idle_maps = needs_idle_maps(dev_priv->dev);
	dev_priv->gtt.base.clear_range = i915_ggtt_clear_range;

	if (unlikely(dev_priv->gtt.do_idle_maps))
		DRM_INFO("applying Ironlake quirks for intel_iommu\n");

	return 0;
}

static void i915_gmch_remove(struct i915_address_space *vm)
{
	intel_gmch_remove();
}

int i915_gem_gtt_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct i915_gtt *gtt = &dev_priv->gtt;
	int ret;

	if (INTEL_INFO(dev)->gen <= 5) {
		gtt->gtt_probe = i915_gmch_probe;
		gtt->base.cleanup = i915_gmch_remove;
	} else if (INTEL_INFO(dev)->gen < 8) {
		gtt->gtt_probe = gen6_gmch_probe;
		gtt->base.cleanup = gen6_gmch_remove;
		if (IS_HASWELL(dev) && dev_priv->ellc_size)
			gtt->base.pte_encode = iris_pte_encode;
		else if (IS_HASWELL(dev))
			gtt->base.pte_encode = hsw_pte_encode;
		else if (IS_VALLEYVIEW(dev))
			gtt->base.pte_encode = byt_pte_encode;
		else if (INTEL_INFO(dev)->gen >= 7)
			gtt->base.pte_encode = ivb_pte_encode;
		else
			gtt->base.pte_encode = snb_pte_encode;
	} else {
		dev_priv->gtt.gtt_probe = gen8_gmch_probe;
		dev_priv->gtt.base.cleanup = gen6_gmch_remove;
	}

	ret = gtt->gtt_probe(dev, &gtt->base.total, &gtt->stolen_size,
			     &gtt->mappable_base, &gtt->mappable_end);
	if (ret)
		return ret;

	gtt->base.dev = dev;

	/* GMADR is the PCI mmio aperture into the global GTT. */
	DRM_INFO("Memory usable by graphics device = %zdM\n",
		 gtt->base.total >> 20);
	DRM_DEBUG_DRIVER("GMADR size = %ldM\n", gtt->mappable_end >> 20);
	DRM_DEBUG_DRIVER("GTT stolen size = %zdM\n", gtt->stolen_size >> 20);
#ifdef CONFIG_INTEL_IOMMU
	if (intel_iommu_gfx_mapped)
		DRM_INFO("VT-d active for gfx access\n");
#endif
	/*
	 * i915.enable_ppgtt is read-only, so do an early pass to validate the
	 * user's requested state against the hardware/driver capabilities.  We
	 * do this now so that we can print out any log messages once rather
	 * than every time we check intel_enable_ppgtt().
	 */
	i915.enable_ppgtt = sanitize_enable_ppgtt(dev, i915.enable_ppgtt);
	DRM_DEBUG_DRIVER("ppgtt mode: %i\n", i915.enable_ppgtt);

	return 0;
}

static struct i915_vma *__i915_gem_vma_create(struct drm_i915_gem_object *obj,
					      struct i915_address_space *vm)
{
	struct i915_vma *vma = kzalloc(sizeof(*vma), GFP_KERNEL);
	if (vma == NULL)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&vma->vma_link);
	INIT_LIST_HEAD(&vma->vm_link);
	INIT_LIST_HEAD(&vma->mm_list);
	INIT_LIST_HEAD(&vma->exec_list);
	vma->vm = vm;
	vma->obj = obj;

	switch (INTEL_INFO(vm->dev)->gen) {
	case 8:
	case 7:
	case 6:
		if (i915_is_ggtt(vm)) {
			vma->unbind_vma = ggtt_unbind_vma;
			vma->bind_vma = ggtt_bind_vma;
		} else {
			vma->unbind_vma = ppgtt_unbind_vma;
			vma->bind_vma = ppgtt_bind_vma;
		}
		break;
	case 5:
	case 4:
	case 3:
	case 2:
		BUG_ON(!i915_is_ggtt(vm));
		vma->unbind_vma = i915_ggtt_unbind_vma;
		vma->bind_vma = i915_ggtt_bind_vma;
		break;
	default:
		BUG();
	}

	/* Keep GGTT vmas first to make debug easier */
	if (i915_is_ggtt(vm))
		list_add(&vma->vma_link, &obj->vma_list);
	else {
		struct i915_hw_ppgtt *ppgtt = i915_vm_to_ppgtt(vm);
		list_add_tail(&vma->vma_link, &obj->vma_list);
		i915_ppgtt_get(ppgtt);
		list_add_tail(&vma->vm_link, &ppgtt->vma_list);
	}

	return vma;
}

struct i915_vma *
i915_gem_obj_lookup_or_create_vma(struct drm_i915_gem_object *obj,
				  struct i915_address_space *vm)
{
	struct i915_vma *vma;

	vma = i915_gem_obj_to_vma(obj, vm);
	if (!vma)
		vma = __i915_gem_vma_create(obj, vm);

	return vma;
}
