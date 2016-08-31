/*
 * drivers/video/tegra/camera/camera_irq.h
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

#ifndef __DRIVERS_VIDEO_TEGRA_CAMERA_CAMERA_IRQ_H
#define __DRIVERS_VIDEO_TEGRA_CAMERA_CAMERA_IRQ_H

#include "camera_priv_defs.h"

int tegra_camera_intr_init(struct tegra_camera *camera);
int tegra_camera_intr_free(struct tegra_camera *camera);
void tegra_camera_stats_worker(struct work_struct *work);
int tegra_camera_enable_irq(struct tegra_camera *camera);
int tegra_camera_disable_irq(struct tegra_camera *camera);

#endif
