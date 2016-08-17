/*
 * board-common.h: Common function API declaration for all board files.
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __MACH_TEGRA_BOARD_COMMON_H
#define __MACH_TEGRA_BOARD_COMMON_H

#include <linux/thermal.h>
#include <linux/platform_data/thermal_sensors.h>

extern struct platform_device *uart_console_debug_device;
int  uart_console_debug_init(int defaul_debug_port);
int tegra_vibrator_init(void);
void tegra_add_cdev_trips(struct thermal_trip_info *trips, int *num_trips);
void tegra_add_tj_trips(struct thermal_trip_info *trips, int *num_trips);
struct tegra_cooling_device *tegra_vc_get_cdev(void);
void tegra_add_vc_trips(struct thermal_trip_info *trips, int *num_trips);
#endif
