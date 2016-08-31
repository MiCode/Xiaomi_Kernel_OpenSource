/*
 * drivers/video/tegra/host/vi/vi.h
 *
 * Tegra Graphics Host ISP
 *
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
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

#ifndef __NVHOST_ISP_H__
#define __NVHOST_ISP_H__

#include "camera_priv_defs.h"

struct isp {
	struct platform_device *ndev;
#if defined(CONFIG_TEGRA_ISOMGR)
	tegra_isomgr_handle isomgr_handle;
#endif
	int dev_id;
};

extern const struct file_operations tegra_isp_ctrl_ops;
int nvhost_isp_t124_finalize_poweron(struct platform_device *);

#endif
