/**
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
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

#ifndef __IMX132_H__
#define __IMX132_H__

#include <linux/ioctl.h>  /* For IOCTL macros */

#define IMX132_IOCTL_SET_MODE		_IOW('o', 1, struct imx132_mode)
#define IMX132_IOCTL_GET_STATUS		_IOR('o', 2, __u8)
#define IMX132_IOCTL_SET_FRAME_LENGTH	_IOW('o', 3, __u32)
#define IMX132_IOCTL_SET_COARSE_TIME	_IOW('o', 4, __u32)
#define IMX132_IOCTL_SET_GAIN		_IOW('o', 5, __u16)
#define IMX132_IOCTL_GET_FUSEID		_IOR('o', 6, struct nvc_fuseid)
#define IMX132_IOCTL_SET_GROUP_HOLD	_IOW('o', 7, struct imx132_ae)

/* IMX132 registers */
#define IMX132_GROUP_PARAM_HOLD			(0x0104)
#define IMX132_COARSE_INTEGRATION_TIME_15_8	(0x0202)
#define IMX132_COARSE_INTEGRATION_TIME_7_0	(0x0203)
#define IMX132_ANA_GAIN_GLOBAL			(0x0205)
#define IMX132_FRAME_LEN_LINES_15_8		(0x0340)
#define IMX132_FRAME_LEN_LINES_7_0		(0x0341)
#define IMX132_FUSE_ID_REG		(0x3500)

#define NUM_OF_FRAME_LEN_REG		2
#define NUM_OF_COARSE_TIME_REG		2
struct imx132_mode {
	int xres;
	int yres;
	__u32 frame_length;
	__u32 coarse_time;
	__u16 gain;
};

struct imx132_ae {
	__u32 frame_length;
	__u8  frame_length_enable;
	__u32 coarse_time;
	__u8  coarse_time_enable;
	__s32 gain;
	__u8  gain_enable;
};

#ifdef __KERNEL__
struct imx132_power_rail {
	struct regulator *dvdd;
	struct regulator *avdd;
	struct regulator *iovdd;
};

struct imx132_platform_data {
	unsigned int cam2_gpio;
	bool ext_reg;
	const char *mclk_name; /* NULL for default */
	int (*power_on)(struct imx132_power_rail *pw);
	int (*power_off)(struct imx132_power_rail *pw);
};
#endif /* __KERNEL__ */

#endif  /* __IMX132_H__ */
