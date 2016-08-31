/*
* Copyright (c) 2012-2014, NVIDIA Corporation.  All rights reserved.
* Copyright (C) 2016 XiaoMi, Inc.
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
* along with this program.  If not, see <http:
*/

#ifndef __IMX179_H__
#define __IMX179_H__

#include <linux/ioctl.h>  /* For IOCTL macros */
#include <linux/edp.h>
#include <media/nvc.h>
#include <media/nvc_image.h>

#define IMX179_IOCTL_SET_MODE		_IOW('o', 1, struct imx179_mode)
#define IMX179_IOCTL_GET_STATUS		_IOR('o', 2, __u8)
#define IMX179_IOCTL_SET_FRAME_LENGTH	_IOW('o', 3, __u32)
#define IMX179_IOCTL_SET_COARSE_TIME	_IOW('o', 4, __u32)
#define IMX179_IOCTL_SET_GAIN		_IOW('o', 5, __u16)
#define IMX179_IOCTL_GET_SENSORDATA	_IOR('o', 6, struct imx179_sensordata)
#define IMX179_IOCTL_SET_GROUP_HOLD	_IOW('o', 7, struct imx179_ae)
#define IMX179_IOCTL_GET_OTPDATA        _IOR('o', 8, struct imx179_otp)
#define IMX179_IOCTL_GET_OTPVEND        _IOR('o', 9, struct imx179_otp)
#define IMX179_IOCTL_SET_POWER		_IOW('o', 20, __u32)
#define IMX179_IOCTL_GET_FLASH_CAP	_IOR('o', 30, __u32)
#define IMX179_IOCTL_SET_FLASH_MODE 	_IOW('o', 31, \
						struct imx179_flash_control)

struct imx179_mode {
	int xres;
	int yres;
	__u32 frame_length;
	__u32 coarse_time;
	__u16 gain;
};

struct imx179_ae {
	__u32 frame_length;
	__u8  frame_length_enable;
	__u32 coarse_time;
	__u8  coarse_time_enable;
	__s32 gain;
	__u8  gain_enable;
};

struct imx179_sensordata {
	__u32 fuse_id_size;
	__u8  fuse_id[16];
};

struct imx179_otp {
    __u32 otp_size;
    __u8  otp_data[803];
};
struct imx179_flash_control {
	u8 enable;
	u8 edge_trig_en;
	u8 start_edge;
	u8 repeat;
	u16 delay_frm;
};


#ifdef __KERNEL__
struct imx179_power_rail {
	struct regulator *dvdd;
	struct regulator *avdd;
	struct regulator *iovdd;
	struct regulator *ext_reg1;
	struct regulator *ext_reg2;
	struct regulator *ext_reg3;
};

struct imx179_platform_data {
	struct imx179_flash_control flash_cap;
	const char *mclk_name; /* NULL for default default_mclk */
	struct edp_client edpc_config;
	unsigned int cam1_gpio;
	unsigned int reset_gpio;
	unsigned int af_gpio;
	bool ext_reg;
	int (*power_on)(struct imx179_power_rail *pw);
	int (*power_off)(struct imx179_power_rail *pw);
};
#endif /* __KERNEL__ */

#endif  /* __IMX179_H__ */
