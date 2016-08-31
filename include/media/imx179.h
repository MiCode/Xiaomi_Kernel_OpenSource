/*
* imx179.h
*
* Copyright (c) 2013, NVIDIA CORPORATION, All rights reserved.
*
* This file is licensed under the terms of the GNU General Public License
* version 2. This program is licensed "as is" without any warranty of any
* kind, whether express or implied.
*/

#ifndef __IMX179_H__
#define __IMX179_H__

#include <media/nvc.h>
#include <media/nvc_image.h>

/* See notes in the nvc.h file on the GPIO usage */
enum imx179_gpio {
	IMX179_GPIO_RESET = 0,
	IMX179_GPIO_PWDN,
	IMX179_GPIO_GP1,
};

/* The enumeration must be in the order the regulators are to be enabled */
/* See Power Requirements note in the driver */
enum imx179_vreg {
	IMX179_VREG_DVDD = 0,
	IMX179_VREG_AVDD,
	IMX179_VREG_IOVDD,
};

struct imx179_flash_config {
	u8 xvs_trigger_enabled;
	u8 sdo_trigger_enabled;
	u8 adjustable_flash_timing;
	u16 pulse_width_uS;
};

struct imx179_platform_data {
	unsigned cfg;
	unsigned num;
	unsigned sync;
	const char *dev_name;
	unsigned gpio_count; /* see nvc.h GPIO notes */
	struct nvc_gpio_pdata *gpio; /* see nvc.h GPIO notes */
	struct imx179_flash_config flash_cap;
	struct nvc_imager_cap *cap;
	unsigned lens_focal_length; /* / _INT2FLOAT_DIVISOR */
	unsigned lens_max_aperture; /* / _INT2FLOAT_DIVISOR */
	unsigned lens_fnumber; /* / _INT2FLOAT_DIVISOR */
	unsigned lens_view_angle_h; /* / _INT2FLOAT_DIVISOR */
	unsigned lens_view_angle_v; /* / _INT2FLOAT_DIVISOR */
	const char *mclk_name; /* NULL for default default_mclk */
	int (*probe_clock)(unsigned long);
	int (*power_on)(struct nvc_regulator *);
	int (*power_off)(struct nvc_regulator *);
};

#endif  /* __IMX179_H__ */
