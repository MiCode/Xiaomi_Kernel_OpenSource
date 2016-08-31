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
#include <linux/tegra-ahb.h>
#include <linux/of.h>
#include <linux/of_iommu.h>
#include <linux/tegra-ahb.h>
#include <linux/dma-mapping.h>
#include <linux/bitops.h>
#include <linux/tegra-soc.h>

#include <asm/page.h>
#include <asm/cacheflush.h>
#include <asm/dma-iommu.h>

#include <mach/tegra_smmu.h>
#include <mach/tegra-swgid.h>

/* HACK! This needs to come from device tree */
#include "../../arch/arm/mach-tegra/iomap.h"

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
#define SMMU_CACHE_CONFIG_STATS_ENABLE		(1 << SMMU_CACHE_CONFIG_STATS_SHIFT)
#define SMMU_CACHE_CONFIG_STATS_TEST_SHIFT	30
#define SMMU_CACHE_CONFIG_STATS_TEST		(1 << SMMU_CACHE_CONFIG_STATS_TEST_SHIFT)

#define SMMU_TLB_CONFIG_HIT_UNDER_MISS__ENABLE	(1 << 29)
#define SMMU_TLB_CONFIG_ACTIVE_LINES__VALUE	0x10
#define SMMU_TLB_CONFIG_RESET_VAL		0x20000000
#define SMMU_TLB_RR_ARB				(1 << 28)

#define SMMU_PTC_CONFIG_CACHE__ENABLE		(1 << 29)
#define SMMU_PTC_CONFIG_INDEX_MAP__PATTERN	0x3f
#define SMMU_PTC_CONFIG_RESET_VAL		0x2000003f
#define SMMU_PTC_REQ_LIMIT			(8 << 24)

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
#define SMMU_TLB_FLUSH_ASID_SHIFT_BASE		31
#define SMMU_TLB_FLUSH_ASID_MATCH_DISABLE	0
#define SMMU_TLB_FLUSH_ASID_MATCH_ENABLE	1
#define SMMU_TLB_FLUSH_ASID_MATCH_SHIFT		31
#define SMMU_TLB_FLUSH_ASID_ENABLE					\
	(SMMU_TLB_FLUSH_ASID_MATCH_ENABLE << SMMU_TLB_FLUSH_ASID_MATCH_SHIFT)

#define SMMU_TLB_FLUSH_ASID_SHIFT(as)		\
	(SMMU_TLB_FLUSH_ASID_SHIFT_BASE - __ffs((as)->smmu->num_as))
#define SMMU_ASID_MASK		((1 << __ffs((as)->smmu->num_as)) - 1)

#define SMMU_PTC_FLUSH				0x34
#define SMMU_PTC_FLUSH_TYPE_ALL			0
#define SMMU_PTC_FLUSH_TYPE_ADR			1
#define SMMU_PTC_FLUSH_ADR_SHIFT		4

#define SMMU_PTC_FLUSH_1			0x9b8

#define SMMU_ASID_SECURITY			0x38
#define SMMU_ASID_SECURITY_1			0x3c
#define SMMU_ASID_SECURITY_2			0x9e0
#define SMMU_ASID_SECURITY_3			0x9e4
#define SMMU_ASID_SECURITY_4			0x9e8
#define SMMU_ASID_SECURITY_5			0x9ec
#define SMMU_ASID_SECURITY_6			0x9f0
#define SMMU_ASID_SECURITY_7			0x9f4

#define SMMU_STATS_CACHE_COUNT_BASE		0x1f0

#define SMMU_STATS_CACHE_COUNT(mc, cache, hitmiss)		\
	(SMMU_STATS_CACHE_COUNT_BASE + 8 * cache + 4 * hitmiss)

#define SMMU_TRANSLATION_ENABLE_0		0x228

#define SMMU_AFI_ASID	0x238   /* PCIE */

#define SMMU_SWGRP_ASID_BASE	SMMU_AFI_ASID

#define HWGRP_COUNT	64

#define SMMU_PDE_NEXT_SHIFT		28

/* AHB Arbiter Registers */
#define AHB_XBAR_CTRL				0xe0
#define AHB_XBAR_CTRL_SMMU_INIT_DONE_DONE	1
#define AHB_XBAR_CTRL_SMMU_INIT_DONE_SHIFT	17

#define SMMU_NUM_ASIDS				4
#define SMMU_NUM_ASIDS_TEGRA12			128
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
#define SMMU_PDIR_SIZE	(sizeof(u32) * SMMU_PDIR_COUNT)
#define SMMU_PTBL_COUNT	1024
#define SMMU_PTBL_SIZE	(sizeof(u32) * SMMU_PTBL_COUNT)
#define SMMU_PDIR_SHIFT	12
#define SMMU_PDE_SHIFT	12
#define SMMU_PTE_SHIFT	12
#define SMMU_PFN_MASK	0x0fffffff

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
		(u32)((page_to_phys(page) >> SMMU_PDE_SHIFT) | (attr))
#define SMMU_EX_PTBL_PAGE(pde)		\
		pfn_to_page((u32)(pde) & SMMU_PFN_MASK)
#define SMMU_PFN_TO_PTE(pfn, attr)	(u32)((pfn) | (attr))

#define SMMU_ASID_ENABLE(asid)	((asid) | (1 << 31))
#define SMMU_ASID_DISABLE	0
#define SMMU_ASID_ASID(n)	((n) & ~SMMU_ASID_ENABLE(0))

/* FIXME: client ID, only valid for T124 */
#define CSR_PTCR 0
#define CSR_DISPLAY0A 1
#define CSR_DISPLAY0AB 2
#define CSR_DISPLAY0B 3
#define CSR_DISPLAY0BB 4
#define CSR_DISPLAY0C 5
#define CSR_DISPLAY0CB 6
#define CSR_AFIR 14
#define CSR_AVPCARM7R 15
#define CSR_DISPLAYHC 16
#define CSR_DISPLAYHCB 17
#define CSR_HDAR 21
#define CSR_HOST1XDMAR 22
#define CSR_HOST1XR 23
#define CSR_MSENCSRD 28
#define CSR_PPCSAHBDMAR 29
#define CSR_PPCSAHBSLVR 30
#define CSR_SATAR 31
#define CSR_VDEBSEVR 34
#define CSR_VDEMBER 35
#define CSR_VDEMCER 36
#define CSR_VDETPER 37
#define CSR_MPCORELPR 38
#define CSR_MPCORER 39
#define CSW_MSENCSWR 43
#define CSW_AFIW 49
#define CSW_AVPCARM7W 50
#define CSW_HDAW 53
#define CSW_HOST1XW 54
#define CSW_MPCORELPW 56
#define CSW_MPCOREW 57
#define CSW_PPCSAHBDMAW 59
#define CSW_PPCSAHBSLVW 60
#define CSW_SATAW 61
#define CSW_VDEBSEVW 62
#define CSW_VDEDBGW 63
#define CSW_VDEMBEW 64
#define CSW_VDETPMW 65
#define CSR_ISPRA 68
#define CSW_ISPWA 70
#define CSW_ISPWB 71
#define CSR_XUSB_HOSTR 74
#define CSW_XUSB_HOSTW 75
#define CSR_XUSB_DEVR 76
#define CSW_XUSB_DEVW 77
#define CSR_ISPRAB 78
#define CSW_ISPWAB 80
#define CSW_ISPWBB 81
#define CSR_TSECSRD 84
#define CSW_TSECSWR 85
#define CSR_A9AVPSCR 86
#define CSW_A9AVPSCW 87
#define CSR_GPUSRD 88
#define CSW_GPUSWR 89
#define CSR_DISPLAYT 90
#define CSR_SDMMCRA 96
#define CSR_SDMMCRAA 97
#define CSR_SDMMCR 98
#define CSR_SDMMCRAB 99
#define CSW_SDMMCWA 100
#define CSW_SDMMCWAA 101
#define CSW_SDMMCW 102
#define CSW_SDMMCWAB 103
#define CSR_VICSRD 108
#define CSW_VICSWR 109
#define CSW_VIW 114
#define CSR_DISPLAYD 115

#define SMMU_CLIENT_CONF0	0x40

#define smmu_client_enable_hwgrp(c, m)	smmu_client_set_hwgrp(c, m, 1)
#define smmu_client_disable_hwgrp(c)	smmu_client_set_hwgrp(c, 0, 0)
#define __smmu_client_enable_hwgrp(c, m) __smmu_client_set_hwgrp(c, m, 1)
#define __smmu_client_disable_hwgrp(c)	__smmu_client_set_hwgrp(c, 0, 0)

static struct device *save_smmu_device;

static size_t smmu_flush_all_th_pages = SZ_512; /* number of threshold pages */

static const u32 smmu_asid_security_ofs[] = {
	SMMU_ASID_SECURITY,
	SMMU_ASID_SECURITY_1,
	SMMU_ASID_SECURITY_2,
	SMMU_ASID_SECURITY_3,
	SMMU_ASID_SECURITY_4,
	SMMU_ASID_SECURITY_5,
	SMMU_ASID_SECURITY_6,
	SMMU_ASID_SECURITY_7,
};

static size_t tegra_smmu_get_offset(int id)
{
	switch (id) {
	case SWGID_DC14:
		return 0x490;
	case SWGID_DC12:
		return 0xa88;
	case SWGID_AFI...SWGID_ISP:
	case SWGID_MPE...SWGID_PPCS1:
		return (id - SWGID_AFI) * sizeof(u32) + SMMU_AFI_ASID;
	case SWGID_SDMMC1A...63:
		return (id - SWGID_SDMMC1A) * sizeof(u32) + 0xa94;
	};

	BUG();
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
	u32			pdir_attr;
	u32			pde_attr;
	u32			pte_attr;
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
	u64		swgids;		/* memory client ID bitmap */
	size_t		ptc_cache_size;
	struct page *avp_vector_page;	/* dummy page shared by all AS's */

	/*
	 * Register image savers for suspend/resume
	 */
	int num_translation_enable;
	u32 translation_enable[4];
	int num_asid_security;
	u32 asid_security[8];

	struct dentry *debugfs_root;
	struct smmu_debugfs_info *debugfs_info;

	int		num_as;
	struct smmu_as	as[0];		/* Run-time allocated array */
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

static void __smmu_client_ordered(struct smmu_device *smmu, int id)
{
	size_t offs;
	u32 val;

	offs = SMMU_CLIENT_CONF0;
	offs += (id / BITS_PER_LONG) * sizeof(u32);

	val = smmu_read(smmu, offs);
	val |= BIT(id % BITS_PER_LONG);
	smmu_write(smmu, val, offs);
}

static void smmu_client_ordered(struct smmu_device *smmu)
{
	int i, id[] = {
		/* Add client ID here to be ordered */
	};

	for (i = 0; i < ARRAY_SIZE(id); i++)
		__smmu_client_ordered(smmu, id[i]);
}

#define VA_PAGE_TO_PA(va, page)	\
	(page_to_phys(page) + ((unsigned long)(va) & ~PAGE_MASK))

#define VA_PAGE_TO_PA_HI(va, page)	\
	(u32)((u64)(page_to_phys(page)) >> 32)

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

		offs = tegra_smmu_get_offset(i);
		val = smmu_read(smmu, offs);
		val &= ~SMMU_ASID_MASK; /* always overwrite ASID */

		if (on)
			val |= mask;
		else if (list_empty(&c->list))
			val = 0; /* turn off if this is the last */
		else
			return 0; /* leave if off but not the last */

		smmu_write(smmu, val, offs);

		dev_dbg(c->dev, "swgid:%d asid:%d %s @%s\n",
			i, val & SMMU_ASID_MASK,
			 (val & BIT(31)) ? "Enabled" : "Disabled", __func__);
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

	for (i = 0; i < smmu->num_translation_enable; i++)
		smmu_write(smmu, smmu->translation_enable[i],
			   SMMU_TRANSLATION_ENABLE_0 + i * sizeof(u32));

	for (i = 0; i < smmu->num_asid_security; i++)
		smmu_write(smmu,
			   smmu->asid_security[i], smmu_asid_security_ofs[i]);

	val = SMMU_PTC_CONFIG_RESET_VAL;
	if (IS_ENABLED(CONFIG_ARCH_TEGRA_12x_SOC) &&
	    (tegra_get_chipid() == TEGRA_CHIPID_TEGRA12))
		val |= SMMU_PTC_REQ_LIMIT;

	smmu_write(smmu, val, SMMU_CACHE_CONFIG(_PTC));

	val = SMMU_TLB_CONFIG_RESET_VAL;
	if (IS_ENABLED(CONFIG_ARCH_TEGRA_12x_SOC) &&
	    (tegra_get_chipid() == TEGRA_CHIPID_TEGRA12)) {
		val |= SMMU_TLB_RR_ARB;
		val |= SMMU_TLB_CONFIG_ACTIVE_LINES__VALUE << 1;
	} else {
		val |= SMMU_TLB_CONFIG_ACTIVE_LINES__VALUE;
	}

	smmu_write(smmu, val, SMMU_CACHE_CONFIG(_TLB));

	if (IS_ENABLED(CONFIG_ARCH_TEGRA_12x_SOC) &&
	    (tegra_get_chipid() == TEGRA_CHIPID_TEGRA12))
		smmu_client_ordered(smmu);

	smmu_flush_regs(smmu, 1);

	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA3
			|| tegra_get_chipid() == TEGRA_CHIPID_TEGRA11
			|| tegra_get_chipid() == TEGRA_CHIPID_TEGRA14) {
		val = ahb_read(smmu, AHB_XBAR_CTRL);
		val |= AHB_XBAR_CTRL_SMMU_INIT_DONE_DONE <<
			AHB_XBAR_CTRL_SMMU_INIT_DONE_SHIFT;
		ahb_write(smmu, val, AHB_XBAR_CTRL);
	}
}


static void __smmu_flush_ptc(struct smmu_device *smmu, u32 *pte,
			     struct page *page)
{
	u32 val;

	if (!pte) {
		smmu_write(smmu, SMMU_PTC_FLUSH_TYPE_ALL, SMMU_PTC_FLUSH);
		return;
	}

	if (IS_ENABLED(CONFIG_ARCH_TEGRA_12x_SOC) &&
		(tegra_get_chipid() == TEGRA_CHIPID_TEGRA12)) {
		val = VA_PAGE_TO_PA_HI(pte, page);
		smmu_write(smmu, val, SMMU_PTC_FLUSH_1);
	}

	val = SMMU_PTC_FLUSH_TYPE_ADR | VA_PAGE_TO_PA(pte, page);
	smmu_write(smmu, val, SMMU_PTC_FLUSH);
}

static void smmu_flush_ptc(struct smmu_device *smmu, u32 *pte,
			   struct page *page)
{
	__smmu_flush_ptc(smmu, pte, page);
	FLUSH_SMMU_REGS(smmu);
}

static inline void __smmu_flush_ptc_all(struct smmu_device *smmu)
{
	__smmu_flush_ptc(smmu, 0, NULL);
}

static void __smmu_flush_tlb(struct smmu_device *smmu, struct smmu_as *as,
			   dma_addr_t iova, int is_pde)
{
	u32 val;

	if (is_pde)
		val = SMMU_TLB_FLUSH_VA(iova, SECTION);
	else
		val = SMMU_TLB_FLUSH_VA(iova, GROUP);

	smmu_write(smmu, val, SMMU_TLB_FLUSH);
}

static inline void __smmu_flush_tlb_section(struct smmu_as *as, dma_addr_t iova)
{
	__smmu_flush_tlb(as->smmu, as, iova, 1);
}

static void flush_ptc_and_tlb(struct smmu_device *smmu,
			      struct smmu_as *as, dma_addr_t iova,
			      u32 *pte, struct page *page, int is_pde)
{
	__smmu_flush_ptc(smmu, pte, page);
	__smmu_flush_tlb(smmu, as, iova, is_pde);
	FLUSH_SMMU_REGS(smmu);
}

#ifdef CONFIG_TEGRA_ERRATA_1053704
/* Flush PTEs within the same L2 pagetable */
static void ____smmu_flush_tlb_range(struct smmu_device *smmu, dma_addr_t iova,
				   dma_addr_t end)
{
	size_t unit = SZ_16K;

	iova = round_down(iova, unit);
	while (iova < end) {
		u32 val;

		val = SMMU_TLB_FLUSH_VA(iova, GROUP);
		smmu_write(smmu, val, SMMU_TLB_FLUSH);
		iova += unit;
	}
}
#endif

static void flush_ptc_and_tlb_range(struct smmu_device *smmu,
				    struct smmu_as *as, dma_addr_t iova,
				    u32 *pte, struct page *page,
				    size_t count)
{
	size_t unit = SZ_16K;
	dma_addr_t end = iova + count * PAGE_SIZE;

	iova = round_down(iova, unit);
	while (iova < end) {
		int i;

		__smmu_flush_ptc(smmu, pte, page);
		pte += smmu->ptc_cache_size / PAGE_SIZE;

		for (i = 0; i < smmu->ptc_cache_size / unit; i++) {
			u32 val;

			val = SMMU_TLB_FLUSH_VA(iova, GROUP);
			smmu_write(smmu, val, SMMU_TLB_FLUSH);
			iova += unit;
		}
	}

	FLUSH_SMMU_REGS(smmu);
}

static inline void flush_ptc_and_tlb_all(struct smmu_device *smmu,
					 struct smmu_as *as)
{
	flush_ptc_and_tlb(smmu, as, 0, 0, NULL, 1);
}

static void free_ptbl(struct smmu_as *as, dma_addr_t iova, bool flush)
{
	int pdn = SMMU_ADDR_TO_PDN(iova);
	u32 *pdir = (u32 *)page_address(as->pdir_page);

	if (pdir[pdn] != _PDE_VACANT(pdn)) {
		dev_dbg(as->smmu->dev, "pdn: %x\n", pdn);

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
static void __smmu_flush_tlb_range(struct smmu_as *as, dma_addr_t iova,
				 dma_addr_t end)
{
	u32 *pdir;
	struct smmu_device *smmu = as->smmu;

	if (!pfn_valid(page_to_pfn(as->pdir_page)))
		return;

	pdir = page_address(as->pdir_page);
	while (iova < end) {
		int pdn = SMMU_ADDR_TO_PDN(iova);

		if (pdir[pdn] & _PDE_NEXT) {
			struct page *page = SMMU_EX_PTBL_PAGE(pdir[pdn]);
			dma_addr_t _end = min_t(dma_addr_t, end,
						SMMU_PDN_TO_ADDR(pdn + 1));

			if (pfn_valid(page_to_pfn(page)))
				____smmu_flush_tlb_range(smmu, iova, _end);

			iova = _end;
		} else {
			if (pdir[pdn])
				__smmu_flush_tlb_section(as, iova);

			iova = SMMU_PDN_TO_ADDR(pdn + 1);
		}

		if (pdn == SMMU_PTBL_COUNT - 1)
			break;
	}
}

static void __smmu_flush_tlb_as(struct smmu_as *as, dma_addr_t iova,
			      dma_addr_t end)
{
	__smmu_flush_tlb_range(as, iova, end);
}
#else
static void __smmu_flush_tlb_as(struct smmu_as *as, dma_addr_t iova,
			      dma_addr_t end)
{
	u32 val;
	struct smmu_device *smmu = as->smmu;

	val = SMMU_TLB_FLUSH_ASID_ENABLE |
		(as->asid << SMMU_TLB_FLUSH_ASID_SHIFT(as));
	smmu_write(smmu, val, SMMU_TLB_FLUSH);
}
#endif

static void flush_ptc_and_tlb_as(struct smmu_as *as, dma_addr_t start,
				 dma_addr_t end)
{
	struct smmu_device *smmu = as->smmu;

	__smmu_flush_ptc_all(smmu);
	__smmu_flush_tlb_as(as, start, end);
	FLUSH_SMMU_REGS(smmu);
}

static void free_pdir(struct smmu_as *as)
{
	unsigned long addr;
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
	u32 *pdir = page_address(as->pdir_page);
	int pdn = SMMU_ADDR_TO_PDN(iova);
	unsigned long addr = SMMU_PDN_TO_ADDR(pdn);
	struct page *page;
	u32 *ptbl;
	gfp_t gfp = GFP_ATOMIC;

	if (IS_ENABLED(CONFIG_PREEMPT) && !in_atomic())
		gfp = GFP_KERNEL;

	if (!IS_ENABLED(CONFIG_TEGRA_IOMMU_SMMU_LINEAR))
		gfp |= __GFP_ZERO;

	/* Vacant - allocate a new page table */
	dev_dbg(as->smmu->dev, "New PTBL pdn: %x\n", pdn);

	page = alloc_page(gfp);
	if (!page)
		return NULL;

	SetPageReserved(page);
	ptbl = (u32 *)page_address(page);
	if (IS_ENABLED(CONFIG_TEGRA_IOMMU_SMMU_LINEAR)) {
		for (i = 0; i < SMMU_PTBL_COUNT; i++) {
			ptbl[i] = _PTE_VACANT(addr);
			addr += SMMU_PAGE_SIZE;
		}
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
static u32 *locate_pte(struct smmu_as *as,
				 dma_addr_t iova, bool allocate,
				 struct page **ptbl_page_p,
				 unsigned int **count)
{
	int ptn = SMMU_ADDR_TO_PTN(iova);
	int pdn = SMMU_ADDR_TO_PDN(iova);
	u32 *pdir = page_address(as->pdir_page);
	u32 *ptbl;

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
	u32 *vaddr;

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
	u32 *pdir;
	unsigned long flags;
	int pdn, err = 0;
	u32 val;
	struct smmu_device *smmu = as->smmu;
	struct page *page;
	unsigned int *cnt;

	/*
	 * do the allocation, then grab as->lock
	 */
	cnt = devm_kzalloc(smmu->dev,
			   sizeof(cnt[0]) * SMMU_PDIR_COUNT,
			   GFP_KERNEL);
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
	smmu_flush_ptc(smmu, pdir, as->pdir_page);
	val = SMMU_TLB_FLUSH_VA_MATCH_ALL |
		SMMU_TLB_FLUSH_ASID_MATCH__ENABLE |
		(as->asid << SMMU_TLB_FLUSH_ASID_SHIFT(as));
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
	u32 *pdir = page_address(as->pdir_page);
	struct smmu_device *smmu = as->smmu;
	unsigned long iova_base = iova;
	bool flush_all = (total > smmu_flush_all_th_pages) ? true : false;

	while (total > 0) {
		int ptn = SMMU_ADDR_TO_PTN(iova);
		int pdn = SMMU_ADDR_TO_PDN(iova);
		struct page *page = SMMU_EX_PTBL_PAGE(pdir[pdn]);
		u32 *ptbl;
		u32 *pte;
		int count;

		if (!pfn_valid(page_to_pfn(page))) {
			total -= SMMU_PDN_TO_ADDR(pdn + 1) - iova;
			iova = SMMU_PDN_TO_ADDR(pdn + 1);
			continue;
		}

		ptbl = page_address(page);
		pte = &ptbl[ptn];
		count = min_t(int, SMMU_PTBL_COUNT - ptn, total);

		dev_dbg(as->smmu->dev, "unmapping %d pages at once\n", count);

		if (pte) {
			unsigned int *rest = &as->pte_count[pdn];
			size_t bytes = sizeof(*pte) * count;

			memset(pte, 0, bytes);
			FLUSH_CPU_DCACHE(pte, page, bytes);

			*rest -= count;
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
		flush_ptc_and_tlb_as(as, iova_base,
				     iova_base + bytes);

	return bytes;
}

static size_t __smmu_iommu_unmap_largepage(struct smmu_as *as, dma_addr_t iova)
{
	int pdn = SMMU_ADDR_TO_PDN(iova);
	u32 *pdir = (u32 *)page_address(as->pdir_page);

	pdir[pdn] = _PDE_VACANT(pdn);
	FLUSH_CPU_DCACHE(&pdir[pdn], as->pdir_page, sizeof pdir[pdn]);
	flush_ptc_and_tlb(as->smmu, as, iova, &pdir[pdn], as->pdir_page, 1);
	return SZ_4M;
}

static int __smmu_iommu_map_pfn(struct smmu_as *as, dma_addr_t iova,
				unsigned long pfn, int prot)
{
	struct smmu_device *smmu = as->smmu;
	u32 *pte;
	unsigned int *count;
	struct page *page;
	int attrs = as->pte_attr;

	pte = locate_pte(as, iova, true, &page, &count);
	if (WARN_ON(!pte))
		return -ENOMEM;

	if (*pte == _PTE_VACANT(iova))
		(*count)++;

	if (dma_get_attr(DMA_ATTR_READ_ONLY, (struct dma_attrs *)prot))
		attrs &= ~_WRITABLE;
	else if (dma_get_attr(DMA_ATTR_WRITE_ONLY, (struct dma_attrs *)prot))
		attrs &= ~_READABLE;

	*pte = SMMU_PFN_TO_PTE(pfn, attrs);
	FLUSH_CPU_DCACHE(pte, page, sizeof(*pte));
	flush_ptc_and_tlb(smmu, as, iova, pte, page, 0);
	put_signature(as, iova, pfn);
	return 0;
}

static int __smmu_iommu_map_page(struct smmu_as *as, dma_addr_t iova,
				 phys_addr_t pa, int prot)
{
	unsigned long pfn = __phys_to_pfn(pa);

	return __smmu_iommu_map_pfn(as, iova, pfn, prot);
}

static int __smmu_iommu_map_largepage(struct smmu_as *as, dma_addr_t iova,
				 phys_addr_t pa, int prot)
{
	int pdn = SMMU_ADDR_TO_PDN(iova);
	u32 *pdir = (u32 *)page_address(as->pdir_page);
	int attrs = _PDE_ATTR;

	if (pdir[pdn] != _PDE_VACANT(pdn))
		return -EINVAL;

	if (dma_get_attr(DMA_ATTR_READ_ONLY, (struct dma_attrs *)prot))
		attrs &= ~_WRITABLE;
	else if (dma_get_attr(DMA_ATTR_WRITE_ONLY, (struct dma_attrs *)prot))
		attrs &= ~_READABLE;

	pdir[pdn] = SMMU_ADDR_TO_PDN(pa) << 10 | attrs;
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
	int (*fn)(struct smmu_as *as, dma_addr_t iova, phys_addr_t pa,
		  int prot);

	dev_dbg(as->smmu->dev, "[%d] %08lx:%pa\n", as->asid, iova, &pa);

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
	err = fn(as, iova, pa, prot);
	spin_unlock_irqrestore(&as->lock, flags);
	return err;
}

static int smmu_iommu_map_pages(struct iommu_domain *domain, unsigned long iova,
				struct page **pages, size_t total, int prot)
{
	struct smmu_as *as = domain->priv;
	struct smmu_device *smmu = as->smmu;
	u32 *pdir = page_address(as->pdir_page);
	int err = 0;
	unsigned long iova_base = iova;
	bool flush_all = (total > smmu_flush_all_th_pages) ? true : false;
	int attrs = as->pte_attr;

	if (dma_get_attr(DMA_ATTR_READ_ONLY, (struct dma_attrs *)prot))
		attrs &= ~_WRITABLE;
	else if (dma_get_attr(DMA_ATTR_WRITE_ONLY, (struct dma_attrs *)prot))
		attrs &= ~_READABLE;

	while (total > 0) {
		int pdn = SMMU_ADDR_TO_PDN(iova);
		int ptn = SMMU_ADDR_TO_PTN(iova);
		unsigned int *rest = &as->pte_count[pdn];
		int count = min_t(size_t, SMMU_PTBL_COUNT - ptn, total);
		struct page *tbl_page;
		u32 *ptbl;
		u32 *pte;
		int i;
		unsigned long flags;

		spin_lock_irqsave(&as->lock, flags);

		if (pdir[pdn] == _PDE_VACANT(pdn)) {
			tbl_page = alloc_ptbl(as, iova, !flush_all);
			if (!tbl_page) {
				err = -ENOMEM;
				spin_unlock_irqrestore(&as->lock, flags);
				goto out;
			}

		} else {
			tbl_page = SMMU_EX_PTBL_PAGE(pdir[pdn]);
		}

		ptbl = page_address(tbl_page);
		for (i = 0; i < count; i++) {
			pte = &ptbl[ptn + i];

			if (*pte == _PTE_VACANT(iova + i * PAGE_SIZE))
				(*rest)++;

			*pte = SMMU_PFN_TO_PTE(page_to_pfn(pages[i]), attrs);
		}

		pte = &ptbl[ptn];
		FLUSH_CPU_DCACHE(pte, tbl_page, count * sizeof(u32 *));
		if (!flush_all)
			flush_ptc_and_tlb_range(smmu, as, iova, pte, tbl_page,
						count);

		iova += PAGE_SIZE * count;
		total -= count;
		pages += count;

		spin_unlock_irqrestore(&as->lock, flags);
	}

out:
	if (flush_all)
		flush_ptc_and_tlb_as(as, iova_base,
				     iova_base + total * PAGE_SIZE);
	return err;
}

static int smmu_iommu_map_sg(struct iommu_domain *domain, unsigned long iova,
			     struct scatterlist *sgl, int npages, int prot)
{
	int err = 0;
	unsigned long iova_base = iova;
	bool flush_all = (npages > smmu_flush_all_th_pages) ? true : false;
	struct smmu_as *as = domain->priv;
	u32 *pdir = page_address(as->pdir_page);
	struct smmu_device *smmu = as->smmu;
	int attrs = as->pte_attr;
	size_t total = npages;
	size_t sg_remaining = sgl->length >> PAGE_SHIFT;
	unsigned long sg_pfn = page_to_pfn(sg_page(sgl));

	if (dma_get_attr(DMA_ATTR_READ_ONLY, (struct dma_attrs *)prot))
		attrs &= ~_WRITABLE;
	else if (dma_get_attr(DMA_ATTR_WRITE_ONLY, (struct dma_attrs *)prot))
		attrs &= ~_READABLE;

	while (total > 0) {
		int pdn = SMMU_ADDR_TO_PDN(iova);
		int ptn = SMMU_ADDR_TO_PTN(iova);
		unsigned int *rest = &as->pte_count[pdn];
		int count = min_t(size_t, SMMU_PTBL_COUNT - ptn, total);
		struct page *tbl_page;
		u32 *ptbl;
		u32 *pte;
		int i;
		unsigned long flags;

		spin_lock_irqsave(&as->lock, flags);

		if (pdir[pdn] == _PDE_VACANT(pdn)) {
			tbl_page = alloc_ptbl(as, iova, !flush_all);
			if (!tbl_page) {
				err = -ENOMEM;
				spin_unlock_irqrestore(&as->lock, flags);
				break;
			}

		} else {
			tbl_page = SMMU_EX_PTBL_PAGE(pdir[pdn]);
		}

		ptbl = page_address(tbl_page);
		for (i = 0; i < count; i++) {

			pte = &ptbl[ptn + i];
			if (*pte == _PTE_VACANT(iova + i * PAGE_SIZE))
				(*rest)++;

			*pte = SMMU_PFN_TO_PTE(sg_pfn++, attrs);
			if (--sg_remaining)
				continue;

			sgl = sg_next(sgl);
			if (sgl) {
				sg_pfn = page_to_pfn(sg_page(sgl));
				sg_remaining = sgl->length >> PAGE_SHIFT;
			}
		}

		pte = &ptbl[ptn];
		FLUSH_CPU_DCACHE(pte, tbl_page, count * sizeof(u32 *));
		if (!flush_all)
			flush_ptc_and_tlb_range(smmu, as, iova, pte, tbl_page,
						count);

		iova += PAGE_SIZE * count;
		total -= count;

		spin_unlock_irqrestore(&as->lock, flags);
	}

	if (flush_all)
		flush_ptc_and_tlb_as(as, iova_base,
				     iova_base + npages * PAGE_SIZE);

	return err;
}

static int __smmu_iommu_unmap(struct smmu_as *as, dma_addr_t iova,
	size_t bytes)
{
	int pdn = SMMU_ADDR_TO_PDN(iova);
	u32 *pdir = page_address(as->pdir_page);

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
					   dma_addr_t iova)
{
	struct smmu_as *as = domain->priv;
	unsigned long flags;
	int pdn = SMMU_ADDR_TO_PDN(iova);
	u32 *pdir = page_address(as->pdir_page);
	phys_addr_t pa = 0;

	spin_lock_irqsave(&as->lock, flags);

	if (pdir[pdn] & _PDE_NEXT) {
		u32 *pte;
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

	dev_dbg(as->smmu->dev, "iova:%pa pfn:%pa asid:%d\n",
		&iova, &pa, as->asid);

	spin_unlock_irqrestore(&as->lock, flags);
	return pa;
}

static int smmu_iommu_domain_has_cap(struct iommu_domain *domain,
				     unsigned long cap)
{
	return 0;
}

#if defined(CONFIG_DMA_API_DEBUG) || defined(CONFIG_FTRACE)
char *debug_dma_platformdata(struct device *dev)
{
	static char buf[21];
	struct dma_iommu_mapping *mapping = to_dma_iommu_mapping(dev);
	struct smmu_as *as;
	int asid = -1;

	if (mapping) {
		as = mapping->domain->priv;
		asid = as->asid;
	}

	sprintf(buf, "%d", asid);
	return buf;
}
#endif

static int smmu_iommu_attach_dev(struct iommu_domain *domain,
				 struct device *dev)
{
	struct smmu_as *as = domain->priv;
	struct smmu_device *smmu = as->smmu;
	struct smmu_client *client, *c;
	struct iommu_linear_map *area = NULL;
	u64 map, temp;
	int err;

	map = tegra_smmu_of_get_swgids(dev);
	temp = tegra_smmu_fixup_swgids(dev, &area);

	if (!map && !temp)
		return -ENODEV;

	if (map && temp && map != temp)
		dev_err(dev, "%llx %llx\n", map, temp);

	if (!map)
		map = temp;

	while (area && area->size) {
		DEFINE_DMA_ATTRS(attrs);
		size_t size = PAGE_ALIGN(area->size);

		dma_set_attr(DMA_ATTR_SKIP_IOVA_GAP, &attrs);
		dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
		err = dma_map_linear_attrs(dev, area->start, size, 0, &attrs);
		if (err == DMA_ERROR_CODE)
			dev_err(dev, "Failed IOVA linear map %pa(%x)\n",
				&area->start, size);
		else
			dev_info(dev, "IOVA linear map %pa(%x)\n",
				 &area->start, size);

		area++;
	}

	map &= smmu->swgids;

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
		__smmu_iommu_map_pfn(as, 0, page_to_pfn(page), 0);

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

	domain->geometry.aperture_start = smmu->iovmm_base;
	domain->geometry.aperture_end   = smmu->iovmm_base +
		smmu->page_count * SMMU_PAGE_SIZE - 1;
	domain->geometry.force_aperture = true;

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
	.map_sg		= smmu_iommu_map_sg,
	.unmap		= smmu_iommu_unmap,
	.iova_to_phys	= smmu_iommu_iova_to_phys,
	.domain_has_cap	= smmu_iommu_domain_has_cap,
	.pgsize_bitmap	= SMMU_IOMMU_PGSIZES,
};

/* Should be in the order of enum */
static const char * const smmu_debugfs_mc[] = { "mc", };
static const char * const smmu_debugfs_cache[] = {  "tlb", "ptc", };

static ssize_t smmu_debugfs_stats_write(struct file *file,
					const char __user *buffer,
					size_t count, loff_t *pos)
{
	struct smmu_debugfs_info *info;
	struct smmu_device *smmu;
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

	info = file_inode(file)->i_private;
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
	struct smmu_debugfs_info *info = s->private;
	struct smmu_device *smmu = info->smmu;
	int i;
	const char * const stats[] = { "hit", "miss", };


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
	return single_open(file, smmu_debugfs_stats_show, inode->i_private);
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

	debugfs_create_size_t("flush_all_threshold_pages", S_IWUSR | S_IRUSR,
			      root, &smmu_flush_all_th_pages);
	return;

err_out:
	smmu_debugfs_delete(smmu);
}

int tegra_smmu_suspend(struct device *dev)
{
	int i;
	struct smmu_device *smmu = dev_get_drvdata(dev);

	for (i = 0; i < smmu->num_translation_enable; i++)
		smmu->translation_enable[i] = smmu_read(smmu,
				SMMU_TRANSLATION_ENABLE_0 + i * sizeof(u32));

	for (i = 0; i < smmu->num_asid_security; i++)
		smmu->asid_security[i] =
			smmu_read(smmu, smmu_asid_security_ofs[i]);

	return 0;
}
EXPORT_SYMBOL(tegra_smmu_suspend);

int tegra_smmu_save(void)
{
	return tegra_smmu_suspend(save_smmu_device);
}

struct device *get_smmu_device(void)
{
	return save_smmu_device;
}
EXPORT_SYMBOL(get_smmu_device);

int tegra_smmu_resume(struct device *dev)
{
	struct smmu_device *smmu = dev_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&smmu->lock, flags);
	smmu_setup_regs(smmu);
	spin_unlock_irqrestore(&smmu->lock, flags);
	return 0;
}
EXPORT_SYMBOL(tegra_smmu_resume);

int tegra_smmu_restore(void)
{
	return tegra_smmu_resume(save_smmu_device);
}

static int tegra_smmu_probe(struct platform_device *pdev)
{
	struct smmu_device *smmu;
	struct resource *regs, *regs2, *window;
	struct device *dev = &pdev->dev;
	int i, num_as;
	size_t bytes;

	if (smmu_handle)
		return -EIO;

	BUILD_BUG_ON(PAGE_SHIFT != SMMU_PAGE_SHIFT);

	save_smmu_device = dev;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs2 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	window = tegra_smmu_window(0);
	if (!regs || !regs2 || !window) {
		dev_err(dev, "No SMMU resources\n");
		return -ENODEV;
	}

	num_as = SMMU_NUM_ASIDS;
	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA12)
		num_as = SMMU_NUM_ASIDS_TEGRA12;

	bytes = sizeof(*smmu) + num_as * sizeof(*smmu->as);
	smmu = devm_kzalloc(dev, bytes, GFP_KERNEL);
	if (!smmu) {
		dev_err(dev, "failed to allocate smmu_device\n");
		return -ENOMEM;
	}

	smmu->dev = dev;
	smmu->num_as = num_as;

	smmu->iovmm_base = (unsigned long)window->start;
	smmu->page_count = resource_size(window) >> SMMU_PAGE_SHIFT;
	smmu->regs = devm_ioremap(dev, regs->start, resource_size(regs));
	smmu->regs_ahbarb = devm_ioremap(dev, regs2->start,
					 resource_size(regs2));
	if (!smmu->regs || !smmu->regs_ahbarb) {
		dev_err(dev, "failed to remap SMMU registers\n");
		return -ENXIO;
	}

	if (IS_ENABLED(CONFIG_ARCH_TEGRA_3x_SOC) &&
	    (tegra_get_chipid() == TEGRA_CHIPID_TEGRA3))
		smmu->swgids = 0x00000000000779ff;
	if (IS_ENABLED(CONFIG_ARCH_TEGRA_11x_SOC) &&
	    (tegra_get_chipid() == TEGRA_CHIPID_TEGRA11))
		smmu->swgids = 0x0000000001b659fe;
	if (IS_ENABLED(CONFIG_ARCH_TEGRA_14x_SOC) &&
	    (tegra_get_chipid() == TEGRA_CHIPID_TEGRA14))
		smmu->swgids = 0x0000000001865bfe;
	if (IS_ENABLED(CONFIG_ARCH_TEGRA_12x_SOC) &&
	    (tegra_get_chipid() == TEGRA_CHIPID_TEGRA12)) {
		smmu->swgids = 0x00000001fffecdcf;
		smmu->num_translation_enable = 4;
		smmu->num_asid_security = 8;
		smmu->ptc_cache_size = SZ_32K;
	} else {
		smmu->num_translation_enable = 3;
		smmu->num_asid_security = 1;
		smmu->ptc_cache_size = SZ_16K;
	}

	for (i = 0; i < smmu->num_translation_enable; i++)
		smmu->translation_enable[i] = ~0;

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
		return -ENOMEM;

	smmu_debugfs_create(smmu);
	smmu_handle = smmu;
	bus_set_iommu(&platform_bus_type, &smmu_iommu_ops);

	dev_info(dev, "Loaded Tegra IOMMU driver\n");
	return 0;
}

static int tegra_smmu_remove(struct platform_device *pdev)
{
	struct smmu_device *smmu = platform_get_drvdata(pdev);
	int i;

	smmu_debugfs_delete(smmu);

	smmu_write(smmu, SMMU_CONFIG_DISABLE, SMMU_CONFIG);
	for (i = 0; i < smmu->num_as; i++)
		free_pdir(&smmu->as[i]);
	__free_page(smmu->avp_vector_page);
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
		if (strncmp(dev_name(dev), "tegra_smmu", 10) == 0)
			break;

		if (!smmu_handle) {
			dev_warn(dev, "No map yet available\n");
			break;
		}

		map = tegra_smmu_get_map(dev, tegra_smmu_of_get_swgids(dev));
		if (!map)
			break;

		if (arm_iommu_attach_device(dev, map)) {
			dev_err(dev, "Failed to attach %s\n", dev_name(dev));
			arm_iommu_release_mapping(map);
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

static int tegra_smmu_init(void)
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
