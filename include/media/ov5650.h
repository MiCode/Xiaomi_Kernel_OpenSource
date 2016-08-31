/*
 * Copyright (C) 2010 Motorola, Inc.
 * Copyright (C) 2011 NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef __OV5650_H__
#define __OV5650_H__

#include <linux/ioctl.h>  /* For IOCTL macros */

#define OV5650_IOCTL_SET_MODE			_IOW('o', 1, struct ov5650_mode)
#define OV5650_IOCTL_SET_FRAME_LENGTH	_IOW('o', 2, __u32)
#define OV5650_IOCTL_SET_COARSE_TIME	_IOW('o', 3, __u32)
#define OV5650_IOCTL_SET_GAIN			_IOW('o', 4, __u16)
#define OV5650_IOCTL_GET_STATUS			_IOR('o', 5, __u8)
#define OV5650_IOCTL_SET_BINNING    	_IOW('o', 6, __u8)
#define OV5650_IOCTL_TEST_PATTERN		_IOW('o', 7, enum ov5650_test_pattern)
#define OV5650_IOCTL_SET_GROUP_HOLD	    _IOW('o', 8, struct ov5650_ae)
#define OV5650_IOCTL_SET_CAMERA_MODE	_IOW('o', 10, __u32)
#define OV5650_IOCTL_SYNC_SENSORS		_IOW('o', 11, __u32)
#define OV5650_IOCTL_GET_FUSEID		_IOR('o', 12, struct nvc_fuseid)

/* OV5650 registers */
#define OV5650_SRM_GRUP_ACCESS          (0x3212)
#define OV5650_ARRAY_CONTROL_01         (0x3621)
#define OV5650_ANALOG_CONTROL_D         (0x370D)
#define OV5650_TIMING_TC_REG_18         (0x3818)
#define OV5650_TIMING_CONTROL_HS_HIGH   (0x3800)
#define OV5650_TIMING_CONTROL_HS_LOW    (0x3801)
#define OV5650_TIMING_CONTROL_VS_HIGH   (0x3802)
#define OV5650_TIMING_CONTROL_VS_LOW    (0x3803)
#define OV5650_TIMING_HW_HIGH           (0x3804)
#define OV5650_TIMING_HW_LOW            (0x3805)
#define OV5650_TIMING_VH_HIGH           (0x3806)
#define OV5650_TIMING_VH_LOW            (0x3807)
#define OV5650_TIMING_TC_REG_18         (0x3818)
#define OV5650_TIMING_HREFST_MAN_HIGH   (0x3824)
#define OV5650_TIMING_HREFST_MAN_LOW    (0x3825)
#define OV5650_H_BINNING_BIT            (1 << 7)
#define OV5650_H_SUBSAMPLING_BIT        (1 << 6)
#define OV5650_V_BINNING_BIT            (1 << 6)
#define OV5650_V_SUBSAMPLING_BIT        (1 << 0)
#define OV5650_GROUP_HOLD_BIT		(1 << 7)
#define OV5650_GROUP_LAUNCH_BIT		(1 << 5)
#define OV5650_GROUP_HOLD_END_BIT	(1 << 4)
#define OV5650_GROUP_ID(id)		(id)

enum ov5650_test_pattern {
	TEST_PATTERN_NONE,
	TEST_PATTERN_COLORBARS,
	TEST_PATTERN_CHECKERBOARD
};

struct ov5650_mode {
	int xres;
	int yres;
	__u32 frame_length;
	__u32 coarse_time;
	__u16 gain;
};

struct ov5650_ae {
	__u32 frame_length;
	__u8 frame_length_enable;
	__u32 coarse_time;
	__u8 coarse_time_enable;
	__s32 gain;
	__u8 gain_enable;
};

#ifdef __KERNEL__
struct ov5650_platform_data {
	int (*power_on)(struct device *);
	int (*power_off)(struct device *);
	void (*synchronize_sensors)(void);
};
#endif /* __KERNEL__ */

#endif  /* __OV5650_H__ */

