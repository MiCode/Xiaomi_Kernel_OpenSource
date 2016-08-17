/*
 * nvc_ov9772.h - ov9772 sensor driver
 *
 *  * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * Contributors:
 *	Phil Breczinski <pbreczinski@nvidia.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#ifndef __OV9772_H__
#define __OV9772_H__

#include <media/nvc.h>
#include <media/nvc_image.h>

/* See notes in the nvc.h file on the GPIO usage */
enum ov9772_gpio_type {
	OV9772_GPIO_TYPE_SHTDN = 0,
	OV9772_GPIO_TYPE_PWRDN,
	OV9772_GPIO_TYPE_I2CMUX,
	OV9772_GPIO_TYPE_GP1,
	OV9772_GPIO_TYPE_GP2,
	OV9772_GPIO_TYPE_GP3,
};

struct ov9772_power_rail {
	struct regulator *dvdd;
	struct regulator *avdd;
	struct regulator *dovdd;
};

struct ov9772_platform_data {
	unsigned cfg;
	unsigned num;
	unsigned sync;
	const char *dev_name;
	unsigned gpio_count;
	struct nvc_gpio_pdata *gpio; /* see nvc.h GPIO notes */
	struct nvc_imager_cap *cap;
	unsigned lens_focal_length; /* / _INT2FLOAT_DIVISOR */
	unsigned lens_max_aperture; /* / _INT2FLOAT_DIVISOR */
	unsigned lens_fnumber; /* / _INT2FLOAT_DIVISOR */
	unsigned lens_view_angle_h; /* / _INT2FLOAT_DIVISOR */
	unsigned lens_view_angle_v; /* / _INT2FLOAT_DIVISOR */
	int (*probe_clock)(unsigned long);
	int (*power_on)(struct ov9772_power_rail *);
	int (*power_off)(struct ov9772_power_rail *);
};
#endif  /* __OV9772_H__ */
