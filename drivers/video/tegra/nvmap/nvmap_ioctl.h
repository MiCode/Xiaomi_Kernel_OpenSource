/*
 * drivers/video/tegra/nvmap/nvmap_ioctl.h
 *
 * ioctl declarations for nvmap
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION. All rights reserved.
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

#ifndef __VIDEO_TEGRA_NVMAP_IOCTL_H
#define __VIDEO_TEGRA_NVMAP_IOCTL_H

#include <linux/ioctl.h>

#ifdef  __KERNEL__
#include <linux/file.h>
#include <linux/nvmap.h>
#endif

/* Hack for L4T builds.
 * gstreamer is directly including this header and
 * looking for ioctls param struct definitions. This hack
 * is necessary till user space gstreamer is fixed to use
 * linux/nvmap.h file.
 *
 */
#ifndef __KERNEL__
struct nvmap_create_handle {
	union {
		__u32 key;	/* ClaimPreservedHandle */
		__u32 id;	/* FromId */
		__u32 size;	/* CreateHandle */
		__s32 fd;	/* DmaBufFd or FromFd */
	};
	__u32 handle;
};

#define NVMAP_IOC_MAGIC 'N'
/* Returns a global ID usable to allow a remote process to create a handle
 * reference to the same handle */
#define NVMAP_IOC_GET_ID  _IOWR(NVMAP_IOC_MAGIC, 13, struct nvmap_create_handle)
#endif

#ifdef  __KERNEL__
int nvmap_ioctl_pinop(struct file *filp, bool is_pin, void __user *arg);

int nvmap_ioctl_get_param(struct file *filp, void __user* arg);

int nvmap_ioctl_getid(struct file *filp, void __user *arg);

int nvmap_ioctl_getfd(struct file *filp, void __user *arg);

int nvmap_ioctl_alloc(struct file *filp, void __user *arg);

int nvmap_ioctl_alloc_kind(struct file *filp, void __user *arg);

int nvmap_ioctl_free(struct file *filp, unsigned long arg);

int nvmap_ioctl_create(struct file *filp, unsigned int cmd, void __user *arg);

int nvmap_map_into_caller_ptr(struct file *filp, void __user *arg);

int nvmap_ioctl_cache_maint(struct file *filp, void __user *arg);

int nvmap_ioctl_rw_handle(struct file *filp, int is_read, void __user* arg);

int nvmap_ioctl_share_dmabuf(struct file *filp, void __user *arg);

#endif	/* __KERNEL__ */

#endif	/*  __VIDEO_TEGRA_NVMAP_IOCTL_H */
