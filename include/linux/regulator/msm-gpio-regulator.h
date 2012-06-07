/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MSM_GPIO_REGULATOR_H__
#define __MSM_GPIO_REGULATOR_H__

#include <linux/regulator/machine.h>

#define GPIO_REGULATOR_DEV_NAME "msm-gpio-regulator"

/**
 * struct gpio_regulator_platform_data - GPIO regulator platform data
 * @init_data:		regulator constraints
 * @gpio_label:		label to use when requesting the GPIO
 * @regulator_name:	name for regulator used during registration
 * @gpio:		gpio number
 * @active_low:		0 = regulator is enabled when GPIO outputs high
 *			1 = regulator is enabled when GPIO outputs low
 */
struct gpio_regulator_platform_data {
	struct regulator_init_data	init_data;
	char				*gpio_label;
	char				*regulator_name;
	unsigned			gpio;
	int				active_low;
};

#endif
