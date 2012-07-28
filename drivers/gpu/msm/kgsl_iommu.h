/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

/* IOMMU registers and masks */
#define KGSL_IOMMU_TTBR0			0x10
#define KGSL_IOMMU_TTBR1			0x14
#define KGSL_IOMMU_FSR				0x20

#define KGSL_IOMMU_TTBR0_PA_MASK		0x0003FFFF
#define KGSL_IOMMU_TTBR0_PA_SHIFT		14
#define KGSL_IOMMU_CTX_TLBIALL			0x800
#define KGSL_IOMMU_CTX_SHIFT			12

/*
 * Max number of iommu units that the gpu core can have
 * On APQ8064, KGSL can control a maximum of 2 IOMMU units.
 */
#define KGSL_IOMMU_MAX_UNITS 2

/* Max number of iommu contexts per IOMMU unit */
#define KGSL_IOMMU_MAX_DEVS_PER_UNIT 2

/* Macros to read/write IOMMU registers */
#define KGSL_IOMMU_SET_IOMMU_REG(base_addr, ctx, REG, val)		\
		writel_relaxed(val, base_addr +				\
				(ctx << KGSL_IOMMU_CTX_SHIFT) +		\
				KGSL_IOMMU_##REG)

#define KGSL_IOMMU_GET_IOMMU_REG(base_addr, ctx, REG)			\
		readl_relaxed(base_addr +				\
			(ctx << KGSL_IOMMU_CTX_SHIFT) +			\
			KGSL_IOMMU_##REG)

/* Gets the lsb value of pagetable */
#define KGSL_IOMMMU_PT_LSB(pt_val)					\
		(pt_val & ~(KGSL_IOMMU_TTBR0_PA_MASK <<			\
				KGSL_IOMMU_TTBR0_PA_SHIFT))

/* offset at which a nop command is placed in setstate_memory */
#define KGSL_IOMMU_SETSTATE_NOP_OFFSET	1024

/*
 * struct kgsl_iommu_device - Structure holding data about iommu contexts
 * @dev: Device pointer to iommu context
 * @attached: Indicates whether this iommu context is presently attached to
 * a pagetable/domain or not
 * @pt_lsb: The LSB of IOMMU_TTBR0 register which is the pagetable
 * register
 * @ctx_id: This iommu units context id. It can be either 0 or 1
 * @clk_enabled: If set indicates that iommu clocks of this iommu context
 * are on, else the clocks are off
 */
struct kgsl_iommu_device {
	struct device *dev;
	bool attached;
	unsigned int pt_lsb;
	enum kgsl_iommu_context_id ctx_id;
	bool clk_enabled;
	struct kgsl_device *kgsldev;
};

/*
 * struct kgsl_iommu_unit - Structure holding data about iommu units. An IOMMU
 * units is basically a separte IOMMU h/w block with it's own IOMMU contexts
 * @dev: Pointer to array of struct kgsl_iommu_device which has information
 * about the IOMMU contexts under this IOMMU unit
 * @dev_count: Number of IOMMU contexts that are valid in the previous feild
 * @reg_map: Memory descriptor which holds the mapped address of this IOMMU
 * units register range
 */
struct kgsl_iommu_unit {
	struct kgsl_iommu_device dev[KGSL_IOMMU_MAX_DEVS_PER_UNIT];
	unsigned int dev_count;
	struct kgsl_memdesc reg_map;
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
 */
struct kgsl_iommu {
	struct kgsl_iommu_unit iommu_units[KGSL_IOMMU_MAX_UNITS];
	unsigned int unit_count;
	unsigned int iommu_last_cmd_ts;
	bool clk_event_queued;
	struct kgsl_device *device;
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
