/*
 * Atmel maXTouch Touchscreen driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
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
#define CTP_PROC_INTERFACE                    1
#define WT_ADD_CTP_INFO                           1
#define KEYCODE_WAKEUP                    143
/*TP Color*/
#define TP_White      0x31
#define  TP_Black      0x32
#define  TP_Golden    0x38
#define FHD_KEY_Y                          2000
#define FHD_MENU_KEY_X              180
#define FHD_HOME_KEY_X              540
#define FHD_BACK_KEY_X              900
int fhd_key_dim_x[] = { 0, FHD_MENU_KEY_X, FHD_HOME_KEY_X, FHD_BACK_KEY_X, };

/*IC Maker*/
#define TP_BIEL        0x31

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

#define CTP_DEBUG_ON 1
#define CTP_DEBUG(fmt, arg...)	do {\
		if (CTP_DEBUG_ON)\
			printk("Atmel-308U:[%d]"fmt"\n", __LINE__, ##arg);\
		} while (0)

#define CTP_INFO(fmt, arg...)           printk("ATMEL-TP-TAG-INFO:"fmt"\n", ##arg)
#define CTP_ERROR(fmt, arg...)          printk("ATMEL-TP-TAG ERROR:"fmt"\n", ##arg)
#endif /* __LINUX_ATMEL_MXT_TS_H */

