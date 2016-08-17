/*
 * Copyright (c) 2011-2013 NVIDIA Corporation. All rights reserved.
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
	struct nvc_focus_nvc *nvc;
	struct nvc_focus_cap *cap;
	u16 lens_freq;	/* Lens actuarator resonance frequence in 0.1 Hz */
	/* 0 - use driver default,
	   1 - use ARC-RES1,
	   2 - use ARC-RES2,
	   3 - use ARC-RES1.5,
	   0xff - direct drive(ARC disabled)
	*/
	u8 arc_mode;
	int gpio_count;
	struct nvc_gpio_pdata *gpio;
	int (*power_on)(struct ad5816_power_rail *pw);
	int (*power_off)(struct ad5816_power_rail *pw);
	int (*detect)(void *buf, size_t size);
};

/* Register Definitions */
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
