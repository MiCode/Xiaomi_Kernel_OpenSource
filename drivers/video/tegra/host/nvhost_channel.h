/*
 * drivers/video/tegra/host/nvhost_channel.h
 *
 * Tegra Graphics Host Channel
 *
 * Copyright (c) 2010-2012, NVIDIA Corporation.
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

#ifndef __NVHOST_CHANNEL_H
#define __NVHOST_CHANNEL_H

#include <linux/cdev.h>
#include <linux/io.h>
#include "nvhost_cdma.h"

#define NVHOST_MAX_WAIT_CHECKS		256
#define NVHOST_MAX_GATHERS		512
#define NVHOST_MAX_HANDLES		1280
#define NVHOST_MAX_POWERGATE_IDS	2

struct nvhost_master;
struct platform_device;
struct nvhost_channel;
struct nvhost_hwctx;

struct nvhost_channel {
	int refcount;
	int chid;
	u32 syncpt_id;
	struct mutex reflock;
	struct mutex submitlock;
	void __iomem *aperture;
	struct nvhost_hwctx *cur_ctx;
	struct device *node;
	struct platform_device *dev;
	struct cdev cdev;
	struct nvhost_hwctx_handler *ctxhandler;
	struct nvhost_cdma cdma;
};

int nvhost_channel_init(struct nvhost_channel *ch,
	struct nvhost_master *dev, int index);

int nvhost_channel_submit(struct nvhost_job *job);

struct nvhost_channel *nvhost_getchannel(struct nvhost_channel *ch);
void nvhost_putchannel(struct nvhost_channel *ch, struct nvhost_hwctx *ctx);
int nvhost_channel_suspend(struct nvhost_channel *ch);

int nvhost_channel_drain_read_fifo(struct nvhost_channel *ch,
			u32 *ptr, unsigned int count, unsigned int *pending);

int nvhost_channel_read_reg(struct nvhost_channel *channel,
	struct nvhost_hwctx *hwctx,
	u32 offset, u32 *value);

struct nvhost_channel *nvhost_alloc_channel_internal(int chindex,
	int max_channels, int *current_channel_count);

void nvhost_free_channel_internal(struct nvhost_channel *ch,
	int *current_channel_count);

int nvhost_channel_save_context(struct nvhost_channel *ch);

#endif
