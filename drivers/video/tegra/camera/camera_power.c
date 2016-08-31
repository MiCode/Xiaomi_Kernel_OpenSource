/*
 * drivers/video/tegra/camera/camera_power.c
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

#include "camera_power.h"

int tegra_camera_power_on(struct tegra_camera *camera)
{
	int ret = 0;
	dev_dbg(camera->dev, "%s++\n", __func__);

	/* Enable external power */
	if (camera->reg) {
		ret = regulator_enable(camera->reg);
		if (ret) {
			dev_err(camera->dev,
				"%s: enable csi regulator failed.\n",
				__func__);
			return ret;
		}
	}
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	/* Powergating DIS must powergate VE partition. Camera
	 * module needs to increase the ref-count of disa to
	 * avoid itself powergated by DIS inadvertently. */
#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC)
	ret = tegra_unpowergate_partition(TEGRA_POWERGATE_DISA);
	if (ret)
		dev_err(camera->dev,
			"%s: DIS unpowergate failed.\n",
			__func__);
#endif
	/* Unpowergate VE */
	ret = tegra_unpowergate_partition(TEGRA_POWERGATE_VENC);
	if (ret)
		dev_err(camera->dev,
			"%s: VENC unpowergate failed.\n",
			__func__);
#endif
	camera->power_on = 1;
	return ret;
}

int tegra_camera_power_off(struct tegra_camera *camera)
{
	int ret = 0;

	dev_dbg(camera->dev, "%s++\n", __func__);

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	/* Powergate VE */
	ret = tegra_powergate_partition(TEGRA_POWERGATE_VENC);
	if (ret)
		dev_err(camera->dev,
			"%s: VENC powergate failed.\n",
			__func__);

#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC)
	ret = tegra_powergate_partition(TEGRA_POWERGATE_DISA);
	if (ret)
		dev_err(camera->dev,
			"%s: DIS powergate failed.\n",
			__func__);
#endif
#endif
	/* Disable external power */
	if (camera->reg) {
		ret = regulator_disable(camera->reg);
		if (ret) {
			dev_err(camera->dev,
				"%s: disable csi regulator failed.\n",
				__func__);
			return ret;
		}
	}
	camera->power_on = 0;
	return ret;
}

int tegra_camera_powergate_init(struct tegra_camera *camera)
{
	int ret = 0;

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	ret = tegra_powergate_partition(TEGRA_POWERGATE_VENC);
	if (ret)
		dev_err(camera->dev, "%s: powergate failed.\n", __func__);
#endif
	return ret;
}
