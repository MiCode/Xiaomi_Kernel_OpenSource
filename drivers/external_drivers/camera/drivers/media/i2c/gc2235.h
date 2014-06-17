/*
 * Support for Sony GC2235 camera sensor.
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

#ifndef __GC2235_H__
#define __GC2235_H__
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

#define I2C_MSG_LENGTH		0x2

#define GC2235_MCLK		192

/* TODO - This should be added into include/linux/videodev2.h */
#ifndef V4L2_IDENT_GC
#define V4L2_IDENT_GC	2235
#endif

/* #defines for register writes and register array processing */
#define GCSENSOR_8BIT		1
#define GCSENSOR_16BIT		2
#define GCSENSOR_32BIT		4

/*
 * gc2235 System control registers
 */
#define GC2235_MASK_5BIT	0x1F
#define GC2235_MASK_4BIT	0xF
#define GC2235_MASK_2BIT	0x3
#define GC2235_INTG_BUF_COUNT		2

#define GC2235_FINE_INTG_TIME		0x0

#define GC2235_ID_DEFAULT 0

#define GC2235_LINES_PER_FRAME		0x0d
#define GC2235_PIXELS_PER_LINE		0x0f

#define GC2235_HORIZONTAL_START_H 0x0b
#define GC2235_HORIZONTAL_START_L 0x0c

#define GC2235_VERTICAL_START_H 0x09
#define GC2235_VERTICAL_START_L 0x0a
#define REG_SH_DELAY_H 0x11
#define REG_SH_DELAY_L 0x12

#define REG_HORI_BLANKING_H 0x05
#define REG_HORI_BLANKING_L 0x06

#define REG_VERT_DUMMY_H 0x07
#define REG_VERT_DUMMY_L 0x08

#define GC2235_HORIZONTAL_OUTPUT_SIZE_H 0x0f
#define GC2235_HORIZONTAL_OUTPUT_SIZE_L 0x10
#define GC2235_VERTICAL_OUTPUT_SIZE_H 0x0d
#define GC2235_VERTICAL_OUTPUT_SIZE_L 0x0e

#define GC2235_IMG_ORIENTATION			0x17
#define GC2235_VFLIP_BIT			2
#define GC2235_HFLIP_BIT			1
#define GC2235_GLOBAL_GAIN			0xb0

#define GC2235_DGC_LEN		10
#define GC2235_MAX_EXPOSURE_SUPPORTED 8191
#define GC2235_MAX_GLOBAL_GAIN_SUPPORTED 0xffff
#define GC2235_MAX_DIGITAL_GAIN_SUPPORTED 0xffff
#define GC2235_REG_EXPO_COARSE                 0x03

/* Defines for register writes and register array processing */
#define GC2235_BYTE_MAX	32 /* change to 32 as needed by otpdata */
#define GC2235_SHORT_MAX	16
#define I2C_RETRY_COUNT		5
#define GC2235_TOK_MASK	0xfff0

#define GC2235_FOCAL_LENGTH_NUM	208	/*2.08mm*/
#define GC2235_FOCAL_LENGTH_DEM	100
#define GC2235_F_NUMBER_DEFAULT_NUM	24
#define GC2235_F_NUMBER_DEM	10
#define GC2235_WAIT_STAT_TIMEOUT	100
#define GC2235_FLICKER_MODE_50HZ	1
#define GC2235_FLICKER_MODE_60HZ	2

/* Defines for OTP Data Registers */
#define GC2235_OTP_START_ADDR		0x3B04
#define GC2235_OTP_DATA_SIZE		1280
#define GC2235_OTP_PAGE_SIZE		64
#define GC2235_OTP_READY_REG		0x3B01
#define GC2235_OTP_PAGE_REG		0x3B02
#define GC2235_OTP_MODE_REG		0x3B00
#define GC2235_OTP_PAGE_MAX		20
#define GC2235_OTP_READY_REG_DONE		1
#define GC2235_OTP_READ_ONETIME		32
#define GC2235_OTP_MODE_READ		1

#define MAX_FMTS 1

#define GC2235_SUBDEV_PREFIX "gc"
#define GC2235_DRIVER	"gc22351x5"
#define GC2235_NAME	"gc2235"
#define GC2235_ID	0x2235

#define GC2235_REG_SENSOR_ID_HIGH_BIT	0xf0
#define GC2235_REG_SENSOR_ID_LOW_BIT	0xf1
#define GC2235_SENSOR_ID_HIGH_BIT	0x22
#define GC2235_SENSOR_ID_LOW_BIT	0x35

#define GC2235_RES_WIDTH_MAX	1616
#define GC2235_RES_HEIGHT_MAX	1216

#define GC2235_BIN_FACTOR_MAX			4
#define GC2235_INTEGRATION_TIME_MARGIN	4
/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define GC2235_FOCAL_LENGTH_DEFAULT 0x1710064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define GC2235_F_NUMBER_DEFAULT 0x16000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define GC2235_F_NUMBER_RANGE 0x160a160a

struct max_res {
	int res_max_width;
	int res_max_height;
};

struct max_res gc2235_max_res[] = {
	[0] = {
		.res_max_width = GC2235_RES_WIDTH_MAX,
		.res_max_height = GC2235_RES_HEIGHT_MAX,
	},
};

#define MAX_FPS_OPTIONS_SUPPORTED       3

enum gc2235_tok_type {
	 GC2235_8BIT  = 0x0001,
	 GC2235_16BIT = 0x0002,
	 GC2235_TOK_TERM   = 0xf000,        /* terminating token for reg list */
	 GC2235_TOK_DELAY  = 0xfe00 /* delay token for reg list */
};

#define GROUPED_PARAMETER_HOLD_ENABLE  {GC2235_8BIT, 0x0104, 0x1}
#define GROUPED_PARAMETER_HOLD_DISABLE  {GC2235_8BIT, 0x0104, 0x0}

/**
 * struct gc2235_reg - MI sensor  register format
 * @type: type of the register
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 *
 * Define a structure for sensor register initialization values
 */
struct gc2235_reg {
	 enum gc2235_tok_type type;
	 u16 sreg;
	 u32 val;        /* @set value for read/mod/write, @mask */
};

struct gc2235_fps_setting {
	 int fps;
	 unsigned short pixels_per_line;
	 unsigned short lines_per_frame;
};

struct gc2235_resolution {
	 u8 *desc;
	 const struct gc2235_reg *regs;
	 int res;
	 int width;
	 int height;
	 int fps;
	 unsigned short pixels_per_line;
	 unsigned short lines_per_frame;
	 unsigned short skip_frames;
	 u8 bin_factor_x;
	 u8 bin_factor_y;
	 bool used;
		u8 bin_mode;
};



struct gc2235_settings {
	struct gc2235_reg const *init_settings;
	struct gc2235_resolution *res_preview;
	struct gc2235_resolution *res_still;
	struct gc2235_resolution *res_video;
	int n_res_preview;
	int n_res_still;
	int n_res_video;
};

static struct gc2235_reg const gc2235_720p_30fps[] = {
	{GC2235_8BIT, 0x90, 0x01},
	{GC2235_8BIT, 0x92, 0xf0},
	{GC2235_8BIT, 0x94, 0xa0},
	{GC2235_8BIT, 0x95, 0x02},
	{GC2235_8BIT, 0x96, 0xe0},
	{GC2235_8BIT, 0x97, 0x05},
	{GC2235_8BIT, 0x98, 0x10},

	{GC2235_8BIT, 0xfe, 0x03},
	{GC2235_8BIT, 0x12, 0x54},
	{GC2235_8BIT, 0x13, 0x06},
	{GC2235_8BIT, 0x04, 0x20},
	{GC2235_8BIT, 0x05, 0x00},
	{GC2235_8BIT, 0xfe, 0x00},
	{GC2235_TOK_TERM, 0, 0},
};

static struct gc2235_reg const gc2235_1600x1200_30fps[] = {
	{GC2235_8BIT, 0xfe, 0x00},
	{GC2235_8BIT, 0x0a, 0x02},
	{GC2235_8BIT, 0x0c, 0x00},
	{GC2235_8BIT, 0x0d, 0x04},
	{GC2235_8BIT, 0x0e, 0xd0},
	{GC2235_8BIT, 0x0f, 0x06},
	{GC2235_8BIT, 0x10, 0x58},

	{GC2235_8BIT, 0x90, 0x01},
	{GC2235_8BIT, 0x92, 0x02},
	{GC2235_8BIT, 0x94, 0x00},
	{GC2235_8BIT, 0x95, 0x04},
	{GC2235_8BIT, 0x96, 0xc0},
	{GC2235_8BIT, 0x97, 0x06},
	{GC2235_8BIT, 0x98, 0x50},

	{GC2235_8BIT, 0xfe, 0x03},
	{GC2235_8BIT, 0x12, 0xe4},
	{GC2235_8BIT, 0x13, 0x07},
	{GC2235_8BIT, 0x04, 0x20},
	{GC2235_8BIT, 0x05, 0x00},
	{GC2235_8BIT, 0xfe, 0x00},
	{GC2235_TOK_TERM, 0, 0},
};

static struct gc2235_reg const gc2235_still_1600x1200_30fps[] = {
	{GC2235_8BIT, 0xfe, 0x00},
	{GC2235_8BIT, 0x0a, 0x02},
	{GC2235_8BIT, 0x0c, 0x00},
	{GC2235_8BIT, 0x0d, 0x04},
	{GC2235_8BIT, 0x0e, 0xd0},
	{GC2235_8BIT, 0x0f, 0x06},
	{GC2235_8BIT, 0x10, 0x58},

	{GC2235_8BIT, 0x90, 0x01},
	{GC2235_8BIT, 0x92, 0x02},
	{GC2235_8BIT, 0x94, 0x00},
	{GC2235_8BIT, 0x95, 0x04},
	{GC2235_8BIT, 0x96, 0xc0},
	{GC2235_8BIT, 0x97, 0x06},
	{GC2235_8BIT, 0x98, 0x50},

	{GC2235_8BIT, 0xfe, 0x03},
	{GC2235_8BIT, 0x12, 0xe4},
	{GC2235_8BIT, 0x13, 0x07},
	{GC2235_8BIT, 0x04, 0x20},
	{GC2235_8BIT, 0x05, 0x00},
	{GC2235_8BIT, 0xfe, 0x00},
	{GC2235_TOK_TERM, 0, 0},
};


/* TODO settings of preview/still/video will be updated with new use case */

struct gc2235_resolution gc2235_res_still[] = {
	{
		.desc = "gc2235_1600x1200_30fps",
		.regs = gc2235_still_1600x1200_30fps,
		.width = 1616,
		.height = 1216,
		.fps = 23,
		.pixels_per_line = 0x8c0,
		.lines_per_frame = 0x500,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.used = 0,
		.skip_frames = 2,
		.bin_mode = 0,
	},
};

static struct gc2235_reg const gc2235_1280_30fps[] = {
	{GC2235_8BIT, 0xfe, 0x00},
	{GC2235_8BIT, 0x0a, 0x98},
	{GC2235_8BIT, 0x0c, 0x00},
	{GC2235_8BIT, 0x0d, 0x03},
	{GC2235_8BIT, 0x0e, 0xa4},
	{GC2235_8BIT, 0x0f, 0x06},
	{GC2235_8BIT, 0x10, 0x50},

	{GC2235_8BIT, 0x90, 0x01},
	{GC2235_8BIT, 0x92, 0x02},
	{GC2235_8BIT, 0x94, 0x00},
	{GC2235_8BIT, 0x95, 0x03},
	{GC2235_8BIT, 0x96, 0x94},
	{GC2235_8BIT, 0x97, 0x06},
	{GC2235_8BIT, 0x98, 0x50},
	{GC2235_TOK_TERM, 0, 0},
};

struct gc2235_resolution gc2235_res_preview[] = {
	{
		.desc = "gc2235_1600x1200_30fps",
		.regs = gc2235_1600x1200_30fps,
		.width = 1616,
		.height = 1216,
		.fps = 23,
		.pixels_per_line = 0x8c0,
		.lines_per_frame = 0x500,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.used = 0,
		.skip_frames = 2,
		.bin_mode = 0,
	},
};

struct gc2235_resolution gc2235_res_video[] = {
	{
		.desc = "gc2235_1280_30fps",
		.regs = gc2235_1280_30fps,
		.width = 1616,
		.height = 916,
		.fps = 30,
		.pixels_per_line = 0x8c0,
		.lines_per_frame = 0x3c4,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.used = 0,
		.skip_frames = 4,
	},
};

/********************** settings for imx - reference *********************/
static struct gc2235_reg const gc2235_init_settings[] = {
	{ GC2235_TOK_TERM, 0, 0}
};

struct gc2235_settings gc2235_sets[] = {
	[0] = {
		.init_settings = gc2235_init_settings,
		.res_preview = gc2235_res_preview,
		.res_still = gc2235_res_still,
		.res_video = gc2235_res_video,
		.n_res_preview = ARRAY_SIZE(gc2235_res_preview),
		.n_res_still = ARRAY_SIZE(gc2235_res_still),
		.n_res_video = ARRAY_SIZE(gc2235_res_video),
	},
};

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


struct gc2235_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, s32 value);
};

/* gc2235 device structure */
struct gc2235_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;
	struct camera_sensor_platform_data *platform_data;
	struct mutex input_lock; /* serialize sensor's ioctl */
	int fmt_idx;
	int status;
	int streaming;
	int power;
	int once_launched;
	int run_mode;
	int vt_pix_clk_freq_mhz;
	int fps_index;
	u32 focus;
	u16 sensor_id;
	u16 coarse_itg;
	u16 fine_itg;
	u16 gain;
	u16 pixels_per_line;
	u16 lines_per_frame;
	u8 fps;
	u8 res;
	u8 type;
	u8 *otp_data;
	struct gc2235_settings *mode_tables;
	const struct gc2235_resolution *curr_res_table;
	int entries_curr_table;
};

#define to_gc2235_sensor(x) container_of(x, struct gc2235_device, sd)

#define GC2235_MAX_WRITE_BUF_SIZE	32
struct gc2235_write_buffer {
	u16 addr;
	u8 data[GC2235_MAX_WRITE_BUF_SIZE];
};

struct gc2235_write_ctrl {
	int index;
	struct gc2235_write_buffer buffer;
};

static const struct gc2235_reg gc2235_param_update[] = {
	{GC2235_TOK_TERM, 0, 0}
};
#endif


