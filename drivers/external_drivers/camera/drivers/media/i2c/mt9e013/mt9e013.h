/*
 * Support for Aptina MT9E013 camera sensor.
 *
 * Copyright (c) 2012 Intel Corporation. All Rights Reserved.
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

#ifndef __MT9E013_H__
#define __MT9E013_H__
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
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#define	MT9E013_NAME	"mt9e013"
#define	MT9E013_ADDR	0x36
#define MT9E013_ID	0x4b00
#define MT9E013_ID2	0x4b01

#define	LAST_REG_SETING		{0xffff, 0xff}
#define	is_last_reg_setting(item) ((item).reg == 0xffff)
#define I2C_MSG_LENGTH		0x2

#define MT9E013_INVALID_CONFIG	0xffffffff

#define MT9E013_MAX_FOCUS_POS	255
#define MT9E013_MAX_FOCUS_NEG	(-255)

#define MT9E013_INTG_UNIT_US	100
#define MT9E013_MCLK		192

#define MT9E013_REG_BITS	16
#define MT9E013_REG_MASK	0xFFFF

/* This should be added into include/linux/videodev2.h */
#ifndef V4L2_IDENT_MT9E013
#define V4L2_IDENT_MT9E013	8245
#endif

/*
 * mt9e013 System control registers
 */
#define MT9E013_SC_CMMN_CHIP_ID                 0x0000
#define MT9E013_SC_CMMN_REV_ID		        0x0002

#define GROUPED_PARAMETER_UPDATE		0x0000
#define GROUPED_PARAMETER_HOLD			0x0100
#define MT9E013_GROUPED_PARAMETER_HOLD		0x0104

#define MT9E013_VT_PIX_CLK_DIV			0x0300
#define MT9E013_VT_SYS_CLK_DIV			0x0302
#define MT9E013_PRE_PLL_CLK_DIV			0x0304
#define MT9E013_PLL_MULTIPLIER			0x0306
#define MT9E013_OP_PIX_DIV			0x0308
#define MT9E013_OP_SYS_DIV			0x030A
#define MT9E013_FRAME_LENGTH_LINES		0x0340
#define MT9E013_LINE_LENGTH_PCK			0x0342
#define MT9E013_COARSE_INTG_TIME_MIN		0x1004
#define MT9E013_COARSE_INTG_TIME_MAX		0x1006
#define MT9E013_FINE_INTG_TIME_MIN		0x1008
#define MT9E013_FINE_INTG_MIN_DEF		0x4FE
#define MT9E013_FINE_INTG_TIME_MAX		0x100A
#define MT9E013_FINE_INTG_MAX_DEF		0x3EE

#define MT9E013_READ_MODE				0x3040
#define MT9E013_READ_MODE_X_ODD_INC		(BIT(6) | BIT(7) | BIT(8))
#define MT9E013_READ_MODE_Y_ODD_INC		(BIT(0) | BIT(1) | BIT(2) |\
						BIT(3) | BIT(4) | BIT(5))

#define MT9E013_HORIZONTAL_START_H		0x0344
#define MT9E013_VERTICAL_START_H		0x0346
#define MT9E013_HORIZONTAL_END_H		0x0348
#define MT9E013_VERTICAL_END_H			0x034a
#define MT9E013_HORIZONTAL_OUTPUT_SIZE_H	0x034c
#define MT9E013_VERTICAL_OUTPUT_SIZE_H		0x034e

#define MT9E013_COARSE_INTEGRATION_TIME		0x3012
#define MT9E013_FINE_INTEGRATION_TIME		0x3014
#define MT9E013_ROW_SPEED			0x3016
#define MT9E013_PIXEL_ORDER			0x0006
#define MT9E013_GLOBAL_GAIN			0x305e
#define MT9E013_GLOBAL_GAIN_WR			0x1000
#define MT9E013_TEST_PATTERN_MODE		0x3070
#define MT9E013_VCM_SLEW_STEP			0x30F0
#define MT9E013_VCM_SLEW_STEP_MAX		0x7
#define MT9E013_VCM_SLEW_STEP_MASK		0x7
#define MT9E013_VCM_CODE			0x30F2
#define MT9E013_VCM_SLEW_TIME			0x30F4
#define MT9E013_VCM_SLEW_TIME_MAX		0xffff
#define MT9E013_VCM_ENABLE			0x8000

/* mt9e013 SCCB */
#define MT9E013_SCCB_CTRL			0x3100
#define MT9E013_AEC_PK_EXPO_H			0x3500
#define MT9E013_AEC_PK_EXPO_M			0x3501
#define MT9E013_AEC_PK_EXPO_L			0x3502
#define MT9E013_AEC_MANUAL_CTRL			0x3503
#define MT9E013_AGC_ADJ_H			0x3508
#define MT9E013_AGC_ADJ_L			0x3509

#define MT9E013_FOCAL_LENGTH_NUM	439	/*4.39mm*/
#define MT9E013_FOCAL_LENGTH_DEM	100
#define MT9E013_F_NUMBER_DEFAULT_NUM	24
#define MT9E013_F_NUMBER_DEM	10

#define MT9E013_X_ADDR_MIN	0X1180
#define MT9E013_Y_ADDR_MIN	0X1182
#define MT9E013_X_ADDR_MAX	0X1184
#define MT9E013_Y_ADDR_MAX	0X1186

#define MT9E013_MIN_FRAME_LENGTH_LINES	0x1140
#define MT9E013_MAX_FRAME_LENGTH_LINES	0x1142
#define MT9E013_MIN_LINE_LENGTH_PCK	0x1144
#define MT9E013_MAX_LINE_LENGTH_PCK	0x1146
#define MT9E013_MIN_LINE_BLANKING_PCK	0x1148
#define MT9E013_MIN_FRAME_BLANKING_LINES 0x114A
#define MT9E013_X_OUTPUT_SIZE	0x034C
#define MT9E013_Y_OUTPUT_SIZE	0x034E

#define MT9E013_BIN_FACTOR_MAX			3

/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define MT9E013_FOCAL_LENGTH_DEFAULT 0x1B70064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define MT9E013_F_NUMBER_DEFAULT 0x18000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define MT9E013_F_NUMBER_RANGE 0x180a180a
#define OTPM_ADD_START_1		0x1000
#define OTPM_DATA_LENGTH_1		0x0100
#define OTPM_COUNT 0x200

/* Defines for register writes and register array processing */
#define MT9E013_BYTE_MAX	30
#define MT9E013_SHORT_MAX	16
#define I2C_RETRY_COUNT		5
#define MT9E013_TOK_MASK	0xfff0

#define	MT9E013_STATUS_POWER_DOWN	0x0
#define	MT9E013_STATUS_STANDBY		0x2
#define	MT9E013_STATUS_ACTIVE		0x3
#define	MT9E013_STATUS_VIEWFINDER	0x4

#define MT9E013_PIXEL_ORDER0	0x0
#define MT9E013_PIXEL_ORDER1	0x1
#define MT9E013_PIXEL_ORDER2	0x2
#define MT9E013_PIXEL_ORDER3	0x3

struct s_ctrl_id {
	struct v4l2_queryctrl qc;
	int (*s_ctrl)(struct v4l2_subdev *sd, u32 val);
	int (*g_ctrl)(struct v4l2_subdev *sd, u32 *val);
};

enum mt9e013_tok_type {
	MT9E013_8BIT  = 0x0001,
	MT9E013_16BIT = 0x0002,
	MT9E013_RMW   = 0x0010,
	MT9E013_TOK_TERM   = 0xf000,	/* terminating token for reg list */
	MT9E013_TOK_DELAY  = 0xfe00	/* delay token for reg list */
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
 * struct mt9e013_fwreg - Firmware burst command
 * @type: FW burst or 8/16 bit register
 * @addr: 16-bit offset to register or other values depending on type
 * @val: data value for burst (or other commands)
 *
 * Define a structure for sensor register initialization values
 */
struct mt9e013_fwreg {
	enum mt9e013_tok_type type; /* value, register or FW burst string */
	u16 addr;	/* target address */
	u32 val[8];
};

/**
 * struct mt9e013_reg - MI sensor  register format
 * @type: type of the register
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 *
 * Define a structure for sensor register initialization values
 */
struct mt9e013_reg {
	enum mt9e013_tok_type type;
	union {
		u16 sreg;
		struct mt9e013_fwreg *fwreg;
	} reg;
	u32 val;	/* @set value for read/mod/write, @mask */
	u32 val2;	/* optional: for rmw, OR mask */
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

struct mt9e013_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, s32 value);
};

struct mt9e013_resolution {
	u8 *desc;
	int res;
	int width;
	int height;
	int fps;
	bool used;
	unsigned short pixels_per_line;
	unsigned short lines_per_frame;
	const struct mt9e013_reg *regs;
	u8 bin_factor_x;
	u8 bin_factor_y;
	unsigned short skip_frames;
};

struct mt9e013_format {
	u8 *desc;
	u32 pixelformat;
	struct s_register_setting *regs;
};

#define MT9E013_FUSEID_SIZE		8
#define MT9E013_FUSEID_START_ADDR	0x31f4

/* mt9e013 device structure */
struct mt9e013_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;

	struct camera_sensor_platform_data *platform_data;
	int fmt_idx;
	int status;
	int streaming;
	int power;
	u16 sensor_id;
	u8 sensor_revision;
	u16 coarse_itg;
	u16 fine_itg;
	u16 gain;
	u32 focus;
	u16 pixels_per_line;
	u16 lines_per_frame;
	u8 fps;
	int run_mode;
	struct timespec timestamp_t_focus_abs;
	s16 number_of_steps;
	struct mutex input_lock; /* serialize sensor's ioctl */
	void *otp_data;
	void *fuseid;
	/* Older VCMs could not maintain the focus position in standby mode. */
	bool keeps_focus_pos;
};

#define MT9E013_MAX_WRITE_BUF_SIZE	32
struct mt9e013_write_buffer {
	u16 addr;
	u8 data[MT9E013_MAX_WRITE_BUF_SIZE];
};

struct mt9e013_write_ctrl {
	int index;
	struct mt9e013_write_buffer buffer;
};

#define MT9E013_OTP_START_ADDR		0x3800
#define MT9E013_OTP_DATA_SIZE		456
#define MT9E013_OTP_READY_REG		0x304a
#define MT9E013_OTP_READY_REG_DONE	(1 << 5)
#define MT9E013_OTP_READY_REG_OK	(1 << 6)

static const struct mt9e013_reg mt9e013_otp_type30[] = {
	{MT9E013_16BIT, {0x3134}, 0xcd95},
	{MT9E013_16BIT, {0x304c}, 0x3000},
	{MT9E013_16BIT, {0x304a}, 0x0010},
	{MT9E013_TOK_TERM, {0}, 0}
};

static const struct mt9e013_reg mt9e013_otp_type31[] = {
	{MT9E013_16BIT, {0x3134}, 0xcd95},
	{MT9E013_16BIT, {0x304c}, 0x3100},
	{MT9E013_16BIT, {0x304a}, 0x0010},
	{MT9E013_TOK_TERM, {0}, 0}
};

static const struct mt9e013_reg mt9e013_otp_type32[] = {
	{MT9E013_16BIT, {0x3134}, 0xcd95},
	{MT9E013_16BIT, {0x304c}, 0x3200},
	{MT9E013_16BIT, {0x304a}, 0x0010},
	{MT9E013_TOK_TERM, {0}, 0}
};

#define MT9E013_OTP_CHECKSUM		1
#define MT9E013_OTP_MOD_CHECKSUM	255

/*
 * Checksum entries in OTP data:
 * @start: start offset of checksum's input data
 * @end: end offset of checksum's input data
 * @checksum: offset where checksum is placed
 */
struct mt9e013_otp_checksum_format {
	u16 start;
	u16 end;
	u16 checksum;
};

static const struct mt9e013_otp_checksum_format
mt9e013_otp_checksum_list[] = {
	{0x0004, 0x00d7, 0x00e1},
	{0x00d8, 0x00df, 0x00e0},
	{0x00e4, 0x01b7, 0x01c1},
	{0x01b8, 0x01bf, 0x01c0},
	{0x0000, 0x01c3, 0x01c4},
};

/* Start Streaming
 * reset_register_restart_bad = 1
 * reset_register_mask_bad = 1
 * reset_register_lock_reg = 1
 * grouped_parameter_hold = 0
 * reset_register_stream = 1 */

static const struct mt9e013_reg mt9e013_start_streaming[] = {
	{MT9E013_16BIT+MT9E013_RMW, {0x301A}, 0x0200, 0x1},
	{MT9E013_16BIT+MT9E013_RMW, {0x301A}, 0x0400, 0x1},
	{MT9E013_16BIT+MT9E013_RMW, {0x301A}, 0x8, 0x1},
	{MT9E013_16BIT, {0x0104}, 0x0},
	{MT9E013_16BIT+MT9E013_RMW, {0x301A}, 0x4, 0x1},
	{MT9E013_TOK_TERM, {0}, 0}
};

#define GROUPED_PARAMETER_HOLD_ENABLE	{MT9E013_8BIT, {0x0104}, 0x1}

#define GROUPED_PARAMETER_HOLD_DISABLE	{MT9E013_8BIT, {0x0104}, 0x0}

#define INIT_VCM_CONTROL {MT9E013_16BIT, {0x30F0}, 0x800C} /* slew_rate[2:0] */
static const struct mt9e013_reg mt9e013_init_vcm[] = {
	INIT_VCM_CONTROL,				   /* VCM_CONTROL */
	{MT9E013_16BIT, {0x30F2}, 0x0000}, /* VCM_NEW_CODE */
	{MT9E013_16BIT, {0x30F4}, 0x0080}, /* VCM_STEP_TIME */
	{MT9E013_TOK_TERM, {0}, 0}
};

#define RESET_REGISTER	{MT9E013_16BIT, {0x301A}, 0x4A38}
static const struct mt9e013_reg mt9e013_reset_register[] = {
	RESET_REGISTER,
	{MT9E013_TOK_TERM, {0}, 0}
};

static const struct mt9e013_reg mt9e013_raw_10[] = {
	{MT9E013_16BIT, {0x0112}, 0x0A0A}, /* CCP_DATA_FORMAT, set to RAW10 mode */
	{MT9E013_TOK_TERM, {0}, 0}
};

static const struct mt9e013_reg mt9e013_scaler[] = {
	{MT9E013_16BIT, {0x0400}, 0x0000}, /* SCALE_MODE: 0:disable */
	{MT9E013_16BIT, {0x0404}, 0x0010}, /* SCALE_M = 16 */
	{MT9E013_TOK_TERM, {0}, 0}
};

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

static const struct mt9e013_reg mt9e013_soft_standby[] = {
	{MT9E013_8BIT, {0x301C}, 0x00},
	{MT9E013_TOK_TERM, {0}, 0}
};

static const struct mt9e013_reg mt9e013_streaming[] = {
	{MT9E013_8BIT, {0x301C}, 0x01},
	{MT9E013_TOK_TERM, {0}, 0}
};

static const struct mt9e013_reg mt9e013_param_hold[] = {
	{MT9E013_8BIT, {0x0104}, 0x01},	/* GROUPED_PARAMETER_HOLD */
	{MT9E013_TOK_TERM, {0}, 0}
};

static const struct mt9e013_reg mt9e013_param_update[] = {
	{MT9E013_8BIT, {0x0104}, 0x00},	/* GROUPED_PARAMETER_HOLD */
	{MT9E013_TOK_TERM, {0}, 0}
};

#endif
