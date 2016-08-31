/*
 * drivers/video/tegra/camera/camera_power.h
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

#ifndef __DRIVERS_VIDEO_TEGRA_CAMERA_CAMERA_POWER_H
#define __DRIVERS_VIDEO_TEGRA_CAMERA_CAMERA_POWER_H
#include "camera_priv_defs.h"

int tegra_camera_power_on(struct tegra_camera *camera);
int tegra_camera_power_off(struct tegra_camera *camera);
int tegra_camera_powergate_init(struct tegra_camera *camera);

#endif

