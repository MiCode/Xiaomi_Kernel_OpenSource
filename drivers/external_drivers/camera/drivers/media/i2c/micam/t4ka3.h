/*
 * Support for T4KA3 camera sensor.
 *
 * Copyright (c) 2014 Intel Corporation. All Rights Reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __T4KA3_H__
#define __T4KA3_H__
#include <linux/atomisp_platform.h>
#include <linux/atomisp.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include <media/media-entity.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>

#define T4KA3_NAME	"t4ka3"

/* Defines for register writes and register array processing */
#define T4KA3_BYTE_MAX	30
#define T4KA3_SHORT_MAX	16
#define I2C_MSG_LENGTH		0x2
#define I2C_RETRY_COUNT		5

#define T4KA3_TEST_PATTERN_MODE	0x0601

#define T4KA3_HFLIP_BIT	0x1
#define T4KA3_VFLIP_BIT	0x2
#define T4KA3_VFLIP_OFFSET	1
#define T4KA3_IMG_ORIENTATION 0x0101

#define I2C_RETRY_COUNT		5
#define MAX_FMTS		1

#define T4KA3_PID_HIGH	0x0000
#define T4KA3_PID_LOW	0x0001
#define T4KA3_MOD_ID		0x1490

#define T4KA3_RES_WIDTH_MAX	3280
#define T4KA3_RES_HEIGHT_MAX	2464

#define T4KA3_COARSE_INTEGRATION_TIME	0x0202
#define T4KA3_GLOBAL_GAIN			0x234

#define T4KA3_FINE_INTG_TIME_MIN 0
#define T4KA3_FINE_INTG_TIME_MAX_MARGIN 0
#define T4KA3_COARSE_INTEGRATION_TIME_MARGIN	6
#define T4KA3_COARSE_INTEGRATION_TIME_MIN	1

#define T4KA3_MAX_EXPOSURE_SUPPORTED		\
	(0xffff - T4KA3_COARSE_INTEGRATION_TIME_MARGIN)
#define T4KA3_MAX_GLOBAL_GAIN_SUPPORTED	0x07FF
#define T4KA3_MIN_GLOBAL_GAIN_SUPPORTED	0x0080

#define T4KA3_DIGGAIN_GREEN_R_H		0x020E
#define T4KA3_DIGGAIN_RED_H		0x0210
#define T4KA3_DIGGAIN_BLUE_H		0x0212
#define T4KA3_DIGGAIN_GREEN_B_H		0x0214


#define T4KA3_INTG_BUF_COUNT		1

#define T4KA3_VT_PIX_CLK_DIV		0x0300
#define T4KA3_VT_SYS_CLK_DIV		0x0302
#define T4KA3_PRE_PLL_CLK_DIV	0x0304
#define T4KA3_PLL_MULTIPLIER		0x0306
#define T4KA3_FRAME_LENGTH_LINES	0x0340
#define T4KA3_LINE_LENGTH_PCK	0x0342


#define T4KA3_MCLK	192

#define T4KA3_HORIZONTAL_START_H	0x0344
#define T4KA3_VERTICAL_START_H	0x0346
#define T4KA3_HORIZONTAL_END_H	0x0348
#define T4KA3_VERTICAL_END_H		0x034a
#define T4KA3_HORIZONTAL_OUTPUT_SIZE_H	0x034c
#define T4KA3_VERTICAL_OUTPUT_SIZE_H		0x034e

/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define T4KA3_FOCAL_LENGTH_DEFAULT	0x1280064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define T4KA3_F_NUMBER_DEFAULT	0x14000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define T4KA3_F_NUMBER_RANGE	0x140a140a

#define T4KA3_BIN_FACTOR_MAX	2

/* Defines for lens/VCM */
#define T4KA3_FOCAL_LENGTH_NUM	296	/* 2.96 mm */
#define T4KA3_FOCAL_LENGTH_DEM	100
#define T4KA3_F_NUMBER_DEFAULT_NUM	20	/*  F/2.0 */
#define T4KA3_F_NUMBER_DEM	10

#define T4KA3_INVALID_CONFIG	0xffffffff

#define	v4l2_format_capture_type_entry(_width, _height, \
		_pixelformat, _bytesperline, _colorspace) \
	{\
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,\
		.fmt.pix.width = (_width),\
		.fmt.pix.height = (_height),\
		.fmt.pix.pixelformat = (_pixelformat),\
		.fmt.pix.bytesperline = (_bytesperline),\
		.fmt.pix.colorspace = (_colorspace),\
		.fmt.pix.sizeimage = (_height)*(_bytesperline),\
	}

#define	s_output_format_entry(_width, _height, _pixelformat, \
		_bytesperline, _colorspace, _fps) \
	{\
		.v4l2_fmt = v4l2_format_capture_type_entry(_width, \
			_height, _pixelformat, _bytesperline, \
				_colorspace),\
		.fps = (_fps),\
	}

#define	s_output_format_reg_entry(_width, _height, _pixelformat, \
		_bytesperline, _colorspace, _fps, _reg_setting) \
	{\
		.s_fmt = s_output_format_entry(_width, _height,\
				_pixelformat, _bytesperline, \
				_colorspace, _fps),\
		.reg_setting = (_reg_setting),\
	}

struct s_ctrl_id {
	struct v4l2_queryctrl qc;
	int (*s_ctrl)(struct v4l2_subdev *sd, u32 val);
	int (*g_ctrl)(struct v4l2_subdev *sd, u32 *val);
};

#define	v4l2_queryctrl_entry_integer(_id, _name,\
		_minimum, _maximum, _step, \
		_default_value, _flags)	\
	{\
		.id = (_id), \
		.type = V4L2_CTRL_TYPE_INTEGER, \
		.name = _name, \
		.minimum = (_minimum), \
		.maximum = (_maximum), \
		.step = (_step), \
		.default_value = (_default_value),\
		.flags = (_flags),\
	}
#define	v4l2_queryctrl_entry_boolean(_id, _name,\
		_default_value, _flags)	\
	{\
		.id = (_id), \
		.type = V4L2_CTRL_TYPE_BOOLEAN, \
		.name = _name, \
		.minimum = 0, \
		.maximum = 1, \
		.step = 1, \
		.default_value = (_default_value),\
		.flags = (_flags),\
	}

#define	s_ctrl_id_entry_integer(_id, _name, \
		_minimum, _maximum, _step, \
		_default_value, _flags, \
		_s_ctrl, _g_ctrl)	\
	{\
		.qc = v4l2_queryctrl_entry_integer(_id, _name,\
				_minimum, _maximum, _step,\
				_default_value, _flags), \
		.s_ctrl = _s_ctrl, \
		.g_ctrl = _g_ctrl, \
	}

#define	s_ctrl_id_entry_boolean(_id, _name, \
		_default_value, _flags, \
		_s_ctrl, _g_ctrl)	\
	{\
		.qc = v4l2_queryctrl_entry_boolean(_id, _name,\
				_default_value, _flags), \
		.s_ctrl = _s_ctrl, \
		.g_ctrl = _g_ctrl, \
	}

struct t4ka3_vcm {
	int (*power_up)(struct v4l2_subdev *sd);
	int (*power_down)(struct v4l2_subdev *sd);
	int (*init)(struct v4l2_subdev *sd);
	int (*t_focus_vcm)(struct v4l2_subdev *sd, u16 val);
	int (*t_focus_abs)(struct v4l2_subdev *sd, s32 value);
	int (*t_focus_rel)(struct v4l2_subdev *sd, s32 value);
	int (*q_focus_status)(struct v4l2_subdev *sd, s32 *value);
	int (*q_focus_abs)(struct v4l2_subdev *sd, s32 *value);
};

struct t4ka3_otp {
	void * (*otp_read)(struct v4l2_subdev *sd, u8 **rawotp, u8 *vendorid);
};


enum t4ka3_tok_type {
	T4KA3_8BIT  = 0x0001,
	T4KA3_16BIT = 0x0002,
	T4KA3_RMW   = 0x0010,
	T4KA3_TOK_TERM   = 0xf000,	/* terminating token for reg list */
	T4KA3_TOK_DELAY  = 0xfe00, /* delay token for reg list */
	T4KA3_TOK_MASK = 0xfff0
};

/*
 * If register address or register width is not 32 bit width,
 * user needs to convert it manually
 */

struct s_register_setting {
	u32 reg;
	u32 val;
};

struct s_output_format {
	struct v4l2_format v4l2_fmt;
	int fps;
};

struct sysfs_debug {
	u16 addr;
	u16 page;
	u16 val;
};

struct t4ka3_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;
	struct camera_sensor_platform_data *platform_data;
	struct mutex input_lock; /* serialize sensor's ioctl */
	struct t4ka3_vcm *vcm_driver;
	struct t4ka3_otp *otp_driver;
	struct sysfs_debug sysdbg;
	int fmt_idx;
	int status;
	int streaming;
	int power;
	int run_mode;
	int vt_pix_clk_freq_mhz;
	u16 sensor_id;
	u16 coarse_itg;
	u16 fine_itg;
	u16 gain;
	u16 digital_gain;
	u16 pixels_per_line;
	u16 lines_per_frame;
	u16 flip;
	u8 fps;
	u8 res;
	u8 type;
	u8 *otp_data;
	u8 *otp_raw_data;
	u8 module_vendor_id;

	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *link_freq;
};

/**
 * struct t4ka3_reg - MI sensor  register format
 * @type: type of the register
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 *
 * Define a structure for sensor register initialization values
 */
struct t4ka3_reg {
	enum t4ka3_tok_type type;
	u16 sreg;
	u32 val;	/* @set value for read/mod/write, @mask */
};

#define to_t4ka3_sensor(x) container_of(x, struct t4ka3_device, sd)

#define T4KA3_MAX_WRITE_BUF_SIZE	30
struct t4ka3_write_buffer {
	u16 addr;
	u8 data[T4KA3_MAX_WRITE_BUF_SIZE];
};

struct t4ka3_write_ctrl {
	int index;
	struct t4ka3_write_buffer buffer;
};

struct t4ka3_format_struct {
	u8 *desc;
	struct regval_list *regs;
	u32 pixelformat;
};

struct t4ka3_resolution {
	u8 *desc;
	const struct t4ka3_reg *regs;
	int res;
	int width;
	int height;
	int fps;
	unsigned short pixels_per_line;
	unsigned short lines_per_frame;
	u8 bin_factor_x;
	u8 bin_factor_y;
	bool used;
	u32 skip_frames;
	u32 code;
	int mipi_freq;
};

struct t4ka3_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, int value);
};

/* init settings */
static struct t4ka3_reg const t4ka3_init_config[] = {
	{T4KA3_8BIT, 0x4136, 0x13},
	{T4KA3_8BIT, 0x4137, 0x33},
	{T4KA3_8BIT, 0x3094, 0x01},
	{T4KA3_8BIT, 0x0233, 0x01},
	{T4KA3_8BIT, 0x4B06, 0x01},
	{T4KA3_8BIT, 0x4B07, 0x01},
	{T4KA3_8BIT, 0x3028, 0x01},
	{T4KA3_8BIT, 0x3032, 0x14},
	{T4KA3_8BIT, 0x305C, 0x0C},
	{T4KA3_8BIT, 0x306D, 0x0A},
	{T4KA3_8BIT, 0x3071, 0xFA},
	{T4KA3_8BIT, 0x307E, 0x0A},
	{T4KA3_8BIT, 0x307F, 0xFC},
	{T4KA3_8BIT, 0x3091, 0x04},
	{T4KA3_8BIT, 0x3092, 0x60},
	{T4KA3_8BIT, 0x3096, 0xC0},
	{T4KA3_8BIT, 0x3100, 0x07},
	{T4KA3_8BIT, 0x3101, 0x4C},
	{T4KA3_8BIT, 0x3118, 0xCC},
	{T4KA3_8BIT, 0x3139, 0x06},
	{T4KA3_8BIT, 0x313A, 0x06},
	{T4KA3_8BIT, 0x313B, 0x04},
	{T4KA3_8BIT, 0x3143, 0x02},
	{T4KA3_8BIT, 0x314F, 0x0E},
	{T4KA3_8BIT, 0x3169, 0x99},
	{T4KA3_8BIT, 0x316A, 0x99},
	{T4KA3_8BIT, 0x3171, 0x05},
	{T4KA3_8BIT, 0x31A1, 0xA7},
	{T4KA3_8BIT, 0x31A2, 0x9C},
	{T4KA3_8BIT, 0x31A3, 0x8F},
	{T4KA3_8BIT, 0x31A4, 0x75},
	{T4KA3_8BIT, 0x31A5, 0xEE},
	{T4KA3_8BIT, 0x31A6, 0xEA},
	{T4KA3_8BIT, 0x31A7, 0xE4},
	{T4KA3_8BIT, 0x31A8, 0xE4},
	{T4KA3_8BIT, 0x31DF, 0x05},
	{T4KA3_8BIT, 0x31EC, 0x1B},
	{T4KA3_8BIT, 0x31ED, 0x1B},
	{T4KA3_8BIT, 0x31EE, 0x1B},
	{T4KA3_8BIT, 0x31F0, 0x1B},
	{T4KA3_8BIT, 0x31F1, 0x1B},
	{T4KA3_8BIT, 0x31F2, 0x1B},
	{T4KA3_8BIT, 0x3204, 0x3F},
	{T4KA3_8BIT, 0x3205, 0x03},
	{T4KA3_8BIT, 0x3210, 0x01},
	{T4KA3_8BIT, 0x3216, 0x68},
	{T4KA3_8BIT, 0x3217, 0x58},
	{T4KA3_8BIT, 0x3218, 0x58},
	{T4KA3_8BIT, 0x321A, 0x68},
	{T4KA3_8BIT, 0x321B, 0x60},
	{T4KA3_8BIT, 0x3238, 0x03},
	{T4KA3_8BIT, 0x3239, 0x03},
	{T4KA3_8BIT, 0x323A, 0x05},
	{T4KA3_8BIT, 0x323B, 0x06},
	{T4KA3_8BIT, 0x3243, 0x03},
	{T4KA3_8BIT, 0x3244, 0x08},
	{T4KA3_8BIT, 0x3245, 0x01},
	{T4KA3_8BIT, 0x3307, 0x19},
	{T4KA3_8BIT, 0x3308, 0x19},
	{T4KA3_8BIT, 0x3320, 0x01},
	{T4KA3_8BIT, 0x3326, 0x15},
	{T4KA3_8BIT, 0x3327, 0x0D},
	{T4KA3_8BIT, 0x3328, 0x01},
	{T4KA3_8BIT, 0x3380, 0x01},
	{T4KA3_8BIT, 0x339E, 0x07},
	{T4KA3_8BIT, 0x3424, 0x00},
	{T4KA3_8BIT, 0x343C, 0x01},
	{T4KA3_8BIT, 0x3398, 0x04},
	{T4KA3_8BIT, 0x343A, 0x10},
	{T4KA3_8BIT, 0x339A, 0x22},
	{T4KA3_8BIT, 0x33B4, 0x00},
	{T4KA3_8BIT, 0x3393, 0x01},
	{T4KA3_8BIT, 0x33B3, 0x6E},
	{T4KA3_8BIT, 0x3433, 0x06},
	{T4KA3_8BIT, 0x3433, 0x00},
	{T4KA3_8BIT, 0x33B3, 0x00},
	{T4KA3_8BIT, 0x3393, 0x03},
	{T4KA3_8BIT, 0x33B4, 0x03},
	{T4KA3_8BIT, 0x343A, 0x00},
	{T4KA3_8BIT, 0x339A, 0x00},
	{T4KA3_8BIT, 0x3398, 0x00},
	{T4KA3_TOK_TERM, 0, 0, }
};

/* Stream mode */
static struct t4ka3_reg const t4ka3_suspend[] = {
	{T4KA3_8BIT, 0x0100, 0x0 },
	{T4KA3_TOK_TERM, 0, 0 },
};

static struct t4ka3_reg const t4ka3_streaming[] = {
	{T4KA3_8BIT, 0x0100, 0x01},
	{T4KA3_TOK_TERM, 0, 0 },
};

/* GROUPED_PARAMETER_HOLD */
static struct t4ka3_reg const t4ka3_param_hold[] = {
	{ T4KA3_8BIT,  0x0104, 0x1 },
	{ T4KA3_TOK_TERM, 0, 0 }
};
static struct t4ka3_reg const t4ka3_param_update[] = {
	{ T4KA3_8BIT,  0x0104, 0x0 },
	{ T4KA3_TOK_TERM, 0, 0 }
};

/* Settings */

static struct t4ka3_reg const t4ka3_736x496_30fps[] = {
	{T4KA3_8BIT, 0x0112, 0x0A},
	{T4KA3_8BIT, 0x0113, 0x0A},
	{T4KA3_8BIT, 0x0114, 0x03},
	{T4KA3_8BIT, 0x4136, 0x13},
	{T4KA3_8BIT, 0x4137, 0x33},
	{T4KA3_8BIT, 0x0820, 0x0A},
	{T4KA3_8BIT, 0x0821, 0x0D},
	{T4KA3_8BIT, 0x0822, 0x00},
	{T4KA3_8BIT, 0x0823, 0x00},
	{T4KA3_8BIT, 0x0301, 0x0A},
	{T4KA3_8BIT, 0x0303, 0x01},
	{T4KA3_8BIT, 0x0305, 0x04},
	{T4KA3_8BIT, 0x0306, 0x02},
	{T4KA3_8BIT, 0x0307, 0x18},
	{T4KA3_8BIT, 0x030B, 0x01},
	{T4KA3_8BIT, 0x034C, 0x02},
	{T4KA3_8BIT, 0x034D, 0xE0},
	{T4KA3_8BIT, 0x034E, 0x01},
	{T4KA3_8BIT, 0x034F, 0xEE},
	{T4KA3_8BIT, 0x0340, 0x09},
	{T4KA3_8BIT, 0x0341, 0xBC},
	{T4KA3_8BIT, 0x0342, 0x0D},
	{T4KA3_8BIT, 0x0343, 0x70},
	{T4KA3_8BIT, 0x0344, 0x00},
	{T4KA3_8BIT, 0x0345, 0x00},
	{T4KA3_8BIT, 0x0346, 0x00},
	{T4KA3_8BIT, 0x0347, 0x00},
	{T4KA3_8BIT, 0x0348, 0x0C},
	{T4KA3_8BIT, 0x0349, 0xCF},
	{T4KA3_8BIT, 0x034A, 0x09},
	{T4KA3_8BIT, 0x034B, 0x9F},
	{T4KA3_8BIT, 0x0408, 0x01},
	{T4KA3_8BIT, 0x0409, 0x74},
	{T4KA3_8BIT, 0x040A, 0x00},
	{T4KA3_8BIT, 0x040B, 0xFA},
	{T4KA3_8BIT, 0x040C, 0x02},
	{T4KA3_8BIT, 0x040D, 0xE0},
	{T4KA3_8BIT, 0x040E, 0x01},
	{T4KA3_8BIT, 0x040F, 0xF0},
	{T4KA3_8BIT, 0x0900, 0x01},
	{T4KA3_8BIT, 0x0901, 0x22},
	{T4KA3_8BIT, 0x0902, 0x00},
	{T4KA3_8BIT, 0x4220, 0x00},
	{T4KA3_8BIT, 0x4222, 0x01},
	{T4KA3_8BIT, 0x3380, 0x01},
	{T4KA3_8BIT, 0x3090, 0x88},
	{T4KA3_8BIT, 0x3394, 0x20},
	{T4KA3_8BIT, 0x3090, 0x08},
	{T4KA3_8BIT, 0x3394, 0x10},
	{T4KA3_TOK_TERM, 0, 0 }
};

static struct t4ka3_reg const t4ka3_896x736_30fps[] = {
	{T4KA3_8BIT, 0x0112, 0x0A},
	{T4KA3_8BIT, 0x0113, 0x0A},
	{T4KA3_8BIT, 0x0114, 0x03},
	{T4KA3_8BIT, 0x4136, 0x13},
	{T4KA3_8BIT, 0x4137, 0x33},
	{T4KA3_8BIT, 0x0820, 0x0A},
	{T4KA3_8BIT, 0x0821, 0x0D},
	{T4KA3_8BIT, 0x0822, 0x00},
	{T4KA3_8BIT, 0x0823, 0x00},
	{T4KA3_8BIT, 0x0301, 0x0A},
	{T4KA3_8BIT, 0x0303, 0x01},
	{T4KA3_8BIT, 0x0305, 0x04},
	{T4KA3_8BIT, 0x0306, 0x02},
	{T4KA3_8BIT, 0x0307, 0x18},
	{T4KA3_8BIT, 0x030B, 0x01},
	{T4KA3_8BIT, 0x034C, 0x03},
	{T4KA3_8BIT, 0x034D, 0x80},
	{T4KA3_8BIT, 0x034E, 0x02},
	{T4KA3_8BIT, 0x034F, 0xDE},
	{T4KA3_8BIT, 0x0340, 0x09},
	{T4KA3_8BIT, 0x0341, 0xBC},
	{T4KA3_8BIT, 0x0342, 0x0D},
	{T4KA3_8BIT, 0x0343, 0x70},
	{T4KA3_8BIT, 0x0344, 0x00},
	{T4KA3_8BIT, 0x0345, 0x00},
	{T4KA3_8BIT, 0x0346, 0x00},
	{T4KA3_8BIT, 0x0347, 0x00},
	{T4KA3_8BIT, 0x0348, 0x0C},
	{T4KA3_8BIT, 0x0349, 0xCF},
	{T4KA3_8BIT, 0x034A, 0x09},
	{T4KA3_8BIT, 0x034B, 0x9F},
	{T4KA3_8BIT, 0x0408, 0x01},
	{T4KA3_8BIT, 0x0409, 0x74},
	{T4KA3_8BIT, 0x040A, 0x00},
	{T4KA3_8BIT, 0x040B, 0xFA},
	{T4KA3_8BIT, 0x040C, 0x03},
	{T4KA3_8BIT, 0x040D, 0x80},
	{T4KA3_8BIT, 0x040E, 0x02},
	{T4KA3_8BIT, 0x040F, 0xE0},
	{T4KA3_8BIT, 0x0900, 0x01},
	{T4KA3_8BIT, 0x0901, 0x22},
	{T4KA3_8BIT, 0x0902, 0x00},
	{T4KA3_8BIT, 0x4220, 0x00},
	{T4KA3_8BIT, 0x4222, 0x01},
	{T4KA3_8BIT, 0x3380, 0x01},
	{T4KA3_8BIT, 0x3090, 0x88},
	{T4KA3_8BIT, 0x3394, 0x20},
	{T4KA3_8BIT, 0x3090, 0x08},
	{T4KA3_8BIT, 0x3394, 0x10},
	{T4KA3_TOK_TERM, 0, 0 }
};

static struct t4ka3_reg const t4ka3_1296x736_30fps[] = {
	{T4KA3_8BIT, 0x0112, 0x0A},
	{T4KA3_8BIT, 0x0113, 0x0A},
	{T4KA3_8BIT, 0x0114, 0x03},
	{T4KA3_8BIT, 0x4136, 0x13},
	{T4KA3_8BIT, 0x4137, 0x33},
	{T4KA3_8BIT, 0x0820, 0x0A},
	{T4KA3_8BIT, 0x0821, 0x0D},
	{T4KA3_8BIT, 0x0822, 0x00},
	{T4KA3_8BIT, 0x0823, 0x00},
	{T4KA3_8BIT, 0x0301, 0x0A},
	{T4KA3_8BIT, 0x0303, 0x01},
	{T4KA3_8BIT, 0x0305, 0x04},
	{T4KA3_8BIT, 0x0306, 0x02},
	{T4KA3_8BIT, 0x0307, 0x18},
	{T4KA3_8BIT, 0x030B, 0x01},
	{T4KA3_8BIT, 0x034C, 0x05},
	{T4KA3_8BIT, 0x034D, 0x10},
	{T4KA3_8BIT, 0x034E, 0x02},
	{T4KA3_8BIT, 0x034F, 0xDE},
	{T4KA3_8BIT, 0x0340, 0x09},
	{T4KA3_8BIT, 0x0341, 0xBC},
	{T4KA3_8BIT, 0x0342, 0x0D},
	{T4KA3_8BIT, 0x0343, 0x70},
	{T4KA3_8BIT, 0x0344, 0x00},
	{T4KA3_8BIT, 0x0345, 0x00},
	{T4KA3_8BIT, 0x0346, 0x00},
	{T4KA3_8BIT, 0x0347, 0x00},
	{T4KA3_8BIT, 0x0348, 0x0C},
	{T4KA3_8BIT, 0x0349, 0xCF},
	{T4KA3_8BIT, 0x034A, 0x09},
	{T4KA3_8BIT, 0x034B, 0x9F},
	{T4KA3_8BIT, 0x0408, 0x00},
	{T4KA3_8BIT, 0x0409, 0xAC},
	{T4KA3_8BIT, 0x040A, 0x00},
	{T4KA3_8BIT, 0x040B, 0xFA},
	{T4KA3_8BIT, 0x040C, 0x05},
	{T4KA3_8BIT, 0x040D, 0x10},
	{T4KA3_8BIT, 0x040E, 0x02},
	{T4KA3_8BIT, 0x040F, 0xE0},
	{T4KA3_8BIT, 0x0900, 0x01},
	{T4KA3_8BIT, 0x0901, 0x22},
	{T4KA3_8BIT, 0x0902, 0x00},
	{T4KA3_8BIT, 0x4220, 0x00},
	{T4KA3_8BIT, 0x4222, 0x01},
	{T4KA3_8BIT, 0x3380, 0x01},
	{T4KA3_8BIT, 0x3090, 0x88},
	{T4KA3_8BIT, 0x3394, 0x20},
	{T4KA3_8BIT, 0x3090, 0x08},
	{T4KA3_8BIT, 0x3394, 0x10},
	{T4KA3_TOK_TERM, 0, 0 }
};

static struct t4ka3_reg const t4ka3_1632x1224_30fps[] = {
	{T4KA3_8BIT, 0x0112, 0x0A},
	{T4KA3_8BIT, 0x0113, 0x0A},
	{T4KA3_8BIT, 0x0114, 0x03},
	{T4KA3_8BIT, 0x4136, 0x13},
	{T4KA3_8BIT, 0x4137, 0x33},
	{T4KA3_8BIT, 0x0820, 0x0A},
	{T4KA3_8BIT, 0x0821, 0x0D},
	{T4KA3_8BIT, 0x0822, 0x00},
	{T4KA3_8BIT, 0x0823, 0x00},
	{T4KA3_8BIT, 0x0301, 0x0A},
	{T4KA3_8BIT, 0x0303, 0x01},
	{T4KA3_8BIT, 0x0305, 0x04},
	{T4KA3_8BIT, 0x0306, 0x02},
	{T4KA3_8BIT, 0x0307, 0x18},
	{T4KA3_8BIT, 0x030B, 0x01},
	{T4KA3_8BIT, 0x034C, 0x06},
	{T4KA3_8BIT, 0x034D, 0x60},
	{T4KA3_8BIT, 0x034E, 0x04},
	{T4KA3_8BIT, 0x034F, 0xC6},
	{T4KA3_8BIT, 0x0340, 0x09},
	{T4KA3_8BIT, 0x0341, 0xBC},
	{T4KA3_8BIT, 0x0342, 0x0D},
	{T4KA3_8BIT, 0x0343, 0x70},
	{T4KA3_8BIT, 0x0344, 0x00},
	{T4KA3_8BIT, 0x0345, 0x00},
	{T4KA3_8BIT, 0x0346, 0x00},
	{T4KA3_8BIT, 0x0347, 0x00},
	{T4KA3_8BIT, 0x0348, 0x0C},
	{T4KA3_8BIT, 0x0349, 0xCF},
	{T4KA3_8BIT, 0x034A, 0x09},
	{T4KA3_8BIT, 0x034B, 0x9F},
	{T4KA3_8BIT, 0x0408, 0x00},
	{T4KA3_8BIT, 0x0409, 0x04},
	{T4KA3_8BIT, 0x040A, 0x00},
	{T4KA3_8BIT, 0x040B, 0x06},
	{T4KA3_8BIT, 0x040C, 0x06},
	{T4KA3_8BIT, 0x040D, 0x60},
	{T4KA3_8BIT, 0x040E, 0x04},
	{T4KA3_8BIT, 0x040F, 0xC8},
	{T4KA3_8BIT, 0x0900, 0x01},
	{T4KA3_8BIT, 0x0901, 0x22},
	{T4KA3_8BIT, 0x0902, 0x00},
	{T4KA3_8BIT, 0x4220, 0x00},
	{T4KA3_8BIT, 0x4222, 0x01},
	{T4KA3_8BIT, 0x3380, 0x01},
	{T4KA3_8BIT, 0x3090, 0x88},
	{T4KA3_8BIT, 0x3394, 0x20},
	{T4KA3_8BIT, 0x3090, 0x08},
	{T4KA3_8BIT, 0x3394, 0x10},
	{T4KA3_TOK_TERM, 0, 0 }
};

static struct t4ka3_reg const t4ka3_1936x1096_30fps[] = {
	{T4KA3_8BIT, 0x0112, 0x0A},
	{T4KA3_8BIT, 0x0113, 0x0A},
	{T4KA3_8BIT, 0x0114, 0x03},
	{T4KA3_8BIT, 0x4136, 0x13},
	{T4KA3_8BIT, 0x4137, 0x33},
	{T4KA3_8BIT, 0x0820, 0x0A},
	{T4KA3_8BIT, 0x0821, 0x0D},
	{T4KA3_8BIT, 0x0822, 0x00},
	{T4KA3_8BIT, 0x0823, 0x00},
	{T4KA3_8BIT, 0x0301, 0x0A},
	{T4KA3_8BIT, 0x0303, 0x01},
	{T4KA3_8BIT, 0x0305, 0x04},
	{T4KA3_8BIT, 0x0306, 0x02},
	{T4KA3_8BIT, 0x0307, 0x18},
	{T4KA3_8BIT, 0x030B, 0x01},
	{T4KA3_8BIT, 0x034C, 0x07},
	{T4KA3_8BIT, 0x034D, 0x90},
	{T4KA3_8BIT, 0x034E, 0x04},
	{T4KA3_8BIT, 0x034F, 0x46},
	{T4KA3_8BIT, 0x0340, 0x09},
	{T4KA3_8BIT, 0x0341, 0xBC},
	{T4KA3_8BIT, 0x0342, 0x0D},
	{T4KA3_8BIT, 0x0343, 0x70},
	{T4KA3_8BIT, 0x0344, 0x00},
	{T4KA3_8BIT, 0x0345, 0x00},
	{T4KA3_8BIT, 0x0346, 0x00},
	{T4KA3_8BIT, 0x0347, 0x00},
	{T4KA3_8BIT, 0x0348, 0x0c},
	{T4KA3_8BIT, 0x0349, 0xCF},
	{T4KA3_8BIT, 0x034A, 0x09},
	{T4KA3_8BIT, 0x034B, 0x9F},
	{T4KA3_8BIT, 0x0408, 0x02},
	{T4KA3_8BIT, 0x0409, 0xA0},
	{T4KA3_8BIT, 0x040A, 0x02},
	{T4KA3_8BIT, 0x040B, 0xAE},
	{T4KA3_8BIT, 0x040C, 0x07},
	{T4KA3_8BIT, 0x040D, 0x90},
	{T4KA3_8BIT, 0x040E, 0x04},
	{T4KA3_8BIT, 0x040F, 0x4B},
	{T4KA3_8BIT, 0x0900, 0x01},
	{T4KA3_8BIT, 0x0901, 0x11},
	{T4KA3_8BIT, 0x0902, 0x00},
	{T4KA3_8BIT, 0x4220, 0x00},
	{T4KA3_8BIT, 0x4222, 0x01},
	{T4KA3_8BIT, 0x3380, 0x01},
	{T4KA3_8BIT, 0x3090, 0x88},
	{T4KA3_8BIT, 0x3394, 0x20},
	{T4KA3_8BIT, 0x3090, 0x08},
	{T4KA3_8BIT, 0x3394, 0x10},
	{T4KA3_TOK_TERM, 0, 0 }
};

static struct t4ka3_reg const t4ka3_2064x1552_30fps[] = {
	{T4KA3_8BIT, 0x0112, 0x0A},
	{T4KA3_8BIT, 0x0113, 0x0A},
	{T4KA3_8BIT, 0x0114, 0x03},
	{T4KA3_8BIT, 0x4136, 0x13},
	{T4KA3_8BIT, 0x4137, 0x33},
	{T4KA3_8BIT, 0x0820, 0x0A},
	{T4KA3_8BIT, 0x0821, 0x0D},
	{T4KA3_8BIT, 0x0822, 0x00},
	{T4KA3_8BIT, 0x0823, 0x00},
	{T4KA3_8BIT, 0x0301, 0x0A},
	{T4KA3_8BIT, 0x0303, 0x01},
	{T4KA3_8BIT, 0x0305, 0x04},
	{T4KA3_8BIT, 0x0306, 0x02},
	{T4KA3_8BIT, 0x0307, 0x18},
	{T4KA3_8BIT, 0x030B, 0x01},
	{T4KA3_8BIT, 0x034C, 0x08},
	{T4KA3_8BIT, 0x034D, 0x10},
	{T4KA3_8BIT, 0x034E, 0x06},
	{T4KA3_8BIT, 0x034F, 0x0E},
	{T4KA3_8BIT, 0x0340, 0x09},
	{T4KA3_8BIT, 0x0341, 0xBC},
	{T4KA3_8BIT, 0x0342, 0x0D},
	{T4KA3_8BIT, 0x0343, 0x70},
	{T4KA3_8BIT, 0x0344, 0x00},
	{T4KA3_8BIT, 0x0345, 0x00},
	{T4KA3_8BIT, 0x0346, 0x00},
	{T4KA3_8BIT, 0x0347, 0x00},
	{T4KA3_8BIT, 0x0348, 0x0C},
	{T4KA3_8BIT, 0x0349, 0xCF},
	{T4KA3_8BIT, 0x034A, 0x09},
	{T4KA3_8BIT, 0x034B, 0x9F},
	{T4KA3_8BIT, 0x0408, 0x02},
	{T4KA3_8BIT, 0x0409, 0x60},
	{T4KA3_8BIT, 0x040A, 0x01},
	{T4KA3_8BIT, 0x040B, 0xCA},
	{T4KA3_8BIT, 0x040C, 0x08},
	{T4KA3_8BIT, 0x040D, 0x10},
	{T4KA3_8BIT, 0x040E, 0x06},
	{T4KA3_8BIT, 0x040F, 0x10},
	{T4KA3_8BIT, 0x0900, 0x01},
	{T4KA3_8BIT, 0x0901, 0x11},
	{T4KA3_8BIT, 0x0902, 0x00},
	{T4KA3_8BIT, 0x4220, 0x00},
	{T4KA3_8BIT, 0x4222, 0x01},
	{T4KA3_8BIT, 0x3380, 0x01},
	{T4KA3_8BIT, 0x3090, 0x88},
	{T4KA3_8BIT, 0x3394, 0x20},
	{T4KA3_8BIT, 0x3090, 0x08},
	{T4KA3_8BIT, 0x3394, 0x10},
	{T4KA3_TOK_TERM, 0, 0 }
};

static struct t4ka3_reg const t4ka3_2576x1936_30fps[] = {
	{T4KA3_8BIT, 0x0112, 0x0A},
	{T4KA3_8BIT, 0x0113, 0x0A},
	{T4KA3_8BIT, 0x0114, 0x03},
	{T4KA3_8BIT, 0x4136, 0x13},
	{T4KA3_8BIT, 0x4137, 0x33},
	{T4KA3_8BIT, 0x0820, 0x0A},
	{T4KA3_8BIT, 0x0821, 0x0D},
	{T4KA3_8BIT, 0x0822, 0x00},
	{T4KA3_8BIT, 0x0823, 0x00},
	{T4KA3_8BIT, 0x0301, 0x0A},
	{T4KA3_8BIT, 0x0303, 0x01},
	{T4KA3_8BIT, 0x0305, 0x04},
	{T4KA3_8BIT, 0x0306, 0x02},
	{T4KA3_8BIT, 0x0307, 0x18},
	{T4KA3_8BIT, 0x030B, 0x01},
	{T4KA3_8BIT, 0x034C, 0x0A},
	{T4KA3_8BIT, 0x034D, 0x10},
	{T4KA3_8BIT, 0x034E, 0x07},
	{T4KA3_8BIT, 0x034F, 0x8E},
	{T4KA3_8BIT, 0x0340, 0x09},
	{T4KA3_8BIT, 0x0341, 0xBC},
	{T4KA3_8BIT, 0x0342, 0x0D},
	{T4KA3_8BIT, 0x0343, 0x70},
	{T4KA3_8BIT, 0x0344, 0x00},
	{T4KA3_8BIT, 0x0345, 0x00},
	{T4KA3_8BIT, 0x0346, 0x00},
	{T4KA3_8BIT, 0x0347, 0x00},
	{T4KA3_8BIT, 0x0348, 0x0C},
	{T4KA3_8BIT, 0x0349, 0xCF},
	{T4KA3_8BIT, 0x034A, 0x09},
	{T4KA3_8BIT, 0x034B, 0x9F},
	{T4KA3_8BIT, 0x0408, 0x01},
	{T4KA3_8BIT, 0x0409, 0x60},
	{T4KA3_8BIT, 0x040A, 0x01},
	{T4KA3_8BIT, 0x040B, 0x0A},
	{T4KA3_8BIT, 0x040C, 0x0A},
	{T4KA3_8BIT, 0x040D, 0x10},
	{T4KA3_8BIT, 0x040E, 0x07},
	{T4KA3_8BIT, 0x040F, 0x90},
	{T4KA3_8BIT, 0x0900, 0x01},
	{T4KA3_8BIT, 0x0901, 0x11},
	{T4KA3_8BIT, 0x0902, 0x00},
	{T4KA3_8BIT, 0x4220, 0x00},
	{T4KA3_8BIT, 0x4222, 0x01},
	{T4KA3_8BIT, 0x3380, 0x01},
	{T4KA3_8BIT, 0x3090, 0x88},
	{T4KA3_8BIT, 0x3394, 0x20},
	{T4KA3_8BIT, 0x3090, 0x08},
	{T4KA3_8BIT, 0x3394, 0x10},
	{T4KA3_TOK_TERM, 0, 0 }
};

static struct t4ka3_reg const t4ka3_3280x1852_30fps[] = {
	{T4KA3_8BIT, 0x0112, 0x0A},
	{T4KA3_8BIT, 0x0113, 0x0A},
	{T4KA3_8BIT, 0x0114, 0x03},
	{T4KA3_8BIT, 0x4136, 0x13},
	{T4KA3_8BIT, 0x4137, 0x33},
	{T4KA3_8BIT, 0x0820, 0x0A},
	{T4KA3_8BIT, 0x0821, 0x0D},
	{T4KA3_8BIT, 0x0822, 0x00},
	{T4KA3_8BIT, 0x0823, 0x00},
	{T4KA3_8BIT, 0x0301, 0x0A},
	{T4KA3_8BIT, 0x0303, 0x01},
	{T4KA3_8BIT, 0x0305, 0x04},
	{T4KA3_8BIT, 0x0306, 0x02},
	{T4KA3_8BIT, 0x0307, 0x18},
	{T4KA3_8BIT, 0x030B, 0x01},
	{T4KA3_8BIT, 0x034C, 0x0C},
	{T4KA3_8BIT, 0x034D, 0xD0},
	{T4KA3_8BIT, 0x034E, 0x07},
	{T4KA3_8BIT, 0x034F, 0x3A},
	{T4KA3_8BIT, 0x0340, 0x09},
	{T4KA3_8BIT, 0x0341, 0xBC},
	{T4KA3_8BIT, 0x0342, 0x0D},
	{T4KA3_8BIT, 0x0343, 0x70},
	{T4KA3_8BIT, 0x0344, 0x00},
	{T4KA3_8BIT, 0x0345, 0x00},
	{T4KA3_8BIT, 0x0346, 0x00},
	{T4KA3_8BIT, 0x0347, 0x00},
	{T4KA3_8BIT, 0x0348, 0x0C},
	{T4KA3_8BIT, 0x0349, 0xCF},
	{T4KA3_8BIT, 0x034A, 0x09},
	{T4KA3_8BIT, 0x034B, 0x9f},
	{T4KA3_8BIT, 0x0408, 0x00},
	{T4KA3_8BIT, 0x0409, 0x00},
	{T4KA3_8BIT, 0x040A, 0x01},
	{T4KA3_8BIT, 0x040B, 0x34},
	{T4KA3_8BIT, 0x040C, 0x0C},
	{T4KA3_8BIT, 0x040D, 0xD0},
	{T4KA3_8BIT, 0x040E, 0x07},
	{T4KA3_8BIT, 0x040F, 0x3C},
	{T4KA3_8BIT, 0x0900, 0x01},
	{T4KA3_8BIT, 0x0901, 0x11},
	{T4KA3_8BIT, 0x0902, 0x00},
	{T4KA3_8BIT, 0x4220, 0x00},
	{T4KA3_8BIT, 0x4222, 0x01},
	{T4KA3_8BIT, 0x3380, 0x01},
	{T4KA3_8BIT, 0x3090, 0x88},
	{T4KA3_8BIT, 0x3394, 0x20},
	{T4KA3_8BIT, 0x3090, 0x08},
	{T4KA3_8BIT, 0x3394, 0x10},
	{T4KA3_TOK_TERM, 0, 0 }
};

static struct t4ka3_reg const t4ka3_3280x2464_30fps[] = {
	{T4KA3_8BIT, 0x0112, 0x0A},
	{T4KA3_8BIT, 0x0113, 0x0A},
	{T4KA3_8BIT, 0x0114, 0x03},
	{T4KA3_8BIT, 0x4136, 0x13},
	{T4KA3_8BIT, 0x4137, 0x33},
	{T4KA3_8BIT, 0x0820, 0x0A},
	{T4KA3_8BIT, 0x0821, 0x0D},
	{T4KA3_8BIT, 0x0822, 0x00},
	{T4KA3_8BIT, 0x0823, 0x00},
	{T4KA3_8BIT, 0x0301, 0x0A},
	{T4KA3_8BIT, 0x0303, 0x01},
	{T4KA3_8BIT, 0x0305, 0x04},
	{T4KA3_8BIT, 0x0306, 0x02},
	{T4KA3_8BIT, 0x0307, 0x18},
	{T4KA3_8BIT, 0x030B, 0x01},
	{T4KA3_8BIT, 0x034C, 0x0C},
	{T4KA3_8BIT, 0x034D, 0xD0},
	{T4KA3_8BIT, 0x034E, 0x09},
	{T4KA3_8BIT, 0x034F, 0x9E},
	{T4KA3_8BIT, 0x0340, 0x09},
	{T4KA3_8BIT, 0x0341, 0xBC},
	{T4KA3_8BIT, 0x0342, 0x0D},
	{T4KA3_8BIT, 0x0343, 0x70},
	{T4KA3_8BIT, 0x0344, 0x00},
	{T4KA3_8BIT, 0x0345, 0x00},
	{T4KA3_8BIT, 0x0346, 0x00},
	{T4KA3_8BIT, 0x0347, 0x00},
	{T4KA3_8BIT, 0x0348, 0x0C},
	{T4KA3_8BIT, 0x0349, 0xCF},
	{T4KA3_8BIT, 0x034A, 0x09},
	{T4KA3_8BIT, 0x034B, 0x9F},
	{T4KA3_8BIT, 0x0408, 0x00},
	{T4KA3_8BIT, 0x0409, 0x00},
	{T4KA3_8BIT, 0x040A, 0x00},
	{T4KA3_8BIT, 0x040B, 0x02},
	{T4KA3_8BIT, 0x040C, 0x0C},
	{T4KA3_8BIT, 0x040D, 0xD0},
	{T4KA3_8BIT, 0x040E, 0x09},
	{T4KA3_8BIT, 0x040F, 0xA0},
	{T4KA3_8BIT, 0x0900, 0x01},
	{T4KA3_8BIT, 0x0901, 0x11},
	{T4KA3_8BIT, 0x0902, 0x00},
	{T4KA3_8BIT, 0x4220, 0x00},
	{T4KA3_8BIT, 0x4222, 0x01},
	{T4KA3_8BIT, 0x3380, 0x01},
	{T4KA3_8BIT, 0x3090, 0x88},
	{T4KA3_8BIT, 0x3394, 0x20},
	{T4KA3_8BIT, 0x3090, 0x08},
	{T4KA3_8BIT, 0x3394, 0x10},
	{T4KA3_TOK_TERM, 0, 0 }
};

struct t4ka3_resolution t4ka3_res_preview[] = {

	{
		.desc = "t4ka3_736x496_30fps",
		.regs = t4ka3_736x496_30fps,
		.width = 736,
		.height = 496,
		.fps = 30,
		.pixels_per_line = 3744, /* consistent with regs arrays */
		.lines_per_frame = 2492, /* consistent with regs arrays */
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
		.skip_frames = 2,
		.mipi_freq = 700800,
		.code = V4L2_MBUS_FMT_SGRBG10_1X10,
	},
	{
		.desc = "t4ka3_896x736_30fps",
		.regs = t4ka3_896x736_30fps,
		.width = 896,
		.height = 736,
		.fps = 30,
		.pixels_per_line = 3744, /* consistent with regs arrays */
		.lines_per_frame = 2492, /* consistent with regs arrays */
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
		.skip_frames = 2,
		.mipi_freq = 700800,
		.code = V4L2_MBUS_FMT_SGRBG10_1X10,
	},
	{
		.desc = "t4ka3_1936x1096_30fps",
		.regs = t4ka3_1936x1096_30fps,
		.width = 1936,
		.height = 1096,
		.fps = 30,
		.pixels_per_line = 3744, /* consistent with regs arrays */
		.lines_per_frame = 2492, /* consistent with regs arrays */
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
		.skip_frames = 2,
		.mipi_freq = 700800,
		.code = V4L2_MBUS_FMT_SGRBG10_1X10,
	},
	{
		.desc = "t4ka3_3280x2464_30fps",
		.regs = t4ka3_3280x2464_30fps,
		.width = 3280,
		.height = 2464,
		.fps = 30,
		.pixels_per_line = 3440, /* consistent with regs arrays */
		.lines_per_frame = 2492, /* consistent with regs arrays */
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
		.skip_frames = 2,
		.mipi_freq = 700800,
		.code = V4L2_MBUS_FMT_SGRBG10_1X10,
	},
};
#define N_RES_PREVIEW (ARRAY_SIZE(t4ka3_res_preview))

struct t4ka3_resolution t4ka3_res_still[] = {
	{
		.desc = "t4ka3_3280x2464_30fps",
		.regs = t4ka3_3280x2464_30fps,
		.width = 3280,
		.height = 2464,
		.fps = 30,
		.pixels_per_line = 3440, /* consistent with regs arrays */
		.lines_per_frame = 2492, /* consistent with regs arrays */
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
		.skip_frames = 2,
		.mipi_freq = 700800,
		.code = V4L2_MBUS_FMT_SGRBG10_1X10,
	},
};
#define N_RES_STILL (ARRAY_SIZE(t4ka3_res_still))

struct t4ka3_resolution t4ka3_res_video[] = {
	{
		.desc = "t4ka3_1296x736_30fps",
		.regs = t4ka3_1296x736_30fps,
		.width = 1296,
		.height = 736,
		.fps = 30,
		.pixels_per_line = 3440, /* consistent with regs arrays */
		.lines_per_frame = 2492, /* consistent with regs arrays */
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
		.skip_frames = 2,
		.mipi_freq = 700800,
		.code = V4L2_MBUS_FMT_SGRBG10_1X10,
	},
	{
		.desc = "t4ka3_3280x1852_30fps",
		.regs = t4ka3_3280x1852_30fps,
		.width = 3280,
		.height = 1852,
		.fps = 30,
		.pixels_per_line = 3440, /* consistent with regs arrays */
		.lines_per_frame = 2492, /* consistent with regs arrays */
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
		.skip_frames = 2,
		.mipi_freq = 700800,
		.code = V4L2_MBUS_FMT_SGRBG10_1X10,
	},
	{
		.desc = "t4ka3_3280x2464_30fps",
		.regs = t4ka3_3280x2464_30fps,
		.width = 3280,
		.height = 2464,
		.fps = 30,
		.pixels_per_line = 3440, /* consistent with regs arrays */
		.lines_per_frame = 2492, /* consistent with regs arrays */
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
		.skip_frames = 2,
		.mipi_freq = 700800,
		.code = V4L2_MBUS_FMT_SGRBG10_1X10,
	},
};
#define N_RES_VIDEO (ARRAY_SIZE(t4ka3_res_video))

struct t4ka3_resolution *t4ka3_res = t4ka3_res_preview;
static int N_RES = N_RES_PREVIEW;

#endif
