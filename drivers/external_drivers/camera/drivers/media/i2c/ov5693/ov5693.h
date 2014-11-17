/*
 * Support for OmniVision OV5693 5M HD camera sensor.
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
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <media/media-entity.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <linux/v4l2-mediabus.h>

#include <linux/atomisp_platform.h>
#include "ad5823.h"

#define OV5693_NAME		"ov5693"

/* Defines for register writes and register array processing */
#define I2C_MSG_LENGTH		0x2

#define OV5693_FOCAL_LENGTH_NUM	278	/*2.78mm*/
#define OV5693_FOCAL_LENGTH_DEM	100
#define OV5693_F_NUMBER_DEFAULT_NUM	26
#define OV5693_F_NUMBER_DEM	10

#define OV5693_MAX_FOCUS_POS	1023
#define OV5693_MAX_FOCUS_POS	1023
#define OV5693_MAX_FOCUS_NEG	(-1023)

#define LARGEST_ALLOWED_RATIO_MISMATCH	800
#define RATIO_SHIFT_BITS		13

/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define OV5693_FOCAL_LENGTH_DEFAULT 0x1160064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define OV5693_F_NUMBER_DEFAULT 0x1a000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define OV5693_F_NUMBER_RANGE 0x1a0a1a0a
#define OV5693_ID	0x5690

#define OV5693_FINE_INTG_TIME_MIN 0
#define OV5693_FINE_INTG_TIME_MAX_MARGIN 0
#define OV5693_COARSE_INTG_TIME_MIN 1
#define OV5693_COARSE_INTG_TIME_MAX_MARGIN 6
#define OV5693_INTEGRATION_TIME_MARGIN	8

#define OV5693_BIN_FACTOR_MAX	2

/*
 * OV5693 System control registers
 */
#define OV5693_SW_RESET				0x0103
#define OV5693_SW_STREAM			0x0100

#define OV5693_SC_CMMN_CHIP_ID			0x300a
#define OV5693_SC_CMMN_SUB_ID			0x302a /* process, version*/

#define OV5693_AEC_PK_EXPO_H			0x3500
#define OV5693_AEC_PK_EXPO_M			0x3501
#define OV5693_AEC_PK_EXPO_L			0x3502
#define OV5693_AGC_ADJ_H			0x350a
#define OV5693_VTS_H				0x380e
#define OV5693_GROUP_ACCESS			0x3208

#define OV5693_MWB_GAIN_R_H			0x3400
#define OV5693_MWB_GAIN_G_H			0x3402
#define OV5693_MWB_GAIN_B_H			0x3404

#define OV5693_H_CROP_START_H			0x3800
#define OV5693_V_CROP_START_H			0x3802
#define OV5693_H_CROP_END_H			0x3804
#define OV5693_V_CROP_END_H			0x3806
#define OV5693_H_OUTSIZE_H			0x3808
#define OV5693_V_OUTSIZE_H			0x380a

#define OV5693_START_STREAMING			0x01
#define OV5693_STOP_STREAMING			0x00

struct ov5693_vcm {
	int (*power_up)(struct v4l2_subdev *sd);
	int (*power_down)(struct v4l2_subdev *sd);
	int (*init)(struct v4l2_subdev *sd);
	int (*t_focus_vcm)(struct v4l2_subdev *sd, u16 val);
	int (*t_focus_abs)(struct v4l2_subdev *sd, s32 value);
	int (*t_focus_rel)(struct v4l2_subdev *sd, s32 value);
	int (*q_focus_status)(struct v4l2_subdev *sd, s32 *value);
	int (*q_focus_abs)(struct v4l2_subdev *sd, s32 *value);
};

struct ov5693_resolution {
	u8 *desc;
	const struct ov5693_reg *regs;
	int res;
	int width;
	int height;
	int fps;
	int pix_clk_freq;
	u16 skip_frames;
	u16 pixels_per_line;
	u16 lines_per_frame;
	u8 bin_factor_x;
	u8 bin_factor_y;
	u8 bin_mode;
	bool used;
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
	struct ov5693_vcm *vcm_driver;
	int fmt_idx;
	u8 res;
	u8 type;

	struct ov5693_resolution *ov5693_res;
	int curr_res_num;

	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *run_mode;
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

/* ov5693 sensor initialization setting */
static struct ov5693_reg const ov5693_init_setting[] = {
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

	/* Manual white balance */
	{OV5693_8BIT, 0x3400, 0x04},
	{OV5693_8BIT, 0x3401, 0x00},
	{OV5693_8BIT, 0x3402, 0x04},
	{OV5693_8BIT, 0x3403, 0x00},
	{OV5693_8BIT, 0x3404, 0x04},
	{OV5693_8BIT, 0x3405, 0x00},
	{OV5693_8BIT, 0x3406, 0x01},

	/* Manual exposure control */
	{OV5693_8BIT, 0x3500, 0x00},
	{OV5693_8BIT, 0x3503, 0x07},
	{OV5693_8BIT, 0x3504, 0x00},
	{OV5693_8BIT, 0x3505, 0x00},
	{OV5693_8BIT, 0x3506, 0x00},
	{OV5693_8BIT, 0x3507, 0x02},
	{OV5693_8BIT, 0x3508, 0x00},

	/* Manual gain control */
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
 * Register settings for various resolution
 */

/*
------------------------------------
@@ FULL QSXGA (2592x1944) 15fps 33.33ms VBlank 2lane 10Bit
100 99 2592 1944
100 98 1 0
102 3601 bb8 ;Pather tool use only
c8  1 f2 ; New FPGA Board
c8 20 22 ; New FPGA Board
c8 10 42 ; MIPI DFGA CYCY3 Board use only
;
c8 f 32  ; input clock to 19.2MHz
;
; OV5690 setting version History
;
;
; V18b
*/
static struct ov5693_reg const ov5693_5M_15fps[] = {
	{OV5693_8BIT, 0x3501, 0x7b},
	{OV5693_8BIT, 0x3502, 0x00},
	{OV5693_8BIT, 0x3708, 0xe2},
	{OV5693_8BIT, 0x3709, 0xc3},
	{OV5693_8BIT, 0x3800, 0x00},	/* x_addr_start: 0 */
	{OV5693_8BIT, 0x3801, 0x00},
	{OV5693_8BIT, 0x3802, 0x00},	/* y_addr_start: 0 */
	{OV5693_8BIT, 0x3803, 0x00},
	{OV5693_8BIT, 0x3804, 0x0a},	/* x_addr_end: 2623 */
	{OV5693_8BIT, 0x3805, 0x3f},
	{OV5693_8BIT, 0x3806, 0x07},	/* y_addr_end: 1955 */
	{OV5693_8BIT, 0x3807, 0xa3},
	{OV5693_8BIT, 0x3808, 0x0a},	/* x output size: 2592 */
	{OV5693_8BIT, 0x3809, 0x20},
	{OV5693_8BIT, 0x380a, 0x07},	/* y output size: 1944 */
	{OV5693_8BIT, 0x380b, 0x98},
	{OV5693_8BIT, 0x380c, 0x0e},	/* total x output size: 3688 */
	{OV5693_8BIT, 0x380d, 0x68},
	{OV5693_8BIT, 0x380e, 0x0f},	/* total y output size: 3968 */
	{OV5693_8BIT, 0x380f, 0x80},
	{OV5693_8BIT, 0x3810, 0x00},	/* x offset: 16 */
	{OV5693_8BIT, 0x3811, 0x10},
	{OV5693_8BIT, 0x3812, 0x00},	/* y offset: 6 */
	{OV5693_8BIT, 0x3813, 0x06},
	{OV5693_8BIT, 0x3814, 0x11},
	{OV5693_8BIT, 0x3815, 0x11},
	{OV5693_8BIT, 0x3820, 0x00},
	{OV5693_8BIT, 0x3821, 0x1e},
	{OV5693_8BIT, 0x5002, 0x00},
	{OV5693_TOK_TERM, 0, 0}
};

/*
@@ OV5693 1940x1096 30fps 8.8ms VBlanking 2lane 10Bit(Scaling)
100 99 1940 1096
100 98 1 0
102 3601 bb8 ;Pather tool use only
102 40 0 ; HDR Mode off
c8  1 f2 ; New FPGA Board
c8 20 22 ; New FPGA Board
c8 10 42 ; MIPI DFGA CYCY3 Board use only
;
c8 f 32  ; input clock to 19.2MHz
;
; OV5690 setting version History
;
;
; V18b
*/
static struct ov5693_reg const ov5693_1080p_30fps[] = {
	{OV5693_8BIT, 0x3501, 0x7b},
	{OV5693_8BIT, 0x3502, 0x00},
	{OV5693_8BIT, 0x3708, 0xe2},
	{OV5693_8BIT, 0x3709, 0xc3},
	{OV5693_8BIT, 0x3800, 0x00},	/* x_addr_start: 0 */
	{OV5693_8BIT, 0x3801, 0x00},
	{OV5693_8BIT, 0x3802, 0x00},	/* y_addr_start: 240 */
	{OV5693_8BIT, 0x3803, 0xf0},
	{OV5693_8BIT, 0x3804, 0x0a},	/* x_addr_end: 2591 */
	{OV5693_8BIT, 0x3805, 0x1f},
	{OV5693_8BIT, 0x3806, 0x06},	/* y_addr_end: 1703 */
	{OV5693_8BIT, 0x3807, 0xa7},
	{OV5693_8BIT, 0x3808, 0x07},	/* x output size: 1940 */
	{OV5693_8BIT, 0x3809, 0x94},
	{OV5693_8BIT, 0x380a, 0x04},	/* y output size: 1096 */
	{OV5693_8BIT, 0x380b, 0x48},
	{OV5693_8BIT, 0x380c, 0x0e},	/* total x output size: 3688 */
	{OV5693_8BIT, 0x380d, 0x68},
	{OV5693_8BIT, 0x380e, 0x0b},	/* total y output size: 2984 */
	{OV5693_8BIT, 0x380f, 0xa8},
	{OV5693_8BIT, 0x3810, 0x00},	/* x offset: 2 */
	{OV5693_8BIT, 0x3811, 0x02},
	{OV5693_8BIT, 0x3812, 0x00},	/* y offset: 2 */
	{OV5693_8BIT, 0x3813, 0x02},
	{OV5693_8BIT, 0x3814, 0x11},
	{OV5693_8BIT, 0x3815, 0x11},
	{OV5693_8BIT, 0x3820, 0x00},
	{OV5693_8BIT, 0x3821, 0x1e},
	{OV5693_8BIT, 0x5002, 0x80},
	{OV5693_TOK_TERM, 0, 0}
};

/*
 * 1296x736 30fps 8.8ms VBlanking 2lane 10Bit (Scaling)
 */
static struct ov5693_reg const ov5693_720p_30fps[] = {
	{OV5693_8BIT, 0x3501, 0x3d},
	{OV5693_8BIT, 0x3502, 0x00},
	{OV5693_8BIT, 0x3708, 0xe6},
	{OV5693_8BIT, 0x3709, 0xc7},
	{OV5693_8BIT, 0x3800, 0x00},	/* x_addr_start: 0 */
	{OV5693_8BIT, 0x3801, 0x00},
	{OV5693_8BIT, 0x3802, 0x00},	/* y_addr_start: 0 */
	{OV5693_8BIT, 0x3803, 0x00},
	{OV5693_8BIT, 0x3804, 0x0a},	/* x_addr_end: 2623 */
	{OV5693_8BIT, 0x3805, 0x3f},
	{OV5693_8BIT, 0x3806, 0x07},	/* y_addr_end: 1955 */
	{OV5693_8BIT, 0x3807, 0xa3},
	{OV5693_8BIT, 0x3808, 0x05},	/* x output size: 1296 */
	{OV5693_8BIT, 0x3809, 0x10},
	{OV5693_8BIT, 0x380a, 0x02},	/* y output size: 736 */
	{OV5693_8BIT, 0x380b, 0xe0},
	{OV5693_8BIT, 0x380c, 0x0a},	/* total x output size: 2688 */
	{OV5693_8BIT, 0x380d, 0x80},
	{OV5693_8BIT, 0x380e, 0x07},	/* total y output size: 1984 */
	{OV5693_8BIT, 0x380f, 0xc0},
	{OV5693_8BIT, 0x3810, 0x00},	/* x offset: 8 */
	{OV5693_8BIT, 0x3811, 0x08},
	{OV5693_8BIT, 0x3812, 0x00},	/* y offset: 2 */
	{OV5693_8BIT, 0x3813, 0x02},
	{OV5693_8BIT, 0x3814, 0x31},
	{OV5693_8BIT, 0x3815, 0x31},
	{OV5693_8BIT, 0x3820, 0x04},
	{OV5693_8BIT, 0x3821, 0x1f},
	{OV5693_8BIT, 0x5002, 0x80},
	{OV5693_TOK_TERM, 0, 0}
};

/*
 * 736x496 30fps 8.8ms VBlanking 2lane 10Bit (Scaling)
 */
static struct ov5693_reg const ov5693_480p_30fps[] = {
	{OV5693_8BIT, 0x3501, 0x3d},
	{OV5693_8BIT, 0x3502, 0x00},
	{OV5693_8BIT, 0x3708, 0xe6},
	{OV5693_8BIT, 0x3709, 0xc7},
	{OV5693_8BIT, 0x3800, 0x00},	/* x_addr_start: 0 */
	{OV5693_8BIT, 0x3801, 0x00},
	{OV5693_8BIT, 0x3802, 0x00},	/* y_addr_start: 7 */
	{OV5693_8BIT, 0x3803, 0x07},
	{OV5693_8BIT, 0x3804, 0x0a},	/* x_addr_end: 2623 */
	{OV5693_8BIT, 0x3805, 0x3f},
	{OV5693_8BIT, 0x3806, 0x07},	/* y_addr_end: 1955 */
	{OV5693_8BIT, 0x3807, 0xa3},
	{OV5693_8BIT, 0x3808, 0x02},	/* x output size: 736 */
	{OV5693_8BIT, 0x3809, 0xe0},
	{OV5693_8BIT, 0x380a, 0x01},	/* y output size: 496 */
	{OV5693_8BIT, 0x380b, 0xf0},
	{OV5693_8BIT, 0x380c, 0x0a},	/* total x output size: 2688 */
	{OV5693_8BIT, 0x380d, 0x80},
	{OV5693_8BIT, 0x380e, 0x07},	/* total y output size: 1984 */
	{OV5693_8BIT, 0x380f, 0xc0},
	{OV5693_8BIT, 0x3810, 0x00},	/* x offset: 8 */
	{OV5693_8BIT, 0x3811, 0x08},
	{OV5693_8BIT, 0x3812, 0x00},	/* y offset: 2 */
	{OV5693_8BIT, 0x3813, 0x02},
	{OV5693_8BIT, 0x3814, 0x31},
	{OV5693_8BIT, 0x3815, 0x31},
	{OV5693_8BIT, 0x3820, 0x04},
	{OV5693_8BIT, 0x3821, 0x1f},
	{OV5693_8BIT, 0x5002, 0x80},
	{OV5693_TOK_TERM, 0, 0}
};

/*
@@ OV5693 656x496 30fps 17ms VBlanking 2lane 10Bit(Scaling)
100 99 656 496
100 98 1 0
102 3601 BB8 ;Pather tool use only
c8  1 f2 ; New FPGA Board
c8 20 22 ; New FPGA Board
; OV5690 setting version History
;
c8 f 32  ; input clock to 19.2MHz
;
; V18b
*/
static struct ov5693_reg const ov5693_VGA_30fps[] = {
	{OV5693_8BIT, 0x3501, 0x3d},
	{OV5693_8BIT, 0x3502, 0x00},
	{OV5693_8BIT, 0x3708, 0xe6},
	{OV5693_8BIT, 0x3709, 0xc7},
	{OV5693_8BIT, 0x3800, 0x00},	/* x_addr_start: 0 */
	{OV5693_8BIT, 0x3801, 0x00},
	{OV5693_8BIT, 0x3802, 0x00},	/* y_addr_start: 0 */
	{OV5693_8BIT, 0x3803, 0x00},
	{OV5693_8BIT, 0x3804, 0x0a},	/* x_addr_end: 2623 */
	{OV5693_8BIT, 0x3805, 0x3f},
	{OV5693_8BIT, 0x3806, 0x07},	/* y_addr_end: 1955 */
	{OV5693_8BIT, 0x3807, 0xa3},
	{OV5693_8BIT, 0x3808, 0x02},	/* x output size: 656 */
	{OV5693_8BIT, 0x3809, 0x90},
	{OV5693_8BIT, 0x380a, 0x01},	/* y output size: 496 */
	{OV5693_8BIT, 0x380b, 0xf0},
	{OV5693_8BIT, 0x380c, 0x0a},	/* total x output size: 2688 */
	{OV5693_8BIT, 0x380d, 0x80},
	{OV5693_8BIT, 0x380e, 0x07},	/* total y output size: 1984 */
	{OV5693_8BIT, 0x380f, 0xc0},
	{OV5693_8BIT, 0x3810, 0x00},	/* x offset: 13 */
	{OV5693_8BIT, 0x3811, 0x0d},
	{OV5693_8BIT, 0x3812, 0x00},	/* y offset: 3 */
	{OV5693_8BIT, 0x3813, 0x03},
	{OV5693_8BIT, 0x3814, 0x31},
	{OV5693_8BIT, 0x3815, 0x31},
	{OV5693_8BIT, 0x3820, 0x04},
	{OV5693_8BIT, 0x3821, 0x1f},
	{OV5693_8BIT, 0x5002, 0x80},
	{OV5693_TOK_TERM, 0, 0}
};

struct ov5693_resolution ov5693_res_preview[] = {
	{
		.desc = "ov5693_VGA_30fps",
		.width = 656,
		.height = 496,
		.fps = 30,
		.pix_clk_freq = 160,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = ov5693_VGA_30fps,
	},
	{
		.desc = "ov5693_1080P_30fps",
		.width = 1940,
		.height = 1096,
		.fps = 30,
		.pix_clk_freq = 160,
		.used = 0,
		.pixels_per_line = 3688,
		.lines_per_frame = 2984,
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = ov5693_1080p_30fps,
	},
	{
		.desc = "ov5693_5M_15fps",
		.width = 2592,
		.height = 1944,
		.fps = 15,
		.pix_clk_freq = 160,
		.used = 0,
		.pixels_per_line = 3688,
		.lines_per_frame = 3968,
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = ov5693_5M_15fps,
	},
};
#define N_RES_PREVIEW (ARRAY_SIZE(ov5693_res_preview))

struct ov5693_resolution ov5693_res_still[] = {
	{
		.desc = "ov5693_VGA_30fps",
		.width = 656,
		.height = 496,
		.fps = 30,
		.pix_clk_freq = 160,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = ov5693_VGA_30fps,
	},
	{
		.desc = "ov5693_1080P_30fps",
		.width = 1940,
		.height = 1096,
		.fps = 30,
		.pix_clk_freq = 160,
		.used = 0,
		.pixels_per_line = 3688,
		.lines_per_frame = 2984,
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = ov5693_1080p_30fps,
	},
	{
		.desc = "ov5693_5M_15fps",
		.width = 2592,
		.height = 1944,
		.fps = 15,
		.pix_clk_freq = 160,
		.used = 0,
		.pixels_per_line = 3688,
		.lines_per_frame = 3968,
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = ov5693_5M_15fps,
	},
};
#define N_RES_STILL (ARRAY_SIZE(ov5693_res_still))

struct ov5693_resolution ov5693_res_video[] = {
	{
		.desc = "ov5693_VGA_30fps",
		.width = 656,
		.height = 496,
		.fps = 30,
		.pix_clk_freq = 160,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = ov5693_VGA_30fps,
	},
	{
		.desc = "ov5693_480P_30fps",
		.width = 736,
		.height = 496,
		.fps = 30,
		.pix_clk_freq = 160,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.bin_mode = 0,
		.skip_frames = 1,
		.regs = ov5693_480p_30fps,
	},
	{
		.desc = "ov5693_720p_30fps",
		.width = 1296,
		.height = 736,
		.fps = 30,
		.pix_clk_freq = 160,
		.used = 0,
		.pixels_per_line = 2688,
		.lines_per_frame = 1984,
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.bin_mode = 0,
		.skip_frames = 1,
		.regs = ov5693_720p_30fps,
	},
	{
		.desc = "ov5693_1080P_30fps",
		.width = 1940,
		.height = 1096,
		.fps = 30,
		.pix_clk_freq = 160,
		.used = 0,
		.pixels_per_line = 3688,
		.lines_per_frame = 2984,
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = ov5693_1080p_30fps,
	},
};
#define N_RES_VIDEO (ARRAY_SIZE(ov5693_res_video))

struct ov5693_vcm ov5693_vcm_ops = {
	.power_up = ad5823_vcm_power_up,
	.power_down = ad5823_vcm_power_down,
	.init = ad5823_vcm_init,
	.t_focus_vcm = ad5823_t_focus_vcm,
	.t_focus_abs = ad5823_t_focus_abs,
	.t_focus_rel = ad5823_t_focus_rel,
	.q_focus_status = ad5823_q_focus_status,
	.q_focus_abs = ad5823_q_focus_abs,
};
#endif
