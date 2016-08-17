/*
 * drivers/video/tegra/host/host1x/host1x_hwctx.h
 *
 * Tegra Graphics Host HOST1X Hardware Context Interface
 *
 * Copyright (c) 2012, NVIDIA Corporation.
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

	u32 save_incrs;
	u32 save_thresh;
	u32 save_slots;

	struct mem_handle *restore;
	u32 *restore_virt;
	struct sg_table *restore_sgt;
	dma_addr_t restore_phys;
	u32 restore_size;
	u32 restore_incrs;
};

struct host1x_hwctx_handler {
	struct nvhost_hwctx_handler h;

	u32 syncpt;
	u32 waitbase;
	u32 restore_size;
	u32 restore_incrs;
	struct mem_handle *save_buf;
	u32 save_incrs;
	u32 save_thresh;
	u32 save_slots;
	struct sg_table *save_sgt;
	dma_addr_t save_phys;
	u32 save_size;
};

#endif
