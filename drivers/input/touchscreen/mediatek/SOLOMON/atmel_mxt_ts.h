
	 /*
	  * Copyright (C) 2016 MediaTek Inc.
	  *
	  * This program is free software; you can redistribute it and/or modify
	  * it under the terms of the GNU General Public License version 2 as
	  * published by the Free Software Foundation.
	  *
	  * This program is distributed in the hope that it will be useful,
	  * but WITHOUT ANY WARRANTY; without even the implied warranty of
	  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	  * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
	 */


#ifndef __LINUX_ATMEL_MXT_TS_LETV_H
#define __LINUX_ATMEL_MXT_TS_LETV_H

#include <linux/types.h>
#include <linux/input.h>

#define MAX_KEYS_SUPPORTED_IN_DRIVER 6
/* To store the config info in DTS file */
struct mxt_config_info{
	u8 type;
	u8 family_id;
	u8 variant_id;
	u8 version;
	u8 build;
	u8 config_year;
	u8 config_month;
	u8 config_date;
	u8 *config;
	int config_length;
};

/* The platform data for the Atmel maXTouch touchscreen driver */
struct mxt_platform_data {
	unsigned long irqflags;
	u8 t19_num_keys;
	const unsigned int *t19_keymap;
	const u8 *num_keys;  //len is NUM_KEY_TYPE
	const unsigned int (*keymap)[MAX_KEYS_SUPPORTED_IN_DRIVER];
	int t15_num_keys;
	const unsigned int *t15_keymap;
	unsigned long gpio_reset;
	unsigned long gpio_vdd;
	unsigned long int gpio_irq;
	const char *cfg_name;
	const char *fw_version;
	const char *input_name;
	struct mxt_config_info info;
};
enum {
	T15_T97_KEY = 0,
	T19_KEY,
	T24_KEY,
	T42_KEY,
	T61_KEY,
	T81_KEY,
	T92_KEY,
	T93_KEY,
	T99_KEY,
	T115_KEY,
	T116_KEY,
	NUM_KEY_TYPE
};
const static unsigned int mxt_keymap[] = {KEY_MENU, KEY_HOMEPAGE, KEY_BACK};
const static unsigned int mxts_keys[NUM_KEY_TYPE][MAX_KEYS_SUPPORTED_IN_DRIVER] = {
	//T15_T97_KEY
	{KEY_MENU , KEY_HOMEPAGE, KEY_BACK},
	//T19_KEY,
	{KEY_POWER},
	//T24_KEY,
	{KEY_F8},
	//T42_KEY,
	{KEY_F8},
	//T61_KEY,
	{KEY_F9},
	//T81_KEY,
	{KEY_F8,KEY_F9},
	//T92_KEY,
	{KEY_F8,KEY_F9,KEY_F10,KEY_F11},
	//T93_KEY,
	{KEY_F9},
	//T99_KEY,
	{KEY_F10},
	//T115_KEY,
	{KEY_F11},
	//T116_KEY,
	{KEY_F12}
};

const static u8 mxts_num_keys[NUM_KEY_TYPE] = {
	//T15_T97_KEY
	5,
	//T19_KEY,
	0,
	//T24_KEY,
	1,
	//T42_KEY,
	1,
	//T61_KEY,
	1,
	//T81_KEY,
	2,
	//T92_KEY,
	4,
	//T93_KEY,
	1,
	//T99_KEY,	
	1,
	//T115_KEY,
	1,
	//T116_KEY,
	1,
};

#endif /* __LINUX_ATMEL_MXT_TS_LETV_H */
