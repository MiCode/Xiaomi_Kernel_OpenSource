/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/errno.h>
#include <linux/iommu.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include <asm/cacheflush.h>

#include "msm_iommu_priv.h"
#include <trace/events/kmem.h>
#include "msm_iommu_pagetable.h"

#define NUM_FL_PTE      4   /* First level */
#define NUM_SL_PTE      512 /* Second level */
#define NUM_TL_PTE      512 /* Third level */

#define PTE_SIZE	8

#define FL_ALIGN	0x20

/* First-level/second-level page table bits */
#define FL_OFFSET(va)           (((va) & 0xC0000000) >> 30)

#define FLSL_BASE_MASK            (0xFFFFFFF000ULL)
#define FLSL_1G_BLOCK_MASK        (0xFFC0000000ULL)
#define FLSL_BLOCK_MASK           (0xFFFFE00000ULL)
#define FLSL_TYPE_BLOCK           (1 << 0)
#define FLSL_TYPE_TABLE           (3 << 0)
#define FLSL_PTE_TYPE_MASK        (3 << 0)
#define FLSL_APTABLE_RO           (2 << 61)
#define FLSL_APTABLE_RW           (0 << 61)

#define FL_TYPE_SECT              (2 << 0)
#define FL_SUPERSECTION           (1 << 18)
#define FL_AP0                    (1 << 10)
#define FL_AP1                    (1 << 11)
#define FL_AP2                    (1 << 15)
#define FL_SHARED                 (1 << 16)
#define FL_BUFFERABLE             (1 << 2)
#define FL_CACHEABLE              (1 << 3)
#define FL_TEX0                   (1 << 12)
#define FL_NG                     (1 << 17)

/* Second-level page table bits */
#define SL_OFFSET(va)             (((va) & 0x3FE00000) >> 21)

/* Third-level page table bits */
#define TL_OFFSET(va)             (((va) & 0x1FF000) >> 12)

#define TL_TYPE_PAGE              (3 << 0)
#define TL_PAGE_MASK              (0xFFFFFFF000ULL)
#define TL_ATTR_INDEX_MASK        (0x7)
#define TL_ATTR_INDEX_SHIFT       (0x2)
#define TL_NS                     (0x1 << 5)
#define TL_AP_RO                  (0x3 << 6) /* Access Permission: R */
#define TL_AP_RW                  (0x1 << 6) /* Access Permission: RW */
#define TL_AP_PR_RW               (0x0 << 6) /* Privileged Mode RW */
#define TL_AP_PR_RO               (0x2 << 6) /* Privileged Mode R */
#define TL_SH_ISH                 (0x3 << 8) /* Inner shareable */
#define TL_SH_OSH                 (0x2 << 8) /* Outer shareable */
#define TL_SH_NSH                 (0x0 << 8) /* Non-shareable */
#define TL_AF                     (0x1 << 10)  /* Access Flag */
#define TL_NG                     (0x1 << 11) /* Non-Global */
#define TL_CH                     (0x1ULL << 52) /* Contiguous hint */
#define TL_PXN                    (0x1ULL << 53) /* Privilege Execute Never */
#define TL_XN                     (0x1ULL << 54) /* Execute Never */

/* normal non-cacheable */
#define PTE_MT_BUFFERABLE         (1 << 2)
/* normal inner write-alloc */
#define PTE_MT_WRITEALLOC         (7 << 2)

#define PTE_MT_MASK               (7 << 2)

#define FOLLOW_TO_NEXT_TABLE(pte) ((u64 *) __va(((*pte) & FLSL_BASE_MASK)))

static void __msm_iommu_pagetable_unmap_range(struct msm_iommu_pt *pt,
				unsigned long va, size_t len, u32 silent);

static inline void clean_pte(u64 *start, u64 *end,
				s32 redirect)
{
	if (!redirect)
		dmac_flush_range(start, end);
}

s32 msm_iommu_pagetable_alloc(struct msm_iommu_pt *pt)
{
	u32 size = PTE_SIZE * NUM_FL_PTE + FL_ALIGN;
	phys_addr_t fl_table_phys;

	pt->unaligned_fl_table = kzalloc(size, GFP_KERNEL);
	if (!pt->unaligned_fl_table)
		return -ENOMEM;


	fl_table_phys = virt_to_phys(pt->unaligned_fl_table);
	fl_table_phys = ALIGN(fl_table_phys, FL_ALIGN);
	pt->fl_table = phys_to_virt(fl_table_phys);

	pt->sl_table_shadow = kzalloc(sizeof(u64 *) * NUM_FL_PTE, GFP_KERNEL);
	if (!pt->sl_table_shadow) {
		kfree(pt->unaligned_fl_table);
		return -ENOMEM;
	}
	clean_pte(pt->fl_table, pt->fl_table + NUM_FL_PTE, pt->redirect);
	return 0;
}

void msm_iommu_pagetable_free(struct msm_iommu_pt *pt)
{
	s32 i;
	u64 *fl_table = pt->fl_table;

	for (i = 0; i < NUM_FL_PTE; ++i) {
		if ((fl_table[i] & FLSL_TYPE_TABLE) == FLSL_TYPE_TABLE) {
			u64 p = fl_table[i] & FLSL_BASE_MASK;
			free_page((u32)phys_to_virt(p));
		}
		if ((pt->sl_table_shadow[i]))
			free_page((u32)pt->sl_table_shadow[i]);
	}
	kfree(pt->unaligned_fl_table);

	pt->unaligned_fl_table = 0;
	pt->fl_table = 0;

	kfree(pt->sl_table_shadow);
}

void msm_iommu_pagetable_free_tables(struct msm_iommu_pt *pt, unsigned long va,
				 size_t len)
{
	/*
	 * Adding 2 for worst case. We could be spanning 3 second level pages
	 * if we unmapped just over 2MB.
	 */
	u32 n_entries = len / SZ_2M + 2;
	u32 fl_offset = FL_OFFSET(va);
	u32 sl_offset = SL_OFFSET(va);
	u32 i;

	for (i = 0; i < n_entries && fl_offset < NUM_FL_PTE; ++i) {
		void *tl_table_va;
		u32 entry;
		u64 *sl_pte_shadow;

		sl_pte_shadow = pt->sl_table_shadow[fl_offset];
		if (!sl_pte_shadow)
			break;
		sl_pte_shadow += sl_offset;
		entry = *sl_pte_shadow;
		tl_table_va = __va(((*sl_pte_shadow) & ~0xFFF));

		if (entry && !(entry & 0xFFF)) {
			free_page((unsigned long)tl_table_va);
			*sl_pte_shadow = 0;
		}
		++sl_offset;
		if (sl_offset >= NUM_TL_PTE) {
			sl_offset = 0;
			++fl_offset;
		}
	}
}


#ifdef CONFIG_ARM_LPAE
/*
 * If LPAE is enabled in the ARM processor then just use the same
 * cache policy as the kernel for the SMMU cached mappings.
 */
static inline u32 __get_cache_attr(void)
{
	return pgprot_kernel & PTE_MT_MASK;
}
#else
/*
 * If LPAE is NOT enabled in the ARM processor then hard code the policy.
 * This is mostly for debugging so that we can enable SMMU LPAE without
 * ARM CPU LPAE.
 */
static inline u32 __get_cache_attr(void)
{
	return PTE_MT_WRITEALLOC;
}

#endif

/*
 * Get the IOMMU attributes for the ARM LPAE long descriptor format page
 * table entry bits. The only upper attribute bits we currently use is the
 * contiguous bit which is set when we actually have a contiguous mapping.
 * Lower attribute bits specify memory attributes and the protection
 * (Read/Write/Execute).
 */
static inline void __get_attr(s32 prot, u64 *upper_attr, u64 *lower_attr)
{
	u32 attr_idx = PTE_MT_BUFFERABLE;

	*upper_attr = 0;
	*lower_attr = 0;

	if (!(prot & (IOMMU_READ | IOMMU_WRITE))) {
		prot |= IOMMU_READ | IOMMU_WRITE;
		WARN_ONCE(1, "No attributes in iommu mapping; assuming RW\n");
	}

	if ((prot & IOMMU_WRITE) && !(prot & IOMMU_READ)) {
		prot |= IOMMU_READ;
		WARN_ONCE(1, "Write-only unsupported; falling back to RW\n");
	}

	if (prot & IOMMU_CACHE)
		attr_idx = __get_cache_attr();

	*lower_attr |= attr_idx;
	*lower_attr |= TL_NG | TL_AF;
	*lower_attr |= (prot & IOMMU_CACHE) ? TL_SH_ISH : TL_SH_NSH;
	if (prot & IOMMU_PRIV)
		*lower_attr |= (prot & IOMMU_WRITE) ? TL_AP_PR_RW : TL_AP_PR_RO;
	else
		*lower_attr |= (prot & IOMMU_WRITE) ? TL_AP_RW : TL_AP_RO;
}

static inline u64 *make_second_level_tbl(struct msm_iommu_pt *pt, u32 offset)
{
	u64 *sl = (u64 *) __get_free_page(GFP_KERNEL);
	u64 *fl_pte = pt->fl_table + offset;

	if (!sl) {
		pr_err("Could not allocate second level table\n");
		goto fail;
	}

	pt->sl_table_shadow[offset] = (u64 *) __get_free_page(GFP_KERNEL);
	if (!pt->sl_table_shadow[offset]) {
		free_page((u32) sl);
		pr_err("Could not allocate second level shadow table\n");
		goto fail;
	}

	memset(sl, 0, SZ_4K);
	memset(pt->sl_table_shadow[offset], 0, SZ_4K);
	clean_pte(sl, sl + NUM_SL_PTE, pt->redirect);

	/* Leave APTable bits 0 to let next level decide access permissinons */
	*fl_pte = (((phys_addr_t)__pa(sl)) & FLSL_BASE_MASK) | FLSL_TYPE_TABLE;
	clean_pte(fl_pte, fl_pte + 1, pt->redirect);
fail:
	return sl;
}

static inline u64 *make_third_level_tbl(s32 redirect, u64 *sl_pte,
					u64 *sl_pte_shadow)
{
	u64 *tl = (u64 *) __get_free_page(GFP_KERNEL);

	if (!tl) {
		pr_err("Could not allocate third level table\n");
		goto fail;
	}
	memset(tl, 0, SZ_4K);
	clean_pte(tl, tl + NUM_TL_PTE, redirect);

	/* Leave APTable bits 0 to let next level decide access permissions */
	*sl_pte = (((phys_addr_t)__pa(tl)) & FLSL_BASE_MASK) | FLSL_TYPE_TABLE;
	*sl_pte_shadow = *sl_pte & ~0xFFF;
	clean_pte(sl_pte, sl_pte + 1, redirect);
fail:
	return tl;
}

static inline s32 tl_4k_map(u64 *tl_pte, phys_addr_t pa,
			    u64 upper_attr, u64 lower_attr, s32 redirect)
{
	s32 ret = 0;

	if (*tl_pte) {
		ret = -EBUSY;
		goto fail;
	}

	*tl_pte = upper_attr | (pa & TL_PAGE_MASK) | lower_attr | TL_TYPE_PAGE;
	clean_pte(tl_pte, tl_pte + 1, redirect);
fail:
	return ret;
}

static inline s32 tl_64k_map(u64 *tl_pte, phys_addr_t pa,
			     u64 upper_attr, u64 lower_attr, s32 redirect)
{
	s32 ret = 0;
	s32 i;

	for (i = 0; i < 16; ++i)
		if (*(tl_pte+i)) {
			ret = -EBUSY;
			goto fail;
		}

	/* Add Contiguous hint TL_CH */
	upper_attr |= TL_CH;

	for (i = 0; i < 16; ++i)
		*(tl_pte+i) = upper_attr | (pa & TL_PAGE_MASK) |
			      lower_attr | TL_TYPE_PAGE;
	clean_pte(tl_pte, tl_pte + 16, redirect);
fail:
	return ret;
}

static inline s32 sl_2m_map(u64 *sl_pte, phys_addr_t pa,
			    u64 upper_attr, u64 lower_attr, s32 redirect)
{
	s32 ret = 0;

	if (*sl_pte) {
		ret = -EBUSY;
		goto fail;
	}

	*sl_pte = upper_attr | (pa & FLSL_BLOCK_MASK) |
		  lower_attr | FLSL_TYPE_BLOCK;
	clean_pte(sl_pte, sl_pte + 1, redirect);
fail:
	return ret;
}

static inline s32 sl_32m_map(u64 *sl_pte, phys_addr_t pa,
			     u64 upper_attr, u64 lower_attr, s32 redirect)
{
	s32 i;
	s32 ret = 0;

	for (i = 0; i < 16; ++i) {
		if (*(sl_pte+i)) {
			ret = -EBUSY;
			goto fail;
		}
	}

	/* Add Contiguous hint TL_CH */
	upper_attr |= TL_CH;

	for (i = 0; i < 16; ++i)
		*(sl_pte+i) = upper_attr | (pa & FLSL_BLOCK_MASK) |
			      lower_attr | FLSL_TYPE_BLOCK;
	clean_pte(sl_pte, sl_pte + 16, redirect);
fail:
	return ret;
}

static inline s32 fl_1G_map(u64 *fl_pte, phys_addr_t pa,
			    u64 upper_attr, u64 lower_attr, s32 redirect)
{
	s32 ret = 0;

	if (*fl_pte) {
		ret = -EBUSY;
		goto fail;
	}

	*fl_pte = upper_attr | (pa & FLSL_1G_BLOCK_MASK) |
		  lower_attr | FLSL_TYPE_BLOCK;

	clean_pte(fl_pte, fl_pte + 1, redirect);
fail:
	return ret;
}

static inline s32 common_error_check(size_t len, u64 const *fl_table)
{
	s32 ret = 0;

	if (len != SZ_1G && len != SZ_32M && len != SZ_2M &&
	    len != SZ_64K && len != SZ_4K) {
		pr_err("Bad length: %d\n", len);
		ret = -EINVAL;
	} else if (!fl_table) {
		pr_err("Null page table\n");
		ret = -EINVAL;
	}
	return ret;
}

static inline s32 handle_1st_lvl(struct msm_iommu_pt *pt, u32 offset,
				 phys_addr_t pa, size_t len, u64 upper_attr,
				 u64 lower_attr)
{
	s32 ret = 0;
	u64 *fl_pte = pt->fl_table + offset;

	if (len == SZ_1G) {
		ret = fl_1G_map(fl_pte, pa, upper_attr, lower_attr,
				pt->redirect);
	} else {
		/* Need second level page table */
		if (*fl_pte == 0) {
			if (make_second_level_tbl(pt, offset) == NULL)
				ret = -ENOMEM;
		}
		if (!ret) {
			if ((*fl_pte & FLSL_TYPE_TABLE) != FLSL_TYPE_TABLE)
				ret = -EBUSY;
		}
	}
	return ret;
}

static inline s32 handle_3rd_lvl(u64 *sl_pte, u64 *sl_pte_shadow, u32 va,
				 phys_addr_t pa, u64 upper_attr,
				 u64 lower_attr, size_t len, s32 redirect)
{
	u64 *tl_table;
	u64 *tl_pte;
	u32 tl_offset;
	s32 ret = 0;
	u32 n_entries;

	/* Need a 3rd level table */
	if (*sl_pte == 0) {
		if (make_third_level_tbl(redirect, sl_pte, sl_pte_shadow)
					 == NULL) {
			ret = -ENOMEM;
			goto fail;
		}
	}

	if ((*sl_pte & FLSL_TYPE_TABLE) != FLSL_TYPE_TABLE) {
		ret = -EBUSY;
		goto fail;
	}

	tl_table = FOLLOW_TO_NEXT_TABLE(sl_pte);
	tl_offset = TL_OFFSET(va);
	tl_pte = tl_table + tl_offset;

	if (len == SZ_64K) {
		ret = tl_64k_map(tl_pte, pa, upper_attr, lower_attr, redirect);
		n_entries = 16;
	} else {
		ret = tl_4k_map(tl_pte, pa, upper_attr, lower_attr, redirect);
		n_entries = 1;
	}

	/* Increment map count */
	if (!ret)
		*sl_pte_shadow += n_entries;

fail:
	return ret;
}

int msm_iommu_pagetable_map(struct msm_iommu_pt *pt, unsigned long va,
			    phys_addr_t pa, size_t len, int prot)
{
	s32 ret;
	struct scatterlist sg;

	ret = common_error_check(len, pt->fl_table);
	if (ret)
		goto fail;

	sg_init_table(&sg, 1);
	sg_dma_address(&sg) = pa;
	sg.length = len;

	ret = msm_iommu_pagetable_map_range(pt, va, &sg, len, prot);

fail:
	return ret;
}

static void fl_1G_unmap(u64 *fl_pte, s32 redirect)
{
	*fl_pte = 0;
	clean_pte(fl_pte, fl_pte + 1, redirect);
}

size_t msm_iommu_pagetable_unmap(struct msm_iommu_pt *pt, unsigned long va,
				size_t len)
{
	msm_iommu_pagetable_unmap_range(pt, va, len);
	return len;
}

static phys_addr_t get_phys_addr(struct scatterlist *sg)
{
	/*
	 * Try sg_dma_address first so that we can
	 * map carveout regions that do not have a
	 * struct page associated with them.
	 */
	phys_addr_t pa = sg_dma_address(sg);
	if (pa == 0)
		pa = sg_phys(sg);
	return pa;
}

#ifdef CONFIG_IOMMU_FORCE_4K_MAPPINGS
static inline int is_fully_aligned(unsigned int va, phys_addr_t pa, size_t len,
				   int align)
{
	if (align == SZ_4K)
		return  IS_ALIGNED(va | pa, align) && (len >= align);
	else
		return 0;
}
#else
static inline int is_fully_aligned(unsigned int va, phys_addr_t pa, size_t len,
				   int align)
{
	return  IS_ALIGNED(va | pa, align) && (len >= align);
}
#endif

s32 msm_iommu_pagetable_map_range(struct msm_iommu_pt *pt, unsigned long va,
		       struct scatterlist *sg, size_t len, s32 prot)
{
	phys_addr_t pa;
	u32 offset = 0;
	u64 *fl_pte;
	u64 *sl_pte;
	u64 *sl_pte_shadow;
	u32 fl_offset;
	u32 sl_offset;
	u64 *sl_table = NULL;
	u32 chunk_size, chunk_offset = 0;
	s32 ret = 0;
	u64 up_at;
	u64 lo_at;
	u32 redirect = pt->redirect;
	unsigned int start_va = va;

	BUG_ON(len & (SZ_4K - 1));

	if (!pt->fl_table) {
		pr_err("Null page table\n");
		ret = -EINVAL;
		goto fail;
	}

	__get_attr(prot, &up_at, &lo_at);

	pa = get_phys_addr(sg);

	while (offset < len) {
		u32 chunk_left = sg->length - chunk_offset;

		fl_offset = FL_OFFSET(va);
		fl_pte = pt->fl_table + fl_offset;

		chunk_size = SZ_4K;
		if (is_fully_aligned(va, pa, chunk_left, SZ_1G))
			chunk_size = SZ_1G;
		else if (is_fully_aligned(va, pa, chunk_left, SZ_32M))
			chunk_size = SZ_32M;
		else if (is_fully_aligned(va, pa, chunk_left, SZ_2M))
			chunk_size = SZ_2M;
		else if (is_fully_aligned(va, pa, chunk_left, SZ_64K))
			chunk_size = SZ_64K;

		trace_iommu_map_range(va, pa, sg->length, chunk_size);

		ret = handle_1st_lvl(pt, fl_offset, pa, chunk_size,
				     up_at, lo_at);
		if (ret)
			goto fail;

		sl_table = FOLLOW_TO_NEXT_TABLE(fl_pte);
		sl_offset = SL_OFFSET(va);
		sl_pte = sl_table + sl_offset;
		sl_pte_shadow = pt->sl_table_shadow[fl_offset] + sl_offset;

		if (chunk_size == SZ_32M)
			ret = sl_32m_map(sl_pte, pa, up_at, lo_at, redirect);
		else if (chunk_size == SZ_2M)
			ret = sl_2m_map(sl_pte, pa, up_at, lo_at, redirect);
		else if (chunk_size == SZ_64K || chunk_size == SZ_4K)
			ret = handle_3rd_lvl(sl_pte, sl_pte_shadow, va, pa,
					     up_at, lo_at, chunk_size,
					     redirect);
		if (ret)
			goto fail;

		offset += chunk_size;
		chunk_offset += chunk_size;
		va += chunk_size;
		pa += chunk_size;

		if (chunk_offset >= sg->length && offset < len) {
			chunk_offset = 0;
			sg = sg_next(sg);
			pa = get_phys_addr(sg);
		}
	}
fail:
	if (ret && offset > 0)
		__msm_iommu_pagetable_unmap_range(pt, start_va, offset, 1);
	return ret;
}

void msm_iommu_pagetable_unmap_range(struct msm_iommu_pt *pt, unsigned long va,
				size_t len)
{
	__msm_iommu_pagetable_unmap_range(pt, va, len, 0);
}

static void __msm_iommu_pagetable_unmap_range(struct msm_iommu_pt *pt,
						unsigned long va,
						size_t len, u32 silent)
{
	u32 offset = 0;
	u64 *fl_pte;
	u64 *sl_pte;
	u64 *tl_pte;
	u32 fl_offset;
	u32 sl_offset;
	u64 *sl_table;
	u64 *tl_table;
	u32 tl_start, tl_end;
	u32 redirect = pt->redirect;

	BUG_ON(len & (SZ_4K - 1));

	while (offset < len) {
		u32 entries;
		u32 left_to_unmap = len - offset;
		u32 type;

		fl_offset = FL_OFFSET(va);
		fl_pte = pt->fl_table + fl_offset;

		if (*fl_pte == 0) {
			if (!silent)
				pr_err("First level PTE is 0 at index 0x%x (offset: 0x%x)\n",
					fl_offset, offset);
			return;
		}
		type = *fl_pte & FLSL_PTE_TYPE_MASK;

		if (type == FLSL_TYPE_BLOCK) {
			fl_1G_unmap(fl_pte, redirect);
			va += SZ_1G;
			offset += SZ_1G;
		} else if (type == FLSL_TYPE_TABLE) {
			sl_table = FOLLOW_TO_NEXT_TABLE(fl_pte);
			sl_offset = SL_OFFSET(va);
			sl_pte = sl_table + sl_offset;
			type = *sl_pte & FLSL_PTE_TYPE_MASK;

			if (type == FLSL_TYPE_BLOCK) {
				*sl_pte = 0;

				clean_pte(sl_pte, sl_pte + 1, redirect);

				offset += SZ_2M;
				va += SZ_2M;
			} else if (type == FLSL_TYPE_TABLE) {
				u64 *sl_pte_shadow =
				    pt->sl_table_shadow[fl_offset] + sl_offset;

				tl_start = TL_OFFSET(va);
				tl_table =  FOLLOW_TO_NEXT_TABLE(sl_pte);
				tl_end = (left_to_unmap / SZ_4K) + tl_start;

				if (tl_end > NUM_TL_PTE)
					tl_end = NUM_TL_PTE;

				entries = tl_end - tl_start;

				memset(tl_table + tl_start, 0,
				       entries * sizeof(*tl_pte));

				clean_pte(tl_table + tl_start,
					  tl_table + tl_end, redirect);

				BUG_ON((*sl_pte_shadow & 0xFFF) < entries);

				/* Decrement map count */
				*sl_pte_shadow -= entries;

				if (!(*sl_pte_shadow & 0xFFF)) {
					*sl_pte = 0;
					clean_pte(sl_pte, sl_pte + 1,
						  pt->redirect);
				}

				offset += entries * SZ_4K;
				va += entries * SZ_4K;
			} else {
				if (!silent)
					pr_err("Second level PTE (0x%llx) is invalid at index 0x%x (offset: 0x%x)\n",
						*sl_pte, sl_offset, offset);
			}
		} else {
			if (!silent)
				pr_err("First level PTE (0x%llx) is invalid at index 0x%x (offset: 0x%x)\n",
					*fl_pte, fl_offset, offset);
		}
	}
}

void msm_iommu_flush_pagetable(struct msm_iommu_pt *pt, unsigned long va,
				size_t len)
{
	/* Consolidated flush of page tables has not been implemented for
	 * LPAE driver as of now.
	 */
}

phys_addr_t msm_iommu_iova_to_phys_soft(struct iommu_domain *domain,
							dma_addr_t va)
{
	pr_err("iova_to_phys is not implemented for LPAE\n");
	return 0;
}

void __init msm_iommu_pagetable_init(void)
{
}
