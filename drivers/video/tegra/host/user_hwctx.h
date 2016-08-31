/*
 * drivers/video/tegra/host/host1x/user_hwctx.h
 *
 * Tegra Graphics Host HOST1X Hardware Context Interface
 *
 * Copyright (c) 2013, NVIDIA Corporation.
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

#ifndef __NVHOST_USER_HWCTX_H
#define __NVHOST_USER_HWCTX_H

#include <linux/kref.h>
#include "nvhost_hwctx.h"

struct nvhost_hwctx_handler;
struct nvhost_channel;
struct sg_table;

#define to_user_hwctx_handler(handler) \
	container_of((handler), struct user_hwctx_handler, h)
#define to_user_hwctx(h) container_of((h), struct user_hwctx, hwctx)
#define user_hwctx_handler(_hwctx) to_user_hwctx_handler((_hwctx)->hwctx.h)

struct user_hwctx {
	struct nvhost_hwctx hwctx;

	struct mem_handle *save_buf;
	u32 save_slots;
	struct sg_table *save_sgt;
	u32 save_size;
	u32 save_offset;

	struct mem_handle *restore;
	u32 restore_size;
	u32 *restore_virt;
	struct sg_table *restore_sgt;
	u32 restore_offset;
};

struct user_hwctx_handler {
	struct nvhost_hwctx_handler h;
};

struct nvhost_hwctx_handler *user_ctxhandler_init(u32 syncpt,
		u32 waitbase, struct nvhost_channel *ch);
void user_ctxhandler_free(struct nvhost_hwctx_handler *h);
int user_hwctx_set_restore(struct user_hwctx *ctx,
		ulong mem, u32 offset, u32 words);
int user_hwctx_set_save(struct user_hwctx *ctx,
		ulong mem, u32 offset, u32 words, struct nvhost_reloc *reloc);

#endif
