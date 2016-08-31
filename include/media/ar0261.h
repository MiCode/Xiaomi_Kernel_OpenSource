/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __AR0261_H__
#define __AR0261_H__

#include <linux/ioctl.h>  /* For IOCTL macros */

#define AR0261_IOCTL_SET_MODE		_IOW('o', 1, struct ar0261_mode)
#define AR0261_IOCTL_GET_STATUS		_IOR('o', 2, __u8)
#define AR0261_IOCTL_SET_FRAME_LENGTH	_IOW('o', 3, __u32)
#define AR0261_IOCTL_SET_COARSE_TIME	_IOW('o', 4, __u32)
#define AR0261_IOCTL_SET_GAIN		_IOW('o', 5, __u16)
#define AR0261_IOCTL_GET_SENSORDATA _IOR('o', 6, struct ar0261_sensordata)
#define AR0261_IOCTL_SET_GROUP_HOLD	_IOW('o', 7, struct ar0261_ae)
#define AR0261_IOCTL_SET_HDR_COARSE_TIME	_IOW('o', 8, struct ar0261_hdr)

/* AR0261 registers */
#define AR0261_GROUP_PARAM_HOLD			(0x0104)
#define AR0261_COARSE_INTEGRATION_TIME	(0x0202)
#define AR0261_ANA_GAIN_GLOBAL			(0x305F)
#define AR0261_FRAME_LEN_LINES		(0x0340)
#define AR0261_COARSE_INTEGRATION_SHORT_TIME	(0x3088)

#define HDR_MODE_OVERRIDE_REGS		4
#define NORMAL_MODE_OVERRIDE_REGS	3
struct ar0261_mode {
	int xres;
	int yres;
	__u32 frame_length;
	__u32 coarse_time;
	__u32 coarse_time_short;
	__u16 gain;
	__u8 hdr_en;
};

struct ar0261_hdr {
	__u32 coarse_time_long;
	__u32 coarse_time_short;
};

struct ar0261_ae {
	__u32 frame_length;
	__u8  frame_length_enable;
	__u32 coarse_time;
	__u32 coarse_time_short;
	__u8  coarse_time_enable;
	__s32 gain;
	__u8  gain_enable;
};

struct ar0261_sensordata {
	__u32 fuse_id_size;
	__u8 fuse_id[16];
};

#ifdef __KERNEL__
struct ar0261_power_rail {
	struct regulator *dvdd;
	struct regulator *avdd;
	struct regulator *iovdd;
};

struct ar0261_platform_data {
	const char *mclk_name; /* NULL for default default_mclk */
	int (*power_on)(struct ar0261_power_rail *pw);
	int (*power_off)(struct ar0261_power_rail *pw);
};
#endif /* __KERNEL__ */

#endif  /* __AR0261_H__ */
