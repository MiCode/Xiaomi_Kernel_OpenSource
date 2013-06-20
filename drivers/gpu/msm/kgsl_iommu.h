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
 *
 */
#ifndef __KGSL_IOMMU_H
#define __KGSL_IOMMU_H

#include <mach/iommu.h>

#define KGSL_IOMMU_CTX_OFFSET_V0	0
#define KGSL_IOMMU_CTX_OFFSET_V1	0x8000
#define KGSL_IOMMU_CTX_SHIFT		12

/* TLBLKCR fields */
#define KGSL_IOMMU_TLBLKCR_LKE_MASK		0x00000001
#define KGSL_IOMMU_TLBLKCR_LKE_SHIFT		0
#define KGSL_IOMMU_TLBLKCR_TLBIALLCFG_MASK	0x00000001
#define KGSL_IOMMU_TLBLKCR_TLBIALLCFG_SHIFT	1
#define KGSL_IOMMU_TLBLKCR_TLBIASIDCFG_MASK	0x00000001
#define KGSL_IOMMU_TLBLKCR_TLBIASIDCFG_SHIFT	2
#define KGSL_IOMMU_TLBLKCR_TLBIVAACFG_MASK	0x00000001
#define KGSL_IOMMU_TLBLKCR_TLBIVAACFG_SHIFT	3
#define KGSL_IOMMU_TLBLKCR_FLOOR_MASK		0x000000FF
#define KGSL_IOMMU_TLBLKCR_FLOOR_SHIFT		8
#define KGSL_IOMMU_TLBLKCR_VICTIM_MASK		0x000000FF
#define KGSL_IOMMU_TLBLKCR_VICTIM_SHIFT		16

/* V2PXX fields */
#define KGSL_IOMMU_V2PXX_INDEX_MASK		0x000000FF
#define KGSL_IOMMU_V2PXX_INDEX_SHIFT		0
#define KGSL_IOMMU_V2PXX_VA_MASK		0x000FFFFF
#define KGSL_IOMMU_V2PXX_VA_SHIFT		12

/* FSYNR1 V0 fields */
#define KGSL_IOMMU_FSYNR1_AWRITE_MASK		0x00000001
#define KGSL_IOMMU_FSYNR1_AWRITE_SHIFT		8
/* FSYNR0 V1 fields */
#define KGSL_IOMMU_V1_FSYNR0_WNR_MASK		0x00000001
#define KGSL_IOMMU_V1_FSYNR0_WNR_SHIFT		4

/* TTBR0 register fields */
#ifdef CONFIG_ARM_LPAE
#define KGSL_IOMMU_CTX_TTBR0_ADDR_MASK_LPAE	0x000000FFFFFFFFE0ULL
#define KGSL_IOMMU_CTX_TTBR0_ADDR_MASK KGSL_IOMMU_CTX_TTBR0_ADDR_MASK_LPAE
#else
#define KGSL_IOMMU_CTX_TTBR0_ADDR_MASK		0xFFFFC000
#endif

/* TLBSTATUS register fields */
#define KGSL_IOMMU_CTX_TLBSTATUS_SACTIVE BIT(0)

/* IMPLDEF_MICRO_MMU_CTRL register fields */
#define KGSL_IOMMU_IMPLDEF_MICRO_MMU_CTRL_HALT BIT(2)
#define KGSL_IOMMU_IMPLDEF_MICRO_MMU_CTRL_IDLE BIT(3)

/* SCTLR fields */
#define KGSL_IOMMU_SCTLR_HUPCF_SHIFT		8

enum kgsl_iommu_reg_map {
	KGSL_IOMMU_GLOBAL_BASE = 0,
	KGSL_IOMMU_CTX_SCTLR,
	KGSL_IOMMU_CTX_TTBR0,
	KGSL_IOMMU_CTX_TTBR1,
	KGSL_IOMMU_CTX_FSR,
	KGSL_IOMMU_CTX_TLBIALL,
	KGSL_IOMMU_CTX_RESUME,
	KGSL_IOMMU_CTX_TLBLKCR,
	KGSL_IOMMU_CTX_V2PUR,
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

/*
 * Max number of iommu units that the gpu core can have
 * On APQ8064, KGSL can control a maximum of 2 IOMMU units.
 */
#define KGSL_IOMMU_MAX_UNITS 2

/* Max number of iommu contexts per IOMMU unit */
#define KGSL_IOMMU_MAX_DEVS_PER_UNIT 2

/* Macros to read/write IOMMU registers */
#define KGSL_IOMMU_SET_CTX_REG_LL(iommu, iommu_unit, ctx, REG, val)	\
		writell_relaxed(val,					\
		iommu_unit->reg_map.hostptr +				\
		iommu->iommu_reg_list[KGSL_IOMMU_CTX_##REG].reg_offset +\
		(ctx << KGSL_IOMMU_CTX_SHIFT) +				\
		iommu->ctx_offset)

#define KGSL_IOMMU_GET_CTX_REG_LL(iommu, iommu_unit, ctx, REG)		\
		readl_relaxed(						\
		iommu_unit->reg_map.hostptr +				\
		iommu->iommu_reg_list[KGSL_IOMMU_CTX_##REG].reg_offset +\
		(ctx << KGSL_IOMMU_CTX_SHIFT) +				\
		iommu->ctx_offset)

#define KGSL_IOMMU_SET_CTX_REG(iommu, iommu_unit, ctx, REG, val)	\
		writel_relaxed(val,					\
		iommu_unit->reg_map.hostptr +				\
		iommu->iommu_reg_list[KGSL_IOMMU_CTX_##REG].reg_offset +\
		(ctx << KGSL_IOMMU_CTX_SHIFT) +				\
		iommu->ctx_offset)

#define KGSL_IOMMU_GET_CTX_REG(iommu, iommu_unit, ctx, REG)		\
		readl_relaxed(						\
		iommu_unit->reg_map.hostptr +				\
		iommu->iommu_reg_list[KGSL_IOMMU_CTX_##REG].reg_offset +\
		(ctx << KGSL_IOMMU_CTX_SHIFT) +				\
		iommu->ctx_offset)

/* Gets the lsb value of pagetable */
#define KGSL_IOMMMU_PT_LSB(iommu, pt_val)				\
	(pt_val & ~(KGSL_IOMMU_CTX_TTBR0_ADDR_MASK))

/* offset at which a nop command is placed in setstate_memory */
#define KGSL_IOMMU_SETSTATE_NOP_OFFSET	1024

/*
 * struct kgsl_iommu_device - Structure holding data about iommu contexts
 * @dev: Device pointer to iommu context
 * @attached: Indicates whether this iommu context is presently attached to
 * a pagetable/domain or not
 * @default_ttbr0: The TTBR0 value set by iommu driver on start up
 * @ctx_id: This iommu units context id. It can be either 0 or 1
 * @clk_enabled: If set indicates that iommu clocks of this iommu context
 * are on, else the clocks are off
 * fault: Flag when set indicates that this iommu device has caused a page
 * fault
 */
struct kgsl_iommu_device {
	struct device *dev;
	bool attached;
	phys_addr_t default_ttbr0;
	enum kgsl_iommu_context_id ctx_id;
	bool clk_enabled;
	struct kgsl_device *kgsldev;
	int fault;
};

/*
 * struct kgsl_iommu_unit - Structure holding data about iommu units. An IOMMU
 * units is basically a separte IOMMU h/w block with it's own IOMMU contexts
 * @dev: Pointer to array of struct kgsl_iommu_device which has information
 * about the IOMMU contexts under this IOMMU unit
 * @dev_count: Number of IOMMU contexts that are valid in the previous feild
 * @reg_map: Memory descriptor which holds the mapped address of this IOMMU
 * units register range
 * @ahb_base - The base address from where IOMMU registers can be accesed from
 * ahb bus
 * @iommu_halt_enable: Valid only on IOMMU-v1, when set indicates that the iommu
 * unit supports halting of the IOMMU, which can be enabled while programming
 * the IOMMU registers for synchronization
 */
struct kgsl_iommu_unit {
	struct kgsl_iommu_device dev[KGSL_IOMMU_MAX_DEVS_PER_UNIT];
	unsigned int dev_count;
	struct kgsl_memdesc reg_map;
	unsigned int ahb_base;
	int iommu_halt_enable;
};

/*
 * struct kgsl_iommu - Structure holding iommu data for kgsl driver
 * @dev: Array of kgsl_iommu_device which contain information about
 * iommu contexts owned by graphics cores
 * @unit_count: Number of IOMMU units that are available for this
 * instance of the IOMMU driver
 * @iommu_last_cmd_ts: The timestamp of last command submitted that
 * aceeses iommu registers
 * @clk_event_queued: Indicates whether an event to disable clocks
 * is already queued or not
 * @device: Pointer to kgsl device
 * @ctx_offset: The context offset to be added to base address when
 * accessing IOMMU registers
 * @iommu_reg_list: List of IOMMU registers { offset, map, shift } array
 * @sync_lock_vars: Pointer to the IOMMU spinlock for serializing access to the
 * IOMMU registers
 * @sync_lock_desc: GPU Memory descriptor for the memory containing the
 * spinlocks
 * @sync_lock_offset - The page offset within a page at which the sync
 * variables are located
 * @sync_lock_initialized: True if the sync_lock feature is enabled
 */
struct kgsl_iommu {
	struct kgsl_iommu_unit iommu_units[KGSL_IOMMU_MAX_UNITS];
	unsigned int unit_count;
	unsigned int iommu_last_cmd_ts;
	bool clk_event_queued;
	struct kgsl_device *device;
	unsigned int ctx_offset;
	struct kgsl_iommu_register_list *iommu_reg_list;
	struct remote_iommu_petersons_spinlock *sync_lock_vars;
	struct kgsl_memdesc sync_lock_desc;
	unsigned int sync_lock_offset;
	bool sync_lock_initialized;
};

/*
 * struct kgsl_iommu_pt - Iommu pagetable structure private to kgsl driver
 * @domain: Pointer to the iommu domain that contains the iommu pagetable
 * @iommu: Pointer to iommu structure
 */
struct kgsl_iommu_pt {
	struct iommu_domain *domain;
	struct kgsl_iommu *iommu;
};

#endif
