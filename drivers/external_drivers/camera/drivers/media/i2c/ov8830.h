/*
 * Support for Omnivision OV8830 camera sensor.
 * Based on Aptina mt9e013 driver.
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

#ifndef __OV8830_H__
#define __OV8830_H__
#include <linux/atomisp_platform.h>
#include <linux/atomisp.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include <linux/types.h>

#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#define to_drv201_device(_sd) (&(container_of(_sd, struct ov8830_device, sd) \
				 ->drv201))

#define DRV201_I2C_ADDR				0x0E
#define DRV201_CONTROL				2
#define DRV201_VCM_CURRENT			3
#define DRV201_STATUS				5
#define DRV201_MODE				6
#define DRV201_VCM_FREQ				7

#define DRV201_DEFAULT_VCM_FREQ			0xe6
#define DRV201_MIN_DEFAULT_VCM_FREQ		0x00
#define DRV201_MAX_DEFAULT_VCM_FREQ		0xff

#define DRV201_MAX_FOCUS_POS			1023
#define DRV201_MODE_LINEAR			2

/* drv201 device structure */
struct drv201_device {
	bool initialized;		/* true if drv201 is detected */
	s32 focus;			/* Current focus value */
	struct timespec focus_time;	/* Time when focus was last time set */
	__u8 buffer[4];			/* Used for i2c transactions */
	const struct camera_af_platform_data *platform_data;
};

#define	OV8830_NAME	"ov8830"
#define	OV8830_ADDR	0x36
#define OV8830_ID	0x4b00

#define OV8830_CHIP_ID	0x8830
#define OV8835_CHIP_ID	0x8835

#define	LAST_REG_SETING		{0xffff, 0xff}
#define	is_last_reg_setting(item) ((item).reg == 0xffff)
#define I2C_MSG_LENGTH		0x2

#define OV8830_INVALID_CONFIG	0xffffffff

#define OV8830_INTG_UNIT_US	100
#define OV8830_MCLK		192

#define OV8830_REG_BITS	16
#define OV8830_REG_MASK	0xFFFF

/* This should be added into include/linux/videodev2.h */
#ifndef V4L2_IDENT_OV8830
#define V4L2_IDENT_OV8830	8245
#endif

/*
 * ov8830 System control registers
 */
#define OV8830_PLL_PLL10			0x3090
#define OV8830_PLL_PLL11			0x3091
#define OV8830_PLL_PLL12			0x3092
#define OV8830_PLL_PLL13			0x3093
#define OV8830_TIMING_VTS			0x380e
#define OV8830_TIMING_HTS			0x380C

#define OV8830_HORIZONTAL_START_H		0x3800
#define OV8830_HORIZONTAL_START_L		0x3801
#define OV8830_VERTICAL_START_H			0x3802
#define OV8830_VERTICAL_START_L			0x3803
#define OV8830_HORIZONTAL_END_H			0x3804
#define OV8830_HORIZONTAL_END_L			0x3805
#define OV8830_VERTICAL_END_H			0x3806
#define OV8830_VERTICAL_END_L			0x3807
#define OV8830_HORIZONTAL_OUTPUT_SIZE_H		0x3808
#define OV8830_HORIZONTAL_OUTPUT_SIZE_L		0x3809
#define OV8830_VERTICAL_OUTPUT_SIZE_H		0x380a
#define OV8830_VERTICAL_OUTPUT_SIZE_L		0x380b

#define OV8830_SC_CMMN_CHIP_ID_H		0x0000
#define OV8830_SC_CMMN_CHIP_ID_L		0x0001

#define OV8830_GROUP_ACCESS			0x3208
#define OV8830_GROUP_ACCESS_HOLD_START		0x00
#define OV8830_GROUP_ACCESS_HOLD_END		0x10
#define OV8830_GROUP_ACCESS_DELAY_LAUNCH	0xA0
#define OV8830_GROUP_ACCESS_QUICK_LAUNCH	0xE0

#define OV8830_LONG_EXPO			0x3500
#define OV8830_AGC_ADJ				0x350B
#define OV8830_TEST_PATTERN_MODE		0x3070

/* ov8830 SCCB */
#define OV8830_SCCB_CTRL			0x3100
#define OV8830_AEC_PK_EXPO_H			0x3500
#define OV8830_AEC_PK_EXPO_M			0x3501
#define OV8830_AEC_PK_EXPO_L			0x3502
#define OV8830_AEC_MANUAL_CTRL			0x3503
#define OV8830_AGC_ADJ_H			0x3508
#define OV8830_AGC_ADJ_L			0x3509

#define OV8830_MWB_RED_GAIN_H			0x3400
#define OV8830_MWB_GREEN_GAIN_H			0x3402
#define OV8830_MWB_BLUE_GAIN_H			0x3404
#define OV8830_MWB_GAIN_MAX			0x0fff

#define OV8830_OTP_BANK0_PID			0x3d00
#define OV8830_CHIP_ID_HIGH			0x300a
#define OV8830_CHIP_ID_LOW			0x300b
#define OV8830_STREAM_MODE			0x0100

#define OV8830_FOCAL_LENGTH_NUM	439	/*4.39mm*/
#define OV8830_FOCAL_LENGTH_DEM	100
#define OV8830_F_NUMBER_DEFAULT_NUM	24
#define OV8830_F_NUMBER_DEM	10

#define OV8830_TIMING_X_INC		0x3814
#define OV8830_TIMING_Y_INC		0x3815

/* sensor_mode_data read_mode adaptation */
#define OV8830_READ_MODE_BINNING_ON	0x0400
#define OV8830_READ_MODE_BINNING_OFF	0x00
#define OV8830_INTEGRATION_TIME_MARGIN	14

#define OV8830_MAX_VTS_VALUE		0x7FFF
#define OV8830_MAX_EXPOSURE_VALUE \
		(OV8830_MAX_VTS_VALUE - OV8830_INTEGRATION_TIME_MARGIN)
#define OV8830_MAX_GAIN_VALUE		0xFF

/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define OV8830_FOCAL_LENGTH_DEFAULT 0x1B70064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define OV8830_F_NUMBER_DEFAULT 0x18000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define OV8830_F_NUMBER_RANGE 0x180a180a
#define OTPM_ADD_START_1		0x1000
#define OTPM_DATA_LENGTH_1		0x0100
#define OTPM_COUNT 0x200

/* Defines for register writes and register array processing */
#define OV8830_BYTE_MAX	32
#define OV8830_SHORT_MAX	16
#define I2C_RETRY_COUNT		5
#define OV8830_TOK_MASK	0xfff0

#define	OV8830_STATUS_POWER_DOWN	0x0
#define	OV8830_STATUS_STANDBY		0x2
#define	OV8830_STATUS_ACTIVE		0x3
#define	OV8830_STATUS_VIEWFINDER	0x4

#define MAX_FPS_OPTIONS_SUPPORTED	3

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


#define	macro_string_entry(VAL)	\
	{ \
		.val = VAL, \
		.string = #VAL, \
	}

enum ov8830_tok_type {
	OV8830_8BIT  = 0x0001,
	OV8830_16BIT = 0x0002,
	OV8830_TOK_TERM   = 0xf000,	/* terminating token for reg list */
	OV8830_TOK_DELAY  = 0xfe00	/* delay token for reg list */
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
 * struct ov8830_fwreg - Firmware burst command
 * @type: FW burst or 8/16 bit register
 * @addr: 16-bit offset to register or other values depending on type
 * @val: data value for burst (or other commands)
 *
 * Define a structure for sensor register initialization values
 */
struct ov8830_fwreg {
	enum ov8830_tok_type type; /* value, register or FW burst string */
	u16 addr;	/* target address */
	u32 val[8];
};

/**
 * struct ov8830_reg - MI sensor  register format
 * @type: type of the register
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 *
 * Define a structure for sensor register initialization values
 */
struct ov8830_reg {
	enum ov8830_tok_type type;
	union {
		u16 sreg;
		struct ov8830_fwreg *fwreg;
	} reg;
	u32 val;	/* @set value for read/mod/write, @mask */
	u32 val2;	/* optional: for rmw, OR mask */
};

struct ov8830_fps_setting {
	int fps;
	unsigned short pixels_per_line;
	unsigned short lines_per_frame;
};

/* Store macro values' debug names */
struct macro_string {
	u8 val;
	char *string;
};

static inline const char *
macro_to_string(const struct macro_string *array, int size, u8 val)
{
	int i;
	for (i = 0; i < size; i++) {
		if (array[i].val == val)
			return array[i].string;
	}
	return "Unknown VAL";
}

struct ov8830_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, s32 value);
};

struct ov8830_resolution {
	u8 *desc;
	int res;
	int width;
	int height;
	bool used;
	const struct ov8830_reg *regs;
	u8 bin_factor_x;
	u8 bin_factor_y;
	unsigned short skip_frames;
	const struct ov8830_fps_setting fps_options[MAX_FPS_OPTIONS_SUPPORTED];
};

struct ov8830_format {
	u8 *desc;
	u32 pixelformat;
	struct s_register_setting *regs;
};

/* ov8830 device structure */
struct ov8830_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;

	struct camera_sensor_platform_data *platform_data;
	int fmt_idx;
	int streaming;
	int power;
	u16 sensor_id;
	u8 sensor_revision;
	int exposure;
	int gain;
	u16 digital_gain;
	struct drv201_device drv201;
	struct mutex input_lock; /* serialize sensor's ioctl */

	const struct ov8830_reg *basic_settings_list;
	const struct ov8830_resolution *curr_res_table;
	int entries_curr_table;
	int fps_index;

	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *run_mode;
};

/*
 * The i2c adapter on Intel Medfield can transfer 32 bytes maximum
 * at a time. In burst mode we require that the buffer is transferred
 * in one shot, so limit the buffer size to 32 bytes minus a safety.
 */
#define OV8830_MAX_WRITE_BUF_SIZE	30
struct ov8830_write_buffer {
	u16 addr;
	u8 data[OV8830_MAX_WRITE_BUF_SIZE];
};

struct ov8830_write_ctrl {
	int index;
	struct ov8830_write_buffer buffer;
};

#define OV8830_RES_WIDTH_MAX	3280
#define OV8830_RES_HEIGHT_MAX	2464

static const struct ov8830_reg ov8835_module_detection[] = {
	{ OV8830_8BIT, { OV8830_STREAM_MODE }, 0x01 }, /* Stream on */
	{ OV8830_8BIT, { 0x3d84 }, 0xc0 }, /* Select Bank 0 */
	{ OV8830_8BIT, { 0x3d81 }, 0x01 }, /* OTP read enable */
	{ OV8830_TOK_TERM, {0}, 0}
};

static const struct ov8830_reg ov8830_BasicSettings[] = {
	{ OV8830_8BIT, { 0x0103 }, 0x01 },
	{ OV8830_8BIT, { 0x0100 }, 0x00 },
	{ OV8830_8BIT, { 0x0102 }, 0x01 },
	{ OV8830_8BIT, { 0x3000 }, 0x00 },
	{ OV8830_8BIT, { 0x3001 }, 0x2a },
	{ OV8830_8BIT, { 0x3002 }, 0x88 },
	{ OV8830_8BIT, { 0x3003 }, 0x00 },
	{ OV8830_8BIT, { 0x3004 }, 0x00 },
	{ OV8830_8BIT, { 0x3005 }, 0x00 },
	{ OV8830_8BIT, { 0x3006 }, 0x00 },
	{ OV8830_8BIT, { 0x3007 }, 0x00 },
	{ OV8830_8BIT, { 0x3008 }, 0x00 },
	{ OV8830_8BIT, { 0x3009 }, 0x00 },
	{ OV8830_8BIT, { 0x3011 }, 0x41 },
	{ OV8830_8BIT, { 0x3012 }, 0x08 },
	{ OV8830_8BIT, { 0x3013 }, 0x10 },
	{ OV8830_8BIT, { 0x3014 }, 0x00 },
	{ OV8830_8BIT, { 0x3015 }, 0x08 },
	{ OV8830_8BIT, { 0x3016 }, 0xf0 },
	{ OV8830_8BIT, { 0x3017 }, 0xf0 },
	{ OV8830_8BIT, { 0x3018 }, 0xf0 },
	{ OV8830_8BIT, { 0x301b }, 0xb4 },
	{ OV8830_8BIT, { 0x301d }, 0x02 },
	{ OV8830_8BIT, { 0x3021 }, 0x00 },
	{ OV8830_8BIT, { 0x3022 }, 0x00 },
	{ OV8830_8BIT, { 0x3024 }, 0x00 },
	{ OV8830_8BIT, { 0x3026 }, 0x00 },
	{ OV8830_8BIT, { 0x3027 }, 0x00 },
	{ OV8830_8BIT, { 0x3081 }, 0x02 },
	{ OV8830_8BIT, { 0x3083 }, 0x01 },
	{ OV8830_8BIT, { 0x3090 }, 0x01 }, /* PLL2 Settings SCLK 192mhZ*/
	{ OV8830_8BIT, { 0x3091 }, 0x14 },
	{ OV8830_8BIT, { 0x3094 }, 0x00 },
	{ OV8830_8BIT, { 0x3092 }, 0x01 },
	{ OV8830_8BIT, { 0x3093 }, 0x00 },
	{ OV8830_8BIT, { 0x3098 }, 0x03 }, /* PLL3 Settings REF_CLK */
	{ OV8830_8BIT, { 0x3099 }, 0x13 },
	{ OV8830_8BIT, { 0x309a }, 0x00 },
	{ OV8830_8BIT, { 0x309b }, 0x00 },
	{ OV8830_8BIT, { 0x309c }, 0x01 },
	{ OV8830_8BIT, { 0x30b3 }, 0x6b }, /* MIPI PLL1 Settings 684.4Mbps */
	{ OV8830_8BIT, { 0x30b4 }, 0x03 },
	{ OV8830_8BIT, { 0x30b5 }, 0x04 },
	{ OV8830_8BIT, { 0x30b6 }, 0x01 },
	{ OV8830_8BIT, { 0x3104 }, 0xa1 },
	{ OV8830_8BIT, { 0x3106 }, 0x01 },
	{ OV8830_8BIT, { 0x3300 }, 0x00 },
	{ OV8830_8BIT, { 0x3400 }, 0x04 },
	{ OV8830_8BIT, { 0x3401 }, 0x00 },
	{ OV8830_8BIT, { 0x3402 }, 0x04 },
	{ OV8830_8BIT, { 0x3403 }, 0x00 },
	{ OV8830_8BIT, { 0x3404 }, 0x04 },
	{ OV8830_8BIT, { 0x3405 }, 0x00 },
	{ OV8830_8BIT, { 0x3406 }, 0x01 },
	{ OV8830_8BIT, { 0x3500 }, 0x00 },
	{ OV8830_8BIT, { 0x3501 }, 0x30 },
	{ OV8830_8BIT, { 0x3502 }, 0x00 },
	{ OV8830_8BIT, { 0x3503 }, 0x07 },
	{ OV8830_8BIT, { 0x3504 }, 0x00 },
	{ OV8830_8BIT, { 0x3505 }, 0x30 },
	{ OV8830_8BIT, { 0x3506 }, 0x00 },
	{ OV8830_8BIT, { 0x3507 }, 0x10 },
	{ OV8830_8BIT, { 0x3508 }, 0x80 },
	{ OV8830_8BIT, { 0x3509 }, 0x10 },
	{ OV8830_8BIT, { 0x350a }, 0x00 },
	{ OV8830_8BIT, { 0x350b }, 0x38 },
	{ OV8830_8BIT, { 0x350c }, 0x00 },
	{ OV8830_8BIT, { 0x350d }, 0x00 },
	{ OV8830_8BIT, { 0x3600 }, 0x78 },
	/* Next 2 values As Per OV recomm. Only for OV8830 */
	{ OV8830_8BIT, { 0x3601 }, 0x0a },
	{ OV8830_8BIT, { 0x3602 }, 0x9c },
	{ OV8830_8BIT, { 0x3604 }, 0x38 },
	{ OV8830_8BIT, { 0x3620 }, 0x64 },
	{ OV8830_8BIT, { 0x3621 }, 0xb5 },
	{ OV8830_8BIT, { 0x3622 }, 0x03 },
	{ OV8830_8BIT, { 0x3625 }, 0x64 },
	{ OV8830_8BIT, { 0x3630 }, 0x55 },
	{ OV8830_8BIT, { 0x3631 }, 0xd2 },
	{ OV8830_8BIT, { 0x3632 }, 0x00 },
	{ OV8830_8BIT, { 0x3633 }, 0x34 },
	{ OV8830_8BIT, { 0x3634 }, 0x03 },
	{ OV8830_8BIT, { 0x3660 }, 0x80 },
	{ OV8830_8BIT, { 0x3662 }, 0x10 },
	{ OV8830_8BIT, { 0x3665 }, 0x00 },
	{ OV8830_8BIT, { 0x3666 }, 0x00 },
	{ OV8830_8BIT, { 0x3667 }, 0x00 },
	{ OV8830_8BIT, { 0x366a }, 0x80 },
	{ OV8830_8BIT, { 0x366c }, 0x00 },
	{ OV8830_8BIT, { 0x366d }, 0x00 },
	{ OV8830_8BIT, { 0x366e }, 0x00 },
	{ OV8830_8BIT, { 0x366f }, 0x20 },
	{ OV8830_8BIT, { 0x3680 }, 0xe0 },
	{ OV8830_8BIT, { 0x3681 }, 0x00 },
	{ OV8830_8BIT, { 0x3701 }, 0x14 },
	{ OV8830_8BIT, { 0x3702 }, 0xbf },
	{ OV8830_8BIT, { 0x3703 }, 0x8c },
	{ OV8830_8BIT, { 0x3704 }, 0x78 },
	{ OV8830_8BIT, { 0x3705 }, 0x02 },
	{ OV8830_8BIT, { 0x3708 }, 0xe4 },
	{ OV8830_8BIT, { 0x3709 }, 0x03 },
	{ OV8830_8BIT, { 0x370a }, 0x00 },
	{ OV8830_8BIT, { 0x370b }, 0x20 },
	{ OV8830_8BIT, { 0x370c }, 0x0c },
	{ OV8830_8BIT, { 0x370d }, 0x11 },
	{ OV8830_8BIT, { 0x370e }, 0x00 },
	{ OV8830_8BIT, { 0x370f }, 0x00 },
	{ OV8830_8BIT, { 0x3710 }, 0x00 },
	{ OV8830_8BIT, { 0x371c }, 0x01 },
	{ OV8830_8BIT, { 0x371f }, 0x0c },
	{ OV8830_8BIT, { 0x3721 }, 0x00 },
	{ OV8830_8BIT, { 0x3724 }, 0x10 },
	{ OV8830_8BIT, { 0x3726 }, 0x00 },
	{ OV8830_8BIT, { 0x372a }, 0x01 },
	{ OV8830_8BIT, { 0x3730 }, 0x18 },
	{ OV8830_8BIT, { 0x3738 }, 0x22 },
	{ OV8830_8BIT, { 0x3739 }, 0x08 },
	{ OV8830_8BIT, { 0x373a }, 0x51 },
	{ OV8830_8BIT, { 0x373b }, 0x02 },
	{ OV8830_8BIT, { 0x373c }, 0x20 },
	{ OV8830_8BIT, { 0x373f }, 0x02 },
	{ OV8830_8BIT, { 0x3740 }, 0x42 },
	{ OV8830_8BIT, { 0x3741 }, 0x02 },
	{ OV8830_8BIT, { 0x3742 }, 0x18 },
	{ OV8830_8BIT, { 0x3743 }, 0x01 },
	{ OV8830_8BIT, { 0x3744 }, 0x02 },
	{ OV8830_8BIT, { 0x3747 }, 0x10 },
	{ OV8830_8BIT, { 0x374c }, 0x04 },
	{ OV8830_8BIT, { 0x3751 }, 0xf0 },
	{ OV8830_8BIT, { 0x3752 }, 0x00 },
	{ OV8830_8BIT, { 0x3753 }, 0x00 },
	{ OV8830_8BIT, { 0x3754 }, 0xc0 },
	{ OV8830_8BIT, { 0x3755 }, 0x00 },
	{ OV8830_8BIT, { 0x3756 }, 0x1a },
	{ OV8830_8BIT, { 0x3758 }, 0x00 },
	{ OV8830_8BIT, { 0x3759 }, 0x0f },
	{ OV8830_8BIT, { 0x375c }, 0x04 },
	{ OV8830_8BIT, { 0x3767 }, 0x01 },
	{ OV8830_8BIT, { 0x376b }, 0x44 },
	{ OV8830_8BIT, { 0x3774 }, 0x10 },
	{ OV8830_8BIT, { 0x3776 }, 0x00 },
	{ OV8830_8BIT, { 0x377f }, 0x08 },
	{ OV8830_8BIT, { 0x3780 }, 0x22 },
	{ OV8830_8BIT, { 0x3781 }, 0x0c },
	{ OV8830_8BIT, { 0x3784 }, 0x2c },
	{ OV8830_8BIT, { 0x3785 }, 0x1e },
	{ OV8830_8BIT, { 0x378f }, 0xf5 },
	{ OV8830_8BIT, { 0x3791 }, 0xb0 },
	{ OV8830_8BIT, { 0x3795 }, 0x00 },
	{ OV8830_8BIT, { 0x3796 }, 0x64 },
	{ OV8830_8BIT, { 0x3797 }, 0x11 },
	{ OV8830_8BIT, { 0x3798 }, 0x30 },
	{ OV8830_8BIT, { 0x3799 }, 0x41 },
	{ OV8830_8BIT, { 0x379a }, 0x07 },
	{ OV8830_8BIT, { 0x379b }, 0xb0 },
	{ OV8830_8BIT, { 0x379c }, 0x0c },
	{ OV8830_8BIT, { 0x37c0 }, 0x00 },
	{ OV8830_8BIT, { 0x37c1 }, 0x00 },
	{ OV8830_8BIT, { 0x37c2 }, 0x00 },
	{ OV8830_8BIT, { 0x37c3 }, 0x00 },
	{ OV8830_8BIT, { 0x37c4 }, 0x00 },
	{ OV8830_8BIT, { 0x37c5 }, 0x00 },
	{ OV8830_8BIT, { 0x37c6 }, 0xa0 },
	{ OV8830_8BIT, { 0x37c7 }, 0x00 },
	{ OV8830_8BIT, { 0x37c8 }, 0x00 },
	{ OV8830_8BIT, { 0x37c9 }, 0x00 },
	{ OV8830_8BIT, { 0x37ca }, 0x00 },
	{ OV8830_8BIT, { 0x37cb }, 0x00 },
	{ OV8830_8BIT, { 0x37cc }, 0x00 },
	{ OV8830_8BIT, { 0x37cd }, 0x00 },
	{ OV8830_8BIT, { 0x37ce }, 0x01 },
	{ OV8830_8BIT, { 0x37cf }, 0x00 },
	{ OV8830_8BIT, { 0x37d1 }, 0x01 },
	{ OV8830_8BIT, { 0x37de }, 0x00 },
	{ OV8830_8BIT, { 0x37df }, 0x00 },
	{ OV8830_8BIT, { 0x3820 }, 0x10 },
	{ OV8830_8BIT, { 0x3821 }, 0x0e },
	{ OV8830_8BIT, { 0x3823 }, 0x00 },
	{ OV8830_8BIT, { 0x3824 }, 0x00 },
	{ OV8830_8BIT, { 0x3825 }, 0x00 },
	{ OV8830_8BIT, { 0x3826 }, 0x00 },
	{ OV8830_8BIT, { 0x3827 }, 0x00 },
	{ OV8830_8BIT, { 0x382a }, 0x04 },
	{ OV8830_8BIT, { 0x3a04 }, 0x09 },
	{ OV8830_8BIT, { 0x3a05 }, 0xa9 },
	{ OV8830_8BIT, { 0x3a06 }, 0x00 },
	{ OV8830_8BIT, { 0x3a07 }, 0xf8 },
	{ OV8830_8BIT, { 0x3a18 }, 0x00 },
	{ OV8830_8BIT, { 0x3a19 }, 0x00 },
	{ OV8830_8BIT, { 0x3b00 }, 0x00 },
	{ OV8830_8BIT, { 0x3b01 }, 0x00 },
	{ OV8830_8BIT, { 0x3b02 }, 0x00 },
	{ OV8830_8BIT, { 0x3b03 }, 0x00 },
	{ OV8830_8BIT, { 0x3b04 }, 0x00 },
	{ OV8830_8BIT, { 0x3b05 }, 0x00 },
	{ OV8830_8BIT, { 0x3d00 }, 0x00 },
	{ OV8830_8BIT, { 0x3d01 }, 0x00 },
	{ OV8830_8BIT, { 0x3d02 }, 0x00 },
	{ OV8830_8BIT, { 0x3d03 }, 0x00 },
	{ OV8830_8BIT, { 0x3d04 }, 0x00 },
	{ OV8830_8BIT, { 0x3d05 }, 0x00 },
	{ OV8830_8BIT, { 0x3d06 }, 0x00 },
	{ OV8830_8BIT, { 0x3d07 }, 0x00 },
	{ OV8830_8BIT, { 0x3d08 }, 0x00 },
	{ OV8830_8BIT, { 0x3d09 }, 0x00 },
	{ OV8830_8BIT, { 0x3d0a }, 0x00 },
	{ OV8830_8BIT, { 0x3d0b }, 0x00 },
	{ OV8830_8BIT, { 0x3d0c }, 0x00 },
	{ OV8830_8BIT, { 0x3d0d }, 0x00 },
	{ OV8830_8BIT, { 0x3d0e }, 0x00 },
	{ OV8830_8BIT, { 0x3d0f }, 0x00 },
	{ OV8830_8BIT, { 0x3d80 }, 0x00 },
	{ OV8830_8BIT, { 0x3d81 }, 0x00 },
	{ OV8830_8BIT, { 0x3d84 }, 0x00 },
	{ OV8830_8BIT, { 0x3e07 }, 0x20 },
	{ OV8830_8BIT, { 0x4000 }, 0x18 },
	{ OV8830_8BIT, { 0x4001 }, 0x04 },
	{ OV8830_8BIT, { 0x4002 }, 0x45 },
	{ OV8830_8BIT, { 0x4004 }, 0x02 },
	{ OV8830_8BIT, { 0x4005 }, 0x18 },
	{ OV8830_8BIT, { 0x4006 }, 0x16 },
	{ OV8830_8BIT, { 0x4008 }, 0x20 },
	{ OV8830_8BIT, { 0x4009 }, 0x10 },
	{ OV8830_8BIT, { 0x400c }, 0x00 },
	{ OV8830_8BIT, { 0x400d }, 0x00 },
	{ OV8830_8BIT, { 0x4058 }, 0x00 },
	{ OV8830_8BIT, { 0x4101 }, 0x12 },
	{ OV8830_8BIT, { 0x4104 }, 0x5b },
	{ OV8830_8BIT, { 0x4303 }, 0x00 },
	{ OV8830_8BIT, { 0x4304 }, 0x08 },
	{ OV8830_8BIT, { 0x4307 }, 0x30 },
	{ OV8830_8BIT, { 0x4315 }, 0x00 },
	{ OV8830_8BIT, { 0x4511 }, 0x05 },
	{ OV8830_8BIT, { 0x4512 }, 0x01 }, /* Binning option = Average */
	{ OV8830_8BIT, { 0x4750 }, 0x00 },
	{ OV8830_8BIT, { 0x4751 }, 0x00 },
	{ OV8830_8BIT, { 0x4752 }, 0x00 },
	{ OV8830_8BIT, { 0x4753 }, 0x00 },
	{ OV8830_8BIT, { 0x4805 }, 0x01 },
	{ OV8830_8BIT, { 0x4806 }, 0x00 },
	{ OV8830_8BIT, { 0x481f }, 0x36 },
	{ OV8830_8BIT, { 0x4831 }, 0x6c },
	{ OV8830_8BIT, { 0x4837 }, 0x0c }, /* MIPI global timing */
	{ OV8830_8BIT, { 0x4a00 }, 0xaa },
	{ OV8830_8BIT, { 0x4a03 }, 0x01 },
	{ OV8830_8BIT, { 0x4a05 }, 0x08 },
	{ OV8830_8BIT, { 0x4a0a }, 0x88 },
	{ OV8830_8BIT, { 0x5000 }, 0x06 },
	{ OV8830_8BIT, { 0x5001 }, 0x01 },
	{ OV8830_8BIT, { 0x5002 }, 0x80 },
	{ OV8830_8BIT, { 0x5003 }, 0x20 },
	{ OV8830_8BIT, { 0x5013 }, 0x00 },
	{ OV8830_8BIT, { 0x5046 }, 0x4a },
	{ OV8830_8BIT, { 0x5780 }, 0x1c },
	{ OV8830_8BIT, { 0x5786 }, 0x20 },
	{ OV8830_8BIT, { 0x5787 }, 0x10 },
	{ OV8830_8BIT, { 0x5788 }, 0x18 },
	{ OV8830_8BIT, { 0x578a }, 0x04 },
	{ OV8830_8BIT, { 0x578b }, 0x02 },
	{ OV8830_8BIT, { 0x578c }, 0x02 },
	{ OV8830_8BIT, { 0x578e }, 0x06 },
	{ OV8830_8BIT, { 0x578f }, 0x02 },
	{ OV8830_8BIT, { 0x5790 }, 0x02 },
	{ OV8830_8BIT, { 0x5791 }, 0xff },
	{ OV8830_8BIT, { 0x5a08 }, 0x02 },
	{ OV8830_8BIT, { 0x5e00 }, 0x00 },
	{ OV8830_8BIT, { 0x5e10 }, 0x0c },
	{ OV8830_TOK_TERM, {0}, 0}
};

/*****************************STILL********************************/

static const struct ov8830_reg ov8830_cont_cap_720P[] = {
	{ OV8830_8BIT, { 0x3800 }, 0x00 },
	{ OV8830_8BIT, { 0x3801 }, 0x0c },
	{ OV8830_8BIT, { 0x3802 }, 0x01 },
	{ OV8830_8BIT, { 0x3803 }, 0x40 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xe3 },
	{ OV8830_8BIT, { 0x3806 }, 0x08 },
	{ OV8830_8BIT, { 0x3807 }, 0x75 },
	{ OV8830_8BIT, { 0x3808 }, 0x05 }, /* Output size 1296x736 */
	{ OV8830_8BIT, { 0x3809 }, 0x10 },
	{ OV8830_8BIT, { 0x380a }, 0x02 },
	{ OV8830_8BIT, { 0x380b }, 0xe0 },
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3814 }, 0x31 },
	{ OV8830_8BIT, { 0x3815 }, 0x31 },
	{ OV8830_8BIT, { 0x3820 }, 0x11 }, /* Binning on */
	{ OV8830_8BIT, { 0x3821 }, 0x0f },
	{ OV8830_TOK_TERM, {0}, 0}
};

static const struct ov8830_reg ov8830_1080P_STILL[] = {
	{ OV8830_8BIT, { 0x3800 }, 0x00 },
	{ OV8830_8BIT, { 0x3801 }, 0x0c },
	{ OV8830_8BIT, { 0x3802 }, 0x01 },
	{ OV8830_8BIT, { 0x3803 }, 0x40 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xe3 },
	{ OV8830_8BIT, { 0x3806 }, 0x08 },
	{ OV8830_8BIT, { 0x3807 }, 0x75 },
	{ OV8830_8BIT, { 0x3808 }, 0x07 }, /* Output size 1936x1104 */
	{ OV8830_8BIT, { 0x3809 }, 0x90 },
	{ OV8830_8BIT, { 0x380a }, 0x04 },
	{ OV8830_8BIT, { 0x380b }, 0x50 },
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3814 }, 0x11 },
	{ OV8830_8BIT, { 0x3815 }, 0x11 },
	{ OV8830_8BIT, { 0x3820 }, 0x10 }, /* Binning off */
	{ OV8830_8BIT, { 0x3821 }, 0x0e },
	{ OV8830_TOK_TERM, {0}, 0}
};

static const struct ov8830_reg ov8830_cont_cap_qvga[] = {
	{ OV8830_8BIT, { 0x3800 }, 0x00 },
	{ OV8830_8BIT, { 0x3801 }, 0x04 },
	{ OV8830_8BIT, { 0x3802 }, 0x00 },
	{ OV8830_8BIT, { 0x3803 }, 0x04 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xdb },
	{ OV8830_8BIT, { 0x3806 }, 0x09 },
	{ OV8830_8BIT, { 0x3807 }, 0xab },
	{ OV8830_8BIT, { 0x3808 }, 0x01 }, /* O/p 336x256 Bin+skip+scale */
	{ OV8830_8BIT, { 0x3809 }, 0x50 },
	{ OV8830_8BIT, { 0x380a }, 0x01 },
	{ OV8830_8BIT, { 0x380b }, 0x00 },
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3814 }, 0x71 },
	{ OV8830_8BIT, { 0x3815 }, 0x71 },
	{ OV8830_8BIT, { 0x3820 }, 0x11 }, /* Binning+skipping on */
	{ OV8830_8BIT, { 0x3821 }, 0x0f },
	{ OV8830_TOK_TERM, {0}, 0}
};

static const struct ov8830_reg ov8830_VGA_STILL[] = {
	{ OV8830_8BIT, { 0x3800 }, 0x00 },
	{ OV8830_8BIT, { 0x3801 }, 0x04 },
	{ OV8830_8BIT, { 0x3802 }, 0x00 },
	{ OV8830_8BIT, { 0x3803 }, 0x04 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xdb },
	{ OV8830_8BIT, { 0x3806 }, 0x09 },
	{ OV8830_8BIT, { 0x3807 }, 0xab },
	{ OV8830_8BIT, { 0x3808 }, 0x02 }, /* Ouput Size 656x496 */
	{ OV8830_8BIT, { 0x3809 }, 0x90 },
	{ OV8830_8BIT, { 0x380a }, 0x01 },
	{ OV8830_8BIT, { 0x380b }, 0xf0 },
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3814 }, 0x31 },
	{ OV8830_8BIT, { 0x3815 }, 0x31 },
	{ OV8830_8BIT, { 0x3820 }, 0x11 }, /* Binning on */
	{ OV8830_8BIT, { 0x3821 }, 0x0f },
	{ OV8830_TOK_TERM, {0}, 0}
};

static const struct ov8830_reg ov8830_1M_STILL[] = {
	{ OV8830_8BIT, { 0x3800 }, 0x00 },
	{ OV8830_8BIT, { 0x3801 }, 0x04 },
	{ OV8830_8BIT, { 0x3802 }, 0x00 },
	{ OV8830_8BIT, { 0x3803 }, 0x04 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xdb },
	{ OV8830_8BIT, { 0x3806 }, 0x09 },
	{ OV8830_8BIT, { 0x3807 }, 0xab },
	{ OV8830_8BIT, { 0x3808 }, 0x04 }, /* Ouput Size 1040x784 1229x922 */
	{ OV8830_8BIT, { 0x3809 }, 0x10 },
	{ OV8830_8BIT, { 0x380a }, 0x03 },
	{ OV8830_8BIT, { 0x380b }, 0x10 },
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3814 }, 0x11 },
	{ OV8830_8BIT, { 0x3815 }, 0x11 },
	{ OV8830_8BIT, { 0x3820 }, 0x10 }, /* Binning off */
	{ OV8830_8BIT, { 0x3821 }, 0x0e },
	{ OV8830_TOK_TERM, {0}, 0}
};

static const struct ov8830_reg ov8830_2M_STILL[] = {
	{ OV8830_8BIT, { 0x3800 }, 0x00 },
	{ OV8830_8BIT, { 0x3801 }, 0x04 },
	{ OV8830_8BIT, { 0x3802 }, 0x00 },
	{ OV8830_8BIT, { 0x3803 }, 0x04 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xdb },
	{ OV8830_8BIT, { 0x3806 }, 0x09 },
	{ OV8830_8BIT, { 0x3807 }, 0xab },
	{ OV8830_8BIT, { 0x3808 }, 0x06 }, /* Ouput Size 1640x1232 */
	{ OV8830_8BIT, { 0x3809 }, 0x68 },
	{ OV8830_8BIT, { 0x380a }, 0x04 },
	{ OV8830_8BIT, { 0x380b }, 0xd0 },
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3814 }, 0x11 },
	{ OV8830_8BIT, { 0x3815 }, 0x11 },
	{ OV8830_8BIT, { 0x3820 }, 0x10 }, /* Binning off */
	{ OV8830_8BIT, { 0x3821 }, 0x0e },
	{ OV8830_TOK_TERM, {0}, 0}
};

static const struct ov8830_reg ov8830_3M_STILL[] = {
	{ OV8830_8BIT, { 0x3800 }, 0x00 },
	{ OV8830_8BIT, { 0x3801 }, 0x04 },
	{ OV8830_8BIT, { 0x3802 }, 0x00 },
	{ OV8830_8BIT, { 0x3803 }, 0x04 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xdb },
	{ OV8830_8BIT, { 0x3806 }, 0x09 },
	{ OV8830_8BIT, { 0x3807 }, 0xab },
	{ OV8830_8BIT, { 0x3808 }, 0x08 }, /* Ouput Size 2064x1552 */
	{ OV8830_8BIT, { 0x3809 }, 0x10 },
	{ OV8830_8BIT, { 0x380a }, 0x06 },
	{ OV8830_8BIT, { 0x380b }, 0x10 },
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3814 }, 0x11 },
	{ OV8830_8BIT, { 0x3815 }, 0x11 },
	{ OV8830_8BIT, { 0x3820 }, 0x10 }, /* Binning off */
	{ OV8830_8BIT, { 0x3821 }, 0x0e },
	{ OV8830_TOK_TERM, {0}, 0}
};

static const struct ov8830_reg ov8830_5M_STILL[] = {
	{ OV8830_8BIT, { 0x3800 }, 0x00 },
	{ OV8830_8BIT, { 0x3801 }, 0x04 },
	{ OV8830_8BIT, { 0x3802 }, 0x00 },
	{ OV8830_8BIT, { 0x3803 }, 0x04 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xdb },
	{ OV8830_8BIT, { 0x3806 }, 0x09 },
	{ OV8830_8BIT, { 0x3807 }, 0xab },
	{ OV8830_8BIT, { 0x3808 }, 0x0a }, /* Ouput Size 2576x1936 */
	{ OV8830_8BIT, { 0x3809 }, 0x10 },
	{ OV8830_8BIT, { 0x380a }, 0x07 },
	{ OV8830_8BIT, { 0x380b }, 0x90 },
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3814 }, 0x11 },
	{ OV8830_8BIT, { 0x3815 }, 0x11 },
	{ OV8830_8BIT, { 0x3820 }, 0x10 }, /* Binning off */
	{ OV8830_8BIT, { 0x3821 }, 0x0e },
	{ OV8830_TOK_TERM, {0}, 0}
};

static const struct ov8830_reg ov8830_6M_STILL[] = {
	{ OV8830_8BIT, { 0x3800 }, 0x00 },
	{ OV8830_8BIT, { 0x3801 }, 0x04 },
	{ OV8830_8BIT, { 0x3802 }, 0x01 },
	{ OV8830_8BIT, { 0x3803 }, 0x36 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xdb },
	{ OV8830_8BIT, { 0x3806 }, 0x08 },
	{ OV8830_8BIT, { 0x3807 }, 0x79 }, /* Read Array: 3291 x 2169 */
	{ OV8830_8BIT, { 0x3808 }, 0x0c }, /* Output size 3280x1852 */
	{ OV8830_8BIT, { 0x3809 }, 0xd0 },
	{ OV8830_8BIT, { 0x380a }, 0x07 },
	{ OV8830_8BIT, { 0x380b }, 0x3c },
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3814 }, 0x11 },
	{ OV8830_8BIT, { 0x3815 }, 0x11 },
	{ OV8830_8BIT, { 0x3820 }, 0x10 }, /* Binning off */
	{ OV8830_8BIT, { 0x3821 }, 0x0e },
	{ OV8830_TOK_TERM, {0}, 0}
};

static const struct ov8830_reg ov8830_8M_STILL[] = {
	{ OV8830_8BIT, { 0x3800 }, 0x00 },
	{ OV8830_8BIT, { 0x3801 }, 0x04 },
	{ OV8830_8BIT, { 0x3802 }, 0x00 },
	{ OV8830_8BIT, { 0x3803 }, 0x04 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xdb },
	{ OV8830_8BIT, { 0x3806 }, 0x09 },
	{ OV8830_8BIT, { 0x3807 }, 0xab },
	{ OV8830_8BIT, { 0x3808 }, 0x0c }, /* Output size 3280x2464 */
	{ OV8830_8BIT, { 0x3809 }, 0xd0 },
	{ OV8830_8BIT, { 0x380a }, 0x09 },
	{ OV8830_8BIT, { 0x380b }, 0xa0 },
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3814 }, 0x11 },
	{ OV8830_8BIT, { 0x3815 }, 0x11 },
	{ OV8830_8BIT, { 0x3820 }, 0x10 }, /* Binning off */
	{ OV8830_8BIT, { 0x3821 }, 0x0e },
	{ OV8830_TOK_TERM, {0}, 0}
};
/*****************************OV8830 PREVIEW********************************/

static struct ov8830_reg const ov8830_PREVIEW_848x616[] = {
	{ OV8830_8BIT, { 0x3800 }, 0x00 },
	{ OV8830_8BIT, { 0x3801 }, 0x08 },
	{ OV8830_8BIT, { 0x3802 }, 0x00 },
	{ OV8830_8BIT, { 0x3803 }, 0x08 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xd7 },
	{ OV8830_8BIT, { 0x3806 }, 0x09 }, /* 8, 3287, 8, 2471 Binning*/
	{ OV8830_8BIT, { 0x3807 }, 0xa7 }, /* Actual Size 3280x2464 */
	{ OV8830_8BIT, { 0x3808 }, 0x03 },
	{ OV8830_8BIT, { 0x3809 }, 0x50 },
	{ OV8830_8BIT, { 0x380a }, 0x02 },
	{ OV8830_8BIT, { 0x380b }, 0x68 }, /* O/p 848x616 Binning+Scaling */
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3814 }, 0x31 },
	{ OV8830_8BIT, { 0x3815 }, 0x31 },
	{ OV8830_8BIT, { 0x3820 }, 0x11 }, /* Vertical Binning 0n */
	{ OV8830_8BIT, { 0x3821 }, 0x0f }, /* Horizontal Binning 0n */
	{ OV8830_TOK_TERM, {0}, 0}
};

static struct ov8830_reg const ov8830_PREVIEW_WIDE_PREVIEW[] = {
	{ OV8830_8BIT, { 0x3800 }, 0x00 },
	{ OV8830_8BIT, { 0x3801 }, 0x0c },
	{ OV8830_8BIT, { 0x3802 }, 0x01 },
	{ OV8830_8BIT, { 0x3803 }, 0x40 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xd3 },
	{ OV8830_8BIT, { 0x3806 }, 0x08 },
	{ OV8830_8BIT, { 0x3807 }, 0x73 }, /* 3268x1840 */
	{ OV8830_8BIT, { 0x3808 }, 0x05 },
	{ OV8830_8BIT, { 0x3809 }, 0x00 },
	{ OV8830_8BIT, { 0x380a }, 0x02 },
	{ OV8830_8BIT, { 0x380b }, 0xd0 }, /* 1280X720*/
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3814 }, 0x31 },
	{ OV8830_8BIT, { 0x3815 }, 0x31 },
	{ OV8830_8BIT, { 0x3820 }, 0x11 }, /* Binning on */
	{ OV8830_8BIT, { 0x3821 }, 0x0f },
	{ OV8830_TOK_TERM, {0}, 0}
};

static struct ov8830_reg const ov8830_PREVIEW_1632x1224[] = {
	{ OV8830_8BIT, { 0x3800 }, 0x00 },
	{ OV8830_8BIT, { 0x3801 }, 0x08 },
	{ OV8830_8BIT, { 0x3802 }, 0x00 },
	{ OV8830_8BIT, { 0x3803 }, 0x08 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xd7 },
	{ OV8830_8BIT, { 0x3806 }, 0x09 },
	{ OV8830_8BIT, { 0x3807 }, 0xa7 }, /* Actual Size 3280x2464 */
	{ OV8830_8BIT, { 0x3808 }, 0x06 },
	{ OV8830_8BIT, { 0x3809 }, 0x60 },
	{ OV8830_8BIT, { 0x380a }, 0x04 },
	{ OV8830_8BIT, { 0x380b }, 0xc8 }, /* Output size: 1632x1224 */
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3814 }, 0x31 },
	{ OV8830_8BIT, { 0x3815 }, 0x31 },
	{ OV8830_8BIT, { 0x3820 }, 0x11 }, /* Vertical Binning 0n */
	{ OV8830_8BIT, { 0x3821 }, 0x0f }, /* Horizontal Binning 0n */
	{ OV8830_TOK_TERM, {0}, 0}
};

/***************** OV8830 VIDEO ***************************************/

static const struct ov8830_reg ov8830_QCIF_strong_dvs[] = {
	{ OV8830_8BIT, { 0x3800 }, 0x00 },
	{ OV8830_8BIT, { 0x3801 }, 0x08 },
	{ OV8830_8BIT, { 0x3802 }, 0x00 },
	{ OV8830_8BIT, { 0x3803 }, 0x08 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xd7 },
	{ OV8830_8BIT, { 0x3806 }, 0x09 }, /* 8, 3287, 8, 2471 Binning on*/
	{ OV8830_8BIT, { 0x3807 }, 0xa7 }, /* Actual Size 3280x2464 */
	{ OV8830_8BIT, { 0x3808 }, 0x00 }, /* O/p Binning + Scaling 216x176 */
	{ OV8830_8BIT, { 0x3809 }, 0xd8 },
	{ OV8830_8BIT, { 0x380a }, 0x00 },
	{ OV8830_8BIT, { 0x380b }, 0xb0 },
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3814 }, 0x31 },
	{ OV8830_8BIT, { 0x3815 }, 0x31 },
	{ OV8830_8BIT, { 0x3820 }, 0x11 },
	{ OV8830_8BIT, { 0x3821 }, 0x0f },
	{ OV8830_TOK_TERM, {0}, 0}
};

static const struct ov8830_reg ov8830_QVGA_strong_dvs[] = {
	{ OV8830_8BIT, { 0x3800 }, 0x00 },
	{ OV8830_8BIT, { 0x3801 }, 0x08 },
	{ OV8830_8BIT, { 0x3802 }, 0x00 },
	{ OV8830_8BIT, { 0x3803 }, 0x08 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xd7 },
	{ OV8830_8BIT, { 0x3806 }, 0x09 }, /* 8, 3287, 8, 2471 Binning on*/
	{ OV8830_8BIT, { 0x3807 }, 0xa7 }, /* Actual Size 3280x2464 */
	{ OV8830_8BIT, { 0x3808 }, 0x01 }, /* 408x308 Binning+Scaling */
	{ OV8830_8BIT, { 0x3809 }, 0x98 },
	{ OV8830_8BIT, { 0x380a }, 0x01 },
	{ OV8830_8BIT, { 0x380b }, 0x34 },
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3814 }, 0x31 },
	{ OV8830_8BIT, { 0x3815 }, 0x31 },
	{ OV8830_8BIT, { 0x3820 }, 0x11 },
	{ OV8830_8BIT, { 0x3821 }, 0x0f },
	{ OV8830_TOK_TERM, {0}, 0}
};

static const struct ov8830_reg ov8830_VGA_strong_dvs[] = {
	{ OV8830_8BIT, { 0x3800 }, 0x00 },
	{ OV8830_8BIT, { 0x3801 }, 0x08 },
	{ OV8830_8BIT, { 0x3802 }, 0x00 },
	{ OV8830_8BIT, { 0x3803 }, 0x08 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xd7 },
	{ OV8830_8BIT, { 0x3806 }, 0x09 }, /* 8, 3287, 8, 2471 Binning on*/
	{ OV8830_8BIT, { 0x3807 }, 0xa7 }, /* Actual Size 3280x2464 */
	{ OV8830_8BIT, { 0x3808 }, 0x03 }, /* 820x616 Binning + Scaling */
	{ OV8830_8BIT, { 0x3809 }, 0x34 },
	{ OV8830_8BIT, { 0x380a }, 0x02 },
	{ OV8830_8BIT, { 0x380b }, 0x68 },
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3814 }, 0x31 },
	{ OV8830_8BIT, { 0x3815 }, 0x31 },
	{ OV8830_8BIT, { 0x3820 }, 0x11 },
	{ OV8830_8BIT, { 0x3821 }, 0x0f },
	{ OV8830_TOK_TERM, {0}, 0}
};

static struct ov8830_reg const ov8830_480p_strong_dvs[] = {
	{ OV8830_8BIT, { 0x3800 }, 0x01 },
	{ OV8830_8BIT, { 0x3801 }, 0x09 },
	{ OV8830_8BIT, { 0x3802 }, 0x01 },
	{ OV8830_8BIT, { 0x3803 }, 0x40 },
	{ OV8830_8BIT, { 0x3804 }, 0x0b },
	{ OV8830_8BIT, { 0x3805 }, 0xd6 },
	{ OV8830_8BIT, { 0x3806 }, 0x08 },
	{ OV8830_8BIT, { 0x3807 }, 0x73 }, /* TODO! 2766 x 1844 */
	{ OV8830_8BIT, { 0x3808 }, 0x03 }, /* 936x602 Binning + Scaling */
	{ OV8830_8BIT, { 0x3809 }, 0xa8 },
	{ OV8830_8BIT, { 0x380a }, 0x02 },
	{ OV8830_8BIT, { 0x380b }, 0x5a },
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3814 }, 0x31 },
	{ OV8830_8BIT, { 0x3815 }, 0x31 },
	{ OV8830_8BIT, { 0x3820 }, 0x11 }, /* Binning on */
	{ OV8830_8BIT, { 0x3821 }, 0x0f },
	{ OV8830_TOK_TERM, {0}, 0}
};

static struct ov8830_reg const ov8830_720p_strong_dvs[] = {
	{ OV8830_8BIT, { 0x3800 }, 0x00 },
	{ OV8830_8BIT, { 0x3801 }, 0x0c },
	{ OV8830_8BIT, { 0x3802 }, 0x01 },
	{ OV8830_8BIT, { 0x3803 }, 0x40 },
	{ OV8830_8BIT, { 0x3804 }, 0x0c },
	{ OV8830_8BIT, { 0x3805 }, 0xd3 },
	{ OV8830_8BIT, { 0x3806 }, 0x08 },
	{ OV8830_8BIT, { 0x3807 }, 0x73 },
	{ OV8830_8BIT, { 0x3808 }, 0x06 }, /* O/p 1568*880 Bin+Scale */
	{ OV8830_8BIT, { 0x3809 }, 0x20 },
	{ OV8830_8BIT, { 0x380a }, 0x03 },
	{ OV8830_8BIT, { 0x380b }, 0x70 },
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3814 }, 0x31 },
	{ OV8830_8BIT, { 0x3815 }, 0x31 },
	{ OV8830_8BIT, { 0x3820 }, 0x11 },
	{ OV8830_8BIT, { 0x3821 }, 0x0f },
	{ OV8830_TOK_TERM, {0}, 0}
};

static const struct ov8830_reg ov8830_1080p_strong_dvs[] = {
	{ OV8830_8BIT, { 0x3800 }, 0x01 },
	{ OV8830_8BIT, { 0x3801 }, 0xd8 },
	{ OV8830_8BIT, { 0x3802 }, 0x02 },
	{ OV8830_8BIT, { 0x3803 }, 0x36 },
	{ OV8830_8BIT, { 0x3804 }, 0x0a },
	{ OV8830_8BIT, { 0x3805 }, 0xff },
	{ OV8830_8BIT, { 0x3806 }, 0x07 },
	{ OV8830_8BIT, { 0x3807 }, 0x65 }, /* 2344 x 1328 Crop */
	{ OV8830_8BIT, { 0x3808 }, 0x09 }, /* 2336x1320 DVS O/p */
	{ OV8830_8BIT, { 0x3809 }, 0x20 },
	{ OV8830_8BIT, { 0x380a }, 0x05 },
	{ OV8830_8BIT, { 0x380b }, 0x28 },
	{ OV8830_8BIT, { 0x3810 }, 0x00 },
	{ OV8830_8BIT, { 0x3811 }, 0x04 },
	{ OV8830_8BIT, { 0x3812 }, 0x00 },
	{ OV8830_8BIT, { 0x3813 }, 0x04 },
	{ OV8830_8BIT, { 0x3814 }, 0x11 },
	{ OV8830_8BIT, { 0x3815 }, 0x11 },
	{ OV8830_8BIT, { 0x3820 }, 0x10 },
	{ OV8830_8BIT, { 0x3821 }, 0x0e },
	{ OV8830_TOK_TERM, {0}, 0}
};

static struct ov8830_resolution ov8830_res_preview[] = {
	{
		 .desc = "OV8830_PREVIEW_848x616",
		 .width = 848,
		 .height = 616,
		 .used = 0,
		 .regs = ov8830_PREVIEW_848x616,
		 .bin_factor_x = 1,
		 .bin_factor_y = 1,
		 .skip_frames = 0,
		 .fps_options = {
			{
				.fps = 30,
				.pixels_per_line = 3608,
				.lines_per_frame = 2773,
			},
			{
			}
		}
	},
	{
		 .desc = "ov8830_wide_preview",
		 .width = 1280,
		 .height = 720,
		 .used = 0,
		 .regs = ov8830_PREVIEW_WIDE_PREVIEW,
		 .bin_factor_x = 1,
		 .bin_factor_y = 1,
		 .skip_frames = 0,
		 .fps_options = {
			{
				.fps = 30,
				.pixels_per_line = 3608,
				.lines_per_frame = 2773,
			},
			{
			}
		},
	},
	{
		 .desc = "ov8830_cont_cap_qvga",
		 .width = 336,
		 .height = 256,
		 .used = 0,
		 .regs = ov8830_cont_cap_qvga,
		 .bin_factor_x = 1,
		 .bin_factor_y = 1,
		 .skip_frames = 0,
		 .fps_options = {
			{
				.fps = 15,
				.pixels_per_line = 4696,
				.lines_per_frame = 2724,
			},
			{
			}
		},
	},
	{
		 .desc = "ov8830_cont_cap_vga",
		 .width = 656,
		 .height = 496,
		 .used = 0,
		 .regs = ov8830_VGA_STILL,
		 .bin_factor_x = 1,
		 .bin_factor_y = 1,
		 .skip_frames = 0,
		 .fps_options =  {
			{
				.fps = 15,
				.pixels_per_line = 4696,
				.lines_per_frame = 2724,
			},
			{
			}
		}
	},
	{
		 .desc = "OV8830_PREVIEW1600x1200",
		 .width = 1632,
		 .height = 1224,
		 .used = 0,
		 .regs = ov8830_PREVIEW_1632x1224,
		 .bin_factor_x = 1,
		 .bin_factor_y = 1,
		 .skip_frames = 0,
		 .fps_options = {
			{
				.fps = 30,
				.pixels_per_line = 3608,
				.lines_per_frame = 2773,
			},
			{
			}
		},
	},
	{
		 .desc = "ov8830_cont_cap_720P",
		 .width = 1296,
		 .height = 736,
		 .used = 0,
		 .regs = ov8830_cont_cap_720P,
		 .bin_factor_x = 1,
		 .bin_factor_y = 1,
		 .skip_frames = 0,
		 .fps_options =  {
			{
				.fps = 15,
				.pixels_per_line = 4696,
				.lines_per_frame = 2724,
			},
			{
			}
		},
	},
	{
		 .desc = "ov8830_cont_cap_1M",
		 .width = 1040,
		 .height = 784,
		 .used = 0,
		 .regs = ov8830_1M_STILL,
		 .bin_factor_x = 0,
		 .bin_factor_y = 0,
		 .skip_frames = 0,
		 .fps_options =  {
			{
				.fps = 15,
				.pixels_per_line = 4696,
				.lines_per_frame = 2724,
			},
			{
			}
		},
	},
	{
		 .desc = "ov8830_cont_cap_1080P",
		 .width = 1936,
		 .height = 1104,
		 .used = 0,
		 .regs = ov8830_1080P_STILL,
		 .bin_factor_x = 0,
		 .bin_factor_y = 0,
		 .skip_frames = 0,
		 .fps_options =  {
			{
				.fps = 15,
				.pixels_per_line = 4696,
				.lines_per_frame = 2724,
			},
			{
			}
		},
	},
	{
		.desc = "ov8830_cont_cap_3M",
		.width = 2064,
		.height = 1552,
		.used = 0,
		.regs = ov8830_3M_STILL,
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.skip_frames = 0,
		.fps_options =  {
			{
				.fps = 15,
				.pixels_per_line = 4696,
				.lines_per_frame = 2724,
			},
			{
			}
		},
	},
	{
		.desc = "ov8830_cont_cap_5M",
		.width = 2576,
		.height = 1936,
		.used = 0,
		.regs = ov8830_5M_STILL,
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.skip_frames = 0,
		.fps_options =  {
			{
				.fps = 15,
				.pixels_per_line = 4696,
				.lines_per_frame = 2724,
			},
			{
			}
		},
	},
	{
		 .desc = "ov8830_cont_cap_6M",
		 .width = 3280,
		 .height = 1852,
		 .used = 0,
		 .regs = ov8830_6M_STILL,
		 .bin_factor_x = 0,
		 .bin_factor_y = 0,
		 .skip_frames = 0,
		 .fps_options =  {
			{
				.fps = 15,
				.pixels_per_line = 4696,
				.lines_per_frame = 2724,
			},
			{
			}
		},
	},
	{
		.desc = "ov8830_cont_cap_8M",
		.width = 3280,
		.height = 2464,
		.used = 0,
		.regs = ov8830_8M_STILL,
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.skip_frames = 0,
		.fps_options = {
			{
				.fps = 15,
				.pixels_per_line = 4464,
				.lines_per_frame = 2867,
			},
			{
			}
		},
	},
};

static struct ov8830_resolution ov8830_res_still[] = {
	{
		 .desc = "STILL_VGA",
		 .width = 656,
		 .height = 496,
		 .used = 0,
		 .regs = ov8830_VGA_STILL,
		 .bin_factor_x = 1,
		 .bin_factor_y = 1,
		 .skip_frames = 1,
		 .fps_options =  {
			{
				.fps = 15,
				.pixels_per_line = 4696,
				.lines_per_frame = 2724,
			},
			{
			}
		},
	},
	{
		 .desc = "STILL_1080P",
		 .width = 1936,
		 .height = 1104,
		 .used = 0,
		 .regs = ov8830_1080P_STILL,
		 .bin_factor_x = 0,
		 .bin_factor_y = 0,
		 .skip_frames = 1,
		 .fps_options =  {
			{
				.fps = 15,
				.pixels_per_line = 4696,
				.lines_per_frame = 2724,
			},
			{
			}
		},
	},
	{
		 .desc = "STILL_1M",
		 .width = 1040,
		 .height = 784,
		 .used = 0,
		 .regs = ov8830_1M_STILL,
		 .bin_factor_x = 0,
		 .bin_factor_y = 0,
		 .skip_frames = 1,
		 .fps_options =  {
			{
				.fps = 15,
				.pixels_per_line = 4696,
				.lines_per_frame = 2724,
			},
			{
			}
		},
	},
	{
		 .desc = "STILL_2M",
		 .width = 1640,
		 .height = 1232,
		 .used = 0,
		 .regs = ov8830_2M_STILL,
		 .bin_factor_x = 0,
		 .bin_factor_y = 0,
		 .skip_frames = 1,
		 .fps_options =  {
			{
				.fps = 15,
				.pixels_per_line = 4696,
				.lines_per_frame = 2724,
			},
			{
			}
		},
	},
	{
		.desc = "STILL_3M",
		.width = 2064,
		.height = 1552,
		.used = 0,
		.regs = ov8830_3M_STILL,
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.skip_frames = 1,
		.fps_options =  {
			{
				.fps = 15,
				.pixels_per_line = 4696,
				.lines_per_frame = 2724,
			},
			{
			}
		},
	},
	{
		.desc = "STILL_5M",
		.width = 2576,
		.height = 1936,
		.used = 0,
		.regs = ov8830_5M_STILL,
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.skip_frames = 1,
		.fps_options =  {
			{
				.fps = 15,
				.pixels_per_line = 4696,
				.lines_per_frame = 2724,
			},
			{
			}
		},
	},
	{
		 .desc = "STILL_6M",
		 .width = 3280,
		 .height = 1852,
		 .used = 0,
		 .regs = ov8830_6M_STILL,
		 .bin_factor_x = 0,
		 .bin_factor_y = 0,
		 .skip_frames = 1,
		 .fps_options =  {
			{
				.fps = 15,
				.pixels_per_line = 4696,
				.lines_per_frame = 2724,
			},
			{
			}
		},
	},
	{
		.desc = "STILL_8M",
		.width = 3280,
		.height = 2464,
		.used = 0,
		.regs = ov8830_8M_STILL,
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.skip_frames = 1,
		.fps_options = {
			{
				.fps = 15,
				.pixels_per_line = 4464,
				.lines_per_frame = 2867,
			},
			{
			}
		},
	},
};

static struct ov8830_resolution ov8830_res_video[] = {
	{
		 .desc = "QCIF_strong_dvs",
		 .width = 216,
		 .height = 176,
		 .used = 0,
		 .regs = ov8830_QCIF_strong_dvs,
		 .bin_factor_x = 1,
		 .bin_factor_y = 1,
		 .skip_frames = 0,
		 .fps_options = {
			{
				 .fps = 30,
				 .pixels_per_line = 4128,
				 .lines_per_frame = 1550,
			},
			{
			}
		},
	},
	{
		 .desc = "QVGA_strong_dvs",
		 .width = 408,
		 .height = 308,
		 .used = 0,
		 .regs = ov8830_QVGA_strong_dvs,
		 .bin_factor_x = 1,
		 .bin_factor_y = 1,
		 .skip_frames = 0,
		 .fps_options = {
			{
				 .fps = 30,
				 .pixels_per_line = 4128,
				 .lines_per_frame = 1550,
			},
			{
			}
		},
	},
	{
		 .desc = "VGA_strong_dvs",
		 .width = 820,
		 .height = 616,
		 .used = 0,
		 .regs = ov8830_VGA_strong_dvs,
		 .bin_factor_x = 1,
		 .bin_factor_y = 1,
		 .skip_frames = 0,
		 .fps_options = {
			{
				 .fps = 30,
				 .pixels_per_line = 4128,
				 .lines_per_frame = 1550,
			},
			{
			}
		},
	},
	{
		.desc = "480p_strong_dvs",
		.width = 936,
		.height = 602,
		.regs = ov8830_480p_strong_dvs,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.skip_frames = 0,
		.fps_options = {
			{
				 .fps = 30,
				 .pixels_per_line = 4128,
				 .lines_per_frame = 1550,
			},
			{
			}
		},
	},
	{
		 .desc = "720p_strong_dvs",
		 .width = 1568,
		 .height = 880,
		 .used = 0,
		 .regs = ov8830_720p_strong_dvs,
		 .bin_factor_x = 1,
		 .bin_factor_y = 1,
		 .skip_frames = 0,
		 .fps_options = {
			{
				 .fps = 30,
				 .pixels_per_line = 4128,
				 .lines_per_frame = 1550,
			},
			{
			}
		},
	},
	{
		 .desc = "MODE1920x1080",
		 .width = 2336,
		 .height = 1320,
		 .used = 0,
		 .regs = ov8830_1080p_strong_dvs,
		 .bin_factor_x = 0,
		 .bin_factor_y = 0,
		 .skip_frames = 0,
		 .fps_options = {
			{
				 .fps = 30,
				 .pixels_per_line = 4100,
				 .lines_per_frame = 1561,
			},
			{
			}
		},
	},
};

#endif
