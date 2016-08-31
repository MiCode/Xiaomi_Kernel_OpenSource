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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __AD5823_H__
#define __AD5823_H__

#include <media/nvc_focus.h>
#include <linux/ioctl.h>  /* For IOCTL macros */

#define AD5823_IOCTL_GET_CONFIG   _IOR('o', 1, struct ad5823_config)
#define AD5823_IOCTL_SET_POSITION _IOW('o', 2, u32)
#define AD5823_IOCTL_SET_CAL_DATA _IOW('o', 3, struct ad5823_cal_data)
#define AD5823_IOCTL_SET_CONFIG _IOW('o', 4, struct nv_focuser_config)

/* address */
#define AD5823_RESET                (0x1)
#define AD5823_MODE                 (0x2)
#define AD5823_VCM_MOVE_TIME        (0x3)
#define AD5823_VCM_CODE_MSB         (0x4)
#define AD5823_VCM_CODE_LSB         (0x5)
#define AD5823_VCM_THRESHOLD_MSB    (0x6)
#define AD5823_VCM_THRESHOLD_LSB    (0x7)
#define AD5823_RING_CTRL            (1 << 2)

#define AD5823_PWR_DEV_OFF          (0)
#define AD5823_PWR_DEV_ON           (1)

struct ad5823_config {
	__u32 settle_time;
	__u32 actuator_range;
	__u32 pos_low;
	__u32 pos_high;
	float focal_length;
	float fnumber;
	float max_aperture;
};

struct ad5823_cal_data {
	__u32 pos_low;
	__u32 pos_high;
};

struct ad5823_platform_data {
	int gpio;
	int (*power_on)(struct ad5823_platform_data *);
	int (*power_off)(struct ad5823_platform_data *);
	int pwr_dev;
};
#endif  /* __AD5820_H__ */

