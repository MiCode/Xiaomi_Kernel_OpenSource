/*
 *
 * FocalTech ft5435 TouchScreen driver header file.
 *
 * Copyright (c) 2010  Focal tech Ltd.
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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
#ifndef __LINUX_FT5435_TS_H__
#define __LINUX_FT5435_TS_H__

#define FT5435_ID		0x55
#define FT5X16_ID		0x0A
#define FT5X36_ID		0x14
#define FT6X06_ID		0x06
#define FT6X36_ID       0x36

#define SET_COVER_MODE
#define FOCALTECH_AUTO_UPGRADE		1
#define FOCALTECH_LOCK_DOWN_INFO	1
#define FOCALTECH_TP_GESTURE
#define FOCALTECH_FAE_MOD

#define USB_CHARGE_DETECT
#define FOCALTECH_ITO_TEST			1
#define FOCALTECH_MAX_VKEY_NUM 3


struct fw_upgrade_info {
	bool auto_cal;
	u16 delay_aa;
	u16 delay_55;
	u8 upgrade_id_1;
	u8 upgrade_id_2;
	u16 delay_readid;
	u16 delay_erase_flash;
};
struct virkey{
	int keycode;
	int x;
	int y;
};
struct ft5435_ts_platform_data {
	struct fw_upgrade_info info;
	const char *name;
	const char *fw_name;
	u32 irqflags;
	u32 irq_gpio;
	u32 irq_gpio_flags;
	u32 reset_gpio;
	u32 reset_gpio_flags;
	u32 family_id;
	u32 x_max;
	u32 y_max;
	u32 x_min;
	u32 y_min;
	u32 panel_minx;
	u32 panel_miny;
	u32 panel_maxx;
	u32 panel_maxy;
	u32 group_id;
	u32 hard_rst_dly;
	u32 soft_rst_dly;
	u32 num_max_touches;
	bool fw_vkey_support;
	bool no_force_update;
	bool i2c_pull_up;
	bool ignore_id_check;
	bool resume_in_workqueue;
	int (*power_init) (bool);
	int (*power_on) (bool);
	int num_virkey;
	struct virkey vkeys[FOCALTECH_MAX_VKEY_NUM];
};
struct ft5435_rawdata_test_result {
	int result;
	int min_limited_value;
	int max_limited_value;
	int min_value;
	int max_value;
	int index[350][3];
};
#endif
