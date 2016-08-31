/*
 *
 * Copyright (c) 2013 NVIDIA CORPORATION. All rights reserved.
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

#ifndef __LINUX_TEGRA_DFLL_BYPASS_H
#define __LINUX_TEGRA_DFLL_BYPASS_H

struct tegra_dfll_bypass_platform_data {
	struct regulator_init_data *reg_init_data;
	unsigned int uV_step;
	unsigned int linear_min_sel;
	unsigned int n_voltages;
	int voltage_time_sel;
	int (*set_bypass_sel)(void *dfll_data, unsigned int sel);
	int (*get_bypass_sel)(void *dfll_data);
	void *dfll_data;
	int msel_gpio;
};

#endif /* __LINUX_TEGRA_DFLL_BYPASS_H */
