/*
 * include/linux/nvhost_vi_ioctl.h
 *
 * Tegra VI Driver
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __LINUX_NVHOST_VI_IOCTL_H
#define __LINUX_NVHOST_VI_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#if !defined(__KERNEL__)
#define __user
#endif

#define NVHOST_VI_IOCTL_MAGIC 'V'

/*
 * /dev/nvhost-ctrl-vi devices
 *
 * Opening a '/dev/nvhost-ctrl-vi' device node creates a way to send
 * ctrl ioctl to vi driver.
 *
 * /dev/nvhost-vi is for channel (context specific) operations. We use
 * /dev/nvhost-ctrl-vi for global (context independent) operations on
 * vi device.
 */

#define NVHOST_VI_IOCTL_ENABLE_TPG _IOW(NVHOST_VI_IOCTL_MAGIC, 1, uint)
#define NVHOST_VI_IOCTL_SET_EMC_INFO _IOW(NVHOST_VI_IOCTL_MAGIC, 2, uint)

#endif
