/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     ov5645_mipi_raw_Sensor.c
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     Source code of Sensor driver
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
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "ov5645_mipi_raw_Sensor.h"

/*****************Modify Following Strings for Debug**********************/
#define PFX "OV5645_camera_sensor"
#define LOG_1 LOG_INF("OV5645,MIPI 1LANE\n")
#define LOG_2 LOG_INF\
("preview/capture/vedio/slim_video/hs_video:\280*960 560Mbps/lane 30fps\n")
/************************   Modify end    ********************************/

#define LOG_INF(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

#define SETTLE_DELAY 100
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = OV5645MIPI_SENSOR_ID,	/* 0x5645 */

	.checksum_value = 0xf7375923, /* checksum value for Camera Auto Test */

	.pre = {
		/* record different mode's pclk */
		.pclk = 55969920,
		/* record different mode's linelength */
		.linelength = 1896,
		/* record different mode's framelength */
		.framelength = 984,
		/* record different mode's startx of grabwindow */
		.startx = 0,
		/* record different mode's starty of grabwindow */
		.starty = 0,
		/* record different mode's width of grabwindow */
		.grabwindow_width = 1296,
		/* record different mode's height of grabwindow */
		.grabwindow_height = 972,
		/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount */
		/* unit , ns, 20 */
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,
		/* following for GetDefaultFramerateByScenario() */
		.max_framerate = 300,
		},
	.cap = {
		/* record different mode's pclk */
		.pclk = 83954880,
		/* record different mode's linelength */
		.linelength = 2844,
		/* record different mode's framelength */
		.framelength = 1968,
		/* record different mode's startx of grabwindow */
		.startx = 0,
		/* record different mode's starty of grabwindow */
		.starty = 0,
		/* record different mode's width of grabwindow */
		.grabwindow_width = 2304,
		/* record different mode's height of grabwindow */
		.grabwindow_height = 1944,
		/*following for MIPIDataLowPwr2HighSpeedSettleDelayCount */
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,	/* unit , ns */
		/*following for GetDefaultFramerateByScenario() */
		.max_framerate = 150,
		},
	.cap1 = {
		/* record different mode's pclk */
		.pclk = 83954880,
		/* record different mode's linelength */
		.linelength = 2844,
		/* record different mode's framelength */
		.framelength = 1968,
		/* record different mode's startx of grabwindow */
		.startx = 0,
		/* record different mode's starty of grabwindow */
		.starty = 0,
		/* record different mode's width of grabwindow */
		.grabwindow_width = 2304,
		/* record different mode's height of grabwindow */
		.grabwindow_height = 1944,
		/*following for MIPIDataLowPwr2HighSpeedSettleDelayCount */
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,	/* unit , ns */
		/*following for GetDefaultFramerateByScenario() */
		.max_framerate = 150,
		},
	.normal_video = {
		/* record different mode's pclk */
		.pclk = 84000000,
		/* record different mode's linelength */
		.linelength = 2500,
		/* record different mode's framelength */
		.framelength = 1120,
		/* record different mode's startx of grabwindow */
		.startx = 0,
		/* record different mode's starty of grabwindow */
		.starty = 0,
		/* record different mode's width of grabwindow */
		.grabwindow_width = 1920,
		/* record different mode's height of grabwindow */
		.grabwindow_height = 1080,
		/*following for MIPIDataLowPwr2HighSpeedSettleDelayCount */
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,	/* unit , ns */
		/*following for GetDefaultFramerateByScenario() */
		.max_framerate = 300,
		},
	.hs_video = {
		/* record different mode's pclk */
		.pclk = 84004800,
		/* record different mode's linelength */
		.linelength = 1892,
		/* record different mode's framelength */
		.framelength = 740,
		/* record different mode's startx of grabwindow */
		.startx = 0,
		/* record different mode's starty of grabwindow */
		.starty = 0,
		/* record different mode's width of grabwindow */
		.grabwindow_width = 1280,
		/* record different mode's height of grabwindow */
		.grabwindow_height = 720,
		/*following for MIPIDataLowPwr2HighSpeedSettleDelayCount */
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,	/* unit , ns */
		/*following for GetDefaultFramerateByScenario() */
		.max_framerate = 600,
		},
	.slim_video = {
		/* record different mode's pclk */
		.pclk = 84004800,
		/* record different mode's linelength */
		.linelength = 1892,
		/* record different mode's framelength */
		.framelength = 1480,
		/* record different mode's startx of grabwindow */
		.startx = 0,
		/* record different mode's starty of grabwindow */
		.starty = 0,
		/* record different mode's width of grabwindow */
		.grabwindow_width = 1280,
		/* record different mode's height of grabwindow */
		.grabwindow_height = 720,
		/*following for MIPIDataLowPwr2HighSpeedSettleDelayCount */
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,	/* unit , ns */
		/*following for GetDefaultFramerateByScenario() */
		.max_framerate = 300,
		},
	/* check  //sensor framelength & shutter margin */
	.margin = 4,
	/* min shutter */
	.min_shutter = 2,
	/* max framelength by sensor register's limitation */
	.max_frame_length = 0x7fff,
	// shutter delay frame for AE cycle,
	// 2 frame with ispGain_delay-shut_delay=2-0=2
	.ae_shut_delay_frame = 0,
	// sensor gain delay frame for AE cycle,
	// 2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	.ae_sensor_gain_delay_frame = 0,
	/* isp gain delay frame for AE cycle */
	.ae_ispGain_delay_frame = 2,
	/* 1, support; 0,not support */
	.ihdr_support = 0,
	/* 1,le first ; 0, se first */
	.ihdr_le_firstline = 0,
	/* support sensor mode num */
	.sensor_mode_num = 5,

	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,

	/* mclk driving current */
	.isp_driving_current = ISP_DRIVING_8MA,
	/* sensor_interface_type */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.mipi_settle_delay_mode = 1,
	/* sensor output first pixel color */
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mclk = 24,
	/* mipi lane num 1lane check */
	.mipi_lane_num = SENSOR_MIPI_2_LANE,
	.i2c_addr_table = {0x78, 0xff},	/* add 0x78 */
	.i2c_speed = 200,
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	/* IMGSENSOR_MODE enum value,record current sensor mode,such as: */
	/* INIT, Preview, Capture, Video,High Speed Video, Slim Video */
	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.current_fps = 300,
	// auto flicker enable: KAL_FALSE for disable auto flicker,
	// KAL_TRUE for enable auto flicker
	.autoflicker_en = KAL_FALSE,
	// test pattern mode or not. KAL_FALSE for in test pattern mode,
	// KAL_TRUE for normal output
	.test_pattern = KAL_FALSE,
	/* current scenario id */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	/* sensor need support LE, SE with HDR feature */
	.ihdr_en = 0,
	/* record current sensor's i2c write id */
	.i2c_write_id = 0x78,
};

/* Sensor output window information */
static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{2624, 1956, 0,    2, 2624, 1952, 1312, 976,
			8, 2, 1296, 972,  0, 0, 1296,  972},/* Preview */
	{2624, 1956, 0,    0, 2624, 1956, 2624, 1956,
			160, 6, 2304, 1944, 0, 0, 2304, 1944},/* capture */
	{2624, 1956, 336, 432, 1952, 1092, 1952, 1092,
			16, 6,   1920, 1080,   0, 0, 1920, 1080},/* video */
	{2624, 1956, 0,   250, 2624, 1456, 1312, 728,
			16, 4, 1280, 720, 0, 0, 1280, 720},/* hs video */
	{2624, 1956, 0,   250, 2624, 1456, 1312, 728,
			16, 4, 1280, 720, 0, 0, 1280, 720}	/* slim video */
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)	/* check */
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)	/* check */
{
	char pusendcmd[4] = { (char)(addr >> 8),
			(char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n",
			imgsensor.dummy_line, imgsensor.dummy_pixel);
	//  you can set dummy by imgsensor.dummy_line
	//	and imgsensor.dummy_pixel,
	//  or you can set dummy by imgsensor.frame_length
	//	and imgsensor.line_length

	/* check */
	write_cmos_sensor(0x380c, imgsensor.line_length >> 8);
	write_cmos_sensor(0x380d, imgsensor.line_length & 0xFF);
	write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
}		/*    set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	//  modify read id method,must write page
	//	first and then read the id

	return ((read_cmos_sensor(0x300a) << 8)
			| read_cmos_sensor(0x300b));	/* check 5645 */
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d, min framelength should enable = %d\n",
		framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;

	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length =
	    (frame_length > imgsensor.min_frame_length) ? frame_length :
		imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length -
		imgsensor.min_frame_length;

	LOG_INF("line = %d, frame = %d, dummy line:%d, max_framelen:%d\n",
		imgsensor.line_length, imgsensor.frame_length,
		imgsensor.dummy_line, imgsensor_info.max_frame_length);
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length -
			imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);

	set_dummy();
}				/* set_max_framerate  */

#if 1
static void write_shutter(kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;

	// P1:0x03_0x04 will increase VBLANK to get
	//	exposure larger than frame exposure
	// AE doesn't update sensor gain at capture mode,
	//	thus extra exposure lines must be updated here.

	/* OV Recommend Solution */
	// if shutter bigger than frame_length,
	//	should extend frame length first
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;
	shutter =
	    (shutter >
	     (imgsensor_info.max_frame_length -
	      imgsensor_info.margin)) ? (imgsensor_info.max_frame_length -
					 imgsensor_info.margin) : shutter;

#if 1
	/* Framelength should be an even number */
	shutter = (shutter >> 1) << 1;
	imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;
#endif

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 /
			imgsensor.frame_length;
		LOG_INF("realtime_fps =%d\n", realtime_fps);
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(0x380e,
			(imgsensor.frame_length >> 8) & 0xFF);
			write_cmos_sensor(0x380f,
			imgsensor.frame_length & 0xFF);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x380e,
		(imgsensor.frame_length >> 8) & 0xFF);
		write_cmos_sensor(0x380f,
		imgsensor.frame_length & 0xFF);
	}

	/* Update Shutter */
	write_cmos_sensor(0x3502, ((shutter << 4) & 0xFF));
	write_cmos_sensor(0x3501, (shutter >> 4) & 0xFF);
	write_cmos_sensor(0x3500, (shutter >> 12) & 0x0F);	/* check */

	LOG_INF("shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);
}

/*************************************************************************
 * FUNCTION
 *    set_shutter
 *
 * DESCRIPTION
 *    This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *    iShutter : exposured lines
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/

static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;

	LOG_INF("[%s] shutter = %d\n", __func__, shutter);
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}


static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 iReg = 0x0000;
#if 0
	/* platform 1xgain = 64, sensor driver 1*gain = 0x10 */
	iReg = gain * 16 / BASEGAIN;	/* check 1~~15 gain */

	if (iReg < 0x10)	/* sensor 1xGain */
		iReg = 0x10;
	if (iReg > 0xf8)	/* sensor 15.5xGain */
		iReg = 0xf8;
#else
	iReg = gain * 128 / BASEGAIN;
	if (iReg < 0x80)
		iReg = 0x80;
	if (iReg > 0x7c0)
		iReg = 0x7c0;
#endif
	return iReg;
}
#endif

/*************************************************************************
 * FUNCTION
 *    set_gain
 *
 * DESCRIPTION
 *    This function is to set global gain to sensor.
 *
 * PARAMETERS
 *    iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *    the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
#if 1
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

	LOG_INF("[%s] gain = %d\n", __func__, gain);
	reg_gain = gain2reg(gain);
	LOG_INF("[%s] reg_gain = %d\n", __func__, reg_gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor(0x350a, (reg_gain >> 8) & 0xff);	/* check */
	write_cmos_sensor(0x350b, reg_gain & 0xff);

	return gain;
}				/*set_gain  */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se,
		kal_uint16 gain)
{
	/* not support HDR */
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
}
#endif

/*************************************************************************
 * FUNCTION
 *    night_mode
 *
 * DESCRIPTION
 *    This function night mode of sensor.
 *
 * PARAMETERS
 *    bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void night_mode(kal_bool enable)
{
	/*No Need to implement this function */
}				/*    night_mode    */

static void sensor_init(void)
{
	LOG_INF("E\n");
	write_cmos_sensor(0x3103, 0x11);
	write_cmos_sensor(0x3008, 0x82);
	mDELAY(2);
	write_cmos_sensor(0x3008, 0x42); /* enter software standby */

	write_cmos_sensor(0x3103, 0x03);
	write_cmos_sensor(0x3503, 0x07);
	write_cmos_sensor(0x3002, 0x1c);
	write_cmos_sensor(0x3006, 0xc3);
	write_cmos_sensor(0x300e, 0x45);
	write_cmos_sensor(0x3017, 0x40);
	write_cmos_sensor(0x3018, 0x00);
	write_cmos_sensor(0x302e, 0x0b);
	write_cmos_sensor(0x3037, 0x13);
	write_cmos_sensor(0x3108, 0x01);
	write_cmos_sensor(0x3611, 0x06);
	write_cmos_sensor(0x3614, 0x50);
	write_cmos_sensor(0x3034, 0x1a);
	write_cmos_sensor(0x3500, 0x00);
	write_cmos_sensor(0x350a, 0x00);
	write_cmos_sensor(0x3620, 0x33);
	write_cmos_sensor(0x3621, 0xe0);
	write_cmos_sensor(0x3622, 0x01);
	write_cmos_sensor(0x3630, 0x2d);
	write_cmos_sensor(0x3631, 0x00);
	write_cmos_sensor(0x3632, 0x32);
	write_cmos_sensor(0x3633, 0x52);
	write_cmos_sensor(0x3634, 0x70);
	write_cmos_sensor(0x3635, 0x13);
	write_cmos_sensor(0x3636, 0x03);
	write_cmos_sensor(0x3702, 0x6e);
	write_cmos_sensor(0x3703, 0x52);
	write_cmos_sensor(0x3704, 0xa0);
	write_cmos_sensor(0x3705, 0x33);
	write_cmos_sensor(0x3709, 0x12);
	write_cmos_sensor(0x370b, 0x61);
	write_cmos_sensor(0x370f, 0x10);
	write_cmos_sensor(0x3715, 0x08);
	write_cmos_sensor(0x3717, 0x01);
	write_cmos_sensor(0x371b, 0x20);
	write_cmos_sensor(0x3731, 0x22);
	write_cmos_sensor(0x3739, 0x70);
	write_cmos_sensor(0x3901, 0x0a);
	write_cmos_sensor(0x3905, 0x02);
	write_cmos_sensor(0x3906, 0x10);
	write_cmos_sensor(0x3719, 0x86);
	write_cmos_sensor(0x3810, 0x00);
	write_cmos_sensor(0x3812, 0x00);
	write_cmos_sensor(0x3824, 0x01);
	write_cmos_sensor(0x3826, 0x03);
	write_cmos_sensor(0x3828, 0x08);
	write_cmos_sensor(0x3a08, 0x01);
	write_cmos_sensor(0x3a18, 0x00);
	write_cmos_sensor(0x3a19, 0xf8);
	write_cmos_sensor(0x3c04, 0x28);
	write_cmos_sensor(0x3c05, 0x98);
	write_cmos_sensor(0x3c07, 0x07);
	write_cmos_sensor(0x3c09, 0xc2);
	write_cmos_sensor(0x3c0a, 0x9c);
	write_cmos_sensor(0x3c0b, 0x40);
	write_cmos_sensor(0x3c01, 0x34);
	write_cmos_sensor(0x4001, 0x02);
	write_cmos_sensor(0x4005, 0x18);
	write_cmos_sensor(0x4050, 0x6e);
	write_cmos_sensor(0x4051, 0x8f);
	write_cmos_sensor(0x4300, 0xf8);
	write_cmos_sensor(0x4520, 0xb0);
	write_cmos_sensor(0x460b, 0x37);
	write_cmos_sensor(0x460c, 0x20);
	write_cmos_sensor(0x4818, 0x01);
	write_cmos_sensor(0x481d, 0xf0);
	write_cmos_sensor(0x481f, 0x50);
	write_cmos_sensor(0x4823, 0x70);
	write_cmos_sensor(0x4831, 0x14);
	write_cmos_sensor(0x5000, 0x06);
	write_cmos_sensor(0x5001, 0x00);
	write_cmos_sensor(0x501d, 0x00);
	write_cmos_sensor(0x501f, 0x03);
	write_cmos_sensor(0x503d, 0x00);
	write_cmos_sensor(0x505c, 0x30);
	write_cmos_sensor(0x5181, 0x59);
	write_cmos_sensor(0x5183, 0x00);
	write_cmos_sensor(0x5191, 0xf0);
	write_cmos_sensor(0x5192, 0x03);
	write_cmos_sensor(0x5684, 0x10);
	write_cmos_sensor(0x5685, 0xa0);
	write_cmos_sensor(0x5686, 0x0c);
	write_cmos_sensor(0x5687, 0x78);
	write_cmos_sensor(0x5a00, 0x08);
	write_cmos_sensor(0x5a21, 0x00);
	write_cmos_sensor(0x5a24, 0x00);
}				/*    MIPI_sensor_Init  */

static void preview_setting(void)
{
	LOG_INF("E\n");
	write_cmos_sensor(0x3008, 0x42); /* enter software standby */

	/* OV5645_1296x972_raw_mipi_2lane_30fps */
	write_cmos_sensor(0x3612, 0xab);
	write_cmos_sensor(0x3618, 0x04);
	write_cmos_sensor(0x3035, 0x11);
	write_cmos_sensor(0x3036, 0x46);
	write_cmos_sensor(0x3501, 0x3d);
	write_cmos_sensor(0x3502, 0x00);
	write_cmos_sensor(0x350b, 0x3f);
	write_cmos_sensor(0x3600, 0x09);
	write_cmos_sensor(0x3601, 0x43);
	write_cmos_sensor(0x3708, 0x66);
	write_cmos_sensor(0x370c, 0xc3);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x00);
	write_cmos_sensor(0x3804, 0x0a);
	write_cmos_sensor(0x3805, 0x3f);
	write_cmos_sensor(0x3806, 0x07);
	write_cmos_sensor(0x3807, 0x9f);
	write_cmos_sensor(0x3808, 0x05);/* 1296 */
	write_cmos_sensor(0x3809, 0x10);
	write_cmos_sensor(0x380a, 0x03);/* 972 */
	write_cmos_sensor(0x380b, 0xcc);
	write_cmos_sensor(0x380c, 0x07);
	write_cmos_sensor(0x380d, 0x68);
	write_cmos_sensor(0x380e, 0x03);
	write_cmos_sensor(0x380f, 0xD8);
	write_cmos_sensor(0x3811, 0x08);
	write_cmos_sensor(0x3813, 0x02);
	write_cmos_sensor(0x3814, 0x31);
	write_cmos_sensor(0x3815, 0x31);
	write_cmos_sensor(0x3820, 0x41);
	write_cmos_sensor(0x3821, 0x07);
	write_cmos_sensor(0x3a02, 0x03);
	write_cmos_sensor(0x3a03, 0xd8);
	write_cmos_sensor(0x3a09, 0xf8);
	write_cmos_sensor(0x3a0a, 0x01);
	write_cmos_sensor(0x3a0b, 0xa4);
	write_cmos_sensor(0x3a0e, 0x02);
	write_cmos_sensor(0x3a0d, 0x02);
	write_cmos_sensor(0x3a14, 0x03);
	write_cmos_sensor(0x3a15, 0xd8);
	write_cmos_sensor(0x4004, 0x02);
	write_cmos_sensor(0x4514, 0xbb);
	write_cmos_sensor(0x4837, 0x21);

	write_cmos_sensor(0x3008, 0x02);/* wake up from standby */
	mDELAY(10);
}				/* preview_setting  */

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E\n");
	write_cmos_sensor(0x3008, 0x42);/* enter software standby */

	/* OV5645_2304x1944_Raw_mipi_2lane_15fps */
	write_cmos_sensor(0x3612, 0xab);
	write_cmos_sensor(0x3618, 0x04);
	write_cmos_sensor(0x3035, 0x12);
	write_cmos_sensor(0x3036, 0x69);
	write_cmos_sensor(0x3501, 0x7a);
	write_cmos_sensor(0x3502, 0xa0);
	write_cmos_sensor(0x350b, 0x3f);
	write_cmos_sensor(0x3600, 0x08);
	write_cmos_sensor(0x3601, 0x33);
	write_cmos_sensor(0x3708, 0x63);
	write_cmos_sensor(0x370c, 0xc0);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x00);
	write_cmos_sensor(0x3804, 0x0a);
	write_cmos_sensor(0x3805, 0x3f);
	write_cmos_sensor(0x3806, 0x07);
	write_cmos_sensor(0x3807, 0x9f);
	write_cmos_sensor(0x3808, 0x09);/* 2304 */
	write_cmos_sensor(0x3809, 0x00);
	write_cmos_sensor(0x380a, 0x07);/* 1944 */
	write_cmos_sensor(0x380b, 0x98);
	write_cmos_sensor(0x380c, 0x0B);/* 2844 */
	write_cmos_sensor(0x380d, 0x1C);
	write_cmos_sensor(0x380e, 0x07);/* 1968 */
	write_cmos_sensor(0x380f, 0xB0);
	write_cmos_sensor(0x3811, 0xa0);
	write_cmos_sensor(0x3813, 0x06);
	write_cmos_sensor(0x3814, 0x11);
	write_cmos_sensor(0x3815, 0x11);
	write_cmos_sensor(0x3820, 0x40);
	write_cmos_sensor(0x3821, 0x06);
	write_cmos_sensor(0x3a02, 0x07);
	write_cmos_sensor(0x3a03, 0xb0);
	write_cmos_sensor(0x3a09, 0x27);
	write_cmos_sensor(0x3a0a, 0x00);
	write_cmos_sensor(0x3a0b, 0xf6);
	write_cmos_sensor(0x3a0e, 0x06);
	write_cmos_sensor(0x3a0d, 0x08);
	write_cmos_sensor(0x3a14, 0x07);
	write_cmos_sensor(0x3a15, 0xb0);
	write_cmos_sensor(0x4004, 0x06);
	write_cmos_sensor(0x4514, 0x00);
	write_cmos_sensor(0x4837, 0x16);

	write_cmos_sensor(0x3008, 0x02);/* wake up from standby */
	mDELAY(10);
}				/*    capture_setting  */

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n", currefps);
	write_cmos_sensor(0x3008, 0x42);/* enter software standby */

	/* OV5645_1920x1080_Raw_mipi_2lane_30fps */
	write_cmos_sensor(0x3612, 0xab);
	write_cmos_sensor(0x3618, 0x04);
	write_cmos_sensor(0x3035, 0x12);
	write_cmos_sensor(0x3036, 0x69);
	write_cmos_sensor(0x3501, 0x45);
	write_cmos_sensor(0x3502, 0xa0);
	write_cmos_sensor(0x350b, 0x3f);
	write_cmos_sensor(0x3600, 0x08);
	write_cmos_sensor(0x3601, 0x33);
	write_cmos_sensor(0x3708, 0x63);
	write_cmos_sensor(0x370c, 0xc0);
	write_cmos_sensor(0x3800, 0x01);
	write_cmos_sensor(0x3801, 0x50);
	write_cmos_sensor(0x3802, 0x01);
	write_cmos_sensor(0x3803, 0xb0);
	write_cmos_sensor(0x3804, 0x08);
	write_cmos_sensor(0x3805, 0xef);
	write_cmos_sensor(0x3806, 0x05);
	write_cmos_sensor(0x3807, 0xef);
	write_cmos_sensor(0x3808, 0x07);/* 1920 */
	write_cmos_sensor(0x3809, 0x80);
	write_cmos_sensor(0x380a, 0x04);/* 1080 */
	write_cmos_sensor(0x380b, 0x38);
	write_cmos_sensor(0x380c, 0x09);
	write_cmos_sensor(0x380d, 0xC4);
	write_cmos_sensor(0x380e, 0x04);
	write_cmos_sensor(0x380f, 0x60);
	write_cmos_sensor(0x3811, 0x10);
	write_cmos_sensor(0x3813, 0x06);
	write_cmos_sensor(0x3814, 0x11);
	write_cmos_sensor(0x3815, 0x11);
	write_cmos_sensor(0x3820, 0x40);
	write_cmos_sensor(0x3821, 0x06);
	write_cmos_sensor(0x3a02, 0x07);
	write_cmos_sensor(0x3a03, 0xb0);
	write_cmos_sensor(0x3a09, 0x27);
	write_cmos_sensor(0x3a0a, 0x00);
	write_cmos_sensor(0x3a0b, 0xf6);
	write_cmos_sensor(0x3a0e, 0x06);
	write_cmos_sensor(0x3a0d, 0x08);
	write_cmos_sensor(0x3a14, 0x07);
	write_cmos_sensor(0x3a15, 0xb0);
	write_cmos_sensor(0x4004, 0x06);
	write_cmos_sensor(0x4514, 0x00);
	write_cmos_sensor(0x4837, 0x16);

	write_cmos_sensor(0x3008, 0x02);/* wake up from standby */
	mDELAY(10);
}				/*normal_video_setting  */

static void hs_video_setting(void)
{
	LOG_INF("E\n");
	write_cmos_sensor(0x3008, 0x42);/* enter software standby */

	/* OV5645_1280x720_raw_mipi_2lane_60fps */
	write_cmos_sensor(0x3612, 0xa9);
	write_cmos_sensor(0x3618, 0x00);
	write_cmos_sensor(0x3035, 0x11);
	write_cmos_sensor(0x3036, 0x69);
	write_cmos_sensor(0x3501, 0x5c);
	write_cmos_sensor(0x3502, 0x00);
	write_cmos_sensor(0x350b, 0x80);
	write_cmos_sensor(0x3600, 0x0a);
	write_cmos_sensor(0x3601, 0x75);
	write_cmos_sensor(0x3708, 0x66);
	write_cmos_sensor(0x370c, 0xc3);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0xf8);
	write_cmos_sensor(0x3804, 0x0a);
	write_cmos_sensor(0x3805, 0x3f);
	write_cmos_sensor(0x3806, 0x06);
	write_cmos_sensor(0x3807, 0xa7);
	write_cmos_sensor(0x3808, 0x05);/* 1280 */
	write_cmos_sensor(0x3809, 0x00);
	write_cmos_sensor(0x380a, 0x02);/* 720 */
	write_cmos_sensor(0x380b, 0xd0);
	write_cmos_sensor(0x380c, 0x07);
	write_cmos_sensor(0x380d, 0x64);
	write_cmos_sensor(0x380e, 0x02);/* 740 */
	write_cmos_sensor(0x380f, 0xe4);
	write_cmos_sensor(0x3811, 0x10);
	write_cmos_sensor(0x3813, 0x04);
	write_cmos_sensor(0x3814, 0x31);
	write_cmos_sensor(0x3815, 0x31);
	write_cmos_sensor(0x3820, 0x41);
	write_cmos_sensor(0x3821, 0x07);
	write_cmos_sensor(0x3a02, 0x02);
	write_cmos_sensor(0x3a03, 0xe4);
	write_cmos_sensor(0x3a09, 0xbc);
	write_cmos_sensor(0x3a0a, 0x01);
	write_cmos_sensor(0x3a0b, 0x72);
	write_cmos_sensor(0x3a0e, 0x01);
	write_cmos_sensor(0x3a0d, 0x02);
	write_cmos_sensor(0x3a14, 0x02);
	write_cmos_sensor(0x3a15, 0xe4);
	write_cmos_sensor(0x4004, 0x02);
	write_cmos_sensor(0x4514, 0xbb);
	write_cmos_sensor(0x4837, 0x21);

	write_cmos_sensor(0x3008, 0x02);/* wake up from standby */
	mDELAY(10);
}				/*hs_video_setting */

static void slim_video_setting(void)
{
	LOG_INF("E\n");
	write_cmos_sensor(0x3008, 0x42);/* enter software standby */

	/* OV5645_1280x720_raw_mipi_2lane_30fps */
	write_cmos_sensor(0x3612, 0xa9);
	write_cmos_sensor(0x3618, 0x00);
	write_cmos_sensor(0x3035, 0x11);
	write_cmos_sensor(0x3036, 0x69);
	write_cmos_sensor(0x3501, 0x5c);
	write_cmos_sensor(0x3502, 0x00);
	write_cmos_sensor(0x350b, 0x80);
	write_cmos_sensor(0x3600, 0x0a);
	write_cmos_sensor(0x3601, 0x75);
	write_cmos_sensor(0x3708, 0x66);
	write_cmos_sensor(0x370c, 0xc3);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0xf8);
	write_cmos_sensor(0x3804, 0x0a);
	write_cmos_sensor(0x3805, 0x3f);
	write_cmos_sensor(0x3806, 0x06);
	write_cmos_sensor(0x3807, 0xa7);
	write_cmos_sensor(0x3808, 0x05);/* 1280 */
	write_cmos_sensor(0x3809, 0x00);
	write_cmos_sensor(0x380a, 0x02);/* 720 */
	write_cmos_sensor(0x380b, 0xd0);
	write_cmos_sensor(0x380c, 0x07);
	write_cmos_sensor(0x380d, 0x64);
	write_cmos_sensor(0x380e, 0x05);/* 1480 */
	write_cmos_sensor(0x380f, 0xC8);
	write_cmos_sensor(0x3811, 0x10);
	write_cmos_sensor(0x3813, 0x04);
	write_cmos_sensor(0x3814, 0x31);
	write_cmos_sensor(0x3815, 0x31);
	write_cmos_sensor(0x3820, 0x41);
	write_cmos_sensor(0x3821, 0x07);
	write_cmos_sensor(0x3a02, 0x02);
	write_cmos_sensor(0x3a03, 0xe4);
	write_cmos_sensor(0x3a09, 0xbc);
	write_cmos_sensor(0x3a0a, 0x01);
	write_cmos_sensor(0x3a0b, 0x72);
	write_cmos_sensor(0x3a0e, 0x01);
	write_cmos_sensor(0x3a0d, 0x02);
	write_cmos_sensor(0x3a14, 0x02);
	write_cmos_sensor(0x3a15, 0xe4);
	write_cmos_sensor(0x4004, 0x02);
	write_cmos_sensor(0x4514, 0xbb);
	write_cmos_sensor(0x4837, 0x21);

	write_cmos_sensor(0x3008, 0x02);/* wake up from standby */
	mDELAY(10);
}				/*slim_video_setting */

/*************************************************************************
 * FUNCTION
 *    get_imgsensor_id
 *
 * DESCRIPTION
 *    This function get the sensor ID
 *
 * PARAMETERS
 *    *sensorID : return the sensor ID
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	// sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
    // we should detect the module used i2c address
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail, write id:0x%x id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		// if Sensor ID is not correct,
		// Must set *sensor_id to 0xFFFFFFFF
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

/*************************************************************************
 * FUNCTION
 *    open
 *
 * DESCRIPTION
 *    This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *    None
 *
 * RETURNS
 *    None
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
	LOG_2;

	// sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	//	we should detect the module used i2c address
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, write id:0x%x id: 0x%x\n",
				imgsensor.i2c_write_id, sensor_id);
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
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}				/*    open  */

/*************************************************************************
 * FUNCTION
 *    close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *    None
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("E\n");

	/*No Need to implement this function */

	return ERROR_NONE;
}				/*    close  */

/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *    This function start the sensor preview.
 *
 * PARAMETERS
 *    *image_window : address pointer of pixel
 *		numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of
 *		line numbers in one period of VSYNC
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

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
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */
	return ERROR_NONE;
}				/* preview  */

/*************************************************************************
 * FUNCTION
 *    capture
 *
 * DESCRIPTION
 *    This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		/// PIP capture: 24fps for less than 13M
		//	20fps for 16M,15fps for 20M
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support,
					so use cap1's setting: %d fps!\n",
			     imgsensor.current_fps,
					imgsensor_info.cap1.max_framerate / 10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	return ERROR_NONE;
}				/* capture() */

static kal_uint32 normal_video(
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			       MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	return ERROR_NONE;
}				/*    normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

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
}				/*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	return ERROR_NONE;
}				/*    slim_video     */

static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT * sensor_resolution)
{
	LOG_INF("E\n");
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

	sensor_resolution->SensorHighSpeedVideoWidth =
		imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight =
		imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth =
		imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
		imgsensor_info.slim_video.grabwindow_height;
	return ERROR_NONE;
}				/*    get_resolution    */

static kal_uint32 get_info(MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity =
		SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity =
		SENSOR_CLOCK_POLARITY_LOW;	/* not use */
	sensor_info->SensorHsyncPolarity =
		SENSOR_CLOCK_POLARITY_LOW;	/* inverse with datasheet */
	sensor_info->SensorVsyncPolarity =
		SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;	/* not use */
	sensor_info->SensorResetActiveHigh = FALSE;	/* not use */
	sensor_info->SensorResetDelayCount = 5;	/* not use */

	sensor_info->SensroInterfaceType =
		imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType =
		imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode =
		imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame =
		imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame =
		imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame =
		imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
		imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
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
	sensor_info->IHDR_Support =
		imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine =
		imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum =
		imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber =
		imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq =
		imgsensor_info.mclk;
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
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

		sensor_info->SensorGrabStartX =
			imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.normal_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

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
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}				/*    get_info  */

static kal_uint32 control(MSDK_SCENARIO_ID_ENUM scenario_id,
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
}				/* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{				/*  */
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
	if (enable)		/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(
	MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length =
		    imgsensor_info.pre.pclk / framerate * 10 /
				imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.pre.framelength) ? (frame_length -
				imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
		imgsensor_info.pre.framelength +
		imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length =
		    imgsensor_info.normal_video.pclk / framerate * 10 /
		    imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.normal_video.framelength) ? (frame_length -
				imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length =
		    imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		frame_length =
		    imgsensor_info.cap.pclk / framerate * 10 /
						imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.cap.framelength) ? (frame_length -
				imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap.framelength +
		imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length =
		    imgsensor_info.hs_video.pclk / framerate * 10 /
		    imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.hs_video.framelength) ? (frame_length -
			imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length =
		imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length =
		    imgsensor_info.slim_video.pclk / framerate * 10 /
		    imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		    imgsensor_info.slim_video.framelength) ? (frame_length -
			imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length =
		    imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	default:		/* coding with  preview scenario by default */
		frame_length =
		    imgsensor_info.pre.pclk / framerate * 10 /
				imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		    imgsensor_info.pre.framelength) ? (frame_length -
			imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
		imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		LOG_INF("error scenario_id = %d, we use preview scenario\n",
		scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(
		MSDK_SCENARIO_ID_ENUM scenario_id,
						    MUINT32 *framerate)
{
	LOG_INF("dongchun: scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*framerate = imgsensor_info.normal_video.max_framerate;
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
	LOG_INF("enable: %d\n", enable);

	/* 0x5036: bit[7]: 0 isp 1 colorbar */
	if (enable)
		write_cmos_sensor(0x5036, 0x80);	/* check */
	else
		write_cmos_sensor(0x5036, 0x00);	/* check */

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;

	SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		LOG_INF(
"feature_Control imgsensor.pclk = %d, imgsensor.current_fps = %d\n",
imgsensor.pclk, imgsensor.current_fps);
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL) * feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(
		sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
		read_cmos_sensor(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		// get the lens driver ID from EEPROM or
		//	just return LENS_DRIVER_ID_DO_NOT_CARE
		// if EEPROM does not exist in camera module.
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
		set_auto_flicker_mode((BOOL) * feature_data_16,
						*(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
		(MSDK_SCENARIO_ID_ENUM) *feature_data,
		*(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
		(MSDK_SCENARIO_ID_ENUM) *(feature_data),
		(MUINT32 *) (uintptr_t) (feature_data + 1));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) * feature_data);
		break;
		/* for factory mode auto testing */
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", (UINT32) *feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", (BOOL) * feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (BOOL) * feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
		(UINT32) *feature_data);

		wininfo = (SENSOR_WINSIZE_INFO_STRUCT *)
				(uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[1],
			sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[2],
			sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[3],
			sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[4],
			sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[0],
		    sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		ihdr_write_shutter_gain((UINT16) *feature_data,
					(UINT16) *(feature_data + 1),
					(UINT16) *(feature_data + 2));
		break;
	default:
		break;
	}

	return ERROR_NONE;
}				/*    feature_control()  */

static SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 OV5645_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*    OV5645MIPISensorInit    */
