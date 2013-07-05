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
 *
 */


#include "mt9e013.h"

static struct mt9e013_i2c_reg_conf mipi_settings[] = {
	/*Disable embedded data*/
	{0x3064, 0x7800},/*SMIA_TEST*/
	/*configure 2-lane MIPI*/
	{0x31AE, 0x0202},/*SERIAL_FORMAT*/
	{0x31B8, 0x0E3F},/*MIPI_TIMING_2*/
	/*set data to RAW10 format*/
	{0x0112, 0x0A0A},/*CCP_DATA_FORMAT*/
	{0x30F0, 0x8000},/*VCM CONTROL*/
};

/*PLL Configuration
(Ext=24MHz, vt_pix_clk=174MHz, op_pix_clk=69.6MHz)*/
static struct mt9e013_i2c_reg_conf pll_settings[] = {
	{0x0300, 0x0004},/*VT_PIX_CLK_DIV*/
	{0x0302, 0x0001},/*VT_SYS_CLK_DIV*/
	{0x0304, 0x0002},/*PRE_PLL_CLK_DIV*/
	{0x0306, 0x003A},/*PLL_MULTIPLIER*/
	{0x0308, 0x000A},/*OP_PIX_CLK_DIV*/
	{0x030A, 0x0001},/*OP_SYS_CLK_DIV*/
};

static struct mt9e013_i2c_reg_conf prev_settings[] = {
	/*Output Size (1632x1224)*/
	{0x0344, 0x0008},/*X_ADDR_START*/
	{0x0348, 0x0CC9},/*X_ADDR_END*/
	{0x0346, 0x0008},/*Y_ADDR_START*/
	{0x034A, 0x0999},/*Y_ADDR_END*/
	{0x034C, 0x0660},/*X_OUTPUT_SIZE*/
	{0x034E, 0x04C8},/*Y_OUTPUT_SIZE*/
	{0x306E, 0xFCB0},/*DATAPATH_SELECT*/
	{0x3040, 0x04C3},/*READ_MODE*/
	{0x3178, 0x0000},/*ANALOG_CONTROL5*/
	{0x3ED0, 0x1E24},/*DAC_LD_4_5*/
	{0x0400, 0x0002},/*SCALING_MODE*/
	{0x0404, 0x0010},/*SCALE_M*/
	/*Timing configuration*/
	{0x0342, 0x1018},/*LINE_LENGTH_PCK*/
	{0x0340, 0x055B},/*FRAME_LENGTH_LINES*/
	{0x0202, 0x0557},/*COARSE_INTEGRATION_TIME*/
	{0x3014, 0x0846},/*FINE_INTEGRATION_TIME_*/
	{0x3010, 0x0130},/*FINE_CORRECTION*/
};

static struct mt9e013_i2c_reg_conf snap_settings[] = {
	/*Output Size (3264x2448)*/
	{0x0344, 0x0008},/*X_ADDR_START */
	{0x0348, 0x0CD7},/*X_ADDR_END*/
	{0x0346, 0x0008},/*Y_ADDR_START */
	{0x034A, 0x09A7},/*Y_ADDR_END*/
	{0x034C, 0x0CD0},/*X_OUTPUT_SIZE*/
	{0x034E, 0x09A0},/*Y_OUTPUT_SIZE*/
	{0x306E, 0xFC80},/*DATAPATH_SELECT*/
	{0x3040, 0x0041},/*READ_MODE*/
	{0x3178, 0x0000},/*ANALOG_CONTROL5*/
	{0x3ED0, 0x1E24},/*DAC_LD_4_5*/
	{0x0400, 0x0000},/*SCALING_MODE*/
	{0x0404, 0x0010},/*SCALE_M*/
	/*Timing configuration*/
	{0x0342, 0x13F8},/*LINE_LENGTH_PCK*/
	{0x0340, 0x0A2F},/*FRAME_LENGTH_LINES*/
	{0x0202, 0x0A1F},/*COARSE_INTEGRATION_TIME*/
	{0x3014, 0x03F6},/*FINE_INTEGRATION_TIME_ */
	{0x3010, 0x0078},/*FINE_CORRECTION*/
};

static struct mt9e013_i2c_reg_conf pll_settings_60fps[] = {
	{0x0300, 0x0004},/*VT_PIX_CLK_DIV*/
	{0x0302, 0x0001},/*VT_SYS_CLK_DIV*/
	{0x0304, 0x0002},/*PRE_PLL_CLK_DIV*/
	{0x0306, 0x0042},/*PLL_MULTIPLIER*/
	{0x0308, 0x000A},/*OP_PIX_CLK_DIV*/
	{0x030A, 0x0001},/*OP_SYS_CLK_DIV*/
};

static struct mt9e013_i2c_reg_conf prev_settings_60fps[] = {
	/*Output Size (1632x1224)*/
	{0x0344, 0x0008},/*X_ADDR_START*/
	{0x0348, 0x0CC5},/*X_ADDR_END*/
	{0x0346, 0x013a},/*Y_ADDR_START*/
	{0x034A, 0x0863},/*Y_ADDR_END*/
	{0x034C, 0x0660},/*X_OUTPUT_SIZE*/
	{0x034E, 0x0396},/*Y_OUTPUT_SIZE*/
	{0x306E, 0xFC80},/*DATAPATH_SELECT*/
	{0x3040, 0x00C3},/*READ_MODE*/
	{0x3178, 0x0000},/*ANALOG_CONTROL5*/
	{0x3ED0, 0x1E24},/*DAC_LD_4_5*/
	{0x0400, 0x0000},/*SCALING_MODE*/
	{0x0404, 0x0010},/*SCALE_M*/
	/*Timing configuration*/
	{0x0342, 0x0BE8},/*LINE_LENGTH_PCK*/
	{0x0340, 0x0425},/*FRAME_LENGTH_LINES*/
	{0x0202, 0x0425},/*COARSE_INTEGRATION_TIME*/
	{0x3014, 0x03F6},/*FINE_INTEGRATION_TIME_*/
	{0x3010, 0x0078},/*FINE_CORRECTION*/
};

static struct mt9e013_i2c_reg_conf pll_settings_120fps[] = {
	{0x0300, 0x0005},/*VT_PIX_CLK_DIV*/
	{0x0302, 0x0001},/*VT_SYS_CLK_DIV*/
	{0x0304, 0x0002},/*PRE_PLL_CLK_DIV*/
	{0x0306, 0x0052},/*PLL_MULTIPLIER*/
	{0x0308, 0x000A},/*OP_PIX_CLK_DIV*/
	{0x030A, 0x0001},/*OP_SYS_CLK_DIV*/
};

static struct mt9e013_i2c_reg_conf prev_settings_120fps[] = {
	{0x0344, 0x0008},/*X_ADDR_START*/
	{0x0348, 0x0685},/*X_ADDR_END*/
	{0x0346, 0x013a},/*Y_ADDR_START*/
	{0x034A, 0x055B},/*Y_ADDR_END*/
	{0x034C, 0x0340},/*X_OUTPUT_SIZE*/
	{0x034E, 0x0212},/*Y_OUTPUT_SIZE*/
	{0x306E, 0xFC80},/*DATAPATH_SELECT*/
	{0x3040, 0x00C3},/*READ_MODE*/
	{0x3178, 0x0000},/*ANALOG_CONTROL5*/
	{0x3ED0, 0x1E24},/*DAC_LD_4_5*/
	{0x0400, 0x0000},/*SCALING_MODE*/
	{0x0404, 0x0010},/*SCALE_M*/
	/*Timing configuration*/
	{0x0342, 0x0970},/*LINE_LENGTH_PCK*/
	{0x0340, 0x02A1},/*FRAME_LENGTH_LINES*/
	{0x0202, 0x02A1},/*COARSE_INTEGRATION_TIME*/
	{0x3014, 0x03F6},/*FINE_INTEGRATION_TIME_*/
	{0x3010, 0x0078},/*FINE_CORRECTION*/
};

static struct mt9e013_i2c_reg_conf recommend_settings[] = {
	{0x3044, 0x0590},
	{0x306E, 0xFC80},
	{0x30B2, 0xC000},
	{0x30D6, 0x0800},
	{0x316C, 0xB42F},
	{0x316E, 0x869C},
	{0x3170, 0x210E},
	{0x317A, 0x010E},
	{0x31E0, 0x1FB9},
	{0x31E6, 0x07FC},
	{0x37C0, 0x0000},
	{0x37C2, 0x0000},
	{0x37C4, 0x0000},
	{0x37C6, 0x0000},
	{0x3E02, 0x8801},
	{0x3E04, 0x2301},
	{0x3E06, 0x8449},
	{0x3E08, 0x6841},
	{0x3E0A, 0x400C},
	{0x3E0C, 0x1001},
	{0x3E0E, 0x2103},
	{0x3E10, 0x4B41},
	{0x3E12, 0x4B26},
	{0x3E16, 0x8802},
	{0x3E18, 0x84FF},
	{0x3E1A, 0x8601},
	{0x3E1C, 0x8401},
	{0x3E1E, 0x840A},
	{0x3E20, 0xFF00},
	{0x3E22, 0x8401},
	{0x3E24, 0x00FF},
	{0x3E26, 0x0088},
	{0x3E28, 0x2E8A},
	{0x3E32, 0x8801},
	{0x3E34, 0x4024},
	{0x3E38, 0x8469},
	{0x3E3C, 0x2301},
	{0x3E3E, 0x3E25},
	{0x3E40, 0x1C01},
	{0x3E42, 0x8486},
	{0x3E44, 0x8401},
	{0x3E46, 0x00FF},
	{0x3E48, 0x8401},
	{0x3E4A, 0x8601},
	{0x3E4C, 0x8402},
	{0x3E4E, 0x00FF},
	{0x3E50, 0x6623},
	{0x3E52, 0x8340},
	{0x3E54, 0x00FF},
	{0x3E56, 0x4A42},
	{0x3E58, 0x2203},
	{0x3E5A, 0x674D},
	{0x3E5C, 0x3F25},
	{0x3E5E, 0x846A},
	{0x3E60, 0x4C01},
	{0x3E62, 0x8401},
	{0x3E66, 0x3901},
	{0x3ECC, 0x00EB},
	{0x3ED0, 0x1E24},
	{0x3ED4, 0xAFC4},
	{0x3ED6, 0x909B},
	{0x3ED8, 0x0006},
	{0x3EDA, 0xCFC6},
	{0x3EDC, 0x4FE4},
	{0x3EE0, 0x2424},
	{0x3EE2, 0x9797},
	{0x3EE4, 0xC100},
	{0x3EE6, 0x0540}
};

struct mt9e013_reg mt9e013_regs = {
	.reg_mipi = &mipi_settings[0],
	.reg_mipi_size = ARRAY_SIZE(mipi_settings),
	.rec_settings = &recommend_settings[0],
	.rec_size = ARRAY_SIZE(recommend_settings),
	.reg_pll = &pll_settings[0],
	.reg_pll_size = ARRAY_SIZE(pll_settings),
	.reg_prev = &prev_settings[0],
	.reg_pll_60fps = &pll_settings_60fps[0],
	.reg_pll_60fps_size = ARRAY_SIZE(pll_settings_60fps),
	.reg_pll_120fps = &pll_settings_120fps[0],
	.reg_pll_120fps_size = ARRAY_SIZE(pll_settings_120fps),
	.reg_prev_size = ARRAY_SIZE(prev_settings),
	.reg_snap = &snap_settings[0],
	.reg_snap_size = ARRAY_SIZE(snap_settings),
	.reg_60fps = &prev_settings_60fps[0],
	.reg_60fps_size = ARRAY_SIZE(prev_settings_60fps),
	.reg_120fps = &prev_settings_120fps[0],
	.reg_120fps_size = ARRAY_SIZE(prev_settings_120fps),
};
