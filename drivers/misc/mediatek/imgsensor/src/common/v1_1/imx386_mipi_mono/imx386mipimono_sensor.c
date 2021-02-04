/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "imx386mipimono_sensor.h"

/*******************Modify Following Strings for Debug************************/
#define PFX "imx386_camera_primax mono"
#define LOG_INF(fmt, args...)	pr_info(PFX "[%s] " fmt, __func__, ##args)
#define LOG_1 LOG_INF("IMX386,MIPI 4LANE\n")
#define SENSORDB LOG_INF
/**********************   Modify end	**************************************/

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static kal_uint8 mode_change;
static struct imgsensor_info_struct imgsensor_info = {
	/* Sensor ID Value: kd_imgsensor.h */
	.sensor_id = IMX386_SENSOR_ID,
	/*checksum value for Camera Auto Test */
	.checksum_value =  0xa353fed,

	.pre = {
		.pclk = 233300000,
		.linelength  = 4296,
		.framelength = 1780,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 2016,
		.grabwindow_height = 1508,
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 433300000,
		.linelength  = 4296,
		.framelength = 3300,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4032,
		.grabwindow_height = 3016,
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
	},
	/* capture1 mode must use same framelength,
	 * linelength with Capture mode for shutter calculate
	 */
	.cap1 = {
		.pclk = 400000000,
		.linelength  = 4704,
		.framelength = 3536,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 240,
	},
	.custom1 = {
	/*stereo camera bayer setting */
	  .pclk = 433300000,
		.linelength  = 4296,
		.framelength = 3300,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4032,
		.grabwindow_height = 3016,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.custom2 = {
	/*stereo camera mono setting */
		.pclk = 433300000,
		.linelength  = 4296,
		.framelength = 3300,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4032,
		.grabwindow_height = 3016,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.custom3 = {
	/* 4k setting */
		.pclk = 300000000,
		.linelength  = 4296,
		.framelength = 2302,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4032,
		.grabwindow_height = 2256,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 433300000,
		.linelength  = 4296,
		.framelength = 3300,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4032,
		.grabwindow_height = 3016,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 600000000,
		.linelength  = 4296,
		.framelength = 1144,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 1296,
		.grabwindow_height = 736,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
	},
	.slim_video = {
		.pclk = 600000000,
		.linelength  = 4296,
		.framelength = 4648,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 1296,
		.grabwindow_height = 736,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},

	.margin = 10,
	.min_shutter = 2,
	.max_frame_length = 0xFFFE,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.temperature_support = 1,
	.sensor_mode_num = 8,
	.frame_time_delay_frame = 3,
	.cap_delay_frame = 3,
	.pre_delay_frame = 3,
	.custom1_delay_frame = 3,
	.custom2_delay_frame = 3,
	.video_delay_frame = 3,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,

	.isp_driving_current = ISP_DRIVING_8MA, /* mclk driving current */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_MANUAL,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_MONO,
	.mclk = 24,/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_4_LANE,/* mipi lane num */
	.i2c_addr_table = {0x34, 0x20, 0xff},
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	/* IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT,
	 * Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x200,			/* current shutter */
	.gain = 0x200,				/* current gain */
	.dummy_pixel = 0,			/* current dummypixel */
	.dummy_line = 0,			/* current dummyline */
	.current_fps = 0,  /* 24fps for PIP, 30fps for Normal or ZSD */
	/*KAL_FALSE for disable auto flicker*/
	.autoflicker_en = KAL_FALSE,
	/*KAL_FALSE for in test pattern mode, KAL_TRUE for normal output */
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = KAL_FALSE, /* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x20,/* record current sensor's i2c write id */
};

/*VC1 for HDR(DT=0X35) , VC2 for PDAF(DT=0X36), unit : 10bit*/
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[8] = {
	{0x02, 0x0a,	 0x00,	 0x08, 0x40, 0x00,
	0x00, 0x2b, 0x7E0, 0x05E4, 0x01, 0x00, 0x0000, 0x0000,
	0x00, 0x36, 0x09D8, 0x0001, 0x03, 0x00, 0x0000, 0x0000},
	/* Capture mode setting */
	{0x03, 0x0a,	 0x00,	 0x08, 0x40, 0x00,
	0x00, 0x2b, 0x0FC0, 0x0BC8, 0x00, 0x35, 0x0280, 0x0001,
	0x00, 0x36, 0x13B0, 0x0001, 0x03, 0x00, 0x0000, 0x0000},
	/* Video mode setting */
	{0x02, 0x0a,	 0x00,	 0x08, 0x40, 0x00,
	0x00, 0x2b, 0x7E0, 0x05E4, 0x00, 0x35, 0x0280, 0x0001,
	0x00, 0x36, 0x09D8, 0x0001, 0x03, 0x00, 0x0000, 0x0000},
	/*Slim Video1  mode setting */
	{0x02, 0x0a,	 0x00,	 0x08, 0x40, 0x00,
	0x00, 0x2b, 0x7E0, 0x05E4, 0x00, 0x35, 0x0280, 0x0001,
	0x00, 0x36, 0x0654, 0x0001, 0x03, 0x00, 0x0000, 0x0000},
	/* Slim Video2 mode setting */
	{0x02, 0x0a,	 0x00,	 0x08, 0x40, 0x00,
	0x00, 0x2b, 0x7E0, 0x05E4, 0x00, 0x35, 0x0280, 0x0001,
	0x00, 0x36, 0x09D8, 0x0001, 0x03, 0x00, 0x0000, 0x0000},
	/* Custom1 mode setting */
	{0x03, 0x0a,	 0x00,	 0x08, 0x40, 0x00,
	0x00, 0x2b, 0x0FC0, 0x0BC8, 0x00, 0x35, 0x0280, 0x0001,
	0x00, 0x36, 0x13B0, 0x0001, 0x03, 0x00, 0x0000, 0x0000},
	/* Custom2 mode setting */
	{0x03, 0x0a,	 0x00,	 0x08, 0x40, 0x00,
	0x00, 0x2b, 0x0FC0, 0x0BC8, 0x00, 0x35, 0x0280, 0x0001,
	0x00, 0x36, 0x13B0, 0x0001, 0x03, 0x00, 0x0000, 0x0000},
	/* Custom3 mode setting */
	{0x02, 0x0a,	 0x00,	 0x08, 0x40, 0x00,
	0x00, 0x2b, 0x0FC0, 0x08D0, 0x00, 0x35, 0x0280, 0x0001,
	/* 0x00, 0x2b, 0x7E0, 0x05E4, 0x00, 0x35, 0x0280, 0x0001, */
	0x00, 0x36, 0x13B0, 0x0001, 0x03, 0x00, 0x0000, 0x0000},
};

/* Sensor output window information*/
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[8] = {
	{ 4032, 3016,	0,	0,	4032,	3016, 2016, 1508,   0,
		0, 2016, 1508,	 0, 0, 2016, 1508}, /* Preview */
	{ 4032, 3016,	0,	0,	4032,	3016, 4032, 3016,   0,
		0, 4032, 3016,	 0, 0, 4032, 3016}, /* capture */
	{ 4032, 3016,	0,	0,	4032,	3016, 4032, 3016,	0,
		0, 4032, 3016, 0, 0, 4032, 3016}, /* normal video*/
	{ 4032, 3016,	720,	772,	2592,	1472, 1296, 736,
		0, 0, 1296, 736,  0, 0, 1296, 736}, /* high speed video */
	{ 4032, 3016,	720,	772,	2592, 1472, 1296, 736,
		0, 0, 1296, 736,  0, 0, 1296, 736}, /* slim video */
	{ 4032, 3016,	0,	0,	4032,	3016, 4032, 3016,   0,
		0, 4032, 3016,	 0, 0, 4032, 3016}, /* custom1 */
	{ 4032, 3016,	0,	0,	4032,	3016, 4032, 3016,   0,
		0, 4032, 3016,	 0, 0, 4032, 3016}, /* custom2 */
	{ 4032, 3016,	0,	380,	4032,	2256, 4032, 2256,
		0,	0, 4032, 2256,	 0, 0, 4032, 2256}, /* custom3 */
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info =
 /* for 2M8 non mirror flip */
{
	.i4OffsetX = 28,
	.i4OffsetY = 31,
	.i4PitchX = 64,
	.i4PitchY = 64,
	.i4PairNum = 16,
	.i4SubBlkW = 16,
	.i4SubBlkH = 16,

	.i4PosL = {{28, 31}, {80, 31}, {44, 35}, {64, 35},
				{32, 51}, {76, 51}, {48, 55}, {60, 55},
				{48, 63}, {60, 63}, {32, 67}, {76, 67},
				{44, 83}, {64, 83}, {28, 87}, {80, 87} },

	.i4PosR = {{28, 35}, {80, 35}, {44, 39}, {64, 39},
				{32, 47}, {76, 47}, {48, 51}, {60, 51},
				{48, 67}, {60, 67}, {32, 71}, {76, 71},
				{44, 79}, {64, 79}, {28, 83}, {80, 83} },
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor_w(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF),
					(char)(para >> 8), (char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}

static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF),
					(char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static void imx386_get_pdaf_reg_setting(MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2*i;
		regDa[idx+1] = read_cmos_sensor(regDa[idx]);
	/* LOG_INF("%x %x", regDa[idx], regDa[idx+1]); */
	}
}

static void imx386_set_pdaf_reg_setting(MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2*i;
		write_cmos_sensor(regDa[idx], regDa[idx+1]);
	/* LOG_INF("%x %x", regDa[idx], regDa[idx+1]); */
	}
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n",
	imgsensor.dummy_line, imgsensor.dummy_pixel);
	write_cmos_sensor_w(0x0340, imgsensor.frame_length & 0xFFFF);
}	/*	set_dummy  */


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d, min framelength should enable(%d)\n",
			framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ?
		frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line =
		imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line =
			imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */

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
#define MAX_SHUTTER	12103350		/* 120s long exposure time */
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_uint32 line_length = 0;
	kal_uint16 long_exp_times = 0;
	kal_uint16 long_exp_shift = 0;

	/* limit max exposure time to be 120s */
	if (shutter > MAX_SHUTTER)
		shutter = MAX_SHUTTER;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	LOG_INF("enter shutter =%d\n", shutter);
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	if (shutter > (imgsensor_info.max_frame_length -
		imgsensor_info.margin)) {
		long_exp_times = shutter / (imgsensor_info.max_frame_length -
			imgsensor_info.margin);
		if (shutter % (imgsensor_info.max_frame_length -
			imgsensor_info.margin))
		long_exp_times++;
		if (long_exp_times > 128)
			long_exp_times = 128;
		long_exp_shift = fls(long_exp_times) - 1;
		if (long_exp_times & (~(1 << long_exp_shift)))
			long_exp_shift++;

		long_exp_times = 1 << long_exp_shift;
		write_cmos_sensor(0x3004, long_exp_shift);
		shutter = shutter / long_exp_times;
		if (shutter > (imgsensor_info.max_frame_length -
			imgsensor_info.margin)) {
			line_length = shutter * 4296 /
			(imgsensor_info.max_frame_length -
			imgsensor_info.margin);
			line_length = (line_length + 1) / 2 * 2;
		}

		spin_lock(&imgsensor_drv_lock);
		if (shutter > imgsensor.min_frame_length -
			imgsensor_info.margin)
			imgsensor.frame_length =
			shutter + imgsensor_info.margin;
		else
			imgsensor.frame_length = imgsensor.min_frame_length;
		if (imgsensor.frame_length > imgsensor_info.max_frame_length)
			imgsensor.frame_length =
			imgsensor_info.max_frame_length;
		spin_unlock(&imgsensor_drv_lock);

		/* line_length range is 4296 <-> 32766 */
		if (line_length > 32766)
			line_length = 32766;
		if (line_length < 4296)
			line_length = 4296;
		write_cmos_sensor_w(0x0342, line_length & 0xFFFF);
	}

	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;
	if (shutter > (imgsensor_info.max_frame_length -
		imgsensor_info.margin))
		shutter = (imgsensor_info.max_frame_length -
		imgsensor_info.margin);
	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10
			/ imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305) {
			set_max_framerate(296, 0);
			write_cmos_sensor(0x0104, 0x01);
		} else if (realtime_fps >= 237 && realtime_fps <= 245) {
			set_max_framerate(236, 0);
			write_cmos_sensor(0x0104, 0x01);
		} else if (realtime_fps >= 147 && realtime_fps <= 150) {
			set_max_framerate(146, 0);
			write_cmos_sensor(0x0104, 0x01);
		} else {
			/* Extend frame length */
			write_cmos_sensor(0x0104, 0x01);
			write_cmos_sensor_w(0x0340,
				imgsensor.frame_length & 0xFFFF);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x0104, 0x01);
		write_cmos_sensor_w(0x0340, imgsensor.frame_length & 0xFFFF);
	}
	write_cmos_sensor(0x0350, 0x01); /* enable auto extend */
	/* Update Shutter */
	write_cmos_sensor_w(0x0202, shutter & 0xFFFF);
	write_cmos_sensor(0x0104, 0x00);
	LOG_INF("shutter=%d, framelength=%d, line_length=%d, shift:%d\n"
		, shutter, imgsensor.frame_length, line_length, long_exp_shift);
}
static void set_shutter_frame_length(kal_uint32 shutter,
	kal_uint32 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_uint32 line_length = 0;
	kal_uint16 long_exp_times = 0;
	kal_uint16 long_exp_shift = 0;

	/* limit max exposure time to be 120s */
	if (shutter > MAX_SHUTTER)
		shutter = MAX_SHUTTER;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	LOG_INF("enter shutter =%d\n", shutter);
	spin_lock(&imgsensor_drv_lock);
	if (frame_length > 1)
		imgsensor.frame_length = frame_length;

	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	/* Just should be called in capture case with long exposure */
	if (shutter > (imgsensor_info.max_frame_length -
			imgsensor_info.margin)) {
		long_exp_times = shutter / (imgsensor_info.max_frame_length -
			imgsensor_info.margin);
		if (shutter % (imgsensor_info.max_frame_length -
			imgsensor_info.margin))
		long_exp_times++;
		if (long_exp_times > 128)
			long_exp_times = 128;
		long_exp_shift = fls(long_exp_times) - 1;
		if (long_exp_times & (~(1 << long_exp_shift)))
			long_exp_shift++;

		long_exp_times = 1 << long_exp_shift;
		write_cmos_sensor(0x3004, long_exp_shift);
		shutter = shutter / long_exp_times;
		if (shutter > (imgsensor_info.max_frame_length -
			imgsensor_info.margin)) {
			line_length = shutter * 4296 /
			(imgsensor_info.max_frame_length -
			imgsensor_info.margin);
		line_length = (line_length + 1) / 2 * 2;
		}

		spin_lock(&imgsensor_drv_lock);
		if (shutter > imgsensor.min_frame_length -
			imgsensor_info.margin)
			imgsensor.frame_length = shutter +
			imgsensor_info.margin;
		else
			imgsensor.frame_length = imgsensor.min_frame_length;
		if (imgsensor.frame_length > imgsensor_info.max_frame_length)
			imgsensor.frame_length =
			imgsensor_info.max_frame_length;
		spin_unlock(&imgsensor_drv_lock);

		/* line_length range is 4296 <-> 32766 */
		if (line_length > 32766)
			line_length = 32766;
		if (line_length < 4296)
			line_length = 4296;
		write_cmos_sensor_w(0x0342, line_length & 0xFFFF);
	}

	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;
	if (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
		shutter = (imgsensor_info.max_frame_length -
		imgsensor_info.margin);
	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10
			/ imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305) {
			set_max_framerate(296, 0);
			write_cmos_sensor(0x0104, 0x01);
		} else if (realtime_fps >= 237 && realtime_fps <= 245) {
			set_max_framerate(236, 0);
			write_cmos_sensor(0x0104, 0x01);
		} else if (realtime_fps >= 147 && realtime_fps <= 150) {
			set_max_framerate(146, 0);
			write_cmos_sensor(0x0104, 0x01);
		} else {
			/* Extend frame length */
			write_cmos_sensor(0x0104, 0x01);
			write_cmos_sensor_w(0x0340,
				imgsensor.frame_length & 0xFFFF);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x0104, 0x01);
		write_cmos_sensor_w(0x0340, imgsensor.frame_length & 0xFFFF);
	}

	/* Update Shutter */
	write_cmos_sensor(0x0350, 0x00); /* Disable auto extend */
	write_cmos_sensor_w(0x0202, shutter & 0xFFFF);
	write_cmos_sensor(0x0104, 0x00);
	LOG_INF("shutter=%d, framelength=%d, length=%d, shift:%d\n"
		, shutter, imgsensor.frame_length, line_length, long_exp_shift);
}
#define MaxGainIndex (255)

static const kal_uint16 IMX386MIPI_sensorGainMapping[MaxGainIndex][2] = {
	{64, 0},
	{65, 6},
	{66, 12},
	{67, 20},
	{68, 27},
	{69, 34},
	{70, 43},
	{71, 51},
	{72, 55},
	{73, 63},
	{74, 67},
	{75, 75},
	{76, 79},
	{77, 85},
	{78, 92},
	{79, 96},
	{80, 100},
	{81, 106},
	{82, 112},
	{83, 116},
	{84, 122},
	{85, 125},
	{86, 130},
	{87, 136},
	{88, 139},
	{89, 144},
	{90, 146},
	{91, 152},
	{92, 154},
	{93, 159},
	{94, 162},
	{95, 167},
	{96, 169},
	{97, 173},
	{98, 176},
	{100, 184},
	{101, 186},
	{102, 190},
	{103, 193},
	{104, 196},
	{105, 200},
	{106, 202},
	{107, 206},
	{108, 208},
	{110, 213},
	{111, 216},
	{112, 220},
	{113, 221},
	{114, 224},
	{115, 226},
	{116, 230},
	{117, 231},
	{118, 234},
	{120, 239},
	{121, 242},
	{122, 243},
	{123, 246},
	{124, 247},
	{125, 249},
	{126, 251},
	{127, 253},
	{128, 255},
	{130, 259},
	{131, 261},
	{132, 263},
	{133, 265},
	{134, 267},
	{135, 269},
	{136, 271},
	{137, 272},
	{138, 274},
	{140, 278},
	{141, 279},
	{142, 281},
	{143, 283},
	{144, 284},
	{145, 286},
	{146, 287},
	{147, 289},
	{148, 290},
	{150, 293},
	{151, 295},
	{152, 296},
	{153, 298},
	{154, 299},
	{155, 300},
	{156, 302},
	{157, 303},
	{158, 304},
	{160, 307},
	{161, 308},
	{162, 310},
	{163, 311},
	{164, 312},
	{165, 313},
	{166, 315},
	{167, 316},
	{168, 317},
	{170, 319},
	{171, 320},
	{172, 321},
	{173, 323},
	{174, 324},
	{175, 325},
	{176, 326},
	{177, 327},
	{178, 328},
	{180, 330},
	{181, 331},
	{182, 332},
	{183, 333},
	{184, 334},
	{185, 335},
	{186, 336},
	{187, 337},
	{188, 338},
	{191, 340},
	{192, 341},
	{193, 342},
	{194, 343},
	{195, 344},
	{196, 345},
	{197, 346},
	{199, 347},
	{200, 348},
	{202, 350},
	{204, 351},
	{205, 352},
	{206, 353},
	{207, 354},
	{209, 355},
	{210, 356},
	{211, 357},
	{213, 358},
	{216, 360},
	{217, 361},
	{218, 362},
	{220, 363},
	{221, 364},
	{223, 365},
	{224, 366},
	{226, 367},
	{228, 368},
	{229, 369},
	{231, 370},
	{232, 371},
	{234, 372},
	{236, 373},
	{237, 374},
	{239, 375},
	{241, 376},
	{243, 377},
	{245, 378},
	{246, 379},
	{248, 380},
	{250, 381},
	{252, 382},
	{254, 383},
	{256, 384},
	{258, 385},
	{260, 386},
	{262, 387},
	{264, 388},
	{266, 389},
	{269, 390},
	{271, 391},
	{273, 392},
	{275, 393},
	{278, 394},
	{280, 395},
	{282, 396},
	{285, 397},
	{287, 398},
	{290, 399},
	{293, 400},
	{295, 401},
	{298, 402},
	{301, 403},
	{303, 404},
	{306, 405},
	{309, 406},
	{312, 407},
	{315, 408},
	{318, 409},
	{321, 410},
	{324, 411},
	{328, 412},
	{331, 413},
	{334, 414},
	{338, 415},
	{341, 416},
	{345, 417},
	{349, 418},
	{352, 419},
	{356, 420},
	{360, 421},
	{364, 422},
	{368, 423},
	{372, 424},
	{377, 425},
	{381, 426},
	{386, 427},
	{390, 428},
	{395, 429},
	{400, 430},
	{405, 431},
	{410, 432},
	{415, 433},
	{420, 434},
	{426, 435},
	{431, 436},
	{437, 437},
	{443, 438},
	{449, 439},
	{455, 440},
	{462, 441},
	{468, 442},
	{475, 443},
	{482, 444},
	{489, 445},
	{496, 446},
	{504, 447},
	{512, 448},
	{520, 449},
	{529, 450},
	{537, 451},
	{546, 452},
	{555, 453},
	{565, 454},
	{575, 455},
	{585, 456},
	{596, 457},
	{607, 458},
	{618, 459},
	{630, 460},
	{643, 461},
	{655, 462},
	{669, 463},
	{683, 464},
	{697, 465},
	{712, 466},
	{728, 467},
	{745, 468},
	{762, 469},
	{780, 470},
	{799, 471},
	{819, 472},
	{840, 473},
	{862, 474},
	{886, 475},
	{910, 476},
	{936, 477},
	{964, 478},
	{993, 479},
	{1024, 480},
};
#if 0
static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0000;
	/* gain = 64 = 1x real gain */
	reg_gain = 512 - (512 * 64 / gain);
	return (kal_uint16)reg_gain;
}
#else

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint8 iI = 0;

	for (iI = 0; iI < MaxGainIndex; iI++) {
		if (gain <= IMX386MIPI_sensorGainMapping[iI][0])
			return IMX386MIPI_sensorGainMapping[iI][1];
	}
	LOG_INF("exit IMX386MIPI_sensorGainMapping function\n");
	return IMX386MIPI_sensorGainMapping[iI-1][1];
}
#endif
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
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0000;

	LOG_INF("set gain = %d\n", gain);
	if (gain < BASEGAIN || gain > 16 * BASEGAIN) {
		LOG_INF("Error gain setting");
		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 16 * BASEGAIN)
			gain = 16 * BASEGAIN;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor_w(0x0204, (reg_gain&0xFFFF));
	/*
	 * WORKAROUND! stream on after set shutter/gain, which will get
	 * first valid capture frame.
	 */
	if (mode_change && (imgsensor.sensor_mode == IMGSENSOR_MODE_CAPTURE)) {
		write_cmos_sensor(0x0100, 0x01);
		mode_change = 0;
	}
	return gain;
}	/*	set_gain  */

/* ihdr_write_shutter_gain not support for s5k2M8 */
static void ihdr_write_shutter_gain(kal_uint16 le,
	kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
	if (imgsensor.ihdr_en) {

		spin_lock(&imgsensor_drv_lock);
		if (le > imgsensor.min_frame_length -
				imgsensor_info.margin)
			imgsensor.frame_length = le + imgsensor_info.margin;
		else
			imgsensor.frame_length = imgsensor.min_frame_length;
		if (imgsensor.frame_length > imgsensor_info.max_frame_length)
			imgsensor.frame_length =
			imgsensor_info.max_frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (le < imgsensor_info.min_shutter)
			le = imgsensor_info.min_shutter;
		if (se < imgsensor_info.min_shutter)
			se = imgsensor_info.min_shutter;

		/* Extend frame length first */
		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);

		write_cmos_sensor(0x3502, (le << 4) & 0xFF);
		write_cmos_sensor(0x3501, (le >> 4) & 0xFF);
		write_cmos_sensor(0x3500, (le >> 12) & 0x0F);

		write_cmos_sensor(0x3512, (se << 4) & 0xFF);
		write_cmos_sensor(0x3511, (se >> 4) & 0xFF);
		write_cmos_sensor(0x3510, (se >> 12) & 0x0F);

		set_gain(gain);
	}

}


/*************************************************************************
 * FUNCTION
 *	night_mode
 *
 * DESCRIPTION
 *	This function night mode of sensor.
 *
 * PARAMETERS
 *	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}	/*	night_mode	*/
static void sensor_init(void)
{
	LOG_INF("%s.\n", __func__);
	/* External Clock Setting */
	write_cmos_sensor(0x0136, 0x18);
	write_cmos_sensor(0x0137, 0x00);
	/* Register version */
	write_cmos_sensor(0x3A7D, 0x00);
	write_cmos_sensor(0x3A7E, 0x01);
	write_cmos_sensor(0x3A7F, 0x05);
	/* Global Setting */
	write_cmos_sensor(0x0101, 0x00);
	write_cmos_sensor(0x3100, 0x00);
	write_cmos_sensor(0x3101, 0x40);
	write_cmos_sensor(0x3102, 0x00);
	write_cmos_sensor(0x3103, 0x10);
	write_cmos_sensor(0x3104, 0x01);
	write_cmos_sensor(0x3105, 0xE8);
	write_cmos_sensor(0x3106, 0x01);
	write_cmos_sensor(0x3107, 0xF0);
	write_cmos_sensor(0x3150, 0x04);
	write_cmos_sensor(0x3151, 0x03);
	write_cmos_sensor(0x3152, 0x02);
	write_cmos_sensor(0x3153, 0x01);
	write_cmos_sensor(0x5A86, 0x00);
	write_cmos_sensor(0x5A87, 0x82);
	write_cmos_sensor(0x5D1A, 0x00);
	write_cmos_sensor(0x5D95, 0x02);
	write_cmos_sensor(0x5E1B, 0x00);
	write_cmos_sensor(0x5F5A, 0x00);
	write_cmos_sensor(0x5F5B, 0x04);
	write_cmos_sensor(0x682C, 0x31);
	write_cmos_sensor(0x6831, 0x31);
	write_cmos_sensor(0x6835, 0x0E);
	write_cmos_sensor(0x6836, 0x31);
	write_cmos_sensor(0x6838, 0x30);
	write_cmos_sensor(0x683A, 0x06);
	write_cmos_sensor(0x683B, 0x33);
	write_cmos_sensor(0x683D, 0x30);
	write_cmos_sensor(0x6842, 0x31);
	write_cmos_sensor(0x6844, 0x31);
	write_cmos_sensor(0x6847, 0x31);
	write_cmos_sensor(0x6849, 0x31);
	write_cmos_sensor(0x684D, 0x0E);
	write_cmos_sensor(0x684E, 0x32);
	write_cmos_sensor(0x6850, 0x31);
	write_cmos_sensor(0x6852, 0x06);
	write_cmos_sensor(0x6853, 0x33);
	write_cmos_sensor(0x6855, 0x31);
	write_cmos_sensor(0x685A, 0x32);
	write_cmos_sensor(0x685C, 0x33);
	write_cmos_sensor(0x685F, 0x31);
	write_cmos_sensor(0x6861, 0x33);
	write_cmos_sensor(0x6865, 0x0D);
	write_cmos_sensor(0x6866, 0x33);
	write_cmos_sensor(0x6868, 0x31);
	write_cmos_sensor(0x686B, 0x34);
	write_cmos_sensor(0x686D, 0x31);
	write_cmos_sensor(0x6872, 0x32);
	write_cmos_sensor(0x6877, 0x33);
	write_cmos_sensor(0x7FF0, 0x01);
	write_cmos_sensor(0x7FF4, 0x08);
	write_cmos_sensor(0x7FF5, 0x3C);
	write_cmos_sensor(0x7FFA, 0x01);
	write_cmos_sensor(0x7FFD, 0x00);
	write_cmos_sensor(0x831E, 0x00);
	write_cmos_sensor(0x831F, 0x00);
	write_cmos_sensor(0x9301, 0xBD);
	write_cmos_sensor(0x9B94, 0x03);
	write_cmos_sensor(0x9B95, 0x00);
	write_cmos_sensor(0x9B96, 0x08);
	write_cmos_sensor(0x9B97, 0x00);
	write_cmos_sensor(0x9B98, 0x0A);
	write_cmos_sensor(0x9B99, 0x00);
	write_cmos_sensor(0x9BA7, 0x18);
	write_cmos_sensor(0x9BA8, 0x18);
	write_cmos_sensor(0x9D04, 0x08);
	write_cmos_sensor(0x9D50, 0x8C);
	write_cmos_sensor(0x9D51, 0x64);
	write_cmos_sensor(0x9D52, 0x50);
	write_cmos_sensor(0x9E31, 0x04);
	write_cmos_sensor(0x9E32, 0x04);
	write_cmos_sensor(0x9E33, 0x04);
	write_cmos_sensor(0x9E34, 0x04);
	write_cmos_sensor(0xA200, 0x00);
	write_cmos_sensor(0xA201, 0x0A);
	write_cmos_sensor(0xA202, 0x00);
	write_cmos_sensor(0xA203, 0x0A);
	write_cmos_sensor(0xA204, 0x00);
	write_cmos_sensor(0xA205, 0x0A);
	write_cmos_sensor(0xA206, 0x01);
	write_cmos_sensor(0xA207, 0xC0);
	write_cmos_sensor(0xA208, 0x00);
	write_cmos_sensor(0xA209, 0xC0);
	write_cmos_sensor(0xA20C, 0x00);
	write_cmos_sensor(0xA20D, 0x0A);
	write_cmos_sensor(0xA20E, 0x00);
	write_cmos_sensor(0xA20F, 0x0A);
	write_cmos_sensor(0xA210, 0x00);
	write_cmos_sensor(0xA211, 0x0A);
	write_cmos_sensor(0xA212, 0x01);
	write_cmos_sensor(0xA213, 0xC0);
	write_cmos_sensor(0xA214, 0x00);
	write_cmos_sensor(0xA215, 0xC0);
	write_cmos_sensor(0xA300, 0x00);
	write_cmos_sensor(0xA301, 0x0A);
	write_cmos_sensor(0xA302, 0x00);
	write_cmos_sensor(0xA303, 0x0A);
	write_cmos_sensor(0xA304, 0x00);
	write_cmos_sensor(0xA305, 0x0A);
	write_cmos_sensor(0xA306, 0x01);
	write_cmos_sensor(0xA307, 0xC0);
	write_cmos_sensor(0xA308, 0x00);
	write_cmos_sensor(0xA309, 0xC0);
	write_cmos_sensor(0xA30C, 0x00);
	write_cmos_sensor(0xA30D, 0x0A);
	write_cmos_sensor(0xA30E, 0x00);
	write_cmos_sensor(0xA30F, 0x0A);
	write_cmos_sensor(0xA310, 0x00);
	write_cmos_sensor(0xA311, 0x0A);
	write_cmos_sensor(0xA312, 0x01);
	write_cmos_sensor(0xA313, 0xC0);
	write_cmos_sensor(0xA314, 0x00);
	write_cmos_sensor(0xA315, 0xC0);
	write_cmos_sensor(0xBC19, 0x01);
	write_cmos_sensor(0xBC1C, 0x0A);
	/* Image Tuning Setting */
	write_cmos_sensor(0x3035, 0x01);
	write_cmos_sensor(0x3051, 0x00);
	write_cmos_sensor(0x7F47, 0x00);
	write_cmos_sensor(0x7F78, 0x00);
	write_cmos_sensor(0x7F89, 0x00);
	write_cmos_sensor(0x7F93, 0x00);
	write_cmos_sensor(0x7FB4, 0x00);
	write_cmos_sensor(0x7FCC, 0x01);
	write_cmos_sensor(0x9D02, 0x00);
	write_cmos_sensor(0x9D44, 0x8C);
	write_cmos_sensor(0x9D62, 0x8C);
	write_cmos_sensor(0x9D63, 0x50);
	write_cmos_sensor(0x9D64, 0x1B);
	write_cmos_sensor(0x9E0D, 0x00);
	write_cmos_sensor(0x9E0E, 0x00);
	write_cmos_sensor(0x9E15, 0x0A);
	write_cmos_sensor(0x9F02, 0x00);
	write_cmos_sensor(0x9F03, 0x23);
	write_cmos_sensor(0x9F4E, 0x00);
	write_cmos_sensor(0x9F4F, 0x42);
	write_cmos_sensor(0x9F54, 0x00);
	write_cmos_sensor(0x9F55, 0x5A);
	write_cmos_sensor(0x9F6E, 0x00);
	write_cmos_sensor(0x9F6F, 0x10);
	write_cmos_sensor(0x9F72, 0x00);
	write_cmos_sensor(0x9F73, 0xC8);
	write_cmos_sensor(0x9F74, 0x00);
	write_cmos_sensor(0x9F75, 0x32);
	write_cmos_sensor(0x9FD3, 0x00);
	write_cmos_sensor(0x9FD4, 0x00);
	write_cmos_sensor(0x9FD5, 0x00);
	write_cmos_sensor(0x9FD6, 0x3C);
	write_cmos_sensor(0x9FD7, 0x3C);
	write_cmos_sensor(0x9FD8, 0x3C);
	write_cmos_sensor(0x9FD9, 0x00);
	write_cmos_sensor(0x9FDA, 0x00);
	write_cmos_sensor(0x9FDB, 0x00);
	write_cmos_sensor(0x9FDC, 0xFF);
	write_cmos_sensor(0x9FDD, 0xFF);
	write_cmos_sensor(0x9FDE, 0xFF);
	write_cmos_sensor(0xA002, 0x00);
	write_cmos_sensor(0xA003, 0x14);
	write_cmos_sensor(0xA04E, 0x00);
	write_cmos_sensor(0xA04F, 0x2D);
	write_cmos_sensor(0xA054, 0x00);
	write_cmos_sensor(0xA055, 0x40);
	write_cmos_sensor(0xA06E, 0x00);
	write_cmos_sensor(0xA06F, 0x10);
	write_cmos_sensor(0xA072, 0x00);
	write_cmos_sensor(0xA073, 0xC8);
	write_cmos_sensor(0xA074, 0x00);
	write_cmos_sensor(0xA075, 0x32);
	write_cmos_sensor(0xA0CA, 0x04);
	write_cmos_sensor(0xA0CB, 0x04);
	write_cmos_sensor(0xA0CC, 0x04);
	write_cmos_sensor(0xA0D3, 0x0A);
	write_cmos_sensor(0xA0D4, 0x0A);
	write_cmos_sensor(0xA0D5, 0x0A);
	write_cmos_sensor(0xA0D6, 0x00);
	write_cmos_sensor(0xA0D7, 0x00);
	write_cmos_sensor(0xA0D8, 0x00);
	write_cmos_sensor(0xA0D9, 0x18);
	write_cmos_sensor(0xA0DA, 0x18);
	write_cmos_sensor(0xA0DB, 0x18);
	write_cmos_sensor(0xA0DC, 0x00);
	write_cmos_sensor(0xA0DD, 0x00);
	write_cmos_sensor(0xA0DE, 0x00);
	write_cmos_sensor(0xBCB2, 0x01);

	write_cmos_sensor(0x0138, 0x01);
}


static void preview_setting(void)
{
	LOG_INF("%s.\n", __func__);
	/*
	 * 1/2Binning@30fps
	 * H: 2016
	 * V: 1508
	 */
	/* Mode Setting */
	write_cmos_sensor(0x0112, 0x0A);
	write_cmos_sensor(0x0113, 0x0A);
	/* Clock Setting */
	write_cmos_sensor(0x0301, 0x06);
	write_cmos_sensor(0x0303, 0x02);
	write_cmos_sensor(0x0305, 0x0C);
	write_cmos_sensor(0x0306, 0x01);
	write_cmos_sensor(0x0307, 0x5E);
	write_cmos_sensor(0x0309, 0x0A);
	write_cmos_sensor(0x030B, 0x01);
	write_cmos_sensor(0x030D, 0x0C);
	write_cmos_sensor(0x030E, 0x01);
	write_cmos_sensor(0x030F, 0x5E);
	write_cmos_sensor(0x0310, 0x00);
	write_cmos_sensor(0x315D, 0x01);
	/* Output Size Setting */
	write_cmos_sensor(0x0342, 0x10);
	write_cmos_sensor(0x0343, 0xC8);
	/* Output Size Setting */
	write_cmos_sensor(0x0340, 0x06);
	write_cmos_sensor(0x0341, 0xF4);
	/* Output Size Setting */
	write_cmos_sensor(0x0344, 0x00);
	write_cmos_sensor(0x0345, 0x00);
	write_cmos_sensor(0x0346, 0x00);
	write_cmos_sensor(0x0347, 0x00);
	write_cmos_sensor(0x0348, 0x0F);
	write_cmos_sensor(0x0349, 0xBF);
	write_cmos_sensor(0x034A, 0x0B);
	write_cmos_sensor(0x034B, 0xC7);
	/* Output Size Setting */
	write_cmos_sensor(0x0385, 0x01);
	write_cmos_sensor(0x0387, 0x01);
	write_cmos_sensor(0x0900, 0x01);
	write_cmos_sensor(0x0901, 0x12);
	write_cmos_sensor(0x300D, 0x00);
	write_cmos_sensor(0x302E, 0x00);
	write_cmos_sensor(0x0401, 0x01);
	write_cmos_sensor(0x0404, 0x00);
	write_cmos_sensor(0x0405, 0x20);
	write_cmos_sensor(0x040C, 0x0F);
	write_cmos_sensor(0x040D, 0xC0);
	write_cmos_sensor(0x040E, 0x05);
	write_cmos_sensor(0x040F, 0xE4);
	write_cmos_sensor(0x034C, 0x07);
	write_cmos_sensor(0x034D, 0xE0);
	write_cmos_sensor(0x034E, 0x05);
	write_cmos_sensor(0x034F, 0xE4);
	/* Other Setting */
	write_cmos_sensor(0x0114, 0x03);
	write_cmos_sensor(0x0408, 0x00);
	write_cmos_sensor(0x0409, 0x00);
	write_cmos_sensor(0x040A, 0x00);
	write_cmos_sensor(0x040B, 0x00);
	write_cmos_sensor(0x0902, 0x00);
	write_cmos_sensor(0x3030, 0x00);
	write_cmos_sensor(0x3031, 0x01);
	write_cmos_sensor(0x3032, 0x00);
	write_cmos_sensor(0x3047, 0x01);
	write_cmos_sensor(0x3049, 0x01);
	write_cmos_sensor(0x30E6, 0x00);
	write_cmos_sensor(0x30E7, 0x00);
	write_cmos_sensor(0x4E25, 0x80);
	write_cmos_sensor(0x663A, 0x01);
	write_cmos_sensor(0x9311, 0x40);
	write_cmos_sensor(0xA0CD, 0x19);
	write_cmos_sensor(0xA0CE, 0x19);
	write_cmos_sensor(0xA0CF, 0x19);
	/* Integration Time Setting */
	write_cmos_sensor(0x0202, 0x06);
	write_cmos_sensor(0x0203, 0xEA);
	/* Gain Setting */
	write_cmos_sensor(0x0204, 0x00);
	write_cmos_sensor(0x0205, 0x00);
	write_cmos_sensor(0x020E, 0x01);
	write_cmos_sensor(0x020F, 0x00);
	write_cmos_sensor(0x0210, 0x01);
	write_cmos_sensor(0x0211, 0x00);
	write_cmos_sensor(0x0212, 0x01);
	write_cmos_sensor(0x0213, 0x00);
	write_cmos_sensor(0x0214, 0x01);
	write_cmos_sensor(0x0215, 0x00);

}	/* preview_setting */

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("%s.\n", __func__);
	/* Mode Setting */
	write_cmos_sensor(0x0112, 0x0A);
	write_cmos_sensor(0x0113, 0x0A);
	/* Clock Setting */
	write_cmos_sensor(0x0301, 0x06);
	write_cmos_sensor(0x0303, 0x02);
	write_cmos_sensor(0x0305, 0x0C);
	write_cmos_sensor(0x0306, 0x02);
	write_cmos_sensor(0x0307, 0x8A);
	write_cmos_sensor(0x0309, 0x0A);
	write_cmos_sensor(0x030B, 0x01);
	write_cmos_sensor(0x030D, 0x0C);
	write_cmos_sensor(0x030E, 0x02);
	write_cmos_sensor(0x030F, 0x8A);
	write_cmos_sensor(0x0310, 0x00);
	/* Output Size Setting */
	write_cmos_sensor(0x0342, 0x10);
	write_cmos_sensor(0x0343, 0xC8);
	write_cmos_sensor(0x0340, 0x0C);
	write_cmos_sensor(0x0341, 0xE4);
	write_cmos_sensor(0x0344, 0x00);
	write_cmos_sensor(0x0345, 0x00);
	write_cmos_sensor(0x0346, 0x00);
	write_cmos_sensor(0x0347, 0x00);
	write_cmos_sensor(0x0348, 0x0F);
	write_cmos_sensor(0x0349, 0xBF);
	write_cmos_sensor(0x034A, 0x0B);
	write_cmos_sensor(0x034B, 0xC7);
	write_cmos_sensor(0x0385, 0x01);
	write_cmos_sensor(0x0387, 0x01);
	write_cmos_sensor(0x0900, 0x00);
	write_cmos_sensor(0x0901, 0x11);
	write_cmos_sensor(0x300D, 0x00);
	write_cmos_sensor(0x302E, 0x00);
	write_cmos_sensor(0x0401, 0x00);
	write_cmos_sensor(0x0404, 0x00);
	write_cmos_sensor(0x0405, 0x10);
	write_cmos_sensor(0x040C, 0x0F);
	write_cmos_sensor(0x040D, 0xC0);
	write_cmos_sensor(0x040E, 0x0B);
	write_cmos_sensor(0x040F, 0xC8);
	write_cmos_sensor(0x034C, 0x0F);
	write_cmos_sensor(0x034D, 0xC0);
	write_cmos_sensor(0x034E, 0x0B);
	write_cmos_sensor(0x034F, 0xC8);
	/* Other Setting */
	write_cmos_sensor(0x0114, 0x03);
	write_cmos_sensor(0x0408, 0x00);
	write_cmos_sensor(0x0409, 0x00);
	write_cmos_sensor(0x040A, 0x00);
	write_cmos_sensor(0x040B, 0x00);
	write_cmos_sensor(0x0902, 0x00);
	write_cmos_sensor(0x3030, 0x00);
	write_cmos_sensor(0x3031, 0x01);
	write_cmos_sensor(0x3032, 0x00);
	write_cmos_sensor(0x3047, 0x01);
	write_cmos_sensor(0x315D, 0x01);
	write_cmos_sensor(0x3049, 0x01);
	write_cmos_sensor(0x30E6, 0x02);
	write_cmos_sensor(0x30E7, 0x59);
	write_cmos_sensor(0x4E25, 0x80);
	write_cmos_sensor(0x663A, 0x02);
	write_cmos_sensor(0x9311, 0x00);
	write_cmos_sensor(0xA0CD, 0x19);
	write_cmos_sensor(0xA0CE, 0x19);
	write_cmos_sensor(0xA0CF, 0x19);
	/* Integration Time Setting */
	write_cmos_sensor(0x0202, 0x0C);
	write_cmos_sensor(0x0203, 0xDA);
	/* Gain Setting */
	write_cmos_sensor(0x0204, 0x00);
	write_cmos_sensor(0x0205, 0x00);
	write_cmos_sensor(0x020E, 0x01);
	write_cmos_sensor(0x020F, 0x00);
	write_cmos_sensor(0x0210, 0x01);
	write_cmos_sensor(0x0211, 0x00);
	write_cmos_sensor(0x0212, 0x01);
	write_cmos_sensor(0x0213, 0x00);
	write_cmos_sensor(0x0214, 0x01);
	write_cmos_sensor(0x0215, 0x00);
}	/* capture setting */

static void hd_4k_setting(void)
{
	LOG_INF("%s.\n", __func__);
	/*
	 * Full-reso (16:9)@30fps
	 * H: 4032
	 * V: 2256
	 */
	/* Mode Setting */
	write_cmos_sensor(0x0112, 0x0A);
	write_cmos_sensor(0x0113, 0x0A);
	/* Clock Setting */
	write_cmos_sensor(0x0301, 0x06);
	write_cmos_sensor(0x0303, 0x02);
	write_cmos_sensor(0x0305, 0x0C);
	write_cmos_sensor(0x0306, 0x01);
	write_cmos_sensor(0x0307, 0xC2);
	write_cmos_sensor(0x0309, 0x0A);
	write_cmos_sensor(0x030B, 0x01);
	write_cmos_sensor(0x030D, 0x0C);
	write_cmos_sensor(0x030E, 0x01);
	write_cmos_sensor(0x030F, 0xC2);
	write_cmos_sensor(0x0310, 0x00);
	/* Output Size Setting */
	write_cmos_sensor(0x0342, 0x10);
	write_cmos_sensor(0x0343, 0xC8);
	write_cmos_sensor(0x0340, 0x08);
	write_cmos_sensor(0x0341, 0xFE);
	write_cmos_sensor(0x0344, 0x00);
	write_cmos_sensor(0x0345, 0x00);
	write_cmos_sensor(0x0346, 0x01);
	write_cmos_sensor(0x0347, 0x7C);
	write_cmos_sensor(0x0348, 0x0F);
	write_cmos_sensor(0x0349, 0xBF);
	write_cmos_sensor(0x034A, 0x0A);
	write_cmos_sensor(0x034B, 0x4B);
	write_cmos_sensor(0x0385, 0x01);
	write_cmos_sensor(0x0387, 0x01);
	write_cmos_sensor(0x0900, 0x00);
	write_cmos_sensor(0x0901, 0x11);
	write_cmos_sensor(0x300D, 0x00);
	write_cmos_sensor(0x302E, 0x00);
	write_cmos_sensor(0x0401, 0x00);
	write_cmos_sensor(0x0404, 0x00);
	write_cmos_sensor(0x0405, 0x10);
	write_cmos_sensor(0x040C, 0x0F);
	write_cmos_sensor(0x040D, 0xC0);
	write_cmos_sensor(0x040E, 0x08);
	write_cmos_sensor(0x040F, 0xD0);
	write_cmos_sensor(0x034C, 0x0F);
	write_cmos_sensor(0x034D, 0xC0);
	write_cmos_sensor(0x034E, 0x08);
	write_cmos_sensor(0x034F, 0xD0);
	/* Other Setting */
	write_cmos_sensor(0x0114, 0x03);
	write_cmos_sensor(0x0408, 0x00);
	write_cmos_sensor(0x0409, 0x00);
	write_cmos_sensor(0x040A, 0x00);
	write_cmos_sensor(0x040B, 0x00);
	write_cmos_sensor(0x0902, 0x00);
	write_cmos_sensor(0x3030, 0x00);
	write_cmos_sensor(0x3031, 0x01);
	write_cmos_sensor(0x3032, 0x00);
	write_cmos_sensor(0x3047, 0x01);
	write_cmos_sensor(0x3049, 0x01);
	write_cmos_sensor(0x30E6, 0x02);
	write_cmos_sensor(0x30E7, 0x59);
	write_cmos_sensor(0x4E25, 0x80);
	write_cmos_sensor(0x663A, 0x02);
	write_cmos_sensor(0x9311, 0x00);
	write_cmos_sensor(0xA0CD, 0x19);
	write_cmos_sensor(0xA0CE, 0x19);
	write_cmos_sensor(0xA0CF, 0x19);
	/* Integration Time Setting */
	write_cmos_sensor(0x0202, 0x08);
	write_cmos_sensor(0x0203, 0xF4);
	/* Gain Setting */
	write_cmos_sensor(0x0204, 0x00);
	write_cmos_sensor(0x0205, 0x00);
	write_cmos_sensor(0x020E, 0x01);
	write_cmos_sensor(0x020F, 0x00);
	write_cmos_sensor(0x0210, 0x01);
	write_cmos_sensor(0x0211, 0x00);
	write_cmos_sensor(0x0212, 0x01);
	write_cmos_sensor(0x0213, 0x00);
	write_cmos_sensor(0x0214, 0x01);
	write_cmos_sensor(0x0215, 0x00);
}

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("%s.\n", __func__);
	/*
	 * 1/2Binning@30fps
	 * H: 2016
	 * V: 1508
	 */
	/* Mode Setting */
	write_cmos_sensor(0x0112, 0x0A);
	write_cmos_sensor(0x0113, 0x0A);
	/* Clock Setting */
	write_cmos_sensor(0x0301, 0x06);
	write_cmos_sensor(0x0303, 0x02);
	write_cmos_sensor(0x0305, 0x0C);
	write_cmos_sensor(0x0306, 0x02);
	write_cmos_sensor(0x0307, 0x8A);
	write_cmos_sensor(0x0309, 0x0A);
	write_cmos_sensor(0x030B, 0x01);
	write_cmos_sensor(0x030D, 0x0C);
	write_cmos_sensor(0x030E, 0x02);
	write_cmos_sensor(0x030F, 0x8A);
	write_cmos_sensor(0x0310, 0x00);
	/* Output Size Setting */
	write_cmos_sensor(0x0342, 0x10);
	write_cmos_sensor(0x0343, 0xC8);
	/* Output Size Setting */
	write_cmos_sensor(0x0340, 0x0C);
	write_cmos_sensor(0x0341, 0xE4);
	/* Output Size Setting */
	write_cmos_sensor(0x0344, 0x00);
	write_cmos_sensor(0x0345, 0x00);
	write_cmos_sensor(0x0346, 0x00);
	write_cmos_sensor(0x0347, 0x00);
	write_cmos_sensor(0x0348, 0x0F);
	write_cmos_sensor(0x0349, 0xBF);
	write_cmos_sensor(0x034A, 0x0B);
	write_cmos_sensor(0x034B, 0xC7);
	/* Output Size Setting */
	write_cmos_sensor(0x0385, 0x01);
	write_cmos_sensor(0x0387, 0x01);
	write_cmos_sensor(0x0900, 0x00);
	write_cmos_sensor(0x0901, 0x11);
	write_cmos_sensor(0x300D, 0x00);
	write_cmos_sensor(0x302E, 0x00);
	write_cmos_sensor(0x0401, 0x00);
	write_cmos_sensor(0x0404, 0x00);
	write_cmos_sensor(0x0405, 0x10);
	write_cmos_sensor(0x040C, 0x0F);
	write_cmos_sensor(0x040D, 0xC0);
	write_cmos_sensor(0x040E, 0x0B);
	write_cmos_sensor(0x040F, 0xC8);
	write_cmos_sensor(0x034C, 0x0F);
	write_cmos_sensor(0x034D, 0xC0);
	write_cmos_sensor(0x034E, 0x0B);
	write_cmos_sensor(0x034F, 0xC8);
	/* Other Setting */
	write_cmos_sensor(0x0114, 0x03);
	write_cmos_sensor(0x0408, 0x00);
	write_cmos_sensor(0x0409, 0x00);
	write_cmos_sensor(0x040A, 0x00);
	write_cmos_sensor(0x040B, 0x00);
	write_cmos_sensor(0x0902, 0x00);
	write_cmos_sensor(0x3030, 0x00);
	write_cmos_sensor(0x3031, 0x01);
	write_cmos_sensor(0x3032, 0x00);
	write_cmos_sensor(0x3047, 0x01);
	write_cmos_sensor(0x315D, 0x01);
	write_cmos_sensor(0x3049, 0x01);
	write_cmos_sensor(0x30E6, 0x02);
	write_cmos_sensor(0x30E7, 0x59);
	write_cmos_sensor(0x4E25, 0x80);
	write_cmos_sensor(0x663A, 0x02);
	write_cmos_sensor(0x9311, 0x00);
	write_cmos_sensor(0xA0CD, 0x19);
	write_cmos_sensor(0xA0CE, 0x19);
	write_cmos_sensor(0xA0CF, 0x19);
	/* Integration Time Setting */
	write_cmos_sensor(0x0202, 0x0C);
	write_cmos_sensor(0x0203, 0xDA);
	/* Gain Setting */
	write_cmos_sensor(0x0204, 0x00);
	write_cmos_sensor(0x0205, 0x00);
	write_cmos_sensor(0x020E, 0x01);
	write_cmos_sensor(0x020F, 0x00);
	write_cmos_sensor(0x0210, 0x01);
	write_cmos_sensor(0x0211, 0x00);
	write_cmos_sensor(0x0212, 0x01);
	write_cmos_sensor(0x0213, 0x00);
	write_cmos_sensor(0x0214, 0x01);
	write_cmos_sensor(0x0215, 0x00);

}

static void hs_video_setting(void)
{
	LOG_INF("%s.\n", __func__);
	/*
	 * 1296X736@120fps
	 * H: 1296
	 * V: 736
	 */
	/* Mode Setting */
	write_cmos_sensor(0x0112, 0x0A);
	write_cmos_sensor(0x0113, 0x0A);
	/* Clock Setting */
	write_cmos_sensor(0x0301, 0x04);
	write_cmos_sensor(0x0303, 0x02);
	write_cmos_sensor(0x0305, 0x0C);
	write_cmos_sensor(0x0306, 0x02);
	write_cmos_sensor(0x0307, 0x58);
	write_cmos_sensor(0x0309, 0x0A);
	write_cmos_sensor(0x030B, 0x01);
	write_cmos_sensor(0x030D, 0x0C);
	write_cmos_sensor(0x030E, 0x02);
	write_cmos_sensor(0x030F, 0x58);
	write_cmos_sensor(0x0310, 0x00);
	/* Output Size Setting */
	write_cmos_sensor(0x0342, 0x10);
	write_cmos_sensor(0x0343, 0xC8);
	/* Output Size Setting */
	write_cmos_sensor(0x0340, 0x04);
	write_cmos_sensor(0x0341, 0x78);
	/* Output Size Setting */
	write_cmos_sensor(0x0344, 0x02);
	write_cmos_sensor(0x0345, 0xD0);
	write_cmos_sensor(0x0346, 0x03);
	write_cmos_sensor(0x0347, 0x04);
	write_cmos_sensor(0x0348, 0x0C);
	write_cmos_sensor(0x0349, 0xEF);
	write_cmos_sensor(0x034A, 0x08);
	write_cmos_sensor(0x034B, 0xC3);
	write_cmos_sensor(0x0385, 0x01);
	write_cmos_sensor(0x0387, 0x01);
	write_cmos_sensor(0x0900, 0x01);
	write_cmos_sensor(0x0901, 0x12);
	write_cmos_sensor(0x300D, 0x00);
	write_cmos_sensor(0x302E, 0x01);
	write_cmos_sensor(0x0401, 0x01);
	write_cmos_sensor(0x0404, 0x00);
	write_cmos_sensor(0x0405, 0x20);
	write_cmos_sensor(0x040C, 0x0A);
	write_cmos_sensor(0x040D, 0x20);
	write_cmos_sensor(0x040E, 0x02);
	write_cmos_sensor(0x040F, 0xE0);
	write_cmos_sensor(0x034C, 0x05);
	write_cmos_sensor(0x034D, 0x10);
	write_cmos_sensor(0x034E, 0x02);
	write_cmos_sensor(0x034F, 0xE0);
	/* Other Setting */
	write_cmos_sensor(0x0114, 0x03);
	write_cmos_sensor(0x0408, 0x00);
	write_cmos_sensor(0x0409, 0x00);
	write_cmos_sensor(0x040A, 0x00);
	write_cmos_sensor(0x040B, 0x00);
	write_cmos_sensor(0x0902, 0x00);
	write_cmos_sensor(0x3030, 0x00);
	write_cmos_sensor(0x3031, 0x01);
	write_cmos_sensor(0x3032, 0x00);
	write_cmos_sensor(0x3047, 0x01);
	write_cmos_sensor(0x3049, 0x01);
	write_cmos_sensor(0x30E6, 0x00);
	write_cmos_sensor(0x30E7, 0x00);
	write_cmos_sensor(0x4E25, 0x80);
	write_cmos_sensor(0x663A, 0x01);
	write_cmos_sensor(0x9311, 0x64);
	write_cmos_sensor(0xA0CD, 0x19);
	write_cmos_sensor(0xA0CE, 0x19);
	write_cmos_sensor(0xA0CF, 0x19);
	/* Integration Time Setting */
	write_cmos_sensor(0x0202, 0x04);
	write_cmos_sensor(0x0203, 0x7E);
	/* Gain Setting */
	write_cmos_sensor(0x0204, 0x00);
	write_cmos_sensor(0x0205, 0x00);
	write_cmos_sensor(0x020E, 0x01);
	write_cmos_sensor(0x020F, 0x00);
	write_cmos_sensor(0x0210, 0x01);
	write_cmos_sensor(0x0211, 0x00);
	write_cmos_sensor(0x0212, 0x01);
	write_cmos_sensor(0x0213, 0x00);
	write_cmos_sensor(0x0214, 0x01);
	write_cmos_sensor(0x0215, 0x00);
}

static void slim_video_setting(void)
{
	LOG_INF("%s.\n", __func__);
	/*
	 * 1296X736@30fps
	 * H: 1296
	 * V: 736
	 */
	/* Mode Setting */
	write_cmos_sensor(0x0112, 0x0A);
	write_cmos_sensor(0x0113, 0x0A);
	/* Clock Setting */
	write_cmos_sensor(0x0301, 0x04);
	write_cmos_sensor(0x0303, 0x02);
	write_cmos_sensor(0x0305, 0x0C);
	write_cmos_sensor(0x0306, 0x02);
	write_cmos_sensor(0x0307, 0x58);
	write_cmos_sensor(0x0309, 0x0A);
	write_cmos_sensor(0x030B, 0x01);
	write_cmos_sensor(0x030D, 0x0C);
	write_cmos_sensor(0x030E, 0x02);
	write_cmos_sensor(0x030F, 0x58);
	write_cmos_sensor(0x0310, 0x00);
	/* Output Size Setting */
	write_cmos_sensor(0x0342, 0x10);
	write_cmos_sensor(0x0343, 0xC8);
	/* Output Size Setting */
	write_cmos_sensor(0x0340, 0x12);
	write_cmos_sensor(0x0341, 0x28);
	/* Output Size Setting */
	write_cmos_sensor(0x0344, 0x02);
	write_cmos_sensor(0x0345, 0xD0);
	write_cmos_sensor(0x0346, 0x03);
	write_cmos_sensor(0x0347, 0x04);
	write_cmos_sensor(0x0348, 0x0C);
	write_cmos_sensor(0x0349, 0xEF);
	write_cmos_sensor(0x034A, 0x08);
	write_cmos_sensor(0x034B, 0xC3);
	write_cmos_sensor(0x0385, 0x01);
	write_cmos_sensor(0x0387, 0x01);
	write_cmos_sensor(0x0900, 0x01);
	write_cmos_sensor(0x0901, 0x12);
	write_cmos_sensor(0x300D, 0x00);
	write_cmos_sensor(0x302E, 0x01);
	write_cmos_sensor(0x0401, 0x01);
	write_cmos_sensor(0x0404, 0x00);
	write_cmos_sensor(0x0405, 0x20);
	write_cmos_sensor(0x040C, 0x0A);
	write_cmos_sensor(0x040D, 0x20);
	write_cmos_sensor(0x040E, 0x02);
	write_cmos_sensor(0x040F, 0xE0);
	write_cmos_sensor(0x034C, 0x05);
	write_cmos_sensor(0x034D, 0x10);
	write_cmos_sensor(0x034E, 0x02);
	write_cmos_sensor(0x034F, 0xE0);
	/* Other Setting */
	write_cmos_sensor(0x0114, 0x03);
	write_cmos_sensor(0x0408, 0x00);
	write_cmos_sensor(0x0409, 0x00);
	write_cmos_sensor(0x040A, 0x00);
	write_cmos_sensor(0x040B, 0x00);
	write_cmos_sensor(0x0902, 0x00);
	write_cmos_sensor(0x3030, 0x00);
	write_cmos_sensor(0x3031, 0x01);
	write_cmos_sensor(0x3032, 0x00);
	write_cmos_sensor(0x3047, 0x01);
	write_cmos_sensor(0x3049, 0x01);
	write_cmos_sensor(0x30E6, 0x00);
	write_cmos_sensor(0x30E7, 0x00);
	write_cmos_sensor(0x4E25, 0x80);
	write_cmos_sensor(0x663A, 0x01);
	write_cmos_sensor(0x9311, 0x64);
	write_cmos_sensor(0xA0CD, 0x19);
	write_cmos_sensor(0xA0CE, 0x19);
	write_cmos_sensor(0xA0CF, 0x19);
	/* Integration Time Setting */
	write_cmos_sensor(0x0202, 0x12);
	write_cmos_sensor(0x0203, 0x1E);
	/* Gain Setting */
	write_cmos_sensor(0x0204, 0x00);
	write_cmos_sensor(0x0205, 0x00);
	write_cmos_sensor(0x020E, 0x01);
	write_cmos_sensor(0x020F, 0x00);
	write_cmos_sensor(0x0210, 0x01);
	write_cmos_sensor(0x0211, 0x00);
	write_cmos_sensor(0x0212, 0x01);
	write_cmos_sensor(0x0213, 0x00);
	write_cmos_sensor(0x0214, 0x01);
	write_cmos_sensor(0x0215, 0x00);
}

static void custom1_mode(void)
{
	LOG_INF("%s.\n", __func__);
	/* Mode Setting */
	write_cmos_sensor(0x0112, 0x0A);
	write_cmos_sensor(0x0113, 0x0A);
	/* Clock Setting */
	write_cmos_sensor(0x0301, 0x06);
	write_cmos_sensor(0x0303, 0x02);
	write_cmos_sensor(0x0305, 0x0C);
	write_cmos_sensor(0x0306, 0x02);
	write_cmos_sensor(0x0307, 0x8A);
	write_cmos_sensor(0x0309, 0x0A);
	write_cmos_sensor(0x030B, 0x01);
	write_cmos_sensor(0x030D, 0x0C);
	write_cmos_sensor(0x030E, 0x02);
	write_cmos_sensor(0x030F, 0x8A);
	write_cmos_sensor(0x0310, 0x00);
	/* Output Size Setting */
	write_cmos_sensor(0x0342, 0x10);
	write_cmos_sensor(0x0343, 0xC8);
	write_cmos_sensor(0x0340, 0x0C);
	write_cmos_sensor(0x0341, 0xE4);
	write_cmos_sensor(0x0344, 0x00);
	write_cmos_sensor(0x0345, 0x00);
	write_cmos_sensor(0x0346, 0x00);
	write_cmos_sensor(0x0347, 0x00);
	write_cmos_sensor(0x0348, 0x0F);
	write_cmos_sensor(0x0349, 0xBF);
	write_cmos_sensor(0x034A, 0x0B);
	write_cmos_sensor(0x034B, 0xC7);
	write_cmos_sensor(0x0385, 0x01);
	write_cmos_sensor(0x0387, 0x01);
	write_cmos_sensor(0x0900, 0x00);
	write_cmos_sensor(0x0901, 0x11);
	write_cmos_sensor(0x300D, 0x00);
	write_cmos_sensor(0x302E, 0x00);
	write_cmos_sensor(0x0401, 0x00);
	write_cmos_sensor(0x0404, 0x00);
	write_cmos_sensor(0x0405, 0x10);
	write_cmos_sensor(0x040C, 0x0F);
	write_cmos_sensor(0x040D, 0xC0);
	write_cmos_sensor(0x040E, 0x0B);
	write_cmos_sensor(0x040F, 0xC8);
	write_cmos_sensor(0x034C, 0x0F);
	write_cmos_sensor(0x034D, 0xC0);
	write_cmos_sensor(0x034E, 0x0B);
	write_cmos_sensor(0x034F, 0xC8);
	/* Other Setting */
	write_cmos_sensor(0x0114, 0x03);
	write_cmos_sensor(0x0408, 0x00);
	write_cmos_sensor(0x0409, 0x00);
	write_cmos_sensor(0x040A, 0x00);
	write_cmos_sensor(0x040B, 0x00);
	write_cmos_sensor(0x0902, 0x00);
	write_cmos_sensor(0x3030, 0x00);
	write_cmos_sensor(0x3031, 0x01);
	write_cmos_sensor(0x3032, 0x00);
	write_cmos_sensor(0x30E6, 0x02);
	write_cmos_sensor(0x30E7, 0x59);
	write_cmos_sensor(0x4E25, 0x80);
	write_cmos_sensor(0x663A, 0x02);
	write_cmos_sensor(0x9311, 0x00);
	write_cmos_sensor(0xA0CD, 0x19);
	write_cmos_sensor(0xA0CE, 0x19);
	write_cmos_sensor(0xA0CF, 0x19);
	/* Integration Time Setting */
	write_cmos_sensor(0x0202, 0x0C);
	write_cmos_sensor(0x0203, 0xDA);
	/* Gain Setting */
	write_cmos_sensor(0x0204, 0x00);
	write_cmos_sensor(0x0205, 0x00);
	write_cmos_sensor(0x020E, 0x01);
	write_cmos_sensor(0x020F, 0x00);
	write_cmos_sensor(0x0210, 0x01);
	write_cmos_sensor(0x0211, 0x00);
	write_cmos_sensor(0x0212, 0x01);
	write_cmos_sensor(0x0213, 0x00);
	write_cmos_sensor(0x0214, 0x01);
	write_cmos_sensor(0x0215, 0x00);
	/*pdaf setting*/
	write_cmos_sensor(0x3047, 0x01);/*PD_CAL_ENALBE*/
	write_cmos_sensor(0x3049, 0x01);/*PD_OUT_EN=1*/
#ifdef PDAF_FIX_WINDOW /* fix window */
	write_cmos_sensor(0x315D, 0x01);/*Area mode*/

	write_cmos_sensor(0x3100, 0x00);
	write_cmos_sensor(0x3101, 0x40);
	write_cmos_sensor(0x3102, 0x00);
	write_cmos_sensor(0x3103, 0x10);
	write_cmos_sensor(0x3104, 0x01);
	write_cmos_sensor(0x3105, 0xE8);
	write_cmos_sensor(0x3106, 0x01);
	write_cmos_sensor(0x3107, 0xF0);
#else/* flexible window */
	write_cmos_sensor(0x315D, 0x02);/*Area mode*/
	write_cmos_sensor(0x315E, 0x01);/*Area0*/
#endif
}


static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor(0x0016) << 8) | read_cmos_sensor(0x0017));
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
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if  (*sensor_id == imgsensor_info.sensor_id) {
			LOG_INF("Get imx386 mono! id=0x%x, sensor id: 0x%x\n"
					, imgsensor.i2c_write_id, *sensor_id);
			*sensor_id = IMX386_MONO_SENSOR_ID;
			return ERROR_NONE;

			}
			LOG_INF("Read sensor fail, addr: 0x%x, sensorid:0x%x\n"
					, imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 1;
	}
	if  (*sensor_id != imgsensor_info.sensor_id) {
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
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

	LOG_1;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n"
					, imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read id fail, id: 0x%x, sensor id: 0x%x\n",
				imgsensor.i2c_write_id, sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 1;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail sequence write in  */
	sensor_init();

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = KAL_FALSE;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}   /*  open  */



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
static kal_uint32 close(void)
{
	LOG_INF("%s.\n", __func__);

	/*No Need to implement this function*/

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
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();

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
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	mode_change = 1;
	if (imgsensor.current_fps == imgsensor_info.cap.max_framerate)
		LOG_INF("use cap30FPS's setting: %d fps!\n",
		imgsensor.current_fps / 10);
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);

	return ERROR_NONE;
}	/* capture() */

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_mode();

	return ERROR_NONE;
}

static kal_uint32 hd_4k_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hd_4k_setting();
	return ERROR_NONE;
}	/*	hd_4k_video   */

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);

	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	return ERROR_NONE;
}

static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
{
	LOG_INF("%s.\n", __func__);
	sensor_resolution->SensorFullWidth =
		imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight =
		imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth =
		imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight =
		imgsensor_info.normal_video.grabwindow_height;

	sensor_resolution->SensorCustom1Width =
		imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height =
		imgsensor_info.custom1.grabwindow_height;

	sensor_resolution->SensorCustom2Width =
		imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height =
		imgsensor_info.custom2.grabwindow_height;

	sensor_resolution->SensorCustom3Width =
		imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height =
		imgsensor_info.custom3.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth =
		imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight =
		imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth =
		imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
		imgsensor_info.slim_video.grabwindow_height;

	return ERROR_NONE;
}	/*	get_resolution	*/

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_INFO_STRUCT *sensor_info,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;
	sensor_info->SensorResetActiveHigh = FALSE;
	sensor_info->SensorResetDelayCount = 5;

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
		imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0;
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->FrameTimeDelayFrame =
		imgsensor_info.frame_time_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->TEMPERATURE_SUPPORT = imgsensor_info.temperature_support;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  /* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;
	sensor_info->PDAF_Support = 2;
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.cap.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		sensor_info->SensorGrabStartX =
			imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.normal_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX =
			imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.hs_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		sensor_info->SensorGrabStartX =
			imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.slim_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
		break;
	default:
		sensor_info->SensorGrabStartX =
			imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		custom1(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		hd_4k_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		hd_4k_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		slim_video(image_window, sensor_config_data);
		break;
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}

static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
		/* Dynamic frame rate */
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) /* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else /* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk / framerate * 10
			/ imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
		imgsensor_info.normal_video.framelength) ?
			(frame_length -
			imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.custom1.pclk / framerate * 10
			/ imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.custom1.framelength) ?
			(frame_length -
			imgsensor_info.custom1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.custom2.pclk / framerate * 10
			/ imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.custom2.framelength) ?
			(frame_length - imgsensor_info.custom2.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom2.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.custom3.pclk / framerate * 10
			/ imgsensor_info.custom3.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.custom3.framelength) ?
			(frame_length - imgsensor_info.custom3.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom3.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		frame_length = imgsensor_info.cap.pclk / framerate * 10
			/ imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.cap.framelength) ?
			(frame_length - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10
			/ imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.hs_video.framelength) ?
			(frame_length -
			imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10
			/ imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.slim_video.framelength) ?
			(frame_length -
			imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		break;
	default:
		frame_length = imgsensor_info.pre.pclk / framerate * 10
			/ imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.pre.framelength) ?
			(frame_length -
			imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		LOG_INF("error scenario_id = %d\n", scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		*framerate = imgsensor_info.custom3.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("%s, enable: %d\n", __func__, enable);

	write_cmos_sensor(0x0601, enable ? 0x02 : 0x00);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	pr_info("streaming_enable(0=Sw tandby,1=streaming): %d\n", enable);
	if (enable) {
		write_cmos_sensor(0x0100, 0X01);
	if (imgsensor.current_scenario_id == MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO)
		mdelay(32);
	} else {
		write_cmos_sensor(0x0100, 0x00);
	}
	return ERROR_NONE;
}

static kal_uint32 get_sensor_temperature(void)
{
	UINT32 temperature;

	temperature = read_cmos_sensor(0x013a);
	LOG_INF("get_temperature(%d)\n", temperature);
	return temperature;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
		 UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;

	switch (feature_id) {
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.pclk;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.pclk;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom2.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom3.pclk;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.pclk;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.cap.framelength << 16)
				+ imgsensor_info.cap.linelength;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.normal_video.framelength << 16)
				+ imgsensor_info.normal_video.linelength;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.hs_video.framelength << 16)
				+ imgsensor_info.hs_video.linelength;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.slim_video.framelength << 16)
				+ imgsensor_info.slim_video.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom1.framelength << 16)
				+ imgsensor_info.custom1.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom2.framelength << 16)
				+ imgsensor_info.custom2.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom3.framelength << 16)
				+ imgsensor_info.custom3.linelength;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT32)*feature_data,
			(UINT32)*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL)*feature_data_16,
			*(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
			*(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", (UINT32)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (BOOL)*feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32)*feature_data);

		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)
			(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[1],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[3],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[4],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[5],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[6],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[7],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data + 1),
			(UINT16)*(feature_data + 2));
		ihdr_write_shutter_gain((UINT16)*feature_data,
			(UINT16)*(feature_data+1),
			(UINT16)*(feature_data + 2));
		break;
	/******************** PDAF START >>> *********/
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		LOG_INF("GET_SENSOR_PDAF_CAPACITY scenarioId:%llu\n",
			*feature_data);
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			(UINT32)*feature_data);
		PDAFinfo = (struct SET_PD_BLOCK_INFO_T *)
			(uintptr_t)(*(feature_data+1));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM3:
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_SET_PDAF:
		LOG_INF("PDAF mode :%d\n", *feature_data_16);
		/* imgsensor.pdaf_mode= *feature_data_16; */
		break;

	case SENSOR_FEATURE_GET_PDAF_DATA:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n",
			(UINT16)*feature_data);
		pvcinfo = (struct SENSOR_VC_INFO_STRUCT *)
			(uintptr_t)(*(feature_data+1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)pvcinfo,
				(void *)&SENSOR_VC_INFO[1],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)pvcinfo,
				(void *)&SENSOR_VC_INFO[2],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)pvcinfo,
				(void *)&SENSOR_VC_INFO[0],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_32 = get_sensor_temperature();
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
		LOG_INF("GET_PDAF_REG_SETTING %d", (*feature_para_len));
		imx386_get_pdaf_reg_setting(
			(*feature_para_len)/sizeof(UINT32),
			feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
		LOG_INF("SET_PDAF_REG_SETTING %d", (*feature_para_len));
		imx386_set_pdaf_reg_setting((*feature_para_len)/sizeof(UINT32),
			feature_data_16);
		break;
	/******************** PDAF END   <<< *********/
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 IMX386_MIPI_MONO_SensorInit(
			struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*	  IMX386_MIPI_MONO_SensorInit	*/
