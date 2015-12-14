/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/scatterlist.h>

#include <asm/cacheflush.h>

#include <linux/qcom_iommu.h>
#include "msm_iommu_priv.h"
#include <trace/events/kmem.h>
#include "msm_iommu_pagetable.h"

#define NUM_FL_PTE      4096
#define NUM_SL_PTE      256
#define GUARD_PTE       2
#define NUM_TEX_CLASS   8

/* First-level page table bits */
#define FL_BASE_MASK            0xFFFFFC00
#define FL_TYPE_TABLE           (1 << 0)
#define FL_TYPE_SECT            (2 << 0)
#define FL_SUPERSECTION         (1 << 18)
#define FL_AP0                  (1 << 10)
#define FL_AP1                  (1 << 11)
#define FL_AP2                  (1 << 15)
#define FL_SHARED               (1 << 16)
#define FL_BUFFERABLE           (1 << 2)
#define FL_CACHEABLE            (1 << 3)
#define FL_TEX0                 (1 << 12)
#define FL_OFFSET(va)           (((va) & 0xFFF00000) >> 20)
#define FL_NG                   (1 << 17)

/* Second-level page table bits */
#define SL_BASE_MASK_LARGE      0xFFFF0000
#define SL_BASE_MASK_SMALL      0xFFFFF000
#define SL_TYPE_LARGE           (1 << 0)
#define SL_TYPE_SMALL           (2 << 0)
#define SL_AP0                  (1 << 4)
#define SL_AP1                  (2 << 4)
#define SL_AP2                  (1 << 9)
#define SL_SHARED               (1 << 10)
#define SL_BUFFERABLE           (1 << 2)
#define SL_CACHEABLE            (1 << 3)
#define SL_TEX0                 (1 << 6)
#define SL_OFFSET(va)           (((va) & 0xFF000) >> 12)
#define SL_NG                   (1 << 11)

/* Memory type and cache policy attributes */
#define MT_SO                   0
#define MT_DEV                  1
#define MT_IOMMU_NORMAL         2
#define CP_NONCACHED            0
#define CP_WB_WA                1
#define CP_WT                   2
#define CP_WB_NWA               3

/* Sharability attributes of MSM IOMMU mappings */
#define MSM_IOMMU_ATTR_NON_SH		0x0
#define MSM_IOMMU_ATTR_SH		0x4

/* Cacheability attributes of MSM IOMMU mappings */
#define MSM_IOMMU_ATTR_NONCACHED	0x0
#define MSM_IOMMU_ATTR_CACHED_WB_WA	0x1
#define MSM_IOMMU_ATTR_CACHED_WB_NWA	0x2
#define MSM_IOMMU_ATTR_CACHED_WT	0x3

static int msm_iommu_tex_class[4];

/* TEX Remap Registers */
#define NMRR_ICP(nmrr, n) (((nmrr) & (3 << ((n) * 2))) >> ((n) * 2))
#define NMRR_OCP(nmrr, n) (((nmrr) & (3 << ((n) * 2 + 16))) >> ((n) * 2 + 16))

#define PRRR_NOS(prrr, n) ((prrr) & (1 << ((n) + 24)) ? 1 : 0)
#define PRRR_MT(prrr, n)  ((((prrr) & (3 << ((n) * 2))) >> ((n) * 2)))

static inline void clean_pte(u32 *start, u32 *end, int redirect)
{
	if (!redirect)
		dmac_flush_range(start, end);
}

int msm_iommu_pagetable_alloc(struct msm_iommu_pt *pt)
{
	pt->fl_table = (u32 *)__get_free_pages(GFP_KERNEL,
							  get_order(SZ_16K));
	if (!pt->fl_table)
		return -ENOMEM;

	pt->fl_table_shadow = (u32 *)__get_free_pages(GFP_KERNEL,
							  get_order(SZ_16K));
	if (!pt->fl_table_shadow) {
		free_pages((unsigned long)pt->fl_table, get_order(SZ_16K));
		return -ENOMEM;
	}

	memset(pt->fl_table, 0, SZ_16K);
	memset(pt->fl_table_shadow, 0, SZ_16K);
	clean_pte(pt->fl_table, pt->fl_table + NUM_FL_PTE, pt->redirect);

	return 0;
}

void msm_iommu_pagetable_free(struct msm_iommu_pt *pt)
{
	u32 *fl_table;
	u32 *fl_table_shadow;
	int i;

	fl_table = pt->fl_table;
	fl_table_shadow = pt->fl_table_shadow;
	for (i = 0; i < NUM_FL_PTE; i++)
		if ((fl_table[i] & 0x03) == FL_TYPE_TABLE)
			free_page((unsigned long) __va(((fl_table[i]) &
							FL_BASE_MASK)));
	free_pages((unsigned long)fl_table, get_order(SZ_16K));
	pt->fl_table = 0;

	free_pages((unsigned long)fl_table_shadow, get_order(SZ_16K));
	pt->fl_table_shadow = 0;
}

void msm_iommu_pagetable_free_tables(struct msm_iommu_pt *pt, unsigned long va,
				 size_t len)
{
	/*
	 * Adding 2 for worst case. We could be spanning 3 second level pages
	 * if we unmapped just over 1MB.
	 */
	u32 n_entries = len / SZ_1M + 2;
	u32 fl_offset = FL_OFFSET(va);
	u32 i;

	for (i = 0; i < n_entries && fl_offset < NUM_FL_PTE; ++i) {
		u32 *fl_pte_shadow = pt->fl_table_shadow + fl_offset;
		void *sl_table_va = __va(((*fl_pte_shadow) & ~0x1FF));
		u32 sl_table = *fl_pte_shadow;

		if (sl_table && !(sl_table & 0x1FF)) {
			free_pages((unsigned long) sl_table_va,
				   get_order(SZ_4K));
			*fl_pte_shadow = 0;
		}
		++fl_offset;
	}
}

static int __get_pgprot(int prot, int len)
{
	unsigned int pgprot;
	int tex;

	if (!(prot & (IOMMU_READ | IOMMU_WRITE))) {
		prot |= IOMMU_READ | IOMMU_WRITE;
		WARN_ONCE(1, "No attributes in iommu mapping; assuming RW\n");
	}

	if ((prot & IOMMU_WRITE) && !(prot & IOMMU_READ)) {
		prot |= IOMMU_READ;
		WARN_ONCE(1, "Write-only unsupported; falling back to RW\n");
	}

	if (prot & IOMMU_CACHE)
		tex = (pgprot_val(PAGE_KERNEL) >> 2) & 0x07;
	else if (prot & IOMMU_DEVICE)
		tex = 0;
	else
		tex = msm_iommu_tex_class[MSM_IOMMU_ATTR_NONCACHED];

	if (tex < 0 || tex > NUM_TEX_CLASS - 1)
		return 0;

	if (len == SZ_16M || len == SZ_1M) {
		pgprot = FL_SHARED;
		pgprot |= tex & 0x01 ? FL_BUFFERABLE : 0;
		pgprot |= tex & 0x02 ? FL_CACHEABLE : 0;
		pgprot |= tex & 0x04 ? FL_TEX0 : 0;
		pgprot |= prot & IOMMU_PRIV ? FL_AP0 :
			(FL_AP0 | FL_AP1);
		pgprot |= prot & IOMMU_WRITE ? 0 : FL_AP2;
	} else	{
		pgprot = SL_SHARED;
		pgprot |= tex & 0x01 ? SL_BUFFERABLE : 0;
		pgprot |= tex & 0x02 ? SL_CACHEABLE : 0;
		pgprot |= tex & 0x04 ? SL_TEX0 : 0;
		pgprot |= prot & IOMMU_PRIV ? SL_AP0 :
			(SL_AP0 | SL_AP1);
		pgprot |= prot & IOMMU_WRITE ? 0 : SL_AP2;
	}

	return pgprot;
}

static u32 *make_second_level(struct msm_iommu_pt *pt, u32 *fl_pte,
				u32 *fl_pte_shadow)
{
	u32 *sl;

	sl = (u32 *) __get_free_pages(GFP_ATOMIC,
			get_order(SZ_4K));

	if (!sl) {
		pr_debug("Could not allocate second level table\n");
		goto fail;
	}
	memset(sl, 0, SZ_4K);
	clean_pte(sl, sl + NUM_SL_PTE + GUARD_PTE, pt->redirect);

	*fl_pte = ((((int)__pa(sl)) & FL_BASE_MASK) | FL_TYPE_TABLE);
	*fl_pte_shadow = *fl_pte & ~0x1FF;

	clean_pte(fl_pte, fl_pte + 1, pt->redirect);
fail:
	return sl;
}

static int sl_4k(u32 *sl_pte, phys_addr_t pa, unsigned int pgprot)
{
	int ret = 0;

	if (*sl_pte) {
		ret = -EBUSY;
		goto fail;
	}

	*sl_pte = (pa & SL_BASE_MASK_SMALL) | SL_NG | SL_SHARED
		| SL_TYPE_SMALL | pgprot;
fail:
	return ret;
}

static int sl_64k(u32 *sl_pte, phys_addr_t pa, unsigned int pgprot)
{
	int ret = 0;

	int i;

	for (i = 0; i < 16; i++)
		if (*(sl_pte+i)) {
			ret = -EBUSY;
			goto fail;
		}

	for (i = 0; i < 16; i++)
		*(sl_pte+i) = (pa & SL_BASE_MASK_LARGE) | SL_NG
				| SL_SHARED | SL_TYPE_LARGE | pgprot;

fail:
	return ret;
}

static inline int fl_1m(u32 *fl_pte, phys_addr_t pa, int pgprot)
{
	if (*fl_pte)
		return -EBUSY;

	*fl_pte = (pa & 0xFFF00000) | FL_NG | FL_TYPE_SECT | FL_SHARED
		| pgprot;

	return 0;
}

static inline int fl_16m(u32 *fl_pte, phys_addr_t pa, int pgprot)
{
	int i;
	int ret = 0;

	for (i = 0; i < 16; i++)
		if (*(fl_pte+i)) {
			ret = -EBUSY;
			goto fail;
		}
	for (i = 0; i < 16; i++)
		*(fl_pte+i) = (pa & 0xFF000000) | FL_SUPERSECTION
			| FL_TYPE_SECT | FL_SHARED | FL_NG | pgprot;
fail:
	return ret;
}

static phys_addr_t __get_phys_sg(void *cookie)
{
	struct scatterlist *sg = cookie;
	struct page *page = sg_page(sg);

	BUG_ON(page == NULL);

	return sg_phys(sg);
}

static unsigned long __get_length_sg(void *cookie, unsigned int total)
{
	struct scatterlist *sg = cookie;

	return sg->length;
}

static int __get_next_sg(void *old, void **new)
{
	struct scatterlist *sg = old;
	*new = sg_next(sg);
	return 0;
}

static phys_addr_t __get_phys_bare(void *cookie)
{
	return (phys_addr_t)cookie;
}

static unsigned long __get_length_bare(void *cookie, unsigned int total)
{
	return total;
}

static int __get_next_bare(void *old, void **new)
{
	/* Put something here in hopes of catching errors... */
	*new = (void *)-1;
	return -EINVAL;
}

struct msm_iommu_map_ops {
	phys_addr_t (*get_phys)(void *cookie);
	unsigned long (*get_length)(void *cookie, unsigned int total);
	int (*get_next)(void *old, void **new);
};

static struct msm_iommu_map_ops regular_ops = {
	.get_phys =	__get_phys_bare,
	.get_length =	__get_length_bare,
	.get_next =	__get_next_bare,
};

static struct msm_iommu_map_ops sg_ops = {
	.get_phys =	__get_phys_sg,
	.get_length =	__get_length_sg,
	.get_next =	__get_next_sg,
};

/*
 * For debugging we may want to force mappings to be 4K only
 */
#ifdef CONFIG_IOMMU_FORCE_4K_MAPPINGS
static inline int is_fully_aligned(unsigned int va, phys_addr_t pa, size_t len,
				   int align)
{
	if (align == SZ_4K) {
		return  IS_ALIGNED(va, align) && IS_ALIGNED(pa, align)
			&& (len >= align);
	} else {
		return 0;
	}
}
#else
static inline int is_fully_aligned(unsigned int va, phys_addr_t pa, size_t len,
				   int align)
{
	return  IS_ALIGNED(va, align) && IS_ALIGNED(pa, align)
		&& (len >= align);
}
#endif

static int __msm_iommu_pagetable_map_range(struct msm_iommu_pt *pt,
		       unsigned long va, void *cookie,
		       struct msm_iommu_map_ops *ops,
		       size_t len, int prot)
{
	phys_addr_t pa;
	unsigned int start_va = va;
	unsigned int offset = 0;
	u32 *fl_pte;
	u32 *fl_pte_shadow;
	u32 fl_offset;
	u32 *sl_table = NULL;
	u32 sl_offset, sl_start;
	unsigned int chunk_size, chunk_offset = 0;
	int ret = 0;
	unsigned int pgprot4k, pgprot64k, pgprot1m, pgprot16m;

	BUG_ON(len & (SZ_4K - 1));

	pgprot4k = __get_pgprot(prot, SZ_4K);
	pgprot64k = __get_pgprot(prot, SZ_64K);
	pgprot1m = __get_pgprot(prot, SZ_1M);
	pgprot16m = __get_pgprot(prot, SZ_16M);
	if (!pgprot4k || !pgprot64k || !pgprot1m || !pgprot16m) {
		ret = -EINVAL;
		goto fail;
	}

	fl_offset = FL_OFFSET(va);		/* Upper 12 bits */
	fl_pte = pt->fl_table + fl_offset;	/* int pointers, 4 bytes */
	fl_pte_shadow = pt->fl_table_shadow + fl_offset;
	pa = ops->get_phys(cookie);

	while (offset < len) {
		chunk_size = SZ_4K;

		if (is_fully_aligned(va, pa,
				    ops->get_length(cookie, len) - chunk_offset,
				     SZ_16M))
			chunk_size = SZ_16M;
		else if (is_fully_aligned(va, pa,
				    ops->get_length(cookie, len) - chunk_offset,
					  SZ_1M))
			chunk_size = SZ_1M;
		/* 64k or 4k determined later */

		trace_iommu_map_range(va, pa, ops->get_length(cookie, len),
					chunk_size);

		/* for 1M and 16M, only first level entries are required */
		if (chunk_size >= SZ_1M) {
			if (chunk_size == SZ_16M) {
				ret = fl_16m(fl_pte, pa, pgprot16m);
				if (ret)
					goto fail;
				clean_pte(fl_pte, fl_pte + 16, pt->redirect);
				fl_pte += 16;
				fl_pte_shadow += 16;
			} else if (chunk_size == SZ_1M) {
				ret = fl_1m(fl_pte, pa, pgprot1m);
				if (ret)
					goto fail;
				clean_pte(fl_pte, fl_pte + 1, pt->redirect);
				fl_pte++;
				fl_pte_shadow++;
			}

			offset += chunk_size;
			chunk_offset += chunk_size;
			va += chunk_size;
			pa += chunk_size;

			if (chunk_offset >= ops->get_length(cookie, len) &&
			    offset < len) {
				chunk_offset = 0;
				if (ops->get_next(cookie, &cookie))
					break;
				pa = ops->get_phys(cookie);
			}
			continue;
		}
		/* for 4K or 64K, make sure there is a second level table */
		if (*fl_pte == 0) {
			if (!make_second_level(pt, fl_pte, fl_pte_shadow)) {
				ret = -ENOMEM;
				goto fail;
			}
		}
		if (!(*fl_pte & FL_TYPE_TABLE)) {
			ret = -EBUSY;
			goto fail;
		}
		sl_table = __va(((*fl_pte) & FL_BASE_MASK));
		sl_offset = SL_OFFSET(va);
		/* Keep track of initial position so we
		 * don't clean more than we have to
		 */
		sl_start = sl_offset;

		/* Build the 2nd level page table */
		while (offset < len && sl_offset < NUM_SL_PTE) {
			/* Map a large 64K page if the chunk is large enough and
			 * the pa and va are aligned
			 */

			if (is_fully_aligned(va, pa,
				    ops->get_length(cookie, len) - chunk_offset,
					     SZ_64K))
				chunk_size = SZ_64K;
			else
				chunk_size = SZ_4K;

			trace_iommu_map_range(va, pa,
				ops->get_length(cookie, len), chunk_size);

			if (chunk_size == SZ_4K) {
				ret = sl_4k(&sl_table[sl_offset], pa, pgprot4k);
				if (ret)
					goto fail;
				sl_offset++;
				/* Increment map count */
				(*fl_pte_shadow)++;
			} else {
				BUG_ON(sl_offset + 16 > NUM_SL_PTE);
				ret = sl_64k(&sl_table[sl_offset], pa,
						pgprot64k);
				if (ret)
					goto fail;
				sl_offset += 16;
				/* Increment map count */
				*fl_pte_shadow += 16;
			}

			offset += chunk_size;
			chunk_offset += chunk_size;
			va += chunk_size;
			pa += chunk_size;

			if (chunk_offset >= ops->get_length(cookie, len) &&
			    offset < len) {
				chunk_offset = 0;
				if (ops->get_next(cookie, &cookie))
					break;
				pa = ops->get_phys(cookie);
			}
		}

		clean_pte(sl_table + sl_start, sl_table + sl_offset,
				pt->redirect);
		fl_pte++;
		fl_pte_shadow++;
		sl_offset = 0;
	}

fail:
	if (ret && offset > 0)
		msm_iommu_pagetable_unmap_range(pt, start_va, offset);

	return ret;
}

void msm_iommu_pagetable_unmap_range(struct msm_iommu_pt *pt, unsigned long va,
				 size_t len)
{
	unsigned int offset = 0;
	u32 *fl_pte;
	u32 *fl_pte_shadow;
	u32 fl_offset;
	u32 *sl_table;
	u32 sl_start, sl_end;
	u32 *temp;
	int used;

	BUG_ON(len & (SZ_4K - 1));

	fl_offset = FL_OFFSET(va);		/* Upper 12 bits */
	fl_pte = pt->fl_table + fl_offset;	/* int pointers, 4 bytes */
	fl_pte_shadow = pt->fl_table_shadow + fl_offset;

	while (offset < len) {
		if (*fl_pte & FL_TYPE_TABLE) {
			unsigned int n_entries;

			sl_start = SL_OFFSET(va);
			sl_table =  __va(((*fl_pte) & FL_BASE_MASK));
			sl_end = ((len - offset) / SZ_4K) + sl_start;

			if (sl_end > NUM_SL_PTE)
				sl_end = NUM_SL_PTE;
			n_entries = sl_end - sl_start;

			for (temp = sl_table + sl_start;
					temp < sl_table + sl_end; temp++)
				BUG_ON(!*temp);

			memset(sl_table + sl_start, 0, n_entries * 4);
			clean_pte(sl_table + sl_start, sl_table + sl_end,
					pt->redirect);

			offset += n_entries * SZ_4K;
			va += n_entries * SZ_4K;

			BUG_ON((*fl_pte_shadow & 0x1FF) < n_entries);

			/* Decrement map count */
			*fl_pte_shadow -= n_entries;
			used = *fl_pte_shadow & 0x1FF;

			if (!used) {
				*fl_pte = 0;
				clean_pte(fl_pte, fl_pte + 1, pt->redirect);
			}

			sl_start = 0;
		} else {
			*fl_pte = 0;
			*fl_pte_shadow = 0;

			clean_pte(fl_pte, fl_pte + 1, pt->redirect);
			va += SZ_1M;
			offset += SZ_1M;
			sl_start = 0;
		}
		fl_pte++;
		fl_pte_shadow++;
	}
}

int msm_iommu_pagetable_map_range(struct msm_iommu_pt *pt, unsigned long va,
		struct scatterlist *sg, size_t len, int prot)
{
	return __msm_iommu_pagetable_map_range(pt, va, sg, &sg_ops, len, prot);
}

size_t msm_iommu_pagetable_unmap(struct msm_iommu_pt *pt, unsigned long va,
				size_t len)
{
	msm_iommu_pagetable_unmap_range(pt, va, len);
	return len;
}

int msm_iommu_pagetable_map(struct msm_iommu_pt *pt, unsigned long va,
			phys_addr_t pa, size_t len, int prot)
{
	int ret;

	ret = __msm_iommu_pagetable_map_range(pt, va, (void *)pa, &regular_ops,
						len, prot);
	return ret;
}

void msm_iommu_flush_pagetable(struct msm_iommu_pt *pt, unsigned long va,
				size_t len)
{
	/* Consolidated flush of page tables has not been implemented for
	 * v7S because this driver anyway takes care of combining flush
	 * for last level PTEs
	 */
}

phys_addr_t msm_iommu_iova_to_phys_soft(struct iommu_domain *domain,
					  dma_addr_t va)
{
	struct msm_iommu_priv *priv = domain->priv;
	struct msm_iommu_pt *pt = &priv->pt;
	u32 *fl_pte;
	u32 fl_offset;
	u32 *sl_table = NULL;
	u32 sl_offset;
	u32 *sl_pte;

	if (!pt->fl_table) {
		pr_err("Page table doesn't exist\n");
		return 0;
	}

	fl_offset = FL_OFFSET(va);
	fl_pte = pt->fl_table + fl_offset;

	if (*fl_pte & FL_TYPE_TABLE) {
		sl_table = __va(((*fl_pte) & FL_BASE_MASK));
		sl_offset = SL_OFFSET(va);
		sl_pte = sl_table + sl_offset;
		/* 64 KB section */
		if (*sl_pte & SL_TYPE_LARGE)
			return (*sl_pte & 0xFFFF0000) | (va & ~0xFFFF0000);
		/* 4 KB section */
		if (*sl_pte & SL_TYPE_SMALL)
			return (*sl_pte & 0xFFFFF000) | (va & ~0xFFFFF000);
	} else {
		/* 16 MB section */
		if (*fl_pte & FL_SUPERSECTION)
			return (*fl_pte & 0xFF000000) | (va & ~0xFF000000);
		/* 1 MB section */
		if (*fl_pte & FL_TYPE_SECT)
			return (*fl_pte & 0xFFF00000) | (va & ~0xFFF00000);
	}
	return 0;
}

static int __init get_tex_class(int icp, int ocp, int mt, int nos)
{
	int i = 0;
	unsigned int prrr;
	unsigned int nmrr;
	int c_icp, c_ocp, c_mt, c_nos;

	prrr = msm_iommu_get_prrr();
	nmrr = msm_iommu_get_nmrr();

	for (i = 0; i < NUM_TEX_CLASS; i++) {
		c_nos = PRRR_NOS(prrr, i);
		c_mt = PRRR_MT(prrr, i);
		c_icp = NMRR_ICP(nmrr, i);
		c_ocp = NMRR_OCP(nmrr, i);

		if (icp == c_icp && ocp == c_ocp && c_mt == mt && c_nos == nos)
			return i;
	}

	return -ENODEV;
}

static void __init setup_iommu_tex_classes(void)
{
	msm_iommu_tex_class[MSM_IOMMU_ATTR_NONCACHED] =
			get_tex_class(CP_NONCACHED, CP_NONCACHED,
			MT_IOMMU_NORMAL, 1);

	msm_iommu_tex_class[MSM_IOMMU_ATTR_CACHED_WB_WA] =
			get_tex_class(CP_WB_WA, CP_WB_WA, MT_IOMMU_NORMAL, 1);

	msm_iommu_tex_class[MSM_IOMMU_ATTR_CACHED_WB_NWA] =
			get_tex_class(CP_WB_NWA, CP_WB_NWA, MT_IOMMU_NORMAL, 1);

	msm_iommu_tex_class[MSM_IOMMU_ATTR_CACHED_WT] =
			get_tex_class(CP_WT, CP_WT, MT_IOMMU_NORMAL, 1);
}

void __init msm_iommu_pagetable_init(void)
{
	setup_iommu_tex_classes();
}
