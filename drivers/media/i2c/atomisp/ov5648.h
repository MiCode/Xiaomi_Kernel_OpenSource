/*
 * Support for OmniVision OV5648 5M camera sensor.
 * Based on OmniVision OV2722 driver.
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

#ifndef __OV5648_H__
#define __OV5648_H__
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
#include <linux/acpi.h>
#include <linux/atomisp_platform.h>

#define OV5648_NAME		"ov5648"

/* Defines for register writes and register array processing */
#define I2C_MSG_LENGTH		0x2
#define I2C_RETRY_COUNT		5

#define OV5648_FOCAL_LENGTH_NUM	334	/*3.34mm */
#define OV5648_FOCAL_LENGTH_DEM	100
#define OV5648_F_NUMBER_DEFAULT_NUM	28
#define OV5648_F_NUMBER_DEM	10

#define MAX_FMTS		1

/* sensor_mode_data read_mode adaptation */
#define OV5648_READ_MODE_BINNING_ON	0x0400
#define OV5648_READ_MODE_BINNING_OFF	0x00
#define OV5648_INTEGRATION_TIME_MARGIN	8

#define OV5648_MAX_EXPOSURE_VALUE	0xFFF1
#define OV5648_MAX_GAIN_VALUE		0xFF

/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define OV5648_FOCAL_LENGTH_DEFAULT 0x1B70064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define OV5648_F_NUMBER_DEFAULT 0x18000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define OV5648_F_NUMBER_RANGE 0x180a180a
#define OV5648_ID	0x5648

#define OV5648_FINE_INTG_TIME_MIN 0
#define OV5648_FINE_INTG_TIME_MAX_MARGIN 0
#define OV5648_COARSE_INTG_TIME_MIN 1
#define OV5648_COARSE_INTG_TIME_MAX_MARGIN 6

#define OV5648_BIN_FACTOR_MAX 4
/*
 * OV5648 System control registers
 */
#define OV5648_SW_SLEEP			0x0100
#define OV5648_SW_RESET			0x0103
#define OV5648_SW_STREAM		0x0100

#define OV5648_SC_CMMN_CHIP_ID_H	0x300A
#define OV5648_SC_CMMN_CHIP_ID_L	0x300B
#define OV5648_SC_CMMN_SCCB_ID		0x300C
#define OV5648_SC_CMMN_SUB_ID		0x302A	/* process, version */

#define OV5648_GROUP_ACCESS 0x3208 /*Bit[7:4] Group control, Bit[3:0] Group ID*/

#define OV5648_EXPOSURE_H	0x3500 /*Bit[3:0] Bit[19:16] of exposure, remaining 16 bits lies in Reg0x3501&Reg0x3502*/
#define OV5648_EXPOSURE_M	0x3501	/*Bit[15:8] of exposure*/
#define OV5648_EXPOSURE_L	0x3502	/*Bit[7:0] of exposure*/
#define OV5648_AGC_H		0x350A	/*Bit[1:0] means Bit[9:8] of gain */
#define OV5648_AGC_L		0x350B	/*Bit[7:0] of gain */

#define OV5648_HORIZONTAL_START_H	0x3800	/*Bit[11:8] */
#define OV5648_HORIZONTAL_START_L	0x3801	/*Bit[7:0] */
#define OV5648_VERTICAL_START_H		0x3802	/*Bit[11:8] */
#define OV5648_VERTICAL_START_L		0x3803	/*Bit[7:0] */
#define OV5648_HORIZONTAL_END_H		0x3804	/*Bit[11:8] */
#define OV5648_HORIZONTAL_END_L		0x3805	/*Bit[7:0] */
#define OV5648_VERTICAL_END_H		0x3806	/*Bit[11:8] */
#define OV5648_VERTICAL_END_L		0x3807	/*Bit[7:0] */
#define OV5648_HORIZONTAL_OUTPUT_SIZE_H	0x3808	/*Bit[3:0] */
#define OV5648_HORIZONTAL_OUTPUT_SIZE_L	0x3809	/*Bit[7:0] */
#define OV5648_VERTICAL_OUTPUT_SIZE_H	0x380a	/*Bit[3:0] */
#define OV5648_VERTICAL_OUTPUT_SIZE_L	0x380b	/*Bit[7:0] */
#define OV5648_TIMING_HTS_H		0x380C	/*HTS High 8-bit*/
#define OV5648_TIMING_HTS_L		0x380D	/*HTS Low 8-bit*/
#define OV5648_TIMING_VTS_H		0x380e	/*VTS High 8-bit */
#define OV5648_TIMING_VTS_L		0x380f	/*VTS Low 8-bit*/

#define OV5648_VFLIP_REG		0x3820
#define OV5648_HFLIP_REG		0x3821
#define OV5648_VFLIP_VALUE		0x06
#define OV5648_HFLIP_VALUE		0x06

#define OV5648_MWB_RED_GAIN_H		0x5186
#define OV5648_MWB_GREEN_GAIN_H		0x5188
#define OV5648_MWB_BLUE_GAIN_H		0x518A
#define OV5648_MWB_GAIN_MAX		0x0fff

#define OV5648_START_STREAMING		0x01
#define OV5648_STOP_STREAMING		0x00

#define VCM_ADDR           0x0c
#define VCM_CODE_MSB       0x03
#define VCM_CODE_LSB       0x04
#define VCM_MAX_FOCUS_POS  1023

#define OV5648_VCM_SLEW_STEP		0x30F0
#define OV5648_VCM_SLEW_STEP_MAX	0x7
#define OV5648_VCM_SLEW_STEP_MASK	0x7
#define OV5648_VCM_CODE			0x30F2
#define OV5648_VCM_SLEW_TIME		0x30F4
#define OV5648_VCM_SLEW_TIME_MAX	0xffff
#define OV5648_VCM_ENABLE		0x8000

#define OV5648_MAX_FOCUS_POS	255
#define OV5648_MAX_FOCUS_NEG	(-255)

// Add OTP operation
#define BG_Ratio_Typical  0x16E
#define RG_Ratio_Typical  0x189

struct otp_struct {
		u16 otp_en;
		u16 module_integrator_id;
		u16 lens_id;
		u16 rg_ratio;
		u16 bg_ratio;
		u16 user_data[2];
		u16 light_rg;
		u16 light_bg;
		int R_gain;
		int G_gain;
		int B_gain;
};
struct ov5648_vcm {
	int (*init) (struct v4l2_subdev *sd);
	int (*t_focus_abs) (struct v4l2_subdev *sd, s32 value);
	int (*t_focus_rel) (struct v4l2_subdev *sd, s32 value);
	int (*q_focus_status) (struct v4l2_subdev *sd, s32 *value);
	int (*q_focus_abs) (struct v4l2_subdev *sd, s32 *value);
	int (*t_vcm_slew) (struct v4l2_subdev *sd, s32 value);
	int (*t_vcm_timing) (struct v4l2_subdev *sd, s32 value);
};

struct regval_list {
	u16 reg_num;
	u8 value;
};

struct ov5648_resolution {
	u8 *desc;
	const struct ov5648_reg *regs;
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

struct ov5648_format {
	u8 *desc;
	u32 pixelformat;
	struct ov5648_reg *regs;
};

struct ov5648_control {
	struct v4l2_queryctrl qc;
	int (*query) (struct v4l2_subdev *sd, s32 *value);
	int (*tweak) (struct v4l2_subdev *sd, s32 value);
};

/*
 * ov5648 device structure.
 */
struct ov5648_device {
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
	struct ov5648_vcm *vcm_driver;
	struct otp_struct current_otp;
	int pre_digitgain;
};

enum ov5648_tok_type {
	OV5648_8BIT = 0x0001,
	OV5648_16BIT = 0x0002,
	OV5648_32BIT = 0x0004,
	OV5648_TOK_TERM = 0xf000,/* terminating token for reg list */
	OV5648_TOK_DELAY = 0xfe00,/* delay token for reg list */
	OV5648_TOK_MASK = 0xfff0
};

/**
 * struct ov5648_reg - MI sensor  register format
 * @type: type of the register
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 *
 * Define a structure for sensor register initialization values
 */
struct ov5648_reg {
	enum ov5648_tok_type type;
	u16 reg;
	u32 val;		/* @set value for read/mod/write, @mask */
};

#define to_ov5648_sensor(x) container_of(x, struct ov5648_device, sd)

#define OV5648_MAX_WRITE_BUF_SIZE	30

struct ov5648_write_buffer {
	u16 addr;
	u8 data[OV5648_MAX_WRITE_BUF_SIZE];
};

struct ov5648_write_ctrl {
	int index;
	struct ov5648_write_buffer buffer;
};

static const struct i2c_device_id ov5648_id[] = {
	{OV5648_NAME, 0},
	{}
};

static const struct ov5648_reg ov5648_global_settings[] = {
	{OV5648_8BIT, 0x0103, 0x01},
	{OV5648_8BIT, 0x3001, 0x00},
	{OV5648_8BIT, 0x3002, 0x00},
	{OV5648_8BIT, 0x3011, 0x02},
	{OV5648_8BIT, 0x3017, 0x05},
	{OV5648_8BIT, 0x3018, 0x4c},
	{OV5648_8BIT, 0x301c, 0xd2},
	{OV5648_8BIT, 0x3022, 0x00},
	{OV5648_8BIT, 0x3034, 0x1a},
	{OV5648_8BIT, 0x3035, 0x21},
	{OV5648_8BIT, 0x3036, 0x69},
	{OV5648_8BIT, 0x3037, 0x03},
	{OV5648_8BIT, 0x3038, 0x00},
	{OV5648_8BIT, 0x3039, 0x00},
	{OV5648_8BIT, 0x303a, 0x00},
	{OV5648_8BIT, 0x303b, 0x19},
	{OV5648_8BIT, 0x303c, 0x11},
	{OV5648_8BIT, 0x303d, 0x30},
	{OV5648_8BIT, 0x3105, 0x11},
	{OV5648_8BIT, 0x3106, 0x05},
	{OV5648_8BIT, 0x3304, 0x28},
	{OV5648_8BIT, 0x3305, 0x41},
	{OV5648_8BIT, 0x3306, 0x30},
	{OV5648_8BIT, 0x3308, 0x00},
	{OV5648_8BIT, 0x3309, 0xc8},
	{OV5648_8BIT, 0x330a, 0x01},
	{OV5648_8BIT, 0x330b, 0x90},
	{OV5648_8BIT, 0x330c, 0x02},
	{OV5648_8BIT, 0x330d, 0x58},
	{OV5648_8BIT, 0x330e, 0x03},
	{OV5648_8BIT, 0x330f, 0x20},
	{OV5648_8BIT, 0x3300, 0x00},
	{OV5648_8BIT, 0x3500, 0x00},
	{OV5648_8BIT, 0x3501, 0x7b},
	{OV5648_8BIT, 0x3502, 0x00},
	{OV5648_8BIT, 0x3503, 0x07},
	{OV5648_8BIT, 0x350a, 0x00},
	{OV5648_8BIT, 0x350b, 0x40},
	{OV5648_8BIT, 0x3601, 0x33},
	{OV5648_8BIT, 0x3602, 0x00},
	{OV5648_8BIT, 0x3611, 0x0e},
	{OV5648_8BIT, 0x3612, 0x2b},
	{OV5648_8BIT, 0x3614, 0x50},
	{OV5648_8BIT, 0x3620, 0x33},
	{OV5648_8BIT, 0x3622, 0x00},
	{OV5648_8BIT, 0x3630, 0xad},
	{OV5648_8BIT, 0x3631, 0x00},
	{OV5648_8BIT, 0x3632, 0x94},
	{OV5648_8BIT, 0x3633, 0x17},
	{OV5648_8BIT, 0x3634, 0x14},
	{OV5648_8BIT, 0x3704, 0xc0},
	{OV5648_8BIT, 0x3705, 0x2a},
	{OV5648_8BIT, 0x3708, 0x63},
	{OV5648_8BIT, 0x3709, 0x12},
	{OV5648_8BIT, 0x370b, 0x23},
	{OV5648_8BIT, 0x370c, 0xc0},
	{OV5648_8BIT, 0x370d, 0x00},
	{OV5648_8BIT, 0x370e, 0x00},
	{OV5648_8BIT, 0x371c, 0x07},
	{OV5648_8BIT, 0x3739, 0xd2},
	{OV5648_8BIT, 0x373c, 0x00},
	{OV5648_8BIT, 0x3800, 0x00},
	{OV5648_8BIT, 0x3801, 0x00},
	{OV5648_8BIT, 0x3802, 0x00},
	{OV5648_8BIT, 0x3803, 0x00},
	{OV5648_8BIT, 0x3804, 0x0a},
	{OV5648_8BIT, 0x3805, 0x3f},
	{OV5648_8BIT, 0x3806, 0x07},
	{OV5648_8BIT, 0x3807, 0xa3},
	{OV5648_8BIT, 0x3808, 0x0a},
	{OV5648_8BIT, 0x3809, 0x20},
	{OV5648_8BIT, 0x380a, 0x07},
	{OV5648_8BIT, 0x380b, 0x98},
	{OV5648_8BIT, 0x380c, 0x0b},
	{OV5648_8BIT, 0x380d, 0x00},
	{OV5648_8BIT, 0x380e, 0x07},
	{OV5648_8BIT, 0x380f, 0xc0},
	{OV5648_8BIT, 0x3810, 0x00},
	{OV5648_8BIT, 0x3811, 0x10},
	{OV5648_8BIT, 0x3812, 0x00},
	{OV5648_8BIT, 0x3813, 0x06},
	{OV5648_8BIT, 0x3814, 0x11},
	{OV5648_8BIT, 0x3815, 0x11},
	{OV5648_8BIT, 0x3817, 0x00},
	{OV5648_8BIT, 0x3820, 0x40},
	{OV5648_8BIT, 0x3821, 0x06},
	{OV5648_8BIT, 0x3826, 0x03},
	{OV5648_8BIT, 0x3829, 0x00},
	{OV5648_8BIT, 0x382b, 0x0b},
	{OV5648_8BIT, 0x3830, 0x00},
	{OV5648_8BIT, 0x3836, 0x00},
	{OV5648_8BIT, 0x3837, 0x00},
	{OV5648_8BIT, 0x3838, 0x00},
	{OV5648_8BIT, 0x3839, 0x04},
	{OV5648_8BIT, 0x383a, 0x00},
	{OV5648_8BIT, 0x383b, 0x01},
	{OV5648_8BIT, 0x3b00, 0x00},
	{OV5648_8BIT, 0x3b02, 0x08},
	{OV5648_8BIT, 0x3b03, 0x00},
	{OV5648_8BIT, 0x3b04, 0x04},
	{OV5648_8BIT, 0x3b05, 0x00},
	{OV5648_8BIT, 0x3b06, 0x04},
	{OV5648_8BIT, 0x3b07, 0x08},
	{OV5648_8BIT, 0x3b08, 0x00},
	{OV5648_8BIT, 0x3b09, 0x02},
	{OV5648_8BIT, 0x3b0a, 0x04},
	{OV5648_8BIT, 0x3b0b, 0x00},
	{OV5648_8BIT, 0x3b0c, 0x3d},
	{OV5648_8BIT, 0x3f01, 0x0d},
	{OV5648_8BIT, 0x3f0f, 0xf5},
	{OV5648_8BIT, 0x4000, 0x89},
	{OV5648_8BIT, 0x4001, 0x02},
	{OV5648_8BIT, 0x4002, 0x45},
	{OV5648_8BIT, 0x4004, 0x04},
	{OV5648_8BIT, 0x4005, 0x18},
	{OV5648_8BIT, 0x4006, 0x08},
	{OV5648_8BIT, 0x4007, 0x10},
	{OV5648_8BIT, 0x4008, 0x00},
	{OV5648_8BIT, 0x4050, 0x6e},
	{OV5648_8BIT, 0x4051, 0x8f},
	{OV5648_8BIT, 0x4300, 0xf8},
	{OV5648_8BIT, 0x4303, 0xff},
	{OV5648_8BIT, 0x4304, 0x00},
	{OV5648_8BIT, 0x4307, 0xff},
	{OV5648_8BIT, 0x4520, 0x00},
	{OV5648_8BIT, 0x4521, 0x00},
	{OV5648_8BIT, 0x4511, 0x22},
	{OV5648_8BIT, 0x4801, 0x0f},
	{OV5648_8BIT, 0x4814, 0x2a},
	{OV5648_8BIT, 0x481f, 0x3c},
	{OV5648_8BIT, 0x4823, 0x3c},
	{OV5648_8BIT, 0x4826, 0x00},
	{OV5648_8BIT, 0x481b, 0x3c},
	{OV5648_8BIT, 0x4827, 0x32},
	{OV5648_8BIT, 0x4837, 0x17},
	{OV5648_8BIT, 0x4b00, 0x06},
	{OV5648_8BIT, 0x4b01, 0x0a},
	{OV5648_8BIT, 0x4b04, 0x10},
	{OV5648_8BIT, 0x5000, 0xff},
	{OV5648_8BIT, 0x5001, 0x00},
	{OV5648_8BIT, 0x5002, 0x41},
	{OV5648_8BIT, 0x5003, 0x0a},
	{OV5648_8BIT, 0x5004, 0x00},
	{OV5648_8BIT, 0x5043, 0x00},
	{OV5648_8BIT, 0x5013, 0x00},
	{OV5648_8BIT, 0x501f, 0x03},
	{OV5648_8BIT, 0x503d, 0x00},
	{OV5648_8BIT, 0x5a00, 0x08},
	{OV5648_8BIT, 0x5b00, 0x01},
	{OV5648_8BIT, 0x5b01, 0x40},
	{OV5648_8BIT, 0x5b02, 0x00},
	{OV5648_8BIT, 0x5b03, 0xf0},
	{OV5648_TOK_TERM, 0, 0}
};

/*
 * Register settings for various resolution
 */
/*B720P(1296X736) 30fps 2lane 10Bit (Binning)*/
static struct ov5648_reg const ov5648_720p_30fps_2lanes[] = {
	{OV5648_8BIT, 0x3708, 0x66},
	{OV5648_8BIT, 0x3709, 0x52},
	{OV5648_8BIT, 0x370c, 0xcf},
	{OV5648_8BIT, 0x3800, 0x00},/* xstart = 0 */
	{OV5648_8BIT, 0x3801, 0x00},/*;xstart10 */
	{OV5648_8BIT, 0x3802, 0x00},/* ystart = 226 */
	{OV5648_8BIT, 0x3803, 0xe2},/* ystart ;fe */
	{OV5648_8BIT, 0x3804, 0x0a},/* xend = 2607 */
	{OV5648_8BIT, 0x3805, 0x2f},/* xend */
	{OV5648_8BIT, 0x3806, 0x06},/* yend = 1701 */
	{OV5648_8BIT, 0x3807, 0xa5},/* yend */
	{OV5648_8BIT, 0x3808, 0x05},/* x output size = 1296 */
	{OV5648_8BIT, 0x3809, 0x10},/*;x output size 00 */
	{OV5648_8BIT, 0x380a, 0x02},/* y output size = 736 */
	{OV5648_8BIT, 0x380b, 0xe0},/*;y output size d0 */
	{OV5648_8BIT, 0x380c, 0x09},/* hts = 1864  2400 */
	{OV5648_8BIT, 0x380d, 0x60},/* hts 48 */
	{OV5648_8BIT, 0x380e, 0x04},/* vts = 754; 1120 */
	{OV5648_8BIT, 0x380f, 0x60},/* vts f2 */
	{OV5648_8BIT, 0x3810, 0x00},/* isp x win (offset)= 0 */
	{OV5648_8BIT, 0x3811, 0x00},/* isp x win;08 */
	{OV5648_8BIT, 0x3812, 0x00},/* isp y win (offset)= 0 */
	{OV5648_8BIT, 0x3813, 0x00},/* isp y win;02 */
	{OV5648_8BIT, 0x3814, 0x31},/* x inc */
	{OV5648_8BIT, 0x3815, 0x31},/* y inc */
	{OV5648_8BIT, 0x3817, 0x00},/* hsync start */
	{OV5648_8BIT, 0x3820, 0x08},/* flip off; v bin off */
	{OV5648_8BIT, 0x3821, 0x01},/* mirror off; h bin on */
	{OV5648_8BIT, 0x4004, 0x02},/* black line number */
	{OV5648_8BIT, 0x4005, 0x18},/* blc level trigger */
	{OV5648_8BIT, 0x4837, 0x17},/* MIPI global timing ;2f;18 */

	{OV5648_8BIT, 0x350b, 0x80},/* gain 8x */
	{OV5648_8BIT, 0x3501, 0x2d},/* exposure */
	{OV5648_8BIT, 0x3502, 0xc0},/* exposure */
	/*;add 19.2MHz 30fps */

	{OV5648_8BIT, 0x380e, 0x02},
	{OV5648_8BIT, 0x380f, 0xf2},
	{OV5648_8BIT, 0x3034, 0x1a},/* mipi 10bit mode */
	{OV5648_8BIT, 0x3035, 0x21},
	{OV5648_8BIT, 0x3036, 0x58},
	{OV5648_8BIT, 0x3037, 0x02},
	{OV5648_8BIT, 0x3038, 0x00},
	{OV5648_8BIT, 0x3039, 0x00},
	{OV5648_8BIT, 0x3106, 0x05},
	{OV5648_8BIT, 0x3105, 0x11},
	{OV5648_8BIT, 0x303a, 0x00},
	{OV5648_8BIT, 0x303b, 0x16},
	{OV5648_8BIT, 0x303c, 0x11},
	{OV5648_8BIT, 0x303d, 0x20},

	{OV5648_TOK_TERM, 0, 0}
};

/*B720P(1296X864) 30fps 2lane 10Bit (Binning)*/
static struct ov5648_reg const ov5648_1296x864_30fps_2lanes[] = {
	{OV5648_8BIT, 0x3708, 0x66},
	{OV5648_8BIT, 0x3709, 0x52},
	{OV5648_8BIT, 0x370c, 0xcf},
	{OV5648_8BIT, 0x3800, 0x00},/* xstart = 0 */
	{OV5648_8BIT, 0x3801, 0x00},/* xstart ;10 */
	{OV5648_8BIT, 0x3802, 0x00},/* ystart = 98 */
	{OV5648_8BIT, 0x3803, 0x62},/* ystart */
	{OV5648_8BIT, 0x3804, 0x0a},/* xend = 2607 */
	{OV5648_8BIT, 0x3805, 0x2f},/* xend */
	{OV5648_8BIT, 0x3806, 0x07},/* yend = 1845 */
	{OV5648_8BIT, 0x3807, 0x35},/* yend */
	{OV5648_8BIT, 0x3808, 0x05},/* x output size = 1296 */
	{OV5648_8BIT, 0x3809, 0x10},/*;x output size */
	{OV5648_8BIT, 0x380a, 0x03},/* y output size = 864 */
	{OV5648_8BIT, 0x380b, 0x60},/*;y output size */
	{OV5648_8BIT, 0x380c, 0x09},/* hts = 1864 ;2400 */
	{OV5648_8BIT, 0x380d, 0x60},/* hts48 */
	{OV5648_8BIT, 0x380e, 0x04},/* vts = 754; 1120 */
	{OV5648_8BIT, 0x380f, 0x60},/* vts f2 */
	{OV5648_8BIT, 0x3810, 0x00},/* isp x win (offset)= 0 */
	{OV5648_8BIT, 0x3811, 0x00},/* isp x win;08 */
	{OV5648_8BIT, 0x3812, 0x00},/* isp y win (offset)= 0 */
	{OV5648_8BIT, 0x3813, 0x00},/* isp y win;02 */
	{OV5648_8BIT, 0x3814, 0x31},/* x inc */
	{OV5648_8BIT, 0x3815, 0x31},/* y inc */
	{OV5648_8BIT, 0x3817, 0x00},/* hsync start */
	{OV5648_8BIT, 0x3820, 0x08},/* flip off; v bin off */
	{OV5648_8BIT, 0x3821, 0x01},/* mirror off; h bin on */
	{OV5648_8BIT, 0x4004, 0x02},/* black line number */
	{OV5648_8BIT, 0x4005, 0x18},/* blc level trigger */
	{OV5648_8BIT, 0x4837, 0x17},/* MIPI global timing ;2f;18 */

	{OV5648_8BIT, 0x350b, 0x80},/* gain 8x */
	{OV5648_8BIT, 0x3501, 0x35},/* exposure */
	{OV5648_8BIT, 0x3502, 0xc0},/* exposure */
	/*;add 19.2MHz 30fps*/

	{OV5648_8BIT, 0x380e, 0x02},
	{OV5648_8BIT, 0x380f, 0xf2},
	{OV5648_8BIT, 0x3034, 0x1a},/* mipi 10bit mode */
	{OV5648_8BIT, 0x3035, 0x21},
	{OV5648_8BIT, 0x3036, 0x58},
	{OV5648_8BIT, 0x3037, 0x02},
	{OV5648_8BIT, 0x3038, 0x00},
	{OV5648_8BIT, 0x3039, 0x00},
	{OV5648_8BIT, 0x3106, 0x05},
	{OV5648_8BIT, 0x3105, 0x11},
	{OV5648_8BIT, 0x303a, 0x00},
	{OV5648_8BIT, 0x303b, 0x16},
	{OV5648_8BIT, 0x303c, 0x11},
	{OV5648_8BIT, 0x303d, 0x20},

	{OV5648_TOK_TERM, 0, 0}
};

static struct ov5648_reg const ov5648_1080p_30fps_2lanes[] = {
	{OV5648_8BIT, 0x3708, 0x63},
	{OV5648_8BIT, 0x3709, 0x12},
	{OV5648_8BIT, 0x370c, 0xc0},
	{OV5648_8BIT, 0x3800, 0x01},/* xstart = 320 */
	{OV5648_8BIT, 0x3801, 0x40},/* xstart */
	{OV5648_8BIT, 0x3802, 0x01},/* ystart = 418 */
	{OV5648_8BIT, 0x3803, 0xa2},/* ystart */
	{OV5648_8BIT, 0x3804, 0x08},/* xend = 2287 */
	{OV5648_8BIT, 0x3805, 0xef},/* xend */
	{OV5648_8BIT, 0x3806, 0x05},/* yend = 1521 */
	{OV5648_8BIT, 0x3807, 0xf1},/* yend */
	{OV5648_8BIT, 0x3808, 0x07},/* x output size = 1940 */
	{OV5648_8BIT, 0x3809, 0x94},/* x output size */
	{OV5648_8BIT, 0x380a, 0x04},/* y output size = 1096 */
	{OV5648_8BIT, 0x380b, 0x48},/* y output size */
	{OV5648_8BIT, 0x380c, 0x09},/* hts = 2500 */
	{OV5648_8BIT, 0x380d, 0xc4},/* hts */
	{OV5648_8BIT, 0x380e, 0x04},/* vts = 1120 */
	{OV5648_8BIT, 0x380f, 0x60},/* vts */
	{OV5648_8BIT, 0x3810, 0x00},/* isp x win = 16 */
	{OV5648_8BIT, 0x3811, 0x10},/* isp x win */
	{OV5648_8BIT, 0x3812, 0x00},/* isp y win = 4 */
	{OV5648_8BIT, 0x3813, 0x04},/* isp y win */
	{OV5648_8BIT, 0x3814, 0x11},/* x inc */
	{OV5648_8BIT, 0x3815, 0x11},/* y inc */
	{OV5648_8BIT, 0x3817, 0x00},/* hsync start */
	{OV5648_8BIT, 0x3820, 0x40},/* flip off; v bin off */
	{OV5648_8BIT, 0x3821, 0x06},/* mirror off; v bin off */
	{OV5648_8BIT, 0x4004, 0x04},/* black line number */
	{OV5648_8BIT, 0x4005, 0x18},/* blc always update */
	{OV5648_8BIT, 0x4837, 0x18},/* MIPI global timing */

	{OV5648_8BIT, 0x350b, 0x40},/* gain 4x */
	{OV5648_8BIT, 0x3501, 0x45},/* exposure */
	{OV5648_8BIT, 0x3502, 0x80},/* exposure */
	/*;add 19.2MHz 30fps */

	{OV5648_8BIT, 0x3034, 0x1a},/* mipi 10bit mode */
	{OV5648_8BIT, 0x3035, 0x21},
	{OV5648_8BIT, 0x3036, 0x58},
	{OV5648_8BIT, 0x3037, 0x02},
	{OV5648_8BIT, 0x3038, 0x00},
	{OV5648_8BIT, 0x3039, 0x00},
	{OV5648_8BIT, 0x3106, 0x05},
	{OV5648_8BIT, 0x3105, 0x11},
	{OV5648_8BIT, 0x303a, 0x00},
	{OV5648_8BIT, 0x303b, 0x16},
	{OV5648_8BIT, 0x303c, 0x11},
	{OV5648_8BIT, 0x303d, 0x20},

	{OV5648_TOK_TERM, 0, 0}
};

/* 2x2 subsampled 4:3 mode giving 1280x960.
 * 84.48Mhz / (HTS*VTS) == 84.48e6/(2500*1126) == 30.01 fps */
static struct ov5648_reg const ov5648_1280x960_30fps_2lanes[] = {
	{OV5648_8BIT, 0x3034, 0x1a},
	{OV5648_8BIT, 0x3035, 0x21},
	{OV5648_8BIT, 0x3036, 0x58},
	{OV5648_8BIT, 0x3037, 0x02},
	{OV5648_8BIT, 0x3038, 0x00},
	{OV5648_8BIT, 0x3039, 0x00},
	{OV5648_8BIT, 0x3106, 0x05},
	{OV5648_8BIT, 0x3105, 0x11},
	{OV5648_8BIT, 0x303a, 0x00},
	{OV5648_8BIT, 0x303b, 0x16},
	{OV5648_8BIT, 0x303c, 0x11},
	{OV5648_8BIT, 0x303d, 0x20},

	{OV5648_8BIT, 0x350a, 0x00}, /* Def. analog gain = 0x080/16.0 = 8x */
	{OV5648_8BIT, 0x350b, 0x80},

	{OV5648_8BIT, 0x3708, 0x66},
	{OV5648_8BIT, 0x3709, 0x52},
	{OV5648_8BIT, 0x370c, 0xcf},

	{OV5648_8BIT, 0x3800, 0x00}, /* Xstart = 0x0000 */
	{OV5648_8BIT, 0x3801, 0x00},
	{OV5648_8BIT, 0x3802, 0x00}, /* Ystart = 0x0000 */
	{OV5648_8BIT, 0x3803, 0x00},
	{OV5648_8BIT, 0x3804, 0x0a}, /* Xend = 0x0a3f = 2623 */
	{OV5648_8BIT, 0x3805, 0x3f},
	{OV5648_8BIT, 0x3806, 0x07}, /* Yend = 0x07a3 = 1955 */
	{OV5648_8BIT, 0x3807, 0xa3},
	{OV5648_8BIT, 0x3808, 0x05}, /* H output size = 0x0510 = 1296 */
	{OV5648_8BIT, 0x3809, 0x10},
	{OV5648_8BIT, 0x380a, 0x03}, /* V output size = 0x03cc = 972 */
	{OV5648_8BIT, 0x380b, 0xcc},
	{OV5648_8BIT, 0x380c, 0x09}, /* H total size = 0x09c4 = 2500 */
	{OV5648_8BIT, 0x380d, 0xc4},
	{OV5648_8BIT, 0x380e, 0x04}, /* V total size = 0x0466 = 1126 */
	{OV5648_8BIT, 0x380f, 0x66},
	{OV5648_8BIT, 0x3810, 0x00}, /* X window offset = 0x0008 */
	{OV5648_8BIT, 0x3811, 0x08},
	{OV5648_8BIT, 0x3812, 0x00}, /* Y window offset = 0x0004 */
	{OV5648_8BIT, 0x3813, 0x04},
	{OV5648_8BIT, 0x3814, 0x31}, /* X subsample step: odd = 3, even = 1 */
	{OV5648_8BIT, 0x3815, 0x31}, /* Y subsample step: odd = 3, even = 1 */
	{OV5648_8BIT, 0x3817, 0x00}, /* HSync start = 0 */
	{OV5648_8BIT, 0x3820, 0x08}, /* V flip off, V binning off */
	{OV5648_8BIT, 0x3821, 0x07}, /* H mirror on, H binning on */

	{OV5648_8BIT, 0x4004, 0x02},
	{OV5648_8BIT, 0x4837, 0x18},
};

static struct ov5648_reg const ov5648_5M_15fps_2lanes[] = {
	/*;add 19.2MHz */
	{OV5648_8BIT, 0x3034, 0x1a},
	{OV5648_8BIT, 0x3035, 0x21},
	{OV5648_8BIT, 0x3036, 0x58},
	{OV5648_8BIT, 0x3037, 0x02},
	{OV5648_8BIT, 0x3038, 0x00},
	{OV5648_8BIT, 0x3039, 0x00},
	{OV5648_8BIT, 0x3106, 0x05},
	{OV5648_8BIT, 0x3105, 0x11},
	{OV5648_8BIT, 0x303a, 0x00},
	{OV5648_8BIT, 0x303b, 0x16},
	{OV5648_8BIT, 0x303c, 0x11},
	{OV5648_8BIT, 0x303d, 0x20},

	{OV5648_8BIT, 0x3708, 0x63},
	{OV5648_8BIT, 0x3709, 0x12},
	{OV5648_8BIT, 0x370c, 0xc0},
	{OV5648_8BIT, 0x3800, 0x00},/* xstart = 0 */
	{OV5648_8BIT, 0x3801, 0x00},/* xstart */
	{OV5648_8BIT, 0x3802, 0x00},/* ystart = 0 */
	{OV5648_8BIT, 0x3803, 0x00},/* ystart */
	{OV5648_8BIT, 0x3804, 0x0a},/* xend = 2623 */
	{OV5648_8BIT, 0x3805, 0x3f},/* xend */
	{OV5648_8BIT, 0x3806, 0x07},/* yend = 1955 */
	{OV5648_8BIT, 0x3807, 0xa3},/* yend */
	{OV5648_8BIT, 0x3808, 0x0a},/* x output size = 2592 */
	{OV5648_8BIT, 0x3809, 0x20},/* x output size */
	{OV5648_8BIT, 0x380a, 0x07},/* y output size = 1944 */
	{OV5648_8BIT, 0x380b, 0x98},/* y output size */
	{OV5648_8BIT, 0x380c, 0x0b},/* hts = 2838 */
	{OV5648_8BIT, 0x380d, 0x16},/* hts */
	{OV5648_8BIT, 0x380e, 0x07},/* vts = 1984 */
	{OV5648_8BIT, 0x380f, 0xc0},/* vts */
	{OV5648_8BIT, 0x3810, 0x00},/* isp x win = 16 */
	{OV5648_8BIT, 0x3811, 0x10},/* isp x win */
	{OV5648_8BIT, 0x3812, 0x00},/* isp y win = 6 */
	{OV5648_8BIT, 0x3813, 0x06},/* isp y win */
	{OV5648_8BIT, 0x3814, 0x11},/* x inc */
	{OV5648_8BIT, 0x3815, 0x11},/* y inc */
	{OV5648_8BIT, 0x3817, 0x00},/* hsync start */
	{OV5648_8BIT, 0x3820, 0x40},/* flip off; v bin off */
	{OV5648_8BIT, 0x3821, 0x06},/* mirror off; v bin off */
	{OV5648_8BIT, 0x4004, 0x04},/* black line number */
	{OV5648_8BIT, 0x4005, 0x18},/* blc always update */
	{OV5648_8BIT, 0x4837, 0x18},/* MIPI global timing */

	{OV5648_8BIT, 0x350b, 0x40},
	{OV5648_8BIT, 0x3501, 0x7b},
	{OV5648_8BIT, 0x3502, 0x00},

	{OV5648_TOK_TERM, 0, 0}
};

struct ov5648_resolution ov5648_res_preview[] = {
	{
	 .desc = "ov5648_5M_15fps",
	 .width = 2592,
	 .height = 1944,
	 .pix_clk_freq = 84,
	 .fps = 15,
	 .used = 0,
	 .pixels_per_line = 2838,
	 .lines_per_frame = 1984,
	 .bin_factor_x = 1,
	 .bin_factor_y = 1,
	 .bin_mode = 0,
	 .skip_frames = 3,
	 .regs = ov5648_5M_15fps_2lanes,
	 },
	{
	 .desc = "ov5648_1280x960_30fps",
	 .width = 1296,
	 .height = 972,
	 .pix_clk_freq = 84,
	 .fps = 30,
	 .used = 0,
	 .pixels_per_line = 2500,
	 .lines_per_frame = 1126,
	 .bin_factor_x = 2,
	 .bin_factor_y = 2,
	 .bin_mode = 1,
	 .skip_frames = 3,
	 .regs = ov5648_1280x960_30fps_2lanes,
	 },
	{
	 .desc = "ov5648_720P_30fps",
	 .width = 1296,
	 .height = 736,
	 .fps = 30,
	 .pix_clk_freq = 84,
	 .used = 0,
	 .pixels_per_line = 2397,
	 .lines_per_frame = 1186,
	 .bin_factor_x = 2,
	 .bin_factor_y = 2,
	 .bin_mode = 1,
	 .skip_frames = 3,
	 .regs = ov5648_720p_30fps_2lanes,
	 },
};

#define N_RES_PREVIEW (ARRAY_SIZE(ov5648_res_preview))

struct ov5648_resolution ov5648_res_still[] = {
	{
	 .desc = "ov5648_5M_15fps",
	 .width = 2592,
	 .height = 1944,
	 .pix_clk_freq = 84,
	 .fps = 15,
	 .used = 0,
	 .pixels_per_line = 2838,
	 .lines_per_frame = 1984,
	 .bin_factor_x = 1,
	 .bin_factor_y = 1,
	 .bin_mode = 0,
	 .skip_frames = 3,
	 .regs = ov5648_5M_15fps_2lanes,
	 },
	{
	 .desc = "ov5648_1280x960_30fps",
	 .width = 1296,
	 .height = 972,
	 .pix_clk_freq = 84,
	 .fps = 30,
	 .used = 0,
	 .pixels_per_line = 2500,
	 .lines_per_frame = 1126,
	 .bin_factor_x = 2,
	 .bin_factor_y = 2,
	 .bin_mode = 1,
	 .skip_frames = 3,
	 .regs = ov5648_1280x960_30fps_2lanes,
	 },
	{
	 .desc = "ov5648_720P_30fps",
	 .width = 1296,
	 .height = 736,
	 .fps = 30,
	 .pix_clk_freq = 84,
	 .used = 0,
	 .pixels_per_line = 2397,
	 .lines_per_frame = 1186,
	 .bin_factor_x = 2,
	 .bin_factor_y = 2,
	 .bin_mode = 1,
	 .skip_frames = 3,
	 .regs = ov5648_720p_30fps_2lanes,
	 },
};

#define N_RES_STILL (ARRAY_SIZE(ov5648_res_still))

struct ov5648_resolution ov5648_res_video[] = {
	{
	 .desc = "ov5648_720P_30fps",
	 .width = 1296,
	 .height = 736,
	 .fps = 30,
	 .pix_clk_freq = 84,
	 .used = 0,
	 .pixels_per_line = 2397,
	 .lines_per_frame = 1186,
	 .bin_factor_x = 2,
	 .bin_factor_y = 2,
	 .bin_mode = 1,
	 .skip_frames = 3,
	 .regs = ov5648_720p_30fps_2lanes,
	 },
	{
	 .desc = "ov5648_1280x960_30fps",
	 .width = 1296,
	 .height = 972,
	 .pix_clk_freq = 84,
	 .fps = 30,
	 .used = 0,
	 .pixels_per_line = 2500,
	 .lines_per_frame = 1126,
	 .bin_factor_x = 2,
	 .bin_factor_y = 2,
	 .bin_mode = 1,
	 .skip_frames = 3,
	 .regs = ov5648_1280x960_30fps_2lanes,
	 },
};

#define N_RES_VIDEO (ARRAY_SIZE(ov5648_res_video))

static struct ov5648_resolution *ov5648_res = ov5648_res_preview;
static int N_RES = N_RES_PREVIEW;
//static int has_otp = -1;	/*0:has valid otp, 1:no valid otp */

#define WV511  0x11
extern int wv511_vcm_init(struct v4l2_subdev *sd);
extern int wv511_t_focus_vcm(struct v4l2_subdev *sd, u16 val);
extern int wv511_t_focus_abs(struct v4l2_subdev *sd, s32 value);
extern int wv511_t_focus_rel(struct v4l2_subdev *sd, s32 value);
extern int wv511_q_focus_status(struct v4l2_subdev *sd, s32 *value);
extern int wv511_q_focus_abs(struct v4l2_subdev *sd, s32 *value);
extern int wv511_t_vcm_slew(struct v4l2_subdev *sd, s32 value);
extern int wv511_t_vcm_timing(struct v4l2_subdev *sd, s32 value);

struct ov5648_vcm ov5648_vcms[] = {
	[WV511] = {
		.init = wv511_vcm_init,
		.t_focus_abs = wv511_t_focus_abs,
		.t_focus_rel = wv511_t_focus_rel,
		.q_focus_status = wv511_q_focus_status,
		.q_focus_abs = wv511_q_focus_abs,
		.t_vcm_slew = wv511_t_vcm_slew,
		.t_vcm_timing = wv511_t_vcm_timing,
	},
};

#endif
