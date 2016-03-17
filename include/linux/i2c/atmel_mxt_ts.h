/*
 * Atmel maXTouch Touchscreen driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Copyright (C) 2016 XiaoMi, Inc.
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __LINUX_ATMEL_MXT_TS_H
#define __LINUX_ATMEL_MXT_TS_H

#include <linux/types.h>

#define MXT224_I2C_ADDR1        0x4A
#define MXT224_I2C_ADDR2        0x4B
#define MXT1386_I2C_ADDR1       0x4C
#define MXT1386_I2C_ADDR2       0x4D
#define MXT1386_I2C_ADDR3       0x5A
#define MXT1386_I2C_ADDR4       0x5B

/* Orient */
#define MXT_NORMAL		0x0
#define MXT_DIAGONAL		0x1
#define MXT_HORIZONTAL_FLIP	0x2
#define MXT_ROTATED_90_COUNTER	0x3
#define MXT_VERTICAL_FLIP	0x4
#define MXT_ROTATED_90		0x5
#define MXT_ROTATED_180		0x6
#define MXT_DIAGONAL_COUNTER	0x7

#define CFG_NAME_SIZE		64

#define MXT_KEYARRAY_MAX_KEYS		32

struct mxt_config_info {
	u8 family_id;
	u8 variant_id;
	u8 version;
	u8 build;
	u8 bootldr_id;
	int lcd_id;
	u8 vendor_id;
	u8 panel_id;
	u8 rev_id;
	/* Points to the firmware name to be upgraded to */
	const char *mxt_cfg_name;
	int *key_codes;
	int key_num;
	u8 selfintthr_stylus;
	u8 t71_tchthr_pos;
	u8 self_chgtime_min;
	u8 self_chgtime_max;
	u8 mult_intthr_sensitive;
	u8 mult_intthr_not_sensitive;
	u8 atchthr_sensitive;
	u8 mult_tchthr_sensitive;
	u8 mult_tchthr_not_sensitive;
	u8 wake_up_self_adcx;
	bool wakeup_gesture_support;
};

/* The platform data for the Atmel maXTouch touchscreen driver */
struct mxt_platform_data {
	struct mxt_config_info *config_array;
	const char *mxt_fw_name;
	size_t config_array_size;
	unsigned long irqflags;
	int reset_gpio;
	int irq_gpio;
	u8(*read_chg) (void);
	const char *input_name;
#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXT_EDGE_SUPPORT
	const char *edge_input_name;
#endif
	u32 reset_gpio_flags;
	u32 irq_gpio_flags;
	u8 gpio_mask;
	int default_config;
	int default_panel_id;
	bool use_ptc_key;
	bool cut_off_power;
};

int mxt_register_glove_mode_notifier(struct notifier_block *nb);
int mxt_unregister_glove_mode_notifier(struct notifier_block *nb);

#endif /* __LINUX_ATMEL_MXT_TS_H */

