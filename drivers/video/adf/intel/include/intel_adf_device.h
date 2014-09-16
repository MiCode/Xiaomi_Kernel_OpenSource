/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef INTEL_ADF_DEVICE_H_
#define INTEL_ADF_DEVICE_H_

#include <asm/intel-mid.h>
#include <video/adf.h>
#include <linux/pci.h>

#include "intel_adf_mm.h"

struct intel_adf_context;

struct intel_adf_device {
	struct adf_device base;
	struct intel_adf_mm mm;
	/*pci device*/
	struct pci_dev *pdev;
	/*display mmio virtual address*/
	u8 *mmio;
	/*timeline for sync up post operations*/
	struct intel_adf_sync_timeline *post_timeline;

	struct list_head active_intfs;
	struct list_head active_engs;
};

static inline struct intel_adf_device *to_intel_dev(struct adf_device *dev)
{
	return container_of(dev, struct intel_adf_device, base);
}

static inline struct intel_adf_device *intf_to_dev(struct adf_interface *intf)
{
	return container_of(adf_interface_parent(intf),
			struct intel_adf_device, base);
}

static inline struct intel_adf_device *eng_to_dev(
		struct adf_overlay_engine *eng)
{
	return container_of(adf_overlay_engine_parent(eng),
			struct intel_adf_device, base);
}

/*
 * FIXME: remove these later
 */
#define IS_ANN() (intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_ANNIEDALE)

extern struct intel_adf_device *intel_adf_device_create(struct pci_dev *pdev,
	struct intel_dc_memory *mem);
extern void intel_adf_device_destroy(struct intel_adf_device *dev);

extern u32 REG_READ(u32 reg);
extern u32 REG_POSTING_READ(u32 reg);
extern void REG_WRITE(u32 reg, u32 val);

#endif /* INTEL_ADF_DEVICE_H_ */
