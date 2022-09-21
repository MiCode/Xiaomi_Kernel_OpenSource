/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * IOMMU API for ARM architected SMMU implementations.
 *
 * Copyright (C) 2013 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

#ifndef _ARM_SMMU_H
#define _ARM_SMMU_H

#include <linux/atomic.h>
#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/io-64-nonatomic-hi-lo.h>
#include <linux/io-pgtable.h>
#include <linux/iommu.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/types.h>

/* Configuration registers */
#define ARM_SMMU_GR0_sCR0		0x0
#define sCR0_VMID16EN			BIT(31)
#define sCR0_SHCFG			GENMASK(23, 22)
#define sCR0_SHCFG_NSH			0x3
#define sCR0_BSU			GENMASK(15, 14)
#define sCR0_FB				BIT(13)
#define sCR0_PTM			BIT(12)
#define sCR0_VMIDPNE			BIT(11)
#define sCR0_USFCFG			BIT(10)
#define sCR0_GCFGFIE			BIT(5)
#define sCR0_GCFGFRE			BIT(4)
#define sCR0_EXIDENABLE			BIT(3)
#define sCR0_GFIE			BIT(2)
#define sCR0_GFRE			BIT(1)
#define sCR0_CLIENTPD			BIT(0)

/* Auxiliary Configuration register */
#define ARM_SMMU_GR0_sACR		0x10

/* Identification registers */
#define ARM_SMMU_GR0_ID0		0x20
#define ID0_S1TS			BIT(30)
#define ID0_S2TS			BIT(29)
#define ID0_NTS				BIT(28)
#define ID0_SMS				BIT(27)
#define ID0_ATOSNS			BIT(26)
#define ID0_PTFS_NO_AARCH32		BIT(25)
#define ID0_PTFS_NO_AARCH32S		BIT(24)
#define ID0_NUMIRPT			GENMASK(23, 16)
#define ID0_CTTW			BIT(14)
#define ID0_NUMSIDB			GENMASK(12, 9)
#define ID0_EXIDS			BIT(8)
#define ID0_NUMSMRG			GENMASK(7, 0)

#define ARM_SMMU_GR0_ID1		0x24
#define ID1_PAGESIZE			BIT(31)
#define ID1_NUMPAGENDXB			GENMASK(30, 28)
#define ID1_NUMS2CB			GENMASK(23, 16)
#define ID1_NUMCB			GENMASK(7, 0)

#define ARM_SMMU_GR0_ID2		0x28
#define ID2_VMID16			BIT(15)
#define ID2_PTFS_64K			BIT(14)
#define ID2_PTFS_16K			BIT(13)
#define ID2_PTFS_4K			BIT(12)
#define ID2_UBS				GENMASK(11, 8)
#define ID2_OAS				GENMASK(7, 4)
#define ID2_IAS				GENMASK(3, 0)

#define ARM_SMMU_GR0_ID3		0x2c
#define ARM_SMMU_GR0_ID4		0x30
#define ARM_SMMU_GR0_ID5		0x34
#define ARM_SMMU_GR0_ID6		0x38

#define ARM_SMMU_GR0_ID7		0x3c
#define ID7_MAJOR			GENMASK(7, 4)
#define ID7_MINOR			GENMASK(3, 0)

#define ARM_SMMU_GR0_sGFSR		0x48
#define ARM_SMMU_GR0_sGFSYNR0		0x50
#define ARM_SMMU_GR0_sGFSYNR1		0x54
#define ARM_SMMU_GR0_sGFSYNR2		0x58

/* Global TLB invalidation */
#define ARM_SMMU_GR0_TLBIVMID		0x64
#define ARM_SMMU_GR0_TLBIALLNSNH	0x68
#define ARM_SMMU_GR0_TLBIALLH		0x6c
#define ARM_SMMU_GR0_sTLBGSYNC		0x70

#define ARM_SMMU_GR0_sTLBGSTATUS	0x74
#define sTLBGSTATUS_GSACTIVE		BIT(0)

/* Stream mapping registers */
#define ARM_SMMU_GR0_SMR(n)		(0x800 + ((n) << 2))
#define SMR_MASK_MASK			GENMASK(14, 0)
#define SID_MASK			GENMASK(14, 0)
#define SMR_VALID			BIT(31)
#define SMR_MASK			GENMASK(31, 16)
#define SMR_ID				GENMASK(15, 0)

#define ARM_SMMU_GR0_S2CR(n)		(0xc00 + ((n) << 2))
#define S2CR_SHCFG			GENMASK(9, 8)
#define S2CR_SHCFG_NSH			0x3

#define S2CR_PRIVCFG			GENMASK(25, 24)
enum arm_smmu_s2cr_privcfg {
	S2CR_PRIVCFG_DEFAULT,
	S2CR_PRIVCFG_DIPAN,
	S2CR_PRIVCFG_UNPRIV,
	S2CR_PRIVCFG_PRIV,
};
#define S2CR_TYPE			GENMASK(17, 16)
enum arm_smmu_s2cr_type {
	S2CR_TYPE_TRANS,
	S2CR_TYPE_BYPASS,
	S2CR_TYPE_FAULT,
};
#define S2CR_EXIDVALID			BIT(10)
#define S2CR_CBNDX			GENMASK(7, 0)

/* Context bank attribute registers */
#define ARM_SMMU_GR1_CBAR(n)		(0x0 + ((n) << 2))
#define CBAR_IRPTNDX			GENMASK(31, 24)
#define CBAR_TYPE			GENMASK(17, 16)
enum arm_smmu_cbar_type {
	CBAR_TYPE_S2_TRANS,
	CBAR_TYPE_S1_TRANS_S2_BYPASS,
	CBAR_TYPE_S1_TRANS_S2_FAULT,
	CBAR_TYPE_S1_TRANS_S2_TRANS,
};
#define CBAR_S1_MEMATTR			GENMASK(15, 12)
#define CBAR_S1_MEMATTR_WB		0xf
#define CBAR_S1_BPSHCFG			GENMASK(9, 8)
#define CBAR_S1_BPSHCFG_NSH		3
#define CBAR_VMID			GENMASK(7, 0)

#define ARM_SMMU_GR1_CBFRSYNRA(n)	(0x400 + ((n) << 2))
#define CBFRSYNRA_SID			GENMASK(15, 0)
#define CBFRSYNRA_SSD			GENMASK(31, 16)


#define ARM_SMMU_GR1_CBA2R(n)		(0x800 + ((n) << 2))
#define CBA2R_VMID16			GENMASK(31, 16)
#define CBA2R_VA64			BIT(0)

#define ARM_SMMU_CB_SCTLR		0x0
#define SCTLR_WACFG			GENMASK(27, 26)
#define SCTLR_WACFG_WA			0x2
#define SCTLR_RACFG			GENMASK(25, 24)
#define SCTLR_RACFG_RA			0x2
#define SCTLR_SHCFG			GENMASK(23, 22)
#define SCTLR_SHCFG_OSH			0x1
#define SCTLR_SHCFG_NSH			0x3
#define SCTLR_MTCFG			BIT(20)
#define SCTLR_MEM_ATTR			GENMASK(19, 16)
#define SCTLR_MEM_ATTR_OISH_WB_CACHE	0xf
#define SCTLR_S1_ASIDPNE		BIT(12)
#define SCTLR_HUPCF			BIT(8)
#define SCTLR_CFCFG			BIT(7)
#define SCTLR_CFIE			BIT(6)
#define SCTLR_CFRE			BIT(5)
#define SCTLR_E				BIT(4)
#define SCTLR_AFE			BIT(2)
#define SCTLR_TRE			BIT(1)
#define SCTLR_M				BIT(0)

#define ARM_SMMU_CB_ACTLR		0x4

#define ARM_SMMU_CB_RESUME		0x8
#define RESUME_TERMINATE		BIT(0)

#define ARM_SMMU_CB_TCR2		0x10
#define TCR2_SEP			GENMASK(17, 15)
#define TCR2_SEP_UPSTREAM		0x7
#define TCR2_AS				BIT(4)

#define ARM_SMMU_CB_TTBR0		0x20
#define ARM_SMMU_CB_TTBR1		0x28
#define TTBRn_ASID			GENMASK_ULL(63, 48)

#define ARM_SMMU_CB_TCR			0x30
#define TCR_TCR1			GENMASK(31, 16)
#define TCR1_TG1			GENMASK(31, 30)
#define TCR1_TG1_16K			0x1
#define TCR1_TG1_4K			0x2
#define TCR1_TG1_64K			0x3
#define TCR1_EPD1			BIT(23)
#define TCR_TCR0			GENMASK(15, 0)
#define TCR0_TG0			GENMASK(15, 14)
#define TCR0_TG0_4K			0x0
#define TCR0_TG0_64K			0x1
#define TCR0_TG0_16K			0x2
#define ARM_SMMU_CB_CONTEXTIDR		0x34
#define ARM_SMMU_CB_S1_MAIR0		0x38
#define ARM_SMMU_CB_S1_MAIR1		0x3c

#define ARM_SMMU_CB_PAR			0x50
#define CB_PAR_F			BIT(0)

#define ARM_SMMU_CB_FSR			0x58
#define FSR_MULTI			BIT(31)
#define FSR_SS				BIT(30)
#define FSR_UUT				BIT(8)
#define FSR_ASF				BIT(7)
#define FSR_TLBLKF			BIT(6)
#define FSR_TLBMCF			BIT(5)
#define FSR_EF				BIT(4)
#define FSR_PF				BIT(3)
#define FSR_AFF				BIT(2)
#define FSR_TF				BIT(1)

#define FSR_IGN				(FSR_AFF | FSR_ASF | \
					 FSR_TLBMCF | FSR_TLBLKF)
#define FSR_FAULT			(FSR_MULTI | FSR_SS | FSR_UUT | \
					 FSR_EF | FSR_PF | FSR_TF | FSR_IGN)

#define ARM_SMMU_CB_FSRRESTORE		0x5c
#define ARM_SMMU_CB_FAR			0x60

#define ARM_SMMU_CB_FSYNR0		0x68
#define FSYNR0_WNR			BIT(4)
#define FSYNR0_PNU			BIT(5)
#define FSYNR0_IND			BIT(6)
#define FSYNR0_NSATTR			BIT(8)

#define ARM_SMMU_CB_FSYNR1		0x6c
#define FSYNR1_BID			GENMASK(15, 13)
#define FSYNR1_PID			GENMASK(12, 8)
#define FSYNR1_MID			GENMASK(7, 0)

#define ARM_SMMU_CB_S1_TLBIVA		0x600
#define ARM_SMMU_CB_S1_TLBIASID		0x610
#define ARM_SMMU_CB_S1_TLBIALL		0x618
#define ARM_SMMU_CB_S1_TLBIVAL		0x620
#define ARM_SMMU_CB_S2_TLBIIPAS2	0x630
#define ARM_SMMU_CB_S2_TLBIIPAS2L	0x638
#define ARM_SMMU_CB_TLBSYNC		0x7f0
#define ARM_SMMU_CB_TLBSTATUS		0x7f4
#define TLBSTATUS_SACTIVE		BIT(0)
#define ARM_SMMU_CB_ATS1PR		0x800

/* Implementation Defined Register Space 5 registers */
/* Relative to IMPL_DEF5 page */
#define ARM_SMMU_STATS_SYNC_INV_TBU_ACK 0x5dc
#define TBU_SYNC_ACK			GENMASK(25, 17)
#define TBU_SYNC_REQ			BIT(16)
#define TBU_INV_ACK			GENMASK(9, 1)
#define TBU_INV_REQ			BIT(0)

/* Relative to SMMU_BASE */
#define ARM_SMMU_TBU_PWR_STATUS         0x2204

/* Relative SMMU_BASE */
#define ARM_SMMU_MMU2QSS_AND_SAFE_WAIT_CNTR 0x2670
#define TCU_SYNC_IN_PRGSS		BIT(20)
#define TCU_INV_IN_PRGSS		BIT(16)

#define ARM_SMMU_CB_ATSR		0x8f0
#define ATSR_ACTIVE			BIT(0)


/* Maximum number of context banks per SMMU */
#define ARM_SMMU_MAX_CBS		128


/* Shared driver definitions */
enum arm_smmu_arch_version {
	ARM_SMMU_V1,
	ARM_SMMU_V1_64K,
	ARM_SMMU_V2,
};

enum arm_smmu_implementation {
	GENERIC_SMMU,
	ARM_MMU500,
	CAVIUM_SMMUV2,
	QCOM_SMMUV2,
	QCOM_SMMUV500,
};

/*
 * Describes resources required for on/off power operation.
 * Separate reference count is provided for atomic/nonatomic
 * operations.
 */
struct arm_smmu_power_resources {
	struct platform_device		*pdev;
	struct device			*dev;

	struct clk			**clocks;
	int				num_clocks;

	struct regulator_bulk_data	*gdscs;
	int				num_gdscs;

	struct icc_path			*icc_path;

	/* Protects power_count */
	struct mutex			power_lock;
	int				power_count;

	/* Protects clock_refs_count */
	spinlock_t			clock_refs_lock;
	int				clock_refs_count;
	int				regulator_defer;
};

/*
 * attach_count
 *	The SMR and S2CR registers are only programmed when the number of
 *	devices attached to the iommu using these registers is > 0. This
 *	is required for the "SID switch" use case for secure display.
 *	Protected by stream_map_mutex.
 */
struct arm_smmu_s2cr {
	struct iommu_group		*group;
	int				count;
	int				attach_count;
	enum arm_smmu_s2cr_type		type;
	enum arm_smmu_s2cr_privcfg	privcfg;
	u8				cbndx;
	bool				cb_handoff;
	bool				pinned;
};

struct arm_smmu_smr {
	u16				mask;
	u16				id;
	bool				valid;
};

struct arm_smmu_device {
	struct device			*dev;

	void __iomem			*base;
	phys_addr_t			phys_addr;
	unsigned int			numpage;
	unsigned int			pgshift;

#define ARM_SMMU_FEAT_COHERENT_WALK	(1 << 0)
#define ARM_SMMU_FEAT_STREAM_MATCH	(1 << 1)
#define ARM_SMMU_FEAT_TRANS_S1		(1 << 2)
#define ARM_SMMU_FEAT_TRANS_S2		(1 << 3)
#define ARM_SMMU_FEAT_TRANS_NESTED	(1 << 4)
#define ARM_SMMU_FEAT_TRANS_OPS		(1 << 5)
#define ARM_SMMU_FEAT_VMID16		(1 << 6)
#define ARM_SMMU_FEAT_FMT_AARCH64_4K	(1 << 7)
#define ARM_SMMU_FEAT_FMT_AARCH64_16K	(1 << 8)
#define ARM_SMMU_FEAT_FMT_AARCH64_64K	(1 << 9)
#define ARM_SMMU_FEAT_FMT_AARCH32_L	(1 << 10)
#define ARM_SMMU_FEAT_FMT_AARCH32_S	(1 << 11)
#define ARM_SMMU_FEAT_EXIDS		(1 << 12)
	u32				features;

#define ARM_SMMU_OPT_FATAL_ASF		(1 << 0)
#define ARM_SMMU_OPT_SKIP_INIT		(1 << 1)
#define ARM_SMMU_OPT_3LVL_TABLES	(1 << 2)
#define ARM_SMMU_OPT_NO_ASID_RETENTION	(1 << 3)
#define ARM_SMMU_OPT_DISABLE_ATOS	(1 << 4)
#define ARM_SMMU_OPT_SPLIT_TABLES	(1 << 5)
	u32				options;
	enum arm_smmu_arch_version	version;
	enum arm_smmu_implementation	model;
	const struct arm_smmu_impl	*impl;

	u32				num_context_banks;
	u32				num_s2_context_banks;
	DECLARE_BITMAP(context_map, ARM_SMMU_MAX_CBS);
	struct arm_smmu_cb		*cbs;
	atomic_t			irptndx;

	u32				num_mapping_groups;
	u16				streamid_mask;
	u16				smr_mask_mask;
	struct arm_smmu_smr		*smrs;
	struct arm_smmu_s2cr		*s2crs;
	struct mutex			stream_map_mutex;
	struct mutex			iommu_group_mutex;
	unsigned long			va_size;
	unsigned long			ipa_size;
	unsigned long			pa_size;
	unsigned long			pgsize_bitmap;

	u32				num_global_irqs;
	u32				num_context_irqs;
	unsigned int			*irqs;
	struct clk_bulk_data		*clks;
	int				num_clks;

	struct list_head		list;

	spinlock_t			global_sync_lock;

	/* IOMMU core code handle */
	struct iommu_device		iommu;

	struct arm_smmu_power_resources *pwr;

	/* protects idr */
	struct mutex			idr_mutex;
	struct idr			asid_idr;

	unsigned long			sync_timed_out;
};

struct qsmmuv500_tbu_device {
	struct list_head		list;
	struct device			*dev;
	struct arm_smmu_device		*smmu;
	void __iomem			*base;
	void __iomem			*status_reg;

	struct arm_smmu_power_resources *pwr;
	u32				sid_start;
	u32				num_sids;

	/* Protects halt count */
	spinlock_t			halt_lock;
	u32				halt_count;
	unsigned int			*irqs;
};

struct arm_smmu_master_cfg {
	struct arm_smmu_device		*smmu;
	s16				smendx[];
};

#define INVALID_SMENDX			-1
#define __fwspec_cfg(fw) ((struct arm_smmu_master_cfg *)fw->iommu_priv)
#define fwspec_smmu(fw)  (__fwspec_cfg(fw)->smmu)
#define fwspec_smendx(fw, i) \
	(i >= fw->num_ids ? INVALID_SMENDX : __fwspec_cfg(fw)->smendx[i])
#define for_each_cfg_sme(fw, i, idx) \
	for (i = 0; idx = fwspec_smendx(fw, i), i < fw->num_ids; ++i)

enum arm_smmu_context_fmt {
	ARM_SMMU_CTX_FMT_NONE,
	ARM_SMMU_CTX_FMT_AARCH64,
	ARM_SMMU_CTX_FMT_AARCH32_L,
	ARM_SMMU_CTX_FMT_AARCH32_S,
};

struct arm_smmu_cfg {
	u8				cbndx;
	u8				irptndx;
	union {
		u16			asid;
		u16			vmid;
	};
	u32				procid;
	enum arm_smmu_cbar_type		cbar;
	enum arm_smmu_context_fmt	fmt;
};
#define INVALID_IRPTNDX			0xff

enum arm_smmu_domain_stage {
	ARM_SMMU_DOMAIN_S1 = 0,
	ARM_SMMU_DOMAIN_S2,
	ARM_SMMU_DOMAIN_NESTED,
	ARM_SMMU_DOMAIN_BYPASS,
};

struct arm_smmu_flush_ops {
	struct iommu_flush_ops		tlb;
	void (*tlb_inv_range)(unsigned long iova, size_t size, size_t granule,
			      bool leaf, void *cookie);
	void (*tlb_sync)(void *cookie);
};

struct arm_smmu_domain {
	struct arm_smmu_device		*smmu;
	struct device			*dev;
	struct io_pgtable_ops		*pgtbl_ops[2];
	const struct arm_smmu_flush_ops	*flush_ops;
	struct arm_smmu_cfg		cfg;
	enum arm_smmu_domain_stage	stage;
	bool				non_strict;
	struct mutex			init_mutex; /* Protects smmu pointer */
	spinlock_t			cb_lock; /* Serialises ATS1* ops */
	spinlock_t			sync_lock; /* Serialises TLB syncs */
	struct msm_io_pgtable_info	pgtbl_info[2];
	DECLARE_BITMAP(attributes, DOMAIN_ATTR_EXTENDED_MAX);
	u32				secure_vmid;
	struct list_head		pte_info_list;
	struct list_head		unassign_list;
	struct mutex			assign_lock;
	struct list_head		secure_pool_list;
	/* nonsecure pool protected by pgtbl_lock */
	struct list_head		nonsecure_pool;
	struct iommu_debug_attachment	*logger;
	struct msm_iommu_domain		domain;
	bool				defer_flush;
};


/* Implementation details, yay! */

struct arm_smmu_impl {
	u32 (*read_reg)(struct arm_smmu_device *smmu, int page, int offset);
	void (*write_reg)(struct arm_smmu_device *smmu, int page, int offset,
			  u32 val);
	u64 (*read_reg64)(struct arm_smmu_device *smmu, int page, int offset);
	void (*write_reg64)(struct arm_smmu_device *smmu, int page, int offset,
			    u64 val);
	int (*cfg_probe)(struct arm_smmu_device *smmu);
	int (*reset)(struct arm_smmu_device *smmu);
	int (*init_context)(struct arm_smmu_domain *smmu_domain);
	void (*init_context_bank)(struct arm_smmu_domain *smmu_domain,
				  struct device *dev);
	phys_addr_t (*iova_to_phys_hard)(struct arm_smmu_domain *smmu_domain,
					 dma_addr_t iova,
					 unsigned long trans_flags);
	void (*tlb_sync_timeout)(struct arm_smmu_device *smmu);
	void (*device_remove)(struct arm_smmu_device *smmu);
	int (*device_group)(struct device *dev, struct iommu_group *group);
};

static inline void __iomem *arm_smmu_page(struct arm_smmu_device *smmu, int n)
{
	return smmu->base + (n << smmu->pgshift);
}

static inline u32 arm_smmu_readl(struct arm_smmu_device *smmu, int page, int offset)
{
	if (smmu->impl && unlikely(smmu->impl->read_reg))
		return smmu->impl->read_reg(smmu, page, offset);
	return readl_relaxed(arm_smmu_page(smmu, page) + offset);
}

static inline void arm_smmu_writel(struct arm_smmu_device *smmu, int page,
				   int offset, u32 val)
{
	if (smmu->impl && unlikely(smmu->impl->write_reg))
		smmu->impl->write_reg(smmu, page, offset, val);
	else
		writel_relaxed(val, arm_smmu_page(smmu, page) + offset);
}

static inline u64 arm_smmu_readq(struct arm_smmu_device *smmu, int page, int offset)
{
	if (smmu->impl && unlikely(smmu->impl->read_reg64))
		return smmu->impl->read_reg64(smmu, page, offset);
	return readq_relaxed(arm_smmu_page(smmu, page) + offset);
}

static inline void arm_smmu_writeq(struct arm_smmu_device *smmu, int page,
				   int offset, u64 val)
{
	if (smmu->impl && unlikely(smmu->impl->write_reg64))
		smmu->impl->write_reg64(smmu, page, offset, val);
	else
		writeq_relaxed(val, arm_smmu_page(smmu, page) + offset);
}

#define ARM_SMMU_GR0		0
#define ARM_SMMU_GR1		1

/*
 * Implementation defined space starts after SMMU GR space, so IMPL_DEF page n
 * is page n + 2 in the SMMU register space.
 */
#define ARM_SMMU_IMPL_DEF0	2
#define ARM_SMMU_IMPL_DEF4	6
#define ARM_SMMU_IMPL_DEF5	7

#define ARM_SMMU_CB(s, n)	((s)->numpage + (n))

#define arm_smmu_gr0_read(s, o)		\
	arm_smmu_readl((s), ARM_SMMU_GR0, (o))
#define arm_smmu_gr0_write(s, o, v)	\
	arm_smmu_writel((s), ARM_SMMU_GR0, (o), (v))

#define arm_smmu_gr1_read(s, o)		\
	arm_smmu_readl((s), ARM_SMMU_GR1, (o))
#define arm_smmu_gr1_write(s, o, v)	\
	arm_smmu_writel((s), ARM_SMMU_GR1, (o), (v))

#define arm_smmu_cb_read(s, n, o)	\
	arm_smmu_readl((s), ARM_SMMU_CB((s), (n)), (o))
#define arm_smmu_cb_write(s, n, o, v)	\
	arm_smmu_writel((s), ARM_SMMU_CB((s), (n)), (o), (v))
#define arm_smmu_cb_readq(s, n, o)	\
	arm_smmu_readq((s), ARM_SMMU_CB((s), (n)), (o))
#define arm_smmu_cb_writeq(s, n, o, v)	\
	arm_smmu_writeq((s), ARM_SMMU_CB((s), (n)), (o), (v))

struct arm_smmu_device *arm_smmu_impl_init(struct arm_smmu_device *smmu);
struct arm_smmu_device *qcom_smmu_impl_init(struct arm_smmu_device *smmu);
struct arm_smmu_device *qsmmuv500_impl_init(struct arm_smmu_device *smmu);
struct arm_smmu_device *qsmmuv2_impl_init(struct arm_smmu_device *smmu);

int arm_mmu500_reset(struct arm_smmu_device *smmu);

int arm_smmu_power_on(struct arm_smmu_power_resources *pwr);
void arm_smmu_power_off(struct arm_smmu_device *smmu,
			struct arm_smmu_power_resources *pwr);

/* Misc. constants */
#define TBUID_SHIFT                     10
#define ARM_MMU500_ACR_CACHE_LOCK	(1 << 26)

#endif /* _ARM_SMMU_H */
