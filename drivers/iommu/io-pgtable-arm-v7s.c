/*
 * CPU-agnostic ARM page table allocator.
 *
 * ARMv7 Short-descriptor format, supporting
 * - Basic memory attributes
 * - Simplified access permissions (AP[2:1] model)
 * - Backwards-compatible TEX remap
 * - Large pages/supersections (if indicated by the caller)
 *
 * Not supporting:
 * - Legacy access permissions (AP[2:0] model)
 *
 * Almost certainly never supporting:
 * - PXN
 * - Domains
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
 *
 * Copyright (C) 2014-2015 ARM Limited
 * Copyright (c) 2014-2015 MediaTek Inc.
 */

#define pr_fmt(fmt)	"arm-v7s io-pgtable: " fmt

#include <linux/atomic.h>
#include <linux/dma-mapping.h>
#include <linux/gfp.h>
#include <linux/iommu.h>
#include <linux/kernel.h>
#include <linux/kmemleak.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include <asm/barrier.h>

#include "io-pgtable.h"

/* Struct accessors */
#define io_pgtable_to_data(x)						\
	container_of((x), struct arm_v7s_io_pgtable, iop)

#define io_pgtable_ops_to_data(x)					\
	io_pgtable_to_data(io_pgtable_ops_to_pgtable(x))

#define MTK_PGTABLE_DEBUG_ENABLED

#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT == 34)
#define ARM_V7S_ADDR_BITS 34
#define ARM_V7S_PHYS_ADDR_BITS 35
#else
#define ARM_V7S_ADDR_BITS		32
#define ARM_V7S_PHYS_ADDR_BITS 34
#endif
#define IOVA_ALIGN(addr)	(addr & (DMA_BIT_MASK(ARM_V7S_ADDR_BITS)))
#define PA_ALIGN(addr)		(addr & (DMA_BIT_MASK(ARM_V7S_PHYS_ADDR_BITS)))

/* 1st 12bits, 2nd 8bits*/
#define _ARM_V7S_LVL_BITS_32BIT(lvl)		(16 - (lvl) * 4)
/* 1st 14bits, 2nd 8bits*/
#define _ARM_V7S_LVL_BITS_34BIT(lvl)		(20 - (lvl) * 6)

/* 1st bit20, 2nd bit12*/
#define ARM_V7S_LVL_SHIFT(lvl)		(32 - (4 + 8 * (lvl)))
#define ARM_V7S_TABLE_SHIFT		10

/* 1st 4096 pgd, 2nd 256 pte */
#define ARM_V7S_PTES_PER_LVL_32BIT(lvl)	(1 << _ARM_V7S_LVL_BITS_32BIT(lvl))
/* 1st 16384 pgd, 2nd 256 pte */
#define ARM_V7S_PTES_PER_LVL_34BIT(lvl)	(1 << _ARM_V7S_LVL_BITS_34BIT(lvl))

/* 1st 16KB pgd object, 2nd 1KB pte object */
#define ARM_V7S_TABLE_SIZE_32BIT(lvl) \
	(ARM_V7S_PTES_PER_LVL_32BIT(lvl) * sizeof(arm_v7s_iopte))
/* 1st 64KB pgd object, 2nd 1KB pte object */
#define ARM_V7S_TABLE_SIZE_34BIT(lvl) \
	(ARM_V7S_PTES_PER_LVL_34BIT(lvl) * sizeof(arm_v7s_iopte))

/* 1st 1MB page size, 2nd 4KB page size*/
#define ARM_V7S_BLOCK_SIZE(lvl)		(1UL << ARM_V7S_LVL_SHIFT(lvl))

/* 32bit, 1st IOVA[31:20], 2nd IOVA[31:12]*/
/* 34bit, 1st IOVA[33:20], 2nd IOVA[33:12]*/
#define ARM_V7S_LVL_MASK(lvl) \
	IOVA_ALIGN((u64)(~0UL << ARM_V7S_LVL_SHIFT(lvl)))

/* 1st descriptor value[31:10]*/
#define ARM_V7S_TABLE_MASK		((u32)(~0U << ARM_V7S_TABLE_SHIFT))

/* 1st IOVA mask 0xfff, 2nd IOVA mask 0xff */
#define _ARM_V7S_IDX_MASK_32BIT(lvl)	(ARM_V7S_PTES_PER_LVL_32BIT(lvl) - 1)
/* 1st IOVA mask 0x3fff, 2nd IOVA mask 0xff */
#define _ARM_V7S_IDX_MASK_34BIT(lvl)	(ARM_V7S_PTES_PER_LVL_34BIT(lvl) - 1)

/* 1st IOVA[31:20] 2nd IOVA[19:12] */
#define ARM_V7S_LVL_IDX_32BIT(addr, lvl)	({ \
	int _l = lvl; \
	((u32)(addr) >> ARM_V7S_LVL_SHIFT(_l)) & _ARM_V7S_IDX_MASK_32BIT(_l); \
})

#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT == 34)
/* 1st IOVA[33:20] 2nd IOVA[19:12] */
#define ARM_V7S_LVL_IDX_34BIT(addr, lvl)	({ \
	int _l = lvl;							\
	(((u64)(addr) & DMA_BIT_MASK(CONFIG_MTK_IOMMU_PGTABLE_EXT)) >> \
	  ARM_V7S_LVL_SHIFT(_l)) & _ARM_V7S_IDX_MASK_34BIT(_l); \
})

#define ARM_V7S_PTES_PER_LVL(lvl)	ARM_V7S_PTES_PER_LVL_34BIT(lvl)
#define ARM_V7S_TABLE_SIZE(lvl)		ARM_V7S_TABLE_SIZE_34BIT(lvl)
#define ARM_V7S_LVL_IDX(addr, lvl)	ARM_V7S_LVL_IDX_34BIT(addr, lvl)
#else
#define ARM_V7S_PTES_PER_LVL(lvl)	ARM_V7S_PTES_PER_LVL_32BIT(lvl)
#define ARM_V7S_TABLE_SIZE(lvl)		ARM_V7S_TABLE_SIZE_32BIT(lvl)
#define ARM_V7S_LVL_IDX(addr, lvl)	ARM_V7S_LVL_IDX_32BIT(addr, lvl)
#endif

/*
 * Large page/supersection entries are effectively a block of 16 page/section
 * entries, along the lines of the LPAE contiguous hint, but all with the
 * same output address. For want of a better common name we'll call them
 * "contiguous" versions of their respective page/section entries here, but
 * noting the distinction (WRT to TLB maintenance) that they represent *one*
 * entry repeated 16 times, not 16 separate entries (as in the LPAE case).
 */
#define ARM_V7S_CONT_PAGES		16

/* PTE type bits: these are all mixed up with XN/PXN bits in most cases */
#define ARM_V7S_PTE_TYPE_TABLE		0x1
#define ARM_V7S_PTE_TYPE_PAGE		0x2
#define ARM_V7S_PTE_TYPE_CONT_PAGE	0x1

#define ARM_V7S_PTE_IS_VALID(pte)	(((pte) & 0x3) != 0)
#define ARM_V7S_PTE_IS_TABLE(pte, lvl) \
	((lvl) == 1 && (((pte) & 0x3) == ARM_V7S_PTE_TYPE_TABLE))

/* Page table bits */
#define ARM_V7S_ATTR_XN(lvl)		BIT(4 * (2 - (lvl)))
#define ARM_V7S_ATTR_B			BIT(2)
#define ARM_V7S_ATTR_C			BIT(3)
#define ARM_V7S_ATTR_NS_TABLE		BIT(3)
#define ARM_V7S_ATTR_NS_SECTION		BIT(19)

#define ARM_V7S_CONT_SECTION		BIT(18)
#define ARM_V7S_CONT_PAGE_XN_SHIFT	15

/*
 * The attribute bits are consistently ordered*, but occupy bits [17:10] of
 * a level 1 PTE vs. bits [11:4] at level 2. Thus we define the individual
 * fields relative to that 8-bit block, plus a total shift relative to the PTE.
 */
#define ARM_V7S_ATTR_SHIFT(lvl)		(16 - (lvl) * 6)
#ifdef CONFIG_MTK_IOMMU_V2
#define ARM_V7S_ATTR_ACP(lvl)		(22 - lvl * 6)
#endif

#define ARM_V7S_ATTR_MASK		0xff
#define ARM_V7S_ATTR_AP0		BIT(0)
#define ARM_V7S_ATTR_AP1		BIT(1)
#define ARM_V7S_ATTR_AP2		BIT(5)
#define ARM_V7S_ATTR_S			BIT(6)
#define ARM_V7S_ATTR_NG			BIT(7)
#define ARM_V7S_TEX_SHIFT		2
#define ARM_V7S_TEX_MASK		0x7
#define ARM_V7S_ATTR_TEX(val)		(((val) & ARM_V7S_TEX_MASK) << ARM_V7S_TEX_SHIFT)

/* MediaTek extend the two bits below for over 4GB mode */
#define ARM_V7S_ATTR_MTK_PA_BIT32	BIT(9)
#define ARM_V7S_ATTR_MTK_PA_BIT33	BIT(4)
#define ARM_V7S_ATTR_MTK_PA_BIT34	BIT(5)

/* *well, except for TEX on level 2 large pages, of course :( */
#define ARM_V7S_CONT_PAGE_TEX_SHIFT	6
#define ARM_V7S_CONT_PAGE_TEX_MASK	(ARM_V7S_TEX_MASK << ARM_V7S_CONT_PAGE_TEX_SHIFT)

/* Simplified access permissions */
#define ARM_V7S_PTE_AF			ARM_V7S_ATTR_AP0
#define ARM_V7S_PTE_AP_UNPRIV		ARM_V7S_ATTR_AP1
#define ARM_V7S_PTE_AP_RDONLY		ARM_V7S_ATTR_AP2

/* Register bits */
#define ARM_V7S_RGN_NC			0
#define ARM_V7S_RGN_WBWA		1
#define ARM_V7S_RGN_WT			2
#define ARM_V7S_RGN_WB			3

#define ARM_V7S_PRRR_TYPE_DEVICE	1
#define ARM_V7S_PRRR_TYPE_NORMAL	2
#define ARM_V7S_PRRR_TR(n, type)	(((type) & 0x3) << ((n) * 2))
#define ARM_V7S_PRRR_DS0		BIT(16)
#define ARM_V7S_PRRR_DS1		BIT(17)
#define ARM_V7S_PRRR_NS0		BIT(18)
#define ARM_V7S_PRRR_NS1		BIT(19)
#define ARM_V7S_PRRR_NOS(n)		BIT((n) + 24)

#define ARM_V7S_NMRR_IR(n, attr)	(((attr) & 0x3) << ((n) * 2))
#define ARM_V7S_NMRR_OR(n, attr)	(((attr) & 0x3) << ((n) * 2 + 16))

#define ARM_V7S_TTBR_S			BIT(1)
#define ARM_V7S_TTBR_NOS		BIT(5)
#define ARM_V7S_TTBR_ORGN_ATTR(attr)	(((attr) & 0x3) << 3)
#define ARM_V7S_TTBR_IRGN_ATTR(attr)					\
	((((attr) & 0x1) << 6) | (((attr) & 0x2) >> 1))

#define ARM_V7S_TCR_PD1			BIT(5)

#ifdef CONFIG_ZONE_DMA32
#define ARM_V7S_TABLE_GFP_DMA GFP_DMA32
#define ARM_V7S_TABLE_SLAB_FLAGS SLAB_CACHE_DMA32
#else
#define ARM_V7S_TABLE_GFP_DMA GFP_DMA
#define ARM_V7S_TABLE_SLAB_FLAGS SLAB_CACHE_DMA
#endif

typedef u32 arm_v7s_iopte;

static bool selftest_running;
#define MTK_IOMMU_CACHE_TRACKING_SUPPORT
#ifdef MTK_IOMMU_CACHE_TRACKING_SUPPORT
static dma_addr_t g_sync_target;
static int g_sync_num;
static unsigned long g_sync_iova;
static int g_sync_lvl;
#endif

struct arm_v7s_io_pgtable {
	struct io_pgtable	iop;

	arm_v7s_iopte		*pgd;
	struct kmem_cache	*l2_tables;
	spinlock_t		split_lock;
};

static bool arm_v7s_pte_is_cont(arm_v7s_iopte pte, int lvl);

static dma_addr_t __arm_v7s_dma_addr(void *pages)
{
	return (dma_addr_t)virt_to_phys(pages);
}

static arm_v7s_iopte paddr_to_iopte(phys_addr_t paddr, int lvl,
				    struct io_pgtable_cfg *cfg)
{
	arm_v7s_iopte pte = paddr & ARM_V7S_LVL_MASK(lvl);

	if (cfg->quirks & IO_PGTABLE_QUIRK_ARM_MTK_4GB) {
		if (paddr & BIT_ULL(32))
			pte |= ARM_V7S_ATTR_MTK_PA_BIT32;
		if (paddr & BIT_ULL(33))
			pte |= ARM_V7S_ATTR_MTK_PA_BIT33;
	}
	return pte;
}

static phys_addr_t iopte_to_paddr(arm_v7s_iopte pte, int lvl,
				  struct io_pgtable_cfg *cfg)
{
	arm_v7s_iopte mask;
	phys_addr_t paddr;

	if (ARM_V7S_PTE_IS_TABLE(pte, lvl))
		mask = ARM_V7S_TABLE_MASK;
	else if (arm_v7s_pte_is_cont(pte, lvl))
		mask = ARM_V7S_LVL_MASK(lvl) * ARM_V7S_CONT_PAGES;
	else
		mask = ARM_V7S_LVL_MASK(lvl);

	paddr = pte & mask;
	if (cfg->quirks & IO_PGTABLE_QUIRK_ARM_MTK_4GB) {
		if (pte & ARM_V7S_ATTR_MTK_PA_BIT32)
			paddr |= BIT_ULL(32);
		if (pte & ARM_V7S_ATTR_MTK_PA_BIT33)
			paddr |= BIT_ULL(33);
	}
	return paddr;
}

static arm_v7s_iopte *iopte_deref(arm_v7s_iopte pte, int lvl,
				  struct arm_v7s_io_pgtable *data)
{
	return phys_to_virt(iopte_to_paddr(pte, lvl, &data->iop.cfg));
}

static void *__arm_v7s_alloc_table(int lvl, gfp_t gfp,
				   struct arm_v7s_io_pgtable *data)
{
	struct io_pgtable_cfg *cfg = &data->iop.cfg;
	struct device *dev = cfg->iommu_dev;
	phys_addr_t phys;
	dma_addr_t dma;
	size_t size = ARM_V7S_TABLE_SIZE(lvl);
	void *table = NULL;

	if (lvl == 1)
		table = (void *)__get_free_pages(
			__GFP_ZERO | ARM_V7S_TABLE_GFP_DMA, get_order(size));
	else if (lvl == 2)
		table = kmem_cache_zalloc(data->l2_tables, gfp);
	phys = virt_to_phys(table);
	if (phys != (arm_v7s_iopte)phys) {
		/* Doesn't fit in PTE */
#ifdef MTK_PGTABLE_DEBUG_ENABLED
		pr_notice("%s, %d, l%d_table phys(0x%lx) > 32bit, virt=0x%lx\n",
			  __func__, __LINE__, lvl, phys,
			  (unsigned long)table);
#endif
#ifdef CONFIG_MTK_IOMMU_V2
		if (lvl == 1)
		goto out_free;
#else
		goto out_free;
#endif
	}
	if (table && !(cfg->quirks & IO_PGTABLE_QUIRK_NO_DMA)) {
		dma = dma_map_single(dev, table, size, DMA_TO_DEVICE);
		if (dma_mapping_error(dev, dma))
			goto out_free;
		/*
		 * We depend on the IOMMU being able to work with any physical
		 * address directly, so if the DMA layer suggests otherwise by
		 * translating or truncating them, that bodes very badly...
		 */
		if (dma != phys)
			goto out_unmap;
	}
		kmemleak_ignore(table);
	return table;

out_unmap:
	dev_err(dev, "Cannot accommodate DMA translation for IOMMU page tables\n");
	dma_unmap_single(dev, dma, size, DMA_TO_DEVICE);
out_free:
	if (lvl == 1)
		free_pages((unsigned long)table, get_order(size));
	else
		kmem_cache_free(data->l2_tables, table);
	return NULL;
}

static void __arm_v7s_free_table(void *table, int lvl,
				 struct arm_v7s_io_pgtable *data)
{
	struct io_pgtable_cfg *cfg = &data->iop.cfg;
	struct device *dev = cfg->iommu_dev;
	size_t size = ARM_V7S_TABLE_SIZE(lvl);

	if (!(cfg->quirks & IO_PGTABLE_QUIRK_NO_DMA))
		dma_unmap_single(dev, __arm_v7s_dma_addr(table), size,
				 DMA_TO_DEVICE);
	if (lvl == 1)
		free_pages((unsigned long)table, get_order(size));
	else
		kmem_cache_free(data->l2_tables, table);
}

static void __arm_v7s_pte_sync(arm_v7s_iopte *ptep, int num_entries,
			       struct io_pgtable_cfg *cfg)
{
	if (cfg->quirks & IO_PGTABLE_QUIRK_NO_DMA)
		return;

	dma_sync_single_for_device(cfg->iommu_dev, __arm_v7s_dma_addr(ptep),
				   num_entries * sizeof(*ptep), DMA_TO_DEVICE);
#ifdef MTK_IOMMU_CACHE_TRACKING_SUPPORT
	if (g_sync_target &&
	    g_sync_target != __arm_v7s_dma_addr(ptep)) {
		pr_notice("%s[WARNING] tgt:0x%lx+%d,cur:0x%lx+%d,iova:0x%lx,lvl:0x%lx\n",
			  __func__, g_sync_target, g_sync_num,
			  __arm_v7s_dma_addr(ptep),
			  num_entries, g_sync_iova, g_sync_lvl);
		WARN_ON(1);
	}
	g_sync_target = 0;
	g_sync_num = 0;
	g_sync_iova = 0;
	g_sync_lvl = 0;
#endif
}
static void __arm_v7s_set_pte(arm_v7s_iopte *ptep, arm_v7s_iopte pte,
			      int num_entries, struct io_pgtable_cfg *cfg)
{
	int i;

	for (i = 0; i < num_entries; i++)
		ptep[i] = pte;

	__arm_v7s_pte_sync(ptep, num_entries, cfg);
}

static arm_v7s_iopte arm_v7s_prot_to_pte(int prot, int lvl,
					 struct io_pgtable_cfg *cfg,
					 phys_addr_t paddr) /* Only for MTK */
{
	bool ap = !(cfg->quirks & IO_PGTABLE_QUIRK_NO_PERMS);
#ifdef CONFIG_MTK_IOMMU_V2
	arm_v7s_iopte pte = ARM_V7S_ATTR_NG;
#else
	arm_v7s_iopte pte = ARM_V7S_ATTR_NG | ARM_V7S_ATTR_S;
#endif

	if (!(prot & IOMMU_MMIO))
		pte |= ARM_V7S_ATTR_TEX(1);
	if (ap) {
		pte |= ARM_V7S_PTE_AF;
		if (!(prot & IOMMU_PRIV))
			pte |= ARM_V7S_PTE_AP_UNPRIV;
		if (!(prot & IOMMU_WRITE))
			pte |= ARM_V7S_PTE_AP_RDONLY;
	}
	pte <<= ARM_V7S_ATTR_SHIFT(lvl);

	if ((prot & IOMMU_NOEXEC) && ap)
		pte |= ARM_V7S_ATTR_XN(lvl);
	if (prot & IOMMU_MMIO)
		pte |= ARM_V7S_ATTR_B;
	else if (prot & IOMMU_CACHE)
		pte |= ARM_V7S_ATTR_B | ARM_V7S_ATTR_C;

#ifdef CONFIG_MTK_IOMMU_V2
	if (WARN_ON(pte & 0x400))
		pr_notice("%s, %d, acp is configured, lvl:%d\n",
			  __func__, __LINE__, lvl);
#endif

	pte |= ARM_V7S_PTE_TYPE_PAGE;
	if (lvl == 1 && (cfg->quirks & IO_PGTABLE_QUIRK_ARM_NS))
		pte |= ARM_V7S_ATTR_NS_SECTION;

	if (cfg->quirks & IO_PGTABLE_QUIRK_ARM_MTK_4GB) {
		if (ARM_V7S_PHYS_ADDR_BITS > 32 &&
		    paddr & BIT_ULL(32))
			pte |= ARM_V7S_ATTR_MTK_PA_BIT32;
		if (ARM_V7S_PHYS_ADDR_BITS > 33 &&
		    paddr & BIT_ULL(33))
			pte |= ARM_V7S_ATTR_MTK_PA_BIT33;
		if (ARM_V7S_PHYS_ADDR_BITS > 34 &&
		    paddr & BIT_ULL(34))
			pte |= ARM_V7S_ATTR_MTK_PA_BIT34;
	}

	return pte;
}

static int arm_v7s_pte_to_prot(arm_v7s_iopte pte, int lvl)
{
	int prot = IOMMU_READ;
	arm_v7s_iopte attr = pte >> ARM_V7S_ATTR_SHIFT(lvl);

	if (!(attr & ARM_V7S_PTE_AP_RDONLY))
		prot |= IOMMU_WRITE;
	if (!(attr & ARM_V7S_PTE_AP_UNPRIV))
		prot |= IOMMU_PRIV;
	if ((attr & (ARM_V7S_TEX_MASK << ARM_V7S_TEX_SHIFT)) == 0)
		prot |= IOMMU_MMIO;
	else if (pte & ARM_V7S_ATTR_C)
		prot |= IOMMU_CACHE;
	if (pte & ARM_V7S_ATTR_XN(lvl))
		prot |= IOMMU_NOEXEC;

	return prot;
}

static arm_v7s_iopte arm_v7s_pte_to_cont(arm_v7s_iopte pte, int lvl)
{
	if (lvl == 1) {
		pte |= ARM_V7S_CONT_SECTION;
	} else if (lvl == 2) {
		arm_v7s_iopte xn = pte & ARM_V7S_ATTR_XN(lvl);
		arm_v7s_iopte tex = pte & ARM_V7S_CONT_PAGE_TEX_MASK;

		pte ^= xn | tex | ARM_V7S_PTE_TYPE_PAGE;
		pte |= (xn << ARM_V7S_CONT_PAGE_XN_SHIFT) |
		       (tex << ARM_V7S_CONT_PAGE_TEX_SHIFT) |
		       ARM_V7S_PTE_TYPE_CONT_PAGE;
	}
	return pte;
}

static arm_v7s_iopte arm_v7s_cont_to_pte(arm_v7s_iopte pte, int lvl)
{
	if (lvl == 1) {
		pte &= ~ARM_V7S_CONT_SECTION;
	} else if (lvl == 2) {
		arm_v7s_iopte xn = pte & BIT(ARM_V7S_CONT_PAGE_XN_SHIFT);
		arm_v7s_iopte tex = pte & (ARM_V7S_CONT_PAGE_TEX_MASK <<
					   ARM_V7S_CONT_PAGE_TEX_SHIFT);

		pte ^= xn | tex | ARM_V7S_PTE_TYPE_CONT_PAGE;
		pte |= (xn >> ARM_V7S_CONT_PAGE_XN_SHIFT) |
		       (tex >> ARM_V7S_CONT_PAGE_TEX_SHIFT) |
		       ARM_V7S_PTE_TYPE_PAGE;
	}
	return pte;
}

static bool arm_v7s_pte_is_cont(arm_v7s_iopte pte, int lvl)
{
	if (lvl == 1 && !ARM_V7S_PTE_IS_TABLE(pte, lvl))
		return pte & ARM_V7S_CONT_SECTION;
	else if (lvl == 2)
		return !(pte & ARM_V7S_PTE_TYPE_PAGE);
	return false;
}

static size_t __arm_v7s_unmap(struct arm_v7s_io_pgtable *, unsigned long,
			      size_t, int, arm_v7s_iopte *);

static int arm_v7s_init_pte(struct arm_v7s_io_pgtable *data,
			    unsigned long iova, phys_addr_t paddr, int prot,
			    int lvl, int num_entries, arm_v7s_iopte *ptep)
{
	struct io_pgtable_cfg *cfg = &data->iop.cfg;
	arm_v7s_iopte pte;
	int i;

	for (i = 0; i < num_entries; i++)
		if (ARM_V7S_PTE_IS_TABLE(ptep[i], lvl)) {
			/*
			 * We need to unmap and free the old table before
			 * overwriting it with a block entry.
			 */
			arm_v7s_iopte *tblp;
			size_t sz = ARM_V7S_BLOCK_SIZE(lvl);

			tblp = ptep - ARM_V7S_LVL_IDX(iova, lvl);
#ifdef MTK_PGTABLE_DEBUG_ENABLED
			pr_notice("%s, %d, unmap before over writing, iova=0x%lx, new paddr=0x%lx, ptep=0x%lx, size=0x%lx, level=%d\n",
				__func__, __LINE__,
				iova + i * sz, paddr,
				(unsigned long)tblp, sz, lvl);
#endif
			if (WARN_ON(__arm_v7s_unmap(data, iova + i * sz,
						    sz, lvl, tblp) != sz))
				return -EINVAL;
		} else if (ptep[i]) {
			/* We require an unmap first */
#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT == 34)
			size_t sz = ARM_V7S_BLOCK_SIZE(lvl);

			pr_debug("%s, %d, invalid ptep of %d, iova=0x%lx, paddr=0x%lx, ptep=0x%lx, size=0x%lx, level=%d\n",
				__func__, __LINE__, i,
				iova + i * sz, paddr, ptep[i], sz, lvl);
#else
			WARN_ON(!selftest_running);
			return -EEXIST;
#endif
		}

	pte = arm_v7s_prot_to_pte(prot, lvl, cfg, paddr);
	if (num_entries > 1)
		pte = arm_v7s_pte_to_cont(pte, lvl);

	pte |= paddr_to_iopte(paddr, lvl, cfg);

#if 0 //def MTK_PGTABLE_DEBUG_ENABLED
	pr_notice("%s, %d, iova=0x%lx, paddr=0x%lx, ptep=0x%lx, pte=0x%lx, num=%d, lvl=%d\n",
		__func__, __LINE__, iova, paddr, ptep, pte, num_entries, lvl);
#endif
	__arm_v7s_set_pte(ptep, pte, num_entries, cfg);
	return 0;
}

static arm_v7s_iopte arm_v7s_install_table(arm_v7s_iopte *table,
					   arm_v7s_iopte *ptep,
					   arm_v7s_iopte curr,
					   struct io_pgtable_cfg *cfg)
{
	arm_v7s_iopte old, new;

	new = virt_to_phys(table) | ARM_V7S_PTE_TYPE_TABLE;
	if (cfg->quirks & IO_PGTABLE_QUIRK_ARM_NS)
		new |= ARM_V7S_ATTR_NS_TABLE;

	/*
	 * Ensure the table itself is visible before its PTE can be.
	 * Whilst we could get away with cmpxchg64_release below, this
	 * doesn't have any ordering semantics when !CONFIG_SMP.
	 */
	dma_wmb();

	old = cmpxchg_relaxed(ptep, curr, new);
	__arm_v7s_pte_sync(ptep, 1, cfg);

	return old;
}

static int __arm_v7s_map(struct arm_v7s_io_pgtable *data, unsigned long iova,
			 phys_addr_t paddr, size_t size, int prot,
			 int lvl, arm_v7s_iopte *ptep)
{
	struct io_pgtable_cfg *cfg = &data->iop.cfg;
	arm_v7s_iopte pte, *cptep;
	int num_entries = size >> ARM_V7S_LVL_SHIFT(lvl);
#ifdef CONFIG_MTK_IOMMU_V2
	phys_addr_t pte_phys;
#endif

	/* Find our entry at the current level */
	ptep += ARM_V7S_LVL_IDX(iova, lvl);
#ifdef MTK_IOMMU_CACHE_TRACKING_SUPPORT
	if (num_entries) {
		if (g_sync_target &&
		    g_sync_target != __arm_v7s_dma_addr(ptep)) {
			pr_notice(
				  "%s[WARNING] tgt:0x%lx+%d,iova:0x%lx,lvl:0x%lx\n",
				  __func__, g_sync_target, g_sync_num,
				  g_sync_iova, g_sync_lvl);
			pr_notice(
				  "%s[WARNING] cur:0x%lx+%d,iova:0x%lx,lvl:0x%lx,size:0x%lx\n",
				  __func__, __arm_v7s_dma_addr(ptep),
				 num_entries, iova, lvl, size);
		}
		g_sync_target = __arm_v7s_dma_addr(ptep);
		g_sync_num = num_entries;
		g_sync_iova = iova;
		g_sync_lvl = lvl;
	}
#endif

	/* If we can install a leaf entry at this level, then do so */
	if (num_entries)
		return arm_v7s_init_pte(data, iova, paddr, prot,
					lvl, num_entries, ptep);

	/* We can't allocate tables at the final level */
	if (WARN_ON(lvl == 2))
		return -EINVAL;

	/* Grab a pointer to the next level */
	pte = READ_ONCE(*ptep);
	if (!pte) {
		cptep = __arm_v7s_alloc_table(lvl + 1, GFP_ATOMIC, data);
		if (!cptep) {
#ifdef MTK_PGTABLE_DEBUG_ENABLED
			pr_notice("%s, %d, error cptep\n", __func__, __LINE__);
#endif
			return -ENOMEM;
		}
#ifdef CONFIG_MTK_IOMMU_V2
		if (lvl == 1 &&
		    cfg->quirks & IO_PGTABLE_QUIRK_ARM_MTK_4GB) {
			pte_phys = virt_to_phys(cptep);
			if (ARM_V7S_PHYS_ADDR_BITS > 32 &&
			    pte_phys & BIT_ULL(32))
				*ptep |= ARM_V7S_ATTR_MTK_PA_BIT32;
			if (ARM_V7S_PHYS_ADDR_BITS > 33 &&
			    pte_phys & BIT_ULL(33))
				*ptep |= ARM_V7S_ATTR_MTK_PA_BIT33;
			if (ARM_V7S_PHYS_ADDR_BITS > 34 &&
			    pte_phys & BIT_ULL(34))
				*ptep |= ARM_V7S_ATTR_MTK_PA_BIT34;
		}
#endif
		pte = arm_v7s_install_table(cptep, ptep, 0, cfg);
		if (pte)
			__arm_v7s_free_table(cptep, lvl + 1, data);
	} else {
		/* We've no easy way of knowing if it's synced yet, so... */
		__arm_v7s_pte_sync(ptep, 1, cfg);
	}

	if (ARM_V7S_PTE_IS_TABLE(pte, lvl)) {
		cptep = iopte_deref(pte, lvl, data);
	} else if (pte) {
		/* We require an unmap first */
		WARN_ON(!selftest_running);
		return -EEXIST;
	}

	/* Rinse, repeat */
	return __arm_v7s_map(data, iova, paddr, size, prot, lvl + 1, cptep);
}

static int arm_v7s_map(struct io_pgtable_ops *ops, unsigned long iova,
			phys_addr_t paddr, size_t size, int prot)
{
	struct arm_v7s_io_pgtable *data = io_pgtable_ops_to_data(ops);
	struct io_pgtable *iop = &data->iop;
	int ret;

	/* If no access, then nothing to do */
	if (!(prot & (IOMMU_READ | IOMMU_WRITE))) {
		pr_notice("%s, %d, err prot:0x%x\n",
			  __func__, __LINE__, prot);
		return 0;
	}

	if (WARN_ON(iova >= 1ULL << data->iop.cfg.ias)) {
		pr_notice("%s, %d, err iova:0x%lx, ias=%d\n",
			  __func__, __LINE__, iova, data->iop.cfg.ias);
		return -ERANGE;
	}

	if (WARN_ON(paddr >= (1ULL << data->iop.cfg.oas))) {
		pr_notice("%s, %d, err paddr:0x%lx, oas=%d, quirks=0x%lx\n",
			  __func__, __LINE__, paddr,
			  data->iop.cfg.oas, iop->cfg.quirks);
		return -ERANGE;
	}

	if (WARN_ON(!PA_ALIGN(paddr))) {
		pr_notice("%s, %d, err paddr:0x%lx\n",
			  __func__, __LINE__, paddr);
		return -ERANGE;
	}

	iova = IOVA_ALIGN(iova);
	paddr = PA_ALIGN(paddr);
	ret = __arm_v7s_map(data, iova, paddr, size, prot, 1, data->pgd);
	/*
	 * Synchronise all PTE updates for the new mapping before there's
	 * a chance for anything to kick off a table walk for the new iova.
	 */
	if (iop->cfg.quirks & IO_PGTABLE_QUIRK_TLBI_ON_MAP) {
		io_pgtable_tlb_add_flush(iop, iova, size,
					 ARM_V7S_BLOCK_SIZE(2), false);
		io_pgtable_tlb_sync(iop);
	} else {
		wmb();
	}

	return ret;
}

static void arm_v7s_free_pgtable(struct io_pgtable *iop)
{
	struct arm_v7s_io_pgtable *data = io_pgtable_to_data(iop);
	int i;

	for (i = 0; i < ARM_V7S_PTES_PER_LVL(1); i++) {
		arm_v7s_iopte pte = data->pgd[i];

		if (ARM_V7S_PTE_IS_TABLE(pte, 1))
			__arm_v7s_free_table(iopte_deref(pte, 1, data),
					     2, data);
	}
	__arm_v7s_free_table(data->pgd, 1, data);
	kmem_cache_destroy(data->l2_tables);
	kfree(data);
}

static arm_v7s_iopte arm_v7s_split_cont(struct arm_v7s_io_pgtable *data,
					unsigned long iova, int idx, int lvl,
					arm_v7s_iopte *ptep)
{
	struct io_pgtable *iop = &data->iop;
	arm_v7s_iopte pte;
	size_t size = ARM_V7S_BLOCK_SIZE(lvl);
	int i;

	/* Check that we didn't lose a race to get the lock */
	pte = *ptep;
	if (!arm_v7s_pte_is_cont(pte, lvl))
		return pte;

	ptep -= idx & (ARM_V7S_CONT_PAGES - 1);
	pte = arm_v7s_cont_to_pte(pte, lvl);
	for (i = 0; i < ARM_V7S_CONT_PAGES; i++)
		ptep[i] = pte + i * size;

	__arm_v7s_pte_sync(ptep, ARM_V7S_CONT_PAGES, &iop->cfg);

	size *= ARM_V7S_CONT_PAGES;
	io_pgtable_tlb_add_flush(iop, iova, size, size, true);
	io_pgtable_tlb_sync(iop);
	return pte;
}

static size_t arm_v7s_split_blk_unmap(struct arm_v7s_io_pgtable *data,
				      unsigned long iova, size_t size,
				      arm_v7s_iopte blk_pte,
				      arm_v7s_iopte *ptep)
{
	struct io_pgtable_cfg *cfg = &data->iop.cfg;
	arm_v7s_iopte pte, *tablep;
	int i, unmap_idx, num_entries, num_ptes;

	tablep = __arm_v7s_alloc_table(2, GFP_ATOMIC, data);
	if (!tablep)
		return 0; /* Bytes unmapped */

	num_ptes = ARM_V7S_PTES_PER_LVL(2);
	num_entries = size >> ARM_V7S_LVL_SHIFT(2);
	unmap_idx = ARM_V7S_LVL_IDX(iova, 2);

	pte = arm_v7s_prot_to_pte(arm_v7s_pte_to_prot(blk_pte, 1), 2, cfg, 0);
	if (num_entries > 1)
		pte = arm_v7s_pte_to_cont(pte, 2);

	for (i = 0; i < num_ptes; i += num_entries, pte += size) {
		/* Unmap! */
		if (i == unmap_idx)
			continue;

#ifdef MTK_PGTABLE_DEBUG_ENABLED
		pr_notice("%s, %d, iova=0x%lx, ptep=0x%lx, pte=0x%lx, num=%d\n",
			__func__, __LINE__, iova, (unsigned long)&tablep[i],
			(unsigned long)pte, num_entries);
#endif
		__arm_v7s_set_pte(&tablep[i], pte, num_entries, cfg);
	}

	pte = arm_v7s_install_table(tablep, ptep, blk_pte, cfg);
	if (pte != blk_pte) {
		__arm_v7s_free_table(tablep, 2, data);

		if (!ARM_V7S_PTE_IS_TABLE(pte, 1))
			return 0;

		tablep = iopte_deref(pte, 1, data);
#ifdef MTK_PGTABLE_DEBUG_ENABLED
		pr_notice("%s, %d, unmap when install failed, iova=0x%lx, ptep=0x%lx, size=0x%lx, level=2\n",
			__func__, __LINE__, iova, (uintptr_t)tablep, size);
#endif
		return __arm_v7s_unmap(data, iova, size, 2, tablep);
	}

	io_pgtable_tlb_add_flush(&data->iop, iova, size, size, true);
	return size;
}

static size_t __arm_v7s_unmap(struct arm_v7s_io_pgtable *data,
			      unsigned long iova, size_t size, int lvl,
			      arm_v7s_iopte *ptep)
{
	arm_v7s_iopte pte[ARM_V7S_CONT_PAGES];
	struct io_pgtable *iop = &data->iop;
#ifdef MTK_PGTABLE_DEBUG_ENABLED
	unsigned long iova_temp = iova;
	size_t blk_size1 = 0;
#endif
	int idx, i = 0, num_entries = size >> ARM_V7S_LVL_SHIFT(lvl);

	/* Something went horribly wrong and we ran out of page table */
	if (WARN_ON(lvl > 2))
		return 0;

	idx = ARM_V7S_LVL_IDX(iova, lvl);
	ptep += idx;
	do {
		pte[i] = READ_ONCE(ptep[i]);
#ifdef MTK_PGTABLE_DEBUG_ENABLED
		blk_size1 = ARM_V7S_BLOCK_SIZE(lvl);
#endif
		if (WARN_ON(!ARM_V7S_PTE_IS_VALID(pte[i]))) {

#ifdef MTK_PGTABLE_DEBUG_ENABLED
			pr_notice("%s, %d, err pte, iova=0x%lx, ptep=0x%lx, i=%d, pte=0x%lx, lvl=%d\n",
				__func__, __LINE__, iova_temp,
				(unsigned long)&ptep[i], i,
				(unsigned long)pte[i], lvl);
			iop->ops.iova_to_phys(&iop->ops, iova_temp);
#endif
			return 0;
		}
#ifdef MTK_PGTABLE_DEBUG_ENABLED
		iova_temp += blk_size1;
#endif
	} while (++i < num_entries);

	/*
	 * If we've hit a contiguous 'large page' entry at this level, it
	 * needs splitting first, unless we're unmapping the whole lot.
	 *
	 * For splitting, we can't rewrite 16 PTEs atomically, and since we
	 * can't necessarily assume TEX remap we don't have a software bit to
	 * mark live entries being split. In practice (i.e. DMA API code), we
	 * will never be splitting large pages anyway, so just wrap this edge
	 * case in a lock for the sake of correctness and be done with it.
	 */
	if (num_entries <= 1 && arm_v7s_pte_is_cont(pte[0], lvl)) {
		unsigned long flags;

		spin_lock_irqsave(&data->split_lock, flags);
		pte[0] = arm_v7s_split_cont(data, iova, idx, lvl, ptep);
		spin_unlock_irqrestore(&data->split_lock, flags);
	}

#ifdef MTK_IOMMU_CACHE_TRACKING_SUPPORT
	if (num_entries) {
		if (g_sync_target &&
		    g_sync_target != __arm_v7s_dma_addr(ptep)) {
			pr_notice(
				  "%s[WARNING] tgt:0x%lx+%d,iova:0x%lx,lvl:0x%lx\n",
				  __func__, g_sync_target, g_sync_num,
				  g_sync_iova, g_sync_lvl);
			pr_notice(
				  "%s[WARNING] cur:0x%lx+%d,iova:0x%lx,lvl:0x%lx,size:0x%lx\n",
				  __func__, __arm_v7s_dma_addr(ptep),
				 num_entries, iova, lvl, size);
		}
		g_sync_target = __arm_v7s_dma_addr(ptep);
		g_sync_num = num_entries;
		g_sync_iova = iova;
		g_sync_lvl = lvl;
	}
#endif
	/* If the size matches this level, we're in the right place */
	if (num_entries) {
		size_t blk_size = ARM_V7S_BLOCK_SIZE(lvl);
#if 0 //def MTK_PGTABLE_DEBUG_ENABLED
		pr_notice("%s, %d,clear pte, iova=0x%lx, ptep=0x%lx, old pte=0x%lx, num=%d\n",
			__func__, __LINE__, iova,
			ptep, READ_ONCE(*ptep), num_entries);
		iop->ops.iova_to_phys(&iop->ops, iova);
#endif

		__arm_v7s_set_pte(ptep, 0, num_entries, &iop->cfg);

		for (i = 0; i < num_entries; i++) {
			if (ARM_V7S_PTE_IS_TABLE(pte[i], lvl)) {
				/* Also flush any partial walks */
				io_pgtable_tlb_add_flush(iop, iova, blk_size,
					ARM_V7S_BLOCK_SIZE(lvl + 1), false);
				io_pgtable_tlb_sync(iop);
				ptep = iopte_deref(pte[i], lvl, data);
				__arm_v7s_free_table(ptep, lvl + 1, data);
			} else {
				io_pgtable_tlb_add_flush(iop, iova, blk_size,
							 blk_size, true);
			}
			iova += blk_size;
		}
		return size;
	} else if (lvl == 1 && !ARM_V7S_PTE_IS_TABLE(pte[0], lvl)) {
		/*
		 * Insert a table at the next level to map the old region,
		 * minus the part we want to unmap
		 */
		return arm_v7s_split_blk_unmap(data, iova, size, pte[0], ptep);
	}

	/* Keep on walkin' */
	ptep = iopte_deref(pte[0], lvl, data);
	return __arm_v7s_unmap(data, iova, size, lvl + 1, ptep);
}

static size_t arm_v7s_unmap(struct io_pgtable_ops *ops, unsigned long iova,
			    size_t size)
{
	struct arm_v7s_io_pgtable *data = io_pgtable_ops_to_data(ops);

	if (WARN_ON(iova >= (1ULL << data->iop.cfg.ias)))
		return 0;

	iova = IOVA_ALIGN(iova);
	return __arm_v7s_unmap(data, iova, size, 1, data->pgd);
}

#ifdef CONFIG_MTK_IOMMU_V2

static int arm_v7s_set_acp(struct arm_v7s_io_pgtable *data,
			unsigned long iova, bool is_acp, unsigned long *size)
{
	struct io_pgtable_cfg *cfg = &data->iop.cfg;
	arm_v7s_iopte *ptep = data->pgd, pte;
	int lvl = 0;
	unsigned int mask, enabled = 0;
	arm_v7s_iopte *ptep_curr;
	dma_addr_t dma_addr;

	/* get target pte of iova */
	do {
		ptep += ARM_V7S_LVL_IDX(iova, ++lvl);
		ptep_curr = ptep;
		pte = READ_ONCE(*ptep);
		ptep = iopte_deref(pte, lvl, data);
	} while (ARM_V7S_PTE_IS_TABLE(pte, lvl));

	if (!ARM_V7S_PTE_IS_VALID(pte)) {
		dma_addr = __arm_v7s_dma_addr(ptep_curr);
		pr_notice("%s, %d, err pte, iova=0x%lx, ptep=0x%lx/0x%lx, pte=0x%lx, level=0x%x\n",
			  __func__, __LINE__, iova, dma_addr,
			  (unsigned long)*ptep_curr, (unsigned long)pte, lvl);
		return -EINVAL;
	}

	*size = ARM_V7S_BLOCK_SIZE(lvl);
	if (arm_v7s_pte_is_cont(pte, lvl))
		*size *= ARM_V7S_CONT_PAGES;


	/* update acp settings of pte */
	mask = 0x1 << ARM_V7S_ATTR_ACP(lvl);
	enabled = pte & mask;

	if (is_acp && !enabled) {
		pte |= mask;
	} else if (!is_acp && enabled) {
		pte &= ~mask;
	} else {
#if 1 //def MTK_PGTABLE_DEBUG_ENABLED
		dma_addr = __arm_v7s_dma_addr(ptep_curr);
		pr_notice("%s, %d, no need of acp switch, iova=0x%lx, ptep=0x%lx/0x%lx, pte=0x%lx, level=%d\n",
			  __func__, __LINE__, iova,
			  dma_addr,
			  (unsigned long)*ptep_curr, (unsigned long)pte, lvl);
#endif
		goto out;
	}

	if (arm_v7s_pte_is_cont(pte, lvl))
		__arm_v7s_set_pte(ptep_curr, pte, ARM_V7S_CONT_PAGES, cfg);
	else
		__arm_v7s_set_pte(ptep_curr, pte, 1, cfg);
#if 1 //def MTK_PGTABLE_DEBUG_ENABLED
	dma_addr = __arm_v7s_dma_addr(ptep_curr);
	pr_notice("%s, %d, iova=0x%lx, mask=0x%x, ptep=0x%lx/0x%lx, pte=0x%lx, level=%d, size=%lu\n",
		  __func__, __LINE__, iova, mask, dma_addr,
		  (unsigned long)*ptep_curr,
		  (unsigned long)pte, lvl, *size);
#endif

out:
	return lvl;
}

static int arm_v7s_switch_acp(struct io_pgtable_ops *ops,
			unsigned long iova, size_t size, bool is_acp)
{
	struct arm_v7s_io_pgtable *data = io_pgtable_ops_to_data(ops);
	struct io_pgtable *iop = &data->iop;
	unsigned long iova_start = iova;
	unsigned long iova_end = iova + size - 1;
	unsigned long pte_sz = 0;
	int lvl;

	while (iova < iova_end) {
		lvl = arm_v7s_set_acp(data, iova, is_acp, &pte_sz);
		if (lvl < 0 || lvl > 2)
			return -1;

		iova += pte_sz;
	}

	/* TLB invalidation */
	io_pgtable_tlb_add_flush(iop, iova_start, size, size, true);
	io_pgtable_tlb_sync(iop);
	return 0;
}
#endif
static phys_addr_t arm_v7s_iova_to_phys(struct io_pgtable_ops *ops,
					unsigned long iova)
{
	struct arm_v7s_io_pgtable *data = io_pgtable_ops_to_data(ops);
	struct io_pgtable_cfg *cfg = &data->iop.cfg;
	arm_v7s_iopte *ptep = data->pgd, pte;
#ifdef MTK_PGTABLE_DEBUG_ENABLED
	arm_v7s_iopte *ptep_curr;
#endif
	phys_addr_t paddr;
	int lvl = 0;
	u32 mask;

	do {
		ptep += ARM_V7S_LVL_IDX(iova, ++lvl);
#ifdef MTK_PGTABLE_DEBUG_ENABLED
		if (lvl == 1)
			ptep_curr = ptep;
#endif
		pte = READ_ONCE(*ptep);
		ptep = iopte_deref(pte, lvl, data);
	} while (ARM_V7S_PTE_IS_TABLE(pte, lvl));

	if (!ARM_V7S_PTE_IS_VALID(pte)) {
		return 0;
	}
	mask = ARM_V7S_LVL_MASK(lvl);
	if (arm_v7s_pte_is_cont(pte, lvl))
		mask *= ARM_V7S_CONT_PAGES;
	paddr = (pte & mask) | (iova & ~mask);

	if (IS_ENABLED(CONFIG_PHYS_ADDR_T_64BIT) &&
	    cfg->quirks & IO_PGTABLE_QUIRK_ARM_MTK_4GB) {
		if (ARM_V7S_PHYS_ADDR_BITS > 32 &&
		    pte & ARM_V7S_ATTR_MTK_PA_BIT32)
			paddr |= BIT_ULL(32);
		if (ARM_V7S_PHYS_ADDR_BITS > 33 &&
		    pte & ARM_V7S_ATTR_MTK_PA_BIT33)
			paddr |= BIT_ULL(33);
		if (ARM_V7S_PHYS_ADDR_BITS > 34 &&
		    pte & ARM_V7S_ATTR_MTK_PA_BIT34)
			paddr |= BIT_ULL(34);
	}
#if 0 //def MTK_PGTABLE_DEBUG_ENABLED
	pr_notice("%s, %d, iova=0x%lx, paddr=0x%lx, ptep=0x%lx, pte=0x%lx, level=%d\n",
		  __func__, __LINE__, iova, paddr,
		  __arm_v7s_dma_addr(ptep_curr), pte, lvl);
#endif
	return paddr;
}

static int mtk_pgtable_align(arm_v7s_iopte *ptep)
{
	unsigned long pgd_pa;
#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT)
	unsigned long align_mask = SZ_16K * (1 <<
			(CONFIG_MTK_IOMMU_PGTABLE_EXT - 32)) - 1;
#else
	unsigned long align_mask = SZ_16K - 1;
#endif

	pgd_pa = (unsigned long)virt_to_phys(ptep);
	if (pgd_pa != (pgd_pa & ~align_mask)) {
		pr_notice("%s, %d, pgd not align, pgd_pa:0x%lx, mask=0x%lx\n",
			  __func__, __LINE__, pgd_pa, ~align_mask);
		return -1;
	}
	return 0;
}

static struct io_pgtable *arm_v7s_alloc_pgtable(struct io_pgtable_cfg *cfg,
						void *cookie)
{
	struct arm_v7s_io_pgtable *data;
	unsigned long base = 0;
#ifdef MTK_PGTABLE_DEBUG_ENABLED
	phys_addr_t phys_addr;
#endif

	if (cfg->ias > ARM_V7S_ADDR_BITS || cfg->oas > ARM_V7S_PHYS_ADDR_BITS) {
		pr_notice("%s, %d, err ias=0x%x, oas=0x%x\n",
			  __func__, __LINE__, cfg->ias, cfg->oas);
		return NULL;
	}

#ifndef CONFIG_MTK_IOMMU_V2
#ifdef PHYS_OFFSET
	if (PHYS_OFFSET > (1ULL << cfg->oas))
		return NULL;
#endif
#endif

	if (cfg->quirks & ~(IO_PGTABLE_QUIRK_ARM_NS |
			    IO_PGTABLE_QUIRK_NO_PERMS |
			    IO_PGTABLE_QUIRK_TLBI_ON_MAP |
			    IO_PGTABLE_QUIRK_ARM_MTK_4GB |
			    IO_PGTABLE_QUIRK_NO_DMA)) {
#ifdef MTK_PGTABLE_DEBUG_ENABLED
		pr_notice("%s, %d, invalid quirks: 0x%lx\n",
			  __func__, __LINE__, cfg->quirks);
#endif
		return NULL;
	}

	/* If ARM_MTK_4GB is enabled, the NO_PERMS is also expected. */
	if ((cfg->quirks & IO_PGTABLE_QUIRK_ARM_MTK_4GB) &&
	    !(cfg->quirks & IO_PGTABLE_QUIRK_NO_PERMS)) {
#ifdef MTK_PGTABLE_DEBUG_ENABLED
		pr_notice("%s, %d, err quirks=0x%lx\n",
			  __func__, __LINE__, cfg->quirks);
#endif
			return NULL;
	}

	data = kmalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
#ifdef MTK_PGTABLE_DEBUG_ENABLED
		pr_notice("%s, %d, failed to allocate data\n",
			  __func__, __LINE__);
#endif
		return NULL;
	}

	spin_lock_init(&data->split_lock);
	data->l2_tables = kmem_cache_create("io-pgtable_armv7s_l2",
					    ARM_V7S_TABLE_SIZE(2),
					    ARM_V7S_TABLE_SIZE(2),
					    ARM_V7S_TABLE_SLAB_FLAGS, NULL);
	if (!data->l2_tables) {
#ifdef MTK_PGTABLE_DEBUG_ENABLED
		pr_notice("%s, %d, err l2_tables\n", __func__, __LINE__);
#endif
		goto out_free_data;
	}

	data->iop.ops = (struct io_pgtable_ops) {
		.map		= arm_v7s_map,
		.unmap		= arm_v7s_unmap,
		.iova_to_phys	= arm_v7s_iova_to_phys,
#ifdef CONFIG_MTK_IOMMU_V2
		.switch_acp	= arm_v7s_switch_acp,
#endif
	};

	/* We have to do this early for __arm_v7s_alloc_table to work... */
	data->iop.cfg = *cfg;

	/*
	 * Unless the IOMMU driver indicates supersection support by
	 * having SZ_16M set in the initial bitmap, they won't be used.
	 */
	cfg->pgsize_bitmap &= SZ_4K | SZ_64K | SZ_1M | SZ_16M;

	/* TCR: T0SZ=0, disable TTBR1 */
	cfg->arm_v7s_cfg.tcr = ARM_V7S_TCR_PD1;

	/*
	 * TEX remap: the indices used map to the closest equivalent types
	 * under the non-TEX-remap interpretation of those attribute bits,
	 * excepting various implementation-defined aspects of shareability.
	 */
	cfg->arm_v7s_cfg.prrr = ARM_V7S_PRRR_TR(1, ARM_V7S_PRRR_TYPE_DEVICE) |
				ARM_V7S_PRRR_TR(4, ARM_V7S_PRRR_TYPE_NORMAL) |
				ARM_V7S_PRRR_TR(7, ARM_V7S_PRRR_TYPE_NORMAL) |
				ARM_V7S_PRRR_DS0 | ARM_V7S_PRRR_DS1 |
				ARM_V7S_PRRR_NS1 | ARM_V7S_PRRR_NOS(7);
	cfg->arm_v7s_cfg.nmrr = ARM_V7S_NMRR_IR(7, ARM_V7S_RGN_WBWA) |
				ARM_V7S_NMRR_OR(7, ARM_V7S_RGN_WBWA);

	/* Looking good; allocate a pgd */
	data->pgd = __arm_v7s_alloc_table(1, GFP_KERNEL, data);
	if (!data->pgd) {
		pr_notice("%s, %d, err pgd\n", __func__, __LINE__);
		goto out_free_data;
	}

#ifdef CONFIG_MTK_IOMMU_V2
	if (mtk_pgtable_align(data->pgd)) {
		pr_notice("%s, %d, err align\n", __func__, __LINE__);
		goto out_free_data;
	}
#endif
	base = (unsigned long)virt_to_phys(data->pgd);

	/* Ensure the empty pgd is visible before any actual TTBR write */
	wmb();

	/* TTBRs */
	cfg->arm_v7s_cfg.ttbr[0] = (unsigned int)base |
				   ARM_V7S_TTBR_S | ARM_V7S_TTBR_NOS |
				   ARM_V7S_TTBR_IRGN_ATTR(ARM_V7S_RGN_WBWA) |
				   ARM_V7S_TTBR_ORGN_ATTR(ARM_V7S_RGN_WBWA);
#ifdef CONFIG_MTK_IOMMU_V2
	cfg->arm_v7s_cfg.ttbr[1] = base >> 32;
#else
	cfg->arm_v7s_cfg.ttbr[1] = 0;
#endif
#ifdef MTK_PGTABLE_DEBUG_ENABLED
	phys_addr = virt_to_phys(data->pgd);
	pr_notice("%s, %d, pgd=0x%lx, cf.ttbr=0x%x,pgd_pa=0x%lx\n",
		  __func__, __LINE__, (uintptr_t)data->pgd,
		cfg->arm_v7s_cfg.ttbr[0], phys_addr);
#endif
	return &data->iop;

out_free_data:
	kmem_cache_destroy(data->l2_tables);
	kfree(data);
	return NULL;
}

struct io_pgtable_init_fns io_pgtable_arm_v7s_init_fns = {
	.alloc	= arm_v7s_alloc_pgtable,
	.free	= arm_v7s_free_pgtable,
};

#ifdef CONFIG_IOMMU_IO_PGTABLE_ARMV7S_SELFTEST

static struct io_pgtable_cfg *cfg_cookie;

static void dummy_tlb_flush_all(void *cookie)
{
	WARN_ON(cookie != cfg_cookie);
}

static void dummy_tlb_add_flush(unsigned long iova, size_t size,
				size_t granule, bool leaf, void *cookie)
{
	WARN_ON(cookie != cfg_cookie);
	WARN_ON(!(size & cfg_cookie->pgsize_bitmap));
}

static void dummy_tlb_sync(void *cookie)
{
	WARN_ON(cookie != cfg_cookie);
}

static const struct iommu_gather_ops dummy_tlb_ops = {
	.tlb_flush_all	= dummy_tlb_flush_all,
	.tlb_add_flush	= dummy_tlb_add_flush,
	.tlb_sync	= dummy_tlb_sync,
};

#define __FAIL(ops)	({				\
		WARN(1, "selftest: test failed\n");	\
		selftest_running = false;		\
		-EFAULT;				\
})

static int __init arm_v7s_do_selftests(void)
{
	struct io_pgtable_ops *ops;
	struct io_pgtable_cfg cfg = {
		.tlb = &dummy_tlb_ops,
#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT == 34)
		.oas = 35,
		.ias = 34,
#else
		.oas = 32,
		.ias = 32,
#endif
		.quirks = IO_PGTABLE_QUIRK_ARM_NS | IO_PGTABLE_QUIRK_NO_DMA,
		.pgsize_bitmap = SZ_4K | SZ_64K | SZ_1M | SZ_16M,
	};
	unsigned int iova, size, iova_start;
	unsigned int i, loopnr = 0;

	selftest_running = true;

	cfg_cookie = &cfg;

	ops = alloc_io_pgtable_ops(ARM_V7S, &cfg, &cfg);
	if (!ops) {
		pr_err("selftest: failed to allocate io pgtable ops\n");
		return -EINVAL;
	}

	/*
	 * Initial sanity checks.
	 * Empty page tables shouldn't provide any translations.
	 */
	if (ops->iova_to_phys(ops, 42))
		return __FAIL(ops);

	if (ops->iova_to_phys(ops, SZ_1G + 42))
		return __FAIL(ops);

	if (ops->iova_to_phys(ops, SZ_2G + 42))
		return __FAIL(ops);

	/*
	 * Distinct mappings of different granule sizes.
	 */
	iova = 0;
	for_each_set_bit(i, &cfg.pgsize_bitmap, BITS_PER_LONG) {
		size = 1UL << i;
		if (ops->map(ops, iova, iova, size, IOMMU_READ |
						    IOMMU_WRITE |
						    IOMMU_NOEXEC |
						    IOMMU_CACHE))
			return __FAIL(ops);

		/* Overlapping mappings */
		if (!ops->map(ops, iova, iova + size, size,
			      IOMMU_READ | IOMMU_NOEXEC))
			return __FAIL(ops);

		if (ops->iova_to_phys(ops, iova + 42) != (iova + 42))
			return __FAIL(ops);

		iova += SZ_16M;
		loopnr++;
	}

	/* Partial unmap */
	i = 1;
	size = 1UL << __ffs(cfg.pgsize_bitmap);
	while (i < loopnr) {
		iova_start = i * SZ_16M;
		if (ops->unmap(ops, iova_start + size, size) != size)
			return __FAIL(ops);

		/* Remap of partial unmap */
		if (ops->map(ops, iova_start + size, size, size, IOMMU_READ))
			return __FAIL(ops);

		if (ops->iova_to_phys(ops, iova_start + size + 42)
		    != (size + 42))
			return __FAIL(ops);
		i++;
	}

	/* Full unmap */
	iova = 0;
	for_each_set_bit(i, &cfg.pgsize_bitmap, BITS_PER_LONG) {
		size = 1UL << i;

		if (ops->unmap(ops, iova, size) != size)
			return __FAIL(ops);

		if (ops->iova_to_phys(ops, iova + 42))
			return __FAIL(ops);

		/* Remap full block */
		if (ops->map(ops, iova, iova, size, IOMMU_WRITE))
			return __FAIL(ops);

		if (ops->iova_to_phys(ops, iova + 42) != (iova + 42))
			return __FAIL(ops);

		iova += SZ_16M;
	}

	free_io_pgtable_ops(ops);

	selftest_running = false;

	pr_info("self test ok\n");
	return 0;
}
subsys_initcall(arm_v7s_do_selftests);
#endif
