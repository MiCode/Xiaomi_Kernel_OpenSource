/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __LINUX_MAX8831_BACKLIGHT_H
#define __LINUX_PWM8831_BACKLIGHT_H

#include <linux/backlight.h>

enum max8831_edp_states {
	MAX8831_EDP_NEG_8,
	MAX8831_EDP_NEG_7,
	MAX8831_EDP_NEG_6,
	MAX8831_EDP_NEG_5,
	MAX8831_EDP_NEG_4,
	MAX8831_EDP_NEG_3,
	MAX8831_EDP_NEG_2,
	MAX8831_EDP_NEG_1,
	MAX8831_EDP_ZERO,
	MAX8831_EDP_1,
	MAX8831_EDP_2,
	MAX8831_EDP_NUM_STATES,
};

#define MAX8831_EDP_BRIGHTNESS_UNIT	25

struct platform_max8831_backlight_data {
	int id;
	const char *name;
	unsigned int max_brightness;
	unsigned int dft_brightness;
	int (*notify)(struct device *dev, int brightness);
	bool (*is_powered)(void);
	unsigned int edp_states[MAX8831_EDP_NUM_STATES];
	unsigned int edp_brightness[MAX8831_EDP_NUM_STATES];
};

#endif
