/*
* imx091.h
*
* Copyright (c) 2012, NVIDIA, All Rights Reserved.
*
* This file is licensed under the terms of the GNU General Public License
* version 2. This program is licensed "as is" without any warranty of any
* kind, whether express or implied.
*/

#ifndef __IMX091_H__
#define __IMX091_H__

#include <media/nvc.h>
#include <media/nvc_image.h>

/* See notes in the nvc.h file on the GPIO usage */
enum imx091_gpio {
	IMX091_GPIO_RESET = 0,
	IMX091_GPIO_PWDN,
	IMX091_GPIO_GP1,
};

/* The enumeration must be in the order the regulators are to be enabled */
/* See Power Requirements note in the driver */
enum imx091_vreg {
	IMX091_VREG_DVDD = 0,
	IMX091_VREG_AVDD,
	IMX091_VREG_IOVDD,
};

struct imx091_flash_config {
	u8 xvs_trigger_enabled;
	u8 sdo_trigger_enabled;
	u8 adjustable_flash_timing;
	u16 pulse_width_uS;
};

struct imx091_platform_data {
	unsigned cfg;
	unsigned num;
	unsigned sync;
	const char *dev_name;
	unsigned gpio_count; /* see nvc.h GPIO notes */
	struct nvc_gpio_pdata *gpio; /* see nvc.h GPIO notes */
	struct imx091_flash_config flash_cap;
	struct nvc_imager_cap *cap;
	unsigned lens_focal_length; /* / _INT2FLOAT_DIVISOR */
	unsigned lens_max_aperture; /* / _INT2FLOAT_DIVISOR */
	unsigned lens_fnumber; /* / _INT2FLOAT_DIVISOR */
	unsigned lens_view_angle_h; /* / _INT2FLOAT_DIVISOR */
	unsigned lens_view_angle_v; /* / _INT2FLOAT_DIVISOR */
	struct edp_client edpc_config;
	int (*probe_clock)(unsigned long);
	int (*power_on)(struct nvc_regulator *);
	int (*power_off)(struct nvc_regulator *);
};

#endif  /* __IMX091_H__ */
