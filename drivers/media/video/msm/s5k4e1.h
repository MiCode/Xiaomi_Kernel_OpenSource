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
 */

#ifndef S5K4E1_H
#define S5K4E1_H
#include <linux/types.h>
#include <mach/board.h>
extern struct s5k4e1_reg s5k4e1_regs;

struct s5k4e1_i2c_reg_conf {
	unsigned short waddr;
	unsigned short wdata;
};

enum s5k4e1_test_mode_t {
	TEST_OFF,
	TEST_1,
	TEST_2,
	TEST_3
};

enum s5k4e1_resolution_t {
	QTR_SIZE,
	FULL_SIZE,
	INVALID_SIZE
};
enum s5k4e1_setting {
	RES_PREVIEW,
	RES_CAPTURE
};
enum s5k4e1_reg_update {
	/* Sensor egisters that need to be updated during initialization */
	REG_INIT,
	/* Sensor egisters that needs periodic I2C writes */
	UPDATE_PERIODIC,
	/* All the sensor Registers will be updated */
	UPDATE_ALL,
	/* Not valid update */
	UPDATE_INVALID
};

enum s5k4e1_reg_pll {
	E013_VT_PIX_CLK_DIV,
	E013_VT_SYS_CLK_DIV,
	E013_PRE_PLL_CLK_DIV,
	E013_PLL_MULTIPLIER,
	E013_OP_PIX_CLK_DIV,
	E013_OP_SYS_CLK_DIV
};

enum s5k4e1_reg_mode {
	E013_X_ADDR_START,
	E013_X_ADDR_END,
	E013_Y_ADDR_START,
	E013_Y_ADDR_END,
	E013_X_OUTPUT_SIZE,
	E013_Y_OUTPUT_SIZE,
	E013_DATAPATH_SELECT,
	E013_READ_MODE,
	E013_ANALOG_CONTROL5,
	E013_DAC_LD_4_5,
	E013_SCALING_MODE,
	E013_SCALE_M,
	E013_LINE_LENGTH_PCK,
	E013_FRAME_LENGTH_LINES,
	E013_COARSE_INTEGRATION_TIME,
	E013_FINE_INTEGRATION_TIME,
	E013_FINE_CORRECTION
};

struct s5k4e1_reg {
	const struct s5k4e1_i2c_reg_conf *reg_mipi;
	const unsigned short reg_mipi_size;
	const struct s5k4e1_i2c_reg_conf *rec_settings;
	const unsigned short rec_size;
	const struct s5k4e1_i2c_reg_conf *reg_pll_p;
	const unsigned short reg_pll_p_size;
	const struct s5k4e1_i2c_reg_conf *reg_pll_s;
	const unsigned short reg_pll_s_size;
	const struct s5k4e1_i2c_reg_conf *reg_prev;
	const unsigned short reg_prev_size;
	const struct s5k4e1_i2c_reg_conf *reg_snap;
	const unsigned short reg_snap_size;
};
#endif /* S5K4E1_H */
