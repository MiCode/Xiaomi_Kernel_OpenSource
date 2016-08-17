/**
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __IMX135_H__
#define __IMX135_H__

#include <linux/ioctl.h>  /* For IOCTL macros */
#include <media/nvc.h>
#include <media/nvc_image.h>

#define IMX135_IOCTL_SET_MODE		_IOW('o', 1, struct imx135_mode)
#define IMX135_IOCTL_GET_STATUS		_IOR('o', 2, __u8)
#define IMX135_IOCTL_SET_FRAME_LENGTH	_IOW('o', 3, __u32)
#define IMX135_IOCTL_SET_COARSE_TIME	_IOW('o', 4, __u32)
#define IMX135_IOCTL_SET_GAIN		_IOW('o', 5, __u16)
#define IMX135_IOCTL_GET_FUSEID		_IOR('o', 6, struct nvc_fuseid)
#define IMX135_IOCTL_SET_GROUP_HOLD	_IOW('o', 7, struct imx135_ae)
#define IMX135_IOCTL_SET_HDR_COARSE_TIME	_IOW('o', 8, struct imx135_hdr)
#define IMX135_IOCTL_SET_POWER		_IOW('o', 20, __u32)
#define IMX135_IOCTL_GET_FLASH_CAP	_IOR('o', 30, __u32)
#define IMX135_IOCTL_SET_FLASH_MODE	_IOW('o', 31, \
						struct imx135_flash_control)

struct imx135_mode {
	int xres;
	int yres;
	__u32 frame_length;
	__u32 coarse_time;
	__u32 coarse_time_short;
	__u16 gain;
	__u8 hdr_en;
};

struct imx135_hdr {
	__u32 coarse_time_long;
	__u32 coarse_time_short;
};

struct imx135_ae {
	__u32 frame_length;
	__u8  frame_length_enable;
	__u32 coarse_time;
	__u32 coarse_time_short;
	__u8  coarse_time_enable;
	__s32 gain;
	__u8  gain_enable;
};

struct imx135_flash_control {
	u8 enable;
	u8 edge_trig_en;
	u8 start_edge;
	u8 repeat;
	u16 delay_frm;
};

#ifdef __KERNEL__
struct imx135_power_rail {
	struct regulator *dvdd;
	struct regulator *avdd;
	struct regulator *iovdd;
};

struct imx135_platform_data {
	struct imx135_flash_control flash_cap;
	int (*power_on)(struct imx135_power_rail *pw);
	int (*power_off)(struct imx135_power_rail *pw);
};
#endif /* __KERNEL__ */

#endif  /* __IMX135_H__ */
