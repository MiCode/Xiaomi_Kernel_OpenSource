/*
 *
 * FocalTech ft5x46 TouchScreen driver header file.
 *
 * Copyright (c) 2010  Focal tech Ltd.
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
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
#ifndef __LINUX_FT5X46_TS_H__
#define __LINUX_FT5X46_TS_H__

#include <linux/types.h>

struct ft5x46_data;

struct ft5x46_bus_ops {
	u16 bustype;
	int (*recv)(struct device *dev, void *buf, int len);
	int (*send)(struct device *dev, const void *buf, int len);
	int (*read)(struct device *dev, u8 addr, void *buf, u8 len);
	int (*write)(struct device *dev, u8 addr, const void *buf, u8 len);
};

/* platform data for Focaltech touchscreen */
struct ft5x46_firmware_data {
	u8	chip;
	u8	vendor;
	const char	*fwname;
	u8	*data;
	int	size;
};

struct ft5x46_rect { /* rectangle on the touch screen */
	u16 left , top;
	u16 width, height;
};

struct ft5x46_keypad_data {
	/* two cases could happen:
	   1.if length == 0, disable keypad functionality.
	   2.else convert touch in kparea to key event. */
	u8 chip;
	unsigned int              length; /* for keymap and button */
	unsigned int       *keymap; /* scancode==>keycode map */
	struct ft5x46_rect *button; /* define button location */
	int *key_pos;
};

struct ft5x46_test_data {
	int tx_num;
	int rx_num;
};

struct ft5x46_upgrade_info {
	u16	delay_aa;	/*delay of write FT_UPGRADE_AA*/
	u16	delay_55;	/*delay of write FT_UPGRADE_55*/
	u8	upgrade_id_1;	/*upgrade id 1*/
	u8	upgrade_id_2;	/*upgrade id 2*/
	u16	delay_readid;	/*delay of read id*/
};

struct ft5x46_ts_platform_data {
	unsigned long irqflags;
	u32 x_max;
	u32 y_max;
	u32 z_max;
	u32 w_max;
	u32 irq_gpio;
	u32 reset_gpio;
	u32 power_ldo_gpio;
	u32 power_ts_gpio;
	u32 irq_gpio_flags;
	u32 reset_gpio_flags;
	u32 power_ldo_gpio_flags;
	u32 power_ts_gpio_flags;
	u32 cfg_size;
	struct ft5x46_firmware_data *firmware; /* terminated by 0 size */
	struct ft5x46_keypad_data *keypad;
	struct ft5x46_test_data *testdata;
	struct ft5x46_upgrade_info ui;
	bool i2c_pull_up;

	unsigned long landing_jiffies;
	int landing_threshold;
	int staying_threshold;
	int moving_threshold;
	u16 raw_min;
	u16 raw_max;
	/* optional callback for platform needs */
	int (*power_init)(bool);
	int (*power_on) (bool);
};

struct ft5x46_data *ft5x46_probe(struct device *dev, const struct ft5x46_bus_ops *bops);
void ft5x46_remove(struct ft5x46_data *ft5x46);
int ft5x46_pm_suspend(struct device *dev);
int ft5x46_pm_resume(struct device *dev);

#endif
