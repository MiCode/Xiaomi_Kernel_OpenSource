// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 s5k3p9spmipiraw_Sensor.c
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

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define_v4l2.h"
#include "kd_imgsensor_errcode.h"

#include "s5k3p9spmipiraw_Sensor.h"
#include "s5k3p9sp_ana_gain_table.h"
#include "s5k3p9sp_Sensor_setting.h"

#include "adaptor-subdrv.h"
#include "adaptor-i2c.h"
#include "adaptor.h"

#define DEBUG_LOG_EN 0

#define PFX "S5K3P9SP_camera_sensor"
#define LOG_INF(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)
#define LOG_DEBUG(...) do { if ((DEBUG_LOG_EN)) LOG_INF(__VA_ARGS__); } while (0)

#define read_cmos_sensor_8(...) subdrv_i2c_rd_u8(__VA_ARGS__)
#define read_cmos_sensor_16(...) subdrv_i2c_rd_u16(__VA_ARGS__)
#define write_cmos_sensor_8(...) subdrv_i2c_wr_u8(__VA_ARGS__)
#define write_cmos_sensor_16(...) subdrv_i2c_wr_u16(__VA_ARGS__)
#define s5k3p9sp_table_write_cmos_sensor_8(...) subdrv_i2c_wr_regs_u8(__VA_ARGS__)
#define s5k3p9sp_table_write_cmos_sensor_16(...) subdrv_i2c_wr_regs_u16(__VA_ARGS__)
#define s5k3p9sp_burst_write_cmos_sensor_8(...) subdrv_i2c_wr_p8(__VA_ARGS__)
#define s5k3p9sp_burst_write_cmos_sensor_16(...) subdrv_i2c_wr_p16(__VA_ARGS__)

#define S5K3P9SP_EEPROM_READ_ID  0xA0
#define S5K3P9SP_EEPROM_WRITE_ID 0xA1

#define _I2C_BUF_SIZE 256
static kal_uint16 _i2c_data[_I2C_BUF_SIZE];
static unsigned int _size_to_write;

static void commit_write_sensor(struct subdrv_ctx *ctx)
{
	if (_size_to_write) {
		s5k3p9sp_table_write_cmos_sensor_16(ctx, _i2c_data, _size_to_write);
		memset(_i2c_data, 0x0, sizeof(_i2c_data));
		_size_to_write = 0;
	}
}

static void set_cmos_sensor_16(struct subdrv_ctx *ctx,
			kal_uint16 reg, kal_uint16 val)
{
	if (_size_to_write + 2 >= _I2C_BUF_SIZE)
		commit_write_sensor(ctx);

	_i2c_data[_size_to_write++] = reg;
	_i2c_data[_size_to_write++] = val;
}

static struct imgsensor_info_struct imgsensor_info = {
		.sensor_id = S5K3P9SP_SENSOR_ID,

		.checksum_value = 0x31e3fbe2,

		.pre = {
			.pclk = 560000000,
			.linelength = 7152,
			.framelength = 2608,
			.startx = 0,
			.starty = 0,
			.grabwindow_width = 2320,
			.grabwindow_height = 1744,
			.mipi_data_lp2hs_settle_dc = 85,
			.mipi_pixel_rate = 269400000,
			.max_framerate = 300,
		},
		.cap = {
#if USE_REMOSAIC
			.pclk = 560000000,
			.linelength = 5088,
			.framelength = 3668,
			.startx = 0,
			.starty = 0,
			.grabwindow_width = 4640,
			.grabwindow_height = 3488,
			.mipi_data_lp2hs_settle_dc = 85,
			.mipi_pixel_rate = 586000000,
			.max_framerate = 300,
#else
			.pclk = 560000000,
			.linelength = 7152,
			.framelength = 2608,
			.startx = 0,
			.starty = 0,
			.grabwindow_width = 2320,
			.grabwindow_height = 1744,
			.mipi_data_lp2hs_settle_dc = 85,
			.mipi_pixel_rate = 269400000,
			.max_framerate = 300,
#endif
		},
		.normal_video = {
			.pclk = 560000000,
			.linelength = 12960,
			.framelength = 1440,
			.startx = 0,
			.starty = 0,
			.grabwindow_width = 1920,
			.grabwindow_height = 1080,
			.mipi_data_lp2hs_settle_dc = 85,
			.mipi_pixel_rate = 216666667,
			.max_framerate = 300,
		},
		.hs_video = {
			.pclk = 560000000,
			.linelength = 12960,
			.framelength = 1440,
			.startx = 0,
			.starty = 0,
			.grabwindow_width = 1920,
			.grabwindow_height = 1080,
			.mipi_data_lp2hs_settle_dc = 85,
			.mipi_pixel_rate = 216666667,
			.max_framerate = 300,
		},
		.slim_video = {
			.pclk = 560000000,
			.linelength = 5088,
			.framelength = 917,
			.startx = 0,
			.starty = 0,
			.grabwindow_width = 640,
			.grabwindow_height = 480,
			.mipi_data_lp2hs_settle_dc = 85,
			.mipi_pixel_rate = 278400000,
			.max_framerate = 1200,
		},
		.margin = 3,
		.min_shutter = 3,
		.min_gain = BASEGAIN, /*1x gain*/
		.max_gain = BASEGAIN * 16, /*16x gain*/
		.min_gain_iso = 100,
		.exp_step = 1,
		.gain_step = 32,
		.gain_type = 2,
		.max_frame_length = 0xfffc,//0xffff-3,
		.ae_shut_delay_frame = 0,
		.ae_sensor_gain_delay_frame = 0,
		.ae_ispGain_delay_frame = 2,
		.ihdr_support = 0,/*1, support; 0,not support*/
		.ihdr_le_firstline = 0,/*1,le first; 0, se first*/
		.sensor_mode_num = 5,/*support sensor mode num*/

		.cap_delay_frame = 2,
		.pre_delay_frame = 2,
		.video_delay_frame = 3,
		.hs_video_delay_frame = 3,
		.slim_video_delay_frame = 3,

		.isp_driving_current = ISP_DRIVING_2MA,
		.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
		.mipi_sensor_type = MIPI_OPHY_NCSI2,
		.mipi_settle_delay_mode = 1,
		.sensor_output_dataformat =
			SENSOR_OUTPUT_FORMAT_RAW_4CELL_BAYER_Gr,
		.mclk = 24,
		.mipi_lane_num = SENSOR_MIPI_4_LANE,
		.i2c_addr_table = {0x20, 0xff},
		.i2c_speed = 1000,
};

/* Sensor output window information */
/*no mirror flip*/
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{4640, 3488,    0,    0, 4640, 3488, 2320, 1744,
	    0,    0, 2320, 1744,    0,    0, 2320, 1744},/* Preview */
#if USE_REMOSAIC
	{4640, 3488,    0,    0, 4640, 3488, 4640, 3488,
	    0,    0, 4640, 3488,    0,    0, 4640, 3488},/* Capture Remosaic */
#else
	{4640, 3488,    0,    0, 4640, 3488, 2320, 1744,
	    0,    0, 2320, 1744,    0,    0, 2320, 1744},/* Capture == Preview*/
#endif
	{4640, 3488,  400,  664, 3840, 2160, 1920, 1080,
	    0,    0, 1920, 1080,    0,    0, 1920, 1080},/* Video */
	{4640, 3488,  400,  664, 3840, 2160, 1920, 1080,
	    0,    0, 1920, 1080,    0,    0, 1920, 1080},/* hs_video == Video */
	{4640, 3488, 1024,  784, 2592, 1920, 1296,  960,
	  328,  240,  640,  480,    0,    0,  640,  480},/* slim_video */
};

static void set_dummy(struct subdrv_ctx *ctx)
{
	DEBUG_LOG(ctx, "dummyline = %d, dummypixels = %d\n",
		ctx->dummy_line, ctx->dummy_pixel);
	set_cmos_sensor_16(ctx, 0x0340, ctx->frame_length);
	set_cmos_sensor_16(ctx, 0x0342, ctx->line_length);

	commit_write_sensor(ctx);
}	/*	set_dummy  */

static void set_max_framerate(struct subdrv_ctx *ctx, UINT16 framerate,
	kal_bool min_framelength_en)
{
	kal_uint32 frame_length = ctx->frame_length;

	LOG_DEBUG("framerate = %d, min framelength should enable %d\n",
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
}	/*	set_max_framerate  */

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
		realtime_fps =
			ctx->pclk / ctx->line_length
			* 10 / ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(ctx, 146, 0);
	}
	/* Update Shutter*/
	set_cmos_sensor_16(ctx, 0x0104, 0x01);//gph start
	set_cmos_sensor_16(ctx, 0x0340, ctx->frame_length);
	set_cmos_sensor_16(ctx, 0x0202, shutter);
	if (!ctx->ae_ctrl_gph_en)
		set_cmos_sensor_16(ctx, 0x0104, 0x00);//grouphold end
	commit_write_sensor(ctx);

	DEBUG_LOG(ctx, "shutter = %d, framelength = %d\n",
		shutter, ctx->frame_length);
}	/*	write_shutter  */

/*
 ************************************************************************
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
 ************************************************************************
 */
static void set_shutter(struct subdrv_ctx *ctx, kal_uint16 shutter)
{
	ctx->shutter = shutter;

	write_shutter(ctx, shutter);
}	/*	set_shutter */

static void set_frame_length(struct subdrv_ctx *ctx, kal_uint16 frame_length)
{
	if (frame_length > 1)
		ctx->frame_length = frame_length;

	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;
	if (ctx->min_frame_length > ctx->frame_length)
		ctx->frame_length = ctx->min_frame_length;

	/* Extend frame length */
	write_cmos_sensor_16(ctx, 0x0340, ctx->frame_length & 0xFFFF);

	LOG_DEBUG("Framelength: set=%d/input=%d/min=%d\n",
		ctx->frame_length, frame_length, ctx->min_frame_length);
}

static void set_shutter_frame_length(struct subdrv_ctx *ctx,
	kal_uint16 shutter, kal_uint16 frame_length)
{	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	ctx->shutter = shutter;

	/* Change frame time */
	if (frame_length > 1)
		dummy_line = frame_length - ctx->frame_length;

	ctx->frame_length =
		ctx->frame_length + dummy_line;


	if (shutter > ctx->frame_length - imgsensor_info.margin)
		ctx->frame_length = shutter + imgsensor_info.margin;


	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;


	shutter =
		(shutter < imgsensor_info.min_shutter)
		? imgsensor_info.min_shutter
		: shutter;
	shutter =
		(shutter >
		(imgsensor_info.max_frame_length - imgsensor_info.margin))
		? (imgsensor_info.max_frame_length - imgsensor_info.margin)
		: shutter;

	if (ctx->autoflicker_en) {
		realtime_fps =
			ctx->pclk / ctx->line_length *
			10 / ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(ctx, 146, 0);
	}

	/* Update Shutter */
	set_cmos_sensor_16(ctx, 0x0340, ctx->frame_length & 0xFFFF);
	set_cmos_sensor_16(ctx, 0X0202, shutter & 0xFFFF);

	commit_write_sensor(ctx);

	LOG_INF("shutter = %d, framelength = %d/%d, dummy_line= %d\n",
		shutter, ctx->frame_length,
		frame_length, dummy_line);

}

static kal_uint16 gain2reg(struct subdrv_ctx *ctx, const kal_uint32 gain)
{
	kal_uint16 reg_gain = 0x0;

	reg_gain = gain * 32 / BASEGAIN;
	return (kal_uint16)reg_gain;
}

/*
 ************************************************************************
 * FUNCTION
 *	set_gain
 *
 * DESCRIPTION
 *	This function is to set global gain to sensor.
 *
 * PARAMETERS
 *	iGain : sensor global gain(base: 0x400)
 *
 * RETURNS
 *	the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 ************************************************************************
 */
static kal_uint32 set_gain(struct subdrv_ctx *ctx, kal_uint32 gain)
{
	kal_uint16 reg_gain;

	if (gain < imgsensor_info.min_gain || gain > imgsensor_info.max_gain) {
		LOG_INF("Error gain setting");

		if (gain < imgsensor_info.min_gain)
			gain = imgsensor_info.min_gain;
		else if (gain > imgsensor_info.max_gain)
			gain = imgsensor_info.max_gain;
	}

	reg_gain = gain2reg(ctx, gain);
	ctx->gain = reg_gain;
	DEBUG_LOG(ctx, "gain = %d , reg_gain = 0x%x\n", gain, reg_gain);

	set_cmos_sensor_16(ctx, 0x0204, reg_gain);
	if (ctx->ae_ctrl_gph_en)
		set_cmos_sensor_16(ctx, 0x0104, 0x00); //grouphold end
	commit_write_sensor(ctx);
	return gain;
}	/*	set_gain  */

static kal_uint32 streaming_control(struct subdrv_ctx *ctx, kal_bool enable)
{
	int timeout = ctx->current_fps ? (10000 / ctx->current_fps) + 1 : 101;
	int i = 0;
	int framecnt = 0;

	LOG_INF("streaming_enable(0= Sw Standby,1= streaming): %d\n", enable);
	if (enable) {
		write_cmos_sensor_8(ctx, 0x0100, 0X01);
		mDELAY(10);
	} else {
		write_cmos_sensor_8(ctx, 0x0100, 0x00);
		for (i = 0; i < timeout; i++) {
			mDELAY(5);
			framecnt = read_cmos_sensor_8(ctx, 0x0005);
			if (framecnt == 0xFF) {
				LOG_DEBUG("Stream Off OK at i=%d.\n", i);
				return ERROR_NONE;
			}
		}
		LOG_INF("Stream Off Fail! framecnt= %d.\n", framecnt);
	}
	return ERROR_NONE;
}

static void sensor_init(struct subdrv_ctx *ctx)
{
	/*Global setting */
	LOG_INF("E\n");
	write_cmos_sensor_16(ctx, 0x6028, 0x4000);
	write_cmos_sensor_16(ctx, 0x6010, 0x0001);
	mdelay(3);
#ifdef USE_TNP_BURST
	write_cmos_sensor_16(ctx, 0x6214, 0x7970);
	write_cmos_sensor_16(ctx, 0x6218, 0x7150);
	write_cmos_sensor_16(ctx, 0x0A02, 0x007E);

	write_cmos_sensor_16(ctx, 0x6028, 0x4000); //TNP burst start
	write_cmos_sensor_16(ctx, 0x6004, 0x0001);
	write_cmos_sensor_16(ctx, 0x6028, 0x2000);

	write_cmos_sensor_16(ctx, 0x602A, 0x3F4C);

	s5k3p9sp_burst_write_cmos_sensor_8(ctx, 0x6F12,
		(u8 *)uTnpArrayInit, sizeof(uTnpArrayInit));

	write_cmos_sensor_16(ctx, 0x6028, 0x4000);
	write_cmos_sensor_16(ctx, 0x6004, 0x0000); //TNP burst end

	write_cmos_sensor_16(ctx, 0x6028, 0x2000); // global
	write_cmos_sensor_16(ctx, 0x602A, 0x16F0);
	write_cmos_sensor_16(ctx, 0x6F12, 0x2929);
	write_cmos_sensor_16(ctx, 0x602A, 0x16F2);
	write_cmos_sensor_16(ctx, 0x6F12, 0x2929);
	write_cmos_sensor_16(ctx, 0x602A, 0x16FA);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0029);
	write_cmos_sensor_16(ctx, 0x602A, 0x16FC);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0029);
	write_cmos_sensor_16(ctx, 0x602A, 0x1708);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0029);
	write_cmos_sensor_16(ctx, 0x602A, 0x170A);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0029);
	write_cmos_sensor_16(ctx, 0x602A, 0x1712);
	write_cmos_sensor_16(ctx, 0x6F12, 0x2929);
	write_cmos_sensor_16(ctx, 0x602A, 0x1714);
	write_cmos_sensor_16(ctx, 0x6F12, 0x2929);
	write_cmos_sensor_16(ctx, 0x602A, 0x1716);
	write_cmos_sensor_16(ctx, 0x6F12, 0x2929);
	write_cmos_sensor_16(ctx, 0x602A, 0x1722);
	write_cmos_sensor_16(ctx, 0x6F12, 0x152A);
	write_cmos_sensor_16(ctx, 0x602A, 0x1724);
	write_cmos_sensor_16(ctx, 0x6F12, 0x152A);
	write_cmos_sensor_16(ctx, 0x602A, 0x172C);
	write_cmos_sensor_16(ctx, 0x6F12, 0x002A);
	write_cmos_sensor_16(ctx, 0x602A, 0x172E);
	write_cmos_sensor_16(ctx, 0x6F12, 0x002A);
	write_cmos_sensor_16(ctx, 0x602A, 0x1736);
	write_cmos_sensor_16(ctx, 0x6F12, 0x1500);
	write_cmos_sensor_16(ctx, 0x602A, 0x1738);
	write_cmos_sensor_16(ctx, 0x6F12, 0x1500);
	write_cmos_sensor_16(ctx, 0x602A, 0x1740);
	write_cmos_sensor_16(ctx, 0x6F12, 0x152A);
	write_cmos_sensor_16(ctx, 0x602A, 0x1742);
	write_cmos_sensor_16(ctx, 0x6F12, 0x152A);
	write_cmos_sensor_16(ctx, 0x602A, 0x16BE);
	write_cmos_sensor_16(ctx, 0x6F12, 0x1515);
	write_cmos_sensor_16(ctx, 0x6F12, 0x1515);
	write_cmos_sensor_16(ctx, 0x602A, 0x16C8);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0029);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0029);
	write_cmos_sensor_16(ctx, 0x602A, 0x16D6);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0015);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0015);
	write_cmos_sensor_16(ctx, 0x602A, 0x16E0);
	write_cmos_sensor_16(ctx, 0x6F12, 0x2929);
	write_cmos_sensor_16(ctx, 0x6F12, 0x2929);
	write_cmos_sensor_16(ctx, 0x6F12, 0x2929);
	write_cmos_sensor_16(ctx, 0x602A, 0x19B8);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0100);
	write_cmos_sensor_16(ctx, 0x602A, 0x2224);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0100);
	write_cmos_sensor_16(ctx, 0x602A, 0x0DF8);
	write_cmos_sensor_16(ctx, 0x6F12, 0x1001);
	write_cmos_sensor_16(ctx, 0x602A, 0x1EDA);
	write_cmos_sensor_16(ctx, 0x6F12, 0x000E);
	write_cmos_sensor_16(ctx, 0x6F12, 0x000E);
	write_cmos_sensor_16(ctx, 0x6F12, 0x000E);
	write_cmos_sensor_16(ctx, 0x6F12, 0x000E);
	write_cmos_sensor_16(ctx, 0x6F12, 0x000E);
	write_cmos_sensor_16(ctx, 0x6F12, 0x000E);
	write_cmos_sensor_16(ctx, 0x6F12, 0x000E);
	write_cmos_sensor_16(ctx, 0x6F12, 0x000E);
	write_cmos_sensor_16(ctx, 0x6F12, 0x000E);
	write_cmos_sensor_16(ctx, 0x6F12, 0x000E);
	write_cmos_sensor_16(ctx, 0x6F12, 0x000E);
	write_cmos_sensor_16(ctx, 0x6F12, 0x000E);
	write_cmos_sensor_16(ctx, 0x6F12, 0x000E);
	write_cmos_sensor_16(ctx, 0x6F12, 0x000E);
	write_cmos_sensor_16(ctx, 0x6F12, 0x000E);
	write_cmos_sensor_16(ctx, 0x6F12, 0x000E);
	write_cmos_sensor_16(ctx, 0x602A, 0x16A0);
	write_cmos_sensor_16(ctx, 0x6F12, 0x3D09);
	write_cmos_sensor_16(ctx, 0x602A, 0x10A8);
	write_cmos_sensor_16(ctx, 0x6F12, 0x000E);
	write_cmos_sensor_16(ctx, 0x602A, 0x1198);
	write_cmos_sensor_16(ctx, 0x6F12, 0x002B);
	write_cmos_sensor_16(ctx, 0x602A, 0x1002);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0001);
	write_cmos_sensor_16(ctx, 0x602A, 0x0F70);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0101);
	write_cmos_sensor_16(ctx, 0x6F12, 0x002F);
	write_cmos_sensor_16(ctx, 0x6F12, 0x007F);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0030);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0080);
	write_cmos_sensor_16(ctx, 0x6F12, 0x000B);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0009);
	write_cmos_sensor_16(ctx, 0x6F12, 0xF46E);
	write_cmos_sensor_16(ctx, 0x602A, 0x0FAA);
	write_cmos_sensor_16(ctx, 0x6F12, 0x000D);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0003);
	write_cmos_sensor_16(ctx, 0x6F12, 0xF464);
	write_cmos_sensor_16(ctx, 0x602A, 0x1698);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0D05);
	write_cmos_sensor_16(ctx, 0x602A, 0x20A0);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0001);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0203);
	write_cmos_sensor_16(ctx, 0x602A, 0x4A74);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0101);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0000);
	write_cmos_sensor_16(ctx, 0x6F12, 0x1F80);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0000);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0000);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0000);
	write_cmos_sensor_16(ctx, 0x602A, 0x0FF4);
	write_cmos_sensor_16(ctx, 0x6F12, 0x0100);
	write_cmos_sensor_16(ctx, 0x6F12, 0x1800);
	write_cmos_sensor_16(ctx, 0x6028, 0x4000);
	write_cmos_sensor_16(ctx, 0x0FEA, 0x1440);
	write_cmos_sensor_16(ctx, 0x0B06, 0x0101);
	write_cmos_sensor_16(ctx, 0xF44A, 0x0007);
	write_cmos_sensor_16(ctx, 0xF456, 0x000A);
	write_cmos_sensor_16(ctx, 0xF46A, 0xBFA0);
	write_cmos_sensor_16(ctx, 0x0D80, 0x1388);
	write_cmos_sensor_16(ctx, 0xB134, 0x0000);
	write_cmos_sensor_16(ctx, 0xB136, 0x0000);
	write_cmos_sensor_16(ctx, 0xB138, 0x0000);
#else
	s5k3p9sp_table_write_cmos_sensor_16(ctx, addr_data_pair_init,
		   sizeof(addr_data_pair_init) / sizeof(kal_uint16));
#endif
	LOG_INF("X\n");

}	/*	sensor_init  */

static void preview_setting(struct subdrv_ctx *ctx)
{
	s5k3p9sp_table_write_cmos_sensor_16(ctx, addr_data_pair_preview,
		   sizeof(addr_data_pair_preview) / sizeof(kal_uint16));
}	/*	preview_setting  */

static void capture_setting(struct subdrv_ctx *ctx)
{
	s5k3p9sp_table_write_cmos_sensor_16(ctx, addr_data_pair_capture,
		   sizeof(addr_data_pair_capture) / sizeof(kal_uint16));
}	/*	capture_setting  */

static void normal_video_setting(struct subdrv_ctx *ctx)
{
	s5k3p9sp_table_write_cmos_sensor_16(ctx, addr_data_pair_normal_video,
		sizeof(addr_data_pair_normal_video) / sizeof(kal_uint16));
}	/*	normal_video_setting  */

static void hs_video_setting(struct subdrv_ctx *ctx)
{
	s5k3p9sp_table_write_cmos_sensor_16(ctx, addr_data_pair_hs_video,
		sizeof(addr_data_pair_hs_video) / sizeof(kal_uint16));
}	/*	hs_video_setting  */

static void slim_video_setting(struct subdrv_ctx *ctx)
{
	s5k3p9sp_table_write_cmos_sensor_16(ctx, addr_data_pair_slim_video,
		   sizeof(addr_data_pair_slim_video) / sizeof(kal_uint16));
}	/*	slim_video_setting  */

#define FOUR_CELL_SIZE 2048
#define FOUR_CELL_ADDR 0x150F
static char four_cell_data[FOUR_CELL_SIZE + 2];
static void read_four_cell_from_eeprom(struct subdrv_ctx *ctx, char *data)
{
	int ret, i;
	char temp;

	if (data != NULL) {
		LOG_INF("return data\n");
		memcpy(data, four_cell_data, FOUR_CELL_SIZE + 2);
	} else {
		LOG_INF("need to read from EEPROM\n");
		/* Check I2C is normal */
		ret = adaptor_i2c_rd_u8(ctx->i2c_client,
			S5K3P9SP_EEPROM_READ_ID >> 1, FOUR_CELL_ADDR, &temp);
		if (ret < 0) {
			LOG_INF("read from EEPROM failed\n");
			return;
		}
		four_cell_data[0] = (FOUR_CELL_SIZE & 0xFF);/*Low*/
		four_cell_data[1] = ((FOUR_CELL_SIZE >> 8) & 0xFF);/*High*/
		/*Multi-Read*/
		for (i = 0; i < FOUR_CELL_SIZE; i++)
			adaptor_i2c_rd_u8(ctx->i2c_client,
				S5K3P9SP_EEPROM_READ_ID >> 1,
				FOUR_CELL_ADDR + i, &four_cell_data[i+2]);
		ctx->is_read_four_cell = 1;
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
 ************************************************************************
 */
static int get_imgsensor_id(struct subdrv_ctx *ctx, UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			*sensor_id =
				((read_cmos_sensor_8(ctx, 0x0000) << 8)
				| read_cmos_sensor_8(ctx, 0x0001));
			LOG_INF("read out sensor id 0x%x\n",
				*sensor_id);
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail, id: 0x%x\n",
				ctx->i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id !=  imgsensor_info.sensor_id) {
/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF*/
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
 ************************************************************************
 */
static int open(struct subdrv_ctx *ctx)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			sensor_id = ((read_cmos_sensor_8(ctx, 0x0000) << 8) |
				read_cmos_sensor_8(ctx, 0x0001));
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, id: 0x%x\n",
				ctx->i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id !=  sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail sequence write in  */
	sensor_init(ctx);

	ctx->autoflicker_en = KAL_FALSE;
	ctx->sensor_mode = IMGSENSOR_MODE_INIT;
	ctx->shutter = 0x3D0;
	ctx->gain = BASEGAIN * 4;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->dummy_pixel = 0;
	ctx->dummy_line = 0;
	ctx->ihdr_mode = 0;
	ctx->test_pattern = 0;
	ctx->current_fps = imgsensor_info.pre.max_framerate;

	return ERROR_NONE;
}	/*	open  */

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
	streaming_control(ctx, KAL_FALSE);

	LOG_INF("E\n");
	return ERROR_NONE;
}	/*	close  */

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
	LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_PREVIEW;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	preview_setting(ctx);

	LOG_INF("X\n");
	return ERROR_NONE;
}	/*	preview   */

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

	LOG_INF("X\n");
	return ERROR_NONE;
} /* capture(ctx) */

static kal_uint32 normal_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_VIDEO;
	ctx->pclk = imgsensor_info.normal_video.pclk;
	ctx->line_length = imgsensor_info.normal_video.linelength;
	ctx->frame_length = imgsensor_info.normal_video.framelength;
	ctx->min_frame_length = imgsensor_info.normal_video.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	normal_video_setting(ctx);

	LOG_INF("X\n");
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	ctx->pclk = imgsensor_info.hs_video.pclk;
	ctx->line_length = imgsensor_info.hs_video.linelength;
	ctx->frame_length = imgsensor_info.hs_video.framelength;
	ctx->min_frame_length = imgsensor_info.hs_video.framelength;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	ctx->autoflicker_en = KAL_FALSE;

	hs_video_setting(ctx);

	LOG_INF("X\n");
	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	ctx->pclk = imgsensor_info.slim_video.pclk;
	ctx->line_length = imgsensor_info.slim_video.linelength;
	ctx->frame_length = imgsensor_info.slim_video.framelength;
	ctx->min_frame_length = imgsensor_info.slim_video.framelength;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	ctx->autoflicker_en = KAL_FALSE;

	slim_video_setting(ctx);

	LOG_INF("X\n");
	return ERROR_NONE;
}	/*	slim_video	 */

static int get_resolution(struct subdrv_ctx *ctx,
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	int i = 0;

	for (i = SENSOR_SCENARIO_ID_MIN; i < SENSOR_SCENARIO_ID_MAX; i++) {
		if (i < imgsensor_info.sensor_mode_num &&
			i < ARRAY_SIZE(imgsensor_winsize_info)) {
			sensor_resolution->SensorWidth[i] = imgsensor_winsize_info[i].w2_tg_size;
			sensor_resolution->SensorHeight[i] = imgsensor_winsize_info[i].h2_tg_size;
		} else {
			sensor_resolution->SensorWidth[i] = 0;
			sensor_resolution->SensorHeight[i] = 0;
		}
	}

	return ERROR_NONE;
} /* get_resolution */

static int get_info(struct subdrv_ctx *ctx, enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MSDK_SENSOR_INFO_STRUCT *sensor_info,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DEBUG("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity =
		SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity =
		SENSOR_CLOCK_POLARITY_LOW;
	/* not use */
	sensor_info->SensorHsyncPolarity =
		SENSOR_CLOCK_POLARITY_LOW;
	/* inverse with datasheet*/
	sensor_info->SensorVsyncPolarity =
		SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType =
		imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType =
		imgsensor_info.mipi_sensor_type;
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

	sensor_info->SensorMasterClockSwitch = 0;
	/* not use */
	sensor_info->SensorDrivingCurrent =
		imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame =
		imgsensor_info.ae_shut_delay_frame;
	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	/* The frame of setting sensor gain */
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->PDAF_Support = 0;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->SensorWidthSampling = 0;	/* 0 is default 1x*/
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x*/
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
	LOG_DEBUG("framerate = %d\n ", framerate);
	if (framerate == 0) {
		/* Dynamic frame rate*/
		return ERROR_NONE;
	}
	if ((framerate == 300) &&
			(ctx->autoflicker_en == KAL_TRUE))
		ctx->current_fps = 296;
	else if ((framerate == 150) &&
			(ctx->autoflicker_en == KAL_TRUE))
		ctx->current_fps = 146;
	else
		ctx->current_fps = framerate;
	set_max_framerate(ctx, ctx->current_fps, 1);
	set_dummy(ctx);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(struct subdrv_ctx *ctx, kal_bool enable, UINT16 framerate)
{
	(void) framerate;

	LOG_DEBUG("enable = %d\n", enable);
	if (enable) {/*enable auto flicker*/
		ctx->autoflicker_en = KAL_TRUE;
	} else {/*Cancel Auto flick*/
		ctx->autoflicker_en = KAL_FALSE;
	}
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(struct subdrv_ctx *ctx,
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	DEBUG_LOG(ctx, "scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		frame_length = imgsensor_info.pre.pclk /
			framerate * 10 /
			imgsensor_info.pre.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.pre.framelength
				+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk /
			framerate * 10 /
			imgsensor_info.normal_video.linelength;
		ctx->dummy_line =
			(frame_length >
				imgsensor_info.normal_video.framelength) ?
			(frame_length -
				imgsensor_info.normal_video.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.normal_video.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		if (ctx->current_fps != imgsensor_info.cap.max_framerate) {
			LOG_DEBUG("Warning: current_fps %d fps is not support",
				framerate);
			LOG_DEBUG("so use cap's setting: %d fps!\n",
				imgsensor_info.cap.max_framerate / 10);
		}
		frame_length = imgsensor_info.cap.pclk /
			framerate * 10 /
			imgsensor_info.cap.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.cap.framelength) ?
			(frame_length - imgsensor_info.cap.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.cap.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk /
			framerate * 10 /
			imgsensor_info.hs_video.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.hs_video.framelength) ?
			(frame_length - imgsensor_info.hs_video.framelength) :
			0;
		ctx->frame_length =
			imgsensor_info.hs_video.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk /
			framerate * 10 /
			imgsensor_info.slim_video.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.slim_video.framelength) ?
			(frame_length - imgsensor_info.slim_video.framelength) :
			0;
		ctx->frame_length =
			imgsensor_info.slim_video.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	default:/*coding with  preview scenario by default*/
		frame_length = imgsensor_info.pre.pclk /
			framerate * 10 /
			imgsensor_info.pre.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.pre.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		LOG_INF("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(struct subdrv_ctx *ctx,
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	LOG_DEBUG("scenario_id = %d\n", scenario_id);

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

static kal_uint32 set_test_pattern_mode(struct subdrv_ctx *ctx, kal_uint32 mode)
{
	if (mode != ctx->test_pattern)
		pr_debug("mode: %d\n", mode);

	if (mode)
		write_cmos_sensor_16(ctx, 0x0600, mode); /*100% Color bar*/
	else if (ctx->test_pattern)
		write_cmos_sensor_16(ctx, 0x0600, 0x0000); /*No pattern*/

	ctx->test_pattern = mode;
	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_data(struct subdrv_ctx *ctx, struct mtk_test_pattern_data *data)
{

	DEBUG_LOG(ctx, "test_patterndata mode = %d  R = %x, Gr = %x,Gb = %x,B = %x\n",
		ctx->test_pattern,
		data->Channel_R >> 22, data->Channel_Gr >> 22,
		data->Channel_Gb >> 22, data->Channel_B >> 22);

	set_cmos_sensor_16(ctx, 0x0602, (data->Channel_R >> 22) & 0x3ff);
	//set_cmos_sensor(ctx, 0x0603, (data->Channel_R >> 22) & 0xff);
	set_cmos_sensor_16(ctx, 0x0604, (data->Channel_Gr >> 22) & 0x3ff);
	//set_cmos_sensor(ctx, 0x0605, (data->Channel_Gr >> 22) & 0xff);
	set_cmos_sensor_16(ctx, 0x0606, (data->Channel_B >> 22) & 0x3ff);
	//set_cmos_sensor(ctx, 0x0607, (data->Channel_B >> 22) & 0xff);
	set_cmos_sensor_16(ctx, 0x0608, (data->Channel_Gb >> 22) & 0x3ff);
	//set_cmos_sensor(ctx, 0x0609, (data->Channel_Gb >> 22) & 0xff);
	commit_write_sensor(ctx);
	return ERROR_NONE;
}

static int feature_control(struct subdrv_ctx *ctx, MSDK_SENSOR_FEATURE_ENUM feature_id,
	UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;

	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	switch (feature_id) {
	case SENSOR_FEATURE_GET_OUTPUT_FORMAT_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM1:
		case SENSOR_SCENARIO_ID_CUSTOM2:
		case SENSOR_SCENARIO_ID_CUSTOM3:
		case SENSOR_SCENARIO_ID_CUSTOM4:
		case SENSOR_SCENARIO_ID_CUSTOM5:
			*(feature_data + 1)
			= (enum ACDK_SENSOR_OUTPUT_DATA_FORMAT_ENUM)
				imgsensor_info.sensor_output_dataformat;
			break;
		}
	break;
	case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
		if ((void *)(uintptr_t) (*(feature_data + 1)) == NULL) {
			*(feature_data + 0) =
				sizeof(s5k3p9sp_ana_gain_table);
		} else {
			memcpy((void *)(uintptr_t) (*(feature_data + 1)),
			(void *)s5k3p9sp_ana_gain_table,
			sizeof(s5k3p9sp_ana_gain_table));
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
	case SENSOR_FEATURE_GET_MAX_EXP_LINE:
		*(feature_data + 2) =
			imgsensor_info.max_frame_length - imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_shutter;
		*(feature_data + 2) = imgsensor_info.exp_step;
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
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= 2500000;
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
		/*night_mode(ctx, (BOOL) *feature_data);*/
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
		/* if EEPROM does not exist in camera module.*/
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
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(ctx,
			(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
			*(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(ctx,
			(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode(ctx, (UINT32)*feature_data);
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN_DATA:
		set_test_pattern_data(ctx, (struct mtk_test_pattern_data *)feature_data);
		break;
	/*for factory mode auto testing*/
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		ctx->current_fps = *feature_data_32;
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_DEBUG("SENSOR_FEATURE_GET_CROP_INFO, scenarioId:%d\n",
			*feature_data_32);
		wininfo =
			(struct SENSOR_WINSIZE_INFO_STRUCT *)
			(uintptr_t)(*(feature_data + 1));
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
	case SENSOR_FEATURE_GET_PDAF_INFO:
		LOG_DEBUG("SENSOR_FEATURE_GET_PDAF_INFO, scenarioId:%lld\n",
			*feature_data);
		PDAFinfo =
			(struct SET_PD_BLOCK_INFO_T *)
			(uintptr_t)(*(feature_data + 1));

		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		LOG_DEBUG("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY, scenarioId:%lld\n",
			*feature_data);
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= 0;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= 0; /* video & capture use same setting*/
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= 0;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= 0;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= 0;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= 0;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length(ctx, (UINT16) *feature_data,
			(UINT16) *(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		/* margin info by scenario */
		*(feature_data + 2) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_GET_4CELL_DATA:
		{
			char *data = (char *)(uintptr_t)(*(feature_data + 1));
			UINT16 type = (UINT16)(*feature_data);
			/*get 4 cell data from eeprom*/
			if (type == FOUR_CELL_CAL_TYPE_XTALK_CAL) {
				LOG_DEBUG("Read Cross Talk Start");
				read_four_cell_from_eeprom(ctx, data);
				LOG_DEBUG("Read Cross Talk = %02x %02x %02x %02x %02x %02x\n",
					(UINT16)data[0], (UINT16)data[1],
					(UINT16)data[2], (UINT16)data[3],
					(UINT16)data[4], (UINT16)data[5]);
			}
		}
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND");
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
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*feature_return_para_32 = 4;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		default:
			*feature_return_para_32 = 1;
			break;
		}
		LOG_DEBUG("SENSOR_FEATURE_GET_BINNING_TYPE, AE_binning_type:%d\n",
			*feature_return_para_32);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		{
			kal_uint32 rate;

			switch (*feature_data) {
			case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
				rate =
				imgsensor_info.cap.mipi_pixel_rate;
				break;
			case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
				rate =
				imgsensor_info.normal_video.mipi_pixel_rate;
				break;
			case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
				rate =
				imgsensor_info.hs_video.mipi_pixel_rate;
				break;
			case SENSOR_SCENARIO_ID_SLIM_VIDEO:
				rate =
				imgsensor_info.slim_video.mipi_pixel_rate;
				break;
			case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
				rate =
					imgsensor_info.pre.mipi_pixel_rate;
				break;
			default:
					rate = 0;
					break;
			}
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
		}
		break;
	case SENSOR_FEATURE_PRELOAD_EEPROM_DATA:
		/*get eeprom preloader data*/
		*feature_return_para_32 = ctx->is_read_four_cell;
		*feature_para_len = 4;
		if (ctx->is_read_four_cell != 1)
			read_four_cell_from_eeprom(ctx, NULL);
		break;
	case SENSOR_FEATURE_SET_FRAMELENGTH:
		set_frame_length(ctx, (UINT16) (*feature_data));
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
			.hsize = 0x0910,
			.vsize = 0x06d0,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
#if USE_REMOSAIC
			.hsize = 0x1220,
			.vsize = 0x0da0,
#else
			.hsize = 0x0910,
			.vsize = 0x06d0,
#endif
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0780,
			.vsize = 0x0438,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_slim_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0280,
			.vsize = 0x01e0,
		},
	},
};

static int get_frame_desc(struct subdrv_ctx *ctx,
		int scenario_id, struct mtk_mbus_frame_desc *fd)
{
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_prev);
		memcpy(fd->entry, frame_desc_prev, sizeof(frame_desc_prev));
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cap);
		memcpy(fd->entry, frame_desc_cap, sizeof(frame_desc_cap));
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_vid);
		memcpy(fd->entry, frame_desc_vid, sizeof(frame_desc_vid));
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_slim_vid);
		memcpy(fd->entry, frame_desc_slim_vid, sizeof(frame_desc_slim_vid));
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
	.ana_gain_step = 32,
	.exposure_def = 0x3D0,
	.exposure_max = 0xfffc - 3,
	.exposure_min = 3,
	.exposure_step = 1,
	.margin = 3,
	.max_frame_length = 0xfffc,

	.mirror = IMAGE_NORMAL, //mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,	/*current shutter*/
	.gain = BASEGAIN * 4,			/*current gain*/
	.dummy_pixel = 0,		/*current dummypixel*/
	.dummy_line = 0,		/*current dummyline*/
	.current_fps = 0,
	/*full size current fps : 24fps for PIP, 30fps for Normal or ZSD*/
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,

	.current_scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW,
	.ihdr_mode = 0, /*sensor need support LE, SE with HDR feature*/
	.i2c_write_id = 0x20,
	.ae_ctrl_gph_en = 0,
};

static int init_ctx(struct subdrv_ctx *ctx,
		struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(ctx, &defctx, sizeof(*ctx));
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}

static int get_csi_param(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id,
	struct mtk_csi_param *csi_param)
{
	csi_param->legacy_phy = 0;
	csi_param->not_fixed_trail_settle = 0;
	csi_param->dphy_trail = 0;

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
	.get_csi_param = get_csi_param,
#ifdef IMGSENSOR_VC_ROUTING
	.get_frame_desc = get_frame_desc,
#endif
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_MCLK, 24, 0},
	{HW_ID_RST, 0, 1},
	{HW_ID_DVDD, 1100000, 1},
	{HW_ID_AVDD, 2800000, 1},
	{HW_ID_DOVDD, 1800000, 3},
	{HW_ID_MCLK_DRIVING_CURRENT, 2, 0},
	{HW_ID_RST, 1, 2}
};

const struct subdrv_entry s5k3p9sp_mipi_raw_entry = {
	.name = "s5k3p9sp_mipi_raw",
	.id = S5K3P9SP_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};
