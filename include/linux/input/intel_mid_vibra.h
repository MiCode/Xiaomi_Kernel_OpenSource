/*
 *  mid_vibra.h - Intel vibrator header
 *
 *  Copyright (C) 2013 Intel Corp
 *  Author: B, Jayachandran <jayachandran.b@intel.com>
 *  Author: Omair Md Abdullah <omair.m.abdullah@intel.com>
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#ifndef __INTEL_MID_VIBRA_H
#define __INTEL_MID_VIBRA_H

#define INTEL_VIBRA_DRV_NAME "intel_vibra_driver"

#define INTEL_VIBRA_MAX_TIMEDIVISOR  0xFF
#define INTEL_VIBRA_MAX_BASEUNIT 0x8000

#define INTEL_VIBRA_ENABLE_GPIO 40
#define INTEL_PWM_ENABLE_GPIO 49


struct mid_vibra_pdata {
	u8 time_divisor;
	u8 base_unit;
	u8 alt_fn;
	u8 ext_drv;
	int gpio_en;
	int gpio_pwm;
	const char *name;
	bool use_gpio_en; /* whether vibra needs gpio based enable control */
};

#endif /* __INTEL_MID_VIBRA_H */
