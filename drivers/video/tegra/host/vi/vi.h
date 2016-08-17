/*
 * drivers/video/tegra/host/vi/vi.h
 *
 * Tegra Graphics Host VI
 *
 * Copyright (c) 2012-2013, NVIDIA Corporation. All rights reserved.
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
};

#endif
