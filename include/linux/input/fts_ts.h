/*
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

#ifndef __LINUX_FTS_TS_H__
#define __LINUX_FTS_TS_H__

#include <linux/types.h>

/* platform data for fts touchscreen */
/* Now we haven't use this, but keep it here for future other vendor use */
struct fts_firmware_data {
	u8 vendor;
	const u8 *data;
	int size;
};

struct fts_config_info {
	const char *config_name;
	const char *firmware_name;
};

struct fts_ts_platform_data {
	int x_max;
	int x_min;
	int y_max;
	int y_min;
	int prox_z;
	int z_max;
	int z_min;
	u32 power_gpio;
	u32 irq_gpio;
	u32 reset_gpio;
	u32 lcd_gpio;
	int *key_codes;
	int key_num;
	unsigned long irqflags;
	const struct fts_config_info *config_info;
	bool is_mutual_key;
	unsigned int touch_info;
};

#endif
