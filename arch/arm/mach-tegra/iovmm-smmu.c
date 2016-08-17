/*
 * arch/arm/mach-tegra/iovmm-smmu.c
 *
 * Tegra I/O VMM implementation for SMMU devices for Tegra 3 series
 * SoCs and later.
 *
 * Copyright (c) 2010-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/random.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <asm/page.h>
#include <asm/cacheflush.h>

#include <mach/iovmm.h>
#include <mach/iomap.h>
#include <mach/tegra_smmu.h>

/*
 * Macros without __ copied from armc.h
 */
#define MC_INTSTATUS_0					0x0
#define MC_ERR_STATUS_0					0x8
#define MC_ERR_ADR_0					0xc

#define MC_SMMU_CONFIG_0				0x10
#define MC_SMMU_CONFIG_0_SMMU_ENABLE_DISABLE		0
#define MC_SMMU_CONFIG_0_SMMU_ENABLE_ENABLE		1

#define MC_SMMU_TLB_CONFIG_0				0x14
#define MC_SMMU_TLB_CONFIG_0_TLB_STATS_ENABLE__MASK	(1 << 31)
#define MC_SMMU_TLB_CONFIG_0_TLB_STATS_ENABLE		(1 << 31)
#define MC_SMMU_TLB_CONFIG_0_TLB_STATS_TEST__MASK	(1 << 30)
#define MC_SMMU_TLB_CONFIG_0_TLB_STATS_TEST		(1 << 30)
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
#define MC_SMMU_TLB_CONFIG_0_TLB_ACTIVE_LINES__VALUE	0x10
#define MC_SMMU_TLB_CONFIG_0_RESET_VAL			0x20000010
#else
#define MC_SMMU_TLB_CONFIG_0_TLB_ACTIVE_LINES__VALUE	0x20
#define MC_SMMU_TLB_CONFIG_0_RESET_VAL			0x20000020
#endif

#define MC_SMMU_PTC_CONFIG_0				0x18
#define MC_SMMU_PTC_CONFIG_0_PTC_STATS_ENABLE__MASK	(1 << 31)
#define MC_SMMU_PTC_CONFIG_0_PTC_STATS_ENABLE		(1 << 31)
#define MC_SMMU_PTC_CONFIG_0_PTC_STATS_TEST__MASK	(1 << 30)
#define MC_SMMU_PTC_CONFIG_0_PTC_STATS_TEST		(1 << 30)
#define MC_SMMU_PTC_CONFIG_0_PTC_INDEX_MAP__PATTERN	0x3f
#define MC_SMMU_PTC_CONFIG_0_RESET_VAL			0x2000003f

#define MC_SMMU_STATS_CONFIG_MASK		\
	MC_SMMU_PTC_CONFIG_0_PTC_STATS_ENABLE__MASK
#define MC_SMMU_STATS_CONFIG_ENABLE		\
	MC_SMMU_PTC_CONFIG_0_PTC_STATS_ENABLE
#define MC_SMMU_STATS_CONFIG_TEST		\
	MC_SMMU_PTC_CONFIG_0_PTC_STATS_TEST

#define MC_SMMU_PTB_ASID_0				0x1c
#define MC_SMMU_PTB_ASID_0_CURRENT_ASID_SHIFT		0

#define MC_SMMU_PTB_DATA_0				0x20
#define MC_SMMU_PTB_DATA_0_RESET_VAL			0
#define MC_SMMU_PTB_DATA_0_ASID_NONSECURE_SHIFT		29
#define MC_SMMU_PTB_DATA_0_ASID_WRITABLE_SHIFT		30
#define MC_SMMU_PTB_DATA_0_ASID_READABLE_SHIFT		31

#define MC_SMMU_TLB_FLUSH_0				0x30
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_MATCH_ALL		0
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_MATCH_SECTION		2
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_MATCH_GROUP		3
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_SHIFT		29
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH_DISABLE	0
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH_ENABLE		1
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH_SHIFT		31

#define MC_SMMU_PTC_FLUSH_0				0x34
#define MC_SMMU_PTC_FLUSH_0_PTC_FLUSH_TYPE_ALL		0
#define MC_SMMU_PTC_FLUSH_0_PTC_FLUSH_TYPE_ADR		1
#define MC_SMMU_PTC_FLUSH_0_PTC_FLUSH_ADR_SHIFT		4

#define MC_SMMU_ASID_SECURITY_0				0x38
#define MC_EMEM_CFG_0					0x50
#define MC_SECURITY_CFG0_0				0x70
#define MC_SECURITY_CFG1_0				0x74
#define MC_SECURITY_CFG2_0				0x78
#define MC_SECURITY_RSV_0				0x7c

#define MC_SMMU_STATS_TLB_HIT_COUNT_0			0x1f0
#define MC_SMMU_STATS_TLB_MISS_COUNT_0			0x1f4
#define MC_SMMU_STATS_PTC_HIT_COUNT_0			0x1f8
#define MC_SMMU_STATS_PTC_MISS_COUNT_0			0x1fc

#define MC_SMMU_TRANSLATION_ENABLE_0_0			0x228
#define MC_SMMU_TRANSLATION_ENABLE_1_0			0x22c
#define MC_SMMU_TRANSLATION_ENABLE_2_0			0x230

#define MC_SMMU_AFI_ASID_0              0x238   /* PCIE (T30) */
#define MC_SMMU_AVPC_ASID_0             0x23c   /* AVP */
#define MC_SMMU_DC_ASID_0               0x240   /* Display controller */
#define MC_SMMU_DCB_ASID_0              0x244   /* Display controller B */
#define MC_SMMU_EPP_ASID_0              0x248   /* Encoder pre-processor */
#define MC_SMMU_G2_ASID_0               0x24c   /* 2D engine */
#define MC_SMMU_HC_ASID_0               0x250   /* Host1x */
#define MC_SMMU_HDA_ASID_0              0x254   /* High-def audio */
#define MC_SMMU_ISP_ASID_0              0x258   /* Image signal processor */
#define MC_SMMU_MPE_ASID_0              0x264   /* MPEG encoder (T30) */
#define MC_SMMU_MSENC_ASID_0            0x264   /* MPEG encoder (T11x) */
#define MC_SMMU_NV_ASID_0               0x268   /* 3D */
#define MC_SMMU_NV2_ASID_0              0x26c   /* 3D secondary (T30) */
#define MC_SMMU_PPCS_ASID_0             0x270   /* AHB */
#define MC_SMMU_SATA_ASID_0             0x278   /* SATA (T30) */
#define MC_SMMU_VDE_ASID_0              0x27c   /* Video decoder */
#define MC_SMMU_VI_ASID_0               0x280   /* Video input */
#define MC_SMMU_XUSB_HOST_ASID_0        0x288   /* USB host (T11x) */
#define MC_SMMU_XUSB_DEV_ASID_0         0x28c   /* USB dev (T11x) */
#define MC_SMMU_TSEC_ASID_0             0x294   /* TSEC (T11x) */
#define MC_SMMU_PPCS1_ASID_0            0x298   /* AHB secondary (T11x) */

/*
 * Tegra11x
 */
#define MC_STAT_CONTROL_0			0x100
#define MC_STAT_EMC_CLOCKS_0			0x110
#define MC_STAT_EMC_FILTER_SET0_ADR_LIMIT_LO_0	0x118
#define MC_STAT_EMC_FILTER_SET0_ADR_LIMIT_HI_0	0x11c
#define MC_STAT_EMC_FILTER_SET0_CLIENT_0_0	0x128
#define MC_STAT_EMC_FILTER_SET0_CLIENT_1_0	0x12c
#define MC_STAT_EMC_FILTER_SET0_CLIENT_2_0	0x130
#define MC_STAT_EMC_SET0_COUNT_0		0x138
#define MC_STAT_EMC_SET0_COUNT_MSBS_0		0x13c
#define MC_STAT_EMC_FILTER_SET1_ADR_LIMIT_LO_0	0x158
#define MC_STAT_EMC_FILTER_SET1_ADR_LIMIT_HI_0	0x15c
#define MC_STAT_EMC_FILTER_SET1_CLIENT_0_0	0x168
#define MC_STAT_EMC_FILTER_SET1_CLIENT_1_0	0x16c
#define MC_STAT_EMC_FILTER_SET1_CLIENT_2_0	0x170
#define MC_STAT_EMC_SET1_COUNT_0		0x178
#define MC_STAT_EMC_SET1_COUNT_MSBS_0		0x17c
#define MC_STAT_EMC_FILTER_SET0_VIRTUAL_ADR_LIMIT_LO_0	0x198
#define MC_STAT_EMC_FILTER_SET0_VIRTUAL_ADR_LIMIT_HI_0	0x19c
#define MC_STAT_EMC_FILTER_SET0_ASID_0		0x1a0
#define MC_STAT_EMC_FILTER_SET1_VIRTUAL_ADR_LIMIT_LO_0	0x1a8
#define MC_STAT_EMC_FILTER_SET1_VIRTUAL_ADR_LIMIT_HI_0	0x1ac
#define MC_STAT_EMC_FILTER_SET1_ASID_0		0x1b0

/*
 * Copied from arahb_arbc.h
 */
#ifndef CONFIG_ARCH_TEGRA_3x_SOC
#define AHB_MASTER_SWID_0		0x18
#endif
#define AHB_ARBITRATION_XBAR_CTRL_0	0xe0
#define AHB_ARBITRATION_XBAR_CTRL_0_SMMU_INIT_DONE_DONE		1
#define AHB_ARBITRATION_XBAR_CTRL_0_SMMU_INIT_DONE_SHIFT	17

/*
 * Copied from arapbdma.h
 */
#ifndef CONFIG_ARCH_TEGRA_3x_SOC
#define APBDMA_CHANNEL_SWID_0		0x3c
#endif

#define MC_SMMU_NUM_ASIDS	4
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_SECTION__MASK		0xffc00000
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_SECTION__SHIFT	12 /* right shift */
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_GROUP__MASK		0xffffc000
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_GROUP__SHIFT	12 /* right shift */
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA(iova, which)	\
	((((iova) & MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_##which##__MASK) >> \
		MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_##which##__SHIFT) |	\
	MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_MATCH_##which)
#define MC_SMMU_PTB_ASID_0_CURRENT_ASID(n)	\
		((n) << MC_SMMU_PTB_ASID_0_CURRENT_ASID_SHIFT)
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH_disable		\
		(MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH_DISABLE <<	\
			MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH_SHIFT)
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH__ENABLE		\
		(MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH_ENABLE <<	\
			MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH_SHIFT)

#define VMM_NAME "iovmm-smmu"
#define DRIVER_NAME "tegra_smmu"

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

#define SMMU_PDE_NEXT_SHIFT		28

#define SMMU_ADDR_TO_PFN(addr)	((addr) >> 12)
#define SMMU_ADDR_TO_PDN(addr)	((addr) >> 22)
#define SMMU_PDN_TO_ADDR(pdn)	((pdn) << 22)

#define _READABLE	(1 << MC_SMMU_PTB_DATA_0_ASID_READABLE_SHIFT)
#define _WRITABLE	(1 << MC_SMMU_PTB_DATA_0_ASID_WRITABLE_SHIFT)
#define _NONSECURE	(1 << MC_SMMU_PTB_DATA_0_ASID_NONSECURE_SHIFT)
#define _PDE_NEXT	(1 << SMMU_PDE_NEXT_SHIFT)
#define _MASK_ATTR	(_READABLE | _WRITABLE | _NONSECURE)

#define _PDIR_ATTR	(_READABLE | _WRITABLE | _NONSECURE)

#define _PDE_ATTR	(_READABLE | _WRITABLE | _NONSECURE)
#define _PDE_ATTR_N	(_PDE_ATTR | _PDE_NEXT)
#define _PDE_VACANT(pdn)	(((pdn) << 10) | _PDE_ATTR)

#define _PTE_ATTR	(_READABLE | _WRITABLE | _NONSECURE)
#define _PTE_VACANT(addr)	(((addr) >> SMMU_PAGE_SHIFT) | _PTE_ATTR)

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

#ifdef CONFIG_ARCH_TEGRA_3x_SOC
#define SMMU_HWC	\
	op(AFI)		\
	op(AVPC)	\
	op(DC)		\
	op(DCB)		\
	op(EPP)		\
	op(G2)		\
	op(HC)		\
	op(HDA)		\
	op(ISP)		\
	op(MPE)		\
	op(NV)		\
	op(NV2)		\
	op(PPCS)	\
	op(SATA)	\
	op(VDE)		\
	op(VI)
#endif

#ifdef CONFIG_ARCH_TEGRA_11x_SOC
#define SMMU_HWC	\
	op(AVPC)	\
	op(DC)		\
	op(DCB)		\
	op(EPP)		\
	op(G2)		\
	op(HC)		\
	op(HDA)		\
	op(ISP)		\
	op(MSENC)	\
	op(NV)		\
	op(PPCS)	\
	op(PPCS1)	\
	op(TSEC)	\
	op(VDE)		\
	op(VI)		\
	op(XUSB_DEV)	\
	op(XUSB_HOST)
#endif

/* Keep this as a "natural" enumeration (no assignments) */
enum smmu_hwclient {
#define op(c)	HWC_##c,
	SMMU_HWC
#undef op
	HWC_COUNT
};

struct smmu_hwc_state {
	unsigned long reg;
	unsigned long enable_disable;
};

/* Hardware client mapping initializer */
#define HWC_INIT(client)	\
	[HWC_##client] = {MC_SMMU_##client##_ASID_0, SMMU_ASID_DISABLE},

static const struct smmu_hwc_state smmu_hwc_state_init[] = {
#define op(c)	HWC_INIT(c)
	SMMU_HWC
#undef op
};


struct domain_hwc_map {
	const char *dev_name;
	const enum smmu_hwclient *hwcs;
	const unsigned int nr_hwcs;
};

/* Enable all hardware clients for SMMU translation */
static const enum smmu_hwclient nvmap_hwcs[] = {
#define op(c)	HWC_##c,
	SMMU_HWC
#undef op
};

static const struct domain_hwc_map smmu_hwc_map[] = {
	{
		.dev_name = "nvmap",
		.hwcs = nvmap_hwcs,
		.nr_hwcs = ARRAY_SIZE(nvmap_hwcs),
	},
};

/*
 * Per address space
 */
struct smmu_as {
	struct smmu_device	*smmu;	/* back pointer to container */
	unsigned int		asid;
	const struct domain_hwc_map	*hwclients;
	struct mutex	lock;	/* for pagetable */
	struct tegra_iovmm_domain domain;
	struct page	*pdir_page;
	unsigned long	pdir_attr;
	unsigned long	pde_attr;
	unsigned long	pte_attr;
	unsigned int	*pte_count;
	struct device	sysfs_dev;
	int		sysfs_use_count;
};

/*
 * Register bank index
 */
enum {
	_MC,
#ifdef TEGRA_MC0_BASE
	_MC0,
#endif
#ifdef TEGRA_MC1_BASE
	_MC1,
#endif
	_AHBARB,
	_APBDMA,
	_REGS,
};

static const struct {
	unsigned long base;
	size_t size;
} tegra_reg[_REGS] = {
	[_MC]	= {TEGRA_MC_BASE, TEGRA_MC_SIZE},
#ifdef TEGRA_MC0_BASE
	[_MC0]	= {TEGRA_MC0_BASE, TEGRA_MC0_SIZE},
#endif
#ifdef TEGRA_MC1_BASE
	[_MC1]	= {TEGRA_MC1_BASE, TEGRA_MC1_SIZE},
#endif
	[_AHBARB]	= {TEGRA_AHB_ARB_BASE, TEGRA_AHB_ARB_SIZE},
	[_APBDMA]	= {TEGRA_APB_DMA_BASE, TEGRA_APB_DMA_SIZE},
};

/*
 * Aliases for register bank base addres holders (remapped)
 */
#define regs_mc		regs[_MC]
#define regs_mc0	regs[_MC0]
#define regs_mc1	regs[_MC1]
#define regs_ahbarb	regs[_AHBARB]
#define regs_apbdma	regs[_APBDMA]

/*
 * Per SMMU device
 */
struct smmu_device {
	void __iomem	*regs[_REGS];
	tegra_iovmm_addr_t	iovmm_base;	/* remappable base address */
	unsigned long	page_count;		/* total remappable size */
	spinlock_t	lock;
	char		*name;
	struct tegra_iovmm_device iovmm_dev;
	int		num_ases;
	struct smmu_as	*as;			/* Run-time allocated array */
	struct smmu_hwc_state	hwc_state[HWC_COUNT];
	struct device	sysfs_dev;
	int		sysfs_use_count;
	bool		enable;
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
	struct page *avp_vector_page;	/* dummy page shared by all AS's */
#endif
	/*
	 * Register image savers for suspend/resume
	 */
	unsigned long translation_enable_0_0;
	unsigned long translation_enable_1_0;
	unsigned long translation_enable_2_0;
	unsigned long asid_security_0;

	unsigned long lowest_asid;	/* Variables for hardware testing */
	unsigned long debug_asid;
	unsigned long signature_pid;	/* For debugging aid */
	unsigned long challenge_code;	/* For debugging aid */
	unsigned long challenge_pid;	/* For debugging aid */

	struct device *dev;
	struct dentry *debugfs_root;
};

#define VA_PAGE_TO_PA(va, page)	\
	(page_to_phys(page) + ((unsigned long)(va) & ~PAGE_MASK))

static inline void
flush_cpu_dcache(void *va, struct page *page, size_t size)
{
	unsigned long _pa_ = VA_PAGE_TO_PA((unsigned long)va, page);
	__cpuc_flush_dcache_area((void *)(va), (size_t)(size));
	outer_flush_range(_pa_, _pa_+(size_t)(size));
}

/*
 * Any interaction between any block on PPSB and a block on APB or AHB
 * must have these read-back to ensure the APB/AHB bus transaction is
 * complete before initiating activity on the PPSB block.
 */
static inline void flush_smmu_regs(struct smmu_device *smmu)
{
	(void)readl((smmu)->regs_mc + MC_SMMU_CONFIG_0);
}

/*
 * Flush all TLB entries and all PTC entries
 * Caller must lock smmu
 */
static void smmu_flush_regs(struct smmu_device *smmu, int enable)
{
	writel(MC_SMMU_PTC_FLUSH_0_PTC_FLUSH_TYPE_ALL,
		smmu->regs_mc + MC_SMMU_PTC_FLUSH_0);
	flush_smmu_regs(smmu);
	writel(MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_MATCH_ALL |
			MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH_disable,
			smmu->regs_mc + MC_SMMU_TLB_FLUSH_0);

	if (enable)
		writel(MC_SMMU_CONFIG_0_SMMU_ENABLE_ENABLE,
				smmu->regs_mc + MC_SMMU_CONFIG_0);

	flush_smmu_regs(smmu);
}

static void smmu_setup_regs(struct smmu_device *smmu)
{
	int i;

	if (smmu->as) {
		int asid;

		/* Set/restore page directory for each AS */
		for (asid = 0; asid < smmu->num_ases; asid++) {
			struct smmu_as *as = &smmu->as[asid];

			writel(MC_SMMU_PTB_ASID_0_CURRENT_ASID(as->asid),
				smmu->regs_mc + MC_SMMU_PTB_ASID_0);
			writel(as->pdir_page
				? SMMU_MK_PDIR(as->pdir_page, as->pdir_attr)
				: MC_SMMU_PTB_DATA_0_RESET_VAL,
				smmu->regs_mc + MC_SMMU_PTB_DATA_0);
		}
	}

	/* Set/restore ASID for each hardware client */
	for (i = 0; i < HWC_COUNT; i++) {
		struct smmu_hwc_state *hwcst = &smmu->hwc_state[i];
		writel(hwcst->enable_disable, smmu->regs_mc + hwcst->reg);
	}

	writel(smmu->translation_enable_0_0,
		smmu->regs_mc + MC_SMMU_TRANSLATION_ENABLE_0_0);
	writel(smmu->translation_enable_1_0,
		smmu->regs_mc + MC_SMMU_TRANSLATION_ENABLE_1_0);
	writel(smmu->translation_enable_2_0,
		smmu->regs_mc + MC_SMMU_TRANSLATION_ENABLE_2_0);
	writel(smmu->asid_security_0,
		smmu->regs_mc + MC_SMMU_ASID_SECURITY_0);
	writel(MC_SMMU_TLB_CONFIG_0_RESET_VAL,
		smmu->regs_mc + MC_SMMU_TLB_CONFIG_0);
	writel(MC_SMMU_PTC_CONFIG_0_RESET_VAL,
		smmu->regs_mc + MC_SMMU_PTC_CONFIG_0);

	smmu_flush_regs(smmu, 1);
	writel(
		readl(smmu->regs_ahbarb + AHB_ARBITRATION_XBAR_CTRL_0) |
		(AHB_ARBITRATION_XBAR_CTRL_0_SMMU_INIT_DONE_DONE <<
			AHB_ARBITRATION_XBAR_CTRL_0_SMMU_INIT_DONE_SHIFT),
		smmu->regs_ahbarb + AHB_ARBITRATION_XBAR_CTRL_0);
}

static int smmu_suspend(struct tegra_iovmm_device *dev)
{
	struct smmu_device *smmu =
		container_of(dev, struct smmu_device, iovmm_dev);

	smmu->translation_enable_0_0 =
		readl(smmu->regs_mc + MC_SMMU_TRANSLATION_ENABLE_0_0);
	smmu->translation_enable_1_0 =
		readl(smmu->regs_mc + MC_SMMU_TRANSLATION_ENABLE_1_0);
	smmu->translation_enable_2_0 =
		readl(smmu->regs_mc + MC_SMMU_TRANSLATION_ENABLE_2_0);
	smmu->asid_security_0 =
		readl(smmu->regs_mc + MC_SMMU_ASID_SECURITY_0);
	return 0;
}

static void smmu_resume(struct tegra_iovmm_device *dev)
{
	struct smmu_device *smmu =
		container_of(dev, struct smmu_device, iovmm_dev);

	if (!smmu->enable)
		return;

	spin_lock(&smmu->lock);
	smmu_setup_regs(smmu);
	spin_unlock(&smmu->lock);
}

static void flush_ptc_and_tlb(struct smmu_device *smmu,
		struct smmu_as *as, unsigned long iova,
		unsigned long *pte, struct page *ptpage, int is_pde)
{
	unsigned long tlb_flush_va = is_pde
			?  MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA(iova, SECTION)
			:  MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA(iova, GROUP);

	writel(MC_SMMU_PTC_FLUSH_0_PTC_FLUSH_TYPE_ADR |
		VA_PAGE_TO_PA(pte, ptpage),
		smmu->regs_mc + MC_SMMU_PTC_FLUSH_0);
	flush_smmu_regs(smmu);
	writel(tlb_flush_va |
		MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH__ENABLE |
		(as->asid << MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_SHIFT),
		smmu->regs_mc + MC_SMMU_TLB_FLUSH_0);
	flush_smmu_regs(smmu);
}

static void free_ptbl(struct smmu_as *as, unsigned long iova)
{
	unsigned long pdn = SMMU_ADDR_TO_PDN(iova);
	unsigned long *pdir = (unsigned long *)kmap(as->pdir_page);

	if (pdir[pdn] != _PDE_VACANT(pdn)) {
		pr_debug("%s:%d pdn=%lx\n", __func__, __LINE__, pdn);

		ClearPageReserved(SMMU_EX_PTBL_PAGE(pdir[pdn]));
		__free_page(SMMU_EX_PTBL_PAGE(pdir[pdn]));
		pdir[pdn] = _PDE_VACANT(pdn);
		flush_cpu_dcache(&pdir[pdn], as->pdir_page, sizeof pdir[pdn]);
		flush_ptc_and_tlb(as->smmu, as, iova, &pdir[pdn],
				as->pdir_page, 1);
	}
	kunmap(as->pdir_page);
}

static void free_pdir(struct smmu_as *as)
{
	if (as->pdir_page) {
		unsigned addr = as->smmu->iovmm_base;
		int count = as->smmu->page_count;

		while (count-- > 0) {
			free_ptbl(as, addr);
			addr += SMMU_PAGE_SIZE * SMMU_PTBL_COUNT;
		}
		ClearPageReserved(as->pdir_page);
		__free_page(as->pdir_page);
		as->pdir_page = NULL;
		kfree(as->pte_count);
		as->pte_count = NULL;
	}
}

static const char * const smmu_debugfs_mc[] = {
	"mc",
#ifdef TEGRA_MC0_BASE
	"mc0",
#endif
#ifdef TEGRA_MC1_BASE
	"mc1",
#endif
};

static const char * const smmu_debugfs_cache[] = {  "tlb", "ptc", };

static ssize_t smmu_debugfs_stats_write(struct file *file,
					const char __user *buffer,
					size_t count, loff_t *pos)
{
	struct inode *inode;
	struct dentry *cache, *mc, *root;
	struct smmu_device *smmu;
	int mc_idx, cache_idx, i;
	u32 offs, val;
	const char * const smmu_debugfs_stats_ctl[] = { "off", "on", "reset"};
	char str[] = "reset";

	count = min_t(size_t, count, sizeof(str));
	if (copy_from_user(str, buffer, count))
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(smmu_debugfs_stats_ctl); i++)
		if (strncmp(str, smmu_debugfs_stats_ctl[i],
			    strlen(smmu_debugfs_stats_ctl[i])) == 0)
			break;

	if (i == ARRAY_SIZE(smmu_debugfs_stats_ctl))
		return -EINVAL;

	cache = file->f_dentry;
	inode = cache->d_inode;
	cache_idx = (int)inode->i_private;
	mc = cache->d_parent;
	mc_idx = (int)mc->d_inode->i_private;
	root = mc->d_parent;
	smmu = root->d_inode->i_private;

	offs = MC_SMMU_TLB_CONFIG_0;
	offs += sizeof(u32) * cache_idx;
	offs += 2 * sizeof(u32) * ARRAY_SIZE(smmu_debugfs_cache) * mc_idx;

	val = readl(smmu->regs + offs);
	switch (i) {
	case 0:
		val &= ~MC_SMMU_STATS_CONFIG_ENABLE;
		val &= ~MC_SMMU_STATS_CONFIG_TEST;
		writel(val, smmu->regs + offs);
		break;
	case 1:
		val |= MC_SMMU_STATS_CONFIG_ENABLE;
		val &= ~MC_SMMU_STATS_CONFIG_TEST;
		writel(val, smmu->regs + offs);
		break;
	case 2:
		val |= MC_SMMU_STATS_CONFIG_TEST;
		writel(val, smmu->regs + offs);
		val &= ~MC_SMMU_STATS_CONFIG_TEST;
		writel(val, smmu->regs + offs);
		break;
	default:
		BUG();
		break;
	}

	pr_debug("%s() %08x, %08x @%08x\n", __func__,
		 val, readl(smmu->regs + offs), offs);

	return count;
}

static int smmu_debugfs_stats_show(struct seq_file *s, void *v)
{
	struct inode *inode;
	struct dentry *cache, *mc, *root;
	struct smmu_device *smmu;
	int mc_idx, cache_idx, i;
	u32 offs;
	const char * const smmu_debugfs_stats[] = { "hit", "miss", };

	inode = s->private;

	cache = d_find_alias(inode);
	cache_idx = (int)inode->i_private;
	mc = cache->d_parent;
	mc_idx = (int)mc->d_inode->i_private;
	root = mc->d_parent;
	smmu = root->d_inode->i_private;

	offs = MC_SMMU_STATS_TLB_HIT_COUNT_0;
	offs += ARRAY_SIZE(smmu_debugfs_stats) * sizeof(u32) * cache_idx;
	offs += ARRAY_SIZE(smmu_debugfs_stats) * sizeof(u32) *
		ARRAY_SIZE(smmu_debugfs_cache) * mc_idx;

	for (i = 0; i < ARRAY_SIZE(smmu_debugfs_stats); i++) {
		u32 val;

		offs += sizeof(u32) * i;
		val = readl(smmu->regs + offs);

		seq_printf(s, "%s:%08x ", smmu_debugfs_stats[i], val);

		pr_debug("%s() %s %08x @%08x\n", __func__,
			 smmu_debugfs_stats[i], val, offs);
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
}

static void smmu_debugfs_create(struct smmu_device *smmu)
{
	int i;
	struct dentry *root;

	root = debugfs_create_file("smmu",
				   S_IFDIR | S_IRWXU | S_IRUGO | S_IXUGO,
				   NULL, smmu, NULL);
	if (!root)
		goto err_out;
	smmu->debugfs_root = root;

	for (i = 0; i < ARRAY_SIZE(smmu_debugfs_mc); i++) {
		int j;
		struct dentry *mc;

		mc = debugfs_create_file(smmu_debugfs_mc[i],
					 S_IFDIR | S_IRWXU | S_IRUGO | S_IXUGO,
					 root, (void *)i, NULL);
		if (!mc)
			goto err_out;

		for (j = 0; j < ARRAY_SIZE(smmu_debugfs_cache); j++) {
			struct dentry *cache;

			cache = debugfs_create_file(smmu_debugfs_cache[j],
						    S_IWUGO | S_IRUGO, mc,
						    (void *)j,
						    &smmu_debugfs_stats_fops);
			if (!cache)
				goto err_out;
		}
	}

	return;

err_out:
	smmu_debugfs_delete(smmu);
}

static int smmu_remove(struct platform_device *pdev)
{
	struct smmu_device *smmu = platform_get_drvdata(pdev);
	int i;

	if (!smmu)
		return 0;

	smmu_debugfs_delete(smmu);

	if (smmu->enable) {
		writel(MC_SMMU_CONFIG_0_SMMU_ENABLE_DISABLE,
			smmu->regs_mc + MC_SMMU_CONFIG_0);
		smmu->enable = 0;
	}
	platform_set_drvdata(pdev, NULL);

	if (smmu->as) {
		int asid;

		for (asid = 0; asid < smmu->num_ases; asid++)
			free_pdir(&smmu->as[asid]);
		kfree(smmu->as);
	}

#ifdef CONFIG_ARCH_TEGRA_3x_SOC
	if (smmu->avp_vector_page)
		__free_page(smmu->avp_vector_page);
#endif
	tegra_iovmm_unregister(&smmu->iovmm_dev);
	for (i = 0; i < _REGS; i++) {
		if (smmu->regs[i]) {
			iounmap(smmu->regs[i]);
			smmu->regs[i] = NULL;
		}
	}
	kfree(smmu);
	return 0;
}

/*
 * Maps PTBL for given iova and returns the PTE address
 * Caller must unmap the mapped PTBL returned in *ptbl_page_p
 */
static unsigned long *locate_pte(struct smmu_as *as,
		unsigned long iova, bool allocate,
		struct page **ptbl_page_p,
		unsigned int **pte_counter)
{
	unsigned long ptn = SMMU_ADDR_TO_PFN(iova);
	unsigned long pdn = SMMU_ADDR_TO_PDN(iova);
	unsigned long *pdir = kmap(as->pdir_page);
	unsigned long *ptbl;

	if (pdir[pdn] != _PDE_VACANT(pdn)) {
		/* Mapped entry table already exists */
		*ptbl_page_p = SMMU_EX_PTBL_PAGE(pdir[pdn]);
		ptbl = kmap(*ptbl_page_p);
	} else if (!allocate) {
		kunmap(as->pdir_page);
		return NULL;
	} else {
		/* Vacant - allocate a new page table */
		pr_debug("%s:%d new PTBL pdn=%lx\n", __func__, __LINE__, pdn);

		*ptbl_page_p = alloc_page(GFP_KERNEL | __GFP_DMA);
		if (!*ptbl_page_p) {
			kunmap(as->pdir_page);
			pr_err(DRIVER_NAME
			": failed to allocate tegra_iovmm_device page table\n");
			return NULL;
		}
		SetPageReserved(*ptbl_page_p);
		ptbl = (unsigned long *)kmap(*ptbl_page_p);
		{
			int pn;
			unsigned long addr = SMMU_PDN_TO_ADDR(pdn);
			for (pn = 0; pn < SMMU_PTBL_COUNT;
				pn++, addr += SMMU_PAGE_SIZE) {
				ptbl[pn] = _PTE_VACANT(addr);
			}
		}
		flush_cpu_dcache(ptbl, *ptbl_page_p, SMMU_PTBL_SIZE);
		pdir[pdn] = SMMU_MK_PDE(*ptbl_page_p,
				as->pde_attr | _PDE_NEXT);
		flush_cpu_dcache(&pdir[pdn], as->pdir_page, sizeof pdir[pdn]);
		flush_ptc_and_tlb(as->smmu, as, iova, &pdir[pdn],
				as->pdir_page, 1);
	}
	*pte_counter = &as->pte_count[pdn];

	kunmap(as->pdir_page);
	return &ptbl[ptn % SMMU_PTBL_COUNT];
}

static void put_signature(struct smmu_as *as,
			unsigned long addr, unsigned long pfn)
{
	if (as->smmu->signature_pid == current->pid) {
		struct page *page = pfn_to_page(pfn);
		unsigned long *vaddr = kmap(page);
		if (vaddr) {
			vaddr[0] = addr;
			vaddr[1] = pfn << PAGE_SHIFT;
			flush_cpu_dcache(vaddr, page, sizeof(vaddr[0]) * 2);
			kunmap(page);
		}
	}
}

static int smmu_map(struct tegra_iovmm_domain *domain,
		struct tegra_iovmm_area *iovma)
{
	struct smmu_as *as = container_of(domain, struct smmu_as, domain);
	unsigned long addr = iovma->iovm_start;
	unsigned long pcount = iovma->iovm_length >> SMMU_PAGE_SHIFT;
	int i;

	pr_debug("%s:%d iova=%lx asid=%d\n", __func__, __LINE__,
		 addr, as - as->smmu->as);

	for (i = 0; i < pcount; i++) {
		unsigned long pfn;
		unsigned long *pte;
		unsigned int *pte_counter;
		struct page *ptpage;

		pfn = iovma->ops->lock_makeresident(iovma, i << PAGE_SHIFT);
		if (!pfn_valid(pfn))
			goto fail;

		mutex_lock(&as->lock);

		pte = locate_pte(as, addr, true, &ptpage, &pte_counter);
		if (!pte)
			goto fail2;

		pr_debug("%s:%d iova=%lx pfn=%lx asid=%d\n",
			 __func__, __LINE__, addr, pfn, as - as->smmu->as);

		if (*pte == _PTE_VACANT(addr))
			(*pte_counter)++;
		*pte = SMMU_PFN_TO_PTE(pfn, as->pte_attr);
		if (unlikely((*pte == _PTE_VACANT(addr))))
			(*pte_counter)--;
		flush_cpu_dcache(pte, ptpage, sizeof *pte);
		flush_ptc_and_tlb(as->smmu, as, addr, pte, ptpage, 0);
		kunmap(ptpage);
		mutex_unlock(&as->lock);
		put_signature(as, addr, pfn);
		addr += SMMU_PAGE_SIZE;
	}
	return 0;

fail:
	mutex_lock(&as->lock);
fail2:
	while (i-- > 0) {
		unsigned long *pte;
		unsigned int *pte_counter;
		struct page *page;

		iovma->ops->release(iovma, i<<PAGE_SHIFT);
		addr -= SMMU_PAGE_SIZE;
		pte = locate_pte(as, addr, false, &page, &pte_counter);
		if (pte) {
			if (*pte != _PTE_VACANT(addr)) {
				*pte = _PTE_VACANT(addr);
				flush_cpu_dcache(pte, page, sizeof *pte);
				flush_ptc_and_tlb(as->smmu, as, addr, pte,
						page, 0);
				kunmap(page);
				if (!--(*pte_counter))
					free_ptbl(as, addr);
			} else {
				kunmap(page);
			}
		}
	}
	mutex_unlock(&as->lock);
	return -ENOMEM;
}

static void smmu_unmap(struct tegra_iovmm_domain *domain,
	struct tegra_iovmm_area *iovma, bool decommit)
{
	struct smmu_as *as = container_of(domain, struct smmu_as, domain);
	unsigned long addr = iovma->iovm_start;
	unsigned int pcount = iovma->iovm_length >> SMMU_PAGE_SHIFT;
	unsigned int i, *pte_counter;

	pr_debug("%s:%d iova=%lx asid=%d\n", __func__, __LINE__,
		 addr, as - as->smmu->as);

	mutex_lock(&as->lock);
	for (i = 0; i < pcount; i++) {
		unsigned long *pte;
		struct page *page;

		if (iovma->ops && iovma->ops->release)
			iovma->ops->release(iovma, i << PAGE_SHIFT);

		pte = locate_pte(as, addr, false, &page, &pte_counter);
		if (pte) {
			if (*pte != _PTE_VACANT(addr)) {
				*pte = _PTE_VACANT(addr);
				flush_cpu_dcache(pte, page, sizeof *pte);
				flush_ptc_and_tlb(as->smmu, as, addr, pte,
						page, 0);
				kunmap(page);
				if (!--(*pte_counter) && decommit) {
					free_ptbl(as, addr);
					smmu_flush_regs(as->smmu, 0);
				}
			}
		}
		addr += SMMU_PAGE_SIZE;
	}
	mutex_unlock(&as->lock);
}

static void smmu_map_pfn(struct tegra_iovmm_domain *domain,
	struct tegra_iovmm_area *iovma, unsigned long addr,
	unsigned long pfn)
{
	struct smmu_as *as = container_of(domain, struct smmu_as, domain);
	struct smmu_device *smmu = as->smmu;
	unsigned long *pte;
	unsigned int *pte_counter;
	struct page *ptpage;

	pr_debug("%s:%d iova=%lx pfn=%lx asid=%d\n", __func__, __LINE__,
		 addr, pfn, as - as->smmu->as);

	BUG_ON(!pfn_valid(pfn));
	mutex_lock(&as->lock);
	pte = locate_pte(as, addr, true, &ptpage, &pte_counter);
	if (pte) {
		if (*pte == _PTE_VACANT(addr))
			(*pte_counter)++;
		*pte = SMMU_PFN_TO_PTE(pfn, as->pte_attr);
		if (unlikely((*pte == _PTE_VACANT(addr))))
			(*pte_counter)--;
		flush_cpu_dcache(pte, ptpage, sizeof *pte);
		flush_ptc_and_tlb(smmu, as, addr, pte, ptpage, 0);
		kunmap(ptpage);
		put_signature(as, addr, pfn);
	}
	mutex_unlock(&as->lock);
}

/*
 * Caller must lock/unlock as
 */
static int alloc_pdir(struct smmu_as *as)
{
	unsigned long *pdir;
	int pdn;

	if (as->pdir_page)
		return 0;

	as->pte_count = kzalloc(sizeof(as->pte_count[0]) * SMMU_PDIR_COUNT,
				GFP_KERNEL);
	if (!as->pte_count) {
		pr_err(DRIVER_NAME
		": failed to allocate tegra_iovmm_device PTE cunters\n");
		return -ENOMEM;
	}
	as->pdir_page = alloc_page(GFP_KERNEL | __GFP_DMA);
	if (!as->pdir_page) {
		pr_err(DRIVER_NAME
		": failed to allocate tegra_iovmm_device page directory\n");
		kfree(as->pte_count);
		as->pte_count = NULL;
		return -ENOMEM;
	}
	SetPageReserved(as->pdir_page);
	pdir = kmap(as->pdir_page);

	for (pdn = 0; pdn < SMMU_PDIR_COUNT; pdn++)
		pdir[pdn] = _PDE_VACANT(pdn);
	flush_cpu_dcache(pdir, as->pdir_page, SMMU_PDIR_SIZE);
	writel(MC_SMMU_PTC_FLUSH_0_PTC_FLUSH_TYPE_ADR |
		VA_PAGE_TO_PA(pdir, as->pdir_page),
		as->smmu->regs_mc + MC_SMMU_PTC_FLUSH_0);
	flush_smmu_regs(as->smmu);
	writel(MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_MATCH_ALL |
		MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH__ENABLE |
		(as->asid << MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_SHIFT),
		as->smmu->regs_mc + MC_SMMU_TLB_FLUSH_0);
	flush_smmu_regs(as->smmu);
	kunmap(as->pdir_page);

	return 0;
}

static void _sysfs_create(struct smmu_as *as, struct device *sysfs_parent);

/*
 * Allocate resources for an AS
 *	TODO: split into "alloc" and "lock"
 */
static struct tegra_iovmm_domain *smmu_alloc_domain(
	struct tegra_iovmm_device *dev, struct tegra_iovmm_client *client)
{
	struct smmu_device *smmu =
		container_of(dev, struct smmu_device, iovmm_dev);
	struct smmu_as *as = NULL;
	const struct domain_hwc_map *map = NULL;
	int asid, i;

	/* Look for a free AS */
	for  (asid = smmu->lowest_asid; asid < smmu->num_ases; asid++) {
		mutex_lock(&smmu->as[asid].lock);
		if (!smmu->as[asid].hwclients) {
			as = &smmu->as[asid];
			break;
		}
		mutex_unlock(&smmu->as[asid].lock);
	}

	if (!as) {
		pr_err(DRIVER_NAME ": no free AS\n");
		return NULL;
	}

	if (alloc_pdir(as) < 0)
		goto bad3;

	/* Look for a matching hardware client group */
	for (i = 0; i < ARRAY_SIZE(smmu_hwc_map); i++) {
		if (!strcmp(smmu_hwc_map[i].dev_name, client->misc_dev->name)) {
			map = &smmu_hwc_map[i];
			break;
		}
	}

	if (!map) {
		pr_err(DRIVER_NAME ": no SMMU resource for %s (%s)\n",
			client->name, client->misc_dev->name);
		goto bad2;
	}

	spin_lock(&smmu->lock);
	/* Update PDIR register */
	writel(MC_SMMU_PTB_ASID_0_CURRENT_ASID(as->asid),
		as->smmu->regs_mc + MC_SMMU_PTB_ASID_0);
	writel(SMMU_MK_PDIR(as->pdir_page, as->pdir_attr),
		as->smmu->regs_mc + MC_SMMU_PTB_DATA_0);
	flush_smmu_regs(smmu);

	/* Put each hardware client in the group into the address space */
	for (i = 0; i < map->nr_hwcs; i++) {
		struct smmu_hwc_state *hwcst = &smmu->hwc_state[map->hwcs[i]];

		/* Is the hardware client busy? */
		if (hwcst->enable_disable != SMMU_ASID_DISABLE &&
			hwcst->enable_disable != SMMU_ASID_ENABLE(as->asid)) {
			pr_err(DRIVER_NAME
				": HW 0x%lx busy for ASID %ld (client!=%s)\n",
				hwcst->reg,
				SMMU_ASID_ASID(hwcst->enable_disable),
				client->name);
			goto bad;
		}
		hwcst->enable_disable = SMMU_ASID_ENABLE(as->asid);
		writel(hwcst->enable_disable, smmu->regs_mc + hwcst->reg);
	}
	flush_smmu_regs(smmu);
	spin_unlock(&smmu->lock);
	as->hwclients = map;
	_sysfs_create(as, client->misc_dev->this_device);
	mutex_unlock(&as->lock);

#ifdef CONFIG_ARCH_TEGRA_3x_SOC
	/* Reserve "page zero" for AVP vectors using a common dummy page */
	smmu_map_pfn(&as->domain, NULL, 0,
		page_to_phys(as->smmu->avp_vector_page) >> SMMU_PAGE_SHIFT);
#endif
	return &as->domain;

bad:
	/* Reset hardware clients that have been enabled */
	while (--i >= 0) {
		struct smmu_hwc_state *hwcst = &smmu->hwc_state[map->hwcs[i]];

		hwcst->enable_disable = SMMU_ASID_DISABLE;
		writel(hwcst->enable_disable, smmu->regs_mc + hwcst->reg);
	}
	flush_smmu_regs(smmu);
	spin_unlock(&as->smmu->lock);
bad2:
	free_pdir(as);
bad3:
	mutex_unlock(&as->lock);
	return NULL;

}

/*
 * Release resources for an AS
 *	TODO: split into "unlock" and "free"
 */
static void smmu_free_domain(
	struct tegra_iovmm_domain *domain, struct tegra_iovmm_client *client)
{
	struct smmu_as *as = container_of(domain, struct smmu_as, domain);
	struct smmu_device *smmu = as->smmu;
	const struct domain_hwc_map *map = NULL;
	int i;

	mutex_lock(&as->lock);
	map = as->hwclients;

	spin_lock(&smmu->lock);
	for (i = 0; i < map->nr_hwcs; i++) {
		struct smmu_hwc_state *hwcst = &smmu->hwc_state[map->hwcs[i]];

		hwcst->enable_disable = SMMU_ASID_DISABLE;
		writel(SMMU_ASID_DISABLE, smmu->regs_mc + hwcst->reg);
	}
	flush_smmu_regs(smmu);
	spin_unlock(&smmu->lock);

	as->hwclients = NULL;
	if (as->pdir_page) {
		spin_lock(&smmu->lock);
		writel(MC_SMMU_PTB_ASID_0_CURRENT_ASID(as->asid),
			smmu->regs_mc + MC_SMMU_PTB_ASID_0);
		writel(MC_SMMU_PTB_DATA_0_RESET_VAL,
			smmu->regs_mc + MC_SMMU_PTB_DATA_0);
		flush_smmu_regs(smmu);
		spin_unlock(&smmu->lock);

		free_pdir(as);
	}
	mutex_unlock(&as->lock);
}

static struct tegra_iovmm_device_ops tegra_iovmm_smmu_ops = {
	.map = smmu_map,
	.unmap = smmu_unmap,
	.map_pfn = smmu_map_pfn,
	.alloc_domain = smmu_alloc_domain,
	.free_domain = smmu_free_domain,
	.suspend = smmu_suspend,
	.resume = smmu_resume,
};

static int smmu_probe(struct platform_device *pdev)
{
	struct smmu_device *smmu = NULL;
	struct resource *window = NULL;
	int e, i, asid;

	BUILD_BUG_ON(PAGE_SHIFT != SMMU_PAGE_SHIFT);
	BUILD_BUG_ON(ARRAY_SIZE(smmu_hwc_state_init) != HWC_COUNT);

	window = tegra_smmu_window(0);
	if (!window) {
		pr_err(DRIVER_NAME ": No SMMU resources\n");
		return -ENODEV;
	}

	smmu = kzalloc(sizeof(*smmu), GFP_KERNEL);
	if (!smmu) {
		pr_err(DRIVER_NAME ": failed to allocate smmu_device\n");
		return -ENOMEM;
	}

	smmu->num_ases = MC_SMMU_NUM_ASIDS;
	smmu->iovmm_base = (tegra_iovmm_addr_t)window->start;
	smmu->page_count = (window->end + 1 - window->start) >> SMMU_PAGE_SHIFT;
	for (i = _MC; i < _REGS; i++) {
		if (tegra_reg[i].base != 0)
			smmu->regs[i] = ioremap(tegra_reg[i].base,
				tegra_reg[i].size);
	}

	smmu->translation_enable_0_0 = ~0;
	smmu->translation_enable_1_0 = ~0;
	smmu->translation_enable_2_0 = ~0;
	smmu->asid_security_0        = 0;

	memcpy(smmu->hwc_state, smmu_hwc_state_init, sizeof(smmu->hwc_state));

	smmu->iovmm_dev.name = VMM_NAME;
	smmu->iovmm_dev.ops = &tegra_iovmm_smmu_ops;
	smmu->iovmm_dev.pgsize_bits = SMMU_PAGE_SHIFT;

	e = tegra_iovmm_register(&smmu->iovmm_dev);
	if (e)
		goto fail;

	smmu->as = kzalloc(sizeof(smmu->as[0]) * smmu->num_ases, GFP_KERNEL);
	if (!smmu->as) {
		pr_err(DRIVER_NAME ": failed to allocate smmu_as\n");
		e = -ENOMEM;
		goto fail;
	}

	/* Initialize address space structure array */
	for (asid = 0; asid < smmu->num_ases; asid++) {
		struct smmu_as *as = &smmu->as[asid];

		as->smmu = smmu;
		as->asid = asid;
		as->pdir_attr = _PDIR_ATTR;
		as->pde_attr  = _PDE_ATTR;
		as->pte_attr  = _PTE_ATTR;

		mutex_init(&as->lock);

		e = tegra_iovmm_domain_init(&as->domain, &smmu->iovmm_dev,
			smmu->iovmm_base,
			smmu->iovmm_base +
				(smmu->page_count << SMMU_PAGE_SHIFT));
		if (e)
			goto fail;
	}
	spin_lock_init(&smmu->lock);
	smmu_setup_regs(smmu);
	smmu->enable = 1;
	smmu->dev = &pdev->dev;
	platform_set_drvdata(pdev, smmu);

#ifdef CONFIG_ARCH_TEGRA_3x_SOC
	smmu->avp_vector_page = alloc_page(GFP_KERNEL);
	if (!smmu->avp_vector_page)
		goto fail;
#endif
	smmu_debugfs_create(smmu);

	return 0;

fail:
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
	if (smmu->avp_vector_page)
		__free_page(smmu->avp_vector_page);
#endif
	if (smmu && smmu->as) {
		for (asid = 0; asid < smmu->num_ases; asid++) {
			if (smmu->as[asid].pdir_page) {
				ClearPageReserved(smmu->as[asid].pdir_page);
				__free_page(smmu->as[asid].pdir_page);
			}
		}
		kfree(smmu->as);
	}
	for (i = 0; i < _REGS; i++) {
		if (smmu->regs[i]) {
			iounmap(smmu->regs[i]);
			smmu->regs[i] = NULL;
		}
	}
	kfree(smmu);
	return e;
}

static struct platform_driver tegra_iovmm_smmu_drv = {
	.probe = smmu_probe,
	.remove = smmu_remove,
	.driver = {
		.name = DRIVER_NAME,
	},
};

static int __devinit smmu_init(void)
{
	return platform_driver_register(&tegra_iovmm_smmu_drv);
}

static void __exit smmu_exit(void)
{
	platform_driver_unregister(&tegra_iovmm_smmu_drv);
}

subsys_initcall(smmu_init);
module_exit(smmu_exit);

/*
 * SMMU-global sysfs interface for debugging
 */
static ssize_t _sysfs_show_reg(struct device *d,
				struct device_attribute *da, char *buf);
static ssize_t _sysfs_store_reg(struct device *d,
				struct device_attribute *da, const char *buf,
				size_t count);

#define _NAME_MAP_SUFFIX(_name, base, suffix)	{	\
	.name = __stringify(_name) suffix,	\
	.offset = _name##_0,		\
	.regbase = (base),		\
	.dev_attr = __ATTR(_name, S_IRUGO | S_IWUSR,	\
			_sysfs_show_reg, _sysfs_store_reg)	\
}
#define _NAME_MAP(_name, base)	_NAME_MAP_SUFFIX(_name, base, "")

static
struct _reg_name_map {
	const char *name;
	size_t	offset;
	unsigned regbase;
	struct device_attribute	dev_attr;
} _smmu_reg_name_map[] = {
	_NAME_MAP(MC_INTSTATUS, _MC),
	_NAME_MAP(MC_ERR_STATUS, _MC),
	_NAME_MAP(MC_ERR_ADR, _MC),

	_NAME_MAP(MC_SMMU_CONFIG, _MC),
	_NAME_MAP(MC_SMMU_TLB_CONFIG, _MC),
	_NAME_MAP(MC_SMMU_PTC_CONFIG, _MC),
	_NAME_MAP(MC_SMMU_PTB_ASID, _MC),
	_NAME_MAP(MC_SMMU_PTB_DATA, _MC),
	_NAME_MAP(MC_SMMU_TLB_FLUSH, _MC),
	_NAME_MAP(MC_SMMU_PTC_FLUSH, _MC),
	_NAME_MAP(MC_SMMU_ASID_SECURITY, _MC),
	_NAME_MAP(MC_EMEM_CFG, _MC),
	_NAME_MAP(MC_SECURITY_CFG0, _MC),
	_NAME_MAP(MC_SECURITY_CFG1, _MC),
	_NAME_MAP(MC_SECURITY_CFG2, _MC),
	_NAME_MAP(MC_SECURITY_RSV, _MC),
	_NAME_MAP(MC_SMMU_STATS_TLB_HIT_COUNT, _MC),
	_NAME_MAP(MC_SMMU_STATS_TLB_MISS_COUNT, _MC),
	_NAME_MAP(MC_SMMU_STATS_PTC_HIT_COUNT, _MC),
	_NAME_MAP(MC_SMMU_STATS_PTC_MISS_COUNT, _MC),
#ifdef TEGRA_MC0_BASE
	_NAME_MAP_SUFFIX(MC_SMMU_STATS_TLB_HIT_COUNT, _MC0, ".0"),
	_NAME_MAP_SUFFIX(MC_SMMU_STATS_TLB_MISS_COUNT, _MC0, ".0"),
	_NAME_MAP_SUFFIX(MC_SMMU_STATS_PTC_HIT_COUNT, _MC0, ".0"),
	_NAME_MAP_SUFFIX(MC_SMMU_STATS_PTC_MISS_COUNT, _MC0, ".0"),
#endif
#ifdef TEGRA_MC1_BASE
	_NAME_MAP_SUFFIX(MC_SMMU_STATS_TLB_HIT_COUNT, _MC1, ".1"),
	_NAME_MAP_SUFFIX(MC_SMMU_STATS_TLB_MISS_COUNT, _MC1, ".1"),
	_NAME_MAP_SUFFIX(MC_SMMU_STATS_PTC_HIT_COUNT, _MC1, ".1"),
	_NAME_MAP_SUFFIX(MC_SMMU_STATS_PTC_MISS_COUNT, _MC1, ".1"),
#endif
	_NAME_MAP(MC_SMMU_TRANSLATION_ENABLE_0, _MC),
	_NAME_MAP(MC_SMMU_TRANSLATION_ENABLE_1, _MC),
	_NAME_MAP(MC_SMMU_TRANSLATION_ENABLE_2, _MC),

	_NAME_MAP(MC_STAT_CONTROL, _MC),
	_NAME_MAP(MC_STAT_EMC_CLOCKS, _MC),
	_NAME_MAP(MC_STAT_EMC_FILTER_SET0_ADR_LIMIT_LO, _MC),
	_NAME_MAP(MC_STAT_EMC_FILTER_SET0_ADR_LIMIT_HI, _MC),
	_NAME_MAP(MC_STAT_EMC_FILTER_SET0_CLIENT_0, _MC),
	_NAME_MAP(MC_STAT_EMC_FILTER_SET0_CLIENT_1, _MC),
	_NAME_MAP(MC_STAT_EMC_FILTER_SET0_CLIENT_2, _MC),
	_NAME_MAP(MC_STAT_EMC_SET0_COUNT, _MC),
	_NAME_MAP(MC_STAT_EMC_SET0_COUNT_MSBS, _MC),
	_NAME_MAP(MC_STAT_EMC_FILTER_SET1_ADR_LIMIT_LO, _MC),
	_NAME_MAP(MC_STAT_EMC_FILTER_SET1_ADR_LIMIT_HI, _MC),
	_NAME_MAP(MC_STAT_EMC_FILTER_SET1_CLIENT_0, _MC),
	_NAME_MAP(MC_STAT_EMC_FILTER_SET1_CLIENT_1, _MC),
	_NAME_MAP(MC_STAT_EMC_FILTER_SET1_CLIENT_2, _MC),
	_NAME_MAP(MC_STAT_EMC_SET1_COUNT, _MC),
	_NAME_MAP(MC_STAT_EMC_SET1_COUNT_MSBS, _MC),
	_NAME_MAP(MC_STAT_EMC_FILTER_SET0_VIRTUAL_ADR_LIMIT_LO, _MC),
	_NAME_MAP(MC_STAT_EMC_FILTER_SET0_VIRTUAL_ADR_LIMIT_HI, _MC),
	_NAME_MAP(MC_STAT_EMC_FILTER_SET0_ASID, _MC),
	_NAME_MAP(MC_STAT_EMC_FILTER_SET1_VIRTUAL_ADR_LIMIT_LO, _MC),
	_NAME_MAP(MC_STAT_EMC_FILTER_SET1_VIRTUAL_ADR_LIMIT_HI, _MC),
	_NAME_MAP(MC_STAT_EMC_FILTER_SET1_ASID, _MC),
#define op(c)	_NAME_MAP(MC_SMMU_##c##_ASID, _MC),
	SMMU_HWC
#undef op
	_NAME_MAP(AHB_ARBITRATION_XBAR_CTRL, _AHBARB),
#ifdef AHB_MASTER_SWID_0
	_NAME_MAP(AHB_MASTER_SWID, _AHBARB),
#endif
#ifdef APBDMA_CHANNEL_SWID_0
	_NAME_MAP(APBDMA_CHANNEL_SWID, _APBDMA),
#endif
};

static struct _reg_name_map *lookup_reg(struct device_attribute *da)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(_smmu_reg_name_map); i++) {
		if (!strcmp(_smmu_reg_name_map[i].name, da->attr.name))
			return &_smmu_reg_name_map[i];
	}
	return NULL;
}

static ssize_t _sysfs_show_reg(struct device *d,
					struct device_attribute *da, char *buf)
{
	struct smmu_device *smmu =
		container_of(d, struct smmu_device, sysfs_dev);
	struct _reg_name_map *reg = lookup_reg(da);

	if (!reg)
		return -ENODEV;
	return sprintf(buf, "%08lx @%08lx\n",
		(unsigned long)readl(smmu->regs[reg->regbase] + reg->offset),
		tegra_reg[reg->regbase].base + reg->offset);
}

#ifdef CONFIG_TEGRA_IOVMM_SMMU_SYSFS
#define good_challenge(smmu)	((smmu->challenge_pid = 0), 1)
#else
static inline int good_challenge(struct smmu_device *smmu)
{
	int ok = (smmu->challenge_pid == current->pid);
	smmu->challenge_pid = 0;
	return ok;
}
#endif

static ssize_t _sysfs_store_reg(struct device *d,
			struct device_attribute *da,
			const char *buf, size_t count)
{
	struct smmu_device *smmu =
		container_of(d, struct smmu_device, sysfs_dev);
	struct _reg_name_map *reg = lookup_reg(da);
	unsigned long value;

	if (!reg)
		return -ENODEV;
	if (kstrtoul(buf, 16, &value))
		return count;
	if (good_challenge(smmu))
		writel(value, smmu->regs[reg->regbase] + reg->offset);
	else if (reg->regbase == _MC) {
		unsigned long mask = 0;
		switch (reg->offset) {
		case MC_SMMU_TLB_CONFIG_0:
			mask = MC_SMMU_TLB_CONFIG_0_TLB_STATS_ENABLE__MASK |
				MC_SMMU_TLB_CONFIG_0_TLB_STATS_TEST__MASK;
			break;
		case MC_SMMU_PTC_CONFIG_0:
			mask = MC_SMMU_PTC_CONFIG_0_PTC_STATS_ENABLE__MASK |
				MC_SMMU_PTC_CONFIG_0_PTC_STATS_TEST__MASK;
			break;
		default:
			break;
		}

		if (mask) {
			unsigned long currval =
				(unsigned long)readl(smmu->regs[reg->regbase] +
						reg->offset);
			currval &= ~mask;
			value &= mask;
			value |= currval;
			writel(value, smmu->regs[reg->regbase] + reg->offset);
		}
	}
	return count;
}

static ssize_t _sysfs_show_smmu(struct device *d,
				struct device_attribute *da, char *buf)
{
	struct smmu_device *smmu =
		container_of(d, struct smmu_device, sysfs_dev);
	ssize_t	rv = 0;
	int asid;

	rv += sprintf(buf + rv , "    regs_mc: %p @%8lx\n",
				smmu->regs_mc, tegra_reg[_MC].base);
#ifdef TEGRA_MC0_BASE
	rv += sprintf(buf + rv , "   regs_mc0: %p @%8lx\n",
				smmu->regs_mc0, tegra_reg[_MC0].base);
#endif
#ifdef TEGRA_MC1_BASE
	rv += sprintf(buf + rv , "   regs_mc1: %p @%8lx\n",
				smmu->regs_mc1, tegra_reg[_MC1].base);
#endif
	rv += sprintf(buf + rv , "regs_ahbarb: %p @%8lx\n",
				smmu->regs_ahbarb, tegra_reg[_AHBARB].base);
	rv += sprintf(buf + rv , "regs_apbdma: %p @%8lx\n",
				smmu->regs_apbdma, tegra_reg[_APBDMA].base);
	rv += sprintf(buf + rv , " iovmm_base: %p\n", (void *)smmu->iovmm_base);
	rv += sprintf(buf + rv , " page_count: %8lx\n", smmu->page_count);
	rv += sprintf(buf + rv , "   num_ases: %d\n", smmu->num_ases);
	rv += sprintf(buf + rv , "         as: %p\n", smmu->as);
	for (asid = 0; asid < smmu->num_ases; asid++) {
		rv +=
	      sprintf(buf + rv , " ----- asid: %d\n", smmu->as[asid].asid);
		rv +=
	      sprintf(buf + rv , "  pdir_page: %p", smmu->as[asid].pdir_page);
		if (smmu->as[asid].pdir_page)
			rv +=
	      sprintf(buf + rv , " @%8lx\n",
			(unsigned long)page_to_phys(smmu->as[asid].pdir_page));
			else
			rv += sprintf(buf + rv , "\n");
	}
	rv += sprintf(buf + rv , "     enable: %s\n",
			smmu->enable ? "yes" : "no");
	return rv;
}

static struct device_attribute _attr_show_smmu
		 = __ATTR(show_smmu, S_IRUGO, _sysfs_show_smmu, NULL);

#define _SYSFS_SHOW_VALUE(name, field, fmt)		\
static ssize_t _sysfs_show_##name(struct device *d,	\
	struct device_attribute *da, char *buf)		\
{							\
	struct smmu_device *smmu =			\
		container_of(d, struct smmu_device, sysfs_dev);	\
	return sprintf(buf, fmt "\n", smmu->field);	\
}

static void (*_sysfs_null_callback)(struct smmu_device *, unsigned long *) =
	NULL;

#define _SYSFS_SET_VALUE_DO(name, field, base, ceil, callback, challenge) \
static ssize_t _sysfs_set_##name(struct device *d,		\
		struct device_attribute *da, const char *buf, size_t count) \
{								\
	unsigned long value;					\
	struct smmu_device *smmu =				\
		container_of(d, struct smmu_device, sysfs_dev);	\
	if (kstrtoul(buf, base, &value))			\
		return count;					\
	if (challenge && 0 <= value && value < ceil) {		\
		smmu->field = value;				\
		if (callback)					\
			callback(smmu, &smmu->field);		\
	}							\
	smmu->challenge_pid = 0;				\
	return count;						\
}
#ifdef CONFIG_TEGRA_IOVMM_SMMU_SYSFS
#define _SYSFS_SET_VALUE(name, field, base, ceil, callback)	\
	_SYSFS_SET_VALUE_DO(name, field, base, ceil, callback, 1)
#else
#define _SYSFS_SET_VALUE(name, field, base, ceil, callback)	\
	_SYSFS_SET_VALUE_DO(name, field, base, ceil, callback,	\
		(smmu->challenge_pid == current->pid))
#endif

_SYSFS_SHOW_VALUE(lowest_asid, lowest_asid, "%lu")
_SYSFS_SET_VALUE(lowest_asid, lowest_asid, 10,
		MC_SMMU_NUM_ASIDS, _sysfs_null_callback)
_SYSFS_SHOW_VALUE(debug_asid, debug_asid, "%lu")
_SYSFS_SET_VALUE(debug_asid, debug_asid, 10,
		MC_SMMU_NUM_ASIDS, _sysfs_null_callback)
_SYSFS_SHOW_VALUE(signature_pid, signature_pid, "%lu")
_SYSFS_SET_VALUE_DO(signature_pid, signature_pid, 10, PID_MAX_LIMIT+1,
		_sysfs_null_callback, 1)

/*
 * Protection for sysfs entries from accidental writing
 *   /sys/devices/smmu/chanllenge_code returns a random number.
 *   The process writes back pid^challenge to /sys/devices/smmu/challenge_code.
 *   The process will be able to alter a protected entry.
 *   The challenge code is reset.
 */
static ssize_t _sysfs_show_challenge_code(struct device *d,
	struct device_attribute *da, char *buf)
{
	struct smmu_device *smmu =
		container_of(d, struct smmu_device, sysfs_dev);
	smmu->challenge_pid = 0;
	smmu->challenge_code = random32();
	return sprintf(buf, "%lx\n", smmu->challenge_code);
}

static ssize_t _sysfs_set_challenge_code(struct device *d,
		struct device_attribute *da, const char *buf, size_t count)
{
	struct smmu_device *smmu =
		container_of(d, struct smmu_device, sysfs_dev);
	unsigned long value;
	if (!kstrtoul(buf, 16, &value)) {
		smmu->challenge_pid = smmu->challenge_code ^ value;
		smmu->challenge_code = random32();
	}
	return count;
}

/*
 * "echo 's d' > /sys/devices/smmu/copy_pdir" copies ASID s's pdir pointer
 * to ASID d. -1 as s resets d's pdir to null.
 */
static ssize_t _sysfs_copy_pdir(struct device *d,
		struct device_attribute *da, const char *buf, size_t count)
{
	struct smmu_device *smmu =
		container_of(d, struct smmu_device, sysfs_dev);
	long fr, to;

	if (kstrtol(buf, 16, &fr))
		return count;
	while (isxdigit(*buf))
		buf++;
	while (isspace(*buf))
		buf++;
	if (kstrtol(buf, 16, &to))
		return count;

	if (good_challenge(smmu) && fr != to &&
		fr < smmu->num_ases && 0 <= to && to < smmu->num_ases) {
		spin_lock(&smmu->lock);
		writel(MC_SMMU_PTB_ASID_0_CURRENT_ASID(to),
			smmu->regs_mc + MC_SMMU_PTB_ASID_0);
		writel((fr >= 0)
		? SMMU_MK_PDIR(smmu->as[fr].pdir_page, smmu->as[fr].pdir_attr)
		: MC_SMMU_PTB_DATA_0_RESET_VAL,
			smmu->regs_mc + MC_SMMU_PTB_DATA_0);
		smmu->as[to].pdir_page = (fr >= 0) ? smmu->as[fr].pdir_page : 0;
		spin_unlock(&smmu->lock);
	}
	return count;
}

static void _sysfs_mask_attr(struct smmu_device *smmu, unsigned long *field)
{
	*field &= _MASK_ATTR;
}

static void _sysfs_mask_pdir_attr(struct smmu_device *smmu,
	unsigned long *field)
{
	unsigned long pdir;

	_sysfs_mask_attr(smmu, field);
	spin_lock(&smmu->lock);
	writel(MC_SMMU_PTB_ASID_0_CURRENT_ASID(smmu->debug_asid),
		smmu->regs_mc + MC_SMMU_PTB_ASID_0);
	pdir = readl(smmu->regs_mc + MC_SMMU_PTB_DATA_0);
	pdir &= ~_MASK_ATTR;
	pdir |= *field;
	writel(pdir, smmu->regs_mc + MC_SMMU_PTB_DATA_0);
	spin_unlock(&smmu->lock);
	flush_smmu_regs(smmu);
}

static void (*_sysfs_mask_attr_callback)(struct smmu_device *,
				unsigned long *field) = &_sysfs_mask_attr;
static void (*_sysfs_mask_pdir_attr_callback)(struct smmu_device *,
				unsigned long *field) = &_sysfs_mask_pdir_attr;

_SYSFS_SHOW_VALUE(pdir_attr, as[smmu->debug_asid].pdir_attr, "%lx")
_SYSFS_SET_VALUE(pdir_attr, as[smmu->debug_asid].pdir_attr, 16,
		_PDIR_ATTR + 1, _sysfs_mask_pdir_attr_callback)
_SYSFS_SHOW_VALUE(pde_attr, as[smmu->debug_asid].pde_attr, "%lx")
_SYSFS_SET_VALUE(pde_attr, as[smmu->debug_asid].pde_attr, 16,
		_PDE_ATTR + 1, _sysfs_mask_attr_callback)
_SYSFS_SHOW_VALUE(pte_attr, as[smmu->debug_asid].pte_attr, "%lx")
_SYSFS_SET_VALUE(pte_attr, as[smmu->debug_asid].pte_attr, 16,
		_PTE_ATTR + 1, _sysfs_mask_attr_callback)

static struct device_attribute _attr_values[] = {
	__ATTR(lowest_asid, S_IRUGO | S_IWUSR,
		_sysfs_show_lowest_asid, _sysfs_set_lowest_asid),
	__ATTR(debug_asid, S_IRUGO | S_IWUSR,
		_sysfs_show_debug_asid, _sysfs_set_debug_asid),
	__ATTR(signature_pid, S_IRUGO | S_IWUSR,
		_sysfs_show_signature_pid, _sysfs_set_signature_pid),
	__ATTR(challenge_code, S_IRUGO | S_IWUSR,
		_sysfs_show_challenge_code, _sysfs_set_challenge_code),
	__ATTR(copy_pdir, S_IWUSR, NULL, _sysfs_copy_pdir),

	__ATTR(pdir_attr, S_IRUGO | S_IWUSR,
		_sysfs_show_pdir_attr, _sysfs_set_pdir_attr),
	__ATTR(pde_attr, S_IRUGO | S_IWUSR,
		_sysfs_show_pde_attr, _sysfs_set_pde_attr),
	__ATTR(pte_attr, S_IRUGO | S_IWUSR,
		_sysfs_show_pte_attr, _sysfs_set_pte_attr),
};

static struct attribute *_smmu_attrs[
	ARRAY_SIZE(_smmu_reg_name_map) + ARRAY_SIZE(_attr_values) + 3];
static struct attribute_group _smmu_attr_group = {
	.attrs = _smmu_attrs
};

static void _sysfs_smmu(struct smmu_device *smmu, struct device *parent)
{
	int i, j;

	if (smmu->sysfs_use_count++ > 0)
		return;
	for (i = 0; i < ARRAY_SIZE(_smmu_reg_name_map); i++) {
		attr_name(_smmu_reg_name_map[i].dev_attr) =
			_smmu_reg_name_map[i].name;
		_smmu_attrs[i] = &_smmu_reg_name_map[i].dev_attr.attr;
	}
	for (j = 0; j < ARRAY_SIZE(_attr_values); j++)
		_smmu_attrs[i++] = &_attr_values[j].attr;
	_smmu_attrs[i++] = &_attr_show_smmu.attr;
	_smmu_attrs[i] = NULL;

	dev_set_name(&smmu->sysfs_dev, "smmu");
	smmu->sysfs_dev.parent = parent;
	smmu->sysfs_dev.driver = NULL;
	smmu->sysfs_dev.release = NULL;
	if (device_register(&smmu->sysfs_dev)) {
		pr_err("%s: failed to register smmu_sysfs_dev\n", __func__);
		smmu->sysfs_use_count--;
		return;
	}
	if (sysfs_create_group(&smmu->sysfs_dev.kobj, &_smmu_attr_group)) {
		pr_err("%s: failed to create group for smmu_sysfs_dev\n",
			__func__);
		smmu->sysfs_use_count--;
		return;
	}
}

static void _sysfs_create(struct smmu_as *as, struct device *parent)
{
	_sysfs_smmu(as->smmu, parent);
}
