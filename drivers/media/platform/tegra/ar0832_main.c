/*
* ar0832_main.c - Aptina AR0832 8M Bayer type sensor driver
*
* Copyright (c) 2011-2014, NVIDIA Corporation. All Rights Reserved.
*
* This file is licensed under the terms of the GNU General Public License
* version 2. This program is licensed "as is" without any warranty of any
* kind, whether express or implied.
*/

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/clk.h>
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

#include <media/ar0832_main.h>
#include <media/nvc.h>

#define POS_ACTUAL_LOW			0
#define POS_ACTUAL_HIGH			255
#define SETTLE_TIME			100
#define AR0832_SLEW_RATE_DISABLED	0
#define AR0832_SLEW_RATE_SLOWEST	7
#define AR0832_FUSE_ID_SIZE		16


struct ar0832_sensor_info {
	int mode;
	struct ar0832_stereo_region region;
};

struct ar0832_focuser_info {
	struct nv_focuser_config config;
	int focuser_init_flag;
	u16 last_position;
};

struct ar0832_power_rail {
	struct regulator *sen_1v8_reg;
	struct regulator *sen_2v8_reg;
};

struct ar0832_dev {
	struct ar0832_sensor_info *sensor_info;
	struct ar0832_focuser_info *focuser_info;
	struct ar0832_platform_data *pdata;
	struct i2c_client *i2c_client;
	struct clk *mclk;
	struct mutex ar0832_camera_lock;
	struct miscdevice misc_dev;
	struct ar0832_power_rail power_rail;
	int brd_power_cnt;
	atomic_t in_use;
	char dname[20];
	int is_stereo;
	u16 sensor_id_data;
	struct dentry *debugdir;
	struct nvc_fuseid fuse_id;
};

#define UpperByte16to8(x) ((u8)((x & 0xFF00) >> 8))
#define LowerByte16to8(x) ((u8)(x & 0x00FF))

#define ar0832_TABLE_WAIT_MS 0
#define ar0832_TABLE_END 1
#define ar0832_MAX_RETRIES 3

/* AR0832 Register */
#define AR0832_SENSORID_REG	0x0002
#define AR0832_RESET_REG	0x301A
#define AR0832_ID_REG		0x31FC
#define AR0832_GLOBAL_GAIN_REG	0x305E
#define AR0832_TEST_PATTERN_REG	0x0600
#define AR0832_GROUP_HOLD_REG	0x0104
#define AR0832_TEST_RED_REG	0x0602
#define AR0832_TEST_GREENR_REG	0x0604
#define AR0832_TEST_BLUE_REG	0x0606
#define AR0832_TEST_GREENB_REG	0x0608



/* AR0832_RESET_REG */
#define AR0832_RESET_REG_GROUPED_PARAMETER_HOLD		(1 << 15)
#define AR0832_RESET_REG_GAIN_INSERT			(1 << 14)
#define AR0832_RESET_REG_SMIA_SERIALIZER_DIS		(1 << 12)
#define AR0832_RESET_REG_RESTART_BAD			(1 << 10)
#define AR0832_RESET_REG_MASK_BAD			(1 << 9)
#define AR0832_RESET_REG_GPI_EN				(1 << 8)
#define AR0832_RESET_REG_PARALLEL_EN			(1 << 7)
#define AR0832_RESET_REG_DRIVE_PINS			(1 << 6)
#define AR0832_RESET_REG_STDBY_EOF			(1 << 4)
#define AR0832_RESET_REG_LOCK_REG			(1 << 3)
#define AR0832_RESET_REG_STREAM				(1 << 2)
#define AR0832_RESET_REG_RESTART			(1 << 1)
#define AR0832_RESET_REG_RESET				(1 << 0)

static struct ar0832_reg mode_start[] = {
	{ar0832_TABLE_END, 0x0000}
};

static struct ar0832_reg mode_3264X2448_8140[] = {
	/* mode start */
	{0x301A, 0x0058},	/* RESET_REGISTER */
	{0x301A, 0x0050},	/* RESET_REGISTER */
	{0x0104, 0x0100},	/* GROUPED_PARAMETER_HOLD */
	{0x3064, 0x7800},	/* RESERVED_MFR_3064 */
	{0x31AE, 0x0202},	/* SERIAL_FORMAT */
	/* MT9E013 Recommended Settings */
	{0x31B0, 0x0083},	/* FRAME_PREAMBLE */
	{0x31B2, 0x004D},	/* LINE_PREAMBLE */
	{0x31B4, 0x0E77},	/* MIPI_TIMING_0 */
	{0x31B6, 0x0D20},	/* MIPI_TIMING_1 */
	{0x31B8, 0x020E},	/* MIPI_TIMING_2 */
	{0x31BA, 0x0710},	/* MIPI_TIMING_3 */
	{0x31BC, 0x2A0D},	/* MIPI_TIMING_4 */
	{ar0832_TABLE_WAIT_MS, 0x0005},
	{0x0112, 0x0A0A},	/* CCP_DATA_FORMAT */
	{0x3044, 0x0590},
	{0x306E, 0xFC80},
	{0x30B2, 0xC000},
	{0x30D6, 0x0800},
	{0x316C, 0xB42F},
	{0x316E, 0x869A},
	{0x3170, 0x210E},
	{0x317A, 0x010E},
	{0x31E0, 0x1FB9},
	{0x31E6, 0x07FC},
	{0x37C0, 0x0000},
	{0x37C2, 0x0000},
	{0x37C4, 0x0000},
	{0x37C6, 0x0000},
	{0x3E00, 0x0011},
	{0x3E02, 0x8801},
	{0x3E04, 0x2801},
	{0x3E06, 0x8449},
	{0x3E08, 0x6841},
	{0x3E0A, 0x400C},
	{0x3E0C, 0x1001},
	{0x3E0E, 0x2603},
	{0x3E10, 0x4B41},
	{0x3E12, 0x4B24},
	{0x3E14, 0xA3CF},
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
	{0x3E30, 0x0000},
	{0x3E32, 0x8801},
	{0x3E34, 0x4029},
	{0x3E36, 0x00FF},
	{0x3E38, 0x8469},
	{0x3E3A, 0x00FF},
	{0x3E3C, 0x2801},
	{0x3E3E, 0x3E2A},
	{0x3E40, 0x1C01},
	{0x3E42, 0xFF84},
	{0x3E44, 0x8401},
	{0x3E46, 0x0C01},
	{0x3E48, 0x8401},
	{0x3E4A, 0x00FF},
	{0x3E4C, 0x8402},
	{0x3E4E, 0x8984},
	{0x3E50, 0x6628},
	{0x3E52, 0x8340},
	{0x3E54, 0x00FF},
	{0x3E56, 0x4A42},
	{0x3E58, 0x2703},
	{0x3E5A, 0x6752},
	{0x3E5C, 0x3F2A},
	{0x3E5E, 0x846A},
	{0x3E60, 0x4C01},
	{0x3E62, 0x8401},
	{0x3E66, 0x3901},
	{0x3E90, 0x2C01},
	{0x3E98, 0x2B02},
	{0x3E92, 0x2A04},
	{0x3E94, 0x2509},
	{0x3E96, 0x0000},
	{0x3E9A, 0x2905},
	{0x3E9C, 0x00FF},
	{0x3ECC, 0x00EB},
	{0x3ED0, 0x1E24},
	{0x3ED4, 0xAFC4},
	{0x3ED6, 0x909B},
	{0x3EE0, 0x2424},
	{0x3EE4, 0xC100},
	{0x3EE6, 0x0540},
	{0x3174, 0x8000},

	/* mode end */
	{0x0300, 0x0004},	/* VT_PIX_CLK_DIV */
	{0x0302, 0x0001},	/* VT_SYS_CLK_DIV */
	{0x0304, 0x0002},	/* PRE_PLL_CLK_DIV */
	{0x0306, 0x0040},	/* PLL_MULTIPLIER */
	{0x0308, 0x000A},	/* OP_PIX_CLK_DIV */
	{0x030A, 0x0001},	/* OP_SYS_CLK_DIV */
	{ar0832_TABLE_WAIT_MS, 0x0001},
	{0x3064, 0x7400},	/* RESERVED_MFR_3064 */
	{0x0104, 0x0100},	/* GROUPED_PARAMETER_HOLD */
	{0x0344, 0x0004},	/* X_ADDR_START */
	{0x0348, 0x0CCB},	/* X_ADDR_END */
	{0x0346, 0x0004},	/* Y_ADDR_START */
	{0x034A, 0x099B},	/* Y_ADDR_END */
	{0x034C, 0x0CC8},	/* X_OUTPUT_SIZE */
	{0x034E, 0x0998},	/* Y_OUTPUT_SIZE */
	{0x3040, 0xC041},	/* READ_MODE */
	{0x306E, 0xFC80},	/* DATAPATH_SELECT */
	{0x0400, 0x0000},	/* SCALING_MODE */
	{0x0404, 0x0010},	/* SCALE_M */
	{0x3178, 0x0000},	/* RESERVED_MFR_3178 */
	{0x3ED0, 0x1E24},	/* RESERVED_MFR_3ED0 */
	{0x0400, 0x0000},	/* SCALING_MODE */
	{0x0404, 0x0010},	/* SCALE_M */
	{0x0342, 0x133C},	/* LINE_LENGTH_PCK */
	{0x0340, 0x0A27},	/* FRAME_LENGTH_LINES */
	{0x0202, 0x0A27},	/* COARSE_INTEGRATION_TIME */
	{0x3014, 0x09DC},	/* FINE_INTEGRATION_TIME */
	{0x3010, 0x0078},	/* FINE_CORRECTION */
	{0x301A, 0x8250},	/* RESET_REGISTER */
	{0x301A, 0x8650},	/* RESET_REGISTER */
	{0x301A, 0x8658},	/* RESET_REGISTER */
	/* gain */
	{0x305e, 0x10AA},	/* gain */
	{0x0104, 0x0000},	/* GROUPED_PARAMETER_HOLD */
	{0x301A, 0x065C},	/* RESET_REGISTER */
	{ar0832_TABLE_END, 0x0000}
};

static struct ar0832_reg mode_3264X2448_8141[] = {
	/* mode start */
	{0x301A, 0x0058},	/* RESET_REGISTER */
	{0x301A, 0x0050},	/* RESET_REGISTER */
	{0x0104, 0x0100},	/* GROUPED_PARAMETER_HOLD */
	{0x3064, 0x7800},	/* RESERVED_MFR_3064 */
	{0x31AE, 0x0202},	/* SERIAL_FORMAT */
	/* AR0832 Recommended Settings */
	{0x3044, 0x0590},
	{0x306E, 0xFC80},
	{0x30B2, 0xC000},
	{0x30D6, 0x0800},
	{0x316C, 0xB42A},
	{0x316E, 0x869C},
	{0x3170, 0x210E},
	{0x317A, 0x010E},
	{0x31E0, 0x1FB9},
	{0x31E6, 0x07FC},
	{0x37C0, 0x0000},
	{0x37C2, 0x0000},
	{0x37C4, 0x0000},
	{0x37C6, 0x0000},
	{0x3E00, 0x0011},
	{0x3E02, 0x8801},
	{0x3E04, 0x2801},
	{0x3E06, 0x8449},
	{0x3E08, 0x6841},
	{0x3E0A, 0x400C},
	{0x3E0C, 0x1001},
	{0x3E0E, 0x2603},
	{0x3E10, 0x4B41},
	{0x3E12, 0x4B24},
	{0x3E14, 0xA3CF},
	{0x3E16, 0x8802},
	{0x3E18, 0x8401},
	{0x3E1A, 0x8601},
	{0x3E1C, 0x8401},
	{0x3E1E, 0x840A},
	{0x3E20, 0xFF00},
	{0x3E22, 0x8401},
	{0x3E24, 0x00FF},
	{0x3E26, 0x0088},
	{0x3E28, 0x2E8A},
	{0x3E30, 0x0000},
	{0x3E32, 0x00FF},
	{0x3E34, 0x4029},
	{0x3E36, 0x00FF},
	{0x3E38, 0x8469},
	{0x3E3A, 0x00FF},
	{0x3E3C, 0x2801},
	{0x3E3E, 0x3E2A},
	{0x3E40, 0x1C01},
	{0x3E42, 0xFF84},
	{0x3E44, 0x8401},
	{0x3E46, 0x0C01},
	{0x3E48, 0x8401},
	{0x3E4A, 0x00FF},
	{0x3E4C, 0x8402},
	{0x3E4E, 0x8984},
	{0x3E50, 0x6628},
	{0x3E52, 0x8340},
	{0x3E54, 0x00FF},
	{0x3E56, 0x4A42},
	{0x3E58, 0x2703},
	{0x3E5A, 0x6752},
	{0x3E5C, 0x3F2A},
	{0x3E5E, 0x846A},
	{0x3E60, 0x4C01},
	{0x3E62, 0x8401},
	{0x3E66, 0x3901},
	{0x3E90, 0x2C01},
	{0x3E98, 0x2B02},
	{0x3E92, 0x2A04},
	{0x3E94, 0x2509},
	{0x3E96, 0xF000},
	{0x3E9A, 0x2905},
	{0x3E9C, 0x00FF},
	{0x3ECC, 0x00EB},
	{0x3ED0, 0x1E24},
	{0x3ED4, 0xFAA4},
	{0x3ED6, 0x909B},
	{0x3EE0, 0x2424},
	{0x3EE4, 0xC100},
	{0x3EE6, 0x0540},
	{0x3174, 0x8000},

	/* mode end */
	{0x0300, 0x0004},	/* VT_PIX_CLK_DIV */
	{0x0302, 0x0001},	/* VT_SYS_CLK_DIV */
	{0x0304, 0x0002},	/* PRE_PLL_CLK_DIV */
	{0x0306, 0x0040},	/* PLL_MULTIPLIER */
	{0x0308, 0x000A},	/* OP_PIX_CLK_DIV */
	{0x030A, 0x0001},	/* OP_SYS_CLK_DIV */
	{ar0832_TABLE_WAIT_MS, 0x0001},
	{0x3064, 0x7400},	/* RESERVED_MFR_3064 */
	{0x0104, 0x0100},	/* GROUPED_PARAMETER_HOLD */
	{0x0344, 0x0004},	/* X_ADDR_START */
	{0x0348, 0x0CCB},	/* X_ADDR_END */
	{0x0346, 0x0004},	/* Y_ADDR_START */
	{0x034A, 0x099B},	/* Y_ADDR_END */
	{0x034C, 0x0CC8},	/* X_OUTPUT_SIZE */
	{0x034E, 0x0998},	/* Y_OUTPUT_SIZE */
	{0x3040, 0xC041},	/* READ_MODE */
	{0x306E, 0xFC80},	/* DATAPATH_SELECT */
	{0x0400, 0x0000},	/* SCALING_MODE */
	{0x0404, 0x0010},	/* SCALE_M */
	{0x3178, 0x0000},	/* RESERVED_MFR_3178 */
	{0x3ED0, 0x1E24},	/* RESERVED_MFR_3ED0 */
	{0x0400, 0x0000},	/* SCALING_MODE */
	{0x0404, 0x0010},	/* SCALE_M */
	{0x0342, 0x133C},	/* LINE_LENGTH_PCK */
	{0x0340, 0x0A27},	/* FRAME_LENGTH_LINES */
	{0x0202, 0x0A27},	/* COARSE_INTEGRATION_TIME */
	{0x3014, 0x09DC},	/* FINE_INTEGRATION_TIME */
	{0x3010, 0x0078},	/* FINE_CORRECTION */
	{0x301A, 0x8250},	/* RESET_REGISTER */
	{0x301A, 0x8650},	/* RESET_REGISTER */
	{0x301A, 0x8658},	/* RESET_REGISTER */
	/* gain */
	{0x305e, 0x10AA},	/* gain */
	{0x0104, 0x0000},	/* GROUPED_PARAMETER_HOLD */
	{0x301A, 0x065C},	/* RESET_REGISTER */
	{ar0832_TABLE_END, 0x0000}
};

static struct ar0832_reg mode_2880X1620_8140[] = {
	/* mode start */
	{0x301A, 0x0058},	/* RESET_REGISTER */
	{0x301A, 0x0050},	/* RESET_REGISTER */
	{0x0104, 0x0100},	/* GROUPED_PARAMETER_HOLD */
	{0x3064, 0x7800},	/* RESERVED_MFR_3064 */
	{0x31AE, 0x0202},	/* SERIAL_FORMAT */
	/* MT9E013 Recommended Settings */
	{0x31B0, 0x0083},	/* FRAME_PREAMBLE */
	{0x31B2, 0x004D},	/* LINE_PREAMBLE */
	{0x31B4, 0x0E77},	/* MIPI_TIMING_0 */
	{0x31B6, 0x0D20},	/* MIPI_TIMING_1 */
	{0x31B8, 0x020E},	/* MIPI_TIMING_2 */
	{0x31BA, 0x0710},	/* MIPI_TIMING_3 */
	{0x31BC, 0x2A0D},	/* MIPI_TIMING_4 */
	{ar0832_TABLE_WAIT_MS, 0x0005},
	{0x0112, 0x0A0A},	/* CCP_DATA_FORMAT */
	{0x3044, 0x0590},
	{0x306E, 0xFC80},
	{0x30B2, 0xC000},
	{0x30D6, 0x0800},
	{0x316C, 0xB42F},
	{0x316E, 0x869A},
	{0x3170, 0x210E},
	{0x317A, 0x010E},
	{0x31E0, 0x1FB9},
	{0x31E6, 0x07FC},
	{0x37C0, 0x0000},
	{0x37C2, 0x0000},
	{0x37C4, 0x0000},
	{0x37C6, 0x0000},
	{0x3E00, 0x0011},
	{0x3E02, 0x8801},
	{0x3E04, 0x2801},
	{0x3E06, 0x8449},
	{0x3E08, 0x6841},
	{0x3E0A, 0x400C},
	{0x3E0C, 0x1001},
	{0x3E0E, 0x2603},
	{0x3E10, 0x4B41},
	{0x3E12, 0x4B24},
	{0x3E14, 0xA3CF},
	{0x3E16, 0x8802},
	{0x3E18, 0x8401},
	{0x3E1A, 0x8601},
	{0x3E1C, 0x8401},
	{0x3E1E, 0x840A},
	{0x3E20, 0xFF00},
	{0x3E22, 0x8401},
	{0x3E24, 0x00FF},
	{0x3E26, 0x0088},
	{0x3E28, 0x2E8A},
	{0x3E30, 0x0000},
	{0x3E32, 0x8801},
	{0x3E34, 0x4029},
	{0x3E36, 0x00FF},
	{0x3E38, 0x8469},
	{0x3E3A, 0x00FF},
	{0x3E3C, 0x2801},
	{0x3E3E, 0x3E2A},
	{0x3E40, 0x1C01},
	{0x3E42, 0xFF84},
	{0x3E44, 0x8401},
	{0x3E46, 0x0C01},
	{0x3E48, 0x8401},
	{0x3E4A, 0x00FF},
	{0x3E4C, 0x8402},
	{0x3E4E, 0x8984},
	{0x3E50, 0x6628},
	{0x3E52, 0x8340},
	{0x3E54, 0x00FF},
	{0x3E56, 0x4A42},
	{0x3E58, 0x2703},
	{0x3E5A, 0x6752},
	{0x3E5C, 0x3F2A},
	{0x3E5E, 0x846A},
	{0x3E60, 0x4C01},
	{0x3E62, 0x8401},
	{0x3E66, 0x3901},
	{0x3E90, 0x2C01},
	{0x3E98, 0x2B02},
	{0x3E92, 0x2A04},
	{0x3E94, 0x2509},
	{0x3E96, 0x0000},
	{0x3E9A, 0x2905},
	{0x3E9C, 0x00FF},
	{0x3ECC, 0x00EB},
	{0x3ED0, 0x1E24},
	{0x3ED4, 0xAFC4},
	{0x3ED6, 0x909B},
	{0x3EE0, 0x2424},
	{0x3EE2, 0x9797},
	{0x3EE4, 0xC100},
	{0x3EE6, 0x0540},
	{0x3174, 0x8000},

	/* mode end */
	{0x0300, 0x0004},	/* VT_PIX_CLK_DIV */
	{0x0302, 0x0001},	/* VT_SYS_CLK_DIV */
	{0x0304, 0x0002},	/* PRE_PLL_CLK_DIV */
	{0x0306, 0x0040},	/* PLL_MULTIPLIER */
	{0x0308, 0x000A},	/* OP_PIX_CLK_DIV */
	{0x030A, 0x0001},	/* OP_SYS_CLK_DIV */
	{ar0832_TABLE_WAIT_MS, 0x0001},
	{0x3064, 0x7400},	/* RESERVED_MFR_3064 */
	{0x0104, 0x0100},	/* GROUPED_PARAMETER_HOLD */
	{0x0344, 0x00C8},	/* X_ADDR_START */
	{0x0348, 0x0C07},	/* X_ADDR_END */
	{0x0346, 0x01A6},	/* Y_ADDR_START */
	{0x034A, 0x07F9},	/* Y_ADDR_END */
	{0x034C, 0x0B40},	/* X_OUTPUT_SIZE */
	{0x034E, 0x0654},	/* Y_OUTPUT_SIZE */
	{0x3040, 0xC041},	/* READ_MODE */
	{0x306E, 0xFC80},	/* DATAPATH_SELECT */
	{0x0400, 0x0000},	/* SCALING_MODE */
	{0x0404, 0x0010},	/* SCALE_M */
	{0x3178, 0x0000},	/* RESERVED_MFR_3178 */
	{0x3ED0, 0x1E24},	/* RESERVED_MFR_3ED0 */

	{0x0342, 0x11B8},	/* LINE_LENGTH_PCK */
	{0x0340, 0x06E3},	/* FRAME_LENGTH_LINES */
	{0x0202, 0x06E3},	/* COARSE_INTEGRATION_TIME */
	{0x3014, 0x0BD8},	/* FINE_INTEGRATION_TIME */
	{0x3010, 0x0078},	/* FINE_CORRECTION */
	{0x301A, 0x8250},	/* RESET_REGISTER */
	{0x301A, 0x8650},	/* RESET_REGISTER */
	{0x301A, 0x8658},	/* RESET_REGISTER */
	/* gain */
	{0x305e, 0x10AA},	/* gain */
	{0x0104, 0x0000},	/* GROUPED_PARAMETER_HOLD */
	{0x301A, 0x065C},	/* RESET_REGISTER */
	{ar0832_TABLE_END, 0x0000}
};

static struct ar0832_reg mode_2880X1620_8141[] = {
	/* mode start */
	{0x301A, 0x0058},	/* RESET_REGISTER */
	{0x301A, 0x0050},	/* RESET_REGISTER */
	{0x0104, 0x0100},	/* GROUPED_PARAMETER_HOLD */
	{0x3064, 0x7800},	/* RESERVED_MFR_3064 */
	{0x31AE, 0x0202},	/* SERIAL_FORMAT */
	/* AR0832 Recommended Settings */
	{0x3044, 0x0590},
	{0x306E, 0xFC80},
	{0x30B2, 0xC000},
	{0x30D6, 0x0800},
	{0x316C, 0xB42F},
	{0x316E, 0x869A},
	{0x3170, 0x210E},
	{0x317A, 0x010E},
	{0x31E0, 0x1FB9},
	{0x31E6, 0x07FC},
	{0x37C0, 0x0000},
	{0x37C2, 0x0000},
	{0x37C4, 0x0000},
	{0x37C6, 0x0000},
	{0x3E00, 0x0011},
	{0x3E02, 0x8801},
	{0x3E04, 0x2801},
	{0x3E06, 0x8449},
	{0x3E08, 0x6841},
	{0x3E0A, 0x400C},
	{0x3E0C, 0x1001},
	{0x3E0E, 0x2603},
	{0x3E10, 0x4B41},
	{0x3E12, 0x4B24},
	{0x3E14, 0xA3CF},
	{0x3E16, 0x8802},
	{0x3E18, 0x8401},
	{0x3E1A, 0x8601},
	{0x3E1C, 0x8401},
	{0x3E1E, 0x840A},
	{0x3E20, 0xFF00},
	{0x3E22, 0x8401},
	{0x3E24, 0x00FF},
	{0x3E26, 0x0088},
	{0x3E28, 0x2E8A},
	{0x3E30, 0x0000},
	{0x3E32, 0x8801},
	{0x3E34, 0x4029},
	{0x3E36, 0x00FF},
	{0x3E38, 0x8469},
	{0x3E3A, 0x00FF},
	{0x3E3C, 0x2801},
	{0x3E3E, 0x3E2A},
	{0x3E40, 0x1C01},
	{0x3E42, 0xFF84},
	{0x3E44, 0x8401},
	{0x3E46, 0x0C01},
	{0x3E48, 0x8401},
	{0x3E4A, 0x00FF},
	{0x3E4C, 0x8402},
	{0x3E4E, 0x8984},
	{0x3E50, 0x6628},
	{0x3E52, 0x8340},
	{0x3E54, 0x00FF},
	{0x3E56, 0x4A42},
	{0x3E58, 0x2703},
	{0x3E5A, 0x6752},
	{0x3E5C, 0x3F2A},
	{0x3E5E, 0x846A},
	{0x3E60, 0x4C01},
	{0x3E62, 0x8401},
	{0x3E66, 0x3901},
	{0x3E90, 0x2C01},
	{0x3E98, 0x2B02},
	{0x3E92, 0x2A04},
	{0x3E94, 0x2509},
	{0x3E96, 0x0000},
	{0x3E9A, 0x2905},
	{0x3E9C, 0x00FF},
	{0x3ECC, 0x00EB},
	{0x3ED0, 0x1E24},
	{0x3ED4, 0xAFC4},
	{0x3ED6, 0x909B},
	{0x3EE0, 0x2424},
	{0x3EE2, 0x9797},
	{0x3EE4, 0xC100},
	{0x3EE6, 0x0540},
	{0x3174, 0x8000},

	/* mode end */
	{0x0300, 0x0004},	/* VT_PIX_CLK_DIV */
	{0x0302, 0x0001},	/* VT_SYS_CLK_DIV */
	{0x0304, 0x0002},	/* PRE_PLL_CLK_DIV */
	{0x0306, 0x0040},	/* PLL_MULTIPLIER */
	{0x0308, 0x000A},	/* OP_PIX_CLK_DIV */
	{0x030A, 0x0001},	/* OP_SYS_CLK_DIV */
	{ar0832_TABLE_WAIT_MS, 0x0001},
	{0x3064, 0x7400},	/* RESERVED_MFR_3064 */
	{0x0104, 0x0100},	/* GROUPED_PARAMETER_HOLD */
	{0x0344, 0x00C8},	/* X_ADDR_START */
	{0x0348, 0x0C07},	/* X_ADDR_END */
	{0x0346, 0x01A6},	/* Y_ADDR_START */
	{0x034A, 0x07F9},	/* Y_ADDR_END */
	{0x034C, 0x0B40},	/* X_OUTPUT_SIZE */
	{0x034E, 0x0654},	/* Y_OUTPUT_SIZE */
	{0x3040, 0xC041},	/* READ_MODE */
	{0x306E, 0xFC80},	/* DATAPATH_SELECT */
	{0x0400, 0x0000},	/* SCALING_MODE */
	{0x0404, 0x0010},	/* SCALE_M */
	{0x3178, 0x0000},	/* RESERVED_MFR_3178 */
	{0x3ED0, 0x1E24},	/* RESERVED_MFR_3ED0 */

	{0x0342, 0x11B8},	/* LINE_LENGTH_PCK */
	{0x0340, 0x06E3},	/* FRAME_LENGTH_LINES */
	{0x0202, 0x06E3},	/* COARSE_INTEGRATION_TIME */
	{0x3014, 0x0BD8},	/* FINE_INTEGRATION_TIME */
	{0x3010, 0x0078},	/* FINE_CORRECTION */
	{0x301A, 0x8250},	/* RESET_REGISTER */
	{0x301A, 0x8650},	/* RESET_REGISTER */
	{0x301A, 0x8658},	/* RESET_REGISTER */
	/* gain */
	{0x305e, 0x10AA},	/* gain */
	{0x0104, 0x0000},	/* GROUPED_PARAMETER_HOLD */
	{0x301A, 0x065C},	/* RESET_REGISTER */
	{ar0832_TABLE_END, 0x0000}
};

static struct ar0832_reg mode_1920X1080_8140[] = {
	/* mode start */
	{0x301A, 0x0058},	/* RESET_REGISTER */
	{0x301A, 0x0050},	/* RESET_REGISTER */
	{0x0104, 0x0100},	/* GROUPED_PARAMETER_HOLD */
	{0x3064, 0x7800},	/* RESERVED_MFR_3064 */
	{0x31AE, 0x0202},	/* SERIAL_FORMAT */
	/* MT9E013 Recommended Settings */
	{0x31B0, 0x0083},	/* FRAME_PREAMBLE */
	{0x31B2, 0x004D},	/* LINE_PREAMBLE */
	{0x31B4, 0x0E77},	/* MIPI_TIMING_0 */
	{0x31B6, 0x0D20},	/* MIPI_TIMING_1 */
	{0x31B8, 0x020E},	/* MIPI_TIMING_2 */
	{0x31BA, 0x0710},	/* MIPI_TIMING_3 */
	{0x31BC, 0x2A0D},	/* MIPI_TIMING_4 */
	{ar0832_TABLE_WAIT_MS, 0x0005},
	{0x0112, 0x0A0A},	/* CCP_DATA_FORMAT */
	{0x3044, 0x0590},
	{0x306E, 0xFC80},
	{0x30B2, 0xC000},
	{0x30D6, 0x0800},
	{0x316C, 0xB42F},
	{0x316E, 0x869A},
	{0x3170, 0x210E},
	{0x317A, 0x010E},
	{0x31E0, 0x1FB9},
	{0x31E6, 0x07FC},
	{0x37C0, 0x0000},
	{0x37C2, 0x0000},
	{0x37C4, 0x0000},
	{0x37C6, 0x0000},
	{0x3E00, 0x0011},
	{0x3E02, 0x8801},
	{0x3E04, 0x2801},
	{0x3E06, 0x8449},
	{0x3E08, 0x6841},
	{0x3E0A, 0x400C},
	{0x3E0C, 0x1001},
	{0x3E0E, 0x2603},
	{0x3E10, 0x4B41},
	{0x3E12, 0x4B24},
	{0x3E14, 0xA3CF},
	{0x3E16, 0x8802},
	{0x3E18, 0x8401},
	{0x3E1A, 0x8601},
	{0x3E1C, 0x8401},
	{0x3E1E, 0x840A},
	{0x3E20, 0xFF00},
	{0x3E22, 0x8401},
	{0x3E24, 0x00FF},
	{0x3E26, 0x0088},
	{0x3E28, 0x2E8A},
	{0x3E30, 0x0000},
	{0x3E32, 0x8801},
	{0x3E34, 0x4029},
	{0x3E36, 0x00FF},
	{0x3E38, 0x8469},
	{0x3E3A, 0x00FF},
	{0x3E3C, 0x2801},
	{0x3E3E, 0x3E2A},
	{0x3E40, 0x1C01},
	{0x3E42, 0xFF84},
	{0x3E44, 0x8401},
	{0x3E46, 0x0C01},
	{0x3E48, 0x8401},
	{0x3E4A, 0x00FF},
	{0x3E4C, 0x8402},
	{0x3E4E, 0x8984},
	{0x3E50, 0x6628},
	{0x3E52, 0x8340},
	{0x3E54, 0x00FF},
	{0x3E56, 0x4A42},
	{0x3E58, 0x2703},
	{0x3E5A, 0x6752},
	{0x3E5C, 0x3F2A},
	{0x3E5E, 0x846A},
	{0x3E60, 0x4C01},
	{0x3E62, 0x8401},
	{0x3E66, 0x3901},
	{0x3E90, 0x2C01},
	{0x3E98, 0x2B02},
	{0x3E92, 0x2A04},
	{0x3E94, 0x2509},
	{0x3E96, 0x0000},
	{0x3E9A, 0x2905},
	{0x3E9C, 0x00FF},
	{0x3ECC, 0x00EB},
	{0x3ED0, 0x1E24},
	{0x3ED4, 0xAFC4},
	{0x3ED6, 0x909B},
	{0x3EE0, 0x2424},
	{0x3EE2, 0x9797},
	{0x3EE4, 0xC100},
	{0x3EE6, 0x0540},
	{0x3174, 0x8000},

	/* mode end */
	{0x0300, 0x0004},	/* VT_PIX_CLK_DIV */
	{0x0302, 0x0001},	/* VT_SYS_CLK_DIV */
	{0x0304, 0x0002},	/* PRE_PLL_CLK_DIV */
	{0x0306, 0x0040},	/* PLL_MULTIPLIER */
	{0x0308, 0x000A},	/* OP_PIX_CLK_DIV */
	{0x030A, 0x0001},	/* OP_SYS_CLK_DIV */
	{ar0832_TABLE_WAIT_MS, 0x0001},
	{0x3064, 0x7400},	/* RESERVED_MFR_3064 */
	{0x0104, 0x0100},	/* GROUPED_PARAMETER_HOLD */
	{0x0344, 0x028C},	/* X_ADDR_START */
	{0x0348, 0x0A0B},	/* X_ADDR_END */
	{0x0346, 0x006E},	/* Y_ADDR_START */
	{0x034A, 0x04A5},	/* Y_ADDR_END */
	{0x034C, 0x0780},	/* X_OUTPUT_SIZE */
	{0x034E, 0x0438},	/* Y_OUTPUT_SIZE */
	{0x3040, 0xC041},	/* READ_MODE */
	{0x306E, 0xFC80},	/* DATAPATH_SELECT */
	{0x0400, 0x0000},	/* SCALING_MODE */
	{0x0404, 0x0010},	/* SCALE_M */
	{0x3178, 0x0000},	/* RESERVED_MFR_3178 */
	{0x3ED0, 0x1E24},	/* RESERVED_MFR_3ED0 */

	{0x0342, 0x1139},	/* LINE_LENGTH_PCK */
	{0x0340, 0x05C4},	/* FRAME_LENGTH_LINES */
	{0x0202, 0x05C4},	/* COARSE_INTEGRATION_TIME */
	{0x3014, 0x0702},	/* FINE_INTEGRATION_TIME */
	{0x3010, 0x0078},	/* FINE_CORRECTION */
	{0x301A, 0x8250},	/* RESET_REGISTER */
	{0x301A, 0x8650},	/* RESET_REGISTER */
	{0x301A, 0x8658},	/* RESET_REGISTER */
	/* gain */
	{0x305e, 0x10AA},	/* gain */
	{0x0104, 0x0000},	/* GROUPED_PARAMETER_HOLD */
	{0x301A, 0x065C},	/* RESET_REGISTER */
	{ar0832_TABLE_END, 0x0000}
};

static struct ar0832_reg mode_1920X1080_8141[] = {
	/* mode start */
	{0x301A, 0x0058},	/* RESET_REGISTER */
	{0x301A, 0x0050},	/* RESET_REGISTER */
	{0x0104, 0x0100},	/* GROUPED_PARAMETER_HOLD */
	{0x3064, 0x7800},	/* RESERVED_MFR_3064 */
	{0x31AE, 0x0202},	/* SERIAL_FORMAT */
	/* AR0832 Recommended Settings */
	{0x3044, 0x0590},
	{0x306E, 0xFC80},
	{0x30B2, 0xC000},
	{0x30D6, 0x0800},
	{0x316C, 0xB42F},
	{0x316E, 0x869A},
	{0x3170, 0x210E},
	{0x317A, 0x010E},
	{0x31E0, 0x1FB9},
	{0x31E6, 0x07FC},
	{0x37C0, 0x0000},
	{0x37C2, 0x0000},
	{0x37C4, 0x0000},
	{0x37C6, 0x0000},
	{0x3E00, 0x0011},
	{0x3E02, 0x8801},
	{0x3E04, 0x2801},
	{0x3E06, 0x8449},
	{0x3E08, 0x6841},
	{0x3E0A, 0x400C},
	{0x3E0C, 0x1001},
	{0x3E0E, 0x2603},
	{0x3E10, 0x4B41},
	{0x3E12, 0x4B24},
	{0x3E14, 0xA3CF},
	{0x3E16, 0x8802},
	{0x3E18, 0x8401},
	{0x3E1A, 0x8601},
	{0x3E1C, 0x8401},
	{0x3E1E, 0x840A},
	{0x3E20, 0xFF00},
	{0x3E22, 0x8401},
	{0x3E24, 0x00FF},
	{0x3E26, 0x0088},
	{0x3E28, 0x2E8A},
	{0x3E30, 0x0000},
	{0x3E32, 0x8801},
	{0x3E34, 0x4029},
	{0x3E36, 0x00FF},
	{0x3E38, 0x8469},
	{0x3E3A, 0x00FF},
	{0x3E3C, 0x2801},
	{0x3E3E, 0x3E2A},
	{0x3E40, 0x1C01},
	{0x3E42, 0xFF84},
	{0x3E44, 0x8401},
	{0x3E46, 0x0C01},
	{0x3E48, 0x8401},
	{0x3E4A, 0x00FF},
	{0x3E4C, 0x8402},
	{0x3E4E, 0x8984},
	{0x3E50, 0x6628},
	{0x3E52, 0x8340},
	{0x3E54, 0x00FF},
	{0x3E56, 0x4A42},
	{0x3E58, 0x2703},
	{0x3E5A, 0x6752},
	{0x3E5C, 0x3F2A},
	{0x3E5E, 0x846A},
	{0x3E60, 0x4C01},
	{0x3E62, 0x8401},
	{0x3E66, 0x3901},
	{0x3E90, 0x2C01},
	{0x3E98, 0x2B02},
	{0x3E92, 0x2A04},
	{0x3E94, 0x2509},
	{0x3E96, 0x0000},
	{0x3E9A, 0x2905},
	{0x3E9C, 0x00FF},
	{0x3ECC, 0x00EB},
	{0x3ED0, 0x1E24},
	{0x3ED4, 0xAFC4},
	{0x3ED6, 0x909B},
	{0x3EE0, 0x2424},
	{0x3EE2, 0x9797},
	{0x3EE4, 0xC100},
	{0x3EE6, 0x0540},
	{0x3174, 0x8000},

	/* mode end */
	{0x0300, 0x0004},	/* VT_PIX_CLK_DIV */
	{0x0302, 0x0001},	/* VT_SYS_CLK_DIV */
	{0x0304, 0x0002},	/* PRE_PLL_CLK_DIV */
	{0x0306, 0x0040},	/* PLL_MULTIPLIER */
	{0x0308, 0x000A},	/* OP_PIX_CLK_DIV */
	{0x030A, 0x0001},	/* OP_SYS_CLK_DIV */
	{ar0832_TABLE_WAIT_MS, 0x0001},
	{0x3064, 0x7400},	/* RESERVED_MFR_3064 */
	{0x0104, 0x0100},	/* GROUPED_PARAMETER_HOLD */
	{0x0344, 0x028C},	/* X_ADDR_START */
	{0x0348, 0x0A0B},	/* X_ADDR_END */
	{0x0346, 0x006E},	/* Y_ADDR_START */
	{0x034A, 0x04A5},	/* Y_ADDR_END */
	{0x034C, 0x0780},	/* X_OUTPUT_SIZE */
	{0x034E, 0x0438},	/* Y_OUTPUT_SIZE */
	{0x3040, 0xC041},	/* READ_MODE */
	{0x306E, 0xFC80},	/* DATAPATH_SELECT */
	{0x0400, 0x0000},	/* SCALING_MODE */
	{0x0404, 0x0010},	/* SCALE_M */
	{0x3178, 0x0000},	/* RESERVED_MFR_3178 */
	{0x3ED0, 0x1E24},	/* RESERVED_MFR_3ED0 */

	{0x0342, 0x1139},	/* LINE_LENGTH_PCK */
	{0x0340, 0x05C4},	/* FRAME_LENGTH_LINES */
	{0x0202, 0x05C4},	/* COARSE_INTEGRATION_TIME */
	{0x3014, 0x0702},	/* FINE_INTEGRATION_TIME */
	{0x3010, 0x0078},	/* FINE_CORRECTION */
	{0x301A, 0x8250},	/* RESET_REGISTER */
	{0x301A, 0x8650},	/* RESET_REGISTER */
	{0x301A, 0x8658},	/* RESET_REGISTER */
	/* gain */
	{0x305e, 0x10AA},	/* gain */
	{0x0104, 0x0000},	/* GROUPED_PARAMETER_HOLD */
	{0x301A, 0x065C},	/* RESET_REGISTER */
	{ar0832_TABLE_END, 0x0000}
};

static struct ar0832_reg mode_1632X1224_8140[] = {
	/* mode start */
	{0x301A, 0x0058},	/* RESET_REGISTER */
	{0x301A, 0x0050},	/* RESET_REGISTER */

	/* SC-CHANGE: to-do 8 bit write */
	{0x0104, 0x0100},	/* GROUPED_PARAMETER_HOLD */

	{0x3064, 0x7800},	/* RESERVED_MFR_3064 */
	{0x31AE, 0x0202},	/* SERIAL_FORMAT */
	/* MT9E013 Recommended Settings */
	{0x31B0, 0x0083},	/* FRAME_PREAMBLE */
	{0x31B2, 0x004D},	/* LINE_PREAMBLE */
	{0x31B4, 0x0E77},	/* MIPI_TIMING_0 */
	{0x31B6, 0x0D20},	/* MIPI_TIMING_1 */
	{0x31B8, 0x020E},	/* MIPI_TIMING_2 */
	{0x31BA, 0x0710},	/* MIPI_TIMING_3 */
	{0x31BC, 0x2A0D},	/* MIPI_TIMING_4 */
	{ar0832_TABLE_WAIT_MS, 0x0005},
	{0x0112, 0x0A0A},	/* CCP_DATA_FORMAT */
	{0x3044, 0x0590},
	{0x306E, 0xFC80},
	{0x30B2, 0xC000},
	{0x30D6, 0x0800},
	{0x316C, 0xB42F},
	{0x316E, 0x869A},
	{0x3170, 0x210E},
	{0x317A, 0x010E},
	{0x31E0, 0x1FB9},
	{0x31E6, 0x07FC},
	{0x37C0, 0x0000},
	{0x37C2, 0x0000},
	{0x37C4, 0x0000},
	{0x37C6, 0x0000},
	{0x3E00, 0x0011},
	{0x3E02, 0x8801},
	{0x3E04, 0x2801},
	{0x3E06, 0x8449},
	{0x3E08, 0x6841},
	{0x3E0A, 0x400C},
	{0x3E0C, 0x1001},
	{0x3E0E, 0x2603},
	{0x3E10, 0x4B41},
	{0x3E12, 0x4B24},
	{0x3E14, 0xA3CF},
	{0x3E16, 0x8802},
	{0x3E18, 0x8401},
	{0x3E1A, 0x8601},
	{0x3E1C, 0x8401},
	{0x3E1E, 0x840A},
	{0x3E20, 0xFF00},
	{0x3E22, 0x8401},
	{0x3E24, 0x00FF},
	{0x3E26, 0x0088},
	{0x3E28, 0x2E8A},
	{0x3E30, 0x0000},
	{0x3E32, 0x8801},
	{0x3E34, 0x4029},
	{0x3E36, 0x00FF},
	{0x3E38, 0x8469},
	{0x3E3A, 0x00FF},
	{0x3E3C, 0x2801},
	{0x3E3E, 0x3E2A},
	{0x3E40, 0x1C01},
	{0x3E42, 0xFF84},
	{0x3E44, 0x8401},
	{0x3E46, 0x0C01},
	{0x3E48, 0x8401},
	{0x3E4A, 0x00FF},
	{0x3E4C, 0x8402},
	{0x3E4E, 0x8984},
	{0x3E50, 0x6628},
	{0x3E52, 0x8340},
	{0x3E54, 0x00FF},
	{0x3E56, 0x4A42},
	{0x3E58, 0x2703},
	{0x3E5A, 0x6752},
	{0x3E5C, 0x3F2A},
	{0x3E5E, 0x846A},
	{0x3E60, 0x4C01},
	{0x3E62, 0x8401},
	{0x3E66, 0x3901},
	{0x3E90, 0x2C01},
	{0x3E98, 0x2B02},
	{0x3E92, 0x2A04},
	{0x3E94, 0x2509},
	{0x3E96, 0x0000},
	{0x3E9A, 0x2905},
	{0x3E9C, 0x00FF},
	{0x3ECC, 0x00EB},
	{0x3ED0, 0x1E24},
	{0x3ED4, 0xAFC4},
	{0x3ED6, 0x909B},
	{0x3EE0, 0x2424},
	{0x3EE2, 0x9797},
	{0x3EE4, 0xC100},
	{0x3EE6, 0x0540},
	{0x3174, 0x8000},

	/* mode end */
	{0x0300, 0x0004},	/* VT_PIX_CLK_DIV */
	{0x0302, 0x0001},	/* VT_SYS_CLK_DIV */
	{0x0304, 0x0002},	/* PRE_PLL_CLK_DIV */

	{0x0306, 0x0040},	/* PLL_MULTIPLIER */

	{0x0308, 0x000A},	/* OP_PIX_CLK_DIV */
	{0x030A, 0x0001},	/* OP_SYS_CLK_DIV */
	{ar0832_TABLE_WAIT_MS, 0x0001}, /* waitmsec 1 */

	{0x3064, 0x7400},	/* RESERVED_MFR_3064 */

	{0x0104, 0x0100},	/* GROUPED_PARAMETER_HOLD */

	{0x0344, 0x0008},	/* X_ADDR_START */
	{0x0348, 0x0CC9},	/* X_ADDR_END */
	{0x0346, 0x0008},	/* Y_ADDR_START */
	{0x034A, 0x0999},	/* Y_ADDR_END */
	{0x034C, 0x0660},	/* X_OUTPUT_SIZE */
	{0x034E, 0x04C8},	/* Y_OUTPUT_SIZE */
	{0x3040, 0xC4C3},	/* READ_MODE */
	{0x306E, 0xFC80},	/* DATAPATH_SELECT */
	{0x3178, 0x0000},	/* RESERVED_MFR_3178 */
	{0x3ED0, 0x1E24},	/* RESERVED_MFR_3ED0 */
	{0x0400, 0x0002},	/* SCALING_MODE */
	{0x0404, 0x0010},	/* SCALE_M */
	{0x0342, 0x101A},	/* LINE_LENGTH_PCK */
	{0x0340, 0x0610},	/* FRAME_LENGTH_LINES */
	{0x0202, 0x0557},	/* COARSE_INTEGRATION_TIME */
	{0x3014, 0x0988},	/* FINE_INTEGRATION_TIME */
	{0x3010, 0x0130},	/* FINE_CORRECTION */
	{0x301A, 0x8250},	/* RESET_REGISTER */
	{0x301A, 0x8650},	/* RESET_REGISTER */
	{0x301A, 0x8658},	/* RESET_REGISTER */

	/* gain */
	{0x305e, 0x10AA},	/* gain */

	/* todo 8-bit write */
	{0x0104, 0x0000},	/* GROUPED_PARAMETER_HOLD */
	{0x301A, 0x065C},	/* RESET_REGISTER */
	{ar0832_TABLE_END, 0x0000}
};

static struct ar0832_reg mode_1632X1224_8141[] = {
	/* mode start */
	{0x301A, 0x0058},	/* RESET_REGISTER */
	{0x301A, 0x0050},	/* RESET_REGISTER */

	/* SC-CHANGE: to-do 8 bit write */
	{0x0104, 0x0100},	/* GROUPED_PARAMETER_HOLD */

	{0x3064, 0x7800},	/* RESERVED_MFR_3064 */
	{0x31AE, 0x0202},	/* SERIAL_FORMAT */
	/* AR0832 Recommended Settings */
	{0x31B0, 0x0083},	/* FRAME_PREAMBLE */
	{0x31B2, 0x004D},	/* LINE_PREAMBLE */
	{0x31B4, 0x0E77},	/* MIPI_TIMING_0 */
	{0x31B6, 0x0D20},	/* MIPI_TIMING_1 */
	{0x31B8, 0x020E},	/* MIPI_TIMING_2 */
	{0x31BA, 0x0710},	/* MIPI_TIMING_3 */
	{0x31BC, 0x2A0D},	/* MIPI_TIMING_4 */
	{ar0832_TABLE_WAIT_MS, 0x0005},
	{0x0112, 0x0A0A},	/* CCP_DATA_FORMAT */
	{0x3044, 0x0590},
	{0x306E, 0xFC80},
	{0x30B2, 0xC000},
	{0x30D6, 0x0800},
	{0x316C, 0xB42F},
	{0x316E, 0x869A},
	{0x3170, 0x210E},
	{0x317A, 0x010E},
	{0x31E0, 0x1FB9},
	{0x31E6, 0x07FC},
	{0x37C0, 0x0000},
	{0x37C2, 0x0000},
	{0x37C4, 0x0000},
	{0x37C6, 0x0000},
	{0x3E00, 0x0011},
	{0x3E02, 0x8801},
	{0x3E04, 0x2801},
	{0x3E06, 0x8449},
	{0x3E08, 0x6841},
	{0x3E0A, 0x400C},
	{0x3E0C, 0x1001},
	{0x3E0E, 0x2603},
	{0x3E10, 0x4B41},
	{0x3E12, 0x4B24},
	{0x3E14, 0xA3CF},
	{0x3E16, 0x8802},
	{0x3E18, 0x8401},
	{0x3E1A, 0x8601},
	{0x3E1C, 0x8401},
	{0x3E1E, 0x840A},
	{0x3E20, 0xFF00},
	{0x3E22, 0x8401},
	{0x3E24, 0x00FF},
	{0x3E26, 0x0088},
	{0x3E28, 0x2E8A},
	{0x3E30, 0x0000},
	{0x3E32, 0x8801},
	{0x3E34, 0x4029},
	{0x3E36, 0x00FF},
	{0x3E38, 0x8469},
	{0x3E3A, 0x00FF},
	{0x3E3C, 0x2801},
	{0x3E3E, 0x3E2A},
	{0x3E40, 0x1C01},
	{0x3E42, 0xFF84},
	{0x3E44, 0x8401},
	{0x3E46, 0x0C01},
	{0x3E48, 0x8401},
	{0x3E4A, 0x00FF},
	{0x3E4C, 0x8402},
	{0x3E4E, 0x8984},
	{0x3E50, 0x6628},
	{0x3E52, 0x8340},
	{0x3E54, 0x00FF},
	{0x3E56, 0x4A42},
	{0x3E58, 0x2703},
	{0x3E5A, 0x6752},
	{0x3E5C, 0x3F2A},
	{0x3E5E, 0x846A},
	{0x3E60, 0x4C01},
	{0x3E62, 0x8401},
	{0x3E66, 0x3901},
	{0x3E90, 0x2C01},
	{0x3E98, 0x2B02},
	{0x3E92, 0x2A04},
	{0x3E94, 0x2509},
	{0x3E96, 0x0000},
	{0x3E9A, 0x2905},
	{0x3E9C, 0x00FF},
	{0x3ECC, 0x00EB},
	{0x3ED0, 0x1E24},
	{0x3ED4, 0xAFC4},
	{0x3ED6, 0x909B},
	{0x3EE0, 0x2424},
	{0x3EE2, 0x9797},
	{0x3EE4, 0xC100},
	{0x3EE6, 0x0540},
	{0x3174, 0x8000},

	/* mode end */
	{0x0300, 0x0004},	/* VT_PIX_CLK_DIV */
	{0x0302, 0x0001},	/* VT_SYS_CLK_DIV */
	{0x0304, 0x0002},	/* PRE_PLL_CLK_DIV */

	{0x0306, 0x0040},	/* PLL_MULTIPLIER */

	{0x0308, 0x000A},	/* OP_PIX_CLK_DIV */
	{0x030A, 0x0001},	/* OP_SYS_CLK_DIV */
	{ar0832_TABLE_WAIT_MS, 0x0001}, /* waitmsec 1 */

	{0x3064, 0x7400},	/* RESERVED_MFR_3064 */

	{0x0104, 0x0100},	/* GROUPED_PARAMETER_HOLD */

	{0x0344, 0x0008},	/* X_ADDR_START */
	{0x0348, 0x0CC9},	/* X_ADDR_END */
	{0x0346, 0x0008},	/* Y_ADDR_START */
	{0x034A, 0x0999},	/* Y_ADDR_END */
	{0x034C, 0x0660},	/* X_OUTPUT_SIZE */
	{0x034E, 0x04C8},	/* Y_OUTPUT_SIZE */
	{0x3040, 0xC4C3},	/* READ_MODE */
	{0x306E, 0xFC80},	/* DATAPATH_SELECT */
	{0x3178, 0x0000},	/* RESERVED_MFR_3178 */
	{0x3ED0, 0x1E24},	/* RESERVED_MFR_3ED0 */
	{0x0400, 0x0002},	/* SCALING_MODE */
	{0x0404, 0x0010},	/* SCALE_M */
	{0x0342, 0x101A},	/* LINE_LENGTH_PCK */
	{0x0340, 0x0610},	/* FRAME_LENGTH_LINES */
	{0x0202, 0x0557},	/* COARSE_INTEGRATION_TIME */
	{0x3014, 0x0988},	/* FINE_INTEGRATION_TIME */
	{0x3010, 0x0130},	/* FINE_CORRECTION */
	{0x301A, 0x8250},	/* RESET_REGISTER */
	{0x301A, 0x8650},	/* RESET_REGISTER */
	{0x301A, 0x8658},	/* RESET_REGISTER */

	/* gain */
	{0x305e, 0x10AA},	/* gain */

	/* todo 8-bit write */
	{0x0104, 0x0000},	/* GROUPED_PARAMETER_HOLD */
	{0x301A, 0x065C},	/* RESET_REGISTER */
	{ar0832_TABLE_END, 0x0000}
};

static struct ar0832_reg mode_800X600_8140[] = {
	/* mode start */
	{0x301A, 0x0058},
	{0x301A, 0x0050},
	{0x0104, 0x0100},
	{0x3064, 0x7800},
	{0x31AE, 0x0202},
	{0x31B8, 0x0E3F},
	{0x31BE, 0xC003},
	{0x3070, 0x0000},
	{ar0832_TABLE_WAIT_MS, 0x0005},

	/* MT9E013 Recommended Settings */
	{0x3044, 0x0590},
	{0x306E, 0xFC80},
	{0x30B2, 0xC000},
	{0x30D6, 0x0800},
	{0x316C, 0xB42F},
	{0x316E, 0x869A},
	{0x3170, 0x210E},
	{0x317A, 0x010E},
	{0x31E0, 0x1FB9},
	{0x31E6, 0x07FC},
	{0x37C0, 0x0000},
	{0x37C2, 0x0000},
	{0x37C4, 0x0000},
	{0x37C6, 0x0000},
	{0x3E00, 0x0011},
	{0x3E02, 0x8801},
	{0x3E04, 0x2801},
	{0x3E06, 0x8449},
	{0x3E08, 0x6841},
	{0x3E0A, 0x400C},
	{0x3E0C, 0x1001},
	{0x3E0E, 0x2603},
	{0x3E10, 0x4B41},
	{0x3E12, 0x4B24},
	{0x3E14, 0xA3CF},
	{0x3E16, 0x8802},
	{0x3E18, 0x8401},
	{0x3E1A, 0x8601},
	{0x3E1C, 0x8401},
	{0x3E1E, 0x840A},
	{0x3E20, 0xFF00},
	{0x3E22, 0x8401},
	{0x3E24, 0x00FF},
	{0x3E26, 0x0088},
	{0x3E28, 0x2E8A},
	{0x3E30, 0x0000},
	{0x3E32, 0x8801},
	{0x3E34, 0x4029},
	{0x3E36, 0x00FF},
	{0x3E38, 0x8469},
	{0x3E3A, 0x00FF},
	{0x3E3C, 0x2801},
	{0x3E3E, 0x3E2A},
	{0x3E40, 0x1C01},
	{0x3E42, 0xFF84},
	{0x3E44, 0x8401},
	{0x3E46, 0x0C01},
	{0x3E48, 0x8401},
	{0x3E4A, 0x00FF},
	{0x3E4C, 0x8402},
	{0x3E4E, 0x8984},
	{0x3E50, 0x6628},
	{0x3E52, 0x8340},
	{0x3E54, 0x00FF},
	{0x3E56, 0x4A42},
	{0x3E58, 0x2703},
	{0x3E5A, 0x6752},
	{0x3E5C, 0x3F2A},
	{0x3E5E, 0x846A},
	{0x3E60, 0x4C01},
	{0x3E62, 0x8401},
	{0x3E66, 0x3901},
	{0x3E90, 0x2C01},
	{0x3E98, 0x2B02},
	{0x3E92, 0x2A04},
	{0x3E94, 0x2509},
	{0x3E96, 0x0000},
	{0x3E9A, 0x2905},
	{0x3E9C, 0x00FF},
	{0x3ECC, 0x00EB},
	{0x3ED0, 0x1E24},
	{0x3ED4, 0xAFC4},
	{0x3ED6, 0x909B},
	{0x3EE0, 0x2424},
	{0x3EE2, 0x9797},
	{0x3EE4, 0xC100},
	{0x3EE6, 0x0540},

	/* mode end */
	{0x3174, 0x8000},

	/* [RAW10] */
	{0x0112, 0x0A0A},

	/* PLL Configuration Ext=24MHz */
	{0x0300, 0x0004},
	{0x0302, 0x0001},
	{0x0304, 0x0002},
	{0x0306, 0x0042},
	{0x0308, 0x000A},
	{0x030A, 0x0001},
	{ar0832_TABLE_WAIT_MS, 0x0001},

	/* Output size */
	{0x0344, 0x04D8},
	{0x0348, 0x07F7},
	{0x0346, 0x03A4},
	{0x034A, 0x05FB},
	{0x034C, 0x0320},
	{0x034E, 0x0258},
	{0x3040, 0xC041},

	{0x306E, 0xFC80},
	{0x3178, 0x0000},
	{0x3ED0, 0x1E24},

	/* Scale Configuration */
	{0x0400, 0x0000},
	{0x0404, 0x0010},

	/* Timing Configuration */
	{0x0342, 0x08A8},
	{0x0340, 0x02E7},
	{0x0202, 0x02E7},
	{0x3014, 0x03F6},
	{0x3010, 0x0078},

	{0x301A, 0x8250},
	{0x301A, 0x8650},
	{0x301A, 0x8658},
	/* STATE= Minimum Gain, 1500 */
	{0x305E, 0x13AF},

	{0x0104, 0x0000},
	{0x301A, 0x065C},
	{ar0832_TABLE_END, 0x0000}
};

static struct ar0832_reg mode_800X600_8141[] = {
	/* mode start */
	{0x301A, 0x0058},	/* RESET_REGISTER */
	{0x301A, 0x0050},	/* RESET_REGISTER */

	/* SC-CHANGE: to-do 8 bit write */
	{0x0104, 0x0100},	/* GROUPED_PARAMETER_HOLD */

	{0x3064, 0x7800},	/* RESERVED_MFR_3064 */
	{0x31AE, 0x0202},	/* SERIAL_FORMAT */
	/* AR0832 Recommended Settings */
	{0x31B0, 0x0083},	/* FRAME_PREAMBLE */
	{0x31B2, 0x004D},	/* LINE_PREAMBLE */
	{0x31B4, 0x0E88},	/* MIPI_TIMING_0 */
	{0x31B6, 0x0D24},	/* MIPI_TIMING_1 */
	{0x31B8, 0x020E},	/* MIPI_TIMING_2 */
	{0x31BA, 0x0710},	/* MIPI_TIMING_3 */
	{0x31BC, 0x2A0D},	/* MIPI_TIMING_4 */
	{ar0832_TABLE_WAIT_MS, 0x0005},
	{0x0112, 0x0A0A},	/* CCP_DATA_FORMAT */
	{0x3044, 0x0590},	/* RESERVED_MFR_3044 */
	{0x306E, 0xFC80},	/* DATAPATH_SELECT */
	{0x30B2, 0xC000},	/* RESERVED_MFR_30B2 */
	{0x30D6, 0x0800},	/* RESERVED_MFR_30D6 */
	{0x316C, 0xB42F},	/* RESERVED_MFR_316C */
	{0x316E, 0x869A},	/* RESERVED_MFR_316E */
	{0x3170, 0x210E},	/* RESERVED_MFR_3170 */
	{0x317A, 0x010E},	/* RESERVED_MFR_317A */
	{0x31E0, 0x1FB9},	/* RESERVED_MFR_31E0 */
	{0x31E6, 0x07FC},	/* RESERVED_MFR_31E6 */
	{0x37C0, 0x0000},	/* P_GR_Q5 */
	{0x37C2, 0x0000},	/* P_RD_Q5 */
	{0x37C4, 0x0000},	/* P_BL_Q5 */
	{0x37C6, 0x0000},	/* P_GB_Q5 */
	{0x3E00, 0x0011},	/* RESERVED_MFR_3E00 */
	{0x3E02, 0x8801},	/* RESERVED_MFR_3E02 */
	{0x3E04, 0x2801},	/* RESERVED_MFR_3E04 */
	{0x3E06, 0x8449},	/* RESERVED_MFR_3E06 */
	{0x3E08, 0x6841},	/* RESERVED_MFR_3E08 */
	{0x3E0A, 0x400C},	/* RESERVED_MFR_3E0A */
	{0x3E0C, 0x1001},	/* RESERVED_MFR_3E0C */
	{0x3E0E, 0x2603},	/* RESERVED_MFR_3E0E */
	{0x3E10, 0x4B41},	/* RESERVED_MFR_3E10 */
	{0x3E12, 0x4B24},	/* RESERVED_MFR_3E12 */
	{0x3E14, 0xA3CF},	/* RESERVED_MFR_3E14 */
	{0x3E16, 0x8802},	/* RESERVED_MFR_3E16 */
	{0x3E18, 0x84FF},	/* RESERVED_MFR_3E18 */
	{0x3E1A, 0x8601},	/* RESERVED_MFR_3E1A */
	{0x3E1C, 0x8401},	/* RESERVED_MFR_3E1C */
	{0x3E1E, 0x840A},	/* RESERVED_MFR_3E1E */
	{0x3E20, 0xFF00},	/* RESERVED_MFR_3E20 */
	{0x3E22, 0x8401},	/* RESERVED_MFR_3E22 */
	{0x3E24, 0x00FF},	/* RESERVED_MFR_3E24 */
	{0x3E26, 0x0088},	/* RESERVED_MFR_3E26 */
	{0x3E28, 0x2E8A},	/* RESERVED_MFR_3E28 */
	{0x3E30, 0x0000},	/* RESERVED_MFR_3E30 */
	{0x3E32, 0x8801},	/* RESERVED_MFR_3E32 */
	{0x3E34, 0x4029},	/* RESERVED_MFR_3E34 */
	{0x3E36, 0x00FF},	/* RESERVED_MFR_3E36 */
	{0x3E38, 0x8469},	/* RESERVED_MFR_3E38 */
	{0x3E3A, 0x00FF},	/* RESERVED_MFR_3E3A */
	{0x3E3C, 0x2801},	/* RESERVED_MFR_3E3C */
	{0x3E3E, 0x3E2A},	/* RESERVED_MFR_3E3E */
	{0x3E40, 0x1C01},	/* RESERVED_MFR_3E40 */
	{0x3E42, 0xFF84},	/* RESERVED_MFR_3E42 */
	{0x3E44, 0x8401},	/* RESERVED_MFR_3E44 */
	{0x3E46, 0x0C01},	/* RESERVED_MFR_3E46 */
	{0x3E48, 0x8401},	/* RESERVED_MFR_3E48 */
	{0x3E4A, 0x00FF},	/* RESERVED_MFR_3E4A */
	{0x3E4C, 0x8402},	/* RESERVED_MFR_3E4C */
	{0x3E4E, 0x8984},	/* RESERVED_MFR_3E4E */
	{0x3E50, 0x6628},	/* RESERVED_MFR_3E50 */
	{0x3E52, 0x8340},	/* RESERVED_MFR_3E52 */
	{0x3E54, 0x00FF},	/* RESERVED_MFR_3E54 */
	{0x3E56, 0x4A42},	/* RESERVED_MFR_3E56 */
	{0x3E58, 0x2703},	/* RESERVED_MFR_3E58 */
	{0x3E5A, 0x6752},	/* RESERVED_MFR_3E5A */
	{0x3E5C, 0x3F2A},	/* RESERVED_MFR_3E5C */
	{0x3E5E, 0x846A},	/* RESERVED_MFR_3E5E */
	{0x3E60, 0x4C01},	/* RESERVED_MFR_3E60 */
	{0x3E62, 0x8401},	/* RESERVED_MFR_3E62 */
	{0x3E66, 0x3901},	/* RESERVED_MFR_3E66 */
	{0x3E90, 0x2C01},	/* RESERVED_MFR_3E90 */
	{0x3E98, 0x2B02},	/* RESERVED_MFR_3E98 */
	{0x3E92, 0x2A04},	/* RESERVED_MFR_3E92 */
	{0x3E94, 0x2509},	/* RESERVED_MFR_3E94 */
	{0x3E96, 0x0000},	/* RESERVED_MFR_3E96 */
	{0x3E9A, 0x2905},	/* RESERVED_MFR_3E9A */
	{0x3E9C, 0x00FF},	/* RESERVED_MFR_3E9C */
	{0x3ECC, 0x00EB},	/* RESERVED_MFR_3ECC */
	{0x3ED0, 0x1E24},	/* RESERVED_MFR_3ED0 */
	{0x3ED4, 0xAFC4},	/* RESERVED_MFR_3ED4 */
	{0x3ED6, 0x909B},	/* RESERVED_MFR_3ED6 */
	{0x3EE0, 0x2424},	/* RESERVED_MFR_3EE0 */
	{0x3EE2, 0x9797},	/* RESERVED_MFR_3EE2 */
	{0x3EE4, 0xC100},	/* RESERVED_MFR_3EE4 */
	{0x3EE6, 0x0540},	/* RESERVED_MFR_3EE6 */

	/* mode end */
	{0x3174, 0x8000},	/* RESERVED_MFR_3174 */
	{0x0300, 0x0004},	/* VT_PIX_CLK_DIV */
	{0x0302, 0x0001},	/* VT_SYS_CLK_DIV */
	{0x0304, 0x0002},	/* PRE_PLL_CLK_DIV */

	{0x0306, 0x0042},	/* PLL_MULTIPLIER */

	{0x0308, 0x000A},	/* OP_PIX_CLK_DIV */
	{0x030A, 0x0001},	/* OP_SYS_CLK_DIV */
	{ar0832_TABLE_WAIT_MS, 0x0001}, /* waitmsec 1 */

	{0x3064, 0x7400},	/* RESERVED_MFR_3064 */

	{0x0104, 0x0100},	/* GROUPED_PARAMETER_HOLD */

	{0x0344, 0x04D8},	/* X_ADDR_START */
	{0x0348, 0x07F7},	/* X_ADDR_END */
	{0x0346, 0x03A4},	/* Y_ADDR_START */
	{0x034A, 0x05FB},	/* Y_ADDR_END */
	{0x034C, 0x0320},	/* X_OUTPUT_SIZE */
	{0x034E, 0x0260},	/* Y_OUTPUT_SIZE */
	{0x3040, 0xC041},	/* READ_MODE */
	{0x306E, 0xFC80},	/* DATAPATH_SELECT */
	{0x3178, 0x0000},	/* RESERVED_MFR_3178 */
	{0x3ED0, 0x1E24},	/* RESERVED_MFR_3ED0 */
	{0x0400, 0x0000},	/* SCALING_MODE */
	{0x0404, 0x0010},	/* SCALE_M */
	{0x0342, 0x08A8},	/* LINE_LENGTH_PCK */
	{0x0340, 0x02E7},	/* FRAME_LENGTH_LINES */
	{0x0202, 0x02E7},	/* COARSE_INTEGRATION_TIME */
	{0x3014, 0x03F6},	/* FINE_INTEGRATION_TIME */
	{0x3010, 0x0078},	/* FINE_CORRECTION */
	{0x301A, 0x8250},	/* RESET_REGISTER */
	{0x301A, 0x8650},	/* RESET_REGISTER */
	{0x301A, 0x8658},	/* RESET_REGISTER */

	/* gain */
	{0x305e, 0x10AA},	/* gain */

	/* todo 8-bit write */
	{0x0104, 0x0000},	/* GROUPED_PARAMETER_HOLD */
	{0x301A, 0x065C},	/* RESET_REGISTER */
	{ar0832_TABLE_END, 0x0000}
};

static struct ar0832_reg mode_end[] = {
	{ar0832_TABLE_END, 0x0000}
};

enum {
	ar0832_MODE_3264X2448,
	ar0832_MODE_2880X1620,
	ar0832_MODE_1920X1080,
	ar0832_MODE_1632X1224,
	ar0832_MODE_800X600
};

static struct ar0832_reg *mode_table_8140[] = {
	[ar0832_MODE_3264X2448] = mode_3264X2448_8140,
	[ar0832_MODE_2880X1620] = mode_2880X1620_8140,
	[ar0832_MODE_1920X1080] = mode_1920X1080_8140,
	[ar0832_MODE_1632X1224] = mode_1632X1224_8140,
	[ar0832_MODE_800X600]   = mode_800X600_8140,
};

static struct ar0832_reg *mode_table_8141[] = {
	[ar0832_MODE_3264X2448] = mode_3264X2448_8141,
	[ar0832_MODE_2880X1620] = mode_2880X1620_8141,
	[ar0832_MODE_1920X1080] = mode_1920X1080_8141,
	[ar0832_MODE_1632X1224] = mode_1632X1224_8141,
	[ar0832_MODE_800X600]   = mode_800X600_8141,
};

static inline void ar0832_msleep(u32 t)
{
	/*
	why usleep_range() instead of msleep() ?
	Read Documentation/timers/timers-howto.txt
	*/
	usleep_range(t*1000, t*1000 + 500);
}

/* 16 bit reg to program frame length */
static inline void ar0832_get_frame_length_regs(struct ar0832_reg *regs,
						u32 frame_length)
{
	regs->addr = 0x0340;
	regs->val = (frame_length) & 0xFFFF;
}

static inline void ar0832_get_coarse_time_regs(struct ar0832_reg *regs,
						u32 coarse_time)
{
	regs->addr = 0x0202;
	regs->val = (coarse_time) & 0xFFFF;
}

static inline void ar0832_get_focuser_vcm_control_regs(struct ar0832_reg *regs,
							u16 value)
{
	regs->addr = 0x30F0;
	regs->val = (value) & 0xFFFF;
}

static inline void ar0832_get_focuser_vcm_step_time_regs
	(struct ar0832_reg *regs, u16 value)
{
	regs->addr = 0x30F4;
	regs->val = (value) & 0xFFFF;
}

static inline void ar0832_get_focuser_data_regs(struct ar0832_reg *regs,
						u16 value)
{
	regs->addr = 0x30F2;
	regs->val = (value) & 0xFFFF;
}

static inline void ar0832_get_gain_regs(struct ar0832_reg *regs, u16 gain)
{
	/* global_gain register*/
	regs->addr = AR0832_GLOBAL_GAIN_REG;
	regs->val = gain;
}

static int ar0832_write_reg8(struct i2c_client *client, u16 addr, u8 val)
{
	int err;
	struct i2c_msg msg;
	unsigned char data[3];
	int retry = 0;

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8) (addr >> 8);;
	data[1] = (u8) (addr & 0xff);
	data[2] = (u8) (val & 0xff);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = data;

	dev_dbg(&client->dev, "0x%x = 0x%x\n", addr, val);

	do {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err > 0)
			return 0;
		retry++;
		dev_err(&client->dev,
			"%s: i2c transfer failed, retrying %x %x\n",
			__func__, addr, val);
		ar0832_msleep(3);
	} while (retry < ar0832_MAX_RETRIES);

	return err;
}

static int ar0832_write_reg16(struct i2c_client *client, u16 addr, u16 val)
{
	int count;
	struct i2c_msg msg;
	unsigned char data[4];
	int retry = 0;

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);
	data[2] = (u8) (val >> 8);
	data[3] = (u8) (val & 0xff);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 4;
	msg.buf = data;

	dev_dbg(&client->dev, "0x%x = 0x%x\n", addr, val);

	do {
		count = i2c_transfer(client->adapter, &msg, 1);
		if (count == 1)
			return 0;
		retry++;
		dev_err(&client->dev,
			"%s: i2c transfer failed, retrying %x %x\n",
		       __func__, addr, val);
		ar0832_msleep(3);
	} while (retry <= ar0832_MAX_RETRIES);

	return -EIO;
}

static int ar0832_read_reg16(struct i2c_client *client, u16 addr, u16 *val)
{
	struct i2c_msg msg[2];
	u8 data[4];

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;
	data[0] = (addr >> 8);
	data[1] = (addr & 0xff);
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = data + 2;

	if (i2c_transfer(client->adapter, msg, 2) == 2) {
		*val = ((data[2] << 8) | data[3]);
		dev_dbg(&client->dev, "0x%x = 0x%x\n", addr, *val);
		return 0;
	} else {
		*val = 0;
		dev_err(&client->dev,
			"%s: i2c read failed.\n", __func__);
		return -1;
	}
}

static int ar0832_write_reg_helper(struct ar0832_dev *dev,
					u16 addr,
					u16 val)
{
	int ret;

	if (addr == 0x104)
		ret = ar0832_write_reg8(dev->i2c_client, addr,
					(val >> 8 & 0xff));
	else
		ret = ar0832_write_reg16(dev->i2c_client, addr, val);

	return ret;
}

static int ar0832_write_table(struct ar0832_dev *dev,
				const struct ar0832_reg table[],
				const struct ar0832_reg override_list[],
				int num_override_regs)
{
	int err;
	const struct ar0832_reg *next;
	u16 val;
	int i;

	for (next = table; next->addr != ar0832_TABLE_END; next++) {
		if (next->addr ==  ar0832_TABLE_WAIT_MS) {
			ar0832_msleep(next->val);
			continue;
		}

		val = next->val;
		/* When an override list is passed in, replace the reg */
		/* value to write if the reg is in the list            */
		if (override_list) {
			for (i = 0; i < num_override_regs; i++) {
				if (next->addr == override_list[i].addr) {
					val = override_list[i].val;
					break;
				}
			}
		}

		err = ar0832_write_reg_helper(dev, next->addr, val);
		if (err)
			return err;
	}
	return 0;
}

static int ar0832_set_frame_length(struct ar0832_dev *dev,
					u32 frame_length)
{
	struct ar0832_reg reg_list;
	struct i2c_client *i2c_client = dev->i2c_client;
	int ret;

	dev_dbg(&i2c_client->dev, "[%s] (0x%08x)\n", __func__,  frame_length);

	ar0832_get_frame_length_regs(&reg_list, frame_length);
	ret = ar0832_write_reg8(i2c_client, AR0832_GROUP_HOLD_REG, 0x1);
	if (ret)
		return ret;

	ret = ar0832_write_reg16(i2c_client, reg_list.addr,
			reg_list.val);
	if (ret)
		return ret;

	ret = ar0832_write_reg8(i2c_client, AR0832_GROUP_HOLD_REG, 0x0);
	if (ret)
		return ret;

	return 0;
}

static int ar0832_set_coarse_time(struct ar0832_dev *dev,
				  u32 coarse_time)
{
	int ret;
	struct ar0832_reg reg_list;
	struct i2c_client *i2c_client = dev->i2c_client;

	dev_dbg(&i2c_client->dev, "[%s] (0x%08x)\n", __func__,  coarse_time);
	ar0832_get_coarse_time_regs(&reg_list, coarse_time);

	ret = ar0832_write_reg8(i2c_client, AR0832_GROUP_HOLD_REG, 0x1);
	if (ret)
		return ret;

	ret = ar0832_write_reg16(i2c_client, reg_list.addr,
			reg_list.val);
	if (ret)
		return ret;

	ret = ar0832_write_reg8(i2c_client, AR0832_GROUP_HOLD_REG, 0x0);
	if (ret)
		return ret;

	return 0;
}

static int ar0832_set_gain(struct ar0832_dev *dev, u16 gain)
{
	int ret = 0;
	struct ar0832_reg reg_list_gain;

	ret = ar0832_write_reg8(dev->i2c_client, AR0832_GROUP_HOLD_REG, 0x1);
	/* Gain Registers Start */
	ar0832_get_gain_regs(&reg_list_gain, gain);
	ret |= ar0832_write_reg16(dev->i2c_client,
				reg_list_gain.addr,
				reg_list_gain.val);
	if (ret)
		return ret;

	/* Gain register End */
	ret |= ar0832_write_reg8(dev->i2c_client, AR0832_GROUP_HOLD_REG, 0x0);

	return ret;
}

static int ar0832_set_mode(struct ar0832_dev *dev,
				struct ar0832_mode *mode)
{
	int sensor_mode;
	int err;
	struct i2c_client *i2c_client = dev->i2c_client;
	struct ar0832_reg reg_ovr[3];
	struct ar0832_reg *mode_seq;

	dev_dbg(&i2c_client->dev, "%s: ++\n", __func__);

	if (mode->xres == 3264 && mode->yres == 2448)
		sensor_mode = ar0832_MODE_3264X2448;
	else if (mode->xres == 2880 && mode->yres == 1620)
		sensor_mode = ar0832_MODE_2880X1620;
	else if (mode->xres == 1920 && mode->yres == 1080)
		sensor_mode = ar0832_MODE_1920X1080;
	else if (mode->xres == 1632 && mode->yres == 1224)
		sensor_mode = ar0832_MODE_1632X1224;
	else if (mode->xres == 800 && mode->yres == 600)
		sensor_mode = ar0832_MODE_800X600;
	else {
		dev_err(&i2c_client->dev,
			"%s: invalid resolution supplied to set mode %d %d\n",
			__func__ , mode->xres, mode->yres);
		return -EINVAL;
	}

	if (dev->sensor_id_data == AR0832_SENSOR_ID_8141)
		mode_seq = mode_table_8141[sensor_mode];
	else
		mode_seq = mode_table_8140[sensor_mode];
	/* get a list of override regs for the asking frame length,    */
	/* coarse integration time, and gain.*/
	err = ar0832_write_table(dev, mode_start, NULL, 0);
	if (err)
		return err;

	/* When we change the resolution */
	ar0832_get_frame_length_regs(&reg_ovr[0], mode->frame_length);
	ar0832_get_coarse_time_regs(&reg_ovr[1], mode->coarse_time);
	ar0832_get_gain_regs(&reg_ovr[2], mode->gain);
	err = ar0832_write_table(dev, mode_seq, reg_ovr, ARRAY_SIZE(reg_ovr));
	if (err)
		return err;

	err = ar0832_write_table(dev, mode_end, NULL, 0);
	if (err)
		return err;

	dev->sensor_info->mode = sensor_mode;
	dev_dbg(&i2c_client->dev, "%s: --\n", __func__);

	return 0;
}

static int ar0832_get_status(struct ar0832_dev *dev, u8 *status)
{
	int err = 0;
	struct i2c_client *i2c_client = dev->i2c_client;

	*status = 0;
	/* FixMe */
	/*
	err = ar0832_read_reg(dev->i2c_client, 0x001, status);
	*/
	dev_dbg(&i2c_client->dev, "%s: %u %d\n", __func__, *status, err);
	return err;
}

static int ar0832_set_alternate_addr(struct i2c_client *client)
{
	int ret = 0;
	u8 new_addr = client->addr;
	u16 val;

	/* Default slave address of ar0832 is 0x36 */
	client->addr = 0x36;
	ret = ar0832_read_reg16(client, AR0832_RESET_REG, &val);
	val &= ~AR0832_RESET_REG_LOCK_REG;
	ret |= ar0832_write_reg16(client, AR0832_RESET_REG, val);
	ret |= ar0832_write_reg16(client, AR0832_ID_REG, new_addr << 1);

	if (!ret) {
		client->addr = new_addr;
		dev_dbg(&client->dev,
			"new slave address is set to 0x%x\n", new_addr);
	}

	ret |= ar0832_read_reg16(client, AR0832_RESET_REG, &val);
	val |= AR0832_RESET_REG_LOCK_REG;
	ret |= ar0832_write_reg16(client, AR0832_RESET_REG, val);

	return ret;
}

static void ar0832_mclk_disable(struct ar0832_dev *info)
{
	dev_dbg(&info->i2c_client->dev, "%s: disable MCLK\n", __func__);
	clk_disable_unprepare(info->mclk);
}

static int ar0832_mclk_enable(struct ar0832_dev *info)
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

static int ar0832_power_on(struct ar0832_dev *dev)
{
	struct i2c_client *i2c_client = dev->i2c_client;
	int ret = 0;

	dev_dbg(&i2c_client->dev, "%s: ++ %d %d\n",
		__func__, dev->is_stereo,
		dev->brd_power_cnt);

	/* Board specific power-on sequence */
	mutex_lock(&dev->ar0832_camera_lock);
	if (dev->brd_power_cnt == 0) {

		ret = ar0832_mclk_enable(dev);
		if (ret < 0)
			goto fail_mclk;

		/* Plug 1.8V and 2.8V power to sensor */
		if (dev->power_rail.sen_1v8_reg) {
			ret = regulator_enable(dev->power_rail.sen_1v8_reg);
			if (ret) {
				dev_err(&i2c_client->dev,
					"%s: failed to enable vdd\n",
					__func__);
				goto fail_regulator_1v8_reg;
			}
		}

		if (dev->power_rail.sen_2v8_reg) {
			ret = regulator_enable(dev->power_rail.sen_2v8_reg);
			if (ret) {
				dev_err(&i2c_client->dev,
					"%s: failed to enable vaa\n",
					__func__);
				goto fail_regulator_2v8_reg;
			}
		}
		dev->pdata->power_on(&i2c_client->dev, dev->is_stereo);
	}
	dev->brd_power_cnt++;

	/* Change slave address */
	if (i2c_client->addr)
		ret = ar0832_set_alternate_addr(i2c_client);

	mutex_unlock(&dev->ar0832_camera_lock);
	return 0;

fail_regulator_2v8_reg:
	regulator_put(dev->power_rail.sen_2v8_reg);
	dev->power_rail.sen_2v8_reg = NULL;
	if (dev->power_rail.sen_1v8_reg)
		regulator_disable(dev->power_rail.sen_1v8_reg);
fail_regulator_1v8_reg:
	regulator_put(dev->power_rail.sen_1v8_reg);
	dev->power_rail.sen_1v8_reg = NULL;
	ar0832_mclk_disable(dev);
fail_mclk:
	mutex_unlock(&dev->ar0832_camera_lock);
	return ret;
}

static void ar0832_power_off(struct ar0832_dev *dev)
{
	struct i2c_client *i2c_client = dev->i2c_client;
	dev_dbg(&i2c_client->dev, "%s: ++ %d\n", __func__, dev->brd_power_cnt);

	/* Board specific power-down sequence */
	mutex_lock(&dev->ar0832_camera_lock);

	if (dev->brd_power_cnt <= 0)
		goto ar0832_pwdn_exit;

	if (dev->brd_power_cnt-- == 1) {
		/* Unplug 1.8V and 2.8V power from sensor */
		if (dev->power_rail.sen_2v8_reg)
			regulator_disable(dev->power_rail.sen_2v8_reg);
		if (dev->power_rail.sen_1v8_reg)
			regulator_disable(dev->power_rail.sen_1v8_reg);
		dev->pdata->power_off(&i2c_client->dev, dev->is_stereo);
		ar0832_mclk_disable(dev);
	}

ar0832_pwdn_exit:
	mutex_unlock(&dev->ar0832_camera_lock);
}

static int ar0832_focuser_set_config(struct ar0832_dev *dev)
{
	struct i2c_client *i2c_client = dev->i2c_client;
	struct ar0832_reg reg_vcm_ctrl, reg_vcm_step_time;
	int ret = 0;
	u8 vcm_slew;
	u16 vcm_control_data;
	u16 vcm_step_time = 1024;

	/* slew_rate of disabled (0) or default value (1) will disable    */
	/* the slew rate in this case. Any value of 2 onwards will enable */
	/* the slew rate to a different degree */
	if (dev->focuser_info->config.slew_rate == SLEW_RATE_DISABLED ||
	   dev->focuser_info->config.slew_rate == SLEW_RATE_DEFAULT)
		vcm_slew = AR0832_SLEW_RATE_DISABLED;
	else
		vcm_slew = dev->focuser_info->config.slew_rate - 1;

	if (vcm_slew > AR0832_SLEW_RATE_SLOWEST)
		vcm_slew = AR0832_SLEW_RATE_SLOWEST;

	/* bit15(0x80) means that VCM driver enable bit. */
	/* bit3(0x08) means that keep VCM(AF position) */
	/* while sensor is in soft standby mode during mode transitions. */
	vcm_control_data = (0x80 << 8 | (0x08 | (vcm_slew & 0x07)));

	ar0832_get_focuser_vcm_control_regs(&reg_vcm_ctrl, vcm_control_data);
	ret = ar0832_write_reg16(dev->i2c_client, reg_vcm_ctrl.addr,
					reg_vcm_ctrl.val);

	dev_dbg(&i2c_client->dev, "%s Reg 0x%X Value 0x%X\n", __func__,
			reg_vcm_ctrl.addr, reg_vcm_ctrl.val);

	if (ret) {
		dev_dbg(&i2c_client->dev, "%s Error writing to register 0x%X\n",
				__func__, reg_vcm_ctrl.addr);
		return ret;
	}

	ar0832_get_focuser_vcm_step_time_regs(&reg_vcm_step_time,
						vcm_step_time);
	ret = ar0832_write_reg16(dev->i2c_client, reg_vcm_step_time.addr,
					reg_vcm_step_time.val);

	dev_dbg(&i2c_client->dev, "%s Reg step_time 0x%X Value 0x%X\n",
			__func__, reg_vcm_step_time.addr,
			reg_vcm_step_time.val);

	return ret;
}

static int ar0832_focuser_set_position(struct ar0832_dev *dev,
					u32 position)
{
	int ret = 0;
	struct ar0832_reg reg_data;

	ar0832_get_focuser_data_regs(&reg_data, position);
	ret = ar0832_write_reg16(dev->i2c_client, reg_data.addr,
				     reg_data.val);
	dev->focuser_info->last_position = position;

	return ret;
}

#ifdef AR0832_FOCUSER_DYNAMIC_STEP_TIME
/*
 * This function is not currently called as we have the hardcoded
 * step time in ar0832_focuser_set_config function. If we need to
 * compute the actual step time based on a number of clocks, we need
 * to use this function. The formula for computing the clock-based
 * step time is obtained from Aptina and is not part of external
 * documentation and hence this code needs to be saved.
 */
static u16 ar0832_get_focuser_vcm_step_time(struct ar0832_dev *dev)
{
	struct i2c_client *i2c_client = dev->i2c_client;
	int ret;
	u16 pll_multiplier = 0;
	u16 pre_pll_clk_div = 0;
	u16 vt_sys_clk_div = 0;
	u16 vt_pix_clk_div = 0;
	u16 vt_pix_clk_freq_mhz = 0;

	ret = ar0832_read_reg16(dev->i2c_client, 0x306, &pll_multiplier);
	if (ret) {
		dev_err(&i2c_client->dev, "%s pll_multiplier read failed\n",
				__func__);
	}

	ret = ar0832_read_reg16(dev->i2c_client, 0x304, &pre_pll_clk_div);
	if (ret) {
		dev_err(&i2c_client->dev, "%s pre_pll_clk_div read failed\n",
				__func__);
	}

	ret = ar0832_read_reg16(dev->i2c_client, 0x302, &vt_sys_clk_div);
	if (ret) {
		dev_err(&i2c_client->dev, "%s vt_sys_clk_div read failed\n",
				__func__);
	}

	ret = ar0832_read_reg16(dev->i2c_client, 0x300, &vt_pix_clk_div);
	if (ret) {
		dev_err(&i2c_client->dev, "%s vt_pix_clk_div read failed\n",
				__func__);
	}

	vt_pix_clk_freq_mhz =
		(24 * pll_multiplier) / (pre_pll_clk_div * vt_sys_clk_div *
		 vt_pix_clk_div);

	dev_dbg(&i2c_client->dev, "%s pll_multiplier 0x%X pre_pll_clk_div 0x%X "
			"vt_sys_clk_div 0x%X vt_pix_clk_div 0x%X vt_pix_clk_freq_mhz 0x%X\n",
			__func__, pll_multiplier,
			pre_pll_clk_div, vt_sys_clk_div,
			vt_pix_clk_div, vt_pix_clk_freq_mhz);

	return vt_pix_clk_freq_mhz;

}
#endif

static inline
int ar0832_get_sensorid(struct ar0832_dev *dev, u16 *sensor_id)
{
	int ret;
	struct i2c_client *i2c_client = dev->i2c_client;

	ret = ar0832_power_on(dev);
	if (ret)
		return ret;

	ret = ar0832_read_reg16(i2c_client, AR0832_SENSORID_REG, sensor_id);
	dev_dbg(&i2c_client->dev,
		"%s: sensor_id - %04x\n", __func__, *sensor_id);

	ar0832_power_off(dev);

	return ret;
}

static long ar0832_set_focuser_capabilities(struct ar0832_dev *dev,
						unsigned long arg)
{
	struct i2c_client *i2c_client = dev->i2c_client;
	struct nv_focuser_config config;

	/* backup the current contents of dev->focuser_info->config for
	 * selective restore later on */
	memcpy(&config, &dev->focuser_info->config,
		sizeof(struct nv_focuser_config));

	if (copy_from_user(&dev->focuser_info->config,
		(const void __user *)arg,
		sizeof(struct nv_focuser_config))) {
		dev_err(&i2c_client->dev,
			"%s: copy_from_user() failed\n", __func__);
		return -EFAULT;
	}

	/* Now do selectively restore members that are overwriten */

	/* Unconditionally restore actual low and high positions */
	dev->focuser_info->config.pos_actual_low = config.pos_actual_low;
	dev->focuser_info->config.pos_actual_high = config.pos_actual_high;

	if (dev->focuser_info->config.pos_working_low == AF_POS_INVALID_VALUE)
		dev->focuser_info->config.pos_working_low =
						config.pos_working_low;

	if (dev->focuser_info->config.pos_working_high == AF_POS_INVALID_VALUE)
		dev->focuser_info->config.pos_working_high =
						config.pos_working_high;

	if (dev->focuser_info->config.focuser_set[0].settle_time == INT_MAX)
		dev->focuser_info->config.focuser_set[0].settle_time =
					config.focuser_set[0].settle_time;

	if (dev->focuser_info->config.slew_rate == INT_MAX)
		dev->focuser_info->config.slew_rate = config.slew_rate;

	return 0;
}

static int ar0832_get_fuseid(struct ar0832_dev *dev)
{
	int ret = 0;
	int i, timeout;
	__u16 bak;
	struct i2c_client *i2c_client = dev->i2c_client;

	if (dev->fuse_id.size)
		return 0;

	ret = ar0832_write_reg16(i2c_client, 0x3052, 0x2704);
	ret |= ar0832_read_reg16(i2c_client, 0x3054, &bak);
	ret |= ar0832_write_reg16(i2c_client, 0x3054, bak | 0x0100);
	ret |= ar0832_read_reg16(i2c_client, 0x3050, &bak);
	ret |= ar0832_write_reg16(i2c_client, 0x3050, bak & 0x00ff);
	ret |= ar0832_write_reg16(i2c_client, 0x3054, 0x0008);
	ret |= ar0832_read_reg16(i2c_client, 0x304a, &bak);
	ret |= ar0832_write_reg16(i2c_client, 0x304a, bak | 0x0010);

	timeout = 0;
	while (!(bak & 0x0020)) {
		ret |= ar0832_read_reg16(i2c_client, 0x304a, &bak);
		timeout += 1;
		if (timeout >= 100) {
			pr_info("ar0832: fuse ID read timed out\n");
			return -EFAULT;
		}
	}

	if (bak & 0x0040)
		pr_info("ar0832: fuse ID read successfully\n");
	else {
		pr_info("ar0832: fuse ID read failed\n");
		return -EFAULT;
	}

	/* OTP memory on ar0832 is larger than we are checking - may
	 * may want to change which bytes are read in the future
	 */
	dev->fuse_id.size = AR0832_FUSE_ID_SIZE;
	for (i = 0; i < 8; i++) {
		if (i * 2 < dev->fuse_id.size) {
			ret |= ar0832_read_reg16(i2c_client, 0x3800 + i, &bak);
			dev->fuse_id.data[i * 2] =
					(__u8)((bak >> 8) & 0x00ff);
			if (i * 2 + 1 < dev->fuse_id.size)
				dev->fuse_id.data[i * 2 + 1] =
						(__u8)(bak & 0x00ff);
		}
	}

	if (ret)
		dev->fuse_id.size = 0;

	return ret;
}

static long ar0832_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int err;
	struct ar0832_dev *dev = file->private_data;
	struct i2c_client *i2c_client = dev->i2c_client;
	struct ar0832_mode mode;
	u16 pos;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(AR0832_IOCTL_SET_POWER_ON):
		dev_dbg(&i2c_client->dev, "AR0832_IOCTL_SET_POWER_ON\n");
		if (copy_from_user(&mode,
			(const void __user *)arg,
			sizeof(struct ar0832_mode))) {
			dev_err(&i2c_client->dev,
				"%s: AR0832_IOCTL_SET_POWER_ON failed\n",
				__func__);
			return -EFAULT;
		}
		dev->is_stereo = mode.stereo;
		return ar0832_power_on(dev);
	case _IOC_NR(AR0832_IOCTL_SET_MODE):
	{
		dev_dbg(&i2c_client->dev, "AR0832_IOCTL_SET_MODE\n");
		if (copy_from_user(&mode,
			(const void __user *)arg,
			sizeof(struct ar0832_mode))) {
			dev_err(&i2c_client->dev,
				"%s: AR0832_IOCTL_SET_MODE failed\n",
				__func__);
			return -EFAULT;
		}
		mutex_lock(&dev->ar0832_camera_lock);
		err = ar0832_set_mode(dev, &mode);

		/*
		 * We need to re-initialize the Focuser registers during mode
		 * switch due to the known issue of focuser retracting
		 */
		ar0832_focuser_set_config(dev);
		dev->focuser_info->focuser_init_flag = true;

		/*
		 * If the last focuser position is not at infinity when we
		 * did the mode switch, we need to go there. Before that,
		 * we need to come back to 0.
		 */
		if (dev->focuser_info->last_position > 0) {
			pos = dev->focuser_info->last_position;
			dev_dbg(&i2c_client->dev, "%s: AR0832_IOCTL_SET_MODE: "
					" Move to 0, restore the backedup focuser position of %d\n",
					__func__, pos);
			ar0832_focuser_set_position(dev, 0);
			ar0832_msleep(10);

			ar0832_focuser_set_position(dev, pos);
			ar0832_msleep(10);
		}
		mutex_unlock(&dev->ar0832_camera_lock);
		return err;
	}
	case _IOC_NR(AR0832_IOCTL_SET_FRAME_LENGTH):
		mutex_lock(&dev->ar0832_camera_lock);
		err = ar0832_set_frame_length(dev, (u32)arg);
		mutex_unlock(&dev->ar0832_camera_lock);
		return err;
	case _IOC_NR(AR0832_IOCTL_SET_COARSE_TIME):
		mutex_lock(&dev->ar0832_camera_lock);
		err = ar0832_set_coarse_time(dev, (u32)arg);
		mutex_unlock(&dev->ar0832_camera_lock);
		return err;
	case _IOC_NR(AR0832_IOCTL_SET_GAIN):
		mutex_lock(&dev->ar0832_camera_lock);
		err = ar0832_set_gain(dev, (u16)arg);
		mutex_unlock(&dev->ar0832_camera_lock);
		return err;
	case _IOC_NR(AR0832_IOCTL_GET_STATUS):
	{
		u8 status;
		dev_dbg(&i2c_client->dev, "AR0832_IOCTL_GET_STATUS\n");
		err = ar0832_get_status(dev, &status);
		if (err)
			return err;
		if (copy_to_user((void __user *)arg, &status,
				 2)) {
			dev_err(&i2c_client->dev,
				"%s: AR0832_IOCTL_GET_STATUS failed\n",
				__func__);
			return -EFAULT;
		}
		return 0;
	}
	case _IOC_NR(AR0832_IOCTL_SET_SENSOR_REGION):
	{
		dev_dbg(&i2c_client->dev, "AR0832_IOCTL_SET_SENSOR_REGION\n");
		/* Right now, it doesn't do anything */

		return 0;
	}

	case _IOC_NR(AR0832_FOCUSER_IOCTL_GET_CONFIG):
		dev_dbg(&i2c_client->dev,
			"%s AR0832_FOCUSER_IOCTL_GET_CONFIG\n", __func__);
		if (copy_to_user((void __user *) arg,
				 &dev->focuser_info->config,
				 sizeof(struct nv_focuser_config)))
        {
			dev_err(&i2c_client->dev,
				"%s: AR0832_FOCUSER_IOCTL_GET_CONFIG failed\n",
				__func__);
			return -EFAULT;
		}
		return 0;

	case _IOC_NR(AR0832_FOCUSER_IOCTL_SET_CONFIG):
		dev_info(&i2c_client->dev,
				"%s AR0832_FOCUSER_IOCTL_SET_CONFIG\n", __func__);
		if (ar0832_set_focuser_capabilities(dev, arg) != 0) {
			dev_err(&i2c_client->dev,
			"%s: AR0832_IOCTL_SET_CONFIG failed\n", __func__);
			return -EFAULT;
		}
		dev_dbg(&i2c_client->dev,
			"%s AR0832_FOCUSER_IOCTL_SET_CONFIG sucess "
			"slew_rate %i, pos_working_high %i, pos_working_low %i\n",
			__func__, dev->focuser_info->config.slew_rate,
			dev->focuser_info->config.pos_working_low,
			dev->focuser_info->config.pos_working_high);
		return 0;

	case _IOC_NR(AR0832_FOCUSER_IOCTL_SET_POSITION):
		dev_dbg(&i2c_client->dev,
			"%s AR0832_FOCUSER_IOCTL_SET_POSITION\n", __func__);
		mutex_lock(&dev->ar0832_camera_lock);
		if (dev->focuser_info->focuser_init_flag == false) {
			ar0832_focuser_set_config(dev);
			dev->focuser_info->focuser_init_flag = true;
		}
		err = ar0832_focuser_set_position(dev, (u32)arg);
		mutex_unlock(&dev->ar0832_camera_lock);
		return err;

	case _IOC_NR(AR0832_IOCTL_GET_SENSOR_ID):
		dev_dbg(&i2c_client->dev,
			"%s AR0832_IOCTL_GET_SENSOR_ID\n", __func__);

		if (!dev->sensor_id_data) {
			err = ar0832_get_sensorid(dev, &dev->sensor_id_data);
			if (err) {
				dev_err(&i2c_client->dev,
					"%s: failed to get sensor id\n",
					__func__);
				return -EFAULT;
			}
		}

		if (copy_to_user((void __user *) arg,
				&dev->sensor_id_data,
				sizeof(dev->sensor_id_data))) {
			dev_err(&i2c_client->dev,
				"%s: AR0832_IOCTL_GET_SENSOR_ID failed\n",
				__func__);
			return -EFAULT;
		}
		return 0;

	case _IOC_NR(AR0832_IOCTL_GET_FUSEID):
	{
		err = ar0832_get_fuseid(dev);
		if (err) {
			pr_err("%s %d %d\n", __func__, __LINE__, err);
			return err;
		}
		if (copy_to_user((void __user *)arg,
				&dev->fuse_id,
				sizeof(struct nvc_fuseid))) {
			pr_info("%s %d\n", __func__, __LINE__);
			return -EFAULT;
		}
		return 0;
	}

	default:
		dev_err(&i2c_client->dev, "(error) %s NONE IOCTL\n",
			__func__);
		return -EINVAL;
	}
	return 0;
}

static int ar0832_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct ar0832_dev *dev = dev_get_drvdata(miscdev->parent);
	struct i2c_client *i2c_client = dev->i2c_client;

	dev_dbg(&i2c_client->dev, "%s: ++\n", __func__);
	if (atomic_xchg(&dev->in_use, 1))
		return -EBUSY;

	dev->focuser_info->focuser_init_flag = false;
	file->private_data = dev;

	return 0;
}

static int ar0832_release(struct inode *inode, struct file *file)
{
	struct ar0832_dev *dev = file->private_data;
	struct i2c_client *i2c_client = dev->i2c_client;

	dev_dbg(&i2c_client->dev, "%s: ++\n", __func__);

	ar0832_power_off(dev);

	file->private_data = NULL;

	dev->focuser_info->focuser_init_flag = false;

	WARN_ON(!atomic_xchg(&dev->in_use, 0));
	return 0;
}

static const struct file_operations ar0832_fileops = {
	.owner = THIS_MODULE,
	.open = ar0832_open,
	.unlocked_ioctl = ar0832_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ar0832_ioctl,
#endif
	.release = ar0832_release,
};

static int ar0832_debugfs_show(struct seq_file *s, void *unused)
{
	struct ar0832_dev *dev = s->private;
	struct i2c_client *i2c_client = dev->i2c_client;
	int ret;
	u16 test_pattern_reg;

	dev_dbg(&dev->i2c_client->dev, "%s: ++\n", __func__);
	if (!dev->brd_power_cnt) {
		dev_info(&i2c_client->dev,
			"%s: camera is off\n", __func__);
		return 0;
	}

	mutex_lock(&dev->ar0832_camera_lock);
	ret = ar0832_read_reg16(i2c_client,
			AR0832_TEST_PATTERN_REG, &test_pattern_reg);
	mutex_unlock(&dev->ar0832_camera_lock);

	if (ret) {
		dev_err(&i2c_client->dev,
			"%s: test pattern write failed\n", __func__);
		return -EFAULT;
	}

	seq_printf(s, "%d\n", test_pattern_reg);
	return 0;
}

static ssize_t ar0832_debugfs_write(
	struct file *file,
	char const __user *buf,
	size_t count,
	loff_t *offset)
{
	struct ar0832_dev *dev = ((struct seq_file *)file->private_data)->private;
	struct i2c_client *i2c_client = dev->i2c_client;
	int ret = 0;
	char buffer[10];
	u16 input, val, red = 0, green = 0, blue = 0;

	dev_dbg(&i2c_client->dev, "%s: ++\n", __func__);
	if (!dev->brd_power_cnt) {
		dev_info(&i2c_client->dev,
			"%s: camera is off\n", __func__);
		return count;
	}

	if (copy_from_user(&buffer, buf, sizeof(buffer)))
		goto debugfs_write_fail;

	input = (u16)simple_strtoul(buffer, NULL, 10);

	mutex_lock(&dev->ar0832_camera_lock);
	ret = ar0832_write_reg8(i2c_client, AR0832_GROUP_HOLD_REG, 0x1);

	switch (input) {
	case 1: /* color bar */
		val = 2;
		break;
	case 2:	/* Red */
		val = 1;
		red = 0x300; /* 10 bit value */
		green = 0;
		blue = 0;
		break;
	case 3: /* Green */
		val = 1;
		red = 0;
		green = 0x300; /* 10 bit value */
		blue = 0;
		break;
	case 4:	/* Blue */
		val = 1;
		red = 0;
		green = 0;
		blue = 0x300; /* 10 bit value */
		break;
	default:
		val = 0;
		break;
	}

	if (input == 2 || input == 3 || input == 4) {
		ret |= ar0832_write_reg_helper(dev,
				AR0832_TEST_RED_REG, red);
		ret |= ar0832_write_reg_helper(dev,
				AR0832_TEST_GREENR_REG, green);
		ret |= ar0832_write_reg_helper(dev,
				AR0832_TEST_GREENR_REG, green);
		ret |= ar0832_write_reg_helper(dev,
				AR0832_TEST_BLUE_REG, blue);
	}

	ret |= ar0832_write_reg_helper(dev, AR0832_TEST_PATTERN_REG, val);
	ret |= ar0832_write_reg8(i2c_client, AR0832_GROUP_HOLD_REG, 0x0);
	mutex_unlock(&dev->ar0832_camera_lock);

	if (ret)
		goto debugfs_write_fail;

	return count;

debugfs_write_fail:
	dev_err(&i2c_client->dev,
			"%s: test pattern write failed\n", __func__);
	return -EFAULT;
}

static int ar0832_debugfs_open(struct inode *inode, struct file *file)
{
	struct ar0832_dev *dev = inode->i_private;
	struct i2c_client *i2c_client = dev->i2c_client;

	dev_dbg(&i2c_client->dev, "%s: ++\n", __func__);

	return single_open(file, ar0832_debugfs_show, inode->i_private);
}

static const struct file_operations ar0832_debugfs_fops = {
	.open		= ar0832_debugfs_open,
	.read		= seq_read,
	.write		= ar0832_debugfs_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void ar0832_remove_debugfs(struct ar0832_dev *dev)
{
	struct i2c_client *i2c_client = dev->i2c_client;

	dev_dbg(&i2c_client->dev, "%s: ++\n", __func__);

	if (dev->debugdir)
		debugfs_remove_recursive(dev->debugdir);
	dev->debugdir = NULL;
}

static void ar0832_create_debugfs(struct ar0832_dev *dev)
{
	struct dentry *ret;
	struct i2c_client *i2c_client = dev->i2c_client;

	dev_dbg(&i2c_client->dev, "%s\n", __func__);

	dev->debugdir = debugfs_create_dir(dev->dname, NULL);
	if (!dev->debugdir)
		goto remove_debugfs;

	ret = debugfs_create_file("test_pattern",
				S_IWUSR | S_IRUGO,
				dev->debugdir, dev,
				&ar0832_debugfs_fops);
	if (!ret)
		goto remove_debugfs;

	return;
remove_debugfs:
	dev_err(&i2c_client->dev, "couldn't create debugfs\n");
	ar0832_remove_debugfs(dev);
}

static int ar0832_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int err;
	struct ar0832_dev *dev = NULL;
	int ret;
	const char *mclk_name;

	dev_info(&client->dev, "ar0832: probing sensor.(id:%s)\n",
		id->name);

	dev = kzalloc(sizeof(struct ar0832_dev), GFP_KERNEL);
	if (!dev)
		goto probe_fail_release;

	dev->sensor_info = kzalloc(sizeof(struct ar0832_sensor_info),
					GFP_KERNEL);
	if (!dev->sensor_info)
		goto probe_fail_release;

	dev->focuser_info = kzalloc(sizeof(struct ar0832_focuser_info),
					GFP_KERNEL);
	if (!dev->focuser_info)
		goto probe_fail_release;

	/* sensor */
	dev->pdata = client->dev.platform_data;
	dev->i2c_client = client;

	/* focuser */
	dev->focuser_info->config.focuser_set[0].settle_time = SETTLE_TIME;
	dev->focuser_info->config.slew_rate = SLEW_RATE_DEFAULT;
	dev->focuser_info->config.pos_actual_low = POS_ACTUAL_LOW;
	dev->focuser_info->config.pos_actual_high = POS_ACTUAL_HIGH;
	dev->focuser_info->config.pos_working_low = POS_ACTUAL_LOW;
	dev->focuser_info->config.pos_working_high = POS_ACTUAL_HIGH;

	snprintf(dev->dname, sizeof(dev->dname), "%s-%s",
		id->name, dev->pdata->id);
	dev->misc_dev.minor = MISC_DYNAMIC_MINOR;
	dev->misc_dev.name = dev->dname;
	dev->misc_dev.fops = &ar0832_fileops;
	dev->misc_dev.mode = S_IRWXUGO;
	dev->misc_dev.parent = &client->dev;
	err = misc_register(&dev->misc_dev);
	if (err) {
		dev_err(&client->dev, "Unable to register misc device!\n");
		ret = -ENOMEM;
		goto probe_fail_free;
	}

	i2c_set_clientdata(client, dev);
	mutex_init(&dev->ar0832_camera_lock);

	mclk_name = dev->pdata->mclk_name ?
		    dev->pdata->mclk_name : "default_mclk";
	dev->mclk = devm_clk_get(&client->dev, mclk_name);
	if (IS_ERR(dev->mclk)) {
		dev_err(&client->dev, "%s: unable to get clock %s\n",
			__func__, mclk_name);
		ret = PTR_ERR(dev->mclk);
		goto probe_fail_free;
	}

	dev->power_rail.sen_1v8_reg = regulator_get(&client->dev, "vdd");
	if (IS_ERR(dev->power_rail.sen_1v8_reg)) {
		dev_err(&client->dev, "%s: failed to get vdd\n",
			__func__);
		ret = PTR_ERR(dev->power_rail.sen_1v8_reg);
		goto probe_fail_free;
	}

	dev->power_rail.sen_2v8_reg = regulator_get(&client->dev, "vaa");
	if (IS_ERR(dev->power_rail.sen_2v8_reg)) {
		dev_err(&client->dev, "%s: failed to get vaa\n",
			__func__);
		ret = PTR_ERR(dev->power_rail.sen_2v8_reg);
		regulator_put(dev->power_rail.sen_1v8_reg);
		dev->power_rail.sen_1v8_reg = NULL;
		goto probe_fail_free;
	}
	/* create debugfs interface */
	ar0832_create_debugfs(dev);

	return 0;

probe_fail_release:
	dev_err(&client->dev, "%s: unable to allocate memory!\n", __func__);
	ret = -ENOMEM;
probe_fail_free:
	if (dev) {
		kfree(dev->focuser_info);
		kfree(dev->sensor_info);
	}
	kfree(dev);
	return ret;
}

static int ar0832_remove(struct i2c_client *client)
{
	struct ar0832_dev *dev = i2c_get_clientdata(client);

	if (dev->power_rail.sen_1v8_reg)
		regulator_put(dev->power_rail.sen_1v8_reg);
	if (dev->power_rail.sen_2v8_reg)
		regulator_put(dev->power_rail.sen_2v8_reg);

	misc_deregister(&dev->misc_dev);
	kfree(dev->sensor_info);
	kfree(dev->focuser_info);

	ar0832_remove_debugfs(dev);

	kfree(dev);
	return 0;
}

static const struct i2c_device_id ar0832_id[] = {
	{ "ar0832", 0 },
	{ }
};

static struct i2c_driver ar0832_i2c_driver = {
	.probe = ar0832_probe,
	.remove = ar0832_remove,
	.id_table = ar0832_id,
	.driver = {
		.name = "ar0832",
		.owner = THIS_MODULE,
	},
};

static int __init ar0832_init(void)
{
	pr_info("%s: ++\n", __func__);
	return i2c_add_driver(&ar0832_i2c_driver);
}

static void __exit ar0832_exit(void)
{
	i2c_del_driver(&ar0832_i2c_driver);
}

module_init(ar0832_init);
module_exit(ar0832_exit);
MODULE_LICENSE("GPL v2");
