/*
 *
 * FocalTech ft5x46 TouchScreen driver header file.
 *
 * Copyright (c) 2010  Focal tech Ltd.
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 * Copyright (C) 2017 XiaoMi, Inc.
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

#include <linux/delay.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/debugfs.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/power_supply.h>
#include <linux/input/mt.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/hwinfo.h>
#ifdef CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#define FT5X46_LOCKDOWN_INFO_SIZE	8
#define FT5X46_CONFIG_INFO_SIZE	8
#define FT5X0X_MAX_FINGER		0x0A
struct ft5x46_bus_ops {
	u16 bustype;
	int (*recv)(struct device *dev, void *buf, int len);
	int (*send)(struct device *dev, const void *buf, int len);
	int (*read)(struct device *dev, u8 addr, void *buf, u8 len);
	int (*write)(struct device *dev, u8 addr, const void *buf, u8 len);
};

struct ft5x46_firmware_data {
	u8	chip;
	u8	vendor;
	u8	lcd_vendor;
	const char	*fwname;
	u8	*data;
	int	size;
};

struct ft5x46_rect {
	u16 left , top;
	u16 width, height;
};

struct ft5x46_tracker {
	int x, y;
	bool detect;
	bool moving;
	unsigned long jiffies;
};

struct ft5x46_ts_event {
	u16 x[FT5X0X_MAX_FINGER];
	u16 y[FT5X0X_MAX_FINGER];
	u8 touch_event[FT5X0X_MAX_FINGER];
	u8 finger_id[FT5X0X_MAX_FINGER];
	u16 pressure;
	u8 touch_point;
};


struct ft5x46_data {
	struct mutex mutex;
	struct device *dev;
	struct input_dev *input;
	struct kobject *vkeys_dir;
	struct kobj_attribute vkeys_attr;
	struct notifier_block power_supply_notifier;
	struct regulator *vdd;
	struct regulator *vddio;
	const struct ft5x46_bus_ops *bops;
	struct ft5x46_ts_event event;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	struct ft5x46_tracker tracker[FT5X0X_MAX_FINGER];
	int  irq;
	bool dbgdump;
	unsigned int test_result;
	bool irq_enabled;
	bool in_suspend;
	struct delayed_work noise_filter_delayed_work;
	u8 chip_id;
	u8 is_usb_plug_in;
	int current_index;
	u8 lockdown_info[FT5X46_LOCKDOWN_INFO_SIZE];
	u8 config_info[FT5X46_CONFIG_INFO_SIZE];
	bool wakeup_mode;
	bool cover_mode;

	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;

	int touchs;
	int keys;

#ifdef CONFIG_FB
	struct notifier_block fb_notif;
#endif
	bool force_charge_calib;
#ifdef CONFIG_TOUCH_DEBUG_FS
	struct dentry *debugfs;
#endif
};
struct ft5x46_keypad_data {
	u8 chip;
	unsigned int              length;
	unsigned int       *keymap;
	struct ft5x46_rect *button;
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
	u32 irq_gpio_flags;
	u32 reset_gpio_flags;
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
	u16 open_min;
	u16 open_max;
	u16 short_min;
	u32 short_max;
	u16 imin_cc;
	u16 imin_cg;
	bool has_key;
	u16 key_rx1;
	u16 key_rx2;
	u16 key_rx3;
	/* optional callback for platform needs */
	int (*power_init)(bool);
	int (*power_on) (bool);
	bool x_flip;
	bool y_flip;
};

struct ft5x46_data *ft5x46_probe(struct device *dev, const struct ft5x46_bus_ops *bops);
void ft5x46_remove(struct ft5x46_data *ft5x46);
int ft5x46_pm_suspend(struct device *dev);
int ft5x46_pm_resume(struct device *dev);

#endif
