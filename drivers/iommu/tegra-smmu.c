/*
 * IOMMU driver for SMMU on Tegra 3 series SoCs and later.
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/iommu.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_iommu.h>
#include <linux/dma-mapping.h>

#include <asm/page.h>
#include <asm/cacheflush.h>
#include <asm/dma-iommu.h>

#include <mach/iomap.h>
#include <mach/hardware.h>
#include <mach/tegra_smmu.h>
#include <mach/tegra-swgid.h>

/* bitmap of the page sizes currently supported */
#define SMMU_IOMMU_PGSIZES	(SZ_4K | SZ_4M)

#define SMMU_CONFIG				0x10
#define SMMU_CONFIG_DISABLE			0
#define SMMU_CONFIG_ENABLE			1

enum {
	_TLB = 0,
	_PTC,
};

#define SMMU_CACHE_CONFIG_BASE			0x14
#define __SMMU_CACHE_CONFIG(mc, cache)		(SMMU_CACHE_CONFIG_BASE + 4 * cache)
#define SMMU_CACHE_CONFIG(cache)		__SMMU_CACHE_CONFIG(_MC, cache)

#define SMMU_CACHE_CONFIG_STATS_SHIFT		31
#define SMMU_CACHE_CONFIG_STATS_MASK		(1 << SMMU_CACHE_CONFIG_STATS_SHIFT)
#define SMMU_CACHE_CONFIG_STATS_ENABLE		(1 << SMMU_CACHE_CONFIG_STATS_SHIFT)
#define SMMU_CACHE_CONFIG_STATS_TEST_SHIFT	30
#define SMMU_CACHE_CONFIG_STATS_TEST_MASK	(1 << SMMU_CACHE_CONFIG_STATS_TEST_SHIFT)
#define SMMU_CACHE_CONFIG_STATS_TEST		(1 << SMMU_CACHE_CONFIG_STATS_TEST_SHIFT)

#define SMMU_TLB_CONFIG_HIT_UNDER_MISS__ENABLE	(1 << 29)
#define SMMU_TLB_CONFIG_ACTIVE_LINES__VALUE	0x10
#define SMMU_TLB_CONFIG_RESET_VAL		0x20000010

#define SMMU_PTC_CONFIG_CACHE__ENABLE		(1 << 29)
#define SMMU_PTC_CONFIG_INDEX_MAP__PATTERN	0x3f
#define SMMU_PTC_CONFIG_RESET_VAL		0x2000003f

#define SMMU_PTB_ASID				0x1c
#define SMMU_PTB_ASID_CURRENT_SHIFT		0

#define SMMU_PTB_DATA				0x20
#define SMMU_PTB_DATA_RESET_VAL			0
#define SMMU_PTB_DATA_ASID_NONSECURE_SHIFT	29
#define SMMU_PTB_DATA_ASID_WRITABLE_SHIFT	30
#define SMMU_PTB_DATA_ASID_READABLE_SHIFT	31

#define SMMU_TLB_FLUSH				0x30
#define SMMU_TLB_FLUSH_VA_MATCH_ALL		0
#define SMMU_TLB_FLUSH_VA_MATCH_SECTION		2
#define SMMU_TLB_FLUSH_VA_MATCH_GROUP		3
#define SMMU_TLB_FLUSH_ASID_SHIFT		29
#define SMMU_TLB_FLUSH_ASID_MATCH_DISABLE	0
#define SMMU_TLB_FLUSH_ASID_MATCH_ENABLE	1
#define SMMU_TLB_FLUSH_ASID_MATCH_SHIFT		31
#define SMMU_TLB_FLUSH_ASID_ENABLE					\
	(SMMU_TLB_FLUSH_ASID_MATCH_ENABLE << SMMU_TLB_FLUSH_ASID_MATCH_SHIFT)

#define SMMU_PTC_FLUSH				0x34
#define SMMU_PTC_FLUSH_TYPE_ALL			0
#define SMMU_PTC_FLUSH_TYPE_ADR			1
#define SMMU_PTC_FLUSH_ADR_SHIFT		4

#define SMMU_ASID_SECURITY			0x38

#define SMMU_STATS_CACHE_COUNT_BASE		0x1f0

#define SMMU_STATS_CACHE_COUNT(mc, cache, hitmiss)		\
	(SMMU_STATS_CACHE_COUNT_BASE + 8 * cache + 4 * hitmiss)

#define SMMU_TRANSLATION_ENABLE_0		0x228
#define SMMU_TRANSLATION_ENABLE_1		0x22c
#define SMMU_TRANSLATION_ENABLE_2		0x230

#define SMMU_AFI_ASID	0x238   /* PCIE */

#define SMMU_SWGRP_ASID_BASE	SMMU_AFI_ASID

#define HWGRP_COUNT	64

#define SMMU_PDE_NEXT_SHIFT		28

/* AHB Arbiter Registers */
#define AHB_XBAR_CTRL				0xe0
#define AHB_XBAR_CTRL_SMMU_INIT_DONE_DONE	1
#define AHB_XBAR_CTRL_SMMU_INIT_DONE_SHIFT	17

#define SMMU_NUM_ASIDS				4
#define SMMU_TLB_FLUSH_VA_SECTION__MASK		0xffc00000
#define SMMU_TLB_FLUSH_VA_SECTION__SHIFT	12 /* right shift */
#define SMMU_TLB_FLUSH_VA_GROUP__MASK		0xffffc000
#define SMMU_TLB_FLUSH_VA_GROUP__SHIFT		12 /* right shift */
#define SMMU_TLB_FLUSH_VA(iova, which)	\
	((((iova) & SMMU_TLB_FLUSH_VA_##which##__MASK) >> \
		SMMU_TLB_FLUSH_VA_##which##__SHIFT) |	\
	SMMU_TLB_FLUSH_VA_MATCH_##which)
#define SMMU_PTB_ASID_CUR(n)	\
		((n) << SMMU_PTB_ASID_CURRENT_SHIFT)

#define SMMU_TLB_FLUSH_ALL 0

#define SMMU_TLB_FLUSH_ASID_MATCH_disable		\
		(SMMU_TLB_FLUSH_ASID_MATCH_DISABLE <<	\
			SMMU_TLB_FLUSH_ASID_MATCH_SHIFT)
#define SMMU_TLB_FLUSH_ASID_MATCH__ENABLE		\
		(SMMU_TLB_FLUSH_ASID_MATCH_ENABLE <<	\
			SMMU_TLB_FLUSH_ASID_MATCH_SHIFT)

#define SMMU_PAGE_SHIFT 12
#define SMMU_PAGE_SIZE	(1 << SMMU_PAGE_SHIFT)

#define SMMU_PDIR_COUNT	1024
#define SMMU_PDIR_SIZE	(sizeof(unsigned long) * SMMU_PDIR_COUNT)
#define SMMU_PTBL_COUNT	1024
#define SMMU_PTBL_SIZE	(sizeof(unsigned long) * SMMU_PTBL_COUNT)
#define SMMU_PDIR_SHIFT	12
#define SMMU_PDE_SHIFT	12
#define SMMU_PTE_SHIFT	12
#define SMMU_PFN_MASK	0x000fffff

#define SMMU_ADDR_TO_PTN(addr)	(((addr) >> 12) & (BIT(10) - 1))
#define SMMU_ADDR_TO_PDN(addr)	((addr) >> 22)
#define SMMU_PDN_TO_ADDR(pdn)	((pdn) << 22)

#define _READABLE	(1 << SMMU_PTB_DATA_ASID_READABLE_SHIFT)
#define _WRITABLE	(1 << SMMU_PTB_DATA_ASID_WRITABLE_SHIFT)
#define _NONSECURE	(1 << SMMU_PTB_DATA_ASID_NONSECURE_SHIFT)
#define _PDE_NEXT	(1 << SMMU_PDE_NEXT_SHIFT)
#define _MASK_ATTR	(_READABLE | _WRITABLE | _NONSECURE)

#define _PDIR_ATTR	(_READABLE | _WRITABLE | _NONSECURE)

#define _PDE_ATTR	(_READABLE | _WRITABLE | _NONSECURE)
#define _PDE_ATTR_N	(_PDE_ATTR | _PDE_NEXT)
#define _PDE_VACANT(pdn)	(0)

#define _PTE_ATTR	(_READABLE | _WRITABLE | _NONSECURE)
#define _PTE_VACANT(addr)	(0)

#ifdef	CONFIG_TEGRA_IOMMU_SMMU_LINEAR
#undef	_PDE_VACANT
#undef	_PTE_VACANT
#define	_PDE_VACANT(pdn)	(((pdn) << 10) | _PDE_ATTR)
#define	_PTE_VACANT(addr)	(((addr) >> SMMU_PAGE_SHIFT) | _PTE_ATTR)
#endif

#define SMMU_MK_PDIR(page, attr)	\
		((page_to_phys(page) >> SMMU_PDIR_SHIFT) | (attr))
#define SMMU_MK_PDE(page, attr)		\
		(unsigned long)((page_to_phys(page) >> SMMU_PDE_SHIFT) | (attr))
#define SMMU_EX_PTBL_PAGE(pde)		\
		pfn_to_page((unsigned long)(pde) & SMMU_PFN_MASK)
#define SMMU_PFN_TO_PTE(pfn, attr)	(unsigned long)((pfn) | (attr))

#define SMMU_ASID_ENABLE(asid)	((asid) | (1 << 31))
#define SMMU_ASID_DISABLE	0
#define SMMU_ASID_ASID(n)	((n) & ~SMMU_ASID_ENABLE(0))

#define smmu_client_enable_hwgrp(c, m)	smmu_client_set_hwgrp(c, m, 1)
#define smmu_client_disable_hwgrp(c)	smmu_client_set_hwgrp(c, 0, 0)
#define __smmu_client_enable_hwgrp(c, m) __smmu_client_set_hwgrp(c, m, 1)
#define __smmu_client_disable_hwgrp(c)	__smmu_client_set_hwgrp(c, 0, 0)

static size_t tegra_smmu_get_offset_base(int id)
{
	if (!(id & BIT(5)))
		return SMMU_SWGRP_ASID_BASE;

	return 0x490 - SWGID_DC14 * sizeof(unsigned long);
}

/*
 * Per client for address space
 */
struct smmu_client {
	struct device		*dev;
	struct list_head	list;
	struct smmu_as		*as;
	u64			swgids;
};

/*
 * Per address space
 */
struct smmu_as {
	struct smmu_device	*smmu;	/* back pointer to container */
	unsigned int		asid;
	spinlock_t		lock;	/* for pagetable */
	struct page		*pdir_page;
	unsigned long		pdir_attr;
	unsigned long		pde_attr;
	unsigned long		pte_attr;
	unsigned int		*pte_count;

	struct list_head	client;
	spinlock_t		client_lock; /* for client list */
};

struct smmu_debugfs_info {
	struct smmu_device *smmu;
	int mc;
	int cache;
};

/*
 * Per SMMU device - IOMMU device
 */
struct smmu_device {
	void __iomem	*regs, *regs_ahbarb;
	unsigned long	iovmm_base;	/* remappable base address */
	unsigned long	page_count;	/* total remappable size */
	spinlock_t	lock;
	char		*name;
	struct device	*dev;
	int		num_as;
	struct smmu_as	*as;		/* Run-time allocated array */
	u64		swgids;		/* memory client ID bitmap */
	struct page *avp_vector_page;	/* dummy page shared by all AS's */

	/*
	 * Register image savers for suspend/resume
	 */
	unsigned long translation_enable_0;
	unsigned long translation_enable_1;
	unsigned long translation_enable_2;
	unsigned long asid_security;

	struct dentry *debugfs_root;
	struct smmu_debugfs_info *debugfs_info;

	struct device_node *ahb;
};

static struct smmu_device *smmu_handle; /* unique for a system */

/*
 *	SMMU/AHB register accessors
 */
static inline u32 smmu_read(struct smmu_device *smmu, size_t offs)
{
	return readl(smmu->regs + offs);
}
static inline void smmu_write(struct smmu_device *smmu, u32 val, size_t offs)
{
	writel(val, smmu->regs + offs);
}

static inline u32 ahb_read(struct smmu_device *smmu, size_t offs)
{
	return readl(smmu->regs_ahbarb + offs);
}
static inline void ahb_write(struct smmu_device *smmu, u32 val, size_t offs)
{
	writel(val, smmu->regs_ahbarb + offs);
}

#define VA_PAGE_TO_PA(va, page)	\
	(page_to_phys(page) + ((unsigned long)(va) & ~PAGE_MASK))

#define FLUSH_CPU_DCACHE(va, page, size)	\
	do {	\
		unsigned long _pa_ = VA_PAGE_TO_PA(va, page);		\
		__cpuc_flush_dcache_area((void *)(va), (size_t)(size));	\
		outer_flush_range(_pa_, _pa_+(size_t)(size));		\
	} while (0)

/*
 * Any interaction between any block on PPSB and a block on APB or AHB
 * must have these read-back barriers to ensure the APB/AHB bus
 * transaction is complete before initiating activity on the PPSB
 * block.
 */
#define FLUSH_SMMU_REGS(smmu)	smmu_read(smmu, SMMU_CONFIG)

static u64 tegra_smmu_of_get_swgids(struct device *dev)
{
	size_t bytes;
	const char *propname = "nvidia,memory-clients";
	const __be32 *prop;
	int i;
	u64 swgids = 0;

	prop = of_get_property(dev->of_node, propname, &bytes);
	if (!prop || !bytes)
		return 0;

	for (i = 0; i < bytes / sizeof(u32); i++, prop++)
		swgids |= 1ULL << be32_to_cpup(prop);

	return swgids;
}

static int __smmu_client_set_hwgrp(struct smmu_client *c, u64 map, int on)
{
	int i;
	struct smmu_as *as = c->as;
	u32 val, offs, mask = SMMU_ASID_ENABLE(as->asid);
	struct smmu_device *smmu = as->smmu;

	WARN_ON(!on && map);
	if (on && !map)
		return -EINVAL;
	if (!on)
		map = c->swgids;

	for_each_set_bit(i, (unsigned long *)&map, HWGRP_COUNT) {

		/* FIXME: PCIe client hasn't been registered as IOMMU */
		if (i == SWGID_AFI)
			continue;

		offs = i * sizeof(unsigned long) +
			tegra_smmu_get_offset_base(i);

		val = smmu_read(smmu, offs);
		val &= ~3; /* always overwrite ASID */

		if (on)
			val |= mask;
		else if (list_empty(&c->list))
			val = 0; /* turn off if this is the last */
		else
			return 0; /* leave if off but not the last */

		smmu_write(smmu, val, offs);

		dev_dbg(c->dev, "swgid:%d asid:%d %s @%s\n",
			i, val & 3, (val & BIT(31)) ? "Enabled" : "Disabled",
			__func__);
	}
	FLUSH_SMMU_REGS(smmu);
	c->swgids = map;
	return 0;

}

static int smmu_client_set_hwgrp(struct smmu_client *c, u64 map, int on)
{
	u32 val;
	unsigned long flags;
	struct smmu_as *as = c->as;
	struct smmu_device *smmu = as->smmu;

	spin_lock_irqsave(&smmu->lock, flags);
	val = __smmu_client_set_hwgrp(c, map, on);
	spin_unlock_irqrestore(&smmu->lock, flags);
	return val;
}

/*
 * Flush all TLB entries and all PTC entries
 * Caller must lock smmu
 */
static void smmu_flush_regs(struct smmu_device *smmu, int enable)
{
	u32 val;

	smmu_write(smmu, SMMU_PTC_FLUSH_TYPE_ALL, SMMU_PTC_FLUSH);
	FLUSH_SMMU_REGS(smmu);
	val = SMMU_TLB_FLUSH_VA_MATCH_ALL |
		SMMU_TLB_FLUSH_ASID_MATCH_disable;
	smmu_write(smmu, val, SMMU_TLB_FLUSH);

	if (enable)
		smmu_write(smmu, SMMU_CONFIG_ENABLE, SMMU_CONFIG);
	FLUSH_SMMU_REGS(smmu);
}

static void smmu_setup_regs(struct smmu_device *smmu)
{
	int i;
	u32 val;

	for (i = 0; i < smmu->num_as; i++) {
		struct smmu_as *as = &smmu->as[i];
		struct smmu_client *c;

		smmu_write(smmu, SMMU_PTB_ASID_CUR(as->asid), SMMU_PTB_ASID);
		val = as->pdir_page ?
			SMMU_MK_PDIR(as->pdir_page, as->pdir_attr) :
			SMMU_PTB_DATA_RESET_VAL;
		smmu_write(smmu, val, SMMU_PTB_DATA);

		list_for_each_entry(c, &as->client, list)
			__smmu_client_set_hwgrp(c, c->swgids, 1);
	}

	smmu_write(smmu, smmu->translation_enable_0, SMMU_TRANSLATION_ENABLE_0);
	smmu_write(smmu, smmu->translation_enable_1, SMMU_TRANSLATION_ENABLE_1);
	smmu_write(smmu, smmu->translation_enable_2, SMMU_TRANSLATION_ENABLE_2);
	smmu_write(smmu, smmu->asid_security, SMMU_ASID_SECURITY);
	smmu_write(smmu, SMMU_TLB_CONFIG_RESET_VAL, SMMU_CACHE_CONFIG(_TLB));
	smmu_write(smmu, SMMU_PTC_CONFIG_RESET_VAL, SMMU_CACHE_CONFIG(_PTC));

	smmu_flush_regs(smmu, 1);

	val = ahb_read(smmu, AHB_XBAR_CTRL);
	val |= AHB_XBAR_CTRL_SMMU_INIT_DONE_DONE <<
		AHB_XBAR_CTRL_SMMU_INIT_DONE_SHIFT;
	ahb_write(smmu, val, AHB_XBAR_CTRL);
}


static void smmu_flush_ptc(struct smmu_device *smmu, unsigned long *pte,
			   struct page *page)
{
	u32 val;

	if (pte)
		val = SMMU_PTC_FLUSH_TYPE_ADR | VA_PAGE_TO_PA(pte, page);
	else
		val = SMMU_PTC_FLUSH_TYPE_ALL;

	smmu_write(smmu, val, SMMU_PTC_FLUSH);
	FLUSH_SMMU_REGS(smmu);
}

static inline void smmu_flush_ptc_all(struct smmu_device *smmu)
{
	smmu_flush_ptc(smmu, 0, NULL);
}

static void smmu_flush_tlb(struct smmu_device *smmu, struct smmu_as *as,
			   dma_addr_t iova, int is_pde)
{
	u32 val;

	if (is_pde)
		val = SMMU_TLB_FLUSH_VA(iova, SECTION);
	else
		val = SMMU_TLB_FLUSH_VA(iova, GROUP);

	smmu_write(smmu, val, SMMU_TLB_FLUSH);
	FLUSH_SMMU_REGS(smmu);
}

static inline void smmu_flush_tlb_section(struct smmu_as *as, dma_addr_t iova)
{
	smmu_flush_tlb(as->smmu, as, iova, 1);
}

static void flush_ptc_and_tlb(struct smmu_device *smmu,
			      struct smmu_as *as, dma_addr_t iova,
			      unsigned long *pte, struct page *page, int is_pde)
{
	smmu_flush_ptc(smmu, pte, page);
	smmu_flush_tlb(smmu, as, iova, is_pde);
}

/* Flush PTEs within the same L2 pagetable */
static void __smmu_flush_tlb_range(struct smmu_device *smmu, dma_addr_t iova,
				   dma_addr_t end)
{
	size_t unit = SZ_16K;

	iova = round_down(iova, unit);
	while (iova < end) {
		u32 val;

		val = SMMU_TLB_FLUSH_VA(iova, GROUP);
		smmu_write(smmu, val, SMMU_TLB_FLUSH);
		FLUSH_SMMU_REGS(smmu);

		iova += unit;
	}
}

static void flush_ptc_and_tlb_range(struct smmu_device *smmu,
				    struct smmu_as *as, dma_addr_t iova,
				    unsigned long *pte, struct page *page,
				    size_t count)
{
	size_t unit = SZ_16K;
	dma_addr_t end = iova + count * PAGE_SIZE;

	iova = round_down(iova, unit);
	while (iova < end) {
		u32 val;

		val = SMMU_PTC_FLUSH_TYPE_ADR | VA_PAGE_TO_PA(pte, page);
		smmu_write(smmu, val, SMMU_PTC_FLUSH);
		FLUSH_SMMU_REGS(smmu);
		pte += unit / PAGE_SIZE;

		val = SMMU_TLB_FLUSH_VA(iova, GROUP);
		smmu_write(smmu, val, SMMU_TLB_FLUSH);
		FLUSH_SMMU_REGS(smmu);
		iova += unit;
	}
}

static inline void flush_ptc_and_tlb_all(struct smmu_device *smmu,
					 struct smmu_as *as)
{
	flush_ptc_and_tlb(smmu, as, 0, 0, NULL, 1);
}

static void free_ptbl(struct smmu_as *as, dma_addr_t iova, bool flush)
{
	unsigned long pdn = SMMU_ADDR_TO_PDN(iova);
	unsigned long *pdir = (unsigned long *)page_address(as->pdir_page);

	if (pdir[pdn] != _PDE_VACANT(pdn)) {
		dev_dbg(as->smmu->dev, "pdn: %lx\n", pdn);

		ClearPageReserved(SMMU_EX_PTBL_PAGE(pdir[pdn]));
		__free_page(SMMU_EX_PTBL_PAGE(pdir[pdn]));
		pdir[pdn] = _PDE_VACANT(pdn);
		FLUSH_CPU_DCACHE(&pdir[pdn], as->pdir_page, sizeof pdir[pdn]);
		if (!flush)
			return;

		flush_ptc_and_tlb(as->smmu, as, iova, &pdir[pdn],
				  as->pdir_page, 1);
	}
}

#ifdef CONFIG_TEGRA_ERRATA_1053704
static void smmu_flush_tlb_range(struct smmu_as *as, dma_addr_t iova,
				 dma_addr_t end)
{
	unsigned long *pdir;
	struct smmu_device *smmu = as->smmu;

	if (!pfn_valid(page_to_pfn(as->pdir_page)))
		return;

	pdir = page_address(as->pdir_page);
	while (iova < end) {
		unsigned long pdn = SMMU_ADDR_TO_PDN(iova);

		if (pdir[pdn] & _PDE_NEXT) {
			struct page *page = SMMU_EX_PTBL_PAGE(pdir[pdn]);
			dma_addr_t _end = min_t(dma_addr_t, end,
						SMMU_PDN_TO_ADDR(pdn + 1));

			if (pfn_valid(page_to_pfn(page)))
				__smmu_flush_tlb_range(smmu, iova, _end);

			iova = _end;
		} else {
			if (pdir[pdn])
				smmu_flush_tlb_section(as, iova);

			iova = SMMU_PDN_TO_ADDR(pdn + 1);
		}

		if (pdn == SMMU_PTBL_COUNT - 1)
			break;
	}
}

static void smmu_flush_tlb_as(struct smmu_as *as, dma_addr_t iova,
			      dma_addr_t end)
{
	smmu_flush_tlb_range(as, iova, end);
}
#else
static void __smmu_flush_tlb_as(struct smmu_as *as)
{
	u32 val;
	struct smmu_device *smmu = as->smmu;

	val = SMMU_TLB_FLUSH_ASID_ENABLE |
		as->asid << SMMU_TLB_FLUSH_ASID_SHIFT;
	smmu_write(smmu, val, SMMU_TLB_FLUSH);
	FLUSH_SMMU_REGS(smmu);
}
static inline void smmu_flush_tlb_as(struct smmu_as *as, dma_addr_t iova,
				     dma_addr_t end)
{
	__smmu_flush_tlb_as(as);
}
#endif

static void flush_ptc_and_tlb_as(struct smmu_as *as, dma_addr_t start,
				 dma_addr_t end)
{
	smmu_flush_ptc_all(as->smmu);
	smmu_flush_tlb_as(as, start, end);
}

static void free_pdir(struct smmu_as *as)
{
	unsigned addr;
	int count;
	struct device *dev = as->smmu->dev;

	if (!as->pdir_page)
		return;

	addr = as->smmu->iovmm_base;
	count = as->smmu->page_count;
	while (count-- > 0) {
		free_ptbl(as, addr, 1);
		addr += SMMU_PAGE_SIZE * SMMU_PTBL_COUNT;
	}
	ClearPageReserved(as->pdir_page);
	__free_page(as->pdir_page);
	as->pdir_page = NULL;
	devm_kfree(dev, as->pte_count);
	as->pte_count = NULL;
}

static struct page *alloc_ptbl(struct smmu_as *as, dma_addr_t iova, bool flush)
{
	int i;
	unsigned long *pdir = page_address(as->pdir_page);
	unsigned long pdn = SMMU_ADDR_TO_PDN(iova);
	unsigned long addr = SMMU_PDN_TO_ADDR(pdn);
	struct page *page;
	unsigned long *ptbl;

	/* Vacant - allocate a new page table */
	dev_dbg(as->smmu->dev, "New PTBL pdn: %lx\n", pdn);

	page = alloc_page(GFP_ATOMIC);
	if (!page)
		return NULL;

	SetPageReserved(page);
	ptbl = (unsigned long *)page_address(page);
	for (i = 0; i < SMMU_PTBL_COUNT; i++) {
		ptbl[i] = _PTE_VACANT(addr);
		addr += SMMU_PAGE_SIZE;
	}

	FLUSH_CPU_DCACHE(ptbl, page, SMMU_PTBL_SIZE);
	pdir[pdn] = SMMU_MK_PDE(page, as->pde_attr | _PDE_NEXT);
	FLUSH_CPU_DCACHE(&pdir[pdn], as->pdir_page, sizeof pdir[pdn]);
	if (flush)
		flush_ptc_and_tlb(as->smmu, as, iova, &pdir[pdn],
				  as->pdir_page, 1);
	return page;
}

/*
 * Maps PTBL for given iova and returns the PTE address
 * Caller must unmap the mapped PTBL returned in *ptbl_page_p
 */
static unsigned long *locate_pte(struct smmu_as *as,
				 dma_addr_t iova, bool allocate,
				 struct page **ptbl_page_p,
				 unsigned int **count)
{
	unsigned long ptn = SMMU_ADDR_TO_PTN(iova);
	unsigned long pdn = SMMU_ADDR_TO_PDN(iova);
	unsigned long *pdir = page_address(as->pdir_page);
	unsigned long *ptbl;

	if (pdir[pdn] != _PDE_VACANT(pdn)) {
		/* Mapped entry table already exists */
		*ptbl_page_p = SMMU_EX_PTBL_PAGE(pdir[pdn]);
	} else if (!allocate) {
		return NULL;
	} else {
		*ptbl_page_p = alloc_ptbl(as, iova, 1);
		if (!*ptbl_page_p)
			return NULL;
	}

	ptbl = page_address(*ptbl_page_p);
	*count = &as->pte_count[pdn];
	return &ptbl[ptn];
}

#ifdef CONFIG_SMMU_SIG_DEBUG
static void put_signature(struct smmu_as *as,
			  dma_addr_t iova, unsigned long pfn)
{
	struct page *page;
	unsigned long *vaddr;

	page = pfn_to_page(pfn);
	vaddr = page_address(page);
	if (!vaddr)
		return;

	vaddr[0] = iova;
	vaddr[1] = pfn << PAGE_SHIFT;
	FLUSH_CPU_DCACHE(vaddr, page, sizeof(vaddr[0]) * 2);
}
#else
static inline void put_signature(struct smmu_as *as,
				 unsigned long addr, unsigned long pfn)
{
}
#endif

/*
 * Caller must not hold as->lock
 */
static int alloc_pdir(struct smmu_as *as)
{
	unsigned long *pdir, flags;
	int pdn, err = 0;
	u32 val;
	struct smmu_device *smmu = as->smmu;
	struct page *page;
	unsigned int *cnt;

	/*
	 * do the allocation outside the as lock
	 */
	cnt = devm_kzalloc(smmu->dev,
			   sizeof(cnt[0]) * SMMU_PDIR_COUNT, GFP_KERNEL);
	page = alloc_page(GFP_KERNEL | __GFP_DMA);

	spin_lock_irqsave(&as->lock, flags);

	if (as->pdir_page) {
		/* We raced, free the redundant */
		err = -EAGAIN;
		goto err_out;
	}

	if (!page || !cnt) {
		dev_err(smmu->dev, "failed to allocate at %s\n", __func__);
		err = -ENOMEM;
		goto err_out;
	}

	as->pdir_page = page;
	as->pte_count = cnt;

	SetPageReserved(as->pdir_page);
	pdir = page_address(as->pdir_page);

	for (pdn = 0; pdn < SMMU_PDIR_COUNT; pdn++)
		pdir[pdn] = _PDE_VACANT(pdn);
	FLUSH_CPU_DCACHE(pdir, as->pdir_page, SMMU_PDIR_SIZE);
	val = SMMU_PTC_FLUSH_TYPE_ADR | VA_PAGE_TO_PA(pdir, as->pdir_page);
	smmu_write(smmu, val, SMMU_PTC_FLUSH);
	FLUSH_SMMU_REGS(as->smmu);
	val = SMMU_TLB_FLUSH_VA_MATCH_ALL |
		SMMU_TLB_FLUSH_ASID_MATCH__ENABLE |
		(as->asid << SMMU_TLB_FLUSH_ASID_SHIFT);
	smmu_write(smmu, val, SMMU_TLB_FLUSH);
	FLUSH_SMMU_REGS(as->smmu);

	spin_unlock_irqrestore(&as->lock, flags);

	return 0;

err_out:
	spin_unlock_irqrestore(&as->lock, flags);

	if (page)
		__free_page(page);
	if (cnt)
		devm_kfree(smmu->dev, cnt);
	return err;
}

static size_t __smmu_iommu_unmap_pages(struct smmu_as *as, dma_addr_t iova,
				       size_t bytes)
{
	int total = bytes >> PAGE_SHIFT;
	unsigned long *pdir = page_address(as->pdir_page);
	struct smmu_device *smmu = as->smmu;
	bool flush_all = (total > SZ_512) ? true : false;

	while (total > 0) {
		unsigned long ptn = SMMU_ADDR_TO_PTN(iova);
		unsigned long pdn = SMMU_ADDR_TO_PDN(iova);
		struct page *page = SMMU_EX_PTBL_PAGE(pdir[pdn]);
		unsigned long *ptbl;
		unsigned long *pte;
		int count;

		if (!pfn_valid(page_to_pfn(page))) {
			total -= SMMU_PDN_TO_ADDR(pdn + 1) - iova;
			iova = SMMU_PDN_TO_ADDR(pdn + 1);
			continue;
		}

		ptbl = page_address(page);
		pte = &ptbl[ptn];
		count = min_t(unsigned long, SMMU_PTBL_COUNT - ptn, total);

		dev_dbg(as->smmu->dev, "unmapping %d pages at once\n", count);

		if (pte) {
			unsigned int *rest = &as->pte_count[pdn];
			size_t bytes = sizeof(*pte) * count;
			int i;

			for (i = 0; i < count; i++) {
				if (ptbl[ptn + i] == _PTE_VACANT(iova + i * PAGE_SIZE))
					continue;

				ptbl[ptn + i] = _PTE_VACANT(iova + i * PAGE_SIZE);
				(*rest)--;
			}

			FLUSH_CPU_DCACHE(pte, page, bytes);
			if (!*rest)
				free_ptbl(as, iova, !flush_all);

			if (!flush_all)
				flush_ptc_and_tlb_range(smmu, as, iova, pte,
							page, count);
		}

		iova += PAGE_SIZE * count;
		total -= count;
	}

	if (flush_all)
		flush_ptc_and_tlb_as(as, iova, iova + bytes);

	return bytes;
}

static size_t __smmu_iommu_unmap_largepage(struct smmu_as *as, dma_addr_t iova)
{
	unsigned long pdn = SMMU_ADDR_TO_PDN(iova);
	unsigned long *pdir = (unsigned long *)page_address(as->pdir_page);

	pdir[pdn] = _PDE_VACANT(pdn);
	FLUSH_CPU_DCACHE(&pdir[pdn], as->pdir_page, sizeof pdir[pdn]);
	flush_ptc_and_tlb(as->smmu, as, iova, &pdir[pdn], as->pdir_page, 1);
	return SZ_4M;
}

static int __smmu_iommu_map_pfn(struct smmu_as *as, dma_addr_t iova,
				 unsigned long pfn)
{
	struct smmu_device *smmu = as->smmu;
	unsigned long *pte;
	unsigned int *count;
	struct page *page;

	pte = locate_pte(as, iova, true, &page, &count);
	if (WARN_ON(!pte))
		return -ENOMEM;

	if (*pte == _PTE_VACANT(iova))
		(*count)++;
	*pte = SMMU_PFN_TO_PTE(pfn, as->pte_attr);
	FLUSH_CPU_DCACHE(pte, page, sizeof(*pte));
	flush_ptc_and_tlb(smmu, as, iova, pte, page, 0);
	put_signature(as, iova, pfn);
	return 0;
}

static int __smmu_iommu_map_page(struct smmu_as *as, dma_addr_t iova,
				 phys_addr_t pa)
{
	unsigned long pfn = __phys_to_pfn(pa);

	return __smmu_iommu_map_pfn(as, iova, pfn);
}

static int __smmu_iommu_map_largepage(struct smmu_as *as, dma_addr_t iova,
				 phys_addr_t pa)
{
	unsigned long pdn = SMMU_ADDR_TO_PDN(iova);
	unsigned long *pdir = (unsigned long *)page_address(as->pdir_page);

	if (pdir[pdn] != _PDE_VACANT(pdn))
		return -EINVAL;

	pdir[pdn] = SMMU_ADDR_TO_PDN(pa) << 10 | _PDE_ATTR;
	FLUSH_CPU_DCACHE(&pdir[pdn], as->pdir_page, sizeof pdir[pdn]);
	flush_ptc_and_tlb(as->smmu, as, iova, &pdir[pdn], as->pdir_page, 1);

	return 0;
}

static int smmu_iommu_map(struct iommu_domain *domain, unsigned long iova,
			  phys_addr_t pa, size_t bytes, int prot)
{
	struct smmu_as *as = domain->priv;
	unsigned long flags;
	int err;
	int (*fn)(struct smmu_as *as, dma_addr_t iova, phys_addr_t pa);

	dev_dbg(as->smmu->dev, "[%d] %08lx:%08x\n", as->asid, iova, pa);

	switch (bytes) {
	case SZ_4K:
		fn = __smmu_iommu_map_page;
		break;
	case SZ_4M:
		fn = __smmu_iommu_map_largepage;
		break;
	default:
		WARN(1,  "%d not supported\n", bytes);
		return -EINVAL;
	}

	spin_lock_irqsave(&as->lock, flags);
	err = fn(as, iova, pa);
	spin_unlock_irqrestore(&as->lock, flags);
	return err;
}

static int smmu_iommu_map_pages(struct iommu_domain *domain, unsigned long iova,
				struct page **pages, size_t total, int prot)
{
	struct smmu_as *as = domain->priv;
	struct smmu_device *smmu = as->smmu;
	unsigned long flags;
	unsigned long *pdir = page_address(as->pdir_page);
	int err = 0;
	bool flush_all = (total > SZ_512) ? true : false;

	spin_lock_irqsave(&as->lock, flags);

	while (total > 0) {
		unsigned long pdn = SMMU_ADDR_TO_PDN(iova);
		unsigned long ptn = SMMU_ADDR_TO_PTN(iova);
		unsigned int *rest = &as->pte_count[pdn];
		int count = min_t(unsigned long, SMMU_PTBL_COUNT - ptn, total);
		struct page *tbl_page;
		unsigned long *ptbl;
		unsigned long *pte;
		int i;

		if (pdir[pdn] == _PDE_VACANT(pdn)) {
			tbl_page = alloc_ptbl(as, iova, !flush_all);
			if (!tbl_page) {
				err = -ENOMEM;
				goto out;
			}

		} else {
			tbl_page = SMMU_EX_PTBL_PAGE(pdir[pdn]);
		}

		if (WARN_ON(!pfn_valid(page_to_pfn(tbl_page))))
			goto skip;

		ptbl = page_address(tbl_page);
		for (i = 0; i < count; i++) {
			pte = &ptbl[ptn + i];

			if (*pte == _PTE_VACANT(iova + i * PAGE_SIZE))
				(*rest)++;

			*pte = SMMU_PFN_TO_PTE(page_to_pfn(pages[i]),
					       as->pte_attr);
		}

		pte = &ptbl[ptn];
		FLUSH_CPU_DCACHE(pte, tbl_page,
				 count * sizeof(unsigned long *));
		if (!flush_all)
			flush_ptc_and_tlb_range(smmu, as, iova, pte, tbl_page,
						count);
skip:
		iova += PAGE_SIZE * count;
		total -= count;
		pages += count;
	}

out:
	if (flush_all)
		flush_ptc_and_tlb_as(as, iova, iova + total * PAGE_SIZE);

	spin_unlock_irqrestore(&as->lock, flags);
	return err;
}

static int __smmu_iommu_unmap(struct smmu_as *as, dma_addr_t iova,
	size_t bytes)
{
	unsigned long pdn = SMMU_ADDR_TO_PDN(iova);
	unsigned long *pdir = page_address(as->pdir_page);

	if (!(pdir[pdn] & _PDE_NEXT))
		return __smmu_iommu_unmap_largepage(as, iova);

	return __smmu_iommu_unmap_pages(as, iova, bytes);
}

static size_t smmu_iommu_unmap(struct iommu_domain *domain, unsigned long iova,
			       size_t bytes)
{
	struct smmu_as *as = domain->priv;
	unsigned long flags;
	size_t unmapped;

	dev_dbg(as->smmu->dev, "[%d] %08lx\n", as->asid, iova);

	spin_lock_irqsave(&as->lock, flags);
	unmapped = __smmu_iommu_unmap(as, iova, bytes);
	spin_unlock_irqrestore(&as->lock, flags);
	return unmapped;
}

static phys_addr_t smmu_iommu_iova_to_phys(struct iommu_domain *domain,
					   unsigned long iova)
{
	struct smmu_as *as = domain->priv;
	unsigned long flags;
	unsigned long pdn = SMMU_ADDR_TO_PDN(iova);
	unsigned long *pdir = page_address(as->pdir_page);
	phys_addr_t pa = 0;

	spin_lock_irqsave(&as->lock, flags);

	if (pdir[pdn] & _PDE_NEXT) {
		unsigned long *pte;
		unsigned int *count;
		struct page *page;

		pte = locate_pte(as, iova, false, &page, &count);
		if (pte) {
			unsigned long pfn = *pte & SMMU_PFN_MASK;
			pa = PFN_PHYS(pfn);
		}
	} else {
		pa = pdir[pdn] << SMMU_PDE_SHIFT;
	}

	dev_dbg(as->smmu->dev,
		"iova:%08lx pa:%08x asid:%d\n", iova, pa, as->asid);

	spin_unlock_irqrestore(&as->lock, flags);
	return pa;
}

static int smmu_iommu_domain_has_cap(struct iommu_domain *domain,
				     unsigned long cap)
{
	return 0;
}

static int smmu_iommu_attach_dev(struct iommu_domain *domain,
				 struct device *dev)
{
	struct smmu_as *as = domain->priv;
	struct smmu_device *smmu = as->smmu;
	struct smmu_client *client, *c;
	u64 map;
	int err;
#if 0
	map = tegra_smmu_of_get_swgids(dev);
	if (!map) {
		map = tegra_smmu_fixup_swgids(dev);
		if (!map)
			return -EINVAL;
	}

	map &= smmu->swgids;
#else
	map = smmu->swgids;
#endif
	client = devm_kzalloc(smmu->dev, sizeof(*c), GFP_KERNEL);
	if (!client)
		return -ENOMEM;
	client->dev = dev;
	client->as = as;

	err = smmu_client_enable_hwgrp(client, map);
	if (err)
		goto err_hwgrp;

	spin_lock(&as->client_lock);
	list_for_each_entry(c, &as->client, list) {
		if (c->dev == dev) {
			dev_err(smmu->dev,
				"%s is already attached\n", dev_name(c->dev));
			err = -EINVAL;
			goto err_client;
		}
	}
	list_add(&client->list, &as->client);
	spin_unlock(&as->client_lock);

	/*
	 * Reserve "page zero" for AVP vectors using a common dummy
	 * page.
	 */
	if (map & SWGID(AVPC)) {
		struct page *page;

		page = as->smmu->avp_vector_page;
		__smmu_iommu_map_pfn(as, 0, page_to_pfn(page));

		pr_debug("Reserve \"page zero\" \
			for AVP vectors using a common dummy\n");
	}

	dev_dbg(smmu->dev, "%s is attached\n", dev_name(dev));
	return 0;

err_client:
	smmu_client_disable_hwgrp(client);
	spin_unlock(&as->client_lock);
err_hwgrp:
	devm_kfree(smmu->dev, client);
	return err;
}

static void smmu_iommu_detach_dev(struct iommu_domain *domain,
				  struct device *dev)
{
	struct smmu_as *as = domain->priv;
	struct smmu_device *smmu = as->smmu;
	struct smmu_client *c;

	spin_lock(&as->client_lock);

	list_for_each_entry(c, &as->client, list) {
		if (c->dev == dev) {
			list_del(&c->list);
			smmu_client_disable_hwgrp(c);
			devm_kfree(smmu->dev, c);
			dev_dbg(smmu->dev,
				"%s is detached\n", dev_name(c->dev));
			goto out;
		}
	}
	dev_err(smmu->dev, "Couldn't find %s\n", dev_name(dev));
out:
	spin_unlock(&as->client_lock);
}

#if !defined(CONFIG_TEGRA_IOMMU_SMMU_LINEAR)
static inline void __smmu_iommu_map_linear(struct smmu_as *as,
					   unsigned long start, size_t size)
{
	int i;
	unsigned long count = size >> PAGE_SHIFT;

	for (i = 0; i < count; i++) {
		unsigned long addr;

		addr = start + i * PAGE_SIZE;
		__smmu_iommu_map_pfn(as, addr, __phys_to_pfn(addr));
	}
}

void smmu_iommu_map_linear(unsigned long start, size_t size)
{
	int i;
	struct smmu_device *smmu = smmu_handle;

	for  (i = 0; i < smmu->num_as; i++) {
		struct smmu_as *as;

		as = &smmu->as[i];
		if (!as->pdir_page)
			continue;

		__smmu_iommu_map_linear(as, start, size);

		dev_dbg(smmu->dev, "%s as[%d]: %08lx(%x)\n",
			__func__, i, start, size);
	}
}
EXPORT_SYMBOL_GPL(smmu_iommu_map_linear);
#endif

static int smmu_iommu_domain_init(struct iommu_domain *domain)
{
	int i, err = -EAGAIN;
	unsigned long flags;
	struct smmu_as *as;
	struct smmu_device *smmu = smmu_handle;

	/* Look for a free AS with lock held */
	for  (i = 0; i < smmu->num_as; i++) {
		as = &smmu->as[i];

		if (as->pdir_page)
			continue;

		err = alloc_pdir(as);
		if (!err)
			goto found;

		if (err != -EAGAIN)
			break;
	}
	if (i == smmu->num_as)
		dev_err(smmu->dev,  "no free AS\n");
	return err;

found:
	spin_lock_irqsave(&smmu->lock, flags);

	/* Update PDIR register */
	smmu_write(smmu, SMMU_PTB_ASID_CUR(as->asid), SMMU_PTB_ASID);
	smmu_write(smmu,
		   SMMU_MK_PDIR(as->pdir_page, as->pdir_attr), SMMU_PTB_DATA);
	FLUSH_SMMU_REGS(smmu);

	spin_unlock_irqrestore(&smmu->lock, flags);

	domain->priv = as;

	dev_dbg(smmu->dev, "smmu_as@%p\n", as);
	return 0;
}

static void smmu_iommu_domain_destroy(struct iommu_domain *domain)
{
	struct smmu_as *as = domain->priv;
	struct smmu_device *smmu = as->smmu;
	unsigned long flags;

	spin_lock_irqsave(&as->lock, flags);

	if (as->pdir_page) {
		spin_lock(&smmu->lock);
		smmu_write(smmu, SMMU_PTB_ASID_CUR(as->asid), SMMU_PTB_ASID);
		smmu_write(smmu, SMMU_PTB_DATA_RESET_VAL, SMMU_PTB_DATA);
		FLUSH_SMMU_REGS(smmu);
		spin_unlock(&smmu->lock);

		free_pdir(as);
	}

	if (!list_empty(&as->client)) {
		struct smmu_client *c;

		list_for_each_entry(c, &as->client, list)
			smmu_iommu_detach_dev(domain, c->dev);
	}

	spin_unlock_irqrestore(&as->lock, flags);

	domain->priv = NULL;
	dev_dbg(smmu->dev, "smmu_as@%p\n", as);
}

static struct iommu_ops smmu_iommu_ops = {
	.domain_init	= smmu_iommu_domain_init,
	.domain_destroy	= smmu_iommu_domain_destroy,
	.attach_dev	= smmu_iommu_attach_dev,
	.detach_dev	= smmu_iommu_detach_dev,
	.map		= smmu_iommu_map,
	.map_pages	= smmu_iommu_map_pages,
	.unmap		= smmu_iommu_unmap,
	.iova_to_phys	= smmu_iommu_iova_to_phys,
	.domain_has_cap	= smmu_iommu_domain_has_cap,
	.pgsize_bitmap	= SMMU_IOMMU_PGSIZES,
};

static const char * const smmu_debugfs_mc[] = { "mc", };
static const char * const smmu_debugfs_cache[] = {  "tlb", "ptc", };

static ssize_t smmu_debugfs_stats_write(struct file *file,
					const char __user *buffer,
					size_t count, loff_t *pos)
{
	struct smmu_debugfs_info *info;
	struct smmu_device *smmu;
	struct dentry *dent;
	int i;
	enum {
		_OFF = 0,
		_ON,
		_RESET,
	};
	const char * const command[] = {
		[_OFF]		= "off",
		[_ON]		= "on",
		[_RESET]	= "reset",
	};
	char str[] = "reset";
	u32 val;
	size_t offs;

	count = min_t(size_t, count, sizeof(str));
	if (copy_from_user(str, buffer, count))
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(command); i++)
		if (strncmp(str, command[i],
			    strlen(command[i])) == 0)
			break;

	if (i == ARRAY_SIZE(command))
		return -EINVAL;

	dent = file->f_dentry;
	info = dent->d_inode->i_private;
	smmu = info->smmu;

	offs = SMMU_CACHE_CONFIG(info->cache);
	val = smmu_read(smmu, offs);
	switch (i) {
	case _OFF:
		val &= ~SMMU_CACHE_CONFIG_STATS_ENABLE;
		val &= ~SMMU_CACHE_CONFIG_STATS_TEST;
		smmu_write(smmu, val, offs);
		break;
	case _ON:
		val |= SMMU_CACHE_CONFIG_STATS_ENABLE;
		val &= ~SMMU_CACHE_CONFIG_STATS_TEST;
		smmu_write(smmu, val, offs);
		break;
	case _RESET:
		val |= SMMU_CACHE_CONFIG_STATS_TEST;
		smmu_write(smmu, val, offs);
		val &= ~SMMU_CACHE_CONFIG_STATS_TEST;
		smmu_write(smmu, val, offs);
		break;
	default:
		BUG();
		break;
	}

	dev_dbg(smmu->dev, "%s() %08x, %08x @%08x\n", __func__,
		val, smmu_read(smmu, offs), offs);

	return count;
}

static int smmu_debugfs_stats_show(struct seq_file *s, void *v)
{
	struct smmu_debugfs_info *info;
	struct smmu_device *smmu;
	struct dentry *dent;
	int i;
	const char * const stats[] = { "hit", "miss", };

	dent = d_find_alias(s->private);
	info = dent->d_inode->i_private;
	smmu = info->smmu;

	for (i = 0; i < ARRAY_SIZE(stats); i++) {
		u32 val;
		size_t offs;

		offs = SMMU_STATS_CACHE_COUNT(info->mc, info->cache, i);
		val = smmu_read(smmu, offs);
		seq_printf(s, "%s:%08x ", stats[i], val);

		dev_dbg(smmu->dev, "%s() %s %08x @%08x\n", __func__,
			stats[i], val, offs);
	}
	seq_printf(s, "\n");

	return 0;
}

static int smmu_debugfs_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, smmu_debugfs_stats_show, inode);
}

static const struct file_operations smmu_debugfs_stats_fops = {
	.open		= smmu_debugfs_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= smmu_debugfs_stats_write,
};

static void smmu_debugfs_delete(struct smmu_device *smmu)
{
	debugfs_remove_recursive(smmu->debugfs_root);
	kfree(smmu->debugfs_info);
}

static void smmu_debugfs_create(struct smmu_device *smmu)
{
	int i;
	size_t bytes;
	struct dentry *root;

	bytes = ARRAY_SIZE(smmu_debugfs_mc) * ARRAY_SIZE(smmu_debugfs_cache) *
		sizeof(*smmu->debugfs_info);
	smmu->debugfs_info = kmalloc(bytes, GFP_KERNEL);
	if (!smmu->debugfs_info)
		return;

	root = debugfs_create_dir(dev_name(smmu->dev), NULL);
	if (!root)
		goto err_out;
	smmu->debugfs_root = root;

	for (i = 0; i < ARRAY_SIZE(smmu_debugfs_mc); i++) {
		int j;
		struct dentry *mc;

		mc = debugfs_create_dir(smmu_debugfs_mc[i], root);
		if (!mc)
			goto err_out;

		for (j = 0; j < ARRAY_SIZE(smmu_debugfs_cache); j++) {
			struct dentry *cache;
			struct smmu_debugfs_info *info;

			info = smmu->debugfs_info;
			info += i * ARRAY_SIZE(smmu_debugfs_mc) + j;
			info->smmu = smmu;
			info->mc = i;
			info->cache = j;

			cache = debugfs_create_file(smmu_debugfs_cache[j],
						    S_IWUSR | S_IRUSR, mc,
						    (void *)info,
						    &smmu_debugfs_stats_fops);
			if (!cache)
				goto err_out;
		}
	}

	return;

err_out:
	smmu_debugfs_delete(smmu);
}

static int tegra_smmu_suspend(struct device *dev)
{
	struct smmu_device *smmu = dev_get_drvdata(dev);

	smmu->translation_enable_0 = smmu_read(smmu, SMMU_TRANSLATION_ENABLE_0);
	smmu->translation_enable_1 = smmu_read(smmu, SMMU_TRANSLATION_ENABLE_1);
	smmu->translation_enable_2 = smmu_read(smmu, SMMU_TRANSLATION_ENABLE_2);
	smmu->asid_security = smmu_read(smmu, SMMU_ASID_SECURITY);
	return 0;
}

static int tegra_smmu_resume(struct device *dev)
{
	struct smmu_device *smmu = dev_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&smmu->lock, flags);
	smmu_setup_regs(smmu);
	spin_unlock_irqrestore(&smmu->lock, flags);
	return 0;
}

static int tegra_smmu_probe(struct platform_device *pdev)
{
	struct smmu_device *smmu;
	struct resource *regs, *regs2, *window;
	struct device *dev = &pdev->dev;
	int i, err = 0;

	if (smmu_handle)
		return -EIO;

	BUILD_BUG_ON(PAGE_SHIFT != SMMU_PAGE_SHIFT);

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs2 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	window = tegra_smmu_window(0);
	if (!regs || !regs2 || !window) {
		dev_err(dev, "No SMMU resources\n");
		return -ENODEV;
	}

	smmu = devm_kzalloc(dev, sizeof(*smmu), GFP_KERNEL);
	if (!smmu) {
		dev_err(dev, "failed to allocate smmu_device\n");
		return -ENOMEM;
	}

	smmu->dev = dev;
	smmu->num_as = SMMU_NUM_ASIDS;
	smmu->iovmm_base = (unsigned long)window->start;
	smmu->page_count = (window->end + 1 - window->start) >> SMMU_PAGE_SHIFT;
	smmu->regs = devm_ioremap(dev, regs->start, resource_size(regs));
	smmu->regs_ahbarb = devm_ioremap(dev, regs2->start,
					 resource_size(regs2));
	if (!smmu->regs || !smmu->regs_ahbarb) {
		dev_err(dev, "failed to remap SMMU registers\n");
		err = -ENXIO;
		goto fail;
	}

	if (IS_ENABLED(CONFIG_ARCH_TEGRA_3x_SOC) &&
	    (tegra_get_chipid() == TEGRA_CHIPID_TEGRA3))
		smmu->swgids = 0x00000000000779ff;
	if (IS_ENABLED(CONFIG_ARCH_TEGRA_11x_SOC) &&
	    (tegra_get_chipid() == TEGRA_CHIPID_TEGRA11))
		smmu->swgids = 0x0000000001b659fe;

	smmu->translation_enable_0 = ~0;
	smmu->translation_enable_1 = ~0;
	smmu->translation_enable_2 = ~0;
	smmu->asid_security = 0;

	smmu->as = devm_kzalloc(dev,
			sizeof(smmu->as[0]) * smmu->num_as, GFP_KERNEL);
	if (!smmu->as) {
		dev_err(dev, "failed to allocate smmu_as\n");
		err = -ENOMEM;
		goto fail;
	}

	for (i = 0; i < smmu->num_as; i++) {
		struct smmu_as *as = &smmu->as[i];

		as->smmu = smmu;
		as->asid = i;
		as->pdir_attr = _PDIR_ATTR;
		as->pde_attr = _PDE_ATTR;
		as->pte_attr = _PTE_ATTR;

		spin_lock_init(&as->lock);
		spin_lock_init(&as->client_lock);
		INIT_LIST_HEAD(&as->client);
	}
	spin_lock_init(&smmu->lock);
	smmu_setup_regs(smmu);
	platform_set_drvdata(pdev, smmu);

	smmu->avp_vector_page = alloc_page(GFP_KERNEL);
	if (!smmu->avp_vector_page)
		goto fail;

	smmu_debugfs_create(smmu);
	smmu_handle = smmu;
	bus_set_iommu(&platform_bus_type, &smmu_iommu_ops);
	return 0;

fail:
	if (smmu->avp_vector_page)
		__free_page(smmu->avp_vector_page);
	devm_iounmap(dev, smmu->regs);
	if (smmu->regs_ahbarb)
		devm_iounmap(dev, smmu->regs_ahbarb);
	if (smmu && smmu->as) {
		for (i = 0; i < smmu->num_as; i++) {
			if (smmu->as[i].pdir_page) {
				ClearPageReserved(smmu->as[i].pdir_page);
				__free_page(smmu->as[i].pdir_page);
			}
		}
		devm_kfree(dev, smmu->as);
	}
	devm_kfree(dev, smmu);
	return err;
}

static int tegra_smmu_remove(struct platform_device *pdev)
{
	struct smmu_device *smmu = platform_get_drvdata(pdev);
	struct device *dev = smmu->dev;

	smmu_debugfs_delete(smmu);

	smmu_write(smmu, SMMU_CONFIG_DISABLE, SMMU_CONFIG);
	platform_set_drvdata(pdev, NULL);
	if (smmu->as) {
		int i;

		for (i = 0; i < smmu->num_as; i++)
			free_pdir(&smmu->as[i]);
		devm_kfree(dev, smmu->as);
	}
	if (smmu->avp_vector_page)
		__free_page(smmu->avp_vector_page);
	devm_iounmap(dev, smmu->regs);
	if (smmu->regs_ahbarb)
		devm_iounmap(dev, smmu->regs_ahbarb);
	devm_kfree(dev, smmu);
	smmu_handle = NULL;
	return 0;
}

const struct dev_pm_ops tegra_smmu_pm_ops = {
	.suspend	= tegra_smmu_suspend,
	.resume		= tegra_smmu_resume,
};

static struct platform_driver tegra_smmu_driver = {
	.probe		= tegra_smmu_probe,
	.remove		= tegra_smmu_remove,
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "tegra_smmu",
		.pm	= &tegra_smmu_pm_ops,
	},
};

static int tegra_smmu_device_notifier(struct notifier_block *nb,
				      unsigned long event, void *_dev)
{
	struct dma_iommu_mapping *map;
	struct device *dev = _dev;

	switch (event) {
	case BUS_NOTIFY_BIND_DRIVER:
		if (get_dma_ops(dev) != &arm_dma_ops)
			break;
		/* FALLTHROUGH */
	case BUS_NOTIFY_ADD_DEVICE:
		if (!smmu_handle) {
			dev_warn(dev, "No map yet available\n");
			break;
		}

		map = tegra_smmu_get_map(dev, tegra_smmu_of_get_swgids(dev));
		if (!map)
			break;

		if (arm_iommu_attach_device(dev, map)) {
			arm_iommu_release_mapping(map);
			dev_err(dev, "Failed to attach %s\n", dev_name(dev));
			break;
		}
		dev_dbg(dev, "Attached %s to map %p\n", dev_name(dev), map);
		break;
	case BUS_NOTIFY_DEL_DEVICE:
		if (dev->driver)
			break;
		/* FALLTHROUGH */
	case BUS_NOTIFY_UNBOUND_DRIVER:
		dev_dbg(dev, "Detaching %s from map %p\n", dev_name(dev),
			to_dma_iommu_mapping(dev));
		arm_iommu_detach_device(dev);
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block tegra_smmu_device_nb = {
	.notifier_call = tegra_smmu_device_notifier,
};

static int __devinit tegra_smmu_init(void)
{
	int err;

	err = platform_driver_register(&tegra_smmu_driver);
	if (err)
		return err;
	if (IS_ENABLED(CONFIG_ARM_DMA_USE_IOMMU))
		bus_register_notifier(&platform_bus_type,
				      &tegra_smmu_device_nb);
	return 0;
}

static int tegra_smmu_remove_map(struct device *dev, void *data)
{
	struct dma_iommu_mapping *map = to_dma_iommu_mapping(dev);
	if (map)
		arm_iommu_release_mapping(map);
	return 0;
}

static void __exit tegra_smmu_exit(void)
{
	if (IS_ENABLED(CONFIG_ARM_DMA_USE_IOMMU)) {
		bus_for_each_dev(&platform_bus_type, NULL, NULL,
				 tegra_smmu_remove_map);
		bus_unregister_notifier(&platform_bus_type,
					&tegra_smmu_device_nb);
	}
	platform_driver_unregister(&tegra_smmu_driver);
}

core_initcall(tegra_smmu_init);
module_exit(tegra_smmu_exit);

MODULE_DESCRIPTION("IOMMU API for SMMU in Tegra SoC");
MODULE_AUTHOR("Hiroshi DOYU <hdoyu@nvidia.com>");
MODULE_LICENSE("GPL v2");
