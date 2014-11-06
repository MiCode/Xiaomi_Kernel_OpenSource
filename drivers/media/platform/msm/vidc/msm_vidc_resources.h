/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#define MAX_BUFFER_TYPES 32
#define IDLE_TIME_WINDOW_SIZE 30


struct load_freq_table {
	u32 load;
	u32 freq;
	u32 supported_codecs;
};

struct reg_value_pair {
	u32 reg;
	u32 value;
};

struct reg_set {
	struct reg_value_pair *reg_tbl;
	int count;
};

struct addr_range {
	u32 start;
	u32 size;
};

struct context_bank_info {
	struct list_head list;
	const char *name;
	u32 buffer_type;
	bool is_secure;
	struct addr_range addr_range;
	struct device *dev;
	struct dma_iommu_mapping *mapping;
};

struct buffer_usage_table {
	u32 buffer_type;
	u32 tz_usage;
};

struct buffer_usage_set {
	struct buffer_usage_table *buffer_usage_tbl;
	u32 count;
};

struct regulator_info {
	struct regulator *regulator;
	bool has_hw_power_collapse;
	char *name;
};

struct regulator_set {
	struct regulator_info *regulator_tbl;
	u32 count;
};

struct clock_info {
	const char *name;
	struct clk *clk;
	struct load_freq_table *load_freq_tbl;
	u32 count; /* == has_scaling iff count != 0 */
	bool has_gating;
};

struct clock_set {
	struct clock_info *clock_tbl;
	u32 count;
};

struct bus_info {
	struct msm_bus_scale_pdata *pdata;
	u32 priv;
	u32 sessions_supported; /* bitmask */
};

struct bus_set {
	struct bus_info *bus_tbl;
	u32 count;
};

enum imem_type {
	IMEM_NONE,
	IMEM_OCMEM,
	IMEM_VMEM,
	IMEM_MAX,
};

struct msm_vidc_platform_resources {
	phys_addr_t firmware_base;
	phys_addr_t register_base;
	uint32_t register_size;
	uint32_t irq;
	struct load_freq_table *load_freq_tbl;
	uint32_t load_freq_tbl_size;
	struct reg_set reg_set;
	struct buffer_usage_set buffer_usage_set;
	uint32_t imem_size;
	enum imem_type imem_type;
	uint32_t max_load;
	struct platform_device *pdev;
	struct regulator_set regulator_set;
	struct clock_set clock_set;
	struct bus_set bus_set;
	bool dynamic_bw_update;
	bool use_non_secure_pil;
	bool sw_power_collapsible;
	bool sys_idle_indicator;
	struct list_head context_banks;
	bool early_fw_load;
	bool thermal_mitigable;
};

static inline bool is_iommu_present(struct msm_vidc_platform_resources *res)
{
	return !list_empty(&res->context_banks);
}

extern uint32_t msm_vidc_pwr_collapse_delay;

#endif

