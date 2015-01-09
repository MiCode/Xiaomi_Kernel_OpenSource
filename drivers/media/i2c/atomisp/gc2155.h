/*
 * Support for GalaxyCore GC2155 2M camera sensor.
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

#ifndef __GC2155_H__
#define __GC2155_H__
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/spinlock.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-device.h>
#include <linux/v4l2-mediabus.h>
#include <media/media-entity.h>

#include <linux/atomisp_platform.h>

#define GC2155_NAME		"gc2155"

/* Defines for register writes and register array processing */
#define I2C_MSG_LENGTH		1
#define I2C_RETRY_COUNT		5

#define GC2155_FOCAL_LENGTH_NUM	260	/* 2.6mm */
#define GC2155_FOCAL_LENGTH_DEM	100
#define GC2155_F_NUMBER_DEFAULT_NUM	28
#define GC2155_F_NUMBER_DEM	10

#define MAX_FMTS		1

/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define GC2155_FOCAL_LENGTH_DEFAULT 0x1160064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define GC2155_F_NUMBER_DEFAULT 0x1a000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define GC2155_F_NUMBER_RANGE 0x1a0a1a0a
#define GC2155_ID	0x2155

#define GC2155_FINE_INTG_TIME_MIN 0
#define GC2155_FINE_INTG_TIME_MAX_MARGIN 0
#define GC2155_COARSE_INTG_TIME_MIN 1
#define GC2155_COARSE_INTG_TIME_MAX_MARGIN 6

/*
 * GC2155 System control registers
 */
#define GC2155_SW_STREAM			0x10

#define GC2155_SC_CMMN_CHIP_ID		0x0

#define GC2155_AEC_PK_EXPO_H			0x03
#define GC2155_AEC_PK_EXPO_L			0x04
#define GC2155_AGC_ADJ			0x50
#if 0
#define GC2155_GROUP_ACCESS			0x3208
#endif

/* gc2155 system registers */
#define REG_CHIP_ID_H		0xF0
#define REG_CHIP_ID_L		0xF1
#define REG_RST_AND_PG_SELECT	0xFE

/* gc2155 page0 registers */
#define REG_EXPO_COARSE_H	0x03
#define REG_EXPO_COARSE_L	0x04
#define REG_H_BLANK_H		0x05
#define REG_H_BLANK_L		0x06
#define REG_V_BLANK_H		0x07
#define REG_V_BLANK_L		0x08
#define REG_ROW_START_H		0x09
#define REG_ROW_START_L		0x0A
#define REG_COL_START_H		0x0B
#define REG_COL_START_L		0x0C
#define REG_WIN_HEIGHT_H	0x0D
#define REG_WIN_HEIGHT_L	0x0E
#define REG_WIN_WIDTH_H		0x0F
#define REG_WIN_WIDTH_L		0x10
#define REG_SH_DELAY_H		0x11
#define REG_SH_DELAY_L		0x12

#define GC2155_START_STREAMING			0x90 /* 10-bit enable */
#define GC2155_STOP_STREAMING			0x00 /* 10-bit disable */

#define GC2155_REG_RST_AND_PG_SELECT	0xFE

#define GC2155_BIN_FACTOR_MAX			3

struct regval_list {
	u16 reg_num;
	u8 value;
};

struct gc2155_resolution {
	u8 *desc;
	const struct gc2155_reg *regs;
	int res;
	int width;
	int height;
	int fps;
	int pix_clk_freq;
	u32 skip_frames;
	u16 pixels_per_line;
	u16 lines_per_frame;
	u8 bin_factor_x;
	u8 bin_factor_y;
	u8 bin_mode;
	bool used;
};

struct gc2155_format {
	u8 *desc;
	u32 pixelformat;
	struct gc2155_reg *regs;
};

struct gc2155_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, s32 value);
};

/*
 * gc2155 device structure.
 */
struct gc2155_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;
	struct mutex input_lock;

	struct camera_sensor_platform_data *platform_data;
	int vt_pix_clk_freq_mhz;
	int fmt_idx;
	int run_mode;
	u8 res;
	u8 type;
};

enum gc2155_tok_type {
	GC2155_8BIT  = 0x0001,
	GC2155_TOK_TERM   = 0xf000,	/* terminating token for reg list */
	GC2155_TOK_DELAY  = 0xfe00,	/* delay token for reg list */
	GC2155_TOK_MASK = 0xfff0
};

/**
 * struct gc2155_reg - MI sensor  register format
 * @type: type of the register
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 *
 * Define a structure for sensor register initialization values
 */
struct gc2155_reg {
	enum gc2155_tok_type type;
	u8 reg;
	u8 val;	/* @set value for read/mod/write, @mask */
};

#define to_gc2155_sensor(x) container_of(x, struct gc2155_device, sd)

#define GC2155_MAX_WRITE_BUF_SIZE	30

struct gc2155_write_buffer {
	u8 addr;
	u8 data[GC2155_MAX_WRITE_BUF_SIZE];
};

struct gc2155_write_ctrl {
	int index;
	struct gc2155_write_buffer buffer;
};

static const struct i2c_device_id gc2155_id[] = {
	{GC2155_NAME, 0},
	{}
};

/*
 * Register settings for various resolution
 */
static const struct gc2155_reg gc2155_reset_register[] = {
	{GC2155_8BIT, 0xfe, 0xf0},
	{GC2155_8BIT, 0xfe, 0xf0},
	{GC2155_8BIT, 0xfe, 0xf0},
	{GC2155_8BIT, 0xfc, 0x06},
	{GC2155_8BIT, 0xf6, 0x00},
	{GC2155_8BIT, 0xf7, 0x1d},
	{GC2155_8BIT, 0xf8, 0x85},
	{GC2155_8BIT, 0xfa, 0x00},
	{GC2155_8BIT, 0xf9, 0x8e},
	{GC2155_8BIT, 0xf2, 0x0f},
	{GC2155_8BIT, 0xfe, 0x00},
	{GC2155_8BIT, 0x03, 0x04},
	{GC2155_8BIT, 0x04, 0x00},
	{GC2155_8BIT, 0x05, 0x01},
	{GC2155_8BIT, 0x06, 0x68},
	{GC2155_8BIT, 0x09, 0x00},
	{GC2155_8BIT, 0x0a, 0x00},
	{GC2155_8BIT, 0x0b, 0x00},
	{GC2155_8BIT, 0x0c, 0x00},
	{GC2155_8BIT, 0x0d, 0x04},
	{GC2155_8BIT, 0x0e, 0xd0},//src width
	{GC2155_8BIT, 0x0f, 0x06},
	{GC2155_8BIT, 0x10, 0x62},
	{GC2155_8BIT, 0x12, 0x2e},
	{GC2155_8BIT, 0x17, 0x14},
	{GC2155_8BIT, 0x18, 0x0a},
	{GC2155_8BIT, 0x19, 0x0b},
	{GC2155_8BIT, 0x1a, 0x09},
	{GC2155_8BIT, 0x1b, 0x4b},
	{GC2155_8BIT, 0x1c, 0x07},
	{GC2155_8BIT, 0x1d, 0x10},
	{GC2155_8BIT, 0x1e, 0x98},
	{GC2155_8BIT, 0x1f, 0x78},
	{GC2155_8BIT, 0x20, 0x03},
	{GC2155_8BIT, 0x21, 0x60},
	{GC2155_8BIT, 0x22, 0xf0},
	{GC2155_8BIT, 0x24, 0x16},
	{GC2155_8BIT, 0x25, 0x01},
	{GC2155_8BIT, 0x26, 0x10},
	{GC2155_8BIT, 0x2d, 0x40},
	{GC2155_8BIT, 0x30, 0x01},
	{GC2155_8BIT, 0x33, 0x01},
	{GC2155_8BIT, 0x34, 0x01},
	{GC2155_8BIT, 0x80, 0x06},//ff//06
	{GC2155_8BIT, 0x81, 0x80},//24//00
	{GC2155_8BIT, 0x82, 0x30},//fa//03//J
	{GC2155_8BIT, 0x83, 0x00},
	{GC2155_8BIT, 0x84, 0x17},//DNDD mode
	{GC2155_8BIT, 0x86, 0x16},//06
	{GC2155_8BIT, 0x88, 0x03},
	{GC2155_8BIT, 0x89, 0x03},// 0xb
	{GC2155_8BIT, 0x85, 0x08},
	{GC2155_8BIT, 0x8a, 0x00},
	{GC2155_8BIT, 0x8b, 0x00},
	{GC2155_8BIT, 0xb0, 0x55},
	{GC2155_8BIT, 0xc3, 0x00},
	{GC2155_8BIT, 0xc4, 0x80},
	{GC2155_8BIT, 0xc5, 0x90},
	{GC2155_8BIT, 0xc6, 0x38},
	{GC2155_8BIT, 0xc7, 0x40},
	{GC2155_8BIT, 0xec, 0x06},
	{GC2155_8BIT, 0xed, 0x04},
	{GC2155_8BIT, 0xee, 0x60},
	{GC2155_8BIT, 0xef, 0x90},
	{GC2155_8BIT, 0xb6, 0x00},//disable AEC
	{GC2155_8BIT, 0x90, 0x01},
	{GC2155_8BIT, 0x91, 0x00},
	{GC2155_8BIT, 0x92, 0x00},//offset 0x10//00
	{GC2155_8BIT, 0x93, 0x00},
	{GC2155_8BIT, 0x94, 0x01},
	{GC2155_8BIT, 0x95, 0x04},
	{GC2155_8BIT, 0x96, 0xc0},//c0//d0
	{GC2155_8BIT, 0x97, 0x06},
	{GC2155_8BIT, 0x98, 0x50},
	{GC2155_8BIT, 0x9a, 0x02},
	{GC2155_8BIT, 0x18, 0x0a},
	{GC2155_8BIT, 0x40, 0x43},
	{GC2155_8BIT, 0x41, 0x28},
	{GC2155_8BIT, 0x42, 0x60},
	{GC2155_8BIT, 0x43, 0x54},
	{GC2155_8BIT, 0x5e, 0x00},
	{GC2155_8BIT, 0x5f, 0x00},
	{GC2155_8BIT, 0x60, 0x00},
	{GC2155_8BIT, 0x61, 0x00},
	{GC2155_8BIT, 0x62, 0x00},
	{GC2155_8BIT, 0x63, 0x00},
	{GC2155_8BIT, 0x64, 0x00},
	{GC2155_8BIT, 0x65, 0x00},
	{GC2155_8BIT, 0x66, 0x20}, //BLK
	{GC2155_8BIT, 0x67, 0x20}, //BLK
	{GC2155_8BIT, 0x68, 0x20}, //BLK
	{GC2155_8BIT, 0x69, 0x20}, //BLK
	{GC2155_8BIT, 0x6a, 0x00},
	{GC2155_8BIT, 0x6b, 0x00},
	{GC2155_8BIT, 0x6c, 0x00},
	{GC2155_8BIT, 0x6d, 0x00},
	{GC2155_8BIT, 0x6e, 0x00},
	{GC2155_8BIT, 0x6f, 0x00},
	{GC2155_8BIT, 0x70, 0x00},
	{GC2155_8BIT, 0x71, 0x00},
	{GC2155_8BIT, 0x72, 0xf0},
	{GC2155_8BIT, 0x7e, 0x00},
	{GC2155_8BIT, 0x7f, 0x3c},
	{GC2155_8BIT, 0xfe, 0x02},
	{GC2155_8BIT, 0x49, 0x00},
	{GC2155_8BIT, 0x4b, 0x02},
	{GC2155_8BIT, 0xfe, 0x00},
	{GC2155_8BIT, 0xfe, 0x01},
	{GC2155_8BIT, 0x01, 0x04},
	{GC2155_8BIT, 0x02, 0xc0},
	{GC2155_8BIT, 0x03, 0x04},
	{GC2155_8BIT, 0x04, 0x90},
	{GC2155_8BIT, 0x05, 0x30},
	{GC2155_8BIT, 0x06, 0x90},
	{GC2155_8BIT, 0x07, 0x20},
	{GC2155_8BIT, 0x08, 0x70},
	{GC2155_8BIT, 0x09, 0x00},
	{GC2155_8BIT, 0x0a, 0xc2},
	{GC2155_8BIT, 0x0b, 0x11},
	{GC2155_8BIT, 0x0c, 0x10},
	{GC2155_8BIT, 0x13, 0x2a},
	{GC2155_8BIT, 0x17, 0x00},
	{GC2155_8BIT, 0x1c, 0x11},
	{GC2155_8BIT, 0x1e, 0x61},
	{GC2155_8BIT, 0x1f, 0x40},
	{GC2155_8BIT, 0x20, 0x40},
	{GC2155_8BIT, 0x22, 0x80},
	{GC2155_8BIT, 0x23, 0x20},
	{GC2155_8BIT, 0x12, 0x00},
	{GC2155_8BIT, 0x15, 0x50},
	{GC2155_8BIT, 0x10, 0x31},
	{GC2155_8BIT, 0x3e, 0x28},
	{GC2155_8BIT, 0x3f, 0xe0},
	{GC2155_8BIT, 0x40, 0xe0},
	{GC2155_8BIT, 0x41, 0x0f},
	{GC2155_8BIT, 0xfe, 0x02},
	{GC2155_8BIT, 0x90, 0x6c},
	{GC2155_8BIT, 0x91, 0x02},
	{GC2155_8BIT, 0x92, 0x44},
	{GC2155_8BIT, 0x97, 0x78},
	{GC2155_8BIT, 0xa2, 0x11},
	{GC2155_8BIT, 0xfe, 0x00},
	{GC2155_8BIT, 0xfe, 0x02},
	{GC2155_8BIT, 0x80, 0xc1},
	{GC2155_8BIT, 0x81, 0x08},
	{GC2155_8BIT, 0x82, 0x05},
	{GC2155_8BIT, 0x83, 0x04},
	{GC2155_8BIT, 0x84, 0x0a},
	{GC2155_8BIT, 0x86, 0x80},
	{GC2155_8BIT, 0x87, 0x30},
	{GC2155_8BIT, 0x88, 0x15},
	{GC2155_8BIT, 0x89, 0x80},
	{GC2155_8BIT, 0x8a, 0x60},
	{GC2155_8BIT, 0x8b, 0x30},
	{GC2155_8BIT, 0xfe, 0x01},
	{GC2155_8BIT, 0x21, 0x14},
	{GC2155_8BIT, 0xfe, 0x02},
	{GC2155_8BIT, 0xa3, 0x60},
	{GC2155_8BIT, 0xa4, 0x40},
	{GC2155_8BIT, 0xa5, 0x40},
	{GC2155_8BIT, 0xa6, 0x80},
	{GC2155_8BIT, 0xab, 0x20},
	{GC2155_8BIT, 0xae, 0x0c},
	{GC2155_8BIT, 0xb3, 0x42},
	{GC2155_8BIT, 0xb4, 0x24},
	{GC2155_8BIT, 0xb6, 0x50},
	{GC2155_8BIT, 0xb7, 0x01},
	{GC2155_8BIT, 0xb9, 0x25},
	{GC2155_8BIT, 0xfe, 0x00},
	{GC2155_8BIT, 0xfe, 0x02},
	{GC2155_8BIT, 0x10, 0x13},
	{GC2155_8BIT, 0x11, 0x19},
	{GC2155_8BIT, 0x12, 0x20},
	{GC2155_8BIT, 0x13, 0x27},
	{GC2155_8BIT, 0x14, 0x37},
	{GC2155_8BIT, 0x15, 0x48},
	{GC2155_8BIT, 0x16, 0x54},
	{GC2155_8BIT, 0x17, 0x62},
	{GC2155_8BIT, 0x18, 0x79},
	{GC2155_8BIT, 0x19, 0x8a},
	{GC2155_8BIT, 0x1a, 0x99},
	{GC2155_8BIT, 0x1b, 0xa6},
	{GC2155_8BIT, 0x1c, 0xb1},
	{GC2155_8BIT, 0x1d, 0xc6},
	{GC2155_8BIT, 0x1e, 0xd2},
	{GC2155_8BIT, 0x1f, 0xde},
	{GC2155_8BIT, 0x20, 0xe8},
	{GC2155_8BIT, 0x21, 0xef},
	{GC2155_8BIT, 0x22, 0xf4},
	{GC2155_8BIT, 0x23, 0xfb},
	{GC2155_8BIT, 0x24, 0xfd},
	{GC2155_8BIT, 0x25, 0xff},
	{GC2155_8BIT, 0xfe, 0x02},
	{GC2155_8BIT, 0x26, 0x0d},
	{GC2155_8BIT, 0x27, 0x12},
	{GC2155_8BIT, 0x28, 0x17},
	{GC2155_8BIT, 0x29, 0x1c},
	{GC2155_8BIT, 0x2a, 0x27},
	{GC2155_8BIT, 0x2b, 0x34},
	{GC2155_8BIT, 0x2c, 0x44},
	{GC2155_8BIT, 0x2d, 0x55},
	{GC2155_8BIT, 0x2e, 0x6e},
	{GC2155_8BIT, 0x2f, 0x81},
	{GC2155_8BIT, 0x30, 0x91},
	{GC2155_8BIT, 0x31, 0x9c},
	{GC2155_8BIT, 0x32, 0xaa},
	{GC2155_8BIT, 0x33, 0xbb},
	{GC2155_8BIT, 0x34, 0xca},
	{GC2155_8BIT, 0x35, 0xd5},
	{GC2155_8BIT, 0x36, 0xe0},
	{GC2155_8BIT, 0x37, 0xe7},
	{GC2155_8BIT, 0x38, 0xed},
	{GC2155_8BIT, 0x39, 0xf6},
	{GC2155_8BIT, 0x3a, 0xfb},
	{GC2155_8BIT, 0x3b, 0xff},
	{GC2155_8BIT, 0xfe, 0x02},
	{GC2155_8BIT, 0xd1, 0x20},
	{GC2155_8BIT, 0xd2, 0x20},
	{GC2155_8BIT, 0xdd, 0x80},
	{GC2155_8BIT, 0xde, 0x84},
	{GC2155_8BIT, 0xfe, 0x01},
	{GC2155_8BIT, 0xc2, 0x15},
	{GC2155_8BIT, 0xc3, 0x0c},
	{GC2155_8BIT, 0xc4, 0x0b},
	{GC2155_8BIT, 0xc8, 0x12},
	{GC2155_8BIT, 0xc9, 0x0b},
	{GC2155_8BIT, 0xca, 0x07},
	{GC2155_8BIT, 0xbc, 0x3e},
	{GC2155_8BIT, 0xbd, 0x2e},
	{GC2155_8BIT, 0xbe, 0x2d},
	{GC2155_8BIT, 0xb6, 0x3e},
	{GC2155_8BIT, 0xb7, 0x2e},
	{GC2155_8BIT, 0xb8, 0x2d},
	{GC2155_8BIT, 0xc5, 0x00},
	{GC2155_8BIT, 0xc6, 0x00},
	{GC2155_8BIT, 0xc7, 0x00},
	{GC2155_8BIT, 0xcb, 0x00},
	{GC2155_8BIT, 0xcc, 0x00},
	{GC2155_8BIT, 0xcd, 0x00},
	{GC2155_8BIT, 0xbf, 0x09},
	{GC2155_8BIT, 0xc0, 0x00},
	{GC2155_8BIT, 0xc1, 0x00},
	{GC2155_8BIT, 0xb9, 0x09},
	{GC2155_8BIT, 0xba, 0x00},
	{GC2155_8BIT, 0xbb, 0x00},
	{GC2155_8BIT, 0xaa, 0x01},
	{GC2155_8BIT, 0xab, 0x0f},
	{GC2155_8BIT, 0xac, 0x0d},
	{GC2155_8BIT, 0xad, 0x00},
	{GC2155_8BIT, 0xae, 0x06},
	{GC2155_8BIT, 0xaf, 0x08},
	{GC2155_8BIT, 0xb0, 0x00},
	{GC2155_8BIT, 0xb1, 0x06},
	{GC2155_8BIT, 0xb2, 0x02},
	{GC2155_8BIT, 0xb3, 0x01},
	{GC2155_8BIT, 0xb4, 0x08},
	{GC2155_8BIT, 0xb5, 0x05},
	{GC2155_8BIT, 0xd0, 0x00},
	{GC2155_8BIT, 0xd1, 0x00},
	{GC2155_8BIT, 0xd2, 0x00},
	{GC2155_8BIT, 0xd6, 0x00},
	{GC2155_8BIT, 0xd7, 0x00},
	{GC2155_8BIT, 0xd8, 0x00},
	{GC2155_8BIT, 0xd9, 0x00},
	{GC2155_8BIT, 0xda, 0x00},
	{GC2155_8BIT, 0xdb, 0x00},
	{GC2155_8BIT, 0xd3, 0x00},
	{GC2155_8BIT, 0xd4, 0x00},
	{GC2155_8BIT, 0xd5, 0x00},
	{GC2155_8BIT, 0xa4, 0x00},
	{GC2155_8BIT, 0xa5, 0x00},
	{GC2155_8BIT, 0xa6, 0x77},
	{GC2155_8BIT, 0xa7, 0x77},
	{GC2155_8BIT, 0xa8, 0x77},
	{GC2155_8BIT, 0xa9, 0x77},
	{GC2155_8BIT, 0xa1, 0x80},
	{GC2155_8BIT, 0xa2, 0x80},
	{GC2155_8BIT, 0xfe, 0x01},
	{GC2155_8BIT, 0xdc, 0x35},
	{GC2155_8BIT, 0xdd, 0x28},
	{GC2155_8BIT, 0xdf, 0x0c},
	{GC2155_8BIT, 0xe0, 0x70},
	{GC2155_8BIT, 0xe1, 0x80},
	{GC2155_8BIT, 0xe2, 0x80},
	{GC2155_8BIT, 0xe3, 0x80},
	{GC2155_8BIT, 0xe6, 0x90},
	{GC2155_8BIT, 0xe7, 0x50},
	{GC2155_8BIT, 0xe8, 0x90},
	{GC2155_8BIT, 0xe9, 0x60},
	{GC2155_8BIT, 0xfe, 0x00},
	{GC2155_8BIT, 0xfe, 0x01},
	{GC2155_8BIT, 0x4f, 0x00},
	{GC2155_8BIT, 0x4f, 0x00},
	{GC2155_8BIT, 0x4b, 0x01},
	{GC2155_8BIT, 0x4f, 0x00},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0x6f},
	{GC2155_8BIT, 0x4e, 0x02},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0x70},
	{GC2155_8BIT, 0x4e, 0x02},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0x8f},
	{GC2155_8BIT, 0x4e, 0x02},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0x90},
	{GC2155_8BIT, 0x4e, 0x02},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0x91},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xaf},
	{GC2155_8BIT, 0x4e, 0x02},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xb0},
	{GC2155_8BIT, 0x4e, 0x02},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xb1},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xcf},
	{GC2155_8BIT, 0x4e, 0x02},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xd0},
	{GC2155_8BIT, 0x4e, 0x02},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xed},
	{GC2155_8BIT, 0x4e, 0x33},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xcd},
	{GC2155_8BIT, 0x4e, 0x33},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xec},
	{GC2155_8BIT, 0x4e, 0x03},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0x6c},
	{GC2155_8BIT, 0x4e, 0x03},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0x6d},
	{GC2155_8BIT, 0x4e, 0x03},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0x6e},
	{GC2155_8BIT, 0x4e, 0x03},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0x8c},
	{GC2155_8BIT, 0x4e, 0x03},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0x8d},
	{GC2155_8BIT, 0x4e, 0x03},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0x8e},
	{GC2155_8BIT, 0x4e, 0x02},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xab},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xac},
	{GC2155_8BIT, 0x4e, 0x03},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xad},
	{GC2155_8BIT, 0x4e, 0x03},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xae},
	{GC2155_8BIT, 0x4e, 0x02},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xcb},
	{GC2155_8BIT, 0x4e, 0x03},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xcc},
	{GC2155_8BIT, 0x4e, 0x03},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xce},
	{GC2155_8BIT, 0x4e, 0x02},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xea},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xec},
	{GC2155_8BIT, 0x4e, 0x03},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xee},
	{GC2155_8BIT, 0x4e, 0x03},
	{GC2155_8BIT, 0x4c, 0x02},
	{GC2155_8BIT, 0x4d, 0x0c},
	{GC2155_8BIT, 0x4e, 0x03},
	{GC2155_8BIT, 0x4c, 0x02},
	{GC2155_8BIT, 0x4d, 0x0d},
	{GC2155_8BIT, 0x4e, 0x03},
	{GC2155_8BIT, 0x4c, 0x02},
	{GC2155_8BIT, 0x4d, 0x2c},
	{GC2155_8BIT, 0x4e, 0x03},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xca},
	{GC2155_8BIT, 0x4e, 0x34},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xcb},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xeb},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xc9},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xa9},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xe9},
	{GC2155_8BIT, 0x4e, 0x04},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xc9},
	{GC2155_8BIT, 0x4c, 0x02},
	{GC2155_8BIT, 0x4d, 0x0a},
	{GC2155_8BIT, 0x4e, 0x05},
	{GC2155_8BIT, 0x4c, 0x02},
	{GC2155_8BIT, 0x4d, 0x0b},
	{GC2155_8BIT, 0x4e, 0x35},
	{GC2155_8BIT, 0x4c, 0x02},
	{GC2155_8BIT, 0x4d, 0x09},
	{GC2155_8BIT, 0x4c, 0x01},
	{GC2155_8BIT, 0x4d, 0xea},
	{GC2155_8BIT, 0x4c, 0x02},
	{GC2155_8BIT, 0x4d, 0x2a},
	{GC2155_8BIT, 0x4c, 0x02},
	{GC2155_8BIT, 0x4d, 0x49},
	{GC2155_8BIT, 0x4c, 0x02},
	{GC2155_8BIT, 0x4d, 0x29},
	{GC2155_8BIT, 0x4c, 0x02},
	{GC2155_8BIT, 0x4d, 0xc8},
	{GC2155_8BIT, 0x4e, 0x36},
	{GC2155_8BIT, 0x4c, 0x02},
	{GC2155_8BIT, 0x4d, 0xa8},
	{GC2155_8BIT, 0x4e, 0x36},
	{GC2155_8BIT, 0x4c, 0x02},
	{GC2155_8BIT, 0x4d, 0x88},
	{GC2155_8BIT, 0x4e, 0x06},
	{GC2155_8BIT, 0x4c, 0x02},
	{GC2155_8BIT, 0x4d, 0xa9},
	{GC2155_8BIT, 0x4e, 0x06},
	{GC2155_8BIT, 0x4c, 0x02},
	{GC2155_8BIT, 0x4d, 0xc9},
	{GC2155_8BIT, 0x4e, 0x06},
	{GC2155_8BIT, 0x4c, 0x02},
	{GC2155_8BIT, 0x4d, 0x89},
	{GC2155_8BIT, 0x4e, 0x06},
	{GC2155_8BIT, 0x4c, 0x02},
	{GC2155_8BIT, 0x4d, 0x69},
	{GC2155_8BIT, 0x4e, 0x06},
	{GC2155_8BIT, 0x4c, 0x02},
	{GC2155_8BIT, 0x4d, 0x6a},
	{GC2155_8BIT, 0x4c, 0x02},
	{GC2155_8BIT, 0x4d, 0xc7},
	{GC2155_8BIT, 0x4e, 0x07},
	{GC2155_8BIT, 0x4c, 0x02},
	{GC2155_8BIT, 0x4d, 0xe7},
	{GC2155_8BIT, 0x4e, 0x07},
	{GC2155_8BIT, 0x4c, 0x03},
	{GC2155_8BIT, 0x4d, 0x06},
	{GC2155_8BIT, 0x4c, 0x03},
	{GC2155_8BIT, 0x4d, 0x07},
	{GC2155_8BIT, 0x4e, 0x37},
	{GC2155_8BIT, 0x4c, 0x03},
	{GC2155_8BIT, 0x4d, 0x08},
	{GC2155_8BIT, 0x4e, 0x07},
	{GC2155_8BIT, 0x4c, 0x02},
	{GC2155_8BIT, 0x4d, 0xe8},
	{GC2155_8BIT, 0x4e, 0x07},
	{GC2155_8BIT, 0x4c, 0x03},
	{GC2155_8BIT, 0x4d, 0x28},
	{GC2155_8BIT, 0x4e, 0x07},
	{GC2155_8BIT, 0x4f, 0x01},
	{GC2155_8BIT, 0xfe, 0x01},
	{GC2155_8BIT, 0x50, 0x80},
	{GC2155_8BIT, 0x51, 0xa8},
	{GC2155_8BIT, 0x52, 0x57},
	{GC2155_8BIT, 0x53, 0x38},
	{GC2155_8BIT, 0x54, 0xc7},
	{GC2155_8BIT, 0x56, 0x0e},
	{GC2155_8BIT, 0x58, 0x08},
	{GC2155_8BIT, 0x5b, 0x00},
	{GC2155_8BIT, 0x5c, 0x74},
	{GC2155_8BIT, 0x5d, 0x8b},
	{GC2155_8BIT, 0x61, 0xd3},
	{GC2155_8BIT, 0x62, 0x90},
	{GC2155_8BIT, 0x63, 0x04},
	{GC2155_8BIT, 0x65, 0x04},
	{GC2155_8BIT, 0x67, 0xb2},
	{GC2155_8BIT, 0x68, 0xac},
	{GC2155_8BIT, 0x69, 0x00},
	{GC2155_8BIT, 0x6a, 0xb2},
	{GC2155_8BIT, 0x6b, 0xac},
	{GC2155_8BIT, 0x6c, 0xdc},
	{GC2155_8BIT, 0x6d, 0xb0},
	{GC2155_8BIT, 0x6e, 0x30},
	{GC2155_8BIT, 0x6f, 0xff},
	{GC2155_8BIT, 0x73, 0x00},
	{GC2155_8BIT, 0x70, 0x05},
	{GC2155_8BIT, 0x71, 0x80},
	{GC2155_8BIT, 0x72, 0xc1},
	{GC2155_8BIT, 0x74, 0x01},
	{GC2155_8BIT, 0x75, 0x01},
	{GC2155_8BIT, 0x7f, 0x0c},
	{GC2155_8BIT, 0x76, 0x70},
	{GC2155_8BIT, 0x77, 0x48},
	{GC2155_8BIT, 0x78, 0x90},
	{GC2155_8BIT, 0x79, 0x55},
	{GC2155_8BIT, 0x7a, 0x48},
	{GC2155_8BIT, 0x7b, 0x60},
	{GC2155_8BIT, 0xfe, 0x00},
	{GC2155_8BIT, 0xfe, 0x02},
	{GC2155_8BIT, 0xc0, 0x01},
	{GC2155_8BIT, 0xc1, 0x50},
	{GC2155_8BIT, 0xc2, 0xf8},
	{GC2155_8BIT, 0xc3, 0x02},
	{GC2155_8BIT, 0xc4, 0xe0},
	{GC2155_8BIT, 0xc5, 0x45},
	{GC2155_8BIT, 0xc6, 0xe8},
	{GC2155_8BIT, 0xc7, 0x55},
	{GC2155_8BIT, 0xc8, 0xf5},
	{GC2155_8BIT, 0xc9, 0x00},
	{GC2155_8BIT, 0xca, 0xea},
	{GC2155_8BIT, 0xcb, 0x45},
	{GC2155_8BIT, 0xcc, 0xf0},
	{GC2155_8BIT, 0xCd, 0x45},
	{GC2155_8BIT, 0xce, 0xf0},
	{GC2155_8BIT, 0xcf, 0x00},
	{GC2155_8BIT, 0xe3, 0xf0},
	{GC2155_8BIT, 0xe4, 0x45},
	{GC2155_8BIT, 0xe5, 0xe8},
	{GC2155_8BIT, 0xfe, 0x00},
	{GC2155_8BIT, 0xf2, 0x0f},
	{GC2155_8BIT, 0xfe, 0x00},
	{GC2155_8BIT, 0x05, 0x01},
	{GC2155_8BIT, 0x06, 0x10},
	{GC2155_8BIT, 0x07, 0x00},
	{GC2155_8BIT, 0x08, 0x82},//VB : a0
	{GC2155_8BIT, 0xfe, 0x01},
	{GC2155_8BIT, 0x25, 0x00},
	{GC2155_8BIT, 0x26, 0xd4},
	{GC2155_8BIT, 0x27, 0x04},
	{GC2155_8BIT, 0x28, 0xf8},
	{GC2155_8BIT, 0x29, 0x08},
	{GC2155_8BIT, 0x2a, 0x48},
	{GC2155_8BIT, 0x2b, 0x0b},
	{GC2155_8BIT, 0x2c, 0x98},
	{GC2155_8BIT, 0x2d, 0x0f},
	{GC2155_8BIT, 0x2e, 0xbc},
	{GC2155_8BIT, 0xfe, 0x00},
	{GC2155_8BIT, 0xfe, 0x03},
	{GC2155_8BIT, 0x01, 0x83},//discontin
	{GC2155_8BIT, 0x02, 0x22},
	{GC2155_8BIT, 0x03, 0x10},
	{GC2155_8BIT, 0x04, 0x20},
	{GC2155_8BIT, 0x05, 0x00},
	{GC2155_8BIT, 0x06, 0x88},
	{GC2155_8BIT, 0x10, 0x00},
	{GC2155_8BIT, 0x11, 0x2b},
	{GC2155_8BIT, 0x12, 0xe4},
	{GC2155_8BIT, 0x13, 0x07},
	{GC2155_8BIT, 0x15, 0x11},//discontin
	{GC2155_8BIT, 0x17, 0xf1},
	{GC2155_8BIT, 0x29, 0x01},//data prepare
	{GC2155_8BIT, 0x22, 0x01},//clk prepare
	{GC2155_8BIT, 0xfe, 0x00},
	{GC2155_TOK_TERM, 0, 0},
};

static struct gc2155_reg const gc2155_480p_30fps[] = {
	/* window */
	{GC2155_8BIT, 0xfe, 0x00},
	{GC2155_8BIT, 0x09, 0x01},
	{GC2155_8BIT, 0x0a, 0x68}, //Start Y
	{GC2155_8BIT, 0x0b, 0x01},
	{GC2155_8BIT, 0x0c, 0xB8}, //Start X
	{GC2155_8BIT, 0x0d, 0x02},
	{GC2155_8BIT, 0x0e, 0x00},
	{GC2155_8BIT, 0x0f, 0x02},
	{GC2155_8BIT, 0x10, 0xf0},
	/* CROP */
	{GC2155_8BIT, 0x90, 0x01},
	{GC2155_8BIT, 0x92, 0x00},
	{GC2155_8BIT, 0x94, 0x01},
	{GC2155_8BIT, 0x95, 0x01},  //CROP H
	{GC2155_8BIT, 0x96, 0xf0},
	{GC2155_8BIT, 0x97, 0x02},  //CROP W 736
	{GC2155_8BIT, 0x98, 0xe0},
	/* PLL */
	{GC2155_8BIT, 0xf8, 0x85},
	/* MIPI */
	{GC2155_8BIT, 0xfe, 0x03},
	{GC2155_8BIT, 0x12, 0x98}, //1280*10/8
	{GC2155_8BIT, 0x13, 0x03},
	{GC2155_8BIT, 0xfe ,0x00},
	{GC2155_TOK_TERM, 0, 0},
};

static struct gc2155_reg const gc2155_720p_30fps[] = {
	/* window */
	{GC2155_8BIT, 0xfe, 0x00},
	{GC2155_8BIT, 0x0a, 0xd8},
	{GC2155_8BIT, 0x0c, 0x88},
	{GC2155_8BIT, 0x0d, 0x03},
	{GC2155_8BIT, 0x0e, 0x00},
	{GC2155_8BIT, 0x0f, 0x05},
	{GC2155_8BIT, 0x10, 0x30},
	/* subsample */
	{GC2155_8BIT, 0x99, 0x11}, //OFF
	/* CROP */
	{GC2155_8BIT, 0x90, 0x01},
	{GC2155_8BIT, 0x92, 0x00},
	{GC2155_8BIT, 0x94, 0x01},
	{GC2155_8BIT, 0x95, 0x02},  //CROP H
	{GC2155_8BIT, 0x96, 0xe0},
	{GC2155_8BIT, 0x97, 0x05},  //CROP W
	{GC2155_8BIT, 0x98, 0x10},
	/* MIPI */
	{GC2155_8BIT, 0xfe, 0x03},
	{GC2155_8BIT, 0x12, 0x54}, //1280*10/8
	{GC2155_8BIT, 0x13, 0x06},
	{GC2155_8BIT, 0xfe, 0x00},
	{GC2155_TOK_TERM, 0, 0},
};

static struct gc2155_reg const gc2155_2M_30fps[] = {
	/* window */
	{GC2155_8BIT, 0xfe, 0x00},
	{GC2155_8BIT, 0x0a, 0x00},
	{GC2155_8BIT, 0x0c, 0x00},
	{GC2155_8BIT, 0x0d, 0x04},
	{GC2155_8BIT, 0x0e, 0xd0},
	{GC2155_8BIT, 0x0f, 0x06},
	{GC2155_8BIT, 0x10, 0x62},
	/* subsample */
	{GC2155_8BIT, 0x99, 0x11}, //OFF
	/* CROP */
	{GC2155_8BIT, 0x90, 0x01},
	{GC2155_8BIT, 0x92, 0x00},
	{GC2155_8BIT, 0x94, 0x01},
	{GC2155_8BIT, 0x95, 0x04},  //CROP H    P H
	{GC2155_8BIT, 0x96, 0xc0},
	{GC2155_8BIT, 0x97, 0x06},  //CROP W    P W
	{GC2155_8BIT, 0x98, 0x50},
	/* MIPI */
	{GC2155_8BIT, 0xfe, 0x03},
	{GC2155_8BIT, 0x12, 0xe4}, //1280*10/8  *10/8
	{GC2155_8BIT, 0x13, 0x07},
        {GC2155_8BIT, 0xfe, 0x00},
	{GC2155_TOK_TERM, 0, 0},
};

static struct gc2155_reg const gc2155_1616x916_30fps[] = {
	/* window */
	{GC2155_8BIT, 0xfe, 0x00},
	{GC2155_8BIT, 0x0a, 0x00},
	{GC2155_8BIT, 0x0c, 0x00},
	{GC2155_8BIT, 0x0d, 0x04}, //1232
	{GC2155_8BIT, 0x0e, 0xd0},
	{GC2155_8BIT, 0x0f, 0x06}, //1634
	{GC2155_8BIT, 0x10, 0x62},
	/* subsample */
	{GC2155_8BIT, 0x99, 0x11}, //OFF
	/* CROP */
	{GC2155_8BIT, 0x90, 0x01},
	{GC2155_8BIT, 0x92, 0x96}, //WinY 150
	{GC2155_8BIT, 0x94, 0x01},
	{GC2155_8BIT, 0x95, 0x03}, //CROP H    P H 916
	{GC2155_8BIT, 0x96, 0x94},
	{GC2155_8BIT, 0x97, 0x06}, //CROP W    P W  1616
	{GC2155_8BIT, 0x98, 0x50},
	/* MIPI */
	{GC2155_8BIT, 0xfe, 0x03},
	{GC2155_8BIT, 0x12, 0xe4}, //1280*10/8  *10/8
	{GC2155_8BIT, 0x13, 0x07},
	{GC2155_8BIT, 0xfe, 0x00},
	{GC2155_TOK_TERM, 0, 0},
};

static struct gc2155_reg const gc2155_1616x1080_30fps[] = {
	/* window */
	{GC2155_8BIT, 0xfe, 0x00},
	{GC2155_8BIT, 0x0a, 0x00},
	{GC2155_8BIT, 0x0c, 0x00},
	{GC2155_8BIT, 0x0d, 0x04}, //1232
	{GC2155_8BIT, 0x0e, 0xd0},
	{GC2155_8BIT, 0x0f, 0x06}, //1634
	{GC2155_8BIT, 0x10, 0x62},
	/* subsample */
	{GC2155_8BIT, 0x99, 0x11}, //OFF
	/* CROP */
	{GC2155_8BIT, 0x90, 0x01},
	{GC2155_8BIT, 0x92, 0x00},
	{GC2155_8BIT, 0x94, 0x01},
	{GC2155_8BIT, 0x95, 0x04}, //CROP H    P H 1080
	{GC2155_8BIT, 0x96, 0x38},
	{GC2155_8BIT, 0x97, 0x06}, //CROP W    P W  1616
	{GC2155_8BIT, 0x98, 0x50},
	/* MIPI */
	{GC2155_8BIT, 0xfe, 0x03},
	{GC2155_8BIT, 0x12, 0xe4}, //1280*10/8  *10/8
	{GC2155_8BIT, 0x13, 0x07},
	{GC2155_8BIT, 0xfe, 0x00},
	{GC2155_TOK_TERM, 0, 0},
};

struct gc2155_resolution gc2155_res_preview[] = {
	{
		.desc = "gc2155_2M_30fps",
		.width = 1616,
		.height = 1216,
		.fps = 30,
		//.pix_clk_freq = 73,
		.used = 0,
		.pixels_per_line = 1632,
		.lines_per_frame = 1248,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 2,
		.regs = gc2155_2M_30fps,
	},
	{
		.desc = "gc2155_1616x916_30fps",
		.width = 1616,
		.height = 916,
		.fps = 30,
		//.pix_clk_freq = 73,
		.used = 0,
		.pixels_per_line = 1632,
		.lines_per_frame = 1248,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 2,
		.regs = gc2155_1616x916_30fps,
	},
};
#define N_RES_PREVIEW (ARRAY_SIZE(gc2155_res_preview))

struct gc2155_resolution gc2155_res_still[] = {
	{
		.desc = "gc2155_2M_30fps",
		.width = 1616,
		.height = 1216,
		.fps = 30,
		//.pix_clk_freq = 73,//calculator in gc2155_get_intg_factor
		.used = 0,
		.pixels_per_line = 1632,//calculator in gc2155_get_intg_factor
		.lines_per_frame = 1248,//calculator in gc2155_get_intg_factor
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 2,
		.regs = gc2155_2M_30fps,
	},
	{
		.desc = "gc2155_1616x916_30fps",
		.width = 1616,
		.height = 916,
		.fps = 30,
		//.pix_clk_freq = 73,
		.used = 0,
		.pixels_per_line = 1632,
		.lines_per_frame = 1248,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 2,
		.regs = gc2155_1616x916_30fps,
	},
};
#define N_RES_STILL (ARRAY_SIZE(gc2155_res_still))

struct gc2155_resolution gc2155_res_video[] = {
	{
		.desc = "gc2155_1616x916_30fps",
		.width = 1616,
		.height = 916,
		.fps = 30,
		//.pix_clk_freq = 73,
		.used = 0,
		.pixels_per_line = 1632,
		.lines_per_frame = 1248,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 2,
		.regs = gc2155_1616x916_30fps,
	},
	{
		.desc = "gc2155_1616x1080_30fps",
		.width = 1616,
		.height = 1080,
		.fps = 30,
		//.pix_clk_freq = 73,
		.used = 0,
		.pixels_per_line = 1632,
		.lines_per_frame = 1248,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 2,
		.regs = gc2155_1616x1080_30fps,
	},
};
#define N_RES_VIDEO (ARRAY_SIZE(gc2155_res_video))

static struct gc2155_resolution *gc2155_res = gc2155_res_preview;
static int N_RES = N_RES_PREVIEW;
#endif
