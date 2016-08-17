/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/export.h>
#include <linux/module.h>

#include <media/mt9m114.h>

#define SIZEOF_I2C_TRANSBUF 128

struct mt9m114_reg {
	u16 cmd; /* command */
	u16 addr;
	u16 val;
};

static struct mt9m114_reg mode_1280x960_30fps[] = {
	{MT9M114_SENSOR_WORD_WRITE, 0x001A, 0x0001},
	{MT9M114_SENSOR_WAIT_MS, 0, 10},
	{MT9M114_SENSOR_WORD_WRITE, 0x001A, 0x0000},
	{MT9M114_SENSOR_WAIT_MS, 0, 50},
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
	{MT9M114_SENSOR_WORD_WRITE, 0x3642, 0x8063},
	{MT9M114_SENSOR_WORD_WRITE, 0x3644, 0x78D0},
	{MT9M114_SENSOR_WORD_WRITE, 0x3646, 0x50CC},
	{MT9M114_SENSOR_WORD_WRITE, 0x3648, 0x3511},
	{MT9M114_SENSOR_WORD_WRITE, 0x364A, 0x0110},
	{MT9M114_SENSOR_WORD_WRITE, 0x364C, 0xBD8A},
	{MT9M114_SENSOR_WORD_WRITE, 0x364E, 0x0CD1},
	{MT9M114_SENSOR_WORD_WRITE, 0x3650, 0x24ED},
	{MT9M114_SENSOR_WORD_WRITE, 0x3652, 0x7C11},
	{MT9M114_SENSOR_WORD_WRITE, 0x3654, 0x0150},
	{MT9M114_SENSOR_WORD_WRITE, 0x3656, 0x124C},
	{MT9M114_SENSOR_WORD_WRITE, 0x3658, 0x3130},
	{MT9M114_SENSOR_WORD_WRITE, 0x365A, 0x508C},
	{MT9M114_SENSOR_WORD_WRITE, 0x365C, 0x21F1},
	{MT9M114_SENSOR_WORD_WRITE, 0x365E, 0x0090},
	{MT9M114_SENSOR_WORD_WRITE, 0x3660, 0xBFCA},
	{MT9M114_SENSOR_WORD_WRITE, 0x3662, 0x0A11},
	{MT9M114_SENSOR_WORD_WRITE, 0x3664, 0x4F4B},
	{MT9M114_SENSOR_WORD_WRITE, 0x3666, 0x28B1},
	{MT9M114_SENSOR_WORD_WRITE, 0x3680, 0x50A9},
	{MT9M114_SENSOR_WORD_WRITE, 0x3682, 0xA04B},
	{MT9M114_SENSOR_WORD_WRITE, 0x3684, 0x0E2D},
	{MT9M114_SENSOR_WORD_WRITE, 0x3686, 0x73EC},
	{MT9M114_SENSOR_WORD_WRITE, 0x3688, 0x164F},
	{MT9M114_SENSOR_WORD_WRITE, 0x368A, 0xF829},
	{MT9M114_SENSOR_WORD_WRITE, 0x368C, 0xC1A8},
	{MT9M114_SENSOR_WORD_WRITE, 0x368E, 0xB0EC},
	{MT9M114_SENSOR_WORD_WRITE, 0x3690, 0xE76A},
	{MT9M114_SENSOR_WORD_WRITE, 0x3692, 0x69AF},
	{MT9M114_SENSOR_WORD_WRITE, 0x3694, 0x378C},
	{MT9M114_SENSOR_WORD_WRITE, 0x3696, 0xA70D},
	{MT9M114_SENSOR_WORD_WRITE, 0x3698, 0x884F},
	{MT9M114_SENSOR_WORD_WRITE, 0x369A, 0xEE8B},
	{MT9M114_SENSOR_WORD_WRITE, 0x369C, 0x5DEF},
	{MT9M114_SENSOR_WORD_WRITE, 0x369E, 0x27CC},
	{MT9M114_SENSOR_WORD_WRITE, 0x36A0, 0xCAAC},
	{MT9M114_SENSOR_WORD_WRITE, 0x36A2, 0x840E},
	{MT9M114_SENSOR_WORD_WRITE, 0x36A4, 0xDAA9},
	{MT9M114_SENSOR_WORD_WRITE, 0x36A6, 0xF00C},
	{MT9M114_SENSOR_WORD_WRITE, 0x36C0, 0x1371},
	{MT9M114_SENSOR_WORD_WRITE, 0x36C2, 0x272F},
	{MT9M114_SENSOR_WORD_WRITE, 0x36C4, 0x2293},
	{MT9M114_SENSOR_WORD_WRITE, 0x36C6, 0xE6D0},
	{MT9M114_SENSOR_WORD_WRITE, 0x36C8, 0xEC32},
	{MT9M114_SENSOR_WORD_WRITE, 0x36CA, 0x11B1},
	{MT9M114_SENSOR_WORD_WRITE, 0x36CC, 0x7BAF},
	{MT9M114_SENSOR_WORD_WRITE, 0x36CE, 0x5813},
	{MT9M114_SENSOR_WORD_WRITE, 0x36D0, 0xB871},
	{MT9M114_SENSOR_WORD_WRITE, 0x36D2, 0x8913},
	{MT9M114_SENSOR_WORD_WRITE, 0x36D4, 0x4610},
	{MT9M114_SENSOR_WORD_WRITE, 0x36D6, 0x7EEE},
	{MT9M114_SENSOR_WORD_WRITE, 0x36D8, 0x0DF3},
	{MT9M114_SENSOR_WORD_WRITE, 0x36DA, 0xB84F},
	{MT9M114_SENSOR_WORD_WRITE, 0x36DC, 0xB532},
	{MT9M114_SENSOR_WORD_WRITE, 0x36DE, 0x1171},
	{MT9M114_SENSOR_WORD_WRITE, 0x36E0, 0x13CF},
	{MT9M114_SENSOR_WORD_WRITE, 0x36E2, 0x22F3},
	{MT9M114_SENSOR_WORD_WRITE, 0x36E4, 0xE090},
	{MT9M114_SENSOR_WORD_WRITE, 0x36E6, 0x8133},
	{MT9M114_SENSOR_WORD_WRITE, 0x3700, 0x88AE},
	{MT9M114_SENSOR_WORD_WRITE, 0x3702, 0x00EA},
	{MT9M114_SENSOR_WORD_WRITE, 0x3704, 0x344F},
	{MT9M114_SENSOR_WORD_WRITE, 0x3706, 0xEC88},
	{MT9M114_SENSOR_WORD_WRITE, 0x3708, 0x3E91},
	{MT9M114_SENSOR_WORD_WRITE, 0x370A, 0xF12D},
	{MT9M114_SENSOR_WORD_WRITE, 0x370C, 0xB0EF},
	{MT9M114_SENSOR_WORD_WRITE, 0x370E, 0x77CD},
	{MT9M114_SENSOR_WORD_WRITE, 0x3710, 0x7930},
	{MT9M114_SENSOR_WORD_WRITE, 0x3712, 0x5C12},
	{MT9M114_SENSOR_WORD_WRITE, 0x3714, 0x500C},
	{MT9M114_SENSOR_WORD_WRITE, 0x3716, 0x22CE},
	{MT9M114_SENSOR_WORD_WRITE, 0x3718, 0x2370},
	{MT9M114_SENSOR_WORD_WRITE, 0x371A, 0x258F},
	{MT9M114_SENSOR_WORD_WRITE, 0x371C, 0x3D30},
	{MT9M114_SENSOR_WORD_WRITE, 0x371E, 0x370C},
	{MT9M114_SENSOR_WORD_WRITE, 0x3720, 0x03ED},
	{MT9M114_SENSOR_WORD_WRITE, 0x3722, 0x9AD0},
	{MT9M114_SENSOR_WORD_WRITE, 0x3724, 0x7ECF},
	{MT9M114_SENSOR_WORD_WRITE, 0x3726, 0x1093},
	{MT9M114_SENSOR_WORD_WRITE, 0x3740, 0x2391},
	{MT9M114_SENSOR_WORD_WRITE, 0x3742, 0xAAD0},
	{MT9M114_SENSOR_WORD_WRITE, 0x3744, 0x28F2},
	{MT9M114_SENSOR_WORD_WRITE, 0x3746, 0xBA4F},
	{MT9M114_SENSOR_WORD_WRITE, 0x3748, 0xC536},
	{MT9M114_SENSOR_WORD_WRITE, 0x374A, 0x1472},
	{MT9M114_SENSOR_WORD_WRITE, 0x374C, 0xD110},
	{MT9M114_SENSOR_WORD_WRITE, 0x374E, 0x2933},
	{MT9M114_SENSOR_WORD_WRITE, 0x3750, 0xD0D1},
	{MT9M114_SENSOR_WORD_WRITE, 0x3752, 0x9F37},
	{MT9M114_SENSOR_WORD_WRITE, 0x3754, 0x34D1},
	{MT9M114_SENSOR_WORD_WRITE, 0x3756, 0x1C6C},
	{MT9M114_SENSOR_WORD_WRITE, 0x3758, 0x3FD2},
	{MT9M114_SENSOR_WORD_WRITE, 0x375A, 0xCB72},
	{MT9M114_SENSOR_WORD_WRITE, 0x375C, 0xBA96},
	{MT9M114_SENSOR_WORD_WRITE, 0x375E, 0x1551},
	{MT9M114_SENSOR_WORD_WRITE, 0x3760, 0xB74F},
	{MT9M114_SENSOR_WORD_WRITE, 0x3762, 0x1672},
	{MT9M114_SENSOR_WORD_WRITE, 0x3764, 0x84F1},
	{MT9M114_SENSOR_WORD_WRITE, 0x3766, 0xC2D6},
	{MT9M114_SENSOR_WORD_WRITE, 0x3782, 0x01E0},
	{MT9M114_SENSOR_WORD_WRITE, 0x3784, 0x0280},
	{MT9M114_SENSOR_WORD_WRITE, 0x37C0, 0xA6EA},
	{MT9M114_SENSOR_WORD_WRITE, 0x37C2, 0x874B},
	{MT9M114_SENSOR_WORD_WRITE, 0x37C4, 0x85CB},
	{MT9M114_SENSOR_WORD_WRITE, 0x37C6, 0x968A},
	{MT9M114_SENSOR_WORD_WRITE, 0x098E, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC960, 0x0AF0},
	{MT9M114_SENSOR_WORD_WRITE, 0xC962, 0x79E2},
	{MT9M114_SENSOR_WORD_WRITE, 0xC964, 0x5EC8},
	{MT9M114_SENSOR_WORD_WRITE, 0xC966, 0x791F},
	{MT9M114_SENSOR_WORD_WRITE, 0xC968, 0x76EE},
	{MT9M114_SENSOR_WORD_WRITE, 0xC96A, 0x0FA0},
	{MT9M114_SENSOR_WORD_WRITE, 0xC96C, 0x7DFA},
	{MT9M114_SENSOR_WORD_WRITE, 0xC96E, 0x7DAF},
	{MT9M114_SENSOR_WORD_WRITE, 0xC970, 0x7E02},
	{MT9M114_SENSOR_WORD_WRITE, 0xC972, 0x7E0A},
	{MT9M114_SENSOR_WORD_WRITE, 0xC974, 0x1964},
	{MT9M114_SENSOR_WORD_WRITE, 0xC976, 0x7CDC},
	{MT9M114_SENSOR_WORD_WRITE, 0xC978, 0x7838},
	{MT9M114_SENSOR_WORD_WRITE, 0xC97A, 0x7C2F},
	{MT9M114_SENSOR_WORD_WRITE, 0xC97C, 0x7792},
	{MT9M114_SENSOR_WORD_WRITE, 0xC95E, 0x0003},

	{MT9M114_SENSOR_WORD_WRITE, 0x098E, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC95E, 0x0003},

	{MT9M114_SENSOR_WORD_WRITE, 0xC892, 0x0267},
	{MT9M114_SENSOR_WORD_WRITE, 0xC894, 0xFF1A},
	{MT9M114_SENSOR_WORD_WRITE, 0xC896, 0xFFB3},
	{MT9M114_SENSOR_WORD_WRITE, 0xC898, 0xFF80},
	{MT9M114_SENSOR_WORD_WRITE, 0xC89A, 0x0166},
	{MT9M114_SENSOR_WORD_WRITE, 0xC89C, 0x0003},
	{MT9M114_SENSOR_WORD_WRITE, 0xC89E, 0xFF9A},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8A0, 0xFEB4},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8A2, 0x024D},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8A4, 0x01BF},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8A6, 0xFF01},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8A8, 0xFFF3},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8AA, 0xFF75},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8AC, 0x0198},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8AE, 0xFFFD},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8B0, 0xFF9A},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8B2, 0xFEE7},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8B4, 0x02A8},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8B6, 0x01D9},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8B8, 0xFF26},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8BA, 0xFFF3},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8BC, 0xFFB3},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8BE, 0x0132},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8C0, 0xFFE8},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8C2, 0xFFDA},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8C4, 0xFECD},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8C6, 0x02C2},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8C8, 0x0075},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8CA, 0x011C},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8CC, 0x009A},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8CE, 0x0105},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8D0, 0x00A4},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8D2, 0x00AC},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8D4, 0x0A8C},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8D6, 0x0F0A},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8D8, 0x1964},

	{MT9M114_SENSOR_WORD_WRITE, 0xC914, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC916, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC918, 0x04FF},
	{MT9M114_SENSOR_WORD_WRITE, 0xC91A, 0x03BF},
	{MT9M114_SENSOR_WORD_WRITE, 0xC904, 0x003B},
	{MT9M114_SENSOR_WORD_WRITE, 0xC906, 0x0041},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC8F2, 0x03},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC8F3, 0x02},
	{MT9M114_SENSOR_WORD_WRITE, 0xC906, 0x003C},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8F4, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8F6, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8F8, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8FA, 0xE724},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8FC, 0x1583},
	{MT9M114_SENSOR_WORD_WRITE, 0xC8FE, 0x2045},
	{MT9M114_SENSOR_WORD_WRITE, 0xC900, 0x05DC},
	{MT9M114_SENSOR_WORD_WRITE, 0xC902, 0x007C},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC90C, 0x80},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC90D, 0x80},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC90E, 0x80},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC90F, 0x88},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC910, 0x80},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC911, 0x80},

	{MT9M114_SENSOR_WORD_WRITE, 0xC926, 0x0060},
	{MT9M114_SENSOR_WORD_WRITE, 0xC928, 0x009A},
	{MT9M114_SENSOR_WORD_WRITE, 0xC946, 0x0070},
	{MT9M114_SENSOR_WORD_WRITE, 0xC948, 0x00F3},
	{MT9M114_SENSOR_WORD_WRITE, 0xC952, 0x0060},
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
	{MT9M114_SENSOR_WORD_WRITE, 0xC93C, 0x0060},
	{MT9M114_SENSOR_WORD_WRITE, 0xC93E, 0x009A},
	{MT9M114_SENSOR_WORD_WRITE, 0xC940, 0x00DC},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC942, 0x38},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC943, 0x30},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC944, 0x50},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC945, 0x19},
	{MT9M114_SENSOR_WORD_WRITE, 0xC94A, 0x00F0},
	{MT9M114_SENSOR_WORD_WRITE, 0xC94C, 0x0010},
	{MT9M114_SENSOR_WORD_WRITE, 0xC94E, 0x01CD},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC950, 0x05},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC951, 0x40},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC87A, 0x42},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC87B, 0x21},
	{MT9M114_SENSOR_BYTE_WRITE, 0xC878, 0x0E},
	{MT9M114_SENSOR_WORD_WRITE, 0xC890, 0x0080},
	{MT9M114_SENSOR_WORD_WRITE, 0xC886, 0x0100},
	{MT9M114_SENSOR_WORD_WRITE, 0xC87C, 0x0010},
	{MT9M114_SENSOR_BYTE_WRITE, 0xB42A, 0x05},
	{MT9M114_SENSOR_BYTE_WRITE, 0xA80A, 0x20},
	{MT9M114_SENSOR_WORD_WRITE, 0x098E, 0x0000},
	{MT9M114_SENSOR_WORD_WRITE, 0xC984, 0x8041},
	{MT9M114_SENSOR_WORD_WRITE, 0x001E, 0x0777},
	{MT9M114_SENSOR_WORD_WRITE, 0x098E, 0xDC00},
	{MT9M114_SENSOR_BYTE_WRITE, 0xDC00, 0x28},
	{MT9M114_SENSOR_WAIT_MS, 0, 50},
	{MT9M114_SENSOR_WORD_WRITE, 0x0080, 0x8002},
	{MT9M114_SENSOR_TABLE_END, 0x0000}
};

struct mt9m114_info {
	struct miscdevice		miscdev_info;
	struct mt9m114_power_rail	power;
	struct mt9m114_sensordata	sensor_data;
	struct i2c_client		*i2c_client;
	struct mt9m114_platform_data	*pdata;
	atomic_t			in_use;
	const struct mt9m114_reg		*mode;
#ifdef CONFIG_DEBUG_FS
	struct dentry			*debugfs_root;
	u32				debug_i2c_offset;
#endif
	u8				i2c_trans_buf[SIZEOF_I2C_TRANSBUF];
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

static long mt9m114_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg);

static inline void mt9m114_msleep(u32 t)
{
	usleep_range(t*1000, t*1000 + 500);
}

static int mt9m114_read_reg(struct i2c_client *client, u16 addr, u8 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[3];

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = data + 2;

	err = i2c_transfer(client->adapter, msg, 2);

	if (err != 2)
		return -EINVAL;

	*val = data[2];

	return 0;
}

static int mt9m114_write_bulk_reg(struct i2c_client *client, u8 *data, int len)
{
	int err;
	struct i2c_msg msg;

	if (!client->adapter)
		return -ENODEV;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = data;

	dev_dbg(&client->dev,
		"%s {0x%04x,", __func__, (int)data[0] << 8 | data[1]);
	for (err = 2; err < len; err++)
		dev_dbg(&client->dev, " 0x%02x", data[err]);
	dev_dbg(&client->dev, "},\n");

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err == 1)
		return 0;

	dev_err(&client->dev, "mt9m114: i2c bulk transfer failed at %x\n",
		(int)data[0] << 8 | data[1]);

	return err;
}

static int mt9m114_write_reg8(struct i2c_client *client, u16 addr, u8 val)
{
	unsigned char data[3];

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);
	data[2] = (u8) (val & 0xff);

	dev_dbg(&client->dev, "0x%x = 0x%x\n", addr, val);
	return mt9m114_write_bulk_reg(client, data, sizeof(data));
}

static int mt9m114_write_reg16(struct i2c_client *client, u16 addr, u16 val)
{
	unsigned char data[4];

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);
	data[2] = (u8) (val >> 8);
	data[3] = (u8) (val & 0xff);

	dev_dbg(&client->dev, "0x%x = 0x%x\n", addr, val);
	return mt9m114_write_bulk_reg(client, data, sizeof(data));
}

static int mt9m114_write_table(
	struct mt9m114_info *info,
	const struct mt9m114_reg table[])
{
	int err;
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
			err = mt9m114_write_reg8(info->i2c_client, next->addr, val);
		else if (next->cmd == MT9M114_SENSOR_WORD_WRITE)
			err = mt9m114_write_reg16(info->i2c_client, next->addr, val);
		if (err)
			return err;
	}
	return 0;
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

	if (info->pdata && info->pdata->power_on)
		info->pdata->power_on(&info->power);
	else {
		dev_err(&info->i2c_client->dev,
			"%s:no valid power_on function.\n", __func__);
		return -EFAULT;
	}
	return 0;
}

int mt9m114_release(struct inode *inode, struct file *file)
{
	struct mt9m114_info *info = file->private_data;

	if (info->pdata && info->pdata->power_off)
		info->pdata->power_off(&info->power);
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
	mt9m114_regulator_get(info, &pw->iovdd, "vif2"); /* interface 1.8v */
	mt9m114_regulator_get(info, &pw->avdd, "vana"); /* ananlog 2.7v */

	return 0;
}

static const struct file_operations mt9m114_fileops = {
	.owner = THIS_MODULE,
	.open = mt9m114_open,
	.unlocked_ioctl = mt9m114_ioctl,
	.release = mt9m114_release,
};

static struct miscdevice mt9m114_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mt9m114",
	.fops = &mt9m114_fileops,
};

#ifdef CONFIG_DEBUG_FS
static int mt9m114_stats_show(struct seq_file *s, void *data)
{
	static struct mt9m114_info *info;

	seq_printf(s, "%-20s : %-20s\n", "Name", "mt9m114-debugfs-testing");
	seq_printf(s, "%-20s : 0x%X\n", "Current i2c-offset Addr",
			info->debug_i2c_offset);
	return 0;
}

static int mt9m114_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt9m114_stats_show, inode->i_private);
}

static const struct file_operations mt9m114_stats_fops = {
	.open       = mt9m114_stats_open,
	.read       = seq_read,
	.llseek     = seq_lseek,
	.release    = single_release,
};

static int debug_i2c_offset_w(void *data, u64 val)
{
	struct mt9m114_info *info = (struct mt9m114_info *)(data);
	dev_info(&info->i2c_client->dev,
			"mt9m114:%s setting i2c offset to 0x%X\n",
			__func__, (u32)val);
	info->debug_i2c_offset = (u32)val;
	dev_info(&info->i2c_client->dev,
			"mt9m114:%s new i2c offset is 0x%X\n", __func__,
			info->debug_i2c_offset);
	return 0;
}

static int debug_i2c_offset_r(void *data, u64 *val)
{
	struct mt9m114_info *info = (struct mt9m114_info *)(data);
	*val = (u64)info->debug_i2c_offset;
	dev_info(&info->i2c_client->dev,
			"mt9m114:%s reading i2c offset is 0x%X\n", __func__,
			info->debug_i2c_offset);
	return 0;
}

static int debug_i2c_read(void *data, u64 *val)
{
	struct mt9m114_info *info = (struct mt9m114_info *)(data);
	u8 temp1 = 0;
	u8 temp2 = 0;
	dev_info(&info->i2c_client->dev,
			"mt9m114:%s reading offset 0x%X\n", __func__,
			info->debug_i2c_offset);
	if (mt9m114_read_reg(info->i2c_client,
				info->debug_i2c_offset, &temp1)
		|| mt9m114_read_reg(info->i2c_client,
			info->debug_i2c_offset+1, &temp2)) {
		dev_err(&info->i2c_client->dev,
				"mt9m114:%s failed\n", __func__);
		return -EIO;
	}
	dev_info(&info->i2c_client->dev,
			"mt9m114:%s read value is 0x%X\n", __func__,
			temp1<<8 | temp2);
	*val = (u64)(temp1<<8 | temp2);
	return 0;
}

static int debug_i2c_write(void *data, u64 val)
{
	struct mt9m114_info *info = (struct mt9m114_info *)(data);
	dev_info(&info->i2c_client->dev,
			"mt9m114:%s writing 0x%X to offset 0x%X\n", __func__,
			(u16)val, info->debug_i2c_offset);
	if (mt9m114_write_reg16(info->i2c_client,
				info->debug_i2c_offset, (u16)val)) {
		dev_err(&info->i2c_client->dev,
			"mt9m114:%s failed\n", __func__);
		return -EIO;
	}
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(i2c_offset_fops, debug_i2c_offset_r,
		debug_i2c_offset_w, "0x%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(i2c_read_fops, debug_i2c_read,
		/*debug_i2c_dummy_w*/ NULL, "0x%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(i2c_write_fops, /*debug_i2c_dummy_r*/NULL,
		debug_i2c_write, "0x%llx\n");

static int mt9m114_debug_init(struct mt9m114_info *info)
{
	dev_dbg(&info->i2c_client->dev, "%s", __func__);

	info->debugfs_root = debugfs_create_dir(mt9m114_device.name, NULL);

	if (!info->debugfs_root)
		goto err_out;

	if (!debugfs_create_file("stats", S_IRUGO,
			info->debugfs_root, info, &mt9m114_stats_fops))
		goto err_out;

	if (!debugfs_create_file("i2c_offset", S_IRUGO | S_IWUSR,
			info->debugfs_root, info, &i2c_offset_fops))
		goto err_out;

	if (!debugfs_create_file("i2c_read", S_IRUGO,
			info->debugfs_root, info, &i2c_read_fops))
		goto err_out;

	if (!debugfs_create_file("i2c_write", S_IWUSR,
			info->debugfs_root, info, &i2c_write_fops))
		goto err_out;

	return 0;

err_out:
	dev_err(&info->i2c_client->dev, "ERROR:%s failed", __func__);
	if (info->debugfs_root)
		debugfs_remove_recursive(info->debugfs_root);
	return -ENOMEM;
}
#endif	/* CONFIG_DEBUG_FS */

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
	int err;

	dev_info(&info->i2c_client->dev,
		"%s: xres %u yres %u\n", __func__, mode->xres, mode->yres);

	sensor_mode = mt9m114_get_mode(info, mode);
	if (sensor_mode == NULL) {
		dev_err(&info->i2c_client->dev,
			"%s: invalid params supplied to set mode %d %d\n",
				__func__, mode->xres, mode->yres);
		return -EINVAL;
	}

	err = mt9m114_write_table(
		info, sensor_mode->mode_tbl);
	if (err)
		return err;

	info->mode = sensor_mode->mode_tbl;

	return 0;
}

static long mt9m114_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct mt9m114_info *info = file->private_data;

	switch (cmd) {
	case MT9M114_SENSOR_IOCTL_SET_MODE:
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
	dev_dbg(&client->dev, "mt9m114: probing sensor.\n");

	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&client->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}

	info->pdata = client->dev.platform_data;
	info->i2c_client = client;
	atomic_set(&info->in_use, 0);
	info->mode = NULL;

	i2c_set_clientdata(client, info);

	mt9m114_power_get(info);
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
#ifdef CONFIG_DEBUG_FS
	mt9m114_debug_init(info);
#endif

	return 0;
}

static int mt9m114_remove(struct i2c_client *client)
{
	struct mt9m114_info *info;
	info = i2c_get_clientdata(client);
	misc_deregister(&mt9m114_device);
	kfree(info);

#ifdef CONFIG_DEBUG_FS
	if (info->debugfs_root)
		debugfs_remove_recursive(info->debugfs_root);
#endif

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
