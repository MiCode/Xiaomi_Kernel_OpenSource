/*
 * Support for Sony OV9724 camera sensor.
 *
 * Copyright (c) 2010 Intel Corporation. All Rights Reserved.
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

#ifndef __OV9724_H__
#define __OV9724_H__
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
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#define OV9724_NAME	"ov9724"
#define V4L2_IDENT_OV9724 8245

/* Defines for register writes and register array processing */
#define OV9724_BYTE_MAX	30
#define OV9724_SHORT_MAX	16
#define I2C_MSG_LENGTH		0x2
#define I2C_RETRY_COUNT		5

#define OV9724_READ_MODE	0x3820
#define OV9724_TEST_PATTERN_MODE			0x0601

#define OV9724_HFLIP_BIT	0x1
#define OV9724_VFLIP_BIT	0x2
#define OV9724_VFLIP_OFFSET	1
#define OV9724_IMG_ORIENTATION	0x0101

#define I2C_RETRY_COUNT		5
#define MAX_FMTS		1

#define OV9724_PID_LOW		0x1
#define OV9724_PID_HIGH		0x0
#define OV9724_REV		0x2
#define OV9724_MOD_ID		0x9724

#define OV9724_RES_WIDTH_MAX	1296
#define OV9724_RES_HEIGHT_MAX	736

#define OV9724_FOCAL_LENGTH_NUM	166	/*1.66mm*/
#define OV9724_FOCAL_LENGTH_DEM	100
#define OV9724_F_NUMBER_DEFAULT_NUM	280
#define OV9724_F_NUMBER_DEM	100

#define OV9724_COARSE_INTEGRATION_TIME		0x0202
#define OV9724_GLOBAL_GAIN			0x0205

#define OV9724_INTG_BUF_COUNT		2

#define OV9724_VT_PIX_CLK_DIV			0x0300
#define OV9724_VT_SYS_CLK_DIV			0x0302
#define OV9724_PRE_PLL_CLK_DIV			0x0304
#define OV9724_PLL_MULTIPLIER			0x0306
#define OV9724_OP_PIX_DIV			0x0300
#define OV9724_OP_SYS_DIV			0x0302
#define OV9724_FRAME_LENGTH_LINES		0x0340
#define OV9724_COARSE_INTG_TIME_MIN		0x1004
#define OV9724_FINE_INTG_TIME_MIN		0x1008

#define OV9724_BIN_FACTOR_MAX			1
#define OV9724_MCLK		192

#define OV9724_HORIZONTAL_START_H 0x0344
#define OV9724_VERTICAL_START_H 0x0346
#define OV9724_HORIZONTAL_END_H 0x0348
#define OV9724_VERTICAL_END_H 0x034a
#define OV9724_HORIZONTAL_OUTPUT_SIZE_H 0x034c
#define OV9724_VERTICAL_OUTPUT_SIZE_H 0x034e

/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define OV9724_FOCAL_LENGTH_DEFAULT 0xA60064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define OV9724_F_NUMBER_DEFAULT 0x1200064

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define OV9724_F_NUMBER_RANGE 0x1D0a1D0a

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

enum ov9724_tok_type {
	OV9724_8BIT  = 0x0001,
	OV9724_16BIT = 0x0002,
	OV9724_RMW   = 0x0010,
	OV9724_TOK_TERM   = 0xf000,	/* terminating token for reg list */
	OV9724_TOK_DELAY  = 0xfe00, /* delay token for reg list */
	OV9724_TOK_MASK = 0xfff0
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

/**
 * struct ov9724_fwreg - Fisare burst command
 * @type: FW burst or 8/16 bit register
 * @addr: 16-bit offset to register or other values depending on type
 * @val: data value for burst (or other commands)
 *
 * Define a structure for sensor register initialization values
 */
struct ov9724_fwreg {
	enum ov9724_tok_type type; /* value, register or FW burst string */
	u16 addr;	/* target address */
	u32 val[8];
};

/**
 * struct ov9724_reg - MI sensor  register format
 * @type: type of the register
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 *
 * Define a structure for sensor register initialization values
 */
struct ov9724_reg {
	enum ov9724_tok_type type;
	union {
		u16 sreg;
		struct ov9724_fwreg *fwreg;
	} reg;
	u32 val;	/* @set value for read/mod/write, @mask */
};

#define to_ov9724_sensor(x) container_of(x, struct ov9724_device, sd)

#define OV9724_MAX_WRITE_BUF_SIZE	30
struct ov9724_write_buffer {
	u16 addr;
	u8 data[OV9724_MAX_WRITE_BUF_SIZE];
};

struct ov9724_write_ctrl {
	int index;
	struct ov9724_write_buffer buffer;
};


struct ov9724_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;
	struct camera_sensor_platform_data *platform_data;
	struct mutex input_lock; /* serialize sensor's ioctl */
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
	u16 pixels_per_line;
	u16 lines_per_frame;
	u8 fps;
	u8 res;
	u8 type;
	u8 sensor_revision;
};

struct ov9724_format_struct {
	u8 *desc;
	struct regval_list *regs;
	u32 pixelformat;
};

struct ov9724_resolution {
	u8 *desc;
	const struct ov9724_reg *regs;
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
};

struct ov9724_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, int value);
};


/************************** settings for ov9724 *************************/
static struct ov9724_reg const ov9724_1040_592_30fps[] = {
	{OV9724_8BIT, {0x0344},	0x00},/* Horizontal start - Hi */
	{OV9724_8BIT, {0x0345},	0x7e},/* Horizontal start - Low */
	{OV9724_8BIT, {0x0346},	0x00},/* Vertical start - Hi */
	{OV9724_8BIT, {0x0347},	0x44},/* Vertical start - Low */
	{OV9724_8BIT, {0x0348},	0x04},/* Horizontal end - Hi */
	{OV9724_8BIT, {0x0349},	0x8d},/* Horizontal end - Low */
	{OV9724_8BIT, {0x034a},	0x02},/* Vertical end - Hi */
	{OV9724_8BIT, {0x034b},	0x97},/* Vertical end - Low */
	{OV9724_8BIT, {0x034c},	0x04},/* Image width - Hi */
	{OV9724_8BIT, {0x034d},	0x10},/* Image width - Low */
	{OV9724_8BIT, {0x034e},	0x02},/* Image Height - Hi */
	{OV9724_8BIT, {0x034f},	0x50},/* Image Height - Low */
	{OV9724_8BIT, {0x4908},	0x14},
	{OV9724_8BIT, {0x4909},	0x08},
	{OV9724_8BIT, {0x3811},	0x0a},
	{OV9724_8BIT, {0x3813},	0x04},
	{OV9724_8BIT, {0x0340},	0x03},/* Vertical length - Hi */
	{OV9724_8BIT, {0x0341},	0x68},/* Vertical length - Low */
	{OV9724_8BIT, {0x0342},	0x05},/* Horizontal length - Hi */
	{OV9724_8BIT, {0x0343},	0x60},/* Horizontal length - Low */

	{OV9724_8BIT, {0x0301},	0x0a},
	{OV9724_8BIT, {0x0303},	0x02},
	{OV9724_8BIT, {0x0305},	0x02},/* Pre-pll divider -Low */
	{OV9724_8BIT, {0x0307},	0x4b},/* Pll multi -Low */
	{OV9724_8BIT, {0x0310},	0x00},

	{OV9724_8BIT, {0x0202},	0x01},/* Integration time - Hi */
	{OV9724_8BIT, {0x0203},	0x80},/* Integration time - Low */
	{OV9724_8BIT, {0x0205},	0x3f},

	{OV9724_8BIT, {0x0383},	0x01},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x0385},	0x01},
	{OV9724_8BIT, {0x0387},	0x01},
	{OV9724_8BIT, {0x3821},	0x00},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x3820},	0xa0},

	{OV9724_8BIT, {0x4801},	0x0f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4801},	0x8f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4814},	0x2b},/* Mipi ctrl14 */
	{OV9724_8BIT, {0x4307},	0x3a},
	{OV9724_8BIT, {0x370a},	0x23},

	{OV9724_8BIT, {0x5000},	0x06},/* Isp ctrl0 */
	{OV9724_8BIT, {0x5001},	0x73},/* Isp ctrl1 */
	{OV9724_TOK_TERM, {0}, 0}
};
static struct ov9724_reg const ov9724_736_496_30fps[] = {
	{OV9724_8BIT, {0x0344},	0x01},/* Horizontal start - Hi */
	{OV9724_8BIT, {0x0345},	0x1e},/* Horizontal start - Low */
	{OV9724_8BIT, {0x0346},	0x00},/* Vertical start - Hi */
	{OV9724_8BIT, {0x0347},	0x98},/* Vertical start - Low */
	{OV9724_8BIT, {0x0348},	0x04},/* Horizontal end - Hi */
	{OV9724_8BIT, {0x0349},	0x55},/* Horizontal end - Low */
	{OV9724_8BIT, {0x034a},	0x02},/* Vertical end - Hi */
	{OV9724_8BIT, {0x034b},	0x9b},/* Vertical end - Low */
	{OV9724_8BIT, {0x034c},	0x02},/* Image width - Hi */
	{OV9724_8BIT, {0x034d},	0xe0},/* Image width - Low */
	{OV9724_8BIT, {0x034e},	0x01},/* Image Height - Hi */
	{OV9724_8BIT, {0x034f},	0xf0},/* Image Height - Low */
	{OV9724_8BIT, {0x4908},	0x14},
	{OV9724_8BIT, {0x4909},	0x08},
	{OV9724_8BIT, {0x3811},	0x0a},
	{OV9724_8BIT, {0x3813},	0x04},
	{OV9724_8BIT, {0x0340},	0x03},/* Vertical length - Hi */
	{OV9724_8BIT, {0x0341},	0x68},/* Vertical length - Low */
	{OV9724_8BIT, {0x0342},	0x05},/* Horizontal length - Hi */
	{OV9724_8BIT, {0x0343},	0x60},/* Horizontal length - Low */

	{OV9724_8BIT, {0x0301},	0x0a},
	{OV9724_8BIT, {0x0303},	0x02},
	{OV9724_8BIT, {0x0305},	0x02},/* Pre-pll divider -Low */
	{OV9724_8BIT, {0x0307},	0x4b},/* Pll multi -Low */
	{OV9724_8BIT, {0x0310},	0x00},

	{OV9724_8BIT, {0x0202},	0x01},/* Integration time - Hi */
	{OV9724_8BIT, {0x0203},	0x80},/* Integration time - Low */
	{OV9724_8BIT, {0x0205},	0x3f},

	{OV9724_8BIT, {0x0383},	0x01},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x0385},	0x01},
	{OV9724_8BIT, {0x0387},	0x01},
	{OV9724_8BIT, {0x3821},	0x00},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x3820},	0xa0},

	{OV9724_8BIT, {0x4801},	0x0f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4801},	0x8f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4814},	0x2b},/* Mipi ctrl14 */
	{OV9724_8BIT, {0x4307},	0x3a},
	{OV9724_8BIT, {0x370a},	0x23},

	{OV9724_8BIT, {0x5000},	0x06},/* Isp ctrl0 */
	{OV9724_8BIT, {0x5001},	0x73},/* Isp ctrl1 */
	{OV9724_TOK_TERM, {0}, 0}
};
static struct ov9724_reg const ov9724_656_496_30fps[] = {
	{OV9724_8BIT, {0x0344},	0x01},/* Horizontal start - Hi */
	{OV9724_8BIT, {0x0345},	0x1e},/* Horizontal start - Low */
	{OV9724_8BIT, {0x0346},	0x00},/* Vertical start - Hi */
	{OV9724_8BIT, {0x0347},	0x98},/* Vertical start - Low */
	{OV9724_8BIT, {0x0348},	0x04},/* Horizontal end - Hi */
	{OV9724_8BIT, {0x0349},	0x55},/* Horizontal end - Low */
	{OV9724_8BIT, {0x034a},	0x02},/* Vertical end - Hi */
	{OV9724_8BIT, {0x034b},	0x9b},/* Vertical end - Low */
	{OV9724_8BIT, {0x034c},	0x02},/* Image width - Hi */
	{OV9724_8BIT, {0x034d},	0x90},/* Image width - Low */
	{OV9724_8BIT, {0x034e},	0x01},/* Image Height - Hi */
	{OV9724_8BIT, {0x034f},	0xf0},/* Image Height - Low */
	{OV9724_8BIT, {0x4908},	0x14},
	{OV9724_8BIT, {0x4909},	0x08},
	{OV9724_8BIT, {0x3811},	0x0a},
	{OV9724_8BIT, {0x3813},	0x04},
	{OV9724_8BIT, {0x0340},	0x03},/* Vertical length - Hi */
	{OV9724_8BIT, {0x0341},	0x68},/* Vertical length - Low */
	{OV9724_8BIT, {0x0342},	0x05},/* Horizontal length - Hi */
	{OV9724_8BIT, {0x0343},	0x60},/* Horizontal length - Low */

	{OV9724_8BIT, {0x0301},	0x0a},
	{OV9724_8BIT, {0x0303},	0x02},
	{OV9724_8BIT, {0x0305},	0x02},/* Pre-pll divider -Low */
	{OV9724_8BIT, {0x0307},	0x4b},/* Pll multi -Low */
	{OV9724_8BIT, {0x0310},	0x00},

	{OV9724_8BIT, {0x0202},	0x01},/* Integration time - Hi */
	{OV9724_8BIT, {0x0203},	0x80},/* Integration time - Low */
	{OV9724_8BIT, {0x0205},	0x3f},

	{OV9724_8BIT, {0x0383},	0x01},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x0385},	0x01},
	{OV9724_8BIT, {0x0387},	0x01},
	{OV9724_8BIT, {0x3821},	0x00},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x3820},	0xa0},

	{OV9724_8BIT, {0x4801},	0x0f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4801},	0x8f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4814},	0x2b},/* Mipi ctrl14 */
	{OV9724_8BIT, {0x4307},	0x3a},
	{OV9724_8BIT, {0x370a},	0x23},

	{OV9724_8BIT, {0x5000},	0x06},/* Isp ctrl0 */
	{OV9724_8BIT, {0x5001},	0x73},/* Isp ctrl1 */
	{OV9724_TOK_TERM, {0}, 0}
};

static struct ov9724_reg const ov9724_368_304_30fps[] = {
	{OV9724_8BIT, {0x0344},	0x00},/* Horizontal start - Hi */
	{OV9724_8BIT, {0x0345},	0x00},/* Horizontal start - Low */
	{OV9724_8BIT, {0x0346},	0x00},/* Vertical start - Hi */
	{OV9724_8BIT, {0x0347},	0x00},/* Vertical start - Low */
	{OV9724_8BIT, {0x0348},	0x04},/* Horizontal end - Hi */
	{OV9724_8BIT, {0x0349},	0xff},/* Horizontal end - Low */
	{OV9724_8BIT, {0x034a},	0x02},/* Vertical end - Hi */
	{OV9724_8BIT, {0x034b},	0xcf},/* Vertical end - Low */
	{OV9724_8BIT, {0x034c},	0x01},/* Image width - Hi */
	{OV9724_8BIT, {0x034d},	0x70},/* Image width - Low */
	{OV9724_8BIT, {0x034e},	0x01},/* Image Height - Hi */
	{OV9724_8BIT, {0x034f},	0x30},/* Image Height - Low */
	{OV9724_8BIT, {0x4908},	0x20},
	{OV9724_8BIT, {0x4909},	0x14},
	{OV9724_8BIT, {0x3811},	0xf8},
	{OV9724_8BIT, {0x3813},	0x3e},
	{OV9724_8BIT, {0x0340},	0x03},/* Vertical length - Hi */
	{OV9724_8BIT, {0x0341},	0x68},/* Vertical length - Low */
	{OV9724_8BIT, {0x0342},	0x05},/* Horizontal length - Hi */
	{OV9724_8BIT, {0x0343},	0x60},/* Horizontal length - Low */

	{OV9724_8BIT, {0x0301},	0x0a},
	{OV9724_8BIT, {0x0303},	0x02},
	{OV9724_8BIT, {0x0305},	0x02},/* Pre-pll divider -Low */
	{OV9724_8BIT, {0x0307},	0x4b},/* Pll multi -Low */
	{OV9724_8BIT, {0x0310},	0x00},

	{OV9724_8BIT, {0x0202},	0x01},/* Integration time - Hi */
	{OV9724_8BIT, {0x0203},	0x80},/* Integration time - Low */
	{OV9724_8BIT, {0x0205},	0x3f},

	{OV9724_8BIT, {0x0383},	0x03},
	{OV9724_8BIT, {0x4501},	0x09},
	{OV9724_8BIT, {0x0385},	0x01},
	{OV9724_8BIT, {0x0387},	0x03},
	{OV9724_8BIT, {0x3821},	0x01},
	{OV9724_8BIT, {0x4501},	0x09},
	{OV9724_8BIT, {0x3820},	0xa1},

	{OV9724_8BIT, {0x4801},	0x0f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4801},	0x8f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4814},	0x2b},/* Mipi ctrl14 */
	{OV9724_8BIT, {0x4307},	0x3a},
	{OV9724_8BIT, {0x370a},	0x23},

	{OV9724_8BIT, {0x5000},	0x06},/* Isp ctrl0 */
	{OV9724_8BIT, {0x5001},	0x73},/* Isp ctrl1 */
	{OV9724_TOK_TERM, {0}, 0}
};
static struct ov9724_reg const ov9724_336_256_30fps[] = {
	{OV9724_8BIT, {0x0344},	0x00},/* Horizontal start - Hi */
	{OV9724_8BIT, {0x0345},	0x10},/* Horizontal start - Low */
	{OV9724_8BIT, {0x0346},	0x00},/* Vertical start - Hi */
	{OV9724_8BIT, {0x0347},	0x14},/* Vertical start - Low */
	{OV9724_8BIT, {0x0348},	0x04},/* Horizontal end - Hi */
	{OV9724_8BIT, {0x0349},	0xff},/* Horizontal end - Low */
	{OV9724_8BIT, {0x034a},	0x02},/* Vertical end - Hi */
	{OV9724_8BIT, {0x034b},	0xcf},/* Vertical end - Low */
	{OV9724_8BIT, {0x034c},	0x01},/* Image width - Hi */
	{OV9724_8BIT, {0x034d},	0x50},/* Image width - Low */
	{OV9724_8BIT, {0x034e},	0x01},/* Image Height - Hi */
	{OV9724_8BIT, {0x034f},	0x00},/* Image Height - Low */
	{OV9724_8BIT, {0x4908},	0x20},
	{OV9724_8BIT, {0x4909},	0x14},
	{OV9724_8BIT, {0x3811},	0xf8},
	{OV9724_8BIT, {0x3813},	0x3e},
	{OV9724_8BIT, {0x0340},	0x03},/* Vertical length - Hi */
	{OV9724_8BIT, {0x0341},	0x68},/* Vertical length - Low */
	{OV9724_8BIT, {0x0342},	0x05},/* Horizontal length - Hi */
	{OV9724_8BIT, {0x0343},	0x60},/* Horizontal length - Low */

	{OV9724_8BIT, {0x0301},	0x0a},
	{OV9724_8BIT, {0x0303},	0x02},
	{OV9724_8BIT, {0x0305},	0x02},/* Pre-pll divider -Low */
	{OV9724_8BIT, {0x0307},	0x4b},/* Pll multi -Low */
	{OV9724_8BIT, {0x0310},	0x00},

	{OV9724_8BIT, {0x0202},	0x01},/* Integration time - Hi */
	{OV9724_8BIT, {0x0203},	0x80},/* Integration time - Low */
	{OV9724_8BIT, {0x0205},	0x3f},

	{OV9724_8BIT, {0x0383},	0x03},
	{OV9724_8BIT, {0x4501},	0x09},
	{OV9724_8BIT, {0x0385},	0x01},
	{OV9724_8BIT, {0x0387},	0x03},
	{OV9724_8BIT, {0x3821},	0x01},
	{OV9724_8BIT, {0x4501},	0x09},
	{OV9724_8BIT, {0x3820},	0xa1},

	{OV9724_8BIT, {0x4801},	0x0f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4801},	0x8f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4814},	0x2b},/* Mipi ctrl14 */
	{OV9724_8BIT, {0x4307},	0x3a},
	{OV9724_8BIT, {0x370a},	0x23},

	{OV9724_8BIT, {0x5000},	0x06},/* Isp ctrl0 */
	{OV9724_8BIT, {0x5001},	0x73},/* Isp ctrl1 */
	{OV9724_TOK_TERM, {0}, 0}
};
static struct ov9724_reg const ov9724_848_616_30fps[] = {
	{OV9724_8BIT, {0x0344},	0x00},/* Horizontal start - Hi */
	{OV9724_8BIT, {0x0345},	0xe6},/* Horizontal start - Low */
	{OV9724_8BIT, {0x0346},	0x00},/* Vertical start - Hi */
	{OV9724_8BIT, {0x0347},	0x34},/* Vertical start - Low */
	{OV9724_8BIT, {0x0348},	0x04},/* Horizontal end - Hi */
	{OV9724_8BIT, {0x0349},	0x55},/* Horizontal end - Low */
	{OV9724_8BIT, {0x034a},	0x02},/* Vertical end - Hi */
	{OV9724_8BIT, {0x034b},	0x9b},/* Vertical end - Low */
	{OV9724_8BIT, {0x034c},	0x03},/* Image width - Hi */
	{OV9724_8BIT, {0x034d},	0x50},/* Image width - Low */
	{OV9724_8BIT, {0x034e},	0x02},/* Image Height - Hi */
	{OV9724_8BIT, {0x034f},	0x68},/* Image Height - Low */
	{OV9724_8BIT, {0x4908},	0x14},
	{OV9724_8BIT, {0x4909},	0x08},
	{OV9724_8BIT, {0x3811},	0x0a},
	{OV9724_8BIT, {0x3813},	0x04},
	{OV9724_8BIT, {0x0340},	0x03},/* Vertical length - Hi */
	{OV9724_8BIT, {0x0341},	0x68},/* Vertical length - Low */
	{OV9724_8BIT, {0x0342},	0x05},/* Horizontal length - Hi */
	{OV9724_8BIT, {0x0343},	0x60},/* Horizontal length - Low */

	{OV9724_8BIT, {0x0301},	0x0a},
	{OV9724_8BIT, {0x0303},	0x02},
	{OV9724_8BIT, {0x0305},	0x02},/* Pre-pll divider -Low */
	{OV9724_8BIT, {0x0307},	0x4b},/* Pll multi -Low */
	{OV9724_8BIT, {0x0310},	0x00},

	{OV9724_8BIT, {0x0202},	0x01},/* Integration time - Hi */
	{OV9724_8BIT, {0x0203},	0x80},/* Integration time - Low */
	{OV9724_8BIT, {0x0205},	0x3f},

	{OV9724_8BIT, {0x0383},	0x01},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x0385},	0x01},
	{OV9724_8BIT, {0x0387},	0x01},
	{OV9724_8BIT, {0x3821},	0x00},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x3820},	0xa0},

	{OV9724_8BIT, {0x4801},	0x0f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4801},	0x8f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4814},	0x2b},/* Mipi ctrl14 */
	{OV9724_8BIT, {0x4307},	0x3a},
	{OV9724_8BIT, {0x370a},	0x23},

	{OV9724_8BIT, {0x5000},	0x06},/* Isp ctrl0 */
	{OV9724_8BIT, {0x5001},	0x73},/* Isp ctrl1 */
	{OV9724_TOK_TERM, {0}, 0}
};
static struct ov9724_reg const ov9724_444_348_30fps[] = {
	{OV9724_8BIT, {0x0344},	0x00},/* Horizontal start - Hi */
	{OV9724_8BIT, {0x0345},	0x00},/* Horizontal start - Low */
	{OV9724_8BIT, {0x0346},	0x00},/* Vertical start - Hi */
	{OV9724_8BIT, {0x0347},	0x00},/* Vertical start - Low */
	{OV9724_8BIT, {0x0348},	0x04},/* Horizontal end - Hi */
	{OV9724_8BIT, {0x0349},	0xff},/* Horizontal end - Low */
	{OV9724_8BIT, {0x034a},	0x02},/* Vertical end - Hi */
	{OV9724_8BIT, {0x034b},	0xcf},/* Vertical end - Low */
	{OV9724_8BIT, {0x034c},	0x01},/* Image width - Hi */
	{OV9724_8BIT, {0x034d},	0xc0},/* Image width - Low */
	{OV9724_8BIT, {0x034e},	0x01},/* Image Height - Hi */
	{OV9724_8BIT, {0x034f},	0x5c},/* Image Height - Low */
	{OV9724_8BIT, {0x4908},	0x20},
	{OV9724_8BIT, {0x4909},	0x14},
	{OV9724_8BIT, {0x3811},	0x10},
	{OV9724_8BIT, {0x3813},	0x0a},
	{OV9724_8BIT, {0x0340},	0x03},/* Vertical length - Hi */
	{OV9724_8BIT, {0x0341},	0x68},/* Vertical length - Low */
	{OV9724_8BIT, {0x0342},	0x05},/* Horizontal length - Hi */
	{OV9724_8BIT, {0x0343},	0x60},/* Horizontal length - Low */

	{OV9724_8BIT, {0x0301},	0x0a},
	{OV9724_8BIT, {0x0303},	0x02},
	{OV9724_8BIT, {0x0305},	0x02},/* Pre-pll divider -Low */
	{OV9724_8BIT, {0x0307},	0x4b},/* Pll multi -Low */
	{OV9724_8BIT, {0x0310},	0x00},

	{OV9724_8BIT, {0x0202},	0x01},/* Integration time - Hi */
	{OV9724_8BIT, {0x0203},	0x80},/* Integration time - Low */
	{OV9724_8BIT, {0x0205},	0x3f},

	{OV9724_8BIT, {0x0383},	0x01},
	{OV9724_8BIT, {0x4501},	0x09},
	{OV9724_8BIT, {0x0385},	0x01},
	{OV9724_8BIT, {0x0387},	0x03},
	{OV9724_8BIT, {0x3821},	0x01},
	{OV9724_8BIT, {0x4501},	0x09},
	{OV9724_8BIT, {0x3820},	0xa1},

	{OV9724_8BIT, {0x4801},	0x0f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4801},	0x8f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4814},	0x2b},/* Mipi ctrl14 */
	{OV9724_8BIT, {0x4307},	0x3a},
	{OV9724_8BIT, {0x370a},	0x23},

	{OV9724_8BIT, {0x5000},	0x06},/* Isp ctrl0 */
	{OV9724_8BIT, {0x5001},	0x73},/* Isp ctrl1 */
	{OV9724_TOK_TERM, {0}, 0}
};
static struct ov9724_reg const ov9724_936_602_30fps[] = {
	{OV9724_8BIT, {0x0344},	0x00},/* Horizontal start - Hi */
	{OV9724_8BIT, {0x0345},	0xae},/* Horizontal start - Low */
	{OV9724_8BIT, {0x0346},	0x00},/* Vertical start - Hi */
	{OV9724_8BIT, {0x0347},	0x44},/* Vertical start - Low */
	{OV9724_8BIT, {0x0348},	0x04},/* Horizontal end - Hi */
	{OV9724_8BIT, {0x0349},	0x9d},/* Horizontal end - Low */
	{OV9724_8BIT, {0x034a},	0x02},/* Vertical end - Hi */
	{OV9724_8BIT, {0x034b},	0xa6},/* Vertical end - Low */
	{OV9724_8BIT, {0x034c},	0x03},/* Image width - Hi */
	{OV9724_8BIT, {0x034d},	0xa8},/* Image width - Low */
	{OV9724_8BIT, {0x034e},	0x02},/* Image Height - Hi */
	{OV9724_8BIT, {0x034f},	0x5a},/* Image Height - Low */
	{OV9724_8BIT, {0x4908},	0x14},
	{OV9724_8BIT, {0x4909},	0x08},
	{OV9724_8BIT, {0x3811},	0x0a},
	{OV9724_8BIT, {0x3813},	0x04},
	{OV9724_8BIT, {0x0340},	0x03},/* Vertical length - Hi */
	{OV9724_8BIT, {0x0341},	0x68},/* Vertical length - Low */
	{OV9724_8BIT, {0x0342},	0x05},/* Horizontal length - Hi */
	{OV9724_8BIT, {0x0343},	0x60},/* Horizontal length - Low */

	{OV9724_8BIT, {0x0301},	0x0a},
	{OV9724_8BIT, {0x0303},	0x02},
	{OV9724_8BIT, {0x0305},	0x02},/* Pre-pll divider -Low */
	{OV9724_8BIT, {0x0307},	0x4b},/* Pll multi -Low */
	{OV9724_8BIT, {0x0310},	0x00},

	{OV9724_8BIT, {0x0202},	0x01},/* Integration time - Hi */
	{OV9724_8BIT, {0x0203},	0x80},/* Integration time - Low */
	{OV9724_8BIT, {0x0205},	0x3f},

	{OV9724_8BIT, {0x0383},	0x01},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x0385},	0x01},
	{OV9724_8BIT, {0x0387},	0x01},
	{OV9724_8BIT, {0x3821},	0x00},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x3820},	0xa0},

	{OV9724_8BIT, {0x4801},	0x0f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4801},	0x8f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4814},	0x2b},/* Mipi ctrl14 */
	{OV9724_8BIT, {0x4307},	0x3a},
	{OV9724_8BIT, {0x370a},	0x23},

	{OV9724_8BIT, {0x5000},	0x06},/* Isp ctrl0 */
	{OV9724_8BIT, {0x5001},	0x73},/* Isp ctrl1 */
	{OV9724_TOK_TERM, {0}, 0}
};

static struct ov9724_reg const ov9724_720p_15fps[] = {
	{OV9724_8BIT, {0x0344},	0x00},/* Horizontal start - Hi */
	{OV9724_8BIT, {0x0345},	0x00},/* Horizontal start - Low */
	{OV9724_8BIT, {0x0346},	0x00},/* Vertical start - Hi */
	{OV9724_8BIT, {0x0347},	0x00},/* Vertical start - Low */
	{OV9724_8BIT, {0x0348},	0x05},/* Horizontal end - Hi */
	{OV9724_8BIT, {0x0349},	0x0b},/* Horizontal end - Low */
	{OV9724_8BIT, {0x034a},	0x02},/* Vertical end - Hi */
	{OV9724_8BIT, {0x034b},	0xdb},/* Vertical end - Low */
	{OV9724_8BIT, {0x034c},	0x05},/* Image width - Hi */
	{OV9724_8BIT, {0x034d},	0x0c},/* Image width - Low */
	{OV9724_8BIT, {0x034e},	0x02},/* Image Height - Hi */
	{OV9724_8BIT, {0x034f},	0xdc},/* Image Height - Low */
	{OV9724_8BIT, {0x4908},	0x14},
	{OV9724_8BIT, {0x4909},	0x08},
	{OV9724_8BIT, {0x3811},	0x0a},
	{OV9724_8BIT, {0x3813},	0x04},
	{OV9724_8BIT, {0x0340},	0x05},/* Vertical length - Hi */
	{OV9724_8BIT, {0x0341},	0xf0},/* Vertical length - Low */
	{OV9724_8BIT, {0x0342},	0x06},/* Horizontal length - Hi */
	{OV9724_8BIT, {0x0343},	0x28},/* Horizontal length - Low */
	{OV9724_8BIT, {0x0305},	0x02},/* Pre-pll divider -Low */
	{OV9724_8BIT, {0x0307},	0x4b},/* Pll multi -Low */

	{OV9724_8BIT, {0x0202},	0x02},/* Integration time - Hi */
	{OV9724_8BIT, {0x0203},	0xf0},/* Integration time - Low */
	{OV9724_8BIT, {0x4801},	0x0f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4801},	0x8f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4814},	0x2b},/* Mipi ctrl14 */
	{OV9724_8BIT, {0x4307},	0x3a},
	{OV9724_8BIT, {0x5000},	0x06},/* Isp ctrl0 */
	{OV9724_8BIT, {0x5001},	0x73},/* Isp ctrl1 */
	{OV9724_8BIT, {0x0205},	0x3f},/* Analog gain - Low */
	{OV9724_TOK_TERM, {0}, 0}
};

static struct ov9724_reg const ov9724_960x720_30fps[] = {
	{OV9724_8BIT, {0x0344},	0x00},/* Horizontal start - Hi */
	{OV9724_8BIT, {0x0345},	0xa0},/* Horizontal start - Low */
	{OV9724_8BIT, {0x0346},	0x00},/* Vertical start - Hi */
	{OV9724_8BIT, {0x0347},	0x00},/* Vertical start - Low */
	{OV9724_8BIT, {0x0348},	0x04},/* Horizontal end - Hi */
	{OV9724_8BIT, {0x0349},	0x6f},/* Horizontal end - Low */
	{OV9724_8BIT, {0x034a},	0x02},/* Vertical end - Hi */
	{OV9724_8BIT, {0x034b},	0xdf},/* Vertical end - Low */
	{OV9724_8BIT, {0x034c},	0x03},/* Image width - Hi */
	{OV9724_8BIT, {0x034d},	0xd0},/* Image width - Low */
	{OV9724_8BIT, {0x034e},	0x02},/* Image Height - Hi */
	{OV9724_8BIT, {0x034f},	0xe0},/* Image Height - Low */
	{OV9724_8BIT, {0x4908},	0x10},
	{OV9724_8BIT, {0x4909},	0x04},
	{OV9724_8BIT, {0x3811},	0x08},
	{OV9724_8BIT, {0x3813},	0x02},
	{OV9724_8BIT, {0x0340},	0x03},/* Vertical length - Hi */
	{OV9724_8BIT, {0x0341},	0x68},/* Vertical length - Low */
	{OV9724_8BIT, {0x0342},	0x05},/* Horizontal length - Hi */
	{OV9724_8BIT, {0x0343},	0x60},/* Horizontal length - Low */

	{OV9724_8BIT, {0x0301},	0x0a},
	{OV9724_8BIT, {0x0303},	0x02},
	{OV9724_8BIT, {0x0305},	0x02},/* Pre-pll divider -Low */
	{OV9724_8BIT, {0x0307},	0x4b},/* Pll multi -Low */
	{OV9724_8BIT, {0x0310},	0x00},

	{OV9724_8BIT, {0x0202},	0x01},/* Integration time - Hi */
	{OV9724_8BIT, {0x0203},	0x80},/* Integration time - Low */
	{OV9724_8BIT, {0x0205},	0x3f},

	{OV9724_8BIT, {0x0383},	0x01},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x0385},	0x01},
	{OV9724_8BIT, {0x0387},	0x01},
	{OV9724_8BIT, {0x3821},	0x00},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x3820},	0xa0},

	{OV9724_8BIT, {0x4801},	0x0f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4801},	0x8f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4814},	0x2b},/* Mipi ctrl14 */
	{OV9724_8BIT, {0x4307},	0x3a},
	{OV9724_8BIT, {0x370a},	0x23},

	{OV9724_8BIT, {0x5000},	0x06},/* Isp ctrl0 */
	{OV9724_8BIT, {0x5001},	0x73},/* Isp ctrl1 */
	{OV9724_TOK_TERM, {0}, 0}
};


static struct ov9724_reg const ov9724_720p_30fps[] = {
	{OV9724_8BIT, {0x0344},	0x00},/* Horizontal start - Hi */
	{OV9724_8BIT, {0x0345},	0x00},/* Horizontal start - Low */
	{OV9724_8BIT, {0x0346},	0x00},/* Vertical start - Hi */
	{OV9724_8BIT, {0x0347},	0x00},/* Vertical start - Low */
	{OV9724_8BIT, {0x0348},	0x05},/* Horizontal end - Hi */
	{OV9724_8BIT, {0x0349},	0x0f},/* Horizontal end - Low */
	{OV9724_8BIT, {0x034a},	0x02},/* Vertical end - Hi */
	{OV9724_8BIT, {0x034b},	0xdf},/* Vertical end - Low */
	{OV9724_8BIT, {0x034c},	0x05},/* Image width - Hi */
	{OV9724_8BIT, {0x034d},	0x10},/* Image width - Low */
	{OV9724_8BIT, {0x034e},	0x02},/* Image Height - Hi */
	{OV9724_8BIT, {0x034f},	0xe0},/* Image Height - Low */
	{OV9724_8BIT, {0x4908},	0x10},
	{OV9724_8BIT, {0x4909},	0x04},
	{OV9724_8BIT, {0x3811},	0x08},
	{OV9724_8BIT, {0x3813},	0x02},
	{OV9724_8BIT, {0x0340},	0x03},/* Vertical length - Hi */
	{OV9724_8BIT, {0x0341},	0x68},/* Vertical length - Low */
	{OV9724_8BIT, {0x0342},	0x05},/* Horizontal length - Hi */
	{OV9724_8BIT, {0x0343},	0x60},/* Horizontal length - Low */

	{OV9724_8BIT, {0x0301},	0x0a},
	{OV9724_8BIT, {0x0303},	0x02},
	{OV9724_8BIT, {0x0305},	0x02},/* Pre-pll divider -Low */
	{OV9724_8BIT, {0x0307},	0x4b},/* Pll multi -Low */
	{OV9724_8BIT, {0x0310},	0x00},

	{OV9724_8BIT, {0x0202},	0x01},/* Integration time - Hi */
	{OV9724_8BIT, {0x0203},	0x80},/* Integration time - Low */
	{OV9724_8BIT, {0x0205},	0x3f},

	{OV9724_8BIT, {0x0383},	0x01},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x0385},	0x01},
	{OV9724_8BIT, {0x0387},	0x01},
	{OV9724_8BIT, {0x3821},	0x00},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x3820},	0xa0},

	{OV9724_8BIT, {0x4801},	0x0f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4801},	0x8f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4814},	0x2b},/* Mipi ctrl14 */
	{OV9724_8BIT, {0x4307},	0x3a},
	{OV9724_8BIT, {0x370a},	0x23},

	{OV9724_8BIT, {0x5000},	0x06},/* Isp ctrl0 */
	{OV9724_8BIT, {0x5001},	0x73},/* Isp ctrl1 */
	{OV9724_TOK_TERM, {0}, 0}
};

static struct ov9724_reg const ov9724_D1NTSC_strong_dvs_30fps[] = {
	{OV9724_8BIT, {0x0344},	0x00},/* Horizontal start - Hi */
	{OV9724_8BIT, {0x0345},	0xae},/* Horizontal start - Low */
	{OV9724_8BIT, {0x0346},	0x00},/* Vertical start - Hi */
	{OV9724_8BIT, {0x0347},	0x44},/* Vertical start - Low */
	{OV9724_8BIT, {0x0348},	0x04},/* Horizontal end - Hi */
	{OV9724_8BIT, {0x0349},	0x9d},/* Horizontal end - Low */
	{OV9724_8BIT, {0x034a},	0x02},/* Vertical end - Hi */
	{OV9724_8BIT, {0x034b},	0xa6},/* Vertical end - Low */
	{OV9724_8BIT, {0x034c},	0x03},/* Image width - Hi */
	{OV9724_8BIT, {0x034d},	0x88},/* Image width - Low */
	{OV9724_8BIT, {0x034e},	0x02},/* Image Height - Hi */
	{OV9724_8BIT, {0x034f},	0x5a},/* Image Height - Low */
	{OV9724_8BIT, {0x4908},	0x14},
	{OV9724_8BIT, {0x4909},	0x08},
	{OV9724_8BIT, {0x3811},	0x0a},
	{OV9724_8BIT, {0x3813},	0x04},
	{OV9724_8BIT, {0x0340},	0x03},/* Vertical length - Hi */
	{OV9724_8BIT, {0x0341},	0x68},/* Vertical length - Low */
	{OV9724_8BIT, {0x0342},	0x05},/* Horizontal length - Hi */
	{OV9724_8BIT, {0x0343},	0x60},/* Horizontal length - Low */

	{OV9724_8BIT, {0x0301},	0x0a},
	{OV9724_8BIT, {0x0303},	0x02},
	{OV9724_8BIT, {0x0305},	0x02},/* Pre-pll divider -Low */
	{OV9724_8BIT, {0x0307},	0x4b},/* Pll multi -Low */
	{OV9724_8BIT, {0x0310},	0x00},

	{OV9724_8BIT, {0x0202},	0x01},/* Integration time - Hi */
	{OV9724_8BIT, {0x0203},	0x80},/* Integration time - Low */
	{OV9724_8BIT, {0x0205},	0x3f},

	{OV9724_8BIT, {0x0383},	0x01},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x0385},	0x01},
	{OV9724_8BIT, {0x0387},	0x01},
	{OV9724_8BIT, {0x3821},	0x00},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x3820},	0xa0},

	{OV9724_8BIT, {0x4801},	0x0f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4801},	0x8f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4814},	0x2b},/* Mipi ctrl14 */
	{OV9724_8BIT, {0x4307},	0x3a},
	{OV9724_8BIT, {0x370a},	0x23},

	{OV9724_8BIT, {0x5000},	0x06},/* Isp ctrl0 */
	{OV9724_8BIT, {0x5001},	0x73},/* Isp ctrl1 */
	{OV9724_TOK_TERM, {0}, 0}
};

static struct ov9724_reg const ov9724_D1PAL_strong_dvs_30fps[] = {
	{OV9724_8BIT, {0x0344},	0x00},/* Horizontal start - Hi */
	{OV9724_8BIT, {0x0345},	0xc8},/* Horizontal start - Low */
	{OV9724_8BIT, {0x0346},	0x00},/* Vertical start - Hi */
	{OV9724_8BIT, {0x0347},	0x06},/* Vertical start - Low */
	{OV9724_8BIT, {0x0348},	0x04},/* Horizontal end - Hi */
	{OV9724_8BIT, {0x0349},	0x37},/* Horizontal end - Low */
	{OV9724_8BIT, {0x034a},	0x02},/* Vertical end - Hi */
	{OV9724_8BIT, {0x034b},	0xc9},/* Vertical end - Low */
	{OV9724_8BIT, {0x034c},	0x03},/* Image width - Hi */
	{OV9724_8BIT, {0x034d},	0x70},/* Image width - Low */
	{OV9724_8BIT, {0x034e},	0x02},/* Image Height - Hi */
	{OV9724_8BIT, {0x034f},	0xc4},/* Image Height - Low */
	{OV9724_8BIT, {0x4908},	0x14},
	{OV9724_8BIT, {0x4909},	0x08},
	{OV9724_8BIT, {0x3811},	0x0a},
	{OV9724_8BIT, {0x3813},	0x04},
	{OV9724_8BIT, {0x0340},	0x03},/* Vertical length - Hi */
	{OV9724_8BIT, {0x0341},	0x68},/* Vertical length - Low */
	{OV9724_8BIT, {0x0342},	0x05},/* Horizontal length - Hi */
	{OV9724_8BIT, {0x0343},	0x60},/* Horizontal length - Low */

	{OV9724_8BIT, {0x0301},	0x0a},
	{OV9724_8BIT, {0x0303},	0x02},
	{OV9724_8BIT, {0x0305},	0x02},/* Pre-pll divider -Low */
	{OV9724_8BIT, {0x0307},	0x4b},/* Pll multi -Low */
	{OV9724_8BIT, {0x0310},	0x00},

	{OV9724_8BIT, {0x0202},	0x01},/* Integration time - Hi */
	{OV9724_8BIT, {0x0203},	0x80},/* Integration time - Low */
	{OV9724_8BIT, {0x0205},	0x3f},

	{OV9724_8BIT, {0x0383},	0x01},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x0385},	0x01},
	{OV9724_8BIT, {0x0387},	0x01},
	{OV9724_8BIT, {0x3821},	0x00},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x3820},	0xa0},

	{OV9724_8BIT, {0x4801},	0x0f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4801},	0x8f},/* Mipi ctrl01 */
	{OV9724_8BIT, {0x4814},	0x2b},/* Mipi ctrl14 */
	{OV9724_8BIT, {0x4307},	0x3a},
	{OV9724_8BIT, {0x370a},	0x23},

	{OV9724_8BIT, {0x5000},	0x06},/* Isp ctrl0 */
	{OV9724_8BIT, {0x5001},	0x73},/* Isp ctrl1 */
	{OV9724_TOK_TERM, {0}, 0}
};

static struct ov9724_reg const ov9724_VGA_strong_dvs_30fps[] = {
	{OV9724_8BIT, {0x0344},	0x00},/* Horizontal start - Hi */
	{OV9724_8BIT, {0x0345},	0xe6},/* Horizontal start - Low */
	{OV9724_8BIT, {0x0346},	0x00},/* Vertical start - Hi */
	{OV9724_8BIT, {0x0347},	0x34},/* Vertical start - Low */
	{OV9724_8BIT, {0x0348},	0x04},/* Horizontal end - Hi */
	{OV9724_8BIT, {0x0349},	0x55},/* Horizontal end - Low */
	{OV9724_8BIT, {0x034a},	0x02},/* Vertical end - Hi */
	{OV9724_8BIT, {0x034b},	0x9b},/* Vertical end - Low */
	{OV9724_8BIT, {0x034c},	0x03},/* Image width - Hi */
	{OV9724_8BIT, {0x034d},	0x34},/* Image width - Low */
	{OV9724_8BIT, {0x034e},	0x02},/* Image Height - Hi */
	{OV9724_8BIT, {0x034f},	0x68},/* Image Height - Low */
	{OV9724_8BIT, {0x4908},	0x14},
	{OV9724_8BIT, {0x4909},	0x08},
	{OV9724_8BIT, {0x3811},	0x0a},
	{OV9724_8BIT, {0x3813},	0x04},
	{OV9724_8BIT, {0x0340},	0x03},/* Vertical length - Hi */
	{OV9724_8BIT, {0x0341},	0x68},/* Vertical length - Low */
	{OV9724_8BIT, {0x0342},	0x05},/* Horizontal length - Hi */
	{OV9724_8BIT, {0x0343},	0x60},/* Horizontal length - Low */

	{OV9724_8BIT, {0x0301},	0x0a},
	{OV9724_8BIT, {0x0303},	0x02},
	{OV9724_8BIT, {0x0305},	0x02},/* Pre-pll divider -Low */
	{OV9724_8BIT, {0x0307},	0x4b},/* Pll multi -Low */
	{OV9724_8BIT, {0x0310},	0x00},

	{OV9724_8BIT, {0x0202},	0x01},/* Integration time - Hi */
	{OV9724_8BIT, {0x0203},	0x80},/* Integration time - Low */
	{OV9724_8BIT, {0x0205},	0x3f},

	{OV9724_8BIT, {0x0383},	0x01},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x0385},	0x01},
	{OV9724_8BIT, {0x0387},	0x01},
	{OV9724_8BIT, {0x3821},	0x00},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x3820},	0xa0},

	{OV9724_8BIT, {0x4801},	0x0f},
	{OV9724_8BIT, {0x4801},	0x8f},
	{OV9724_8BIT, {0x4814},	0x2b},
	{OV9724_8BIT, {0x4307},	0x3a},
	{OV9724_8BIT, {0x370a},	0x23},

	{OV9724_8BIT, {0x5000},	0x06},
	{OV9724_8BIT, {0x5001},	0x73},
	{OV9724_TOK_TERM, {0}, 0}
};

static struct ov9724_reg const ov9724_WIDE_PREVIEW_30fps[] = {
	{OV9724_8BIT, {0x0344},	0x00},/* Horizontal start - Hi */
	{OV9724_8BIT, {0x0345},	0x7e},/* Horizontal start - Low */
	{OV9724_8BIT, {0x0346},	0x00},/* Vertical start - Hi */
	{OV9724_8BIT, {0x0347},	0x44},/* Vertical start - Low */
	{OV9724_8BIT, {0x0348},	0x04},/* Horizontal end - Hi */
	{OV9724_8BIT, {0x0349},	0x8d},/* Horizontal end - Low */
	{OV9724_8BIT, {0x034a},	0x02},/* Vertical end - Hi */
	{OV9724_8BIT, {0x034b},	0x97},/* Vertical end - Low */
	{OV9724_8BIT, {0x034c},	0x04},/* Image width - Hi */
	{OV9724_8BIT, {0x034d},	0x10},/* Image width - Low */
	{OV9724_8BIT, {0x034e},	0x02},/* Image Height - Hi */
	{OV9724_8BIT, {0x034f},	0x54},/* Image Height - Low */
	{OV9724_8BIT, {0x4908},	0x14},
	{OV9724_8BIT, {0x4909},	0x08},
	{OV9724_8BIT, {0x3811},	0x0a},
	{OV9724_8BIT, {0x3813},	0x04},
	{OV9724_8BIT, {0x0340},	0x03},/* Vertical length - Hi */
	{OV9724_8BIT, {0x0341},	0x68},/* Vertical length - Low */
	{OV9724_8BIT, {0x0342},	0x05},/* Horizontal length - Hi */
	{OV9724_8BIT, {0x0343},	0x60},/* Horizontal length - Low */

	{OV9724_8BIT, {0x0301},	0x0a},
	{OV9724_8BIT, {0x0303},	0x02},
	{OV9724_8BIT, {0x0305},	0x02},/* Pre-pll divider -Low */
	{OV9724_8BIT, {0x0307},	0x4b},/* Pll multi -Low */
	{OV9724_8BIT, {0x0310},	0x00},

	{OV9724_8BIT, {0x0202},	0x01},/* Integration time - Hi */
	{OV9724_8BIT, {0x0203},	0x80},/* Integration time - Low */
	{OV9724_8BIT, {0x0205},	0x3f},

	{OV9724_8BIT, {0x0383},	0x01},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x0385},	0x01},
	{OV9724_8BIT, {0x0387},	0x01},
	{OV9724_8BIT, {0x3821},	0x00},
	{OV9724_8BIT, {0x4501},	0x08},
	{OV9724_8BIT, {0x3820},	0xa0},

	{OV9724_8BIT, {0x4801},	0x0f},
	{OV9724_8BIT, {0x4801},	0x8f},
	{OV9724_8BIT, {0x4814},	0x2b},
	{OV9724_8BIT, {0x4307},	0x3a},
	{OV9724_8BIT, {0x370a},	0x23},

	{OV9724_8BIT, {0x5000},	0x06},
	{OV9724_8BIT, {0x5001},	0x73},
	{OV9724_TOK_TERM, {0}, 0}
};


static struct ov9724_reg const ov9724_QVGA_strong_dvs_30fps[] = {
	{OV9724_8BIT, {0x0344},	0x00},/* Horizontal start - Hi */
	{OV9724_8BIT, {0x0345},	0x00},/* Horizontal start - Low */
	{OV9724_8BIT, {0x0346},	0x00},/* Vertical start - Hi */
	{OV9724_8BIT, {0x0347},	0x00},/* Vertical start - Low */
	{OV9724_8BIT, {0x0348},	0x04},/* Horizontal end - Hi */
	{OV9724_8BIT, {0x0349},	0xff},/* Horizontal end - Low */
	{OV9724_8BIT, {0x034a},	0x02},/* Vertical end - Hi */
	{OV9724_8BIT, {0x034b},	0xcf},/* Vertical end - Low */
	{OV9724_8BIT, {0x034c},	0x01},/* Image width - Hi */
	{OV9724_8BIT, {0x034d},	0x98},/* Image width - Low */
	{OV9724_8BIT, {0x034e},	0x01},/* Image Height - Hi */
	{OV9724_8BIT, {0x034f},	0x34},/* Image Height - Low */
	{OV9724_8BIT, {0x4908},	0x20},
	{OV9724_8BIT, {0x4909},	0x14},
	{OV9724_8BIT, {0x3811},	0xf8},
	{OV9724_8BIT, {0x3813},	0x3e},
	{OV9724_8BIT, {0x0340},	0x03},/* Vertical length - Hi */
	{OV9724_8BIT, {0x0341},	0x68},/* Vertical length - Low */
	{OV9724_8BIT, {0x0342},	0x05},/* Horizontal length - Hi */
	{OV9724_8BIT, {0x0343},	0x60},/* Horizontal length - Low */

	{OV9724_8BIT, {0x0301},	0x0a},
	{OV9724_8BIT, {0x0303},	0x02},
	{OV9724_8BIT, {0x0305},	0x02},/* Pre-pll divider -Low */
	{OV9724_8BIT, {0x0307},	0x4b},/* Pll multi -Low */
	{OV9724_8BIT, {0x0310},	0x00},

	{OV9724_8BIT, {0x0202},	0x01},/* Integration time - Hi */
	{OV9724_8BIT, {0x0203},	0x80},/* Integration time - Low */
	{OV9724_8BIT, {0x0205},	0x3f},

	{OV9724_8BIT, {0x0383},	0x03},
	{OV9724_8BIT, {0x4501},	0x09},
	{OV9724_8BIT, {0x0385},	0x01},
	{OV9724_8BIT, {0x0387},	0x03},
	{OV9724_8BIT, {0x3821},	0x01},
	{OV9724_8BIT, {0x4501},	0x09},
	{OV9724_8BIT, {0x3820},	0xa1},

	{OV9724_8BIT, {0x4801},	0x0f},
	{OV9724_8BIT, {0x4801},	0x8f},
	{OV9724_8BIT, {0x4814},	0x2b},
	{OV9724_8BIT, {0x4307},	0x3a},
	{OV9724_8BIT, {0x370a},	0x23},

	{OV9724_8BIT, {0x5000},	0x06},
	{OV9724_8BIT, {0x5001},	0x73},
	{OV9724_TOK_TERM, {0}, 0}
};

static struct ov9724_reg const ov9724_QCIF_strong_dvs_30fps[] = {
	{OV9724_8BIT, {0x0344},	0x00},/* Horizontal start - Hi */
	{OV9724_8BIT, {0x0345},	0x00},/* Horizontal start - Low */
	{OV9724_8BIT, {0x0346},	0x00},/* Vertical start - Hi */
	{OV9724_8BIT, {0x0347},	0x00},/* Vertical start - Low */
	{OV9724_8BIT, {0x0348},	0x04},/* Horizontal end - Hi */
	{OV9724_8BIT, {0x0349},	0xff},/* Horizontal end - Low */
	{OV9724_8BIT, {0x034a},	0x02},/* Vertical end - Hi */
	{OV9724_8BIT, {0x034b},	0xcf},/* Vertical end - Low */
	{OV9724_8BIT, {0x034c},	0x00},/* Image width - Hi */
	{OV9724_8BIT, {0x034d},	0xd8},/* Image width - Low */
	{OV9724_8BIT, {0x034e},	0x00},/* Image Height - Hi */
	{OV9724_8BIT, {0x034f},	0xb0},/* Image Height - Low */
	{OV9724_8BIT, {0x4908},	0x20},
	{OV9724_8BIT, {0x4909},	0x14},
	{OV9724_8BIT, {0x3811},	0xe0},
	{OV9724_8BIT, {0x3813},	0x12},
	{OV9724_8BIT, {0x0340},	0x03},/* Vertical length - Hi */
	{OV9724_8BIT, {0x0341},	0x68},/* Vertical length - Low */
	{OV9724_8BIT, {0x0342},	0x05},/* Horizontal length - Hi */
	{OV9724_8BIT, {0x0343},	0x60},/* Horizontal length - Low */


	{OV9724_8BIT, {0x0301},	0x0a},
	{OV9724_8BIT, {0x0303},	0x02},
	{OV9724_8BIT, {0x0305},	0x02},/* Pre-pll divider -Low */
	{OV9724_8BIT, {0x0307},	0x4b},/* Pll multi -Low */
	{OV9724_8BIT, {0x0310},	0x00},


	{OV9724_8BIT, {0x0202},	0x01},/* Integration time - Hi */
	{OV9724_8BIT, {0x0203},	0x80},/* Integration time - Low */
	{OV9724_8BIT, {0x0205},	0x3f},


	{OV9724_8BIT, {0x0383},	0x07},
	{OV9724_8BIT, {0x4501},	0x09},
	{OV9724_8BIT, {0x0385},	0x05},
	{OV9724_8BIT, {0x0387},	0x03},
	{OV9724_8BIT, {0x3821},	0x01},
	{OV9724_8BIT, {0x4501},	0x09},
	{OV9724_8BIT, {0x3820},	0xa1},


	{OV9724_8BIT, {0x4801},	0x0f},
	{OV9724_8BIT, {0x4801},	0x8f},
	{OV9724_8BIT, {0x4814},	0x2b},
	{OV9724_8BIT, {0x4307},	0x3a},
	{OV9724_8BIT, {0x370a},	0xa2},


	{OV9724_8BIT, {0x5000},	0x06},
	{OV9724_8BIT, {0x5001},	0x73},
	{OV9724_TOK_TERM, {0}, 0}
};

static struct ov9724_reg const ov9724_LOW_30fps[] = {
	{OV9724_8BIT, {0x0344},	0x00},/* Horizontal start - Hi */
	{OV9724_8BIT, {0x0345},	0x00},/* Horizontal start - Low */
	{OV9724_8BIT, {0x0346},	0x00},/* Vertical start - Hi */
	{OV9724_8BIT, {0x0347},	0x00},/* Vertical start - Low */
	{OV9724_8BIT, {0x0348},	0x04},/* Horizontal end - Hi */
	{OV9724_8BIT, {0x0349},	0xff},/* Horizontal end - Low */
	{OV9724_8BIT, {0x034a},	0x02},/* Vertical end - Hi */
	{OV9724_8BIT, {0x034b},	0xcf},/* Vertical end - Low */
	{OV9724_8BIT, {0x034c},	0x00},/* Image width - Hi */
	{OV9724_8BIT, {0x034d},	0xd0},/* Image width - Low */
	{OV9724_8BIT, {0x034e},	0x00},/* Image Height - Hi */
	{OV9724_8BIT, {0x034f},	0xa0},/* Image Height - Low */
	{OV9724_8BIT, {0x4908},	0x20},
	{OV9724_8BIT, {0x4909},	0x14},
	{OV9724_8BIT, {0x3811},	0xe0},
	{OV9724_8BIT, {0x3813},	0x12},
	{OV9724_8BIT, {0x0340},	0x03},/* Vertical length - Hi */
	{OV9724_8BIT, {0x0341},	0x68},/* Vertical length - Low */
	{OV9724_8BIT, {0x0342},	0x05},/* Horizontal length - Hi */
	{OV9724_8BIT, {0x0343},	0x60},/* Horizontal length - Low */

	{OV9724_8BIT, {0x0301},	0x0a},
	{OV9724_8BIT, {0x0303},	0x02},
	{OV9724_8BIT, {0x0305},	0x02},/* Pre-pll divider -Low */
	{OV9724_8BIT, {0x0307},	0x4b},/* Pll multi -Low */
	{OV9724_8BIT, {0x0310},	0x00},

	{OV9724_8BIT, {0x0202},	0x01},/* Integration time - Hi */
	{OV9724_8BIT, {0x0203},	0x80},/* Integration time - Low */
	{OV9724_8BIT, {0x0205},	0x3f},

	{OV9724_8BIT, {0x0383},	0x07},
	{OV9724_8BIT, {0x4501},	0x09},
	{OV9724_8BIT, {0x0385},	0x05},
	{OV9724_8BIT, {0x0387},	0x03},
	{OV9724_8BIT, {0x3821},	0x01},
	{OV9724_8BIT, {0x4501},	0x09},
	{OV9724_8BIT, {0x3820},	0xa1},

	{OV9724_8BIT, {0x4801},	0x0f},
	{OV9724_8BIT, {0x4801},	0x8f},
	{OV9724_8BIT, {0x4814},	0x2b},
	{OV9724_8BIT, {0x4307},	0x3a},
	{OV9724_8BIT, {0x370a},	0xa2},

	{OV9724_8BIT, {0x5000},	0x06},
	{OV9724_8BIT, {0x5001},	0x73},
	{OV9724_TOK_TERM, {0}, 0}
};

static struct ov9724_reg const ov9724_QCIF_30fps[] = {
	{OV9724_8BIT, {0x0344},	0x00},/* Horizontal start - Hi */
	{OV9724_8BIT, {0x0345},	0x20},/* Horizontal start - Low */
	{OV9724_8BIT, {0x0346},	0x00},/* Vertical start - Hi */
	{OV9724_8BIT, {0x0347},	0x00},/* Vertical start - Low */
	{OV9724_8BIT, {0x0348},	0x04},/* Horizontal end - Hi */
	{OV9724_8BIT, {0x0349},	0xff},/* Horizontal end - Low */
	{OV9724_8BIT, {0x034a},	0x02},/* Vertical end - Hi */
	{OV9724_8BIT, {0x034b},	0xcf},/* Vertical end - Low */
	{OV9724_8BIT, {0x034c},	0x00},/* Image width - Hi */
	{OV9724_8BIT, {0x034d},	0xc0},/* Image width - Low */
	{OV9724_8BIT, {0x034e},	0x00},/* Image Height - Hi */
	{OV9724_8BIT, {0x034f},	0xa0},/* Image Height - Low */
	{OV9724_8BIT, {0x4908},	0x20},
	{OV9724_8BIT, {0x4909},	0x14},
	{OV9724_8BIT, {0x3811},	0xe0},
	{OV9724_8BIT, {0x3813},	0x12},
	{OV9724_8BIT, {0x0340},	0x03},/* Vertical length - Hi */
	{OV9724_8BIT, {0x0341},	0x68},/* Vertical length - Low */
	{OV9724_8BIT, {0x0342},	0x05},/* Horizontal length - Hi */
	{OV9724_8BIT, {0x0343},	0x60},/* Horizontal length - Low */

	{OV9724_8BIT, {0x0301},	0x0a},
	{OV9724_8BIT, {0x0303},	0x02},
	{OV9724_8BIT, {0x0305},	0x02},/* Pre-pll divider -Low */
	{OV9724_8BIT, {0x0307},	0x4b},/* Pll multi -Low */
	{OV9724_8BIT, {0x0310},	0x00},

	{OV9724_8BIT, {0x0202},	0x01},/* Integration time - Hi */
	{OV9724_8BIT, {0x0203},	0x80},/* Integration time - Low */
	{OV9724_8BIT, {0x0205},	0x3f},

	{OV9724_8BIT, {0x0383},	0x07},
	{OV9724_8BIT, {0x4501},	0x09},
	{OV9724_8BIT, {0x0385},	0x05},
	{OV9724_8BIT, {0x0387},	0x03},
	{OV9724_8BIT, {0x3821},	0x01},
	{OV9724_8BIT, {0x4501},	0x09},
	{OV9724_8BIT, {0x3820},	0xa1},

	{OV9724_8BIT, {0x4801},	0x0f},
	{OV9724_8BIT, {0x4801},	0x8f},
	{OV9724_8BIT, {0x4814},	0x2b},
	{OV9724_8BIT, {0x4307},	0x3a},
	{OV9724_8BIT, {0x370a},	0xa2},

	{OV9724_8BIT, {0x5000},	0x06},
	{OV9724_8BIT, {0x5001},	0x73},
	{OV9724_TOK_TERM, {0}, 0}
};

/* TODO settings of preview/still/video will be updated with new use case */
struct ov9724_resolution ov9724_res_preview[] = {
	{
		.desc = "ov9724_960x720_30fps",
		.regs = ov9724_960x720_30fps,
		.width = 976,
		.height = 736,
		.fps = 30,
		.pixels_per_line = 0x0560, /* consistent with regs arrays */
		.lines_per_frame = 0x0368, /* consistent with regs arrays */
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
		.skip_frames = 0,
	},
	{
		.desc = "ov9724_720p_30fps",
		.regs = ov9724_720p_30fps,
		.width = 1296,
		.height = 736,
		.fps = 30,
		.pixels_per_line = 0x0560, /* consistent with regs arrays */
		.lines_per_frame = 0x0368, /* consistent with regs arrays */
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
		.skip_frames = 0,
	},

};
#define N_RES_PREVIEW (ARRAY_SIZE(ov9724_res_preview))

struct ov9724_resolution ov9724_res_still[] = {
	{
		.desc = "ov9724_960x720_30fps",
		.regs = ov9724_960x720_30fps,
		.width = 976,
		.height = 736,
		.fps = 30,
		.pixels_per_line = 0x0560, /* consistent with regs arrays */
		.lines_per_frame = 0x0368, /* consistent with regs arrays */
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
		.skip_frames = 0,
	},
	{
		.desc = "ov9724_720p_30fps",
		.regs = ov9724_720p_30fps,
		.width = 1296,
		.height = 736,
		.fps = 30,
		.pixels_per_line = 0x0560, /* consistent with regs arrays */
		.lines_per_frame = 0x0368, /* consistent with regs arrays */
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
		.skip_frames = 0,
	},
};
#define N_RES_STILL (ARRAY_SIZE(ov9724_res_still))

struct ov9724_resolution ov9724_res_video[] = {
	{
		.desc = "QCIF_30fps",
		.regs = ov9724_QCIF_30fps,
		.width = 192,
		.height = 160,
		.fps = 30,
		.pixels_per_line = 0x0560, /* consistent with regs arrays */
		.lines_per_frame = 0x0368, /* consistent with regs arrays */
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.used = 0,
		.skip_frames = 0,
	},
	{
		.desc = "VGA_strong_dvs_30fps",
		.regs = ov9724_336_256_30fps,
		.width = 336,
		.height = 256,
		.fps = 30,
		.pixels_per_line = 0x0560, /* consistent with regs arrays */
		.lines_per_frame = 0x0368, /* consistent with regs arrays */
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
		.skip_frames = 0,
	},
	{
		.desc = "VGA_strong_dvs_30fps",
		.regs = ov9724_368_304_30fps,
		.width = 368,
		.height = 304,
		.fps = 30,
		.pixels_per_line = 0x0560, /* consistent with regs arrays */
		.lines_per_frame = 0x0368, /* consistent with regs arrays */
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
		.skip_frames = 0,
	},
	{
		.desc = "VGA_strong_dvs_30fps",
		.regs = ov9724_656_496_30fps,
		.width = 656,
		.height = 496,
		.fps = 30,
		.pixels_per_line = 0x0560, /* consistent with regs arrays */
		.lines_per_frame = 0x0368, /* consistent with regs arrays */
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
		.skip_frames = 0,
	},
	{
		.desc = "VGA_strong_dvs_30fps",
		.regs = ov9724_736_496_30fps,
		.width = 736,
		.height = 496,
		.fps = 30,
		.pixels_per_line = 0x0560, /* consistent with regs arrays */
		.lines_per_frame = 0x0368, /* consistent with regs arrays */
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
		.skip_frames = 0,
	},
	{
		.desc = "ov9724_720p_30fps",
		.regs = ov9724_720p_30fps,
		.width = 1296,
		.height =	736,
		.fps = 30,
		.pixels_per_line = 0x0560, /* consistent with regs arrays */
		.lines_per_frame = 0x0368, /* consistent with regs arrays */
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
		.skip_frames = 0,
	},
};
#define N_RES_VIDEO (ARRAY_SIZE(ov9724_res_video))

struct ov9724_resolution *ov9724_res = ov9724_res_preview;
static int N_RES = N_RES_PREVIEW;


static struct ov9724_reg const ov9724_suspend[] = {
	 {OV9724_8BIT,  {0x0100}, 0x0},
	 {OV9724_TOK_TERM, {0}, 0}
};

static struct ov9724_reg const ov9724_streaming[] = {
	 {OV9724_8BIT,  {0x0100}, 0x1},
	 {OV9724_TOK_TERM, {0}, 0}
};

static struct ov9724_reg const ov9724_param_hold[] = {
	{OV9724_8BIT,  {0x0104}, 0x1},	/* GROUPED_PARAMETER_HOLD */
	{OV9724_TOK_TERM, {0}, 0}
};

static struct ov9724_reg const ov9724_param_update[] = {
	{OV9724_8BIT,  {0x0104}, 0x0},	/* GROUPED_PARAMETER_HOLD */
	{OV9724_TOK_TERM, {0}, 0}
};

/* init settings */
static struct ov9724_reg const ov9724_init_config[] = {
	{OV9724_8BIT, {0x0103},	0x01},/* Soft reset */
	{OV9724_8BIT, {0x3210},	0x43},
	{OV9724_8BIT, {0x3606},	0x75},
	{OV9724_8BIT, {0x3705},	0x41},
	{OV9724_8BIT, {0x3601},	0x34},
	{OV9724_8BIT, {0x3607},	0x94},
	{OV9724_8BIT, {0x3608},	0x20},
	{OV9724_TOK_TERM, {0}, 0}
};
#endif
