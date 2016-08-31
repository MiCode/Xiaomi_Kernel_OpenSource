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

struct platform_max8831_backlight_data {
	int id;
	const char *name;
	unsigned int max_brightness;
	unsigned int dft_brightness;
	int (*notify)(struct device *dev, int brightness);
	bool (*is_powered)(void);
};

#endif
