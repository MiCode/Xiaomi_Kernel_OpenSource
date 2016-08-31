/*
 * drivers/video/tegra/host/t30/3dctx_t114.h
 *
 * Tegra Graphics Host Context Switching for Tegra11x SOCs
 *
 * Copyright (c) 2011-2013, NVIDIA Corporation.
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

#ifndef __NVHOST_3DCTX_T114_H
#define __NVHOST_3DCTX_T114_H

struct nvhost_hwctx_handler;
struct platform_device;
struct nvhost_channel;
struct nvhost_hwctx;
struct mem_mgr;

struct nvhost_hwctx_handler *nvhost_gr3d_t114_ctxhandler_init(u32 syncpt,
	u32 base, struct nvhost_channel *ch);

int nvhost_gr3d_t114_init(struct platform_device *dev);
void nvhost_gr3d_t114_deinit(struct platform_device *dev);
int nvhost_gr3d_t114_prepare_power_off(struct platform_device *dev);
int nvhost_gr3d_t114_finalize_power_on(struct platform_device *dev);

#endif
