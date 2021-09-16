// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 s5k2t7spmipiraw_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/


#define PFX "S5K2T7SP_camera_sensor"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__


#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define_v4l2.h"
#include "kd_imgsensor_errcode.h"

#include "s5k2t7spmipiraw_Sensor.h"
#include "s5k2t7sp_ana_gain_table.h"

#include "adaptor-subdrv.h"
#include "adaptor-i2c.h"

#define read_cmos_sensor_8(...) subdrv_i2c_rd_u8(__VA_ARGS__)
#define read_cmos_sensor(...) subdrv_i2c_rd_u16(__VA_ARGS__)
#define write_cmos_sensor_8(...) subdrv_i2c_wr_u8(__VA_ARGS__)
#define write_cmos_sensor(...) subdrv_i2c_wr_u16(__VA_ARGS__)
#define table_write_cmos_sensor(...) subdrv_i2c_wr_regs_u16(__VA_ARGS__)

/*
 * #define LOG_INF(format, args...) pr_debug(
 * PFX "[%s] " format, __func__, ##args)
 */
static void sensor_init(struct subdrv_ctx *ctx);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5K2T7SP_SENSOR_ID,
	.checksum_value = 0x67b95889,

	.pre = {
		.pclk = 688000000, /*//30fps case*/
		.linelength = 9008, /*//0x2330*/
		.framelength = 2544, /*//0x09F0*/
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592, /*//0x0A20*/
		.grabwindow_height = 1940, /*//0x0794*/
		//grabwindow_height should be 16's N times
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 300,
		.mipi_pixel_rate = 213600000,
	},
	.cap = {
		.pclk = 678400000, /*//30fps case*/
		.linelength = 5640, /*//0x1608*/
		.framelength = 4008, /*//0x0FA8*/
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 5184, /*//0x1440*/
		.grabwindow_height = 3880, /*//0x0F28*/
		/*//grabwindow_height should be 16's N times*/
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 672000000,
	},
	.cap1 = {
		.pclk = 678000000, /*//30fps case*/
		.linelength = 5640, /*//0x1608*/
		.framelength = 4008, /*//0x0FA8*/
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 5184, /*//0x1440*/
		.grabwindow_height = 3880, /*//0x0F28*/
		/*//grabwindow_height should be 16's N times*/
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.cap2 = {
		.pclk = 678000000, /*//30fps case*/
		.linelength = 5640, /*//0x1608*/
		.framelength = 4008, /*//0x0FA8*/
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 5184, /*//0x1440*/
		.grabwindow_height = 3880, /*//0x0F28*/
		/*//grabwindow_height should be 16's N times*/
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 688000000, /*//30fps case*/
		.linelength = 9008, /*//0x2330*/
		.framelength = 2544, /*//0x09f0*/
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592, /*//0x0a20*/
		.grabwindow_height = 1940, /*//0x0794*/
		/*//grabwindow_height should be 16's N times*/
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 213600000,
	},
	.hs_video = {/*slow motion*/
		.pclk = 688000000, /*//30fps case*/
		.linelength = 5608, /*0x0500*/
		.framelength = 1022, /*//0x02d0*/
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280, /*//0x1440*/
		.grabwindow_height = 720, /*//0x0F28*/
		/*//grabwindow_height should be 16's N times*/
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
		.mipi_pixel_rate = 400800000,
	},
	.slim_video = {/*VT Call*/
		.pclk = 688000000, /*//30fps case*/
		.linelength = 9008, /*//0x1608*/
		.framelength = 2544, /*//0x0FA8*/
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280, /*//0x1440*/
		.grabwindow_height = 720, /*//0x0F28*/
		/*//grabwindow_height should be 16's N times*/
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 213600000,
	},
	.margin = 8,		/* sensor framelength & shutter margin */
	.min_shutter = 4,	/* min shutter */

	/* max framelength by sensor register's limitation */
	.max_frame_length = 0xffff,

	/* shutter delay frame for AE cycle,
	 * 2 frame with ispGain_delay-shut_delay=2-0=2
	 */
	.ae_shut_delay_frame = 0,

	/* sensor gain delay frame for AE cycle,
	 * 2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	 */
	.ae_sensor_gain_delay_frame = 0,

	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 5,	/* support sensor mode num */

	.cap_delay_frame = 3,	/* enter capture delay frame num */
	.pre_delay_frame = 3,	/* enter preview delay frame num */
	.video_delay_frame = 3,	/* enter video delay frame num */

	/* enter high speed video  delay frame num */
	.hs_video_delay_frame = 3,

	.slim_video_delay_frame = 3,	/* enter slim video delay frame num */

	.frame_time_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_8MA,	/* mclk driving current */

	/* sensor_interface_type */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,

	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,

	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.mipi_settle_delay_mode = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_BAYER_Gr,
	.mclk = 24,	/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_speed = 1000, /*support 1MHz write*/
	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
	.i2c_addr_table = { 0x5a, 0xff},
};




//int chip_id;
/* VC_Num, VC_PixelNum, ModeSelect, EXPO_Ratio, ODValue, RG_STATSMODE */
/* VC0_ID, VC0_DataType, VC0_SIZEH, VC0_SIZE,
 * VC1_ID, VC1_DataType, VC1_SIZEH, VC1_SIZEV
 */
/* VC2_ID, VC2_DataType, VC2_SIZEH, VC2_SIZE,
 * VC3_ID, VC3_DataType, VC3_SIZEH, VC3_SIZEV
 */

/*static SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3]=
 *  {// Preview mode setting
 *  {0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
 *  0x00, 0x2B, 0x0910, 0x06D0, 0x01, 0x00, 0x0000, 0x0000,
 *  0x02, 0x30, 0x00B4, 0x0360, 0x03, 0x00, 0x0000, 0x0000},
 * // Video mode setting
 *{0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
 *0x00, 0x2B, 0x1220, 0x0DA0, 0x01, 0x00, 0x0000, 0x0000,
 *0x02, 0x30, 0x00B4, 0x0360, 0x03, 0x00, 0x0000, 0x0000},
 * // Capture mode setting
 *{0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
 *0x00, 0x2B, 0x1220, 0x0DA0, 0x01, 0x00, 0x0000, 0x0000,
 *0x02, 0x30, 0x00B4, 0x0360, 0x03, 0x00, 0x0000, 0x0000}};
 */

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{ 5200, 3880,    8,    0, 5184, 3880, 2592, 1940,
	     0,    0, 2592, 1940,    0,    0, 2592, 1940},

	{ 5200, 3880,    0,    0, 5184, 3880, 5184, 3880,
	     0,    0, 5184, 3880,    0,    0, 5184, 3880},

	{ 5200, 3880,    8,    0, 5184, 3880, 2592, 1940,
	     0,    0, 2592, 1940,    0,    0, 2592, 1940},

	{ 5200, 3880,    1312,  1228, 2576, 1440, 1280, 720,
	     4,    0, 1280, 720,    0,    0, 1280, 720},

	{ 5200, 3896,    1320,    1220, 2576, 1440, 1280, 720,
	     4,    0, 1280, 720,    0,    0, 1280, 720},
};


/* no mirror flip, and no binning -revised by dj */
/* static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
 * .i4OffsetX = 16,
 * .i4OffsetY = 16,
 * .i4PitchX  = 64,
 * .i4PitchY  = 64,
 * .i4PairNum  =16,
 * .i4SubBlkW  =16,
 * .i4SubBlkH  =16,
 * .i4PosL = {{20,23},{72,23},{36,27},{56,27},{24,43},{68,43},{40,47},
 * {52,47},{40,55},{52,55},{24,59},{68,59},{36,75},{56,75},{20,79},{72,79}},
 * .i4PosR = {{20,27},{72,27},{36,31},{56,31},{24,39},{68,39},{40,43},{52,43},
 * {40,59},{52,59},{24,63},{68,63},{36,71},{56,71},{20,75},{72,75}},
 * .iMirrorFlip = 0,
 * .i4BlockNumX = 72,
 * .i4BlockNumY = 54,
 * };
 */

#define RWB_ID_OFFSET 0x0F73
#define EEPROM_READ_ID  0xA4
#define EEPROM_WRITE_ID   0xA5




static void set_dummy(struct subdrv_ctx *ctx)
{
	pr_debug("dummyline = %d, dummypixels = %d\n",
		ctx->dummy_line, ctx->dummy_pixel);

	/* return; //for test */
	write_cmos_sensor(ctx, 0x0340, ctx->frame_length);
	write_cmos_sensor(ctx, 0x0342, ctx->line_length);
}				/*      set_dummy  */


static void set_max_framerate(struct subdrv_ctx *ctx, UINT16 framerate, kal_bool min_framelength_en)
{

	kal_uint32 frame_length = ctx->frame_length;

	pr_debug("framerate = %d, min framelength should enable %d\n",
		framerate, min_framelength_en);

	frame_length = ctx->pclk / framerate * 10 / ctx->line_length;
	if (frame_length >= ctx->min_frame_length)
		ctx->frame_length = frame_length;
	else
		ctx->frame_length = ctx->min_frame_length;

	ctx->dummy_line =
		ctx->frame_length - ctx->min_frame_length;

	if (ctx->frame_length > imgsensor_info.max_frame_length) {
		ctx->frame_length = imgsensor_info.max_frame_length;

		ctx->dummy_line =
			ctx->frame_length - ctx->min_frame_length;
	}
	if (min_framelength_en)
		ctx->min_frame_length = ctx->frame_length;
	set_dummy(ctx);
}				/*      set_max_framerate  */

static void write_shutter(struct subdrv_ctx *ctx, kal_uint16 shutter)
{

	kal_uint16 realtime_fps = 0;

	if (shutter > ctx->min_frame_length - imgsensor_info.margin)
		ctx->frame_length = shutter + imgsensor_info.margin;
	else
		ctx->frame_length = ctx->min_frame_length;
	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk
			/ ctx->line_length * 10 / ctx->frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(ctx, 146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(ctx, 0x0340, ctx->frame_length);

		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(ctx, 0x0340, ctx->frame_length);
		pr_debug("(else)ctx->frame_length = %d\n",
			ctx->frame_length);

	}
	/* Update Shutter */
	write_cmos_sensor(ctx, 0x0202, shutter);
	pr_debug("shutter =%d, framelength =%d\n",
		shutter, ctx->frame_length);

}				/*      write_shutter  */



/*************************************************************************
 * FUNCTION
 *	set_shutter
 *
 * DESCRIPTION
 *	This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *	iShutter : exposured lines
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter(struct subdrv_ctx *ctx, kal_uint16 shutter)
{
	ctx->shutter = shutter;

	write_shutter(ctx, shutter);
}				/*      set_shutter */



static kal_uint16 gain2reg(struct subdrv_ctx *ctx, const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0;

	reg_gain = gain / 2;
	return (kal_uint16) reg_gain;
}

/*************************************************************************
 * FUNCTION
 *	set_gain
 *
 * DESCRIPTION
 *	This function is to set global gain to sensor.
 *
 * PARAMETERS
 *	iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *	the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 set_gain(struct subdrv_ctx *ctx, kal_uint32 gain)
{
	kal_uint16 reg_gain;

	/* gain=1024;//for test */
	/* return; //for test */

	if (gain < BASEGAIN || gain > 16 * BASEGAIN) {
		pr_debug("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 16 * BASEGAIN)
			gain = 16 * BASEGAIN;
	}

	reg_gain = gain2reg(ctx, gain);
	ctx->gain = reg_gain;
	pr_debug("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor(ctx, 0x0204, reg_gain);
	/* write_cmos_sensor_8(ctx, 0x0204,(reg_gain>>8)); */
	/* write_cmos_sensor_8(ctx, 0x0205,(reg_gain&0xff)); */

	return gain;
}				/*      set_gain  */

static void set_mirror_flip(struct subdrv_ctx *ctx, kal_uint8 image_mirror)
{

	kal_uint8 itemp;

	pr_debug("image_mirror = %d\n", image_mirror);
	itemp = read_cmos_sensor_8(ctx, 0x0101);
	itemp &= ~0x03;

	switch (image_mirror) {

	case IMAGE_NORMAL:
		write_cmos_sensor_8(ctx, 0x0101, itemp);
		break;

	case IMAGE_V_MIRROR:
		write_cmos_sensor_8(ctx, 0x0101, itemp | 0x02);
		break;

	case IMAGE_H_MIRROR:
		write_cmos_sensor_8(ctx, 0x0101, itemp | 0x01);
		break;

	case IMAGE_HV_MIRROR:
		write_cmos_sensor_8(ctx, 0x0101, itemp | 0x03);
		break;
	}
}

/*************************************************************************
 * FUNCTION
 *	check_stremoff
 *
 * DESCRIPTION
 *	waiting function until sensor streaming finish.
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/

static void check_streamoff(struct subdrv_ctx *ctx)
{
	unsigned int i = 0;
	int timeout = ctx->current_fps ? (10000 / ctx->current_fps) + 1 : 101;

	mdelay(3);
	for (i = 0; i < timeout; i++) {
		if (read_cmos_sensor_8(ctx, 0x0005) != 0xFF)
			mdelay(1);
		else
			break;
	}
	if (read_cmos_sensor_8(ctx, 0x0005) != 0xFF)
		sensor_init(ctx);
	pr_debug("%s exit! %d\n", __func__, i);
}

static kal_uint32 streaming_control(struct subdrv_ctx *ctx, kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
		write_cmos_sensor_8(ctx, 0x0100, 0x01);
	} else {
		write_cmos_sensor_8(ctx, 0x0100, 0x00);
		check_streamoff(ctx);
	}
	return ERROR_NONE;
}

static kal_uint16 addr_data_pair_init_2t7sp[] = {
	0x6028, 0x2000,
	0x602A, 0x3FE4,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0449,
	0x6F12, 0x0348,
	0x6F12, 0x044A,
	0x6F12, 0x4860,
	0x6F12, 0x101A,
	0x6F12, 0x0881,
	0x6F12, 0x00F0,
	0x6F12, 0xE3B8,
	0x6F12, 0x2000,
	0x6F12, 0x42FA,
	0x6F12, 0x2000,
	0x6F12, 0x2120,
	0x6F12, 0x2000,
	0x6F12, 0x9C00,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x2DE9,
	0x6F12, 0xF041,
	0x6F12, 0x7F48,
	0x6F12, 0x0022,
	0x6F12, 0x0068,
	0x6F12, 0x85B2,
	0x6F12, 0x040C,
	0x6F12, 0x2946,
	0x6F12, 0x2046,
	0x6F12, 0x00F0,
	0x6F12, 0x15F9,
	0x6F12, 0x00F0,
	0x6F12, 0x18F9,
	0x6F12, 0x7A4E,
	0x6F12, 0x0022,
	0x6F12, 0x7A49,
	0x6F12, 0x0C36,
	0x6F12, 0x01EB,
	0x6F12, 0x8203,
	0x6F12, 0x02F0,
	0x6F12, 0x0307,
	0x6F12, 0x03F5,
	0x6F12, 0xD960,
	0x6F12, 0xD3F8,
	0x6F12, 0xC836,
	0x6F12, 0x36F8,
	0x6F12, 0x1770,
	0x6F12, 0x521C,
	0x6F12, 0x7B43,
	0x6F12, 0x03F5,
	0x6F12, 0x0063,
	0x6F12, 0x1B0B,
	0x6F12, 0x0360,
	0x6F12, 0x082A,
	0x6F12, 0xEDD3,
	0x6F12, 0x2946,
	0x6F12, 0x2046,
	0x6F12, 0xBDE8,
	0x6F12, 0xF041,
	0x6F12, 0x0122,
	0x6F12, 0x00F0,
	0x6F12, 0xF6B8,
	0x6F12, 0x10B5,
	0x6F12, 0x4FF4,
	0x6F12, 0x8041,
	0x6F12, 0x0F20,
	0x6F12, 0x00F0,
	0x6F12, 0xFAF8,
	0x6F12, 0x0022,
	0x6F12, 0xBDE8,
	0x6F12, 0x1040,
	0x6F12, 0x0121,
	0x6F12, 0x4FF2,
	0x6F12, 0xA040,
	0x6F12, 0x00F0,
	0x6F12, 0xE8B8,
	0x6F12, 0x10B5,
	0x6F12, 0x0446,
	0x6F12, 0x00F0,
	0x6F12, 0xF3F8,
	0x6F12, 0x6349,
	0x6F12, 0x91F8,
	0x6F12, 0x4423,
	0x6F12, 0x012A,
	0x6F12, 0x05D1,
	0x6F12, 0x94F8,
	0x6F12, 0xA500,
	0x6F12, 0x10B1,
	0x6F12, 0x604A,
	0x6F12, 0x0120,
	0x6F12, 0x1080,
	0x6F12, 0x5D48,
	0x6F12, 0x91F8,
	0x6F12, 0xBE11,
	0x6F12, 0x0C30,
	0x6F12, 0x007A,
	0x6F12, 0x4000,
	0x6F12, 0x40EA,
	0x6F12, 0x4120,
	0x6F12, 0x5C49,
	0x6F12, 0x0880,
	0x6F12, 0x5C49,
	0x6F12, 0x40F2,
	0x6F12, 0x5510,
	0x6F12, 0x8880,
	0x6F12, 0x43F6,
	0x6F12, 0xFF73,
	0x6F12, 0x0422,
	0x6F12, 0xAFF2,
	0x6F12, 0x5F01,
	0x6F12, 0x0F20,
	0x6F12, 0x00F0,
	0x6F12, 0xD7F8,
	0x6F12, 0x5348,
	0x6F12, 0xB0F8,
	0x6F12, 0xE214,
	0x6F12, 0xBDE8,
	0x6F12, 0x1040,
	0x6F12, 0x4FF2,
	0x6F12, 0x1400,
	0x6F12, 0x00F0,
	0x6F12, 0xD3B8,
	0x6F12, 0x5248,
	0x6F12, 0x90F8,
	0x6F12, 0x7610,
	0x6F12, 0x5248,
	0x6F12, 0x0229,
	0x6F12, 0x04D1,
	0x6F12, 0x90F8,
	0x6F12, 0x9C10,
	0x6F12, 0x09B1,
	0x6F12, 0x0120,
	0x6F12, 0x7047,
	0x6F12, 0x0168,
	0x6F12, 0xC0F8,
	0x6F12, 0xD410,
	0x6F12, 0x0020,
	0x6F12, 0x7047,
	0x6F12, 0x70B5,
	0x6F12, 0x0446,
	0x6F12, 0x4448,
	0x6F12, 0x0022,
	0x6F12, 0x4168,
	0x6F12, 0x0D0C,
	0x6F12, 0x8EB2,
	0x6F12, 0x3146,
	0x6F12, 0x2846,
	0x6F12, 0x00F0,
	0x6F12, 0x9FF8,
	0x6F12, 0x6007,
	0x6F12, 0x05D5,
	0x6F12, 0x0122,
	0x6F12, 0x1146,
	0x6F12, 0x4FF2,
	0x6F12, 0xA040,
	0x6F12, 0x00F0,
	0x6F12, 0x97F8,
	0x6F12, 0x2046,
	0x6F12, 0x00F0,
	0x6F12, 0xB2F8,
	0x6F12, 0x3146,
	0x6F12, 0x2846,
	0x6F12, 0xBDE8,
	0x6F12, 0x7040,
	0x6F12, 0x0122,
	0x6F12, 0x00F0,
	0x6F12, 0x8DB8,
	0x6F12, 0x70B5,
	0x6F12, 0x00F0,
	0x6F12, 0xADF8,
	0x6F12, 0xE0B3,
	0x6F12, 0x49F2,
	0x6F12, 0x3430,
	0x6F12, 0x00F0,
	0x6F12, 0xADF8,
	0x6F12, 0x3A49,
	0x6F12, 0x0A88,
	0x6F12, 0x3A4D,
	0x6F12, 0xB0FB,
	0x6F12, 0xF2F0,
	0x6F12, 0xB5F8,
	0x6F12, 0x4A10,
	0x6F12, 0x401A,
	0x6F12, 0x0021,
	0x6F12, 0x00F0,
	0x6F12, 0xA7F8,
	0x6F12, 0x364C,
	0x6F12, 0x334E,
	0x6F12, 0x00B2,
	0x6F12, 0xA081,
	0x6F12, 0xF17A,
	0x6F12, 0x11F0,
	0x6F12, 0x060F,
	0x6F12, 0x0DD0,
	0x6F12, 0x0020,
	0x6F12, 0x00F0,
	0x6F12, 0xA1F8,
	0x6F12, 0xF07A,
	0x6F12, 0x5C35,
	0x6F12, 0xC0F3,
	0x6F12, 0x4003,
	0x6F12, 0x6A78,
	0x6F12, 0x2978,
	0x6F12, 0xB4F9,
	0x6F12, 0x0C00,
	0x6F12, 0x00F0,
	0x6F12, 0x9CF8,
	0x6F12, 0x07E0,
	0x6F12, 0x2249,
	0x6F12, 0x0C31,
	0x6F12, 0x4A89,
	0x6F12, 0x8989,
	0x6F12, 0x5043,
	0x6F12, 0x01EB,
	0x6F12, 0x2030,
	0x6F12, 0xA081,
	0x6F12, 0xB4F9,
	0x6F12, 0x0800,
	0x6F12, 0x0028,
	0x6F12, 0x0AD0,
	0x6F12, 0xB4F9,
	0x6F12, 0x0C20,
	0x6F12, 0x6168,
	0x6F12, 0xC1EB,
	0x6F12, 0x0221,
	0x6F12, 0xB4F9,
	0x6F12, 0x0A20,
	0x6F12, 0x5143,
	0x6F12, 0x91FB,
	0x6F12, 0xF0F0,
	0x6F12, 0x2080,
	0x6F12, 0x70BD,
	0x6F12, 0xFFE7,
	0x6F12, 0xBDE8,
	0x6F12, 0x7040,
	0x6F12, 0x0021,
	0x6F12, 0x3820,
	0x6F12, 0x00F0,
	0x6F12, 0x81B8,
	0x6F12, 0x10B5,
	0x6F12, 0x0022,
	0x6F12, 0xAFF2,
	0x6F12, 0xBB11,
	0x6F12, 0x1B48,
	0x6F12, 0x00F0,
	0x6F12, 0x7FF8,
	0x6F12, 0x0F4C,
	0x6F12, 0xAFF2,
	0x6F12, 0x5711,
	0x6F12, 0x2060,
	0x6F12, 0x1848,
	0x6F12, 0x8164,
	0x6F12, 0x0021,
	0x6F12, 0x4163,
	0x6F12, 0xAFF2,
	0x6F12, 0x0B11,
	0x6F12, 0x0022,
	0x6F12, 0x0163,
	0x6F12, 0xAFF2,
	0x6F12, 0xF301,
	0x6F12, 0x1448,
	0x6F12, 0x00F0,
	0x6F12, 0x6EF8,
	0x6F12, 0x0022,
	0x6F12, 0xAFF2,
	0x6F12, 0xC501,
	0x6F12, 0x6060,
	0x6F12, 0x1248,
	0x6F12, 0x00F0,
	0x6F12, 0x67F8,
	0x6F12, 0x1149,
	0x6F12, 0xA060,
	0x6F12, 0x4FF6,
	0x6F12, 0xC120,
	0x6F12, 0x0968,
	0x6F12, 0x4883,
	0x6F12, 0x10BD,
	0x6F12, 0x0000,
	0x6F12, 0x2000,
	0x6F12, 0x42E0,
	0x6F12, 0x2000,
	0x6F12, 0x2240,
	0x6F12, 0x4000,
	0x6F12, 0xF410,
	0x6F12, 0x4000,
	0x6F12, 0xF192,
	0x6F12, 0x4000,
	0x6F12, 0x9000,
	0x6F12, 0x2000,
	0x6F12, 0x0900,
	0x6F12, 0x2000,
	0x6F12, 0x2120,
	0x6F12, 0x4000,
	0x6F12, 0x9338,
	0x6F12, 0x2000,
	0x6F12, 0x1880,
	0x6F12, 0x2000,
	0x6F12, 0x3280,
	0x6F12, 0x0000,
	0x6F12, 0x303B,
	0x6F12, 0x2000,
	0x6F12, 0x08B0,
	0x6F12, 0x0000,
	0x6F12, 0x4F0B,
	0x6F12, 0x0000,
	0x6F12, 0x9D6F,
	0x6F12, 0x2000,
	0x6F12, 0x0510,
	0x6F12, 0x40F6,
	0x6F12, 0xAF2C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x43F2,
	0x6F12, 0x3B0C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x40F2,
	0x6F12, 0x3B4C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x45F6,
	0x6F12, 0x8D7C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x40F2,
	0x6F12, 0xA54C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x40F6,
	0x6F12, 0xD12C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x44F6,
	0x6F12, 0x0B7C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x40F2,
	0x6F12, 0x0F7C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x40F6,
	0x6F12, 0xC32C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x40F6,
	0x6F12, 0x0D2C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x45F6,
	0x6F12, 0x5D3C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x41F2,
	0x6F12, 0x370C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x40F2,
	0x6F12, 0x9F3C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x40F2,
	0x6F12, 0xE51C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6028, 0x2000,
	0x602A, 0x130C,
	0x6F12, 0x0001,
	0x602A, 0x13BA,
	0x6F12, 0x0C48,
	0x602A, 0x1390,
	0x6F12, 0x0015,
	0x602A, 0x139E,
	0x6F12, 0x0050,
	0x602A, 0x139C,
	0x6F12, 0x02AF,
	0x602A, 0x139A,
	0x6F12, 0x7086,
	0x602A, 0x13A2,
	0x6F12, 0x0430,
	0x602A, 0x13BC,
	0x6F12, 0x0114,
	0x602A, 0x12AA,
	0x6F12, 0x0300,
	0x6F12, 0x0307,
	0x602A, 0x0A58,
	0x6F12, 0x0008,
	0x602A, 0x0A98,
	0x6F12, 0x011D,
	0x602A, 0x0AA0,
	0x6F12, 0x00F7,
	0x602A, 0x0AB8,
	0x6F12, 0x0116,
	0x602A, 0x0AC0,
	0x6F12, 0x00FE,
	0x602A, 0x0C48,
	0x6F12, 0x002D,
	0x602A, 0x0DF0,
	0x6F12, 0x0012,
	0x602A, 0x0DF8,
	0x6F12, 0x00E0,
	0x602A, 0x0E30,
	0x6F12, 0x0012,
	0x602A, 0x0E38,
	0x6F12, 0x00E0,
	0x602A, 0x0F00,
	0x6F12, 0x0001,
	0x602A, 0x0B88,
	0x6F12, 0x0011,
	0x602A, 0x0B90,
	0x6F12, 0x0015,
	0x602A, 0x09BE,
	0x6F12, 0x0080,
	0x602A, 0x1382,
	0x6F12, 0x3CFC,
	0x602A, 0x1E80,
	0x6F12, 0x0100,
	0x6F12, 0x003F,
	0x6F12, 0x003F,
	0x6F12, 0x0004,
	0x6F12, 0x000B,
	0x6F12, 0xF48E,
	0x6F12, 0x0004,
	0x6F12, 0x000B,
	0x6F12, 0xF490,
	0x6F12, 0x0005,
	0x6F12, 0x0004,
	0x6F12, 0xF488,
	0x0220, 0x0000,
	0x6028, 0x2000,
	0x602A, 0x0990,
	0x6F12, 0x0002,
	0x602A, 0x1C7E,
	0x6F12, 0x001A,
	0x0B04, 0x0101,
	0x6028, 0x2000,
	0x602A, 0x1E5C,
	0x6F12, 0x0100,
	0x302A, 0x0CA0,
	0x300E, 0x0100,
	0x0138, 0x0100,
	0x6028, 0x2000,
	0x602A, 0x18DE,
	0x6F12, 0x0F28,
	0x602A, 0x1952,
	0x6F12, 0x000A,
	0x602A, 0x42EC,
	0x6F12, 0x1000,
	0x6F12, 0x1000,
	0x6F12, 0x1000,
	0x6F12, 0x1000,
	0x6F12, 0x0489,
	0x6F12, 0x1000,
	0x6F12, 0x0000,
	0x602A, 0x18DA,
	0x6F12, 0x2850,
	0x602A, 0x1A74,
	0x6F12, 0x0100
};

static void sensor_init(struct subdrv_ctx *ctx)
{
	pr_debug("%s E\n", __func__);
	/* initial sequence */
	// Convert from : "InitGlobal.sset"


	write_cmos_sensor(ctx, 0x6028, 0x4000);
	write_cmos_sensor(ctx, 0x0000, 0x0005);
	write_cmos_sensor(ctx, 0x0000, 0x2174);
	write_cmos_sensor(ctx, 0x6010, 0x0001);
	mdelay(3);
	write_cmos_sensor(ctx, 0x6214, 0x7970);
	write_cmos_sensor(ctx, 0x6218, 0x7150);
	write_cmos_sensor(ctx, 0x0A02, 0x5F00);

	table_write_cmos_sensor(ctx, addr_data_pair_init_2t7sp,
		sizeof(addr_data_pair_init_2t7sp) / sizeof(kal_uint16));

	write_cmos_sensor_8(ctx, 0x0138, 0x01);/*enable temperature*/
}				/*      sensor_init  */



static kal_uint16 addr_data_pair_pre_2t7sp[] = {
	0x6028, 0x2000,
	0x0344, 0x0000,
	0x0346, 0x0008,
	0x0348, 0x144F,
	0x034A, 0x0F2F,
	0x034C, 0x0A20,
	0x034E, 0x0794,
	0x0408, 0x0004,
	0x040A, 0x0000,
	0x0900, 0x0112,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0400, 0x0000,
	0x0404, 0x0010,
	0x301E, 0x0210,
	0x0110, 0x0002,
	0x0114, 0x0300,
	0x0136, 0x1800,
	0x0300, 0x0005,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00D7,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0004,
	0x030E, 0x0059,
	0x0310, 0x0100,
	0x0312, 0x0100,
	0x0340, 0x09F0,
	0x0342, 0x2330,
	0x602A, 0x1C78,
	0x6F12, 0x8101
};


static void preview_setting(struct subdrv_ctx *ctx)
{
	pr_debug("%s E\n", __func__);

	/* Convert from : "2T7SP_5M_2592x1940_30fps_MIPI534mbps.sset"*/


	/*$MV1[MCLK:24,Width:2592,Height:1940,Format:MIPI_RAW10,mipi_lane:4*/
	/*,mipi_datarate:534,pvi_pclk_inverse:0]*/

	/* ExtClk :	24	MHz*/
	/* Vt_pix_clk :	688	MHz*/
	/* MIPI_output_speed :	534	Mbps/lane*/
	/* Crop_Width :	5200	px*/
	/* Crop_Height :	3880	px*/
	/* Output_Width :	2592	px*/
	/* Output_Height :	1940	px*/
	/* Frame rate :	30.02	fps*/
	/* Output format :	Raw10*/
	/* H-size :	9008	px*/
	/* H-blank :	6416	px*/
	/* V-size :	2544	line*/
	/* V-blank :	604	line*/
	/* Lane :	4	lane*/
	/* First Pixel :	Gr	First*/
	table_write_cmos_sensor(ctx, addr_data_pair_pre_2t7sp,
			sizeof(addr_data_pair_pre_2t7sp) / sizeof(kal_uint16));



}				/*      preview_setting  */



static kal_uint16 addr_data_pair_cap_2t7sp[] = {
	0x6028, 0x2000,
	0x0344, 0x0008,
	0x0346, 0x0008,
	0x0348, 0x1447,
	0x034A, 0x0F2F,
	0x034C, 0x1440,
	0x034E, 0x0F28,
	0x0408, 0x0000,
	0x040A, 0x0000,
	0x0900, 0x0011,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0386, 0x0001,
	0x0400, 0x0000,
	0x0404, 0x0010,
	0x301E, 0x0110,
	0x0110, 0x0002,
	0x0114, 0x0300,
	0x0136, 0x1800,
	0x0300, 0x0005,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00D4,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0004,
	0x030E, 0x008C,
	0x0310, 0x0100,
	0x0312, 0x0000,
	0x0340, 0x0FA8,
	0x0342, 0x1608,
	0x602A, 0x1C78,
	0x6F12, 0x8100
};



static void capture_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
	pr_debug("%s E! currefps:%d\n", __func__, currefps);

/*
 * /  write_cmos_sensor(ctx, 0x6028, 0x4000);
 *    write_cmos_sensor_8(ctx, 0x0100, 0x00);
 *    if (currefps == 150){
 *	 Convert from : "Init.txt"
 *	 No setfile ready yet    } else if (currefps == 240){
 *	 Convert from : "Init.txt"
 *
 *
 *	No setfile ready yet    } else {	//30fps
 *	Convert from : "2T7SP_20M_5184x3880_30fps_MIPI1680mbps.sset"
 *
 *
 *	$MV1[MCLK:24,Width:5184,Height:3880,Format:MIPI_RAW10,mipi_lane:4
 *         ,mipi_datarate:1680,pvi_pclk_inverse:0]
 *	 ExtClk :	24	MHz
 *	 Vt_pix_clk :	678.4	MHz
 *	 MIPI_output_speed :	1680	Mbps/lane
 *	 Crop_Width :	5184	px
 *	 Crop_Height :	3880	px
 *	 Output_Width :	5184	px
 *	 Output_Height :	3880	px
 *	 Frame rate :	30.01	fps
 *	 Output format :	Raw10
 *	 H-size :	5640	px
 *	 H-blank :	456	px
 *	 V-size :	4008	line
 *	 V-blank :	128	line
 *	 Lane :	4	lane
 *	 First Pixel :	Gr	First
 */
	table_write_cmos_sensor(ctx, addr_data_pair_cap_2t7sp,
			sizeof(addr_data_pair_cap_2t7sp) / sizeof(kal_uint16));


}

static kal_uint16 addr_data_pair_video_2t7sp[] = {
	0x6028, 0x2000,
	0x0344, 0x0000,
	0x0346, 0x0008,
	0x0348, 0x144F,
	0x034A, 0x0F2F,
	0x034C, 0x0A20,
	0x034E, 0x0794,
	0x0408, 0x0004,
	0x040A, 0x0000,
	0x0900, 0x0112,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0400, 0x0000,
	0x0404, 0x0010,
	0x301E, 0x0210,
	0x0110, 0x0002,
	0x0114, 0x0300,
	0x0136, 0x1800,
	0x0300, 0x0005,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00D7,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0004,
	0x030E, 0x0059,
	0x0310, 0x0100,
	0x0312, 0x0100,
	0x0340, 0x09F0,
	0x0342, 0x2330,
	0x602A, 0x1C78,
	0x6F12, 0x8101
};

static void normal_video_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
	pr_debug("%s currefps:%d", __func__, currefps);


	/*Convert from : "2T7SP_20M_5184x3880_30fps_MIPI1680mbps.sset"*/



	/*//$MV1[MCLK:24,Width:5184,Height:3880,
	 *Format:MIPI_RAW10,mipi_lane:4,mipi_datarate:1680,pvi_pclk_inverse:0]
	 */
	/*
	 * ExtClk :	24	MHz
	 * Vt_pix_clk :	678.4	MHz
	 * MIPI_output_speed :	1680	Mbps/lane
	 * Crop_Width :	5184	px
	 * Crop_Height :	3880	px
	 * Output_Width :	5184	px
	 * Output_Height :	3880	px
	 * Frame rate :	30.01	fps
	 * Output format :	Raw10
	 * H-size :	5640	px
	 * H-blank :	456	px
	 * V-size :	4008	line
	 * V-blank :	128	line
	 * Lane :	4	lane
	 * First Pixel :	Gr	First
	 */

	table_write_cmos_sensor(ctx, addr_data_pair_video_2t7sp,
		sizeof(addr_data_pair_video_2t7sp) / sizeof(kal_uint16));



}

static kal_uint16 addr_data_pair_hs_2t7sp[] = {
	0x6028, 0x2000, /*new*/
	0x0344, 0x0520,
	0x0346, 0x04CC,
	0x0348, 0x0F2F,
	0x034A, 0x0A6B,
	0x034C, 0x0500,
	0x034E, 0x02D0,
	0x0408, 0x0004,
	0x040A, 0x0000,
	0x0900, 0x0112,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0400, 0x0000,
	0x0404, 0x0010,
	0x301E, 0x0210,
	0x0110, 0x0002,
	0x0114, 0x0300,
	0x0136, 0x1800,
	0x0300, 0x0005,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00D7,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0004,
	0x030E, 0x00a7,
	0x0310, 0x0100,
	0x0312, 0x0100,
	0x0340, 0x03FE,
	0x0342, 0x15E8,
	0x602A, 0x1C78,
	0x6F12, 0x8101
};

static void hs_video_setting(struct subdrv_ctx *ctx)
{
	pr_debug("%s", __func__);


	/*//VGA 120fps*/

	/*// Convert from : "Init.txt"*/
	/*check_streamoff(ctx);*/
	table_write_cmos_sensor(ctx, addr_data_pair_hs_2t7sp,
			sizeof(addr_data_pair_hs_2t7sp) / sizeof(kal_uint16));


}

static kal_uint16 addr_data_pair_slim_2t7sp[] = {
	0x6028, 0x2000,
	0x0344, 0x0520,
	0x0346, 0x04CC,
	0x0348, 0x0F2F,
	0x034A, 0x0A6B,
	0x034C, 0x0500,
	0x034E, 0x02D0,
	0x0408, 0x0004,
	0x040A, 0x0000,
	0x0900, 0x0112,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0400, 0x0000,
	0x0404, 0x0010,
	0x301E, 0x0210,
	0x0110, 0x0002,
	0x0114, 0x0300,
	0x0136, 0x1800,
	0x0300, 0x0005,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00D7,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0004,
	0x030E, 0x0059,
	0x0310, 0x0100,
	0x0312, 0x0100,
	0x0340, 0x09F0,
	0x0342, 0x2330,
	0x602A, 0x1C78,
	0x6F12, 0x8101
};

static void slim_video_setting(struct subdrv_ctx *ctx)
{
	pr_debug("%s", __func__);
	/* 1080p 60fps */

	/* Convert from : "Init.txt"*/

	table_write_cmos_sensor(ctx, addr_data_pair_slim_2t7sp,
		sizeof(addr_data_pair_slim_2t7sp) / sizeof(kal_uint16));

}

#define FOUR_CELL_SIZE 3072/*size = 3072 = 0xc00*/
static int Is_Read_4Cell;
static char Four_Cell_Array[FOUR_CELL_SIZE + 2];
static void read_4cell_from_eeprom(struct subdrv_ctx *ctx, char *data)
{
	int ret;
	int addr = 0x763;/*Start of 4 cell data*/
	char temp;

	if (Is_Read_4Cell != 1) {
		pr_debug("Need to read i2C\n");

		/* Check I2C is normal */
		ret = adaptor_i2c_rd_u8(ctx->i2c_client,
			EEPROM_READ_ID >> 1, addr, &temp);
		if (ret != 0) {
			pr_debug("iReadRegI2C error\n");
			return;
		}

		Four_Cell_Array[0] = (FOUR_CELL_SIZE & 0xff);/*Low*/
		Four_Cell_Array[1] = ((FOUR_CELL_SIZE >> 8) & 0xff);/*High*/

		/*Multi-Read*/
		adaptor_i2c_rd_p8(ctx->i2c_client,
			EEPROM_READ_ID >> 1, addr,
			&Four_Cell_Array[2], FOUR_CELL_SIZE);

		Is_Read_4Cell = 1;
	}

	if (data != NULL) {
		pr_debug("return data\n");
		memcpy(data, Four_Cell_Array, FOUR_CELL_SIZE);
	}
}

/*************************************************************************
 * FUNCTION
 *	get_imgsensor_id
 *
 * DESCRIPTION
 *	This function get the sensor ID
 *
 * PARAMETERS
 *	*sensorID : return the sensor ID
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static int get_imgsensor_id(struct subdrv_ctx *ctx, UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
//	kal_uint16 sp8spFlag = 0;

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			*sensor_id = ((read_cmos_sensor_8(ctx, 0x0000) << 8)
				      | read_cmos_sensor_8(ctx, 0x0001));

			if (*sensor_id == imgsensor_info.sensor_id) {
				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, *sensor_id);
				/* preload 4cell data */
				read_4cell_from_eeprom(ctx, NULL);
				return ERROR_NONE;

		/* 4Cell version check, 2T7 And 2T7's checking is differet
		 *	sp8spFlag = (((read_cmos_sensor(0x000C) & 0xFF) << 8)
		 *		|((read_cmos_sensor(0x000E) >> 8) & 0xFF));
		 *	pr_debug(
		 *	"sp8Flag(0x%x),0x5003 used by s5k2t7sp\n", sp8spFlag);
		 *
		 *	if (sp8spFlag == 0x5003) {
		 *		pr_debug("it is s5k2t7sp\n");
		 *		return ERROR_NONE;
		 *	}
		 *
		 *		pr_debug(
		 *	"2t7 type is 0x(%x),0x000C(0x%x),0x000E(0x%x)\n",
		 *		sp8spFlag,
		 *		read_cmos_sensor(0x000C),
		 *		read_cmos_sensor(0x000E));
		 *
		 *		*sensor_id = 0xFFFFFFFF;
		 *	return ERROR_SENSOR_CONNECT_FAIL;
		 */
			}
			pr_debug("Read sensor id fail, id: 0x%x\n",
				ctx->i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
	/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}


/*************************************************************************
 * FUNCTION
 *	open
 *
 * DESCRIPTION
 *	This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static int open(struct subdrv_ctx *ctx)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;

	pr_debug("%s", __func__);

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			sensor_id = (
		(read_cmos_sensor_8(ctx, 0x0000) << 8) | read_cmos_sensor_8(ctx, 0x0001));

			if (sensor_id == imgsensor_info.sensor_id) {
				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, sensor_id);
				break;
			}

			pr_debug("Read sensor id fail, id: 0x%x\n",
				ctx->i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail sequence write in  */
	sensor_init(ctx);


	ctx->autoflicker_en = KAL_FALSE;
	ctx->sensor_mode = IMGSENSOR_MODE_INIT;
	ctx->shutter = 0x3D0;
	ctx->gain = 0x100;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->dummy_pixel = 0;
	ctx->dummy_line = 0;
	ctx->ihdr_mode = 0;
	ctx->test_pattern = KAL_FALSE;
	ctx->current_fps = imgsensor_info.pre.max_framerate;

	return ERROR_NONE;
}				/*      open  */



/*************************************************************************
 * FUNCTION
 *	close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static int close(struct subdrv_ctx *ctx)
{
	pr_debug("E\n");

	return ERROR_NONE;
}				/*      close  */


/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *	This function start the sensor preview.
 *
 * PARAMETERS
 *	*image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s E\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_PREVIEW;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	preview_setting(ctx);
	set_mirror_flip(ctx, ctx->mirror);

	return ERROR_NONE;
}				/*      preview   */

/*************************************************************************
 * FUNCTION
 *	capture
 *
 * DESCRIPTION
 *	This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s E\n", __func__);
	ctx->sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (ctx->current_fps == imgsensor_info.cap1.max_framerate) {
	/* PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M */
		ctx->pclk = imgsensor_info.cap1.pclk;
		ctx->line_length = imgsensor_info.cap1.linelength;
		ctx->frame_length = imgsensor_info.cap1.framelength;
		ctx->min_frame_length = imgsensor_info.cap1.framelength;
		ctx->autoflicker_en = KAL_FALSE;
	} else if (ctx->current_fps == imgsensor_info.cap2.max_framerate) {
		ctx->pclk = imgsensor_info.cap2.pclk;
		ctx->line_length = imgsensor_info.cap2.linelength;
		ctx->frame_length = imgsensor_info.cap2.framelength;
		ctx->min_frame_length = imgsensor_info.cap2.framelength;
		ctx->autoflicker_en = KAL_FALSE;
	} else {

		if (ctx->current_fps != imgsensor_info.cap.max_framerate) {
			pr_debug("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				ctx->current_fps,
				imgsensor_info.cap.max_framerate / 10);
		}

		ctx->pclk = imgsensor_info.cap.pclk;
		ctx->line_length = imgsensor_info.cap.linelength;
		ctx->frame_length = imgsensor_info.cap.framelength;
		ctx->min_frame_length = imgsensor_info.cap.framelength;
		ctx->autoflicker_en = KAL_FALSE;
	}

	capture_setting(ctx, ctx->current_fps);
	set_mirror_flip(ctx, ctx->mirror);

	return ERROR_NONE;
}				/* capture(ctx) */

static kal_uint32 normal_video(struct subdrv_ctx *ctx,
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s E\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_VIDEO;
	ctx->pclk = imgsensor_info.normal_video.pclk;
	ctx->line_length = imgsensor_info.normal_video.linelength;
	ctx->frame_length = imgsensor_info.normal_video.framelength;
	ctx->min_frame_length = imgsensor_info.normal_video.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	normal_video_setting(ctx, ctx->current_fps);
	set_mirror_flip(ctx, ctx->mirror);

	return ERROR_NONE;
}				/*      normal_video   */

static kal_uint32 hs_video(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s E\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	ctx->pclk = imgsensor_info.hs_video.pclk;
	/* ctx->video_mode = KAL_TRUE; */
	ctx->line_length = imgsensor_info.hs_video.linelength;
	ctx->frame_length = imgsensor_info.hs_video.framelength;
	ctx->min_frame_length = imgsensor_info.hs_video.framelength;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	/* ctx->current_fps = 300; */
	ctx->autoflicker_en = KAL_FALSE;
	hs_video_setting(ctx);
	set_mirror_flip(ctx, ctx->mirror);

	return ERROR_NONE;
}				/*      hs_video   */

static kal_uint32 slim_video(struct subdrv_ctx *ctx,
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s E\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	ctx->pclk = imgsensor_info.slim_video.pclk;
	/* ctx->video_mode = KAL_TRUE; */
	ctx->line_length = imgsensor_info.slim_video.linelength;
	ctx->frame_length = imgsensor_info.slim_video.framelength;
	ctx->min_frame_length = imgsensor_info.slim_video.framelength;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	/* ctx->current_fps = 300; */
	ctx->autoflicker_en = KAL_FALSE;
	slim_video_setting(ctx);
	set_mirror_flip(ctx, ctx->mirror);

	return ERROR_NONE;
}				/*      slim_video       */



static int get_resolution(struct subdrv_ctx *ctx,
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	int i = 0;

	for (i = SENSOR_SCENARIO_ID_MIN; i < SENSOR_SCENARIO_ID_MAX; i++) {
		if (i < imgsensor_info.sensor_mode_num) {
			sensor_resolution->SensorWidth[i] = imgsensor_winsize_info[i].w2_tg_size;
			sensor_resolution->SensorHeight[i] = imgsensor_winsize_info[i].h2_tg_size;
		} else {
			sensor_resolution->SensorWidth[i] = 0;
			sensor_resolution->SensorHeight[i] = 0;
		}
	}
	return ERROR_NONE;
}				/*      get_resolution  */

static int get_info(struct subdrv_ctx *ctx, enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	/*pr_debug("get_info -> scenario_id = %d\n", scenario_id);*/

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* not use */
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* inverse with datasheet */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;	/* not use */
	sensor_info->SensorResetActiveHigh = FALSE;	/* not use */
	sensor_info->SensorResetDelayCount = 5;	/* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;

	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_PREVIEW] =
		imgsensor_info.pre_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_CAPTURE] =
		imgsensor_info.cap_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_VIDEO] =
		imgsensor_info.video_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO] =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_SLIM_VIDEO] =
		imgsensor_info.slim_video_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM1] =
		imgsensor_info.custom1_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM2] =
		imgsensor_info.custom2_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM3] =
		imgsensor_info.custom3_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM4] =
		imgsensor_info.custom4_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;

	/* The frame of setting sensor gain*/
	sensor_info->AESensorGainDelayFrame =
				imgsensor_info.ae_sensor_gain_delay_frame;

	sensor_info->AEISPGainDelayFrame =
				imgsensor_info.ae_ispGain_delay_frame;

	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	/* change pdaf support mode to pdaf VC mode */
	sensor_info->PDAF_Support = 0;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;	/* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;	/* not use */
	sensor_info->SensorPixelClockCount = 3;	/* not use */
	sensor_info->SensorDataLatchCount = 2;	/* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;


	return ERROR_NONE;
}				/*      get_info  */


static int control(struct subdrv_ctx *ctx, enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	ctx->current_scenario_id = scenario_id;
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		preview(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		capture(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		normal_video(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		hs_video(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		slim_video(ctx, image_window, sensor_config_data);
		break;
	default:
		pr_debug("Error ScenarioId setting");
		preview(ctx, image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}				/* control(ctx) */

static kal_uint32 set_video_mode(struct subdrv_ctx *ctx, UINT16 framerate)
{
	/* //pr_debug("framerate = %d\n ", framerate); */
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
		/* Dynamic frame rate */
		return ERROR_NONE;
	if ((framerate == 300) && (ctx->autoflicker_en == KAL_TRUE))
		ctx->current_fps = 296;
	else if ((framerate == 150) && (ctx->autoflicker_en == KAL_TRUE))
		ctx->current_fps = 146;
	else
		ctx->current_fps = framerate;
	set_max_framerate(ctx, ctx->current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(struct subdrv_ctx *ctx,
	kal_bool enable, UINT16 framerate)
{
	pr_debug("enable = %d, framerate = %d\n", enable, framerate);
	if (enable)		/* enable auto flicker */
		ctx->autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
		ctx->autoflicker_en = KAL_FALSE;
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(struct subdrv_ctx *ctx,
	enum MSDK_SCENARIO_ID_ENUM scenario_id,	MUINT32 framerate)
{
	kal_uint32 frame_length;

	pr_debug("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		frame_length = imgsensor_info.pre.pclk
			/ framerate * 10 / imgsensor_info.pre.linelength;

		ctx->dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;

		ctx->frame_length =
			imgsensor_info.pre.framelength + ctx->dummy_line;

		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		else {
			/*No need to set*/
			pr_debug("frame_length %d < shutter %d",
				ctx->frame_length, ctx->shutter);
		}
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk
		    / framerate * 10 / imgsensor_info.normal_video.linelength;

		ctx->dummy_line =
	    (frame_length > imgsensor_info.normal_video.framelength)
	  ? (frame_length - imgsensor_info.normal_video.  framelength) : 0;

		ctx->frame_length =
		 imgsensor_info.normal_video.framelength + ctx->dummy_line;

		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		else {
			/*No need to set*/
			pr_debug("frame_length %d < shutter %d",
				ctx->frame_length, ctx->shutter);
		}
		break;

	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
	if (ctx->current_fps == imgsensor_info.cap1.max_framerate) {

		frame_length = imgsensor_info.cap1.pclk
			/ framerate * 10 / imgsensor_info.cap1.linelength;

		ctx->dummy_line =
		      (frame_length > imgsensor_info.cap1.framelength)
		    ? (frame_length - imgsensor_info.cap1.  framelength) : 0;

		ctx->frame_length =
			imgsensor_info.cap1.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
	} else if (ctx->current_fps == imgsensor_info.cap2.max_framerate) {
		frame_length = imgsensor_info.cap2.pclk
			/ framerate * 10 / imgsensor_info.cap2.linelength;
		ctx->dummy_line =
		      (frame_length > imgsensor_info.cap2.framelength)
		    ? (frame_length - imgsensor_info.cap2.  framelength) : 0;

		ctx->frame_length =
			imgsensor_info.cap2.framelength + ctx->dummy_line;

		ctx->min_frame_length = ctx->frame_length;
	} else {
		if (ctx->current_fps != imgsensor_info.cap.max_framerate)
			pr_debug("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				framerate,
				imgsensor_info.cap.max_framerate / 10);

		frame_length = imgsensor_info.cap.pclk
			/ framerate * 10 / imgsensor_info.cap.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.cap.framelength)
			? (frame_length - imgsensor_info.cap.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.cap.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
	}
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		else {
			/*No need to set*/
			pr_debug("frame_length %d < shutter %d",
				ctx->frame_length, ctx->shutter);
		}
		break;

	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk
			/ framerate * 10 / imgsensor_info.hs_video.linelength;
		ctx->dummy_line =
		  (frame_length > imgsensor_info.hs_video.framelength)
		? (frame_length - imgsensor_info.hs_video.  framelength) : 0;

		ctx->frame_length =
		    imgsensor_info.hs_video.framelength + ctx->dummy_line;

		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		else {
			/*No need to set*/
			pr_debug("frame_length %d < shutter %d",
				ctx->frame_length, ctx->shutter);
		}
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk
			/ framerate * 10 / imgsensor_info.slim_video.linelength;

		ctx->dummy_line =
		  (frame_length > imgsensor_info.slim_video.framelength)
		? (frame_length - imgsensor_info.slim_video.  framelength) : 0;

		ctx->frame_length =
		  imgsensor_info.slim_video.framelength + ctx->dummy_line;

		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		else {
			/*No need to set*/
			pr_debug("frame_length %d < shutter %d",
				ctx->frame_length, ctx->shutter);
		}
		break;

	default:		/* coding with  preview scenario by default */
		frame_length = imgsensor_info.pre.pclk
			/ framerate * 10 / imgsensor_info.pre.linelength;

		ctx->dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;

		ctx->frame_length =
			imgsensor_info.pre.framelength + ctx->dummy_line;

		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		else {
			/*No need to set*/
			pr_debug("frame_length %d < shutter %d",
				ctx->frame_length, ctx->shutter);
		}
		pr_debug("error scenario_id = %d, we use preview scenario\n",
		scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(struct subdrv_ctx *ctx,
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	/*pr_debug("scenario_id = %d\n", scenario_id);*/

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(struct subdrv_ctx *ctx, kal_bool enable)
{
	pr_debug("enable: %d\n", enable);

	if (enable) {
/* 0 : Normal, 1 : Solid Color, 2 : Color Bar, 3 : Shade Color Bar, 4 : PN9 */
		write_cmos_sensor(ctx, 0x0600, 0x0002);
	} else {
		write_cmos_sensor(ctx, 0x0600, 0x0000);
	}
	ctx->test_pattern = enable;
	return ERROR_NONE;
}
static kal_uint32 get_sensor_temperature(struct subdrv_ctx *ctx)
{
	UINT8 temperature;
	INT32 temperature_convert;

	temperature = read_cmos_sensor_8(ctx, 0x013a);

	if (temperature >= 0x0 && temperature <= 0x78)
		temperature_convert = temperature;
	else
		temperature_convert = -1;

	/*pr_info("temp_c(%d), read_reg(%d), enable %d\n",
	 *	temperature_convert, temperature, read_cmos_sensor_8(ctx, 0x0138));
	 */

	return temperature_convert;
}


static int feature_control(struct subdrv_ctx *ctx, MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;

	/* SET_PD_BLOCK_INFO_T *PDAFinfo; */
	/* SENSOR_VC_INFO_STRUCT *pvcinfo; */
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/*pr_debug("feature_id = %d\n", feature_id);*/
	switch (feature_id) {
	case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
		if ((void *)(uintptr_t) (*(feature_data + 1)) == NULL) {
			*(feature_data + 0) =
				sizeof(s5k2t7sp_ana_gain_table);
		} else {
			memcpy((void *)(uintptr_t) (*(feature_data + 1)),
			(void *)s5k2t7sp_ana_gain_table,
			sizeof(s5k2t7sp_ana_gain_table));
		}
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.pclk;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.pclk;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.pclk;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.pclk;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.cap.framelength << 16)
				+ imgsensor_info.cap.linelength;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.normal_video.framelength << 16)
				+ imgsensor_info.normal_video.linelength;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.hs_video.framelength << 16)
				+ imgsensor_info.hs_video.linelength;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.slim_video.framelength << 16)
				+ imgsensor_info.slim_video.linelength;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;

	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = ctx->line_length;
		*feature_return_para_16 = ctx->frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = ctx->pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(ctx, *feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
	/* night_mode((BOOL) *feature_data); no need to implement this mode */
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain(ctx, (UINT32) * feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;

	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor_8(ctx, sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		break;

	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor_8(ctx, sensor_reg_data->RegAddr);
		break;

	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or
		 * just return LENS_DRIVER_ID_DO_NOT_CARE
		 */
		/* if EEPROM does not exist in camera module. */
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(ctx, *feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(ctx, feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode(ctx, (BOOL) (*feature_data_16),
					*(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(ctx,
	    (enum MSDK_SCENARIO_ID_ENUM) *feature_data, *(feature_data + 1));
		break;

	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(ctx,
			(enum MSDK_SCENARIO_ID_ENUM) *(feature_data),
			  (MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode(ctx, (BOOL) (*feature_data));
		break;

	/* for factory mode auto testing */
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		pr_debug("current fps :%d\n", *feature_data_32);
		ctx->current_fps = (UINT16)*feature_data_32;
		break;
	case SENSOR_FEATURE_SET_HDR:
		pr_debug("ihdr enable :%d\n", *feature_data_32);
		ctx->ihdr_mode = (UINT8)*feature_data_32;
		break;

	case SENSOR_FEATURE_GET_CROP_INFO:
		/* pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
		 *	(UINT32) *feature_data);
		 */

		wininfo =
	(struct SENSOR_WINSIZE_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[1],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[3],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[4],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));

/* ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),
 * (UINT16)*(feature_data+2));
 */
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		pr_debug("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1));
/* ihdr_write_shutter((UINT16)*feature_data,(UINT16)*(feature_data+1)); */
		break;

	case SENSOR_FEATURE_GET_4CELL_DATA:/*get 4 cell data from eeprom*/
	{
		int type = (kal_uint16)(*feature_data);
		char *data = (char *)(uintptr_t)(*(feature_data+1));

		if (type == FOUR_CELL_CAL_TYPE_XTALK_CAL) {
			pr_debug("Read Cross Talk Start");
			read_4cell_from_eeprom(ctx, data);
			pr_debug("Read Cross Talk = %02x %02x %02x %02x %02x %02x\n",
				(UINT16)data[0], (UINT16)data[1],
				(UINT16)data[2], (UINT16)data[3],
				(UINT16)data[4], (UINT16)data[5]);
		}
		break;
	}


		/******************** PDAF START >>> *********/
		/*
		 * case SENSOR_FEATURE_GET_PDAF_INFO:
		 * pr_debug("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
		 * (UINT16)*feature_data);
		 * PDAFinfo =
		 * (SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		 * switch (*feature_data) {
		 * case SENSOR_SCENARIO_ID_NORMAL_CAPTURE: //full
		 * case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		 * case SENSOR_SCENARIO_ID_NORMAL_PREVIEW: //2x2 binning
		 * memcpy((void *)PDAFinfo,
		 * (void *)&imgsensor_pd_info,
		 * sizeof(SET_PD_BLOCK_INFO_T)); //need to check
		 * break;
		 * case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		 * case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		 * default:
		 * break;
		 * }
		 * break;
		 * case SENSOR_FEATURE_GET_VC_INFO:
		 * pr_debug("SENSOR_FEATURE_GET_VC_INFO %d\n",
		 * (UINT16)*feature_data);
		 * pvcinfo =
		 * (SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		 * switch (*feature_data_32) {
		 * case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		 * memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[2],
		 * sizeof(SENSOR_VC_INFO_STRUCT));
		 * break;
		 * case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		 * memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[1],
		 * sizeof(SENSOR_VC_INFO_STRUCT));
		 * break;
		 * case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		 * default:
		 * memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[0],
		 * sizeof(SENSOR_VC_INFO_STRUCT));
		 * break;
		 * }
		 * break;
		 * case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		 * pr_debug(
		 * "SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
		 * (UINT16)*feature_data);
		 * //PDAF capacity enable or not
		 * switch (*feature_data) {
		 * case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		 * (MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
		 * break;
		 * case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		 * *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
		 * // video & capture use same setting
		 * break;
		 * case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		 * *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
		 * break;
		 * case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		 * //need to check
		 * *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
		 * break;
		 * case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		 * *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
		 * break;
		 * default:
		 * *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
		 * break;
		 * }
		 * break;
		 * case SENSOR_FEATURE_GET_PDAF_DATA: //get cal data from eeprom
		 * pr_debug("SENSOR_FEATURE_GET_PDAF_DATA\n");
		 * read_2T7_eeprom((kal_uint16 )(*feature_data),
		 * (char*)(uintptr_t)(*(feature_data+1)),
		 * (kal_uint32)(*(feature_data+2)));
		 * pr_debug("SENSOR_FEATURE_GET_PDAF_DATA success\n");
		 * break;
		 * case SENSOR_FEATURE_SET_PDAF:
		 * pr_debug("PDAF mode :%d\n", *feature_data_16);
		 * ctx->pdaf_mode= *feature_data_16;
		 * break;
		 */
		/******************** PDAF END   <<< *********/
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(ctx, KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(ctx, *feature_data);
		streaming_control(ctx, KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*feature_return_para_32 = 1; /*BINNING_NONE*/
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			*feature_return_para_32 = 2; /*BINNING_AVERAGED*/
			break;
		}
		pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			*feature_return_para_32);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = get_sensor_temperature(ctx);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:

		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		break;
	default:
		break;
	}

	return ERROR_NONE;
}				/*      feature_control(ctx)  */

static const struct subdrv_ctx defctx = {

	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_max = BASEGAIN * 16,
	.ana_gain_min = BASEGAIN,
	.ana_gain_step = 1,
	.exposure_def = 0x3D0,
	.exposure_max = 0xffff - 8,
	.exposure_min = 4,
	.exposure_step = 1,
	.frame_time_delay_frame = 2,
	.margin = 8,
	.max_frame_length = 0xffff,

	.mirror = IMAGE_NORMAL,	/* mirrorflip information */

	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.sensor_mode = IMGSENSOR_MODE_INIT,

	.shutter = 0x3D0,	/* current shutter */
	.gain = BASEGAIN * 4,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 0,	/* full size current fps : 24fps for PIP,
				 * 30fps for Normal or ZSD
				 */

	/* auto flicker enable: KAL_FALSE for disable auto flicker,
	 * KAL_TRUE for enable auto flicker
	 */
	.autoflicker_en = KAL_FALSE,

		/* test pattern mode or not.
		 * KAL_FALSE for in test pattern mode,
		 * KAL_TRUE for normal output
		 */
	.test_pattern = KAL_FALSE,

	/* current scenario id */
	.current_scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW,

	/* sensor need support LE, SE with HDR feature */
	.ihdr_mode = KAL_FALSE,
	.i2c_write_id = 0x20,	/* record current sensor's i2c write id */
};

static int get_temp(struct subdrv_ctx *ctx, int *temp)
{
	*temp = get_sensor_temperature(ctx) * 1000;
	return 0;
}

static int init_ctx(struct subdrv_ctx *ctx,
		struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(ctx, &defctx, sizeof(*ctx));
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}

static struct subdrv_ops ops = {
	.get_id = get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = open,
	.get_info = get_info,
	.get_resolution = get_resolution,
	.control = control,
	.feature_control = feature_control,
	.close = close,
	.get_temp = get_temp,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_MCLK, 24, 0},
	{HW_ID_RST, 0, 0},
	{HW_ID_DOVDD, 1800000, 0},
	{HW_ID_AVDD, 2800000, 0},
	{HW_ID_DVDD, 1000000, 0},
	{HW_ID_MCLK_DRIVING_CURRENT, 8, 1},
	{HW_ID_RST, 1, 2},
};

const struct subdrv_entry s5k2t7sp_mipi_raw_entry = {
	.name = "s5k2t7sp_mipi_raw",
	.id = S5K2T7SP_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

