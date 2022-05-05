// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 IMX576mipi_Sensor.c
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

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include <linux/slab.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define_v4l2.h"
#include "kd_imgsensor_errcode.h"

#include "imx576mipiraw_Sensor.h"
#include "imx576_ana_gain_table.h"

#include "adaptor-subdrv.h"
#include "adaptor-i2c.h"

#define read_cmos_sensor_8(...) subdrv_i2c_rd_u8(__VA_ARGS__)
#define read_cmos_sensor(...) subdrv_i2c_rd_u16(__VA_ARGS__)
#define write_cmos_sensor_8(...) subdrv_i2c_wr_u8(__VA_ARGS__)
#define imx576_table_write_cmos_sensor(...) subdrv_i2c_wr_regs_u8(__VA_ARGS__)
#define imx576_seq_write_cmos_sensor(...) subdrv_i2c_wr_p16(__VA_ARGS__)

#define PFX "IMX576_camera_sensor"
#define NO_USE_3HDR 1
#define LSC_DEBUG 0

#define MULTI_WRITE 1
#if MULTI_WRITE
#define I2C_BUFFER_LEN 765 /* trans# max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN 3
#endif

static kal_uint32 streaming_control(struct subdrv_ctx *ctx, kal_bool enable);
static kal_uint16 imx576_HDR_synthesis = 1;

#define LOG_INF(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = IMX576_SENSOR_ID,

	.checksum_value = 0x4cb91a94,

	.pre = {
		#if NO_USE_3HDR
		/*setting for normal binning*/
		.pclk = 420000000,
		.linelength = 3160,
		.framelength = 4430,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2880,
		.grabwindow_height = 2156,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 416000000,
		.max_framerate = 300,
		#else
		/*for 3hdr setting*/
		.pclk = 830400000,
		.linelength = 6144,
		.framelength = 4505,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2880,
		.grabwindow_height = 2156,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 417200000,
		.max_framerate = 300,
		#endif
	},
	.pre_3HDR = {
		/*for 3hdr setting*/
		.pclk = 830400000,//820800000,
		.linelength = 6144,
		.framelength = 4505,//4453,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2880,
		.grabwindow_height = 2156,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 417200000,//444400000,
		.max_framerate = 300,
	},
	.cap = {
		/*setting for remosaic*/
		.pclk = 657600000,
		.linelength = 6144,
		.framelength = 4459,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 5760,
		.grabwindow_height = 4312,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 648800000,
		.max_framerate = 240,
	},
	.normal_video = {
		#if NO_USE_3HDR
		/*setting for normal binning*/
		.pclk = 420000000,
		.linelength = 3160,
		.framelength = 4430,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2880,
		.grabwindow_height = 2156,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 416000000,
		.max_framerate = 300,
		#else
		/*for 3hdr setting*/
		.pclk = 820800000,
		.linelength = 6144,
		.framelength = 4453,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2880,
		.grabwindow_height = 2156,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 444400000,
		.max_framerate = 300,
		#endif
	},
	.hs_video = {
		/*setting for normal binning*/
		.pclk = 420000000,
		.linelength = 3160,
		.framelength = 4430,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2880,
		.grabwindow_height = 2156,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 416000000,
		.max_framerate = 300,
	},
	.slim_video = {
		/*setting for normal binning*/
		.pclk = 420000000,
		.linelength = 3160,
		.framelength = 4430,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2880,
		.grabwindow_height = 2156,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 416000000,
		.max_framerate = 300,
	},
	.margin = 61,
	.min_shutter = 6,
	.min_gain = BASEGAIN, /*1x gain*/
	.max_gain = BASEGAIN * 16, /*16x gain*/
	.min_gain_iso = 100,
	.gain_step = 1,
	.gain_type = 0,

	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	  /* 1, support; 0,not support */
	.ihdr_le_firstline = 0,  /* 1,le first ; 0, se first */
	.temperature_support = 1,/* 1, support; 0,not support */
	.sensor_mode_num = 5,	  /* support sensor mode num */

	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,
	.frame_time_delay_frame = 3,
	.isp_driving_current = ISP_DRIVING_8MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode = 0,
	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x34, 0x20, 0xff},
	.i2c_speed = 1000, /* i2c read/write speed */
};




/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
{ 5760,  4312,  0,  0,  5760,  4312,  2880,  2156,
0000,  0000,  2880,  2156,  0,  0,  2880,  2156},/*Preview*/
{ 5760,  4312,  0,  0,  5760,  4312,  5760,  4312,
0000,  0000,  5760,  4312,  0,  0,  5760,  4312}, /* remosic*/
{ 5760,  4312,  0,  0,  5760,  4312,  2880,  2156,
0000,  0000,  2880,  2156,  0,  0,  2880,  2156}, /* video */
{ 5760,  4312,  0,  0,  5760,  4312,  2880,  2156,
0000,  0000,  2880,  2156,  0,  0,  2880,  2156}, /*hs video*/
{ 5760,  4312,  0,  0,  5760,  4312,  2880,  2156,
0000,  0000,  2880,  2156,  0,  0,  2880,  2156}, /* slim video*/
};

static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3] = {
	/* Preview mode setting */
	{0x05, 0x0a, 0x00, 0x08, 0x40, 0x00,
	/*VC0:raw, VC1:Embedded data*/
	 0x00, 0x2b, 0x0B40, 0x086C, 0x00, 0x12, 0x0E10, 0x0002,
	/*VC2:Y HIST(3HDR), VC3:AE HIST(3HDR)*/
	 0x00, 0x31, 0x0E10, 0x0001, 0x00, 0x32, 0x0E10, 0x0001,
	/*VC4:Flicker(3HDR), VC5:no data*/
	 0x00, 0x33, 0x0E10, 0x0001, 0x00, 0x00, 0x0000, 0x0000},
	/* Capture mode setting */
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1680, 0x10D8, 0x00, 0x00, 0x0000, 0x0000,
	 0x00, 0x00, 0x0000, 0x0000, 0x00, 0x00, 0x0000, 0x0000},
	/* Video mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 /*VC0:raw, VC1:Embedded data*/
	 0x00, 0x2b, 0x0B40, 0x086C, 0x00, 0x12, 0x0E10, 0x0002,
	/*VC2:Y HIST(3HDR), VC3:AE HIST(3HDR)*/
	 0x00, 0x31, 0x0E10, 0x0001, 0x00, 0x32, 0x0E10, 0x0001,
	/*VC4:Flicker(3HDR), VC5:no data*/
	 0x00, 0x33, 0x0E10, 0x0001, 0x00, 0x00, 0x0000, 0x0000}
};

static void set_dummy(struct subdrv_ctx *ctx)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n",
		ctx->dummy_line, ctx->dummy_pixel);

	write_cmos_sensor_8(ctx, 0x0104, 0x01);
	write_cmos_sensor_8(ctx, 0x0340, ctx->frame_length >> 8);
	write_cmos_sensor_8(ctx, 0x0341, ctx->frame_length & 0xFF);
	write_cmos_sensor_8(ctx, 0x0342, ctx->line_length >> 8);
	write_cmos_sensor_8(ctx, 0x0343, ctx->line_length & 0xFF);
	write_cmos_sensor_8(ctx, 0x0104, 0x00);
}	/*	set_dummy  */


static void set_max_framerate(struct subdrv_ctx *ctx,
		UINT16 framerate, kal_bool min_framelength_en)
{

	kal_uint32 frame_length = ctx->frame_length;

	LOG_INF("framerate = %d, min framelength should enable %d\n",
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
}	/*	set_max_framerate  */

static void write_shutter(struct subdrv_ctx *ctx, kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;


	//if (shutter > ctx->min_frame_length - imgsensor_info.margin)
	//	ctx->frame_length = shutter + imgsensor_info.margin;
	//else
	ctx->frame_length = ctx->min_frame_length;
	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;

	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	if (ctx->autoflicker_en) {
		realtime_fps =
	  ctx->pclk / ctx->line_length * 10 / ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(ctx, 146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor_8(ctx, 0x0104, 0x01);
			write_cmos_sensor_8(ctx, 0x0340,
					    ctx->frame_length >> 8);
			write_cmos_sensor_8(ctx, 0x0341,
					    ctx->frame_length & 0xFF);
			write_cmos_sensor_8(ctx, 0x0104, 0x00);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor_8(ctx, 0x0104, 0x01);
		write_cmos_sensor_8(ctx, 0x0340, ctx->frame_length >> 8);
		write_cmos_sensor_8(ctx, 0x0341, ctx->frame_length & 0xFF);
		write_cmos_sensor_8(ctx, 0x0104, 0x00);
	}

	/* Update Shutter */
	write_cmos_sensor_8(ctx, 0x0104, 0x01);
	write_cmos_sensor_8(ctx, 0x0350, 0x01); /* Enable auto extend */
	write_cmos_sensor_8(ctx, 0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0203, shutter  & 0xFF);
	write_cmos_sensor_8(ctx, 0x0104, 0x00);
	LOG_INF("Exit! shutter =%d, framelength =%d\n",
		shutter, ctx->frame_length);

}	/*	write_shutter  */



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
}	/*	set_shutter */

static kal_uint16 gain2reg(struct subdrv_ctx *ctx, const kal_uint32 gain)
{
	kal_uint16 reg_gain;

	reg_gain = 1024 - (1024*64)/gain;

	/*LOG_INF("imx576 gain =%d, reg_gain =%d\n", gain, reg_gain);*/

	return reg_gain;
}

/*write AWB gain to sensor*/
static void feedback_awbgain(struct subdrv_ctx *ctx,
		kal_uint32 r_gain, kal_uint32 b_gain)
{
	UINT32 r_gain_int = 0;
	UINT32 b_gain_int = 0;

	r_gain_int = r_gain / 512;
	b_gain_int = b_gain / 512;

	write_cmos_sensor_8(ctx, 0x0104, 0x01);

	/*write r_gain*/
	write_cmos_sensor_8(ctx, 0x0B90, r_gain_int);
	write_cmos_sensor_8(ctx, 0x0B91,
		(((r_gain*100) / 512) - (r_gain_int * 100)) * 2);

	/*write _gain*/
	write_cmos_sensor_8(ctx, 0x0B92, b_gain_int);
	write_cmos_sensor_8(ctx, 0x0B93,
		(((b_gain * 100) / 512) - (b_gain_int * 100)) * 2);

	write_cmos_sensor_8(ctx, 0x0104, 0x00);
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

	if (gain < imgsensor_info.min_gain || gain > imgsensor_info.max_gain) {
		LOG_INF("Error gain setting");

		if (gain < imgsensor_info.min_gain)
			gain = imgsensor_info.min_gain;
		else
			gain = imgsensor_info.max_gain;
	}


	reg_gain = gain2reg(ctx, gain);
	ctx->gain = reg_gain;
	LOG_INF("gain = %d, reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor_8(ctx, 0x0104, 0x01);
	write_cmos_sensor_8(ctx, 0x0204, (reg_gain>>8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0205, reg_gain & 0xFF);
	write_cmos_sensor_8(ctx, 0x0104, 0x00);

	return gain;
}	/*	set_gain  */

static kal_uint16 imx576_init_setting[] = {
	0x0136, 0x18,
	0x0137, 0x00,
	0x3C7E, 0x02,
	0x3C7F, 0x02,
	0xAE09, 0x04,
	0xAE0A, 0x16,
	0xAF05, 0x18,
	0x380C, 0x00,
	0x3C00, 0x10,
	0x3C01, 0x10,
	0x3C02, 0x10,
	0x3C03, 0x10,
	0x3C04, 0x10,
	0x3C05, 0x01,
	0x3C08, 0xFF,
	0x3C09, 0xFF,
	0x3C0A, 0x01,
	0x3C0D, 0xFF,
	0x3C0E, 0xFF,
	0x3C0F, 0x20,
	0x3F89, 0x01,
	0x4430, 0x00,
	0x4B8E, 0x18,
	0x4B8F, 0x10,
	0x4BA8, 0x08,
	0x4BAA, 0x08,
	0x4BAB, 0x08,
	0x4BC9, 0x10,
	0x5511, 0x01,
	0x560B, 0x5B,
	0x56A7, 0x60,
	0x5B3B, 0x60,
	0x5BA7, 0x60,
	0x6002, 0x00,
	0x6014, 0x01,
	0x6118, 0x0A,
	0x6122, 0x0A,
	0x6128, 0x0A,
	0x6132, 0x0A,
	0x6138, 0x0A,
	0x6142, 0x0A,
	0x6148, 0x0A,
	0x6152, 0x0A,
	0x617B, 0x04,
	0x617E, 0x04,
	0x6187, 0x04,
	0x618A, 0x04,
	0x6193, 0x04,
	0x6196, 0x04,
	0x619F, 0x04,
	0x61A2, 0x04,
	0x61AB, 0x04,
	0x61AE, 0x04,
	0x61B7, 0x04,
	0x61BA, 0x04,
	0x61C3, 0x04,
	0x61C6, 0x04,
	0x61CF, 0x04,
	0x61D2, 0x04,
	0x61DB, 0x04,
	0x61DE, 0x04,
	0x61E7, 0x04,
	0x61EA, 0x04,
	0x61F3, 0x04,
	0x61F6, 0x04,
	0x61FF, 0x04,
	0x6202, 0x04,
	0x620B, 0x04,
	0x620E, 0x04,
	0x6217, 0x04,
	0x621A, 0x04,
	0x6223, 0x04,
	0x6226, 0x04,
	0x671D, 0x00,
	0x6725, 0x00,
	0x6738, 0x03,
	0x673B, 0x01,
	0x6B0B, 0x02,
	0x6B0C, 0x01,
	0x6B0D, 0x05,
	0x6B0F, 0x04,
	0x6B10, 0x02,
	0x6B11, 0x06,
	0x6B12, 0x03,
	0x6B13, 0x07,
	0x6B14, 0x0D,
	0x6B15, 0x09,
	0x6B16, 0x0C,
	0x6B17, 0x08,
	0x6B18, 0x0E,
	0x6B19, 0x0A,
	0x6B1A, 0x0F,
	0x6B1B, 0x0B,
	0x6B1C, 0x01,
	0x6B1D, 0x05,
	0x6B1F, 0x04,
	0x6B20, 0x02,
	0x6B21, 0x06,
	0x6B22, 0x03,
	0x6B23, 0x07,
	0x6B24, 0x0D,
	0x6B25, 0x09,
	0x6B26, 0x0C,
	0x6B27, 0x08,
	0x6B28, 0x0E,
	0x6B29, 0x0A,
	0x6B2A, 0x0F,
	0x6B2B, 0x0B,
	0x746E, 0x01,
	0x7501, 0x1D,
	0x7505, 0x3D,
	0x7508, 0x49,
	0x7509, 0x0D,
	0x750A, 0x8A,
	0x750B, 0x0C,
	0x750C, 0x4D,
	0x750D, 0x2D,
	0x750E, 0x8E,
	0x750F, 0x2C,
	0x7948, 0x01,
	0x7949, 0x06,
	0x794B, 0x04,
	0x794C, 0x04,
	0x794D, 0x3A,
	0x7951, 0x00,
	0x7952, 0x01,
	0x7955, 0x00,
	0x9004, 0x10,
	0x9200, 0xA0,
	0x9201, 0xA7,
	0x9202, 0xA0,
	0x9203, 0xAA,
	0x9204, 0xA0,
	0x9205, 0xAD,
	0x9206, 0xA0,
	0x9207, 0xB0,
	0x9208, 0xA0,
	0x9209, 0xB3,
	0x920A, 0xB7,
	0x920B, 0x34,
	0x920C, 0xB7,
	0x920D, 0x36,
	0x920E, 0xB7,
	0x920F, 0x37,
	0x9210, 0xB7,
	0x9211, 0x38,
	0x9212, 0xB7,
	0x9213, 0x39,
	0x9214, 0xB7,
	0x9215, 0x3A,
	0x9216, 0xB7,
	0x9217, 0x3C,
	0x9218, 0xB7,
	0x9219, 0x3D,
	0x921A, 0xB7,
	0x921B, 0x3E,
	0x921C, 0xB7,
	0x921D, 0x3F,
	0x921E, 0x7F,
	0x921F, 0x77,
	0x9816, 0x14,
	0x9865, 0x8C,
	0x9866, 0x64,
	0x9867, 0x50,
	0x9990, 0x0B,
	0x9991, 0x0B,
	0x9992, 0x0B,
	0x9993, 0x0B,
	0x9994, 0x0B,
	0x9995, 0x0D,
	0x9996, 0x0B,
	0x99AF, 0x0F,
	0x99B0, 0x0F,
	0x99B1, 0x0F,
	0x99B2, 0x0F,
	0x99B3, 0x0F,
	0x99E1, 0x0F,
	0x99E2, 0x0F,
	0x99E3, 0x0F,
	0x99E4, 0x0F,
	0x99E5, 0x0F,
	0x99E6, 0x0F,
	0x99E7, 0x0F,
	0x99E8, 0x0F,
	0x99E9, 0x0F,
	0x99EA, 0x0F,
	0xAE1B, 0x04,
	0xAE1C, 0x03,
	0xAE1D, 0x03,
	0xE286, 0x31,
	0xE2A6, 0x32,
	0xE2C6, 0x33,
	/*image quality reg settings*/
	0x4038, 0x00,
	0x9856, 0xA0,
	0x9857, 0x78,
	0x9858, 0x64,
	0x986E, 0x64,
	0x9870, 0x3C,
	0x993A, 0x0E,
	0x993B, 0x0E,
	0x9953, 0x08,
	0x9954, 0x08,
	0x996B, 0x0F,
	0x996D, 0x0F,
	0x996F, 0x0F,
	0x9981, 0x00,
	0x9982, 0x00,
	0x9986, 0x00,
	0x9987, 0x00,
	0x998E, 0x0F,
	0xA101, 0x01,
	0xA103, 0x01,
	0xA105, 0x01,
	0xA107, 0x01,
	0xA109, 0x01,
	0xA10B, 0x01,
	0xA10D, 0x01,
	0xA10F, 0x01,
	0xA111, 0x01,
	0xA113, 0x01,
	0xA115, 0x01,
	0xA117, 0x01,
	0xA119, 0x01,
	0xA11B, 0x01,
	0xA11D, 0x01,
	0xA21E, 0x06,
	0xA21F, 0x13,
	0xA220, 0x13,
	0xA31E, 0x00,
	0xA31F, 0x20,
	0xA6A9, 0x02,
	0xA6AD, 0x02,
	0xA75D, 0x00,
	0xA75F, 0x00,
	0xA763, 0x00,
	0xA765, 0x00,
	0xA831, 0x56,
	0xA832, 0x2B,
	0xA833, 0x55,
	0xA834, 0x55,
	0xA835, 0x16,
	0xA837, 0x51,
	0xA838, 0x34,
	0xA854, 0x58,
	0xA855, 0x49,
	0xA856, 0x45,
	0xA857, 0x02,
	0xA858, 0x02,
	0xA85A, 0x32,
	0xA85B, 0x19,
	0xA85C, 0x12,
	0xA85D, 0x02,
	0xA85E, 0x02,
	0xA931, 0x13,
	0xA937, 0x04,
	0xA93D, 0x92,
	0xA943, 0x3F,
	0xA949, 0x64,
	0xA94F, 0x12,
	0xA955, 0x04,
	0xA95B, 0x68,
	0xAA58, 0x00,
	0xAA59, 0x01,
	0xAB03, 0x10,
	0xAB04, 0x10,
	0xAB05, 0x10,
	0xAC72, 0x01,
	0xAC73, 0x26,
	0xAC74, 0x01,
	0xAC75, 0x26,
	0xAC76, 0x00,
	0xAC77, 0xC4,
	0xAD6A, 0x03,
	0xAD6B, 0xFF,
	0xAD77, 0x00,
	0xAD82, 0x03,
	0xAD83, 0xFF,
	0xAE06, 0x04,
	0xAE07, 0x16,
	0xAE08, 0xFF,
	0xAE0B, 0xFF,
	0xAF01, 0x04,
	0xAF03, 0x0A,
	0xB048, 0x0A,
	0xE8DA, 0x00,
	0xE8DD, 0x00,
	0xE8E3, 0x00,
	0xE8EC, 0x00,
	0xE8EF, 0x00,
	0xE8F0, 0x00,
	0xE8F1, 0x00,
	0xE8F2, 0x00,
	0xE8F3, 0x05,
	0xE918, 0x00,
};

static void sensor_init(struct subdrv_ctx *ctx)
{
	LOG_INF("%s enter\n", __func__);

	imx576_table_write_cmos_sensor(ctx, imx576_init_setting,
		sizeof(imx576_init_setting) / sizeof(kal_uint16));

	/*enable temperature sensor, TEMP_SEN_CTL:*/
	write_cmos_sensor_8(ctx, 0x0138, 0x01);
}	/*	sensor_init  */


static kal_uint16 imx576_preview_setting_3HDR[] = {
	/*mode setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x0342, 0x18,
	0x0343, 0x00,
	0x0340, 0x11,
	0x0341, 0x99,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x16,
	0x0349, 0x7F,
	0x034A, 0x10,
	0x034B, 0xD7,
	0x0220, 0x63,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x0A, /*[1:0]=0:Averaged*/
	0x3140, 0x04,
	0x3246, 0x01,
	0x3247, 0x01,
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x0B,
	0x040D, 0x40,
	0x040E, 0x08,
	0x040F, 0x6C,
	0x034C, 0x0B,
	0x034D, 0x40,
	0x034E, 0x08,
	0x034F, 0x6C,
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x5A,
	0x030B, 0x02,
	0x030D, 0x0C,
	0x030E, 0x04,
	0x030F, 0x13,
	0x0310, 0x01,
	0x0B06, 0x00,
	0x3620, 0x00,
	0x3F0C, 0x00,
	0x3F14, 0x01,
	0x3F80, 0x03,
	0x3F81, 0xCA,
	0x3FFC, 0x00,
	0x3FFD, 0x6A,
	0x0202, 0x07,
	0x0203, 0xD0,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3FE0, 0x03,
	0x3FE1, 0xE8,
	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3FE2, 0x00,
	0x3FE3, 0x00,
	0x3FE4, 0x01,
	0x3FE5, 0x00,
	/*3ex HDR setting*/
	0x323A, 0x01,
	0x323B, 0x01,
	0x323C, 0x01,
	0x37E0, 0x00,
	0x37E1, 0x00,
	0x37E2, 0x00,
	0x37E3, 0x00,
	0x37E4, 0x0B,
	0x37E5, 0x40,
	0x37E6, 0x08,
	0x37E7, 0x6C,
	0x37F0, 0x00,
	0x37F1, 0x00,
	0x37F2, 0x00,
	0x37F3, 0x00,
	0x37F4, 0x02,
	0x37F5, 0xD0,
	0x37F6, 0x00,
	0x37F7, 0x16,
	0x37F8, 0x00,
	0xAE27, 0x05,
	0xAE28, 0x05,
	0xAE29, 0x05,
	0x3C00, 0x10,
	0x3C01, 0x10,
	0x3C02, 0x10,
	0x3C03, 0x10,
	0x3C04, 0x10,
	0x7343, 0x00,
	0x3C05, 0x00,
	0x3C0A, 0x01,
	0x3C06, 0x00,
	0x3C07, 0x00,
	0x3C08, 0x03,
	0x3C09, 0xFF,
	0x3C0B, 0x00,
	0x3C0C, 0x00,
	0x3C0D, 0xFF,
	0x3C0E, 0xFF,
	0x0B00, 0x01,
	0x380C, 0x00,
	0x380D, 0x80,
	0xA101, 0x01,
	0xA103, 0x01,
	0xA105, 0x01,
	0xA107, 0x01,
	0xA109, 0x01,
	0xA10B, 0x01,
	0xA10D, 0x01,
	0xA10F, 0x01,
	0xA111, 0x01,
	0xA113, 0x01,
	0xA115, 0x01,
	0xA117, 0x01,
	0xA119, 0x01,
	0xA11B, 0x01,
	0xA11D, 0x01,
	0xEB06, 0x00,
	0xEB08, 0x00,
	0xEB0A, 0x00,
	0xEB12, 0x00,
	0xEB14, 0x00,
	0xEB16, 0x00,
	0xEB07, 0x08,
	0xEB09, 0x08,
	0xEB0B, 0x08,
	0xEB13, 0x10,
	0xEB15, 0x10,
	0xEB17, 0x10,
	0xA13C, 0x00,
	0xA13D, 0x20,
	0xA13E, 0x00,
	0xA13F, 0x20,
	0xA140, 0x00,
	0xA141, 0x20,
	0xA142, 0x00,
	0xA143, 0x20,
	0xA144, 0x00,
	0xA145, 0x20,
	0xA146, 0x00,
	0xA147, 0x20,
	0xA148, 0x00,
	0xA149, 0x20,
	0xA14A, 0x00,
	0xA14B, 0x20,
	0xA14C, 0x00,
	0xA14D, 0x20,
	0xA14E, 0x00,
	0xA14F, 0x20,
	0xA150, 0x00,
	0xA151, 0x20,
	0xA152, 0x00,
	0xA153, 0x20,
	0xA154, 0x00,
	0xA155, 0x20,
	0xA156, 0x00,
	0xA157, 0x20,
	0xA158, 0x00,
	0xA159, 0x20,
	0x3F22, 0x00,
	0x3F36, 0x00,
	0x3F25, 0x00,
	0x3F23, 0x00,
	0x3F38, 0x00,
	0x3F27, 0x00,
	0x3F39, 0x00,
	0x3F37, 0x00,
};

static void preview_setting_3HDR(struct subdrv_ctx *ctx)
{
	LOG_INF("E\n");

	imx576_table_write_cmos_sensor(ctx, imx576_preview_setting_3HDR,
		sizeof(imx576_preview_setting_3HDR) / sizeof(kal_uint16));

}

static kal_uint16 imx576_preview_setting[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x0342, 0x0C,
	0x0343, 0x58,
	0x0340, 0x11,
	0x0341, 0x4E,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x16,
	0x0349, 0x7F,
	0x034A, 0x10,
	0x034B, 0xD7,
	0x0220, 0x62,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08, /*[1:0]=0:Averaged*/
	0x3140, 0x00,
	0x3246, 0x81,
	0x3247, 0x81,
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x0B,
	0x040D, 0x40,
	0x040E, 0x08,
	0x040F, 0x6C,
	0x034C, 0x0B,
	0x034D, 0x40,
	0x034E, 0x08,
	0x034F, 0x6C,
	0x0301, 0x05,
	0x0303, 0x04,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x5E,
	0x030B, 0x02,
	0x030D, 0x06,
	0x030E, 0x02,
	0x030F, 0x08,
	0x0310, 0x01,
	0x0B06, 0x01,
	0x3620, 0x00,
	0x3F0C, 0x00,
	0x3F14, 0x01,
	0x3F80, 0x03,
	0x3F81, 0xE8,
	0x3FFC, 0x00,
	0x3FFD, 0x26,
	0x7995, 0x01,
	0x0202, 0x07,
	0x0203, 0xD0,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3FE0, 0x03,
	0x3FE1, 0xE8,
	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3FE2, 0x00,
	0x3FE3, 0x00,
	0x3FE4, 0x01,
	0x3FE5, 0x00,
	0x3E20, 0x01,
	0x3E37, 0x00,
};

static void preview_setting(struct subdrv_ctx *ctx)
{
	#if NO_USE_3HDR
	LOG_INF("using binning_normal_setting\n");
	imx576_table_write_cmos_sensor(ctx, imx576_preview_setting,
		sizeof(imx576_preview_setting) / sizeof(kal_uint16));
	#else
	LOG_INF("using binning_3hdr_setting\n");
	preview_setting_3HDR(ctx);
	/*3hdr aec initial setting*/

	#endif
}	/* preview_setting  */
/* ==================================================== */
/* 3P3SP EVT0 */
/* Full resolution */
/* x_output_size: 2304 */
/* y_output_size: 1728 */
/* frame_rate: 30.000 */
/* output_format: RAW10 */
/* output_interface: MIPI */
/* output_lanes: 4 */
/* output_clock_mhz: 720.00 */
/* system_clock_mhz: 280.00 */
/* input_clock_mhz: 24.00 */
/*  */
/* $Rev$: Revision 0.00 */
/* $Date$: 20151201 */
/* ==================================================== */
/* $MV1[MCLK:24,Width:2304,Height:1728,Format:MIPI_RAW10, */
/* mipi_lane:4,mipi_datarate:720,pvi_pclk_inverse:0] */

static kal_uint16 imx576_capture_setting[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x0342, 0x18,
	0x0343, 0x00,
	0x0340, 0x11,
	0x0341, 0x6B,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x16,
	0x0349, 0x7F,
	0x034A, 0x10,
	0x034B, 0xD7,
	0x0220, 0x62,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x0A,
	0x3140, 0x00,
	0x3246, 0x01,
	0x3247, 0x01,
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x16,
	0x040D, 0x80,
	0x040E, 0x10,
	0x040F, 0xD8,
	0x034C, 0x16,
	0x034D, 0x80,
	0x034E, 0x10,
	0x034F, 0xD8,
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x12,
	0x030B, 0x01,
	0x030D, 0x0C,
	0x030E, 0x03,
	0x030F, 0x2B,
	0x0310, 0x01,
	0x0B06, 0x01,
	0x3620, 0x01,
	0x3F0C, 0x00,
	0x3F14, 0x01,
	0x3F80, 0x01,
	0x3F81, 0x72,
	0x3FFC, 0x00,
	0x3FFD, 0x3C,
	0x0202, 0x07,
	0x0203, 0xD0,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3FE0, 0x03,
	0x3FE1, 0xE8,
	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3FE2, 0x00,
	0x3FE3, 0x00,
	0x3FE4, 0x01,
	0x3FE5, 0x00,
	0x323A, 0x00,
	0x323B, 0x00,
	0x323C, 0x00,
};

/* Pll Setting - VCO = 280Mhz */
static void capture_setting(struct subdrv_ctx *ctx)
{
	LOG_INF("E!\n");
	/* full size 29.76fps */
	imx576_table_write_cmos_sensor(ctx, imx576_capture_setting,
		sizeof(imx576_capture_setting) / sizeof(kal_uint16));

	if (!imx576_HDR_synthesis) {
		/* 3hdr_full_size */
		LOG_INF("%d: IMX576 HDR synthesis BYPASS",
			imx576_HDR_synthesis);
		/*0x0220[0]:HDR_MODE; 0:disable,1:Enable*/
		write_cmos_sensor_8(ctx, 0x0220, 0x63);
		write_cmos_sensor_8(ctx, 0x3140, 0x04); /*QBC HDR*/
		write_cmos_sensor_8(ctx, 0x3F22, 0x01);
		write_cmos_sensor_8(ctx, 0x3F36, 0x01);
		write_cmos_sensor_8(ctx, 0x3F25, 0x01);
		write_cmos_sensor_8(ctx, 0x3F23, 0x01);
		write_cmos_sensor_8(ctx, 0x3F38, 0x01);
		write_cmos_sensor_8(ctx, 0x3F27, 0x01);
		write_cmos_sensor_8(ctx, 0x3F39, 0x01);
		write_cmos_sensor_8(ctx, 0x3F37, 0x01);
		/* 3hdr_full_size */
	}
} /* capture setting */

static void normal_video_setting(struct subdrv_ctx *ctx)
{
	LOG_INF("E\n");
	preview_setting(ctx);	/* Tower modify 20160214 */
}

static void hs_video_setting(struct subdrv_ctx *ctx)
{
	LOG_INF("E\n");
	preview_setting(ctx);  /* Tower modify 20160214 */
}

static void slim_video_setting(struct subdrv_ctx *ctx)
{
	LOG_INF("E\n");
	preview_setting(ctx);
}

/*add for read real sensor chip id*/
static kal_uint32 return_lot_id_from_otp(struct subdrv_ctx *ctx)
{
	kal_uint16 val = 0;
	int i = 0;
	kal_uint32 sensor_id = 0;

	if (write_cmos_sensor_8(ctx, 0x0a02, 0x3f) < 0)
		return 0xFFFFFFFF;
	write_cmos_sensor_8(ctx, 0x0a00, 0x01);

	for (i = 0; i < 3; i++) {
		val = read_cmos_sensor(ctx, 0x0A01);
		if ((val & 0x01) == 0x01)
			break;
		mDELAY(3);
	}
	if (i == 3)
		LOG_INF("read otp fail Err !\n"); /* print log */

	LOG_INF("0x0A22 0x%x 0x0A23 0x%x\n",
		read_cmos_sensor_8(ctx, 0x0A22), read_cmos_sensor_8(ctx, 0x0A23));
	sensor_id =
	  (read_cmos_sensor_8(ctx, 0x0A22) << 4) |
	  (read_cmos_sensor_8(ctx, 0x0A23) >> 4);
	if (sensor_id == IMX576_SENSOR_ID) {
		LOG_INF("This is 0.91cut version, version = 0x%x\n",
			read_cmos_sensor_8(ctx, 0x0018));
		return sensor_id;
	}

	return 0;
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

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20, */
	/* we should detect the module used i2c address */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			*sensor_id = return_lot_id_from_otp(ctx);
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			LOG_INF(
				"Read sensor id fail, i2c_write_id: 0x%x  sensor_id: 0x%x\n",
				ctx->i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/* if Sensor ID is not correct, */
		/* Must set *sensor_id to 0xFFFFFFFF */
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

	LOG_INF("preview 2880*2156@30fps; capture 5760*4312@24fps\n");

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20, */
	/* we should detect the module used i2c address */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			sensor_id = return_lot_id_from_otp(ctx);
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, sensor_id);
				break;
			}
			LOG_INF(
				"Read sensor id fail, id: 0x%x sensor_id=0x%x\n",
				ctx->i2c_write_id, sensor_id);
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
} /* open */



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
	LOG_INF("E\n");
	/*No Need to implement this function*/
	streaming_control(ctx, KAL_FALSE);
	return ERROR_NONE;
} /* close */


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
static kal_uint32 preview(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E ctx->hdr_mode=%d\n", ctx->hdr_mode);

	if (ctx->hdr_mode) {
		LOG_INF("E 3HDR\n");
		ctx->sensor_mode = IMGSENSOR_MODE_PREVIEW;
		ctx->pclk = imgsensor_info.pre_3HDR.pclk;
		ctx->line_length = imgsensor_info.pre_3HDR.linelength;
		ctx->frame_length = imgsensor_info.pre_3HDR.framelength;
		ctx->min_frame_length =
			imgsensor_info.pre_3HDR.framelength;
		ctx->autoflicker_en = KAL_FALSE;
		preview_setting_3HDR(ctx);
	} else {
		LOG_INF("E normal\n");
		ctx->sensor_mode = IMGSENSOR_MODE_PREVIEW;
		ctx->pclk = imgsensor_info.pre.pclk;
		ctx->line_length = imgsensor_info.pre.linelength;
		ctx->frame_length = imgsensor_info.pre.framelength;
		ctx->min_frame_length = imgsensor_info.pre.framelength;
		ctx->autoflicker_en = KAL_FALSE;
		preview_setting(ctx);
	}
	return ERROR_NONE;
}

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
static kal_uint32 capture(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	ctx->sensor_mode = IMGSENSOR_MODE_CAPTURE;
	ctx->pclk = imgsensor_info.cap.pclk;
	ctx->line_length = imgsensor_info.cap.linelength;
	ctx->frame_length = imgsensor_info.cap.framelength;
	ctx->min_frame_length = imgsensor_info.cap.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	capture_setting(ctx);

	return ERROR_NONE;
}	/* capture(ctx) */
static kal_uint32 normal_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E ctx->hdr_mode=%d\n", ctx->hdr_mode);

	if (ctx->hdr_mode) {
		LOG_INF("E preview 3HDR\n");
		ctx->sensor_mode = IMGSENSOR_MODE_PREVIEW;
		ctx->pclk = imgsensor_info.pre_3HDR.pclk;
		ctx->line_length = imgsensor_info.pre_3HDR.linelength;
		ctx->frame_length = imgsensor_info.pre_3HDR.framelength;
		ctx->min_frame_length =
		imgsensor_info.pre_3HDR.framelength;
		ctx->autoflicker_en = KAL_FALSE;
		preview_setting_3HDR(ctx);
	} else {
		ctx->sensor_mode = IMGSENSOR_MODE_VIDEO;
		ctx->pclk = imgsensor_info.normal_video.pclk;
		ctx->line_length = imgsensor_info.normal_video.linelength;
		ctx->frame_length
			= imgsensor_info.normal_video.framelength;
		ctx->min_frame_length
			= imgsensor_info.normal_video.framelength;
		ctx->autoflicker_en = KAL_FALSE;
		normal_video_setting(ctx);
		/* preview_setting(ctx); */
	}

	return ERROR_NONE;
} /* normal_video */

static kal_uint32 hs_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

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

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

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

	return ERROR_NONE;
} /* slim_video */

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
} /* get_resolution */

static int get_info(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MSDK_SENSOR_INFO_STRUCT *sensor_info,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
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

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame =
			imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
			imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->PDAF_Support = 0;

	sensor_info->HDR_Support = 4;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->TEMPERATURE_SUPPORT = imgsensor_info.temperature_support;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */



	sensor_info->SensorWidthSampling = 0;  /* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

	return ERROR_NONE;
}	/*	get_info  */


static int control(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
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
		LOG_INF("Error ScenarioId setting");
		preview(ctx, image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}

	return ERROR_NONE;
}	/* control(ctx) */



static kal_uint32 set_video_mode(struct subdrv_ctx *ctx, UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);
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
	if (enable != ctx->autoflicker_en)
		LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	if (enable) /* enable auto flicker */
		ctx->autoflicker_en = KAL_TRUE;
	else /* Cancel Auto flick */
		ctx->autoflicker_en = KAL_FALSE;
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		if (ctx->hdr_mode) {
			frame_length =
			imgsensor_info.pre_3HDR.pclk / framerate * 10 /
			imgsensor_info.pre_3HDR.linelength;
			ctx->dummy_line =
		(frame_length > imgsensor_info.pre_3HDR.framelength)
		? (frame_length - imgsensor_info.pre_3HDR.framelength)
		: 0;
			ctx->frame_length =
				imgsensor_info.pre_3HDR.framelength +
				ctx->dummy_line;
			ctx->min_frame_length =
				ctx->frame_length;
		} else {
			frame_length =
			    imgsensor_info.pre.pclk / framerate * 10 /
			    imgsensor_info.pre.linelength;
			ctx->dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			?
			(frame_length - imgsensor_info.pre.framelength)
			: 0;
			ctx->frame_length =
				imgsensor_info.pre.framelength +
				ctx->dummy_line;
			ctx->min_frame_length =
				ctx->frame_length;
		}
		set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		if (framerate == 0)
			return ERROR_NONE;
		if (ctx->hdr_mode) {
			frame_length =
			 imgsensor_info.pre_3HDR.pclk / framerate * 10 /
			 imgsensor_info.pre_3HDR.linelength;
			ctx->dummy_line =
		(frame_length > imgsensor_info.pre_3HDR.framelength) ?
	       (frame_length - imgsensor_info.pre_3HDR.framelength) : 0;
			ctx->frame_length =
				imgsensor_info.pre_3HDR.framelength +
				ctx->dummy_line;
			ctx->min_frame_length =
				ctx->frame_length;
		} else {
			frame_length =
				imgsensor_info.normal_video.pclk /
				framerate * 10 /
				imgsensor_info.normal_video.linelength;
			ctx->dummy_line =
	   (frame_length > imgsensor_info.normal_video.framelength) ?
	   (frame_length - imgsensor_info.normal_video.framelength) : 0;
			ctx->frame_length =
			    imgsensor_info.normal_video.framelength +
			    ctx->dummy_line;
			ctx->min_frame_length =
				ctx->frame_length;
		}

		set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		frame_length = imgsensor_info.cap.pclk /
			framerate * 10 /
			imgsensor_info.cap.linelength;
		ctx->dummy_line =
		    (frame_length > imgsensor_info.cap.framelength)
		    ? (frame_length - imgsensor_info.cap.framelength)
		    : 0;
		ctx->frame_length =
			imgsensor_info.cap.framelength +
			ctx->dummy_line;
		ctx->min_frame_length =
			ctx->frame_length;
		set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		frame_length =
			imgsensor_info.hs_video.pclk / framerate * 10 /
			imgsensor_info.hs_video.linelength;
		ctx->dummy_line =
	      (frame_length > imgsensor_info.hs_video.framelength) ?
	      (frame_length - imgsensor_info.hs_video.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.hs_video.framelength +
			ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		frame_length =
			imgsensor_info.slim_video.pclk /
			framerate * 10 /
			imgsensor_info.slim_video.linelength;
		ctx->dummy_line =
	    (frame_length > imgsensor_info.slim_video.framelength) ?
	    (frame_length - imgsensor_info.slim_video.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.slim_video.framelength +
			ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		set_dummy(ctx);
		break;
	default:  /* coding with  preview scenario by default */
		frame_length =
			imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		ctx->dummy_line =
		    (frame_length > imgsensor_info.pre.framelength) ?
		    (frame_length - imgsensor_info.pre.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.pre.framelength +
			ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		set_dummy(ctx);
		LOG_INF(
			"error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(struct subdrv_ctx *ctx,
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

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
	LOG_INF("enable: %d\n", enable);

	if (enable) {
	/* 0x5E00[8]: 1 enable,  0 disable */
	/* 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK */
		write_cmos_sensor_8(ctx, 0x0601, 0x0002);
	} else {
	/* 0x5E00[8]: 1 enable,  0 disable */
	/* 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK */
		write_cmos_sensor_8(ctx, 0x0601, 0x0000);
	}
	ctx->test_pattern = enable;
	return ERROR_NONE;
}

static void hdr_write_tri_shutter(struct subdrv_ctx *ctx,
		kal_uint16 le, kal_uint16 me, kal_uint16 se)
{
	kal_uint16 realtime_fps = 0;

	LOG_INF("E! le:0x%x, me:0x%x, se:0x%x\n", le, me, se);
	if (le > ctx->min_frame_length - imgsensor_info.margin)
		ctx->frame_length = le + imgsensor_info.margin;
	else
		ctx->frame_length = ctx->min_frame_length;
	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;
	if (le < imgsensor_info.min_shutter)
		le = imgsensor_info.min_shutter;

	if (ctx->autoflicker_en) {
		realtime_fps =
			ctx->pclk / ctx->line_length * 10 /
			ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(ctx, 146, 0);
		else {
			write_cmos_sensor_8(ctx, 0x0104, 0x01);
			write_cmos_sensor_8(ctx, 0x0340,
			ctx->frame_length >> 8); /*FRM_LENGTH_LINES[15:8]*/
			write_cmos_sensor_8(ctx, 0x0341,
			ctx->frame_length & 0xFF);/*FRM_LENGTH_LINES[7:0]*/
			write_cmos_sensor_8(ctx, 0x0104, 0x00);
		}
	} else {
		write_cmos_sensor_8(ctx, 0x0104, 0x01);
		write_cmos_sensor_8(ctx, 0x0340, ctx->frame_length >> 8);
		write_cmos_sensor_8(ctx, 0x0341, ctx->frame_length & 0xFF);
		write_cmos_sensor_8(ctx, 0x0104, 0x00);
	}

	write_cmos_sensor_8(ctx, 0x0104, 0x01);
	/* Long exposure */
	write_cmos_sensor_8(ctx, 0x0202, (le >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0203, le & 0xFF);
	/* Muddle exposure */
	/*MID_COARSE_INTEG_TIME[15:8]*/
	write_cmos_sensor_8(ctx, 0x3FE0, (me >> 8) & 0xFF);
	/*MID_COARSE_INTEG_TIME[7:0]*/
	write_cmos_sensor_8(ctx, 0x3FE1, me & 0xFF);
	/* Short exposure */
	write_cmos_sensor_8(ctx, 0x0224, (se >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0225, se & 0xFF);
	write_cmos_sensor_8(ctx, 0x0104, 0x00);

	LOG_INF("L! le:0x%x, me:0x%x, se:0x%x\n", le, me, se);

}

static void hdr_write_tri_gain(struct subdrv_ctx *ctx,
		kal_uint16 lgain, kal_uint16 mg, kal_uint16 sg)
{
	kal_uint16 reg_lg, reg_mg, reg_sg;

	if (lgain < BASEGAIN || lgain > 16 * BASEGAIN) {
		LOG_INF("Error gain setting");

		if (lgain < BASEGAIN)
			lgain = BASEGAIN;
		else if (lgain > 16 * BASEGAIN)
			lgain = 16 * BASEGAIN;
	}

	reg_lg = gain2reg(ctx, lgain);
	reg_mg = gain2reg(ctx, mg);
	reg_sg = gain2reg(ctx, sg);
	ctx->gain = reg_lg;
	write_cmos_sensor_8(ctx, 0x0104, 0x01);
	/* Long Gian */
	write_cmos_sensor_8(ctx, 0x0204, (reg_lg>>8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0205, reg_lg & 0xFF);
	/* Middle Gian */
	write_cmos_sensor_8(ctx, 0x3FE2, (reg_mg>>8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x3FE3, reg_mg & 0xFF);
	/* Short Gian */
	write_cmos_sensor_8(ctx, 0x0216, (reg_sg>>8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0217, reg_sg & 0xFF);

	if (lgain > mg) {
		/*LOG_INF("long gain > medium gain\n");*/
		write_cmos_sensor_8(ctx, 0xEB06, 0x00);
		write_cmos_sensor_8(ctx, 0xEB08, 0x00);
		write_cmos_sensor_8(ctx, 0xEB0A, 0x00);
		write_cmos_sensor_8(ctx, 0xEB12, 0x00);
		write_cmos_sensor_8(ctx, 0xEB14, 0x00);
		write_cmos_sensor_8(ctx, 0xEB16, 0x00);

		write_cmos_sensor_8(ctx, 0xEB07, 0x08);
		write_cmos_sensor_8(ctx, 0xEB09, 0x08);
		write_cmos_sensor_8(ctx, 0xEB0B, 0x08);
		write_cmos_sensor_8(ctx, 0xEB13, 0x10);
		write_cmos_sensor_8(ctx, 0xEB15, 0x10);
		write_cmos_sensor_8(ctx, 0xEB17, 0x10);
	} else {
		/*LOG_INF("long gain <= medium gain\n");*/
		write_cmos_sensor_8(ctx, 0xEB06, 0x00);
		write_cmos_sensor_8(ctx, 0xEB08, 0x00);
		write_cmos_sensor_8(ctx, 0xEB0A, 0x00);
		write_cmos_sensor_8(ctx, 0xEB12, 0x01);
		write_cmos_sensor_8(ctx, 0xEB14, 0x01);
		write_cmos_sensor_8(ctx, 0xEB16, 0x01);

		write_cmos_sensor_8(ctx, 0xEB07, 0xC8);
		write_cmos_sensor_8(ctx, 0xEB09, 0xC8);
		write_cmos_sensor_8(ctx, 0xEB0B, 0xC8);
		write_cmos_sensor_8(ctx, 0xEB13, 0x2C);
		write_cmos_sensor_8(ctx, 0xEB15, 0x2C);
		write_cmos_sensor_8(ctx, 0xEB17, 0x2C);
	}
	write_cmos_sensor_8(ctx, 0x0104, 0x00);

	LOG_INF(
		"lgain:0x%x, reg_lg:0x%x, sg:0x%x, reg_mg:0x%x, mg:0x%x, reg_sg:0x%x\n",
		lgain, reg_lg, mg, reg_mg, sg, reg_sg);

}

static void imx576_set_lsc_reg_setting(struct subdrv_ctx *ctx,
		kal_uint8 index, kal_uint16 *regDa, MUINT32 regNum)
{
	#if LSC_DEBUG
	int i;
	#endif
	int startAddr[4] = {0x9D88, 0x9CB0, 0x9BD8, 0x9B00};
	/*0:B,1:Gb,2:Gr,3:R*/

	LOG_INF("E! index:%d, regNum:%d\n", index, regNum);

	if (write_cmos_sensor_8(ctx, 0x0B00, 0x01) != 0) { /*lsc enable*/
		LOG_INF("Write i2c failed with lsc enable\n");
		return;
	}

	write_cmos_sensor_8(ctx, 0x9014, 0x01);
	write_cmos_sensor_8(ctx, 0x4439, 0x01);
	mdelay(1);
	LOG_INF("Addr 0xB870, 0x380D Value:0x%x %x\n",
		read_cmos_sensor_8(ctx, 0xB870),
		read_cmos_sensor_8(ctx, 0x380D));

	write_cmos_sensor_8(ctx, 0x0104, 0x01);

	/*define Knot point, 2'b01:u3.7*/
	write_cmos_sensor_8(ctx, 0x9750, 0x01);
	write_cmos_sensor_8(ctx, 0x9751, 0x01);
	write_cmos_sensor_8(ctx, 0x9752, 0x01);
	write_cmos_sensor_8(ctx, 0x9753, 0x01);

	imx576_seq_write_cmos_sensor(ctx, startAddr[index], regDa, regNum);

	write_cmos_sensor_8(ctx, 0x0104, 0x00);

	#if LSC_DEBUG
	for (i = 0; i < regNum; i++) {
		LOG_INF("l Addr:0x%x Value:0x%x %x Table:0x%x\n",
			(startAddr[index] + 2*i),
			read_cmos_sensor_8(ctx, startAddr[index] + 2*i),
			read_cmos_sensor_8(ctx, startAddr[index] + 2*i + 1),
			regDa[i]);
	}
	#endif
	write_cmos_sensor_8(ctx, 0x0B00, 0x00); /*lsc disable*/


}

static void set_imx576_ATR(struct subdrv_ctx *ctx,
		kal_uint16 LimitGain, kal_uint16 LtcRate, kal_uint16 PostGain)
{
	LOG_INF("Limit Gain:0x%x LTC Rate:0x%x Post Gain:0x%x\n",
		LimitGain, LtcRate, PostGain);

	write_cmos_sensor_8(ctx, 0x0104, 0x01);

	write_cmos_sensor_8(ctx, 0x7F77, PostGain); /*RG_TC_VE_POST_GAIN*/

	write_cmos_sensor_8(ctx, 0x3C00, LtcRate & 0xFF); /*TC_LTC_RATIO_1*/
	write_cmos_sensor_8(ctx, 0x3C01, LtcRate & 0xFF); /*TC_LTC_RATIO_2*/
	write_cmos_sensor_8(ctx, 0x3C02, LtcRate & 0xFF); /*TC_LTC_RATIO_3*/
	write_cmos_sensor_8(ctx, 0x3C03, LtcRate & 0xFF); /*TC_LTC_RATIO_4*/
	write_cmos_sensor_8(ctx, 0x3C04, LtcRate & 0xFF); /*TC_LTC_RATIO_5*/

	write_cmos_sensor_8(ctx, 0xA141, LimitGain & 0xFF); /*TC_LIMIT_GAIN_1*/
	write_cmos_sensor_8(ctx, 0xA140, (LimitGain >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0xA147, LimitGain & 0xFF); /*TC_LIMIT_GAIN_2*/
	write_cmos_sensor_8(ctx, 0xA146, (LimitGain >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0xA14D, LimitGain & 0xFF); /*TC_LIMIT_GAIN_3*/
	write_cmos_sensor_8(ctx, 0xA14C, (LimitGain >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0xA153, LimitGain & 0xFF); /*TC_LIMIT_GAIN_4*/
	write_cmos_sensor_8(ctx, 0xA152, (LimitGain >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0xA159, LimitGain & 0xFF); /*TC_LIMIT_GAIN_5*/
	write_cmos_sensor_8(ctx, 0xA158, (LimitGain >> 8) & 0xFF);

	write_cmos_sensor_8(ctx, 0x0104, 0x00);
}

static kal_uint32 imx576_awb_gain(struct subdrv_ctx *ctx,
		struct SET_SENSOR_AWB_GAIN *pSetSensorAWB)
{
	UINT32 rgain_32, grgain_32, gbgain_32, bgain_32;

	grgain_32 = (pSetSensorAWB->ABS_GAIN_GR + 1) >> 1;
	rgain_32 = (pSetSensorAWB->ABS_GAIN_R + 1) >> 1;
	bgain_32 = (pSetSensorAWB->ABS_GAIN_B + 1) >> 1;
	gbgain_32 = (pSetSensorAWB->ABS_GAIN_GB + 1) >> 1;

	write_cmos_sensor_8(ctx, 0x0104, 0x01);

	write_cmos_sensor_8(ctx, 0x0b8e, (grgain_32 >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0b8f, grgain_32 & 0xFF);
	write_cmos_sensor_8(ctx, 0x0b90, (rgain_32 >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0b91, rgain_32 & 0xFF);
	write_cmos_sensor_8(ctx, 0x0b92, (bgain_32 >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0b93, bgain_32 & 0xFF);
	write_cmos_sensor_8(ctx, 0x0b94, (gbgain_32 >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0b95, gbgain_32 & 0xFF);

	write_cmos_sensor_8(ctx, 0x0104, 0x00);

	return ERROR_NONE;
}

static kal_uint32 streaming_control(struct subdrv_ctx *ctx, kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable)
		write_cmos_sensor_8(ctx, 0x0100, 0x01);
	else
		write_cmos_sensor_8(ctx, 0x0100, 0x00);
	return ERROR_NONE;
}

static kal_uint32 get_sensor_temperature(struct subdrv_ctx *ctx)
{
	UINT8 temperature;
	INT32 temperature_convert;

	temperature = read_cmos_sensor_8(ctx, 0x013a);

	if (temperature <= 0x4F)
		temperature_convert = temperature;
	else if (temperature >= 0x50 && temperature <= 0x7F)
		temperature_convert = 80;
	else if (temperature >= 0x80 && temperature <= 0xEC)
		temperature_convert = -20;
	else
		temperature_convert = (INT8) temperature;

	/*
	 * LOG_INF("temp_c(%d), read_reg(%d)\n",
	 * temperature_convert, temperature);
	 */

	return temperature_convert;
}

static int feature_control(
		struct subdrv_ctx *ctx,
		MSDK_SENSOR_FEATURE_ENUM feature_id,
		UINT8 *feature_para,
		UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;

	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
			(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/*LOG_INF("feature_id = %d\n", feature_id);*/
	switch (feature_id) {
	case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
		if ((void *)(uintptr_t) (*(feature_data + 1)) == NULL) {
			*(feature_data + 0) =
				sizeof(imx576_ana_gain_table);
		} else {
			memcpy((void *)(uintptr_t) (*(feature_data + 1)),
			(void *)imx576_ana_gain_table,
			sizeof(imx576_ana_gain_table));
		}
		break;
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_gain;
		*(feature_data + 2) = imgsensor_info.max_gain;
		break;
	case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
		*(feature_data + 0) = imgsensor_info.min_gain_iso;
		*(feature_data + 1) = imgsensor_info.gain_step;
		*(feature_data + 2) = imgsensor_info.gain_type;
		break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_shutter;
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
#if NO_USE_3HDR
			if (ctx->hdr_mode)
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre_3HDR.pclk;
			else
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
#else
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.pre.pclk;
#endif
			break;
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
#if NO_USE_3HDR
			if (ctx->hdr_mode)
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.pre_3HDR.framelength << 16)
				+ imgsensor_info.pre_3HDR.linelength;
			else
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
#else
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
#endif
			break;
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
		LOG_INF(
			"feature_Control ctx->pclk = %d,ctx->current_fps = %d\n",
			ctx->pclk, ctx->current_fps);
		*feature_return_para_32 = ctx->pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(ctx, *feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
	     /* night_mode((BOOL) *feature_data); */
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
		/* get the lens driver ID from EEPROM */
		/* or just return LENS_DRIVER_ID_DO_NOT_CARE */
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
		set_auto_flicker_mode(ctx, (BOOL)*feature_data_16,
				      *(feature_data_16+1));
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_32 = get_sensor_temperature(ctx);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(ctx,
			(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
			*(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(ctx,
			(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA. No support\n");
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode(ctx, (BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", *feature_data_32);
		ctx->current_fps = (UINT16)*feature_data_32;
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		wininfo =
			(struct SENSOR_WINSIZE_INFO_STRUCT *)
			(uintptr_t)(*(feature_data+1));
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
	/*HDR CMD */
	case SENSOR_FEATURE_SET_HDR_ATR:
		LOG_INF(
			"SENSOR_FEATURE_SET_HDR_ATR Limit_Gain=%d, LTC Rate=%d, Post_Gain=%d\n",
			(UINT16)*feature_data,
			(UINT16)*(feature_data + 1),
			(UINT16)*(feature_data + 2));
		set_imx576_ATR(ctx, (UINT16)*feature_data,
			(UINT16)*(feature_data + 1),
			(UINT16)*(feature_data + 2));
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("hdr enable :%d\n", *feature_data_32);
		ctx->hdr_mode = (UINT8)*feature_data_32;
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		LOG_INF(
			"SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d, no support\n",
			(UINT16) *feature_data,	(UINT16) *(feature_data + 1));
		break;
	case SENSOR_FEATURE_SET_HDR_TRI_SHUTTER:
		LOG_INF(
			"SENSOR_FEATURE_SET_HDR_TRI_SHUTTER LE=%d, SE=%d, ME=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		hdr_write_tri_shutter(ctx, (UINT16)*feature_data,
					(UINT16)*(feature_data+1),
					(UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_SET_HDR_TRI_GAIN:
		LOG_INF(
			"SENSOR_FEATURE_SET_HDR_TRI_GAIN LGain=%d, SGain=%d, MGain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		hdr_write_tri_gain(ctx, (UINT16)*feature_data,
				   (UINT16)*(feature_data+1),
				   (UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		pvcinfo =
	     (struct SENSOR_VC_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));
		switch (*feature_data_32) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		/* modify to separate 3hdr and remosaic */
		if (ctx->sensor_mode == IMGSENSOR_MODE_CAPTURE) {
			/*write AWB gain to sensor*/
			feedback_awbgain(ctx, (UINT32)*(feature_data_32 + 1),
					(UINT32)*(feature_data_32 + 2));
		} else {
			imx576_awb_gain(ctx,
				(struct SET_SENSOR_AWB_GAIN *) feature_para);
		}
		break;
	case SENSOR_FEATURE_SET_LSC_TBL:
		{
		kal_uint8 index =
			*(((kal_uint8 *)feature_para) + (*feature_para_len));

		imx576_set_lsc_reg_setting(ctx, index, feature_data_16,
					  (*feature_para_len)/sizeof(UINT16));
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY:
		/*
		 * SENSOR_VHDR_MODE_NONE  = 0x0,
		 * SENSOR_VHDR_MODE_IVHDR = 0x01,
		 * SENSOR_VHDR_MODE_MVHDR = 0x02,
		 * SENSOR_VHDR_MODE_ZVHDR = 0x09
		 * SENSOR_VHDR_MODE_4CELL_MVHDR = 0x0A
		 */
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x2;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		default:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x0;
			break;
		}
		LOG_INF(
			"SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY scenarioId:%llu, HDR:%llu\n",
			*feature_data, *(feature_data+1));
		break;
		/*END OF HDR CMD */
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	{
		kal_uint32 rate;

		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			rate = imgsensor_info.cap.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			rate = imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			rate = imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			rate = imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
			rate = imgsensor_info.pre.mipi_pixel_rate;
			break;
		default:
			rate = 0;
			break;
		}
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
	}
	break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		PDAFinfo =
		(struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			/*
			 * memcpy((void *)PDAFinfo,(void *)&imgsensor_pd_info,
			 * sizeof(SET_PD_BLOCK_INFO_T));
			 */
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		/* LOG_INF("PDAF_CAPACITY scenarioId:%d\n", *feature_data); */
		/* PDAF capacity enable or not*/
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			/* video & capture use same setting */
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(ctx, KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
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
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			if (*(feature_data + 2))/* HDR on */
				*feature_return_para_32 = 1;/*BINNING_NONE*/
			else
				*feature_return_para_32 = 2;/*BINNING_AVERAGED*/
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		default:
			*feature_return_para_32 = 2; /*BINNING_AVERAGED*/
			break;
		}
		pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d\n",
			*feature_return_para_32);

		*feature_para_len = 4;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}	/*	feature_control(ctx)  */

#ifdef IMGSENSOR_VC_ROUTING
static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0b40,
			.vsize = 0x086c,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x0e10,
			.vsize = 0x0002,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x31,
			.hsize = 0x0e10,
			.vsize = 0x0001,
			.user_data_desc = VC_3HDR_Y,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x32,
			.hsize = 0x0e10,
			.vsize = 0x0001,
			.user_data_desc = VC_3HDR_Y,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x33,
			.hsize = 0x0e10,
			.vsize = 0x0001,
			.user_data_desc = VC_3HDR_Y,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1680,
			.vsize = 0x10d8,
		},
	},
};

static int get_frame_desc(struct subdrv_ctx *ctx,
		int scenario_id, struct mtk_mbus_frame_desc *fd)
{
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_prev);
		memcpy(fd->entry, frame_desc_prev, sizeof(frame_desc_prev));
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cap);
		memcpy(fd->entry, frame_desc_cap, sizeof(frame_desc_cap));
		break;
	default:
		return -1;
	}

	return 0;
}
#endif


static const struct subdrv_ctx defctx = {

	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_max = BASEGAIN * 16,
	.ana_gain_min = BASEGAIN,
	.ana_gain_step = 1,
	.exposure_def = 0x3D0,
	.exposure_max = 0xffff - 61,
	.exposure_min = 6,
	.exposure_step = 1,
	.frame_time_delay_frame = 3,
	.margin = 61,
	.max_frame_length = 0xffff,
	.hdr_cap = HDR_CAP_3HDR | HDR_CAP_ATR,

	.mirror = IMAGE_NORMAL, /* mirrorflip information */
	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 *  INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0, /* current shutter */
	.gain = BASEGAIN * 4, /* current gain */
	.dummy_pixel = 0, /* current dummypixel */
	.dummy_line = 0, /* current dummyline */
	.current_fps = 0,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW,
	.ihdr_mode = 0, /* sensor need support LE, SE with HDR feature */
	.hdr_mode = 0, /* HDR mODE : 0: disable HDR, 1:IHDR, 2:HDR, 9:ZHDR */
	.i2c_write_id = 0x20,
};

static int init_ctx(struct subdrv_ctx *ctx,
		struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(ctx, &defctx, sizeof(*ctx));
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}

static int get_temp(struct subdrv_ctx *ctx, int *temp)
{
	*temp = get_sensor_temperature(ctx) * 1000;
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
#ifdef IMGSENSOR_VC_ROUTING
	.get_frame_desc = get_frame_desc,
#endif
	.get_temp = get_temp,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_MCLK, 24, 0},
	{HW_ID_PDN, 0, 0},
	{HW_ID_RST, 0, 0},
	{HW_ID_AVDD, 2800000, 0},
	{HW_ID_DOVDD, 1800000, 0},
	{HW_ID_DVDD, 1100000, 1},
	{HW_ID_MCLK_DRIVING_CURRENT, 8, 1},
	{HW_ID_PDN, 1, 0},
	{HW_ID_RST, 1, 8},
};

const struct subdrv_entry imx576_mipi_raw_entry = {
	.name = "imx576_mipi_raw",
	.id = IMX576_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

