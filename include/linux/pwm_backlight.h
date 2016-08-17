/* Generic PWM backlight driver data - see drivers/video/backlight/pwm_bl.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef __LINUX_PWM_BACKLIGHT_H
#define __LINUX_PWM_BACKLIGHT_H

#include <linux/backlight.h>

enum tegra_pwm_bl_edp_states {
	TEGRA_PWM_BL_EDP_NEG_3,
	TEGRA_PWM_BL_EDP_NEG_2,
	TEGRA_PWM_BL_EDP_NEG_1,
	TEGRA_PWM_BL_EDP_ZERO,
	TEGRA_PWM_BL_EDP_1,
	TEGRA_PWM_BL_EDP_2,
	TEGRA_PWM_BL_EDP_NUM_STATES,
};

struct platform_pwm_backlight_data {
	int pwm_id;
	unsigned int max_brightness;
	unsigned int dft_brightness;
	unsigned int lth_brightness;
	unsigned int pwm_period_ns;
	unsigned int pwm_gpio;
	unsigned int edp_states[TEGRA_PWM_BL_EDP_NUM_STATES];
	unsigned int edp_brightness[TEGRA_PWM_BL_EDP_NUM_STATES];
	int (*init)(struct device *dev);
	int (*notify)(struct device *dev, int brightness);
	void (*notify_after)(struct device *dev, int brightness);
	void (*exit)(struct device *dev);
	int (*check_fb)(struct device *dev, struct fb_info *info);
};

#endif
