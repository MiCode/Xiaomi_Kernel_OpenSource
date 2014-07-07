/*
 * Support for OmniVision OV5693 5M camera sensor.
 *
 * Copyright (c) 2013 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef __OV5693_H__
#define __OV5693_H__
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/spinlock.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <linux/v4l2-mediabus.h>
#include <media/media-entity.h>

#include <linux/atomisp_platform.h>

#define OV5693_NAME		"ov5693"

/* Defines for register writes and register array processing */
#define I2C_MSG_LENGTH		0x2
#define I2C_RETRY_COUNT		5

#define OV5693_FOCAL_LENGTH_NUM	334	/*3.34mm*/
#define OV5693_FOCAL_LENGTH_DEM	100
#define OV5693_F_NUMBER_DEFAULT_NUM	24
#define OV5693_F_NUMBER_DEM	10

#define MAX_FMTS		1

/* sensor_mode_data read_mode adaptation */
#define OV5693_READ_MODE_BINNING_ON	0x0400
#define OV5693_READ_MODE_BINNING_OFF	0x00
#define OV5693_INTEGRATION_TIME_MARGIN	8

#define OV5693_MAX_EXPOSURE_VALUE	0xFFF1
#define OV5693_MAX_GAIN_VALUE		0xFF

/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define OV5693_FOCAL_LENGTH_DEFAULT 0x1B70064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define OV5693_F_NUMBER_DEFAULT 0x18000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define OV5693_F_NUMBER_RANGE 0x180a180a
#define OV5693_ID	0x5690

#define OV5693_FINE_INTG_TIME_MIN 0
#define OV5693_FINE_INTG_TIME_MAX_MARGIN 0
#define OV5693_COARSE_INTG_TIME_MIN 1
#define OV5693_COARSE_INTG_TIME_MAX_MARGIN (0xffff - 6)

#define OV5693_BIN_FACTOR_MAX 4
/*
 * OV5693 System control registers
 */
#define OV5693_SW_SLEEP				0x0100
#define OV5693_SW_RESET				0x0103
#define OV5693_SW_STREAM			0x0100

#define OV5693_SC_CMMN_CHIP_ID_H		0x300A
#define OV5693_SC_CMMN_CHIP_ID_L		0x300B
#define OV5693_SC_CMMN_SCCB_ID			0x300C
#define OV5693_SC_CMMN_SUB_ID			0x302A /* process, version*/

#define OV5693_GROUP_ACCESS							0x3208 /*Bit[7:4] Group control, Bit[3:0] Group ID*/

#define OV5693_EXPOSURE_H							0x3500 /*Bit[3:0] Bit[19:16] of exposure, remaining 16 bits lies in Reg0x3501&Reg0x3502*/
#define OV5693_EXPOSURE_M							0x3501
#define OV5693_EXPOSURE_L							0x3502
#define OV5693_AGC_H								0x350A /*Bit[1:0] means Bit[9:8] of gain*/
#define OV5693_AGC_L								0x350B /*Bit[7:0] of gain*/

#define OV5693_HORIZONTAL_START_H					0x3800 /*Bit[11:8]*/
#define OV5693_HORIZONTAL_START_L					0x3801 /*Bit[7:0]*/
#define OV5693_VERTICAL_START_H						0x3802 /*Bit[11:8]*/
#define OV5693_VERTICAL_START_L						0x3803 /*Bit[7:0]*/
#define OV5693_HORIZONTAL_END_H						0x3804 /*Bit[11:8]*/
#define OV5693_HORIZONTAL_END_L						0x3805 /*Bit[7:0]*/
#define OV5693_VERTICAL_END_H						0x3806 /*Bit[11:8]*/
#define OV5693_VERTICAL_END_L						0x3807 /*Bit[7:0]*/
#define OV5693_HORIZONTAL_OUTPUT_SIZE_H				0x3808 /*Bit[3:0]*/
#define OV5693_HORIZONTAL_OUTPUT_SIZE_L				0x3809 /*Bit[7:0]*/
#define OV5693_VERTICAL_OUTPUT_SIZE_H				0x380a /*Bit[3:0]*/
#define OV5693_VERTICAL_OUTPUT_SIZE_L				0x380b /*Bit[7:0]*/
#define OV5693_TIMING_HTS_H							0x380C  /*High 8-bit, and low 8-bit HTS address is 0x380d*/
#define OV5693_TIMING_HTS_L							0x380D  /*High 8-bit, and low 8-bit HTS address is 0x380d*/
#define OV5693_TIMING_VTS_H							0x380e  /*High 8-bit, and low 8-bit HTS address is 0x380f*/
#define OV5693_TIMING_VTS_L							0x380f  /*High 8-bit, and low 8-bit HTS address is 0x380f*/

#define OV5693_MWB_RED_GAIN_H			0x3400
#define OV5693_MWB_GREEN_GAIN_H			0x3402
#define OV5693_MWB_BLUE_GAIN_H			0x3404
#define OV5693_MWB_GAIN_MAX				0x0fff

#define OV5693_START_STREAMING			0x01
#define OV5693_STOP_STREAMING			0x00

#define VCM_ADDR           0x0c
#define VCM_CODE_MSB       0x04

#define OV5693_INVALID_CONFIG	0xffffffff

#define OV5693_VCM_SLEW_STEP			0x30F0
#define OV5693_VCM_SLEW_STEP_MAX		0x7
#define OV5693_VCM_SLEW_STEP_MASK		0x7
#define OV5693_VCM_CODE				0x30F2
#define OV5693_VCM_SLEW_TIME			0x30F4
#define OV5693_VCM_SLEW_TIME_MAX		0xffff
#define OV5693_VCM_ENABLE			0x8000

#define OV5693_VCM_MAX_FOCUS_NEG       -1023
#define OV5693_VCM_MAX_FOCUS_POS       1023

struct regval_list {
	u16 reg_num;
	u8 value;
};

struct ov5693_resolution {
	u8 *desc;
	const struct ov5693_reg *regs;
	int res;
	int width;
	int height;
	int fps;
	int pix_clk_freq;
	u16 pixels_per_line;
	u16 lines_per_frame;
	u8 bin_factor_x;
	u8 bin_factor_y;
	u8 bin_mode;
	bool used;
};

struct ov5693_format {
	u8 *desc;
	u32 pixelformat;
	struct ov5693_reg *regs;
};

struct ov5693_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, s32 value);
};

/*
 * ov5693 device structure.
 */
struct ov5693_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;
	struct mutex input_lock;

	struct camera_sensor_platform_data *platform_data;
	struct timespec timestamp_t_focus_abs;
	int vt_pix_clk_freq_mhz;
	int fmt_idx;
	int run_mode;
	u32 focus;
	s16 number_of_steps;
	u8 res;
	u8 type;
};

enum ov5693_tok_type {
	OV5693_8BIT  = 0x0001,
	OV5693_16BIT = 0x0002,
	OV5693_32BIT = 0x0004,
	OV5693_TOK_TERM   = 0xf000,	/* terminating token for reg list */
	OV5693_TOK_DELAY  = 0xfe00,	/* delay token for reg list */
	OV5693_TOK_MASK = 0xfff0
};

/**
 * struct ov5693_reg - MI sensor  register format
 * @type: type of the register
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 *
 * Define a structure for sensor register initialization values
 */
struct ov5693_reg {
	enum ov5693_tok_type type;
	u16 reg;
	u32 val;	/* @set value for read/mod/write, @mask */
};

#define to_ov5693_sensor(x) container_of(x, struct ov5693_device, sd)

#define OV5693_MAX_WRITE_BUF_SIZE	30

struct ov5693_write_buffer {
	u16 addr;
	u8 data[OV5693_MAX_WRITE_BUF_SIZE];
};

struct ov5693_write_ctrl {
	int index;
	struct ov5693_write_buffer buffer;
};

static const struct i2c_device_id ov5693_id[] = {
	{OV5693_NAME, 0},
	{}
};

static struct ov5693_reg const ov5693_global_setting[] = {
	{OV5693_8BIT, 0x0103, 0x01},
	{OV5693_8BIT, 0x3001, 0x0a},
	{OV5693_8BIT, 0x3002, 0x80},
	{OV5693_8BIT, 0x3006, 0x00},
	{OV5693_8BIT, 0x3011, 0x21},
	{OV5693_8BIT, 0x3012, 0x09},
	{OV5693_8BIT, 0x3013, 0x10},
	{OV5693_8BIT, 0x3014, 0x00},
	{OV5693_8BIT, 0x3015, 0x08},
	{OV5693_8BIT, 0x3016, 0xf0},
	{OV5693_8BIT, 0x3017, 0xf0},
	{OV5693_8BIT, 0x3018, 0xf0},
	{OV5693_8BIT, 0x301b, 0xb4},
	{OV5693_8BIT, 0x301d, 0x02},
	{OV5693_8BIT, 0x3021, 0x00},
	{OV5693_8BIT, 0x3022, 0x01},
	{OV5693_8BIT, 0x3028, 0x44},
	{OV5693_8BIT, 0x3098, 0x02},
	{OV5693_8BIT, 0x3099, 0x19},
	{OV5693_8BIT, 0x309a, 0x02},
	{OV5693_8BIT, 0x309b, 0x01},
	{OV5693_8BIT, 0x309c, 0x00},
	{OV5693_8BIT, 0x30a0, 0xd2},
	{OV5693_8BIT, 0x30a2, 0x01},
	{OV5693_8BIT, 0x30b2, 0x00},
	{OV5693_8BIT, 0x30b3, 0x7d},
	{OV5693_8BIT, 0x30b4, 0x03},
	{OV5693_8BIT, 0x30b5, 0x04},
	{OV5693_8BIT, 0x30b6, 0x01},
	{OV5693_8BIT, 0x3104, 0x21},
	{OV5693_8BIT, 0x3106, 0x00},
	{OV5693_8BIT, 0x3400, 0x04},
	{OV5693_8BIT, 0x3401, 0x00},
	{OV5693_8BIT, 0x3402, 0x04},
	{OV5693_8BIT, 0x3403, 0x00},
	{OV5693_8BIT, 0x3404, 0x04},
	{OV5693_8BIT, 0x3405, 0x00},
	{OV5693_8BIT, 0x3406, 0x01},
	{OV5693_8BIT, 0x3500, 0x00},
	{OV5693_8BIT, 0x3503, 0x07},
	{OV5693_8BIT, 0x3504, 0x00},
	{OV5693_8BIT, 0x3505, 0x00},
	{OV5693_8BIT, 0x3506, 0x00},
	{OV5693_8BIT, 0x3507, 0x02},
	{OV5693_8BIT, 0x3508, 0x00},
	{OV5693_8BIT, 0x3509, 0x10},
	{OV5693_8BIT, 0x350a, 0x00},
	{OV5693_8BIT, 0x350b, 0x40},
	{OV5693_8BIT, 0x3601, 0x0a},
	{OV5693_8BIT, 0x3602, 0x38},
	{OV5693_8BIT, 0x3612, 0x80},
	{OV5693_8BIT, 0x3620, 0x54},
	{OV5693_8BIT, 0x3621, 0xc7},
	{OV5693_8BIT, 0x3622, 0x0f},
	{OV5693_8BIT, 0x3625, 0x10},
	{OV5693_8BIT, 0x3630, 0x55},
	{OV5693_8BIT, 0x3631, 0xf4},
	{OV5693_8BIT, 0x3632, 0x00},
	{OV5693_8BIT, 0x3633, 0x34},
	{OV5693_8BIT, 0x3634, 0x02},
	{OV5693_8BIT, 0x364d, 0x0d},
	{OV5693_8BIT, 0x364f, 0xdd},
	{OV5693_8BIT, 0x3660, 0x04},
	{OV5693_8BIT, 0x3662, 0x10},
	{OV5693_8BIT, 0x3663, 0xf1},
	{OV5693_8BIT, 0x3665, 0x00},
	{OV5693_8BIT, 0x3666, 0x20},
	{OV5693_8BIT, 0x3667, 0x00},
	{OV5693_8BIT, 0x366a, 0x80},
	{OV5693_8BIT, 0x3680, 0xe0},
	{OV5693_8BIT, 0x3681, 0x00},
	{OV5693_8BIT, 0x3700, 0x42},
	{OV5693_8BIT, 0x3701, 0x14},
	{OV5693_8BIT, 0x3702, 0xa0},
	{OV5693_8BIT, 0x3703, 0xd8},
	{OV5693_8BIT, 0x3704, 0x78},
	{OV5693_8BIT, 0x3705, 0x02},
	{OV5693_8BIT, 0x370a, 0x00},
	{OV5693_8BIT, 0x370b, 0x20},
	{OV5693_8BIT, 0x370c, 0x0c},
	{OV5693_8BIT, 0x370d, 0x11},
	{OV5693_8BIT, 0x370e, 0x00},
	{OV5693_8BIT, 0x370f, 0x40},
	{OV5693_8BIT, 0x3710, 0x00},
	{OV5693_8BIT, 0x371a, 0x1c},
	{OV5693_8BIT, 0x371b, 0x05},
	{OV5693_8BIT, 0x371c, 0x01},
	{OV5693_8BIT, 0x371e, 0xa1},
	{OV5693_8BIT, 0x371f, 0x0c},
	{OV5693_8BIT, 0x3721, 0x00},
	{OV5693_8BIT, 0x3724, 0x10},
	{OV5693_8BIT, 0x3726, 0x00},
	{OV5693_8BIT, 0x372a, 0x01},
	{OV5693_8BIT, 0x3730, 0x10},
	{OV5693_8BIT, 0x3738, 0x22},
	{OV5693_8BIT, 0x3739, 0xe5},
	{OV5693_8BIT, 0x373a, 0x50},
	{OV5693_8BIT, 0x373b, 0x02},
	{OV5693_8BIT, 0x373c, 0x41},
	{OV5693_8BIT, 0x373f, 0x02},
	{OV5693_8BIT, 0x3740, 0x42},
	{OV5693_8BIT, 0x3741, 0x02},
	{OV5693_8BIT, 0x3742, 0x18},
	{OV5693_8BIT, 0x3743, 0x01},
	{OV5693_8BIT, 0x3744, 0x02},
	{OV5693_8BIT, 0x3747, 0x10},
	{OV5693_8BIT, 0x374c, 0x04},
	{OV5693_8BIT, 0x3751, 0xf0},
	{OV5693_8BIT, 0x3752, 0x00},
	{OV5693_8BIT, 0x3753, 0x00},
	{OV5693_8BIT, 0x3754, 0xc0},
	{OV5693_8BIT, 0x3755, 0x00},
	{OV5693_8BIT, 0x3756, 0x1a},
	{OV5693_8BIT, 0x3758, 0x00},
	{OV5693_8BIT, 0x3759, 0x0f},
	{OV5693_8BIT, 0x376b, 0x44},
	{OV5693_8BIT, 0x375c, 0x04},
	{OV5693_8BIT, 0x3774, 0x10},
	{OV5693_8BIT, 0x3776, 0x00},
	{OV5693_8BIT, 0x377f, 0x08},
	{OV5693_8BIT, 0x3780, 0x22},
	{OV5693_8BIT, 0x3781, 0x0c},
	{OV5693_8BIT, 0x3784, 0x2c},
	{OV5693_8BIT, 0x3785, 0x1e},
	{OV5693_8BIT, 0x378f, 0xf5},
	{OV5693_8BIT, 0x3791, 0xb0},
	{OV5693_8BIT, 0x3795, 0x00},
	{OV5693_8BIT, 0x3796, 0x64},
	{OV5693_8BIT, 0x3797, 0x11},
	{OV5693_8BIT, 0x3798, 0x30},
	{OV5693_8BIT, 0x3799, 0x41},
	{OV5693_8BIT, 0x379a, 0x07},
	{OV5693_8BIT, 0x379b, 0xb0},
	{OV5693_8BIT, 0x379c, 0x0c},
	{OV5693_8BIT, 0x37c5, 0x00},
	{OV5693_8BIT, 0x37c6, 0x00},
	{OV5693_8BIT, 0x37c7, 0x00},
	{OV5693_8BIT, 0x37c9, 0x00},
	{OV5693_8BIT, 0x37ca, 0x00},
	{OV5693_8BIT, 0x37cb, 0x00},
	{OV5693_8BIT, 0x37de, 0x00},
	{OV5693_8BIT, 0x37df, 0x00},
	{OV5693_8BIT, 0x3800, 0x00},
	{OV5693_8BIT, 0x3801, 0x00},
	{OV5693_8BIT, 0x3802, 0x00},
	{OV5693_8BIT, 0x3804, 0x0a},
	{OV5693_8BIT, 0x3805, 0x3f},
	{OV5693_8BIT, 0x3810, 0x00},
	{OV5693_8BIT, 0x3812, 0x00},
	{OV5693_8BIT, 0x3823, 0x00},
	{OV5693_8BIT, 0x3824, 0x00},
	{OV5693_8BIT, 0x3825, 0x00},
	{OV5693_8BIT, 0x3826, 0x00},
	{OV5693_8BIT, 0x3827, 0x00},
	{OV5693_8BIT, 0x382a, 0x04},
	{OV5693_8BIT, 0x3a04, 0x06},
	{OV5693_8BIT, 0x3a05, 0x14},
	{OV5693_8BIT, 0x3a06, 0x00},
	{OV5693_8BIT, 0x3a07, 0xfe},
	{OV5693_8BIT, 0x3b00, 0x00},
	{OV5693_8BIT, 0x3b02, 0x00},
	{OV5693_8BIT, 0x3b03, 0x00},
	{OV5693_8BIT, 0x3b04, 0x00},
	{OV5693_8BIT, 0x3b05, 0x00},
	{OV5693_8BIT, 0x3e07, 0x20},
	{OV5693_8BIT, 0x4000, 0x08},
	{OV5693_8BIT, 0x4001, 0x04},
	{OV5693_8BIT, 0x4002, 0x45},
	{OV5693_8BIT, 0x4004, 0x08},
	{OV5693_8BIT, 0x4005, 0x18},
	{OV5693_8BIT, 0x4006, 0x20},
	{OV5693_8BIT, 0x4008, 0x24},
	{OV5693_8BIT, 0x4009, 0x10},
	{OV5693_8BIT, 0x400c, 0x00},
	{OV5693_8BIT, 0x400d, 0x00},
	{OV5693_8BIT, 0x4058, 0x00},
	{OV5693_8BIT, 0x404e, 0x37},
	{OV5693_8BIT, 0x404f, 0x8f},
	{OV5693_8BIT, 0x4058, 0x00},
	{OV5693_8BIT, 0x4101, 0xb2},
	{OV5693_8BIT, 0x4303, 0x00},
	{OV5693_8BIT, 0x4304, 0x08},
	{OV5693_8BIT, 0x4307, 0x30},
	{OV5693_8BIT, 0x4311, 0x04},
	{OV5693_8BIT, 0x4315, 0x01},
	{OV5693_8BIT, 0x4511, 0x05},
	{OV5693_8BIT, 0x4512, 0x01},
	{OV5693_8BIT, 0x4806, 0x00},
	{OV5693_8BIT, 0x4816, 0x52},
	{OV5693_8BIT, 0x481f, 0x30},
	{OV5693_8BIT, 0x4826, 0x2c},
	{OV5693_8BIT, 0x4831, 0x64},
	{OV5693_8BIT, 0x4d00, 0x04},
	{OV5693_8BIT, 0x4d01, 0x71},
	{OV5693_8BIT, 0x4d02, 0xfd},
	{OV5693_8BIT, 0x4d03, 0xf5},
	{OV5693_8BIT, 0x4d04, 0x0c},
	{OV5693_8BIT, 0x4d05, 0xcc},
	{OV5693_8BIT, 0x4837, 0x0a},
	{OV5693_8BIT, 0x5000, 0x06},
	{OV5693_8BIT, 0x5001, 0x01},
	{OV5693_8BIT, 0x5003, 0x20},
	{OV5693_8BIT, 0x5046, 0x0a},
	{OV5693_8BIT, 0x5013, 0x00},
	{OV5693_8BIT, 0x5046, 0x0a},
	{OV5693_8BIT, 0x5780, 0x1c},
	{OV5693_8BIT, 0x5786, 0x20},
	{OV5693_8BIT, 0x5787, 0x10},
	{OV5693_8BIT, 0x5788, 0x18},
	{OV5693_8BIT, 0x578a, 0x04},
	{OV5693_8BIT, 0x578b, 0x02},
	{OV5693_8BIT, 0x578c, 0x02},
	{OV5693_8BIT, 0x578e, 0x06},
	{OV5693_8BIT, 0x578f, 0x02},
	{OV5693_8BIT, 0x5790, 0x02},
	{OV5693_8BIT, 0x5791, 0xff},
	{OV5693_8BIT, 0x5842, 0x01},
	{OV5693_8BIT, 0x5843, 0x2b},
	{OV5693_8BIT, 0x5844, 0x01},
	{OV5693_8BIT, 0x5845, 0x92},
	{OV5693_8BIT, 0x5846, 0x01},
	{OV5693_8BIT, 0x5847, 0x8f},
	{OV5693_8BIT, 0x5848, 0x01},
	{OV5693_8BIT, 0x5849, 0x0c},
	{OV5693_8BIT, 0x5e00, 0x00},
	{OV5693_8BIT, 0x5e10, 0x0c},
	{OV5693_8BIT, 0x0100, 0x00},
	{OV5693_TOK_TERM, 0, 0}
};

/*
 * 654x496 30fps 17ms VBlanking 2lane 10Bit (Scaling)
 */
static struct ov5693_reg const ov5693_654x496[] = {
	{OV5693_8BIT, 0x3501, 0x3d},
	{OV5693_8BIT, 0x3502, 0x00},
	{OV5693_8BIT, 0x3708, 0xe6},
	{OV5693_8BIT, 0x3709, 0xc7},
	{OV5693_8BIT, 0x3803, 0x00},
	{OV5693_8BIT, 0x3806, 0x07},
	{OV5693_8BIT, 0x3807, 0xa3},
	{OV5693_8BIT, 0x3808, 0x02},
	{OV5693_8BIT, 0x3809, 0x90},
	{OV5693_8BIT, 0x380a, 0x01},
	{OV5693_8BIT, 0x380b, 0xf0},
	{OV5693_8BIT, 0x380c, 0x0a},
	{OV5693_8BIT, 0x380d, 0x80},
	{OV5693_8BIT, 0x380e, 0x07},
	{OV5693_8BIT, 0x380f, 0xc0},
	{OV5693_8BIT, 0x3811, 0x08},
	{OV5693_8BIT, 0x3813, 0x02},
	{OV5693_8BIT, 0x3814, 0x31},
	{OV5693_8BIT, 0x3815, 0x31},
	{OV5693_8BIT, 0x3820, 0x04},
	{OV5693_8BIT, 0x3821, 0x1f},
	{OV5693_8BIT, 0x5002, 0x80},
	{OV5693_8BIT, 0x0100, 0x01},
	{OV5693_TOK_TERM, 0, 0}
};

static struct ov5693_reg const ov5693_736x496[] = {
	{OV5693_8BIT, 0x3501, 0x3d},
	{OV5693_8BIT, 0x3502, 0x00},
	{OV5693_8BIT, 0x3708, 0xe6},
	{OV5693_8BIT, 0x3709, 0xc7},
	{OV5693_8BIT, 0x3803, 0x68},
	{OV5693_8BIT, 0x3806, 0x07},
	{OV5693_8BIT, 0x3807, 0x3b},
	{OV5693_8BIT, 0x3808, 0x02},
	{OV5693_8BIT, 0x3809, 0xe0},
	{OV5693_8BIT, 0x380a, 0x01},
	{OV5693_8BIT, 0x380b, 0xf0},
	{OV5693_8BIT, 0x380c, 0x0a}, //hts
	{OV5693_8BIT, 0x380d, 0x80},
	{OV5693_8BIT, 0x380e, 0x07}, //vts
	{OV5693_8BIT, 0x380f, 0xc0},
	{OV5693_8BIT, 0x3811, 0x08},
	{OV5693_8BIT, 0x3813, 0x02},
	{OV5693_8BIT, 0x3814, 0x31},
	{OV5693_8BIT, 0x3815, 0x31},
	{OV5693_8BIT, 0x3820, 0x04},
	{OV5693_8BIT, 0x3821, 0x1f},
	{OV5693_8BIT, 0x5002, 0x80},
	{OV5693_8BIT, 0x0100, 0x01},
	{OV5693_TOK_TERM, 0, 0}
};

/*
static struct ov5693_reg const ov5693_736x496[] = {
	{OV5693_8BIT, 0x3501, 0x7b},
	{OV5693_8BIT, 0x3502, 0x00},
	{OV5693_8BIT, 0x3708, 0xe6},
	{OV5693_8BIT, 0x3709, 0xc3},
	{OV5693_8BIT, 0x3803, 0x00},
	{OV5693_8BIT, 0x3806, 0x07},
	{OV5693_8BIT, 0x3807, 0xa3},
	{OV5693_8BIT, 0x3808, 0x02},
	{OV5693_8BIT, 0x3809, 0xe0},
	{OV5693_8BIT, 0x380a, 0x01},
	{OV5693_8BIT, 0x380b, 0xf0},
	{OV5693_8BIT, 0x380c, 0x0d}, //hts
	{OV5693_8BIT, 0x380d, 0xb0},
	{OV5693_8BIT, 0x380e, 0x05}, //vts
	{OV5693_8BIT, 0x380f, 0xf2},
	{OV5693_8BIT, 0x3811, 0x08},
	{OV5693_8BIT, 0x3813, 0x02},
	{OV5693_8BIT, 0x3814, 0x31},
	{OV5693_8BIT, 0x3815, 0x31},
	{OV5693_8BIT, 0x3820, 0x01},
	{OV5693_8BIT, 0x3821, 0x1f},
	{OV5693_8BIT, 0x5002, 0x00},
	{OV5693_8BIT, 0x0100, 0x01},
	{OV5693_TOK_TERM, 0, 0}
};
*/
/*
 * 976x556 30fps 8.8ms VBlanking 2lane 10Bit (Scaling)
 */
static struct ov5693_reg const ov5693_976x556[] = {
	{OV5693_8BIT, 0x3501, 0x7b},
	{OV5693_8BIT, 0x3502, 0x00},
	{OV5693_8BIT, 0x3708, 0xe2},
	{OV5693_8BIT, 0x3709, 0xc3},
	{OV5693_8BIT, 0x3803, 0xf0},
	{OV5693_8BIT, 0x3806, 0x06},
	{OV5693_8BIT, 0x3807, 0xa7},
	{OV5693_8BIT, 0x3808, 0x03},
	{OV5693_8BIT, 0x3809, 0xd0},
	{OV5693_8BIT, 0x380a, 0x02},
	{OV5693_8BIT, 0x380b, 0x2C},
	{OV5693_8BIT, 0x380c, 0x0a},
	{OV5693_8BIT, 0x380d, 0x80},
	{OV5693_8BIT, 0x380e, 0x07},
	{OV5693_8BIT, 0x380f, 0xc0},
	{OV5693_8BIT, 0x3811, 0x10},
	{OV5693_8BIT, 0x3813, 0x02},
	{OV5693_8BIT, 0x3814, 0x11},
	{OV5693_8BIT, 0x3815, 0x11},
	{OV5693_8BIT, 0x3820, 0x00},
	{OV5693_8BIT, 0x3821, 0x1e},
	{OV5693_8BIT, 0x5002, 0x80},
	{OV5693_8BIT, 0x0100, 0x01},
	{OV5693_TOK_TERM, 0, 0}
};

static struct ov5693_reg const ov5693_1296x736[] = {
	{OV5693_8BIT, 0x3501, 0x7b},
	{OV5693_8BIT, 0x3502, 0x00},
	{OV5693_8BIT, 0x3708, 0xe6},
	{OV5693_8BIT, 0x3709, 0xc3},
	{OV5693_8BIT, 0x3803, 0x00},
	{OV5693_8BIT, 0x3806, 0x07},
	{OV5693_8BIT, 0x3807, 0xa3},
	{OV5693_8BIT, 0x3808, 0x05},
	{OV5693_8BIT, 0x3809, 0x10},
	{OV5693_8BIT, 0x380a, 0x02},
	{OV5693_8BIT, 0x380b, 0xe0},
	{OV5693_8BIT, 0x380c, 0x0d}, //hts
	{OV5693_8BIT, 0x380d, 0xb0},
	{OV5693_8BIT, 0x380e, 0x05}, //vts
	{OV5693_8BIT, 0x380f, 0xf2},
	{OV5693_8BIT, 0x3811, 0x08},
	{OV5693_8BIT, 0x3813, 0x02},
	{OV5693_8BIT, 0x3814, 0x31},
	{OV5693_8BIT, 0x3815, 0x31},
	{OV5693_8BIT, 0x3820, 0x01},
	{OV5693_8BIT, 0x3821, 0x1f},
	{OV5693_8BIT, 0x5002, 0x00},
	{OV5693_8BIT, 0x0100, 0x01},
	{OV5693_TOK_TERM, 0, 0}
};

static struct ov5693_reg const ov5693_1636p_30fps[] = {
	{OV5693_8BIT, 0x3501, 0x7b},
	{OV5693_8BIT, 0x3502, 0x00},
	{OV5693_8BIT, 0x3708, 0xe2},
	{OV5693_8BIT, 0x3709, 0xc3},
	{OV5693_8BIT, 0x3803, 0xf0},
	{OV5693_8BIT, 0x3806, 0x06},
	{OV5693_8BIT, 0x3807, 0xa7},
	{OV5693_8BIT, 0x3808, 0x06},
	{OV5693_8BIT, 0x3809, 0x64},
	{OV5693_8BIT, 0x380a, 0x04},
	{OV5693_8BIT, 0x380b, 0x48},
	{OV5693_8BIT, 0x380c, 0x0a}, //hts
	{OV5693_8BIT, 0x380d, 0x80},
	{OV5693_8BIT, 0x380e, 0x07}, //vts
	{OV5693_8BIT, 0x380f, 0xc0},
	{OV5693_8BIT, 0x3811, 0x02},
	{OV5693_8BIT, 0x3813, 0x02},
	{OV5693_8BIT, 0x3814, 0x11},
	{OV5693_8BIT, 0x3815, 0x11},
	{OV5693_8BIT, 0x3820, 0x00},
	{OV5693_8BIT, 0x3821, 0x1e},
	{OV5693_8BIT, 0x5002, 0x80},
	{OV5693_8BIT, 0x0100, 0x01},
	{OV5693_TOK_TERM, 0, 0}
};

/*
 * 1940x1096 30fps 8.8ms VBlanking 2lane 10bit (Scaling)
 */
static struct ov5693_reg const ov5693_1940x1096[] = {
	{OV5693_8BIT, 0x3501, 0x7b},
	{OV5693_8BIT, 0x3502, 0x00},
	{OV5693_8BIT, 0x3708, 0xe2},
	{OV5693_8BIT, 0x3709, 0xc3},
	{OV5693_8BIT, 0x3803, 0xf0},
	{OV5693_8BIT, 0x3806, 0x06},
	{OV5693_8BIT, 0x3807, 0xa7},
	{OV5693_8BIT, 0x3808, 0x07},
	{OV5693_8BIT, 0x3809, 0x94},
	{OV5693_8BIT, 0x380a, 0x04},
	{OV5693_8BIT, 0x380b, 0x48},
	{OV5693_8BIT, 0x380c, 0x0a},
	{OV5693_8BIT, 0x380d, 0x80},
	{OV5693_8BIT, 0x380e, 0x07},
	{OV5693_8BIT, 0x380f, 0xc0},
	{OV5693_8BIT, 0x3811, 0x02},
	{OV5693_8BIT, 0x3813, 0x02},
	{OV5693_8BIT, 0x3814, 0x11},
	{OV5693_8BIT, 0x3815, 0x11},
	{OV5693_8BIT, 0x3820, 0x00},
	{OV5693_8BIT, 0x3821, 0x1e},
	{OV5693_8BIT, 0x5002, 0x80},
	{OV5693_8BIT, 0x0100, 0x01},
	{OV5693_TOK_TERM, 0, 0}
};

/*
 * 2592x1944 30fps 0.6ms VBlanking 2lane 10Bit
 */
static struct ov5693_reg const ov5693_2592x1944_30fps[] = {
	{OV5693_8BIT, 0x3501, 0x7b},
	{OV5693_8BIT, 0x3502, 0x00},
	{OV5693_8BIT, 0x3708, 0xe2},
	{OV5693_8BIT, 0x3709, 0xc3},
	{OV5693_8BIT, 0x3803, 0x00},
	{OV5693_8BIT, 0x3806, 0x07},
	{OV5693_8BIT, 0x3807, 0xa3},
	{OV5693_8BIT, 0x3808, 0x0a},
	{OV5693_8BIT, 0x3809, 0x20},
	{OV5693_8BIT, 0x380a, 0x07},
	{OV5693_8BIT, 0x380b, 0x98},
	{OV5693_8BIT, 0x380c, 0x0a},
	{OV5693_8BIT, 0x380d, 0x80},
	{OV5693_8BIT, 0x380e, 0x07},
	{OV5693_8BIT, 0x380f, 0xc0},
	{OV5693_8BIT, 0x3811, 0x10},
	{OV5693_8BIT, 0x3813, 0x00},
	{OV5693_8BIT, 0x3814, 0x11},
	{OV5693_8BIT, 0x3815, 0x11},
	{OV5693_8BIT, 0x3820, 0x00},
	{OV5693_8BIT, 0x3821, 0x1e},
	{OV5693_8BIT, 0x5002, 0x00},
	{OV5693_8BIT, 0x0100, 0x01},
	{OV5693_TOK_TERM, 0, 0}
};

struct ov5693_resolution ov5693_res_preview[] = {
	{
		.desc = "ov5693_5M_30fps",
		.width = 2592,
		.height = 1944,
		.pix_clk_freq = 81,
		.fps = 30,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.regs = ov5693_2592x1944_30fps,
	},
};
#define N_RES_PREVIEW (ARRAY_SIZE(ov5693_res_preview))

struct ov5693_resolution ov5693_res_still[] = {
	{
		.desc = "ov5693_5M_30fps",
		.width = 2592,
		.height = 1944,
		.pix_clk_freq = 81,
		.fps = 30,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.regs = ov5693_2592x1944_30fps,
	},
};
#define N_RES_STILL (ARRAY_SIZE(ov5693_res_still))

struct ov5693_resolution ov5693_res_video[] = {
	{
		.desc = "ov5693_736x496_30fps",
		.width = 736,
		.height = 496,
		.fps = 30,
		.pix_clk_freq = 81,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.bin_mode = 1,
		.regs = ov5693_736x496,
	},
	{
		.desc = "ov5693_1296x736_30fps",
		.width = 1296,
		.height = 736,
		.fps = 30,
		.pix_clk_freq = 81,
		.used = 0,
		.pixels_per_line = 3504,
		.lines_per_frame = 1522,
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.bin_mode = 1,
		.regs = ov5693_1296x736,
	},
	{
		.desc = "ov5693_1636P_30fps",
		.width = 1636,
		.height = 1096,
		.fps = 30,
		.pix_clk_freq = 81,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.regs = ov5693_1636p_30fps,
	},
	{
		.desc = "ov5693_1080P_30fps",
		.width = 1940,
		.height = 1096,
		.fps = 30,
		.pix_clk_freq = 81,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.regs = ov5693_1940x1096,
	},
};
#define N_RES_VIDEO (ARRAY_SIZE(ov5693_res_video))

static struct ov5693_resolution *ov5693_res = ov5693_res_preview;
static int N_RES = N_RES_PREVIEW;
#endif
