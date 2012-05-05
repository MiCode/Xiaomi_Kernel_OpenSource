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

/*
 * Max number of iommu units that the gpu core can have
 * On APQ8064, KGSL can control a maximum of 2 IOMMU units.
 */
#define KGSL_IOMMU_MAX_UNITS 2

/* Max number of iommu contexts per IOMMU unit */
#define KGSL_IOMMU_MAX_DEVS_PER_UNIT 2

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
 * @device: Pointer to kgsl device
 * @asids: A bit structure indicating which id's are presently used
 * @asid: Contains the initial value of IOMMU_CONTEXTIDR when a domain
 * is first attached
 */
struct kgsl_iommu {
	struct kgsl_iommu_unit iommu_units[KGSL_IOMMU_MAX_UNITS];
	unsigned int unit_count;
	unsigned int iommu_last_cmd_ts;
	struct kgsl_device *device;
	unsigned long *asids;
	unsigned int asid;
	unsigned int active_ctx;
};

#endif
