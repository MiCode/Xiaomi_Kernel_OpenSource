/*
 * Support for OmniVision S5K6B2YX 1080p HD camera sensor.
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

#ifndef __S5K6B2YX_H__
#define __S5K6B2YX_H__
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
#include <media/v4l2-ctrls.h>

#include <linux/atomisp_platform.h>

#define S5K6B2YX_NAME		"s5k6b2yx"

/* Defines for register writes and register array processing */
#define I2C_MSG_LENGTH		0x2
#define I2C_RETRY_COUNT		5

#define S5K6B2YX_FOCAL_LENGTH_NUM	278	/*2.78mm*/
#define S5K6B2YX_FOCAL_LENGTH_DEM	100
#define S5K6B2YX_F_NUMBER_DEFAULT_NUM	26
#define S5K6B2YX_F_NUMBER_DEM	10

#define MAX_FMTS		1

/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define S5K6B2YX_FOCAL_LENGTH_DEFAULT 0x1160064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define S5K6B2YX_F_NUMBER_DEFAULT 0x1a000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define S5K6B2YX_F_NUMBER_RANGE 0x1a0a1a0a
#define S5K6B2YX_ID	0x6b20

#define S5K6B2YX_FINE_INTG_TIME_MIN 0
#define S5K6B2YX_FINE_INTG_TIME_MAX_MARGIN 0
#define S5K6B2YX_COARSE_INTG_TIME_MIN 1
#define S5K6B2YX_COARSE_INTG_TIME_MAX_MARGIN (0xffff - 6)

/*
 * S5K6B2YX System control registers
 */
#define S5K6B2YX_REG_CHIP_ID_H		0x0000
#define S5K6B2YX_REG_REVISION		0x0002

#define S5K6B2YX_REG_MODE_SELECT	0x0100
#define S5K6B2YX_REG_IMG_ORI		0x0101
#define S5K6B2YX_REG_SOFT_RESET		0x0103
#define S5K6B2YX_REG_GROUND_HOLD	0x0104

#define S5K6B2YX_REG_FINE_INTEG		0x0200
#define S5K6B2YX_REG_COARSE_INTEG	0x0202
#define S5K6B2YX_REG_ANALOG_GAIN	0x0204

#define S5K6B2YX_REG_VT_PIX_CLK_DIV_H	0x0300
#define S5K6B2YX_REG_VT_SYS_CLK_DIV_H	0x0302
#define S5K6B2YX_REG_PRE_PLL_CLK_DIV_H	0x0304
#define S5K6B2YX_REG_PLL_MULTIPLIER_H	0x0306
#define S5K6B2YX_REG_OP_PIX_CLK_DIV_H	0x0308
#define S5K6B2YX_REG_OP_SYS_CLK_DIV_H	0x030a

#define S5K6B2YX_REG_H_CROP_START_H	0x0344
#define S5K6B2YX_REG_V_CROP_START_H	0x0346
#define S5K6B2YX_REG_H_CROP_END_H	0x0348
#define S5K6B2YX_REG_V_CROP_END_H	0x034a
#define S5K6B2YX_REG_H_OUTSIZE_H	0x034c
#define S5K6B2YX_REG_V_OUTSIZE_H	0x034e

#define S5K6B2YX_START_STREAMING	0x01
#define S5K6B2YX_STOP_STREAMING		0x00

struct regval_list {
	u16 reg_num;
	u8 value;
};

struct s5k6b2yx_resolution {
	u8 *desc;
	const struct s5k6b2yx_reg *regs;
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
	int mipi_freq;
};

struct s5k6b2yx_format {
	u8 *desc;
	u32 pixelformat;
	struct s5k6b2yx_reg *regs;
};

struct s5k6b2yx_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, s32 value);
};

/*
 * s5k6b2yx device structure.
 */
struct s5k6b2yx_device {
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

	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *link_freq;
};

enum s5k6b2yx_tok_type {
	S5K6B2YX_8BIT  = 0x0001,
	S5K6B2YX_16BIT = 0x0002,
	S5K6B2YX_32BIT = 0x0004,
	S5K6B2YX_TOK_TERM   = 0xf000,	/* terminating token for reg list */
	S5K6B2YX_TOK_DELAY  = 0xfe00,	/* delay token for reg list */
	S5K6B2YX_TOK_MASK = 0xfff0
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
	u16 reg;
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

static const struct i2c_device_id s5k6b2yx_id[] = {
	{S5K6B2YX_NAME, 0},
	{}
};

/*
 * Register settings for one-time initialization
 */
static struct s5k6b2yx_reg const s5k6b2yx_init[] = {
	/* Vendor specific */
	{S5K6B2YX_8BIT, 0x31d3, 0x01}, /* efuse read en */
	{S5K6B2YX_8BIT, 0x3426, 0x3a}, /* [4]corr_en[3:2]gain_b_sel,[1:0]gain_r_sel */
	{S5K6B2YX_8BIT, 0x340d, 0x30}, /* efuse clock off */

	{S5K6B2YX_8BIT, 0x3067, 0x25}, /* adc_sat[mV]=617mV */
	{S5K6B2YX_8BIT, 0x307d, 0x08}, /* dbr_tune_tgs */
	{S5K6B2YX_8BIT, 0x307e, 0x08}, /* dbr_tune_rg */
	{S5K6B2YX_8BIT, 0x307f, 0x08}, /* dbr_tune_fdb */
	{S5K6B2YX_8BIT, 0x3080, 0x04}, /* dbr_tune_ntg */
	{S5K6B2YX_8BIT, 0x3073, 0x73}, /* comp1_bias, comp2_bias */
	{S5K6B2YX_8BIT, 0x3074, 0x45}, /* pix_bias, pix_bias_boost */
	{S5K6B2YX_8BIT, 0x3075, 0xd4}, /* clp_lvl */
	{S5K6B2YX_8BIT, 0x3085, 0xf0}, /* rdv_option; LOB_PLA enable */
	{S5K6B2YX_8BIT, 0x3068, 0x55}, /* ms[15:8]; x4~ */
	{S5K6B2YX_8BIT, 0x3069, 0x00}, /* ms[7:0]; x1~x4 */
	{S5K6B2YX_8BIT, 0x3063, 0x08}, /* cds_option[15:8];[11]ldb nmos sw enable=1 */
	{S5K6B2YX_8BIT, 0x3064, 0x00}, /* cds_option[7:0]; */
	{S5K6B2YX_8BIT, 0x3010, 0x04}, /* FD start 2->4 for low lux fluctuation */

	{S5K6B2YX_8BIT, 0x3247, 0x11}, /*[4] fadlc_blst_en */
	{S5K6B2YX_8BIT, 0x3083, 0x00}, /* blst_en_cintr = 16 */
	{S5K6B2YX_8BIT, 0x3084, 0x10},

	/* PLL Setting: ext_clk = 19.2MHz; PLL output = 744MHz */
	{S5K6B2YX_8BIT, 0x0305, 0x04}, /* pll_pre_pre_div = 4 */
	{S5K6B2YX_8BIT, 0x0306, 0x00},
	{S5K6B2YX_8BIT, 0x0307, 0x9B}, /* pll_multiplier = 155 */

	/* Vendor specific */
	{S5K6B2YX_8BIT, 0x3351, 0x02},
	{S5K6B2YX_8BIT, 0x3352, 0xdc},
	{S5K6B2YX_8BIT, 0x3353, 0x00},
	{S5K6B2YX_8BIT, 0x3354, 0x00},

	/* others */
	{S5K6B2YX_8BIT, 0x7339, 0x03}, /* [2]dphy_en1, [1]dphy_en0, [0] dhpy_en_clk */

	{S5K6B2YX_8BIT, 0x0202, 0x03},
	{S5K6B2YX_8BIT, 0x0203, 0x88}, /* TBD: Coarse_integration_time */
	{S5K6B2YX_8BIT, 0x0204, 0x00},
	{S5K6B2YX_8BIT, 0x0205, 0x2a}, /* TBD: Analogue_gain_code_global */
	{S5K6B2YX_TOK_TERM, 0, 0},
};
/*
 * Register settings for various resolution
 */
static struct s5k6b2yx_reg const s5k6b2yx_1936_1096_30fps[] = {
	{S5K6B2YX_8BIT, 0x0344, 0x00}, /* x_addr_start MSB */
	{S5K6B2YX_8BIT, 0x0345, 0x00}, /* x_addr_start LSB */
	{S5K6B2YX_8BIT, 0x0346, 0x00}, /* y_addr_start MSB */
	{S5K6B2YX_8BIT, 0x0347, 0x02}, /* y_addr_start LSB */

	{S5K6B2YX_8BIT, 0x0348, 0x07}, /* x_addr_end MSB */
	{S5K6B2YX_8BIT, 0x0349, 0x8F}, /* x_addr_end LSB */
	{S5K6B2YX_8BIT, 0x034a, 0x04}, /* y_addr_end MSB */
	{S5K6B2YX_8BIT, 0x034b, 0x47}, /* y_addr_end LSB */

	{S5K6B2YX_8BIT, 0x034c, 0x07}, /* x_output_size MSB */
	{S5K6B2YX_8BIT, 0x034d, 0x90}, /* x_output_size LSB */
	{S5K6B2YX_8BIT, 0x034e, 0x04}, /* y_output_size MSB */
	{S5K6B2YX_8BIT, 0x034f, 0x48}, /* y_output_size LSB */

	{S5K6B2YX_8BIT, 0x0340, 0x04}, /* frame_length_lines MSB */
	{S5K6B2YX_8BIT, 0x0341, 0x66}, /* frame_length_lines LSB */
	{S5K6B2YX_8BIT, 0x0342, 0x08}, /* line_length_pck MSB */
	{S5K6B2YX_8BIT, 0x0343, 0x9b}, /* line_length_pck LSB */
	{S5K6B2YX_TOK_TERM, 0, 0},
};

struct s5k6b2yx_resolution s5k6b2yx_res_preview[] = {
	{
		.desc = "s5k6b2yx_1936_1096_30fps",
		.width = 1936,
		.height = 1096,
		.fps = 30,
		.pix_clk_freq = 74,
		.used = 0,
		.pixels_per_line = 2203,
		.lines_per_frame = 1126,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = s5k6b2yx_1936_1096_30fps,
		.mipi_freq = 372000,
	},
};
#define N_RES_PREVIEW (ARRAY_SIZE(s5k6b2yx_res_preview))

struct s5k6b2yx_resolution s5k6b2yx_res_still[] = {
	{
		.desc = "s5k6b2yx_1936_1096_30fps",
		.width = 1936,
		.height = 1096,
		.fps = 30,
		.pix_clk_freq = 74,
		.used = 0,
		.pixels_per_line = 2203,
		.lines_per_frame = 1126,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = s5k6b2yx_1936_1096_30fps,
		.mipi_freq = 372000,
	},
};
#define N_RES_STILL (ARRAY_SIZE(s5k6b2yx_res_still))

struct s5k6b2yx_resolution s5k6b2yx_res_video[] = {
	{
		.desc = "s5k6b2yx_1936_1096_30fps",
		.width = 1936,
		.height = 1096,
		.fps = 30,
		.pix_clk_freq = 74,
		.used = 0,
		.pixels_per_line = 2203,
		.lines_per_frame = 1126,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = s5k6b2yx_1936_1096_30fps,
		.mipi_freq = 372000,
	},
};
#define N_RES_VIDEO (ARRAY_SIZE(s5k6b2yx_res_video))

static struct s5k6b2yx_resolution *s5k6b2yx_res = s5k6b2yx_res_preview;
static int N_RES = N_RES_PREVIEW;
#endif
