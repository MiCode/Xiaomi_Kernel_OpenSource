/*
 * drivers/video/tegra/host/gr3d/gr3d_t20.h
 *
 * Tegra Graphics Host 3D for Tegra2
 *
 * Copyright (c) 2011-2012, NVIDIA Corporation.
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

#ifndef __NVHOST_GR3D_GR3D_T20_H
#define __NVHOST_GR3D_GR3D_T20_H

#include <linux/types.h>

struct nvhost_hwctx_handler;
struct nvhost_hwctx;
struct nvhost_channel;
struct platform_device;

struct nvhost_hwctx_handler *nvhost_gr3d_t20_ctxhandler_init(
		u32 syncpt, u32 waitbase,
		struct nvhost_channel *ch);

int nvhost_gr3d_t20_read_reg(
	struct platform_device *dev,
	struct nvhost_channel *channel,
	struct nvhost_hwctx *hwctx,
	u32 offset,
	u32 *value);

#endif
