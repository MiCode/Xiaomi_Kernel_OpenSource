/*
 * Support for S5K6B2YX camera sensor.
 *
 * Copyright (c) 2014 Intel Corporation. All Rights Reserved.
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

#ifndef __S5K6B2YX_H__
#define __S5K6B2YX_H__
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
#include <media/v4l2-ctrls.h>

#define S5K6B2YX_NAME	"s5k6b2yx"
#define V4L2_IDENT_S5K6B2YX 8245

/* Defines for register writes and register array processing */
#define S5K6B2YX_BYTE_MAX	30
#define S5K6B2YX_SHORT_MAX	16
#define I2C_MSG_LENGTH		0x2
#define I2C_RETRY_COUNT		5

#define S5K6B2YX_TEST_PATTERN_MODE	0x0601

#define S5K6B2YX_HFLIP_BIT	0x1
#define S5K6B2YX_VFLIP_BIT	0x2
#define S5K6B2YX_VFLIP_OFFSET	1
#define S5K6B2YX_IMG_ORIENTATION 0x0101

#define I2C_RETRY_COUNT		5
#define MAX_FMTS		1

#define S5K6B2YX_PID_LOW	0x1
#define S5K6B2YX_PID_HIGH	0x0
#define S5K6B2YX_REV		0x1
#define S5K6B2YX_MOD_ID		0x6B20

#define S5K6B2YX_RES_WIDTH_MAX	1936
#define S5K6B2YX_RES_HEIGHT_MAX	1096

#define S5K6B2YX_FINE_INTEGRATION_TIME		0x0200
#define S5K6B2YX_COARSE_INTEGRATION_TIME	0x0202
#define S5K6B2YX_GLOBAL_GAIN			0x0204

#define S5K6B2YX_FINE_INTG_TIME_MIN 0
#define S5K6B2YX_FINE_INTG_TIME_MAX_MARGIN 0
#define S5K6B2YX_COARSE_INTEGRATION_TIME_MARGIN	6
#define S5K6B2YX_COARSE_INTEGRATION_TIME_MIN	1

#define S5K6B2YX_MAX_EXPOSURE_SUPPORTED		(0xffff - S5K6B2YX_COARSE_INTEGRATION_TIME_MARGIN)
#define S5K6B2YX_MAX_GLOBAL_GAIN_SUPPORTED	0x0200
#define S5K6B2YX_MIN_GLOBAL_GAIN_SUPPORTED	0x0020

#define S5K6B2YX_INTG_BUF_COUNT		2

#define S5K6B2YX_VT_PIX_CLK_DIV		0x0300
#define S5K6B2YX_VT_SYS_CLK_DIV		0x0302
#define S5K6B2YX_PRE_PLL_CLK_DIV	0x0304
#define S5K6B2YX_PLL_MULTIPLIER		0x0306
#define S5K6B2YX_FRAME_LENGTH_LINES	0x0340

#define S5K6B2YX_MCLK	192

#define S5K6B2YX_HORIZONTAL_START_H	0x0344
#define S5K6B2YX_VERTICAL_START_H	0x0346
#define S5K6B2YX_HORIZONTAL_END_H	0x0348
#define S5K6B2YX_VERTICAL_END_H		0x034a
#define S5K6B2YX_HORIZONTAL_OUTPUT_SIZE_H	0x034c
#define S5K6B2YX_VERTICAL_OUTPUT_SIZE_H		0x034e

/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define S5K6B2YX_FOCAL_LENGTH_DEFAULT	0x14a0064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define S5K6B2YX_F_NUMBER_DEFAULT	0x18000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define S5K6B2YX_F_NUMBER_RANGE	0x180a180a

#define S5K6B2YX_BIN_FACTOR_MAX	2

/* Defines for lens/VCM */
#define S5K6B2YX_FOCAL_LENGTH_NUM	185	/* 1.85 mm */
#define S5K6B2YX_FOCAL_LENGTH_DEM	100
#define S5K6B2YX_F_NUMBER_DEFAULT_NUM	24	/*  F/2.4 */
#define S5K6B2YX_F_NUMBER_DEM	10

#define S5K6B2YX_INVALID_CONFIG	0xffffffff

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

enum s5k6b2yx_tok_type {
	S5K6B2YX_8BIT  = 0x0001,
	S5K6B2YX_16BIT = 0x0002,
	S5K6B2YX_RMW   = 0x0010,
	S5K6B2YX_TOK_TERM   = 0xf000,	/* terminating token for reg list */
	S5K6B2YX_TOK_DELAY  = 0xfe00, /* delay token for reg list */
	S5K6B2YX_TOK_MASK = 0xfff0
};

enum s5k6b2yx_mode {
	CAM_HW_STBY = 0, /* hw standby mode */
	CAM_SW_STBY, /* sw standby mode */
	CAM_VIS_STBY /* low power vision sening standby mode */
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

struct s5k6b2yx_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;
	struct camera_sensor_platform_data *platform_data;
	struct mutex input_lock; /* serialize sensor's ioctl */
	struct s5k6b2yx_vcm *vcm_driver;
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
	u8 sensor_revision;
	enum s5k6b2yx_mode mode;

	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *link_freq;
};

/**
 * struct s5k6b2yx_reg - MI sensor  register format
 * @type: type of the register
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 *
 * Define a structure for sensor register initialization values
 */
struct s5k6b2yx_reg {
	enum s5k6b2yx_tok_type type;
	u16 sreg;
	u32 val;	/* @set value for read/mod/write, @mask */
};

#define to_s5k6b2yx_sensor(x) container_of(x, struct s5k6b2yx_device, sd)

#define S5K6B2YX_MAX_WRITE_BUF_SIZE	30
struct s5k6b2yx_write_buffer {
	u16 addr;
	u8 data[S5K6B2YX_MAX_WRITE_BUF_SIZE];
};

struct s5k6b2yx_write_ctrl {
	int index;
	struct s5k6b2yx_write_buffer buffer;
};

struct s5k6b2yx_format_struct {
	u8 *desc;
	struct regval_list *regs;
	u32 pixelformat;
};

struct s5k6b2yx_resolution {
	u8 *desc;
	const struct s5k6b2yx_reg *regs;
	int res;
	int width;
	int height;
	int fps;
	unsigned short pixels_per_line;
	unsigned short lines_per_frame;
	u8 bin_factor_x;
	u8 bin_factor_y;
	bool used;
	enum s5k6b2yx_mode mode;
	u32 skip_frames;
	u32 code;
	int mipi_freq;
};

struct s5k6b2yx_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, int value);
};
/* init settings */
static struct s5k6b2yx_reg const s5k6b2yx_init_config[] = {
	/* Vendor specific */
	{ S5K6B2YX_8BIT, 0x31d3, 0x01 }, /* efuse read en */
	/* [4]corr_en[3:2]gain_b_sel,[1:0]gain_r_sel */
	{ S5K6B2YX_8BIT, 0x3426, 0x3a },
	{ S5K6B2YX_8BIT, 0x340d, 0x30 }, /* efuse clock off */

	{ S5K6B2YX_8BIT, 0x3067, 0x25 }, /* adc_sat[mV]=617mV */
	{ S5K6B2YX_8BIT, 0x307d, 0x08 }, /* dbr_tune_tgs */
	{ S5K6B2YX_8BIT, 0x307e, 0x08 }, /* dbr_tune_rg */
	{ S5K6B2YX_8BIT, 0x307f, 0x08 }, /* dbr_tune_fdb */
	{ S5K6B2YX_8BIT, 0x3080, 0x04 }, /* dbr_tune_ntg */
	{ S5K6B2YX_8BIT, 0x3073, 0x73 }, /* comp1_bias, comp2_bias */
	{ S5K6B2YX_8BIT, 0x3074, 0x45 }, /* pix_bias, pix_bias_boost */
	{ S5K6B2YX_8BIT, 0x3075, 0xd4 }, /* clp_lvl */
	{ S5K6B2YX_8BIT, 0x3085, 0xf0 }, /* rdv_option; LOB_PLA enable */
	{ S5K6B2YX_8BIT, 0x3068, 0x55 }, /* ms[15:8]; x4~ */
	{ S5K6B2YX_8BIT, 0x3069, 0x00 }, /* ms[7:0]; x1~x4 */
	/* cds_option[15:8];[11]ldb nmos sw enable=1 */
	{ S5K6B2YX_8BIT, 0x3063, 0x08 },
	{ S5K6B2YX_8BIT, 0x3064, 0x00 }, /* cds_option[7:0]; */
	/* FD start 2->4 for low lux fluctuation */
	{ S5K6B2YX_8BIT, 0x3010, 0x04 },

	{ S5K6B2YX_8BIT, 0x3247, 0x11 }, /*[4] fadlc_blst_en */
	{ S5K6B2YX_8BIT, 0x3083, 0x00 }, /* blst_en_cintr = 16 */
	{ S5K6B2YX_8BIT, 0x3084, 0x10 },

	/* PLL Setting: ext_clk = 19.2MHz; PLL output = 744MHz */
	{ S5K6B2YX_8BIT, 0x0305, 0x04 }, /* pll_pre_pre_div = 4 */
	{ S5K6B2YX_8BIT, 0x0306, 0x00 },
	{ S5K6B2YX_8BIT, 0x0307, 0x9b }, /* pll_multiplier = 155 */

	/* Vendor specific */
	{ S5K6B2YX_8BIT, 0x3351, 0x02 },
	{ S5K6B2YX_8BIT, 0x3352, 0xdc },
	{ S5K6B2YX_8BIT, 0x3353, 0x00 },
	{ S5K6B2YX_8BIT, 0x3354, 0x00 },

	/* others */
	/* [2]dphy_en1, [1]dphy_en0, [0] dhpy_en_clk */
	{ S5K6B2YX_8BIT, 0x7339, 0x03 },
	{ S5K6B2YX_8BIT, 0x0202, 0x03 },
	{ S5K6B2YX_8BIT, 0x0203, 0x88 }, /* TBD: Coarse_integration_time */
	{ S5K6B2YX_8BIT, 0x0204, 0x00 },
	{ S5K6B2YX_8BIT, 0x0205, 0x2a }, /* TBD: Analogue_gain_code_global */




	{S5K6B2YX_TOK_TERM, 0, 0}
};

/* Stream mode */
static struct s5k6b2yx_reg const s5k6b2yx_suspend[] = {
	{S5K6B2YX_8BIT, 0x0100, 0x0 },
	{S5K6B2YX_TOK_TERM, 0, 0 },
};

static struct s5k6b2yx_reg const s5k6b2yx_streaming[] = {
	{S5K6B2YX_8BIT, 0x0100, 0x1 },
	{S5K6B2YX_TOK_TERM, 0, 0 },
};

static struct s5k6b2yx_reg const s5k6b2yx_vis_suspend[] = {
	{S5K6B2YX_8BIT, 0x4100, 0x0 },
	{S5K6B2YX_TOK_TERM, 0, 0 }
};

static struct s5k6b2yx_reg const s5k6b2yx_vis_streaming[] = {
	{S5K6B2YX_8BIT, 0x4100, 0x1 },
	{ S5K6B2YX_TOK_TERM, 0, 0}
};

/* GROUPED_PARAMETER_HOLD */
static struct s5k6b2yx_reg const s5k6b2yx_param_hold[] = {
	{ S5K6B2YX_8BIT,  0x0104, 0x1 },
	{ S5K6B2YX_TOK_TERM, 0, 0 }
};
static struct s5k6b2yx_reg const s5k6b2yx_param_update[] = {
	{ S5K6B2YX_8BIT,  0x0104, 0x0 },
	{ S5K6B2YX_TOK_TERM, 0, 0 }
};

/* Settings */
static struct s5k6b2yx_reg const s5k6b2yx_184x104_15fps[] = {
	{ S5K6B2YX_8BIT, 0x4307, 0xB7}, /* pll_multiplier */
	{ S5K6B2YX_8BIT, 0x6030, 0x13}, /* EXTCLK_MHz */
	{ S5K6B2YX_8BIT, 0x6031, 0x37},
	{ S5K6B2YX_8BIT, 0x3412, 0x4B}, /* streaming_enable_time */
	{ S5K6B2YX_8BIT, 0x3413, 0x13},
	{ S5K6B2YX_8BIT, 0x7412, 0x07}, /* streaming_enable_time_alv */
	{ S5K6B2YX_8BIT, 0x7413, 0x80},

	/* 8bit mode */
	{ S5K6B2YX_8BIT, 0x7030, 0x0E},
	{ S5K6B2YX_8BIT, 0x7031, 0x2F},

	/* Analog Tuning */
	{ S5K6B2YX_8BIT, 0x7067, 0x00}, /* adc_sat_alv (392mV) (20120807) */
	{ S5K6B2YX_8BIT, 0x7074, 0x22}, /* pix_bias_alv, pix_bias_boost_alv */


	/* Dark Tuning */
	{ S5K6B2YX_8BIT, 0x7402, 0x1F}, /* data_depedestal_adlc_alv */
	{ S5K6B2YX_8BIT, 0x7403, 0xC0},
	{ S5K6B2YX_8BIT, 0x7247, 0x01}, /* adlc_option (20121116) */

	/* Remove Dark Band (20121031) */
	/* streaming_enable_time_alv (103.9usec) */
	{ S5K6B2YX_8BIT, 0x7412, 0x09},
	{ S5K6B2YX_8BIT, 0x7413, 0xB9},
	{ S5K6B2YX_8BIT, 0x7430, 0x05}, /* cintc_default_1_alv */
	{ S5K6B2YX_8BIT, 0x7432, 0x02}, /* cintc_default_2_alv */
	{ S5K6B2YX_8BIT, 0x7433, 0x32},

	/* Remove  Sun spot (20120807) */
	{ S5K6B2YX_8BIT, 0x7075, 0x3D}, /* clp_lvl_alv */

	/* Remove CFPN (20120830) -> (20121026 EVT1) */
	{ S5K6B2YX_8BIT, 0x7066, 0x09}, /* off_rst_alv */


	/* AE setting (20121025 EVT1) */
	/* weight */
	{ S5K6B2YX_8BIT, 0x6000, 0x01},
	{ S5K6B2YX_8BIT, 0x6001, 0x10},
	{ S5K6B2YX_8BIT, 0x6002, 0x14},
	{ S5K6B2YX_8BIT, 0x6003, 0x41},
	{ S5K6B2YX_8BIT, 0x6004, 0x14},
	{ S5K6B2YX_8BIT, 0x6005, 0x41},
	{ S5K6B2YX_8BIT, 0x6006, 0x01},
	{ S5K6B2YX_8BIT, 0x6007, 0x10},

	/* number of pixel */
	{ S5K6B2YX_8BIT, 0x5030, 0x1C},
	{ S5K6B2YX_8BIT, 0x5031, 0x08},

	/* Speed */
	{ S5K6B2YX_8BIT, 0x5034, 0x00},

	/* Innner Target Tolerance */
	{ S5K6B2YX_8BIT, 0x503F, 0x03},

	/* patch height (20121116) */
	{ S5K6B2YX_8BIT, 0x6015, 0x19},

	/* G + R Setting (20120813) */
	/* Vision Senser Data = 0.5*Gr + 0.5*R */
	{ S5K6B2YX_8BIT, 0x6029, 0x02}, /* [2:0] : 1bit integer, 2bit fraction*/
	{ S5K6B2YX_8BIT, 0x602A, 0x02}, /* [2:0] : 1bit integer, 2bit fraction*/


	/* For Analog Gain 16x (20120904) */
	{ S5K6B2YX_8BIT, 0x7018, 0xCF},
	{ S5K6B2YX_8BIT, 0x7019, 0xDB},
	{ S5K6B2YX_8BIT, 0x702A, 0x8D},
	{ S5K6B2YX_8BIT, 0x702B, 0x60},
	{ S5K6B2YX_8BIT, 0x5035, 0x02}, /* analog gain max */


	/* BIT_RATE_MBPS_alv (585Mbps) */
	{ S5K6B2YX_8BIT, 0x7351, 0x02},
	{ S5K6B2YX_8BIT, 0x7352, 0x49},
	{ S5K6B2YX_8BIT, 0x7353, 0x00},
	{ S5K6B2YX_8BIT, 0x7354, 0x00},
	/* [2]dphy_en1, [1]dphy_en0, [0] dhpy_en_clk */
	{ S5K6B2YX_8BIT, 0x7339, 0x03},
#ifdef VISION_MODE_TEST_PATTERN
	{ S5K6B2YX_8BIT, 0x7203, 0x42}, /* to enable test pattern */
#endif

	{ S5K6B2YX_TOK_TERM, 0, 0 }
};

static struct s5k6b2yx_reg const s5k6b2yx_1936x1096_30fps[] = {
    /* Vendor specific */
	{ S5K6B2YX_8BIT, 0x31d3, 0x01 }, /* efuse read en */
	/* [4]corr_en[3:2]gain_b_sel,[1:0]gain_r_sel */
	{ S5K6B2YX_8BIT, 0x3426, 0x3a },
	{ S5K6B2YX_8BIT, 0x340d, 0x30 }, /* efuse clock off */

	{ S5K6B2YX_8BIT, 0x3067, 0x25 }, /* adc_sat[mV]=617mV */
	{ S5K6B2YX_8BIT, 0x307d, 0x08 }, /* dbr_tune_tgs */
	{ S5K6B2YX_8BIT, 0x307e, 0x08 }, /* dbr_tune_rg */
	{ S5K6B2YX_8BIT, 0x307f, 0x08 }, /* dbr_tune_fdb */
	{ S5K6B2YX_8BIT, 0x3080, 0x04 }, /* dbr_tune_ntg */
	{ S5K6B2YX_8BIT, 0x3073, 0x73 }, /* comp1_bias, comp2_bias */
	{ S5K6B2YX_8BIT, 0x3074, 0x45 }, /* pix_bias, pix_bias_boost */
	{ S5K6B2YX_8BIT, 0x3075, 0xd4 }, /* clp_lvl */
	{ S5K6B2YX_8BIT, 0x3085, 0xf0 }, /* rdv_option; LOB_PLA enable */
	{ S5K6B2YX_8BIT, 0x3068, 0x55 }, /* ms[15:8]; x4~ */
	{ S5K6B2YX_8BIT, 0x3069, 0x00 }, /* ms[7:0]; x1~x4 */
	/* cds_option[15:8];[11]ldb nmos sw enable=1 */
	{ S5K6B2YX_8BIT, 0x3063, 0x08 },
	{ S5K6B2YX_8BIT, 0x3064, 0x00 }, /* cds_option[7:0]; */
	/* FD start 2->4 for low lux fluctuation */
	{ S5K6B2YX_8BIT, 0x3010, 0x04 },

	{ S5K6B2YX_8BIT, 0x3247, 0x11 }, /*[4] fadlc_blst_en */
	{ S5K6B2YX_8BIT, 0x3083, 0x00 }, /* blst_en_cintr = 16 */
	{ S5K6B2YX_8BIT, 0x3084, 0x10 },

	/* PLL Setting: ext_clk = 19.2MHz; PLL output = 744MHz */
	{ S5K6B2YX_8BIT, 0x0305, 0x04 }, /* pll_pre_pre_div = 4 */
	{ S5K6B2YX_8BIT, 0x0306, 0x00 },
	{ S5K6B2YX_8BIT, 0x0307, 0x9b }, /* pll_multiplier = 155 */

	/* Vendor specific */
	{ S5K6B2YX_8BIT, 0x3351, 0x02 },
	{ S5K6B2YX_8BIT, 0x3352, 0xdc },
	{ S5K6B2YX_8BIT, 0x3353, 0x00 },
	{ S5K6B2YX_8BIT, 0x3354, 0x00 },

	/* others */
	/* [2]dphy_en1, [1]dphy_en0, [0] dhpy_en_clk */
	{ S5K6B2YX_8BIT, 0x7339, 0x03 },
	{ S5K6B2YX_8BIT, 0x0202, 0x03 },
	{ S5K6B2YX_8BIT, 0x0203, 0x88 }, /* TBD: Coarse_integration_time */
	{ S5K6B2YX_8BIT, 0x0204, 0x00 },
	{ S5K6B2YX_8BIT, 0x0205, 0x2a }, /* TBD: Analogue_gain_code_global */

	/* Resolution Setting */
	{ S5K6B2YX_8BIT, 0x0344, 0x00 }, /* x_addr_start MSB */
	{ S5K6B2YX_8BIT, 0x0345, 0x00 }, /* x_addr_start LSB */
	{ S5K6B2YX_8BIT, 0x0346, 0x00 }, /* y_addr_start MSB */
	{ S5K6B2YX_8BIT, 0x0347, 0x00 }, /* y_addr_start LSB */

	{ S5K6B2YX_8BIT, 0x0348, 0x07 }, /* x_addr_end MSB */
	{ S5K6B2YX_8BIT, 0x0349, 0x8f }, /* x_addr_end LSB */
	{ S5K6B2YX_8BIT, 0x034a, 0x04 }, /* y_addr_end MSB */
	{ S5K6B2YX_8BIT, 0x034b, 0x47 }, /* y_addr_end LSB */

	{ S5K6B2YX_8BIT, 0x034c, 0x07 }, /* x_output_size MSB */
	{ S5K6B2YX_8BIT, 0x034d, 0x90 }, /* x_output_size LSB */
	{ S5K6B2YX_8BIT, 0x034e, 0x04 }, /* y_output_size MSB */
	{ S5K6B2YX_8BIT, 0x034f, 0x48 }, /* y_output_size LSB */

	{ S5K6B2YX_8BIT, 0x0340, 0x04 }, /* frame_length_lines MSB */
	{ S5K6B2YX_8BIT, 0x0341, 0x66 }, /* frame_length_lines LSB */
	{ S5K6B2YX_8BIT, 0x0342, 0x08 }, /* line_length_pck MSB */
	{ S5K6B2YX_8BIT, 0x0343, 0x9b }, /* line_length_pck LSB */
	{ S5K6B2YX_TOK_TERM, 0, 0 }
};

struct s5k6b2yx_resolution s5k6b2yx_res_preview[] = {
	{
		.desc = "s5k6b2yx_184x104_15fps",
		.regs = s5k6b2yx_184x104_15fps,
		.width = 184,
		.height = 104,
		.fps = 15,
		.pixels_per_line = 2203, /* consistent with regs arrays */
		.lines_per_frame = 1126, /* consistent with regs arrays */
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.used = 0,
		.skip_frames = 2,
		.mipi_freq = 292000,
		.code = V4L2_MBUS_FMT_SGRBG8_1X8,
		.mode = CAM_VIS_STBY,
	},
	{
		.desc = "s5k6b2yx_1936x1096_30fps",
		.regs = s5k6b2yx_1936x1096_30fps,
		.width = 1936,
		.height = 1096,
		.fps = 30,
		.pixels_per_line = 2203, /* consistent with regs arrays */
		.lines_per_frame = 1126, /* consistent with regs arrays */
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.used = 0,
		.skip_frames = 2,
		.mipi_freq = 372000,
		.mode = CAM_SW_STBY,
		.code = V4L2_MBUS_FMT_SGRBG10_1X10,
	},
};
#define N_RES_PREVIEW (ARRAY_SIZE(s5k6b2yx_res_preview))

struct s5k6b2yx_resolution s5k6b2yx_res_still[] = {
	{
		.desc = "s5k6b2yx_1936x1096_30fps",
		.regs = s5k6b2yx_1936x1096_30fps,
		.width = 1936,
		.height = 1096,
		.fps = 30,
		.pixels_per_line = 2203, /* consistent with regs arrays */
		.lines_per_frame = 1126, /* consistent with regs arrays */
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.used = 0,
		.skip_frames = 2,
		.mipi_freq = 372000,
		.mode = CAM_SW_STBY,
		.code = V4L2_MBUS_FMT_SGRBG10_1X10,
	},
};
#define N_RES_STILL (ARRAY_SIZE(s5k6b2yx_res_still))

struct s5k6b2yx_resolution s5k6b2yx_res_video[] = {
	{
		.desc = "s5k6b2yx_1936x1096_30fps",
		.regs = s5k6b2yx_1936x1096_30fps,
		.width = 1936,
		.height = 1096,
		.fps = 30,
		.pixels_per_line = 2203, /* consistent with regs arrays */
		.lines_per_frame = 1126, /* consistent with regs arrays */
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.used = 0,
		.skip_frames = 2,
		.mipi_freq = 372000,
		.mode = CAM_SW_STBY,
		.code = V4L2_MBUS_FMT_SGRBG10_1X10,
	},
};
#define N_RES_VIDEO (ARRAY_SIZE(s5k6b2yx_res_video))

struct s5k6b2yx_resolution *s5k6b2yx_res = s5k6b2yx_res_preview;
static int N_RES = N_RES_PREVIEW;

#endif
