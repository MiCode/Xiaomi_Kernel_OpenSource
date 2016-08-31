/*
 * include/linux/nvhost_isp_ioctl.h
 *
 * Tegra ISP Driver
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

#ifndef __LINUX_NVHOST_ISP_IOCTL_H
#define __LINUX_NVHOST_ISP_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define ISP_SOFT_ISO_CLIENT 0
#define ISP_HARD_ISO_CLIENT 1

#if !defined(__KERNEL__)
#define __user
#endif

struct isp_emc {
	uint isp_bw;
	uint isp_clk;
	uint bpp_input;
	uint bpp_output;
};

#define NVHOST_ISP_IOCTL_MAGIC 'I'

/*
 * /dev/nvhost-ctrl-isp devices
 *
 * Opening a '/dev/nvhost-ctrl-isp' device node creates a way to send
 * ctrl ioctl to isp driver.
 *
 * /dev/nvhost-isp is for channel (context specific) operations. We use
 * /dev/nvhost-ctrl-isp for global (context independent) operations on
 * isp device.
 */

#define NVHOST_ISP_IOCTL_SET_EMC _IOW(NVHOST_ISP_IOCTL_MAGIC, 1, struct isp_emc)

#endif

