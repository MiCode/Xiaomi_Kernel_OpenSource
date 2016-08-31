/*
 * ar0261.c - ar0261 sensor driver
 *
 * Copyright (c) 2013-2014, NVIDIA Corporation. All Rights Reserved.
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

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/regmap.h>
#include <media/ar0261.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

struct ar0261_reg {
	u16 addr;
	u16 val;
};

struct ar0261_info {
	struct miscdevice miscdev_info;
	struct ar0261_power_rail power;
	struct ar0261_sensordata sensor_data;
	struct i2c_client *i2c_client;
	struct ar0261_platform_data *pdata;
	struct clk *mclk;
	struct regmap *regmap;
	atomic_t in_use;
	int mode;
};

#define AR0261_TABLE_WAIT_MS 0
#define AR0261_TABLE_END 1

static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

static struct ar0261_reg mode_1920x1080[] = {
	{0x301A, 0x0019},
	{AR0261_TABLE_WAIT_MS, 10},
	{0x301A, 0x0218},
	{0x31B0, 0x0062},
	{0x31B2, 0x0046},
	{0x31B4, 0x3248},
	{0x31B6, 0x22A6},
	{0x31B8, 0x1832},
	{0x31BA, 0x1052},
	{0x31BC, 0x0408},
	{0x31AE, 0x0201},
	{AR0261_TABLE_WAIT_MS, 1},
	{0x3044, 0x0590},
	{0x3EE6, 0x60AD},
	{0x3EDC, 0xDBFA},
	{0x301A, 0x0218},
	{AR0261_TABLE_WAIT_MS, 10},
	{0x3D00, 0x0481},
	{0x3D02, 0xFFFF},
	{0x3D04, 0xFFFF},
	{0x3D06, 0xFFFF},
	{0x3D08, 0x6600},
	{0x3D0A, 0x0311},
	{0x3D0C, 0x8C67},
	{0x3D0E, 0x0808},
	{0x3D10, 0x4380},
	{0x3D12, 0x4343},
	{0x3D14, 0x8043},
	{0x3D16, 0x4330},
	{0x3D18, 0x0543},
	{0x3D1A, 0x4381},
	{0x3D1C, 0x4C85},
	{0x3D1E, 0x2022},
	{0x3D20, 0x8020},
	{0x3D22, 0xA093},
	{0x3D24, 0x5A8A},
	{0x3D26, 0x4C81},
	{0x3D28, 0x5981},
	{0x3D2A, 0x1E00},
	{0x3D2C, 0x5F83},
	{0x3D2E, 0x5C80},
	{0x3D30, 0x5C81},
	{0x3D32, 0x5F58},
	{0x3D34, 0x6880},
	{0x3D36, 0x1060},
	{0x3D38, 0x8541},
	{0x3D3A, 0xB350},
	{0x3D3C, 0x5F10},
	{0x3D3E, 0x6050},
	{0x3D40, 0x5780},
	{0x3D42, 0x6880},
	{0x3D44, 0x2220},
	{0x3D46, 0x805D},
	{0x3D48, 0x8140},
	{0x3D4A, 0x864B},
	{0x3D4C, 0x8524},
	{0x3D4E, 0x08A0},
	{0x3D50, 0x55B8},
	{0x3D52, 0x429C},
	{0x3D54, 0x4281},
	{0x3D56, 0x4081},
	{0x3D58, 0x2808},
	{0x3D5A, 0x2810},
	{0x3D5C, 0x5727},
	{0x3D5E, 0x1069},
	{0x3D60, 0x4B52},
	{0x3D62, 0x8265},
	{0x3D64, 0x8A65},
	{0x3D66, 0xA95E},
	{0x3D68, 0x5080},
	{0x3D6A, 0x5250},
	{0x3D6C, 0x6080},
	{0x3D6E, 0x6922},
	{0x3D70, 0x2080},
	{0x3D72, 0x5D80},
	{0x3D74, 0x4080},
	{0x3D76, 0x5681},
	{0x3D78, 0x5781},
	{0x3D7A, 0x4B86},
	{0x3D7C, 0x2408},
	{0x3D7E, 0x9345},
	{0x3D80, 0x8144},
	{0x3D82, 0x4481},
	{0x3D84, 0x4586},
	{0x3D86, 0x4E80},
	{0x3D88, 0x4FCD},
	{0x3D8A, 0x4685},
	{0x3D8C, 0x0006},
	{0x3D8E, 0x8143},
	{0x3D90, 0x4380},
	{0x3D92, 0x4343},
	{0x3D94, 0x8043},
	{0x3D96, 0x4380},
	{0x3D98, 0x4343},
	{0x3D9A, 0x8043},
	{0x3D9C, 0x4380},
	{0x3D9E, 0x4343},
	{0x3DA0, 0x8648},
	{0x3DA2, 0x4880},
	{0x3DA4, 0x6B6B},
	{0x3DA6, 0x814C},
	{0x3DA8, 0x864D},
	{0x3DAA, 0xA442},
	{0x3DAC, 0x8641},
	{0x3DAE, 0x804D},
	{0x3DB0, 0x864C},
	{0x3DB2, 0x8A45},
	{0x3DB4, 0x8144},
	{0x3DB6, 0x4481},
	{0x3DB8, 0x4583},
	{0x3DBA, 0x46B7},
	{0x3DBC, 0x7386},
	{0x3DBE, 0x4685},
	{0x3DC0, 0x0006},
	{0x3DC2, 0x8143},
	{0x3DC4, 0x4380},
	{0x3DC6, 0x4343},
	{0x3DC8, 0x8043},
	{0x3DCA, 0x4380},
	{0x3DCC, 0x4343},
	{0x3DCE, 0x8043},
	{0x3DD0, 0x4380},
	{0x3DD2, 0x4343},
	{0x3DD4, 0x8648},
	{0x3DD6, 0x4880},
	{0x3DD8, 0x6A6A},
	{0x3DDA, 0x814C},
	{0x3DDC, 0x864D},
	{0x3DDE, 0xA442},
	{0x3DE0, 0x8641},
	{0x3DE2, 0x804D},
	{0x3DE4, 0x864C},
	{0x3DE6, 0x8A45},
	{0x3DE8, 0x8144},
	{0x3DEA, 0x4481},
	{0x3DEC, 0x4583},
	{0x3DEE, 0x4686},
	{0x3DF0, 0x73FF},
	{0x3DF2, 0xD358},
	{0x3DF4, 0x835B},
	{0x3DF6, 0x825A},
	{0x3DF8, 0x8153},
	{0x3DFA, 0x5467},
	{0x3DFC, 0x6363},
	{0x3DFE, 0x2640},
	{0x3E00, 0x6470},
	{0x3E02, 0xFFFF},
	{0x3E04, 0xFFFF},
	{0x3E06, 0xFFED},
	{0x3E08, 0x4580},
	{0x3E0A, 0x4384},
	{0x3E0C, 0x4380},
	{0x3E0E, 0x0280},
	{0x3E10, 0x8402},
	{0x3E12, 0x8080},
	{0x3E14, 0x6A84},
	{0x3E16, 0x6A80},
	{0x3E18, 0x4484},
	{0x3E1A, 0x4480},
	{0x3E1C, 0x4578},
	{0x3E1E, 0x8270},
	{0x3E20, 0x0000},
	{0x3E22, 0x0000},
	{0x3E24, 0x0000},
	{0x3E26, 0x0000},
	{0x3E28, 0x0000},
	{0x3E2A, 0x0000},
	{0x3E2C, 0x0000},
	{0x3E2E, 0x0000},
	{0x3E30, 0x0000},
	{0x3E32, 0x0000},
	{0x3E34, 0x0000},
	{0x3E36, 0x0000},
	{0x3E38, 0x0000},
	{0x3E3A, 0x0000},
	{0x3E3C, 0x0000},
	{0x3E3E, 0x0000},
	{0x3E40, 0x0000},
	{0x3E42, 0x0000},
	{0x3E44, 0x0000},
	{0x3E46, 0x0000},
	{0x3E48, 0x0000},
	{0x3E4A, 0x0000},
	{0x3E4C, 0x0000},
	{0x3E4E, 0x0000},
	{0x3E50, 0x0000},
	{0x3E52, 0x0000},
	{0x3E54, 0x0000},
	{0x3E56, 0x0000},
	{0x3E58, 0x0000},
	{0x3E5A, 0x0000},
	{0x3E5C, 0x0000},
	{0x3E5E, 0x0000},
	{0x3E60, 0x0000},
	{0x3E62, 0x0000},
	{0x3E64, 0x0000},
	{0x3E66, 0x0000},
	{0x3E68, 0x0000},
	{0x3E6A, 0x0000},
	{0x3E6C, 0x0000},
	{0x3E6E, 0x0000},
	{0x3E70, 0x0000},
	{0x3E72, 0x0000},
	{0x3E74, 0x0000},
	{0x3E76, 0x0000},
	{0x3E78, 0x0000},
	{0x3E7A, 0x0000},
	{0x3E7C, 0x0000},
	{0x3E7E, 0x0000},
	{0x3E80, 0x0000},
	{0x3E82, 0x0000},
	{0x3E84, 0x0000},
	{0x3E86, 0x0000},
	{0x3E88, 0x0000},
	{0x3E8A, 0x0000},
	{0x3E8C, 0x0000},
	{0x3E8E, 0x0000},
	{0x3E90, 0x0000},
	{0x3E92, 0x0000},
	{0x3E94, 0x0000},
	{0x3E96, 0x0000},
	{0x3E98, 0x0000},
	{0x3E9A, 0x0000},
	{0x3E9C, 0x0000},
	{0x3E9E, 0x0000},
	{0x3EA0, 0x0000},
	{0x3EA2, 0x0000},
	{0x3EA4, 0x0000},
	{0x3EA6, 0x0000},
	{0x3EA8, 0x0000},
	{0x3EAA, 0x0000},
	{0x3EAC, 0x0000},
	{0x3EAE, 0x0000},
	{0x3EB0, 0x0000},
	{0x3EB2, 0x0000},
	{0x3EB4, 0x0000},
	{0x3EB6, 0x0000},
	{0x3EB8, 0x0000},
	{0x3EBA, 0x0000},
	{0x3EBC, 0x0000},
	{0x3EBE, 0x0000},
	{0x3EC0, 0x0000},
	{0x3EC2, 0x0000},
	{0x3EC4, 0x0000},
	{0x3EC6, 0x0000},
	{0x3EC8, 0x0000},
	{0x3ECA, 0x0000},
	{0x301A, 0x021C},
	{0x0342, 0x10CC},
	{0x0340, 0x04A4},
	{0x0202, 0x0496},
	{0x0312, 0x045D},
	{0x31AE, 0x0201},
	{0x0300, 0x0005},
	{0x0302, 0x0001},
	{0x0304, 0x0202},
	{0x0306, 0x4040},
	{0x0308, 0x000A},
	{0x030A, 0x0001},
	{0x0344, 0x0008},
	{0x0348, 0x0787},
	{0x0346, 0x0008},
	{0x034A, 0x043F},
	{0x034C, 0x0780},
	{0x034E, 0x0438},
	{0x3040, 0x0041},
	{0x0104, 0x0001},
	{0x3ECC, 0x008F},
	{0x3ECE, 0xA8F0},
	{0x3ED0, 0xFFFF},
	{0x3ED6, 0x7193},
	{0x3ED8, 0x8A11},
	{0x30D2, 0x0020},
	{0x30D4, 0x0040},
	{0x3180, 0x80FF},
	{0x0104, 0x0000},
	{0x301A, 0x001C},
	{AR0261_TABLE_END, 0x0000}
};
static struct ar0261_reg mode_1920x1080_HDR[] = {
	{0x301A, 0x0019},
	{AR0261_TABLE_WAIT_MS, 10},
	{0x301A, 0x0218},
	{0x31B0, 0x0062},
	{0x31B2, 0x0046},
	{0x31B4, 0x3248},
	{0x31B6, 0x22A6},
	{0x31B8, 0x1832},
	{0x31BA, 0x1052},
	{0x31BC, 0x0408},
	{0x31AE, 0x0201},
	{AR0261_TABLE_WAIT_MS, 1},
	{0x3044, 0x0590},
	{0x3EE6, 0x60AD},
	{0x3EDC, 0xDBFA},
	{0x301A, 0x0218},
	{AR0261_TABLE_WAIT_MS, 10},
	{0x3D00, 0x0481},
	{0x3D02, 0xFFFF},
	{0x3D04, 0xFFFF},
	{0x3D06, 0xFFFF},
	{0x3D08, 0x6600},
	{0x3D0A, 0x0311},
	{0x3D0C, 0x8C67},
	{0x3D0E, 0x0808},
	{0x3D10, 0x4380},
	{0x3D12, 0x4343},
	{0x3D14, 0x8043},
	{0x3D16, 0x4330},
	{0x3D18, 0x0543},
	{0x3D1A, 0x4381},
	{0x3D1C, 0x4C85},
	{0x3D1E, 0x2022},
	{0x3D20, 0x8020},
	{0x3D22, 0xA093},
	{0x3D24, 0x5A8A},
	{0x3D26, 0x4C81},
	{0x3D28, 0x5981},
	{0x3D2A, 0x1E00},
	{0x3D2C, 0x5F83},
	{0x3D2E, 0x5C80},
	{0x3D30, 0x5C81},
	{0x3D32, 0x5F58},
	{0x3D34, 0x6880},
	{0x3D36, 0x1060},
	{0x3D38, 0x8541},
	{0x3D3A, 0xB350},
	{0x3D3C, 0x5F10},
	{0x3D3E, 0x6050},
	{0x3D40, 0x5780},
	{0x3D42, 0x6880},
	{0x3D44, 0x2220},
	{0x3D46, 0x805D},
	{0x3D48, 0x8140},
	{0x3D4A, 0x864B},
	{0x3D4C, 0x8524},
	{0x3D4E, 0x08A0},
	{0x3D50, 0x55B8},
	{0x3D52, 0x429C},
	{0x3D54, 0x4281},
	{0x3D56, 0x4081},
	{0x3D58, 0x2808},
	{0x3D5A, 0x2810},
	{0x3D5C, 0x5727},
	{0x3D5E, 0x1069},
	{0x3D60, 0x4B52},
	{0x3D62, 0x8265},
	{0x3D64, 0x8A65},
	{0x3D66, 0xA95E},
	{0x3D68, 0x5080},
	{0x3D6A, 0x5250},
	{0x3D6C, 0x6080},
	{0x3D6E, 0x6922},
	{0x3D70, 0x2080},
	{0x3D72, 0x5D80},
	{0x3D74, 0x4080},
	{0x3D76, 0x5681},
	{0x3D78, 0x5781},
	{0x3D7A, 0x4B86},
	{0x3D7C, 0x2408},
	{0x3D7E, 0x9345},
	{0x3D80, 0x8144},
	{0x3D82, 0x4481},
	{0x3D84, 0x4586},
	{0x3D86, 0x4E80},
	{0x3D88, 0x4FCD},
	{0x3D8A, 0x4685},
	{0x3D8C, 0x0006},
	{0x3D8E, 0x8143},
	{0x3D90, 0x4380},
	{0x3D92, 0x4343},
	{0x3D94, 0x8043},
	{0x3D96, 0x4380},
	{0x3D98, 0x4343},
	{0x3D9A, 0x8043},
	{0x3D9C, 0x4380},
	{0x3D9E, 0x4343},
	{0x3DA0, 0x8648},
	{0x3DA2, 0x4880},
	{0x3DA4, 0x6B6B},
	{0x3DA6, 0x814C},
	{0x3DA8, 0x864D},
	{0x3DAA, 0xA442},
	{0x3DAC, 0x8641},
	{0x3DAE, 0x804D},
	{0x3DB0, 0x864C},
	{0x3DB2, 0x8A45},
	{0x3DB4, 0x8144},
	{0x3DB6, 0x4481},
	{0x3DB8, 0x4583},
	{0x3DBA, 0x46B7},
	{0x3DBC, 0x7386},
	{0x3DBE, 0x4685},
	{0x3DC0, 0x0006},
	{0x3DC2, 0x8143},
	{0x3DC4, 0x4380},
	{0x3DC6, 0x4343},
	{0x3DC8, 0x8043},
	{0x3DCA, 0x4380},
	{0x3DCC, 0x4343},
	{0x3DCE, 0x8043},
	{0x3DD0, 0x4380},
	{0x3DD2, 0x4343},
	{0x3DD4, 0x8648},
	{0x3DD6, 0x4880},
	{0x3DD8, 0x6A6A},
	{0x3DDA, 0x814C},
	{0x3DDC, 0x864D},
	{0x3DDE, 0xA442},
	{0x3DE0, 0x8641},
	{0x3DE2, 0x804D},
	{0x3DE4, 0x864C},
	{0x3DE6, 0x8A45},
	{0x3DE8, 0x8144},
	{0x3DEA, 0x4481},
	{0x3DEC, 0x4583},
	{0x3DEE, 0x4686},
	{0x3DF0, 0x73FF},
	{0x3DF2, 0xD358},
	{0x3DF4, 0x835B},
	{0x3DF6, 0x825A},
	{0x3DF8, 0x8153},
	{0x3DFA, 0x5467},
	{0x3DFC, 0x6363},
	{0x3DFE, 0x2640},
	{0x3E00, 0x6470},
	{0x3E02, 0xFFFF},
	{0x3E04, 0xFFFF},
	{0x3E06, 0xFFED},
	{0x3E08, 0x4580},
	{0x3E0A, 0x4384},
	{0x3E0C, 0x4380},
	{0x3E0E, 0x0280},
	{0x3E10, 0x8402},
	{0x3E12, 0x8080},
	{0x3E14, 0x6A84},
	{0x3E16, 0x6A80},
	{0x3E18, 0x4484},
	{0x3E1A, 0x4480},
	{0x3E1C, 0x4578},
	{0x3E1E, 0x8270},
	{0x3E20, 0x0000},
	{0x3E22, 0x0000},
	{0x3E24, 0x0000},
	{0x3E26, 0x0000},
	{0x3E28, 0x0000},
	{0x3E2A, 0x0000},
	{0x3E2C, 0x0000},
	{0x3E2E, 0x0000},
	{0x3E30, 0x0000},
	{0x3E32, 0x0000},
	{0x3E34, 0x0000},
	{0x3E36, 0x0000},
	{0x3E38, 0x0000},
	{0x3E3A, 0x0000},
	{0x3E3C, 0x0000},
	{0x3E3E, 0x0000},
	{0x3E40, 0x0000},
	{0x3E42, 0x0000},
	{0x3E44, 0x0000},
	{0x3E46, 0x0000},
	{0x3E48, 0x0000},
	{0x3E4A, 0x0000},
	{0x3E4C, 0x0000},
	{0x3E4E, 0x0000},
	{0x3E50, 0x0000},
	{0x3E52, 0x0000},
	{0x3E54, 0x0000},
	{0x3E56, 0x0000},
	{0x3E58, 0x0000},
	{0x3E5A, 0x0000},
	{0x3E5C, 0x0000},
	{0x3E5E, 0x0000},
	{0x3E60, 0x0000},
	{0x3E62, 0x0000},
	{0x3E64, 0x0000},
	{0x3E66, 0x0000},
	{0x3E68, 0x0000},
	{0x3E6A, 0x0000},
	{0x3E6C, 0x0000},
	{0x3E6E, 0x0000},
	{0x3E70, 0x0000},
	{0x3E72, 0x0000},
	{0x3E74, 0x0000},
	{0x3E76, 0x0000},
	{0x3E78, 0x0000},
	{0x3E7A, 0x0000},
	{0x3E7C, 0x0000},
	{0x3E7E, 0x0000},
	{0x3E80, 0x0000},
	{0x3E82, 0x0000},
	{0x3E84, 0x0000},
	{0x3E86, 0x0000},
	{0x3E88, 0x0000},
	{0x3E8A, 0x0000},
	{0x3E8C, 0x0000},
	{0x3E8E, 0x0000},
	{0x3E90, 0x0000},
	{0x3E92, 0x0000},
	{0x3E94, 0x0000},
	{0x3E96, 0x0000},
	{0x3E98, 0x0000},
	{0x3E9A, 0x0000},
	{0x3E9C, 0x0000},
	{0x3E9E, 0x0000},
	{0x3EA0, 0x0000},
	{0x3EA2, 0x0000},
	{0x3EA4, 0x0000},
	{0x3EA6, 0x0000},
	{0x3EA8, 0x0000},
	{0x3EAA, 0x0000},
	{0x3EAC, 0x0000},
	{0x3EAE, 0x0000},
	{0x3EB0, 0x0000},
	{0x3EB2, 0x0000},
	{0x3EB4, 0x0000},
	{0x3EB6, 0x0000},
	{0x3EB8, 0x0000},
	{0x3EBA, 0x0000},
	{0x3EBC, 0x0000},
	{0x3EBE, 0x0000},
	{0x3EC0, 0x0000},
	{0x3EC2, 0x0000},
	{0x3EC4, 0x0000},
	{0x3EC6, 0x0000},
	{0x3EC8, 0x0000},
	{0x3ECA, 0x0000},
	{0x301A, 0x021C},
	{0x0342, 0x10CC},
	{0x0340, 0x04A4},
	{0x0312, 0x045D},
	{0x0202, 0x1952},
	{0x3088, 0x0244},
	{0x303E, 0x0001},
	{0x31AE, 0x0201},
	{0x0300, 0x0005},
	{0x0302, 0x0001},
	{0x0304, 0x0202},
	{0x0306, 0x4040},
	{0x0308, 0x000A},
	{0x030A, 0x0001},
	{0x0344, 0x0008},
	{0x0348, 0x0787},
	{0x0346, 0x0008},
	{0x034A, 0x043F},
	{0x034C, 0x0780},
	{0x034E, 0x0438},
	{0x3040, 0x0041},
	{0x0104, 0x0001},
	{0x3ECC, 0x008F},
	{0x3ECE, 0xA8F0},
	{0x3ED0, 0xFFFF},
	{0x3ED6, 0x7193},
	{0x3ED8, 0x8A11},
	{0x30D2, 0x0020},
	{0x30D4, 0x0040},
	{0x3180, 0x80FF},
	{0x0104, 0x0000},
	{0x301A, 0x001C},
	{AR0261_TABLE_END, 0x0000}
};

enum {
	AR0261_MODE_1920X1080,
	AR0261_MODE_1920X1080_HDR,
};

static struct ar0261_reg *mode_table[] = {
	[AR0261_MODE_1920X1080] = mode_1920x1080,
	[AR0261_MODE_1920X1080_HDR] = mode_1920x1080_HDR,
};

static inline void
msleep_range(unsigned int delay_base)
{
	usleep_range(delay_base*1000, delay_base*1000+500);
}

static inline void
ar0261_get_frame_length_regs(struct ar0261_reg *regs, u32 frame_length)
{
	regs->addr = AR0261_FRAME_LEN_LINES;
	regs->val = frame_length;
}

static inline void
ar0261_get_coarse_time_regs(struct ar0261_reg *regs, u32 coarse_time)
{
	regs->addr = AR0261_COARSE_INTEGRATION_TIME;
	regs->val = coarse_time;
}

static inline void
ar0261_get_coarse_time_short_regs(struct ar0261_reg *regs, u32 coarse_time)
{
	regs->addr = AR0261_COARSE_INTEGRATION_SHORT_TIME;
	regs->val = coarse_time;
}

static inline void
ar0261_get_gain_reg(struct ar0261_reg *regs, u16 gain)
{
	regs->addr = AR0261_ANA_GAIN_GLOBAL;
	regs->val = gain;
}

static int
ar0261_read_reg(struct ar0261_info *info, u16 addr, u16 *val)
{
	int err;
	unsigned char data[2];

	err = regmap_raw_read(info->regmap, addr, data, sizeof(data));
	if (!err)
		*val = (u16)data[0] << 8 | data[1];

	return err;
}

static int
ar0261_write_reg(struct ar0261_info *info, u16 addr, u16 val)
{
	int err;
	unsigned char data[2];

	data[0] = (u8) (val >> 8);
	data[1] = (u8) (val & 0xff);
	err = regmap_raw_write(info->regmap, addr, data, sizeof(data));
	if (err)
		dev_err(&info->i2c_client->dev,
			"%s:i2c write failed, %x = %x\n", __func__, addr, val);

	return err;
}

static inline int ar0261_i2c_wr8(struct ar0261_info *info, u16 reg, u8 val)
{
	return regmap_write(info->regmap, reg, val);
}

static int
ar0261_write_table(struct ar0261_info *info,
			 const struct ar0261_reg table[],
			 const struct ar0261_reg override_list[],
			 int num_override_regs)
{
	const struct ar0261_reg *next;
	int err = 0;
	int i;
	u16 val;

	dev_info(&info->i2c_client->dev, "ar0261_write_table\n");

	for (next = table; next->addr != AR0261_TABLE_END; next++) {

		if (next->addr == AR0261_TABLE_WAIT_MS) {
			msleep_range(next->val);
			continue;
		}

		val = next->val;

		/* When an override list is passed in, replace the reg */
		/* value to write if the reg is in the list */
		if (override_list) {
			for (i = 0; i < num_override_regs; i++) {
				if (next->addr == override_list[i].addr) {
					val = override_list[i].val;
					break;
				}
			}
		}

		err = ar0261_write_reg(info, next->addr, val);
		if (err)
			break;
	}

	return err;
}

static int
ar0261_set_mode(struct ar0261_info *info, struct ar0261_mode *mode)
{
	struct device *dev = &info->i2c_client->dev;
	int sensor_mode;
	int err;
	struct ar0261_reg reg_list[HDR_MODE_OVERRIDE_REGS];

	dev_info(dev, "%s: res [%ux%u] framelen %u coarsetime %u gain %u hdr %d\n",
		__func__, mode->xres, mode->yres,
		mode->frame_length, mode->coarse_time, mode->gain,
		mode->hdr_en);

	if ((mode->xres == 1920) && (mode->yres == 1080)) {
		if (mode->hdr_en == 0)
			sensor_mode = AR0261_MODE_1920X1080;
		else if (mode->hdr_en == 1)
			sensor_mode = AR0261_MODE_1920X1080_HDR;
		else {
			dev_err(dev, "%s: invalid hdr setting to set mode %d\n",
			__func__, mode->hdr_en);
			return -EINVAL;
		}
	} else {
		dev_err(dev, "%s: invalid resolution to set mode %d %d\n",
			__func__, mode->xres, mode->yres);
		return -EINVAL;
	}

	/*
	 * get a list of override regs for the asking frame length,
	 * coarse integration time, and gain.
	 */
	ar0261_get_frame_length_regs(reg_list, mode->frame_length);
	ar0261_get_coarse_time_regs(reg_list + 1, mode->coarse_time);
	ar0261_get_gain_reg(reg_list + 2, mode->gain);
	/* if HDR is enabled */
	if (mode->hdr_en == 1) {
		ar0261_get_coarse_time_short_regs(
			reg_list + 3, mode->coarse_time_short);
	}

	err = ar0261_write_table(info, mode_table[sensor_mode],
			reg_list, mode->hdr_en ? HDR_MODE_OVERRIDE_REGS :
			NORMAL_MODE_OVERRIDE_REGS);
	if (err)
		return err;

	info->mode = sensor_mode;
	dev_info(dev, "[ar0261]: stream on.\n");
	return 0;
}

static int
ar0261_get_status(struct ar0261_info *info, u8 *dev_status)
{
	/* TBD */
	*dev_status = 0;
	return 0;
}

static int
ar0261_set_frame_length(struct ar0261_info *info,
				u32 frame_length,
				bool group_hold)
{
	struct ar0261_reg reg_list;
	int ret;

	ar0261_get_frame_length_regs(&reg_list, frame_length);

	if (group_hold) {
		ret = ar0261_i2c_wr8(info, AR0261_GROUP_PARAM_HOLD, 0x01);
		if (ret)
			return ret;
	}

	ret = ar0261_write_reg(info, reg_list.addr, reg_list.val);
	if (ret)
		return ret;

	if (group_hold) {
		ret = ar0261_i2c_wr8(info, AR0261_GROUP_PARAM_HOLD, 0x0);
		if (ret)
			return ret;
	}

	return 0;
}

static int
ar0261_set_coarse_time(struct ar0261_info *info,
				u32 coarse_time,
				bool group_hold)
{
	int ret;
	struct ar0261_reg reg_list;

	ar0261_get_coarse_time_regs(&reg_list, coarse_time);

	if (group_hold) {
		ret = ar0261_i2c_wr8(info, AR0261_GROUP_PARAM_HOLD, 0x01);
		if (ret)
			return ret;
	}

	ret = ar0261_write_reg(info, reg_list.addr, reg_list.val);
	if (ret)
		return ret;

	if (group_hold) {
		ret = ar0261_i2c_wr8(info, AR0261_GROUP_PARAM_HOLD, 0x0);
		if (ret)
			return ret;
	}
	return 0;
}

static int
ar0261_set_hdr_coarse_time(struct ar0261_info *info,
				struct ar0261_hdr *values,
				bool group_hold)
{
	struct ar0261_reg reg_list;
	struct ar0261_reg reg_list_short;
	int ret;

	/* get long and short coarse time registers */
	ar0261_get_coarse_time_regs(&reg_list, values->coarse_time_long);
	ar0261_get_coarse_time_short_regs(&reg_list_short,
			values->coarse_time_short);
	/* set group hold */

	if (group_hold) {
		ret = ar0261_i2c_wr8(info, AR0261_GROUP_PARAM_HOLD, 0x1);
		if (ret)
			return ret;
	}

	/* writing long exposure */
	ret = ar0261_write_reg(info, reg_list.addr, reg_list.val);
	if (ret)
		return ret;
	/* writing short exposure */
	ret = ar0261_write_reg(info, reg_list_short.addr,
		 reg_list_short.val);
	if (ret)
		return ret;

	if (group_hold) {
		ret = ar0261_i2c_wr8(info, AR0261_GROUP_PARAM_HOLD, 0x0);
		if (ret)
			return ret;
	}
	return 0;
}

static int
ar0261_set_gain(struct ar0261_info *info, u16 gain, bool group_hold)
{
	int ret;
	struct ar0261_reg reg_list;

	ar0261_get_gain_reg(&reg_list, gain);

	if (group_hold) {
		ret = ar0261_i2c_wr8(info, AR0261_GROUP_PARAM_HOLD, 0x1);
		if (ret)
			return ret;
	}

	ret = ar0261_i2c_wr8(info, reg_list.addr, reg_list.val);
	if (ret)
		return ret;

	if (group_hold) {
		ret = ar0261_i2c_wr8(info, AR0261_GROUP_PARAM_HOLD, 0x0);
		if (ret)
			return ret;
	}
	return 0;
}

static int
ar0261_set_group_hold(struct ar0261_info *info, struct ar0261_ae *ae)
{
	int ret;
	int count = 0;
	bool grouphold_enabled = false;
	struct ar0261_hdr values;

	values.coarse_time_long = ae->coarse_time;
	values.coarse_time_short = ae->coarse_time_short;

	if (ae->gain_enable)
		count++;
	if (ae->coarse_time_enable)
		count++;
	if (ae->frame_length_enable)
		count++;
	if (count >= 2)
		grouphold_enabled = true;

	if (grouphold_enabled) {
		ret = ar0261_i2c_wr8(info, AR0261_GROUP_PARAM_HOLD, 0x1);
		if (ret)
			return ret;
	}


	if (ae->gain_enable)
		ar0261_set_gain(info, ae->gain, false);
	if (ae->coarse_time_enable)
		ar0261_set_hdr_coarse_time(info, &values, false);
	if (ae->frame_length_enable)
		ar0261_set_frame_length(info, ae->frame_length, false);

	if (grouphold_enabled) {
		ret = ar0261_i2c_wr8(info, AR0261_GROUP_PARAM_HOLD, 0x0);
		if (ret)
			return ret;
	}

	return 0;
}

static int ar0261_get_sensor_id(struct ar0261_info *info)
{
	int ret = 0;
	int i = 0;
	u16 store = 0;

	pr_info("%s\n", __func__);
	if (info->sensor_data.fuse_id_size)
		return 0;

	ret |= ar0261_write_reg(info, 0x301A, 0x0018);
	ret |= ar0261_write_reg(info, 0x3054, 0x0500);
	ret |= ar0261_write_reg(info, 0x3052, 0x0000);
	ret |= ar0261_write_reg(info, 0x304A, 0x0400);
	ret |= ar0261_write_reg(info, 0x304C, 0x0220);
	ret |= ar0261_write_reg(info, 0x304A, 0x0410);

	i = 0;
	do {
		ret |= ar0261_read_reg(info, 0x304A, &store);
		usleep_range(1000, 1500);
		i++;
	} while (!(store & (0x1 << 5)) && (i <= 10));

	if (!(store & (0x1 << 6)))
		return 0;

	for (i = 0; i < 16; i += 2) {
		ret |= ar0261_read_reg(info, 0x3804 + i, &store);
		info->sensor_data.fuse_id[i] = store;
		info->sensor_data.fuse_id[i+1] = store >> 8;
	}

	ret |= ar0261_write_reg(info, 0x301A, 0x001C);

	if (!ret)
		info->sensor_data.fuse_id_size = i;

	return ret;
}

static void ar0261_mclk_disable(struct ar0261_info *info)
{
	dev_dbg(&info->i2c_client->dev, "%s: disable MCLK\n", __func__);
	clk_disable_unprepare(info->mclk);
}

static int ar0261_mclk_enable(struct ar0261_info *info)
{
	int err;
	unsigned long mclk_init_rate = 24000000;

	dev_dbg(&info->i2c_client->dev, "%s: enable MCLK with %lu Hz\n",
			__func__, mclk_init_rate);

	err = clk_set_rate(info->mclk, mclk_init_rate);
	if (!err)
		err = clk_prepare_enable(info->mclk);
	return err;
}

static long
ar0261_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	int err;
	struct ar0261_info *info = file->private_data;
	struct device *dev = &info->i2c_client->dev;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(AR0261_IOCTL_SET_MODE):
	{
		struct ar0261_mode mode;
		if (copy_from_user(&mode,
			(const void __user *)arg,
			sizeof(struct ar0261_mode))) {
			dev_err(dev, "%s:Failed to get mode from user.\n",
			__func__);
			return -EFAULT;
		}
		return ar0261_set_mode(info, &mode);
	}
	case _IOC_NR(AR0261_IOCTL_SET_FRAME_LENGTH):
		return ar0261_set_frame_length(info, (u32)arg, true);
	case _IOC_NR(AR0261_IOCTL_SET_COARSE_TIME):
		return ar0261_set_coarse_time(info, (u32)arg, true);
	case _IOC_NR(AR0261_IOCTL_SET_GAIN):
		return ar0261_set_gain(info, (u16)arg, true);
	case _IOC_NR(AR0261_IOCTL_GET_STATUS):
	{
		u8 status;

		err = ar0261_get_status(info, &status);
		if (err)
			return err;
		if (copy_to_user((void __user *)arg, &status, 1)) {
			dev_err(dev, "%s:Failed to copy status to user.\n",
			__func__);
			return -EFAULT;
		}
		return 0;
	}
	case _IOC_NR(AR0261_IOCTL_GET_SENSORDATA):
	{
		err = ar0261_get_sensor_id(info);

		if (err) {
			dev_err(dev, "%s:Failed to get fuse id info.\n",
			__func__);
			return err;
		}
		if (copy_to_user((void __user *)arg,
				&info->sensor_data,
				sizeof(struct ar0261_sensordata))) {
			dev_info(dev, "%s:Fail copy fuse id to user space\n",
				__func__);
			return -EFAULT;
		}
		return 0;
	}
	case _IOC_NR(AR0261_IOCTL_SET_GROUP_HOLD):
	{
		struct ar0261_ae ae;
		if (copy_from_user(&ae, (const void __user *)arg,
				sizeof(struct ar0261_ae))) {
			dev_info(dev, "%s:fail group hold\n", __func__);
			return -EFAULT;
		}
		return ar0261_set_group_hold(info, &ae);
	}
	case _IOC_NR(AR0261_IOCTL_SET_HDR_COARSE_TIME):
	{
		struct ar0261_hdr values;

		dev_dbg(&info->i2c_client->dev,
				"AR0261_IOCTL_SET_HDR_COARSE_TIME\n");
		if (copy_from_user(&values,
			(const void __user *)arg,
			sizeof(struct ar0261_hdr))) {
				err = -EFAULT;
				break;
		}
		err = ar0261_set_hdr_coarse_time(info, &values, true);
		break;
	}
	default:
		dev_err(dev, "%s:unknown cmd.\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int
ar0261_open(struct inode *inode, struct file *file)
{
	int err;
	struct miscdevice *miscdev = file->private_data;
	struct ar0261_info *info;

	info = container_of(miscdev, struct ar0261_info, miscdev_info);
	/* check if the device is in use */
	if (atomic_xchg(&info->in_use, 1)) {
		dev_info(&info->i2c_client->dev, "%s:BUSY!\n", __func__);
		return -EBUSY;
	}

	file->private_data = info;

	err = ar0261_mclk_enable(info);
	if (err < 0)
		return err;

	if (info->pdata && info->pdata->power_on)
		err = info->pdata->power_on(&info->power);
	else {
		dev_err(&info->i2c_client->dev,
			"%s:no valid power_on function.\n", __func__);
		err = -EEXIST;
	}
	if (err < 0)
		goto ar0261_open_fail;

	return 0;

ar0261_open_fail:
	ar0261_mclk_disable(info);
	return err;
}

static int
ar0261_release(struct inode *inode, struct file *file)
{
	struct ar0261_info *info = file->private_data;

	if (info->pdata && info->pdata->power_off)
		info->pdata->power_off(&info->power);

	ar0261_mclk_disable(info);

	file->private_data = NULL;

	/* warn if device is already released */
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	return 0;
}

static int ar0261_power_put(struct ar0261_power_rail *pw)
{
	if (likely(pw->dvdd))
		regulator_put(pw->dvdd);

	if (likely(pw->avdd))
		regulator_put(pw->avdd);

	if (likely(pw->iovdd))
		regulator_put(pw->iovdd);

	pw->dvdd = NULL;
	pw->avdd = NULL;
	pw->iovdd = NULL;

	return 0;
}

static int ar0261_regulator_get(struct ar0261_info *info,
	struct regulator **vreg, char vreg_name[])
{
	struct regulator *reg = NULL;
	int err = 0;

	reg = regulator_get(&info->i2c_client->dev, vreg_name);
	if (unlikely(IS_ERR(reg))) {
		dev_err(&info->i2c_client->dev, "%s %s ERR: %d\n",
			__func__, vreg_name, (int)reg);
		err = PTR_ERR(reg);
		reg = NULL;
	} else
		dev_dbg(&info->i2c_client->dev, "%s: %s\n",
			__func__, vreg_name);

	*vreg = reg;
	return err;
}

static int ar0261_power_get(struct ar0261_info *info)
{
	struct ar0261_power_rail *pw = &info->power;

	ar0261_regulator_get(info, &pw->dvdd, "vdig"); /* digital 1.2v */
	ar0261_regulator_get(info, &pw->avdd, "vana"); /* analog 2.7v */
	ar0261_regulator_get(info, &pw->iovdd, "vif"); /* interface 1.8v */

	return 0;
}

static const struct file_operations ar0261_fileops = {
	.owner = THIS_MODULE,
	.open = ar0261_open,
	.unlocked_ioctl = ar0261_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ar0261_ioctl,
#endif
	.release = ar0261_release,
};

static struct miscdevice ar0261_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ar0261",
	.fops = &ar0261_fileops,
};

static int
ar0261_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ar0261_info *info;
	const char *mclk_name;
	int err = 0;

	pr_info("[ar0261]: probing sensor.\n");

	info = devm_kzalloc(&client->dev,
		sizeof(struct ar0261_info), GFP_KERNEL);
	if (!info) {
		pr_err("[ar0261]:%s:Unable to allocate memory!\n", __func__);
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
	info->mode = -1;

	mclk_name = info->pdata->mclk_name ?
		info->pdata->mclk_name : "default_mclk";
	info->mclk = devm_clk_get(&client->dev, mclk_name);
	if (IS_ERR(info->mclk)) {
		dev_err(&client->dev, "%s: unable to get clock %s\n",
			__func__, mclk_name);
		return PTR_ERR(info->mclk);
	}

	i2c_set_clientdata(client, info);

	ar0261_power_get(info);

	memcpy(&info->miscdev_info,
		&ar0261_device,
		sizeof(struct miscdevice));

	err = misc_register(&info->miscdev_info);
	if (err) {
		ar0261_power_put(&info->power);
		pr_err("[ar0261]:%s:Unable to register misc device!\n",
		__func__);
	}

	return err;
}

static int
ar0261_remove(struct i2c_client *client)
{
	struct ar0261_info *info = i2c_get_clientdata(client);

	ar0261_power_put(&info->power);
	misc_deregister(&ar0261_device);
	return 0;
}

static const struct i2c_device_id ar0261_id[] = {
	{ "ar0261", 0 },
};

MODULE_DEVICE_TABLE(i2c, ar0261_id);

static struct i2c_driver ar0261_i2c_driver = {
	.driver = {
		.name = "ar0261",
		.owner = THIS_MODULE,
	},
	.probe = ar0261_probe,
	.remove = ar0261_remove,
	.id_table = ar0261_id,
};

static int __init
ar0261_init(void)
{
	pr_info("[ar0261] sensor driver loading\n");
	return i2c_add_driver(&ar0261_i2c_driver);
}

static void __exit
ar0261_exit(void)
{
	i2c_del_driver(&ar0261_i2c_driver);
}

module_init(ar0261_init);
module_exit(ar0261_exit);

