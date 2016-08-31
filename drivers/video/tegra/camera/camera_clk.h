/*
 * drivers/video/tegra/camera/camera_clk.h
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __DRIVERS_VIDEO_TEGRA_CAMERA_CAMERA_CLK_H
#define __DRIVERS_VIDEO_TEGRA_CAMERA_CAMERA_CLK_H
#include "camera_priv_defs.h"

int tegra_camera_enable_clk(struct tegra_camera *camera);
int tegra_camera_disable_clk(struct tegra_camera *camera);
int tegra_camera_clk_set_rate(struct tegra_camera *camera);
int tegra_camera_init_clk(struct tegra_camera *camera,
	struct clock_data *clock_init);
unsigned int tegra_camera_get_max_bw(struct tegra_camera *camera);
int tegra_camera_set_latency_allowance(struct tegra_camera *camera,
	unsigned long vi_freq);
#endif
