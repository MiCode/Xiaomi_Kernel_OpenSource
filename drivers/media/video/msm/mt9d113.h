/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#ifndef MT9D113_H
#define MT9D113_H

#include <linux/types.h>
#include <mach/camera.h>

extern struct mt9d113_reg mt9d113_regs;

enum mt9d113_width {
	WORD_LEN,
	BYTE_LEN
};

struct mt9d113_i2c_reg_conf {
	unsigned short waddr;
	unsigned short wdata;
};

struct mt9d113_reg {
	const struct mt9d113_i2c_reg_conf *pll_tbl;
	uint16_t pll_tbl_size;
	const struct mt9d113_i2c_reg_conf *register_tbl;
	uint16_t register_tbl_size;
	const struct mt9d113_i2c_reg_conf *err_tbl;
	uint16_t err_tbl_size;
	const struct mt9d113_i2c_reg_conf *low_light_tbl;
	uint16_t low_light_tbl_size;
	const struct mt9d113_i2c_reg_conf *awb_tbl;
	uint16_t awb_tbl_size;
	const struct mt9d113_i2c_reg_conf *patch_tbl;
	uint16_t patch_tbl_size;
	const struct mt9d113_i2c_reg_conf *eeprom_tbl ;
	uint16_t eeprom_tbl_size ;
};

enum mt9d113_test_mode_t {
	TEST_OFF,
	TEST_1,
	TEST_2,
	TEST_3
};

enum mt9d113_resolution_t {
	QTR_SIZE,
	FULL_SIZE,
	INVALID_SIZE
};

enum mt9d113_setting {
	RES_PREVIEW,
	RES_CAPTURE
};
#endif /* MT9D113_H */
