/*
 * drivers/video/tegra/camera/camera_emc.c
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

#include "camera_clk.h"
#include "camera_emc.h"

int tegra_camera_enable_emc(struct tegra_camera *camera)
{
	int ret = 0;

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

#if defined(CONFIG_TEGRA_ISOMGR)
	{
		int ret = 0;
		/* deallocate isomgr bw */
		ret = tegra_camera_isomgr_request(camera, 0, 0);
		if (ret) {
			dev_err(camera->dev,
			"%s: failed to deallocate memory in isomgr\n",
			__func__);
			return -ENOMEM;
		}
	}
#endif
	return 0;
}

#if defined(CONFIG_TEGRA_ISOMGR)
int tegra_camera_isomgr_register(struct tegra_camera *camera)
{
	dev_dbg(camera->dev, "%s++\n", __func__);

	/* Dedicated bw is what VI could ask for at most */
	camera->isomgr_handle = tegra_isomgr_register(TEGRA_ISO_CLIENT_VI_0,
					/* dedicated bw, KBps*/
					tegra_camera_get_max_bw(camera),
					NULL,	/* tegra_isomgr_renegotiate */
					NULL);	/* *priv */
	if (!camera->isomgr_handle) {
		dev_err(camera->dev, "%s: unable to register isomgr\n",
					__func__);
		return -ENOMEM;
	}

	return 0;
}

int tegra_camera_isomgr_unregister(struct tegra_camera *camera)
{
	tegra_isomgr_unregister(camera->isomgr_handle);
	camera->isomgr_handle = NULL;

	return 0;
}

/*
 * bw: memory BW in KBps
 * lt: latency in usec
 */
int tegra_camera_isomgr_request(struct tegra_camera *camera, unsigned long bw,
				unsigned long lt)
{
	int ret = 0;

	dev_dbg(camera->dev, "%s++ bw=%lu, lt=%lu\n", __func__, bw, lt);

	/* return value of tegra_isomgr_reserve is dvfs latency in usec */
	ret = tegra_isomgr_reserve(camera->isomgr_handle,
				bw,	/* KB/sec */
				lt);	/* usec */
	if (!ret) {
		dev_err(camera->dev,
		"%s: failed to reserve %lu KBps\n", __func__, bw);
		return -ENOMEM;
	}

	/* return value of tegra_isomgr_realize is dvfs latency in usec */
	ret = tegra_isomgr_realize(camera->isomgr_handle);
	if (ret)
		dev_dbg(camera->dev, "%s: camera isomgr latency is %d usec",
		__func__, ret);
	else {
		dev_err(camera->dev,
		"%s: failed to realize %lu KBps\n", __func__, bw);
		return -ENOMEM;
	}

	return 0;
}
#else
int tegra_camera_isomgr_register(struct tegra_camera *camera)
{
	return 0;
}

int tegra_camera_isomgr_unregister(struct tegra_camera *camera)
{
	return 0;
}

int tegra_camera_isomgr_request(struct tegra_camera *camera, unsigned long bw,
				unsigned long lt)
{
	return 0;
}
#endif
