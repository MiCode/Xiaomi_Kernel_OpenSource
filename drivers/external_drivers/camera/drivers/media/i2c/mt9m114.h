/*
 * Support for mt9m114 Camera Sensor.
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

#ifndef __A1040_H__
#define __A1040_H__

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

#define V4L2_IDENT_MT9M114 8245

#define MT9P111_REV3
#define FULLINISUPPORT

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

/* mask to set sensor read_mode via misensor_rmw_reg */
#define MISENSOR_R_MODE_MASK	0x0330
/* mask to set sensor vert_flip and horz_mirror */
#define MISENSOR_VFLIP_MASK	0x0002
#define MISENSOR_HFLIP_MASK	0x0001
#define MISENSOR_FLIP_EN	1
#define MISENSOR_FLIP_DIS	0

/* bits set to set sensor read_mode via misensor_rmw_reg */
#define MISENSOR_SKIPPING_SET	0x0011
#define MISENSOR_SUMMING_SET	0x0033
#define MISENSOR_NORMAL_SET	0x0000

/* sensor register that control sensor read-mode and mirror */
#define MISENSOR_READ_MODE	0xC834
/* sensor ae-track status register */
#define MISENSOR_AE_TRACK_STATUS	0xA800
/* sensor awb status register */
#define MISENSOR_AWB_STATUS	0xAC00
/* sensor coarse integration time register */
#define MISENSOR_COARSE_INTEGRATION_TIME 0xC83C

//registers
#define REG_SW_RESET                    0x301A
#define REG_SW_STREAM                   0xDC00
#define REG_SCCB_CTRL                   0x3100
#define REG_SC_CMMN_CHIP_ID             0x0000
#define REG_V_START                     0xc800 //16bits
#define REG_H_START                     0xc802 //16bits
#define REG_V_END                       0xc804 //16bits
#define REG_H_END                       0xc806 //16bits
#define REG_PIXEL_CLK                   0xc808 //32bits
#define REG_TIMING_VTS                  0xc812 //16bits
#define REG_TIMING_HTS                  0xc814 //16bits
#define REG_WIDTH                       0xC868 //16bits
#define REG_HEIGHT                      0xC86A //16bits
#define REG_EXPO_COARSE                 0x3012 //16bits
#define REG_EXPO_FINE                   0x3014 //16bits
#define REG_GAIN                        0x305E
#define REG_ANALOGGAIN                  0x305F
#define REG_ADDR_ACESSS                 0x098E //logical_address_access
#define REG_COMM_Register               0x0080 //command_register

#define SENSOR_DETECTED		1
#define SENSOR_NOT_DETECTED	0

#define I2C_RETRY_COUNT		5
#define MSG_LEN_OFFSET		2

#ifndef MIPI_CONTROL
#define MIPI_CONTROL		0x3400	/* MIPI_Control */
#endif

/* GPIO pin on Moorestown */
#define GPIO_SCLK_25		44
#define GPIO_STB_PIN		47

#define GPIO_STDBY_PIN		49   /* ab:new */
#define GPIO_RESET_PIN		50

/* System control register for Aptina A-1040SOC*/
#define MT9M114_PID		0x0

/* MT9P111_DEVICE_ID */
#define MT9M114_MOD_ID		0x2481

#define MT9M114_FINE_INTG_TIME_MIN 0
#define MT9M114_FINE_INTG_TIME_MAX_MARGIN 0
#define MT9M114_COARSE_INTG_TIME_MIN 1
#define MT9M114_COARSE_INTG_TIME_MAX_MARGIN 6


/* ulBPat; */

#define MT9M114_BPAT_RGRGGBGB	(1 << 0)
#define MT9M114_BPAT_GRGRBGBG	(1 << 1)
#define MT9M114_BPAT_GBGBRGRG	(1 << 2)
#define MT9M114_BPAT_BGBGGRGR	(1 << 3)

#define MT9M114_FOCAL_LENGTH_NUM	208	/*2.08mm*/
#define MT9M114_FOCAL_LENGTH_DEM	100
#define MT9M114_F_NUMBER_DEFAULT_NUM	24
#define MT9M114_F_NUMBER_DEM	10
#define MT9M114_WAIT_STAT_TIMEOUT	100
#define MT9M114_FLICKER_MODE_50HZ	1
#define MT9M114_FLICKER_MODE_60HZ	2
/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define MT9M114_FOCAL_LENGTH_DEFAULT 0xD00064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define MT9M114_F_NUMBER_DEFAULT 0x18000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define MT9M114_F_NUMBER_RANGE 0x180a180a

/* Supported resolutions */
enum {
#if 0
	MT9M114_RES_QCIF,
	MT9M114_RES_QVGA,
	MT9M114_RES_VGA,
	MT9M114_RES_480P,
	MT9M114_RES_576P,
	MT9M114_RES_720P,
#endif
	MT9M114_RES_720_480p_768,
	MT9M114_RES_736P,
	MT9M114_RES_864P,
	MT9M114_RES_960P,
};
#define MT9M114_RES_960P_SIZE_H		1296
#define MT9M114_RES_960P_SIZE_V		976
#define MT9M114_RES_720P_SIZE_H		1280
#define MT9M114_RES_720P_SIZE_V		720
#define MT9M114_RES_576P_SIZE_H		1024
#define MT9M114_RES_576P_SIZE_V		576
#define MT9M114_RES_480P_SIZE_H		768
#define MT9M114_RES_480P_SIZE_V		480
#define MT9M114_RES_VGA_SIZE_H		640
#define MT9M114_RES_VGA_SIZE_V		480
#define MT9M114_RES_QVGA_SIZE_H		320
#define MT9M114_RES_QVGA_SIZE_V		240
#define MT9M114_RES_QCIF_SIZE_H		176
#define MT9M114_RES_QCIF_SIZE_V		144

#define MT9M114_RES_720_480p_768_SIZE_H 736
#define MT9M114_RES_720_480p_768_SIZE_V 496
#define MT9M114_RES_736P_SIZE_H 1296
#define MT9M114_RES_736P_SIZE_V 736
#define MT9M114_RES_864P_SIZE_H 1296
#define MT9M114_RES_864P_SIZE_V 864
#define MT9M114_RES_976P_SIZE_H 1296
#define MT9M114_RES_976P_SIZE_V 976

#define MT9M114_BIN_FACTOR_MAX			3

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

/*
 * struct misensor_fwreg - Firmware burst command
 * @type: FW burst or 8/16 bit register
 * @addr: 16-bit offset to register or other values depending on type
 * @valx: data value for burst (or other commands)
 *
 * Define a structure for sensor register initialization values
 */
struct misensor_fwreg {
	u32	type;	/* type of value, register or FW burst string */
	u32	addr;	/* target address */
	u32	val0;
	u32	val1;
	u32	val2;
	u32	val3;
	u32	val4;
	u32	val5;
	u32	val6;
	u32	val7;
};

struct regval_list {
	u16 reg_num;
	u8 value;
};

struct mt9m114_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;

	struct camera_sensor_platform_data *platform_data;
	int real_model_id;
	int nctx;
	int power;

	unsigned int bus_width;
	unsigned int mode;
	unsigned int field_inv;
	unsigned int field_sel;
	unsigned int ycseq;
	unsigned int conv422;
	unsigned int bpat;
	unsigned int hpol;
	unsigned int vpol;
	unsigned int edge;
	unsigned int bls;
	unsigned int gamma;
	unsigned int cconv;
	unsigned int res;
	unsigned int dwn_sz;
	unsigned int blc;
	unsigned int agc;
	unsigned int awb;
	unsigned int aec;
	/* extention SENSOR version 2 */
	unsigned int cie_profile;

	/* extention SENSOR version 3 */
	unsigned int flicker_freq;

	/* extension SENSOR version 4 */
	unsigned int smia_mode;
	unsigned int mipi_mode;

	/* Add name here to load shared library */
	unsigned int type;

	/*Number of MIPI lanes*/
	unsigned int mipi_lanes;
	char name[32];

	u8 lightfreq;
};

struct mt9m114_format_struct {
	u8 *desc;
	u32 pixelformat;
	struct regval_list *regs;
};

struct mt9m114_res_struct {
	u8 *desc;
	int res;
	int width;
	int height;
	int fps;
	int skip_frames;
	bool used;
	struct regval_list *regs;
	u16 pixels_per_line;
	u16 lines_per_frame;
	u8 bin_factor_x;
	u8 bin_factor_y;
	u8 bin_mode;
};

struct mt9m114_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, int value);
};

/* 2 bytes used for address: 256 bytes total */
#define MT9M114_MAX_WRITE_BUF_SIZE	254
struct mt9m114_write_buffer {
	u16 addr;
	u8 data[MT9M114_MAX_WRITE_BUF_SIZE];
};

struct mt9m114_write_ctrl {
	int index;
	struct mt9m114_write_buffer buffer;
};

/*
 * Modes supported by the mt9m114 driver.
 * Please, keep them in ascending order.
 */
static struct mt9m114_res_struct mt9m114_res[] = {
#if 0
	{
	.desc	= "QCIF",
	.res	= MT9M114_RES_QCIF,
	.width	= 176,
	.height	= 144,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 1,
	},
	{
	.desc	= "QVGA",
	.res	= MT9M114_RES_QVGA,
	.width	= 320,
	.height	= 240,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 1,
	},
	{
	.desc	= "VGA",
	.res	= MT9M114_RES_VGA,
	.width	= 640,
	.height	= 480,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 1,
	},
	{
	.desc	= "480p",
	.res	= MT9M114_RES_480P,
	.width	= 768,
	.height	= 480,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 1,
	},
	{
	.desc	= "576p",
	.res	= MT9M114_RES_576P,
	.width	= 1024,
	.height	= 576,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 3,
	},
	{
	.desc	= "720p",
	.res	= MT9M114_RES_720P,
	.width	= 1280,
	.height	= 720,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 1,
	},
#endif
	{
	.desc	= "480P",
	.res	= MT9M114_RES_720_480p_768,
	.width	= 736,
	.height = 496,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 1,

	.pixels_per_line = 0x0644,
	.lines_per_frame = 0x03E5,
	.bin_factor_x = 1,
	.bin_factor_y = 1,
	.bin_mode = 0,
	},
	{
	.desc	= "720P",
	.res	= MT9M114_RES_736P,
	.width	= 1296,
	.height = 736,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 1,

	.pixels_per_line = 0x0640,
	.lines_per_frame = 0x0307,
	.bin_factor_x = 1,
	.bin_factor_y = 1,
	.bin_mode = 0,
	},
	{
	.desc	= "848P",
	.res	= MT9M114_RES_864P,
	.width	= 1296,
	.height = 864,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 1,

	.pixels_per_line = 0x0640,
	.lines_per_frame = 0x03E8,
	.bin_factor_x = 1,
	.bin_factor_y = 1,
	.bin_mode = 0,
	},
	{
	.desc	= "960P",
	.res	= MT9M114_RES_960P,
	.width	= 1296,
	.height	= 976,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	.skip_frames = 1,

	.pixels_per_line = 0x0644, /* consistent with regs arrays */
	.lines_per_frame = 0x03E5, /* consistent with regs arrays */
	.bin_factor_x = 1,
	.bin_factor_y = 1,
	.bin_mode = 0,
	},
};
#define N_RES (ARRAY_SIZE(mt9m114_res))

static const struct i2c_device_id mt9m114_id[] = {
	{"mt9m114", 0},
	{}
};

static struct misensor_reg const mt9m114_exitstandby[] = {
	 {MISENSOR_16BIT,  0x098E, 0xDC00},
     // exit-standby
     {MISENSOR_8BIT,  0xDC00, 0x54},
	 {MISENSOR_16BIT,  0x0080, 0x8002},
	 {MISENSOR_TOK_TERM, 0, 0}
};


static struct misensor_reg const mt9m114_suspend[] = {
	 {MISENSOR_16BIT,  0x098E, 0xDC00},
	 {MISENSOR_8BIT,  0xDC00, 0x40},
	 {MISENSOR_16BIT,  0x0080, 0x8002},
	 {MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const mt9m114_streaming[] = {
	 {MISENSOR_16BIT,  0x098E, 0xDC00},
	 {MISENSOR_8BIT,  0xDC00, 0x34},
	 {MISENSOR_16BIT,  0x0080, 0x8002},
	 {MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const mt9m114_standby_reg[] = {
	 {MISENSOR_16BIT,  0x098E, 0xDC00},
	 {MISENSOR_8BIT,  0xDC00, 0x50},
	 {MISENSOR_16BIT,  0x0080, 0x8002},
	 {MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const mt9m114_wakeup_reg[] = {
	 {MISENSOR_16BIT,  0x098E, 0xDC00},
	 {MISENSOR_8BIT,  0xDC00, 0x54},
	 {MISENSOR_16BIT,  0x0080, 0x8002},
	 {MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const mt9m114_chgstat_reg[] = {
	{MISENSOR_16BIT,  0x098E, 0xDC00},
	{MISENSOR_8BIT,  0xDC00, 0x28},
	{MISENSOR_16BIT,  0x0080, 0x8002},
	{MISENSOR_TOK_TERM, 0, 0}
};

// [1296x976_30fps] - Intel
static struct misensor_reg const mt9m114_960P_init[] = {
	{MISENSOR_16BIT, 0x098E, 0x1000},
	{MISENSOR_8BIT, 0xC97E, 0x01},	  //cam_sysctl_pll_enable = 1
	{MISENSOR_16BIT, 0xC980, 0x0128}, //cam_sysctl_pll_divider_m_n = 276
	{MISENSOR_16BIT, 0xC982, 0x0700}, //cam_sysctl_pll_divider_p = 1792
	{MISENSOR_16BIT, 0xC800, 0x0000}, //cam_sensor_cfg_y_addr_start = 0
	{MISENSOR_16BIT, 0xC802, 0x0000}, //cam_sensor_cfg_x_addr_start = 0
	{MISENSOR_16BIT, 0xC804, 0x03CF}, //cam_sensor_cfg_y_addr_end = 971
	{MISENSOR_16BIT, 0xC806, 0x050F}, //cam_sensor_cfg_x_addr_end = 1291
	{MISENSOR_16BIT, 0xC808, 0x02DC}, //cam_sensor_cfg_pixclk = 48000000
	{MISENSOR_16BIT, 0xC80A, 0x6C00},
	{MISENSOR_16BIT, 0xC80C, 0x0001}, //cam_sensor_cfg_row_speed = 1
	{MISENSOR_16BIT, 0xC80E, 0x00DB}, //cam_sensor_cfg_fine_integ_time_min = 219
	{MISENSOR_16BIT, 0xC810, 0x05B3}, //cam_sensor_cfg_fine_integ_time_max = 1459
	{MISENSOR_16BIT, 0xC812, 0x03F6}, //cam_sensor_cfg_frame_length_lines = 1006
	{MISENSOR_16BIT, 0xC814, 0x063E}, //cam_sensor_cfg_line_length_pck = 1590
	{MISENSOR_16BIT, 0xC816, 0x0060}, //cam_sensor_cfg_fine_correction = 96
	{MISENSOR_16BIT, 0xC818, 0x03C3}, //cam_sensor_cfg_cpipe_last_row = 963
	{MISENSOR_16BIT, 0xC826, 0x0020}, //cam_sensor_cfg_reg_0_data = 32
	{MISENSOR_16BIT, 0xC834, 0x0000}, //cam_sensor_control_read_mode = 0
	{MISENSOR_16BIT, 0xC854, 0x0000}, //cam_crop_window_xoffset = 0
	{MISENSOR_16BIT, 0xC856, 0x0000}, //cam_crop_window_yoffset = 0
	{MISENSOR_16BIT, 0xC858, 0x0508}, //cam_crop_window_width = 1280
	{MISENSOR_16BIT, 0xC85A, 0x03C8}, //cam_crop_window_height = 960
	{MISENSOR_8BIT,  0xC85C, 0x03},   //cam_crop_cropmode = 3
	{MISENSOR_16BIT, 0xC868, 0x0508}, //cam_output_width = 1280
	{MISENSOR_16BIT, 0xC86A, 0x03C8}, //cam_output_height = 960
	{MISENSOR_TOK_TERM, 0, 0},
};

//[1296x976_30fps_768Mbps]
static struct misensor_reg const mt9m114_976P_init[] = {
	{MISENSOR_16BIT, 0x98E, 0x1000},
	{MISENSOR_8BIT, 0xC97E, 0x01},	  //cam_sysctl_pll_enable = 1
	{MISENSOR_16BIT, 0xC980, 0x0128}, //cam_sysctl_pll_divider_m_n = 276
	{MISENSOR_16BIT, 0xC982, 0x0700}, //cam_sysctl_pll_divider_p = 1792
	{MISENSOR_16BIT, 0xC800, 0x0000}, //cam_sensor_cfg_y_addr_start = 0
	{MISENSOR_16BIT, 0xC802, 0x0000}, //cam_sensor_cfg_x_addr_start = 0
	{MISENSOR_16BIT, 0xC804, 0x03CF}, //cam_sensor_cfg_y_addr_end = 975
	{MISENSOR_16BIT, 0xC806, 0x050F}, //cam_sensor_cfg_x_addr_end = 1295
	{MISENSOR_32BIT, 0xC808, 0x2DC6C00}, //cam_sensor_cfg_pixclk = 480000
	{MISENSOR_16BIT, 0xC80C, 0x0001}, //cam_sensor_cfg_row_speed = 1
	{MISENSOR_16BIT, 0xC80E, 0x00DB}, //cam_sensor_cfg_fine_integ_time_min = 219
	{MISENSOR_16BIT, 0xC810, 0x05B3}, //0x062E //cam_sensor_cfg_fine_integ_time_max = 1459
	{MISENSOR_16BIT, 0xC812, 0x03E5}, //0x074C //cam_sensor_cfg_frame_length_lines = 1006
	{MISENSOR_16BIT, 0xC814, 0x0644}, //0x06B1 /cam_sensor_cfg_line_length_pck = 1590
	{MISENSOR_16BIT, 0xC816, 0x0060}, //cam_sensor_cfg_fine_correction = 96
	{MISENSOR_16BIT, 0xC818, 0x03C3}, //cam_sensor_cfg_cpipe_last_row = 963
	{MISENSOR_16BIT, 0xC826, 0x0020}, //cam_sensor_cfg_reg_0_data = 32
	{MISENSOR_16BIT, 0xC834, 0x0000}, //cam_sensor_control_read_mode = 0
	{MISENSOR_16BIT, 0xC854, 0x0000}, //cam_crop_window_xoffset = 0
	{MISENSOR_16BIT, 0xC856, 0x0000}, //cam_crop_window_yoffset = 0
	{MISENSOR_16BIT, 0xC858, 0x0508}, //cam_crop_window_width = 1288
	{MISENSOR_16BIT, 0xC85A, 0x03C8}, //cam_crop_window_height = 968
	{MISENSOR_8BIT, 0xC85C, 0x03}, //cam_crop_cropmode = 3
	{MISENSOR_16BIT, 0xC868, 0x0508}, //cam_output_width = 1288
	{MISENSOR_16BIT, 0xC86A, 0x03C8}, //cam_output_height = 968
	{MISENSOR_8BIT, 0xC878, 0x00}, //0x0E //cam_aet_aemode = 0
	{MISENSOR_TOK_TERM, 0, 0}
};

// [1296x864_30fps]
static struct misensor_reg const mt9m114_864P_init[] = {
	{MISENSOR_16BIT, 0x98E, 0x1000},
	{MISENSOR_8BIT, 0xC97E, 0x01},	  //cam_sysctl_pll_enable = 1
	{MISENSOR_16BIT, 0xC980, 0x0128}, //cam_sysctl_pll_divider_m_n = 276
	{MISENSOR_16BIT, 0xC982, 0x0700}, //cam_sysctl_pll_divider_p = 1792
	{MISENSOR_16BIT, 0xC800, 0x0038}, //cam_sensor_cfg_y_addr_start = 56
	{MISENSOR_16BIT, 0xC802, 0x0000}, //cam_sensor_cfg_x_addr_start = 0
	{MISENSOR_16BIT, 0xC804, 0x0397}, //cam_sensor_cfg_y_addr_end = 919
	{MISENSOR_16BIT, 0xC806, 0x050F}, //cam_sensor_cfg_x_addr_end = 1295
	{MISENSOR_32BIT, 0xC808, 0x2DC6C00}, //cam_sensor_cfg_pixclk = 48000000
	{MISENSOR_16BIT, 0xC80C, 0x0001}, //cam_sensor_cfg_row_speed = 1
	{MISENSOR_16BIT, 0xC80E, 0x00DB}, //cam_sensor_cfg_fine_integ_time_min = 219
	{MISENSOR_16BIT, 0xC810, 0x05BD}, //cam_sensor_cfg_fine_integ_time_max = 1469
	{MISENSOR_16BIT, 0xC812, 0x03E8}, //cam_sensor_cfg_frame_length_lines = 1000
	{MISENSOR_16BIT, 0xC814, 0x0640}, //cam_sensor_cfg_line_length_pck = 1600
	{MISENSOR_16BIT, 0xC816, 0x0060}, //cam_sensor_cfg_fine_correction = 96
	{MISENSOR_16BIT, 0xC818, 0x035B}, //cam_sensor_cfg_cpipe_last_row = 859
	{MISENSOR_16BIT, 0xC826, 0x0020}, //cam_sensor_cfg_reg_0_data = 32
	{MISENSOR_16BIT, 0xC834, 0x0000}, //cam_sensor_control_read_mode = 0
	{MISENSOR_16BIT, 0xC854, 0x0000}, //cam_crop_window_xoffset = 0
	{MISENSOR_16BIT, 0xC856, 0x0000}, //cam_crop_window_yoffset = 0
	{MISENSOR_16BIT, 0xC858, 0x0508}, //cam_crop_window_width = 1288
	{MISENSOR_16BIT, 0xC85A, 0x0358}, //cam_crop_window_height = 856
	{MISENSOR_8BIT, 0xC85C, 0x03}, //cam_crop_cropmode = 3
	{MISENSOR_16BIT, 0xC868, 0x0508}, //cam_output_width = 1288
	{MISENSOR_16BIT, 0xC86A, 0x0358}, //cam_output_height = 856
	{MISENSOR_8BIT, 0xC878, 0x00}, //0x0E //cam_aet_aemode = 0
	{MISENSOR_TOK_TERM, 0, 0}
};

// [1296x736_30fps]
static struct misensor_reg const mt9m114_736P_init[] = {
	{MISENSOR_16BIT, 0x98E, 0x1000},
	{MISENSOR_8BIT, 0xC97E, 0x01},	  //cam_sysctl_pll_enable = 1
	{MISENSOR_16BIT, 0xC980, 0x011F}, //cam_sysctl_pll_divider_m_n = 287
	{MISENSOR_16BIT, 0xC982, 0x0700}, //cam_sysctl_pll_divider_p = 1792
	{MISENSOR_16BIT, 0xC800, 0x0078}, //cam_sensor_cfg_y_addr_start = 120
	{MISENSOR_16BIT, 0xC802, 0x0000}, //cam_sensor_cfg_x_addr_start = 0
	{MISENSOR_16BIT, 0xC804, 0x0357}, //cam_sensor_cfg_y_addr_end = 855
	{MISENSOR_16BIT, 0xC806, 0x050F}, //cam_sensor_cfg_x_addr_end = 1295
	{MISENSOR_32BIT, 0xC808, 0x237A07F}, //cam_sensor_cfg_pixclk = 37199999
	{MISENSOR_16BIT, 0xC80C, 0x0001}, //cam_sensor_cfg_row_speed = 1
	{MISENSOR_16BIT, 0xC80E, 0x00DB}, //cam_sensor_cfg_fine_integ_time_min = 219
	{MISENSOR_16BIT, 0xC810, 0x05BD}, //0x062E //cam_sensor_cfg_fine_integ_time_max = 1469
	{MISENSOR_16BIT, 0xC812, 0x0307}, //0x074C //cam_sensor_cfg_frame_length_lines = 775
	{MISENSOR_16BIT, 0xC814, 0x0640}, //0x06B1 /cam_sensor_cfg_line_length_pck = 1600
	{MISENSOR_16BIT, 0xC816, 0x0060}, //cam_sensor_cfg_fine_correction = 96
	{MISENSOR_16BIT, 0xC818, 0x02DB}, //cam_sensor_cfg_cpipe_last_row = 731
	{MISENSOR_16BIT, 0xC826, 0x0020}, //cam_sensor_cfg_reg_0_data = 32
	{MISENSOR_16BIT, 0xC834, 0x0000}, //cam_sensor_control_read_mode = 0
	{MISENSOR_16BIT, 0xC854, 0x0000}, //cam_crop_window_xoffset = 0
	{MISENSOR_16BIT, 0xC856, 0x0000}, //cam_crop_window_yoffset = 0
	{MISENSOR_16BIT, 0xC858, 0x0508}, //cam_crop_window_width = 1288
	{MISENSOR_16BIT, 0xC85A, 0x02D8}, //cam_crop_window_height = 728
	{MISENSOR_8BIT, 0xC85C, 0x03}, //cam_crop_cropmode = 3
	{MISENSOR_16BIT, 0xC868, 0x0508}, //cam_output_width = 1288
	{MISENSOR_16BIT, 0xC86A, 0x02D8}, //cam_output_height = 728
	{MISENSOR_8BIT, 0xC878, 0x00}, //0x0E //cam_aet_aemode = 0
	{MISENSOR_TOK_TERM, 0, 0}
};

//[736x496_30fps_768Mbps]
static struct misensor_reg const mt9m114_720_480P_init[] = {
	{MISENSOR_16BIT, 0x98E, 0x1000},
	{MISENSOR_8BIT, 0xC97E, 0x01},	  //cam_sysctl_pll_enable = 1
	{MISENSOR_16BIT, 0xC980, 0x0128}, //cam_sysctl_pll_divider_m_n = 276
	{MISENSOR_16BIT, 0xC982, 0x0700}, //cam_sysctl_pll_divider_p = 1792
	{MISENSOR_16BIT, 0xC800, 0x00F0}, //cam_sensor_cfg_y_addr_start = 240
	{MISENSOR_16BIT, 0xC802, 0x0118}, //cam_sensor_cfg_x_addr_start = 280
	{MISENSOR_16BIT, 0xC804, 0x02DF}, //cam_sensor_cfg_y_addr_end = 735
	{MISENSOR_16BIT, 0xC806, 0x03F7}, //cam_sensor_cfg_x_addr_end = 1015
	{MISENSOR_32BIT, 0xC808, 0x2DC6C00}, //cam_sensor_cfg_pixclk = 48000000
	{MISENSOR_16BIT, 0xC80C, 0x0001}, //cam_sensor_cfg_row_speed = 1
	{MISENSOR_16BIT, 0xC80E, 0x00DB}, //cam_sensor_cfg_fine_integ_time_min = 219
	{MISENSOR_16BIT, 0xC810, 0x05B3}, //0x062E //cam_sensor_cfg_fine_integ_time_max = 1459
	{MISENSOR_16BIT, 0xC812, 0x03E5}, //0x074C //cam_sensor_cfg_frame_length_lines = 997
	{MISENSOR_16BIT, 0xC814, 0x0644}, //0x06B1 /cam_sensor_cfg_line_length_pck = 1604
	{MISENSOR_16BIT, 0xC816, 0x0060}, //cam_sensor_cfg_fine_correction = 96
	{MISENSOR_16BIT, 0xC818, 0x03C3}, //cam_sensor_cfg_cpipe_last_row = 963
	{MISENSOR_16BIT, 0xC826, 0x0020}, //cam_sensor_cfg_reg_0_data = 32
	{MISENSOR_16BIT, 0xC834, 0x0000}, //cam_sensor_control_read_mode = 0
	{MISENSOR_16BIT, 0xC854, 0x0000}, //cam_crop_window_xoffset = 0
	{MISENSOR_16BIT, 0xC856, 0x0000}, //cam_crop_window_yoffset = 0
	{MISENSOR_16BIT, 0xC858, 0x02D8}, //cam_crop_window_width = 728
	{MISENSOR_16BIT, 0xC85A, 0x01E8}, //cam_crop_window_height = 488
	{MISENSOR_8BIT, 0xC85C, 0x03}, //cam_crop_cropmode = 3
	{MISENSOR_16BIT, 0xC868, 0x02D8}, //cam_output_width = 728
	{MISENSOR_16BIT, 0xC86A, 0x01E8}, //cam_output_height = 488
	{MISENSOR_8BIT, 0xC878, 0x00}, //0x0E //cam_aet_aemode = 0
	{MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const mt9m114_common[] = {
	/* reset */
	{MISENSOR_16BIT,  0x301A, 0x0234},
	//LOAD = Step2-PLL_Timing      //PLL and Timing
	{MISENSOR_16BIT, 0x098E, 0x1000}, // LOGICAL_ADDRESS_ACCESS
	{MISENSOR_8BIT, 0xC97E, 0x01},    //cam_sysctl_pll_enable = 1
	{MISENSOR_16BIT, 0xC980, 0x0128}, //cam_sysctl_pll_divider_m_n = 276
	{MISENSOR_16BIT, 0xC982, 0x0700}, //cam_sysctl_pll_divider_p = 1792
	{MISENSOR_16BIT, 0xC800, 0x0000}, //cam_sensor_cfg_y_addr_start = 216
	{MISENSOR_16BIT, 0xC802, 0x0000}, //cam_sensor_cfg_x_addr_start = 168
	{MISENSOR_16BIT, 0xC804, 0x03CD}, //cam_sensor_cfg_y_addr_end = 761
	{MISENSOR_16BIT, 0xC806, 0x050D}, //cam_sensor_cfg_x_addr_end = 1127
	{MISENSOR_16BIT, 0xC808, 0x02DC}, //cam_sensor_cfg_pixclk = 24000000
	{MISENSOR_16BIT, 0xC80A, 0x6C00},
	{MISENSOR_16BIT, 0xC80C, 0x0001}, //cam_sensor_cfg_row_speed = 1
	{MISENSOR_16BIT, 0xC80E, 0x01C3}, //cam_sensor_cfg_fine_integ_time_min = 219
	{MISENSOR_16BIT, 0xC810, 0x03F7}, //cam_sensor_cfg_fine_integ_time_max = 1149
	{MISENSOR_16BIT, 0xC812, 0x0500}, //cam_sensor_cfg_frame_length_lines = 625
	{MISENSOR_16BIT, 0xC814, 0x04E2}, //cam_sensor_cfg_line_length_pck = 1280
	{MISENSOR_16BIT, 0xC816, 0x00E0}, //cam_sensor_cfg_fine_correction = 96
	{MISENSOR_16BIT, 0xC818, 0x01E3}, //cam_sensor_cfg_cpipe_last_row = 541
	{MISENSOR_16BIT, 0xC826, 0x0020}, //cam_sensor_cfg_reg_0_data = 32
	{MISENSOR_16BIT, 0xC834, 0x0330}, //cam_sensor_control_read_mode = 0
	{MISENSOR_16BIT, 0xC854, 0x0000}, //cam_crop_window_xoffset = 0
	{MISENSOR_16BIT, 0xC856, 0x0000}, //cam_crop_window_yoffset = 0
	{MISENSOR_16BIT, 0xC858, 0x0280}, //cam_crop_window_width = 952
	{MISENSOR_16BIT, 0xC85A, 0x01E0}, //cam_crop_window_height = 538
	{MISENSOR_8BIT, 0xC85C, 0x03},    //cam_crop_cropmode = 3
	{MISENSOR_16BIT, 0xC868, 0x0280}, //cam_output_width = 952
	{MISENSOR_16BIT, 0xC86A, 0x01E0}, //cam_output_height = 538
	//LOAD = Step3-Recommended     //Patch,Errata and Sensor optimization Setting
	{MISENSOR_16BIT, 0x316A, 0x8270}, // DAC_TXLO_ROW
	{MISENSOR_16BIT, 0x316C, 0x8270}, // DAC_TXLO
	{MISENSOR_16BIT, 0x3ED0, 0x2305}, // DAC_LD_4_5
	{MISENSOR_16BIT, 0x3ED2, 0x77CF}, // DAC_LD_6_7
	{MISENSOR_16BIT, 0x316E, 0x8202}, // DAC_ECL
	{MISENSOR_16BIT, 0x3180, 0x87FF}, // DELTA_DK_CONTROL
	{MISENSOR_16BIT, 0x30D4, 0x6080}, // COLUMN_CORRECTION
	{MISENSOR_16BIT, 0xA802, 0x0008}, // AE_TRACK_MODE
	{MISENSOR_16BIT, 0x3E14, 0xFF39}, // SAMP_COL_PUP2
	{MISENSOR_16BIT, 0x31E0, 0x0003}, // PIX_DEF_ID
	//LOAD = Step8-Features		//Ports, special features, etc.
	{MISENSOR_16BIT, 0x098E, 0x0000}, // LOGICAL_ADDRESS_ACCESS
	{MISENSOR_16BIT, 0x001E, 0x0777}, // PAD_SLEW
	{MISENSOR_16BIT, 0x098E, 0x0000}, // LOGICAL_ADDRESS_ACCESS
	{MISENSOR_16BIT, 0xC984, 0x8001}, // CAM_PORT_OUTPUT_CONTROL
	{MISENSOR_16BIT, 0xC988, 0x0F00}, // CAM_PORT_MIPI_TIMING_T_HS_ZERO
	{MISENSOR_16BIT, 0xC98A, 0x0B07}, // CAM_PORT_MIPI_TIMING_T_HS_EXIT_HS_TRAIL
	{MISENSOR_16BIT, 0xC98C, 0x0D01}, // CAM_PORT_MIPI_TIMING_T_CLK_POST_CLK_PRE
	{MISENSOR_16BIT, 0xC98E, 0x071D}, // CAM_PORT_MIPI_TIMING_T_CLK_TRAIL_CLK_ZERO
	{MISENSOR_16BIT, 0xC990, 0x0006}, // CAM_PORT_MIPI_TIMING_T_LPX
	{MISENSOR_16BIT, 0xC992, 0x0A0C}, // CAM_PORT_MIPI_TIMING_INIT_TIMING
	{MISENSOR_16BIT, 0x3C5A, 0x0009}, // MIPI_DELAY_TRIM
	{MISENSOR_16BIT, 0xC86C, 0x0210}, // CAM_OUTPUT_FORMAT
	{MISENSOR_16BIT, 0xA804, 0x0000}, // AE_TRACK_ALGO
	//default exposure
	{MISENSOR_16BIT, 0x3012, 0x0110}, // COMMAND_REGISTER
	{MISENSOR_TOK_TERM, 0, 0},

};

static struct misensor_reg const mt9m114_antiflicker_50hz[] = {
	 {MISENSOR_16BIT,  0x098E, 0xC88B},
	 {MISENSOR_8BIT,  0xC88B, 0x32},
	 {MISENSOR_8BIT,  0xDC00, 0x28},
	 {MISENSOR_16BIT,  0x0080, 0x8002},
	 {MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const mt9m114_antiflicker_60hz[] = {
	 {MISENSOR_16BIT,  0x098E, 0xC88B},
	 {MISENSOR_8BIT,  0xC88B, 0x3C},
	 {MISENSOR_8BIT,  0xDC00, 0x28},
	 {MISENSOR_16BIT,  0x0080, 0x8002},
	 {MISENSOR_TOK_TERM, 0, 0}
};

#endif
