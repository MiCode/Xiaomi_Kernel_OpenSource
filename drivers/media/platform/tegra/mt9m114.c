/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/atomic.h>
#include <linux/regulator/consumer.h>
#include <linux/regmap.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/sysedp.h>

#include <media/mt9m114.h>

#define SIZEOF_I2C_TRANSBUF 128
#define STATUS_BIT_MASK 0x02
#define STATUS_REG 0x0081

struct mt9m114_reg {
	u16 cmd; /* command */
	u16 addr;
	u16 val;
};

static struct mt9m114_reg mode_1280x960_30fps[] = {
	{MT9M114_SENSOR_WORD_WRITE, 0x301A, 0x0234},

	{MT9M114_SENSOR_WORD_WRITE, 0x098E, 0x1000},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC97E, 0x01},
	{MT9M114_SENSOR_WORD_WRITE, 0xC980, 0x0120},
	{MT9M114_SENSOR_WORD_WRITE, 0xC982, 0x0700},
	{MT9M114_SENSOR_WORD_WRITE, 0xC800, 0x0004},
	{MT9M114_SENSOR_WORD_WRITE, 0xC802, 0x0004},
	{MT9M114_SENSOR_WORD_WRITE, 0xC804, 0x03CB},
	{MT9M114_SENSOR_WORD_WRITE, 0xC806, 0x050B},
	{MT9M114_SENSOR_WORD_WRITE, 0xC808, 0x02DC},
	{MT9M114_SENSOR_WORD_WRITE, 0xC80A, 0x6C00},
	{MT9M114_SENSOR_WORD_WRITE, 0xC80C, 0x0001},
	{MT9M114_SENSOR_WORD_WRITE, 0xC80E, 0x00DB},
	{MT9M114_SENSOR_WORD_WRITE, 0xC810, 0x05B3},
	{MT9M114_SENSOR_WORD_WRITE, 0xC812, 0x03EE},
	{MT9M114_SENSOR_WORD_WRITE, 0xC814, 0x0636},
	{MT9M114_SENSOR_WORD_WRITE, 0xC816, 0x0060},
	{MT9M114_SENSOR_WORD_WRITE, 0xC818, 0x03C3},
	{MT9M114_SENSOR_WORD_WRITE, 0xC826, 0x0020},
	{MT9M114_SENSOR_WORD_WRITE, 0xC834, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC854, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC856, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC858, 0x0500},
	{MT9M114_SENSOR_WORD_WRITE, 0xC85A, 0x03C0},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC85C, 0x03},
	{MT9M114_SENSOR_WORD_WRITE, 0xC868, 0x0500},
	{MT9M114_SENSOR_WORD_WRITE, 0xC86A, 0x03C0},
	{MT9M114_SENSOR_WORD_WRITE, 0xC88C, 0x1E02},
	{MT9M114_SENSOR_WORD_WRITE, 0xC88E, 0x0780},
	{MT9M114_SENSOR_WORD_WRITE, 0xC914, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC916, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC918, 0x04FF},
	{MT9M114_SENSOR_WORD_WRITE, 0xC91A, 0x03BF},
	{MT9M114_SENSOR_WORD_WRITE, 0xC91C, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC91E, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC920, 0x00FF},
	{MT9M114_SENSOR_WORD_WRITE, 0xC922, 0x00BF},
	{MT9M114_SENSOR_BYTE_WRITE, 0xE801, 0x00},

	{MT9M114_SENSOR_WORD_WRITE, 0x316A, 0x8270},
	{MT9M114_SENSOR_WORD_WRITE, 0x316C, 0x8270},
	{MT9M114_SENSOR_WORD_WRITE, 0x3ED0, 0x2305},
	{MT9M114_SENSOR_WORD_WRITE, 0x3ED2, 0x77CF},
	{MT9M114_SENSOR_WORD_WRITE, 0x316E, 0x8202},
	{MT9M114_SENSOR_WORD_WRITE, 0x3180, 0x87FF},
	{MT9M114_SENSOR_WORD_WRITE, 0x30D4, 0x6080},
	{MT9M114_SENSOR_WORD_WRITE, 0xA802, 0x0008},

	{MT9M114_SENSOR_WORD_WRITE, 0x3E14, 0xFF39},
	{MT9M114_SENSOR_WORD_WRITE, 0x301A, 0x8234},

	{MT9M114_SENSOR_WORD_WRITE, 0x098E, 0x495E},
	{MT9M114_SENSOR_WORD_WRITE, 0xC95E, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0x3640, 0x02B0},
	{MT9M114_SENSOR_WORD_WRITE, 0x3642, 0x9A6C},
	{MT9M114_SENSOR_WORD_WRITE, 0x3644, 0x07F1},
	{MT9M114_SENSOR_WORD_WRITE, 0x3646, 0x286B},
	{MT9M114_SENSOR_WORD_WRITE, 0x3648, 0xA8AE},
	{MT9M114_SENSOR_WORD_WRITE, 0x364A, 0x0210},
	{MT9M114_SENSOR_WORD_WRITE, 0x364C, 0xDE6C},
	{MT9M114_SENSOR_WORD_WRITE, 0x364E, 0x2D51},
	{MT9M114_SENSOR_WORD_WRITE, 0x3650, 0x7DAC},
	{MT9M114_SENSOR_WORD_WRITE, 0x3652, 0xBF2D},
	{MT9M114_SENSOR_WORD_WRITE, 0x3654, 0x0310},
	{MT9M114_SENSOR_WORD_WRITE, 0x3656, 0x42AB},
	{MT9M114_SENSOR_WORD_WRITE, 0x3658, 0x5B10},
	{MT9M114_SENSOR_WORD_WRITE, 0x365A, 0x166B},
	{MT9M114_SENSOR_WORD_WRITE, 0x365C, 0x28EA},
	{MT9M114_SENSOR_WORD_WRITE, 0x365E, 0x0190},
	{MT9M114_SENSOR_WORD_WRITE, 0x3660, 0xDF2C},
	{MT9M114_SENSOR_WORD_WRITE, 0x3662, 0x0B91},
	{MT9M114_SENSOR_WORD_WRITE, 0x3664, 0x2C8B},
	{MT9M114_SENSOR_WORD_WRITE, 0x3666, 0xECEE},
	{MT9M114_SENSOR_WORD_WRITE, 0x3680, 0x9E69},
	{MT9M114_SENSOR_WORD_WRITE, 0x3682, 0x3FAA},
	{MT9M114_SENSOR_WORD_WRITE, 0x3684, 0x426B},
	{MT9M114_SENSOR_WORD_WRITE, 0x3686, 0xEDAB},
	{MT9M114_SENSOR_WORD_WRITE, 0x3688, 0x0BCF},
	{MT9M114_SENSOR_WORD_WRITE, 0x368A, 0x4529},
	{MT9M114_SENSOR_WORD_WRITE, 0x368C, 0x5F4B},
	{MT9M114_SENSOR_WORD_WRITE, 0x368E, 0xD84D},
	{MT9M114_SENSOR_WORD_WRITE, 0x3690, 0x16A9},
	{MT9M114_SENSOR_WORD_WRITE, 0x3692, 0x4B0F},
	{MT9M114_SENSOR_WORD_WRITE, 0x3694, 0x3E4C},
	{MT9M114_SENSOR_WORD_WRITE, 0x3696, 0x910B},
	{MT9M114_SENSOR_WORD_WRITE, 0x3698, 0xD58F},
	{MT9M114_SENSOR_WORD_WRITE, 0x369A, 0x492C},
	{MT9M114_SENSOR_WORD_WRITE, 0x369C, 0x6BB0},
	{MT9M114_SENSOR_WORD_WRITE, 0x369E, 0x0FCB},
	{MT9M114_SENSOR_WORD_WRITE, 0x36A0, 0x164C},
	{MT9M114_SENSOR_WORD_WRITE, 0x36A2, 0xCA6E},
	{MT9M114_SENSOR_WORD_WRITE, 0x36A4, 0x920C},
	{MT9M114_SENSOR_WORD_WRITE, 0x36A6, 0x684F},
	{MT9M114_SENSOR_WORD_WRITE, 0x36C0, 0x1751},
	{MT9M114_SENSOR_WORD_WRITE, 0x36C2, 0x156E},
	{MT9M114_SENSOR_WORD_WRITE, 0x36C4, 0xE671},
	{MT9M114_SENSOR_WORD_WRITE, 0x36C6, 0xECEF},
	{MT9M114_SENSOR_WORD_WRITE, 0x36C8, 0x5B93},
	{MT9M114_SENSOR_WORD_WRITE, 0x36CA, 0x3E91},
	{MT9M114_SENSOR_WORD_WRITE, 0x36CC, 0x10CE},
	{MT9M114_SENSOR_WORD_WRITE, 0x36CE, 0xB171},
	{MT9M114_SENSOR_WORD_WRITE, 0x36D0, 0xCB2D},
	{MT9M114_SENSOR_WORD_WRITE, 0x36D2, 0x21B3},
	{MT9M114_SENSOR_WORD_WRITE, 0x36D4, 0x0131},
	{MT9M114_SENSOR_WORD_WRITE, 0x36D6, 0xEB4B},
	{MT9M114_SENSOR_WORD_WRITE, 0x36D8, 0x8192},
	{MT9M114_SENSOR_WORD_WRITE, 0x36DA, 0x2F4F},
	{MT9M114_SENSOR_WORD_WRITE, 0x36DC, 0x7393},
	{MT9M114_SENSOR_WORD_WRITE, 0x36DE, 0x18B1},
	{MT9M114_SENSOR_WORD_WRITE, 0x36E0, 0x1CEE},
	{MT9M114_SENSOR_WORD_WRITE, 0x36E2, 0xF111},
	{MT9M114_SENSOR_WORD_WRITE, 0x36E4, 0x84F0},
	{MT9M114_SENSOR_WORD_WRITE, 0x36E6, 0x5B33},
	{MT9M114_SENSOR_WORD_WRITE, 0x3700, 0xD12C},
	{MT9M114_SENSOR_WORD_WRITE, 0x3702, 0x320D},
	{MT9M114_SENSOR_WORD_WRITE, 0x3704, 0x74D0},
	{MT9M114_SENSOR_WORD_WRITE, 0x3706, 0x94D0},
	{MT9M114_SENSOR_WORD_WRITE, 0x3708, 0xBE11},
	{MT9M114_SENSOR_WORD_WRITE, 0x370A, 0xF66D},
	{MT9M114_SENSOR_WORD_WRITE, 0x370C, 0x0C49},
	{MT9M114_SENSOR_WORD_WRITE, 0x370E, 0x3551},
	{MT9M114_SENSOR_WORD_WRITE, 0x3710, 0xAD0F},
	{MT9M114_SENSOR_WORD_WRITE, 0x3712, 0xCBD2},
	{MT9M114_SENSOR_WORD_WRITE, 0x3714, 0xEB8D},
	{MT9M114_SENSOR_WORD_WRITE, 0x3716, 0x660E},
	{MT9M114_SENSOR_WORD_WRITE, 0x3718, 0x1772},
	{MT9M114_SENSOR_WORD_WRITE, 0x371A, 0xEC70},
	{MT9M114_SENSOR_WORD_WRITE, 0x371C, 0xA5B3},
	{MT9M114_SENSOR_WORD_WRITE, 0x371E, 0x912C},
	{MT9M114_SENSOR_WORD_WRITE, 0x3720, 0x114C},
	{MT9M114_SENSOR_WORD_WRITE, 0x3722, 0x5590},
	{MT9M114_SENSOR_WORD_WRITE, 0x3724, 0xF84F},
	{MT9M114_SENSOR_WORD_WRITE, 0x3726, 0xE670},
	{MT9M114_SENSOR_WORD_WRITE, 0x3740, 0xAE90},
	{MT9M114_SENSOR_WORD_WRITE, 0x3742, 0x8C8E},
	{MT9M114_SENSOR_WORD_WRITE, 0x3744, 0x4073},
	{MT9M114_SENSOR_WORD_WRITE, 0x3746, 0xB970},
	{MT9M114_SENSOR_WORD_WRITE, 0x3748, 0x8174},
	{MT9M114_SENSOR_WORD_WRITE, 0x374A, 0xA310},
	{MT9M114_SENSOR_WORD_WRITE, 0x374C, 0x888F},
	{MT9M114_SENSOR_WORD_WRITE, 0x374E, 0x1193},
	{MT9M114_SENSOR_WORD_WRITE, 0x3750, 0x9431},
	{MT9M114_SENSOR_WORD_WRITE, 0x3752, 0xCE73},
	{MT9M114_SENSOR_WORD_WRITE, 0x3754, 0xAAD0},
	{MT9M114_SENSOR_WORD_WRITE, 0x3756, 0x430E},
	{MT9M114_SENSOR_WORD_WRITE, 0x3758, 0x10F4},
	{MT9M114_SENSOR_WORD_WRITE, 0x375A, 0xF271},
	{MT9M114_SENSOR_WORD_WRITE, 0x375C, 0x8835},
	{MT9M114_SENSOR_WORD_WRITE, 0x375E, 0xD530},
	{MT9M114_SENSOR_WORD_WRITE, 0x3760, 0xA3EE},
	{MT9M114_SENSOR_WORD_WRITE, 0x3762, 0x48F3},
	{MT9M114_SENSOR_WORD_WRITE, 0x3764, 0xE60F},
	{MT9M114_SENSOR_WORD_WRITE, 0x3766, 0x8CD4},
	{MT9M114_SENSOR_WORD_WRITE, 0x3784, 0x0258},
	{MT9M114_SENSOR_WORD_WRITE, 0x3782, 0x0200},
	{MT9M114_SENSOR_WORD_WRITE, 0x37C0, 0xF984},
	{MT9M114_SENSOR_WORD_WRITE, 0x37C2, 0xDC68},
	{MT9M114_SENSOR_WORD_WRITE, 0x37C4, 0xDB0A},
	{MT9M114_SENSOR_WORD_WRITE, 0x37C6, 0x61C6},
	{MT9M114_SENSOR_WORD_WRITE, 0x098E, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC960, 0x0AF0},
	{MT9M114_SENSOR_WORD_WRITE, 0xC962, 0x7841},
	{MT9M114_SENSOR_WORD_WRITE, 0xC964, 0x59C0},
	{MT9M114_SENSOR_WORD_WRITE, 0xC966, 0x7734},
	{MT9M114_SENSOR_WORD_WRITE, 0xC968, 0x7328},
	{MT9M114_SENSOR_WORD_WRITE, 0xC96A, 0x0FA0},
	{MT9M114_SENSOR_WORD_WRITE, 0xC96C, 0x7FF8},
	{MT9M114_SENSOR_WORD_WRITE, 0xC96E, 0x7F92},
	{MT9M114_SENSOR_WORD_WRITE, 0xC970, 0x801C},
	{MT9M114_SENSOR_WORD_WRITE, 0xC972, 0x7E4A},
	{MT9M114_SENSOR_WORD_WRITE, 0xC974, 0x1964},
	{MT9M114_SENSOR_WORD_WRITE, 0xC976, 0x7AD2},
	{MT9M114_SENSOR_WORD_WRITE, 0xC978, 0x6F48},
	{MT9M114_SENSOR_WORD_WRITE, 0xC97A, 0x7BE9},
	{MT9M114_SENSOR_WORD_WRITE, 0xC97C, 0x7967},
	{MT9M114_SENSOR_WORD_WRITE, 0xC95E, 0x0003},

	{MT9M114_SENSOR_WORD_WRITE, 0x098E, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC95E, 0x0003},

	{MT9M114_SENSOR_WORD_WRITE, 0xC892, 0x01F7},
	{MT9M114_SENSOR_WORD_WRITE, 0xC894, 0xFF89},
	{MT9M114_SENSOR_WORD_WRITE, 0xC896, 0xFF80},
	{MT9M114_SENSOR_WORD_WRITE, 0xC898, 0xFF84},
	{MT9M114_SENSOR_WORD_WRITE, 0xC89A, 0x012C},
	{MT9M114_SENSOR_WORD_WRITE, 0xC89C, 0x0050},
	{MT9M114_SENSOR_WORD_WRITE, 0xC89E, 0xFF89},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8A0, 0xFEB0},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8A2, 0x02C7},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8C8, 0x006D},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8CA, 0x011B},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8A4, 0x021C},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8A6, 0xFF34},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8A8, 0xFFB0},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8AA, 0xFF87},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8AC, 0x0140},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8AE, 0x0039},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8B0, 0xFFC6},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8B2, 0xFF1D},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8B4, 0x021D},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8CC, 0x0094},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8CE, 0x00FF},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8B6, 0x01FF},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8B8, 0xFF55},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8BA, 0xFFAB},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8BC, 0xFF9F},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8BE, 0x0181},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8C0, 0xFFE0},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8C2, 0xFFF3},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8C4, 0xFF77},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8C6, 0x0196},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8D0, 0x00AD},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8D2, 0x008D},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8D4, 0x09C4},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8D6, 0x0D67},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8D8, 0x1964},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8EC, 0x09C4},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8EE, 0x1964},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8F2, 0x0003},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8F3, 0x0002},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8F4, 0xF800},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8F6, 0xA800},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8F8, 0x7C00},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8FA, 0x3580},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8FC, 0x0DC0},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8FE, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC900, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC902, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC904, 0x0031},
	{MT9M114_SENSOR_WORD_WRITE, 0xC906, 0x002A},

	{MT9M114_SENSOR_BYTE_WRITE, 0xC90C, 0xB0},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC90D, 0x98},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC90E, 0x80},
	{MT9M114_SENSOR_WORD_WRITE, 0xC926, 0x0020},
	{MT9M114_SENSOR_WORD_WRITE, 0xC928, 0x009A},
	{MT9M114_SENSOR_WORD_WRITE, 0xC946, 0x0070},
	{MT9M114_SENSOR_WORD_WRITE, 0xC948, 0x00F3},
	{MT9M114_SENSOR_WORD_WRITE, 0xC952, 0x0020},
	{MT9M114_SENSOR_WORD_WRITE, 0xC954, 0x009A},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC92A, 0x80},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC92B, 0x4B},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC92C, 0x00},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC92D, 0xFF},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC92E, 0x3C},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC92F, 0x02},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC930, 0x06},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC931, 0x64},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC932, 0x01},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC933, 0x0C},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC934, 0x3C},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC935, 0x3C},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC936, 0x3C},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC937, 0x0F},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC938, 0x64},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC939, 0x64},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC93A, 0x64},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC93B, 0x32},
	{MT9M114_SENSOR_WORD_WRITE, 0xC93C, 0x0020},
	{MT9M114_SENSOR_WORD_WRITE, 0xC93E, 0x009A},
	{MT9M114_SENSOR_WORD_WRITE, 0xC940, 0x00DC},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC942, 0x38},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC943, 0x30},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC944, 0x50},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC945, 0x19},
	{MT9M114_SENSOR_WORD_WRITE, 0xC94A, 0x0230},
	{MT9M114_SENSOR_WORD_WRITE, 0xC94C, 0x0010},
	{MT9M114_SENSOR_WORD_WRITE, 0xC94E, 0x01CD},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC950, 0x05},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC951, 0x40},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC87B, 0x1B},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC878, 0x0E},
	{MT9M114_SENSOR_WORD_WRITE, 0xC890, 0x0080},
	{MT9M114_SENSOR_WORD_WRITE, 0xC886, 0x0100},
	{MT9M114_SENSOR_WORD_WRITE, 0xC87C, 0x005A},
	{MT9M114_SENSOR_BYTE_WRITE, 0xB42A, 0x05},
	{MT9M114_SENSOR_BYTE_WRITE, 0xA80A, 0x20},
	{MT9M114_SENSOR_WORD_WRITE, 0x098E, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC984, 0x8041},
	{MT9M114_SENSOR_WORD_WRITE, 0x001E, 0x0777},
	{MT9M114_SENSOR_WORD_WRITE, 0x098E, 0xDC00},
	{MT9M114_SENSOR_WORD_WRITE, 0xC988, 0x0F00},
	{MT9M114_SENSOR_WORD_WRITE, 0xC98A, 0x0B07},
	{MT9M114_SENSOR_WORD_WRITE, 0xC98C, 0x0D01},
	{MT9M114_SENSOR_WORD_WRITE, 0xC98E, 0x071D},
	{MT9M114_SENSOR_WORD_WRITE, 0xC990, 0x0006},
	{MT9M114_SENSOR_WORD_WRITE, 0xC992, 0x0A0C},
	{MT9M114_SENSOR_WORD_WRITE, 0xA802, 0x0008},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC908, 0x01},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC879, 0x01},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC909, 0x02},
	{MT9M114_SENSOR_BYTE_WRITE, 0xA80A, 0x18},
	{MT9M114_SENSOR_BYTE_WRITE, 0xA80B, 0x18},
	{MT9M114_SENSOR_BYTE_WRITE, 0xAC16, 0x18},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC878, 0x0E},

	{MT9M114_SENSOR_BYTE_WRITE, 0xDC00, 0x28},
	{MT9M114_SENSOR_WAIT_MS, 0, 50},
	{MT9M114_SENSOR_WORD_WRITE, 0x0080, 0x8002},
	{MT9M114_SENSOR_TABLE_END, 0x0000}
};

static struct mt9m114_reg mt9m114_Whitebalance_Auto[] = {
	{MT9M114_SENSOR_WORD_WRITE, 0x098E, 0x0000},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC909, 0x02},
	{MT9M114_SENSOR_TABLE_END, 0x0000}
};

static struct mt9m114_reg mt9m114_Whitebalance_Cloudy[] = {
	{MT9M114_SENSOR_WORD_WRITE, 0x098E, 0x0000},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC909, 0x00},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8F0, 0x1964},
	{MT9M114_SENSOR_TABLE_END, 0x0000}
};

static struct mt9m114_reg mt9m114_Whitebalance_Daylight[] = {
	{MT9M114_SENSOR_WORD_WRITE, 0x098E, 0x0000},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC909, 0x00},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8F0, 0x1964},
	{MT9M114_SENSOR_TABLE_END, 0x0000}
};

static struct mt9m114_reg mt9m114_Whitebalance_Incandescent[] = {
	{MT9M114_SENSOR_WORD_WRITE, 0x098E, 0x0000},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC909, 0x00},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8F0, 0x0A8C},
	{MT9M114_SENSOR_TABLE_END, 0x0000}
};

static struct mt9m114_reg mt9m114_Whitebalance_Fluorescent[] = {
	{MT9M114_SENSOR_WORD_WRITE, 0x098E, 0x0000},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC909, 0x00},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8F0, 0x0E74},
	{MT9M114_SENSOR_TABLE_END, 0x0000}
};

static struct mt9m114_reg mt9m114_EV_zero[] = {
	{MT9M114_SENSOR_WORD_WRITE, 0x098E, 0xC87A},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC87A, 0x3C},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC87B, 0x1E},
	{MT9M114_SENSOR_TABLE_END, 0x0000}
};

static struct mt9m114_reg mt9m114_EV_plus_1[] = {
	{MT9M114_SENSOR_WORD_WRITE, 0x098E, 0xC87A},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC87A, 0x42},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC87B, 0x21},
	{MT9M114_SENSOR_TABLE_END, 0x0000}
};

static struct mt9m114_reg mt9m114_EV_plus_2[] = {
	{MT9M114_SENSOR_WORD_WRITE, 0x098E, 0xC87A},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC87A, 0x48},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC87B, 0x24},
	{MT9M114_SENSOR_TABLE_END, 0x0000}
};

static struct mt9m114_reg mt9m114_EV_minus_1[] = {
	{MT9M114_SENSOR_WORD_WRITE, 0x098E, 0xC87A},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC87A, 0x36},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC87B, 0x1B},
	{MT9M114_SENSOR_TABLE_END, 0x0000}
};

static struct mt9m114_reg mt9m114_EV_minus_2[] = {
	{MT9M114_SENSOR_WORD_WRITE, 0x098E, 0xC87A},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC87A, 0x32},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC87B, 0x19},
	{MT9M114_SENSOR_TABLE_END, 0x0000}
};

struct mt9m114_info {
	struct miscdevice		miscdev_info;
	struct mt9m114_power_rail	power;
	struct mt9m114_sensordata	sensor_data;
	struct i2c_client		*i2c_client;
	struct mt9m114_platform_data	*pdata;
	struct regmap			*regmap;
	atomic_t			in_use;
	const struct mt9m114_reg	*mode;
	u8				i2c_trans_buf[SIZEOF_I2C_TRANSBUF];
	struct clk *mclk;
	struct sysedp_consumer *sysedpc;
};

struct mt9m114_mode_desc {
	u16			xres;
	u16			yres;
	const struct mt9m114_reg *mode_tbl;
	struct mt9m114_modeinfo	mode_info;
};

static struct mt9m114_mode_desc mode_table[] = {
	{
		.xres = 1280,
		.yres = 960,
		.mode_tbl = mode_1280x960_30fps,
	},
	{ },
};

static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

static long mt9m114_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg);

static inline void mt9m114_msleep(u32 t)
{
	usleep_range(t*1000, t*1000 + 500);
}

static int mt9m114_read_reg(struct mt9m114_info *info, u16 addr, u8 *val)
{
	dev_dbg(&info->i2c_client->dev, "0x%x = 0x%x\n", addr, val);
	return regmap_read(info->regmap, addr, (unsigned int *) val);
}

static int mt9m114_write_reg8(struct mt9m114_info *info, u16 addr, u8 val)
{
	dev_dbg(&info->i2c_client->dev, "0x%x = 0x%x\n", addr, val);
	return regmap_write(info->regmap, addr, val);
}

static int mt9m114_write_reg16(struct mt9m114_info *info, u16 addr, u16 val)
{
	unsigned char data[2];

	data[0] = (u8) (val >> 8);
	data[1] = (u8) (val & 0xff);

	dev_dbg(&info->i2c_client->dev, "0x%x = 0x%x\n", addr, val);
	return regmap_raw_write(info->regmap, addr, data, sizeof(data));
}

static int mt9m114_write_table(
	struct mt9m114_info *info,
	const struct mt9m114_reg table[])
{
	int err = 0;
	const struct mt9m114_reg *next;
	u16 val;

	dev_dbg(&info->i2c_client->dev, "yuv %s\n", __func__);

	for (next = table; next->cmd != MT9M114_SENSOR_TABLE_END; next++) {
		if (next->cmd == MT9M114_SENSOR_WAIT_MS) {
			msleep(next->val);
		continue;
		}

		val = next->val;

		if (next->cmd == MT9M114_SENSOR_BYTE_WRITE)
			err = mt9m114_write_reg8(info, next->addr, val);
		else if (next->cmd == MT9M114_SENSOR_WORD_WRITE)
			err = mt9m114_write_reg16(info, next->addr, val);
		if (err)
			return err;
	}
	return 0;
}

static void mt9m114_mclk_disable(struct mt9m114_info *info)
{
	dev_info(&info->i2c_client->dev, "%s: disable MCLK\n", __func__);
	clk_disable_unprepare(info->mclk);
}

static int mt9m114_mclk_enable(struct mt9m114_info *info)
{
	int err;
	unsigned long mclk_init_rate = 24000000;

	dev_info(&info->i2c_client->dev, "%s: enable MCLK with %lu Hz\n",
		__func__, mclk_init_rate);

	err = clk_set_rate(info->mclk, mclk_init_rate);
	if (!err)
		err = clk_prepare_enable(info->mclk);
	return err;
}

static int mt9m114_open(struct inode *inode, struct file *file)
{
	struct miscdevice	*miscdev = file->private_data;
	struct mt9m114_info *info = dev_get_drvdata(miscdev->parent);

	dev_dbg(&info->i2c_client->dev, "mt9m114: open.\n");
	info = container_of(miscdev, struct mt9m114_info, miscdev_info);
	/* check if the device is in use */
	if (atomic_xchg(&info->in_use, 1)) {
		dev_info(&info->i2c_client->dev, "%s:BUSY!\n", __func__);
		return -EBUSY;
	}

	file->private_data = info;

	if (info->pdata && info->pdata->power_on) {
		mt9m114_mclk_enable(info);
		info->pdata->power_on(&info->power);
	} else {
		dev_err(&info->i2c_client->dev,
			"%s:no valid power_on function.\n", __func__);
		return -EFAULT;
	}
	return 0;
}

int mt9m114_release(struct inode *inode, struct file *file)
{
	struct mt9m114_info *info = file->private_data;

	if (info->pdata && info->pdata->power_off) {
		info->pdata->power_off(&info->power);
		mt9m114_mclk_disable(info);
		sysedp_set_state(info->sysedpc, 0);
	}
	file->private_data = NULL;

	/* warn if device is already released */
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	return 0;
}

static int mt9m114_regulator_get(struct mt9m114_info *info,
	struct regulator **vreg, char vreg_name[])
{
	struct regulator *reg = NULL;
	int err = 0;

	reg = devm_regulator_get(&info->i2c_client->dev, vreg_name);
	if (unlikely(IS_ERR(reg))) {
		dev_err(&info->i2c_client->dev, "%s %s ERR: %d\n",
			__func__, vreg_name, (int)reg);
		err = PTR_ERR(reg);
		reg = NULL;
	} else {
		dev_dbg(&info->i2c_client->dev, "%s: %s\n",
			__func__, vreg_name);
	}

	*vreg = reg;
	return err;
}

static int mt9m114_power_get(struct mt9m114_info *info)
{
	struct mt9m114_power_rail *pw = &info->power;

	dev_dbg(&info->i2c_client->dev, "mt9m114: %s\n", __func__);

	/* note: mt9m114 uses i2c address 0x90, which is different from
	 * most of other sensors that using 0x64 or 0x20.
	 *
	 * This needs us to define a new vif2 as 2-0048
	 * for platform board file that uses mt9m114
	 * otherwise below could not get the regulator
	 *
	 * This rails of "vif2" and "vana" can be modified as needed
	 * for a new platform.
	 *
	 * mt9m114: need to get 1.8v first
	 */
	mt9m114_regulator_get(info, &pw->iovdd, "vif"); /* interface 1.8v */
	mt9m114_regulator_get(info, &pw->avdd, "vana"); /* ananlog 2.8v */

	return 0;
}

static const struct file_operations mt9m114_fileops = {
	.owner = THIS_MODULE,
	.open = mt9m114_open,
	.unlocked_ioctl = mt9m114_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mt9m114_ioctl,
#endif
	.release = mt9m114_release,
};

static struct miscdevice mt9m114_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mt9m114",
	.fops = &mt9m114_fileops,
};

static struct mt9m114_modeinfo def_modeinfo = {
	.xres = 1280,
	.yres = 960,
};

static struct mt9m114_mode_desc *mt9m114_get_mode(
	struct mt9m114_info *info, struct mt9m114_mode *mode)
{
	struct mt9m114_mode_desc *mt = mode_table;

	while (mt->xres) {
		if ((mt->xres == mode->xres) &&
			(mt->yres == mode->yres))
				break;
		mt++;
	}

	if (!mt->xres)
		mt = NULL;
	return mt;
}

static int mt9m114_mode_info_init(struct mt9m114_info *info)
{
	struct mt9m114_mode_desc *md = mode_table;
	const struct mt9m114_reg *mt;
	struct mt9m114_modeinfo *mi;

	dev_dbg(&info->i2c_client->dev, "%s", __func__);
	while (md->xres) {
		mi = &md->mode_info;
		mt = md->mode_tbl;
		memcpy(mi, &def_modeinfo, sizeof(*mi));
		dev_dbg(&info->i2c_client->dev, "mode %d x %d ",
			md->xres, md->yres);
		mi->xres = md->xres;
		mi->yres = md->yres;
		md++;
	}
	return 0;
}

static int mt9m114_set_mode(struct mt9m114_info *info,
	struct mt9m114_mode *mode)
{
	struct mt9m114_mode_desc *sensor_mode;
	int err = 0;
	u16 val = 0;
	int count = 10;

	dev_info(&info->i2c_client->dev,
		"%s: xres %u yres %u\n", __func__, mode->xres, mode->yres);

	sensor_mode = mt9m114_get_mode(info, mode);
	if (sensor_mode == NULL) {
		dev_err(&info->i2c_client->dev,
			"%s: invalid params supplied to set mode %d %d\n",
				__func__, mode->xres, mode->yres);
		return -EINVAL;
	}

	sysedp_set_state(info->sysedpc, 1);

	/* polling the status to check if the sensor is ready. */
	err = mt9m114_read_reg(info, STATUS_REG, &val);
	if (err)
		return err;

	while ((val & STATUS_BIT_MASK) != 0 && count > 0) {
		mt9m114_msleep(5);
		err = mt9m114_read_reg(info, STATUS_REG, &val);
		if (err)
			return err;
		count--;
	}

	if (count == 0) {
		dev_err(&info->i2c_client->dev,
			"%s: status register polling timed out\n",
			__func__);
		return -EFAULT;
	}

	err = mt9m114_write_table(
		info, sensor_mode->mode_tbl);
	if (err)
		return err;

	info->mode = sensor_mode->mode_tbl;

	return 0;
}

static int mt9m114_min_frame_rate_reg(
	struct mt9m114_info *info,
	u16 min_frame_rate)
{
	int err = 0;

	err += mt9m114_write_reg16(info, 0xC88E, min_frame_rate);
	err += mt9m114_write_reg16(info, 0x0080, 0x8002);
	return err;
}

static long mt9m114_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct mt9m114_info *info = file->private_data;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(MT9M114_SENSOR_IOCTL_SET_MODE):
	{
		struct mt9m114_mode mode;

		dev_dbg(&info->i2c_client->dev, "MT9M114_IOCTL_SET_MODE\n");
		if (copy_from_user(&mode, (const void __user *)arg,
			sizeof(struct mt9m114_mode))) {
			err = -EFAULT;
			break;
		}
		err = mt9m114_set_mode(info, &mode);
		break;
	}
	case _IOC_NR(MT9M114_SENSOR_IOCTL_SET_WHITE_BALANCE):
	{
		u8 whitebalance;

		if (copy_from_user(&whitebalance, (const void __user *)arg,
			sizeof(whitebalance))) {
			return -EFAULT;
		}

		switch (whitebalance) {
		case MT9M114_YUV_Whitebalance_Auto:
			err = mt9m114_write_table(info,
					mt9m114_Whitebalance_Auto);
			break;
		case MT9M114_YUV_Whitebalance_Daylight:
			err = mt9m114_write_table(info,
					mt9m114_Whitebalance_Daylight);
			break;
		case MT9M114_YUV_Whitebalance_CloudyDaylight:
			err = mt9m114_write_table(info,
					mt9m114_Whitebalance_Cloudy);
			break;
		case MT9M114_YUV_Whitebalance_Incandescent:
			err = mt9m114_write_table(info,
					mt9m114_Whitebalance_Incandescent);
			break;
		case MT9M114_YUV_Whitebalance_Fluorescent:
			err = mt9m114_write_table(info,
					mt9m114_Whitebalance_Fluorescent);
			break;
		default:
			break;
		}

		if (err)
			return err;

		return 0;
	}

	case _IOC_NR(MT9M114_SENSOR_IOCTL_SET_EV):
	{
		short ev;

		if (copy_from_user(&ev,
				(const void __user *)arg,
				sizeof(short)))
			return -EFAULT;

		if (ev == -2)
			err = mt9m114_write_table(info, mt9m114_EV_minus_2);
		else if (ev == -1)
			err = mt9m114_write_table(info, mt9m114_EV_minus_1);
		else if (ev == 0)
			err = mt9m114_write_table(info, mt9m114_EV_zero);
		else if (ev == 1)
			err = mt9m114_write_table(info, mt9m114_EV_plus_1);
		else if (ev == 2)
			err = mt9m114_write_table(info, mt9m114_EV_plus_2);
		else
			err = -1;

		if (err)
			return err;
		return 0;
	}

	case _IOC_NR(MT9M114_SENSOR_IOCTL_SET_MIN_FPS):
	{
		u16 min_frame_rate;

		if (copy_from_user(&min_frame_rate,
				(const void __user *)arg,
				sizeof(u16)))
			return -EFAULT;

		err = mt9m114_min_frame_rate_reg(info, min_frame_rate);
		if (err)
			return err;
		return 0;
	}

	default:
		dev_dbg(&info->i2c_client->dev, "INVALID IOCTL\n");
		err = -EINVAL;
	}

	if (err)
		dev_err(&info->i2c_client->dev,
			"%s - %x: ERR = %d\n", __func__, cmd, err);
	return err;
}

static int mt9m114_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err;
	struct mt9m114_info *info;
	const char *mclk_name;
	dev_dbg(&client->dev, "mt9m114: probing sensor.\n");

	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&client->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}

	info->regmap = devm_regmap_init_i2c(client, &sensor_regmap_config);
	if (IS_ERR(info->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(info->regmap));
		return -ENODEV;
	}

	info->pdata = client->dev.platform_data;
	info->i2c_client = client;
	atomic_set(&info->in_use, 0);
	info->mode = NULL;

	i2c_set_clientdata(client, info);

	mclk_name = info->pdata->mclk_name ?
		    info->pdata->mclk_name : "default_mclk";
	info->mclk = devm_clk_get(&client->dev, mclk_name);
	if (IS_ERR(info->mclk)) {
		dev_err(&client->dev, "%s: unable to get clock %s\n",
			__func__, mclk_name);
		return PTR_ERR(info->mclk);
	}

	mt9m114_power_get(info);
	info->sysedpc = sysedp_create_consumer("mt9m114", "mt9m114");
	mt9m114_mode_info_init(info);

	memcpy(&info->miscdev_info,
		&mt9m114_device,
		sizeof(struct miscdevice));

	err = misc_register(&info->miscdev_info);
	if (err) {
		dev_err(&info->i2c_client->dev, "mt9m114: Unable to register misc device!\n");
		kfree(info);
		return err;
	}

	return 0;
}

static int mt9m114_remove(struct i2c_client *client)
{
	struct mt9m114_info *info;
	info = i2c_get_clientdata(client);
	misc_deregister(&mt9m114_device);
	sysedp_free_consumer(info->sysedpc);
	kfree(info);

	return 0;
}

static const struct i2c_device_id mt9m114_id[] = {
	{ "mt9m114", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, mt9m114_id);

static struct i2c_driver mt9m114_i2c_driver = {
	.driver = {
		.name = "mt9m114",
		.owner = THIS_MODULE,
	},
	.probe = mt9m114_probe,
	.remove = mt9m114_remove,
	.id_table = mt9m114_id,
};

static int __init mt9m114_init(void)
{
	pr_info("mt9m114 sensor driver loading\n");
	return i2c_add_driver(&mt9m114_i2c_driver);
}

static void __exit mt9m114_exit(void)
{
	i2c_del_driver(&mt9m114_i2c_driver);
}

module_init(mt9m114_init);
module_exit(mt9m114_exit);
MODULE_LICENSE("GPL v2");
