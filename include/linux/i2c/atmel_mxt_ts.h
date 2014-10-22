/*
 * Atmel maXTouch Touchscreen driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 * Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __LINUX_ATMEL_MXT_TS_H
#define __LINUX_ATMEL_MXT_TS_H

#include <linux/types.h>

/* For key_map array */
#define MXT_NUM_GPIO		4

/* Orient */
#define MXT_NORMAL		0x0
#define MXT_DIAGONAL		0x1
#define MXT_HORIZONTAL_FLIP	0x2
#define MXT_ROTATED_90_COUNTER	0x3
#define MXT_VERTICAL_FLIP	0x4
#define MXT_ROTATED_90		0x5
#define MXT_ROTATED_180		0x6
#define MXT_DIAGONAL_COUNTER	0x7

/* MXT_TOUCH_KEYARRAY_T15 */
#define MXT_KEYARRAY_MAX_KEYS	32

/* Bootoader IDs */
#define MXT_BOOTLOADER_ID_224		0x0A
#define MXT_BOOTLOADER_ID_224E		0x06
#define MXT_BOOTLOADER_ID_336S		0x1A
#define MXT_BOOTLOADER_ID_1386		0x01
#define MXT_BOOTLOADER_ID_1386E		0x10
#define MXT_BOOTLOADER_ID_1664S		0x14

#define PINCTRL_STATE_ACTIVE   "pmx_ts_active"
#define PINCTRL_STATE_SUSPEND  "pmx_ts_suspend"
#define PINCTRL_STATE_RELEASE  "pmx_ts_release"

/* Config data for a given maXTouch controller with a specific firmware */
struct mxt_config_info {
	const u8 *config;
	int config_length;
	u8 family_id;
	u8 variant_id;
	u8 version;
	u8 build;
	u8 bootldr_id;
	/* Points to the firmware name to be upgraded to */
	const char *fw_name;
};

/* The platform data for the Atmel maXTouch touchscreen driver */
struct mxt_platform_data {
	const struct mxt_config_info *config_array;
	size_t config_array_size;

	/* touch panel's minimum and maximum coordinates */
	u32 panel_minx;
	u32 panel_maxx;
	u32 panel_miny;
	u32 panel_maxy;

	/* display's minimum and maximum coordinates */
	u32 disp_minx;
	u32 disp_maxx;
	u32 disp_miny;
	u32 disp_maxy;

	unsigned long irqflags;
	bool is_tp;
	const unsigned int key_map[MXT_NUM_GPIO];
	bool	i2c_pull_up;
	bool	digital_pwr_regulator;
	int reset_gpio;
	u32 reset_gpio_flags;
	int irq_gpio;
	u32 irq_gpio_flags;
	int i2cmode_gpio;
	int *key_codes;
	bool need_calibration;
	bool no_force_update;
	bool no_lpm_support;
	u8 bl_addr;

	u8(*read_chg) (void);
	int (*init_hw) (bool);
	int (*power_on) (bool);
};

#endif /* __LINUX_ATMEL_MXT_TS_H */
