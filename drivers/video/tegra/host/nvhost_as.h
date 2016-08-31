/*
 * drivers/video/tegra/host/nvhost_as.h
 *
 * Tegra Host Address Space
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef __NVHOST_AS_H
#define __NVHOST_AS_H

#include <linux/atomic.h>
#include <linux/nvhost_as_ioctl.h>

#include "nvhost_channel.h"
#include "nvhost_acm.h"
#include "nvhost_memmgr.h"
#include "chip_support.h"

struct nvhost_as_share;

struct nvhost_as_moduleops {
	int (*alloc_share)(struct nvhost_as_share *);
	int (*release_share)(struct nvhost_as_share *);
	int (*alloc_space)(struct nvhost_as_share *,
			   struct nvhost_as_alloc_space_args*);
	int (*free_space)(struct nvhost_as_share *,
			  struct nvhost_as_free_space_args*);
	int (*bind_hwctx)(struct nvhost_as_share *,
			  struct nvhost_hwctx *);
	int (*map_buffer)(struct nvhost_as_share *,
			  int memmgr_fd,
			  ulong mem_id,
			  u64 *offset_align,
			  u32 flags /*NVHOST_AS_MAP_BUFFER_FLAGS_*/);
	int (*unmap_buffer)(struct nvhost_as_share *, u64 offset);
};

struct nvhost_as_share {
	struct nvhost_as *as;
	atomic_t ref_cnt;
	int id;

	struct nvhost_master *host;
	struct nvhost_channel *ch;
	struct device *as_dev;

	struct mutex bound_list_lock;
	struct list_head bound_list;

	struct list_head share_list_node;
	void *priv; /* holds pointer to module support for the share */
};

struct nvhost_as {
	struct mutex share_list_lock;
	struct list_head share_list; /* list of all shares */
	struct nvhost_channel *ch;
	int last_share_id; /* dummy allocator for now */
};


int nvhost_as_init_device(struct platform_device *dev);
int nvhost_as_alloc_share(struct nvhost_channel *ch,
			  struct nvhost_as_share **as);
int nvhost_as_release_share(struct nvhost_as_share *as_share,
			    struct nvhost_hwctx *hwctx);


int nvhost_as_ioctl_alloc_space(struct nvhost_as_share *as_share,
				struct nvhost_as_alloc_space_args *args);
int nvhost_as_ioctl_free_space(struct nvhost_as_share *as_share,
			       struct nvhost_as_free_space_args *args);
int nvhost_as_ioctl_bind_channel(struct nvhost_as_share *as_share,
				 struct nvhost_as_bind_channel_args *args);
int nvhost_as_ioctl_map_buffer(struct nvhost_as_share *as_share,
			       struct nvhost_as_map_buffer_args *args);
int nvhost_as_ioctl_unmap_buffer(struct nvhost_as_share *as_share,
				 struct nvhost_as_unmap_buffer_args *args);

/* struct file_operations driver interface */
int nvhost_as_dev_open(struct inode *inode, struct file *filp);
int nvhost_as_dev_release(struct inode *inode, struct file *filp);
long nvhost_as_dev_ctl(struct file *filp, unsigned int cmd, unsigned long arg);
#endif
