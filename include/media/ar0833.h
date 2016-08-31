/*
 * Copyright (C) 2010 Motorola, Inc.
 * Copyright (C) 2012-2013, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef __AR0833_H__
#define __AR0833_H__

#include <linux/ioctl.h>  /* For IOCTL macros */

#define AR0833_IOCTL_SET_MODE		_IOW('o', 1, struct ar0833_mode)
#define AR0833_IOCTL_SET_FRAME_LENGTH	_IOW('o', 2, __u32)
#define AR0833_IOCTL_SET_COARSE_TIME	_IOW('o', 3, __u32)
#define AR0833_IOCTL_SET_GAIN		_IOW('o', 4, __u16)
#define AR0833_IOCTL_GET_STATUS		_IOR('o', 5, __u8)
#define AR0833_IOCTL_GET_MODE		_IOR('o', 6, struct ar0833_modeinfo)
#define AR0833_IOCTL_SET_HDR_COARSE_TIME	_IOW('o', 7, struct ar0833_hdr)
#define AR0833_IOCTL_SET_GROUP_HOLD	_IOW('o', 8, struct ar0833_ae)

struct ar0833_mode {
	int xres;
	int yres;
	__u32 frame_length;
	__u32 coarse_time;
	__u32 coarse_time_short;
	__u16 gain;
	__u8 hdr_en;
};

struct ar0833_modeinfo {
	int xres;
	int yres;
	__u8 hdr;
	__u8 lanes;
	__u16 line_len;
	__u16 frame_len;
	__u16 coarse_time;
	__u16 coarse_time_2nd;
	__u16 xstart;
	__u16 xend;
	__u16 ystart;
	__u16 yend;
	__u16 xsize;
	__u16 ysize;
	__u16 gain;
	__u8 x_flip;
	__u8 y_flip;
	__u8 x_bin;
	__u8 y_bin;
	__u16 vt_pix_clk_div;
	__u16 vt_sys_clk_div;
	__u16 pre_pll_clk_div;
	__u16 pll_multi;
	__u16 op_pix_clk_div;
	__u16 op_sys_clk_div;
};

struct ar0833_hdr {
	__u32 coarse_time_long;
	__u32 coarse_time_short;
};

struct ar0833_ae {
	__u32 frame_length;
	__u8  frame_length_enable;
	__u32 coarse_time;
	__u32 coarse_time_short;
	__u8  coarse_time_enable;
	__s32 gain;
	__u8  gain_enable;
};

struct ar0833_sensordata {
	__u32 fuse_id_size;
	__u8  fuse_id[16];
};

#ifdef __KERNEL__
struct ar0833_power_rail {
	struct regulator *dvdd;
	struct regulator *avdd;
	struct regulator *iovdd;
};

struct ar0833_platform_data {
	int (*power_on)(struct ar0833_power_rail *pw);
	int (*power_off)(struct ar0833_power_rail *pw);
	const char *mclk_name;
};
#endif /* __KERNEL__ */

#endif  /* __AR0833_H__ */
