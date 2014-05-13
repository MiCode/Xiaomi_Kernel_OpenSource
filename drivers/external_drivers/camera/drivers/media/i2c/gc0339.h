/*
 * Support for GalaxyCore GC0339 VGA camera sensor.
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

#ifndef __GC0339_H__
#define __GC0339_H__
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

#define GC0339_NAME		"gc0339_raw"

/* Defines for register writes and register array processing */
#define I2C_MSG_LENGTH		1
#define I2C_RETRY_COUNT		5

#define GC0339_FOCAL_LENGTH_NUM	278	/*2.78mm*/
#define GC0339_FOCAL_LENGTH_DEM	100
#define GC0339_F_NUMBER_DEFAULT_NUM	26
#define GC0339_F_NUMBER_DEM	10

#define MAX_FMTS		1

/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define GC0339_FOCAL_LENGTH_DEFAULT 0x1160064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define GC0339_F_NUMBER_DEFAULT 0x1a000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define GC0339_F_NUMBER_RANGE 0x1a0a1a0a
#define GC0339_ID	0xc8

#define GC0339_FINE_INTG_TIME_MIN 0
#define GC0339_FINE_INTG_TIME_MAX_MARGIN 0
#define GC0339_COARSE_INTG_TIME_MIN 1
#define GC0339_COARSE_INTG_TIME_MAX_MARGIN 6

/*
 * GC0339 System control registers
 */
#define GC0339_SW_STREAM			0x60

#define GC0339_SC_CMMN_CHIP_ID		0x0

#define GC0339_AEC_PK_EXPO_H			0x03
#define GC0339_AEC_PK_EXPO_L			0x04
#define GC0339_AGC_ADJ			0x50
#if 0
#define GC0339_GROUP_ACCESS			0x3208
#endif

#define GC0339_H_CROP_START_H			0x07
#define GC0339_H_CROP_START_L			0x08
#define GC0339_V_CROP_START_H			0x05
#define GC0339_V_CROP_START_L			0x06
#define GC0339_H_OUTSIZE_H			0x0B
#define GC0339_H_OUTSIZE_L			0x0C
#define GC0339_V_OUTSIZE_H			0x09
#define GC0339_V_OUTSIZE_L			0x0A
#define GC0339_H_BLANKING_H			0x0F /* [3:0] */
#define GC0339_H_BLANKING_L			0x01
#define GC0339_V_BLANKING_H			0x0F /* [7:4] */
#define GC0339_V_BLANKING_L			0x02
#define GC0339_SH_DELAY			0x12

#define GC0339_START_STREAMING			0x98 /* 10-bit enable */
#define GC0339_STOP_STREAMING			0x88 /* 10-bit disable */

#define GC0339_BIN_FACTOR_MAX			3

#define REG_VER1 0

struct regval_list {
	u16 reg_num;
	u8 value;
};

struct gc0339_resolution {
	u8 *desc;
	const struct gc0339_reg *regs;
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

struct gc0339_format {
	u8 *desc;
	u32 pixelformat;
	struct gc0339_reg *regs;
};

struct gc0339_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, s32 value);
};

/*
 * gc0339 device structure.
 */
struct gc0339_device {
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

enum gc0339_tok_type {
	GC0339_8BIT  = 0x0001,
	GC0339_TOK_TERM   = 0xf000,	/* terminating token for reg list */
	GC0339_TOK_DELAY  = 0xfe00,	/* delay token for reg list */
	GC0339_TOK_MASK = 0xfff0
};

/**
 * struct gc0339_reg - MI sensor  register format
 * @type: type of the register
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 *
 * Define a structure for sensor register initialization values
 */
struct gc0339_reg {
	enum gc0339_tok_type type;
	u8 reg;
	u8 val;	/* @set value for read/mod/write, @mask */
};

#define to_gc0339_sensor(x) container_of(x, struct gc0339_device, sd)

#define GC0339_MAX_WRITE_BUF_SIZE	30

struct gc0339_write_buffer {
	u8 addr;
	u8 data[GC0339_MAX_WRITE_BUF_SIZE];
};

struct gc0339_write_ctrl {
	int index;
	struct gc0339_write_buffer buffer;
};

static const struct i2c_device_id gc0339_id[] = {
	{GC0339_NAME, 0},
	{}
};

/*
 * Register settings for various resolution
 */
static const struct gc0339_reg gc0339_reset_register[] = {
#if REG_VER1
	{GC0339_8BIT, 0xFC, 0x10},
	{GC0339_8BIT, 0xFE, 0x00},
	{GC0339_8BIT, 0xF6, 0x05},
	{GC0339_8BIT, 0xF7, 0x01},
	{GC0339_8BIT, 0xF7, 0x03},
	{GC0339_8BIT, 0xFC, 0x16},
	
	{GC0339_8BIT, 0x06, 0x00},
	{GC0339_8BIT, 0x08, 0x05},
	{GC0339_8BIT, 0x09, 0x01}, /* 484 */
	{GC0339_8BIT, 0x0A, 0xE4},
	{GC0339_8BIT, 0x0B, 0x02}, /* 644 */
	{GC0339_8BIT, 0x0C, 0x84},
	
	{GC0339_8BIT, 0x01, 0x90}, /* DummyHor 144 */
	{GC0339_8BIT, 0x02, 0x2F}, /* DummyVer 47 */
	
	{GC0339_8BIT, 0x0F, 0x00}, /* DummyHor 144 */
	{GC0339_8BIT, 0x14, 0x00},
	{GC0339_8BIT, 0x1A, 0x21},
	{GC0339_8BIT, 0x1B, 0x08},
	{GC0339_8BIT, 0x1C, 0x19},
	{GC0339_8BIT, 0x1D, 0xEA},
	{GC0339_8BIT, 0x20, 0xB0},
	{GC0339_8BIT, 0x2E, 0x00},
	
	{GC0339_8BIT, 0x30, 0xB7},
	{GC0339_8BIT, 0x31, 0x7F},
	{GC0339_8BIT, 0x32, 0x00},
	{GC0339_8BIT, 0x39, 0x04},
	{GC0339_8BIT, 0x3A, 0x20},
	{GC0339_8BIT, 0x3B, 0x20},
	{GC0339_8BIT, 0x3C, 0x00},
	{GC0339_8BIT, 0x3D, 0x00},
	{GC0339_8BIT, 0x3E, 0x00},
	{GC0339_8BIT, 0x3F, 0x00},
	
	{GC0339_8BIT, 0x62, 0x20},
	{GC0339_8BIT, 0x63, 0x03},
	{GC0339_8BIT, 0x69, 0x13},
	{GC0339_8BIT, 0x60, 0x80},
	{GC0339_8BIT, 0x65, 0x20}, /* 20 -> 21 */
	{GC0339_8BIT, 0x6C, 0x40},
	{GC0339_8BIT, 0x6D, 0x01},
	{GC0339_8BIT, 0x6A, 0x33},
	
	{GC0339_8BIT, 0x4A, 0x50},
	{GC0339_8BIT, 0x4B, 0x40},
	{GC0339_8BIT, 0x4C, 0x40},
	{GC0339_8BIT, 0xE8, 0x04},
	{GC0339_8BIT, 0xE9, 0xBB},
	
	{GC0339_8BIT, 0x42, 0x20},
	{GC0339_8BIT, 0x47, 0x10},
	
	{GC0339_8BIT, 0x50, 0x80},
	
	{GC0339_8BIT, 0xD0, 0x00},
	{GC0339_8BIT, 0xD2, 0x00}, /* disable AE */
	{GC0339_8BIT, 0xD3, 0x50},
	
	{GC0339_8BIT, 0x71, 0x01},
	{GC0339_8BIT, 0x72, 0x01},
	{GC0339_8BIT, 0x73, 0x05},
	{GC0339_8BIT, 0x74, 0x01},
	{GC0339_8BIT, 0x76, 0x03},
	{GC0339_8BIT, 0x79, 0x01},
	{GC0339_8BIT, 0x7B, 0x03},
#else
	{GC0339_8BIT, 0xfc, 0x10},
	{GC0339_8BIT, 0xfe, 0x00},
	{GC0339_8BIT, 0xf6, 0x07},
	{GC0339_8BIT, 0xf7, 0x01},
	{GC0339_8BIT, 0xf7, 0x03},
	{GC0339_8BIT, 0xfc, 0x16},
	{GC0339_8BIT, 0x06, 0x01},
	{GC0339_8BIT, 0x08, 0x00},
	{GC0339_8BIT, 0x09, 0x01},
	{GC0339_8BIT, 0x0a, 0xf2},
	{GC0339_8BIT, 0x0b, 0x02},
	{GC0339_8BIT, 0x0c, 0x94},
	{GC0339_8BIT, 0x0f, 0x02},
	{GC0339_8BIT, 0x14, 0x23},
	{GC0339_8BIT, 0x1a, 0x21},
	{GC0339_8BIT, 0x1b, 0x08},
	{GC0339_8BIT, 0x1c, 0x19},
	{GC0339_8BIT, 0x1d, 0xea},
	{GC0339_8BIT, 0x61, 0x2b},
	{GC0339_8BIT, 0x62, 0x34},
	{GC0339_8BIT, 0x63, 0x03},
	{GC0339_8BIT, 0x30, 0xb7},
	{GC0339_8BIT, 0x31, 0x7f},
	{GC0339_8BIT, 0x32, 0x00},
	{GC0339_8BIT, 0x39, 0x04},
	{GC0339_8BIT, 0x3a, 0x20},
	{GC0339_8BIT, 0x3b, 0x20},
	{GC0339_8BIT, 0x3c, 0x04},
	{GC0339_8BIT, 0x3d, 0x04},
	{GC0339_8BIT, 0x3e, 0x04},
	{GC0339_8BIT, 0x3f, 0x04},
	{GC0339_8BIT, 0x69, 0x03},
	//{GC0339_8BIT, 0x60, 0x82},
	{GC0339_8BIT, 0x65, 0x10},
	{GC0339_8BIT, 0x6c, 0x40},
	{GC0339_8BIT, 0x6d, 0x01},
	{GC0339_8BIT, 0x67, 0x10},
	{GC0339_8BIT, 0x4a, 0x40},
	{GC0339_8BIT, 0x4b, 0x40},
	{GC0339_8BIT, 0x4c, 0x40},
	{GC0339_8BIT, 0xe8, 0x04},
	{GC0339_8BIT, 0xe9, 0xbb},
	{GC0339_8BIT, 0x42, 0x20},
	{GC0339_8BIT, 0x47, 0x10},
	{GC0339_8BIT, 0x50, 0x40},
	{GC0339_8BIT, 0xd0, 0x00},
	{GC0339_8BIT, 0xd3, 0x50},
	{GC0339_8BIT, 0xf6, 0x05},
	{GC0339_8BIT, 0x01, 0x6a},
	{GC0339_8BIT, 0x02, 0x0c},
	{GC0339_8BIT, 0x0f, 0x00},
	{GC0339_8BIT, 0x6a, 0x55},//11
	{GC0339_8BIT, 0x71, 0x01},
	{GC0339_8BIT, 0x72, 0x01},
	{GC0339_8BIT, 0x73, 0x01},
	{GC0339_8BIT, 0x79, 0x01},
	{GC0339_8BIT, 0x7a, 0x01},
	{GC0339_8BIT, 0x2e, 0x10},
	{GC0339_8BIT, 0x2b, 0x00},
	{GC0339_8BIT, 0x2c, 0x03},
	{GC0339_8BIT, 0xd2, 0x00},
	{GC0339_8BIT, 0x20, 0xb0},
	//{GC0339_8BIT, 0x60, 0x92},
#endif
	{GC0339_TOK_TERM, 0, 0},
};

static struct gc0339_reg const gc0339_VGA_30fps[] = {
#if REG_VER1
	{GC0339_8BIT, 0x15, 0x0A},
	{GC0339_8BIT, 0x62, 0x20},
	{GC0339_8BIT, 0x63, 0x03},
	{GC0339_8BIT, 0x06, 0x00}, /* Row_start */
	{GC0339_8BIT, 0x08, 0x05}, /* Column start */
	{GC0339_8BIT, 0x09, 0x01}, /* Window height */
	{GC0339_8BIT, 0x0A, 0xE4},
	{GC0339_8BIT, 0x0B, 0x02}, /* Window width */
	{GC0339_8BIT, 0x0C, 0x84},
#else
	{GC0339_8BIT, 0x15, 0x0A},
	{GC0339_8BIT, 0x62, 0x34},
	{GC0339_8BIT, 0x63, 0x03},
	{GC0339_8BIT, 0x06, 0x01}, /* Row_start */
	{GC0339_8BIT, 0x08, 0x00}, /* Column start */
	{GC0339_8BIT, 0x09, 0x01}, /* Window height */
	{GC0339_8BIT, 0x0a, 0xf2},
	{GC0339_8BIT, 0x0b, 0x02}, /* Window width */
	{GC0339_8BIT, 0x0c, 0x94},
#endif
	{GC0339_TOK_TERM, 0, 0},
};

static struct gc0339_reg const gc0339_CIF_30fps[] = {
#if REG_VER1
	{GC0339_8BIT, 0x15, 0x8A}, /* CIF */
	{GC0339_8BIT, 0x62, 0xBD}, /* LWC */
	{GC0339_8BIT, 0x63, 0x01},
	{GC0339_8BIT, 0x06, 0x00}, /* Row_start */
	{GC0339_8BIT, 0x08, 0x05}, /* Column start */
	{GC0339_8BIT, 0x09, 0x01}, /* Window height */
	{GC0339_8BIT, 0x0A, 0xD0},
	{GC0339_8BIT, 0x0B, 0x02}, /* Window width */
	{GC0339_8BIT, 0x0C, 0x40},
#else
	{GC0339_8BIT, 0x15, 0x8A}, /* CIF */
	{GC0339_8BIT, 0x62, 0xD1}, /* LWC */
	{GC0339_8BIT, 0x63, 0x01}, /* (368 + 4) / 4 * 5 */
	{GC0339_8BIT, 0x06, 0x00}, /* Row_start */
	{GC0339_8BIT, 0x08, 0x05}, /* Column start */
	{GC0339_8BIT, 0x09, 0x01}, /* Window height */
	{GC0339_8BIT, 0x0A, 0xD0},
	{GC0339_8BIT, 0x0B, 0x02}, /* Window width */
	{GC0339_8BIT, 0x0C, 0x40},
#endif
	{GC0339_TOK_TERM, 0, 0},
};

struct gc0339_resolution gc0339_res_preview[] = {
#if REG_VER1
	{
		.desc = "gc0339_VGA_30fps",
		.width = 640,
		.height = 480,
		.fps = 30,
		//.pix_clk_freq = 73,
		.used = 0,
		.pixels_per_line = 0x0314,
		.lines_per_frame = 0x0213,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 2,
		.regs = gc0339_VGA_30fps,
	},
#else
	{
		.desc = "gc0339_VGA_30fps",
		.width = 656,
		.height = 496,
		.fps = 30,
		//.pix_clk_freq = 73,
		.used = 0,
		.pixels_per_line = 0x0314,
		.lines_per_frame = 0x0213,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 2,
		.regs = gc0339_VGA_30fps,
	},
#endif
};
#define N_RES_PREVIEW (ARRAY_SIZE(gc0339_res_preview))

struct gc0339_resolution gc0339_res_still[] = {
#if REG_VER1
	{
		.desc = "gc0339_VGA_30fps",
		.width = 640,
		.height = 480,
		.fps = 30,
		//.pix_clk_freq = 73,
		.used = 0,
		.pixels_per_line = 0x0314,
		.lines_per_frame = 0x0213,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 2,
		.regs = gc0339_VGA_30fps,
	},
#else
	{
		.desc = "gc0339_VGA_30fps",
		.width = 656,
		.height = 496,
		.fps = 30,
		//.pix_clk_freq = 73,
		.used = 0,
		.pixels_per_line = 0x0314,
		.lines_per_frame = 0x0213,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 2,
		.regs = gc0339_VGA_30fps,
	},
#endif
};
#define N_RES_STILL (ARRAY_SIZE(gc0339_res_still))

struct gc0339_resolution gc0339_res_video[] = {
#if REG_VER1
#if 0
	{
		.desc = "gc0339_CIF_30fps",
		.width = 352,
		.height = 288,
		.fps = 30,
		//.pix_clk_freq = 73,
		.used = 0,
		.pixels_per_line = 0x01F0,
		.lines_per_frame = 0x014F,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 1,
		.regs = gc0339_CIF_30fps,
	},
#endif
	{
		.desc = "gc0339_VGA_30fps",
		.width = 640,
		.height = 480,
		.fps = 30,
		//.pix_clk_freq = 73,
		.used = 0,
		.pixels_per_line = 0x0314,
		.lines_per_frame = 0x0213,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 2,
		.regs = gc0339_VGA_30fps,
	},
#else
	{
		.desc = "gc0339_CIF_30fps",
		.width = 368,
		.height = 304,
		.fps = 30,
		//.pix_clk_freq = 73,
		.used = 0,
		.pixels_per_line = 0x01F0,
		.lines_per_frame = 0x014F,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 1,
		.regs = gc0339_CIF_30fps,
	},
	{
		.desc = "gc0339_VGA_30fps",
		.width = 656,
		.height = 496,
		.fps = 30,
		//.pix_clk_freq = 73,
		.used = 0,
		.pixels_per_line = 0x0314,
		.lines_per_frame = 0x0213,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 2,
		.regs = gc0339_VGA_30fps,
	},
#endif
};
#define N_RES_VIDEO (ARRAY_SIZE(gc0339_res_video))

static struct gc0339_resolution *gc0339_res = gc0339_res_preview;
static int N_RES = N_RES_PREVIEW;
#endif

