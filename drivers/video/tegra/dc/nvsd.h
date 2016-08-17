/*
 * drivers/video/tegra/dc/nvsd.h
 *
 * Copyright (c) 2010-2012, NVIDIA CORPORATION, All rights reserved.
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

#ifndef __DRIVERS_VIDEO_TEGRA_DC_NVSD_H
#define __DRIVERS_VIDEO_TEGRA_DC_NVSD_H

void nvsd_init(struct tegra_dc *dc, struct tegra_dc_sd_settings *settings);
bool nvsd_update_brightness(struct tegra_dc *dc);
int nvsd_create_sysfs(struct device *dev);
void __devexit nvsd_remove_sysfs(struct device *dev);

#endif
