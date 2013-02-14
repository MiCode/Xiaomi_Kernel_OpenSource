/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef OV9726_H
#define OV9726_H
#include <linux/types.h>
#include <mach/board.h>

/* 16bit address - 8 bit context register structure */
struct reg_struct_type {
	uint16_t	reg_addr;
	unsigned char	reg_val;
};

enum ov9726_test_mode_t {
	TEST_OFF,
	TEST_1,
	TEST_2,
	TEST_3
};

enum ov9726_resolution_t {
	QTR_SIZE,
	FULL_SIZE,
	INVALID_SIZE
};
extern struct reg_struct_type ov9726_init_settings_array[];
extern int32_t ov9726_array_length;
#endif

