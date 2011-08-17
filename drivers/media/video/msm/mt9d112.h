/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef MT9D112_H
#define MT9D112_H

#include <linux/types.h>
#include <mach/camera.h>

extern struct mt9d112_reg mt9d112_regs;

enum mt9d112_width {
	WORD_LEN,
	BYTE_LEN
};

struct mt9d112_i2c_reg_conf {
	unsigned short waddr;
	unsigned short wdata;
	enum mt9d112_width width;
	unsigned short mdelay_time;
};

struct mt9d112_reg {
	const struct register_address_value_pair *prev_snap_reg_settings;
	uint16_t prev_snap_reg_settings_size;
	const struct register_address_value_pair *noise_reduction_reg_settings;
	uint16_t noise_reduction_reg_settings_size;
	const struct mt9d112_i2c_reg_conf *plltbl;
	uint16_t plltbl_size;
	const struct mt9d112_i2c_reg_conf *stbl;
	uint16_t stbl_size;
	const struct mt9d112_i2c_reg_conf *rftbl;
	uint16_t rftbl_size;
};

#endif /* MT9D112_H */
