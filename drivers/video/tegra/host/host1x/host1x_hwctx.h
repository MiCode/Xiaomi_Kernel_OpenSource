/*
 * drivers/video/tegra/host/host1x/host1x_hwctx.h
 *
 * Tegra Graphics Host HOST1X Hardware Context Interface
 *
 * Copyright (c) 2012-2013, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __NVHOST_HOST1X_HWCTX_H
#define __NVHOST_HOST1X_HWCTX_H

#include <linux/kref.h>
#include "nvhost_hwctx.h"

struct nvhost_hwctx_handler;
struct nvhost_channel;
struct sg_table;

#define to_host1x_hwctx_handler(handler) \
	container_of((handler), struct host1x_hwctx_handler, h)
#define to_host1x_hwctx(h) container_of((h), struct host1x_hwctx, hwctx)
#define host1x_hwctx_handler(_hwctx) to_host1x_hwctx_handler((_hwctx)->hwctx.h)

struct host1x_hwctx {
	struct nvhost_hwctx hwctx;

	u32 restore_size;
	u32 *cpuva;
	dma_addr_t iova;
	bool mem_flag;
};

struct host1x_hwctx_handler {
	struct nvhost_hwctx_handler h;

	u32 restore_size;
	u32 restore_incrs;
	u32 save_incrs;
	u32 save_slots;
	u32 save_size;
	u32 *cpuva;
	dma_addr_t iova;
};

#endif
