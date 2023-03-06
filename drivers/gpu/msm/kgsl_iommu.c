// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2011-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/bitfield.h>
#include <linux/compat.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/of_platform.h>
#include <linux/scatterlist.h>
#include <linux/qcom-iommu-util.h>
#include <linux/qcom-io-pgtable.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/qcom_scm.h>
#include <linux/random.h>
#include <linux/regulator/consumer.h>
#include <soc/qcom/secure_buffer.h>

#include "adreno.h"
#include "kgsl_device.h"
#include "kgsl_iommu.h"
#include "kgsl_mmu.h"
#include "kgsl_pwrctrl.h"
#include "kgsl_sharedmem.h"
#include "kgsl_trace.h"

#define KGSL_IOMMU_SPLIT_TABLE_BASE 0x0001ff8000000000ULL

#define KGSL_IOMMU_IDR1_OFFSET 0x24
#define IDR1_NUMPAGENDXB GENMASK(30, 28)
#define IDR1_PAGESIZE BIT(31)

static const struct kgsl_mmu_pt_ops secure_pt_ops;
static const struct kgsl_mmu_pt_ops default_pt_ops;
static const struct kgsl_mmu_pt_ops iopgtbl_pt_ops;

/* Zero page for non-secure VBOs */
static struct page *kgsl_vbo_zero_page;

/*
 * struct kgsl_iommu_addr_entry - entry in the kgsl_pagetable rbtree.
 * @base: starting virtual address of the entry
 * @size: size of the entry
 * @node: the rbtree node
 */
struct kgsl_iommu_addr_entry {
	uint64_t base;
	uint64_t size;
	struct rb_node node;
};

static struct kmem_cache *addr_entry_cache;

/* These are dummy TLB ops for the io-pgtable instances */

static void _tlb_flush_all(void *cookie)
{
}

static void _tlb_flush_walk(unsigned long iova, size_t size,
		size_t granule, void *cookie)
{
}

static void _tlb_add_page(struct iommu_iotlb_gather *gather,
		unsigned long iova, size_t granule, void *cookie)
{
}

static const struct iommu_flush_ops kgsl_iopgtbl_tlb_ops = {
	.tlb_flush_all = _tlb_flush_all,
	.tlb_flush_walk = _tlb_flush_walk,
	.tlb_add_page = _tlb_add_page,
};

static bool _iommu_domain_check_bool(struct iommu_domain *domain, int attr)
{
	u32 val;
	int ret = iommu_domain_get_attr(domain, attr, &val);

	return (!ret && val);
}

static int _iommu_domain_context_bank(struct iommu_domain *domain)
{
	int val, ret;

	ret = iommu_domain_get_attr(domain, DOMAIN_ATTR_CONTEXT_BANK, &val);

	return ret ? ret : val;
}

static struct kgsl_iommu_pt *to_iommu_pt(struct kgsl_pagetable *pagetable)
{
	return container_of(pagetable, struct kgsl_iommu_pt, base);
}

static u32 get_llcc_flags(struct iommu_domain *domain,
		struct kgsl_memdesc *memdesc)
{
	struct adreno_device *adreno_dev =
		ADRENO_DEVICE(KGSL_MMU_DEVICE(memdesc->pagetable->mmu));

	/*
	 * A621 GPU is only used to generate MV grid during LSR. There is a dedicated
	 * slice LLCC_GPUMV for MV grid buffer. This slice will not be utilized if
	 * IOMMU_USE_LLC_NWA flag is used as this implies no write-allocate policy.
	 * Hence use IOMMU_USE_UPSTREAM_HINT to use read-allocate and write-allocate
	 * cache policy.
	 */
	if (adreno_is_a621(adreno_dev))
		return IOMMU_USE_UPSTREAM_HINT;

	if (_iommu_domain_check_bool(domain, DOMAIN_ATTR_USE_LLC_NWA))
		return IOMMU_USE_LLC_NWA;

	if (_iommu_domain_check_bool(domain, DOMAIN_ATTR_USE_UPSTREAM_HINT))
		return IOMMU_USE_UPSTREAM_HINT;

	return 0;
}


static int _iommu_get_protection_flags(struct iommu_domain *domain,
	struct kgsl_memdesc *memdesc)
{
	int flags = IOMMU_READ | IOMMU_WRITE | IOMMU_NOEXEC;

	flags |= get_llcc_flags(domain, memdesc);

	if (memdesc->flags & KGSL_MEMFLAGS_GPUREADONLY)
		flags &= ~IOMMU_WRITE;

	if (memdesc->priv & KGSL_MEMDESC_PRIVILEGED)
		flags |= IOMMU_PRIV;

	if (memdesc->flags & KGSL_MEMFLAGS_IOCOHERENT)
		flags |= IOMMU_CACHE;

	if (memdesc->priv & KGSL_MEMDESC_UCODE)
		flags &= ~IOMMU_NOEXEC;

	return flags;
}

/* Get a scattterlist for the subrange in the child memdesc */
static int get_sg_from_child(struct sg_table *sgt, struct kgsl_memdesc *child,
		u64 offset, u64 length)
{
	int npages = (length >> PAGE_SHIFT);
	int pgoffset = (offset >> PAGE_SHIFT);
	struct scatterlist *target_sg;
	struct sg_page_iter iter;
	int ret;

	if (child->pages)
		return sg_alloc_table_from_pages(sgt,
			child->pages + pgoffset, npages, 0,
			length, GFP_KERNEL);

	ret = sg_alloc_table(sgt, npages, GFP_KERNEL);
	if (ret)
		return ret;

	target_sg = sgt->sgl;

	for_each_sg_page(child->sgt->sgl, &iter, npages, pgoffset) {
		sg_set_page(target_sg, sg_page_iter_page(&iter), PAGE_SIZE, 0);
		target_sg = sg_next(target_sg);
	}

	return 0;
}

static struct iommu_domain *to_iommu_domain(struct kgsl_iommu_context *context)
{
	return context->domain;
}

static struct kgsl_iommu *to_kgsl_iommu(struct kgsl_pagetable *pt)
{
	return &pt->mmu->iommu;
}

/*
 * One page allocation for a guard region to protect against over-zealous
 * GPU pre-fetch
 */
static struct page *kgsl_guard_page;
static struct page *kgsl_secure_guard_page;

static struct page *iommu_get_guard_page(struct kgsl_memdesc *memdesc)
{
	if (kgsl_memdesc_is_secured(memdesc)) {
		if (!kgsl_secure_guard_page)
			kgsl_secure_guard_page = kgsl_alloc_secure_page();

		return kgsl_secure_guard_page;
	}

	if (!kgsl_guard_page)
		kgsl_guard_page = alloc_page(GFP_KERNEL | __GFP_ZERO |
			__GFP_NORETRY | __GFP_HIGHMEM);

	return kgsl_guard_page;
}

static size_t iommu_pgsize(unsigned long pgsize_bitmap, unsigned long iova,
			   phys_addr_t paddr, size_t size, size_t *count)
{
	unsigned int pgsize_idx, pgsize_idx_next;
	unsigned long pgsizes;
	size_t offset, pgsize, pgsize_next;
	unsigned long addr_merge = paddr | iova;

	/* Page sizes supported by the hardware and small enough for @size */
	pgsizes = pgsize_bitmap & GENMASK(__fls(size), 0);

	/* Constrain the page sizes further based on the maximum alignment */
	if (likely(addr_merge))
		pgsizes &= GENMASK(__ffs(addr_merge), 0);

	/* Make sure we have at least one suitable page size */
	if (!pgsizes)
		return 0;

	/* Pick the biggest page size remaining */
	pgsize_idx = __fls(pgsizes);
	pgsize = BIT(pgsize_idx);
	if (!count)
		return pgsize;

	/* Find the next biggest support page size, if it exists */
	pgsizes = pgsize_bitmap & ~GENMASK(pgsize_idx, 0);
	if (!pgsizes)
		goto out_set_count;

	pgsize_idx_next = __ffs(pgsizes);
	pgsize_next = BIT(pgsize_idx_next);

	/*
	 * There's no point trying a bigger page size unless the virtual
	 * and physical addresses are similarly offset within the larger page.
	 */
	if ((iova ^ paddr) & (pgsize_next - 1))
		goto out_set_count;

	/* Calculate the offset to the next page size alignment boundary */
	offset = pgsize_next - (addr_merge & (pgsize_next - 1));

	/*
	 * If size is big enough to accommodate the larger page, reduce
	 * the number of smaller pages.
	 */
	if (offset + pgsize_next <= size)
		size = offset;

out_set_count:
	*count = size >> pgsize_idx;
	return pgsize;
}

static int _iopgtbl_unmap_pages(struct kgsl_iommu_pt *pt, u64 gpuaddr,
	size_t size)
{
	struct io_pgtable_ops *ops = pt->pgtbl_ops;
	size_t unmapped = 0;

	while (unmapped < size) {
		size_t ret, size_to_unmap, remaining, pgcount;

		remaining = (size - unmapped);
		size_to_unmap = iommu_pgsize(pt->info.cfg.pgsize_bitmap,
				gpuaddr, gpuaddr, remaining, &pgcount);
		if (size_to_unmap == 0)
			break;

		ret = ops->unmap_pages(ops, gpuaddr, size_to_unmap,
				pgcount, NULL);
		if (ret == 0)
			break;

		gpuaddr += ret;
		unmapped += ret;
	}

	return (unmapped == size) ? 0 : -EINVAL;
}

static void kgsl_iommu_flush_tlb(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = &mmu->iommu;

	iommu_flush_iotlb_all(to_iommu_domain(&iommu->user_context));

	/* As LPAC is optional, check LPAC domain is present before flush */
	if (iommu->lpac_context.domain)
		iommu_flush_iotlb_all(to_iommu_domain(&iommu->lpac_context));
}

static int _iopgtbl_unmap(struct kgsl_iommu_pt *pt, u64 gpuaddr, size_t size)
{
	struct kgsl_device *device = KGSL_MMU_DEVICE(pt->base.mmu);
	struct io_pgtable_ops *ops = pt->pgtbl_ops;
	int ret = 0;

	if (ops->unmap_pages) {
		ret = _iopgtbl_unmap_pages(pt, gpuaddr, size);
		if (ret)
			return ret;
		goto flush;
	}

	while (size) {
		if ((ops->unmap(ops, gpuaddr, PAGE_SIZE, NULL)) != PAGE_SIZE)
			return -EINVAL;

		gpuaddr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}

flush:
	/* Skip TLB Operations if GPU is in slumber */
	if (mutex_trylock(&device->mutex)) {
		if (device->state == KGSL_STATE_SLUMBER) {
			mutex_unlock(&device->mutex);
			return 0;
		}
		mutex_unlock(&device->mutex);
	}

	kgsl_iommu_flush_tlb(pt->base.mmu);
	return 0;
}

static size_t _iopgtbl_map_sg(struct kgsl_iommu_pt *pt, u64 gpuaddr,
		struct sg_table *sgt, int prot)
{
	struct io_pgtable_ops *ops = pt->pgtbl_ops;
	struct scatterlist *sg;
	size_t mapped = 0;
	u64 addr = gpuaddr;
	int ret, i;

	if (ops->map_sg) {
		ret = ops->map_sg(ops, addr, sgt->sgl, sgt->nents, prot,
			GFP_KERNEL, &mapped);
		if (ret) {
			_iopgtbl_unmap(pt, gpuaddr, mapped);
			return 0;
		}
		return mapped;
	}

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		size_t size = sg->length;
		phys_addr_t phys = sg_phys(sg);

		while (size) {
			ret = ops->map(ops, addr, phys, PAGE_SIZE, prot, GFP_KERNEL);

			if (ret) {
				_iopgtbl_unmap(pt, gpuaddr, mapped);
				return 0;
			}

			phys += PAGE_SIZE;
			mapped += PAGE_SIZE;
			addr += PAGE_SIZE;
			size -= PAGE_SIZE;
		}
	}

	return mapped;
}


static int
kgsl_iopgtbl_map_child(struct kgsl_pagetable *pt, struct kgsl_memdesc *memdesc,
	u64 offset, struct kgsl_memdesc *child, u64 child_offset, u64 length)
{
	struct kgsl_iommu *iommu = &pt->mmu->iommu;
	struct iommu_domain *domain = to_iommu_domain(&iommu->user_context);
	struct kgsl_iommu_pt *iommu_pt = to_iommu_pt(pt);
	struct sg_table sgt;
	u32 flags;
	size_t mapped;
	int ret;

	ret = get_sg_from_child(&sgt, child, child_offset, length);
	if (ret)
		return ret;

	/* Inherit the flags from the child for this mapping */
	flags = _iommu_get_protection_flags(domain, child);

	mapped = _iopgtbl_map_sg(iommu_pt, memdesc->gpuaddr + offset, &sgt, flags);

	sg_free_table(&sgt);

	return mapped ? 0 : -ENOMEM;
}


static int
kgsl_iopgtbl_unmap_range(struct kgsl_pagetable *pt, struct kgsl_memdesc *memdesc,
		u64 offset, u64 length)
{
	if (WARN_ON(offset >= memdesc->size ||
		(offset + length) > memdesc->size))
		return -ERANGE;

	return _iopgtbl_unmap(to_iommu_pt(pt), memdesc->gpuaddr + offset,
			length);
}

static size_t _iopgtbl_map_page_to_range(struct kgsl_iommu_pt *pt,
		struct page *page, u64 gpuaddr, size_t range, int prot)
{
	struct io_pgtable_ops *ops = pt->pgtbl_ops;
	size_t mapped = 0;
	u64 addr = gpuaddr;
	int ret;

	while (range) {
		ret = ops->map(ops, addr, page_to_phys(page), PAGE_SIZE,
			prot, GFP_KERNEL);
		if (ret) {
			_iopgtbl_unmap(pt, gpuaddr, mapped);
			return 0;
		}

		mapped += PAGE_SIZE;
		addr += PAGE_SIZE;
		range -= PAGE_SIZE;
	}

	return mapped;
}

static int kgsl_iopgtbl_map_zero_page_to_range(struct kgsl_pagetable *pt,
		struct kgsl_memdesc *memdesc, u64 offset, u64 length)
{
	struct kgsl_iommu *iommu = &pt->mmu->iommu;
	struct iommu_domain *domain = to_iommu_domain(&iommu->user_context);
	/*
	 * The SMMU only does the PRT compare at the bottom level of the page table, because
	 * there is not an easy way for the hardware to perform this check at earlier levels.
	 * Mark this page writable to avoid page faults while writing to it. Since the address
	 * of this zero page is programmed in PRR register, MMU will intercept any accesses to
	 * the page before they go to DDR and will terminate the transaction.
	 */
	u32 flags = IOMMU_READ | IOMMU_WRITE | IOMMU_NOEXEC | get_llcc_flags(domain, memdesc);
	struct kgsl_iommu_pt *iommu_pt = to_iommu_pt(pt);
	struct page *page = kgsl_vbo_zero_page;

	if (WARN_ON(!page))
		return -ENODEV;

	if (WARN_ON((offset >= memdesc->size) ||
		(offset + length) > memdesc->size))
		return -ERANGE;

	if (!_iopgtbl_map_page_to_range(iommu_pt, page, memdesc->gpuaddr + offset,
		length, flags))
		return -ENOMEM;

	return 0;
}

static int kgsl_iopgtbl_map(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
	struct kgsl_iommu_pt *pt = to_iommu_pt(pagetable);
	struct kgsl_iommu *iommu = &pagetable->mmu->iommu;
	struct iommu_domain *domain = to_iommu_domain(&iommu->user_context);
	size_t mapped, padding;
	int prot;

	/* Get the protection flags for the user context */
	prot = _iommu_get_protection_flags(domain, memdesc);

	if (!memdesc->sgt) {
		struct sg_table sgt;
		int ret;

		ret = sg_alloc_table_from_pages(&sgt, memdesc->pages,
			memdesc->page_count, 0, memdesc->size, GFP_KERNEL);
		if (ret)
			return ret;
		mapped = _iopgtbl_map_sg(pt, memdesc->gpuaddr, &sgt, prot);
		sg_free_table(&sgt);
	} else {
		mapped = _iopgtbl_map_sg(pt, memdesc->gpuaddr, memdesc->sgt,
			prot);
	}

	if (mapped == 0)
		return -ENOMEM;

	padding = kgsl_memdesc_footprint(memdesc) - mapped;

	if (padding) {
		struct page *page = iommu_get_guard_page(memdesc);
		size_t ret;

		if (page)
			ret = _iopgtbl_map_page_to_range(pt, page,
				memdesc->gpuaddr + mapped, padding,
				prot & ~IOMMU_WRITE);

		if (!page || !ret) {
			_iopgtbl_unmap(pt, memdesc->gpuaddr, mapped);
			return -ENOMEM;
		}
	}

	return 0;
}

static int kgsl_iopgtbl_unmap(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
	return _iopgtbl_unmap(to_iommu_pt(pagetable), memdesc->gpuaddr,
		kgsl_memdesc_footprint(memdesc));
}

static int _iommu_unmap(struct iommu_domain *domain, u64 addr, size_t size)
{
	size_t unmapped = 0;

	if (!domain)
		return 0;

	/* Sign extend TTBR1 addresses all the way to avoid warning */
	if (addr & (1ULL << 48))
		addr |= 0xffff000000000000;

	unmapped = iommu_unmap(domain, addr, size);

	return (unmapped == size) ? 0 : -ENOMEM;
}


static size_t _iommu_map_page_to_range(struct iommu_domain *domain,
		struct page *page, u64 gpuaddr, size_t range, int prot)
{
	size_t mapped = 0;
	u64 addr = gpuaddr;

	if (!page)
		return 0;

	/* Sign extend TTBR1 addresses all the way to avoid warning */
	if (gpuaddr & (1ULL << 48))
		gpuaddr |= 0xffff000000000000;


	while (range) {
		int ret = iommu_map(domain, addr, page_to_phys(page),
			PAGE_SIZE, prot);
		if (ret) {
			iommu_unmap(domain, gpuaddr, mapped);
			return 0;
		}

		addr += PAGE_SIZE;
		mapped += PAGE_SIZE;
		range -= PAGE_SIZE;
	}

	return mapped;
}

static size_t _iommu_map_sg(struct iommu_domain *domain, u64 gpuaddr,
		struct sg_table *sgt, int prot)
{
	/* Sign extend TTBR1 addresses all the way to avoid warning */
	if (gpuaddr & (1ULL << 48))
		gpuaddr |= 0xffff000000000000;

	return iommu_map_sg(domain, gpuaddr, sgt->sgl, sgt->orig_nents, prot);
}

static int
_kgsl_iommu_map(struct iommu_domain *domain, struct kgsl_memdesc *memdesc)
{
	int prot = _iommu_get_protection_flags(domain, memdesc);
	size_t mapped, padding;
	int ret = 0;

	/*
	 * For paged memory allocated through kgsl, memdesc->pages is not NULL.
	 * Allocate sgt here just for its map operation. Contiguous memory
	 * already has its sgt, so no need to allocate it here.
	 */
	if (!memdesc->pages) {
		mapped = _iommu_map_sg(domain, memdesc->gpuaddr,
				memdesc->sgt, prot);
	} else {
		struct sg_table sgt;

		ret = sg_alloc_table_from_pages(&sgt, memdesc->pages,
			memdesc->page_count, 0, memdesc->size, GFP_KERNEL);
		if (ret)
			return ret;

		mapped = _iommu_map_sg(domain, memdesc->gpuaddr, &sgt, prot);
		sg_free_table(&sgt);
	}

	if (!mapped)
		return -ENOMEM;

	padding = kgsl_memdesc_footprint(memdesc) - mapped;

	if (padding) {
		struct page *page = iommu_get_guard_page(memdesc);
		size_t guard_mapped;

		if (page)
			guard_mapped = _iommu_map_page_to_range(domain, page,
				memdesc->gpuaddr + mapped, padding, prot & ~IOMMU_WRITE);

		if (!page || !guard_mapped) {
			_iommu_unmap(domain, memdesc->gpuaddr, mapped);
			ret = -ENOMEM;
		}
	}

	return ret;
}

static int kgsl_iommu_secure_map(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
	struct kgsl_iommu *iommu = &pagetable->mmu->iommu;
	struct iommu_domain *domain = to_iommu_domain(&iommu->secure_context);

	return _kgsl_iommu_map(domain, memdesc);
}

/*
 * Return true if the address is in the TTBR0 region. This is used for cases
 * when the "default" pagetable is used for both TTBR0 and TTBR1
 */
static bool is_lower_address(struct kgsl_mmu *mmu, u64 addr)
{
	return (test_bit(KGSL_MMU_IOPGTABLE, &mmu->features) &&
		addr < KGSL_IOMMU_SPLIT_TABLE_BASE);
}

static int _kgsl_iommu_unmap(struct iommu_domain *domain,
		struct kgsl_memdesc *memdesc)
{
	if (memdesc->size == 0 || memdesc->gpuaddr == 0)
		return -EINVAL;

	return _iommu_unmap(domain, memdesc->gpuaddr,
		kgsl_memdesc_footprint(memdesc));
}

/* Map on the default pagetable and the LPAC pagetable if it exists */
static int kgsl_iommu_default_map(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
	struct kgsl_mmu *mmu = pagetable->mmu;
	struct kgsl_iommu *iommu = &mmu->iommu;
	struct iommu_domain *domain, *lpac;
	int ret;

	if (is_lower_address(mmu, memdesc->gpuaddr))
		return kgsl_iopgtbl_map(pagetable, memdesc);

	domain = to_iommu_domain(&iommu->user_context);

	/* Map the object to the default GPU domain */
	ret = _kgsl_iommu_map(domain, memdesc);

	/* Also map the object to the LPAC domain if it exists */
	lpac = to_iommu_domain(&iommu->lpac_context);

	if (!ret && lpac) {
		ret = _kgsl_iommu_map(lpac, memdesc);

		/* On failure, also unmap from the default domain */
		if (ret)
			_kgsl_iommu_unmap(domain, memdesc);

	}

	return ret;
}

static int kgsl_iommu_secure_unmap(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
	struct kgsl_iommu *iommu = &pagetable->mmu->iommu;

	if (memdesc->size == 0 || memdesc->gpuaddr == 0)
		return -EINVAL;

	return _kgsl_iommu_unmap(to_iommu_domain(&iommu->secure_context),
		memdesc);
}

static int kgsl_iommu_default_unmap(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
	struct kgsl_mmu *mmu = pagetable->mmu;
	struct kgsl_iommu *iommu = &mmu->iommu;
	int ret;

	if (memdesc->size == 0 || memdesc->gpuaddr == 0)
		return -EINVAL;

	if (is_lower_address(mmu, memdesc->gpuaddr))
		return kgsl_iopgtbl_unmap(pagetable, memdesc);

	/* Unmap from the default domain */
	ret = _kgsl_iommu_unmap(to_iommu_domain(&iommu->user_context), memdesc);

	/* Unmap from the LPAC domain if it exists */
	ret |= _kgsl_iommu_unmap(to_iommu_domain(&iommu->lpac_context), memdesc);
	return ret;
}

static bool kgsl_iommu_addr_is_global(struct kgsl_mmu *mmu, u64 addr)
{
	if (test_bit(KGSL_MMU_IOPGTABLE, &mmu->features))
		return (addr >= KGSL_IOMMU_SPLIT_TABLE_BASE);

	return ((addr >= KGSL_IOMMU_GLOBAL_MEM_BASE(mmu)) &&
		(addr < KGSL_IOMMU_GLOBAL_MEM_BASE(mmu) +
		 KGSL_IOMMU_GLOBAL_MEM_SIZE));
}

static void __iomem *kgsl_iommu_reg(struct kgsl_iommu_context *ctx,
		u32 offset)
{
	struct kgsl_iommu *iommu = KGSL_IOMMU(ctx->kgsldev);

	if (!iommu->cb0_offset) {
		u32 reg =
			readl_relaxed(iommu->regbase + KGSL_IOMMU_IDR1_OFFSET);

		iommu->pagesize =
			FIELD_GET(IDR1_PAGESIZE, reg) ? SZ_64K : SZ_4K;

		/*
		 * The number of pages in the global address space or
		 * translation bank address space is 2^(NUMPAGENDXB + 1).
		 */
		iommu->cb0_offset = iommu->pagesize *
			(1 << (FIELD_GET(IDR1_NUMPAGENDXB, reg) + 1));
	}

	return (void __iomem *) (iommu->regbase + iommu->cb0_offset +
		(ctx->cb_num * iommu->pagesize) + offset);
}

static u64 KGSL_IOMMU_GET_CTX_REG_Q(struct kgsl_iommu_context *ctx, u32 offset)
{
	void __iomem *addr = kgsl_iommu_reg(ctx, offset);

	return readq_relaxed(addr);
}

static void KGSL_IOMMU_SET_CTX_REG(struct kgsl_iommu_context *ctx, u32 offset,
		u32 val)
{
	void __iomem *addr = kgsl_iommu_reg(ctx, offset);

	writel_relaxed(val, addr);
}

static u32 KGSL_IOMMU_GET_CTX_REG(struct kgsl_iommu_context *ctx, u32 offset)
{
	void __iomem *addr = kgsl_iommu_reg(ctx, offset);

	return readl_relaxed(addr);
}

static int kgsl_iommu_get_gpuaddr(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc);

static void kgsl_iommu_map_secure_global(struct kgsl_mmu *mmu,
		struct kgsl_memdesc *memdesc)
{
	if (IS_ERR_OR_NULL(mmu->securepagetable))
		return;

	if (!memdesc->gpuaddr) {
		int ret = kgsl_iommu_get_gpuaddr(mmu->securepagetable,
			memdesc);

		if (WARN_ON(ret))
			return;
	}

	kgsl_iommu_secure_map(mmu->securepagetable, memdesc);
}

#define KGSL_GLOBAL_MEM_PAGES (KGSL_IOMMU_GLOBAL_MEM_SIZE >> PAGE_SHIFT)

static u64 global_get_offset(struct kgsl_device *device, u64 size,
		unsigned long priv)
{
	int start = 0, bit;

	if (!device->global_map) {
		device->global_map =
			kcalloc(BITS_TO_LONGS(KGSL_GLOBAL_MEM_PAGES),
			sizeof(unsigned long), GFP_KERNEL);
		if (!device->global_map)
			return (unsigned long) -ENOMEM;
	}

	if (priv & KGSL_MEMDESC_RANDOM) {
		u32 offset = KGSL_GLOBAL_MEM_PAGES - (size >> PAGE_SHIFT);

		start = get_random_int() % offset;
	}

	while (start >= 0) {
		bit = bitmap_find_next_zero_area(device->global_map,
			KGSL_GLOBAL_MEM_PAGES, start, size >> PAGE_SHIFT, 0);

		if (bit < KGSL_GLOBAL_MEM_PAGES)
			break;

		/*
		 * Later implementations might want to randomize this to reduce
		 * predictability
		 */
		start--;
	}

	if (WARN_ON(start < 0))
		return (unsigned long) -ENOMEM;

	bitmap_set(device->global_map, bit, size >> PAGE_SHIFT);

	return bit << PAGE_SHIFT;
}

static void kgsl_iommu_map_global(struct kgsl_mmu *mmu,
		struct kgsl_memdesc *memdesc, u32 padding)
{
	struct kgsl_device *device = KGSL_MMU_DEVICE(mmu);

	if (memdesc->flags & KGSL_MEMFLAGS_SECURE) {
		kgsl_iommu_map_secure_global(mmu, memdesc);
		memdesc->priv |= KGSL_MEMDESC_MAPPED;
		return;
	}

	if (!memdesc->gpuaddr) {
		u64 offset;

		offset = global_get_offset(device, memdesc->size + padding,
			memdesc->priv);

		if (IS_ERR_VALUE(offset))
			return;

		memdesc->gpuaddr = mmu->defaultpagetable->global_base + offset;
		memdesc->pagetable = mmu->defaultpagetable;
	}

	kgsl_iommu_default_map(mmu->defaultpagetable, memdesc);
	memdesc->priv |= KGSL_MEMDESC_MAPPED;
}

/* Print the mem entry for the pagefault debugging */
static void print_entry(struct device *dev, struct kgsl_mem_entry *entry,
		pid_t pid)
{
	char name[32];

	if (!entry) {
		dev_crit(dev, "**EMPTY**\n");
		return;
	}

	kgsl_get_memory_usage(name, sizeof(name), entry->memdesc.flags);

	dev_err(dev, "[%016llX - %016llX] %s %s (pid = %d) (%s)\n",
	      entry->memdesc.gpuaddr,
	      entry->memdesc.gpuaddr + entry->memdesc.size - 1,
	      entry->memdesc.priv & KGSL_MEMDESC_GUARD_PAGE ? "(+guard)" : "",
	      entry->pending_free ? "(pending free)" : "",
	      pid, name);
}

/* Check if the address in the list of recently freed memory */
static void kgsl_iommu_check_if_freed(struct device *dev,
		struct kgsl_iommu_context *context, u64 addr, u32 ptname)
{
	uint64_t gpuaddr = addr;
	uint64_t size = 0;
	uint64_t flags = 0;
	char name[32];
	pid_t pid;

	if (!kgsl_memfree_find_entry(ptname, &gpuaddr, &size, &flags, &pid))
		return;

	kgsl_get_memory_usage(name, sizeof(name), flags);

	dev_err(dev, "---- premature free ----\n");
	dev_err(dev, "[%8.8llX-%8.8llX] (%s) was already freed by pid %d\n",
		gpuaddr, gpuaddr + size, name, pid);
}

static struct kgsl_process_private *kgsl_iommu_get_process(u64 ptbase)
{
	struct kgsl_process_private *p;
	struct kgsl_iommu_pt *iommu_pt;

	read_lock(&kgsl_driver.proclist_lock);

	list_for_each_entry(p, &kgsl_driver.process_list, list) {
		iommu_pt = to_iommu_pt(p->pagetable);
		if (iommu_pt->ttbr0 == MMU_SW_PT_BASE(ptbase)) {
			if (!kgsl_process_private_get(p))
				p = NULL;

			read_unlock(&kgsl_driver.proclist_lock);
			return p;
		}
	}

	read_unlock(&kgsl_driver.proclist_lock);

	return NULL;
}

static void kgsl_iommu_add_fault_info(struct kgsl_context *context,
		unsigned long addr, int flags)
{
	struct kgsl_pagefault_report *report;
	u32 fault_flag = 0;

	if (!context || !(context->flags & KGSL_CONTEXT_FAULT_INFO))
		return;

	report = kzalloc(sizeof(struct kgsl_pagefault_report), GFP_KERNEL);
	if (!report)
		return;

	if (flags & IOMMU_FAULT_TRANSLATION)
		fault_flag = KGSL_PAGEFAULT_TYPE_TRANSLATION;
	else if (flags & IOMMU_FAULT_PERMISSION)
		fault_flag = KGSL_PAGEFAULT_TYPE_PERMISSION;
	else if (flags & IOMMU_FAULT_EXTERNAL)
		fault_flag = KGSL_PAGEFAULT_TYPE_EXTERNAL;
	else if (flags & IOMMU_FAULT_TRANSACTION_STALLED)
		fault_flag = KGSL_PAGEFAULT_TYPE_TRANSACTION_STALLED;

	fault_flag |= (flags & IOMMU_FAULT_WRITE) ? KGSL_PAGEFAULT_TYPE_WRITE :
			KGSL_PAGEFAULT_TYPE_READ;

	report->fault_addr = addr;
	report->fault_type = fault_flag;
	if (kgsl_add_fault(context, KGSL_FAULT_TYPE_PAGEFAULT, report))
		kfree(report);
}

static void kgsl_iommu_print_fault(struct kgsl_mmu *mmu,
		struct kgsl_iommu_context *ctxt, unsigned long addr,
		u64 ptbase, u32 contextid,
		int flags, struct kgsl_process_private *private,
		struct kgsl_context *context)
{
	struct kgsl_device *device = KGSL_MMU_DEVICE(mmu);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_mem_entry *prev = NULL, *next = NULL, *entry;
	const char *fault_type = NULL;
	const char *comm = NULL;
	u32 ptname = KGSL_MMU_GLOBAL_PT;
	int id;

	if (private) {
		comm = private->comm;
		ptname = pid_nr(private->pid);
	}

	trace_kgsl_mmu_pagefault(device, addr,
			ptname, comm,
			(flags & IOMMU_FAULT_WRITE) ? "write" : "read");

	if (flags & IOMMU_FAULT_TRANSLATION)
		fault_type = "translation";
	else if (flags & IOMMU_FAULT_PERMISSION)
		fault_type = "permission";
	else if (flags & IOMMU_FAULT_EXTERNAL)
		fault_type = "external";
	else if (flags & IOMMU_FAULT_TRANSACTION_STALLED)
		fault_type = "transaction stalled";
	else
		fault_type = "unknown";


	/* FIXME: This seems buggy */
	if (test_bit(KGSL_FT_PAGEFAULT_LOG_ONE_PER_PAGE, &mmu->pfpolicy))
		if (!kgsl_mmu_log_fault_addr(mmu, ptbase, addr))
			return;

	if (!__ratelimit(&ctxt->ratelimit))
		return;

	dev_crit(device->dev,
		"GPU PAGE FAULT: addr = %lX pid= %d name=%s drawctxt=%d context pid = %d\n", addr,
		ptname, comm, contextid, context ? context->tid : 0);

	dev_crit(device->dev,
		"context=%s TTBR0=0x%llx (%s %s fault)\n",
		ctxt->name, ptbase,
		(flags & IOMMU_FAULT_WRITE) ? "write" : "read", fault_type);

	if (gpudev->iommu_fault_block) {
		u32 fsynr1 = KGSL_IOMMU_GET_CTX_REG(ctxt,
			KGSL_IOMMU_CTX_FSYNR1);

		dev_crit(device->dev,
			"FAULTING BLOCK: %s\n",
			gpudev->iommu_fault_block(device, fsynr1));
	}

	/* Don't print the debug if this is a permissions fault */
	if ((flags & IOMMU_FAULT_PERMISSION))
		return;

	kgsl_iommu_check_if_freed(device->dev, ctxt, addr, ptname);

	/*
	 * Don't print any debug information if the address is
	 * in the global region. These are rare and nobody needs
	 * to know the addresses that are in here
	 */
	if (kgsl_iommu_addr_is_global(mmu, addr)) {
		dev_crit(device->dev, "Fault in global memory\n");
		return;
	}

	if (!private)
		return;

	dev_crit(device->dev, "---- nearby memory ----\n");

	spin_lock(&private->mem_lock);
	idr_for_each_entry(&private->mem_idr, entry, id) {
		u64 cur = entry->memdesc.gpuaddr;

		if (cur < addr) {
			if (!prev || prev->memdesc.gpuaddr < cur)
				prev = entry;
		}

		if (cur > addr) {
			if (!next || next->memdesc.gpuaddr > cur)
				next = entry;
		}
	}

	print_entry(device->dev, prev, pid_nr(private->pid));
	dev_crit(device->dev, "<- fault @ %16.16lx\n", addr);
	print_entry(device->dev, next, pid_nr(private->pid));

	spin_unlock(&private->mem_lock);
}

/*
 * Return true if the IOMMU should stall and trigger a snapshot on a pagefault
 */
static bool kgsl_iommu_check_stall_on_fault(struct kgsl_iommu_context *ctx,
	struct kgsl_mmu *mmu, int flags)
{
	struct kgsl_device *device = KGSL_MMU_DEVICE(mmu);

	if (!(flags & IOMMU_FAULT_TRANSACTION_STALLED))
		return false;

	if (!test_bit(KGSL_FT_PAGEFAULT_GPUHALT_ENABLE, &mmu->pfpolicy))
		return false;

	if (test_bit(KGSL_MMU_PAGEFAULT_TERMINATE, &mmu->features))
		return false;

	/*
	 * Sometimes, there can be multiple invocations of the fault handler.
	 * Make sure we trigger reset/recovery only once.
	 */
	if (ctx->stalled_on_fault)
		return false;

	if (!mutex_trylock(&device->mutex))
		return true;

	/*
	 * Turn off GPU IRQ so we don't get faults from it too.
	 * The device mutex must be held to change power state
	 */
	if (gmu_core_isenabled(device))
		kgsl_pwrctrl_irq(device, false);
	else
		kgsl_pwrctrl_change_state(device, KGSL_STATE_AWARE);

	mutex_unlock(&device->mutex);
	return true;
}

static int kgsl_iommu_fault_handler(struct kgsl_mmu *mmu,
		struct kgsl_iommu_context *ctx, unsigned long addr, int flags)
{
	struct kgsl_device *device = KGSL_MMU_DEVICE(mmu);
	u64 ptbase;
	u32 contextidr;
	bool stall;
	struct kgsl_process_private *private;
	struct kgsl_context *context;

	ptbase = KGSL_IOMMU_GET_CTX_REG_Q(ctx, KGSL_IOMMU_CTX_TTBR0);
	contextidr = KGSL_IOMMU_GET_CTX_REG(ctx, KGSL_IOMMU_CTX_CONTEXTIDR);

	private = kgsl_iommu_get_process(ptbase);
	context = kgsl_context_get(device, contextidr);

	stall = kgsl_iommu_check_stall_on_fault(ctx, mmu, flags);

	kgsl_iommu_print_fault(mmu, ctx, addr, ptbase, contextidr, flags, private,
		context);
	kgsl_iommu_add_fault_info(context, addr, flags);

	if (stall) {
		struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
		u32 sctlr;

		/*
		 * Disable context fault interrupts as we do not clear FSR in
		 * the ISR. Will be re-enabled after FSR is cleared.
		 */
		sctlr = KGSL_IOMMU_GET_CTX_REG(ctx, KGSL_IOMMU_CTX_SCTLR);
		sctlr &= ~(0x1 << KGSL_IOMMU_SCTLR_CFIE_SHIFT);
		KGSL_IOMMU_SET_CTX_REG(ctx, KGSL_IOMMU_CTX_SCTLR, sctlr);

		/* This is used by reset/recovery path */
		ctx->stalled_on_fault = true;

		/* Go ahead with recovery*/
		if (adreno_dev->dispatch_ops && adreno_dev->dispatch_ops->fault)
			adreno_dev->dispatch_ops->fault(adreno_dev,
				ADRENO_IOMMU_PAGE_FAULT);
	}

	kgsl_context_put(context);
	kgsl_process_private_put(private);

	/* Return -EBUSY to keep the IOMMU driver from resuming on a stall */
	return stall ? -EBUSY : 0;
}

static int kgsl_iommu_default_fault_handler(struct iommu_domain *domain,
	struct device *dev, unsigned long addr, int flags, void *token)
{
	struct kgsl_mmu *mmu = token;
	struct kgsl_iommu *iommu = &mmu->iommu;

	return kgsl_iommu_fault_handler(mmu, &iommu->user_context,
		addr, flags);
}

static int kgsl_iommu_lpac_fault_handler(struct iommu_domain *domain,
	struct device *dev, unsigned long addr, int flags, void *token)
{
	struct kgsl_mmu *mmu = token;
	struct kgsl_iommu *iommu = &mmu->iommu;
	struct kgsl_iommu_context *ctx = &iommu->lpac_context;
	u32 fsynr0, fsynr1;

	fsynr0 = KGSL_IOMMU_GET_CTX_REG(ctx, KGSL_IOMMU_CTX_FSYNR0);
	fsynr1 = KGSL_IOMMU_GET_CTX_REG(ctx, KGSL_IOMMU_CTX_FSYNR1);

	dev_crit(KGSL_MMU_DEVICE(mmu)->dev,
		"LPAC PAGE FAULT iova=0x%16lx, fsynr0=0x%x, fsynr1=0x%x\n",
		addr, fsynr0, fsynr1);

	return kgsl_iommu_fault_handler(mmu, &iommu->lpac_context,
		addr, flags);
}

static int kgsl_iommu_secure_fault_handler(struct iommu_domain *domain,
	struct device *dev, unsigned long addr, int flags, void *token)
{
	struct kgsl_mmu *mmu = token;
	struct kgsl_iommu *iommu = &mmu->iommu;

	return kgsl_iommu_fault_handler(mmu, &iommu->secure_context,
		addr, flags);
}

/*
 * kgsl_iommu_disable_clk() - Disable iommu clocks
 * Disable IOMMU clocks
 */
static void kgsl_iommu_disable_clk(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = &mmu->iommu;

	atomic_dec(&iommu->clk_enable_count);

	/*
	 * Make sure the clk refcounts are good. An unbalance may
	 * cause the clocks to be off when we need them on.
	 */
	WARN_ON(atomic_read(&iommu->clk_enable_count) < 0);

	clk_bulk_disable_unprepare(iommu->num_clks, iommu->clks);

	if (!IS_ERR_OR_NULL(iommu->cx_gdsc))
		regulator_disable(iommu->cx_gdsc);
}

/*
 * kgsl_iommu_enable_clk - Enable iommu clocks
 * Enable all the IOMMU clocks
 */
static void kgsl_iommu_enable_clk(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = &mmu->iommu;

	if (!IS_ERR_OR_NULL(iommu->cx_gdsc))
		WARN_ON(regulator_enable(iommu->cx_gdsc));

	WARN_ON(clk_bulk_prepare_enable(iommu->num_clks, iommu->clks));

	atomic_inc(&iommu->clk_enable_count);
}

/* kgsl_iommu_get_ttbr0 - Get TTBR0 setting for a pagetable */
static u64 kgsl_iommu_get_ttbr0(struct kgsl_pagetable *pagetable)
{
	struct kgsl_iommu_pt *pt = to_iommu_pt(pagetable);

	/* This will be zero if KGSL_MMU_IOPGTABLE is not enabled */
	return pt->ttbr0;
}

/* Set TTBR0 for the given context with the specific configuration */
static void kgsl_iommu_set_ttbr0(struct kgsl_iommu_context *context,
		struct kgsl_mmu *mmu, const struct io_pgtable_cfg *pgtbl_cfg)
{
	struct adreno_smmu_priv *adreno_smmu;

	/* Quietly return if the context doesn't have a domain */
	if (!context->domain)
		return;

	adreno_smmu = dev_get_drvdata(&context->pdev->dev);

	/* Enable CX and clocks before we call into SMMU to setup registers */
	kgsl_iommu_enable_clk(mmu);
	adreno_smmu->set_ttbr0_cfg(adreno_smmu->cookie, pgtbl_cfg);
	kgsl_iommu_disable_clk(mmu);
}

static int kgsl_iommu_get_asid(struct kgsl_pagetable *pt, struct kgsl_context *context)
{
	struct kgsl_iommu *iommu = to_kgsl_iommu(pt);
	struct iommu_domain *domain;

	if (kgsl_context_is_lpac(context))
		domain = to_iommu_domain(&iommu->lpac_context);
	else
		domain = to_iommu_domain(&iommu->user_context);

	return qcom_iommu_get_asid_nr(domain);
}

static int kgsl_iommu_get_context_bank(struct kgsl_pagetable *pt, struct kgsl_context *context)
{
	struct kgsl_iommu *iommu = to_kgsl_iommu(pt);
	struct iommu_domain *domain;

	if (kgsl_context_is_lpac(context))
		domain = to_iommu_domain(&iommu->lpac_context);
	else
		domain = to_iommu_domain(&iommu->user_context);

	return _iommu_domain_context_bank(domain);
}

static void kgsl_iommu_destroy_default_pagetable(struct kgsl_pagetable *pagetable)
{
	struct kgsl_device *device = KGSL_MMU_DEVICE(pagetable->mmu);
	struct kgsl_iommu_pt *pt = to_iommu_pt(pagetable);
	struct kgsl_global_memdesc *md;

	list_for_each_entry(md, &device->globals, node) {
		if (md->memdesc.flags & KGSL_MEMFLAGS_SECURE)
			continue;

		kgsl_iommu_default_unmap(pagetable, &md->memdesc);
	}

	kfree(pt);
}

static void kgsl_iommu_destroy_pagetable(struct kgsl_pagetable *pagetable)
{
	struct kgsl_iommu_pt *pt = to_iommu_pt(pagetable);

	qcom_free_io_pgtable_ops(pt->pgtbl_ops);
	kfree(pt);
}

static void _enable_gpuhtw_llc(struct kgsl_mmu *mmu, struct iommu_domain *domain)
{
	int val = 1;

	if (!test_bit(KGSL_MMU_LLCC_ENABLE, &mmu->features))
		return;

	if (mmu->subtype == KGSL_IOMMU_SMMU_V500)
		iommu_domain_set_attr(domain, DOMAIN_ATTR_USE_LLC_NWA, &val);
	else
		iommu_domain_set_attr(domain, DOMAIN_ATTR_USE_UPSTREAM_HINT, &val);
}

static int set_smmu_aperture(struct kgsl_device *device,
		struct kgsl_iommu_context *context)
{
	int ret;

	if (!test_bit(KGSL_MMU_SMMU_APERTURE, &device->mmu.features))
		return 0;

	ret = qcom_scm_kgsl_set_smmu_aperture(context->cb_num);
	if (ret == -EBUSY)
		ret = qcom_scm_kgsl_set_smmu_aperture(context->cb_num);

	if (ret)
		dev_err(&device->pdev->dev, "Unable to set the SMMU aperture: %d. The aperture needs to be set to use per-process pagetables\n",
			ret);

	return ret;
}

static int set_smmu_lpac_aperture(struct kgsl_device *device,
		struct kgsl_iommu_context *context)
{
	int ret;

	if (!test_bit(KGSL_MMU_SMMU_APERTURE, &device->mmu.features))
		return 0;

	ret = qcom_scm_kgsl_set_smmu_lpac_aperture(context->cb_num);
	if (ret == -EBUSY)
		ret = qcom_scm_kgsl_set_smmu_lpac_aperture(context->cb_num);

	if (ret)
		dev_err(&device->pdev->dev, "Unable to set the LPAC SMMU aperture: %d. The aperture needs to be set to use per-process pagetables\n",
			ret);

	return ret;
}

/* FIXME: better name feor this function */
static int kgsl_iopgtbl_alloc(struct kgsl_iommu_context *ctx, struct kgsl_iommu_pt *pt)
{
	struct adreno_smmu_priv *adreno_smmu = dev_get_drvdata(&ctx->pdev->dev);
	const struct io_pgtable_cfg *cfg = NULL;
	void *domain = (void *)adreno_smmu->cookie;

	if (adreno_smmu->cookie)
		cfg = adreno_smmu->get_ttbr1_cfg(adreno_smmu->cookie);
	if (!cfg)
		return -ENODEV;

	pt->info = adreno_smmu->pgtbl_info;
	pt->info.cfg = *cfg;
	pt->info.cfg.quirks &= ~IO_PGTABLE_QUIRK_ARM_TTBR1;
	pt->info.cfg.tlb = &kgsl_iopgtbl_tlb_ops;
	pt->pgtbl_ops = qcom_alloc_io_pgtable_ops(QCOM_ARM_64_LPAE_S1, &pt->info, domain);

	if (!pt->pgtbl_ops)
		return -ENOMEM;

	pt->ttbr0 = pt->info.cfg.arm_lpae_s1_cfg.ttbr;

	return 0;
}

static struct kgsl_pagetable *kgsl_iommu_default_pagetable(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = &mmu->iommu;
	struct kgsl_iommu_pt *iommu_pt;
	int ret;

	iommu_pt = kzalloc(sizeof(*iommu_pt), GFP_KERNEL);
	if (!iommu_pt)
		return ERR_PTR(-ENOMEM);

	kgsl_mmu_pagetable_init(mmu, &iommu_pt->base, KGSL_MMU_GLOBAL_PT);

	iommu_pt->base.fault_addr = U64_MAX;
	iommu_pt->base.rbtree = RB_ROOT;
	iommu_pt->base.pt_ops = &default_pt_ops;

	if (test_bit(KGSL_MMU_64BIT, &mmu->features)) {
		iommu_pt->base.compat_va_start = KGSL_IOMMU_SVM_BASE32(mmu);
		if (test_bit(KGSL_MMU_IOPGTABLE, &mmu->features))
			iommu_pt->base.compat_va_end = KGSL_MEMSTORE_TOKEN_ADDRESS;
		else
			iommu_pt->base.compat_va_end = KGSL_IOMMU_GLOBAL_MEM_BASE64;
		iommu_pt->base.va_start = KGSL_IOMMU_VA_BASE64;
		iommu_pt->base.va_end = KGSL_IOMMU_VA_END64;

	} else {
		iommu_pt->base.va_start = KGSL_IOMMU_SVM_BASE32(mmu);
		iommu_pt->base.va_end = KGSL_IOMMU_GLOBAL_MEM_BASE(mmu);
		iommu_pt->base.compat_va_start = iommu_pt->base.va_start;
		iommu_pt->base.compat_va_end = iommu_pt->base.va_end;
	}

	if (!test_bit(KGSL_MMU_IOPGTABLE, &mmu->features)) {
		iommu_pt->base.global_base = KGSL_IOMMU_GLOBAL_MEM_BASE(mmu);

		kgsl_mmu_pagetable_add(mmu, &iommu_pt->base);
		return &iommu_pt->base;
	}

	iommu_pt->base.global_base = KGSL_IOMMU_SPLIT_TABLE_BASE;

	/*
	 * Set up a "default' TTBR0 for the pagetable - this would only be used
	 * in cases when the per-process pagetable allocation failed for some
	 * reason
	 */
	ret = kgsl_iopgtbl_alloc(&iommu->user_context, iommu_pt);
	if (ret) {
		kfree(iommu_pt);
		return ERR_PTR(ret);
	}

	kgsl_mmu_pagetable_add(mmu, &iommu_pt->base);
	return &iommu_pt->base;

}

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)
static struct kgsl_pagetable *kgsl_iommu_secure_pagetable(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu_pt *iommu_pt;

	if (!mmu->secured)
		return ERR_PTR(-EPERM);

	iommu_pt = kzalloc(sizeof(*iommu_pt), GFP_KERNEL);
	if (!iommu_pt)
		return ERR_PTR(-ENOMEM);

	kgsl_mmu_pagetable_init(mmu, &iommu_pt->base, KGSL_MMU_SECURE_PT);
	iommu_pt->base.fault_addr = U64_MAX;
	iommu_pt->base.rbtree = RB_ROOT;
	iommu_pt->base.pt_ops = &secure_pt_ops;

	iommu_pt->base.compat_va_start = KGSL_IOMMU_SECURE_BASE32;
	iommu_pt->base.compat_va_end = KGSL_IOMMU_SECURE_END32;
	iommu_pt->base.va_start = KGSL_IOMMU_SECURE_BASE(mmu);
	iommu_pt->base.va_end = KGSL_IOMMU_SECURE_END(mmu);

	kgsl_mmu_pagetable_add(mmu, &iommu_pt->base);
	return &iommu_pt->base;
}
#else
static struct kgsl_pagetable *kgsl_iommu_secure_pagetable(struct kgsl_mmu *mmu)
{
	return ERR_PTR(-EPERM);
}
#endif

static struct kgsl_pagetable *kgsl_iopgtbl_pagetable(struct kgsl_mmu *mmu, u32 name)
{
	struct kgsl_iommu *iommu = &mmu->iommu;
	struct kgsl_iommu_pt *pt;
	int ret;

	pt = kzalloc(sizeof(*pt), GFP_KERNEL);
	if (!pt)
		return ERR_PTR(-ENOMEM);

	kgsl_mmu_pagetable_init(mmu, &pt->base, name);

	pt->base.fault_addr = U64_MAX;
	pt->base.rbtree = RB_ROOT;
	pt->base.pt_ops = &iopgtbl_pt_ops;

	if (test_bit(KGSL_MMU_64BIT, &mmu->features)) {
		pt->base.compat_va_start = KGSL_IOMMU_SVM_BASE32(mmu);
		pt->base.compat_va_end = KGSL_MEMSTORE_TOKEN_ADDRESS;
		pt->base.va_start = KGSL_IOMMU_VA_BASE64;
		pt->base.va_end = KGSL_IOMMU_VA_END64;

		if (is_compat_task()) {
			pt->base.svm_start = KGSL_IOMMU_SVM_BASE32(mmu);
			pt->base.svm_end = KGSL_MEMSTORE_TOKEN_ADDRESS;
		} else {
			pt->base.svm_start = KGSL_IOMMU_SVM_BASE64;
			pt->base.svm_end = KGSL_IOMMU_SVM_END64;
		}

	} else {
		pt->base.va_start = KGSL_IOMMU_SVM_BASE32(mmu);
		pt->base.va_end = KGSL_IOMMU_GLOBAL_MEM_BASE(mmu);
		pt->base.compat_va_start = pt->base.va_start;
		pt->base.compat_va_end = pt->base.va_end;
		pt->base.svm_start = KGSL_IOMMU_SVM_BASE32(mmu);
		pt->base.svm_end = KGSL_IOMMU_SVM_END32;
	}

	ret = kgsl_iopgtbl_alloc(&iommu->user_context, pt);
	if (ret) {
		kfree(pt);
		return ERR_PTR(ret);
	}

	kgsl_mmu_pagetable_add(mmu, &pt->base);
	return &pt->base;
}

static struct kgsl_pagetable *kgsl_iommu_getpagetable(struct kgsl_mmu *mmu,
		unsigned long name)
{
	struct kgsl_pagetable *pt;

	/* If we already know the pagetable, return it */
	pt = kgsl_get_pagetable(name);
	if (pt)
		return pt;

	/* If io-pgtables are not in effect, just use the default pagetable */
	if (!test_bit(KGSL_MMU_IOPGTABLE, &mmu->features))
		return mmu->defaultpagetable;

	pt = kgsl_iopgtbl_pagetable(mmu, name);

	/*
	 * If the io-pgtable allocation didn't work then fall back to the
	 * default pagetable for this cycle
	 */
	if (!pt)
		return mmu->defaultpagetable;

	return pt;
}

static void kgsl_iommu_detach_context(struct kgsl_iommu_context *context)
{
	if (!context->domain)
		return;

	iommu_detach_device(context->domain, &context->pdev->dev);
	iommu_domain_free(context->domain);

	context->domain = NULL;

	platform_device_put(context->pdev);

	context->pdev = NULL;
}

static void kgsl_iommu_close(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = &mmu->iommu;

	/* First put away the default pagetables */
	kgsl_mmu_putpagetable(mmu->defaultpagetable);
	kgsl_mmu_putpagetable(mmu->securepagetable);

	/*
	 * Flush the workqueue to ensure pagetables are
	 * destroyed before proceeding further
	 */
	flush_workqueue(kgsl_driver.workqueue);

	mmu->defaultpagetable = NULL;
	mmu->securepagetable = NULL;

	kgsl_iommu_set_ttbr0(&iommu->lpac_context, mmu, NULL);
	kgsl_iommu_set_ttbr0(&iommu->user_context, mmu, NULL);

	/* Next, detach the context banks */
	kgsl_iommu_detach_context(&iommu->user_context);
	kgsl_iommu_detach_context(&iommu->lpac_context);
	kgsl_iommu_detach_context(&iommu->secure_context);

	kgsl_free_secure_page(kgsl_secure_guard_page);
	kgsl_secure_guard_page = NULL;

	if (kgsl_guard_page != NULL) {
		__free_page(kgsl_guard_page);
		kgsl_guard_page = NULL;
	}

	kmem_cache_destroy(addr_entry_cache);
	addr_entry_cache = NULL;
}

/* Program the PRR marker and enable it in the ACTLR register */
static void _iommu_context_set_prr(struct kgsl_mmu *mmu,
		struct kgsl_iommu_context *ctx)
{
	struct kgsl_iommu *iommu = &mmu->iommu;
	struct page *page = kgsl_vbo_zero_page;
	u32 val;

	if (ctx->cb_num < 0)
		return;

	/* Quietly return if the context doesn't have a domain */
	if (!ctx->domain)
		return;

	if (!page)
		return;

	writel_relaxed(lower_32_bits(page_to_phys(page)),
		iommu->regbase + KGSL_IOMMU_PRR_CFG_LADDR);

	writel_relaxed(upper_32_bits(page_to_phys(page)),
		iommu->regbase + KGSL_IOMMU_PRR_CFG_UADDR);

	val = KGSL_IOMMU_GET_CTX_REG(ctx, KGSL_IOMMU_CTX_ACTLR);
	val |= FIELD_PREP(KGSL_IOMMU_ACTLR_PRR_ENABLE, 1);
	KGSL_IOMMU_SET_CTX_REG(ctx, KGSL_IOMMU_CTX_ACTLR, val);

	/* Make sure all of the preceding writes have posted */
	wmb();
}

static void kgsl_iommu_configure_gpu_sctlr(struct kgsl_mmu *mmu,
		unsigned long pf_policy,
		struct kgsl_iommu_context *ctx)
{
	u32 sctlr_val;

	/*
	 * If pagefault policy is GPUHALT_ENABLE,
	 *   If terminate feature flag is enabled:
	 *     1) Program CFCFG to 0 to terminate the faulting transaction
	 *     2) Program HUPCF to 0 (terminate subsequent transactions
	 *        in the presence of an outstanding fault)
	 *   Else configure stall:
	 *     1) Program CFCFG to 1 to enable STALL mode
	 *     2) Program HUPCF to 0 (Stall subsequent
	 *        transactions in the presence of an outstanding fault)
	 * else
	 * 1) Program CFCFG to 0 to disable STALL mode (0=Terminate)
	 * 2) Program HUPCF to 1 (Process subsequent transactions
	 *    independently of any outstanding fault)
	 */

	sctlr_val = KGSL_IOMMU_GET_CTX_REG(ctx, KGSL_IOMMU_CTX_SCTLR);
	if (test_bit(KGSL_FT_PAGEFAULT_GPUHALT_ENABLE, &pf_policy)) {
		if (test_bit(KGSL_MMU_PAGEFAULT_TERMINATE, &mmu->features)) {
			sctlr_val &= ~(0x1 << KGSL_IOMMU_SCTLR_CFCFG_SHIFT);
			sctlr_val &= ~(0x1 << KGSL_IOMMU_SCTLR_HUPCF_SHIFT);
		} else {
			sctlr_val |= (0x1 << KGSL_IOMMU_SCTLR_CFCFG_SHIFT);
			sctlr_val &= ~(0x1 << KGSL_IOMMU_SCTLR_HUPCF_SHIFT);
		}
	} else {
		sctlr_val &= ~(0x1 << KGSL_IOMMU_SCTLR_CFCFG_SHIFT);
		sctlr_val |= (0x1 << KGSL_IOMMU_SCTLR_HUPCF_SHIFT);
	}
	KGSL_IOMMU_SET_CTX_REG(ctx, KGSL_IOMMU_CTX_SCTLR, sctlr_val);
}

static int kgsl_iommu_start(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = &mmu->iommu;

	kgsl_iommu_enable_clk(mmu);

	/* Set the following registers only when the MMU type is QSMMU */
	if (mmu->subtype != KGSL_IOMMU_SMMU_V500) {
		/* Enable hazard check from GPU_SMMU_HUM_CFG */
		writel_relaxed(0x02, iommu->regbase + 0x6800);

		/* Write to GPU_SMMU_DORA_ORDERING to disable reordering */
		writel_relaxed(0x01, iommu->regbase + 0x64a0);

		/* make sure register write committed */
		wmb();
	}

	kgsl_iommu_configure_gpu_sctlr(mmu, mmu->pfpolicy, &iommu->user_context);

	_iommu_context_set_prr(mmu, &iommu->user_context);
	if (mmu->secured)
		_iommu_context_set_prr(mmu, &iommu->secure_context);

	if (iommu->lpac_context.domain) {
		_iommu_context_set_prr(mmu, &iommu->lpac_context);
		kgsl_iommu_configure_gpu_sctlr(mmu, mmu->pfpolicy, &iommu->lpac_context);
	}

	kgsl_iommu_disable_clk(mmu);
	return 0;
}

static void kgsl_iommu_context_clear_fsr(struct kgsl_mmu *mmu, struct kgsl_iommu_context *ctx)
{
	unsigned int sctlr_val;

	if (ctx->stalled_on_fault) {
		kgsl_iommu_enable_clk(mmu);
		KGSL_IOMMU_SET_CTX_REG(ctx, KGSL_IOMMU_CTX_FSR, 0xffffffff);
		/*
		 * Re-enable context fault interrupts after clearing
		 * FSR to prevent the interrupt from firing repeatedly
		 */
		sctlr_val = KGSL_IOMMU_GET_CTX_REG(ctx, KGSL_IOMMU_CTX_SCTLR);
		sctlr_val |= (0x1 << KGSL_IOMMU_SCTLR_CFIE_SHIFT);
		KGSL_IOMMU_SET_CTX_REG(ctx, KGSL_IOMMU_CTX_SCTLR, sctlr_val);
		/*
		 * Make sure the above register writes
		 * are not reordered across the barrier
		 * as we use writel_relaxed to write them
		 */
		wmb();
		kgsl_iommu_disable_clk(mmu);
		ctx->stalled_on_fault = false;
	}
}

static void kgsl_iommu_clear_fsr(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = &mmu->iommu;

	kgsl_iommu_context_clear_fsr(mmu, &iommu->user_context);

	if (iommu->lpac_context.domain)
		kgsl_iommu_context_clear_fsr(mmu, &iommu->lpac_context);
}

static void kgsl_iommu_context_pagefault_resume(struct kgsl_iommu *iommu,
		struct kgsl_iommu_context *ctx, bool terminate)
{
	u32 sctlr_val = 0;

	if (!ctx->stalled_on_fault)
		return;

	if (!terminate)
		goto clear_fsr;

	sctlr_val = KGSL_IOMMU_GET_CTX_REG(ctx, KGSL_IOMMU_CTX_SCTLR);
	/*
	 * As part of recovery, GBIF halt sequence should be performed.
	 * In a worst case scenario, if any GPU block is generating a
	 * stream of un-ending faulting transactions, SMMU would enter
	 * stall-on-fault mode again after resuming and not let GBIF
	 * halt succeed. In order to avoid that situation and terminate
	 * those faulty transactions, set CFCFG and HUPCF to 0.
	 */
	sctlr_val &= ~(0x1 << KGSL_IOMMU_SCTLR_CFCFG_SHIFT);
	sctlr_val &= ~(0x1 << KGSL_IOMMU_SCTLR_HUPCF_SHIFT);
	KGSL_IOMMU_SET_CTX_REG(ctx, KGSL_IOMMU_CTX_SCTLR, sctlr_val);
	/*
	 * Make sure the above register write is not reordered across
	 * the barrier as we use writel_relaxed to write it.
	 */
	wmb();

clear_fsr:
	/*
	 * This will only clear fault bits in FSR. FSR.SS will still
	 * be set. Writing to RESUME (below) is the only way to clear
	 * FSR.SS bit.
	 */
	KGSL_IOMMU_SET_CTX_REG(ctx, KGSL_IOMMU_CTX_FSR, 0xffffffff);
	/*
	 * Make sure the above register write is not reordered across
	 * the barrier as we use writel_relaxed to write it.
	 */
	wmb();

	/*
	 * Write 1 to RESUME.TnR to terminate the stalled transaction.
	 * This will also allow the SMMU to process new transactions.
	 */
	KGSL_IOMMU_SET_CTX_REG(ctx, KGSL_IOMMU_CTX_RESUME, 1);
	/*
	 * Make sure the above register writes are not reordered across
	 * the barrier as we use writel_relaxed to write them.
	 */
	wmb();
}

static void kgsl_iommu_pagefault_resume(struct kgsl_mmu *mmu, bool terminate)
{
	struct kgsl_iommu *iommu = &mmu->iommu;

	kgsl_iommu_context_pagefault_resume(iommu, &iommu->user_context, terminate);

	if (iommu->lpac_context.domain)
		kgsl_iommu_context_pagefault_resume(iommu, &iommu->lpac_context, terminate);
}

static u64
kgsl_iommu_get_current_ttbr0(struct kgsl_mmu *mmu, struct kgsl_context *context)
{
	u64 val;
	struct kgsl_iommu *iommu = &mmu->iommu;
	struct kgsl_iommu_context *ctx = &iommu->user_context;

	if (kgsl_context_is_lpac(context))
		ctx = &iommu->lpac_context;

	/*
	 * We cannot enable or disable the clocks in interrupt context, this
	 * function is called from interrupt context if there is an axi error
	 */
	if (in_interrupt())
		return 0;

	if (ctx->cb_num < 0)
		return 0;

	kgsl_iommu_enable_clk(mmu);
	val = KGSL_IOMMU_GET_CTX_REG_Q(ctx, KGSL_IOMMU_CTX_TTBR0);
	kgsl_iommu_disable_clk(mmu);
	return val;
}

/*
 * kgsl_iommu_set_pf_policy() - Set the pagefault policy for IOMMU
 * @mmu: Pointer to mmu structure
 * @pf_policy: The pagefault polict to set
 *
 * Check if the new policy indicated by pf_policy is same as current
 * policy, if same then return else set the policy
 */
static int kgsl_iommu_set_pf_policy_ctxt(struct kgsl_mmu *mmu,
				unsigned long pf_policy, struct kgsl_iommu_context *ctx)
{
	int cur, new;
	struct kgsl_iommu *iommu = &mmu->iommu;

	cur = test_bit(KGSL_FT_PAGEFAULT_GPUHALT_ENABLE, &mmu->pfpolicy);
	new = test_bit(KGSL_FT_PAGEFAULT_GPUHALT_ENABLE, &pf_policy);

	if (cur == new)
		return 0;

	kgsl_iommu_enable_clk(mmu);

	kgsl_iommu_configure_gpu_sctlr(mmu, pf_policy, &iommu->user_context);
	if (iommu->lpac_context.domain)
		kgsl_iommu_configure_gpu_sctlr(mmu, pf_policy, &iommu->lpac_context);

	kgsl_iommu_disable_clk(mmu);
	return 0;
}

static int kgsl_iommu_set_pf_policy(struct kgsl_mmu *mmu,
				unsigned long pf_policy)
{
	struct kgsl_iommu *iommu = &mmu->iommu;

	kgsl_iommu_set_pf_policy_ctxt(mmu, pf_policy, &iommu->user_context);

	if (iommu->lpac_context.domain)
		kgsl_iommu_set_pf_policy_ctxt(mmu, pf_policy, &iommu->lpac_context);

	return 0;
}

static struct kgsl_iommu_addr_entry *_find_gpuaddr(
		struct kgsl_pagetable *pagetable, uint64_t gpuaddr)
{
	struct rb_node *node = pagetable->rbtree.rb_node;

	while (node != NULL) {
		struct kgsl_iommu_addr_entry *entry = rb_entry(node,
			struct kgsl_iommu_addr_entry, node);

		if (gpuaddr < entry->base)
			node = node->rb_left;
		else if (gpuaddr > entry->base)
			node = node->rb_right;
		else
			return entry;
	}

	return NULL;
}

static int _remove_gpuaddr(struct kgsl_pagetable *pagetable,
		uint64_t gpuaddr)
{
	struct kgsl_iommu_addr_entry *entry;

	entry = _find_gpuaddr(pagetable, gpuaddr);

	if (WARN(!entry, "GPU address %llx doesn't exist\n", gpuaddr))
		return -ENOMEM;

	rb_erase(&entry->node, &pagetable->rbtree);
	kmem_cache_free(addr_entry_cache, entry);
	return 0;
}

static int _insert_gpuaddr(struct kgsl_pagetable *pagetable,
		uint64_t gpuaddr, uint64_t size)
{
	struct rb_node **node, *parent = NULL;
	struct kgsl_iommu_addr_entry *new =
		kmem_cache_alloc(addr_entry_cache, GFP_ATOMIC);

	if (new == NULL)
		return -ENOMEM;

	new->base = gpuaddr;
	new->size = size;

	node = &pagetable->rbtree.rb_node;

	while (*node != NULL) {
		struct kgsl_iommu_addr_entry *this;

		parent = *node;
		this = rb_entry(parent, struct kgsl_iommu_addr_entry, node);

		if (new->base < this->base)
			node = &parent->rb_left;
		else if (new->base > this->base)
			node = &parent->rb_right;
		else {
			/* Duplicate entry */
			WARN(1, "duplicate gpuaddr: 0x%llx\n", gpuaddr);
			kmem_cache_free(addr_entry_cache, new);
			return -EEXIST;
		}
	}

	rb_link_node(&new->node, parent, node);
	rb_insert_color(&new->node, &pagetable->rbtree);

	return 0;
}

static uint64_t _get_unmapped_area(struct kgsl_pagetable *pagetable,
		uint64_t bottom, uint64_t top, uint64_t size,
		uint64_t align)
{
	struct rb_node *node = rb_first(&pagetable->rbtree);
	uint64_t start;

	bottom = ALIGN(bottom, align);
	start = bottom;

	while (node != NULL) {
		uint64_t gap;
		struct kgsl_iommu_addr_entry *entry = rb_entry(node,
			struct kgsl_iommu_addr_entry, node);

		/*
		 * Skip any entries that are outside of the range, but make sure
		 * to account for some that might straddle the lower bound
		 */
		if (entry->base < bottom) {
			if (entry->base + entry->size > bottom)
				start = ALIGN(entry->base + entry->size, align);
			node = rb_next(node);
			continue;
		}

		/* Stop if we went over the top */
		if (entry->base >= top)
			break;

		/* Make sure there is a gap to consider */
		if (start < entry->base) {
			gap = entry->base - start;

			if (gap >= size)
				return start;
		}

		/* Stop if there is no more room in the region */
		if (entry->base + entry->size >= top)
			return (uint64_t) -ENOMEM;

		/* Start the next cycle at the end of the current entry */
		start = ALIGN(entry->base + entry->size, align);
		node = rb_next(node);
	}

	if (start + size <= top)
		return start;

	return (uint64_t) -ENOMEM;
}

static uint64_t _get_unmapped_area_topdown(struct kgsl_pagetable *pagetable,
		uint64_t bottom, uint64_t top, uint64_t size,
		uint64_t align)
{
	struct rb_node *node = rb_last(&pagetable->rbtree);
	uint64_t end = top;
	uint64_t mask = ~(align - 1);
	struct kgsl_iommu_addr_entry *entry;

	/* Make sure that the bottom is correctly aligned */
	bottom = ALIGN(bottom, align);

	/* Make sure the requested size will fit in the range */
	if (size > (top - bottom))
		return -ENOMEM;

	/* Walk back through the list to find the highest entry in the range */
	for (node = rb_last(&pagetable->rbtree); node != NULL; node = rb_prev(node)) {
		entry = rb_entry(node, struct kgsl_iommu_addr_entry, node);
		if (entry->base < top)
			break;
	}

	while (node != NULL) {
		uint64_t offset;

		entry = rb_entry(node, struct kgsl_iommu_addr_entry, node);

		/* If the entire entry is below the range the search is over */
		if ((entry->base + entry->size) < bottom)
			break;

		/* Get the top of the entry properly aligned */
		offset = ALIGN(entry->base + entry->size, align);

		/*
		 * Try to allocate the memory from the top of the gap,
		 * making sure that it fits between the top of this entry and
		 * the bottom of the previous one
		 */

		if ((end > size) && (offset < end)) {
			uint64_t chunk = (end - size) & mask;

			if (chunk >= offset)
				return chunk;
		}

		/*
		 * If we get here and the current entry is outside of the range
		 * then we are officially out of room
		 */

		if (entry->base < bottom)
			return (uint64_t) -ENOMEM;

		/* Set the top of the gap to the current entry->base */
		end = entry->base;

		/* And move on to the next lower entry */
		node = rb_prev(node);
	}

	/* If we get here then there are no more entries in the region */
	if ((end > size) && (((end - size) & mask) >= bottom))
		return (end - size) & mask;

	return (uint64_t) -ENOMEM;
}

static uint64_t kgsl_iommu_find_svm_region(struct kgsl_pagetable *pagetable,
		uint64_t start, uint64_t end, uint64_t size,
		uint64_t alignment)
{
	uint64_t addr;

	/* Avoid black holes */
	if (WARN(end <= start, "Bad search range: 0x%llx-0x%llx", start, end))
		return (uint64_t) -EINVAL;

	spin_lock(&pagetable->lock);
	addr = _get_unmapped_area_topdown(pagetable,
			start, end, size, alignment);
	spin_unlock(&pagetable->lock);
	return addr;
}

static bool iommu_addr_in_svm_ranges(struct kgsl_pagetable *pagetable,
	u64 gpuaddr, u64 size)
{
	if ((gpuaddr >= pagetable->compat_va_start && gpuaddr < pagetable->compat_va_end) &&
		((gpuaddr + size) > pagetable->compat_va_start &&
			(gpuaddr + size) <= pagetable->compat_va_end))
		return true;

	if ((gpuaddr >= pagetable->svm_start && gpuaddr < pagetable->svm_end) &&
		((gpuaddr + size) > pagetable->svm_start &&
			(gpuaddr + size) <= pagetable->svm_end))
		return true;

	return false;
}

static int kgsl_iommu_set_svm_region(struct kgsl_pagetable *pagetable,
		uint64_t gpuaddr, uint64_t size)
{
	int ret = -ENOMEM;
	struct rb_node *node;

	/* Make sure the requested address doesn't fall out of SVM range */
	if (!iommu_addr_in_svm_ranges(pagetable, gpuaddr, size))
		return -ENOMEM;

	spin_lock(&pagetable->lock);
	node = pagetable->rbtree.rb_node;

	while (node != NULL) {
		uint64_t start, end;
		struct kgsl_iommu_addr_entry *entry = rb_entry(node,
			struct kgsl_iommu_addr_entry, node);

		start = entry->base;
		end = entry->base + entry->size;

		if (gpuaddr  + size <= start)
			node = node->rb_left;
		else if (end <= gpuaddr)
			node = node->rb_right;
		else
			goto out;
	}

	ret = _insert_gpuaddr(pagetable, gpuaddr, size);
out:
	spin_unlock(&pagetable->lock);
	return ret;
}


static int kgsl_iommu_get_gpuaddr(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
	int ret = 0;
	uint64_t addr, start, end, size;
	unsigned int align;

	if (WARN_ON(kgsl_memdesc_use_cpu_map(memdesc)))
		return -EINVAL;

	if (memdesc->flags & KGSL_MEMFLAGS_SECURE &&
			pagetable->name != KGSL_MMU_SECURE_PT)
		return -EINVAL;

	size = kgsl_memdesc_footprint(memdesc);

	align = max_t(uint64_t, 1 << kgsl_memdesc_get_align(memdesc),
			PAGE_SIZE);

	if (memdesc->flags & KGSL_MEMFLAGS_FORCE_32BIT) {
		start = pagetable->compat_va_start;
		end = pagetable->compat_va_end;
	} else {
		start = pagetable->va_start;
		end = pagetable->va_end;
	}

	spin_lock(&pagetable->lock);

	addr = _get_unmapped_area(pagetable, start, end, size, align);

	if (addr == (uint64_t) -ENOMEM) {
		ret = -ENOMEM;
		goto out;
	}

	/*
	 * This path is only called in a non-SVM path with locks so we can be
	 * sure we aren't racing with anybody so we don't need to worry about
	 * taking the lock
	 */
	ret = _insert_gpuaddr(pagetable, addr, size);
	if (ret == 0) {
		memdesc->gpuaddr = addr;
		memdesc->pagetable = pagetable;
	}

out:
	spin_unlock(&pagetable->lock);
	return ret;
}

static void kgsl_iommu_put_gpuaddr(struct kgsl_memdesc *memdesc)
{
	if (memdesc->pagetable == NULL)
		return;

	spin_lock(&memdesc->pagetable->lock);

	_remove_gpuaddr(memdesc->pagetable, memdesc->gpuaddr);

	spin_unlock(&memdesc->pagetable->lock);
}

static int kgsl_iommu_svm_range(struct kgsl_pagetable *pagetable,
		uint64_t *lo, uint64_t *hi, uint64_t memflags)
{
	bool gpu_compat = (memflags & KGSL_MEMFLAGS_FORCE_32BIT) != 0;

	if (lo != NULL)
		*lo = gpu_compat ? pagetable->compat_va_start : pagetable->svm_start;
	if (hi != NULL)
		*hi = gpu_compat ? pagetable->compat_va_end : pagetable->svm_end;

	return 0;
}

static bool kgsl_iommu_addr_in_range(struct kgsl_pagetable *pagetable,
		uint64_t gpuaddr, uint64_t size)
{
	if (gpuaddr == 0)
		return false;

	if (gpuaddr >= pagetable->va_start && (gpuaddr + size) <
			pagetable->va_end)
		return true;

	if (gpuaddr >= pagetable->compat_va_start && (gpuaddr + size) <
			pagetable->compat_va_end)
		return true;

	if (gpuaddr >= pagetable->svm_start && (gpuaddr + size) <
			pagetable->svm_end)
		return true;

	return false;
}

static int kgsl_iommu_setup_context(struct kgsl_mmu *mmu,
		struct device_node *parent,
		struct kgsl_iommu_context *context, const char *name,
		iommu_fault_handler_t handler)
{
	struct device_node *node = of_find_node_by_name(parent, name);
	struct platform_device *pdev;
	struct kgsl_device *device = KGSL_MMU_DEVICE(mmu);
	int ret;

	if (!node)
		return -ENOENT;

	pdev = of_find_device_by_node(node);
	ret = of_dma_configure(&pdev->dev, node, true);
	of_node_put(node);

	if (ret)
		return ret;

	context->cb_num = -1;
	context->name = name;
	context->kgsldev = device;
	context->pdev = pdev;
	ratelimit_default_init(&context->ratelimit);

	/* Set the adreno_smmu priv data for the device */
	dev_set_drvdata(&pdev->dev, &context->adreno_smmu);

	/* Create a new context */
	context->domain = iommu_domain_alloc(&platform_bus_type);
	if (!context->domain) {
		/*FIXME: Put back the pdev here? */
		return -ENODEV;
	}

	_enable_gpuhtw_llc(mmu, context->domain);

	ret = iommu_attach_device(context->domain, &context->pdev->dev);
	if (ret) {
		/* FIXME: put back the device here? */
		iommu_domain_free(context->domain);
		context->domain = NULL;
		return ret;
	}

	iommu_set_fault_handler(context->domain, handler, mmu);

	context->cb_num = _iommu_domain_context_bank(context->domain);

	if (context->cb_num >= 0)
		return 0;

	dev_err(&device->pdev->dev, "Couldn't get the context bank for %s: %d\n",
		context->name, context->cb_num);

	iommu_detach_device(context->domain, &context->pdev->dev);
	iommu_domain_free(context->domain);

	/* FIXME: put back the device here? */
	context->domain = NULL;

	return context->cb_num;
}

static int iommu_probe_user_context(struct kgsl_device *device,
		struct device_node *node)
{
	struct kgsl_iommu *iommu = KGSL_IOMMU(device);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_mmu *mmu = &device->mmu;
	struct kgsl_iommu_pt *pt;
	int ret;

	ret = kgsl_iommu_setup_context(mmu, node, &iommu->user_context,
		"gfx3d_user", kgsl_iommu_default_fault_handler);
	if (ret)
		return ret;

	/*
	 * It is problematic if smmu driver does system suspend before consumer
	 * device (gpu). So smmu driver creates a device_link to act as a
	 * supplier which in turn will ensure correct order during system
	 * suspend. In kgsl, since we don't initialize iommu on the gpu device,
	 * we should create a device_link between kgsl iommu device and gpu
	 * device to maintain a correct suspend order between smmu device and
	 * gpu device.
	 */
	if (!device_link_add(&device->pdev->dev, &iommu->user_context.pdev->dev,
			DL_FLAG_AUTOREMOVE_CONSUMER))
		dev_err(&iommu->user_context.pdev->dev,
				"Unable to create device link to gpu device\n");

	ret = kgsl_iommu_setup_context(mmu, node, &iommu->lpac_context,
			"gfx3d_lpac", kgsl_iommu_lpac_fault_handler);
	/* LPAC is optional, ignore setup failures in absence of LPAC feature */
	if ((ret < 0) && ADRENO_FEATURE(adreno_dev, ADRENO_LPAC))
		goto err;

	/*
	 * FIXME: If adreno_smmu->cookie wasn't initialized then we can't do
	 * IOPGTABLE
	 */

	/* Make the default pagetable */
	mmu->defaultpagetable = kgsl_iommu_default_pagetable(mmu);
	if (IS_ERR(mmu->defaultpagetable))
		return PTR_ERR(mmu->defaultpagetable);

	/* If IOPGTABLE isn't enabled then we are done */
	if (!test_bit(KGSL_MMU_IOPGTABLE, &mmu->features))
		return 0;

	pt = to_iommu_pt(mmu->defaultpagetable);

	/* Enable TTBR0 on the default and LPAC contexts */
	kgsl_iommu_set_ttbr0(&iommu->user_context, mmu, &pt->info.cfg);

	set_smmu_aperture(device, &iommu->user_context);

	kgsl_iommu_set_ttbr0(&iommu->lpac_context, mmu, &pt->info.cfg);

	ret = set_smmu_lpac_aperture(device, &iommu->lpac_context);
	/* LPAC is optional, ignore setup failures in absence of LPAC feature */
	if ((ret < 0) && ADRENO_FEATURE(adreno_dev, ADRENO_LPAC)) {
		kgsl_iommu_detach_context(&iommu->lpac_context);
		goto err;
	}

	return 0;

err:
	kgsl_mmu_putpagetable(mmu->defaultpagetable);
	mmu->defaultpagetable = NULL;
	kgsl_iommu_detach_context(&iommu->user_context);

	return ret;
}

static int iommu_probe_secure_context(struct kgsl_device *device,
		struct device_node *parent)
{
	struct device_node *node;
	struct platform_device *pdev;
	int ret;
	struct kgsl_iommu *iommu = KGSL_IOMMU(device);
	struct kgsl_mmu *mmu = &device->mmu;
	struct kgsl_iommu_context *context = &iommu->secure_context;
	int secure_vmid = VMID_CP_PIXEL;

	if (!mmu->secured)
		return -EPERM;

	node = of_find_node_by_name(parent, "gfx3d_secure");
	if (!node)
		return -ENOENT;

	pdev = of_find_device_by_node(node);
	ret = of_dma_configure(&pdev->dev, node, true);
	of_node_put(node);

	if (ret)
		return ret;

	context->cb_num = -1;
	context->name = "gfx3d_secure";
	context->kgsldev = device;
	context->pdev = pdev;
	ratelimit_default_init(&context->ratelimit);

	context->domain = iommu_domain_alloc(&platform_bus_type);
	if (!context->domain) {
		/* FIXME: put away the device */
		return -ENODEV;
	}

	ret = iommu_domain_set_attr(context->domain, DOMAIN_ATTR_SECURE_VMID,
		&secure_vmid);
	if (ret) {
		dev_err(&device->pdev->dev, "Unable to set the secure VMID: %d\n", ret);
		iommu_domain_free(context->domain);
		context->domain = NULL;

		/* FIXME: put away the device */
		return ret;
	}

	_enable_gpuhtw_llc(mmu, context->domain);

	ret = iommu_attach_device(context->domain, &context->pdev->dev);
	if (ret) {
		iommu_domain_free(context->domain);
		/* FIXME: Put way the device */
		context->domain = NULL;
		return ret;
	}

	iommu_set_fault_handler(context->domain,
		kgsl_iommu_secure_fault_handler, mmu);

	context->cb_num = _iommu_domain_context_bank(context->domain);

	if (context->cb_num < 0) {
		iommu_detach_device(context->domain, &context->pdev->dev);
		iommu_domain_free(context->domain);
		context->domain = NULL;
		return context->cb_num;
	}

	mmu->securepagetable = kgsl_iommu_secure_pagetable(mmu);

	if (IS_ERR(mmu->securepagetable))
		mmu->secured = false;

	return 0;
}

static const char * const kgsl_iommu_clocks[] = {
	"gcc_gpu_memnoc_gfx",
	"gcc_gpu_snoc_dvm_gfx",
	"gpu_cc_ahb",
	"gpu_cc_cx_gmu",
	"gpu_cc_hlos1_vote_gpu_smmu",
	"gpu_cc_hub_aon",
	"gpu_cc_hub_cx_int",
	"gcc_bimc_gpu_axi",
	"gcc_gpu_ahb",
	"gcc_gpu_axi_clk",
};

static const struct kgsl_mmu_ops kgsl_iommu_ops;

static void kgsl_iommu_check_config(struct kgsl_mmu *mmu,
		struct device_node *parent)
{
	struct device_node *node = of_find_node_by_name(parent, "gfx3d_user");
	struct device_node *phandle;

	if (!node)
		return;

	phandle = of_parse_phandle(node, "iommus", 0);

	if (phandle) {
		if (of_device_is_compatible(phandle, "qcom,qsmmu-v500"))
			mmu->subtype = KGSL_IOMMU_SMMU_V500;
		if (of_device_is_compatible(phandle, "qcom,adreno-smmu"))
			set_bit(KGSL_MMU_IOPGTABLE, &mmu->features);

		of_node_put(phandle);
	}

	of_node_put(node);
}

int kgsl_iommu_bind(struct kgsl_device *device, struct platform_device *pdev)
{
	u32 val[2];
	int ret, i;
	struct kgsl_iommu *iommu = KGSL_IOMMU(device);
	struct kgsl_mmu *mmu = &device->mmu;
	struct device_node *node = pdev->dev.of_node;
	struct kgsl_global_memdesc *md;

	/* Create a kmem cache for the pagetable address objects */
	if (!addr_entry_cache) {
		addr_entry_cache = KMEM_CACHE(kgsl_iommu_addr_entry, 0);
		if (!addr_entry_cache) {
			ret = -ENOMEM;
			goto err;
		}
	}

	ret = of_property_read_u32_array(node, "reg", val, 2);
	if (ret) {
		dev_err(&device->pdev->dev,
			"%pOF: Unable to read KGSL IOMMU register range\n",
			node);
		goto err;
	}

	iommu->regbase = devm_ioremap(&device->pdev->dev, val[0], val[1]);
	if (!iommu->regbase) {
		dev_err(&device->pdev->dev, "Couldn't map IOMMU registers\n");
		ret = -ENOMEM;
		goto err;
	}

	iommu->pdev = pdev;
	iommu->num_clks = 0;

	iommu->clks = devm_kcalloc(&pdev->dev, ARRAY_SIZE(kgsl_iommu_clocks),
				sizeof(*iommu->clks), GFP_KERNEL);
	if (!iommu->clks) {
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < ARRAY_SIZE(kgsl_iommu_clocks); i++) {
		struct clk *c;

		c = devm_clk_get(&device->pdev->dev, kgsl_iommu_clocks[i]);
		if (IS_ERR(c))
			continue;

		iommu->clks[iommu->num_clks].id = kgsl_iommu_clocks[i];
		iommu->clks[iommu->num_clks++].clk = c;
	}

	/* Get the CX regulator if it is available */
	iommu->cx_gdsc = devm_regulator_get(&pdev->dev, "vddcx");

	set_bit(KGSL_MMU_PAGED, &mmu->features);

	mmu->type = KGSL_MMU_TYPE_IOMMU;
	mmu->mmu_ops = &kgsl_iommu_ops;

	/* Peek at the phandle to set up configuration */
	kgsl_iommu_check_config(mmu, node);

	/* Probe the default pagetable */
	ret = iommu_probe_user_context(device, node);
	if (ret) {
		of_platform_depopulate(&pdev->dev);
		goto err;
	}

	/* Probe the secure pagetable (this is optional) */
	iommu_probe_secure_context(device, node);

	/* Map any globals that might have been created early */
	list_for_each_entry(md, &device->globals, node)
		kgsl_iommu_map_global(mmu, &md->memdesc, 0);

	/* QDSS is supported only when QCOM_KGSL_QDSS_STM is enabled */
	if (IS_ENABLED(CONFIG_QCOM_KGSL_QDSS_STM))
		device->qdss_desc = kgsl_allocate_global_fixed(device,
					"qcom,gpu-qdss-stm", "gpu-qdss");

	device->qtimer_desc = kgsl_allocate_global_fixed(device,
		"qcom,gpu-timer", "gpu-qtimer");

	/*
	 * Only support VBOs on MMU500 hardware that supports the PRR
	 * marker register to ignore writes to the zero page
	 */
	if (mmu->subtype == KGSL_IOMMU_SMMU_V500) {
		/*
		 * We need to allocate a page because we need a known physical
		 * address to program in the PRR register but the hardware
		 * should intercept accesses to the page before they go to DDR
		 * so this should be mostly just a placeholder
		 */
		kgsl_vbo_zero_page = alloc_page(GFP_KERNEL | __GFP_ZERO |
			__GFP_NORETRY | __GFP_HIGHMEM);
		if (kgsl_vbo_zero_page)
			set_bit(KGSL_MMU_SUPPORT_VBO, &mmu->features);
	}

	return 0;

err:
	kmem_cache_destroy(addr_entry_cache);
	addr_entry_cache = NULL;

	return ret;
}

static const struct kgsl_mmu_ops kgsl_iommu_ops = {
	.mmu_close = kgsl_iommu_close,
	.mmu_start = kgsl_iommu_start,
	.mmu_clear_fsr = kgsl_iommu_clear_fsr,
	.mmu_get_current_ttbr0 = kgsl_iommu_get_current_ttbr0,
	.mmu_enable_clk = kgsl_iommu_enable_clk,
	.mmu_disable_clk = kgsl_iommu_disable_clk,
	.mmu_set_pf_policy = kgsl_iommu_set_pf_policy,
	.mmu_pagefault_resume = kgsl_iommu_pagefault_resume,
	.mmu_getpagetable = kgsl_iommu_getpagetable,
	.mmu_map_global = kgsl_iommu_map_global,
	.mmu_flush_tlb = kgsl_iommu_flush_tlb,
};

static const struct kgsl_mmu_pt_ops iopgtbl_pt_ops = {
	.mmu_map = kgsl_iopgtbl_map,
	.mmu_map_child = kgsl_iopgtbl_map_child,
	.mmu_map_zero_page_to_range = kgsl_iopgtbl_map_zero_page_to_range,
	.mmu_unmap = kgsl_iopgtbl_unmap,
	.mmu_unmap_range = kgsl_iopgtbl_unmap_range,
	.mmu_destroy_pagetable = kgsl_iommu_destroy_pagetable,
	.get_ttbr0 = kgsl_iommu_get_ttbr0,
	.get_context_bank = kgsl_iommu_get_context_bank,
	.get_asid = kgsl_iommu_get_asid,
	.get_gpuaddr = kgsl_iommu_get_gpuaddr,
	.put_gpuaddr = kgsl_iommu_put_gpuaddr,
	.set_svm_region = kgsl_iommu_set_svm_region,
	.find_svm_region = kgsl_iommu_find_svm_region,
	.svm_range = kgsl_iommu_svm_range,
	.addr_in_range = kgsl_iommu_addr_in_range,
};

static const struct kgsl_mmu_pt_ops secure_pt_ops = {
	.mmu_map = kgsl_iommu_secure_map,
	.mmu_unmap = kgsl_iommu_secure_unmap,
	.mmu_destroy_pagetable = kgsl_iommu_destroy_pagetable,
	.get_context_bank = kgsl_iommu_get_context_bank,
	.get_asid = kgsl_iommu_get_asid,
	.get_gpuaddr = kgsl_iommu_get_gpuaddr,
	.put_gpuaddr = kgsl_iommu_put_gpuaddr,
	.addr_in_range = kgsl_iommu_addr_in_range,
};

static const struct kgsl_mmu_pt_ops default_pt_ops = {
	.mmu_map = kgsl_iommu_default_map,
	.mmu_unmap = kgsl_iommu_default_unmap,
	.mmu_destroy_pagetable = kgsl_iommu_destroy_default_pagetable,
	.get_ttbr0 = kgsl_iommu_get_ttbr0,
	.get_context_bank = kgsl_iommu_get_context_bank,
	.get_asid = kgsl_iommu_get_asid,
	.get_gpuaddr = kgsl_iommu_get_gpuaddr,
	.put_gpuaddr = kgsl_iommu_put_gpuaddr,
	.addr_in_range = kgsl_iommu_addr_in_range,
};
