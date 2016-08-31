/*
 * drivers/video/tegra/camera/camera_priv.h
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

#ifndef __DRIVERS_VIDEO_TEGRA_CAMERA_CAMERA_PRIV_H
#define __DRIVERS_VIDEO_TEGRA_CAMERA_CAMERA_PRIV_H

#include <linux/nvhost.h>
#include <linux/io.h>
#include "camera_priv_defs.h"

static inline unsigned long tegra_camera_readl(struct tegra_camera *camera,
						unsigned long reg)
{
	unsigned long ret;
	struct platform_device *ndev = to_platform_device(camera->dev);
	struct nvhost_device_data *pdata = platform_get_drvdata(ndev);

	WARN(!tegra_is_clk_enabled(camera->clock[CAMERA_VI_CLK].clk),
		"VI is clock-gated");

	ret = readl(pdata->aperture[0] + reg);
	return ret;
}

static inline void tegra_camera_writel(struct tegra_camera *camera,
					unsigned long val,
					unsigned long reg)
{
	struct platform_device *ndev = to_platform_device(camera->dev);
	struct nvhost_device_data *pdata = platform_get_drvdata(ndev);

	WARN(!tegra_is_clk_enabled(camera->clock[CAMERA_VI_CLK].clk),
		"VI is clock-gated");

	writel(val, pdata->aperture[0] + reg);
}


#endif
