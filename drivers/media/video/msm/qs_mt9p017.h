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

#ifndef QS_MT9P017_H
#define QS_MT9P017_H
#include <linux/types.h>
#include <linux/kernel.h>
extern struct qs_mt9p017_reg qs_mt9p017_regs;

struct qs_mt9p017_i2c_reg_conf {
	unsigned short waddr;
	unsigned short wdata;
};

struct qs_mt9p017_i2c_conf_array {
	struct qs_mt9p017_i2c_reg_conf *conf;
	unsigned short size;
};

struct qs_mt9p017_i2c_read_t {
	unsigned short raddr;
	unsigned short *rdata;
	int rlen;
};

struct qs_mt9p017_i2c_read_seq_t {
	unsigned short raddr;
	unsigned char *rdata;
	int rlen;
};

enum qs_mt9p017_test_mode_t {
	TEST_OFF,
	TEST_1,
	TEST_2,
	TEST_3
};

enum qs_mt9p017_resolution_t {
	QTR_2D_SIZE,
	FULL_2D_SIZE,
	QTR_3D_SIZE,
	FULL_3D_SIZE,
	INVALID_SIZE
};
enum qs_mt9p017_setting {
	RES_PREVIEW,
	RES_CAPTURE,
	RES_3D_PREVIEW,
	RES_3D_CAPTURE
};
enum qs_mt9p017_cam_mode_t {
	MODE_2D_RIGHT,
	MODE_2D_LEFT,
	MODE_3D,
	MODE_INVALID
};
enum qs_mt9p017_reg_update {
	/* Sensor egisters that need to be updated during initialization */
	REG_INIT,
	/* Sensor egisters that needs periodic I2C writes */
	UPDATE_PERIODIC,
	/* All the sensor Registers will be updated */
	UPDATE_ALL,
	/* Not valid update */
	UPDATE_INVALID
};

enum qs_mt9p017_reg_mode {
	QS_MT9P017_LINE_LENGTH_PCK = 7,
	QS_MT9P017_FRAME_LENGTH_LINES,
};

struct qs_mt9p017_reg {
	const struct qs_mt9p017_i2c_reg_conf *rec_settings;
	const unsigned short rec_size;
	const struct qs_mt9p017_i2c_reg_conf *reg_pll;
	const unsigned short reg_pll_size;
	const struct qs_mt9p017_i2c_reg_conf *reg_3d_pll;
	const unsigned short reg_3d_pll_size;
	const struct qs_mt9p017_i2c_reg_conf *reg_lens;
	const unsigned short reg_lens_size;
	const struct qs_mt9p017_i2c_conf_array *conf_array;
};
#endif /* QS_MT9P017_H */
