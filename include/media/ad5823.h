/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __AD5823_H__
#define __AD5823_H__

#include <media/nvc_focus.h>
#include <media/nvc.h>

typedef enum {
	AD5823_VREG_VDD = 0,
	AD5823_VREG_VDD_AF,
	AD5823_VREG_VDD_I2C,
	AD5823_VREG_VDD_CAM_MB,
	AD5823_VREG_VDD_CAM_AF
} ad5823_vreg;

typedef enum {
	AD5823_GPIO_RESET = 0,
	AD5823_GPIO_I2CMUX,
	AD5823_GPIO_GP1,
	AD5823_GPIO_GP2,
	AD5823_GPIO_GP3,
	AD5823_GPIO_CAM_AF_PWDN
} ad5823_gpio_types;

struct ad5823_power_rail {
	struct regulator *vdd;
	struct regulator *vdd_i2c;
};

struct ad5823_platform_data {
	int cfg;
	int num;
	int sync;
	const char *dev_name;
	struct nvc_focus_nvc (*nvc);
	struct nvc_focus_cap (*cap);
	struct ad5823_pdata_info (*info);
	int gpio_count;
	struct nvc_gpio_pdata *gpio;
	int (*power_on)(struct ad5823_power_rail *pw);
	int (*power_off)(struct ad5823_power_rail *pw);
};

struct ad5823_pdata_info {
	__u32 focal_length;
	__u32 fnumber;
	__u32 max_aperture;
	__u32 settle_time;
	__s16 pos_low;
	__s16 pos_high;
	__s16 limit_low;
	__s16 limit_high;
	int move_timeoutms;
	__u32 focus_hyper_ratio;
	__u32 focus_hyper_div;
};


#define RESET			0x01
#define MODE			0x02
#define VCM_MOVE_TIME		0x03
#define VCM_CODE_MSB	0x04
#define VCM_CODE_LSB	0x05
#define VCM_THRESHOLD_MSB	0x06
#define VCM_THRESHOLD_LSB	0x07

#endif
/* __AD5823_H__ */
