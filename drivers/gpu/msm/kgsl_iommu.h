/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef __KGSL_IOMMU_H
#define __KGSL_IOMMU_H

#include <linux/adreno-smmu-priv.h>
#include <linux/io-pgtable.h>
/*
 * These defines control the address range for allocations that
 * are mapped into all pagetables.
 */
#define KGSL_IOMMU_GLOBAL_MEM_SIZE	(20 * SZ_1M)
#define KGSL_IOMMU_GLOBAL_MEM_BASE32	0xf8000000
#define KGSL_IOMMU_GLOBAL_MEM_BASE64	\
	(KGSL_MEMSTORE_TOKEN_ADDRESS - KGSL_IOMMU_GLOBAL_MEM_SIZE)

/*
 * This is a dummy token address that we use to identify memstore when the user
 * wants to map it. mmap() uses a unsigned long for the offset so we need a 32
 * bit value that works with all sized apps. We chose a value that was purposely
 * unmapped so if you increase the global memory size make sure it doesn't
 * conflict
 */

#define KGSL_MEMSTORE_TOKEN_ADDRESS	(KGSL_IOMMU_SECURE_BASE32 - SZ_4K)

#define KGSL_IOMMU_GLOBAL_MEM_BASE(__mmu)	\
	(test_bit(KGSL_MMU_64BIT, &(__mmu)->features) ? \
		KGSL_IOMMU_GLOBAL_MEM_BASE64 : KGSL_IOMMU_GLOBAL_MEM_BASE32)

#define KGSL_IOMMU_SVM_BASE32(__mmu)	\
	(ADRENO_DEVICE(KGSL_MMU_DEVICE(__mmu))->uche_gmem_base + \
		ADRENO_DEVICE(KGSL_MMU_DEVICE(__mmu))->gpucore->gmem_size)

#define KGSL_IOMMU_SVM_END32		(0xC0000000 - SZ_16M)

/*
 * Limit secure size to 256MB for 32bit kernels.
 */
#define KGSL_IOMMU_SECURE_SIZE32 SZ_256M
#define KGSL_IOMMU_SECURE_BASE32	\
	(KGSL_IOMMU_SECURE_BASE64 - KGSL_IOMMU_SECURE_SIZE32)
#define KGSL_IOMMU_SECURE_END32 KGSL_IOMMU_SECURE_BASE64

#define KGSL_IOMMU_SECURE_BASE64	0x100000000ULL
#define KGSL_IOMMU_SECURE_END64	\
	(KGSL_IOMMU_SECURE_BASE64 + KGSL_IOMMU_SECURE_SIZE64)

#define KGSL_IOMMU_MAX_SECURE_SIZE 0xFFFFF000

#define KGSL_IOMMU_SECURE_SIZE64	\
	(KGSL_IOMMU_MAX_SECURE_SIZE - KGSL_IOMMU_SECURE_SIZE32)

#define KGSL_IOMMU_SECURE_BASE(_mmu) (test_bit(KGSL_MMU_64BIT, \
			&(_mmu)->features) ? KGSL_IOMMU_SECURE_BASE64 : \
			KGSL_IOMMU_SECURE_BASE32)
#define KGSL_IOMMU_SECURE_END(_mmu) (test_bit(KGSL_MMU_64BIT, \
			&(_mmu)->features) ? KGSL_IOMMU_SECURE_END64 : \
			KGSL_IOMMU_SECURE_END32)
#define KGSL_IOMMU_SECURE_SIZE(_mmu) (test_bit(KGSL_MMU_64BIT, \
			&(_mmu)->features) ? KGSL_IOMMU_MAX_SECURE_SIZE : \
			KGSL_IOMMU_SECURE_SIZE32)

/* The CPU supports 39 bit addresses */
#define KGSL_IOMMU_SVM_BASE64		0x1000000000ULL
#define KGSL_IOMMU_SVM_END64		0x4000000000ULL
#define KGSL_IOMMU_VA_BASE64		0x4000000000ULL
#define KGSL_IOMMU_VA_END64		0x8000000000ULL

#define CP_APERTURE_REG			0
#define CP_SMMU_APERTURE_ID		0x1B

/* Global SMMU register offsets */
#define KGSL_IOMMU_PRR_CFG_LADDR	0x6008
#define KGSL_IOMMU_PRR_CFG_UADDR	0x600c

/* Register offsets */
#define KGSL_IOMMU_CTX_SCTLR		0x0000
#define KGSL_IOMMU_CTX_ACTLR		0x0004
#define KGSL_IOMMU_CTX_TTBR0		0x0020
#define KGSL_IOMMU_CTX_CONTEXTIDR	0x0034
#define KGSL_IOMMU_CTX_FSR		0x0058
#define KGSL_IOMMU_CTX_TLBIALL		0x0618
#define KGSL_IOMMU_CTX_RESUME		0x0008
#define KGSL_IOMMU_CTX_FSYNR0		0x0068
#define KGSL_IOMMU_CTX_FSYNR1		0x006c
#define KGSL_IOMMU_CTX_TLBSYNC		0x07f0
#define KGSL_IOMMU_CTX_TLBSTATUS	0x07f4

/* TLBSTATUS register fields */
#define KGSL_IOMMU_CTX_TLBSTATUS_SACTIVE BIT(0)

/* SCTLR fields */
#define KGSL_IOMMU_SCTLR_HUPCF_SHIFT		8
#define KGSL_IOMMU_SCTLR_CFCFG_SHIFT		7
#define KGSL_IOMMU_SCTLR_CFIE_SHIFT		6

#define KGSL_IOMMU_ACTLR_PRR_ENABLE		BIT(5)

/* FSR fields */
#define KGSL_IOMMU_FSR_SS_SHIFT		30

/* ASID field in TTBR register */
#define KGSL_IOMMU_ASID_START_BIT	48

/* offset at which a nop command is placed in setstate */
#define KGSL_IOMMU_SETSTATE_NOP_OFFSET	1024

/*
 * struct kgsl_iommu_context - Structure holding data about an iommu context
 * bank
 * @pdev: pointer to the iommu context's platform device
 * @name: context name
 * @id: The id of the context, used for deciding how it is used.
 * @cb_num: The hardware context bank number, used for calculating register
 *		offsets.
 * @kgsldev: The kgsl device that uses this context.
 * @stalled_on_fault: Flag when set indicates that this iommu device is stalled
 * on a page fault
 */
struct kgsl_iommu_context {
	struct platform_device *pdev;
	const char *name;
	int cb_num;
	struct kgsl_device *kgsldev;
	bool stalled_on_fault;
	/** ratelimit: Ratelimit state for the context */
	struct ratelimit_state ratelimit;
	struct iommu_domain *domain;
	struct adreno_smmu_priv adreno_smmu;
};

/*
 * struct kgsl_iommu - Structure holding iommu data for kgsl driver
 * @regbase: Virtual address of the IOMMU register base
 * @regstart: Physical address of the iommu registers
 * @regsize: Length of the iommu register region.
 * @setstate: Scratch GPU memory for IOMMU operations
 * @clk_enable_count: The ref count of clock enable calls
 * @clks: Array of pointers to IOMMU clocks
 * @smmu_info: smmu info used in a5xx preemption
 */
struct kgsl_iommu {
	/** @user_context: Container for the user iommu context */
	struct kgsl_iommu_context user_context;
	/** @secure_context: Container for the secure iommu context */
	struct kgsl_iommu_context secure_context;
	/** @lpac_context: Container for the LPAC iommu context */
	struct kgsl_iommu_context lpac_context;
	void __iomem *regbase;
	struct kgsl_memdesc *setstate;
	atomic_t clk_enable_count;
	struct clk_bulk_data *clks;
	int num_clks;
	struct kgsl_memdesc *smmu_info;
	/** @pdev: Pointer to the platform device for the IOMMU device */
	struct platform_device *pdev;
	/**
	 * @ppt_active: Set when the first per process pagetable is created.
	 * This is used to warn when global buffers are created that might not
	 * be mapped in all contexts
	 */
	bool ppt_active;
	/** @cb0_offset: Offset of context bank 0 from iommu register base */
	u32 cb0_offset;
	/** @pagesize: Size of each context bank register space */
	u32 pagesize;
	/** @cx_gdsc: CX GDSC handle in case the IOMMU needs it */
	struct regulator *cx_gdsc;
};

/*
 * struct kgsl_iommu_pt - Iommu pagetable structure private to kgsl driver
 * @base: Container of the base kgsl pagetable
 * @ttbr0: register value to set when using this pagetable
 * @ pgtbl_ops: Pagetable operations for mapping/unmapping buffers
 * @info: Pagetable info used to allocate pagetable operations
 */
struct kgsl_iommu_pt {
	struct kgsl_pagetable base;
	u64 ttbr0;
	struct io_pgtable_ops *pgtbl_ops;
	struct qcom_io_pgtable_info info;
};

#endif
