/*
* ov9726.h
*
* Copyright (c) 2011, NVIDIA, All Rights Reserved.
*
* This file is licensed under the terms of the GNU General Public License
* version 2. This program is licensed "as is" without any warranty of any
* kind, whether express or implied.
*/

#ifndef __OV9726_H__
#define __OV9726_H__

#include <linux/ioctl.h>

#define OV9726_I2C_ADDR			0x20

#define OV9726_IOCTL_SET_MODE		_IOW('o', 1, struct ov9726_mode)
#define OV9726_IOCTL_SET_FRAME_LENGTH	_IOW('o', 2, __u32)
#define OV9726_IOCTL_SET_COARSE_TIME	_IOW('o', 3, __u32)
#define OV9726_IOCTL_SET_GAIN		_IOW('o', 4, __u16)
#define OV9726_IOCTL_GET_STATUS	_IOR('o', 5, __u8)
#define OV9726_IOCTL_SET_GROUP_HOLD	_IOW('o', 6, struct ov9726_ae)
#define OV9726_IOCTL_GET_FUSEID	_IOR('o', 7, struct nvc_fuseid)

struct ov9726_mode {
	int	mode_id;
	int	xres;
	int	yres;
	__u32	frame_length;
	__u32	coarse_time;
	__u16	gain;
};

struct ov9726_ae {
	__u32 frame_length;
	__u32 coarse_time;
	__u16 gain;
	__u8 frame_length_enable;
	__u8 coarse_time_enable;
	__u8 gain_enable;
};

struct ov9726_reg {
	__u16	addr;
	__u16	val;
};

#ifdef __KERNEL__
#define OV9726_REG_FRAME_LENGTH_HI	0x340
#define OV9726_REG_FRAME_LENGTH_LO	0x341
#define OV9726_REG_COARSE_TIME_HI	0x202
#define OV9726_REG_COARSE_TIME_LO	0x203
#define OV9726_REG_GAIN_HI		0x204
#define OV9726_REG_GAIN_LO		0x205

#define OV9726_MAX_RETRIES		3

#define OV9726_TABLE_WAIT_MS		0
#define OV9726_TABLE_END		1

struct ov9726_platform_data {
	int	(*power_on)(struct device *);
	int	(*power_off)(struct device *);
	const char *mclk_name;
	unsigned    gpio_rst;
	bool        rst_low_active;
	unsigned    gpio_pwdn;
	bool        pwdn_low_active;
};
#endif /* __KERNEL__ */

#endif  /* __OV9726_H__ */

