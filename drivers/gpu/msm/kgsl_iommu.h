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
 *
 */
#ifndef __KGSL_IOMMU_H
#define __KGSL_IOMMU_H

#include <linux/qcom_iommu.h>
#include "kgsl.h"

/* Pagetable virtual base */
#define KGSL_IOMMU_CTX_OFFSET_V1	0x8000
#define KGSL_IOMMU_CTX_OFFSET_V2	0x9000
#define KGSL_IOMMU_CTX_OFFSET_V2_A5XX	0x8000
#define KGSL_IOMMU_CTX_OFFSET_A405V2	0x8000
#define KGSL_IOMMU_CTX_SHIFT		12

/* IOMMU V2 AHB base is fixed */
#define KGSL_IOMMU_V2_AHB_BASE_OFFSET		0xA000
#define KGSL_IOMMU_V2_AHB_BASE_OFFSET_A405  0x48000
#define KGSL_IOMMU_V2_AHB_BASE_OFFSET_A5XX  0x40000
/* IOMMU_V2 AHB base points to ContextBank1 */
#define KGSL_IOMMU_CTX_AHB_OFFSET_V2   0

/* FSYNR1 V0 fields */
#define KGSL_IOMMU_FSYNR1_AWRITE_MASK		0x00000001
#define KGSL_IOMMU_FSYNR1_AWRITE_SHIFT		8
/* FSYNR0 V1 fields */
#define KGSL_IOMMU_V1_FSYNR0_WNR_MASK		0x00000001
#define KGSL_IOMMU_V1_FSYNR0_WNR_SHIFT		4

/*
 * TTBR0 register fields
 * On arm64 bit mask is not required
 */
#ifdef CONFIG_ARM64
	#define KGSL_IOMMU_CTX_TTBR0_ADDR_MASK	0x0000FFFFFFFFFFFFULL
#else
	#ifdef CONFIG_IOMMU_LPAE
		#define KGSL_IOMMU_CTX_TTBR0_ADDR_MASK_LPAE \
					0x000000FFFFFFFFE0ULL
		#define KGSL_IOMMU_CTX_TTBR0_ADDR_MASK \
					KGSL_IOMMU_CTX_TTBR0_ADDR_MASK_LPAE
	#else
		#define KGSL_IOMMU_CTX_TTBR0_ADDR_MASK	0xFFFFC000
	#endif
#endif

/* TLBSTATUS register fields */
#define KGSL_IOMMU_CTX_TLBSTATUS_SACTIVE BIT(0)

/* IMPLDEF_MICRO_MMU_CTRL register fields */
#define KGSL_IOMMU_IMPLDEF_MICRO_MMU_CTRL_HALT  0x00000004
#define KGSL_IOMMU_IMPLDEF_MICRO_MMU_CTRL_IDLE  0x00000008

/* SCTLR fields */
#define KGSL_IOMMU_SCTLR_HUPCF_SHIFT		8

enum kgsl_iommu_reg_map {
	KGSL_IOMMU_GLOBAL_BASE = 0,
	KGSL_IOMMU_CTX_SCTLR,
	KGSL_IOMMU_CTX_TTBR0,
	KGSL_IOMMU_CTX_TTBR1,
	KGSL_IOMMU_CTX_FSR,
	KGSL_IOMMU_CTX_FAR,
	KGSL_IOMMU_CTX_TLBIALL,
	KGSL_IOMMU_CTX_RESUME,
	KGSL_IOMMU_CTX_FSYNR0,
	KGSL_IOMMU_CTX_FSYNR1,
	KGSL_IOMMU_CTX_TLBSYNC,
	KGSL_IOMMU_CTX_TLBSTATUS,
	KGSL_IOMMU_IMPLDEF_MICRO_MMU_CTRL,
	KGSL_IOMMU_REG_MAX
};

struct kgsl_iommu_register_list {
	unsigned int reg_offset;
	int ctx_reg;
};

/* Max number of iommu clks per IOMMU unit */
#define KGSL_IOMMU_MAX_CLKS 6

enum kgsl_iommu_context_id {
	KGSL_IOMMU_CONTEXT_USER = 0,
	KGSL_IOMMU_CONTEXT_SECURE = 1,
	KGSL_IOMMU_CONTEXT_MAX = 2,
};

/**
 * struct kgsl_iommu_ctx - Struct holding context name and id
 * @dev:                Device pointer
 * @iommu_ctx_name:     Context name
 * @ctx_id:             Iommu context ID
 */
struct kgsl_iommu_ctx {
	struct device *dev;
	const char *iommu_ctx_name;
	enum kgsl_iommu_context_id ctx_id;
};

/**
 * struct kgsl_device_iommu_data - Struct holding iommu context data obtained
 * from dtsi file
 * @iommu_ctxs:         Pointer to array of struct holding context name and id
 * @iommu_ctx_count:    Number of contexts defined in the dtsi file
 * @regstart:           Start of iommu registers physical address
 * @regsize:            Size of registers physical address block
 * @clks                Iommu clocks
 * @features            Iommu features, ex RETENTION, DMA API
 */
struct kgsl_device_iommu_data {
	struct kgsl_iommu_ctx *iommu_ctxs;
	int iommu_ctx_count;
	unsigned int regstart;
	unsigned int regsize;
	struct clk *clks[KGSL_IOMMU_MAX_CLKS];
	unsigned int features;
};


#define KGSL_IOMMU_REG(iommu, ctx, REG) \
	((iommu)->regbase + \
	 (iommu)->iommu_reg_list[KGSL_IOMMU_CTX_##REG].reg_offset + \
	 ((ctx) << KGSL_IOMMU_CTX_SHIFT) + (iommu)->ctx_offset)

/* Macros to read/write IOMMU registers */
#define KGSL_IOMMU_SET_CTX_REG_Q(iommu, ctx, REG, val)	\
		writeq_relaxed((val), KGSL_IOMMU_REG(iommu, ctx, REG))

#define KGSL_IOMMU_GET_CTX_REG_Q(iommu, ctx, REG)		\
		readq_relaxed(KGSL_IOMMU_REG(iommu, ctx, REG))

#define KGSL_IOMMU_SET_CTX_REG(iommu, ctx, REG, val)	\
		writel_relaxed((val), KGSL_IOMMU_REG(iommu, ctx, REG))

#define KGSL_IOMMU_GET_CTX_REG(iommu, ctx, REG)		\
		readl_relaxed(KGSL_IOMMU_REG(iommu, ctx, REG))

/* Gets the lsb value of pagetable */
#define KGSL_IOMMMU_PT_LSB(iommu, pt_val)				\
	(pt_val & ~(KGSL_IOMMU_CTX_TTBR0_ADDR_MASK))

/* offset at which a nop command is placed in setstate_memory */
#define KGSL_IOMMU_SETSTATE_NOP_OFFSET	1024

/*
 * struct kgsl_iommu_context - Structure holding data about iommu contexts
 * @dev: Device pointer to iommu context
 * @name: context name
 * @attached: Indicates whether this iommu context is presently attached to
 * a pagetable/domain or not
 * @default_ttbr0: The TTBR0 value set by iommu driver on start up
 * @ctx_id: The hardware context ID for the device
 * are on, else the clocks are off
 * fault: Flag when set indicates that this iommu device has caused a page
 * fault
 */
struct kgsl_iommu_context {
	struct device *dev;
	const char *name;
	bool attached;
	uint64_t default_ttbr0;
	enum kgsl_iommu_context_id ctx_id;
	struct kgsl_device *kgsldev;
	int fault;
};

/*
 * struct kgsl_iommu - Structure holding iommu data for kgsl driver
 * @device: Pointer to KGSL device struct
 * @ctx: Array of kgsl_iommu_context structs
 * @regbase: Virtual address of the IOMMU register base
 * @ahb_base_offset - The base address from where IOMMU registers can be
 * accesed from AHB bus
 * @clk_enable_count: The ref count of clock enable calls
 * @clks: Array of pointers to IOMMU clocks
 * @ctx_offset: The context offset to be added to base address when
 * accessing IOMMU registers from the CPU
 * @ctx_ahb_offset: The context offset to be added to base address when
 * accessing IOMMU registers from the GPU
 * @iommu_reg_list: List of IOMMU registers { offset, map, shift } array
 * @gtcu_iface_clk: The gTCU AHB Clock connected to SMMU
 * @smmu_info: smmu info used in a5xx preemption
 */
struct kgsl_iommu {
	struct kgsl_device *device;
	struct kgsl_iommu_context ctx[KGSL_IOMMU_CONTEXT_MAX];
	void __iomem *regbase;
	unsigned int ahb_base_offset;
	atomic_t clk_enable_count;
	struct clk *clks[KGSL_IOMMU_MAX_CLKS];
	unsigned int ctx_offset;
	unsigned int ctx_ahb_offset;
	struct kgsl_iommu_register_list *iommu_reg_list;
	struct clk *gtcu_iface_clk;
	struct clk *gtbu_clk;
	struct clk *gtbu1_clk;
	struct kgsl_memdesc smmu_info;
};

/*
 * struct kgsl_iommu_pt - Iommu pagetable structure private to kgsl driver
 * @domain: Pointer to the iommu domain that contains the iommu pagetable
 * @iommu: Pointer to iommu structure
 * @pt_base: physical base pointer of this pagetable.
 */
struct kgsl_iommu_pt {
	struct iommu_domain *domain;
	struct kgsl_iommu *iommu;
	phys_addr_t pt_base;
};

/*
 * kgsl_msm_supports_iommu_v2 - Checks whether IOMMU version is V2 or not
 *
 * Checks whether IOMMU version is V2 or not by parsing nodes.
 * Return: 1 if IOMMU v2 is found else 0
 */
#ifdef CONFIG_OF
static inline int _kgsl_msm_checks_iommu_v2(void)
{
	struct device_node *node;
	node = of_find_compatible_node(NULL, NULL, "qcom,msm-smmu-v2");
	if (!node)
		node = of_find_compatible_node(NULL, NULL, "qcom,smmu-v2");
	if (node) {
		of_node_put(node);
		return 1;
	}
	return 0;
}
#endif

#if !defined(CONFIG_MSM_IOMMU_V0) && defined(CONFIG_OF)
static int soc_supports_v2 = -1;
static inline int kgsl_msm_supports_iommu_v2(void)
{
	if (soc_supports_v2 != -1)
		return soc_supports_v2;

	soc_supports_v2 = _kgsl_msm_checks_iommu_v2();

	return soc_supports_v2;
}
#else
static inline int kgsl_msm_supports_iommu_v2(void)
{
	return 0;
}
#endif

#endif
