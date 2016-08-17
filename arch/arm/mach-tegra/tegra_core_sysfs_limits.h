/*
 * arch/arm/mach-tegra/tegra_core_sysfs_limits.h
 *
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
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

#ifndef _MACH_TEGRA_CORE_SYSFS_LIMITS_H_
#define _MACH_TEGRA_CORE_SYSFS_LIMITS_H_

struct core_dvfs_cap_table {
	const char *cap_name;
	struct clk *cap_clk;
	unsigned long freqs[MAX_DVFS_FREQS];
};

struct core_bus_limit_table {
	const char *limit_clk_name;
	struct clk *limit_clk;
	struct kobj_attribute refcnt_attr;
	struct kobj_attribute level_attr;
	int refcnt;
	int level;
};

int tegra_init_core_cap(struct core_dvfs_cap_table *table, int table_size,
	const int *millivolts, int millivolts_num, struct kobject *cap_kobj);
int tegra_init_shared_bus_cap(struct core_bus_limit_table *table,
	int table_size, struct kobject *cap_kobj);
int tegra_init_shared_bus_floor(struct core_bus_limit_table *table,
	int table_size, struct kobject *floor_kobj);

#endif /* _MACH_TEGRA_CORE_SYSFS_LIMITS_H_ */
