/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef BACKLIGHT_DEV_H_
#define BACKLIGHT_DEV_H_

#include <linux/backlight.h>

#define BRIGHTNESS_MIN_LEVEL 1
#define BRIGHTNESS_INIT_LEVEL 50
#define BRIGHTNESS_MAX_LEVEL 100

struct intel_adf_context;

int backlight_init(struct intel_adf_context *adf_ctx);
extern void backlight_exit(struct backlight_device *bl_dev);

#endif /* BACKLIGHT_DEV_H_ */
