/*
 * Tegra Graphics Host Hardware Context Interface
 *
 * Copyright (c) 2010-2014, NVIDIA Corporation.  All rights reserved.
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

#ifndef __NVHOST_HWCTX_H
#define __NVHOST_HWCTX_H

#include <linux/string.h>
#include <linux/kref.h>

#include <linux/nvhost.h>

struct nvhost_channel;
struct nvhost_cdma;
struct mem_mgr;
struct nvhost_dbg_session;

struct nvhost_hwctx {
	struct kref ref;
	struct nvhost_hwctx_handler *h;
	struct nvhost_channel *channel;
	bool valid;
	bool has_timedout;
	u32 timeout_ms_max;
	bool timeout_debug_dump;

	struct mem_mgr *memmgr;

	struct mem_handle *error_notifier_ref;
	struct nvhost_notification *error_notifier;
	void *error_notifier_va;

	u32 save_incrs;
	u32 save_slots;

	u32 restore_incrs;
	void *priv; /* chip support state */

	struct list_head as_share_bound_list_node;
	struct nvhost_as_share *as_share;
	struct nvhost_dbg_session *dbg_session;
};

struct nvhost_hwctx_handler {
	struct nvhost_hwctx * (*alloc) (struct nvhost_hwctx_handler *h,
			struct nvhost_channel *ch);
	void (*get) (struct nvhost_hwctx *ctx);
	void (*put) (struct nvhost_hwctx *ctx);
	void (*save_push) (struct nvhost_hwctx *ctx,
			struct nvhost_cdma *cdma);
	void (*restore_push) (struct nvhost_hwctx *ctx,
			struct nvhost_cdma *cdma);
	u32 syncpt;
	u32 waitbase;
	void *priv;
};


struct hwctx_reginfo {
	unsigned int offset:12;
	unsigned int count:16;
	unsigned int type:2;
        unsigned int rst_off;  //restore reg offset.
};

enum {
	HWCTX_REGINFO_DIRECT = 0,
	HWCTX_REGINFO_INDIRECT,
	HWCTX_REGINFO_INDIRECT_4X
};

#define HWCTX_REGINFO(offset, count, type) {offset, count, HWCTX_REGINFO_##type, offset}
#define HWCTX_REGINFO_RST(offset, count, type, rst) {offset, count, HWCTX_REGINFO_##type, rst}

#endif
