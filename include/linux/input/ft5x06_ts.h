/*
 *
 * FocalTech ft5x06 TouchScreen driver header file.
 *
 * Copyright (c) 2010  Focal tech Ltd.
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 * Copyright (C) 2015 XiaoMi, Inc.
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
#ifndef __LINUX_FT5X06_TS_H__
#define __LINUX_FT5X06_TS_H__

#include <linux/types.h>

/* platform data for Focaltech touchscreen */
struct ft5x06_firmware_data {
	u8	chip;
	u8	vendor;
	const char	*fwname;
	u8	*data;
	int	size;
};

struct ft5x06_rect { /* rectangle on the touch screen */
	u16 left , top;
	u16 width, height;
};

struct ft5x06_keypad_data {
	/* two cases could happen:
	   1.if length == 0, disable keypad functionality.
	   2.else convert touch in kparea to key event. */
	u8 chip;
	unsigned int              length; /* for keymap and button */
	unsigned int       *keymap; /* scancode==>keycode map */
	struct ft5x06_rect *button; /* define button location */
	int *key_pos;
};

struct ft5x06_test_data {
	int tx_num;
	int rx_num;
};

struct ft5x06_ts_platform_data {
	unsigned long irqflags;
	u32 x_max;
	u32 y_max;
	u32 z_max;
	u32 w_max;
	u32 irq_gpio;
	u32 reset_gpio;
	u32 irq_gpio_flags;
	u32 reset_gpio_flags;
	u32 cfg_size;
	struct ft5x06_firmware_data *firmware; /* terminated by 0 size */
	struct ft5x06_keypad_data *keypad;
	struct ft5x06_test_data *testdata;
	bool i2c_pull_up;

	unsigned long                landing_jiffies;
	int                          landing_threshold;
	int                          staying_threshold;
	int                          moving_threshold;
	u16                          raw_min;
	u16                          raw_max;
	/* optional callback for platform needs */
	int (*power_init)(bool);
	int (*power_on) (bool);
};

#endif
