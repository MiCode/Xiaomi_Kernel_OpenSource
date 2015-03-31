/*
 * Atmel maXTouch Touchscreen driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 * Copyright (C) 2015 XiaoMi, Inc.
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
	/* Points to the firmware name to be upgraded to */
	const char* mxt_cfg_name;
	int *key_codes;
	int key_num;
	u8 selfthr_suspend;
	u8 selfintthr_stylus;
	u8 selfintthr_suspend;
	u8 t71_tchthr_pos;
	u16 self_no_touch_threshold;
	u8 mult_no_touch_threshold;
	u8 self_chgtime_min;
	u8 self_chgtime_max;
	u8 mult_intthr_sensitive;
	u8 mult_intthr_not_sensitive;
	u8 atchthr_sensitive;
	u8 mult_tchthr_sensitive;
	u8 mult_tchthr_not_sensitive;
};

/* The platform data for the Atmel maXTouch touchscreen driver */
struct mxt_platform_data {
	struct mxt_config_info *config_array;
	const char* mxt_fw_name;
	size_t config_array_size;
	unsigned long irqflags;
	int power_gpio;
	int reset_gpio;
	int irq_gpio;
	u8(*read_chg) (void);
	const char *input_name;
	int unlock_move_threshold;
	int moving_threshold;
	int staying_threshold;
	int landing_threshold;
	int landing_edge_threshold;
	unsigned long landing_jiffies;
	int edge_clip;
	u32 reset_gpio_flags;
	u32 irq_gpio_flags;
	u32 power_gpio_flags;
	u8 gpio_mask;
	u8 *linearity_reg_pos;
	u8 *linearity_singlex;
	u8 *linearity_dualx;
	int linearity_para_num;
	u8 rx_num;
	u8 tx_num;
	u16 ref_diff_threshold;
	u16 ref_diff_halfline_threshold;
};

#endif /* __LINUX_ATMEL_MXT_TS_H */

