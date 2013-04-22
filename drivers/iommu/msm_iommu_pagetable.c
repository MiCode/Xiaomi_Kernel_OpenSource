/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <mach/iommu.h>
#include <mach/msm_iommu_priv.h>
#include <trace/events/kmem.h>
#include "msm_iommu_pagetable.h"

/* Sharability attributes of MSM IOMMU mappings */
#define MSM_IOMMU_ATTR_NON_SH		0x0
#define MSM_IOMMU_ATTR_SH		0x4

/* Cacheability attributes of MSM IOMMU mappings */
#define MSM_IOMMU_ATTR_NONCACHED	0x0
#define MSM_IOMMU_ATTR_CACHED_WB_WA	0x1
#define MSM_IOMMU_ATTR_CACHED_WB_NWA	0x2
#define MSM_IOMMU_ATTR_CACHED_WT	0x3

static int msm_iommu_tex_class[4];

static inline void clean_pte(unsigned long *start, unsigned long *end,
				int redirect)
{
	if (!redirect)
		dmac_flush_range(start, end);
}

int msm_iommu_pagetable_alloc(struct msm_iommu_pt *pt)
{
	pt->fl_table = (unsigned long *)__get_free_pages(GFP_KERNEL,
							  get_order(SZ_16K));
	if (!pt->fl_table)
		return -ENOMEM;

	memset(pt->fl_table, 0, SZ_16K);
	clean_pte(pt->fl_table, pt->fl_table + NUM_FL_PTE, pt->redirect);

	return 0;
}

void msm_iommu_pagetable_free(struct msm_iommu_pt *pt)
{
	unsigned long *fl_table;
	int i;

	fl_table = pt->fl_table;
	for (i = 0; i < NUM_FL_PTE; i++)
		if ((fl_table[i] & 0x03) == FL_TYPE_TABLE)
			free_page((unsigned long) __va(((fl_table[i]) &
							FL_BASE_MASK)));
	free_pages((unsigned long)fl_table, get_order(SZ_16K));
	pt->fl_table = 0;
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
		tex = (pgprot_kernel >> 2) & 0x07;
	else
		tex = msm_iommu_tex_class[MSM_IOMMU_ATTR_NONCACHED];

	if (tex < 0 || tex > NUM_TEX_CLASS - 1)
		return 0;

	if (len == SZ_16M || len == SZ_1M) {
		pgprot = FL_SHARED;
		pgprot |= tex & 0x01 ? FL_BUFFERABLE : 0;
		pgprot |= tex & 0x02 ? FL_CACHEABLE : 0;
		pgprot |= tex & 0x04 ? FL_TEX0 : 0;
		pgprot |= FL_AP0 | FL_AP1;
		pgprot |= prot & IOMMU_WRITE ? 0 : FL_AP2;
	} else	{
		pgprot = SL_SHARED;
		pgprot |= tex & 0x01 ? SL_BUFFERABLE : 0;
		pgprot |= tex & 0x02 ? SL_CACHEABLE : 0;
		pgprot |= tex & 0x04 ? SL_TEX0 : 0;
		pgprot |= SL_AP0 | SL_AP1;
		pgprot |= prot & IOMMU_WRITE ? 0 : SL_AP2;
	}

	return pgprot;
}

static unsigned long *make_second_level(struct msm_iommu_pt *pt,
					unsigned long *fl_pte)
{
	unsigned long *sl;
	sl = (unsigned long *) __get_free_pages(GFP_KERNEL,
			get_order(SZ_4K));

	if (!sl) {
		pr_debug("Could not allocate second level table\n");
		goto fail;
	}
	memset(sl, 0, SZ_4K);
	clean_pte(sl, sl + NUM_SL_PTE, pt->redirect);

	*fl_pte = ((((int)__pa(sl)) & FL_BASE_MASK) | \
			FL_TYPE_TABLE);

	clean_pte(fl_pte, fl_pte + 1, pt->redirect);
fail:
	return sl;
}

static int sl_4k(unsigned long *sl_pte, phys_addr_t pa, unsigned int pgprot)
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

static int sl_64k(unsigned long *sl_pte, phys_addr_t pa, unsigned int pgprot)
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

static inline int fl_1m(unsigned long *fl_pte, phys_addr_t pa, int pgprot)
{
	if (*fl_pte)
		return -EBUSY;

	*fl_pte = (pa & 0xFFF00000) | FL_NG | FL_TYPE_SECT | FL_SHARED
		| pgprot;

	return 0;
}

static inline int fl_16m(unsigned long *fl_pte, phys_addr_t pa, int pgprot)
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

int msm_iommu_pagetable_map(struct msm_iommu_pt *pt, unsigned long va,
			phys_addr_t pa, size_t len, int prot)
{
	unsigned long *fl_pte;
	unsigned long fl_offset;
	unsigned long *sl_table;
	unsigned long *sl_pte;
	unsigned long sl_offset;
	unsigned int pgprot;
	int ret = 0;

	if (len != SZ_16M && len != SZ_1M &&
	    len != SZ_64K && len != SZ_4K) {
		pr_debug("Bad size: %d\n", len);
		ret = -EINVAL;
		goto fail;
	}

	if (!pt->fl_table) {
		pr_debug("Null page table\n");
		ret = -EINVAL;
		goto fail;
	}

	pgprot = __get_pgprot(prot, len);
	if (!pgprot) {
		ret = -EINVAL;
		goto fail;
	}

	fl_offset = FL_OFFSET(va);		/* Upper 12 bits */
	fl_pte = pt->fl_table + fl_offset;	/* int pointers, 4 bytes */

	if (len == SZ_16M) {
		ret = fl_16m(fl_pte, pa, pgprot);
		if (ret)
			goto fail;
		clean_pte(fl_pte, fl_pte + 16, pt->redirect);
	}

	if (len == SZ_1M) {
		ret = fl_1m(fl_pte, pa, pgprot);
		if (ret)
			goto fail;
		clean_pte(fl_pte, fl_pte + 1, pt->redirect);
	}

	/* Need a 2nd level table */
	if (len == SZ_4K || len == SZ_64K) {

		if (*fl_pte == 0) {
			if (make_second_level(pt, fl_pte) == NULL) {
				ret = -ENOMEM;
				goto fail;
			}
		}

		if (!(*fl_pte & FL_TYPE_TABLE)) {
			ret = -EBUSY;
			goto fail;
		}
	}

	sl_table = (unsigned long *) __va(((*fl_pte) & FL_BASE_MASK));
	sl_offset = SL_OFFSET(va);
	sl_pte = sl_table + sl_offset;

	if (len == SZ_4K) {
		ret = sl_4k(sl_pte, pa, pgprot);
		if (ret)
			goto fail;
		clean_pte(sl_pte, sl_pte + 1, pt->redirect);
	}

	if (len == SZ_64K) {
		ret = sl_64k(sl_pte, pa, pgprot);
		if (ret)
			goto fail;
		clean_pte(sl_pte, sl_pte + 16, pt->redirect);
	}

fail:
	return ret;
}

size_t msm_iommu_pagetable_unmap(struct msm_iommu_pt *pt, unsigned long va,
				size_t len)
{
	unsigned long *fl_pte;
	unsigned long fl_offset;
	unsigned long *sl_table;
	unsigned long *sl_pte;
	unsigned long sl_offset;
	int i, ret = 0;

	if (len != SZ_16M && len != SZ_1M &&
	    len != SZ_64K && len != SZ_4K) {
		pr_debug("Bad length: %d\n", len);
		ret = -EINVAL;
		goto fail;
	}

	if (!pt->fl_table) {
		pr_debug("Null page table\n");
		ret = -EINVAL;
		goto fail;
	}

	fl_offset = FL_OFFSET(va);		/* Upper 12 bits */
	fl_pte = pt->fl_table + fl_offset;	/* int pointers, 4 bytes */

	if (*fl_pte == 0) {
		pr_debug("First level PTE is 0\n");
		ret = -ENODEV;
		goto fail;
	}

	/* Unmap supersection */
	if (len == SZ_16M) {
		for (i = 0; i < 16; i++)
			*(fl_pte+i) = 0;

		clean_pte(fl_pte, fl_pte + 16, pt->redirect);
	}

	if (len == SZ_1M) {
		*fl_pte = 0;
		clean_pte(fl_pte, fl_pte + 1, pt->redirect);
	}

	sl_table = (unsigned long *) __va(((*fl_pte) & FL_BASE_MASK));
	sl_offset = SL_OFFSET(va);
	sl_pte = sl_table + sl_offset;

	if (len == SZ_64K) {
		for (i = 0; i < 16; i++)
			*(sl_pte+i) = 0;

		clean_pte(sl_pte, sl_pte + 16, pt->redirect);
	}

	if (len == SZ_4K) {
		*sl_pte = 0;
		clean_pte(sl_pte, sl_pte + 1, pt->redirect);
	}

	if (len == SZ_4K || len == SZ_64K) {
		int used = 0;

		for (i = 0; i < NUM_SL_PTE; i++)
			if (sl_table[i])
				used = 1;
		if (!used) {
			free_page((unsigned long)sl_table);
			*fl_pte = 0;
			clean_pte(fl_pte, fl_pte + 1, pt->redirect);
		}
	}

fail:
	return ret;
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

static int check_range(unsigned long *fl_table, unsigned int va,
				 unsigned int len)
{
	unsigned int offset = 0;
	unsigned long *fl_pte;
	unsigned long fl_offset;
	unsigned long *sl_table;
	unsigned long sl_start, sl_end;
	int i;

	fl_offset = FL_OFFSET(va);	/* Upper 12 bits */
	fl_pte = fl_table + fl_offset;	/* int pointers, 4 bytes */

	while (offset < len) {
		if (*fl_pte & FL_TYPE_TABLE) {
			sl_start = SL_OFFSET(va);
			sl_table =  __va(((*fl_pte) & FL_BASE_MASK));
			sl_end = ((len - offset) / SZ_4K) + sl_start;

			if (sl_end > NUM_SL_PTE)
				sl_end = NUM_SL_PTE;

			for (i = sl_start; i < sl_end; i++) {
				if (sl_table[i] != 0) {
					pr_err("%08x - %08x already mapped\n",
						va, va + SZ_4K);
					return -EBUSY;
				}
				offset += SZ_4K;
				va += SZ_4K;
			}


			sl_start = 0;
		} else {
			if (*fl_pte != 0) {
				pr_err("%08x - %08x already mapped\n",
				       va, va + SZ_1M);
				return -EBUSY;
			}
			va += SZ_1M;
			offset += SZ_1M;
			sl_start = 0;
		}
		fl_pte++;
	}
	return 0;
}

static inline int is_fully_aligned(unsigned int va, phys_addr_t pa, size_t len,
				   int align)
{
	return  IS_ALIGNED(va, align) && IS_ALIGNED(pa, align)
		&& (len >= align);
}

int msm_iommu_pagetable_map_range(struct msm_iommu_pt *pt, unsigned int va,
		       struct scatterlist *sg, unsigned int len, int prot)
{
	phys_addr_t pa;
	unsigned int start_va = va;
	unsigned int offset = 0;
	unsigned long *fl_pte;
	unsigned long fl_offset;
	unsigned long *sl_table = NULL;
	unsigned long sl_offset, sl_start;
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
	pa = get_phys_addr(sg);

	ret = check_range(pt->fl_table, va, len);
	if (ret)
		goto fail;

	while (offset < len) {
		chunk_size = SZ_4K;

		if (is_fully_aligned(va, pa, sg->length - chunk_offset,
				     SZ_16M))
			chunk_size = SZ_16M;
		else if (is_fully_aligned(va, pa, sg->length - chunk_offset,
					  SZ_1M))
			chunk_size = SZ_1M;
		/* 64k or 4k determined later */

		trace_iommu_map_range(va, pa, sg->length, chunk_size);

		/* for 1M and 16M, only first level entries are required */
		if (chunk_size >= SZ_1M) {
			if (chunk_size == SZ_16M) {
				ret = fl_16m(fl_pte, pa, pgprot16m);
				if (ret)
					goto fail;
				clean_pte(fl_pte, fl_pte + 16, pt->redirect);
				fl_pte += 16;
			} else if (chunk_size == SZ_1M) {
				ret = fl_1m(fl_pte, pa, pgprot1m);
				if (ret)
					goto fail;
				clean_pte(fl_pte, fl_pte + 1, pt->redirect);
				fl_pte++;
			}

			offset += chunk_size;
			chunk_offset += chunk_size;
			va += chunk_size;
			pa += chunk_size;

			if (chunk_offset >= sg->length && offset < len) {
				chunk_offset = 0;
				sg = sg_next(sg);
				pa = get_phys_addr(sg);
			}
			continue;
		}
		/* for 4K or 64K, make sure there is a second level table */
		if (*fl_pte == 0) {
			if (!make_second_level(pt, fl_pte)) {
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

			if (is_fully_aligned(va, pa, sg->length - chunk_offset,
					     SZ_64K))
				chunk_size = SZ_64K;
			else
				chunk_size = SZ_4K;

			trace_iommu_map_range(va, pa, sg->length,
							chunk_size);

			if (chunk_size == SZ_4K) {
				sl_4k(&sl_table[sl_offset], pa, pgprot4k);
				sl_offset++;
			} else {
				BUG_ON(sl_offset + 16 > NUM_SL_PTE);
				sl_64k(&sl_table[sl_offset], pa, pgprot64k);
				sl_offset += 16;
			}


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

		clean_pte(sl_table + sl_start, sl_table + sl_offset,
				pt->redirect);
		fl_pte++;
		sl_offset = 0;
	}

fail:
	if (ret && offset > 0)
		msm_iommu_pagetable_unmap_range(pt, start_va, offset);

	return ret;
}

void msm_iommu_pagetable_unmap_range(struct msm_iommu_pt *pt, unsigned int va,
				 unsigned int len)
{
	unsigned int offset = 0;
	unsigned long *fl_pte;
	unsigned long fl_offset;
	unsigned long *sl_table;
	unsigned long sl_start, sl_end;
	int used, i;

	BUG_ON(len & (SZ_4K - 1));

	fl_offset = FL_OFFSET(va);		/* Upper 12 bits */
	fl_pte = pt->fl_table + fl_offset;	/* int pointers, 4 bytes */

	while (offset < len) {
		if (*fl_pte & FL_TYPE_TABLE) {
			sl_start = SL_OFFSET(va);
			sl_table =  __va(((*fl_pte) & FL_BASE_MASK));
			sl_end = ((len - offset) / SZ_4K) + sl_start;

			if (sl_end > NUM_SL_PTE)
				sl_end = NUM_SL_PTE;

			memset(sl_table + sl_start, 0, (sl_end - sl_start) * 4);
			clean_pte(sl_table + sl_start, sl_table + sl_end,
					pt->redirect);

			offset += (sl_end - sl_start) * SZ_4K;
			va += (sl_end - sl_start) * SZ_4K;

			/* Unmap and free the 2nd level table if all mappings
			 * in it were removed. This saves memory, but the table
			 * will need to be re-allocated the next time someone
			 * tries to map these VAs.
			 */
			used = 0;

			/* If we just unmapped the whole table, don't bother
			 * seeing if there are still used entries left.
			 */
			if (sl_end - sl_start != NUM_SL_PTE)
				for (i = 0; i < NUM_SL_PTE; i++)
					if (sl_table[i]) {
						used = 1;
						break;
					}
			if (!used) {
				free_page((unsigned long)sl_table);
				*fl_pte = 0;

				clean_pte(fl_pte, fl_pte + 1, pt->redirect);
			}

			sl_start = 0;
		} else {
			*fl_pte = 0;
			clean_pte(fl_pte, fl_pte + 1, pt->redirect);
			va += SZ_1M;
			offset += SZ_1M;
			sl_start = 0;
		}
		fl_pte++;
	}
}

static int __init get_tex_class(int icp, int ocp, int mt, int nos)
{
	int i = 0;
	unsigned int prrr = 0;
	unsigned int nmrr = 0;
	int c_icp, c_ocp, c_mt, c_nos;

	RCP15_PRRR(prrr);
	RCP15_NMRR(nmrr);

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
			get_tex_class(CP_NONCACHED, CP_NONCACHED, MT_NORMAL, 1);

	msm_iommu_tex_class[MSM_IOMMU_ATTR_CACHED_WB_WA] =
			get_tex_class(CP_WB_WA, CP_WB_WA, MT_NORMAL, 1);

	msm_iommu_tex_class[MSM_IOMMU_ATTR_CACHED_WB_NWA] =
			get_tex_class(CP_WB_NWA, CP_WB_NWA, MT_NORMAL, 1);

	msm_iommu_tex_class[MSM_IOMMU_ATTR_CACHED_WT] =
			get_tex_class(CP_WT, CP_WT, MT_NORMAL, 1);
}

void __init msm_iommu_pagetable_init(void)
{
	setup_iommu_tex_classes();
}
