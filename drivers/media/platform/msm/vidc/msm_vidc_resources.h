/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_VIDC_RESOURCES_H__
#define __MSM_VIDC_RESOURCES_H__

#include <linux/platform_device.h>
#include <media/msm_vidc.h>

struct load_freq_table {
	u32 load;
	u32 freq;
};

struct reg_value_pair {
	u32 reg;
	u32 value;
};

struct reg_set {
	struct reg_value_pair *reg_tbl;
	int count;
};

struct msm_vidc_platform_resources {
	uint32_t fw_base_addr;
	uint32_t register_base;
	uint32_t register_size;
	uint32_t irq;
	struct load_freq_table *load_freq_tbl;
	uint32_t load_freq_tbl_size;
	struct msm_vidc_iommu_info *iommu_maps;
	uint32_t iommu_maps_size;
	struct reg_set reg_set;
	struct platform_device *pdev;
};

#endif

