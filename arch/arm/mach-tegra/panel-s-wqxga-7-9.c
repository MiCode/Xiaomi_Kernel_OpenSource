/*
 * arch/arm/mach-tegra/panel-s-wqxga-7-9.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http:
 */

#include <mach/dc.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/tegra_pwm_bl.h>
#include <linux/regulator/consumer.h>
#include <linux/pwm_backlight.h>
#include <linux/max8831_backlight.h>
#include <linux/platform_data/lp855x.h>
#include <linux/leds.h>
#include <linux/ioport.h>
#include <generated/mach-types.h>
#include "board.h"
#include "board-panel.h"
#include "devices.h"
#include "gpio-names.h"
#include "tegra11_host1x_devices.h"

#define TEGRA_DSI_GANGED_MODE	1

#define DSI_PANEL_RESET		1

#define DC_CTRL_MODE	TEGRA_DC_OUT_CONTINUOUS_MODE

static bool reg_requested;
static bool gpio_requested;
static struct platform_device *disp_device;
static struct regulator *avdd_lcd_vsp_5v5;
static struct regulator *avdd_lcd_vsn_5v5;
static struct regulator *vdd_lcd_bl;
static struct regulator *vdd_lcd_bl_en;
static struct regulator *dvdd_lcd_1v8;

static tegra_dc_bl_output dsi_s_wqxga_7_9_bl_output_measured = {
	0, 1, 2, 3, 4, 5, 6, 7,
	8, 9, 11, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 21, 22,
	23, 24, 25, 26, 27, 28, 29, 30,
	31, 32, 32, 33, 34, 35, 36, 37,
	38, 39, 40, 41, 42, 43, 43, 44,
	45, 46, 47, 48, 49, 50, 51, 52,
	53, 54, 54, 55, 56, 57, 58, 59,
	60, 61, 62, 63, 63, 64, 65, 66,
	67, 68, 69, 70, 71, 72, 73, 74,
	75, 76, 77, 78, 79, 80, 80, 81,
	82, 83, 84, 85, 86, 87, 88, 89,
	90, 91, 92, 93, 94, 95, 96, 97,
	98, 99, 100, 101, 102, 103, 104, 105,
	106, 107, 108, 109, 110, 111, 112, 113,
	114, 115, 116, 117, 118, 119, 120, 121,
	122, 123, 124, 125, 126, 127, 128, 129,
	130, 131, 132, 133, 134, 135, 136, 137,
	138, 140, 141, 142, 143, 144, 145, 146,
	147, 148, 149, 150, 151, 152, 153, 154,
	155, 156, 157, 158, 159, 160, 161, 162,
	163, 164, 165, 166, 167, 168, 169, 170,
	171, 172, 173, 174, 175, 177, 178, 179,
	180, 181, 182, 183, 184, 185, 186, 187,
	188, 189, 190, 191, 192, 193, 194, 195,
	196, 197, 198, 200, 201, 202, 203, 204,
	205, 206, 207, 208, 209, 210, 211, 212,
	213, 214, 215, 217, 218, 219, 220, 221,
	222, 223, 224, 225, 226, 227, 228, 229,
	230, 231, 232, 234, 235, 236, 237, 238,
	239, 240, 241, 242, 243, 244, 245, 246,
	248, 249, 250, 251, 252, 253, 254, 255,
};

static u8 __maybe_unused p0_00[] = {0xf0, 0x55, 0xaa, 0x52, 0x08, 0x00};
static u8 __maybe_unused p0_01[] = {0xb1, 0xE8, 0x11};
static u8 __maybe_unused p0_02[] = {0xb5, 0x08, 0x00};
static u8 __maybe_unused p0_03[] = {0xb8, 0x03, 0x00, 0x00, 0x00};
static u8 __maybe_unused p0_04[] = {0xbc, 0x0F, 0x12};
static u8 __maybe_unused p0_05[] = {0xbd, 0x10, 0xC8, 0x08, 0x0C};
static u8 __maybe_unused p0_06[] = {0xc0, 0x07, 0x02, 0x03, 0x07, 0x08, 0x09, 0x19, 0x1A, 0x1B, 0x00};
static u8 __maybe_unused p0_07[] = {0xd9, 0x01, 0x01, 0x90};

static u8 __maybe_unused p1_00[] = {0xf0,  0x55, 0xAA, 0x52, 0x08, 0x01};
static u8 __maybe_unused p1_01[] = {0xb2,  0x02, 0x02};
static u8 __maybe_unused p1_02[] = {0xb3,  0x28, 0x28};
static u8 __maybe_unused p1_03[] = {0xb4,  0x28, 0x28};
static u8 __maybe_unused p1_04[] = {0xb7,  0x01      };
static u8 __maybe_unused p1_05[] = {0xb8,  0x04, 0x04};
static u8 __maybe_unused p1_051[] = {0xd5,  0x07};
static u8 __maybe_unused p1_06[] = {0xb9,  0x34, 0x34};
static u8 __maybe_unused p1_07[] = {0xba,  0x34, 0x34};
static u8 __maybe_unused p1_08[] = {0xbc,  0x78, 0x00};
static u8 __maybe_unused p1_09[] = {0xbd,  0x78, 0x00};
static u8 __maybe_unused p1_10[] = {0xbe, 0x00, 0x92};
static u8 __maybe_unused p1_11[] = {0xbf, 0x00, 0x92};
static u8 __maybe_unused p1_12[] = {0xc3, 0x6E, 0x6E};
static u8 __maybe_unused p1_13[] = {0xc4, 0x6E, 0x6E};
static u8 __maybe_unused p1_14[] = {0xca, 0x03      };
static u8 __maybe_unused p1_15[] = {0xce, 0x04      };
static u8 __maybe_unused p1_16[] = {0xd8, 0xC0, 0x0E};

static u8 __maybe_unused p3_00[] = {0xf0, 0x55, 0xAA, 0x52, 0x08, 0x03};
static u8 __maybe_unused p3_01[] = {0xb0, 0x00, 0x00, 0x00, 0x00};
static u8 __maybe_unused p3_02[] = {0xb1, 0x00, 0x00, 0x00, 0x00};
static u8 __maybe_unused p3_03[] = {0xb2, 0x00, 0x00, 0x08, 0x04, 0x00, 0x98, 0x4B};
static u8 __maybe_unused p3_04[] = {0xb6, 0xF0, 0x08, 0x05, 0x04, 0x00, 0x00, 0x00, 0x00, 0x98, 0x4B};
static u8 __maybe_unused p3_05[] = {0xba, 0x85, 0x03, 0x00, 0x06, 0x00, 0x98, 0x4B};
static u8 __maybe_unused p3_06[] = {0xbb, 0x85, 0x03, 0x00, 0x02, 0x00, 0x98, 0x4B};
static u8 __maybe_unused p3_07[] = {0xc4, 0x00, 0x00};

static u8 __maybe_unused p4_00[] = {0xf0, 0x55, 0xAA, 0x52, 0x08, 0x04};
static u8 __maybe_unused p4_01[] = {0xea, 0x00};

static u8 __maybe_unused p5_00[] = {0xf0, 0x55, 0xAA, 0x52, 0x08, 0x05};
static u8 __maybe_unused p5_01[] = {0xb0, 0x11, 0x03, 0x00, 0x01};
static u8 __maybe_unused p5_02[] = {0xb1, 0x30, 0x00};
static u8 __maybe_unused p5_03[] = {0xb2, 0x03, 0x02, 0x22};
static u8 __maybe_unused p5_04[] = {0xb3, 0x83, 0x23, 0x10, 0x64};
static u8 __maybe_unused p5_05[] = {0xb4, 0xC5, 0x75, 0x07, 0x57};
static u8 __maybe_unused p5_06[] = {0xb5, 0x00, 0xC4, 0x71, 0x07, 0x00, 0xAB, 0x0A};
static u8 __maybe_unused p5_07[] = {0xb6, 0x00, 0x00, 0xD5, 0x71, 0x07, 0x57};
static u8 __maybe_unused p5_08[] = {0xb9, 0x00, 0x00, 0x00, 0x05, 0x00};
static u8 __maybe_unused p5_09[] = {0xc0, 0x75, 0x07, 0x00, 0x54, 0x05};
static u8 __maybe_unused p5_10[] = {0xc6, 0x00, 0x00, 0x00, 0x00};
static u8 __maybe_unused p5_11[] = {0xd0, 0x00, 0x08, 0x02, 0x00, 0x10};
static u8 __maybe_unused p5_12[] = {0xd1, 0x00, 0x08, 0x06, 0x80, 0x20};
static u8 __maybe_unused p5_13[] = {0xe7, 0x08, 0xA2};
static u8 __maybe_unused p5_14[] = {0xe8, 0x02, 0xFF};
static u8 __maybe_unused p5_15[] = {0xe9, 0x00};
static u8 __maybe_unused p5_16[] = {0xea, 0x57};
static u8 __maybe_unused p5_17[] = {0xeb, 0xAB};
static u8 __maybe_unused p5_18[] = {0xec, 0xA3};
static u8 __maybe_unused p5_19[] = {0xed, 0xA3};
static u8 __maybe_unused p5_20[] = {0xee, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3};
static u8 __maybe_unused p5_21[] = {0xef, 0xAA};

static u8 __maybe_unused p6_00[] = {0xf0, 0x55, 0xAA, 0x52, 0x08, 0x06};
static u8 __maybe_unused p6_01[] = {0xb0, 0x7D, 0x49, 0x7D, 0x41, 0x7D};
static u8 __maybe_unused p6_02[] = {0xb1, 0x51, 0x57, 0x7D, 0x7D, 0x59};
static u8 __maybe_unused p6_03[] = {0xb2, 0x53, 0x7D, 0x7D, 0x7A, 0x7D};
static u8 __maybe_unused p6_04[] = {0xb3, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D};
static u8 __maybe_unused p6_05[] = {0xb4, 0x7D, 0x7D};
static u8 __maybe_unused p6_06[] = {0xb5, 0x48, 0x7D, 0x40, 0x7D, 0x7D};
static u8 __maybe_unused p6_07[] = {0xb6, 0x7D, 0x7D, 0x50, 0x56, 0x7D};
static u8 __maybe_unused p6_08[] = {0xb7, 0x7D, 0x58, 0x52, 0x7A, 0x7D};
static u8 __maybe_unused p6_09[] = {0xb8, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D};
static u8 __maybe_unused p6_10[] = {0xb9, 0x7D, 0x7D};
static u8 __maybe_unused p6_11[] = {0xc0, 0x3D, 0x08, 0x3D, 0x00, 0x3D};
static u8 __maybe_unused p6_12[] = {0xc1, 0x18, 0x12, 0x3D, 0x3D, 0x10};
static u8 __maybe_unused p6_13[] = {0xc2, 0x02, 0x3D, 0x3D, 0x3A, 0x3D};
static u8 __maybe_unused p6_14[] = {0xc3, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D};
static u8 __maybe_unused p6_15[] = {0xc4, 0x3D, 0x3D};
static u8 __maybe_unused p6_16[] = {0xc5, 0x09, 0x3D, 0x01, 0x3D, 0x3D};
static u8 __maybe_unused p6_17[] = {0xc6, 0x3D, 0x3D, 0x19, 0x13, 0x3D};
static u8 __maybe_unused p6_18[] = {0xc7, 0x3D, 0x11, 0x17, 0x3A, 0x3D};
static u8 __maybe_unused p6_19[] = {0xc8, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D};
static u8 __maybe_unused p6_20[] = {0xc9, 0x3D, 0x3D};

static u8 __maybe_unused cmd3_00[] = {0xff, 0xaa, 0x55, 0xa5, 0x80};
static u8 __maybe_unused cmd3_01[] = {0x6F, 0x1F};
static u8 __maybe_unused cmd3_02[] = {0xF4, 0x02};
static u8 __maybe_unused cmd3_03[] = {0x6F, 0x18};
static u8 __maybe_unused cmd3_04[] = {0xF2, 0x80};
static u8 __maybe_unused cmd3_05[] = {0xF9, 0x08};
static u8 __maybe_unused cmd3_06[] = {0x6F, 0x0C};
static u8 __maybe_unused cmd3_07[] = {0xF9, 0x04};
static u8 __maybe_unused cmd3_08[] = {0x6F, 0x09};
static u8 __maybe_unused cmd3_09[] = {0xF9, 0x03};
static u8 __maybe_unused cmd3_10[] = {0x6F, 0x05};
static u8 __maybe_unused cmd3_11[] = {0xF9, 0x04};
static u8 __maybe_unused cmd3_12[] = {0x6F, 0x06};
static u8 __maybe_unused cmd3_13[] = {0xF9, 0x86};
static u8 __maybe_unused cmd3_14[] = {0x6F, 0x01};
static u8 __maybe_unused cmd3_15[] = {0xFA, 0x0F};
static u8 __maybe_unused cmd3_16[] = {0x6F, 0x0B};
static u8 __maybe_unused cmd3_17[] = {0xFA, 0x07};
static u8 __maybe_unused cmd3_18[] = {0x6F, 0x09};
static u8 __maybe_unused cmd3_19[] = {0xFA, 0x40};
static u8 __maybe_unused cmd3_20[] = {0x6F, 0x18};
static u8 __maybe_unused cmd3_21[] = {0xFA, 0x00};
static u8 __maybe_unused cmd3_22[] = {0x6F, 0x19};
static u8 __maybe_unused cmd3_23[] = {0xFA, 0x08};
static u8 __maybe_unused cmd3_24[] = {0x6F, 0x0D};
static u8 __maybe_unused cmd3_25[] = {0xFA, 0x03};
static u8 __maybe_unused cmd3_26[] = {0x6F, 0x21};
static u8 __maybe_unused cmd3_27[] = {0xF4, 0x09};
static u8 __maybe_unused cmd3_28[] = {0x6F, 0x23};
static u8 __maybe_unused cmd3_29[] = {0xF4, 0x13};
static u8 __maybe_unused cmd3_30[] = {0x6F, 0x05};
static u8 __maybe_unused cmd3_31[] = {0xFC, 0x0E};
static u8 __maybe_unused cmd3_32[] = {0x6F, 0x04};
static u8 __maybe_unused cmd3_33[] = {0xFC, 0x0A};
static u8 __maybe_unused cmd3_34[] = {0x6F, 0x06};
static u8 __maybe_unused cmd3_35[] = {0xFC, 0x0E};
static u8 __maybe_unused cmd3_36[] = {0x6F, 0x12};
static u8 __maybe_unused cmd3_37[] = {0xF4, 0x11};
static u8 __maybe_unused cmd3_38[] = {0x6F, 0x1F};
static u8 __maybe_unused cmd3_39[] = {0xF4, 0x02};
static u8 __maybe_unused cmd3_40[] = {0x6F, 0x08};
static u8 __maybe_unused cmd3_41[] = {0xFC, 0x10};
static u8 __maybe_unused cmd3_42[] = {0x6F, 0x18};
static u8 __maybe_unused cmd3_43[] = {0xF2, 0x80};
static u8 __maybe_unused cmd3_44[] = {0x6F, 0x01};
static u8 __maybe_unused cmd3_45[] = {0xF7, 0xCC};
static u8 __maybe_unused cmd3_46[] = {0x6F, 0x02};
static u8 __maybe_unused cmd3_47[] = {0xF7, 0x3F};
static u8 __maybe_unused cmd3_48[] = {0x6F, 0x12};
static u8 __maybe_unused cmd3_49[] = {0xFA, 0x80};
static u8 __maybe_unused cmd3_50[] = {0x6F, 0x1C};
static u8 __maybe_unused cmd3_51[] = {0xFA, 0x80};
static u8 __maybe_unused cmd3_52[] = {0x6F, 0x06};
static u8 __maybe_unused cmd3_53[] = {0xF9, 0x06};

static u8 __maybe_unused p2_00[] = {0xf0, 0x55, 0xAA, 0x52, 0x08, 0x02};
static u8 __maybe_unused p2_01[] = {0xD1, 0x00, 0x65, 0x00, 0x74, 0x00, 0x90, 0x00, 0xA8, 0x00, 0xB9, 0x00, 0xDB, 0x00, 0xF7, 0x01, 0x26};
static u8 __maybe_unused p2_02[] = {0xD2, 0x01, 0x4B, 0x01, 0x85, 0x01, 0xAF, 0x01, 0xF7, 0x02, 0x2E, 0x02, 0x2F, 0x02, 0x63, 0x02, 0x96};
static u8 __maybe_unused p2_03[] = {0xD3, 0x02, 0xB6, 0x02, 0xE3, 0x03, 0x03, 0x03, 0x2C, 0x03, 0x47, 0x03, 0x65, 0x03, 0x79, 0x03, 0x8E};
static u8 __maybe_unused p2_04[] = {0xD4, 0x03, 0xAD, 0x03, 0xB6};
static u8 __maybe_unused p2_05[] = {0xD5, 0x00, 0x65, 0x00, 0x74, 0x00, 0x90, 0x00, 0xA8, 0x00, 0xB9, 0x00, 0xDB, 0x00, 0xF7, 0x01, 0x26};
static u8 __maybe_unused p2_06[] = {0xD6, 0x01, 0x4B, 0x01, 0x85, 0x01, 0xAF, 0x01, 0xF7, 0x02, 0x2E, 0x02, 0x2F, 0x02, 0x63, 0x02, 0x96};
static u8 __maybe_unused p2_07[] = {0xD7, 0x02, 0xB6, 0x02, 0xE3, 0x03, 0x03, 0x03, 0x2C, 0x03, 0x47, 0x03, 0x65, 0x03, 0x79, 0x03, 0x8E};
static u8 __maybe_unused p2_08[] = {0xD8, 0x03, 0xAD, 0x03, 0xB6};
static u8 __maybe_unused p2_09[] = {0xD9, 0x00, 0x65, 0x00, 0x04, 0x00, 0x90, 0x00, 0xA8, 0x00, 0xB9, 0x00, 0xDB, 0x00, 0xF7, 0x01, 0x26};
static u8 __maybe_unused p2_10[] = {0xDD, 0x01, 0x4B, 0x01, 0x85, 0x01, 0xAF, 0x01, 0xF7, 0x02, 0x2E, 0x02, 0x2F, 0x02, 0x63, 0x02, 0x96};
static u8 __maybe_unused p2_11[] = {0xDE, 0x02, 0xB6, 0x02, 0xE3, 0x03, 0x03, 0x03, 0x2C, 0x03, 0x47, 0x03, 0x65, 0x03, 0x79, 0x03, 0x8E};
static u8 __maybe_unused p2_12[] = {0xDF, 0x03, 0xAD, 0x03, 0xB6};
static u8 __maybe_unused p2_13[] = {0xE0, 0x00, 0x4F, 0x00, 0x5E, 0x00, 0x78, 0x00, 0x90, 0x00, 0xA1, 0x00, 0xC5, 0x00, 0xE1, 0x01, 0x12};
static u8 __maybe_unused p2_14[] = {0xE1, 0x01, 0x37, 0x01, 0x75, 0x01, 0xA7, 0x01, 0xF3, 0x02, 0x2E, 0x02, 0x2F, 0x02, 0x65, 0x02, 0x9C};
static u8 __maybe_unused p2_15[] = {0xE2, 0x02, 0xC4, 0x02, 0xF3, 0x03, 0x15, 0x03, 0x42, 0x03, 0x5D, 0x03, 0x7D, 0x03, 0x91, 0x03, 0xA6};
static u8 __maybe_unused p2_16[] = {0xE3, 0x03, 0xC3, 0x03, 0xCC};
static u8 __maybe_unused p2_17[] = {0xE4, 0x00, 0x4F, 0x00, 0x5E, 0x00, 0x78, 0x00, 0x90, 0x00, 0xA1, 0x00, 0xC5, 0x00, 0xE1, 0x01, 0x12};
static u8 __maybe_unused p2_18[] = {0xE5, 0x01, 0x37, 0x01, 0x75, 0x01, 0xA7, 0x01, 0xF3, 0x02, 0x2E, 0x02, 0x2F, 0x02, 0x65, 0x02, 0x9C};
static u8 __maybe_unused p2_19[] = {0xE6, 0x02, 0xC4, 0x02, 0xF3, 0x03, 0x15, 0x03, 0x42, 0x03, 0x5D, 0x03, 0x7D, 0x03, 0x91, 0x03, 0xA6};
static u8 __maybe_unused p2_20[] = {0xE7, 0x03, 0xC3, 0x03, 0xCC};
static u8 __maybe_unused p2_21[] = {0xE8, 0x00, 0x4F, 0x00, 0x5E, 0x00, 0x78, 0x00, 0x90, 0x00, 0xA1, 0x00, 0xC5, 0x00, 0xE1, 0x01, 0x12};
static u8 __maybe_unused p2_22[] = {0xE9, 0x01, 0x37, 0x01, 0x75, 0x01, 0xA7, 0x01, 0xF3, 0x02, 0x2E, 0x02, 0x2F, 0x02, 0x65, 0x02, 0x9C};
static u8 __maybe_unused p2_23[] = {0xEA, 0x02, 0xC4, 0x02, 0xF3, 0x03, 0x15, 0x03, 0x42, 0x03, 0x5D, 0x03, 0x7D, 0x03, 0x91, 0x03, 0xA6};
static u8 __maybe_unused p2_24[] = {0xEB, 0x03, 0xC3, 0x03, 0xCC};

static struct tegra_dsi_cmd dsi_s_wqxga_7_9_init_cmd[] = {
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p0_00),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p0_01),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p0_02),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p0_03),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p0_04),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p0_05),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p0_06),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p0_07),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p1_00),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p1_01),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p1_02),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p1_03),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p1_04),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p1_05),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p1_051),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p1_06),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p1_07),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p1_08),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p1_09),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p1_10),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p1_11),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p1_12),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p1_13),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p1_14),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p1_15),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p1_16),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p3_00),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p3_01),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p3_02),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p3_03),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p3_04),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p3_05),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p3_06),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p3_07),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p4_00),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p4_01),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_00),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_01),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_02),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_03),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_04),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_05),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_06),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_07),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_08),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_09),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_10),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_11),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_12),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_13),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_14),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_15),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_16),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_17),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_18),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_19),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_20),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p5_21),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_00),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_01),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_02),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_03),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_04),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_05),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_06),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_07),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_08),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_09),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_10),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_11),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_12),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_13),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_14),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_15),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_16),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_17),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_18),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_19),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p6_20),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_00),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_01),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_02),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_03),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_04),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_05),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_06),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_07),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_08),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_09),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_10),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_11),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_12),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_13),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_14),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_15),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_16),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_17),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_18),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_19),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_20),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_21),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_22),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_23),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_24),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_25),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_26),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_27),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_28),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_29),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_30),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_31),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_32),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_33),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_34),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_35),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_36),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_37),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_38),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_39),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_40),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_41),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_42),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_43),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_44),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_45),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_46),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_47),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_48),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_49),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_50),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_51),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_52),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, cmd3_53),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_00),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_01),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_02),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_03),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_04),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_05),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_06),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_07),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_08),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_09),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_10),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_11),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_12),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_13),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_14),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_15),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_16),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_17),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_18),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_19),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_20),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_21),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_22),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_23),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, p2_24),
	DSI_CMD_SHORT_BOTH(DSI_DCS_WRITE_0_PARAM, DSI_DCS_EXIT_SLEEP_MODE, 0x0),
	DSI_DLY_MS(130),
	DSI_SEND_FRAME(6),

	DSI_CMD_SHORT_BOTH(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_ON, 0x0),
	DSI_DLY_MS(120),
};

static struct tegra_dsi_cmd dsi_s_wqxga_7_9_suspend_cmd[] = {
	DSI_CMD_SHORT_BOTH(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_OFF, DSI_DCS_NO_OP),
	DSI_DLY_MS(50),
	DSI_CMD_SHORT_BOTH(DSI_DCS_WRITE_0_PARAM, DSI_DCS_ENTER_SLEEP_MODE, DSI_DCS_NO_OP),
	DSI_DLY_MS(120),
};

static struct tegra_dsi_out dsi_s_wqxga_7_9_pdata = {
	.controller_vs = DSI_VS_1,

	.n_data_lanes = 8,
	.ganged_type = TEGRA_DSI_GANGED_SYMMETRIC_LEFT_RIGHT,
	.video_burst_mode = TEGRA_DSI_VIDEO_NONE_BURST_MODE,
	.refresh_rate = 60,

	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.panel_reset = DSI_PANEL_RESET,
	.power_saving_suspend = true,

	.video_clock_mode = TEGRA_DSI_VIDEO_CLOCK_TX_ONLY,
	.video_clock_mode = TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS,
	.dsi_init_cmd = dsi_s_wqxga_7_9_init_cmd,
	.n_init_cmd = ARRAY_SIZE(dsi_s_wqxga_7_9_init_cmd),
	.dsi_suspend_cmd = dsi_s_wqxga_7_9_suspend_cmd,
	.n_suspend_cmd = ARRAY_SIZE(dsi_s_wqxga_7_9_suspend_cmd),
	.bl_name = "pwm-backlight",
	.lp00_pre_panel_wakeup = true,
	.ulpm_not_supported = true,
	.no_pkt_seq_hbp = false,
};

static int ardbeg_dsi_regulator_get(struct device *dev)
{
	int err = 0;

	if (reg_requested)
		return 0;
	dvdd_lcd_1v8 = regulator_get(dev, "dvdd_lcdio");
	if (IS_ERR_OR_NULL(dvdd_lcd_1v8)) {
		err = PTR_ERR(dvdd_lcd_1v8);
		dvdd_lcd_1v8 = NULL;
		goto fail;
	}
	avdd_lcd_vsp_5v5 = regulator_get(dev, "avdd_lcd");
	if (IS_ERR_OR_NULL(avdd_lcd_vsp_5v5)) {
		pr_err("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd_vsp_5v5);
		avdd_lcd_vsp_5v5 = NULL;
		goto fail;
	}
	avdd_lcd_vsn_5v5 = regulator_get(dev, "bvdd_lcd");
	if (IS_ERR_OR_NULL(avdd_lcd_vsn_5v5)) {
		pr_err("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd_vsn_5v5);
		avdd_lcd_vsn_5v5 = NULL;
		goto fail;
	}

	vdd_lcd_bl = regulator_get(dev, "vdd_lcd_bl");
	if (IS_ERR_OR_NULL(vdd_lcd_bl)) {
		pr_err("vdd_lcd_bl regulator get failed\n");
		err = PTR_ERR(vdd_lcd_bl);
		vdd_lcd_bl = NULL;
		goto fail;
	}

	vdd_lcd_bl_en = regulator_get(dev, "vdd_lcd_bl_en");
	if (IS_ERR_OR_NULL(vdd_lcd_bl_en)) {
		pr_err("vdd_lcd_bl_en regulator get failed\n");
		err = PTR_ERR(vdd_lcd_bl_en);
		vdd_lcd_bl_en = NULL;
		goto fail;
	}
	reg_requested = true;
	return 0;
fail:
	return err;
}

static int ardbeg_dsi_gpio_get(void)
{
	int err = 0;

	if (gpio_requested)
		return 0;

	err = gpio_request(dsi_s_wqxga_7_9_pdata.dsi_panel_rst_gpio,
		"panel rst");
	if (err < 0) {
		pr_err("panel reset gpio request failed\n");
		goto fail;
	}

	/* free pwm GPIO */
	err = gpio_request(dsi_s_wqxga_7_9_pdata.dsi_panel_bl_pwm_gpio,
		"panel pwm");
	if (err < 0) {
		pr_err("panel pwm gpio request failed\n");
		goto fail;
	}
	gpio_free(dsi_s_wqxga_7_9_pdata.dsi_panel_bl_pwm_gpio);
	gpio_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_s_wqxga_7_9_postpoweron(struct device *dev)
{
	int err = 0;
	struct board_info board_info;

	tegra_get_board_info(&board_info);
	err = ardbeg_dsi_regulator_get(dev);
	if (err < 0) {
		pr_err("dsi regulator get failed\n");
		goto fail;
	}

	err = ardbeg_dsi_gpio_get();
	if (err < 0) {
		pr_err("dsi gpio request failed\n");
		goto fail;
	}

	if (dvdd_lcd_1v8) {
		err = regulator_enable(dvdd_lcd_1v8);
		if (err < 0) {
			pr_err("dvdd_lcd regulator enable failed\n");
			goto fail;
		}
	}
	msleep(200);

	if (avdd_lcd_vsp_5v5) {
		err = regulator_enable(avdd_lcd_vsp_5v5);
		if (err < 0) {
			pr_err("avdd_lcd regulator enable failed\n");
			goto fail;
		}
	}
	msleep(200);
	if (avdd_lcd_vsn_5v5) {
		err = regulator_enable(avdd_lcd_vsn_5v5);
		if (err < 0) {
			pr_err("bvdd_lcd regulator enable failed\n");
			goto fail;
		}
	}

	msleep(260);

	if (vdd_lcd_bl) {
		err = regulator_enable(vdd_lcd_bl);
		if (err < 0) {
			pr_err("vdd_lcd_bl regulator enable failed\n");
			goto fail;
		}
	}

	if (vdd_lcd_bl_en) {
		err = regulator_enable(vdd_lcd_bl_en);
		if (err < 0) {
			pr_err("vdd_lcd_bl_en regulator enable failed\n");
			goto fail;
		}
	}

	msleep(20);
#if DSI_PANEL_RESET
	gpio_direction_output(dsi_s_wqxga_7_9_pdata.dsi_panel_rst_gpio, 1);
	usleep_range(1000, 5000);
	msleep(10);
	gpio_set_value(dsi_s_wqxga_7_9_pdata.dsi_panel_rst_gpio, 0);
	usleep_range(1000, 5000);
	msleep(10);
	gpio_set_value(dsi_s_wqxga_7_9_pdata.dsi_panel_rst_gpio, 1);
	msleep(30);
#endif

	return 0;
fail:
	return err;
}

static int dsi_s_wqxga_7_9_enable(struct device *dev)
{
	return 0;
}

static int dsi_s_wqxga_7_9_disable(void)
{
	gpio_set_value(dsi_s_wqxga_7_9_pdata.dsi_panel_rst_gpio, 0);
	if (vdd_lcd_bl)
		regulator_disable(vdd_lcd_bl);

	if (vdd_lcd_bl_en)
		regulator_disable(vdd_lcd_bl_en);

	if (avdd_lcd_vsn_5v5)
		regulator_disable(avdd_lcd_vsn_5v5);
	msleep(200);
	if (avdd_lcd_vsp_5v5)
		regulator_disable(avdd_lcd_vsp_5v5);
	msleep(200);

	if (dvdd_lcd_1v8)
		regulator_disable(dvdd_lcd_1v8);
	msleep(200);

	return 0;
}

static int dsi_s_wqxga_7_9_postsuspend(void)
{
	gpio_set_value(dsi_s_wqxga_7_9_pdata.dsi_panel_rst_gpio, 0);
	return 0;
}

static struct tegra_dc_mode dsi_s_wqxga_7_9_modes[] = {
	{
		.pclk = 214824960,
		.h_ref_to_sync = 11,
		.v_ref_to_sync = 1,
		.h_sync_width = 28,
		.v_sync_width = 2,
		.h_back_porch = 76,
		.v_back_porch = 8,
		.h_active = 1536,
		.v_active = 2048,
		.h_front_porch = 88,
		.v_front_porch = 14,
	},
};

#ifdef CONFIG_TEGRA_DC_CMU
static struct tegra_dc_cmu dsi_s_wqxga_7_9_cmu = {
	/* lut1 maps sRGB to linear space. */
	{
	0,    1,    2,    4,    5,    6,    7,    9,
	10,   11,   12,   14,   15,   16,   18,   20,
	21,   23,   25,   27,   29,   31,   33,   35,
	37,   40,   42,   45,   48,   50,   53,   56,
	59,   62,   66,   69,   72,   76,   79,   83,
	87,   91,   95,   99,   103,  107,  112,  116,
	121,  126,  131,  136,  141,  146,  151,  156,
	162,  168,  173,  179,  185,  191,  197,  204,
	210,  216,  223,  230,  237,  244,  251,  258,
	265,  273,  280,  288,  296,  304,  312,  320,
	329,  337,  346,  354,  363,  372,  381,  390,
	400,  409,  419,  428,  438,  448,  458,  469,
	479,  490,  500,  511,  522,  533,  544,  555,
	567,  578,  590,  602,  614,  626,  639,  651,
	664,  676,  689,  702,  715,  728,  742,  755,
	769,  783,  797,  811,  825,  840,  854,  869,
	884,  899,  914,  929,  945,  960,  976,  992,
	1008, 1024, 1041, 1057, 1074, 1091, 1108, 1125,
	1142, 1159, 1177, 1195, 1213, 1231, 1249, 1267,
	1286, 1304, 1323, 1342, 1361, 1381, 1400, 1420,
	1440, 1459, 1480, 1500, 1520, 1541, 1562, 1582,
	1603, 1625, 1646, 1668, 1689, 1711, 1733, 1755,
	1778, 1800, 1823, 1846, 1869, 1892, 1916, 1939,
	1963, 1987, 2011, 2035, 2059, 2084, 2109, 2133,
	2159, 2184, 2209, 2235, 2260, 2286, 2312, 2339,
	2365, 2392, 2419, 2446, 2473, 2500, 2527, 2555,
	2583, 2611, 2639, 2668, 2696, 2725, 2754, 2783,
	2812, 2841, 2871, 2901, 2931, 2961, 2991, 3022,
	3052, 3083, 3114, 3146, 3177, 3209, 3240, 3272,
	3304, 3337, 3369, 3402, 3435, 3468, 3501, 3535,
	3568, 3602, 3636, 3670, 3705, 3739, 3774, 3809,
	3844, 3879, 3915, 3950, 3986, 4022, 4059, 4095,
	},
	/* csc */
	{
		0x105, 0x3D5, 0x024, /* 1.021 -0.164 0.143 */
		0x3EA, 0x121, 0x3C1, /* -0.082 1.128 -0.245 */
		0x002, 0x00A, 0x0F4, /* 0.007 0.038 0.955 */
	},
	/* lut2 maps linear space to sRGB */
	{
		0,    1,    2,    2,    3,    4,    5,    6,
		6,    7,    8,    9,    10,   10,   11,   12,
		13,   13,   14,   15,   15,   16,   16,   17,
		18,   18,   19,   19,   20,   20,   21,   21,
		22,   22,   23,   23,   23,   24,   24,   25,
		25,   25,   26,   26,   27,   27,   27,   28,
		28,   29,   29,   29,   30,   30,   30,   31,
		31,   31,   32,   32,   32,   33,   33,   33,
		34,   34,   34,   34,   35,   35,   35,   36,
		36,   36,   37,   37,   37,   37,   38,   38,
		38,   38,   39,   39,   39,   40,   40,   40,
		40,   41,   41,   41,   41,   42,   42,   42,
		42,   43,   43,   43,   43,   43,   44,   44,
		44,   44,   45,   45,   45,   45,   46,   46,
		46,   46,   46,   47,   47,   47,   47,   48,
		48,   48,   48,   48,   49,   49,   49,   49,
		49,   50,   50,   50,   50,   50,   51,   51,
		51,   51,   51,   52,   52,   52,   52,   52,
		53,   53,   53,   53,   53,   54,   54,   54,
		54,   54,   55,   55,   55,   55,   55,   55,
		56,   56,   56,   56,   56,   57,   57,   57,
		57,   57,   57,   58,   58,   58,   58,   58,
		58,   59,   59,   59,   59,   59,   59,   60,
		60,   60,   60,   60,   60,   61,   61,   61,
		61,   61,   61,   62,   62,   62,   62,   62,
		62,   63,   63,   63,   63,   63,   63,   64,
		64,   64,   64,   64,   64,   64,   65,   65,
		65,   65,   65,   65,   66,   66,   66,   66,
		66,   66,   66,   67,   67,   67,   67,   67,
		67,   67,   68,   68,   68,   68,   68,   68,
		68,   69,   69,   69,   69,   69,   69,   69,
		70,   70,   70,   70,   70,   70,   70,   71,
		71,   71,   71,   71,   71,   71,   72,   72,
		72,   72,   72,   72,   72,   72,   73,   73,
		73,   73,   73,   73,   73,   74,   74,   74,
		74,   74,   74,   74,   74,   75,   75,   75,
		75,   75,   75,   75,   75,   76,   76,   76,
		76,   76,   76,   76,   77,   77,   77,   77,
		77,   77,   77,   77,   78,   78,   78,   78,
		78,   78,   78,   78,   78,   79,   79,   79,
		79,   79,   79,   79,   79,   80,   80,   80,
		80,   80,   80,   80,   80,   81,   81,   81,
		81,   81,   81,   81,   81,   81,   82,   82,
		82,   82,   82,   82,   82,   82,   83,   83,
		83,   83,   83,   83,   83,   83,   83,   84,
		84,   84,   84,   84,   84,   84,   84,   84,
		85,   85,   85,   85,   85,   85,   85,   85,
		85,   86,   86,   86,   86,   86,   86,   86,
		86,   86,   87,   87,   87,   87,   87,   87,
		87,   87,   87,   88,   88,   88,   88,   88,
		88,   88,   88,   88,   88,   89,   89,   89,
		89,   89,   89,   89,   89,   89,   90,   90,
		90,   90,   90,   90,   90,   90,   90,   90,
		91,   91,   91,   91,   91,   91,   91,   91,
		91,   91,   92,   92,   92,   92,   92,   92,
		92,   92,   92,   92,   93,   93,   93,   93,
		93,   93,   93,   93,   93,   93,   94,   94,
		94,   94,   94,   94,   94,   94,   94,   94,
		95,   95,   95,   95,   95,   95,   95,   95,
		95,   95,   96,   96,   96,   96,   96,   96,
		96,   96,   96,   96,   96,   97,   97,   97,
		97,   97,   97,   97,   97,   97,   97,   98,
		98,   98,   98,   98,   98,   98,   98,   98,
		98,   98,   99,   99,   99,   99,   99,   99,
		99,   100,  101,  101,  102,  103,  103,  104,
		105,  105,  106,  107,  107,  108,  109,  109,
		110,  111,  111,  112,  113,  113,  114,  115,
		115,  116,  116,  117,  118,  118,  119,  119,
		120,  120,  121,  122,  122,  123,  123,  124,
		124,  125,  126,  126,  127,  127,  128,  128,
		129,  129,  130,  130,  131,  131,  132,  132,
		133,  133,  134,  134,  135,  135,  136,  136,
		137,  137,  138,  138,  139,  139,  140,  140,
		141,  141,  142,  142,  143,  143,  144,  144,
		145,  145,  145,  146,  146,  147,  147,  148,
		148,  149,  149,  150,  150,  150,  151,  151,
		152,  152,  153,  153,  153,  154,  154,  155,
		155,  156,  156,  156,  157,  157,  158,  158,
		158,  159,  159,  160,  160,  160,  161,  161,
		162,  162,  162,  163,  163,  164,  164,  164,
		165,  165,  166,  166,  166,  167,  167,  167,
		168,  168,  169,  169,  169,  170,  170,  170,
		171,  171,  172,  172,  172,  173,  173,  173,
		174,  174,  174,  175,  175,  176,  176,  176,
		177,  177,  177,  178,  178,  178,  179,  179,
		179,  180,  180,  180,  181,  181,  182,  182,
		182,  183,  183,  183,  184,  184,  184,  185,
		185,  185,  186,  186,  186,  187,  187,  187,
		188,  188,  188,  189,  189,  189,  189,  190,
		190,  190,  191,  191,  191,  192,  192,  192,
		193,  193,  193,  194,  194,  194,  195,  195,
		195,  196,  196,  196,  196,  197,  197,  197,
		198,  198,  198,  199,  199,  199,  200,  200,
		200,  200,  201,  201,  201,  202,  202,  202,
		202,  203,  203,  203,  204,  204,  204,  205,
		205,  205,  205,  206,  206,  206,  207,  207,
		207,  207,  208,  208,  208,  209,  209,  209,
		209,  210,  210,  210,  211,  211,  211,  211,
		212,  212,  212,  213,  213,  213,  213,  214,
		214,  214,  214,  215,  215,  215,  216,  216,
		216,  216,  217,  217,  217,  217,  218,  218,
		218,  219,  219,  219,  219,  220,  220,  220,
		220,  221,  221,  221,  221,  222,  222,  222,
		223,  223,  223,  223,  224,  224,  224,  224,
		225,  225,  225,  225,  226,  226,  226,  226,
		227,  227,  227,  227,  228,  228,  228,  228,
		229,  229,  229,  229,  230,  230,  230,  230,
		231,  231,  231,  231,  232,  232,  232,  232,
		233,  233,  233,  233,  234,  234,  234,  234,
		235,  235,  235,  235,  236,  236,  236,  236,
		237,  237,  237,  237,  238,  238,  238,  238,
		239,  239,  239,  239,  240,  240,  240,  240,
		240,  241,  241,  241,  241,  242,  242,  242,
		242,  243,  243,  243,  243,  244,  244,  244,
		244,  244,  245,  245,  245,  245,  246,  246,
		246,  246,  247,  247,  247,  247,  247,  248,
		248,  248,  248,  249,  249,  249,  249,  249,
		250,  250,  250,  250,  251,  251,  251,  251,
		251,  252,  252,  252,  252,  253,  253,  253,
		253,  253,  254,  254,  254,  254,  255,  255,
	},
};
#endif

static int dsi_s_wqxga_7_9_bl_notify(struct device *unused, int brightness)
{
	int cur_sd_brightness = atomic_read(&sd_brightness);

	brightness = (brightness * cur_sd_brightness) / 255;

	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");
	else
		brightness = dsi_s_wqxga_7_9_bl_output_measured[brightness];

	return brightness;
}

static int dsi_s_wqxga_7_9_check_fb(struct device *dev, struct fb_info *info)
{
	return info->device == &disp_device->dev;
}

static int __init dsi_s_wqxga_7_9_register_bl_dev(void)
{
	int err = 0;
	return err;
}

static void dsi_s_wqxga_7_9_set_disp_device(
	struct platform_device *board_display_device)
{
	disp_device = board_display_device;
}

static void dsi_s_wqxga_7_9_dc_out_init(struct tegra_dc_out *dc)
{
	dc->dsi = &dsi_s_wqxga_7_9_pdata;
	dc->parent_clk = "pll_d_out0";
	dc->modes = dsi_s_wqxga_7_9_modes;
	dc->n_modes = ARRAY_SIZE(dsi_s_wqxga_7_9_modes);
	dc->enable = dsi_s_wqxga_7_9_enable;
	dc->postpoweron = dsi_s_wqxga_7_9_postpoweron;
	dc->disable = dsi_s_wqxga_7_9_disable;
	dc->postsuspend	= dsi_s_wqxga_7_9_postsuspend,
	dc->width = 120;
	dc->height = 160;
	dc->flags = DC_CTRL_MODE;
}

static void dsi_s_wqxga_7_9_fb_data_init(struct tegra_fb_data *fb)
{
	fb->xres = dsi_s_wqxga_7_9_modes[0].h_active;
	fb->yres = dsi_s_wqxga_7_9_modes[0].v_active;
}

static void
dsi_s_wqxga_7_9_sd_settings_init(struct tegra_dc_sd_settings *settings)
{
	settings->bl_device_name = "pwm-backlight";
}
#ifdef CONFIG_TEGRA_DC_CMU
static void dsi_s_wqxga_7_9_cmu_init(struct tegra_dc_platform_data *pdata)
{
	pdata->cmu = &dsi_s_wqxga_7_9_cmu;
}
#endif
struct tegra_panel __initdata dsi_s_wqxga_7_9 = {
	.init_sd_settings = dsi_s_wqxga_7_9_sd_settings_init,
	.init_dc_out = dsi_s_wqxga_7_9_dc_out_init,
	.init_fb_data = dsi_s_wqxga_7_9_fb_data_init,
	.register_bl_dev = dsi_s_wqxga_7_9_register_bl_dev,
#ifdef CONFIG_TEGRA_DC_CMU
	.init_cmu_data = dsi_s_wqxga_7_9_cmu_init,
#endif
	.set_disp_device = dsi_s_wqxga_7_9_set_disp_device,
};
EXPORT_SYMBOL(dsi_s_wqxga_7_9);

