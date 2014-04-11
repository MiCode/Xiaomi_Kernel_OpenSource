/*
 * Support for mt9d113 Camera Sensor.
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

#ifndef __MT9D113_H__
#define __MT9D113_H__

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

#define V4L2_IDENT_MT9D113 0x2580

#define MT9D113_FOCAL_LENGTH_NUM	439	/*4.39mm*/
#define MT9D113_FOCAL_LENGTH_DEM	100
#define MT9D113_F_NUMBER_DEFAULT_NUM	24
#define MT9D113_F_NUMBER_DEM	10
/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define MT9D113_FOCAL_LENGTH_DEFAULT 0xD00064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define MT9D113_F_NUMBER_DEFAULT 0x18000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define MT9D113_F_NUMBER_RANGE 0x180a180a

#define MT9D113_COL_EFF_MASK	0x7
#define MT9D113_COL_EFF_DISABLE	0x0
#define MT9D113_COL_EFF_MONO	0x1
#define MT9D113_COL_EFF_SEPIA	0x2
#define MT9D113_COL_EFF_NEG	0x3

#define MT9D113_BIT_FD_MANUAL	7
#define MT9D113_MASK_FD_MANUAL	(1 << MT9D113_BIT_FD_MANUAL)
enum {
	MT9D113_FD_MANUAL_DIS,
	MT9D113_FD_MANUAL_EN,
};
#define MT9D113_BIT_FD_SET	6
#define MT9D113_MASK_FD_SET	(1 << MT9D113_BIT_FD_SET)
enum {
	MT9D113_FD_60HZ,
	MT9D113_FD_50HZ,
};

#define MT9D113_REG_CHIPID	0x0
#define MT9D113_REG_PLL_DIV	0x0010
#define MT9D113_REG_PLL_P	0x0012
#define MT9D113_REG_PLL_CTRL	0x0014
#define MT9D113_REG_STBY_CTRL	0x0018
#define MT9D113_REG_MISC_CTRL	0x001a
#define MT9D113_REG_STBY_MODE	0x0028

#define MT9D113_REG_MIPI_CTRL	0x3400
#define MT9D113_REG_MIPI_STAT	0x3402

#define MT9D113_STBY_MODE_1	0x0

#define MIPI_STAT_BIT_MIPI_STBY	0
#define MIPI_STAT_MASK_MIPI_STBY	(1 << MIPI_STAT_BIT_MIPI_STBY)

#define MIPI_CTRL_BIT_MIPI_STBY	1
#define MIPI_CTRL_MASK_MIPI_STBY	(1 << MIPI_CTRL_BIT_MIPI_STBY)

#define MIPI_CTRL_BIT_MIPI_EOF	4
#define MIPI_CTRL_MASK_MIPI_EOF	(1 << MIPI_CTRL_BIT_MIPI_EOF)

#define STBY_CTRL_BIT_STBY_STAT	14
#define STBY_CTRL_MASK_STBY_STAT	(1 << STBY_CTRL_BIT_STBY_STAT)

#define STBY_CTRL_BIT_STBY_REQ	0
#define STBY_CTRL_MASK_STBY_REQ	(1 << STBY_CTRL_BIT_STBY_REQ)

#define PLL_CTRL_BIT_PLL_STAT	15
#define PLL_CTRL_MASK_PLL_STAT	(1 << PLL_CTRL_BIT_PLL_STAT)

#define MISC_CTRL_BIT_EN_PARALL	9
#define MISC_CTRL_MASK_EN_PARALL	(1 << MISC_CTRL_BIT_EN_PARALL)

#define MISC_CTRL_BIT_EN_MIPI_TX	3
#define MISC_CTRL_MASK_EN_MIPI_TX	(1 << MISC_CTRL_BIT_EN_MIPI_TX)


#define STBY_CTRL_BIT_STOP_MCU	2
#define STBY_CTRL_MASK_STOP_MCU	(1 << STBY_CTRL_BIT_STOP_MCU)

#define MT9D113_MCU_VAR_ADDR	0x098c
#define MT9D113_MCU_VAR_DATA0	0x0990
#define MT9D113_MCU_VAR_DATA1	0x0992
#define MT9D113_MCU_VAR_DATA2	0x0994
#define MT9D113_MCU_VAR_DATA3	0x0996
#define MT9D113_MCU_VAR_DATA4	0x0998
#define MT9D113_MCU_VAR_DATA5	0x099a
#define MT9D113_MCU_VAR_DATA6	0x099c
#define MT9D113_MCU_VAR_DATA7	0x099e

#define MT9D113_VAR_MON_ID_0	0xa024
#define MT9D113_VAR_SEQ_CMD	0xa103
#define SEQ_CMD_RUN		0x0
#define SEQ_CMD_REFRESH_MODE	0x0006
#define SEQ_CMD_REFRESH		0x0005
#define MT9D113_VAR_SEQ_STATE	0xa104

#define MT9D113_VAR_COL_EFF_A	0x2759
#define MT9D113_VAR_FD_MODE	0x2404

/* current integration time access */
#define MT9D113_VAR_INTEGRATION_TIME	0x2222

#define MT9D113_VAR_AE_MAX_INDEX	0xa20c
#define MT9D113_AE_MAX_INDEX_0	0x0003
#define MT9D113_AE_MAX_INDEX_1	0x000b

#define MT9D113_VAR_AE_GAIN	0xa21c
#define MT9D113_VAR_AE_D_GAIN	0x221f

/* #defines for register writes and register array processing */
#define MISENSOR_8BIT		1
#define MISENSOR_16BIT		2
#define MISENSOR_32BIT		4

#define MISENSOR_FWBURST0	0x80
#define MISENSOR_FWBURST1	0x81
#define MISENSOR_FWBURST4	0x84
#define MISENSOR_FWBURST	0x88

#define MISENSOR_TOK_TERM	0xf000	/* terminating token for reg list */
#define MISENSOR_TOK_DELAY	0xfe00	/* delay token for reg list */
#define MISENSOR_TOK_FWLOAD	0xfd00	/* token indicating load FW */
#define MISENSOR_TOK_POLL	0xfc00	/* token indicating poll instruction */
#define MISENSOR_TOK_RMW	0x0010  /* RMW operation */
#define MISENSOR_TOK_MASK	0xfff0
#define MISENSOR_AWB_STEADY	(1<<0)	/* awb steady */
#define MISENSOR_AE_READY	(1<<3)	/* ae status ready */

#define I2C_RETRY_COUNT		5
#define MSG_LEN_OFFSET		2

/* Resolution Table */
enum {
	MT9D113_RES_QCIF,
	MT9D113_RES_QVGA,
	MT9D113_RES_CIF,
	MT9D113_RES_VGA_WIDE,
	MT9D113_RES_VGA,
	MT9D113_RES_480P,
	MT9D113_RES_SVGA,
	MT9D113_RES_720P,
	MT9D113_RES_1M,
	MT9D113_RES_2M,
};

#define MT9D113_RES_2M_SIZE_H		1600
#define MT9D113_RES_2M_SIZE_V		1200
#define MT9D113_RES_1M_SIZE_H		1024
#define MT9D113_RES_1M_SIZE_V		768
#define MT9D113_RES_720P_SIZE_H		1280
#define MT9D113_RES_720P_SIZE_V		720
#define MT9D113_RES_SVGA_SIZE_H		800
#define MT9D113_RES_SVGA_SIZE_V		600
#define MT9D113_RES_480P_SIZE_H		768
#define MT9D113_RES_480P_SIZE_V		480
#define MT9D113_RES_VGA_SIZE_H		640
#define MT9D113_RES_VGA_SIZE_V		480
#define MT9D113_RES_VGA_WIDE_SIZE_V		360
#define MT9D113_RES_CIF_SIZE_H		352
#define MT9D113_RES_CIF_SIZE_V		288
#define MT9D113_RES_QVGA_SIZE_H		320
#define MT9D113_RES_QVGA_SIZE_V		240
#define MT9D113_RES_QCIF_SIZE_H		176
#define MT9D113_RES_QCIF_SIZE_V		144

/* completion status polling requirements, usage based on Aptina .INI Rev2 */
enum poll_reg {
	NO_POLLING,
	PRE_POLLING,
	POST_POLLING,
};
/*
 * struct misensor_reg - MI sensor  register format
 * @length: length of the register
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 * Define a structure for sensor register initialization values
 */
struct misensor_reg {
	u32 length;
	u32 reg;
	u32 val;	/* value or for read/mod/write, AND mask */
	u32 val2;	/* optional; for rmw, OR mask */
};

struct regval_list {
	u16 reg_num;
	u8 value;
};

struct mt9d113_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;

	struct camera_sensor_platform_data *platform_data;
	int real_model_id;
	unsigned int res;
	int color_effect;
	int run_mode;
	int last_run_mode;
};

struct mt9d113_format_struct {
	u8 *desc;
	u32 pixelformat;
	struct regval_list *regs;
};

struct mt9d113_res_struct {
	u8 *desc;
	int res;
	int width;
	int height;
	int fps;
	int skip_frames;
	int row_time;
	bool used;
	struct regval_list *regs;
};

struct mt9d113_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, int value);
};

#define MT9D113_MAX_WRITE_BUF_SIZE	32
struct mt9d113_write_buffer {
	u16 addr;
	u8 data[MT9D113_MAX_WRITE_BUF_SIZE];
};

struct mt9d113_write_ctrl {
	int index;
	struct mt9d113_write_buffer buffer;
};

/*
 * Modes supported by the mt9d113 driver.
 * Please, keep them in ascending order.
 */
static struct mt9d113_res_struct mt9d113_res[] = {
	{
	.desc	= "QCIF",
	.res	= MT9D113_RES_QCIF,
	.width	= 176,
	.height	= 144,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 3,
	.row_time = 48,
	},
	{
	.desc	= "QVGA",
	.res	= MT9D113_RES_QVGA,
	.width	= 320,
	.height	= 240,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 3,
	.row_time = 48,
	},
	{
	.desc	= "CIF",
	.res	= MT9D113_RES_CIF,
	.width	= 352,
	.height	= 288,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 3,
	.row_time = 48,
	},
	{
	.desc	= "VGA_WIDE",
	.res	= MT9D113_RES_VGA_WIDE,
	.width	= 640,
	.height	= 360,
	.fps	= 29,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 3,
	.row_time = 48,
	},
	{
	.desc	= "VGA",
	.res	= MT9D113_RES_VGA,
	.width	= 640,
	.height	= 480,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 3,
	.row_time = 48,
	},
	{
	.desc	= "SVGA",
	.res	= MT9D113_RES_SVGA,
	.width	= 800,
	.height	= 600,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 3,
	.row_time = 48,
	},
	{
	.desc	= "720p",
	.res	= MT9D113_RES_720P,
	.width	= 1280,
	.height	= 720,
	.fps	= 29,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 3,
	.row_time = 42,
	},
	{
	.desc	= "1M",
	.res	= MT9D113_RES_1M,
	.width	= 1024,
	.height	= 768,
	.fps	= 15,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 3,
	.row_time = 52,
	},
	{
	.desc	= "2M",
	.res	= MT9D113_RES_2M,
	.width	= 1600,
	.height	= 1200,
	.fps	= 10,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 3,
	.row_time = 52,
	},
};
#define N_RES (ARRAY_SIZE(mt9d113_res))

static const struct i2c_device_id mt9d113_id[] = {
	{"mt9d113", 0},
	{}
};

/*
 * Context A setting for 176x144
 *
; Max Frame Time: 33.3333 msec
; Max Frame Clocks: 1413333.3 clocks (42.400 MHz)
; Pixel Clock: divided by 1
; Skip Mode: 2x cols, 2x rows, Bin Mode: Yes
; Horiz clks:  808 active + 1237 blank = 2045 total
; Vert  rows:  608 active + 83 blank = 691 total
; Extra Delay: 238 clocks
;
; Actual Frame Clocks: 1413333 clocks
; Row Time: 48.231 usec / 2045 clocks
; Frame time: 33.333325 msec
; Frames per Sec: 30 fps
;
; 50Hz Flicker Period: 207.33 lines
; 60Hz Flicker Period: 172.78 lines

; hblank: 29.17us
; vblank: 4ms
 */
static struct misensor_reg const mt9d113_qcif_30_init[] = {
	{MISENSOR_16BIT, 0x98c, 0x2703},/*Output Width (a)*/
	{MISENSOR_16BIT, 0x990, 0x00b0},/*      = 176*/
	{MISENSOR_16BIT, 0x98c, 0x2705},/*Output Height (a)*/
	{MISENSOR_16BIT, 0x990, 0x0090},/*      = 144*/
	{MISENSOR_16BIT, 0x98c, 0x2707},/*Output Width (b)*/
	{MISENSOR_16BIT, 0x990, 0x0640},/*      = 1600*/
	{MISENSOR_16BIT, 0x98c, 0x2709},/*Output Height (b)*/
	{MISENSOR_16BIT, 0x990, 0x04b0},/*      = 1200*/
	{MISENSOR_16BIT, 0x98c, 0x270d},/*Row Start (a)*/
	{MISENSOR_16BIT, 0x990, 0x000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x270f},/*column Start (a)*/
	{MISENSOR_16BIT, 0x990, 0x000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x2711},/*Row end (a)*/
	{MISENSOR_16BIT, 0x990, 0x04bd},/*      = 1213*/
	{MISENSOR_16BIT, 0x98c, 0x2713},/*column end (a)*/
	{MISENSOR_16BIT, 0x990, 0x064d},/*      = 1613*/
	{MISENSOR_16BIT, 0x98c, 0x2715},/*Row Speed (a)*/
	{MISENSOR_16BIT, 0x990, 0x0111},/*      = 273*/
	{MISENSOR_16BIT, 0x98c, 0x2717},/*Read Mode (a)*/
	{MISENSOR_16BIT, 0x990, 0x046f},/*      = 1135*/
	{MISENSOR_16BIT, 0x98c, 0x2719},/*sensor_fine_correction (a)*/
	{MISENSOR_16BIT, 0x990, 0x005a},/*      = 90*/
	{MISENSOR_16BIT, 0x98c, 0x271b},/*sensor_fine_IT_min (a)*/
	{MISENSOR_16BIT, 0x990, 0x01be},/*      = 446*/
	{MISENSOR_16BIT, 0x98c, 0x271d},/*sensor_fine_IT_max_margin (a)*/
	{MISENSOR_16BIT, 0x990, 0x0131},/*      = 305*/
	{MISENSOR_16BIT, 0x98c, 0x271f},/*frame Lines (a)*/
	{MISENSOR_16BIT, 0x990, 0x02b3},/*      = 691*/
	{MISENSOR_16BIT, 0x98c, 0x2721},/*Line Length (a)*/
	{MISENSOR_16BIT, 0x990, 0x07fd},/*      = 2045*/
	{MISENSOR_16BIT, 0x98c, 0x2723},/*Row Start (b)*/
	{MISENSOR_16BIT, 0x990, 0x0004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x2725},/*column Start (b)*/
	{MISENSOR_16BIT, 0x990, 0x0004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x2727},/*Row end (b)*/
	{MISENSOR_16BIT, 0x990, 0x04bb},/*      = 1211*/
	{MISENSOR_16BIT, 0x98c, 0x2729},/*column end (b)*/
	{MISENSOR_16BIT, 0x990, 0x064b},/*      = 1611*/
	{MISENSOR_16BIT, 0x98c, 0x272b},/*Row Speed (b)*/
	{MISENSOR_16BIT, 0x990, 0x0111},/*      = 273*/
	{MISENSOR_16BIT, 0x98c, 0x272d},/*Read Mode (b)*/
	{MISENSOR_16BIT, 0x990, 0x0024},/*      = 36*/
	{MISENSOR_16BIT, 0x98c, 0x272f},/*sensor_fine_correction (b)*/
	{MISENSOR_16BIT, 0x990, 0x003a},/*      = 58*/
	{MISENSOR_16BIT, 0x98c, 0x2731},/*sensor_fine_IT_min (b)*/
	{MISENSOR_16BIT, 0x990, 0x00f6},/*      = 246*/
	{MISENSOR_16BIT, 0x98c, 0x2733},/*sensor_fine_IT_max_margin (b)*/
	{MISENSOR_16BIT, 0x990, 0x008b},/*      = 139*/
	{MISENSOR_16BIT, 0x98c, 0x2735},/*frame Lines (b)*/
	{MISENSOR_16BIT, 0x990, 0x050d},/*      = 1293*/
	{MISENSOR_16BIT, 0x98c, 0x2737},/*Line Length (b)*/
	{MISENSOR_16BIT, 0x990, 0x0c24},/*      = 3108*/
	{MISENSOR_16BIT, 0x98c, 0x2739},/*crop_X0 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x273b},/*crop_X1 (a)*/
	{MISENSOR_16BIT, 0x990, 0x031f},/*      = 799*/
	{MISENSOR_16BIT, 0x98c, 0x273d},/*crop_Y0 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x273f},/*crop_Y1 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0257},/*      = 599*/
	{MISENSOR_16BIT, 0x98c, 0x2747},/*crop_X0 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x2749},/*crop_X1 (b)*/
	{MISENSOR_16BIT, 0x990, 0x063f},/*      = 1599*/
	{MISENSOR_16BIT, 0x98c, 0x274b},/*crop_Y0 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x274d},/*crop_Y1 (b)*/
	{MISENSOR_16BIT, 0x990, 0x04af},/*      = 1199*/
	{MISENSOR_16BIT, 0x98c, 0x222d},/*R9 Step*/
	{MISENSOR_16BIT, 0x990, 0x00ad},/*      = 173*/
	{MISENSOR_16BIT, 0x98c, 0xa408},/*search_f1_50*/
	{MISENSOR_16BIT, 0x990, 0x2a},/*      = 42*/
	{MISENSOR_16BIT, 0x98c, 0xa409},/*search_f2_50*/
	{MISENSOR_16BIT, 0x990, 0x2c},/*      = 44*/
	{MISENSOR_16BIT, 0x98c, 0xa40a},/*search_f1_60*/
	{MISENSOR_16BIT, 0x990, 0x32},/*      = 50*/
	{MISENSOR_16BIT, 0x98c, 0xa40b},/*search_f2_60*/
	{MISENSOR_16BIT, 0x990, 0x34},/*      = 52*/
	{MISENSOR_16BIT, 0x98c, 0x2411},/*R9_Step_60 (a)*/
	{MISENSOR_16BIT, 0x990, 0x00ad},/*      = 173*/
	{MISENSOR_16BIT, 0x98c, 0x2413},/*R9_Step_50 (a)*/
	{MISENSOR_16BIT, 0x990, 0x00cf},/*      = 207*/
	{MISENSOR_16BIT, 0x98c, 0x2415},/*R9_Step_60 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0071},/*      = 113*/
	{MISENSOR_16BIT, 0x98c, 0x2417},/*R9_Step_50 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0087},/*      = 135*/
	{MISENSOR_16BIT, 0x98c, 0xa404},/*fd Mode*/
	{MISENSOR_16BIT, 0x990, 0x10},/*      = 16*/
	{MISENSOR_16BIT, 0x98c, 0xa40d},/*Stat_min*/
	{MISENSOR_16BIT, 0x990, 0x02},/*      = 2*/
	{MISENSOR_16BIT, 0x98c, 0xa40e},/*Stat_max*/
	{MISENSOR_16BIT, 0x990, 0x03},/*      = 3*/
	{MISENSOR_16BIT, 0x98c, 0xa410},/*Min_amplitude*/
	{MISENSOR_16BIT, 0x990, 0x0a},/*      = 10*/
	{MISENSOR_TOK_TERM, 0, 0}
};

/*
 * Context A setting for 352x288
 *
; Max Frame Time: 33.3333 msec
; Max Frame Clocks: 1413333.3 clocks (42.400 MHz)
; Pixel Clock: divided by 1
; Skip Mode: 2x cols, 2x rows, Bin Mode: Yes
; Horiz clks:  808 active + 1237 blank = 2045 total
; Vert  rows:  608 active + 83 blank = 691 total
; Extra Delay: 238 clocks
;
; Actual Frame Clocks: 1413333 clocks
; Row Time: 48.231 usec / 2045 clocks
; Frame time: 33.333325 msec
; Frames per Sec: 30 fps
;
; 50Hz Flicker Period: 207.33 lines
; 60Hz Flicker Period: 172.78 lines

; hblank: 29.17us
; vblank: 4ms
 */
static struct misensor_reg const mt9d113_cif_30_init[] = {
	{MISENSOR_16BIT, 0x98c, 0x2703},/*Output Width (a)*/
	{MISENSOR_16BIT, 0x990, 0x0160},/*      = 352*/
	{MISENSOR_16BIT, 0x98c, 0x2705},/*Output Height (a)*/
	{MISENSOR_16BIT, 0x990, 0x0120},/*      = 288*/
	{MISENSOR_16BIT, 0x98c, 0x2707},/*Output Width (b)*/
	{MISENSOR_16BIT, 0x990, 0x0640},/*      = 1600*/
	{MISENSOR_16BIT, 0x98c, 0x2709},/*Output Height (b)*/
	{MISENSOR_16BIT, 0x990, 0x04b0},/*      = 1200*/
	{MISENSOR_16BIT, 0x98c, 0x270d},/*Row Start (a)*/
	{MISENSOR_16BIT, 0x990, 0x000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x270f},/*column Start (a)*/
	{MISENSOR_16BIT, 0x990, 0x000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x2711},/*Row end (a)*/
	{MISENSOR_16BIT, 0x990, 0x04bd},/*      = 1213*/
	{MISENSOR_16BIT, 0x98c, 0x2713},/*column end (a)*/
	{MISENSOR_16BIT, 0x990, 0x064d},/*      = 1613*/
	{MISENSOR_16BIT, 0x98c, 0x2715},/*Row Speed (a)*/
	{MISENSOR_16BIT, 0x990, 0x0111},/*      = 273*/
	{MISENSOR_16BIT, 0x98c, 0x2717},/*Read Mode (a)*/
	{MISENSOR_16BIT, 0x990, 0x046f},/*      = 1135*/
	{MISENSOR_16BIT, 0x98c, 0x2719},/*sensor_fine_correction (a)*/
	{MISENSOR_16BIT, 0x990, 0x005a},/*      = 90*/
	{MISENSOR_16BIT, 0x98c, 0x271b},/*sensor_fine_IT_min (a)*/
	{MISENSOR_16BIT, 0x990, 0x01be},/*      = 446*/
	{MISENSOR_16BIT, 0x98c, 0x271d},/*sensor_fine_IT_max_margin (a)*/
	{MISENSOR_16BIT, 0x990, 0x0131},/*      = 305*/
	{MISENSOR_16BIT, 0x98c, 0x271f},/*frame Lines (a)*/
	{MISENSOR_16BIT, 0x990, 0x02b3},/*      = 691*/
	{MISENSOR_16BIT, 0x98c, 0x2721},/*Line Length (a)*/
	{MISENSOR_16BIT, 0x990, 0x07fd},/*      = 2045*/
	{MISENSOR_16BIT, 0x98c, 0x2723},/*Row Start (b)*/
	{MISENSOR_16BIT, 0x990, 0x0004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x2725},/*column Start (b)*/
	{MISENSOR_16BIT, 0x990, 0x0004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x2727},/*Row end (b)*/
	{MISENSOR_16BIT, 0x990, 0x04bb},/*      = 1211*/
	{MISENSOR_16BIT, 0x98c, 0x2729},/*column end (b)*/
	{MISENSOR_16BIT, 0x990, 0x064b},/*      = 1611*/
	{MISENSOR_16BIT, 0x98c, 0x272b},/*Row Speed (b)*/
	{MISENSOR_16BIT, 0x990, 0x0111},/*      = 273*/
	{MISENSOR_16BIT, 0x98c, 0x272d},/*Read Mode (b)*/
	{MISENSOR_16BIT, 0x990, 0x0024},/*      = 36*/
	{MISENSOR_16BIT, 0x98c, 0x272f},/*sensor_fine_correction (b)*/
	{MISENSOR_16BIT, 0x990, 0x003a},/*      = 58*/
	{MISENSOR_16BIT, 0x98c, 0x2731},/*sensor_fine_IT_min (b)*/
	{MISENSOR_16BIT, 0x990, 0x00f6},/*      = 246*/
	{MISENSOR_16BIT, 0x98c, 0x2733},/*sensor_fine_IT_max_margin (b)*/
	{MISENSOR_16BIT, 0x990, 0x008b},/*      = 139*/
	{MISENSOR_16BIT, 0x98c, 0x2735},/*frame Lines (b)*/
	{MISENSOR_16BIT, 0x990, 0x050d},/*      = 1293*/
	{MISENSOR_16BIT, 0x98c, 0x2737},/*Line Length (b)*/
	{MISENSOR_16BIT, 0x990, 0x0c24},/*      = 3108*/
	{MISENSOR_16BIT, 0x98c, 0x2739},/*crop_X0 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x273b},/*crop_X1 (a)*/
	{MISENSOR_16BIT, 0x990, 0x031f},/*      = 799*/
	{MISENSOR_16BIT, 0x98c, 0x273d},/*crop_Y0 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x273f},/*crop_Y1 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0257},/*      = 599*/
	{MISENSOR_16BIT, 0x98c, 0x2747},/*crop_X0 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x2749},/*crop_X1 (b)*/
	{MISENSOR_16BIT, 0x990, 0x063f},/*      = 1599*/
	{MISENSOR_16BIT, 0x98c, 0x274b},/*crop_Y0 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x274d},/*crop_Y1 (b)*/
	{MISENSOR_16BIT, 0x990, 0x04af},/*      = 1199*/
	{MISENSOR_16BIT, 0x98c, 0x222d},/*R9 Step*/
	{MISENSOR_16BIT, 0x990, 0x00ad},/*      = 173*/
	{MISENSOR_16BIT, 0x98c, 0xa408},/*search_f1_50*/
	{MISENSOR_16BIT, 0x990, 0x2a},/*      = 42*/
	{MISENSOR_16BIT, 0x98c, 0xa409},/*search_f2_50*/
	{MISENSOR_16BIT, 0x990, 0x2c},/*      = 44*/
	{MISENSOR_16BIT, 0x98c, 0xa40a},/*search_f1_60*/
	{MISENSOR_16BIT, 0x990, 0x32},/*      = 50*/
	{MISENSOR_16BIT, 0x98c, 0xa40b},/*search_f2_60*/
	{MISENSOR_16BIT, 0x990, 0x34},/*      = 52*/
	{MISENSOR_16BIT, 0x98c, 0x2411},/*R9_Step_60 (a)*/
	{MISENSOR_16BIT, 0x990, 0x00ad},/*      = 173*/
	{MISENSOR_16BIT, 0x98c, 0x2413},/*R9_Step_50 (a)*/
	{MISENSOR_16BIT, 0x990, 0x00cf},/*      = 207*/
	{MISENSOR_16BIT, 0x98c, 0x2415},/*R9_Step_60 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0071},/*      = 113*/
	{MISENSOR_16BIT, 0x98c, 0x2417},/*R9_Step_50 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0087},/*      = 135*/
	{MISENSOR_16BIT, 0x98c, 0xa404},/*fd Mode*/
	{MISENSOR_16BIT, 0x990, 0x10},/*      = 16*/
	{MISENSOR_16BIT, 0x98c, 0xa40d},/*Stat_min*/
	{MISENSOR_16BIT, 0x990, 0x02},/*      = 2*/
	{MISENSOR_16BIT, 0x98c, 0xa40e},/*Stat_max*/
	{MISENSOR_16BIT, 0x990, 0x03},/*      = 3*/
	{MISENSOR_16BIT, 0x98c, 0xa410},/*Min_amplitude*/
	{MISENSOR_16BIT, 0x990, 0x0a},/*      = 10*/
	{MISENSOR_TOK_TERM, 0, 0}
};

/*
 * Context A setting for 320x240
 *
; Max Frame Time: 33.3333 msec
; Max Frame Clocks: 1413333.3 clocks (42.400 MHz)
; Pixel Clock: divided by 1
; Skip Mode: 2x cols, 2x rows, Bin Mode: Yes
; Horiz clks:  808 active + 1237 blank = 2045 total
; Vert  rows:  608 active + 83 blank = 691 total
; Extra Delay: 238 clocks
;
; Actual Frame Clocks: 1413333 clocks
; Row Time: 48.231 usec / 2045 clocks
; Frame time: 33.333325 msec
; Frames per Sec: 30 fps
;
; 50Hz Flicker Period: 207.33 lines
; 60Hz Flicker Period: 172.78 lines

; hblank: 29.17us
; vblank: 4ms
 */
static struct misensor_reg const mt9d113_qvga_30_init[] = {
	{MISENSOR_16BIT, 0x98c, 0x2703},/*Output Width (a)*/
	{MISENSOR_16BIT, 0x990, 0x0140},/*      = 320*/
	{MISENSOR_16BIT, 0x98c, 0x2705},/*Output Height (a)*/
	{MISENSOR_16BIT, 0x990, 0x00f0},/*      = 240*/
	{MISENSOR_16BIT, 0x98c, 0x2707},/*Output Width (b)*/
	{MISENSOR_16BIT, 0x990, 0x0640},/*      = 1600*/
	{MISENSOR_16BIT, 0x98c, 0x2709},/*Output Height (b)*/
	{MISENSOR_16BIT, 0x990, 0x04b0},/*      = 1200*/
	{MISENSOR_16BIT, 0x98c, 0x270d},/*Row Start (a)*/
	{MISENSOR_16BIT, 0x990, 0x000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x270f},/*column Start (a)*/
	{MISENSOR_16BIT, 0x990, 0x000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x2711},/*Row end (a)*/
	{MISENSOR_16BIT, 0x990, 0x04bd},/*      = 1213*/
	{MISENSOR_16BIT, 0x98c, 0x2713},/*column end (a)*/
	{MISENSOR_16BIT, 0x990, 0x064d},/*      = 1613*/
	{MISENSOR_16BIT, 0x98c, 0x2715},/*Row Speed (a)*/
	{MISENSOR_16BIT, 0x990, 0x0111},/*      = 273*/
	{MISENSOR_16BIT, 0x98c, 0x2717},/*Read Mode (a)*/
	{MISENSOR_16BIT, 0x990, 0x046f},/*      = 1135*/
	{MISENSOR_16BIT, 0x98c, 0x2719},/*sensor_fine_correction (a)*/
	{MISENSOR_16BIT, 0x990, 0x005a},/*      = 90*/
	{MISENSOR_16BIT, 0x98c, 0x271b},/*sensor_fine_IT_min (a)*/
	{MISENSOR_16BIT, 0x990, 0x01be},/*      = 446*/
	{MISENSOR_16BIT, 0x98c, 0x271d},/*sensor_fine_IT_max_margin (a)*/
	{MISENSOR_16BIT, 0x990, 0x0131},/*      = 305*/
	{MISENSOR_16BIT, 0x98c, 0x271f},/*frame Lines (a)*/
	{MISENSOR_16BIT, 0x990, 0x02b3},/*      = 691*/
	{MISENSOR_16BIT, 0x98c, 0x2721},/*Line Length (a)*/
	{MISENSOR_16BIT, 0x990, 0x07fd},/*      = 2045*/
	{MISENSOR_16BIT, 0x98c, 0x2723},/*Row Start (b)*/
	{MISENSOR_16BIT, 0x990, 0x0004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x2725},/*column Start (b)*/
	{MISENSOR_16BIT, 0x990, 0x0004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x2727},/*Row end (b)*/
	{MISENSOR_16BIT, 0x990, 0x04bb},/*      = 1211*/
	{MISENSOR_16BIT, 0x98c, 0x2729},/*column end (b)*/
	{MISENSOR_16BIT, 0x990, 0x064b},/*      = 1611*/
	{MISENSOR_16BIT, 0x98c, 0x272b},/*Row Speed (b)*/
	{MISENSOR_16BIT, 0x990, 0x0111},/*      = 273*/
	{MISENSOR_16BIT, 0x98c, 0x272d},/*Read Mode (b)*/
	{MISENSOR_16BIT, 0x990, 0x0024},/*      = 36*/
	{MISENSOR_16BIT, 0x98c, 0x272f},/*sensor_fine_correction (b)*/
	{MISENSOR_16BIT, 0x990, 0x003a},/*      = 58*/
	{MISENSOR_16BIT, 0x98c, 0x2731},/*sensor_fine_IT_min (b)*/
	{MISENSOR_16BIT, 0x990, 0x00f6},/*      = 246*/
	{MISENSOR_16BIT, 0x98c, 0x2733},/*sensor_fine_IT_max_margin (b)*/
	{MISENSOR_16BIT, 0x990, 0x008b},/*      = 139*/
	{MISENSOR_16BIT, 0x98c, 0x2735},/*frame Lines (b)*/
	{MISENSOR_16BIT, 0x990, 0x050d},/*      = 1293*/
	{MISENSOR_16BIT, 0x98c, 0x2737},/*Line Length (b)*/
	{MISENSOR_16BIT, 0x990, 0x0c24},/*      = 3108*/
	{MISENSOR_16BIT, 0x98c, 0x2739},/*crop_X0 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x273b},/*crop_X1 (a)*/
	{MISENSOR_16BIT, 0x990, 0x031f},/*      = 799*/
	{MISENSOR_16BIT, 0x98c, 0x273d},/*crop_Y0 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x273f},/*crop_Y1 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0257},/*      = 599*/
	{MISENSOR_16BIT, 0x98c, 0x2747},/*crop_X0 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x2749},/*crop_X1 (b)*/
	{MISENSOR_16BIT, 0x990, 0x063f},/*      = 1599*/
	{MISENSOR_16BIT, 0x98c, 0x274b},/*crop_Y0 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x274d},/*crop_Y1 (b)*/
	{MISENSOR_16BIT, 0x990, 0x04af},/*      = 1199*/
	{MISENSOR_16BIT, 0x98c, 0x222d},/*R9 Step*/
	{MISENSOR_16BIT, 0x990, 0x00ad},/*      = 173*/
	{MISENSOR_16BIT, 0x98c, 0xa408},/*search_f1_50*/
	{MISENSOR_16BIT, 0x990, 0x2a},/*      = 42*/
	{MISENSOR_16BIT, 0x98c, 0xa409},/*search_f2_50*/
	{MISENSOR_16BIT, 0x990, 0x2c},/*      = 44*/
	{MISENSOR_16BIT, 0x98c, 0xa40a},/*search_f1_60*/
	{MISENSOR_16BIT, 0x990, 0x32},/*      = 50*/
	{MISENSOR_16BIT, 0x98c, 0xa40b},/*search_f2_60*/
	{MISENSOR_16BIT, 0x990, 0x34},/*      = 52*/
	{MISENSOR_16BIT, 0x98c, 0x2411},/*R9_Step_60 (a)*/
	{MISENSOR_16BIT, 0x990, 0x00ad},/*      = 173*/
	{MISENSOR_16BIT, 0x98c, 0x2413},/*R9_Step_50 (a)*/
	{MISENSOR_16BIT, 0x990, 0x00cf},/*      = 207*/
	{MISENSOR_16BIT, 0x98c, 0x2415},/*R9_Step_60 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0071},/*      = 113*/
	{MISENSOR_16BIT, 0x98c, 0x2417},/*R9_Step_50 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0087},/*      = 135*/
	{MISENSOR_16BIT, 0x98c, 0xa404},/*fd Mode*/
	{MISENSOR_16BIT, 0x990, 0x10},/*      = 16*/
	{MISENSOR_16BIT, 0x98c, 0xa40d},/*Stat_min*/
	{MISENSOR_16BIT, 0x990, 0x02},/*      = 2*/
	{MISENSOR_16BIT, 0x98c, 0xa40e},/*Stat_max*/
	{MISENSOR_16BIT, 0x990, 0x03},/*      = 3*/
	{MISENSOR_16BIT, 0x98c, 0xa410},/*Min_amplitude*/
	{MISENSOR_16BIT, 0x990, 0x0a},/*      = 10*/
	{MISENSOR_TOK_TERM, 0, 0}
};

/*
 * Context A setting for 640x480
 *
; Max Frame Time: 33.3333 msec
; Max Frame Clocks: 1413333.3 clocks (42.400 MHz)
; Pixel Clock: divided by 1
; Skip Mode: 2x cols, 2x rows, Bin Mode: Yes
; Horiz clks:  808 active + 1237 blank = 2045 total
; Vert  rows:  608 active + 83 blank = 691 total
; Extra Delay: 238 clocks
;
; Actual Frame Clocks: 1413333 clocks
; Row Time: 48.231 usec / 2045 clocks
; Frame time: 33.333325 msec
; Frames per Sec: 30 fps
;
; 50Hz Flicker Period: 207.33 lines
; 60Hz Flicker Period: 172.78 lines

; hblank: 29.17us
; vblank: 4ms
 */
static struct misensor_reg const mt9d113_vga_30_init[] = {
	{MISENSOR_16BIT, 0x98c, 0x2703},/*Output Width (a)*/
	{MISENSOR_16BIT, 0x990, 0x0280},/*      = 640*/
	{MISENSOR_16BIT, 0x98c, 0x2705},/*Output Height (a)*/
	{MISENSOR_16BIT, 0x990, 0x01e0},/*      = 480*/
	{MISENSOR_16BIT, 0x98c, 0x2707},/*Output Width (b)*/
	{MISENSOR_16BIT, 0x990, 0x0640},/*      = 1600*/
	{MISENSOR_16BIT, 0x98c, 0x2709},/*Output Height (b)*/
	{MISENSOR_16BIT, 0x990, 0x04b0},/*      = 1200*/
	{MISENSOR_16BIT, 0x98c, 0x270d},/*Row Start (a)*/
	{MISENSOR_16BIT, 0x990, 0x000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x270f},/*column Start (a)*/
	{MISENSOR_16BIT, 0x990, 0x000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x2711},/*Row end (a)*/
	{MISENSOR_16BIT, 0x990, 0x04bd},/*      = 1213*/
	{MISENSOR_16BIT, 0x98c, 0x2713},/*column end (a)*/
	{MISENSOR_16BIT, 0x990, 0x064d},/*      = 1613*/
	{MISENSOR_16BIT, 0x98c, 0x2715},/*Row Speed (a)*/
	{MISENSOR_16BIT, 0x990, 0x0111},/*      = 273*/
	{MISENSOR_16BIT, 0x98c, 0x2717},/*Read Mode (a)*/
	{MISENSOR_16BIT, 0x990, 0x046f},/*      = 1135*/
	{MISENSOR_16BIT, 0x98c, 0x2719},/*sensor_fine_correction (a)*/
	{MISENSOR_16BIT, 0x990, 0x005a},/*      = 90*/
	{MISENSOR_16BIT, 0x98c, 0x271b},/*sensor_fine_IT_min (a)*/
	{MISENSOR_16BIT, 0x990, 0x01be},/*      = 446*/
	{MISENSOR_16BIT, 0x98c, 0x271d},/*sensor_fine_IT_max_margin (a)*/
	{MISENSOR_16BIT, 0x990, 0x0131},/*      = 305*/
	{MISENSOR_16BIT, 0x98c, 0x271f},/*frame Lines (a)*/
	{MISENSOR_16BIT, 0x990, 0x02b3},/*      = 691*/
	{MISENSOR_16BIT, 0x98c, 0x2721},/*Line Length (a)*/
	{MISENSOR_16BIT, 0x990, 0x07fd},/*      = 2045*/
	{MISENSOR_16BIT, 0x98c, 0x2723},/*Row Start (b)*/
	{MISENSOR_16BIT, 0x990, 0x0004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x2725},/*column Start (b)*/
	{MISENSOR_16BIT, 0x990, 0x0004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x2727},/*Row end (b)*/
	{MISENSOR_16BIT, 0x990, 0x04bb},/*      = 1211*/
	{MISENSOR_16BIT, 0x98c, 0x2729},/*column end (b)*/
	{MISENSOR_16BIT, 0x990, 0x064b},/*      = 1611*/
	{MISENSOR_16BIT, 0x98c, 0x272b},/*Row Speed (b)*/
	{MISENSOR_16BIT, 0x990, 0x0111},/*      = 273*/
	{MISENSOR_16BIT, 0x98c, 0x272d},/*Read Mode (b)*/
	{MISENSOR_16BIT, 0x990, 0x0024},/*      = 36*/
	{MISENSOR_16BIT, 0x98c, 0x272f},/*sensor_fine_correction (b)*/
	{MISENSOR_16BIT, 0x990, 0x003a},/*      = 58*/
	{MISENSOR_16BIT, 0x98c, 0x2731},/*sensor_fine_IT_min (b)*/
	{MISENSOR_16BIT, 0x990, 0x00f6},/*      = 246*/
	{MISENSOR_16BIT, 0x98c, 0x2733},/*sensor_fine_IT_max_margin (b)*/
	{MISENSOR_16BIT, 0x990, 0x008b},/*      = 139*/
	{MISENSOR_16BIT, 0x98c, 0x2735},/*frame Lines (b)*/
	{MISENSOR_16BIT, 0x990, 0x050d},/*      = 1293*/
	{MISENSOR_16BIT, 0x98c, 0x2737},/*Line Length (b)*/
	{MISENSOR_16BIT, 0x990, 0x0c24},/*      = 3108*/
	{MISENSOR_16BIT, 0x98c, 0x2739},/*crop_X0 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x273b},/*crop_X1 (a)*/
	{MISENSOR_16BIT, 0x990, 0x031f},/*      = 799*/
	{MISENSOR_16BIT, 0x98c, 0x273d},/*crop_Y0 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x273f},/*crop_Y1 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0257},/*      = 599*/
	{MISENSOR_16BIT, 0x98c, 0x2747},/*crop_X0 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x2749},/*crop_X1 (b)*/
	{MISENSOR_16BIT, 0x990, 0x063f},/*      = 1599*/
	{MISENSOR_16BIT, 0x98c, 0x274b},/*crop_Y0 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x274d},/*crop_Y1 (b)*/
	{MISENSOR_16BIT, 0x990, 0x04af},/*      = 1199*/
	{MISENSOR_16BIT, 0x98c, 0x222d},/*R9 Step*/
	{MISENSOR_16BIT, 0x990, 0x00ad},/*      = 173*/
	{MISENSOR_16BIT, 0x98c, 0xa408},/*search_f1_50*/
	{MISENSOR_16BIT, 0x990, 0x2a},/*      = 42*/
	{MISENSOR_16BIT, 0x98c, 0xa409},/*search_f2_50*/
	{MISENSOR_16BIT, 0x990, 0x2c},/*      = 44*/
	{MISENSOR_16BIT, 0x98c, 0xa40a},/*search_f1_60*/
	{MISENSOR_16BIT, 0x990, 0x32},/*      = 50*/
	{MISENSOR_16BIT, 0x98c, 0xa40b},/*search_f2_60*/
	{MISENSOR_16BIT, 0x990, 0x34},/*      = 52*/
	{MISENSOR_16BIT, 0x98c, 0x2411},/*R9_Step_60 (a)*/
	{MISENSOR_16BIT, 0x990, 0x00ad},/*      = 173*/
	{MISENSOR_16BIT, 0x98c, 0x2413},/*R9_Step_50 (a)*/
	{MISENSOR_16BIT, 0x990, 0x00cf},/*      = 207*/
	{MISENSOR_16BIT, 0x98c, 0x2415},/*R9_Step_60 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0071},/*      = 113*/
	{MISENSOR_16BIT, 0x98c, 0x2417},/*R9_Step_50 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0087},/*      = 135*/
	{MISENSOR_16BIT, 0x98c, 0xa404},/*fd Mode*/
	{MISENSOR_16BIT, 0x990, 0x10},/*      = 16*/
	{MISENSOR_16BIT, 0x98c, 0xa40d},/*Stat_min*/
	{MISENSOR_16BIT, 0x990, 0x02},/*      = 2*/
	{MISENSOR_16BIT, 0x98c, 0xa40e},/*Stat_max*/
	{MISENSOR_16BIT, 0x990, 0x03},/*      = 3*/
	{MISENSOR_16BIT, 0x98c, 0xa410},/*Min_amplitude*/
	{MISENSOR_16BIT, 0x990, 0x0a},/*      = 10*/
	{MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const mt9d113_vga_wide_29_init[] = {
	{MISENSOR_16BIT, 0x98c, 0x2703},/*Output Width (a)*/
	{MISENSOR_16BIT, 0x990, 0x0280},/*      = 640*/
	{MISENSOR_16BIT, 0x98c, 0x2705},/*Output Height (a)*/
	{MISENSOR_16BIT, 0x990, 0x0168},/*      = 360*/
	{MISENSOR_16BIT, 0x98c, 0x2707},/*Output Width (b)*/
	{MISENSOR_16BIT, 0x990, 0x0640},/*      = 1600*/
	{MISENSOR_16BIT, 0x98c, 0x2709},/*Output Height (b)*/
	{MISENSOR_16BIT, 0x990, 0x04b0},/*      = 1200*/
	{MISENSOR_16BIT, 0x98c, 0x270d},/*Row Start (a)*/
	{MISENSOR_16BIT, 0x990, 0x0f6},/*      = 246*/
	{MISENSOR_16BIT, 0x98c, 0x270f},/*column Start (a)*/
	{MISENSOR_16BIT, 0x990, 0x0a6},/*      = 166*/
	{MISENSOR_16BIT, 0x98c, 0x2711},/*Row end (a)*/
	{MISENSOR_16BIT, 0x990, 0x03cd},/*      = 973*/
	{MISENSOR_16BIT, 0x98c, 0x2713},/*column end (a)*/
	{MISENSOR_16BIT, 0x990, 0x05ad},/*      = 1453*/
	{MISENSOR_16BIT, 0x98c, 0x2715},/*Row Speed (a)*/
	{MISENSOR_16BIT, 0x990, 0x0111},/*      = 273*/
	{MISENSOR_16BIT, 0x98c, 0x2717},/*Read Mode (a)*/
	{MISENSOR_16BIT, 0x990, 0x0027},/*      = 39*/
	{MISENSOR_16BIT, 0x98c, 0x2719},/*sensor_fine_correction (a)*/
	{MISENSOR_16BIT, 0x990, 0x003a},/*      = 58*/
	{MISENSOR_16BIT, 0x98c, 0x271b},/*sensor_fine_IT_min (a)*/
	{MISENSOR_16BIT, 0x990, 0x00f6},/*      = 246*/
	{MISENSOR_16BIT, 0x98c, 0x271d},/*sensor_fine_IT_max_margin (a)*/
	{MISENSOR_16BIT, 0x990, 0x008b},/*      = 139*/
	{MISENSOR_16BIT, 0x98c, 0x271f},/*frame Lines (a)*/
	{MISENSOR_16BIT, 0x990, 0x032d},/*      = 813*/
	{MISENSOR_16BIT, 0x98c, 0x2721},/*Line Length (a)*/
	{MISENSOR_16BIT, 0x990, 0x0706},/*      = 1798*/
	{MISENSOR_16BIT, 0x98c, 0x2723},/*Row Start (b)*/
	{MISENSOR_16BIT, 0x990, 0x0004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x2725},/*column Start (b)*/
	{MISENSOR_16BIT, 0x990, 0x0004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x2727},/*Row end (b)*/
	{MISENSOR_16BIT, 0x990, 0x04bb},/*      = 1211*/
	{MISENSOR_16BIT, 0x98c, 0x2729},/*column end (b)*/
	{MISENSOR_16BIT, 0x990, 0x064b},/*      = 1611*/
	{MISENSOR_16BIT, 0x98c, 0x272b},/*Row Speed (b)*/
	{MISENSOR_16BIT, 0x990, 0x0111},/*      = 273*/
	{MISENSOR_16BIT, 0x98c, 0x272d},/*Read Mode (b)*/
	{MISENSOR_16BIT, 0x990, 0x0024},/*      = 36*/
	{MISENSOR_16BIT, 0x98c, 0x272f},/*sensor_fine_correction (b)*/
	{MISENSOR_16BIT, 0x990, 0x003a},/*      = 58*/
	{MISENSOR_16BIT, 0x98c, 0x2731},/*sensor_fine_IT_min (b)*/
	{MISENSOR_16BIT, 0x990, 0x00f6},/*      = 246*/
	{MISENSOR_16BIT, 0x98c, 0x2733},/*sensor_fine_IT_max_margin (b)*/
	{MISENSOR_16BIT, 0x990, 0x008b},/*      = 139*/
	{MISENSOR_16BIT, 0x98c, 0x2735},/*frame Lines (b)*/
	{MISENSOR_16BIT, 0x990, 0x050d},/*      = 1293*/
	{MISENSOR_16BIT, 0x98c, 0x2737},/*Line Length (b)*/
	{MISENSOR_16BIT, 0x990, 0x0c24},/*      = 3108*/
	{MISENSOR_16BIT, 0x98c, 0x2739},/*crop_X0 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x273b},/*crop_X1 (a)*/
	{MISENSOR_16BIT, 0x990, 0x04ff},/*      = 1279*/
	{MISENSOR_16BIT, 0x98c, 0x273d},/*crop_Y0 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x273f},/*crop_Y1 (a)*/
	{MISENSOR_16BIT, 0x990, 0x02cf},/*      = 719*/
	{MISENSOR_16BIT, 0x98c, 0x2747},/*crop_X0 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x2749},/*crop_X1 (b)*/
	{MISENSOR_16BIT, 0x990, 0x063f},/*      = 1599*/
	{MISENSOR_16BIT, 0x98c, 0x274b},/*crop_Y0 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x274d},/*crop_Y1 (b)*/
	{MISENSOR_16BIT, 0x990, 0x04af},/*      = 1199*/
	{MISENSOR_16BIT, 0x98c, 0x222d},/*R9 Step*/
	{MISENSOR_16BIT, 0x990, 0x00c5},/*      = 197*/
	{MISENSOR_16BIT, 0x98c, 0xa408},/*search_f1_50*/
	{MISENSOR_16BIT, 0x990, 0x30},/*      = 48*/
	{MISENSOR_16BIT, 0x98c, 0xa409},/*search_f2_50*/
	{MISENSOR_16BIT, 0x990, 0x32},/*      = 50*/
	{MISENSOR_16BIT, 0x98c, 0xa40a},/*search_f1_60*/
	{MISENSOR_16BIT, 0x990, 0x3a},/*      = 58*/
	{MISENSOR_16BIT, 0x98c, 0xa40b},/*search_f2_60*/
	{MISENSOR_16BIT, 0x990, 0x3c},/*      = 60*/
	{MISENSOR_16BIT, 0x98c, 0x2411},/*R9_Step_60 (a)*/
	{MISENSOR_16BIT, 0x990, 0x00c5},/*      = 197*/
	{MISENSOR_16BIT, 0x98c, 0x2413},/*R9_Step_50 (a)*/
	{MISENSOR_16BIT, 0x990, 0x00ec},/*      = 236*/
	{MISENSOR_16BIT, 0x98c, 0x2415},/*R9_Step_60 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0071},/*      = 113*/
	{MISENSOR_16BIT, 0x98c, 0x2417},/*R9_Step_50 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0087},/*      = 135*/
	{MISENSOR_16BIT, 0x98c, 0xa404},/*fd Mode*/
	{MISENSOR_16BIT, 0x990, 0x10},/*      = 16*/
	{MISENSOR_16BIT, 0x98c, 0xa40d},/*Stat_min*/
	{MISENSOR_16BIT, 0x990, 0x02},/*      = 2*/
	{MISENSOR_16BIT, 0x98c, 0xa40e},/*Stat_max*/
	{MISENSOR_16BIT, 0x990, 0x03},/*      = 3*/
	{MISENSOR_16BIT, 0x98c, 0xa410},/*Min_amplitude*/
	{MISENSOR_16BIT, 0x990, 0x0a},/*      = 10*/
	{MISENSOR_TOK_TERM, 0, 0}
};

/*
 * Context A setting for 1280x720
; Max Frame Time: 34.4828 msec
; Max Frame Clocks: 1462068.9 clocks (42.400 MHz)
; Pixel Clock: divided by 1
; Skip Mode: 1x cols, 1x rows, Bin Mode: No
; Horiz clks:  1288 active + 510 blank = 1798 total
; Vert  rows:  728 active + 85 blank = 813 total
; Extra Delay: 294 clocks
;
; Actual Frame Clocks: 1462068 clocks
; Row Time: 42.406 usec / 1798 clocks
; Frame time: 34.482736 msec
; Frames per Sec: 29 fps
;
; 50Hz Flicker Period: 235.82 lines
; 60Hz Flicker Period: 196.51 lines

; hblank: 12us
; vblank: 3.6ms
 */
static struct misensor_reg const mt9d113_720p_29_init[] = {
	{MISENSOR_16BIT, 0x98c, 0x2703},/*Output Width (a)*/
	{MISENSOR_16BIT, 0x990, 0x0500},/*      = 1280*/
	{MISENSOR_16BIT, 0x98c, 0x2705},/*Output Height (a)*/
	{MISENSOR_16BIT, 0x990, 0x02d0},/*      = 720*/
	{MISENSOR_16BIT, 0x98c, 0x2707},/*Output Width (b)*/
	{MISENSOR_16BIT, 0x990, 0x0640},/*      = 1600*/
	{MISENSOR_16BIT, 0x98c, 0x2709},/*Output Height (b)*/
	{MISENSOR_16BIT, 0x990, 0x04b0},/*      = 1200*/
	{MISENSOR_16BIT, 0x98c, 0x270d},/*Row Start (a)*/
	{MISENSOR_16BIT, 0x990, 0x0f6},/*      = 246*/
	{MISENSOR_16BIT, 0x98c, 0x270f},/*column Start (a)*/
	{MISENSOR_16BIT, 0x990, 0x0a6},/*      = 166*/
	{MISENSOR_16BIT, 0x98c, 0x2711},/*Row end (a)*/
	{MISENSOR_16BIT, 0x990, 0x03cd},/*      = 973*/
	{MISENSOR_16BIT, 0x98c, 0x2713},/*column end (a)*/
	{MISENSOR_16BIT, 0x990, 0x05ad},/*      = 1453*/
	{MISENSOR_16BIT, 0x98c, 0x2715},/*Row Speed (a)*/
	{MISENSOR_16BIT, 0x990, 0x0111},/*      = 273*/
	{MISENSOR_16BIT, 0x98c, 0x2717},/*Read Mode (a)*/
	{MISENSOR_16BIT, 0x990, 0x0027},/*      = 39*/
	{MISENSOR_16BIT, 0x98c, 0x2719},/*sensor_fine_correction (a)*/
	{MISENSOR_16BIT, 0x990, 0x003a},/*      = 58*/
	{MISENSOR_16BIT, 0x98c, 0x271b},/*sensor_fine_IT_min (a)*/
	{MISENSOR_16BIT, 0x990, 0x00f6},/*      = 246*/
	{MISENSOR_16BIT, 0x98c, 0x271d},/*sensor_fine_IT_max_margin (a)*/
	{MISENSOR_16BIT, 0x990, 0x008b},/*      = 139*/
	{MISENSOR_16BIT, 0x98c, 0x271f},/*frame Lines (a)*/
	{MISENSOR_16BIT, 0x990, 0x032d},/*      = 813*/
	{MISENSOR_16BIT, 0x98c, 0x2721},/*Line Length (a)*/
	{MISENSOR_16BIT, 0x990, 0x0706},/*      = 1798*/
	{MISENSOR_16BIT, 0x98c, 0x2723},/*Row Start (b)*/
	{MISENSOR_16BIT, 0x990, 0x0004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x2725},/*column Start (b)*/
	{MISENSOR_16BIT, 0x990, 0x0004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x2727},/*Row end (b)*/
	{MISENSOR_16BIT, 0x990, 0x04bb},/*      = 1211*/
	{MISENSOR_16BIT, 0x98c, 0x2729},/*column end (b)*/
	{MISENSOR_16BIT, 0x990, 0x064b},/*      = 1611*/
	{MISENSOR_16BIT, 0x98c, 0x272b},/*Row Speed (b)*/
	{MISENSOR_16BIT, 0x990, 0x0111},/*      = 273*/
	{MISENSOR_16BIT, 0x98c, 0x272d},/*Read Mode (b)*/
	{MISENSOR_16BIT, 0x990, 0x0024},/*      = 36*/
	{MISENSOR_16BIT, 0x98c, 0x272f},/*sensor_fine_correction (b)*/
	{MISENSOR_16BIT, 0x990, 0x003a},/*      = 58*/
	{MISENSOR_16BIT, 0x98c, 0x2731},/*sensor_fine_IT_min (b)*/
	{MISENSOR_16BIT, 0x990, 0x00f6},/*      = 246*/
	{MISENSOR_16BIT, 0x98c, 0x2733},/*sensor_fine_IT_max_margin (b)*/
	{MISENSOR_16BIT, 0x990, 0x008b},/*      = 139*/
	{MISENSOR_16BIT, 0x98c, 0x2735},/*frame Lines (b)*/
	{MISENSOR_16BIT, 0x990, 0x050d},/*      = 1293*/
	{MISENSOR_16BIT, 0x98c, 0x2737},/*Line Length (b)*/
	{MISENSOR_16BIT, 0x990, 0x0c24},/*      = 3108*/
	{MISENSOR_16BIT, 0x98c, 0x2739},/*crop_X0 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x273b},/*crop_X1 (a)*/
	{MISENSOR_16BIT, 0x990, 0x04ff},/*      = 1279*/
	{MISENSOR_16BIT, 0x98c, 0x273d},/*crop_Y0 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x273f},/*crop_Y1 (a)*/
	{MISENSOR_16BIT, 0x990, 0x02cf},/*      = 719*/
	{MISENSOR_16BIT, 0x98c, 0x2747},/*crop_X0 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x2749},/*crop_X1 (b)*/
	{MISENSOR_16BIT, 0x990, 0x063f},/*      = 1599*/
	{MISENSOR_16BIT, 0x98c, 0x274b},/*crop_Y0 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x274d},/*crop_Y1 (b)*/
	{MISENSOR_16BIT, 0x990, 0x04af},/*      = 1199*/
	{MISENSOR_16BIT, 0x98c, 0x222d},/*R9 Step*/
	{MISENSOR_16BIT, 0x990, 0x00c5},/*      = 197*/
	{MISENSOR_16BIT, 0x98c, 0xa408},/*search_f1_50*/
	{MISENSOR_16BIT, 0x990, 0x30},/*      = 48*/
	{MISENSOR_16BIT, 0x98c, 0xa409},/*search_f2_50*/
	{MISENSOR_16BIT, 0x990, 0x32},/*      = 50*/
	{MISENSOR_16BIT, 0x98c, 0xa40a},/*search_f1_60*/
	{MISENSOR_16BIT, 0x990, 0x3a},/*      = 58*/
	{MISENSOR_16BIT, 0x98c, 0xa40b},/*search_f2_60*/
	{MISENSOR_16BIT, 0x990, 0x3c},/*      = 60*/
	{MISENSOR_16BIT, 0x98c, 0x2411},/*R9_Step_60 (a)*/
	{MISENSOR_16BIT, 0x990, 0x00c5},/*      = 197*/
	{MISENSOR_16BIT, 0x98c, 0x2413},/*R9_Step_50 (a)*/
	{MISENSOR_16BIT, 0x990, 0x00ec},/*      = 236*/
	{MISENSOR_16BIT, 0x98c, 0x2415},/*R9_Step_60 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0071},/*      = 113*/
	{MISENSOR_16BIT, 0x98c, 0x2417},/*R9_Step_50 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0087},/*      = 135*/
	{MISENSOR_16BIT, 0x98c, 0xa404},/*fd Mode*/
	{MISENSOR_16BIT, 0x990, 0x10},/*      = 16*/
	{MISENSOR_16BIT, 0x98c, 0xa40d},/*Stat_min*/
	{MISENSOR_16BIT, 0x990, 0x02},/*      = 2*/
	{MISENSOR_16BIT, 0x98c, 0xa40e},/*Stat_max*/
	{MISENSOR_16BIT, 0x990, 0x03},/*      = 3*/
	{MISENSOR_16BIT, 0x98c, 0xa410},/*Min_amplitude*/
	{MISENSOR_16BIT, 0x990, 0x0a},/*      = 10*/
	{MISENSOR_TOK_TERM, 0, 0}
};

/*
 * Context A setting for 800x600
; Max Frame Time: 33.3333 msec
; Max Frame Clocks: 1413333.3 clocks (42.400 MHz)
; Pixel Clock: divided by 1
; Skip Mode: 2x cols, 2x rows, Bin Mode: Yes
; Horiz clks:  808 active + 1237 blank = 2045 total
; Vert  rows:  608 active + 83 blank = 691 total
; Extra Delay: 238 clocks
;
; Actual Frame Clocks: 1413333 clocks
; Row Time: 48.231 usec / 2045 clocks
; Frame time: 33.333325 msec
; Frames per Sec: 30 fps
;
; 50Hz Flicker Period: 207.33 lines
; 60Hz Flicker Period: 172.78 lines

; hblank: 29.17us
; vblank: 4ms
 */
static struct misensor_reg const mt9d113_svga_30_init[] = {
	{MISENSOR_16BIT, 0x98c, 0x2703},/*Output Width (a)*/
	{MISENSOR_16BIT, 0x990, 0x0320},/*      = 800*/
	{MISENSOR_16BIT, 0x98c, 0x2705},/*Output Height (a)*/
	{MISENSOR_16BIT, 0x990, 0x0258},/*      = 600*/
	{MISENSOR_16BIT, 0x98c, 0x2707},/*Output Width (b)*/
	{MISENSOR_16BIT, 0x990, 0x0640},/*      = 1600*/
	{MISENSOR_16BIT, 0x98c, 0x2709},/*Output Height (b)*/
	{MISENSOR_16BIT, 0x990, 0x04b0},/*      = 1200*/
	{MISENSOR_16BIT, 0x98c, 0x270d},/*Row Start (a)*/
	{MISENSOR_16BIT, 0x990, 0x000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x270f},/*column Start (a)*/
	{MISENSOR_16BIT, 0x990, 0x000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x2711},/*Row end (a)*/
	{MISENSOR_16BIT, 0x990, 0x04bd},/*      = 1213*/
	{MISENSOR_16BIT, 0x98c, 0x2713},/*column end (a)*/
	{MISENSOR_16BIT, 0x990, 0x064d},/*      = 1613*/
	{MISENSOR_16BIT, 0x98c, 0x2715},/*Row Speed (a)*/
	{MISENSOR_16BIT, 0x990, 0x0111},/*      = 273*/
	{MISENSOR_16BIT, 0x98c, 0x2717},/*Read Mode (a)*/
	{MISENSOR_16BIT, 0x990, 0x046f},/*      = 1135*/
	{MISENSOR_16BIT, 0x98c, 0x2719},/*sensor_fine_correction (a)*/
	{MISENSOR_16BIT, 0x990, 0x005a},/*      = 90*/
	{MISENSOR_16BIT, 0x98c, 0x271b},/*sensor_fine_IT_min (a)*/
	{MISENSOR_16BIT, 0x990, 0x01be},/*      = 446*/
	{MISENSOR_16BIT, 0x98c, 0x271d},/*sensor_fine_IT_max_margin (a)*/
	{MISENSOR_16BIT, 0x990, 0x0131},/*      = 305*/
	{MISENSOR_16BIT, 0x98c, 0x271f},/*frame Lines (a)*/
	{MISENSOR_16BIT, 0x990, 0x02b3},/*      = 691*/
	{MISENSOR_16BIT, 0x98c, 0x2721},/*Line Length (a)*/
	{MISENSOR_16BIT, 0x990, 0x07fd},/*      = 2045*/
	{MISENSOR_16BIT, 0x98c, 0x2723},/*Row Start (b)*/
	{MISENSOR_16BIT, 0x990, 0x0004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x2725},/*column Start (b)*/
	{MISENSOR_16BIT, 0x990, 0x0004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x2727},/*Row end (b)*/
	{MISENSOR_16BIT, 0x990, 0x04bb},/*      = 1211*/
	{MISENSOR_16BIT, 0x98c, 0x2729},/*column end (b)*/
	{MISENSOR_16BIT, 0x990, 0x064b},/*      = 1611*/
	{MISENSOR_16BIT, 0x98c, 0x272b},/*Row Speed (b)*/
	{MISENSOR_16BIT, 0x990, 0x0111},/*      = 273*/
	{MISENSOR_16BIT, 0x98c, 0x272d},/*Read Mode (b)*/
	{MISENSOR_16BIT, 0x990, 0x0024},/*      = 36*/
	{MISENSOR_16BIT, 0x98c, 0x272f},/*sensor_fine_correction (b)*/
	{MISENSOR_16BIT, 0x990, 0x003a},/*      = 58*/
	{MISENSOR_16BIT, 0x98c, 0x2731},/*sensor_fine_IT_min (b)*/
	{MISENSOR_16BIT, 0x990, 0x00f6},/*      = 246*/
	{MISENSOR_16BIT, 0x98c, 0x2733},/*sensor_fine_IT_max_margin (b)*/
	{MISENSOR_16BIT, 0x990, 0x008b},/*      = 139*/
	{MISENSOR_16BIT, 0x98c, 0x2735},/*frame Lines (b)*/
	{MISENSOR_16BIT, 0x990, 0x050d},/*      = 1293*/
	{MISENSOR_16BIT, 0x98c, 0x2737},/*Line Length (b)*/
	{MISENSOR_16BIT, 0x990, 0x0c24},/*      = 3108*/
	{MISENSOR_16BIT, 0x98c, 0x2739},/*crop_X0 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x273b},/*crop_X1 (a)*/
	{MISENSOR_16BIT, 0x990, 0x031f},/*      = 799*/
	{MISENSOR_16BIT, 0x98c, 0x273d},/*crop_Y0 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x273f},/*crop_Y1 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0257},/*      = 599*/
	{MISENSOR_16BIT, 0x98c, 0x2747},/*crop_X0 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x2749},/*crop_X1 (b)*/
	{MISENSOR_16BIT, 0x990, 0x063f},/*      = 1599*/
	{MISENSOR_16BIT, 0x98c, 0x274b},/*crop_Y0 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x274d},/*crop_Y1 (b)*/
	{MISENSOR_16BIT, 0x990, 0x04af},/*      = 1199*/
	{MISENSOR_16BIT, 0x98c, 0x222d},/*R9 Step*/
	{MISENSOR_16BIT, 0x990, 0x00ad},/*      = 173*/
	{MISENSOR_16BIT, 0x98c, 0xa408},/*search_f1_50*/
	{MISENSOR_16BIT, 0x990, 0x2a},/*      = 42*/
	{MISENSOR_16BIT, 0x98c, 0xa409},/*search_f2_50*/
	{MISENSOR_16BIT, 0x990, 0x2c},/*      = 44*/
	{MISENSOR_16BIT, 0x98c, 0xa40a},/*search_f1_60*/
	{MISENSOR_16BIT, 0x990, 0x32},/*      = 50*/
	{MISENSOR_16BIT, 0x98c, 0xa40b},/*search_f2_60*/
	{MISENSOR_16BIT, 0x990, 0x34},/*      = 52*/
	{MISENSOR_16BIT, 0x98c, 0x2411},/*R9_Step_60 (a)*/
	{MISENSOR_16BIT, 0x990, 0x00ad},/*      = 173*/
	{MISENSOR_16BIT, 0x98c, 0x2413},/*R9_Step_50 (a)*/
	{MISENSOR_16BIT, 0x990, 0x00cf},/*      = 207*/
	{MISENSOR_16BIT, 0x98c, 0x2415},/*R9_Step_60 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0071},/*      = 113*/
	{MISENSOR_16BIT, 0x98c, 0x2417},/*R9_Step_50 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0087},/*      = 135*/
	{MISENSOR_16BIT, 0x98c, 0xa404},/*fd Mode*/
	{MISENSOR_16BIT, 0x990, 0x10},/*      = 16*/
	{MISENSOR_16BIT, 0x98c, 0xa40d},/*Stat_min*/
	{MISENSOR_16BIT, 0x990, 0x02},/*      = 2*/
	{MISENSOR_16BIT, 0x98c, 0xa40e},/*Stat_max*/
	{MISENSOR_16BIT, 0x990, 0x03},/*      = 3*/
	{MISENSOR_16BIT, 0x98c, 0xa410},/*Min_amplitude*/
	{MISENSOR_16BIT, 0x990, 0x0a},/*      = 10*/
	{MISENSOR_TOK_TERM, 0, 0}
};

/*
 * Context A setting for 1024x768
; Max Frame Time: 66.8896 msec
; Max Frame Clocks: 2836120.4 clocks (42.400 MHz)
; Pixel Clock: divided by 1
; Skip Mode: 1x cols, 1x rows, Bin Mode: No
; Horiz clks:  1608 active + 585 blank = 2193 total
; Vert  rows:  1208 active + 85 blank = 1293 total
; Extra Delay: 571 clocks
;
; Actual Frame Clocks: 2836120 clocks
; Row Time: 51.722 usec / 2193 clocks
; Frame time: 66.889623 msec
; Frames per Sec: 14.950 fps
;
; 50Hz Flicker Period: 193.34 lines
; 60Hz Flicker Period: 161.12 lines

; hblank: 13.79us
; vblank: 4.4ms
 */
static struct misensor_reg const mt9d113_1m_15_init[] = {
	{MISENSOR_16BIT, 0x98c, 0x2703},/*Output Width (a)*/
	{MISENSOR_16BIT, 0x990, 0x0400},/*      = 1024*/
	{MISENSOR_16BIT, 0x98c, 0x2705},/*Output Height (a)*/
	{MISENSOR_16BIT, 0x990, 0x0300},/*      = 768*/
	{MISENSOR_16BIT, 0x98c, 0x2707},/*Output Width (b)*/
	{MISENSOR_16BIT, 0x990, 0x0640},/*      = 1600*/
	{MISENSOR_16BIT, 0x98c, 0x2709},/*Output Height (b)*/
	{MISENSOR_16BIT, 0x990, 0x04b0},/*      = 1200*/
	{MISENSOR_16BIT, 0x98c, 0x270D},/*Row Start (a)*/
	{MISENSOR_16BIT, 0x990, 0x004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x270f},/*column Start (a)*/
	{MISENSOR_16BIT, 0x990, 0x004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x2711},/*Row end (a)*/
	{MISENSOR_16BIT, 0x990, 0x04bb},/*      = 1211*/
	{MISENSOR_16BIT, 0x98c, 0x2713},/*column end (a)*/
	{MISENSOR_16BIT, 0x990, 0x064b},/*      = 1611*/
	{MISENSOR_16BIT, 0x98c, 0x2715},/*Row Speed (a)*/
	{MISENSOR_16BIT, 0x990, 0x0111},/*      = 273*/
	{MISENSOR_16BIT, 0x98c, 0x2717},/*Read Mode (a)*/
	{MISENSOR_16BIT, 0x990, 0x0027},/*      = 39*/
	{MISENSOR_16BIT, 0x98c, 0x2719},/*sensor_fine_correction (a)*/
	{MISENSOR_16BIT, 0x990, 0x003a},/*      = 58*/
	{MISENSOR_16BIT, 0x98c, 0x271b},/*sensor_fine_IT_min (a)*/
	{MISENSOR_16BIT, 0x990, 0x00f6},/*      = 246*/
	{MISENSOR_16BIT, 0x98c, 0x271D},/*sensor_fine_IT_max_margin (a)*/
	{MISENSOR_16BIT, 0x990, 0x008b},/*      = 139*/
	{MISENSOR_16BIT, 0x98c, 0x271f},/*frame Lines (a)*/
	{MISENSOR_16BIT, 0x990, 0x050d},/*      = 1293*/
	{MISENSOR_16BIT, 0x98c, 0x2721},/*Line Length (a)*/
	{MISENSOR_16BIT, 0x990, 0x0891},/*      = 2193*/
	{MISENSOR_16BIT, 0x98c, 0x2723},/*Row Start (b)*/
	{MISENSOR_16BIT, 0x990, 0x0004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x2725},/*column Start (b)*/
	{MISENSOR_16BIT, 0x990, 0x0004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x2727},/*Row end (b)*/
	{MISENSOR_16BIT, 0x990, 0x04bb},/*      = 1211*/
	{MISENSOR_16BIT, 0x98c, 0x2729},/*column end (b)*/
	{MISENSOR_16BIT, 0x990, 0x064b},/*      = 1611*/
	{MISENSOR_16BIT, 0x98c, 0x272b},/*Row Speed (b)*/
	{MISENSOR_16BIT, 0x990, 0x0111},/*      = 273*/
	{MISENSOR_16BIT, 0x98c, 0x272D},/*Read Mode (b)*/
	{MISENSOR_16BIT, 0x990, 0x0024},/*      = 36*/
	{MISENSOR_16BIT, 0x98c, 0x272f},/*sensor_fine_correction (b)*/
	{MISENSOR_16BIT, 0x990, 0x003a},/*      = 58*/
	{MISENSOR_16BIT, 0x98c, 0x2731},/*sensor_fine_IT_min (b)*/
	{MISENSOR_16BIT, 0x990, 0x00f6},/*      = 246*/
	{MISENSOR_16BIT, 0x98c, 0x2733},/*sensor_fine_IT_max_margin (b)*/
	{MISENSOR_16BIT, 0x990, 0x008b},/*      = 139*/
	{MISENSOR_16BIT, 0x98c, 0x2735},/*frame Lines (b)*/
	{MISENSOR_16BIT, 0x990, 0x050D},/*      = 1293*/
	{MISENSOR_16BIT, 0x98c, 0x2737},/*Line Length (b)*/
	{MISENSOR_16BIT, 0x990, 0x0c24},/*      = 3108*/
	{MISENSOR_16BIT, 0x98c, 0x2739},/*crop_X0 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x273b},/*crop_X1 (a)*/
	{MISENSOR_16BIT, 0x990, 0x063f},/*      = 1599*/
	{MISENSOR_16BIT, 0x98c, 0x273D},/*crop_Y0 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x273f},/*crop_Y1 (a)*/
	{MISENSOR_16BIT, 0x990, 0x04af},/*      = 1199*/
	{MISENSOR_16BIT, 0x98c, 0x2747},/*crop_X0 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x2749},/*crop_X1 (b)*/
	{MISENSOR_16BIT, 0x990, 0x063f},/*      = 1599*/
	{MISENSOR_16BIT, 0x98c, 0x274b},/*crop_Y0 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x274D},/*crop_Y1 (b)*/
	{MISENSOR_16BIT, 0x990, 0x04af},/*      = 1199*/
	{MISENSOR_16BIT, 0x98c, 0x222D},/*R9 Step*/
	{MISENSOR_16BIT, 0x990, 0x00a1},/*      = 161*/
	{MISENSOR_16BIT, 0x98c, 0xa408},/*search_f1_50*/
	{MISENSOR_16BIT, 0x990, 0x27},/*      = 39*/
	{MISENSOR_16BIT, 0x98c, 0xa409},/*search_f2_50*/
	{MISENSOR_16BIT, 0x990, 0x29},/*      = 41*/
	{MISENSOR_16BIT, 0x98c, 0xa40a},/*search_f1_60*/
	{MISENSOR_16BIT, 0x990, 0x2f},/*      = 47*/
	{MISENSOR_16BIT, 0x98c, 0xa40b},/*search_f2_60*/
	{MISENSOR_16BIT, 0x990, 0x31},/*      = 49*/
	{MISENSOR_16BIT, 0x98c, 0x2411},/*R9_Step_60 (a)*/
	{MISENSOR_16BIT, 0x990, 0x00a1},/*      = 161*/
	{MISENSOR_16BIT, 0x98c, 0x2413},/*R9_Step_50 (a)*/
	{MISENSOR_16BIT, 0x990, 0x00c1},/*      = 193*/
	{MISENSOR_16BIT, 0x98c, 0x2415},/*R9_Step_60 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0071},/*      = 113*/
	{MISENSOR_16BIT, 0x98c, 0x2417},/*R9_Step_50 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0087},/*      = 135*/
	{MISENSOR_16BIT, 0x98c, 0xa404},/*fD Mode*/
	{MISENSOR_16BIT, 0x990, 0x10},/*      = 16*/
	{MISENSOR_16BIT, 0x98c, 0xa40D},/*Stat_min*/
	{MISENSOR_16BIT, 0x990, 0x02},/*      = 2*/
	{MISENSOR_16BIT, 0x98c, 0xa40e},/*Stat_max*/
	{MISENSOR_16BIT, 0x990, 0x03},/*      = 3*/
	{MISENSOR_16BIT, 0x98c, 0xa410},/*Min_amplitude*/
	{MISENSOR_16BIT, 0x990, 0x0a},/*      = 10*/
	{MISENSOR_TOK_TERM, 0, 0}
};

/*
 * Context A setting for 1600x1200
; Max Frame Time: 66.8896 msec
; Max Frame Clocks: 2836120.4 clocks (42.400 MHz)
; Pixel Clock: divided by 1
; Skip Mode: 1x cols, 1x rows, Bin Mode: No
; Horiz clks:  1608 active + 585 blank = 2193 total
; Vert  rows:  1208 active + 85 blank = 1293 total
; Extra Delay: 571 clocks
;
; Actual Frame Clocks: 2836120 clocks
; Row Time: 51.722 usec / 2193 clocks
; Frame time: 66.889623 msec
; Frames per Sec: 14.950 fps
;
; 50Hz Flicker Period: 193.34 lines
; 60Hz Flicker Period: 161.12 lines

; hblank: 13.79us
; vblank: 4.4ms
 */
static struct misensor_reg const mt9d113_2m_15_init[] = {
	{MISENSOR_16BIT, 0x98c, 0x2703},/*Output Width (a)*/
	{MISENSOR_16BIT, 0x990, 0x0640},/*      = 1600*/
	{MISENSOR_16BIT, 0x98c, 0x2705},/*Output Height (a)*/
	{MISENSOR_16BIT, 0x990, 0x04b0},/*      = 1200*/
	{MISENSOR_16BIT, 0x98c, 0x2707},/*Output Width (b)*/
	{MISENSOR_16BIT, 0x990, 0x0640},/*      = 1600*/
	{MISENSOR_16BIT, 0x98c, 0x2709},/*Output Height (b)*/
	{MISENSOR_16BIT, 0x990, 0x04b0},/*      = 1200*/
	{MISENSOR_16BIT, 0x98c, 0x270D},/*Row Start (a)*/
	{MISENSOR_16BIT, 0x990, 0x004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x270f},/*column Start (a)*/
	{MISENSOR_16BIT, 0x990, 0x004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x2711},/*Row end (a)*/
	{MISENSOR_16BIT, 0x990, 0x04bb},/*      = 1211*/
	{MISENSOR_16BIT, 0x98c, 0x2713},/*column end (a)*/
	{MISENSOR_16BIT, 0x990, 0x064b},/*      = 1611*/
	{MISENSOR_16BIT, 0x98c, 0x2715},/*Row Speed (a)*/
	{MISENSOR_16BIT, 0x990, 0x0111},/*      = 273*/
	{MISENSOR_16BIT, 0x98c, 0x2717},/*Read Mode (a)*/
	{MISENSOR_16BIT, 0x990, 0x0027},/*      = 39*/
	{MISENSOR_16BIT, 0x98c, 0x2719},/*sensor_fine_correction (a)*/
	{MISENSOR_16BIT, 0x990, 0x003a},/*      = 58*/
	{MISENSOR_16BIT, 0x98c, 0x271b},/*sensor_fine_IT_min (a)*/
	{MISENSOR_16BIT, 0x990, 0x00f6},/*      = 246*/
	{MISENSOR_16BIT, 0x98c, 0x271D},/*sensor_fine_IT_max_margin (a)*/
	{MISENSOR_16BIT, 0x990, 0x008b},/*      = 139*/
	{MISENSOR_16BIT, 0x98c, 0x271f},/*frame Lines (a)*/
	{MISENSOR_16BIT, 0x990, 0x050d},/*      = 1293*/
	{MISENSOR_16BIT, 0x98c, 0x2721},/*Line Length (a)*/
	{MISENSOR_16BIT, 0x990, 0x0891},/*      = 2193*/
	{MISENSOR_16BIT, 0x98c, 0x2723},/*Row Start (b)*/
	{MISENSOR_16BIT, 0x990, 0x0004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x2725},/*column Start (b)*/
	{MISENSOR_16BIT, 0x990, 0x0004},/*      = 4*/
	{MISENSOR_16BIT, 0x98c, 0x2727},/*Row end (b)*/
	{MISENSOR_16BIT, 0x990, 0x04bb},/*      = 1211*/
	{MISENSOR_16BIT, 0x98c, 0x2729},/*column end (b)*/
	{MISENSOR_16BIT, 0x990, 0x064b},/*      = 1611*/
	{MISENSOR_16BIT, 0x98c, 0x272b},/*Row Speed (b)*/
	{MISENSOR_16BIT, 0x990, 0x0111},/*      = 273*/
	{MISENSOR_16BIT, 0x98c, 0x272D},/*Read Mode (b)*/
	{MISENSOR_16BIT, 0x990, 0x0024},/*      = 36*/
	{MISENSOR_16BIT, 0x98c, 0x272f},/*sensor_fine_correction (b)*/
	{MISENSOR_16BIT, 0x990, 0x003a},/*      = 58*/
	{MISENSOR_16BIT, 0x98c, 0x2731},/*sensor_fine_IT_min (b)*/
	{MISENSOR_16BIT, 0x990, 0x00f6},/*      = 246*/
	{MISENSOR_16BIT, 0x98c, 0x2733},/*sensor_fine_IT_max_margin (b)*/
	{MISENSOR_16BIT, 0x990, 0x008b},/*      = 139*/
	{MISENSOR_16BIT, 0x98c, 0x2735},/*frame Lines (b)*/
	{MISENSOR_16BIT, 0x990, 0x050D},/*      = 1293*/
	{MISENSOR_16BIT, 0x98c, 0x2737},/*Line Length (b)*/
	{MISENSOR_16BIT, 0x990, 0x0c24},/*      = 3108*/
	{MISENSOR_16BIT, 0x98c, 0x2739},/*crop_X0 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x273b},/*crop_X1 (a)*/
	{MISENSOR_16BIT, 0x990, 0x063f},/*      = 1599*/
	{MISENSOR_16BIT, 0x98c, 0x273D},/*crop_Y0 (a)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x273f},/*crop_Y1 (a)*/
	{MISENSOR_16BIT, 0x990, 0x04af},/*      = 1199*/
	{MISENSOR_16BIT, 0x98c, 0x2747},/*crop_X0 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x2749},/*crop_X1 (b)*/
	{MISENSOR_16BIT, 0x990, 0x063f},/*      = 1599*/
	{MISENSOR_16BIT, 0x98c, 0x274b},/*crop_Y0 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0000},/*      = 0*/
	{MISENSOR_16BIT, 0x98c, 0x274D},/*crop_Y1 (b)*/
	{MISENSOR_16BIT, 0x990, 0x04af},/*      = 1199*/
	{MISENSOR_16BIT, 0x98c, 0x222D},/*R9 Step*/
	{MISENSOR_16BIT, 0x990, 0x00a1},/*      = 161*/
	{MISENSOR_16BIT, 0x98c, 0xa408},/*search_f1_50*/
	{MISENSOR_16BIT, 0x990, 0x27},/*      = 39*/
	{MISENSOR_16BIT, 0x98c, 0xa409},/*search_f2_50*/
	{MISENSOR_16BIT, 0x990, 0x29},/*      = 41*/
	{MISENSOR_16BIT, 0x98c, 0xa40a},/*search_f1_60*/
	{MISENSOR_16BIT, 0x990, 0x2f},/*      = 47*/
	{MISENSOR_16BIT, 0x98c, 0xa40b},/*search_f2_60*/
	{MISENSOR_16BIT, 0x990, 0x31},/*      = 49*/
	{MISENSOR_16BIT, 0x98c, 0x2411},/*R9_Step_60 (a)*/
	{MISENSOR_16BIT, 0x990, 0x00a1},/*      = 161*/
	{MISENSOR_16BIT, 0x98c, 0x2413},/*R9_Step_50 (a)*/
	{MISENSOR_16BIT, 0x990, 0x00c1},/*      = 193*/
	{MISENSOR_16BIT, 0x98c, 0x2415},/*R9_Step_60 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0071},/*      = 113*/
	{MISENSOR_16BIT, 0x98c, 0x2417},/*R9_Step_50 (b)*/
	{MISENSOR_16BIT, 0x990, 0x0087},/*      = 135*/
	{MISENSOR_16BIT, 0x98c, 0xa404},/*fD Mode*/
	{MISENSOR_16BIT, 0x990, 0x10},/*      = 16*/
	{MISENSOR_16BIT, 0x98c, 0xa40D},/*Stat_min*/
	{MISENSOR_16BIT, 0x990, 0x02},/*      = 2*/
	{MISENSOR_16BIT, 0x98c, 0xa40e},/*Stat_max*/
	{MISENSOR_16BIT, 0x990, 0x03},/*      = 3*/
	{MISENSOR_16BIT, 0x98c, 0xa410},/*Min_amplitude*/
	{MISENSOR_16BIT, 0x990, 0x0a},/*      = 10*/
	{MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const mt9d113_default_gain[] = {
	{MISENSOR_16BIT, 0x098c, 0x221f}, /* ae_d_gain */
	{MISENSOR_16BIT, 0x0990, 0x80},
	{MISENSOR_TOK_TERM, 0, 0}
};

/*
 * Soft Reset
 * 1: Set SYSCTL 0x001A[1:0] to 0x3 to initiate internal reset cycle.
 * 2: Wait 6000 EXTCLK cycles.
 * 3: Reset SYSCTL 0x001A[1:0] to 0x0 for normal operation.
 *
 * SYSCTL
 * bit9=0: Parallel output port is disabled.
 * bit8=0: Output is enabled (gpio ?
 * bit6=1: running at full speed
 * bit4=1: GPIO not remained power on in standby
 * bit3=0: MIPI Transmitter disabled by default
 * bit1=0: MIPI Transmitter not in reset
 * bit0: toggle for SOC soft reset
 */
static struct misensor_reg const mt9d113_reset[] = {
	{MISENSOR_16BIT, 0x001a, 0x0051},
	{MISENSOR_TOK_DELAY, 0, 1},
	{MISENSOR_16BIT, 0x001a, 0x0050},
	{MISENSOR_TOK_DELAY, 0, 1}, /* wait for normal operation */
	{MISENSOR_TOK_TERM, 0, 0}
};

/* AWB_CCM initialization */
static struct misensor_reg const mt9d113_awb_ccm[] = {
	{MISENSOR_16BIT, 0x098c, 0x2306}, /* MCU_ADDRESS [AWB_CCM_L_0]*/
	{MISENSOR_16BIT, 0x0990, 0x02a2}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x231c}, /* MCU_ADDRESS [AWB_CCM_RL_0]*/
	{MISENSOR_16BIT, 0x0990, 0x00b3}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x2308}, /* MCU_ADDRESS [AWB_CCM_L_1]*/
	{MISENSOR_16BIT, 0x0990, 0xfef7}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x231e}, /* MCU_ADDRESS [AWB_CCM_RL_1]*/
	{MISENSOR_16BIT, 0x0990, 0xfefb}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x230a}, /* MCU_ADDRESS [AWB_CCM_L_2]*/
	{MISENSOR_16BIT, 0x0990, 0xffa1}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x2320}, /* MCU_ADDRESS [AWB_CCM_RL_2]*/
	{MISENSOR_16BIT, 0x0990, 0x0057}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x230c}, /* MCU_ADDRESS [AWB_CCM_L_3]*/
	{MISENSOR_16BIT, 0x0990, 0xff38}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x2322}, /* MCU_ADDRESS [AWB_CCM_RL_3]*/
	{MISENSOR_16BIT, 0x0990, 0x0046}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x230e}, /* MCU_ADDRESS [AWB_CCM_L_4]*/
	{MISENSOR_16BIT, 0x0990, 0x02e1}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x2324}, /* MCU_ADDRESS [AWB_CCM_RL_4]*/
	{MISENSOR_16BIT, 0x0990, 0xffe1}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x2310}, /* MCU_ADDRESS [AWB_CCM_L_5]*/
	{MISENSOR_16BIT, 0x0990, 0xff3d}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x2326}, /* MCU_ADDRESS [AWB_CCM_RL_5]*/
	{MISENSOR_16BIT, 0x0990, 0xffc0}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x2312}, /* MCU_ADDRESS [AWB_CCM_L_6]*/
	{MISENSOR_16BIT, 0x0990, 0xffbd}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x2328}, /* MCU_ADDRESS [AWB_CCM_RL_6]*/
	{MISENSOR_16BIT, 0x0990, 0x001a}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x2314}, /* MCU_ADDRESS [AWB_CCM_L_7]*/
	{MISENSOR_16BIT, 0x0990, 0xfe81}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x232a}, /* MCU_ADDRESS [AWB_CCM_RL_7]*/
	{MISENSOR_16BIT, 0x0990, 0x0091}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x2316}, /* MCU_ADDRESS [AWB_CCM_L_8]*/
	{MISENSOR_16BIT, 0x0990, 0x0307}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x232c}, /* MCU_ADDRESS [AWB_CCM_RL_8]*/
	{MISENSOR_16BIT, 0x0990, 0xff48}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x2318}, /* MCU_ADDRESS [AWB_CCM_L_9]*/
	{MISENSOR_16BIT, 0x0990, 0x0020}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x231a}, /* MCU_ADDRESS [AWB_CCM_L_10]*/
	{MISENSOR_16BIT, 0x0990, 0x0033}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x232e}, /* MCU_ADDRESS [AWB_CCM_RL_9]*/
	{MISENSOR_16BIT, 0x0990, 0x0008}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x2330}, /* MCU_ADDRESS [AWB_CCM_RL_10]*/
	{MISENSOR_16BIT, 0x0990, 0xfff7}, /* MCU_DATA_0*/

	{MISENSOR_16BIT, 0x098c, 0xa363}, /* MCU_ADDRESS [aWb_TG_MIN0]*/
	{MISENSOR_16BIT, 0x0990, 0x00d2}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0xa364}, /* MCU_ADDRESS [aWb_TG_MaX0]*/
	{MISENSOR_16BIT, 0x0990, 0x00ee}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0xa369}, /* MCU_ADDRESS [AWB_KR_R] */
	{MISENSOR_16BIT, 0x0990, 0x0080}, /* MCU_DATA_0 */
	{MISENSOR_16BIT, 0x098c, 0xa36a}, /* MCU_ADDRESS [AWB_KG_R] */
	{MISENSOR_16BIT, 0x0990, 0x0090}, /* MCU_DATA_0 */
	{MISENSOR_16BIT, 0x098c, 0xa36b}, /* MCU_ADDRESS [AWB_KB_R] */
	{MISENSOR_16BIT, 0x0990, 0x0088}, /* MCU_DATA_0 */


	/* NO dS, aptinal private */
	{MISENSOR_16BIT, 0x3244, 0x0328}, /* aWb_CONFIG4, aWb fine tuning*/
	{MISENSOR_16BIT, 0x323e, 0xc22c}, /* aWb fine tuning, bit[11-15]*/
	{MISENSOR_TOK_TERM, 0, 0}
};

/* Low light settings */
static struct misensor_reg const mt9d113_lowlight[] = {
	{MISENSOR_16BIT, 0x098c, 0x2b28}, /* MCU_ADDRESS [hg_ll_bg_start]*/
	{MISENSOR_16BIT, 0x0990, 0x35e8}, /* 13800*/
	{MISENSOR_16BIT, 0x098c, 0x2b2a}, /* MCU_ADDRESS [hg_ll_bg_stop]*/
	{MISENSOR_16BIT, 0x0990, 0xb3b0}, /* 46000*/

	{MISENSOR_16BIT, 0x098c, 0xab20}, /* MCU_ADDRESS [hg_ll_sat1]*/
	{MISENSOR_16BIT, 0x0990, 0x004b}, /* 75*/
	{MISENSOR_16BIT, 0x098c, 0xab24}, /* MCU_ADDRESS [hg_ll_sat2]*/
	{MISENSOR_16BIT, 0x0990, 0x0000}, /* 0*/
	{MISENSOR_16BIT, 0x098c, 0xab25}, /* MCU_ADDRESS [hg_ll_thresh2]*/
	{MISENSOR_16BIT, 0x0990, 0x00ff}, /* 255*/

	{MISENSOR_16BIT, 0x098c, 0xab30}, /* MCU_ADDRESS [hg_nr_stop_r]*/
	{MISENSOR_16BIT, 0x0990, 0x00ff}, /* 255*/
	{MISENSOR_16BIT, 0x098c, 0xab31}, /* MCU_ADDRESS [hg_nr_stop_g]*/
	{MISENSOR_16BIT, 0x0990, 0x00ff}, /* 255*/
	{MISENSOR_16BIT, 0x098c, 0xab32}, /* MCU_ADDRESS [hg_nr_stop_b]*/
	{MISENSOR_16BIT, 0x0990, 0x00ff}, /* 255*/
	{MISENSOR_16BIT, 0x098c, 0xab33}, /* MCU_ADDRESS [hg_nr_stop_ol]*/
	{MISENSOR_16BIT, 0x0990, 0x0057}, /* 87*/

	{MISENSOR_16BIT, 0x098c, 0xab34}, /* MCU_ADDRESS [hg_nr_gainstart]*/
	{MISENSOR_16BIT, 0x0990, 0x0080}, /* 128*/
	{MISENSOR_16BIT, 0x098c, 0xab35}, /* MCU_ADDRESS [hg_nr_gainstop]*/
	{MISENSOR_16BIT, 0x0990, 0x00ff}, /* 255*/

	{MISENSOR_16BIT, 0x098c, 0xab36}, /* MCU_ADDRESS [hg_cluster_dc_th]*/
	{MISENSOR_16BIT, 0x0990, 0x0014}, /* 20*/

	{MISENSOR_16BIT, 0x098c, 0xab37}, /* MCU_ADDRESS [hg_gamma_mor_ctrl]*/
	{MISENSOR_16BIT, 0x0990, 0x0003}, /* 3*/

	{MISENSOR_16BIT, 0x098c, 0x2b38}, /* MCU_ADDRESS [hg_gammastartmor]*/
	{MISENSOR_16BIT, 0x0990, 0x32c8}, /* 13000 [100 lux]*/
	{MISENSOR_16BIT, 0x098c, 0x2b3a}, /* MCU_ADDRESS [hg_gammastopmor]*/
	{MISENSOR_16BIT, 0x0990, 0x7918}, /* 31000 [20 lux]*/

	{MISENSOR_16BIT, 0x098c, 0x2b62}, /* MCU_ADDRESS [hg_ftb_start_bm]*/
	{MISENSOR_16BIT, 0x0990, 0xfffe}, /* Disable FTB*/
	{MISENSOR_16BIT, 0x098c, 0x2b64}, /* MCU_ADDRESS [hg_ftb_stop_bm]*/
	{MISENSOR_16BIT, 0x0990, 0xffff}, /* Disable FTB*/

	{MISENSOR_TOK_TERM, 0, 0}
};

/* SOC2030 patch */
static struct misensor_reg const patch_soc2030[] = {
	{MISENSOR_16BIT, 0x98c, 0x415},
	{MISENSOR_16BIT, 0x990, 0xf601},
	{MISENSOR_16BIT, 0x992, 0x42c1},
	{MISENSOR_16BIT, 0x994, 0x326},
	{MISENSOR_16BIT, 0x996, 0x11f6},
	{MISENSOR_16BIT, 0x998, 0x143},
	{MISENSOR_16BIT, 0x99a, 0xc104},
	{MISENSOR_16BIT, 0x99c, 0x260a},
	{MISENSOR_16BIT, 0x99e, 0xcc04},

	{MISENSOR_16BIT, 0x98c, 0x425},
	{MISENSOR_16BIT, 0x990, 0x33bd},
	{MISENSOR_16BIT, 0x992, 0xa362},
	{MISENSOR_16BIT, 0x994, 0xbd04},
	{MISENSOR_16BIT, 0x996, 0x3339},
	{MISENSOR_16BIT, 0x998, 0xc6ff},
	{MISENSOR_16BIT, 0x99a, 0xf701},
	{MISENSOR_16BIT, 0x99c, 0x6439},
	{MISENSOR_16BIT, 0x99e, 0xde5d},

	{MISENSOR_16BIT, 0x98c, 0x435},
	{MISENSOR_16BIT, 0x990, 0x18ce},
	{MISENSOR_16BIT, 0x992, 0x325},
	{MISENSOR_16BIT, 0x994, 0xcc00},
	{MISENSOR_16BIT, 0x996, 0x27bd},
	{MISENSOR_16BIT, 0x998, 0xc2b8},
	{MISENSOR_16BIT, 0x99a, 0xcc04},
	{MISENSOR_16BIT, 0x99c, 0xbdfd},
	{MISENSOR_16BIT, 0x99e, 0x33b},

	{MISENSOR_16BIT, 0x98c, 0x445},
	{MISENSOR_16BIT, 0x990, 0xcc06},
	{MISENSOR_16BIT, 0x992, 0x6bfd},
	{MISENSOR_16BIT, 0x994, 0x32f},
	{MISENSOR_16BIT, 0x996, 0xcc03},
	{MISENSOR_16BIT, 0x998, 0x25dd},
	{MISENSOR_16BIT, 0x99a, 0x5dc6},
	{MISENSOR_16BIT, 0x99c, 0x1ed7},
	{MISENSOR_16BIT, 0x99e, 0x6cd7},

	{MISENSOR_16BIT, 0x98c, 0x455},
	{MISENSOR_16BIT, 0x990, 0x6d5f},
	{MISENSOR_16BIT, 0x992, 0xd76e},
	{MISENSOR_16BIT, 0x994, 0xd78d},
	{MISENSOR_16BIT, 0x996, 0x8620},
	{MISENSOR_16BIT, 0x998, 0x977a},
	{MISENSOR_16BIT, 0x99a, 0xd77b},
	{MISENSOR_16BIT, 0x99c, 0x979a},
	{MISENSOR_16BIT, 0x99e, 0xc621},

	{MISENSOR_16BIT, 0x98c, 0x465},
	{MISENSOR_16BIT, 0x990, 0xd79b},
	{MISENSOR_16BIT, 0x992, 0xfe01},
	{MISENSOR_16BIT, 0x994, 0x6918},
	{MISENSOR_16BIT, 0x996, 0xce03},
	{MISENSOR_16BIT, 0x998, 0x4dcc},
	{MISENSOR_16BIT, 0x99a, 0x13},
	{MISENSOR_16BIT, 0x99c, 0xbdc2},
	{MISENSOR_16BIT, 0x99e, 0xb8cc},

	{MISENSOR_16BIT, 0x98c, 0x475},
	{MISENSOR_16BIT, 0x990, 0x5e9},
	{MISENSOR_16BIT, 0x992, 0xfd03},
	{MISENSOR_16BIT, 0x994, 0x4fcc},
	{MISENSOR_16BIT, 0x996, 0x34d},
	{MISENSOR_16BIT, 0x998, 0xfd01},
	{MISENSOR_16BIT, 0x99a, 0x69fe},
	{MISENSOR_16BIT, 0x99c, 0x2bd},
	{MISENSOR_16BIT, 0x99e, 0x18ce},

	{MISENSOR_16BIT, 0x98c, 0x485},
	{MISENSOR_16BIT, 0x990, 0x361},
	{MISENSOR_16BIT, 0x992, 0xcc00},
	{MISENSOR_16BIT, 0x994, 0x11bd},
	{MISENSOR_16BIT, 0x996, 0xc2b8},
	{MISENSOR_16BIT, 0x998, 0xcc06},
	{MISENSOR_16BIT, 0x99a, 0x28fd},
	{MISENSOR_16BIT, 0x99c, 0x36f},
	{MISENSOR_16BIT, 0x99e, 0xcc03},

	{MISENSOR_16BIT, 0x98c, 0x495},
	{MISENSOR_16BIT, 0x990, 0x61fd},
	{MISENSOR_16BIT, 0x992, 0x2bd},
	{MISENSOR_16BIT, 0x994, 0xde00},
	{MISENSOR_16BIT, 0x996, 0x18ce},
	{MISENSOR_16BIT, 0x998, 0xc2},
	{MISENSOR_16BIT, 0x99a, 0xcc00},
	{MISENSOR_16BIT, 0x99c, 0x37bd},
	{MISENSOR_16BIT, 0x99e, 0xc2b8},

	{MISENSOR_16BIT, 0x98c, 0x4a5},
	{MISENSOR_16BIT, 0x990, 0xcc06},
	{MISENSOR_16BIT, 0x992, 0x4fdd},
	{MISENSOR_16BIT, 0x994, 0xe6cc},
	{MISENSOR_16BIT, 0x996, 0xc2},
	{MISENSOR_16BIT, 0x998, 0xdd00},
	{MISENSOR_16BIT, 0x99a, 0xc601},
	{MISENSOR_16BIT, 0x99c, 0xf701},
	{MISENSOR_16BIT, 0x99e, 0x64c6},

	{MISENSOR_16BIT, 0x98c, 0x4b5},
	{MISENSOR_16BIT, 0x990, 0x5f7},
	{MISENSOR_16BIT, 0x992, 0x165},
	{MISENSOR_16BIT, 0x994, 0x7f01},
	{MISENSOR_16BIT, 0x996, 0x6639},
	{MISENSOR_16BIT, 0x998, 0x373c},
	{MISENSOR_16BIT, 0x99a, 0x3c3c},
	{MISENSOR_16BIT, 0x99c, 0x3c3c},
	{MISENSOR_16BIT, 0x99e, 0x30ec},

	{MISENSOR_16BIT, 0x98c, 0x4c5},
	{MISENSOR_16BIT, 0x990, 0x11ed},
	{MISENSOR_16BIT, 0x992, 0x2ec},
	{MISENSOR_16BIT, 0x994, 0xfed},
	{MISENSOR_16BIT, 0x996, 0x8f},
	{MISENSOR_16BIT, 0x998, 0x30ed},
	{MISENSOR_16BIT, 0x99a, 0x4ec},
	{MISENSOR_16BIT, 0x99c, 0xdee},
	{MISENSOR_16BIT, 0x99e, 0x4bd},

	{MISENSOR_16BIT, 0x98c, 0x4d5},
	{MISENSOR_16BIT, 0x990, 0xa406},
	{MISENSOR_16BIT, 0x992, 0x30ec},
	{MISENSOR_16BIT, 0x994, 0x2ed},
	{MISENSOR_16BIT, 0x996, 0x6fc},
	{MISENSOR_16BIT, 0x998, 0x10c0},
	{MISENSOR_16BIT, 0x99a, 0x2705},
	{MISENSOR_16BIT, 0x99c, 0xccff},
	{MISENSOR_16BIT, 0x99e, 0xffed},

	{MISENSOR_16BIT, 0x98c, 0x4e5},
	{MISENSOR_16BIT, 0x990, 0x6f6},
	{MISENSOR_16BIT, 0x992, 0x256},
	{MISENSOR_16BIT, 0x994, 0x8616},
	{MISENSOR_16BIT, 0x996, 0x3dc3},
	{MISENSOR_16BIT, 0x998, 0x261},
	{MISENSOR_16BIT, 0x99a, 0x8fe6},
	{MISENSOR_16BIT, 0x99c, 0x9c4},
	{MISENSOR_16BIT, 0x99e, 0x7c1},

	{MISENSOR_16BIT, 0x98c, 0x4f5},
	{MISENSOR_16BIT, 0x990, 0x226},
	{MISENSOR_16BIT, 0x992, 0x1dfc},
	{MISENSOR_16BIT, 0x994, 0x10c2},
	{MISENSOR_16BIT, 0x996, 0x30ed},
	{MISENSOR_16BIT, 0x998, 0x2fc},
	{MISENSOR_16BIT, 0x99a, 0x10c0},
	{MISENSOR_16BIT, 0x99c, 0xed00},
	{MISENSOR_16BIT, 0x99e, 0xc602},

	{MISENSOR_16BIT, 0x98c, 0x505},
	{MISENSOR_16BIT, 0x990, 0xbdc2},
	{MISENSOR_16BIT, 0x992, 0x5330},
	{MISENSOR_16BIT, 0x994, 0xec00},
	{MISENSOR_16BIT, 0x996, 0xfd10},
	{MISENSOR_16BIT, 0x998, 0xc0ec},
	{MISENSOR_16BIT, 0x99a, 0x2fd},
	{MISENSOR_16BIT, 0x99c, 0x10c2},
	{MISENSOR_16BIT, 0x99e, 0x201b},

	{MISENSOR_16BIT, 0x98c, 0x515},
	{MISENSOR_16BIT, 0x990, 0xfc10},
	{MISENSOR_16BIT, 0x992, 0xc230},
	{MISENSOR_16BIT, 0x994, 0xed02},
	{MISENSOR_16BIT, 0x996, 0xfc10},
	{MISENSOR_16BIT, 0x998, 0xc0ed},
	{MISENSOR_16BIT, 0x99a, 0xc6},
	{MISENSOR_16BIT, 0x99c, 0x1bd},
	{MISENSOR_16BIT, 0x99e, 0xc253},

	{MISENSOR_16BIT, 0x98c, 0x525},
	{MISENSOR_16BIT, 0x990, 0x30ec},
	{MISENSOR_16BIT, 0x992, 0xfd},
	{MISENSOR_16BIT, 0x994, 0x10c0},
	{MISENSOR_16BIT, 0x996, 0xec02},
	{MISENSOR_16BIT, 0x998, 0xfd10},
	{MISENSOR_16BIT, 0x99a, 0xc2c6},
	{MISENSOR_16BIT, 0x99c, 0x80d7},
	{MISENSOR_16BIT, 0x99e, 0x85c6},

	{MISENSOR_16BIT, 0x98c, 0x535},
	{MISENSOR_16BIT, 0x990, 0x40f7},
	{MISENSOR_16BIT, 0x992, 0x10c4},
	{MISENSOR_16BIT, 0x994, 0xf602},
	{MISENSOR_16BIT, 0x996, 0x5686},
	{MISENSOR_16BIT, 0x998, 0x163d},
	{MISENSOR_16BIT, 0x99a, 0xc302},
	{MISENSOR_16BIT, 0x99c, 0x618f},
	{MISENSOR_16BIT, 0x99e, 0xec14},

	{MISENSOR_16BIT, 0x98c, 0x545},
	{MISENSOR_16BIT, 0x990, 0xfd10},
	{MISENSOR_16BIT, 0x992, 0xc501},
	{MISENSOR_16BIT, 0x994, 0x101},
	{MISENSOR_16BIT, 0x996, 0x101},
	{MISENSOR_16BIT, 0x998, 0xfc10},
	{MISENSOR_16BIT, 0x99a, 0xc2dd},
	{MISENSOR_16BIT, 0x99c, 0x7ffc},
	{MISENSOR_16BIT, 0x99e, 0x10c7},

	{MISENSOR_16BIT, 0x98c, 0x555},
	{MISENSOR_16BIT, 0x990, 0xdd76},
	{MISENSOR_16BIT, 0x992, 0xf602},
	{MISENSOR_16BIT, 0x994, 0x5686},
	{MISENSOR_16BIT, 0x996, 0x163d},
	{MISENSOR_16BIT, 0x998, 0xc302},
	{MISENSOR_16BIT, 0x99a, 0x618f},
	{MISENSOR_16BIT, 0x99c, 0xec14},
	{MISENSOR_16BIT, 0x99e, 0x939f},

	{MISENSOR_16BIT, 0x98c, 0x565},
	{MISENSOR_16BIT, 0x990, 0x30ed},
	{MISENSOR_16BIT, 0x992, 0x8dc},
	{MISENSOR_16BIT, 0x994, 0x7693},
	{MISENSOR_16BIT, 0x996, 0x9d25},
	{MISENSOR_16BIT, 0x998, 0x8f6},
	{MISENSOR_16BIT, 0x99a, 0x2bc},
	{MISENSOR_16BIT, 0x99c, 0x4f93},
	{MISENSOR_16BIT, 0x99e, 0x7f23},

	{MISENSOR_16BIT, 0x98c, 0x575},
	{MISENSOR_16BIT, 0x990, 0x3df6},
	{MISENSOR_16BIT, 0x992, 0x2bc},
	{MISENSOR_16BIT, 0x994, 0x4f93},
	{MISENSOR_16BIT, 0x996, 0x7f23},
	{MISENSOR_16BIT, 0x998, 0x6f6},
	{MISENSOR_16BIT, 0x99a, 0x2bc},
	{MISENSOR_16BIT, 0x99c, 0x4fdd},
	{MISENSOR_16BIT, 0x99e, 0x7fdc},

	{MISENSOR_16BIT, 0x98c, 0x585},
	{MISENSOR_16BIT, 0x990, 0x9ddd},
	{MISENSOR_16BIT, 0x992, 0x76f6},
	{MISENSOR_16BIT, 0x994, 0x2bc},
	{MISENSOR_16BIT, 0x996, 0x4f93},
	{MISENSOR_16BIT, 0x998, 0x7f26},
	{MISENSOR_16BIT, 0x99a, 0xfe6},
	{MISENSOR_16BIT, 0x99c, 0xac1},
	{MISENSOR_16BIT, 0x99e, 0x226},

	{MISENSOR_16BIT, 0x98c, 0x595},
	{MISENSOR_16BIT, 0x990, 0x9d6},
	{MISENSOR_16BIT, 0x992, 0x85c1},
	{MISENSOR_16BIT, 0x994, 0x8026},
	{MISENSOR_16BIT, 0x996, 0x314},
	{MISENSOR_16BIT, 0x998, 0x7401},
	{MISENSOR_16BIT, 0x99a, 0xf602},
	{MISENSOR_16BIT, 0x99c, 0xbc4f},
	{MISENSOR_16BIT, 0x99e, 0x937f},

	{MISENSOR_16BIT, 0x98c, 0x5a5},
	{MISENSOR_16BIT, 0x990, 0x2416},
	{MISENSOR_16BIT, 0x992, 0xde7f},
	{MISENSOR_16BIT, 0x994, 0x9df},
	{MISENSOR_16BIT, 0x996, 0x7f30},
	{MISENSOR_16BIT, 0x998, 0xec08},
	{MISENSOR_16BIT, 0x99a, 0xdd76},
	{MISENSOR_16BIT, 0x99c, 0x200a},
	{MISENSOR_16BIT, 0x99e, 0xdc76},

	{MISENSOR_16BIT, 0x98c, 0x5b5},
	{MISENSOR_16BIT, 0x990, 0xa308},
	{MISENSOR_16BIT, 0x992, 0x2304},
	{MISENSOR_16BIT, 0x994, 0xec08},
	{MISENSOR_16BIT, 0x996, 0xdd76},
	{MISENSOR_16BIT, 0x998, 0x1274},
	{MISENSOR_16BIT, 0x99a, 0x122},
	{MISENSOR_16BIT, 0x99c, 0xde5d},
	{MISENSOR_16BIT, 0x99e, 0xee14},

	{MISENSOR_16BIT, 0x98c, 0x5c5},
	{MISENSOR_16BIT, 0x990, 0xad00},
	{MISENSOR_16BIT, 0x992, 0x30ed},
	{MISENSOR_16BIT, 0x994, 0x11ec},
	{MISENSOR_16BIT, 0x996, 0x6ed},
	{MISENSOR_16BIT, 0x998, 0x2cc},
	{MISENSOR_16BIT, 0x99a, 0x80},
	{MISENSOR_16BIT, 0x99c, 0xed00},
	{MISENSOR_16BIT, 0x99e, 0x8f30},

	{MISENSOR_16BIT, 0x98c, 0x5d5},
	{MISENSOR_16BIT, 0x990, 0xed04},
	{MISENSOR_16BIT, 0x992, 0xec11},
	{MISENSOR_16BIT, 0x994, 0xee04},
	{MISENSOR_16BIT, 0x996, 0xbda4},
	{MISENSOR_16BIT, 0x998, 0x630},
	{MISENSOR_16BIT, 0x99a, 0xe603},
	{MISENSOR_16BIT, 0x99c, 0xd785},
	{MISENSOR_16BIT, 0x99e, 0x30c6},

	{MISENSOR_16BIT, 0x98c, 0x5e5},
	{MISENSOR_16BIT, 0x990, 0xb3a},
	{MISENSOR_16BIT, 0x992, 0x3539},
	{MISENSOR_16BIT, 0x994, 0x3c3c},
	{MISENSOR_16BIT, 0x996, 0x3c34},
	{MISENSOR_16BIT, 0x998, 0xcc32},
	{MISENSOR_16BIT, 0x99a, 0x3ebd},
	{MISENSOR_16BIT, 0x99c, 0xa558},
	{MISENSOR_16BIT, 0x99e, 0x30ed},

	{MISENSOR_16BIT, 0x98c, 0x5f5},
	{MISENSOR_16BIT, 0x990, 0x4bd},
	{MISENSOR_16BIT, 0x992, 0xb2d7},
	{MISENSOR_16BIT, 0x994, 0x30e7},
	{MISENSOR_16BIT, 0x996, 0x6cc},
	{MISENSOR_16BIT, 0x998, 0x323e},
	{MISENSOR_16BIT, 0x99a, 0xed00},
	{MISENSOR_16BIT, 0x99c, 0xec04},
	{MISENSOR_16BIT, 0x99e, 0xbda5},

	{MISENSOR_16BIT, 0x98c, 0x605},
	{MISENSOR_16BIT, 0x990, 0x44cc},
	{MISENSOR_16BIT, 0x992, 0x3244},
	{MISENSOR_16BIT, 0x994, 0xbda5},
	{MISENSOR_16BIT, 0x996, 0x585f},
	{MISENSOR_16BIT, 0x998, 0x30ed},
	{MISENSOR_16BIT, 0x99a, 0x2cc},
	{MISENSOR_16BIT, 0x99c, 0x3244},
	{MISENSOR_16BIT, 0x99e, 0xed00},

	{MISENSOR_16BIT, 0x98c, 0x615},
	{MISENSOR_16BIT, 0x990, 0xf601},
	{MISENSOR_16BIT, 0x992, 0xd54f},
	{MISENSOR_16BIT, 0x994, 0xea03},
	{MISENSOR_16BIT, 0x996, 0xaa02},
	{MISENSOR_16BIT, 0x998, 0xbda5},
	{MISENSOR_16BIT, 0x99a, 0x4430},
	{MISENSOR_16BIT, 0x99c, 0xe606},
	{MISENSOR_16BIT, 0x99e, 0x3838},

	{MISENSOR_16BIT, 0x98c, 0x625},
	{MISENSOR_16BIT, 0x990, 0x3831},
	{MISENSOR_16BIT, 0x992, 0x39bd},
	{MISENSOR_16BIT, 0x994, 0xd661},
	{MISENSOR_16BIT, 0x996, 0xf602},
	{MISENSOR_16BIT, 0x998, 0xf4c1},
	{MISENSOR_16BIT, 0x99a, 0x126},
	{MISENSOR_16BIT, 0x99c, 0xbfe},
	{MISENSOR_16BIT, 0x99e, 0x2bd},

	{MISENSOR_16BIT, 0x98c, 0x635},
	{MISENSOR_16BIT, 0x990, 0xee10},
	{MISENSOR_16BIT, 0x992, 0xfc02},
	{MISENSOR_16BIT, 0x994, 0xf5ad},
	{MISENSOR_16BIT, 0x996, 0x39},
	{MISENSOR_16BIT, 0x998, 0xf602},
	{MISENSOR_16BIT, 0x99a, 0xf4c1},
	{MISENSOR_16BIT, 0x99c, 0x226},
	{MISENSOR_16BIT, 0x99e, 0xafe},

	{MISENSOR_16BIT, 0x98c, 0x645},
	{MISENSOR_16BIT, 0x990, 0x2bd},
	{MISENSOR_16BIT, 0x992, 0xee10},
	{MISENSOR_16BIT, 0x994, 0xfc02},
	{MISENSOR_16BIT, 0x996, 0xf7ad},
	{MISENSOR_16BIT, 0x998, 0x39},
	{MISENSOR_16BIT, 0x99a, 0x3cbd},
	{MISENSOR_16BIT, 0x99c, 0xb059},
	{MISENSOR_16BIT, 0x99e, 0xcc00},

	{MISENSOR_16BIT, 0x98c, 0x655},
	{MISENSOR_16BIT, 0x990, 0x28bd},
	{MISENSOR_16BIT, 0x992, 0xa558},
	{MISENSOR_16BIT, 0x994, 0x8300},
	{MISENSOR_16BIT, 0x996, 0x27},
	{MISENSOR_16BIT, 0x998, 0xbcc},
	{MISENSOR_16BIT, 0x99a, 0x26},
	{MISENSOR_16BIT, 0x99c, 0x30ed},
	{MISENSOR_16BIT, 0x99e, 0xc6},

	{MISENSOR_16BIT, 0x98c, 0x665},
	{MISENSOR_16BIT, 0x990, 0x3bd},
	{MISENSOR_16BIT, 0x992, 0xa544},
	{MISENSOR_16BIT, 0x994, 0x3839},
	{MISENSOR_16BIT, 0x996, 0xbdd9},
	{MISENSOR_16BIT, 0x998, 0x42d6},
	{MISENSOR_16BIT, 0x99a, 0x9acb},
	{MISENSOR_16BIT, 0x99c, 0x1d7},
	{MISENSOR_16BIT, 0x99e, 0x9b39},

	/* hard coded start address of the patch at "patchSetup" */
	{MISENSOR_16BIT, 0x98c, 0x2006},
	{MISENSOR_16BIT, 0x990, 0x415},
	/* execute the patch */
	{MISENSOR_16BIT, 0x98c, 0xa005},
	{MISENSOR_16BIT, 0x990, 0x1},

	{MISENSOR_TOK_TERM, 0, 0}
};

/* Noise settings */
static struct misensor_reg const mt9d113_noise_reduce[] = {
	{MISENSOR_16BIT, 0x33f4, 0x005b}, /* KeRNeL_cONfIG*/
	{MISENSOR_16BIT, 0x098c, 0xa20e}, /* MCU_ADDRESS [ae_MaX_VIRTGaIN]*/
	{MISENSOR_16BIT, 0x0990, 0x00a0}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0x2212}, /* MCU_ADDRESS [ae_MaX_dGaIN_ae1]*/
	{MISENSOR_16BIT, 0x0990, 0x01ee}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0xab30}, /* MCU_ADDRESS [HG_NR_STOP_R]*/
	{MISENSOR_16BIT, 0x0990, 0x00ff}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0xab31}, /* MCU_ADDRESS [HG_NR_STOP_G]*/
	{MISENSOR_16BIT, 0x0990, 0x00ff}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0xab32}, /* MCU_ADDRESS [HG_NR_STOP_b]*/
	{MISENSOR_16BIT, 0x0990, 0x00ff}, /* MCU_DATA_0*/
	/*{MISENSOR_16BIT, 0x098c, 0xab33}, // MCU_ADDRESS [HG_NR_STOP_OL]*/
	/*{MISENSOR_16BIT, 0x0990, 0x00ff}, // MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0xab35}, /* MCU_ADDRESS [HG_NR_GaINSTOP]*/
	{MISENSOR_16BIT, 0x0990, 0x00ff}, /* MCU_DATA_0*/

	{MISENSOR_16BIT, 0x098c, 0x2b2a}, /* MCU_ADDRESS[HG_LL_bRIGHTNeSSSTOP]*/
	{MISENSOR_16BIT, 0x0990, 0x3e80}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0xab20}, /* MCU_ADDRESS [HG_LL_SaT1]*/
	{MISENSOR_16BIT, 0x0990, 0x0048}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0xab24}, /* MCU_ADDRESS [HG_LL_SaT2]*/
	{MISENSOR_16BIT, 0x0990, 0x0000}, /* MCU_DATA_0*/

	{MISENSOR_TOK_TERM, 0, 0}
};

/* Noise setting in video mode */
static struct misensor_reg const mt9d113_video_noise_setting[] = {
	{MISENSOR_16BIT, 0x098c, 0x2212}, /* MCU_ADDRESS [AE_MAX_DGAIN_AE1]*/
	{MISENSOR_16BIT, 0x0990, 0x00c0}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0xab34}, /* MCU_ADDRESS [HG_NR_GAINSTART]*/
	{MISENSOR_16BIT, 0x0990, 0x00b0}, /* MCU_DATA_0*/
	{MISENSOR_TOK_TERM, 0, 0}
};

/* Noise setting in preview/still mode */
static struct misensor_reg const mt9d113_preview_noise_setting[] = {
	{MISENSOR_16BIT, 0x098c, 0x2212}, /* MCU_ADDRESS [AE_MAX_DGAIN_AE1]*/
	{MISENSOR_16BIT, 0x0990, 0x01ee}, /* MCU_DATA_0*/
	{MISENSOR_16BIT, 0x098c, 0xab34}, /* MCU_ADDRESS [HG_NR_GAINSTART]*/
	{MISENSOR_16BIT, 0x0990, 0x0080}, /* MCU_DATA_0*/
	{MISENSOR_TOK_TERM, 0, 0}
};

/* Sharpness */
static struct misensor_reg const mt9d113_sharpness[] = {
	{MISENSOR_16BIT, 0x326c, 0x1304}, /* APERTURE_PARAMETERS */

	{MISENSOR_TOK_TERM, 0, 0}
};

/* LSC 95% */
static struct misensor_reg const mt9d113_lsc_95[] = {
	{MISENSOR_16BIT, 0x3210, 0x01b0},
	{MISENSOR_16BIT, 0x364e, 0x04d0},
	{MISENSOR_16BIT, 0x3650, 0xe7ac},
	{MISENSOR_16BIT, 0x3652, 0x3332},
	{MISENSOR_16BIT, 0x3654, 0x1e6d},
	{MISENSOR_16BIT, 0x3656, 0xd751},
	{MISENSOR_16BIT, 0x3658, 0x00d0},
	{MISENSOR_16BIT, 0x365a, 0x656b},
	{MISENSOR_16BIT, 0x365c, 0x4292},
	{MISENSOR_16BIT, 0x365e, 0x1908},
	{MISENSOR_16BIT, 0x3660, 0xff30},
	{MISENSOR_16BIT, 0x3662, 0x00d0},
	{MISENSOR_16BIT, 0x3664, 0xfc68},
	{MISENSOR_16BIT, 0x3666, 0x1892},
	{MISENSOR_16BIT, 0x3668, 0x254e},
	{MISENSOR_16BIT, 0x366a, 0x42cf},
	{MISENSOR_16BIT, 0x366c, 0x00d0},
	{MISENSOR_16BIT, 0x366e, 0xfd6b},
	{MISENSOR_16BIT, 0x3670, 0x2a12},
	{MISENSOR_16BIT, 0x3672, 0x3aaa},
	{MISENSOR_16BIT, 0x3674, 0xce91},
	{MISENSOR_16BIT, 0x3676, 0x8b2d},
	{MISENSOR_16BIT, 0x3678, 0x91cf},
	{MISENSOR_16BIT, 0x367a, 0xa04f},
	{MISENSOR_16BIT, 0x367c, 0x1c50},
	{MISENSOR_16BIT, 0x367e, 0x09b3},
	{MISENSOR_16BIT, 0x3680, 0xc0ad},
	{MISENSOR_16BIT, 0x3682, 0x262f},
	{MISENSOR_16BIT, 0x3684, 0x8150},
	{MISENSOR_16BIT, 0x3686, 0xa911},
	{MISENSOR_16BIT, 0x3688, 0x1252},
	{MISENSOR_16BIT, 0x368a, 0xb1cd},
	{MISENSOR_16BIT, 0x368c, 0xa64e},
	{MISENSOR_16BIT, 0x368e, 0x8731},
	{MISENSOR_16BIT, 0x3690, 0x0c70},
	{MISENSOR_16BIT, 0x3692, 0x0d93},
	{MISENSOR_16BIT, 0x3694, 0x8a6d},
	{MISENSOR_16BIT, 0x3696, 0x4c2f},
	{MISENSOR_16BIT, 0x3698, 0x846e},
	{MISENSOR_16BIT, 0x369a, 0x9491},
	{MISENSOR_16BIT, 0x369c, 0x14b2},
	{MISENSOR_16BIT, 0x369e, 0x7152},
	{MISENSOR_16BIT, 0x36a0, 0xb44f},
	{MISENSOR_16BIT, 0x36a2, 0x3a55},
	{MISENSOR_16BIT, 0x36a4, 0x3813},
	{MISENSOR_16BIT, 0x36a6, 0xb898},
	{MISENSOR_16BIT, 0x36a8, 0x0313},
	{MISENSOR_16BIT, 0x36aa, 0x9891},
	{MISENSOR_16BIT, 0x36ac, 0x46d5},
	{MISENSOR_16BIT, 0x36ae, 0x6bb3},
	{MISENSOR_16BIT, 0x36b0, 0xcd58},
	{MISENSOR_16BIT, 0x36b2, 0x6152},
	{MISENSOR_16BIT, 0x36b4, 0xc951},
	{MISENSOR_16BIT, 0x36b6, 0x5d75},
	{MISENSOR_16BIT, 0x36b8, 0x6534},
	{MISENSOR_16BIT, 0x36ba, 0xcb78},
	{MISENSOR_16BIT, 0x36bc, 0x7052},
	{MISENSOR_16BIT, 0x36be, 0x9ab0},
	{MISENSOR_16BIT, 0x36c0, 0x3c35},
	{MISENSOR_16BIT, 0x36c2, 0x27b3},
	{MISENSOR_16BIT, 0x36c4, 0xbe18},
	{MISENSOR_16BIT, 0x36c6, 0xae50},
	{MISENSOR_16BIT, 0x36c8, 0xa2d1},
	{MISENSOR_16BIT, 0x36ca, 0x33b0},
	{MISENSOR_16BIT, 0x36cc, 0x6614},
	{MISENSOR_16BIT, 0x36ce, 0xf234},
	{MISENSOR_16BIT, 0x36d0, 0x8b51},
	{MISENSOR_16BIT, 0x36d2, 0x8572},
	{MISENSOR_16BIT, 0x36d4, 0x2133},
	{MISENSOR_16BIT, 0x36d6, 0x3e34},
	{MISENSOR_16BIT, 0x36d8, 0x0835},
	{MISENSOR_16BIT, 0x36da, 0xce30},
	{MISENSOR_16BIT, 0x36dc, 0xb66e},
	{MISENSOR_16BIT, 0x36de, 0x6b33},
	{MISENSOR_16BIT, 0x36e0, 0x1b73},
	{MISENSOR_16BIT, 0x36e2, 0x00d5},
	{MISENSOR_16BIT, 0x36e4, 0xe5cf},
	{MISENSOR_16BIT, 0x36e6, 0xb1d1},
	{MISENSOR_16BIT, 0x36e8, 0xe4b2},
	{MISENSOR_16BIT, 0x36ea, 0x00f3},
	{MISENSOR_16BIT, 0x36ec, 0x21d4},
	{MISENSOR_16BIT, 0x36ee, 0x99f3},
	{MISENSOR_16BIT, 0x36f0, 0x4a33},
	{MISENSOR_16BIT, 0x36f2, 0x8a99},
	{MISENSOR_16BIT, 0x36f4, 0xee36},
	{MISENSOR_16BIT, 0x36f6, 0x33fb},
	{MISENSOR_16BIT, 0x36f8, 0x9972},
	{MISENSOR_16BIT, 0x36fa, 0x16f5},
	{MISENSOR_16BIT, 0x36fc, 0xa839},
	{MISENSOR_16BIT, 0x36fe, 0xf297},
	{MISENSOR_16BIT, 0x3700, 0x64bb},
	{MISENSOR_16BIT, 0x3702, 0xb5ef},
	{MISENSOR_16BIT, 0x3704, 0x4375},
	{MISENSOR_16BIT, 0x3706, 0xa7d9},
	{MISENSOR_16BIT, 0x3708, 0xcef8},
	{MISENSOR_16BIT, 0x370a, 0x5abb},
	{MISENSOR_16BIT, 0x370c, 0x9fb3},
	{MISENSOR_16BIT, 0x370e, 0x7eb3},
	{MISENSOR_16BIT, 0x3710, 0x90f9},
	{MISENSOR_16BIT, 0x3712, 0xde76},
	{MISENSOR_16BIT, 0x3714, 0x49db},
	{MISENSOR_16BIT, 0x3644, 0x0320},
	{MISENSOR_16BIT, 0x3642, 0x0258},
	{MISENSOR_16BIT, 0x3210, 0x01b8},
	{MISENSOR_TOK_TERM, 0, 0}
};

#endif
