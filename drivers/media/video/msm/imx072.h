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

#ifndef IMX072_H
#define IMX072_H
#include <linux/types.h>
#include <mach/board.h>
extern struct imx072_reg imx072_regs;

struct imx072_i2c_reg_conf {
	unsigned short waddr;
	unsigned short wdata;
};

struct imx072_i2c_conf_array {
	struct imx072_i2c_reg_conf *conf;
	unsigned short size;
};

enum imx072_test_mode_t {
	TEST_OFF,
	TEST_1,
	TEST_2,
	TEST_3
};

enum imx072_resolution_t {
	QTR_2D_SIZE,
	FULL_2D_SIZE,
	QTR_3D_SIZE,
	FULL_3D_SIZE,
	INVALID_SIZE
};
enum imx072_setting {
	RES_PREVIEW,
	RES_CAPTURE,
	RES_3D_PREVIEW,
	RES_3D_CAPTURE
};
enum imx072_cam_mode_t {
	MODE_2D_RIGHT,
	MODE_2D_LEFT,
	MODE_3D,
	MODE_INVALID
};
enum imx072_reg_update {
	/* Sensor egisters that need to be updated during initialization */
	REG_INIT,
	/* Sensor egisters that needs periodic I2C writes */
	UPDATE_PERIODIC,
	/* All the sensor Registers will be updated */
	UPDATE_ALL,
	/* Not valid update */
	UPDATE_INVALID
};

enum imx072_reg_mode {
	IMX072_FRAME_LENGTH_LINES_HI = 0,
	IMX072_FRAME_LENGTH_LINES_LO,
	IMX072_LINE_LENGTH_PCK_HI,
	IMX072_LINE_LENGTH_PCK_LO,
};

struct imx072_reg {
	const struct imx072_i2c_reg_conf *rec_settings;
	const unsigned short rec_size;
	const struct imx072_i2c_conf_array *conf_array;
};
#endif /* IMX072_H */
