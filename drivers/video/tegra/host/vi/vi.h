/*
 * drivers/video/tegra/host/vi/vi.h
 *
 * Tegra Graphics Host VI
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __NVHOST_VI_H__
#define __NVHOST_VI_H__

#include "camera_priv_defs.h"

struct vi {
	struct tegra_camera *camera;
	struct platform_device *ndev;
	struct regulator *reg;
	uint vi_bw;
#if defined(CONFIG_TEGRA_ISOMGR)
	tegra_isomgr_handle isomgr_handle;
#endif
};

extern const struct file_operations tegra_vi_ctrl_ops;
int nvhost_vi_prepare_poweroff(struct platform_device *);
int nvhost_vi_finalize_poweron(struct platform_device *);
int nvhost_vi_init(struct platform_device *);
void nvhost_vi_deinit(struct platform_device *);
void nvhost_vi_reset(struct platform_device *);

#endif
