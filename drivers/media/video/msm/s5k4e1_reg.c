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


#include "s5k4e1.h"

struct s5k4e1_i2c_reg_conf s5k4e1_mipi_settings[] = {
	{0x30BD, 0x00},/* SEL_CCP[0] */
	{0x3084, 0x15},/* SYNC Mode */
	{0x30BE, 0x1A},/* M_PCLKDIV_AUTO[4], M_DIV_PCLK[3:0] */
	{0x30C1, 0x01},/* pack video enable [0] */
	{0x30EE, 0x02},/* DPHY enable [ 1] */
	{0x3111, 0x86},/* Embedded data off [5] */
};

/* PLL Configuration */
struct s5k4e1_i2c_reg_conf s5k4e1_pll_preview_settings[] = {
	{0x0305, 0x04},
	{0x0306, 0x00},
	{0x0307, 0x44},
	{0x30B5, 0x00},
	{0x30E2, 0x01},/* num lanes[1:0] = 2 */
	{0x30F1, 0xB0},
};

struct s5k4e1_i2c_reg_conf s5k4e1_pll_snap_settings[] = {
	{0x0305, 0x04},
	{0x0306, 0x00},
	{0x0307, 0x44},
	{0x30B5, 0x00},
	{0x30E2, 0x01},/* num lanes[1:0] = 2 */
	{0x30F1, 0xB0},
};

struct s5k4e1_i2c_reg_conf s5k4e1_prev_settings[] = {
	/* output size (1304 x 980) */
	{0x30A9, 0x02},/* Horizontal Binning On */
	{0x300E, 0xEB},/* Vertical Binning On */
	{0x0387, 0x03},/* y_odd_inc 03(10b AVG) */
	{0x0344, 0x00},/* x_addr_start 0 */
	{0x0345, 0x00},
	{0x0348, 0x0A},/* x_addr_end 2607 */
	{0x0349, 0x2F},
	{0x0346, 0x00},/* y_addr_start 0 */
	{0x0347, 0x00},
	{0x034A, 0x07},/* y_addr_end 1959 */
	{0x034B, 0xA7},
	{0x0380, 0x00},/* x_even_inc 1 */
	{0x0381, 0x01},
	{0x0382, 0x00},/* x_odd_inc 1 */
	{0x0383, 0x01},
	{0x0384, 0x00},/* y_even_inc 1 */
	{0x0385, 0x01},
	{0x0386, 0x00},/* y_odd_inc 3 */
	{0x0387, 0x03},
	{0x034C, 0x05},/* x_output_size 1304 */
	{0x034D, 0x18},
	{0x034E, 0x03},/* y_output_size 980 */
	{0x034F, 0xd4},
	{0x30BF, 0xAB},/* outif_enable[7], data_type[5:0](2Bh = bayer 10bit} */
	{0x30C0, 0xA0},/* video_offset[7:4] 3260%12 */
	{0x30C8, 0x06},/* video_data_length 1600 = 1304 * 1.25 */
	{0x30C9, 0x5E},
	/* Timing Configuration */
	{0x0202, 0x03},
	{0x0203, 0x14},
	{0x0204, 0x00},
	{0x0205, 0x80},
	{0x0340, 0x03},/* Frame Length */
	{0x0341, 0xE0},
	{0x0342, 0x0A},/* 2738  Line Length */
	{0x0343, 0xB2},
};

struct s5k4e1_i2c_reg_conf s5k4e1_snap_settings[] = {
	/*Output Size (2608x1960)*/
	{0x30A9, 0x03},/* Horizontal Binning Off */
	{0x300E, 0xE8},/* Vertical Binning Off */
	{0x0387, 0x01},/* y_odd_inc */
	{0x034C, 0x0A},/* x_output size */
	{0x034D, 0x30},
	{0x034E, 0x07},/* y_output size */
	{0x034F, 0xA8},
	{0x30BF, 0xAB},/* outif_enable[7], data_type[5:0](2Bh = bayer 10bit} */
	{0x30C0, 0x80},/* video_offset[7:4] 3260%12 */
	{0x30C8, 0x0C},/* video_data_length 3260 = 2608 * 1.25 */
	{0x30C9, 0xBC},
	/*Timing configuration*/
	{0x0202, 0x06},
	{0x0203, 0x28},
	{0x0204, 0x00},
	{0x0205, 0x80},
	{0x0340, 0x07},/* Frame Length */
	{0x0341, 0xB4},
	{0x0342, 0x0A},/* 2738 Line Length */
	{0x0343, 0xB2},
};

struct s5k4e1_i2c_reg_conf s5k4e1_recommend_settings[] = {
	/*CDS timing setting ... */
	{0x3000, 0x05},
	{0x3001, 0x03},
	{0x3002, 0x08},
	{0x3003, 0x0A},
	{0x3004, 0x50},
	{0x3005, 0x0E},
	{0x3006, 0x5E},
	{0x3007, 0x00},
	{0x3008, 0x78},
	{0x3009, 0x78},
	{0x300A, 0x50},
	{0x300B, 0x08},
	{0x300C, 0x14},
	{0x300D, 0x00},
	{0x300E, 0xE8},
	{0x300F, 0x82},
	{0x301B, 0x77},

	/* CDS option setting ... */
	{0x3010, 0x00},
	{0x3011, 0x3A},
	{0x3029, 0x04},
	{0x3012, 0x30},
	{0x3013, 0xA0},
	{0x3014, 0x00},
	{0x3015, 0x00},
	{0x3016, 0x30},
	{0x3017, 0x94},
	{0x3018, 0x70},
	{0x301D, 0xD4},
	{0x3021, 0x02},
	{0x3022, 0x24},
	{0x3024, 0x40},
	{0x3027, 0x08},

	/* Pixel option setting ...   */
	{0x301C, 0x04},
	{0x30D8, 0x3F},
	{0x302B, 0x01},

	{0x3070, 0x5F},
	{0x3071, 0x00},
	{0x3080, 0x04},
	{0x3081, 0x38},
};

struct s5k4e1_reg s5k4e1_regs = {
	.reg_mipi = &s5k4e1_mipi_settings[0],
	.reg_mipi_size = ARRAY_SIZE(s5k4e1_mipi_settings),
	.rec_settings = &s5k4e1_recommend_settings[0],
	.rec_size = ARRAY_SIZE(s5k4e1_recommend_settings),
	.reg_pll_p = &s5k4e1_pll_preview_settings[0],
	.reg_pll_p_size = ARRAY_SIZE(s5k4e1_pll_preview_settings),
	.reg_pll_s = &s5k4e1_pll_snap_settings[0],
	.reg_pll_s_size = ARRAY_SIZE(s5k4e1_pll_snap_settings),
	.reg_prev = &s5k4e1_prev_settings[0],
	.reg_prev_size = ARRAY_SIZE(s5k4e1_prev_settings),
	.reg_snap = &s5k4e1_snap_settings[0],
	.reg_snap_size = ARRAY_SIZE(s5k4e1_snap_settings),
};
