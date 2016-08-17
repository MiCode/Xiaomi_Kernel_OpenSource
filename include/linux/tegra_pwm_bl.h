/* Tegra PWM backlight data *
 *
 * Copyright (C) 2011 NVIDIA Corporation
 * Author: Renuka Apte <rapte@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef TEGRA_PWM_BL_H
#define TEGRA_PWM_BL_H

#include <linux/backlight.h>

struct platform_tegra_pwm_backlight_data {
	int which_dc;
	int which_pwm;
	int gpio_conf_to_sfio;
	unsigned int dft_brightness;
	unsigned int max_brightness;
	unsigned int period;
	unsigned int clk_div;
	unsigned int clk_select;
	int (*notify)(struct device *dev, int brightness);
	int (*check_fb)(struct device *dev, struct fb_info *info);
};

#endif /* TERGA_PWM_BL_H */
