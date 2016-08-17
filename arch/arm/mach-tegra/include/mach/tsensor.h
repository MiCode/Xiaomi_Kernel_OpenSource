/*
 * arch/arm/mach-tegra/include/mach/tsensor.h
 *
 * Tegra tsensor header file
 *
 * Copyright (c) 2011-2012, NVIDIA Corporation.
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

#ifndef __MACH_TEGRA_TSENSOR_H
#define __MACH_TEGRA_TSENSOR_H

#include <linux/types.h>
#include <linux/thermal.h>

struct tegra_tsensor_cdev {
	struct thermal_cooling_device *cdev;
	long trip_temp;
	enum thermal_trip_type type;
	int tc1;
	int tc2;
	int passive_delay;
};

#define TSENSOR_MAX_ACTIVE (16)

struct tegra_tsensor_platform_data {
	long shutdown_temp;
	struct tegra_tsensor_cdev passive;
	struct tegra_tsensor_cdev active[TSENSOR_MAX_ACTIVE + 1];
};

#endif /* __MACH_TEGRA_TSENSOR_H */

