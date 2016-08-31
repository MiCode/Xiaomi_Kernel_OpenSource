/*
 * Copyright (c) 2011-2013 NVIDIA Corporation. All rights reserved.
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

#ifndef __AD5816_H__
#define __AD5816_H__

#include <linux/miscdevice.h>
#include <media/nvc_focus.h>
#include <media/nvc.h>

struct ad5816_power_rail {
	struct regulator *vdd;
	struct regulator *vdd_i2c;
};

struct ad5816_platform_data {
	int cfg;
	int num;
	int sync;
	const char *dev_name;
	struct nvc_focus_nvc (*nvc);
	struct nvc_focus_cap (*cap);
	struct ad5816_pdata_info (*info);
	int gpio_count;
	struct nvc_gpio_pdata *gpio;
	int (*power_on)(struct ad5816_power_rail *pw);
	int (*power_off)(struct ad5816_power_rail *pw);
	int (*detect)(void *buf, size_t size);
};

struct ad5816_pdata_info {
	float focal_length;
	float fnumber;
	__u32 settle_time;
	__s16 pos_low;
	__s16 pos_high;
	__s16 limit_low;
	__s16 limit_high;
	int move_timeoutms;
	__u32 focus_hyper_ratio;
	__u32 focus_hyper_div;
};

// Register Definitions
#define IC_INFO			0x00
#define IC_VERSION		0x01
#define CONTROL			0x02
#define VCM_CODE_MSB		0x03
#define VCM_CODE_LSB		0x04
#define STATUS			0x05
#define MODE			0x06
#define VCM_FREQ		0x07
#define VCM_THRESHOLD		0x08
#define SCL_LOW_DETECTION	0xC0


#endif
/* __AD5816_H__ */
