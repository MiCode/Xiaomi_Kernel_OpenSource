/*
 * Support for ov5640 Camera Sensor.
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

#ifndef __OV5640_H__
#define __OV5640_H__

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
#include <linux/atomisp_platform.h>
#include <linux/atomisp.h>

#define OV5640_NAME	"ov5640"

#define V4L2_IDENT_OV5640 1111
#define	LAST_REG_SETING	{0xffff, 0xff}

#define OV5640_FOCAL_LENGTH_NUM	439	/*4.39mm*/
#define OV5640_FOCAL_LENGTH_DEM	100
#define OV5640_F_NUMBER_DEFAULT_NUM	24
#define OV5640_F_NUMBER_DEM	10
#define OV5640_FOCUS_ZONE_ARRAY_WIDTH	80
#define OV5640_FOCUS_ZONE_ARRAY_HEIGHT	60

#define OV5640_XVCLK		1920
#define OV5640_AE_TARGET	45
#define OV5640_DEFAULT_GAIN	50
#define OV5640_DEFAULT_SHUTTER	1000

/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define OV5640_FOCAL_LENGTH_DEFAULT 0xD00064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define OV5640_F_NUMBER_DEFAULT 0x18000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define OV5640_F_NUMBER_RANGE 0x180a180a

/* #defines for register writes and register array processing */
#define MISENSOR_8BIT		1
#define MISENSOR_16BIT		2
#define MISENSOR_32BIT		4

#define MISENSOR_TOK_TERM	0xf000	/* terminating token for reg list */
#define MISENSOR_TOK_DELAY	0xfe00	/* delay token for reg list */
#define MISENSOR_TOK_FWLOAD	0xfd00	/* token indicating load FW */
#define MISENSOR_TOK_POLL	0xfc00	/* token indicating poll instruction */

#define I2C_RETRY_COUNT		5
#define MSG_LEN_OFFSET		2

#define OV5640_REG_PID		0x300a
#define OV5640_REG_SYS_RESET	0x3000
#define OV5640_REG_FW_START	0x8000
#define OV5640_REG_FOCUS_MODE	0x3022	/* focus mode reg */
#define OV5640_REG_FOCUS_ZONE_X	0x3024	/* X coordinate of focus zone center */
#define OV5640_REG_FOCUS_ZONE_Y	0x3025	/* Y coordinate of focus zone center */
#define OV5640_REG_FOCUS_STATUS	0x3029	/* focus status reg */

/* system pll control reg */
#define OV5640_REG_PLL_CTRL_0	0x3034
#define OV5640_REG_PLL_CTRL_1	0x3035
#define OV5640_REG_PLL_CTRL_2	0x3036
#define OV5640_REG_PLL_CTRL_3	0x3037

/* pad clock divider for SCCB clock */
#define OV5640_REG_CLK_DIVIDER	0x3108

/* total horizontal size reg */
#define OV5640_REG_TIMING_HTS	0x380c

/* total vertical size reg */
#define OV5640_REG_TIMING_VTS	0x380e

/* exposure output reg */
#define OV5640_REG_EXPOSURE_0	0x3500
#define OV5640_REG_EXPOSURE_1	0x3502

/* gain reg */
#define OV5640_REG_GAIN	0x350a

/* light frequency control reg */
#define OV5640_REG_LIGHT_CTRL_0	0x3c01
#define OV5640_REG_LIGHT_CTRL_1	0x3c00
#define OV5640_REG_LIGHT_CTRL_2	0x3c0c

/* light frequency */
#define OV5640_LIGHT_50HZ	50
#define OV5640_LIGHT_60HZ	60

/* automatic banding filter */
#define OV5640_AUTO_BAND	0x80

/* 60HZ band step reg and 60HZ max bands */
#define OV5640_REG_B60_STEP	0x3a0a
#define OV5640_REG_B60_MAX	0x3a0d

/* 50HZ band step reg and 50HZ max bands */
#define OV5640_REG_B50_STEP	0x3a08
#define OV5640_REG_B50_MAX	0x3a0e

/* AEC domain control reg */
#define OV5640_REG_AE_STAB_IN_H	0x3a0f	/* stable in high */
#define OV5640_REG_AE_STAB_IN_L	0x3a10	/* stable in low */
#define OV5640_REG_AE_STAB_OUT_H	0x3a1b	/* stable out high */
#define OV5640_REG_AE_STAB_OUT_L	0x3a1e	/* stable out low */
#define OV5640_REG_AE_FAST_H	0x3a11	/* fast zone high */
#define OV5640_REG_AE_FAST_L	0x3a1f	/* fast zone low */

/* AEC mode control reg */
#define OV5640_REG_AE_MODE_CTRL	0x3503

#define OV5640_AUTO_AG_AE	0x00	/* auto AG&AE */
#define OV5640_MANUAL_AG_AE	0x03	/* manual AG&AE */

/* AEC system control reg */
#define OV5640_REG_AE_SYS_CTRL	0x3a00

/* image exposure average readout reg */
#define OV5640_REG_AE_AVERAGE	0x56a1

/* frame control reg */
#define OV5640_REG_FRAME_CTRL	0x4202

#define OV5640_FRAME_START	0x00
#define OV5640_FRAME_STOP	0x0f

#define OV5640_MCU_RESET	0x20
#define OV5640_SINGLE_FOCUS	0x03
#define OV5640_CONTINUE_FOCUS	0x04
#define OV5640_PAUSE_FOCUS	0x06
#define OV5640_RELEASE_FOCUS	0x08
#define OV5640_RELAUNCH_FOCUS	0x12
#define OV5640_S_FOCUS_ZONE	0x81

/* focus firmware is downloaded but not to be initialized */
#define OV5640_FOCUS_FW_DL	0x7f
#define OV5640_FOCUS_FW_INIT	0x7e	/* focus firmware is initializing */
#define OV5640_FOCUS_FW_IDLE	0x70	/* focus firmware is idle */
#define OV5640_FOCUS_FW_RUN	0x00	/* focus firmware is running */
#define OV5640_FOCUS_FW_FINISH	0x10	/* focus is finished */

#define OV5640_REG_AWB_CTRL	0x3406

#define OV5640_AWB_GAIN_AUTO	0
#define OV5640_AWB_GAIN_MANUAL	1

#define MIN_SYSCLK		10
#define MIN_VTS			8
#define MIN_HTS			8
#define MIN_SHUTTER		0
#define MIN_GAIN		0

/* OV5640_DEVICE_ID */
#define OV5640_MOD_ID		0x5640

#define AF_FW_PATH	"OV5640_AF_FW.bin"

/* Supported resolutions */
enum {
	OV5640_RES_QVGA,
	OV5640_RES_DVGA,
	OV5640_RES_320P,
	OV5640_RES_360P,
	OV5640_RES_VGA,
	OV5640_RES_480P,
	OV5640_RES_720P,
	OV5640_RES_1080P,
	OV5640_RES_1088P,
	OV5640_RES_D3M,
	OV5640_RES_3M,
	OV5640_RES_D5M,
	OV5640_RES_5M,
};
#define OV5640_RES_5M_SIZE_H		2560
#define OV5640_RES_5M_SIZE_V		1920
#define OV5640_RES_D5M_SIZE_H		2496
#define OV5640_RES_D5M_SIZE_V		1664
#define OV5640_RES_D3M_SIZE_H		2112
#define OV5640_RES_D3M_SIZE_V		1408
#define OV5640_RES_3M_SIZE_H		2048
#define OV5640_RES_3M_SIZE_V		1536
#define OV5640_RES_1088P_SIZE_H		1920
#define OV5640_RES_1088P_SIZE_V		1088
#define OV5640_RES_1080P_SIZE_H		1920
#define OV5640_RES_1080P_SIZE_V		1080
#define OV5640_RES_720P_SIZE_H		1280
#define OV5640_RES_720P_SIZE_V		720
#define OV5640_RES_480P_SIZE_H		720
#define OV5640_RES_480P_SIZE_V		480
#define OV5640_RES_VGA_SIZE_H		640
#define OV5640_RES_VGA_SIZE_V		480
#define OV5640_RES_360P_SIZE_H		640
#define OV5640_RES_360P_SIZE_V		360
#define OV5640_RES_320P_SIZE_H		480
#define OV5640_RES_320P_SIZE_V		320
#define OV5640_RES_DVGA_SIZE_H		416
#define OV5640_RES_DVGA_SIZE_V		312
#define OV5640_RES_QVGA_SIZE_H		320
#define OV5640_RES_QVGA_SIZE_V		240

/*
 * struct misensor_reg - MI sensor  register format
 * @length: length of the register
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 * Define a structure for sensor register initialization values
 */
struct misensor_reg {
	u16 length;
	u16 reg;
	u32 val;	/* value or for read/mod/write */
};

struct regval_list {
	u16 reg_num;
	u8 value;
};

struct ov5640_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;
	struct firmware *firmware;

	struct camera_sensor_platform_data *platform_data;
	int run_mode;
	int focus_mode;
	int night_mode;
	bool focus_mode_change;
	int color_effect;
	bool streaming;
	bool preview_ag_ae;
	u16 sensor_id;
	u8 sensor_revision;
	unsigned int ae_high;
	unsigned int ae_low;
	unsigned int preview_shutter;
	unsigned int preview_gain16;
	unsigned int average;
	unsigned int preview_sysclk;
	unsigned int preview_hts;
	unsigned int preview_vts;
	unsigned int res;
};

struct ov5640_priv_data {
	u32 port;
	u32 num_of_lane;
	u32 input_format;
	u32 raw_bayer_order;
};

struct ov5640_format_struct {
	u8 *desc;
	u32 pixelformat;
	struct regval_list *regs;
};

struct ov5640_res_struct {
	u8 *desc;
	int res;
	int width;
	int height;
	int fps;
	int skip_frames;
	bool used;
	struct regval_list *regs;
};

#define OV5640_MAX_WRITE_BUF_SIZE	32
struct ov5640_write_buffer {
	u16 addr;
	u8 data[OV5640_MAX_WRITE_BUF_SIZE];
};

struct ov5640_write_ctrl {
	int index;
	struct ov5640_write_buffer buffer;
};

struct ov5640_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, int value);
};

#define N_ov5640_FMTS ARRAY_SIZE(ov5640_formats)

/*
 * Modes supported by the mt9m114 driver.
 * Please, keep them in ascending order.
 */
static struct ov5640_res_struct ov5640_res[] = {
	{
	.desc	= "QVGA",
	.res	= OV5640_RES_QVGA,
	.width	= 320,
	.height	= 240,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 4,
	},
	{
	.desc	= "DVGA",
	.res	= OV5640_RES_DVGA,
	.width	= 416,
	.height	= 312,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 4,
	},
	{
	.desc	= "320P",
	.res	= OV5640_RES_320P,
	.width	= 480,
	.height	= 320,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 4,
	},
	{
	.desc	= "360P",
	.res	= OV5640_RES_360P,
	.width	= 640,
	.height	= 360,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 5,
	},
	{
	.desc	= "VGA",
	.res	= OV5640_RES_VGA,
	.width	= 640,
	.height	= 480,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 4,
	},
	{
	.desc	= "480P",
	.res	= OV5640_RES_480P,
	.width	= 720,
	.height	= 480,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 4,
	},
	{
	.desc	= "720p",
	.res	= OV5640_RES_720P,
	.width	= 1280,
	.height	= 720,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 4,
	},
	{
	.desc	= "1080P",
	.res	= OV5640_RES_1080P,
	.width	= 1920,
	.height	= 1080,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 4,
	},
	{
	.desc	= "1088P",
	.res	= OV5640_RES_1088P,
	.width	= 1920,
	.height	= 1088,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 4,
	},
	{
	.desc	= "D3M",
	.res	= OV5640_RES_D3M,
	.width	= 2112,
	.height	= 1408,
	.fps	= 15,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 4,
	},
	{
	.desc	= "3M",
	.res	= OV5640_RES_3M,
	.width	= 2048,
	.height	= 1536,
	.fps	= 15,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 4,
	},
	{
	.desc	= "D5M",
	.res	= OV5640_RES_D5M,
	.width	= 2496,
	.height	= 1664,
	.fps	= 15,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 4,
	},
	{
	.desc	= "5M",
	.res	= OV5640_RES_5M,
	.width	= 2560,
	.height	= 1920,
	.fps	= 15,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 4,
	},
};
#define N_RES (ARRAY_SIZE(ov5640_res))

static const struct i2c_device_id ov5640_id[] = {
	{"ov5640", 0},
	{}
};

static struct misensor_reg const ov5640_standby_reg[] = {
	 {MISENSOR_8BIT,  0x300e, 0x5d},
	 {MISENSOR_8BIT,  0x3008, 0x42},	/* software powerdown */
	 {MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const ov5640_wakeup_reg[] = {
	{MISENSOR_8BIT,  0x3008, 0x02},
	{MISENSOR_8BIT,  0x300e, 0x45},
	{MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const ov5640_normal_effect[] = {
	{MISENSOR_8BIT, 0x3212, 0x03},	/* start group 3 */
	{MISENSOR_8BIT, 0x5580, 0x06},
	{MISENSOR_8BIT, 0x5583, 0x40},	/* sat U */
	{MISENSOR_8BIT, 0x5584, 0x10},	/* sat V */
	{MISENSOR_8BIT, 0x5003, 0x08},
	{MISENSOR_8BIT, 0x3212, 0x13},	/* end group 3 */
	{MISENSOR_8BIT, 0x3212, 0xA3},	/* lanuch group 3 */
	{MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const ov5640_sepia_effect[] = {
	{MISENSOR_8BIT, 0x3212, 0x03},	/* start group 3 */
	{MISENSOR_8BIT, 0x5580, 0x1E},
	{MISENSOR_8BIT, 0x5583, 0x40},	/* sat U */
	{MISENSOR_8BIT, 0x5584, 0xA0},	/* sat V */
	{MISENSOR_8BIT, 0x5003, 0x08},
	{MISENSOR_8BIT, 0x3212, 0x13},	/* end group 3 */
	{MISENSOR_8BIT, 0x3212, 0xA3},	/* lanuch group 3 */
	{MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const ov5640_negative_effect[] = {
	{MISENSOR_8BIT, 0x3212, 0x03},	/* start group 3 */
	{MISENSOR_8BIT, 0x5580, 0x46},
	{MISENSOR_8BIT, 0x5583, 0x40},	/* sat U */
	{MISENSOR_8BIT, 0x5584, 0x10},	/* sat V */
	{MISENSOR_8BIT, 0x5003, 0x08},
	{MISENSOR_8BIT, 0x3212, 0x13},	/* end group 3 */
	{MISENSOR_8BIT, 0x3212, 0xA3},	/* lanuch group 3 */
	{MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const ov5640_bw_effect[] = {
	{MISENSOR_8BIT, 0x3212, 0x03},	/* start group 3 */
	{MISENSOR_8BIT, 0x5580, 0x1E},
	{MISENSOR_8BIT, 0x5583, 0x80},	/* sat U */
	{MISENSOR_8BIT, 0x5584, 0x80},	/* sat V */
	{MISENSOR_8BIT, 0x5003, 0x08},
	{MISENSOR_8BIT, 0x3212, 0x13},	/* end group 3 */
	{MISENSOR_8BIT, 0x3212, 0xA3},	/* lanuch group 3 */
	{MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const ov5640_blue_effect[] = {
	{MISENSOR_8BIT, 0x3212, 0x03},	/* start group 3 */
	{MISENSOR_8BIT, 0x5580, 0x1E},
	{MISENSOR_8BIT, 0x5583, 0xA0},	/* sat U */
	{MISENSOR_8BIT, 0x5584, 0x40},	/* sat V */
	{MISENSOR_8BIT, 0x5003, 0x08},
	{MISENSOR_8BIT, 0x3212, 0x13},	/* end group 3 */
	{MISENSOR_8BIT, 0x3212, 0xA3},	/* lanuch group 3 */
	{MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const ov5640_green_effect[] = {
	{MISENSOR_8BIT, 0x3212, 0x03},	/* start group 3 */
	{MISENSOR_8BIT, 0x5580, 0x1E},
	{MISENSOR_8BIT, 0x5583, 0x60},	/* sat U */
	{MISENSOR_8BIT, 0x5584, 0x60},	/* sat V */
	{MISENSOR_8BIT, 0x5003, 0x08},
	{MISENSOR_8BIT, 0x3212, 0x13},	/* end group 3 */
	{MISENSOR_8BIT, 0x3212, 0xA3},	/* lanuch group 3 */
	{MISENSOR_TOK_TERM, 0, 0}
};

/* 5M, yuv422, 2lanes, mipi, 12fps */
static struct misensor_reg const ov5640_5M_init[] = {
	{MISENSOR_8BIT, 0x3C07, 0x08},
	{MISENSOR_8BIT, 0x3820, 0x40},
	{MISENSOR_8BIT, 0x3821, 0x06},
	{MISENSOR_8BIT, 0x3814, 0x11},
	{MISENSOR_8BIT, 0x3815, 0x11},
	{MISENSOR_8BIT, 0x3800, 0x00},
	{MISENSOR_8BIT, 0x3801, 0x00},	/* X start of input size. value is 0 */
	{MISENSOR_8BIT, 0x3802, 0x00},
	{MISENSOR_8BIT, 0x3803, 0x00},	/* Y start of input size. value is 0 */
	{MISENSOR_8BIT, 0x3804, 0x0A},
	{MISENSOR_8BIT, 0x3805, 0x1F},	/* X end of input size. value is 2591 */
	{MISENSOR_8BIT, 0x3806, 0x07},
	{MISENSOR_8BIT, 0x3807, 0x87},	/* Y end of input size. value is 1927 */
	{MISENSOR_8BIT, 0x3808, 0x0A},
	{MISENSOR_8BIT, 0x3809, 0x00},	/* DVP output H_width, value is 2560 */
	{MISENSOR_8BIT, 0x380A, 0x07},
	{MISENSOR_8BIT, 0x380B, 0x80},	/* DVP output V_heigh, value is 1920 */
	{MISENSOR_8BIT, 0x3810, 0x00},
	{MISENSOR_8BIT, 0x3811, 0x10},	/* X offset of pre-scaling */
	{MISENSOR_8BIT, 0x3812, 0x00},
	{MISENSOR_8BIT, 0x3813, 0x04},	/* Y offset of pre-scaling */
	{MISENSOR_8BIT, 0x3618, 0x04},
	{MISENSOR_8BIT, 0x3612, 0x2B},
	{MISENSOR_8BIT, 0x3708, 0x64},
	{MISENSOR_8BIT, 0x3709, 0x12},
	{MISENSOR_8BIT, 0x370C, 0x00},
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0x27},	/* 50HZ band steps */
	{MISENSOR_8BIT, 0x3A0A, 0x00},
	{MISENSOR_8BIT, 0x3A0B, 0xF6},	/* 60HZ band steps */
	{MISENSOR_8BIT, 0x3A0E, 0x06},	/* 50HZ max band in one frame */
	{MISENSOR_8BIT, 0x3A0D, 0x08},	/* 60HZ max band in one frame */
	{MISENSOR_8BIT, 0x4001, 0x02},	/* BLC start line */
	{MISENSOR_8BIT, 0x4004, 0x06},	/* BLC line number */
	{MISENSOR_8BIT, 0x3000, 0x00},	/* system reset */
	{MISENSOR_8BIT, 0x3001, 0x08},	/* system reset 1 */
	{MISENSOR_8BIT, 0x3002, 0x00},	/* system reset */
	{MISENSOR_8BIT, 0x3004, 0xFF},	/* enable all clock */
	{MISENSOR_8BIT, 0x3006, 0xC3},	/* enable clocks */
	{MISENSOR_8BIT, 0x300E, 0x45},	/* MIPI enable, 2lane mode */
	{MISENSOR_8BIT, 0x302E, 0x08},
	{MISENSOR_8BIT, 0x4713, 0x02},	/* jepg mode select. JPEG mode 2 */
	{MISENSOR_8BIT, 0x4407, 0x0C},	/* jpeg ctrl */
	{MISENSOR_8BIT, 0x440E, 0x00},
	{MISENSOR_8BIT, 0x460B, 0x37},	/* debug mode */
	{MISENSOR_8BIT, 0x460C, 0x20},	/* VFIFO ctrl */
	{MISENSOR_8BIT, 0x3824, 0x01},	/* VFIFO */
	{MISENSOR_8BIT, 0x5001, 0x83},	/* isp ctrl */
	{MISENSOR_8BIT, 0x4005, 0x1A},	/* BLC ctrl */
	/* PLL */
	{MISENSOR_8BIT, 0x3034, 0x18},
	{MISENSOR_8BIT, 0x3035, 0x11},
	{MISENSOR_8BIT, 0x3036, 0x68},
	{MISENSOR_8BIT, 0x3037, 0x13},
	{MISENSOR_8BIT, 0x3108, 0x01},
	{MISENSOR_8BIT, 0x3824, 0x01},
	{MISENSOR_8BIT, 0x460C, 0x20},
	{MISENSOR_8BIT, 0x300E, 0x45},
	{MISENSOR_8BIT, 0x303B, 0x14},
	{MISENSOR_8BIT, 0x303C, 0x11},
	{MISENSOR_8BIT, 0x303D, 0x17},
	{MISENSOR_8BIT, 0x380C, 0x0A},
	{MISENSOR_8BIT, 0x380D, 0xB4},	/* total H-size is 2740 */
	{MISENSOR_8BIT, 0x380E, 0x07},
	{MISENSOR_8BIT, 0x380F, 0xE8},	/* total v-size is 2024 */
	{MISENSOR_8BIT, 0x3A02, 0x07},
	{MISENSOR_8BIT, 0x3A03, 0xE4},	/* 60HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A14, 0x07},
	{MISENSOR_8BIT, 0x3A15, 0xE4},	/* 50HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0x2F},
	{MISENSOR_8BIT, 0x3A0A, 0x00},
	{MISENSOR_8BIT, 0x3A0B, 0xFD},
	{MISENSOR_8BIT, 0x3A0E, 0x06},
	{MISENSOR_8BIT, 0x3A0D, 0x08},
	{MISENSOR_8BIT, 0x4837, 0x0C},
	{MISENSOR_8BIT, 0x5306, 0x08},
	{MISENSOR_8BIT, 0x5308, 0x00},
	{MISENSOR_TOK_TERM, 0, 0}
};

/* D5M, yuv422, 2lanes, mipi, 15fps */
static struct misensor_reg const ov5640_D5M_init[] = {
	{MISENSOR_8BIT, 0x3C07, 0x08},
	{MISENSOR_8BIT, 0x3820, 0x40},
	{MISENSOR_8BIT, 0x3821, 0x06},
	{MISENSOR_8BIT, 0x3814, 0x11},
	{MISENSOR_8BIT, 0x3815, 0x11},
	{MISENSOR_8BIT, 0x3800, 0x00},
	{MISENSOR_8BIT, 0x3801, 0x00},	/* X start of input size. value is 0 */
	{MISENSOR_8BIT, 0x3802, 0x00},
	{MISENSOR_8BIT, 0x3803, 0x00},	/* Y start of input size. value is 0 */
	{MISENSOR_8BIT, 0x3804, 0x09},
	{MISENSOR_8BIT, 0x3805, 0xDF},	/* X end of input size. value is 2527 */
	{MISENSOR_8BIT, 0x3806, 0x06},
	{MISENSOR_8BIT, 0x3807, 0x87},	/* Y end of input size. value is 1671 */
	{MISENSOR_8BIT, 0x3808, 0x09},
	{MISENSOR_8BIT, 0x3809, 0xC0},	/* DVP output H_width, value is 2496 */
	{MISENSOR_8BIT, 0x380A, 0x06},
	{MISENSOR_8BIT, 0x380B, 0x80},	/* DVP output V_heigh, value is 1664 */
	{MISENSOR_8BIT, 0x3810, 0x00},
	{MISENSOR_8BIT, 0x3811, 0x10},	/* X offset of pre-scaling */
	{MISENSOR_8BIT, 0x3812, 0x00},
	{MISENSOR_8BIT, 0x3813, 0x04},	/* Y offset of pre-scaling */
	{MISENSOR_8BIT, 0x3618, 0x04},
	{MISENSOR_8BIT, 0x3612, 0x2B},
	{MISENSOR_8BIT, 0x3708, 0x64},
	{MISENSOR_8BIT, 0x3709, 0x12},
	{MISENSOR_8BIT, 0x370C, 0x00},
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0x24},	/* 50HZ band steps */
	{MISENSOR_8BIT, 0x3A0A, 0x00},
	{MISENSOR_8BIT, 0x3A0B, 0xF3},	/* 60HZ band steps */
	{MISENSOR_8BIT, 0x3A0E, 0x08},	/* 50HZ max band in one frame */
	{MISENSOR_8BIT, 0x3A0D, 0x09},	/* 60HZ max band in one frame */
	{MISENSOR_8BIT, 0x4001, 0x02},	/* BLC start line */
	{MISENSOR_8BIT, 0x4004, 0x06},	/* BLC line number */
	{MISENSOR_8BIT, 0x3000, 0x00},	/* system reset */
	{MISENSOR_8BIT, 0x3001, 0x08},	/* system reset 1 */
	{MISENSOR_8BIT, 0x3002, 0x00},	/* system reset */
	{MISENSOR_8BIT, 0x3004, 0xFF},	/* enable all clock */
	{MISENSOR_8BIT, 0x3006, 0xC3},	/* enable clocks */
	{MISENSOR_8BIT, 0x300E, 0x45},	/* MIPI enable, 2lane mode */
	{MISENSOR_8BIT, 0x302E, 0x08},
	{MISENSOR_8BIT, 0x4713, 0x02},	/* jepg mode select. JPEG mode 2 */
	{MISENSOR_8BIT, 0x4407, 0x0C},	/* jpeg ctrl */
	{MISENSOR_8BIT, 0x440E, 0x00},
	{MISENSOR_8BIT, 0x460B, 0x37},	/* debug mode */
	{MISENSOR_8BIT, 0x460C, 0x20},	/* VFIFO ctrl */
	{MISENSOR_8BIT, 0x3824, 0x01},	/* VFIFO */
	{MISENSOR_8BIT, 0x5001, 0x83},	/* isp ctrl */
	{MISENSOR_8BIT, 0x4005, 0x1A},	/* BLC ctrl */
	/* PLL */
	{MISENSOR_8BIT, 0x3034, 0x18},
	{MISENSOR_8BIT, 0x3035, 0x11},
	{MISENSOR_8BIT, 0x3036, 0x68},
	{MISENSOR_8BIT, 0x3037, 0x13},
	{MISENSOR_8BIT, 0x3108, 0x01},
	{MISENSOR_8BIT, 0x3824, 0x01},
	{MISENSOR_8BIT, 0x460C, 0x20},
	{MISENSOR_8BIT, 0x300E, 0x45},
	{MISENSOR_8BIT, 0x303B, 0x14},
	{MISENSOR_8BIT, 0x303C, 0x11},
	{MISENSOR_8BIT, 0x303D, 0x17},
	{MISENSOR_8BIT, 0x380C, 0x0B},
	{MISENSOR_8BIT, 0x380D, 0x1C},	/* total H-size is 2844 */
	{MISENSOR_8BIT, 0x380E, 0x07},
	{MISENSOR_8BIT, 0x380F, 0x95},	/* total v-size is 1941 */
	{MISENSOR_8BIT, 0x3A02, 0x07},
	{MISENSOR_8BIT, 0x3A03, 0x91},	/* 60HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A14, 0x07},
	{MISENSOR_8BIT, 0x3A15, 0x91},	/* 50HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0x24},
	{MISENSOR_8BIT, 0x3A0A, 0x00},
	{MISENSOR_8BIT, 0x3A0B, 0xF3},
	{MISENSOR_8BIT, 0x3A0E, 0x06},
	{MISENSOR_8BIT, 0x3A0D, 0x07},
	{MISENSOR_8BIT, 0x4837, 0x0C},
	{MISENSOR_8BIT, 0x5306, 0x08},
	{MISENSOR_8BIT, 0x5308, 0x00},
	{MISENSOR_TOK_TERM, 0, 0}
};

/* 3M, yuv422, 2lanes, mipi, 15fps */
static struct misensor_reg const ov5640_3M_init[] = {
	{MISENSOR_8BIT, 0x3C07, 0x08},
	{MISENSOR_8BIT, 0x3820, 0x40},
	{MISENSOR_8BIT, 0x3821, 0x06},
	{MISENSOR_8BIT, 0x3814, 0x11},
	{MISENSOR_8BIT, 0x3815, 0x11},
	{MISENSOR_8BIT, 0x3800, 0x00},
	{MISENSOR_8BIT, 0x3801, 0x00},	/* X start of input size. value is 0 */
	{MISENSOR_8BIT, 0x3802, 0x00},
	{MISENSOR_8BIT, 0x3803, 0x00},	/* Y start of input size. value is 0 */
	{MISENSOR_8BIT, 0x3804, 0x0A},
	{MISENSOR_8BIT, 0x3805, 0x1F},	/* X end of input size. value is 2591 */
	{MISENSOR_8BIT, 0x3806, 0x07},
	{MISENSOR_8BIT, 0x3807, 0x87},	/* Y end of input size. value is 1927 */
	{MISENSOR_8BIT, 0x3808, 0x08},
	{MISENSOR_8BIT, 0x3809, 0x00},	/* DVP output H_width, value is 2048 */
	{MISENSOR_8BIT, 0x380A, 0x06},
	{MISENSOR_8BIT, 0x380B, 0x00},	/* DVP output V_heigh, value is 1536 */
	{MISENSOR_8BIT, 0x3810, 0x00},
	{MISENSOR_8BIT, 0x3811, 0x10},	/* X offset of pre-scaling */
	{MISENSOR_8BIT, 0x3812, 0x00},
	{MISENSOR_8BIT, 0x3813, 0x04},	/* Y offset of pre-scaling */
	{MISENSOR_8BIT, 0x3618, 0x04},
	{MISENSOR_8BIT, 0x3612, 0x2B},
	{MISENSOR_8BIT, 0x3708, 0x64},
	{MISENSOR_8BIT, 0x3709, 0x12},
	{MISENSOR_8BIT, 0x370C, 0x00},
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0x24},	/* 50HZ band steps */
	{MISENSOR_8BIT, 0x3A0A, 0x00},
	{MISENSOR_8BIT, 0x3A0B, 0xF3},	/* 60HZ band steps */
	{MISENSOR_8BIT, 0x3A0E, 0x08},	/* 50HZ max band in one frame */
	{MISENSOR_8BIT, 0x3A0D, 0x09},	/* 60HZ max band in one frame */
	{MISENSOR_8BIT, 0x4001, 0x02},	/* BLC start line */
	{MISENSOR_8BIT, 0x4004, 0x06},	/* BLC line number */
	{MISENSOR_8BIT, 0x3000, 0x00},	/* system reset */
	{MISENSOR_8BIT, 0x3001, 0x08},	/* system reset 1 */
	{MISENSOR_8BIT, 0x3002, 0x00},	/* system reset */
	{MISENSOR_8BIT, 0x3004, 0xFF},	/* enable all clock */
	{MISENSOR_8BIT, 0x3006, 0xC3},	/* enable clocks */
	{MISENSOR_8BIT, 0x300E, 0x45},	/* MIPI enable, 2lane mode */
	{MISENSOR_8BIT, 0x302E, 0x08},
	{MISENSOR_8BIT, 0x4713, 0x02},	/* jepg mode select. JPEG mode 2 */
	{MISENSOR_8BIT, 0x4407, 0x0C},	/* jpeg ctrl */
	{MISENSOR_8BIT, 0x440E, 0x00},
	{MISENSOR_8BIT, 0x460B, 0x37},	/* debug mode */
	{MISENSOR_8BIT, 0x460C, 0x20},	/* VFIFO ctrl */
	{MISENSOR_8BIT, 0x3824, 0x01},	/* VFIFO */
	{MISENSOR_8BIT, 0x5001, 0xA3},	/* isp ctrl, enable downscaling */
	{MISENSOR_8BIT, 0x4005, 0x1A},	/* BLC ctrl */
	/* PLL */
	{MISENSOR_8BIT, 0x3034, 0x18},
	{MISENSOR_8BIT, 0x3035, 0x11},
	{MISENSOR_8BIT, 0x3036, 0x68},
	{MISENSOR_8BIT, 0x3037, 0x13},
	{MISENSOR_8BIT, 0x3108, 0x01},
	{MISENSOR_8BIT, 0x3824, 0x01},
	{MISENSOR_8BIT, 0x460C, 0x20},
	{MISENSOR_8BIT, 0x300E, 0x45},
	{MISENSOR_8BIT, 0x303B, 0x14},
	{MISENSOR_8BIT, 0x303C, 0x11},
	{MISENSOR_8BIT, 0x303D, 0x17},
	{MISENSOR_8BIT, 0x380C, 0x0A},
	{MISENSOR_8BIT, 0x380D, 0xA0},	/* total H-size is 2720 */
	{MISENSOR_8BIT, 0x380E, 0x07},
	{MISENSOR_8BIT, 0x380F, 0xF6},	/* total v-size is 2038 */
	{MISENSOR_8BIT, 0x3A02, 0x07},
	{MISENSOR_8BIT, 0x3A03, 0xF2},	/* 60HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A14, 0x07},
	{MISENSOR_8BIT, 0x3A15, 0xF2},	/* 50HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0x31},
	{MISENSOR_8BIT, 0x3A0A, 0x00},
	{MISENSOR_8BIT, 0x3A0B, 0xFE},
	{MISENSOR_8BIT, 0x3A0E, 0x06},
	{MISENSOR_8BIT, 0x3A0D, 0x08},
	{MISENSOR_8BIT, 0x4837, 0x0C},
	{MISENSOR_8BIT, 0x5306, 0x08},
	{MISENSOR_8BIT, 0x5308, 0x00},
	{MISENSOR_TOK_TERM, 0, 0}
};

/* D3M, yuv422, 2lanes, mipi, 15fps */
static struct misensor_reg const ov5640_D3M_init[] = {
	{MISENSOR_8BIT, 0x3C07, 0x08},
	{MISENSOR_8BIT, 0x3820, 0x40},
	{MISENSOR_8BIT, 0x3821, 0x06},
	{MISENSOR_8BIT, 0x3814, 0x11},
	{MISENSOR_8BIT, 0x3815, 0x11},
	{MISENSOR_8BIT, 0x3800, 0x00},
	{MISENSOR_8BIT, 0x3801, 0x00},	/* X start of input size. value is 0 */
	{MISENSOR_8BIT, 0x3802, 0x00},
	{MISENSOR_8BIT, 0x3803, 0x00},	/* Y start of input size. value is 0 */
	{MISENSOR_8BIT, 0x3804, 0x09},
	{MISENSOR_8BIT, 0x3805, 0xDF},	/* X end of input size. value is 2527 */
	{MISENSOR_8BIT, 0x3806, 0x06},
	{MISENSOR_8BIT, 0x3807, 0x87},	/* Y end of input size. value is 1671 */
	{MISENSOR_8BIT, 0x3808, 0x08},
	{MISENSOR_8BIT, 0x3809, 0x40},	/* DVP output H_width, value is 2112 */
	{MISENSOR_8BIT, 0x380A, 0x05},
	{MISENSOR_8BIT, 0x380B, 0x80},	/* DVP output V_heigh, value is 1408 */
	{MISENSOR_8BIT, 0x3810, 0x00},
	{MISENSOR_8BIT, 0x3811, 0x10},	/* X offset of pre-scaling */
	{MISENSOR_8BIT, 0x3812, 0x00},
	{MISENSOR_8BIT, 0x3813, 0x04},	/* Y offset of pre-scaling */
	{MISENSOR_8BIT, 0x3618, 0x04},
	{MISENSOR_8BIT, 0x3612, 0x2B},
	{MISENSOR_8BIT, 0x3708, 0x64},
	{MISENSOR_8BIT, 0x3709, 0x12},
	{MISENSOR_8BIT, 0x370C, 0x00},
	{MISENSOR_8BIT, 0x3A02, 0x07},
	{MISENSOR_8BIT, 0x3A03, 0xB0},	/* 60HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0x24},	/* 50HZ band steps */
	{MISENSOR_8BIT, 0x3A0A, 0x00},
	{MISENSOR_8BIT, 0x3A0B, 0xF3},	/* 60HZ band steps */
	{MISENSOR_8BIT, 0x3A0E, 0x08},	/* 50HZ max band in one frame */
	{MISENSOR_8BIT, 0x3A0D, 0x09},	/* 60HZ max band in one frame */
	{MISENSOR_8BIT, 0x3A14, 0x07},
	{MISENSOR_8BIT, 0x3A15, 0xB0},	/* 50HZ max exposure output limit */
	{MISENSOR_8BIT, 0x4001, 0x02},	/* BLC start line */
	{MISENSOR_8BIT, 0x4004, 0x06},	/* BLC line number */
	{MISENSOR_8BIT, 0x3000, 0x00},	/* system reset */
	{MISENSOR_8BIT, 0x3001, 0x08},	/* system reset 1 */
	{MISENSOR_8BIT, 0x3002, 0x00},	/* system reset */
	{MISENSOR_8BIT, 0x3004, 0xFF},	/* enable all clock */
	{MISENSOR_8BIT, 0x3006, 0xC3},	/* enable clocks */
	{MISENSOR_8BIT, 0x300E, 0x45},	/* MIPI enable, 2lane mode */
	{MISENSOR_8BIT, 0x302E, 0x08},
	{MISENSOR_8BIT, 0x4713, 0x02},	/* jepg mode select. JPEG mode 2 */
	{MISENSOR_8BIT, 0x4407, 0x0C},	/* jpeg ctrl */
	{MISENSOR_8BIT, 0x440E, 0x00},
	{MISENSOR_8BIT, 0x460B, 0x37},	/* debug mode */
	{MISENSOR_8BIT, 0x460C, 0x20},	/* VFIFO ctrl */
	{MISENSOR_8BIT, 0x3824, 0x01},	/* VFIFO */
	{MISENSOR_8BIT, 0x5001, 0xA3},	/* isp ctrl, enable downscaling */
	{MISENSOR_8BIT, 0x4005, 0x1A},	/* BLC ctrl */
	/* PLL */
	{MISENSOR_8BIT, 0x3034, 0x18},
	{MISENSOR_8BIT, 0x3035, 0x11},
	{MISENSOR_8BIT, 0x3036, 0x68},
	{MISENSOR_8BIT, 0x3037, 0x13},
	{MISENSOR_8BIT, 0x3108, 0x01},
	{MISENSOR_8BIT, 0x3824, 0x01},
	{MISENSOR_8BIT, 0x460C, 0x20},
	{MISENSOR_8BIT, 0x300E, 0x45},
	{MISENSOR_8BIT, 0x303B, 0x14},
	{MISENSOR_8BIT, 0x303C, 0x11},
	{MISENSOR_8BIT, 0x303D, 0x17},
	{MISENSOR_8BIT, 0x380C, 0x0B},
	{MISENSOR_8BIT, 0x380D, 0x1C},	/* total H-size is 2844 */
	{MISENSOR_8BIT, 0x380E, 0x07},
	{MISENSOR_8BIT, 0x380F, 0x95},	/* total v-size is 1941 */
	{MISENSOR_8BIT, 0x3A02, 0x07},
	{MISENSOR_8BIT, 0x3A03, 0x91},	/* 60HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A14, 0x07},
	{MISENSOR_8BIT, 0x3A15, 0x91},	/* 50HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0x24},
	{MISENSOR_8BIT, 0x3A0A, 0x00},
	{MISENSOR_8BIT, 0x3A0B, 0xF3},
	{MISENSOR_8BIT, 0x3A0E, 0x06},
	{MISENSOR_8BIT, 0x3A0D, 0x07},
	{MISENSOR_8BIT, 0x4837, 0x0C},
	{MISENSOR_8BIT, 0x5306, 0x08},
	{MISENSOR_8BIT, 0x5308, 0x00},
	{MISENSOR_TOK_TERM, 0, 0}
};

/* camera: 1088p, yuv422, 2lanes, mipi, 30fps */
static struct misensor_reg const ov5640_1088p_init[] = {
	{MISENSOR_8BIT, 0x3017, 0x00},
	{MISENSOR_8BIT, 0x3018, 0x00},
	{MISENSOR_8BIT, 0x3C04, 0x28},
	{MISENSOR_8BIT, 0x3C05, 0x98},
	{MISENSOR_8BIT, 0x3C06, 0x00},
	{MISENSOR_8BIT, 0x3C07, 0x08},
	{MISENSOR_8BIT, 0x3C08, 0x00},
	{MISENSOR_8BIT, 0x3C09, 0x1C},
	{MISENSOR_8BIT, 0x3C0A, 0x9C},
	{MISENSOR_8BIT, 0x3C0B, 0x40},
	{MISENSOR_8BIT, 0x3820, 0x41},
	{MISENSOR_8BIT, 0x3821, 0x06}, /* disable binning */
	{MISENSOR_8BIT, 0x3814, 0x11},
	{MISENSOR_8BIT, 0x3815, 0x11},
	{MISENSOR_8BIT, 0x3800, 0x01},
	{MISENSOR_8BIT, 0x3801, 0x50},
	{MISENSOR_8BIT, 0x3802, 0x01},
	{MISENSOR_8BIT, 0x3803, 0xB2},
	{MISENSOR_8BIT, 0x3804, 0x08},
	{MISENSOR_8BIT, 0x3805, 0xEF},	/* X end of isp input size */
	{MISENSOR_8BIT, 0x3806, 0x05},
	{MISENSOR_8BIT, 0x3807, 0xF9},	/* Y end of isp input size */
	{MISENSOR_8BIT, 0x3808, 0x07},
	{MISENSOR_8BIT, 0x3809, 0x80},	/* 1920 */
	{MISENSOR_8BIT, 0x380A, 0x04},
	{MISENSOR_8BIT, 0x380B, 0x40},	/* 1088 */
	{MISENSOR_8BIT, 0x3810, 0x00},
	{MISENSOR_8BIT, 0x3811, 0x10},
	{MISENSOR_8BIT, 0x3812, 0x00},
	{MISENSOR_8BIT, 0x3813, 0x04},
	{MISENSOR_8BIT, 0x3618, 0x04},
	{MISENSOR_8BIT, 0x3612, 0x2B},
	{MISENSOR_8BIT, 0x3708, 0x62},
	{MISENSOR_8BIT, 0x3709, 0x12},
	{MISENSOR_8BIT, 0x370C, 0x00},
	{MISENSOR_8BIT, 0x3A02, 0x04},
	{MISENSOR_8BIT, 0x3A03, 0x60},
	{MISENSOR_8BIT, 0x3A14, 0x04},
	{MISENSOR_8BIT, 0x3A15, 0x60},
	{MISENSOR_8BIT, 0x4001, 0x02},
	{MISENSOR_8BIT, 0x4004, 0x06},
	{MISENSOR_8BIT, 0x4005, 0x1A}, /* BLC always update */
	{MISENSOR_8BIT, 0x3000, 0x00},
	{MISENSOR_8BIT, 0x3001, 0x08},	/* system reset 1 */
	{MISENSOR_8BIT, 0x3002, 0x1C},
	{MISENSOR_8BIT, 0x3004, 0xFF},
	{MISENSOR_8BIT, 0x3006, 0xC3},
	{MISENSOR_8BIT, 0x302E, 0x08},
	{MISENSOR_8BIT, 0x4300, 0x32},
	{MISENSOR_8BIT, 0x501F, 0x00},
	{MISENSOR_8BIT, 0x4713, 0x02},
	{MISENSOR_8BIT, 0x4407, 0x04},
	{MISENSOR_8BIT, 0x440E, 0x00},
	{MISENSOR_8BIT, 0x460B, 0x35},
	{MISENSOR_8BIT, 0x5001, 0x83},
	{MISENSOR_8BIT, 0x5025, 0x00},
	/* PLL */
	{MISENSOR_8BIT, 0x3034, 0x18},
	{MISENSOR_8BIT, 0x3035, 0x11},
	{MISENSOR_8BIT, 0x3036, 0x68},
	{MISENSOR_8BIT, 0x3037, 0x13},
	{MISENSOR_8BIT, 0x3108, 0x01},
	{MISENSOR_8BIT, 0x3824, 0x02},
	{MISENSOR_8BIT, 0x460C, 0x20},
	{MISENSOR_8BIT, 0x300E, 0x45},
	{MISENSOR_8BIT, 0x303B, 0x14},
	{MISENSOR_8BIT, 0x303C, 0x11},
	{MISENSOR_8BIT, 0x303D, 0x17},
	{MISENSOR_8BIT, 0x380C, 0x09},
	{MISENSOR_8BIT, 0x380D, 0x77},	/* h-size is 2423 */
	{MISENSOR_8BIT, 0x380E, 0x04},
	{MISENSOR_8BIT, 0x380F, 0xCA},	/* v-size is 1226 */
	{MISENSOR_8BIT, 0x3A02, 0x04},
	{MISENSOR_8BIT, 0x3A03, 0xC6},	/* 60HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A14, 0x04},
	{MISENSOR_8BIT, 0x3A15, 0xC6},	/* 50HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0x57},
	{MISENSOR_8BIT, 0x3A0A, 0x01},
	{MISENSOR_8BIT, 0x3A0B, 0x1E},
	{MISENSOR_8BIT, 0x3A0E, 0x03},
	{MISENSOR_8BIT, 0x3A0D, 0x04},
	{MISENSOR_8BIT, 0x4837, 0x0C},
	{MISENSOR_8BIT, 0x5306, 0x18},
	{MISENSOR_8BIT, 0x5308, 0x10},
	{MISENSOR_TOK_TERM, 0, 0}
};

/* camera: 1080p, yuv422, 2lanes, mipi, 30fps */
static struct misensor_reg const ov5640_1080p_init[] = {
	{MISENSOR_8BIT, 0x3017, 0x00},
	{MISENSOR_8BIT, 0x3018, 0x00},
	{MISENSOR_8BIT, 0x3C04, 0x28},
	{MISENSOR_8BIT, 0x3C05, 0x98},
	{MISENSOR_8BIT, 0x3C06, 0x00},
	{MISENSOR_8BIT, 0x3C07, 0x08},
	{MISENSOR_8BIT, 0x3C08, 0x00},
	{MISENSOR_8BIT, 0x3C09, 0x1C},
	{MISENSOR_8BIT, 0x3C0A, 0x9C},
	{MISENSOR_8BIT, 0x3C0B, 0x40},
	{MISENSOR_8BIT, 0x3820, 0x41},
	{MISENSOR_8BIT, 0x3821, 0x06},
	{MISENSOR_8BIT, 0x3814, 0x11},
	{MISENSOR_8BIT, 0x3815, 0x11},
	{MISENSOR_8BIT, 0x3800, 0x01},
	{MISENSOR_8BIT, 0x3801, 0x50},
	{MISENSOR_8BIT, 0x3802, 0x01},
	{MISENSOR_8BIT, 0x3803, 0xB2},
	{MISENSOR_8BIT, 0x3804, 0x08},
	{MISENSOR_8BIT, 0x3805, 0xEF},
	{MISENSOR_8BIT, 0x3806, 0x05},
	{MISENSOR_8BIT, 0x3807, 0xF2},
	{MISENSOR_8BIT, 0x3808, 0x07},
	{MISENSOR_8BIT, 0x3809, 0x80},
	{MISENSOR_8BIT, 0x380A, 0x04},
	{MISENSOR_8BIT, 0x380B, 0x38},
	{MISENSOR_8BIT, 0x3810, 0x00},
	{MISENSOR_8BIT, 0x3811, 0x10},
	{MISENSOR_8BIT, 0x3812, 0x00},
	{MISENSOR_8BIT, 0x3813, 0x06},
	{MISENSOR_8BIT, 0x3618, 0x04},
	{MISENSOR_8BIT, 0x3612, 0x2B},
	{MISENSOR_8BIT, 0x3708, 0x62},
	{MISENSOR_8BIT, 0x3709, 0x12},
	{MISENSOR_8BIT, 0x370C, 0x00},
	{MISENSOR_8BIT, 0x4001, 0x02},
	{MISENSOR_8BIT, 0x4004, 0x06},
	{MISENSOR_8BIT, 0x4005, 0x1A}, /* BLC always update */
	{MISENSOR_8BIT, 0x3000, 0x00},
	{MISENSOR_8BIT, 0x3001, 0x08},	/* system reset 1 */
	{MISENSOR_8BIT, 0x3002, 0x1C},
	{MISENSOR_8BIT, 0x3004, 0xFF},
	{MISENSOR_8BIT, 0x3006, 0xC3},
	{MISENSOR_8BIT, 0x302E, 0x08},
	{MISENSOR_8BIT, 0x4300, 0x32},
	{MISENSOR_8BIT, 0x501F, 0x00},
	{MISENSOR_8BIT, 0x4713, 0x02},
	{MISENSOR_8BIT, 0x4407, 0x04},
	{MISENSOR_8BIT, 0x440E, 0x00},
	{MISENSOR_8BIT, 0x460B, 0x35},
	{MISENSOR_8BIT, 0x5001, 0x83},
	{MISENSOR_8BIT, 0x5025, 0x00},
	/* PLL */
	{MISENSOR_8BIT, 0x3034, 0x18},
	{MISENSOR_8BIT, 0x3035, 0x11},
	{MISENSOR_8BIT, 0x3036, 0x68},
	{MISENSOR_8BIT, 0x3037, 0x13},
	{MISENSOR_8BIT, 0x3108, 0x01},
	{MISENSOR_8BIT, 0x3824, 0x02},
	{MISENSOR_8BIT, 0x460C, 0x20},
	{MISENSOR_8BIT, 0x300E, 0x45},
	{MISENSOR_8BIT, 0x303B, 0x14},
	{MISENSOR_8BIT, 0x303C, 0x11},
	{MISENSOR_8BIT, 0x303D, 0x17},
	{MISENSOR_8BIT, 0x380C, 0x09},
	{MISENSOR_8BIT, 0x380D, 0x77},	/* h-size is 2423 */
	{MISENSOR_8BIT, 0x380E, 0x04},
	{MISENSOR_8BIT, 0x380F, 0xCA},	/* v-size is 1226 */
	{MISENSOR_8BIT, 0x3A02, 0x04},
	{MISENSOR_8BIT, 0x3A03, 0xC6},	/* 60HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A14, 0x04},
	{MISENSOR_8BIT, 0x3A15, 0xC6},	/* 50HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0x57},
	{MISENSOR_8BIT, 0x3A0A, 0x01},
	{MISENSOR_8BIT, 0x3A0B, 0x1E},
	{MISENSOR_8BIT, 0x3A0E, 0x03},
	{MISENSOR_8BIT, 0x3A0D, 0x04},
	{MISENSOR_8BIT, 0x4837, 0x0C},
	{MISENSOR_8BIT, 0x5306, 0x18},
	{MISENSOR_8BIT, 0x5308, 0x10},
	{MISENSOR_TOK_TERM, 0, 0}
};

/* camera: 720p, yuv422, 2lanes, mipi, 30fps */
static struct misensor_reg const ov5640_720p_init[] = {
	{MISENSOR_8BIT, 0x3C07, 0x08},
	{MISENSOR_8BIT, 0x3820, 0x40},
	{MISENSOR_8BIT, 0x3821, 0x06},
	{MISENSOR_8BIT, 0x3814, 0x11},
	{MISENSOR_8BIT, 0x3815, 0x11},
	{MISENSOR_8BIT, 0x3800, 0x01},
	{MISENSOR_8BIT, 0x3801, 0x50},	/* X start of input size */
	{MISENSOR_8BIT, 0x3802, 0x01},
	{MISENSOR_8BIT, 0x3803, 0xB2},	/* Y start of input size */
	{MISENSOR_8BIT, 0x3804, 0x08},
	{MISENSOR_8BIT, 0x3805, 0xEF},	/* X end of input size */
	{MISENSOR_8BIT, 0x3806, 0x05},
	{MISENSOR_8BIT, 0x3807, 0xF2},	/* Y end of input size */
	{MISENSOR_8BIT, 0x3808, 0x05},
	{MISENSOR_8BIT, 0x3809, 0x00},	/* DVP output H_width */
	{MISENSOR_8BIT, 0x380A, 0x02},
	{MISENSOR_8BIT, 0x380B, 0xD0},	/* DVP output V_heigh */
	{MISENSOR_8BIT, 0x3810, 0x00},
	{MISENSOR_8BIT, 0x3811, 0x10},	/* X offset of pre-scaling */
	{MISENSOR_8BIT, 0x3812, 0x00},
	{MISENSOR_8BIT, 0x3813, 0x04},	/* Y offset of pre-scaling */
	{MISENSOR_8BIT, 0x3618, 0x04},
	{MISENSOR_8BIT, 0x3612, 0x2B},
	{MISENSOR_8BIT, 0x3708, 0x64},
	{MISENSOR_8BIT, 0x3709, 0x12},
	{MISENSOR_8BIT, 0x370C, 0x00},
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0x97},
	{MISENSOR_8BIT, 0x3A0A, 0x01},
	{MISENSOR_8BIT, 0x3A0B, 0x53},
	{MISENSOR_8BIT, 0x3A0E, 0x03},
	{MISENSOR_8BIT, 0x3A0D, 0x03},
	{MISENSOR_8BIT, 0x4001, 0x02},	/* BLC start line */
	{MISENSOR_8BIT, 0x4004, 0x06},	/* BLC line number */
	{MISENSOR_8BIT, 0x3000, 0x00},	/* system reset */
	{MISENSOR_8BIT, 0x3001, 0x08},	/* system reset 1 */
	{MISENSOR_8BIT, 0x3002, 0x00},	/* system reset */
	{MISENSOR_8BIT, 0x3004, 0xFF},	/* enable all clock */
	{MISENSOR_8BIT, 0x3006, 0xC3},	/* enable clocks */
	{MISENSOR_8BIT, 0x300E, 0x45},	/* MIPI enable, 2lane mode */
	{MISENSOR_8BIT, 0x302E, 0x08},
	{MISENSOR_8BIT, 0x4713, 0x02},	/* jepg mode select. JPEG mode 2 */
	{MISENSOR_8BIT, 0x4407, 0x0C},	/* jpeg ctrl */
	{MISENSOR_8BIT, 0x440E, 0x00},
	{MISENSOR_8BIT, 0x460B, 0x37},	/* debug mode */
	{MISENSOR_8BIT, 0x460C, 0x20},	/* VFIFO ctrl */
	{MISENSOR_8BIT, 0x3824, 0x01},	/* VFIFO */
	{MISENSOR_8BIT, 0x5001, 0xA3},	/* isp ctrl, enable downscaling*/
	{MISENSOR_8BIT, 0x4005, 0x1A},	/* BLC ctrl */
	/* PLL */
	{MISENSOR_8BIT, 0x3034, 0x18},
	{MISENSOR_8BIT, 0x3035, 0x11},
	{MISENSOR_8BIT, 0x3036, 0x68},	/* same with 1080p */
	{MISENSOR_8BIT, 0x3037, 0x13},
	{MISENSOR_8BIT, 0x3108, 0x01},
	{MISENSOR_8BIT, 0x3824, 0x01},
	{MISENSOR_8BIT, 0x460C, 0x20},
	{MISENSOR_8BIT, 0x300E, 0x45},
	{MISENSOR_8BIT, 0x303B, 0x14},
	{MISENSOR_8BIT, 0x303C, 0x11},
	{MISENSOR_8BIT, 0x303D, 0x17},
	{MISENSOR_8BIT, 0x380C, 0x09},
	{MISENSOR_8BIT, 0x380D, 0x18},	/* total H-size is 2328 */
	{MISENSOR_8BIT, 0x380E, 0x04},
	{MISENSOR_8BIT, 0x380F, 0xA8},	/* total v-size is 1192 */
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0x66},
	{MISENSOR_8BIT, 0x3A0A, 0x01},
	{MISENSOR_8BIT, 0x3A0B, 0x2A},
	{MISENSOR_8BIT, 0x3A0E, 0x03},
	{MISENSOR_8BIT, 0x3A0D, 0x04},
	{MISENSOR_8BIT, 0x3A02, 0x04},
	{MISENSOR_8BIT, 0x3A03, 0xA4},	/* 60HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A14, 0x04},
	{MISENSOR_8BIT, 0x3A15, 0xA4},	/* 50HZ max exposure output limit */
	{MISENSOR_8BIT, 0x4837, 0x0C},
	{MISENSOR_8BIT, 0x5306, 0x18},
	{MISENSOR_8BIT, 0x5308, 0x10},
	{MISENSOR_TOK_TERM, 0, 0}
};

/* camera: 480p, yuv422, 2lanes, mipi, 30fps */
static struct misensor_reg const ov5640_480p_init[] = {
	{MISENSOR_8BIT, 0x3C07, 0x08},
	{MISENSOR_8BIT, 0x3820, 0x41},
	{MISENSOR_8BIT, 0x3821, 0x07},	/* enable binning */
	{MISENSOR_8BIT, 0x3814, 0x31},
	{MISENSOR_8BIT, 0x3815, 0x31},	/* 2X2 binning */
	{MISENSOR_8BIT, 0x3800, 0x00},
	{MISENSOR_8BIT, 0x3801, 0x00},
	{MISENSOR_8BIT, 0x3802, 0x00},
	{MISENSOR_8BIT, 0x3803, 0x04},
	{MISENSOR_8BIT, 0x3804, 0x0A},
	{MISENSOR_8BIT, 0x3805, 0x3F},
	{MISENSOR_8BIT, 0x3806, 0x06},
	{MISENSOR_8BIT, 0x3807, 0xD9},
	{MISENSOR_8BIT, 0x3808, 0x02},
	{MISENSOR_8BIT, 0x3809, 0xD0},
	{MISENSOR_8BIT, 0x380A, 0x01},
	{MISENSOR_8BIT, 0x380B, 0xE0},
	{MISENSOR_8BIT, 0x3810, 0x00},
	{MISENSOR_8BIT, 0x3811, 0x10},
	{MISENSOR_8BIT, 0x3812, 0x00},
	{MISENSOR_8BIT, 0x3813, 0x0B},
	{MISENSOR_8BIT, 0x3618, 0x00},
	{MISENSOR_8BIT, 0x3612, 0x29},
	{MISENSOR_8BIT, 0x3708, 0x64},
	{MISENSOR_8BIT, 0x3709, 0x52},
	{MISENSOR_8BIT, 0x370C, 0x03},
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0x27},
	{MISENSOR_8BIT, 0x3A0A, 0x00},
	{MISENSOR_8BIT, 0x3A0B, 0xF6},
	{MISENSOR_8BIT, 0x3A0E, 0x03},
	{MISENSOR_8BIT, 0x3A0D, 0x04},
	{MISENSOR_8BIT, 0x4001, 0x02},
	{MISENSOR_8BIT, 0x4004, 0x02},
	{MISENSOR_8BIT, 0x3000, 0x00},
	{MISENSOR_8BIT, 0x3001, 0x08},	/* system reset 1 */
	{MISENSOR_8BIT, 0x3002, 0x1C},
	{MISENSOR_8BIT, 0x3004, 0xFF},
	{MISENSOR_8BIT, 0x3006, 0xC3},
	{MISENSOR_8BIT, 0x300E, 0x45},
	{MISENSOR_8BIT, 0x302E, 0x08},
	{MISENSOR_8BIT, 0x4713, 0x03},
	{MISENSOR_8BIT, 0x4407, 0x04},
	{MISENSOR_8BIT, 0x440E, 0x00},
	{MISENSOR_8BIT, 0x460B, 0x35},
	{MISENSOR_8BIT, 0x460C, 0x22},
	{MISENSOR_8BIT, 0x3824, 0x02},
	{MISENSOR_8BIT, 0x5001, 0xA3},	/* enable scaling */
	{MISENSOR_8BIT, 0x4005, 0x18},
	/* PLL */
	{MISENSOR_8BIT, 0x3034, 0x18},
	{MISENSOR_8BIT, 0x3035, 0x14},
	{MISENSOR_8BIT, 0x3036, 0x68},
	{MISENSOR_8BIT, 0x3037, 0x13},
	{MISENSOR_8BIT, 0x3108, 0x01},
	{MISENSOR_8BIT, 0x3824, 0x04},
	{MISENSOR_8BIT, 0x460C, 0x20},
	{MISENSOR_8BIT, 0x300E, 0x45},
	{MISENSOR_8BIT, 0x303B, 0x14},
	{MISENSOR_8BIT, 0x303C, 0x11},
	{MISENSOR_8BIT, 0x303D, 0x17},
	{MISENSOR_8BIT, 0x380C, 0x07},
	{MISENSOR_8BIT, 0x380D, 0x70},	/* total h_size is 1904 */
	{MISENSOR_8BIT, 0x380E, 0x05},
	{MISENSOR_8BIT, 0x380F, 0xB0},	/* total V-size is 1456 */
	{MISENSOR_8BIT, 0x3A02, 0x05},
	{MISENSOR_8BIT, 0x3A03, 0xAC},	/* 60HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A14, 0x05},
	{MISENSOR_8BIT, 0x3A15, 0xAC},	/* 50HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0xB4},
	{MISENSOR_8BIT, 0x3A0A, 0x01},
	{MISENSOR_8BIT, 0x3A0B, 0x6C},
	{MISENSOR_8BIT, 0x3A0E, 0x03},
	{MISENSOR_8BIT, 0x3A0D, 0x04},
	{MISENSOR_8BIT, 0x4837, 0x30},
	{MISENSOR_8BIT, 0x5306, 0x08},
	{MISENSOR_8BIT, 0x5308, 0x00},
	{MISENSOR_TOK_TERM, 0, 0}
};

/* camera: 320p, yuv422, 2lanes, mipi, 30fps */
static struct misensor_reg const ov5640_320p_init[] = {
	{MISENSOR_8BIT, 0x3C07, 0x08},
	{MISENSOR_8BIT, 0x3820, 0x41},
	{MISENSOR_8BIT, 0x3821, 0x07},	/* enable binning */
	{MISENSOR_8BIT, 0x3814, 0x31},
	{MISENSOR_8BIT, 0x3815, 0x31},	/* 2X2 binning */
	{MISENSOR_8BIT, 0x3800, 0x00},
	{MISENSOR_8BIT, 0x3801, 0x00},
	{MISENSOR_8BIT, 0x3802, 0x00},
	{MISENSOR_8BIT, 0x3803, 0x00},
	{MISENSOR_8BIT, 0x3804, 0x09},
	{MISENSOR_8BIT, 0x3805, 0xDF},
	{MISENSOR_8BIT, 0x3806, 0x06},
	{MISENSOR_8BIT, 0x3807, 0x87},
	{MISENSOR_8BIT, 0x3808, 0x01},
	{MISENSOR_8BIT, 0x3809, 0xE0},
	{MISENSOR_8BIT, 0x380A, 0x01},
	{MISENSOR_8BIT, 0x380B, 0x40},
	{MISENSOR_8BIT, 0x3810, 0x00},
	{MISENSOR_8BIT, 0x3811, 0x0B},
	{MISENSOR_8BIT, 0x3812, 0x00},
	{MISENSOR_8BIT, 0x3813, 0x04},
	{MISENSOR_8BIT, 0x3618, 0x00},
	{MISENSOR_8BIT, 0x3612, 0x29},
	{MISENSOR_8BIT, 0x3708, 0x64},
	{MISENSOR_8BIT, 0x3709, 0x52},
	{MISENSOR_8BIT, 0x370C, 0x03},
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0xB6},
	{MISENSOR_8BIT, 0x3A0A, 0x01},
	{MISENSOR_8BIT, 0x3A0B, 0x6D},
	{MISENSOR_8BIT, 0x3A0E, 0x03},
	{MISENSOR_8BIT, 0x3A0D, 0x03},
	{MISENSOR_8BIT, 0x4001, 0x02},
	{MISENSOR_8BIT, 0x4004, 0x02},
	{MISENSOR_8BIT, 0x3000, 0x00},
	{MISENSOR_8BIT, 0x3001, 0x08},	/* system reset 1 */
	{MISENSOR_8BIT, 0x3002, 0x1C},
	{MISENSOR_8BIT, 0x3004, 0xFF},
	{MISENSOR_8BIT, 0x3006, 0xC3},
	{MISENSOR_8BIT, 0x300E, 0x45},
	{MISENSOR_8BIT, 0x302E, 0x08},
	{MISENSOR_8BIT, 0x4713, 0x03},
	{MISENSOR_8BIT, 0x4407, 0x04},
	{MISENSOR_8BIT, 0x440E, 0x00},
	{MISENSOR_8BIT, 0x460B, 0x35},
	{MISENSOR_8BIT, 0x3824, 0x02},
	{MISENSOR_8BIT, 0x5001, 0xA3},	/* enable scaling */
	{MISENSOR_8BIT, 0x4005, 0x18},
	/* PLL */
	{MISENSOR_8BIT, 0x3034, 0x18},
	{MISENSOR_8BIT, 0x3035, 0x14},
	{MISENSOR_8BIT, 0x3036, 0x68},
	{MISENSOR_8BIT, 0x3037, 0x13},
	{MISENSOR_8BIT, 0x3108, 0x01},
	{MISENSOR_8BIT, 0x3824, 0x04},
	{MISENSOR_8BIT, 0x460C, 0x22},
	{MISENSOR_8BIT, 0x300E, 0x45},
	{MISENSOR_8BIT, 0x303B, 0x14},
	{MISENSOR_8BIT, 0x303C, 0x11},
	{MISENSOR_8BIT, 0x303D, 0x17},
	{MISENSOR_8BIT, 0x380C, 0x07},
	{MISENSOR_8BIT, 0x380D, 0x68},	/* total h_size is 1896 */
	{MISENSOR_8BIT, 0x380E, 0x05},
	{MISENSOR_8BIT, 0x380F, 0xB0},	/* total V-size is 1456 */
	{MISENSOR_8BIT, 0x3A02, 0x05},
	{MISENSOR_8BIT, 0x3A03, 0xAC},	/* 60HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A14, 0x05},
	{MISENSOR_8BIT, 0x3A15, 0xAC},	/* 50HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0xB6},
	{MISENSOR_8BIT, 0x3A0A, 0x01},
	{MISENSOR_8BIT, 0x3A0B, 0x6D},
	{MISENSOR_8BIT, 0x3A0E, 0x03},
	{MISENSOR_8BIT, 0x3A0D, 0x03},
	{MISENSOR_8BIT, 0x4837, 0x30},
	{MISENSOR_8BIT, 0x5306, 0x08},
	{MISENSOR_8BIT, 0x5308, 0x00},
	{MISENSOR_TOK_TERM, 0, 0}
};
/* camera: 360p, yuv422, 2lanes, mipi, 30fps */
static struct misensor_reg const ov5640_360p_init[] = {
	{MISENSOR_8BIT, 0x3C07, 0x08},
	{MISENSOR_8BIT, 0x3820, 0x40},
	{MISENSOR_8BIT, 0x3821, 0x06},
	{MISENSOR_8BIT, 0x3814, 0x11},
	{MISENSOR_8BIT, 0x3815, 0x11},
	{MISENSOR_8BIT, 0x3800, 0x01},
	{MISENSOR_8BIT, 0x3801, 0x50},	/* X start of input size */
	{MISENSOR_8BIT, 0x3802, 0x01},
	{MISENSOR_8BIT, 0x3803, 0xB2},	/* Y start of input size */
	{MISENSOR_8BIT, 0x3804, 0x08},
	{MISENSOR_8BIT, 0x3805, 0xEF},	/* X end of input size */
	{MISENSOR_8BIT, 0x3806, 0x05},
	{MISENSOR_8BIT, 0x3807, 0xF2},	/* Y end of input size */
	{MISENSOR_8BIT, 0x3808, 0x02},
	{MISENSOR_8BIT, 0x3809, 0x80},	/* DVP output H_width */
	{MISENSOR_8BIT, 0x380A, 0x01},
	{MISENSOR_8BIT, 0x380B, 0x68},	/* DVP output V_heigh */
	{MISENSOR_8BIT, 0x3810, 0x00},
	{MISENSOR_8BIT, 0x3811, 0x10},	/* X offset of pre-scaling */
	{MISENSOR_8BIT, 0x3812, 0x00},
	{MISENSOR_8BIT, 0x3813, 0x04},	/* Y offset of pre-scaling */
	{MISENSOR_8BIT, 0x3618, 0x04},
	{MISENSOR_8BIT, 0x3612, 0x2B},
	{MISENSOR_8BIT, 0x3708, 0x64},
	{MISENSOR_8BIT, 0x3709, 0x12},
	{MISENSOR_8BIT, 0x370C, 0x00},
	{MISENSOR_8BIT, 0x4001, 0x02},	/* BLC start line */
	{MISENSOR_8BIT, 0x4004, 0x02},	/* BLC line number */
	{MISENSOR_8BIT, 0x3000, 0x00},	/* system reset */
	{MISENSOR_8BIT, 0x3001, 0x08},	/* system reset 1 */
	{MISENSOR_8BIT, 0x3002, 0x00},	/* system reset */
	{MISENSOR_8BIT, 0x3004, 0xFF},	/* enable all clock */
	{MISENSOR_8BIT, 0x3006, 0xC3},	/* enable clocks */
	{MISENSOR_8BIT, 0x300E, 0x45},	/* MIPI enable, 2lane mode */
	{MISENSOR_8BIT, 0x302E, 0x08},
	{MISENSOR_8BIT, 0x4713, 0x02},	/* jepg mode select. JPEG mode 2 */
	{MISENSOR_8BIT, 0x4407, 0x0C},	/* jpeg ctrl */
	{MISENSOR_8BIT, 0x440E, 0x00},
	{MISENSOR_8BIT, 0x460B, 0x37},	/* debug mode */
	{MISENSOR_8BIT, 0x460C, 0x20},	/* VFIFO ctrl */
	{MISENSOR_8BIT, 0x4837, 0x2C},	/* PCLK PERIOD */
	{MISENSOR_8BIT, 0x3824, 0x01},	/* VFIFO */
	{MISENSOR_8BIT, 0x5001, 0xA3},	/* isp ctrl, enable downscaling */
	{MISENSOR_8BIT, 0x4005, 0x18},	/* BLC ctrl */
	/* PLL */
	{MISENSOR_8BIT, 0x3034, 0x18},
	{MISENSOR_8BIT, 0x3035, 0x11},
	{MISENSOR_8BIT, 0x3036, 0x68},
	{MISENSOR_8BIT, 0x3037, 0x13},
	{MISENSOR_8BIT, 0x3108, 0x01},
	{MISENSOR_8BIT, 0x3824, 0x01},
	{MISENSOR_8BIT, 0x460C, 0x20},
	{MISENSOR_8BIT, 0x300E, 0x45},
	{MISENSOR_8BIT, 0x303B, 0x14},
	{MISENSOR_8BIT, 0x303C, 0x11},
	{MISENSOR_8BIT, 0x303D, 0x17},
	{MISENSOR_8BIT, 0x380C, 0x09},
	{MISENSOR_8BIT, 0x380D, 0x05},	/* total H-size is 2309 */
	{MISENSOR_8BIT, 0x380E, 0x04},
	{MISENSOR_8BIT, 0x380F, 0xB0},	/* total v-size is 1200*/
	{MISENSOR_8BIT, 0x3A02, 0x04},
	{MISENSOR_8BIT, 0x3A03, 0xAC},	/* 60HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A14, 0x04},
	{MISENSOR_8BIT, 0x3A15, 0xAC},	/* 50HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0x68},
	{MISENSOR_8BIT, 0x3A0A, 0x01},
	{MISENSOR_8BIT, 0x3A0B, 0x2C},
	{MISENSOR_8BIT, 0x3A0E, 0x03},
	{MISENSOR_8BIT, 0x3A0D, 0x04},
	{MISENSOR_8BIT, 0x4837, 0x0C},
	{MISENSOR_8BIT, 0x5306, 0x18},
	{MISENSOR_8BIT, 0x5308, 0x10},
	{MISENSOR_TOK_TERM, 0, 0}
};
/* camera vga 30fps, yuv, 2lanes */
static struct misensor_reg const ov5640_vga_init[] = {
	{MISENSOR_8BIT, 0x3C07, 0x08},
	{MISENSOR_8BIT, 0x3820, 0x41},
	{MISENSOR_8BIT, 0x3821, 0x07},	/* enable binning */
	{MISENSOR_8BIT, 0x3814, 0x31},
	{MISENSOR_8BIT, 0x3815, 0x31},	/* 2X2 binning */
	{MISENSOR_8BIT, 0x3800, 0x00},
	{MISENSOR_8BIT, 0x3801, 0x00},	/* x start of isp input */
	{MISENSOR_8BIT, 0x3802, 0x00},
	{MISENSOR_8BIT, 0x3803, 0x00},	/* Y start of isp input */
	{MISENSOR_8BIT, 0x3804, 0x0A},
	{MISENSOR_8BIT, 0x3805, 0x1F},	/* X end of isp input */
	{MISENSOR_8BIT, 0x3806, 0x07},
	{MISENSOR_8BIT, 0x3807, 0x87},	/* Y end of isp input */
	{MISENSOR_8BIT, 0x3808, 0x02},
	{MISENSOR_8BIT, 0x3809, 0x80},	/* DVP output */
	{MISENSOR_8BIT, 0x380A, 0x01},
	{MISENSOR_8BIT, 0x380B, 0xE0},	/* DVP output */
	{MISENSOR_8BIT, 0x3810, 0x00},
	{MISENSOR_8BIT, 0x3811, 0x08},	/* X offset */
	{MISENSOR_8BIT, 0x3812, 0x00},
	{MISENSOR_8BIT, 0x3813, 0x02},	/* Y offset */
	{MISENSOR_8BIT, 0x3618, 0x00},
	{MISENSOR_8BIT, 0x3612, 0x29},
	{MISENSOR_8BIT, 0x3708, 0x64},
	{MISENSOR_8BIT, 0x3709, 0x52},
	{MISENSOR_8BIT, 0x370C, 0x03},
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0x27},
	{MISENSOR_8BIT, 0x3A0A, 0x00},
	{MISENSOR_8BIT, 0x3A0B, 0xF6},
	{MISENSOR_8BIT, 0x3A0E, 0x03},
	{MISENSOR_8BIT, 0x3A0D, 0x04},
	{MISENSOR_8BIT, 0x4001, 0x02},
	{MISENSOR_8BIT, 0x4004, 0x02},
	{MISENSOR_8BIT, 0x3000, 0x00},
	{MISENSOR_8BIT, 0x3001, 0x08},	/* system reset 1 */
	{MISENSOR_8BIT, 0x3002, 0x1C},
	{MISENSOR_8BIT, 0x3004, 0xFF},
	{MISENSOR_8BIT, 0x3006, 0xC3},
	{MISENSOR_8BIT, 0x300E, 0x45},
	{MISENSOR_8BIT, 0x302E, 0x08},
	{MISENSOR_8BIT, 0x4713, 0x03},
	{MISENSOR_8BIT, 0x4407, 0x04},
	{MISENSOR_8BIT, 0x440E, 0x00},
	{MISENSOR_8BIT, 0x460B, 0x35},
	{MISENSOR_8BIT, 0x460C, 0x22},
	{MISENSOR_8BIT, 0x3824, 0x02},
	{MISENSOR_8BIT, 0x5001, 0xA3},	/* enable scaling */
	{MISENSOR_8BIT, 0x4005, 0x18},
	/* PLL */
	{MISENSOR_8BIT, 0x3034, 0x18},
	{MISENSOR_8BIT, 0x3035, 0x14},
	{MISENSOR_8BIT, 0x3036, 0x68},
	{MISENSOR_8BIT, 0x3037, 0x13},
	{MISENSOR_8BIT, 0x3108, 0x01},
	{MISENSOR_8BIT, 0x3824, 0x04},
	{MISENSOR_8BIT, 0x460C, 0x20},
	{MISENSOR_8BIT, 0x300E, 0x45},
	{MISENSOR_8BIT, 0x303B, 0x14},
	{MISENSOR_8BIT, 0x303C, 0x11},
	{MISENSOR_8BIT, 0x303D, 0x17},
	{MISENSOR_8BIT, 0x380C, 0x07},
	{MISENSOR_8BIT, 0x380D, 0x70},	/* total h_size is 1904 */
	{MISENSOR_8BIT, 0x380E, 0x05},
	{MISENSOR_8BIT, 0x380F, 0xB0},	/* total V-size is 1456 */
	{MISENSOR_8BIT, 0x3A02, 0x05},
	{MISENSOR_8BIT, 0x3A03, 0xAC},	/* 60HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A14, 0x05},
	{MISENSOR_8BIT, 0x3A15, 0xAC},	/* 50HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0xB4},
	{MISENSOR_8BIT, 0x3A0A, 0x01},
	{MISENSOR_8BIT, 0x3A0B, 0x6C},
	{MISENSOR_8BIT, 0x3A0E, 0x03},
	{MISENSOR_8BIT, 0x3A0D, 0x04},
	{MISENSOR_8BIT, 0x4837, 0x30},
	{MISENSOR_8BIT, 0x5306, 0x08},
	{MISENSOR_8BIT, 0x5308, 0x00},
	{MISENSOR_TOK_TERM, 0, 0}
};

/* camera qvga 30fps, yuv, 2lanes */
static struct misensor_reg const ov5640_qvga_init[] = {
	{MISENSOR_8BIT, 0x3C07, 0x08},
	{MISENSOR_8BIT, 0x3820, 0x41},
	{MISENSOR_8BIT, 0x3821, 0x07},	/* enable binning */
	{MISENSOR_8BIT, 0x3814, 0x31},
	{MISENSOR_8BIT, 0x3815, 0x31},	/* 2X2 binning */
	{MISENSOR_8BIT, 0x3800, 0x00},
	{MISENSOR_8BIT, 0x3801, 0x00},
	{MISENSOR_8BIT, 0x3802, 0x00},
	{MISENSOR_8BIT, 0x3803, 0x00},
	{MISENSOR_8BIT, 0x3804, 0x0A},
	{MISENSOR_8BIT, 0x3805, 0x1F},
	{MISENSOR_8BIT, 0x3806, 0x07},
	{MISENSOR_8BIT, 0x3807, 0x87},
	{MISENSOR_8BIT, 0x3808, 0x01},
	{MISENSOR_8BIT, 0x3809, 0x40},
	{MISENSOR_8BIT, 0x380A, 0x00},
	{MISENSOR_8BIT, 0x380B, 0xF0},
	{MISENSOR_8BIT, 0x3810, 0x00},
	{MISENSOR_8BIT, 0x3811, 0x08},
	{MISENSOR_8BIT, 0x3812, 0x00},
	{MISENSOR_8BIT, 0x3813, 0x02},
	{MISENSOR_8BIT, 0x3618, 0x00},
	{MISENSOR_8BIT, 0x3612, 0x29},
	{MISENSOR_8BIT, 0x3708, 0x64},
	{MISENSOR_8BIT, 0x3709, 0x52},
	{MISENSOR_8BIT, 0x370C, 0x03},
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0x27},
	{MISENSOR_8BIT, 0x3A0A, 0x00},
	{MISENSOR_8BIT, 0x3A0B, 0xF6},
	{MISENSOR_8BIT, 0x3A0E, 0x03},
	{MISENSOR_8BIT, 0x3A0D, 0x04},
	{MISENSOR_8BIT, 0x4001, 0x02},
	{MISENSOR_8BIT, 0x4004, 0x02},
	{MISENSOR_8BIT, 0x3000, 0x00},
	{MISENSOR_8BIT, 0x3001, 0x08},	/* system reset 1 */
	{MISENSOR_8BIT, 0x3002, 0x1C},
	{MISENSOR_8BIT, 0x3004, 0xFF},
	{MISENSOR_8BIT, 0x3006, 0xC3},
	{MISENSOR_8BIT, 0x300E, 0x45},
	{MISENSOR_8BIT, 0x302E, 0x08},
	{MISENSOR_8BIT, 0x4713, 0x03},
	{MISENSOR_8BIT, 0x4407, 0x04},
	{MISENSOR_8BIT, 0x440E, 0x00},
	{MISENSOR_8BIT, 0x460B, 0x35},
	{MISENSOR_8BIT, 0x460C, 0x22},
	{MISENSOR_8BIT, 0x3824, 0x02},
	{MISENSOR_8BIT, 0x5001, 0xA3},	/* enable scaling */
	{MISENSOR_8BIT, 0x4005, 0x18},
	/* PLL */
	{MISENSOR_8BIT, 0x3034, 0x18},
	{MISENSOR_8BIT, 0x3035, 0x14},
	{MISENSOR_8BIT, 0x3036, 0x68},
	{MISENSOR_8BIT, 0x3037, 0x13},
	{MISENSOR_8BIT, 0x3108, 0x01},
	{MISENSOR_8BIT, 0x3824, 0x04},
	{MISENSOR_8BIT, 0x460C, 0x22},
	{MISENSOR_8BIT, 0x300E, 0x45},
	{MISENSOR_8BIT, 0x303B, 0x14},
	{MISENSOR_8BIT, 0x303C, 0x11},
	{MISENSOR_8BIT, 0x303D, 0x17},
	{MISENSOR_8BIT, 0x380C, 0x07},
	{MISENSOR_8BIT, 0x380D, 0x68},	/* total h_size is 1896 */
	{MISENSOR_8BIT, 0x380E, 0x05},
	{MISENSOR_8BIT, 0x380F, 0xB0},	/* total V-size is 1456 */
	{MISENSOR_8BIT, 0x3A02, 0x05},
	{MISENSOR_8BIT, 0x3A03, 0xAC},	/* 60HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A14, 0x05},
	{MISENSOR_8BIT, 0x3A15, 0xAC},	/* 50HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0xB6},
	{MISENSOR_8BIT, 0x3A0A, 0x01},
	{MISENSOR_8BIT, 0x3A0B, 0x6D},
	{MISENSOR_8BIT, 0x3A0E, 0x03},
	{MISENSOR_8BIT, 0x3A0D, 0x03},
	{MISENSOR_8BIT, 0x4837, 0x30},
	{MISENSOR_8BIT, 0x5306, 0x08},
	{MISENSOR_8BIT, 0x5308, 0x00},
	{MISENSOR_TOK_TERM, 0, 0}
};

/* camera dvga 30fps, yuv, 2lanes */
static struct misensor_reg const ov5640_dvga_init[] = {
	{MISENSOR_8BIT, 0x3C07, 0x08},
	{MISENSOR_8BIT, 0x3820, 0x41},
	{MISENSOR_8BIT, 0x3821, 0x07},	/* enable binning */
	{MISENSOR_8BIT, 0x3814, 0x31},
	{MISENSOR_8BIT, 0x3815, 0x31},	/* 2X2 binning */
	{MISENSOR_8BIT, 0x3800, 0x00},
	{MISENSOR_8BIT, 0x3801, 0x00},
	{MISENSOR_8BIT, 0x3802, 0x00},
	{MISENSOR_8BIT, 0x3803, 0x00},
	{MISENSOR_8BIT, 0x3804, 0x0A},
	{MISENSOR_8BIT, 0x3805, 0x1F},
	{MISENSOR_8BIT, 0x3806, 0x07},
	{MISENSOR_8BIT, 0x3807, 0x87},
	{MISENSOR_8BIT, 0x3808, 0x01},
	{MISENSOR_8BIT, 0x3809, 0xA0},	/* DVP output, value is 416 */
	{MISENSOR_8BIT, 0x380A, 0x01},
	{MISENSOR_8BIT, 0x380B, 0x38},	/* DVP output, value is 312 */
	{MISENSOR_8BIT, 0x3810, 0x00},
	{MISENSOR_8BIT, 0x3811, 0x08},
	{MISENSOR_8BIT, 0x3812, 0x00},
	{MISENSOR_8BIT, 0x3813, 0x02},
	{MISENSOR_8BIT, 0x3618, 0x00},
	{MISENSOR_8BIT, 0x3612, 0x29},
	{MISENSOR_8BIT, 0x3708, 0x64},
	{MISENSOR_8BIT, 0x3709, 0x52},
	{MISENSOR_8BIT, 0x370C, 0x03},
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0x27},
	{MISENSOR_8BIT, 0x3A0A, 0x00},
	{MISENSOR_8BIT, 0x3A0B, 0xF6},
	{MISENSOR_8BIT, 0x3A0E, 0x03},
	{MISENSOR_8BIT, 0x3A0D, 0x04},
	{MISENSOR_8BIT, 0x4001, 0x02},
	{MISENSOR_8BIT, 0x4004, 0x02},
	{MISENSOR_8BIT, 0x3000, 0x00},
	{MISENSOR_8BIT, 0x3001, 0x08},	/* system reset 1 */
	{MISENSOR_8BIT, 0x3002, 0x1C},
	{MISENSOR_8BIT, 0x3004, 0xFF},
	{MISENSOR_8BIT, 0x3006, 0xC3},
	{MISENSOR_8BIT, 0x300E, 0x45},
	{MISENSOR_8BIT, 0x302E, 0x08},
	{MISENSOR_8BIT, 0x4713, 0x03},
	{MISENSOR_8BIT, 0x4407, 0x04},
	{MISENSOR_8BIT, 0x440E, 0x00},
	{MISENSOR_8BIT, 0x460B, 0x35},
	{MISENSOR_8BIT, 0x460C, 0x22},
	{MISENSOR_8BIT, 0x3824, 0x02},
	{MISENSOR_8BIT, 0x5001, 0xA3},	/* enable scaling */
	{MISENSOR_8BIT, 0x4005, 0x18},
	/* PLL */
	{MISENSOR_8BIT, 0x3034, 0x18},
	{MISENSOR_8BIT, 0x3035, 0x14},
	{MISENSOR_8BIT, 0x3036, 0x68},
	{MISENSOR_8BIT, 0x3037, 0x13},
	{MISENSOR_8BIT, 0x3108, 0x01},
	{MISENSOR_8BIT, 0x3824, 0x04},
	{MISENSOR_8BIT, 0x460C, 0x20},
	{MISENSOR_8BIT, 0x300E, 0x45},
	{MISENSOR_8BIT, 0x303B, 0x14},
	{MISENSOR_8BIT, 0x303C, 0x11},
	{MISENSOR_8BIT, 0x303D, 0x17},
	{MISENSOR_8BIT, 0x380C, 0x07},
	{MISENSOR_8BIT, 0x380D, 0x70},	/* total h_size is 1904 */
	{MISENSOR_8BIT, 0x380E, 0x05},
	{MISENSOR_8BIT, 0x380F, 0xB0},	/* total V-size is 1456 */
	{MISENSOR_8BIT, 0x3A02, 0x05},
	{MISENSOR_8BIT, 0x3A03, 0xAC},	/* 60HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A14, 0x05},
	{MISENSOR_8BIT, 0x3A15, 0xAC},	/* 50HZ max exposure output limit */
	{MISENSOR_8BIT, 0x3A08, 0x01},
	{MISENSOR_8BIT, 0x3A09, 0xB4},
	{MISENSOR_8BIT, 0x3A0A, 0x01},
	{MISENSOR_8BIT, 0x3A0B, 0x6C},
	{MISENSOR_8BIT, 0x3A0E, 0x03},
	{MISENSOR_8BIT, 0x3A0D, 0x04},
	{MISENSOR_8BIT, 0x4837, 0x31},
	{MISENSOR_8BIT, 0x5306, 0x08},
	{MISENSOR_8BIT, 0x5308, 0x00},
	{MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const ov5640_common[] = {
	 {MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const ov5640_iq[] = {
	{MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const ov5640_init[] = {
	/* init software */
	{MISENSOR_8BIT, 0x3103, 0x11},
	{MISENSOR_TOK_DELAY, {0}, 5},
	{MISENSOR_8BIT, 0x3008, 0x82},
	{MISENSOR_TOK_DELAY, {0}, 5},
	{MISENSOR_8BIT, 0x3008, 0x42},	/* software power down */
	{MISENSOR_8BIT, 0x3103, 0x03},	/* SCCB system control */
	/* set Frex Vsync href PCLK D[9:6} input */
	{MISENSOR_8BIT, 0x3017, 0x00},
	{MISENSOR_8BIT, 0x3018, 0x00},	/* set d[5:0] GPIO[1:0] input */
	{MISENSOR_8BIT, 0x3034, 0x18},	/* MIPI 8-bit mode*/
	{MISENSOR_8BIT, 0x3037, 0x13},	/* PLL */
	{MISENSOR_8BIT, 0x3108, 0x01},	/* system divider */
	{MISENSOR_8BIT, 0x3630, 0x36},
	{MISENSOR_8BIT, 0x3631, 0x0E},
	{MISENSOR_8BIT, 0x3632, 0xE2},
	{MISENSOR_8BIT, 0x3633, 0x12},
	{MISENSOR_8BIT, 0x3621, 0xE0},
	{MISENSOR_8BIT, 0x3704, 0xA0},
	{MISENSOR_8BIT, 0x3703, 0x5A},
	{MISENSOR_8BIT, 0x3715, 0x78},
	{MISENSOR_8BIT, 0x3717, 0x01},
	{MISENSOR_8BIT, 0x370B, 0x60},
	{MISENSOR_8BIT, 0x3705, 0x1A},
	{MISENSOR_8BIT, 0x3905, 0x02},
	{MISENSOR_8BIT, 0x3906, 0x10},
	{MISENSOR_8BIT, 0x3901, 0x0A},
	{MISENSOR_8BIT, 0x3731, 0x12},
	{MISENSOR_8BIT, 0x3600, 0x08},	/* VCM debug mode */
	{MISENSOR_8BIT, 0x3601, 0x33},	/* VCM debug mode */
	{MISENSOR_8BIT, 0x302D, 0x60},	/* system control */
	{MISENSOR_8BIT, 0x3620, 0x52},
	{MISENSOR_8BIT, 0x371B, 0x20},
	{MISENSOR_8BIT, 0x471C, 0x50},
	{MISENSOR_8BIT, 0x3A13, 0x43},	/* AGC pre-gain 40 = 1x */
	{MISENSOR_8BIT, 0x3A18, 0x00},	/* gain ceiling */
	{MISENSOR_8BIT, 0x3A19, 0xF8},	/* gain ceiling */
	{MISENSOR_8BIT, 0x3635, 0x13},
	{MISENSOR_8BIT, 0x3636, 0x03},
	{MISENSOR_8BIT, 0x3634, 0x40},
	{MISENSOR_8BIT, 0x3622, 0x01},
	{MISENSOR_8BIT, 0x3C00, 0x04},	/* 50Hz/60Hz */
	{MISENSOR_8BIT, 0x3C01, 0xB4},	/* 50/60Hz */
	{MISENSOR_8BIT, 0x3C04, 0x28},	/* threshold for low sum */
	{MISENSOR_8BIT, 0x3C05, 0x98},	/* threshold for high sum */
	{MISENSOR_8BIT, 0x3C06, 0x00},	/* light meter 1 threshold high */
	{MISENSOR_8BIT, 0x3C08, 0x00},	/* light meter 2 threshold high */
	{MISENSOR_8BIT, 0x3C09, 0x1C},	/* light meter 2 threshold low */
	{MISENSOR_8BIT, 0x3C0A, 0x9C},	/* sample number high */
	{MISENSOR_8BIT, 0x3C0B, 0x40},	/* sample number low */
	/* timing */
	{MISENSOR_8BIT, 0x3800, 0x00},	/* HS */
	{MISENSOR_8BIT, 0x3801, 0x00},	/* HS */
	{MISENSOR_8BIT, 0x3802, 0x00},	/* VS */
	{MISENSOR_8BIT, 0x3804, 0x0A},	/* HW */
	{MISENSOR_8BIT, 0x3805, 0x3F},	/* HW */
	{MISENSOR_8BIT, 0x3810, 0x00},	/* H offset high */
	{MISENSOR_8BIT, 0x3811, 0x10},	/* H offset low */
	{MISENSOR_8BIT, 0x3812, 0x00},	/* V offset high */
	{MISENSOR_8BIT, 0x3708, 0x64},
	{MISENSOR_8BIT, 0x3A08, 0x01},	/* B50 */
	{MISENSOR_8BIT, 0x4001, 0x02},	/* BLC start line */
	{MISENSOR_8BIT, 0x4005, 0x1A},	/* BLC always update */
	{MISENSOR_8BIT, 0x3000, 0x00},	/* system reset 0 */
	{MISENSOR_8BIT, 0x3001, 0x08},	/* system reset 1 */
	{MISENSOR_8BIT, 0x3002, 0x1C},	/* system reset 2 */
	{MISENSOR_8BIT, 0x3004, 0xFF},	/* clock enable 00 */
	{MISENSOR_8BIT, 0x3006, 0xC3},	/* clock enable 2 */
	{MISENSOR_8BIT, 0x300E, 0x45},	/* MIPI control 2 lane MIPI on */
	{MISENSOR_8BIT, 0x302E, 0x08},
	{MISENSOR_8BIT, 0x4300, 0x32},	/* YUV 422 UYVY */
	{MISENSOR_8BIT, 0x501F, 0x00},	/* ISP YUV 422 */
	{MISENSOR_8BIT, 0x4407, 0x04},	/* JPEG QS */
	{MISENSOR_8BIT, 0x440E, 0x00},
	/* ISP control LENC on GAMMA on BPC on WPC on CIP on */
	{MISENSOR_8BIT, 0x5000, 0xA7},
	/* AWB */
	{MISENSOR_8BIT, 0x5180, 0xFF},
	{MISENSOR_8BIT, 0x5181, 0xF2},
	{MISENSOR_8BIT, 0x5182, 0x00},
	{MISENSOR_8BIT, 0x5183, 0x14},
	{MISENSOR_8BIT, 0x5184, 0x25},
	{MISENSOR_8BIT, 0x5185, 0x24},
	{MISENSOR_8BIT, 0x5189, 0x8D},
	{MISENSOR_8BIT, 0x518A, 0x61},
	{MISENSOR_8BIT, 0x518C, 0x94},
	{MISENSOR_8BIT, 0x518B, 0xAF},
	{MISENSOR_8BIT, 0x5187, 0x17},
	{MISENSOR_8BIT, 0x5188, 0x0F},
	{MISENSOR_8BIT, 0x518D, 0x41},
	{MISENSOR_8BIT, 0x518F, 0x75},
	{MISENSOR_8BIT, 0x518E, 0x34},
	{MISENSOR_8BIT, 0x5190, 0x43},
	{MISENSOR_8BIT, 0x5191, 0xF5},
	{MISENSOR_8BIT, 0x5192, 0x0A},
	{MISENSOR_8BIT, 0x5186, 0x16},
	{MISENSOR_8BIT, 0x5193, 0x70},
	{MISENSOR_8BIT, 0x5194, 0xF0},
	{MISENSOR_8BIT, 0x5195, 0xF0},
	{MISENSOR_8BIT, 0x5196, 0x03},
	{MISENSOR_8BIT, 0x5197, 0x01},
	{MISENSOR_8BIT, 0x5198, 0x05},
	{MISENSOR_8BIT, 0x5199, 0xDB},
	{MISENSOR_8BIT, 0x519A, 0x04},
	{MISENSOR_8BIT, 0x519B, 0x00},
	{MISENSOR_8BIT, 0x519C, 0x08},
	{MISENSOR_8BIT, 0x519D, 0x20},
	{MISENSOR_8BIT, 0x519E, 0x38},
	/* color matrix */
	{MISENSOR_8BIT, 0x5381, 0x1F},
	{MISENSOR_8BIT, 0x5382, 0x5C},
	{MISENSOR_8BIT, 0x5383, 0x05},
	{MISENSOR_8BIT, 0x5384, 0x03},
	{MISENSOR_8BIT, 0x5385, 0x6C},
	{MISENSOR_8BIT, 0x5386, 0x6F},
	{MISENSOR_8BIT, 0x5387, 0x6E},
	{MISENSOR_8BIT, 0x5388, 0x62},
	{MISENSOR_8BIT, 0x5389, 0x0C},
	{MISENSOR_8BIT, 0x538A, 0x01},
	{MISENSOR_8BIT, 0x538B, 0x98},
	/* CIP */
	{MISENSOR_8BIT, 0x5300, 0x08},	/* sharpen MT th1 */
	{MISENSOR_8BIT, 0x5301, 0x30},	/* sharpen MT th2 */
	{MISENSOR_8BIT, 0x5302, 0x18},	/* sharpen MT offset 1 */
	{MISENSOR_8BIT, 0x5303, 0x0E},	/* sharpen MT offset 2 */
	{MISENSOR_8BIT, 0x5304, 0x08},	/* DNS threshold 1 */
	{MISENSOR_8BIT, 0x5305, 0x30},	/* DNS threshold 2 */
	{MISENSOR_8BIT, 0x5306, 0x08},	/* DNS offset 1 */
	{MISENSOR_8BIT, 0x5307, 0x16},	/* DNS offset 2 */
	{MISENSOR_8BIT, 0x5308, 0x00},	/* auto de-noise */
	{MISENSOR_8BIT, 0x5309, 0x08},	/* sharpen TH th1 */
	{MISENSOR_8BIT, 0x530A, 0x30},	/* sharpen TH th2 */
	{MISENSOR_8BIT, 0x530B, 0x04},	/* sharpen TH offset 1 */
	{MISENSOR_8BIT, 0x530C, 0x06},	/* sharpen TH offset 2 */
	/* gamma */
	{MISENSOR_8BIT, 0x5480, 0x01},
	{MISENSOR_8BIT, 0x5481, 0x08},
	{MISENSOR_8BIT, 0x5482, 0x14},
	{MISENSOR_8BIT, 0x5483, 0x28},
	{MISENSOR_8BIT, 0x5484, 0x51},
	{MISENSOR_8BIT, 0x5485, 0x65},
	{MISENSOR_8BIT, 0x5486, 0x71},
	{MISENSOR_8BIT, 0x5487, 0x7D},
	{MISENSOR_8BIT, 0x5488, 0x87},
	{MISENSOR_8BIT, 0x5489, 0x91},
	{MISENSOR_8BIT, 0x548A, 0x9A},
	{MISENSOR_8BIT, 0x548B, 0xAA},
	{MISENSOR_8BIT, 0x548C, 0xB8},
	{MISENSOR_8BIT, 0x548D, 0xCD},
	{MISENSOR_8BIT, 0x548E, 0xDD},
	{MISENSOR_8BIT, 0x548F, 0xEA},
	{MISENSOR_8BIT, 0x5490, 0x1D},
	/* UV adjust */
	{MISENSOR_8BIT, 0x5580, 0x06},	/* sat on contrast on */
	{MISENSOR_8BIT, 0x5583, 0x40},	/* sat U */
	{MISENSOR_8BIT, 0x5584, 0x10},	/* sat VV */
	{MISENSOR_8BIT, 0x5589, 0x10},	/* UV adjust th1 */
	{MISENSOR_8BIT, 0x558A, 0x00},	/* UV adjust th2[8] */
	{MISENSOR_8BIT, 0x558B, 0xF8},	/* UV adjust th2[7:0] */
	{MISENSOR_8BIT, 0x501D, 0x40},	/* enable manual offset of contrast */
	/* lens correction */
	{MISENSOR_8BIT, 0x5800, 0x3D},
	{MISENSOR_8BIT, 0x5801, 0x1E},
	{MISENSOR_8BIT, 0x5802, 0x15},
	{MISENSOR_8BIT, 0x5803, 0x17},
	{MISENSOR_8BIT, 0x5804, 0x1E},
	{MISENSOR_8BIT, 0x5805, 0x3F},
	{MISENSOR_8BIT, 0x5806, 0x10},
	{MISENSOR_8BIT, 0x5807, 0x0A},
	{MISENSOR_8BIT, 0x5808, 0x07},
	{MISENSOR_8BIT, 0x5809, 0x07},
	{MISENSOR_8BIT, 0x580A, 0x0B},
	{MISENSOR_8BIT, 0x580B, 0x13},
	{MISENSOR_8BIT, 0x580C, 0x0A},
	{MISENSOR_8BIT, 0x580D, 0x04},
	{MISENSOR_8BIT, 0x580E, 0x00},
	{MISENSOR_8BIT, 0x580F, 0x00},
	{MISENSOR_8BIT, 0x5810, 0x04},
	{MISENSOR_8BIT, 0x5811, 0x0C},
	{MISENSOR_8BIT, 0x5812, 0x0A},
	{MISENSOR_8BIT, 0x5813, 0x04},
	{MISENSOR_8BIT, 0x5814, 0x00},
	{MISENSOR_8BIT, 0x5815, 0x00},
	{MISENSOR_8BIT, 0x5816, 0x04},
	{MISENSOR_8BIT, 0x5817, 0x0C},
	{MISENSOR_8BIT, 0x5818, 0x10},
	{MISENSOR_8BIT, 0x5819, 0x0B},
	{MISENSOR_8BIT, 0x581A, 0x07},
	{MISENSOR_8BIT, 0x581B, 0x07},
	{MISENSOR_8BIT, 0x581C, 0x0A},
	{MISENSOR_8BIT, 0x581D, 0x14},
	{MISENSOR_8BIT, 0x581E, 0x37},
	{MISENSOR_8BIT, 0x581F, 0x1F},
	{MISENSOR_8BIT, 0x5820, 0x18},
	{MISENSOR_8BIT, 0x5821, 0x18},
	{MISENSOR_8BIT, 0x5822, 0x1F},
	{MISENSOR_8BIT, 0x5823, 0x2F},
	{MISENSOR_8BIT, 0x5824, 0x48},
	{MISENSOR_8BIT, 0x5825, 0x2A},
	{MISENSOR_8BIT, 0x5826, 0x2C},
	{MISENSOR_8BIT, 0x5827, 0x08},
	{MISENSOR_8BIT, 0x5828, 0x66},
	{MISENSOR_8BIT, 0x5829, 0x0A},
	{MISENSOR_8BIT, 0x582A, 0x26},
	{MISENSOR_8BIT, 0x582B, 0x24},
	{MISENSOR_8BIT, 0x582C, 0x26},
	{MISENSOR_8BIT, 0x582D, 0x08},
	{MISENSOR_8BIT, 0x582E, 0x08},
	{MISENSOR_8BIT, 0x582F, 0x42},
	{MISENSOR_8BIT, 0x5830, 0x40},
	{MISENSOR_8BIT, 0x5831, 0x22},
	{MISENSOR_8BIT, 0x5832, 0x06},
	{MISENSOR_8BIT, 0x5833, 0x0A},
	{MISENSOR_8BIT, 0x5834, 0x24},
	{MISENSOR_8BIT, 0x5835, 0x24},
	{MISENSOR_8BIT, 0x5836, 0x26},
	{MISENSOR_8BIT, 0x5837, 0x06},
	{MISENSOR_8BIT, 0x5838, 0x48},
	{MISENSOR_8BIT, 0x5839, 0x08},
	{MISENSOR_8BIT, 0x583A, 0x28},
	{MISENSOR_8BIT, 0x583B, 0x06},
	{MISENSOR_8BIT, 0x583C, 0x4A},
	{MISENSOR_8BIT, 0x583D, 0xCE},
	/* AE */
	{MISENSOR_8BIT, 0x5025, 0x00},
	{MISENSOR_8BIT, 0x3A0F, 0x30},	/* stable in high */
	{MISENSOR_8BIT, 0x3A10, 0x28},	/* stable in low */
	{MISENSOR_8BIT, 0x3A1B, 0x30},	/* stable out high */
	{MISENSOR_8BIT, 0x3A1E, 0x26},	/* stable out low */
	{MISENSOR_8BIT, 0x3A11, 0x60},	/* fast zone high */
	{MISENSOR_8BIT, 0x3A1F, 0x14},	/* fast zone low */
	{MISENSOR_8BIT, 0x350A, 0x00},
	{MISENSOR_8BIT, 0x350B, 0x32},	/* default gain 50 */
	{MISENSOR_8BIT, 0x3500, 0x00},
	{MISENSOR_8BIT, 0x3501, 0x03},
	{MISENSOR_8BIT, 0x3502, 0xE8},	/* default shutter 1000 */
	/* BLC */
	{MISENSOR_8BIT, 0x4000, 0x89},
	{MISENSOR_8BIT, 0x4001, 0x02},	/* BLC start line */
	{MISENSOR_8BIT, 0x4002, 0x45},
	{MISENSOR_8BIT, 0x4003, 0x08},
	{MISENSOR_8BIT, 0x4005, 0x18},
	{MISENSOR_8BIT, 0x4009, 0x10},
	{MISENSOR_8BIT, 0x4202, 0x00},	/* stream on */
	{MISENSOR_8BIT, 0x4202, 0x0F},	/* stream off */
	{MISENSOR_8BIT, 0x3008, 0x02},	/* wake up */
	{MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const ov5640_focus_init[] = {
	{MISENSOR_8BIT, 0x3022, 0x00},
	{MISENSOR_8BIT, 0x3023, 0x00},
	{MISENSOR_8BIT, 0x3024, 0x00},
	{MISENSOR_8BIT, 0x3025, 0x00},
	{MISENSOR_8BIT, 0x3026, 0x00},
	{MISENSOR_8BIT, 0x3027, 0x00},
	{MISENSOR_8BIT, 0x3028, 0x00},
	{MISENSOR_8BIT, 0x3029, 0x7F},
	{MISENSOR_8BIT, 0x3000, 0x00},
	{MISENSOR_TOK_TERM, 0, 0}
};
#endif
