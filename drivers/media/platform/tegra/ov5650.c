/*
 * ov5650.c - ov5650 sensor driver
 *
 * Copyright (C) 2011 Google Inc.
 * Copyright (c) 2013-2014, NVIDIA CORPORATION, All Rights Reserved.
 *
 * Contributors:
 *      Rebecca Schultz Zavin <rebecca@android.com>
 *
 * Leverage OV9640.c
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>

#include <media/ov5650.h>
#include <video/tegra_camera.h>
#include <media/nvc.h>

#define SIZEOF_I2C_TRANSBUF 32

struct ov5650_reg {
	u16 addr;
	u16 val;
};

struct ov5650_sensor {
	struct i2c_client *i2c_client;
	struct ov5650_platform_data *pdata;
};

struct ov5650_info {
	int mode;
	enum StereoCameraMode camera_mode;
	struct clk *mclk;
	struct ov5650_sensor left;
	struct ov5650_sensor right;
	struct nvc_fuseid fuse_id;
	struct mutex mutex_le;
	struct mutex mutex_ri;
	int power_refcnt_le;
	int power_refcnt_ri;
	u8 i2c_trans_buf[SIZEOF_I2C_TRANSBUF];
};

static struct ov5650_info *stereo_ov5650_info;

#define OV5650_TABLE_WAIT_MS 0
#define OV5650_TABLE_END 1
#define OV5650_MAX_RETRIES 3
#define OV5650_FUSE_ID_SIZE 5

static struct ov5650_reg tp_none_seq[] = {
	{0x5046, 0x00},
	{OV5650_TABLE_END, 0x0000}
};

static struct ov5650_reg tp_cbars_seq[] = {
	{0x503D, 0xC0},
	{0x503E, 0x00},
	{0x5046, 0x01},
	{OV5650_TABLE_END, 0x0000}
};

static struct ov5650_reg tp_checker_seq[] = {
	{0x503D, 0xC0},
	{0x503E, 0x0A},
	{0x5046, 0x01},
	{OV5650_TABLE_END, 0x0000}
};

static struct ov5650_reg *test_pattern_modes[] = {
	tp_none_seq,
	tp_cbars_seq,
	tp_checker_seq,
};

static struct ov5650_reg reset_seq[] = {
	{0x3008, 0x82},
	{OV5650_TABLE_WAIT_MS, 5},
	{0x3008, 0x42},
	{OV5650_TABLE_WAIT_MS, 5},
	{OV5650_TABLE_END, 0x0000},
};

static struct ov5650_reg mode_start[] = {
	{0x3103, 0x93},
	{0x3017, 0xff},
	{0x3018, 0xfc},

	{0x3600, 0x50},
	{0x3601, 0x0d},
	{0x3604, 0x50},
	{0x3605, 0x04},
	{0x3606, 0x3f},
	{0x3612, 0x1a},
	{0x3630, 0x22},
	{0x3631, 0x22},
	{0x3702, 0x3a},
	{0x3704, 0x18},
	{0x3705, 0xda},
	{0x3706, 0x41},
	{0x370a, 0x80},
	{0x370b, 0x40},
	{0x370e, 0x00},
	{0x3710, 0x28},
	{0x3712, 0x13},
	{0x3830, 0x50},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3a00, 0x38},


	{0x3603, 0xa7},
	{0x3615, 0x50},
	{0x3620, 0x56},
	{0x3810, 0x00},
	{0x3836, 0x00},
	{0x3a1a, 0x06},
	{0x4000, 0x01},
	{0x401c, 0x48},
	{0x401d, 0x08},
	{0x5000, 0x06},
	{0x5001, 0x00},
	{0x5002, 0x00},
	{0x503d, 0x00},
	{0x5046, 0x00},

	{0x300f, 0x8f},

	{0x3010, 0x10},
	{0x3011, 0x14},
	{0x3012, 0x02},
	{0x3815, 0x82},
	{0x3503, 0x33},
	{0x3613, 0x44},
	{OV5650_TABLE_END, 0x0},
};

static struct ov5650_reg mode_2592x1944[] = {
	{0x3621, 0x2f},

	{0x3632, 0x55},
	{0x3703, 0xe6},
	{0x370c, 0xa0},
	{0x370d, 0x04},
	{0x3713, 0x2f},
	{0x3800, 0x02},
	{0x3801, 0x58},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x0a},
	{0x3805, 0x20},
	{0x3806, 0x07},
	{0x3807, 0xa0},
	{0x3808, 0x0a},

	{0x3809, 0x20},

	{0x380a, 0x07},

	{0x380b, 0xa0},

	{0x380c, 0x0c},

	{0x380d, 0xb4},

	{0x380e, 0x07},

	{0x380f, 0xb0},

	{0x3818, 0xc0},
	{0x381a, 0x3c},
	{0x3a0d, 0x06},
	{0x3c01, 0x00},
	{0x3007, 0x3f},
	{0x5059, 0x80},
	{0x3003, 0x03},
	{0x3500, 0x00},
	{0x3501, 0x7a},

	{0x3502, 0xd0},

	{0x350a, 0x00},
	{0x350b, 0x00},
	{0x401d, 0x08},
	{0x4801, 0x0f},
	{0x300e, 0x0c},
	{0x4803, 0x50},
	{0x4800, 0x34},
	{OV5650_TABLE_END, 0x0000}
};

static struct ov5650_reg mode_1296x972[] = {
	{0x3621, 0xaf},

	{0x3632, 0x5a},
	{0x3703, 0xb0},
	{0x370c, 0xc5},
	{0x370d, 0x42},
	{0x3713, 0x2f},
	{0x3800, 0x03},
	{0x3801, 0x3c},
	{0x3802, 0x00},
	{0x3803, 0x06},
	{0x3804, 0x05},
	{0x3805, 0x10},
	{0x3806, 0x03},
	{0x3807, 0xd0},
	{0x3808, 0x05},

	{0x3809, 0x10},

	{0x380a, 0x03},

	{0x380b, 0xd0},

	{0x380c, 0x08},

	{0x380d, 0xa8},

	{0x380e, 0x05},

	{0x380f, 0xa4},

	{0x3818, 0xc1},
	{0x381a, 0x00},
	{0x3a0d, 0x08},
	{0x3c01, 0x00},
	{0x3007, 0x3b},
	{0x5059, 0x80},
	{0x3003, 0x03},
	{0x3500, 0x00},

	{0x3501, 0x5a},
	{0x3502, 0x10},
	{0x350a, 0x00},
	{0x350b, 0x10},
	{0x401d, 0x08},
	{0x4801, 0x0f},
	{0x300e, 0x0c},
	{0x4803, 0x50},
	{0x4800, 0x34},
	{OV5650_TABLE_END, 0x0000}
};

static struct ov5650_reg mode_2080x1164[] = {
	{0x3103, 0x93},
	{0x3007, 0x3b},
	{0x3017, 0xff},
	{0x3018, 0xfc},

	{0x3600, 0x54},
	{0x3601, 0x05},
	{0x3603, 0xa7},
	{0x3604, 0x40},
	{0x3605, 0x04},
	{0x3606, 0x3f},
	{0x3612, 0x1a},
	{0x3613, 0x44},
	{0x3615, 0x52},
	{0x3620, 0x56},
	{0x3623, 0x01},
	{0x3630, 0x22},
	{0x3631, 0x36},
	{0x3632, 0x5f},
	{0x3633, 0x24},

	{0x3702, 0x3a},
	{0x3704, 0x18},
	{0x3706, 0x41},
	{0x370b, 0x40},
	{0x370e, 0x00},
	{0x3710, 0x28},
	{0x3711, 0x24},
	{0x3712, 0x13},

	{0x3810, 0x00},
	{0x3815, 0x82},
	{0x3830, 0x50},
	{0x3836, 0x00},

	{0x3a1a, 0x06},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3a00, 0x38},

	{0x3a0d, 0x06},
	{0x3c01, 0x34},

	{0x401f, 0x03},
	{0x4000, 0x05},
	{0x401d, 0x08},
	{0x4001, 0x02},

	{0x5001, 0x00},
	{0x5002, 0x00},
	{0x503d, 0x00},
	{0x5046, 0x00},

	{0x300f, 0x8f},

	{0x3010, 0x10},
	{0x3011, 0x14},
	{0x3012, 0x02},
	{0x3503, 0x33},


	{0x3621, 0x2f},

	{0x3703, 0xe6},
	{0x370c, 0x00},
	{0x370d, 0x04},
	{0x3713, 0x22},
	{0x3714, 0x27},
	{0x3705, 0xda},
	{0x370a, 0x80},

	{0x3800, 0x02},
	{0x3801, 0x12},
	{0x3802, 0x00},
	{0x3803, 0x0a},
	{0x3804, 0x08},
	{0x3805, 0x20},
	{0x3806, 0x04},
	{0x3807, 0x92},
	{0x3808, 0x08},

	{0x3809, 0x20},

	{0x380a, 0x04},

	{0x380b, 0x92},

	{0x380c, 0x0a},

	{0x380d, 0x96},

	{0x380e, 0x04},

	{0x380f, 0x9e},

	{0x3818, 0xc0},
	{0x381a, 0x3c},
	{0x381c, 0x31},
	{0x381d, 0x8e},
	{0x381e, 0x04},
	{0x381f, 0x92},
	{0x3820, 0x04},
	{0x3821, 0x19},
	{0x3824, 0x01},
	{0x3827, 0x0a},
	{0x401c, 0x46},

	{0x3003, 0x03},
	{0x3500, 0x00},
	{0x3501, 0x49},
	{0x3502, 0xa0},
	{0x350a, 0x00},
	{0x350b, 0x00},
	{0x4801, 0x0f},
	{0x300e, 0x0c},
	{0x4803, 0x50},
	{0x4800, 0x34},

	{OV5650_TABLE_END, 0x0000}
};

static struct ov5650_reg mode_1920x1080[] = {
	{0x3103, 0x93},
	{0x3007, 0x3b},
	{0x3017, 0xff},
	{0x3018, 0xfc},

	{0x3600, 0x54},
	{0x3601, 0x05},
	{0x3603, 0xa7},
	{0x3604, 0x40},
	{0x3605, 0x04},
	{0x3606, 0x3f},
	{0x3612, 0x1a},
	{0x3613, 0x44},
	{0x3615, 0x52},
	{0x3620, 0x56},
	{0x3623, 0x01},
	{0x3630, 0x22},
	{0x3631, 0x36},
	{0x3632, 0x5f},
	{0x3633, 0x24},

	{0x3702, 0x3a},
	{0x3704, 0x18},
	{0x3706, 0x41},
	{0x370b, 0x40},
	{0x370e, 0x00},
	{0x3710, 0x28},
	{0x3711, 0x24},
	{0x3712, 0x13},

	{0x3810, 0x00},
	{0x3815, 0x82},

	{0x3830, 0x50},
	{0x3836, 0x00},

	{0x3a1a, 0x06},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3a00, 0x38},
	{0x3a0d, 0x06},
	{0x3c01, 0x34},

	{0x401f, 0x03},
	{0x4000, 0x05},
	{0x401d, 0x08},
	{0x4001, 0x02},

	{0x5001, 0x00},
	{0x5002, 0x00},
	{0x503d, 0x00},
	{0x5046, 0x00},

	{0x300f, 0x8f},
	{0x3010, 0x10},
	{0x3011, 0x14},
	{0x3012, 0x02},
	{0x3503, 0x33},

	{0x3621, 0x2f},
	{0x3703, 0xe6},
	{0x370c, 0x00},
	{0x370d, 0x04},
	{0x3713, 0x22},
	{0x3714, 0x27},
	{0x3705, 0xda},
	{0x370a, 0x80},

	{0x3800, 0x02},
	{0x3801, 0x94},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x07},
	{0x3805, 0x80},
	{0x3806, 0x04},
	{0x3807, 0x40},
	{0x3808, 0x07},
	{0x3809, 0x80},
	{0x380a, 0x04},
	{0x380b, 0x40},
	{0x380c, 0x0a},
	{0x380d, 0x84},
	{0x380e, 0x04},
	{0x380f, 0xa4},
	{0x3818, 0xc0},
	{0x381a, 0x3c},
	{0x381c, 0x31},
	{0x381d, 0xa4},
	{0x381e, 0x04},
	{0x381f, 0x60},
	{0x3820, 0x03},
	{0x3821, 0x1a},
	{0x3824, 0x01},
	{0x3827, 0x0a},
	{0x401c, 0x46},

	{0x3003, 0x03},
	{0x3500, 0x00},
	{0x3501, 0x49},
	{0x3502, 0xa0},
	{0x350a, 0x00},
	{0x350b, 0x00},
	{0x4801, 0x0f},
	{0x300e, 0x0c},
	{0x4803, 0x50},
	{0x4800, 0x34},

	{OV5650_TABLE_END, 0x0000}
};


static struct ov5650_reg mode_1264x704[] = {
	{0x3600, 0x54},
	{0x3601, 0x05},
	{0x3604, 0x40},
	{0x3705, 0xdb},
	{0x370a, 0x81},
	{0x3615, 0x52},
	{0x3810, 0x40},
	{0x3836, 0x41},
	{0x4000, 0x05},
	{0x401c, 0x42},
	{0x401d, 0x08},
	{0x5046, 0x09},
	{0x3010, 0x00},
	{0x3503, 0x00},
	{0x3613, 0xc4},

	{0x3621, 0xaf},

	{0x3632, 0x55},
	{0x3703, 0x9a},
	{0x370c, 0x00},
	{0x370d, 0x42},
	{0x3713, 0x22},
	{0x3800, 0x02},
	{0x3801, 0x54},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x05},
	{0x3805, 0x00},
	{0x3806, 0x02},
	{0x3807, 0xd0},
	{0x3808, 0x05},

	{0x3809, 0x00},

	{0x380a, 0x02},

	{0x380b, 0xd0},

	{0x380c, 0x08},

	{0x380d, 0x72},

	{0x380e, 0x02},

	{0x380f, 0xe4},

	{0x3818, 0xc1},
	{0x381a, 0x3c},
	{0x3a0d, 0x06},
	{0x3c01, 0x34},
	{0x3007, 0x3b},
	{0x5059, 0x80},
	{0x3003, 0x03},
	{0x3500, 0x04},
	{0x3501, 0xa5},

	{0x3502, 0x10},

	{0x350a, 0x00},
	{0x350b, 0x00},
	{0x4801, 0x0f},
	{0x300e, 0x0c},
	{0x4803, 0x50},
	{0x4800, 0x24},
	{0x300f, 0x8b},

	{0x3711, 0x24},
	{0x3713, 0x92},
	{0x3714, 0x17},
	{0x381c, 0x10},
	{0x381d, 0x82},
	{0x381e, 0x05},
	{0x381f, 0xc0},
	{0x3821, 0x20},
	{0x3824, 0x23},
	{0x3825, 0x2c},
	{0x3826, 0x00},
	{0x3827, 0x0c},
	{0x3623, 0x01},
	{0x3633, 0x24},
	{0x3632, 0x5f},
	{0x401f, 0x03},

	{OV5650_TABLE_END, 0x0000}
};

static struct ov5650_reg mode_320x240[] = {
	{0x3103, 0x93},
	{0x3b07, 0x0c},
	{0x3017, 0xff},
	{0x3018, 0xfc},
	{0x3706, 0x41},
	{0x3613, 0xc4},
	{0x370d, 0x42},
	{0x3703, 0x9a},
	{0x3630, 0x22},
	{0x3605, 0x04},
	{0x3606, 0x3f},
	{0x3712, 0x13},
	{0x370e, 0x00},
	{0x370b, 0x40},
	{0x3600, 0x54},
	{0x3601, 0x05},
	{0x3713, 0x22},
	{0x3714, 0x27},
	{0x3631, 0x22},
	{0x3612, 0x1a},
	{0x3604, 0x40},
	{0x3705, 0xdc},
	{0x370a, 0x83},
	{0x370c, 0xc8},
	{0x3710, 0x28},
	{0x3702, 0x3a},
	{0x3704, 0x18},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3a00, 0x38},
	{0x3800, 0x02},
	{0x3801, 0x54},
	{0x3803, 0x0c},
	{0x380c, 0x0c},
	{0x380d, 0xb4},
	{0x380e, 0x07},
	{0x380f, 0xb0},
	{0x3830, 0x50},
	{0x3a08, 0x12},
	{0x3a09, 0x70},
	{0x3a0a, 0x0f},
	{0x3a0b, 0x60},
	{0x3a0d, 0x06},
	{0x3a0e, 0x06},
	{0x3a13, 0x54},
	{0x3815, 0x82},
	{0x5059, 0x80},
	{0x3615, 0x52},
	{0x505a, 0x0a},
	{0x505b, 0x2e},
	{0x3713, 0x92},
	{0x3714, 0x17},
	{0x3803, 0x0a},
	{0x3804, 0x05},
	{0x3805, 0x00},
	{0x3806, 0x01},
	{0x3807, 0x00},
	{0x3808, 0x01},
	{0x3809, 0x40},
	{0x380a, 0x01},
	{0x380b, 0x00},
	{0x380c, 0x0a},

	{0x380d, 0x04},

	{0x380e, 0x01},

	{0x380f, 0x38},

	{0x3500, 0x00},
	{0x3501, 0x13},
	{0x3502, 0x80},
	{0x350b, 0x7f},

	{0x3815, 0x81},
	{0x3824, 0x23},
	{0x3825, 0x20},
	{0x3826, 0x00},
	{0x3827, 0x08},
	{0x370d, 0xc2},
	{0x3a08, 0x17},
	{0x3a09, 0x64},
	{0x3a0a, 0x13},
	{0x3a0b, 0x80},
	{0x3a00, 0x58},
	{0x3a1a, 0x06},
	{0x3503, 0x33},
	{0x3623, 0x01},
	{0x3633, 0x24},
	{0x3c01, 0x34},
	{0x3c04, 0x28},
	{0x3c05, 0x98},
	{0x3c07, 0x07},
	{0x3c09, 0xc2},
	{0x4000, 0x05},
	{0x401d, 0x08},
	{0x4001, 0x02},
	{0x401c, 0x42},
	{0x5046, 0x09},
	{0x3810, 0x40},
	{0x3836, 0x41},
	{0x505f, 0x04},
	{0x5001, 0x00},
	{0x5002, 0x02},
	{0x503d, 0x00},
	{0x5901, 0x08},
	{0x585a, 0x01},
	{0x585b, 0x2c},
	{0x585c, 0x01},
	{0x585d, 0x93},
	{0x585e, 0x01},
	{0x585f, 0x90},
	{0x5860, 0x01},
	{0x5861, 0x0d},
	{0x5180, 0xc0},
	{0x5184, 0x00},
	{0x470a, 0x00},
	{0x470b, 0x00},
	{0x470c, 0x00},
	{0x300f, 0x8e},
	{0x3603, 0xa7},
	{0x3632, 0x55},
	{0x3620, 0x56},
	{0x3621, 0xaf},
	{0x3818, 0xc3},
	{0x3631, 0x36},
	{0x3632, 0x5f},
	{0x3711, 0x24},
	{0x401f, 0x03},

	{0x3011, 0x14},
	{0x3007, 0x3B},
	{0x300f, 0x8f},
	{0x4801, 0x0f},
	{0x3003, 0x03},
	{0x300e, 0x0c},
	{0x3010, 0x15},
	{0x4803, 0x50},
	{0x4800, 0x24},
	{0x4837, 0x40},
	{0x3815, 0x82},

	{OV5650_TABLE_END, 0x0000}
};

static struct ov5650_reg mode_end[] = {
	{0x3212, 0x00},
	{0x3003, 0x01},
	{0x3212, 0x10},
	{0x3212, 0xa0},
	{0x3008, 0x02},

	{OV5650_TABLE_END, 0x0000}
};

enum {
	OV5650_MODE_2592x1944,
	OV5650_MODE_1296x972,
	OV5650_MODE_2080x1164,
	OV5650_MODE_1920x1080,
	OV5650_MODE_1264x704,
	OV5650_MODE_320x240,
	OV5650_MODE_INVALID
};

static struct ov5650_reg *mode_table[] = {
	[OV5650_MODE_2592x1944] = mode_2592x1944,
	[OV5650_MODE_1296x972]  = mode_1296x972,
	[OV5650_MODE_2080x1164] = mode_2080x1164,
	[OV5650_MODE_1920x1080] = mode_1920x1080,
	[OV5650_MODE_1264x704]  = mode_1264x704,
	[OV5650_MODE_320x240]   = mode_320x240
};

static inline void ov5650_get_frame_length_regs(struct ov5650_reg *regs,
						u32 frame_length)
{
	regs->addr = 0x380e;
	regs->val = (frame_length >> 8) & 0xff;
	(regs + 1)->addr = 0x380f;
	(regs + 1)->val = (frame_length) & 0xff;
}

static inline void ov5650_get_coarse_time_regs(struct ov5650_reg *regs,
						u32 coarse_time)
{
	regs->addr = 0x3500;
	regs->val = (coarse_time >> 12) & 0xff;
	(regs + 1)->addr = 0x3501;
	(regs + 1)->val = (coarse_time >> 4) & 0xff;
	(regs + 2)->addr = 0x3502;
	(regs + 2)->val = (coarse_time & 0xf) << 4;
}

static inline void ov5650_get_gain_reg(struct ov5650_reg *regs, u16 gain)
{
	regs->addr = 0x350b;
	regs->val = gain;
}

static int ov5650_read_reg(struct i2c_client *client, u16 addr, u8 *val)
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

	/* high byte goes out first */
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

static int ov5650_read_reg_helper(struct ov5650_info *info,
					u16 addr, u8 *val)
{
	int ret;
	switch (info->camera_mode) {
	case Main:
	case StereoCameraMode_Left:
		ret = ov5650_read_reg(info->left.i2c_client, addr, val);
		break;
	case StereoCameraMode_Stereo:
		ret = ov5650_read_reg(info->left.i2c_client, addr, val);
		if (ret)
			break;
		ret = ov5650_read_reg(info->right.i2c_client, addr, val);
		break;
	case StereoCameraMode_Right:
		ret = ov5650_read_reg(info->right.i2c_client, addr, val);
		break;
	default:
		return -1;
	}
	return ret;
}

static int ov5650_write_reg(struct i2c_client *client, u16 addr, u8 val)
{
	int err;
	struct i2c_msg msg;
	unsigned char data[3];

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);
	data[2] = (u8) (val & 0xff);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = data;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err == 1)
		return 0;

	pr_err("ov5650: i2c transfer failed, retrying %x %x\n",	addr, val);

	return err;
}

static int ov5650_write_reg_helper(struct ov5650_info *info,
					u16 addr, u8 val)
{
	int ret;
	switch (info->camera_mode) {
	case Main:
	case StereoCameraMode_Left:
		ret = ov5650_write_reg(info->left.i2c_client, addr, val);
		break;
	case StereoCameraMode_Stereo:
		ret = ov5650_write_reg(info->left.i2c_client, addr, val);
		if (ret)
			break;
		ret = ov5650_write_reg(info->right.i2c_client, addr, val);
		break;
	case StereoCameraMode_Right:
		ret = ov5650_write_reg(info->right.i2c_client, addr, val);
		break;
	default:
		return -1;
	}
	return ret;
}

static int ov5650_write_bulk_reg(struct i2c_client *client, u8 *data, int len)
{
	int err;
	struct i2c_msg msg;

	if (!client->adapter)
		return -ENODEV;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = data;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err == 1)
		return 0;

	pr_err("ov5650: i2c bulk transfer failed at %x\n",
		(int)data[0] << 8 | data[1]);

	return err;
}

static int ov5650_write_bulk_reg_helper(struct ov5650_info *info, int len)
{
	int ret;
	switch (info->camera_mode) {
	case Main:
	case StereoCameraMode_Left:
		ret = ov5650_write_bulk_reg(info->left.i2c_client,
					info->i2c_trans_buf, len);
		break;
	case StereoCameraMode_Stereo:
		ret = ov5650_write_bulk_reg(info->left.i2c_client,
					info->i2c_trans_buf, len);
		if (ret)
			break;
		ret = ov5650_write_bulk_reg(info->right.i2c_client,
					info->i2c_trans_buf, len);
		break;
	case StereoCameraMode_Right:
		ret = ov5650_write_bulk_reg(info->right.i2c_client,
					info->i2c_trans_buf, len);
		break;
	default:
		return -1;
	}
	return ret;
}

static int ov5650_write_table(struct ov5650_info *info,
				const struct ov5650_reg table[],
				const struct ov5650_reg override_list[],
				int num_override_regs)
{
	int err;
	const struct ov5650_reg *next, *n_next;
	u8 *b_ptr = info->i2c_trans_buf;
	unsigned int buf_filled = 0;
	unsigned int i;
	u16 val;

	for (next = table; next->addr != OV5650_TABLE_END; next++) {
		if (next->addr == OV5650_TABLE_WAIT_MS) {
			msleep(next->val);
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

		if (!buf_filled) {
			b_ptr = info->i2c_trans_buf;
			*b_ptr++ = next->addr >> 8;
			*b_ptr++ = next->addr & 0xff;
			buf_filled = 2;
		}
		*b_ptr++ = val;
		buf_filled++;

		n_next = next + 1;
		if (n_next->addr != OV5650_TABLE_END &&
			n_next->addr != OV5650_TABLE_WAIT_MS &&
			buf_filled < SIZEOF_I2C_TRANSBUF &&
			n_next->addr == next->addr + 1) {
			continue;
		}

		err = ov5650_write_bulk_reg_helper(info, buf_filled);
		if (err)
			return err;

		buf_filled = 0;
	}
	return 0;
}

static int ov5650_set_mode(struct ov5650_info *info, struct ov5650_mode *mode)
{
	int sensor_mode;
	int err;
	struct ov5650_reg reg_list[6];

	pr_info("%s: xres %u yres %u framelength %u coarsetime %u gain %u\n",
		__func__, mode->xres, mode->yres, mode->frame_length,
		mode->coarse_time, mode->gain);
	if (mode->xres == 2592 && mode->yres == 1944)
		sensor_mode = OV5650_MODE_2592x1944;
	else if (mode->xres == 1296 && mode->yres == 972)
		sensor_mode = OV5650_MODE_1296x972;
	else if (mode->xres == 2080 && mode->yres == 1164)
		sensor_mode = OV5650_MODE_2080x1164;
	else if (mode->xres == 1920 && mode->yres == 1080)
		sensor_mode = OV5650_MODE_1920x1080;
	else if (mode->xres == 1264 && mode->yres == 704)
		sensor_mode = OV5650_MODE_1264x704;
	else if (mode->xres == 320 && mode->yres == 240)
		sensor_mode = OV5650_MODE_320x240;
	else {
		pr_err("%s: invalid resolution supplied to set mode %d %d\n",
		       __func__, mode->xres, mode->yres);
		return -EINVAL;
	}

	/* get a list of override regs for the asking frame length, */
	/* coarse integration time, and gain.                       */
	ov5650_get_frame_length_regs(reg_list, mode->frame_length);
	ov5650_get_coarse_time_regs(reg_list + 2, mode->coarse_time);
	ov5650_get_gain_reg(reg_list + 5, mode->gain);

	err = ov5650_write_table(info, reset_seq, NULL, 0);
	if (err)
		return err;

	err = ov5650_write_table(info, mode_start, NULL, 0);
	if (err)
		return err;

	err = ov5650_write_table(info, mode_table[sensor_mode],
		reg_list, 6);
	if (err)
		return err;

	err = ov5650_write_table(info, mode_end, NULL, 0);
	if (err)
		return err;

	info->mode = sensor_mode;
	return 0;
}

static int ov5650_set_frame_length(struct ov5650_info *info, u32 frame_length)
{
	int ret;
	struct ov5650_reg reg_list[2];
	u8 *b_ptr = info->i2c_trans_buf;

	ov5650_get_frame_length_regs(reg_list, frame_length);

	*b_ptr++ = reg_list[0].addr >> 8;
	*b_ptr++ = reg_list[0].addr & 0xff;
	*b_ptr++ = reg_list[0].val & 0xff;
	*b_ptr++ = reg_list[1].val & 0xff;
	ret = ov5650_write_bulk_reg_helper(info, 4);

	return ret;
}

static int ov5650_set_coarse_time(struct ov5650_info *info, u32 coarse_time)
{
	int ret;
	struct ov5650_reg reg_list[3];
	u8 *b_ptr = info->i2c_trans_buf;

	ov5650_get_coarse_time_regs(reg_list, coarse_time);

	*b_ptr++ = reg_list[0].addr >> 8;
	*b_ptr++ = reg_list[0].addr & 0xff;
	*b_ptr++ = reg_list[0].val & 0xff;
	*b_ptr++ = reg_list[1].val & 0xff;
	*b_ptr++ = reg_list[2].val & 0xff;
	ret = ov5650_write_bulk_reg_helper(info, 5);

	return ret;
}

static int ov5650_set_gain(struct ov5650_info *info, u16 gain)
{
	int ret;
	struct ov5650_reg reg_list;

	ov5650_get_gain_reg(&reg_list, gain);
	ret = ov5650_write_reg_helper(info, reg_list.addr, reg_list.val);

	return ret;
}

static int ov5650_set_group_hold(struct ov5650_info *info, struct ov5650_ae *ae)
{
	int ret;
	int count = 0;
	bool groupHoldEnabled = false;

	if (ae->gain_enable)
		count++;
	if (ae->coarse_time_enable)
		count++;
	if (ae->frame_length_enable)
		count++;
	if (count >= 2)
		groupHoldEnabled = true;

	if (groupHoldEnabled) {
		ret = ov5650_write_reg_helper(info, 0x3212, 0x01);
		if (ret)
			return ret;
	}

	if (ae->gain_enable)
		ov5650_set_gain(info, ae->gain);
	if (ae->coarse_time_enable)
		ov5650_set_coarse_time(info, ae->coarse_time);
	if (ae->frame_length_enable)
		ov5650_set_frame_length(info, ae->frame_length);

	if (groupHoldEnabled) {
		ret = ov5650_write_reg_helper(info, 0x3212, 0x11);
		if (ret)
			return ret;

		ret = ov5650_write_reg_helper(info, 0x3212, 0xa1);
		if (ret)
			return ret;
	}

	return 0;
}


static int ov5650_set_binning(struct ov5650_info *info, u8 enable)
{
	s32 ret;
	u8  array_ctrl_reg, analog_ctrl_reg, timing_reg;
	u32 val;

	if (info->mode == OV5650_MODE_2592x1944
	 || info->mode == OV5650_MODE_2080x1164
	 || info->mode >= OV5650_MODE_INVALID) {
		return -EINVAL;
	}

	ov5650_read_reg_helper(info, OV5650_ARRAY_CONTROL_01, &array_ctrl_reg);
	ov5650_read_reg_helper(info, OV5650_ANALOG_CONTROL_D, &analog_ctrl_reg);
	ov5650_read_reg_helper(info, OV5650_TIMING_TC_REG_18, &timing_reg);

	ret = ov5650_write_reg_helper(info,
			OV5650_SRM_GRUP_ACCESS,
			OV5650_GROUP_ID(3));
	if (ret < 0)
		return -EIO;

	if (!enable) {
		ret = ov5650_write_reg_helper(info,
			OV5650_ARRAY_CONTROL_01,
			array_ctrl_reg |
			(OV5650_H_BINNING_BIT | OV5650_H_SUBSAMPLING_BIT));

		if (ret < 0)
			goto exit;

		ret = ov5650_write_reg_helper(info,
			OV5650_ANALOG_CONTROL_D,
			analog_ctrl_reg & ~OV5650_V_BINNING_BIT);

		if (ret < 0)
			goto exit;

		ret = ov5650_write_reg_helper(info,
			OV5650_TIMING_TC_REG_18,
			timing_reg | OV5650_V_SUBSAMPLING_BIT);

		if (ret < 0)
			goto exit;

		if (info->mode == OV5650_MODE_1296x972)
			val = 0x1A2;
		else
			/* FIXME: this value is not verified yet. */
			val = 0x1A8;

		ret = ov5650_write_reg_helper(info,
			OV5650_TIMING_CONTROL_HS_HIGH,
			(val >> 8));

		if (ret < 0)
			goto exit;

		ret = ov5650_write_reg_helper(info,
			OV5650_TIMING_CONTROL_HS_LOW,
			(val & 0xFF));
	} else {
		ret = ov5650_write_reg_helper(info,
			OV5650_ARRAY_CONTROL_01,
			(array_ctrl_reg | OV5650_H_BINNING_BIT)
			& ~OV5650_H_SUBSAMPLING_BIT);

		if (ret < 0)
			goto exit;

		ret = ov5650_write_reg_helper(info,
			OV5650_ANALOG_CONTROL_D,
			analog_ctrl_reg | OV5650_V_BINNING_BIT);

		if (ret < 0)
			goto exit;

		ret = ov5650_write_reg_helper(info,
			OV5650_TIMING_TC_REG_18,
			timing_reg | OV5650_V_SUBSAMPLING_BIT);

		if (ret < 0)
			goto exit;

		if (info->mode == OV5650_MODE_1296x972)
			val = 0x33C;
		else
			val = 0x254;

		ret = ov5650_write_reg_helper(info,
			OV5650_TIMING_CONTROL_HS_HIGH,
			(val >> 8));

		if (ret < 0)
			goto exit;

		ret = ov5650_write_reg_helper(info,
			OV5650_TIMING_CONTROL_HS_LOW,
			(val & 0xFF));
	}

exit:
	ret = ov5650_write_reg_helper(info,
		OV5650_SRM_GRUP_ACCESS,
		(OV5650_GROUP_HOLD_END_BIT | OV5650_GROUP_ID(3)));

	ret |= ov5650_write_reg_helper(info,
		OV5650_SRM_GRUP_ACCESS,
		(OV5650_GROUP_HOLD_BIT | OV5650_GROUP_LAUNCH_BIT |
		OV5650_GROUP_ID(3)));

	return ret;
}

static int ov5650_test_pattern(struct ov5650_info *info,
			       enum ov5650_test_pattern pattern)
{
	if (pattern >= ARRAY_SIZE(test_pattern_modes))
		return -EINVAL;

	return ov5650_write_table(info,
				  test_pattern_modes[pattern],
				  NULL, 0);
}

static int set_power_helper(struct ov5650_platform_data *pdata,
			struct device *dev, int powerLevel, int *ref_cnt)
{
	if (pdata) {
		if (powerLevel && pdata->power_on) {
			if (*ref_cnt == 0)
				pdata->power_on(dev);
			*ref_cnt = *ref_cnt + 1;
		}
		else if (pdata->power_off) {
			*ref_cnt = *ref_cnt - 1;
			if (*ref_cnt <= 0)
				pdata->power_off(dev);
		}
	}
	return 0;
}

static void ov5650_mclk_disable(struct ov5650_info *info)
{
	pr_info("%s: disable MCLK\n", __func__);
	clk_disable_unprepare(info->mclk);
}

static int ov5650_mclk_enable(struct ov5650_info *info)
{
	int err;
	unsigned long mclk_init_rate = 24000000;

	pr_info("%s: enable MCLK with %lu Hz\n", __func__, mclk_init_rate);

	err = clk_set_rate(info->mclk, mclk_init_rate);
	if (!err)
		err = clk_prepare_enable(info->mclk);
	return err;
}

static int ov5650_set_power(struct ov5650_info *info, int powerLevel)
{
	pr_info("%s: powerLevel=%d camera mode=%d\n", __func__, powerLevel,
			info->camera_mode);

	if (powerLevel) {
		int err = ov5650_mclk_enable(info);
		if (err < 0)
			return err;
	}

	if (StereoCameraMode_Left & info->camera_mode) {
		mutex_lock(&info->mutex_le);
		set_power_helper(info->left.pdata, &info->left.i2c_client->dev,
			powerLevel, &info->power_refcnt_le);
		mutex_unlock(&info->mutex_le);
	}

	if (StereoCameraMode_Right & info->camera_mode) {
		mutex_lock(&info->mutex_ri);
		set_power_helper(info->right.pdata, &info->right.i2c_client->dev,
			powerLevel, &info->power_refcnt_ri);
		mutex_unlock(&info->mutex_ri);
	}

	if (!powerLevel)
		ov5650_mclk_disable(info);

	return 0;
}

static int ov5650_get_fuseid(struct ov5650_info *info)
{
	int ret = 0;
	int i;
	u8  bak;

	pr_info("%s\n", __func__);
	if (info->fuse_id.size)
		return 0;

	ov5650_set_power(info, 1);

	for (i = 0; i < OV5650_FUSE_ID_SIZE; i++) {
		ret |= ov5650_write_reg_helper(info, 0x3d00, i);
		ret |= ov5650_read_reg_helper(info, 0x3d04,
				&bak);
		info->fuse_id.data[i] = bak;
	}

	if (!ret)
		info->fuse_id.size = i;

	ov5650_set_power(info, 0);
	return ret;
}

static long ov5650_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	int err;
	struct ov5650_info *info = file->private_data;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(OV5650_IOCTL_SET_CAMERA_MODE):
	{
		if (info->camera_mode != arg) {
			err = ov5650_set_power(info, 0);
			if (err) {
				pr_info("%s %d\n", __func__, __LINE__);
				return err;
			}
			info->camera_mode = arg;
			err = ov5650_set_power(info, 1);
			if (err)
				return err;
		}
		return 0;
	}
	case _IOC_NR(OV5650_IOCTL_SYNC_SENSORS):
		if (info->right.pdata->synchronize_sensors)
			info->right.pdata->synchronize_sensors();
		return 0;
	case _IOC_NR(OV5650_IOCTL_SET_MODE):
	{
		struct ov5650_mode mode;
		if (copy_from_user(&mode,
				   (const void __user *)arg,
				   sizeof(struct ov5650_mode))) {
			pr_info("%s %d\n", __func__, __LINE__);
			return -EFAULT;
		}

		return ov5650_set_mode(info, &mode);
	}
	case _IOC_NR(OV5650_IOCTL_SET_FRAME_LENGTH):
		return ov5650_set_frame_length(info, (u32)arg);
	case _IOC_NR(OV5650_IOCTL_SET_COARSE_TIME):
		return ov5650_set_coarse_time(info, (u32)arg);
	case _IOC_NR(OV5650_IOCTL_SET_GAIN):
		return ov5650_set_gain(info, (u16)arg);
	case _IOC_NR(OV5650_IOCTL_SET_BINNING):
		return ov5650_set_binning(info, (u8)arg);
	case _IOC_NR(OV5650_IOCTL_GET_STATUS):
	{
		u16 status = 0;
		if (copy_to_user((void __user *)arg, &status,
				 2)) {
			pr_info("%s %d\n", __func__, __LINE__);
			return -EFAULT;
		}
		return 0;
	}
	case _IOC_NR(OV5650_IOCTL_TEST_PATTERN):
	{
		err = ov5650_test_pattern(info, (enum ov5650_test_pattern) arg);
		if (err)
			pr_err("%s %d %d\n", __func__, __LINE__, err);
		return err;
	}
	case _IOC_NR(OV5650_IOCTL_SET_GROUP_HOLD):
	{
		struct ov5650_ae ae;
		if (copy_from_user(&ae,
				(const void __user *)arg,
				sizeof(struct ov5650_ae))) {
			pr_info("%s %d\n", __func__, __LINE__);
			return -EFAULT;
		}
		return ov5650_set_group_hold(info, &ae);
	}
	case _IOC_NR(OV5650_IOCTL_GET_FUSEID):
	{
		err = ov5650_get_fuseid(info);
		if (err) {
			pr_err("%s %d %d\n", __func__, __LINE__, err);
			return err;
		}
		if (copy_to_user((void __user *)arg,
				&info->fuse_id,
				sizeof(struct nvc_fuseid))) {
			pr_err("%s: %d: fail copy fuse id to user space\n",
				__func__, __LINE__);
			return -EFAULT;
		}
		return 0;
	}
	default:
		return -EINVAL;
	}
	return 0;
}

static int ov5650_open(struct inode *inode, struct file *file)
{
	pr_info("%s\n", __func__);
	file->private_data = stereo_ov5650_info;
	ov5650_set_power(stereo_ov5650_info, 1);
	return 0;
}

int ov5650_release(struct inode *inode, struct file *file)
{
	struct ov5650_info *info = file->private_data;

	ov5650_set_power(info, 0);
	file->private_data = NULL;
	return 0;
}


static const struct file_operations ov5650_fileops = {
	.owner = THIS_MODULE,
	.open = ov5650_open,
	.unlocked_ioctl = ov5650_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ov5650_ioctl,
#endif
	.release = ov5650_release,
};

static struct miscdevice ov5650_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ov5650",
	.fops = &ov5650_fileops,
};

static int ov5650_probe_common(struct i2c_client *client)
{
	int err;

	if (!stereo_ov5650_info) {
		stereo_ov5650_info = kzalloc(sizeof(struct ov5650_info),
					GFP_KERNEL);
		if (!stereo_ov5650_info) {
			pr_err("ov5650: Unable to allocate memory!\n");
			return -ENOMEM;
		}

		stereo_ov5650_info->mclk = devm_clk_get(&client->dev,
			"default_mclk");
		if (IS_ERR(stereo_ov5650_info->mclk)) {
			dev_err(&client->dev, "%s: unable to get mclk\n",
				__func__);
			return PTR_ERR(stereo_ov5650_info->mclk);
		}

		err = misc_register(&ov5650_device);
		if (err) {
			pr_err("ov5650: Unable to register misc device!\n");
			kfree(stereo_ov5650_info);
			return err;
		}
	}
	return 0;
}

static int ov5650_remove_common(struct i2c_client *client)
{
	if (stereo_ov5650_info->left.i2c_client ||
		stereo_ov5650_info->right.i2c_client)
		return 0;

	misc_deregister(&ov5650_device);
	kfree(stereo_ov5650_info);
	stereo_ov5650_info = NULL;

	return 0;
}

static int left_ov5650_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err;
	pr_info("%s: probing sensor.\n", __func__);

	err = ov5650_probe_common(client);
	if (err)
		return err;

	stereo_ov5650_info->left.pdata = client->dev.platform_data;
	stereo_ov5650_info->left.i2c_client = client;
	mutex_init(&stereo_ov5650_info->mutex_le);
	mutex_init(&stereo_ov5650_info->mutex_ri);

	return 0;
}

static int left_ov5650_remove(struct i2c_client *client)
{
	if (stereo_ov5650_info) {
		stereo_ov5650_info->left.i2c_client = NULL;
		ov5650_remove_common(client);
	}
	return 0;
}

static const struct i2c_device_id left_ov5650_id[] = {
	{ "ov5650", 0 },
	{ "ov5650L", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, left_ov5650_id);

static struct i2c_driver left_ov5650_i2c_driver = {
	.driver = {
		.name = "ov5650",
		.owner = THIS_MODULE,
	},
	.probe = left_ov5650_probe,
	.remove = left_ov5650_remove,
	.id_table = left_ov5650_id,
};

static int right_ov5650_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err;
	pr_info("%s: probing sensor.\n", __func__);

	err = ov5650_probe_common(client);
	if (err)
		return err;

	stereo_ov5650_info->right.pdata = client->dev.platform_data;
	stereo_ov5650_info->right.i2c_client = client;

	return 0;
}

static int right_ov5650_remove(struct i2c_client *client)
{
	if (stereo_ov5650_info) {
		stereo_ov5650_info->right.i2c_client = NULL;
		ov5650_remove_common(client);
	}
	return 0;
}

static const struct i2c_device_id right_ov5650_id[] = {
	{ "ov5650R", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, right_ov5650_id);

static struct i2c_driver right_ov5650_i2c_driver = {
	.driver = {
		.name = "ov5650R",
		.owner = THIS_MODULE,
	},
	.probe = right_ov5650_probe,
	.remove = right_ov5650_remove,
	.id_table = right_ov5650_id,
};

static int __init ov5650_init(void)
{
	int ret;
	pr_info("ov5650 sensor driver loading\n");
	ret = i2c_add_driver(&left_ov5650_i2c_driver);
	if (ret)
		return ret;
	return i2c_add_driver(&right_ov5650_i2c_driver);
}

static void __exit ov5650_exit(void)
{
	i2c_del_driver(&right_ov5650_i2c_driver);
	i2c_del_driver(&left_ov5650_i2c_driver);
}

module_init(ov5650_init);
module_exit(ov5650_exit);
MODULE_LICENSE("GPL v2");
