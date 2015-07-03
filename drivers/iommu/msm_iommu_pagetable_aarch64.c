/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#define NUM_PT_LEVEL	4
#define NUM_PTE		512 /* generic for all levels */
#define NUM_FL_PTE      512 /* First level */
#define NUM_SL_PTE      512 /* Second level */
#define NUM_TL_PTE      512 /* Third level */
#define NUM_LL_PTE      512 /* Fourth (Last) level */

#define PTE_SIZE	8

#define FL_ALIGN	SZ_4K

/* First-level/second-level page table bits */
#define FL_SHIFT		39
#define FL_OFFSET(va)		(((va) & 0xFF8000000000ULL) >> FL_SHIFT)

/* Second-level page table bits */
#define SL_SHIFT		30
#define SL_OFFSET(va)		(((va) & 0x7FC0000000ULL) >> SL_SHIFT)

/* Third-level page table bits */
#define TL_SHIFT		21
#define TL_OFFSET(va)		(((va) & 0x3FE00000ULL) >> TL_SHIFT)

/* Fourth-level (Last level) page table bits */
#define LL_SHIFT		12
#define LL_OFFSET(va)		(((va) & 0x1FF000ULL) >> LL_SHIFT)


#define FLSL_BASE_MASK		(0xFFFFFFFFF000ULL)
#define FLSL_1G_BLOCK_MASK	(0xFFFFC0000000ULL)
#define FLSL_BLOCK_MASK		(0xFFFFE00000ULL)
#define FLSL_TYPE_BLOCK		(1 << 0)
#define FLSL_TYPE_TABLE		(3 << 0)
#define FLSL_PTE_TYPE_MASK	(3 << 0)
#define FLSL_APTABLE_RO		(2 << 61)
#define FLSL_APTABLE_RW		(0 << 61)

#define FL_TYPE_SECT		(2 << 0)
#define FL_SUPERSECTION		(1 << 18)
#define FL_AP0			(1 << 10)
#define FL_AP1			(1 << 11)
#define FL_AP2			(1 << 15)
#define FL_SHARED		(1 << 16)
#define FL_BUFFERABLE		(1 << 2)
#define FL_CACHEABLE		(1 << 3)
#define FL_TEX0			(1 << 12)
#define FL_NG			(1 << 17)

#define LL_TYPE_PAGE		(3 << 0)
#define LL_PAGE_MASK		(0xFFFFFFFFF000ULL)
#define LL_ATTR_INDEX_MASK	(0x7)
#define LL_ATTR_INDEX_SHIFT	(0x2)
#define LL_NS			(0x1 << 5)
#define LL_AP_RO		(0x3 << 6) /* Access Permission: R */
#define LL_AP_RW		(0x1 << 6) /* Access Permission: RW */
#define LL_AP_PR_RW		(0x0 << 6) /* Privileged Mode RW */
#define LL_AP_PR_RO		(0x2 << 6) /* Privileged Mode R */
#define LL_SH_ISH		(0x3 << 8) /* Inner shareable */
#define LL_SH_OSH		(0x2 << 8) /* Outer shareable */
#define LL_SH_NSH		(0x0 << 8) /* Non-shareable */
#define LL_AF			(0x1 << 10)  /* Access Flag */
#define LL_NG			(0x1 << 11) /* Non-Global */
#define LL_CH			(0x1ULL << 52) /* Contiguous hint */
#define LL_PXN			(0x1ULL << 53) /* Privilege Execute Never */
#define LL_XN			(0x1ULL << 54) /* Execute Never */

/* normal non-cacheable */
#define PTE_MT_BUFFERABLE		(1 << 2)
/* normal inner write-alloc */
#define PTE_MT_WRITEALLOC		(7 << 2)

#define PTE_MT_MASK		(7 << 2)

#define FOLLOW_TO_NEXT_TABLE(pte) ((u64 *) __va(((*pte) & FLSL_BASE_MASK)))
#define SUB_LEVEL_MAPPING_NOT_REQUIRED	1

static void __msm_iommu_pagetable_unmap_range(struct msm_iommu_pt *pt,
						unsigned long va, size_t len,
						u32 silent);

s32 msm_iommu_pagetable_alloc(struct msm_iommu_pt *pt)
{
	pt->fl_table = (u64 *) get_zeroed_page(GFP_ATOMIC);
	if (!pt->fl_table)
		return -ENOMEM;

	return 0;
}

/*
 * Everything in this page table (and sub-level tables) will be
 * cleared and freed
 */
static void free_pagetable_level(u64 phys, int level, int loop)
{
	u64 *table = phys_to_virt(phys);
	int i;

	if (level > NUM_PT_LEVEL)
		return;
	else if (level == NUM_PT_LEVEL || !loop)
		goto free_this_level;

	for (i = 0; i < NUM_FL_PTE; ++i) {
		if ((table[i] & FLSL_TYPE_TABLE) == FLSL_TYPE_TABLE) {
			u64 p = table[i] & FLSL_BASE_MASK;

			if (p)
				free_pagetable_level(p, level + 1, 1);
		}
	}

free_this_level:
	free_page((unsigned long)table);
}

/*
 * Free the page tables at all the level irrespective of whether
 * mapping exists or not. This is to be called at domain_destroy
 */
void msm_iommu_pagetable_free(struct msm_iommu_pt *pt)
{
	u64 *fl_table = pt->fl_table;

	free_pagetable_level(virt_to_phys(fl_table), 1, 1);
	pt->fl_table = 0;
}

static bool is_table_empty(u64 *table)
{
	int i;

	for (i = 0; i < NUM_FL_PTE; i++)
		if (table[i] != 0)
			return false;

	return true;
}

void msm_iommu_pagetable_free_tables(struct msm_iommu_pt *pt, unsigned long va,
				 size_t len)
{
	/*
	 * We free the page tables at the time of msm_iommu_pagetable_free.
	 * So, we really don't need to do anything here.
	 */
}

static inline u32 __get_cache_attr(void)
{
	return PTE_MT_WRITEALLOC;
}

/*
 * Get the IOMMU attributes for the ARM AARCH64 long descriptor format page
 * table entry bits. The only upper attribute bits we currently use is the
 * contiguous bit which is set when we actually have a contiguous mapping.
 * Lower attribute bits specify memory attributes and the protection
 * (Read/Write/Execute).
 */
static void __get_attr(int prot, u64 *upper_attr, u64 *lower_attr)
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
	*lower_attr |= LL_NG | LL_AF;
	*lower_attr |= (prot & IOMMU_CACHE) ? LL_SH_ISH : LL_SH_NSH;
	if (prot & IOMMU_PRIV)
		*lower_attr |= (prot & IOMMU_WRITE) ? LL_AP_PR_RW : LL_AP_PR_RO;
	else
		*lower_attr |= (prot & IOMMU_WRITE) ? LL_AP_RW : LL_AP_RO;
}

static u64 *make_next_level_table(s32 redirect, u64 *pte)
{
	u64 *next_level_table = (u64 *)get_zeroed_page(GFP_ATOMIC);

	if (!next_level_table) {
		pr_err("Could not allocate next level table\n");
		goto fail;
	}

	/* Leave APTable bits 0 to let next level decide access permissions */
	*pte = (((phys_addr_t)__pa(next_level_table)) &
			FLSL_BASE_MASK) | FLSL_TYPE_TABLE;
fail:
	return next_level_table;
}

static inline s32 ll_4k_map(u64 *ll_pte, phys_addr_t pa,
			    u64 upper_attr, u64 lower_attr, s32 redirect)
{
	s32 ret = 0;

	if (*ll_pte) {
		ret = -EBUSY;
		pr_err("%s: Busy ll_pte %p -> %lx\n",
			__func__, ll_pte, (unsigned long) *ll_pte);
		goto fail;
	}

	*ll_pte = upper_attr | (pa & LL_PAGE_MASK) | lower_attr | LL_TYPE_PAGE;
fail:
	return ret;
}

static inline s32 ll_64k_map(u64 *ll_pte, phys_addr_t pa,
			     u64 upper_attr, u64 lower_attr, s32 redirect)
{
	s32 ret = 0;
	s32 i;

	for (i = 0; i < 16; ++i) {
		if (*(ll_pte+i)) {
			ret = -EBUSY;
			pr_err("%s: Busy ll_pte %p -> %lx\n",
				__func__, ll_pte, (unsigned long) *ll_pte);
			goto fail;
		}
	}

	/* Add Contiguous hint LL_CH */
	upper_attr |= LL_CH;

	for (i = 0; i < 16; ++i)
		*(ll_pte+i) = upper_attr | (pa & LL_PAGE_MASK) |
			      lower_attr | LL_TYPE_PAGE;
fail:
	return ret;
}

static inline s32 tl_2m_map(u64 *tl_pte, phys_addr_t pa,
			    u64 upper_attr, u64 lower_attr, s32 redirect)
{
	s32 ret = 0;

	if (*tl_pte) {
		ret = -EBUSY;
		pr_err("%s: Busy tl_pte %p -> %lx\n",
			__func__, tl_pte, (unsigned long) *tl_pte);
		goto fail;
	}

	*tl_pte = upper_attr | (pa & FLSL_BLOCK_MASK) |
		  lower_attr | FLSL_TYPE_BLOCK;
fail:
	return ret;
}

static inline s32 tl_32m_map(u64 *tl_pte, phys_addr_t pa,
			     u64 upper_attr, u64 lower_attr, s32 redirect)
{
	s32 i;
	s32 ret = 0;

	for (i = 0; i < 16; ++i) {
		if (*(tl_pte+i)) {
			ret = -EBUSY;
			pr_err("%s: Busy tl_pte %p -> %lx\n",
				__func__, tl_pte, (unsigned long) *tl_pte);
			goto fail;
		}
	}

	/* Add Contiguous hint TL_CH */
	upper_attr |= LL_CH;

	for (i = 0; i < 16; ++i)
		*(tl_pte+i) = upper_attr | (pa & FLSL_BLOCK_MASK) |
			      lower_attr | FLSL_TYPE_BLOCK;
fail:
	return ret;
}

static inline s32 sl_1G_map(u64 *sl_pte, phys_addr_t pa,
			    u64 upper_attr, u64 lower_attr, s32 redirect)
{
	s32 ret = 0;

	if (*sl_pte) {
		ret = -EBUSY;
		pr_err("%s: Busy sl_pte %p -> %lx\n",
			__func__, sl_pte, (unsigned long) *sl_pte);
		goto fail;
	}

	*sl_pte = upper_attr | (pa & FLSL_1G_BLOCK_MASK) |
		  lower_attr | FLSL_TYPE_BLOCK;

fail:
	return ret;
}

static inline s32 handle_1st_lvl(struct msm_iommu_pt *pt, u64 *fl_pte,
				 phys_addr_t pa, size_t len, u64 upper_attr,
				 u64 lower_attr)
{
	s32 ret = 0;

	/* Need a 2nd level page table */
	if (*fl_pte == 0)
		if (!make_next_level_table(pt->redirect, fl_pte))
			ret = -ENOMEM;

	if (!ret)
		if ((*fl_pte & FLSL_TYPE_TABLE) != FLSL_TYPE_TABLE)
			ret = -EBUSY;

	return ret;
}

static inline s32 handle_2nd_lvl(struct msm_iommu_pt *pt, u64 *sl_pte,
				phys_addr_t pa, size_t chunk_size,
				u64 upper_attr, u64 lower_attr)
{
	s32 ret = 0;

	if (chunk_size == SZ_1G) {
		ret = sl_1G_map(sl_pte, pa, upper_attr, lower_attr,
				pt->redirect);

		if (!ret)
			return SUB_LEVEL_MAPPING_NOT_REQUIRED;
	}

	/* Need a 3rd level page table */
	if (*sl_pte == 0)
		if (!make_next_level_table(pt->redirect, sl_pte))
			ret = -ENOMEM;

	if (!ret)
		if ((*sl_pte & FLSL_TYPE_TABLE) != FLSL_TYPE_TABLE)
			ret = -EBUSY;

	return ret;
}

static inline s32 handle_3rd_lvl(struct msm_iommu_pt *pt, u64 *tl_pte,
				phys_addr_t pa, size_t chunk_size,
				u64 upper_attr, u64 lower_attr)
{
	s32 ret = 0;

	if (chunk_size == SZ_32M) {
		ret = tl_32m_map(tl_pte, pa, upper_attr, lower_attr,
				pt->redirect);
		if (!ret)
			return SUB_LEVEL_MAPPING_NOT_REQUIRED;
	} else if (chunk_size == SZ_2M) {
		ret = tl_2m_map(tl_pte, pa, upper_attr, lower_attr,
				pt->redirect);
		if (!ret)
			return SUB_LEVEL_MAPPING_NOT_REQUIRED;
	}

	/* Need a 4th level page table */
	if (*tl_pte == 0)
		if (!make_next_level_table(pt->redirect, tl_pte))
			ret = -ENOMEM;

	if (!ret)
		if ((*tl_pte & FLSL_TYPE_TABLE) != FLSL_TYPE_TABLE)
			ret = -EBUSY;

	return ret;
}

static inline s32 handle_4th_lvl(struct msm_iommu_pt *pt, u64 *ll_pte,
				phys_addr_t pa, size_t chunk_size,
				u64 upper_attr, u64 lower_attr)
{
	s32 ret = 0;

	if (chunk_size == SZ_64K)
		ret = ll_64k_map(ll_pte, pa, upper_attr, lower_attr,
				pt->redirect);
	else if (chunk_size == SZ_4K)
		ret = ll_4k_map(ll_pte, pa, upper_attr, lower_attr,
				pt->redirect);

	return ret;
}

static phys_addr_t __get_phys_sg(void *cookie)
{
	struct scatterlist *sg = cookie;
	struct page *page = sg_page(sg);

	BUG_ON(page == NULL);

	return sg_phys(sg);
}

static inline size_t __get_length_sg(void *cookie, unsigned int total)
{
	struct scatterlist *sg = cookie;

	return sg->length;
}

static inline int __get_next_sg(void *old, void **new)
{
	struct scatterlist *sg = old;
	*new = sg_next(sg);
	return 0;
}

static inline phys_addr_t __get_phys_bare(void *cookie)
{
	return (phys_addr_t)cookie;
}

static inline size_t __get_length_bare(void *cookie, unsigned int total)
{
	return total;
}

static inline int __get_next_bare(void *old, void **new)
{
	/* Put something here in hopes of catching errors... */
	*new = (void *)-1;
	return -EINVAL;
}

struct msm_iommu_map_ops {
	phys_addr_t (*get_phys)(void *cookie);
	size_t (*get_length)(void *cookie, unsigned int total);
	int (*get_next)(void *old, void **new);
};

static struct msm_iommu_map_ops regular_ops = {
	.get_phys =     __get_phys_bare,
	.get_length =   __get_length_bare,
	.get_next =     __get_next_bare,
};

static struct msm_iommu_map_ops sg_ops = {
	.get_phys =     __get_phys_sg,
	.get_length =   __get_length_sg,
	.get_next =     __get_next_sg,
};

#ifdef CONFIG_IOMMU_FORCE_4K_MAPPINGS
static inline int is_fully_aligned(unsigned int va, phys_addr_t pa, size_t len,
				   int align)
{
	if (align == SZ_4K)
		return  IS_ALIGNED(va | pa | len, align) && (len >= align);
	else
		return 0;
}
#else
static inline int is_fully_aligned(unsigned int va, phys_addr_t pa, size_t len,
				   int align)
{
	return  IS_ALIGNED(va | pa | len, align) && (len >= align);
}
#endif

static int __msm_iommu_pagetable_map_range(struct msm_iommu_pt *pt,
			unsigned long va, void *cookie,
			struct msm_iommu_map_ops *ops,
			size_t len, int prot)
{
	phys_addr_t pa;
	u64 offset = 0;
	u64 *fl_pte;
	u64 *sl_pte;
	u64 *tl_pte;
	u64 *ll_pte;
	u32 fl_offset;
	u32 sl_offset;
	u32 tl_offset;
	u32 ll_offset;
	u64 *sl_table = NULL;
	u64 *tl_table = NULL;
	u64 *ll_table = NULL;
	u64 chunk_size, chunk_offset = 0;
	s32 ret = 0;
	u64 up_at;
	u64 lo_at;
	unsigned long va_to_map = va;

	BUG_ON(len & (SZ_4K - 1));

	if (!pt->fl_table) {
		pr_err("Null page table\n");
		ret = -EINVAL;
		goto fail;
	}

	__get_attr(prot, &up_at, &lo_at);

	pa = ops->get_phys(cookie);

	while (offset < len) {
		u64 chunk_left = ops->get_length(cookie, len) - chunk_offset;

		chunk_size = SZ_4K;
		if (is_fully_aligned(va_to_map, pa, chunk_left, SZ_1G))
			chunk_size = SZ_1G;
		else if (is_fully_aligned(va_to_map, pa, chunk_left, SZ_32M))
			chunk_size = SZ_32M;
		else if (is_fully_aligned(va_to_map, pa, chunk_left, SZ_2M))
			chunk_size = SZ_2M;
		else if (is_fully_aligned(va_to_map, pa, chunk_left, SZ_64K))
			chunk_size = SZ_64K;

		trace_iommu_map_range(va_to_map, pa,
				ops->get_length(cookie, len),
				chunk_size);

		/* First level */
		fl_offset = FL_OFFSET(va_to_map);
		fl_pte = pt->fl_table + fl_offset;
		ret = handle_1st_lvl(pt, fl_pte, pa, chunk_size, up_at, lo_at);

		if (ret)
			goto fail;

		/* Second level */
		sl_table = FOLLOW_TO_NEXT_TABLE(fl_pte);
		sl_offset = SL_OFFSET(va_to_map);
		sl_pte = sl_table + sl_offset;
		ret = handle_2nd_lvl(pt, sl_pte, pa, chunk_size, up_at, lo_at);

		if (ret < 0)
			goto fail;

		if (ret == SUB_LEVEL_MAPPING_NOT_REQUIRED)
			goto proceed_further;

		/* Third level */
		tl_table = FOLLOW_TO_NEXT_TABLE(sl_pte);
		tl_offset = TL_OFFSET(va_to_map);
		tl_pte = tl_table + tl_offset;
		ret = handle_3rd_lvl(pt, tl_pte, pa, chunk_size, up_at, lo_at);

		if (ret < 0)
			goto fail;

		if (ret == SUB_LEVEL_MAPPING_NOT_REQUIRED)
			goto proceed_further;

		/* Fourth level */
		ll_table = FOLLOW_TO_NEXT_TABLE(tl_pte);
		ll_offset = LL_OFFSET(va_to_map);
		ll_pte = ll_table + ll_offset;
		ret = handle_4th_lvl(pt, ll_pte, pa, chunk_size, up_at, lo_at);

		if (ret)
			goto fail;

proceed_further:
		offset += chunk_size;
		chunk_offset += chunk_size;
		va_to_map += chunk_size;
		pa += chunk_size;

		if (chunk_offset >= ops->get_length(cookie, len)
				&& offset < len) {
			chunk_offset = 0;
			if (ops->get_next(cookie, &cookie))
				break;
			pa = ops->get_phys(cookie);
		}

		ret = 0;
	}
fail:
	if (ret && offset > 0) {
		pr_err("Something_wrong in mapping\n");
		__msm_iommu_pagetable_unmap_range(pt, va, offset, 1);
	}
	return ret;
}

static u64 clear_4th_level(u64 va, u64 *ll_pte, u64 len, u32 redirect,
				u32 silent)
{
	u64 start_offset = LL_OFFSET(va);
	u64 offset, end_offset;
	u64 *pte = ll_pte;
	u64 num_pte;
	u64 chunk_size;

	if ((len / SZ_4K) + start_offset < NUM_LL_PTE)
		end_offset = start_offset + LL_OFFSET(len);
	else
		end_offset = NUM_LL_PTE;

	/* Clear multiple PTEs in the same loop */
	for (offset = start_offset; offset < end_offset; offset++) {
		if (*pte == 0) {
			if (!silent)
				pr_err("Last level PTE is 0 at 0x%p\n", pte);
			return 0;
		}

		*pte = 0;
		pte++;
	}

	num_pte = end_offset - start_offset;
	chunk_size = SZ_4K * num_pte;

	return chunk_size;
}

static u64 clear_3rd_level(u64 va, u64 *tl_pte, u64 len, u32 redirect,
				u32 silent)
{
	u64 chunk_size = 0;
	u64 type = 0;
	u64 *ll_table = NULL;
	u64 *ll_pte;
	u32 ll_offset;

	if (*tl_pte == 0) {
		if (!silent)
			pr_err("Third level PTE is 0 at 0x%p\n", tl_pte);
		return 0;
	}

	type = *tl_pte & FLSL_PTE_TYPE_MASK;
	if (type == FLSL_TYPE_BLOCK) {
		if (len < SZ_2M)
			BUG();

		*tl_pte = 0;
		return SZ_2M;
	} else if (type == FLSL_TYPE_TABLE) {
		ll_table = FOLLOW_TO_NEXT_TABLE(tl_pte);
		ll_offset = LL_OFFSET(va);
		ll_pte = ll_table + ll_offset;
		chunk_size = clear_4th_level(va, ll_pte, len,
					redirect, silent);

		if (is_table_empty(ll_table)) {
			u64 p = (*tl_pte) & FLSL_BASE_MASK;

			if (p) {
				free_pagetable_level(p, 4, 0);
				*tl_pte = 0;
			}
		}
	} else {
		pr_err("Third level PTE is corrupted at 0x%p -> 0x%lx\n",
				tl_pte, (unsigned long)*tl_pte);
	}

	return chunk_size;
}

static u64 clear_2nd_level(u64 va, u64 *sl_pte, u64 len, u32 redirect,
				u32 silent)
{
	u64 chunk_size = 0;
	u64 type = 0;
	u64 *tl_table = NULL;
	u64 *tl_pte;
	u32 tl_offset;

	if (*sl_pte == 0) {
		if (!silent)
			pr_err("Second level PTE is 0 at 0x%p\n", sl_pte);
		return 0;
	}

	type = *sl_pte & FLSL_PTE_TYPE_MASK;
	if (type == FLSL_TYPE_BLOCK) {
		if (len < SZ_1G)
			BUG();

		*sl_pte = 0;
		return SZ_1G;
	} else if (type == FLSL_TYPE_TABLE) {
		tl_table = FOLLOW_TO_NEXT_TABLE(sl_pte);
		tl_offset = TL_OFFSET(va);
		tl_pte = tl_table + tl_offset;
		chunk_size = clear_3rd_level(va, tl_pte, len, redirect,
					silent);

		if (is_table_empty(tl_table)) {
			u64 p = (*sl_pte) & FLSL_BASE_MASK;

			if (p) {
				free_pagetable_level(p, 3, 0);
				*sl_pte = 0;
			}
		}
	} else {
		pr_err("Second level PTE is corrupted at 0x%p -> 0x%lx\n",
				sl_pte, (unsigned long)*sl_pte);
	}

	return chunk_size;
}

static u64 clear_1st_level(u64 va, u64 *fl_pte, u64 len, u32 redirect,
				u32 silent)
{
	u64 chunk_size = 0;
	u64 type = 0;
	u64 *sl_table = NULL;
	u64 *sl_pte;
	u32 sl_offset;

	if (*fl_pte == 0) {
		if (!silent)
			pr_err("First level PTE is 0 at 0x%p\n", fl_pte);

		return 0;
	}

	type = *fl_pte & FLSL_PTE_TYPE_MASK;
	if (type == FLSL_TYPE_BLOCK) {
		if (!silent)
			pr_err("First level PTE has BLOCK mapping at 0x%p\n",
				fl_pte);
		return 0;
	} else if (type == FLSL_TYPE_TABLE) {
		sl_table = FOLLOW_TO_NEXT_TABLE(fl_pte);
		sl_offset = SL_OFFSET(va);
		sl_pte = sl_table + sl_offset;
		chunk_size = clear_2nd_level(va, sl_pte, len, redirect,
					silent);

		if (is_table_empty(sl_table)) {
			u64 p = (*fl_pte) & FLSL_BASE_MASK;

			if (p) {
				free_pagetable_level(p, 2, 0);
				*fl_pte = 0;
			}
		}
	} else {
		pr_err("First level PTE is corrupted at 0x%p -> 0x%lx\n",
				fl_pte, (unsigned long)*fl_pte);
	}

	return chunk_size;
}

static u64 clear_in_chunks(struct msm_iommu_pt *pt, u64 va, u64 len, u32 silent)
{
	u64 *fl_pte;
	u32 fl_offset;

	fl_offset = FL_OFFSET(va);
	fl_pte = pt->fl_table + fl_offset;

	return clear_1st_level(va, fl_pte, len, pt->redirect, silent);
}

static void __msm_iommu_pagetable_unmap_range(struct msm_iommu_pt *pt,
						unsigned long va, size_t len,
						u32 silent)
{
	u64 offset = 0;
	u64 va_to_unmap, left_to_unmap;
	u64 chunk_size = 0;

	BUG_ON(len & (SZ_4K - 1));

	while (offset < len) {
		left_to_unmap = len - offset;
		va_to_unmap = va + offset;
		chunk_size = clear_in_chunks(pt, va_to_unmap, left_to_unmap,
					silent);

		if (!chunk_size) {
			WARN_ON(1);
			return;
		}

		offset += chunk_size;
	}
}

static void flush_pagetable_level(u64 base, int level, unsigned long va,
					size_t len)
{
	unsigned long i;
	unsigned long start;
	unsigned long len_offset;
	unsigned long end = NUM_FL_PTE;
	unsigned long level_granurality;
	unsigned long va_left = va;
	size_t len_left = len;
	u64 *table = phys_to_virt(base);

	if (level <= NUM_PT_LEVEL) {
		switch (level) {
		case 1:
			start = FL_OFFSET(va);
			level_granurality = 1ULL << FL_SHIFT;
			len_offset = FL_OFFSET(len);
			break;
		case 2:
			start = SL_OFFSET(va);
			level_granurality = 1ULL << SL_SHIFT;
			len_offset = SL_OFFSET(len);
			break;
		case 3:
			start = TL_OFFSET(va);
			level_granurality = 1ULL << TL_SHIFT;
			len_offset = TL_OFFSET(len);
			break;
		case 4:
			start = LL_OFFSET(va);
			level_granurality = 1ULL << LL_SHIFT;
			len_offset = LL_OFFSET(len);
			goto flush_this_level;
		default:
			return;
		}
	}

	if ((len / level_granurality) + start < NUM_PTE)
		end = start + len_offset;
	else
		end = NUM_PTE;

	for (i = start; i <= end; ++i) {
		if ((table[i] & FLSL_TYPE_TABLE) == FLSL_TYPE_TABLE) {
			u64 p = table[i] & FLSL_BASE_MASK;

			if (p)
				flush_pagetable_level(p, level + 1, va_left,
						len_left);
		}

		va_left += level_granurality;
		len_left -= level_granurality;
	}

flush_this_level:
	dmac_flush_range(table + start, table + end);
}

int msm_iommu_pagetable_map_range(struct msm_iommu_pt *pt, unsigned long va,
			struct scatterlist *sg, size_t len, int prot)
{
	return __msm_iommu_pagetable_map_range(pt, va, sg, &sg_ops, len, prot);
}

void msm_iommu_pagetable_unmap_range(struct msm_iommu_pt *pt, unsigned long va,
					size_t len)
{
	__msm_iommu_pagetable_unmap_range(pt, va, len, 0);
}

int msm_iommu_pagetable_map(struct msm_iommu_pt *pt, unsigned long va,
			    phys_addr_t pa, size_t len, int prot)
{
	s32 ret;

	ret = __msm_iommu_pagetable_map_range(pt, va, (void *) pa, &regular_ops,
			len, prot);

	return ret;
}

size_t msm_iommu_pagetable_unmap(struct msm_iommu_pt *pt, unsigned long va,
				size_t len)
{
	msm_iommu_pagetable_unmap_range(pt, va, len);
	return len;
}

void msm_iommu_flush_pagetable(struct msm_iommu_pt *pt, unsigned long va,
				size_t len)
{
	u64 *fl_table = pt->fl_table;

	if (!pt->redirect)
		flush_pagetable_level(virt_to_phys(fl_table), 1, va, len);
}

static phys_addr_t get_phys_from_va(unsigned long va, u64 *table, int level)
{
	u64 type;
	u64 mask;		/* For single mapping */
	u64 section_mask;	/* For section mapping */
	u64 *pte;

	if (level <= NUM_PT_LEVEL) {
		switch (level) {
		case 1:
			pte = table + FL_OFFSET(va);
			break;
		case 2:
			pte = table + SL_OFFSET(va);
			mask = 0xFFFFC0000000ULL;
			break;
		case 3:
			pte = table + TL_OFFSET(va);
			mask = 0xFFFFFFE00000ULL;
			section_mask = 0xFFFFFE000000ULL;
			break;
		case 4:
			pte = table + LL_OFFSET(va);
			mask = 0xFFFFFFFFF000ULL;
			section_mask = 0xFFFFFFFF0000ULL;
			break;

		default:
			pte = NULL;
			return 0;
		}

		type = *pte & FLSL_PTE_TYPE_MASK;

		if (type == FLSL_TYPE_BLOCK || level == NUM_PT_LEVEL) {
			if ((*pte & LL_CH) == LL_CH) {
				return (*pte & section_mask) |
					(va & ~section_mask);
			} else {
				return (*pte & mask) |
					(va & ~mask);
			}
		} else if (type == FLSL_TYPE_TABLE) {
			return get_phys_from_va(va, FOLLOW_TO_NEXT_TABLE(pte),
					level + 1);
		}
	}

	return 0;
}

phys_addr_t msm_iommu_iova_to_phys_soft(struct iommu_domain *domain,
							dma_addr_t va)
{
	struct msm_iommu_priv *priv = domain->priv;
	struct msm_iommu_pt *pt = &priv->pt;

	return get_phys_from_va(va, pt->fl_table, 1);
}

void __init msm_iommu_pagetable_init(void)
{
}
