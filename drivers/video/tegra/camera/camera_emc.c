/*
 * drivers/video/tegra/camera/camera_emc.c
 *
 * Copyright (C) 2013 Nvidia Corp
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

#include "camera_emc.h"

int tegra_camera_enable_emc(struct tegra_camera *camera)
{
	int ret = tegra_emc_disable_eack();

	dev_dbg(camera->dev, "%s++\n", __func__);
	clk_prepare_enable(camera->clock[CAMERA_EMC_CLK].clk);
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	clk_set_rate(camera->clock[TEGRA_CAMERA_EMC_CLK].clk, 300000000);
#endif
	return ret;
}

int tegra_camera_disable_emc(struct tegra_camera *camera)
{
	dev_dbg(camera->dev, "%s++\n", __func__);
	clk_disable_unprepare(camera->clock[CAMERA_EMC_CLK].clk);
	return tegra_emc_enable_eack();
}
