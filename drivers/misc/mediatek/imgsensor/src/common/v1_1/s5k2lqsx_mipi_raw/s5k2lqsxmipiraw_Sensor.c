/*
 * Copyright (C) 2019 MediaTek Inc.
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
/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 S5K2LQSXmipi_Sensor.c
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
#define PFX "S5K2LQ_camera_sensor"
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
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "s5k2lqsxmipiraw_Sensor.h"
/*#include <mmdvfs_mgr.h>*/
#include <linux/pm_qos.h>



static DEFINE_SPINLOCK(imgsensor_drv_lock);
static bool bIsLongExposure = KAL_FALSE;

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5K2LQSX_SENSOR_ID,

	.checksum_value = 0xf1fad6d0,

	.pre = {
		.pclk = 840000000,
		.linelength = 17440,
		.framelength = 1604,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2016,
		.grabwindow_height = 1512,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 136000000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 840000000,
		.linelength = 8832,
		.framelength = 3170,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4032,
		.grabwindow_height = 3024,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 512000000,
		.max_framerate = 300,

	},
	.normal_video = {
		.pclk = 840000000,
		.linelength = 8832,
		.framelength = 3170,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4032,
		.grabwindow_height = 2272,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 512000000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 960000000,
		.linelength = 6392,
		.framelength = 1250,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 550400000,
		.max_framerate = 1200,
		},
	.slim_video = { /* use preview setting*/
		.pclk = 960000000,
		.linelength = 19640,
		.framelength = 1628,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2016,
		.grabwindow_height = 1512,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		},
	.custom1 = { /*use cap_setting for stereo camera*/
		.pclk = 960000000,
		.linelength = 10160,
		.framelength = 3148,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4032,
		.grabwindow_height = 3024,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		},
	.custom2 = { /*cpy from preview*/
		.pclk = 960000000,
		.linelength = 19640,
		.framelength = 1628,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2016,
		.grabwindow_height = 1512,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		},
	 .custom3 = { /*cpy from preview*/
		.pclk = 960000000,
		.linelength = 13528,
		.framelength = 1182,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 196800000,
		.max_framerate = 600,
		},

		.margin = 5,
		.min_shutter = 4,
		.max_frame_length = 0xffff,
		.ae_shut_delay_frame = 0,
		.ae_sensor_gain_delay_frame = 0,
		.ae_ispGain_delay_frame = 2,
		.frame_time_delay_frame = 2,
		.ihdr_support = 0,	  /*1, support; 0,not support*/
		.ihdr_le_firstline = 0,  /*1,le first ; 0, se first*/
		.sensor_mode_num = 8,	  /*support sensor mode num*/

		.cap_delay_frame = 2,/*3 guanjd modify */
		.pre_delay_frame = 2,/*3 guanjd modify */
		.video_delay_frame = 3,
		.hs_video_delay_frame = 3,
		.slim_video_delay_frame = 3,
		.custom1_delay_frame = 2,
		.custom2_delay_frame = 2,
		.custom3_delay_frame = 2,

		.isp_driving_current = ISP_DRIVING_4MA,
		.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
		/*0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2*/
		.mipi_sensor_type = MIPI_OPHY_NCSI2,
		/*0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL*/
		.mipi_settle_delay_mode = 1,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
		.mclk = 24,
		.mipi_lane_num = SENSOR_MIPI_4_LANE,
		.i2c_addr_table = {0x20, 0xff},
		.i2c_speed = 1000,
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,			/*mirrorflip information*/
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,				/*current shutter*/
	.gain = 0x100,					/*current gain*/
	.dummy_pixel = 0,				/*current dummypixel*/
	.dummy_line = 0,				/*current dummyline*/
	.current_fps = 0,
	/*auto flicker enable: KAL_FALSE for disable auto flicker,
	 * KAL_TRUE for enable auto flicker
	 */
	.autoflicker_en = KAL_FALSE,
	/*test pattern mode or not. KAL_FALSE for in test pattern mode,
	 * KAL_TRUE for normal output
	 */
	.test_pattern = KAL_FALSE,
	/*current scenario id*/
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_mode = 0, /*sensor need support LE, SE with HDR feature*/
	.i2c_write_id = 0x20,
	.current_ae_effective_frame = 1,
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[8] = {
	{ 4032, 3024,	0,	0,   4032, 3024, 2016,  1512,
	0000, 0000, 2016,  1512,	  0,	0, 2016, 1512}, /*Preview*/
	{ 4032, 3024,	0,	0,   4032, 3024, 4032,  3024,
	0000, 0000, 4032,  3024,	  0,	0, 4032, 3024}, /*capture*/
	{ 4032, 3024,	0,	376, 4032, 2272, 4032,	2272,
	0000, 0000, 4032,  2272,	  0,	0, 4032, 2272}, /*video*/
	{ 4032, 3024,	96,	432, 3840, 2160, 1920,  1080,
	0000, 0000, 1920,  1080,	  0,	0, 1920, 1080}, /*high s video*/
	{ 4032, 3024,	0,	0,   4032, 3024, 2016,  1512,
	0000, 0000, 2016,  1512,	  0,	0, 2016, 1512}, /*slim video*/
	{ 4032, 3024,	0,	0,   4032, 3024, 4032,  3024,
	0000, 0000, 4032,  3024,	  0,	0, 4032, 3024}, /*custom1*/
	{ 4032, 3024,	0,	0,   4032, 3024, 2016,  1512,
	0000, 0000, 2016,  1512,	  0,	0, 2016, 1512}, /*custom2*/
	{ 4032, 3024,	96,	432,   3840, 2160, 1920,  1080,
	0000, 0000, 1920,  1080,	  0,	0, 1920, 1080}, /*custom3*/
};


/*no mirror flip*/
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX = 0,
	.i4PitchY = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = { {0, 0} },
	.i4PosR = { {0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.i4Crop = {
		{0, 0}, {0, 0}, {0, 376}, {0, 0}, {0, 0},
		{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}
	},
	.iMirrorFlip = 0,
};


/*VC1 None , VC2 for PDAF(DT=0X30), unit : 10bit*/
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3] = {
	/* Preview mode setting */
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 0x1070, 0x0C30, 0x00, 0x00, 0x0280, 0x0001,
		0x01, 0x30, 0x09D8, 0x017A, 0x03, 0x00, 0x0000, 0x0000
	},
	/* Capture mode setting */
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 0x1070, 0x0C30, 0x00, 0x00, 0x0280, 0x0001,
		0x01, 0x30, 0x13B0, 0x02F4, 0x03, 0x00, 0x0000, 0x0000
	},
	/* Video mode setting */
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 0x1070, 0x0C30, 0x00, 0x00, 0x0280, 0x0001,
		0x01, 0x30, 0x13B0, 0x0238, 0x03, 0x00, 0x0000, 0x0000
	},
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {
		(char)(addr >> 8), (char)(addr & 0xFF)};
	 /*Add this func to set i2c speed by each sensor*/
	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 2, imgsensor.i2c_write_id);
	return ((get_byte << 8) & 0xff00) | ((get_byte >> 8) & 0x00ff);
}


static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF),
				(char)(para >> 8), (char)(para & 0xFF)};
	/*Add this func to set i2c speed by each sensor*/
	iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF)};
	/*Add this func to set i2c speed by each sensor*/
	iReadRegI2C(pusendcmd, 2,
			(u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};
	 /*Add this func to set i2c speed by each sensor*/
	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 1020	/* trans# max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN 4

#endif

static kal_uint16 table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data = 0;

	tosend = 0;
	IDX = 0;
	while (len > IDX) {
		addr = para[IDX];

		{
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)(data >> 8);
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}
#if MULTI_WRITE
/* Write when remain buffer size is less than 4 bytes or reach end of data */
	if ((I2C_BUFFER_LEN - tosend) < 4 || IDX == len || addr != addr_last) {
		iBurstWriteReg_multi(puSendCmd, tosend,
			imgsensor.i2c_write_id,
			4, imgsensor_info.i2c_speed);
		tosend = 0;
	}
#else
		iWriteRegI2CTiming(puSendCmd, 4,
			imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
		tosend = 0;

#endif
	}
	return 0;
}



static void set_dummy(void)
{
	pr_debug("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	 write_cmos_sensor(0x0340, imgsensor.frame_length);
	 write_cmos_sensor(0x0342, imgsensor.line_length);
}	/*	set_dummy  */


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{

	kal_uint32 frame_length = imgsensor.frame_length;

	pr_debug("framerate = %d, min framelength should enable %d\n",
		framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	if (frame_length >= imgsensor.min_frame_length)
		imgsensor.frame_length = frame_length;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

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

static kal_uint32 streaming_control(kal_bool enable)
{
	int timeout = (10000 / imgsensor.current_fps) + 1;
	int i = 0;
	int framecnt = 0;

	pr_debug("streaming_enable(0= Sw Standby,1= streaming): %d\n",
			enable);
	if (enable) {
		write_cmos_sensor_8(0x0100, 0X01);
		mdelay(10);
	} else {
		write_cmos_sensor_8(0x0100, 0x00);
		for (i = 0; i < timeout; i++) {
			mdelay(5);
			framecnt = read_cmos_sensor_8(0x0005);
			if (framecnt == 0xFF) {
				pr_debug(" Stream Off OK at i=%d.\n", i);
				return ERROR_NONE;
			}
		}
		pr_debug("Stream Off Fail! framecnt= %d.\n", framecnt);
	}
	return ERROR_NONE;
}


static void write_shutter(kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;

	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x2D1E);
	write_cmos_sensor(0x6F12, 0x8000);
	write_cmos_sensor(0x602A, 0x2D22);
	write_cmos_sensor(0x6F12, 0x0310);
	write_cmos_sensor(0x602A, 0x379E);
	write_cmos_sensor(0x6F12, 0x000D);

	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0342, imgsensor.line_length);
	write_cmos_sensor(0x0340, imgsensor.frame_length);
	write_cmos_sensor(0x0202, 0x0100);
	write_cmos_sensor(0x0702, 0x0000);
	write_cmos_sensor(0x0704, 0x0000);
	/*stream on*/
	/*write_cmos_sensor_8(0x0100, 0x01);*/

	pr_debug(
	"enter normal shutter shutter = %d, imgsensor.frame_lengths = 0x%x, imgsensor.line_length = 0x%x\n",
		shutter, imgsensor.frame_length, imgsensor.line_length);

	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk /
			imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length*/
			write_cmos_sensor(0x0340, imgsensor.frame_length);
		}
	} else {
		/* Extend frame length*/
		write_cmos_sensor(0x0340, imgsensor.frame_length);
	}

	/* Update Shutter*/
	    write_cmos_sensor(0x0202, shutter);
	pr_debug("shutter = %d, framelength = %d\n",
			shutter, imgsensor.frame_length);

}	/*	write_shutter  */



/*************************************************************************
 * FUNCTION
 * set_shutter
 *
 * DESCRIPTION
 * This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *  iShutter : exposured lines
 *
 * RETURNS
 *  None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}	/*	set_shutter */

/*	write_shutter  */
static void set_shutter_frame_length(
				kal_uint16 shutter, kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);
	/* Change frame time */
	if (frame_length > 1)
		dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;

	/*  */
	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ?
				imgsensor_info.min_shutter : shutter;
	shutter =
	  (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
	  ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk /
			imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(0x0340,
				imgsensor.frame_length & 0xFFFF);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
	}

	/* Update Shutter */
	write_cmos_sensor(0X0202, shutter & 0xFFFF);

	pr_debug("shutter = %d, framelength = %d/%d, dummy_line= %d\n",
			shutter, imgsensor.frame_length,
		frame_length, dummy_line);

}		/*      write_shutter  */


static kal_uint16 gain2reg(const kal_uint16 gain)
{
	 kal_uint16 reg_gain = 0x0;

	reg_gain = gain / 2;
	return (kal_uint16)reg_gain;
}

/*************************************************************************
 * FUNCTION
 * set_gain
 *
 * DESCRIPTION
 * This function is to set global gain to sensor.
 *
 * PARAMETERS
 * iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 * the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

	/*gain= 1024;for test*/
	/*return; for test*/

	if (gain < BASEGAIN || gain > 32 * BASEGAIN) {
		pr_debug("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 32 * BASEGAIN)
			gain = 32 * BASEGAIN;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	pr_debug("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor(0x0204, reg_gain);
	/*write_cmos_sensor_8(0x0204,(reg_gain>>8));*/
	/*write_cmos_sensor_8(0x0205,(reg_gain&0xff));*/

	return gain;
}	/*	set_gain  */

static void set_mirror_flip(kal_uint8 image_mirror)
{
	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor_8(0x0101, 0x00);   /* Gr*/
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor_8(0x0101, 0x01);
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor_8(0x0101, 0x02);
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor_8(0x0101, 0x03);/*Gb*/
		break;
	default:
		pr_debug("Error image_mirror setting\n");
	}
}

/*************************************************************************
 * FUNCTION
 * night_mode
 *
 * DESCRIPTION
 * This function night mode of sensor.
 *
 * PARAMETERS
 * bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 * None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
#if 0
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}	/*	night_mode	*/
#endif

static kal_uint16 addr_data_pair_init[] = {

	0x6214, 0xF970,
	0x6218, 0xF150,
	0x6028, 0x4000,
	0x0136, 0x1800,
	0x0A02, 0x01FF,
	0x6028, 0x2000,
	0x602A, 0x39B8,
	0x6F12, 0x0C00,
	0x602A, 0x3B04,
	0x6F12, 0x2006,
	0x6F12, 0x0084,
	0x602A, 0x39DE,
	0x6F12, 0x0628,
	0x6F12, 0x0080,
	0x602A, 0x39D6,
	0x6F12, 0x00FD,
	0x6F12, 0x22EF,
	0x602A, 0x0DE4,
	0x6F12, 0x011E,
	0x602A, 0x0DE2,
	0x6F12, 0x0001,
	0x602A, 0x0D8C,
	0x6F12, 0x0005,
	0x602A, 0x0E80,
	0x6F12, 0x0101,
	0x6F12, 0x01FF,
	0x602A, 0x0E86,
	0x6F12, 0x01FF,
	0x602A, 0x0E90,
	0x6F12, 0x0020,
	0x6F12, 0x0060,
	0x6F12, 0xB118,
	0x602A, 0x0E84,
	0x6F12, 0x003D,
	0x602A, 0x0E88,
	0x6F12, 0x003D,
	0x602A, 0x0EBA,
	0x6F12, 0x0001,
	0x6F12, 0x0002,
	0x6F12, 0xB102,
	0x6F12, 0x002F,
	0x6F12, 0x0F2F,
	0x6F12, 0xF46E,
	0x602A, 0x4D26,
	0x6F12, 0x0100,
	0x602A, 0x4D32,
	0x6F12, 0x0100,
	0x602A, 0x0F4A,
	0x6F12, 0x0003,
	0x6F12, 0x0005,
	0x602A, 0x5772,
	0x6F12, 0x0002,
	0x602A, 0x5766,
	0x6F12, 0x0100,
	0x602A, 0x0DE0,
	0x6F12, 0x0101,
	0x602A, 0x3E10,
	0x6F12, 0x1E3C,
	0x602A, 0x39D0,
	0x6F12, 0x0096,
	0x602A, 0x4148,
	0x6F12, 0x0245,
	0x6028, 0x4000,
	0xF462, 0x0017,
	0xF464, 0x0018,
	0xF46C, 0x0010,
	0xF466, 0x0018,
	0xF41A, 0x0000,
	0xF468, 0x0000,
	0xF48A, 0x0080,
	0x0110, 0x1002,
	0x0B06, 0x0101,
	0x0FEA, 0x2040,
	0x6B36, 0x5200,
	0x6B40, 0x2000,
	0x001C, 0x0000,
};

static kal_uint16 addr_data_pair_preview[] = {
	0x6028, 0x4000,
	0x6214, 0xF970,
	0x6218, 0xF150,
	0x0344, 0x0000,
	0x0346, 0x0000,
	0x0348, 0x1F7F,
	0x034A, 0x0BCF,
	0x034C, 0x07E0,
	0x034E, 0x05E8,
	0x0350, 0x0000,
	0x0352, 0x0000,
	0x0900, 0x0112,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0386, 0x0003,
	0x0404, 0x2000,
	0x0402, 0x1010,
	0x0114, 0x0301,
	0x0110, 0x1002,
	0x0136, 0x1800,
	0x0304, 0x0004,
	0x0306, 0x00AF,
	0x0302, 0x0001,
	0x0300, 0x0005,
	0x030C, 0x0000,
	0x030E, 0x0003,
	0x0310, 0x00AA,
	0x0312, 0x0002,
	0x030A, 0x0001,
	0x0308, 0x0008,
	0x0342, 0x4420,
	0x0340, 0x0644,
	0x0202, 0x0100,
	0x021E, 0x0000,
	0x0C00, 0x0000,
	0x0C02, 0x0000,
	0x0C04, 0x0000,
	0x0C06, 0x0000,
	0x2B94, 0x0000,
	0x2B98, 0x0000,
	0x0B02, 0x0203,
	0x0B80, 0x0100,
	0x0B84, 0x0201,
	0x0B04, 0x0001,
	0x0902, 0x0001,
	0x0118, 0x0102,
	0x6028, 0x2000,
	0x602A, 0x0EF0,
	0x6F12, 0x0101,
	0x602A, 0x0F12,
	0x6F12, 0x002F,
	0x602A, 0x39BA,
	0x6F12, 0x2848,
	0x6F12, 0x5304,
	0x602A, 0x3982,
	0x6F12, 0x0F0D,
	0x602A, 0x3988,
	0x6F12, 0x2E1E,
	0x602A, 0x398E,
	0x6F12, 0x2E2E,
	0x602A, 0x39BE,
	0x6F12, 0x0808,
	0x602A, 0x3984,
	0x6F12, 0x0D0B,
	0x602A, 0x398A,
	0x6F12, 0x1E1E,
	0x602A, 0x3990,
	0x6F12, 0x2E2E,
	0x602A, 0x2586,
	0x6F12, 0x0002,
	0x602A, 0x2592,
	0x6F12, 0x0004,
	0x602A, 0x259E,
	0x6F12, 0x000B,
	0x602A, 0x25A4,
	0x6F12, 0x000B,
	0x602A, 0x25C2,
	0x6F12, 0x000B,
	0x602A, 0x25C8,
	0x6F12, 0x000B,
	0x602A, 0x25FE,
	0x6F12, 0x0018,
	0x602A, 0x262E,
	0x6F12, 0x0000,
	0x602A, 0x2736,
	0x6F12, 0x000F,
	0x602A, 0x273C,
	0x6F12, 0x002D,
	0x602A, 0x2742,
	0x6F12, 0x0046,
	0x602A, 0x27E4,
	0x6F12, 0x000B,
	0x602A, 0x27F0,
	0x6F12, 0x002E,
	0x602A, 0x27F6,
	0x6F12, 0x003B,
	0x602A, 0x27FC,
	0x6F12, 0x0049,
	0x602A, 0x2802,
	0x6F12, 0x0056,
	0x602A, 0x2808,
	0x6F12, 0x0063,
	0x602A, 0x280E,
	0x6F12, 0x0071,
	0x602A, 0x2814,
	0x6F12, 0x007E,
	0x602A, 0x281A,
	0x6F12, 0x008B,
	0x602A, 0x2820,
	0x6F12, 0x0099,
	0x602A, 0x2826,
	0x6F12, 0x00A6,
	0x602A, 0x282C,
	0x6F12, 0x02F8,
	0x602A, 0x2832,
	0x6F12, 0x0305,
	0x602A, 0x2838,
	0x6F12, 0x0312,
	0x602A, 0x283E,
	0x6F12, 0x031F,
	0x602A, 0x2844,
	0x6F12, 0x032C,
	0x602A, 0x284A,
	0x6F12, 0x033A,
	0x602A, 0x2850,
	0x6F12, 0x0347,
	0x602A, 0x2856,
	0x6F12, 0x0354,
	0x602A, 0x285C,
	0x6F12, 0x0362,
	0x602A, 0x2862,
	0x6F12, 0x036F,
	0x602A, 0x2868,
	0x6F12, 0x037C,
	0x602A, 0x28BC,
	0x6F12, 0x002A,
	0x602A, 0x28E6,
	0x6F12, 0x00BE,
	0x602A, 0x28EC,
	0x6F12, 0x00BE,
	0x602A, 0x28F8,
	0x6F12, 0x0511,
	0x602A, 0x3798,
	0x6F12, 0x093A,
	0x602A, 0x25CE,
	0x6F12, 0x0048,
	0x602A, 0x25E0,
	0x6F12, 0x0006,
	0x602A, 0x2D1E,
	0x6F12, 0x8000,
	0x602A, 0x379E,
	0x6F12, 0x000D,
	0x602A, 0x2640,
	0x6F12, 0x0005,
	0x602A, 0x2532,
	0x6F12, 0x01E0,
	0x602A, 0x253E,
	0x6F12, 0x03C0,
	0x602A, 0x256E,
	0x6F12, 0x08C0,
	0x602A, 0x3994,
	0x6F12, 0x0507,
	0x602A, 0x399A,
	0x6F12, 0x0F04,
	0x602A, 0x39A0,
	0x6F12, 0x0404,
	0x602A, 0x39A6,
	0x6F12, 0x0B07,
	0x602A, 0x3970,
	0x6F12, 0x0300,
	0x602A, 0x258A,
	0x6F12, 0x0009,
	0x602A, 0x2596,
	0x6F12, 0x0010,
	0x602A, 0x25A2,
	0x6F12, 0x0007,
	0x602A, 0x25A8,
	0x6F12, 0x0007,
	0x602A, 0x25B4,
	0x6F12, 0x0002,
	0x602A, 0x25C0,
	0x6F12, 0x0006,
	0x602A, 0x25C6,
	0x6F12, 0x0007,
	0x602A, 0x25CC,
	0x6F12, 0x0007,
	0x602A, 0x25D2,
	0x6F12, 0x0030,
	0x602A, 0x25E4,
	0x6F12, 0x001E,
	0x602A, 0x25EA,
	0x6F12, 0x0002,
	0x602A, 0x25F0,
	0x6F12, 0x0006,
	0x602A, 0x25F6,
	0x6F12, 0x0008,
	0x602A, 0x25FC,
	0x6F12, 0x0002,
	0x602A, 0x2602,
	0x6F12, 0x0002,
	0x602A, 0x260E,
	0x6F12, 0x0002,
	0x602A, 0x2614,
	0x6F12, 0x0015,
	0x602A, 0x261A,
	0x6F12, 0x0036,
	0x602A, 0x2620,
	0x6F12, 0x002F,
	0x602A, 0x2626,
	0x6F12, 0x0035,
	0x602A, 0x2632,
	0x6F12, 0x0006,
	0x602A, 0x264A,
	0x6F12, 0x000E,
	0x602A, 0x2650,
	0x6F12, 0x000E,
	0x602A, 0x273A,
	0x6F12, 0x000D,
	0x602A, 0x2740,
	0x6F12, 0x0033,
	0x602A, 0x2746,
	0x6F12, 0x0044,
	0x602A, 0x27E8,
	0x6F12, 0x0009,
	0x602A, 0x27EE,
	0x6F12, 0x0021,
	0x602A, 0x27F4,
	0x6F12, 0x002C,
	0x602A, 0x27FA,
	0x6F12, 0x0037,
	0x602A, 0x2800,
	0x6F12, 0x0042,
	0x602A, 0x2806,
	0x6F12, 0x004D,
	0x602A, 0x280C,
	0x6F12, 0x0058,
	0x602A, 0x2812,
	0x6F12, 0x009D,
	0x602A, 0x2818,
	0x6F12, 0x00A8,
	0x602A, 0x281E,
	0x6F12, 0x00B3,
	0x602A, 0x2824,
	0x6F12, 0x00BE,
	0x602A, 0x282A,
	0x6F12, 0x00C9,
	0x602A, 0x2830,
	0x6F12, 0x01B5,
	0x602A, 0x2836,
	0x6F12, 0x01C0,
	0x602A, 0x283C,
	0x6F12, 0x01CB,
	0x602A, 0x2842,
	0x6F12, 0x01D6,
	0x602A, 0x2848,
	0x6F12, 0x01E1,
	0x602A, 0x284E,
	0x6F12, 0x02C1,
	0x602A, 0x2854,
	0x6F12, 0x02CC,
	0x602A, 0x285A,
	0x6F12, 0x02D7,
	0x602A, 0x2860,
	0x6F12, 0x02E2,
	0x602A, 0x2866,
	0x6F12, 0x02ED,
	0x602A, 0x286C,
	0x6F12, 0x02F8,
	0x602A, 0x28C0,
	0x6F12, 0x0014,
	0x602A, 0x28EA,
	0x6F12, 0x003C,
	0x602A, 0x28F0,
	0x6F12, 0x003C,
	0x602A, 0x28FC,
	0x6F12, 0x0063,
	0x602A, 0x379C,
	0x6F12, 0x0320,
	0x602A, 0x2D22,
	0x6F12, 0x0310,
	0x602A, 0x2536,
	0x6F12, 0x01A0,
	0x602A, 0x2542,
	0x6F12, 0x04C0,
	0x602A, 0x2572,
	0x6F12, 0x0880,
	0x602A, 0x3996,
	0x6F12, 0x0707,
	0x602A, 0x399C,
	0x6F12, 0x0404,
	0x602A, 0x39A2,
	0x6F12, 0x0404,
	0x602A, 0x39A8,
	0x6F12, 0x0707,
	0x602A, 0x39AC,
	0x6F12, 0x0002,
	0x6F12, 0x0202,
	0x602A, 0x39DA,
	0x6F12, 0x03A5,
	0x602A, 0x3B0E,
	0x6F12, 0x0101,
	0x602A, 0x3B0A,
	0x6F12, 0x0100,
	0x6F12, 0x06C0,
	0x602A, 0x3B98,
	0x6F12, 0x0064,
	0x602A, 0x3B96,
	0x6F12, 0x076C,
	0x602A, 0x3B94,
	0x6F12, 0x05A0,
	0x602A, 0x3B9C,
	0x6F12, 0x06E0,
	0x602A, 0x3B92,
	0x6F12, 0x06DE,
	0x602A, 0x3B9A,
	0x6F12, 0x076C,
	0x602A, 0x39D4,
	0x6F12, 0x19CC,
	0x602A, 0x0DDA,
	0x6F12, 0x0000,
	0x602A, 0x3B00,
	0x6F12, 0x7082,
	0x6F12, 0x0CCA,
	0x602A, 0x51A6,
	0x6F12, 0xFFC0,
	0x602A, 0x51E8,
	0x6F12, 0xFFC0,
	0x602A, 0x51AA,
	0x6F12, 0x0001,
	0x6F12, 0x0101,
	0x602A, 0x51D2,
	0x6F12, 0x3F2D,
	0x602A, 0x51D6,
	0x6F12, 0x2D3F,
	0x602A, 0x51B6,
	0x6F12, 0x02BC,
	0x602A, 0x51C6,
	0x6F12, 0x001F,
	0x6F12, 0x002D,
	0x6F12, 0x001F,
	0x6F12, 0x001F,
	0x6F12, 0x002D,
	0x6F12, 0x001F,
	0x602A, 0x51B0,
	0x6F12, 0x0000,
	0x6F12, 0x0240,
	0x6F12, 0x0000,
	0x602A, 0x51EC,
	0x6F12, 0x0001,
	0x6F12, 0x0101,
	0x602A, 0x5214,
	0x6F12, 0x3F2D,
	0x602A, 0x5218,
	0x6F12, 0x2D3F,
	0x602A, 0x51F8,
	0x6F12, 0x02BC,
	0x602A, 0x5208,
	0x6F12, 0x001F,
	0x6F12, 0x002D,
	0x6F12, 0x001F,
	0x6F12, 0x001F,
	0x6F12, 0x002D,
	0x6F12, 0x001F,
	0x602A, 0x51F2,
	0x6F12, 0x0000,
	0x6F12, 0x0240,
	0x6F12, 0x0000,
	0x602A, 0x0E64,
	0x6F12, 0x2500,
	0x602A, 0x0E78,
	0x6F12, 0x0000,
	0x6F12, 0x7C00,
	0x602A, 0x5770,
	0x6F12, 0x0000,
	0x6028, 0x4000,
	0x010C, 0x0000,
	0x6214, 0xF9F0,
	0x6218, 0xF9F0,
	0x6028, 0x4000,
	0x0100, 0x0100,
};

static kal_uint16 addr_data_pair_capture[] = {
	0x6028, 0x4000,
	0x6214, 0xF970,
	0x6218, 0xF150,
	0x0344, 0x0000,
	0x0346, 0x0000,
	0x0348, 0x1F7F,
	0x034A, 0x0BCF,
	0x034C, 0x0FC0,
	0x034E, 0x0BD0,
	0x0350, 0x0000,
	0x0352, 0x0000,
	0x0900, 0x0011,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0386, 0x0001,
	0x0404, 0x1000,
	0x0402, 0x1010,
	0x0114, 0x0301,
	0x0110, 0x1002,
	0x0136, 0x1800,
	0x0304, 0x0004,
	0x0306, 0x00AF,
	0x0302, 0x0001,
	0x0300, 0x0005,
	0x030C, 0x0000,
	0x030E, 0x0003,
	0x0310, 0x00A0,
	0x0312, 0x0000,
	0x030A, 0x0001,
	0x0308, 0x0008,
	0x0342, 0x2280,
	0x0340, 0x0C62,
	0x0202, 0x0100,
	0x021E, 0x0000,
	0x0C00, 0x0000,
	0x0C02, 0x0000,
	0x0C04, 0x0000,
	0x0C06, 0x0000,
	0x2B94, 0x0000,
	0x2B98, 0x0000,
	0x0B02, 0x0203,
	0x0B80, 0x0100,
	0x0B84, 0x0201,
	0x0B04, 0x0001,
	0x0902, 0x0000,
	0x0118, 0x0102,
	0x6028, 0x2000,
	0x602A, 0x0EF0,
	0x6F12, 0x0101,
	0x602A, 0x0F12,
	0x6F12, 0xFFFF,
	0x602A, 0x39BA,
	0x6F12, 0x2848,
	0x6F12, 0x5304,
	0x602A, 0x3982,
	0x6F12, 0x0F0D,
	0x602A, 0x3988,
	0x6F12, 0x2E1E,
	0x602A, 0x398E,
	0x6F12, 0x2E2E,
	0x602A, 0x39BE,
	0x6F12, 0x0808,
	0x602A, 0x3984,
	0x6F12, 0x0D0B,
	0x602A, 0x398A,
	0x6F12, 0x1E1E,
	0x602A, 0x3990,
	0x6F12, 0x2E2E,
	0x602A, 0x2586,
	0x6F12, 0x0002,
	0x602A, 0x2592,
	0x6F12, 0x0004,
	0x602A, 0x259E,
	0x6F12, 0x000B,
	0x602A, 0x25A4,
	0x6F12, 0x000B,
	0x602A, 0x25C2,
	0x6F12, 0x000B,
	0x602A, 0x25C8,
	0x6F12, 0x000B,
	0x602A, 0x25FE,
	0x6F12, 0x0018,
	0x602A, 0x262E,
	0x6F12, 0x0000,
	0x602A, 0x2736,
	0x6F12, 0x000F,
	0x602A, 0x273C,
	0x6F12, 0x002D,
	0x602A, 0x2742,
	0x6F12, 0x0046,
	0x602A, 0x27E4,
	0x6F12, 0x000B,
	0x602A, 0x27F0,
	0x6F12, 0x002E,
	0x602A, 0x27F6,
	0x6F12, 0x003B,
	0x602A, 0x27FC,
	0x6F12, 0x0049,
	0x602A, 0x2802,
	0x6F12, 0x0056,
	0x602A, 0x2808,
	0x6F12, 0x0063,
	0x602A, 0x280E,
	0x6F12, 0x0071,
	0x602A, 0x2814,
	0x6F12, 0x007E,
	0x602A, 0x281A,
	0x6F12, 0x008B,
	0x602A, 0x2820,
	0x6F12, 0x0099,
	0x602A, 0x2826,
	0x6F12, 0x00A6,
	0x602A, 0x282C,
	0x6F12, 0x02F8,
	0x602A, 0x2832,
	0x6F12, 0x0305,
	0x602A, 0x2838,
	0x6F12, 0x0312,
	0x602A, 0x283E,
	0x6F12, 0x031F,
	0x602A, 0x2844,
	0x6F12, 0x032C,
	0x602A, 0x284A,
	0x6F12, 0x033A,
	0x602A, 0x2850,
	0x6F12, 0x0347,
	0x602A, 0x2856,
	0x6F12, 0x0354,
	0x602A, 0x285C,
	0x6F12, 0x0362,
	0x602A, 0x2862,
	0x6F12, 0x036F,
	0x602A, 0x2868,
	0x6F12, 0x037C,
	0x602A, 0x28BC,
	0x6F12, 0x002A,
	0x602A, 0x28E6,
	0x6F12, 0x00BE,
	0x602A, 0x28EC,
	0x6F12, 0x00BE,
	0x602A, 0x28F8,
	0x6F12, 0x0044,
	0x602A, 0x3798,
	0x6F12, 0x046D,
	0x602A, 0x25CE,
	0x6F12, 0x0048,
	0x602A, 0x25E0,
	0x6F12, 0x0006,
	0x602A, 0x2D1E,
	0x6F12, 0x8000,
	0x602A, 0x379E,
	0x6F12, 0x000D,
	0x602A, 0x2640,
	0x6F12, 0x0005,
	0x602A, 0x2532,
	0x6F12, 0x01E0,
	0x602A, 0x253E,
	0x6F12, 0x03C0,
	0x602A, 0x256E,
	0x6F12, 0x08C0,
	0x602A, 0x3994,
	0x6F12, 0x0507,
	0x602A, 0x399A,
	0x6F12, 0x0F04,
	0x602A, 0x39A0,
	0x6F12, 0x0404,
	0x602A, 0x39A6,
	0x6F12, 0x0B07,
	0x602A, 0x3970,
	0x6F12, 0x0300,
	0x602A, 0x258A,
	0x6F12, 0x0009,
	0x602A, 0x2596,
	0x6F12, 0x0010,
	0x602A, 0x25A2,
	0x6F12, 0x0007,
	0x602A, 0x25A8,
	0x6F12, 0x0007,
	0x602A, 0x25B4,
	0x6F12, 0x0002,
	0x602A, 0x25C0,
	0x6F12, 0x0006,
	0x602A, 0x25C6,
	0x6F12, 0x0007,
	0x602A, 0x25CC,
	0x6F12, 0x0007,
	0x602A, 0x25D2,
	0x6F12, 0x0030,
	0x602A, 0x25E4,
	0x6F12, 0x001E,
	0x602A, 0x25EA,
	0x6F12, 0x0002,
	0x602A, 0x25F0,
	0x6F12, 0x0006,
	0x602A, 0x25F6,
	0x6F12, 0x0008,
	0x602A, 0x25FC,
	0x6F12, 0x0002,
	0x602A, 0x2602,
	0x6F12, 0x0002,
	0x602A, 0x260E,
	0x6F12, 0x0002,
	0x602A, 0x2614,
	0x6F12, 0x0015,
	0x602A, 0x261A,
	0x6F12, 0x0036,
	0x602A, 0x2620,
	0x6F12, 0x002F,
	0x602A, 0x2626,
	0x6F12, 0x0035,
	0x602A, 0x2632,
	0x6F12, 0x0006,
	0x602A, 0x264A,
	0x6F12, 0x000E,
	0x602A, 0x2650,
	0x6F12, 0x000E,
	0x602A, 0x273A,
	0x6F12, 0x000D,
	0x602A, 0x2740,
	0x6F12, 0x0033,
	0x602A, 0x2746,
	0x6F12, 0x0044,
	0x602A, 0x27E8,
	0x6F12, 0x0009,
	0x602A, 0x27EE,
	0x6F12, 0x0021,
	0x602A, 0x27F4,
	0x6F12, 0x002C,
	0x602A, 0x27FA,
	0x6F12, 0x0037,
	0x602A, 0x2800,
	0x6F12, 0x0042,
	0x602A, 0x2806,
	0x6F12, 0x004D,
	0x602A, 0x280C,
	0x6F12, 0x0058,
	0x602A, 0x2812,
	0x6F12, 0x009D,
	0x602A, 0x2818,
	0x6F12, 0x00A8,
	0x602A, 0x281E,
	0x6F12, 0x00B3,
	0x602A, 0x2824,
	0x6F12, 0x00BE,
	0x602A, 0x282A,
	0x6F12, 0x00C9,
	0x602A, 0x2830,
	0x6F12, 0x01B5,
	0x602A, 0x2836,
	0x6F12, 0x01C0,
	0x602A, 0x283C,
	0x6F12, 0x01CB,
	0x602A, 0x2842,
	0x6F12, 0x01D6,
	0x602A, 0x2848,
	0x6F12, 0x01E1,
	0x602A, 0x284E,
	0x6F12, 0x02C1,
	0x602A, 0x2854,
	0x6F12, 0x02CC,
	0x602A, 0x285A,
	0x6F12, 0x02D7,
	0x602A, 0x2860,
	0x6F12, 0x02E2,
	0x602A, 0x2866,
	0x6F12, 0x02ED,
	0x602A, 0x286C,
	0x6F12, 0x02F8,
	0x602A, 0x28C0,
	0x6F12, 0x0014,
	0x602A, 0x28EA,
	0x6F12, 0x003C,
	0x602A, 0x28F0,
	0x6F12, 0x003C,
	0x602A, 0x28FC,
	0x6F12, 0x0063,
	0x602A, 0x379C,
	0x6F12, 0x0320,
	0x602A, 0x2D22,
	0x6F12, 0x0310,
	0x602A, 0x2536,
	0x6F12, 0x01A0,
	0x602A, 0x2542,
	0x6F12, 0x04C0,
	0x602A, 0x2572,
	0x6F12, 0x0880,
	0x602A, 0x3996,
	0x6F12, 0x0707,
	0x602A, 0x399C,
	0x6F12, 0x0404,
	0x602A, 0x39A2,
	0x6F12, 0x0404,
	0x602A, 0x39A8,
	0x6F12, 0x0707,
	0x602A, 0x39AC,
	0x6F12, 0x0002,
	0x6F12, 0x0202,
	0x602A, 0x39DA,
	0x6F12, 0x03A5,
	0x602A, 0x3B0E,
	0x6F12, 0x0101,
	0x602A, 0x3B0A,
	0x6F12, 0x0100,
	0x6F12, 0x06C0,
	0x602A, 0x3B98,
	0x6F12, 0x0064,
	0x602A, 0x3B96,
	0x6F12, 0x076C,
	0x602A, 0x3B94,
	0x6F12, 0x05A0,
	0x602A, 0x3B9C,
	0x6F12, 0x06E0,
	0x602A, 0x3B92,
	0x6F12, 0x06DE,
	0x602A, 0x3B9A,
	0x6F12, 0x076C,
	0x602A, 0x39D4,
	0x6F12, 0x19CC,
	0x602A, 0x0DDA,
	0x6F12, 0x0000,
	0x602A, 0x3B00,
	0x6F12, 0x7082,
	0x6F12, 0x0CCA,
	0x602A, 0x51A6,
	0x6F12, 0xFFC0,
	0x602A, 0x51E8,
	0x6F12, 0xFFC0,
	0x602A, 0x51AA,
	0x6F12, 0x0001,
	0x6F12, 0x0101,
	0x602A, 0x51D2,
	0x6F12, 0x3F2D,
	0x602A, 0x51D6,
	0x6F12, 0x2D3F,
	0x602A, 0x51B6,
	0x6F12, 0x02BC,
	0x602A, 0x51C6,
	0x6F12, 0x001F,
	0x6F12, 0x002D,
	0x6F12, 0x001F,
	0x6F12, 0x001F,
	0x6F12, 0x002D,
	0x6F12, 0x001F,
	0x602A, 0x51B0,
	0x6F12, 0x0000,
	0x6F12, 0x0240,
	0x6F12, 0x0000,
	0x602A, 0x51EC,
	0x6F12, 0x0001,
	0x6F12, 0x0101,
	0x602A, 0x5214,
	0x6F12, 0x3F2D,
	0x602A, 0x5218,
	0x6F12, 0x2D3F,
	0x602A, 0x51F8,
	0x6F12, 0x02BC,
	0x602A, 0x5208,
	0x6F12, 0x001F,
	0x6F12, 0x002D,
	0x6F12, 0x001F,
	0x6F12, 0x001F,
	0x6F12, 0x002D,
	0x6F12, 0x001F,
	0x602A, 0x51F2,
	0x6F12, 0x0000,
	0x6F12, 0x0240,
	0x6F12, 0x0000,
	0x602A, 0x0E64,
	0x6F12, 0x2500,
	0x602A, 0x0E78,
	0x6F12, 0x0000,
	0x6F12, 0x7C00,
	0x602A, 0x5770,
	0x6F12, 0x0000,
	0x6028, 0x4000,
	0x010C, 0x0000,
	0x6214, 0xF9F0,
	0x6218, 0xF9F0,
	0x6028, 0x4000,
	0x0100, 0x0100,
};

static kal_uint16 addr_data_pair_normal_video[] = {
	0x6028, 0x4000,
	0x6214, 0xF970,
	0x6218, 0xF150,
	0x0344, 0x0000,
	0x0346, 0x0000,
	0x0348, 0x1F7F,
	0x034A, 0x0BCF,
	0x034C, 0x0FC0,
	0x034E, 0x0BD0,
	0x0350, 0x0000,
	0x0352, 0x0000,
	0x0900, 0x0011,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0386, 0x0001,
	0x0404, 0x1000,
	0x0402, 0x1010,
	0x0114, 0x0301,
	0x0110, 0x1002,
	0x0136, 0x1800,
	0x0304, 0x0004,
	0x0306, 0x00AF,
	0x0302, 0x0001,
	0x0300, 0x0005,
	0x030C, 0x0000,
	0x030E, 0x0003,
	0x0310, 0x00A0,
	0x0312, 0x0000,
	0x030A, 0x0001,
	0x0308, 0x0008,
	0x0342, 0x2280,
	0x0340, 0x0C62,
	0x0202, 0x0100,
	0x021E, 0x0000,
	0x0C00, 0x0000,
	0x0C02, 0x0000,
	0x0C04, 0x0000,
	0x0C06, 0x0000,
	0x2B94, 0x0000,
	0x2B98, 0x0000,
	0x0B02, 0x0203,
	0x0B80, 0x0100,
	0x0B84, 0x0201,
	0x0B04, 0x0001,
	0x0902, 0x0000,
	0x0118, 0x0102,
	0x6028, 0x2000,
	0x602A, 0x0EF0,
	0x6F12, 0x0101,
	0x602A, 0x0F12,
	0x6F12, 0xFFFF,
	0x602A, 0x39BA,
	0x6F12, 0x2848,
	0x6F12, 0x5304,
	0x602A, 0x3982,
	0x6F12, 0x0F0D,
	0x602A, 0x3988,
	0x6F12, 0x2E1E,
	0x602A, 0x398E,
	0x6F12, 0x2E2E,
	0x602A, 0x39BE,
	0x6F12, 0x0808,
	0x602A, 0x3984,
	0x6F12, 0x0D0B,
	0x602A, 0x398A,
	0x6F12, 0x1E1E,
	0x602A, 0x3990,
	0x6F12, 0x2E2E,
	0x602A, 0x2586,
	0x6F12, 0x0002,
	0x602A, 0x2592,
	0x6F12, 0x0004,
	0x602A, 0x259E,
	0x6F12, 0x000B,
	0x602A, 0x25A4,
	0x6F12, 0x000B,
	0x602A, 0x25C2,
	0x6F12, 0x000B,
	0x602A, 0x25C8,
	0x6F12, 0x000B,
	0x602A, 0x25FE,
	0x6F12, 0x0018,
	0x602A, 0x262E,
	0x6F12, 0x0000,
	0x602A, 0x2736,
	0x6F12, 0x000F,
	0x602A, 0x273C,
	0x6F12, 0x002D,
	0x602A, 0x2742,
	0x6F12, 0x0046,
	0x602A, 0x27E4,
	0x6F12, 0x000B,
	0x602A, 0x27F0,
	0x6F12, 0x002E,
	0x602A, 0x27F6,
	0x6F12, 0x003B,
	0x602A, 0x27FC,
	0x6F12, 0x0049,
	0x602A, 0x2802,
	0x6F12, 0x0056,
	0x602A, 0x2808,
	0x6F12, 0x0063,
	0x602A, 0x280E,
	0x6F12, 0x0071,
	0x602A, 0x2814,
	0x6F12, 0x007E,
	0x602A, 0x281A,
	0x6F12, 0x008B,
	0x602A, 0x2820,
	0x6F12, 0x0099,
	0x602A, 0x2826,
	0x6F12, 0x00A6,
	0x602A, 0x282C,
	0x6F12, 0x02F8,
	0x602A, 0x2832,
	0x6F12, 0x0305,
	0x602A, 0x2838,
	0x6F12, 0x0312,
	0x602A, 0x283E,
	0x6F12, 0x031F,
	0x602A, 0x2844,
	0x6F12, 0x032C,
	0x602A, 0x284A,
	0x6F12, 0x033A,
	0x602A, 0x2850,
	0x6F12, 0x0347,
	0x602A, 0x2856,
	0x6F12, 0x0354,
	0x602A, 0x285C,
	0x6F12, 0x0362,
	0x602A, 0x2862,
	0x6F12, 0x036F,
	0x602A, 0x2868,
	0x6F12, 0x037C,
	0x602A, 0x28BC,
	0x6F12, 0x002A,
	0x602A, 0x28E6,
	0x6F12, 0x00BE,
	0x602A, 0x28EC,
	0x6F12, 0x00BE,
	0x602A, 0x28F8,
	0x6F12, 0x0044,
	0x602A, 0x3798,
	0x6F12, 0x046D,
	0x602A, 0x25CE,
	0x6F12, 0x0048,
	0x602A, 0x25E0,
	0x6F12, 0x0006,
	0x602A, 0x2D1E,
	0x6F12, 0x8000,
	0x602A, 0x379E,
	0x6F12, 0x000D,
	0x602A, 0x2640,
	0x6F12, 0x0005,
	0x602A, 0x2532,
	0x6F12, 0x01E0,
	0x602A, 0x253E,
	0x6F12, 0x03C0,
	0x602A, 0x256E,
	0x6F12, 0x08C0,
	0x602A, 0x3994,
	0x6F12, 0x0507,
	0x602A, 0x399A,
	0x6F12, 0x0F04,
	0x602A, 0x39A0,
	0x6F12, 0x0404,
	0x602A, 0x39A6,
	0x6F12, 0x0B07,
	0x602A, 0x3970,
	0x6F12, 0x0300,
	0x602A, 0x258A,
	0x6F12, 0x0009,
	0x602A, 0x2596,
	0x6F12, 0x0010,
	0x602A, 0x25A2,
	0x6F12, 0x0007,
	0x602A, 0x25A8,
	0x6F12, 0x0007,
	0x602A, 0x25B4,
	0x6F12, 0x0002,
	0x602A, 0x25C0,
	0x6F12, 0x0006,
	0x602A, 0x25C6,
	0x6F12, 0x0007,
	0x602A, 0x25CC,
	0x6F12, 0x0007,
	0x602A, 0x25D2,
	0x6F12, 0x0030,
	0x602A, 0x25E4,
	0x6F12, 0x001E,
	0x602A, 0x25EA,
	0x6F12, 0x0002,
	0x602A, 0x25F0,
	0x6F12, 0x0006,
	0x602A, 0x25F6,
	0x6F12, 0x0008,
	0x602A, 0x25FC,
	0x6F12, 0x0002,
	0x602A, 0x2602,
	0x6F12, 0x0002,
	0x602A, 0x260E,
	0x6F12, 0x0002,
	0x602A, 0x2614,
	0x6F12, 0x0015,
	0x602A, 0x261A,
	0x6F12, 0x0036,
	0x602A, 0x2620,
	0x6F12, 0x002F,
	0x602A, 0x2626,
	0x6F12, 0x0035,
	0x602A, 0x2632,
	0x6F12, 0x0006,
	0x602A, 0x264A,
	0x6F12, 0x000E,
	0x602A, 0x2650,
	0x6F12, 0x000E,
	0x602A, 0x273A,
	0x6F12, 0x000D,
	0x602A, 0x2740,
	0x6F12, 0x0033,
	0x602A, 0x2746,
	0x6F12, 0x0044,
	0x602A, 0x27E8,
	0x6F12, 0x0009,
	0x602A, 0x27EE,
	0x6F12, 0x0021,
	0x602A, 0x27F4,
	0x6F12, 0x002C,
	0x602A, 0x27FA,
	0x6F12, 0x0037,
	0x602A, 0x2800,
	0x6F12, 0x0042,
	0x602A, 0x2806,
	0x6F12, 0x004D,
	0x602A, 0x280C,
	0x6F12, 0x0058,
	0x602A, 0x2812,
	0x6F12, 0x009D,
	0x602A, 0x2818,
	0x6F12, 0x00A8,
	0x602A, 0x281E,
	0x6F12, 0x00B3,
	0x602A, 0x2824,
	0x6F12, 0x00BE,
	0x602A, 0x282A,
	0x6F12, 0x00C9,
	0x602A, 0x2830,
	0x6F12, 0x01B5,
	0x602A, 0x2836,
	0x6F12, 0x01C0,
	0x602A, 0x283C,
	0x6F12, 0x01CB,
	0x602A, 0x2842,
	0x6F12, 0x01D6,
	0x602A, 0x2848,
	0x6F12, 0x01E1,
	0x602A, 0x284E,
	0x6F12, 0x02C1,
	0x602A, 0x2854,
	0x6F12, 0x02CC,
	0x602A, 0x285A,
	0x6F12, 0x02D7,
	0x602A, 0x2860,
	0x6F12, 0x02E2,
	0x602A, 0x2866,
	0x6F12, 0x02ED,
	0x602A, 0x286C,
	0x6F12, 0x02F8,
	0x602A, 0x28C0,
	0x6F12, 0x0014,
	0x602A, 0x28EA,
	0x6F12, 0x003C,
	0x602A, 0x28F0,
	0x6F12, 0x003C,
	0x602A, 0x28FC,
	0x6F12, 0x0063,
	0x602A, 0x379C,
	0x6F12, 0x0320,
	0x602A, 0x2D22,
	0x6F12, 0x0310,
	0x602A, 0x2536,
	0x6F12, 0x01A0,
	0x602A, 0x2542,
	0x6F12, 0x04C0,
	0x602A, 0x2572,
	0x6F12, 0x0880,
	0x602A, 0x3996,
	0x6F12, 0x0707,
	0x602A, 0x399C,
	0x6F12, 0x0404,
	0x602A, 0x39A2,
	0x6F12, 0x0404,
	0x602A, 0x39A8,
	0x6F12, 0x0707,
	0x602A, 0x39AC,
	0x6F12, 0x0002,
	0x6F12, 0x0202,
	0x602A, 0x39DA,
	0x6F12, 0x03A5,
	0x602A, 0x3B0E,
	0x6F12, 0x0101,
	0x602A, 0x3B0A,
	0x6F12, 0x0100,
	0x6F12, 0x06C0,
	0x602A, 0x3B98,
	0x6F12, 0x0064,
	0x602A, 0x3B96,
	0x6F12, 0x076C,
	0x602A, 0x3B94,
	0x6F12, 0x05A0,
	0x602A, 0x3B9C,
	0x6F12, 0x06E0,
	0x602A, 0x3B92,
	0x6F12, 0x06DE,
	0x602A, 0x3B9A,
	0x6F12, 0x076C,
	0x602A, 0x39D4,
	0x6F12, 0x19CC,
	0x602A, 0x0DDA,
	0x6F12, 0x0000,
	0x602A, 0x3B00,
	0x6F12, 0x7082,
	0x6F12, 0x0CCA,
	0x602A, 0x51A6,
	0x6F12, 0xFFC0,
	0x602A, 0x51E8,
	0x6F12, 0xFFC0,
	0x602A, 0x51AA,
	0x6F12, 0x0001,
	0x6F12, 0x0101,
	0x602A, 0x51D2,
	0x6F12, 0x3F2D,
	0x602A, 0x51D6,
	0x6F12, 0x2D3F,
	0x602A, 0x51B6,
	0x6F12, 0x02BC,
	0x602A, 0x51C6,
	0x6F12, 0x001F,
	0x6F12, 0x002D,
	0x6F12, 0x001F,
	0x6F12, 0x001F,
	0x6F12, 0x002D,
	0x6F12, 0x001F,
	0x602A, 0x51B0,
	0x6F12, 0x0000,
	0x6F12, 0x0240,
	0x6F12, 0x0000,
	0x602A, 0x51EC,
	0x6F12, 0x0001,
	0x6F12, 0x0101,
	0x602A, 0x5214,
	0x6F12, 0x3F2D,
	0x602A, 0x5218,
	0x6F12, 0x2D3F,
	0x602A, 0x51F8,
	0x6F12, 0x02BC,
	0x602A, 0x5208,
	0x6F12, 0x001F,
	0x6F12, 0x002D,
	0x6F12, 0x001F,
	0x6F12, 0x001F,
	0x6F12, 0x002D,
	0x6F12, 0x001F,
	0x602A, 0x51F2,
	0x6F12, 0x0000,
	0x6F12, 0x0240,
	0x6F12, 0x0000,
	0x602A, 0x0E64,
	0x6F12, 0x2500,
	0x602A, 0x0E78,
	0x6F12, 0x0000,
	0x6F12, 0x7C00,
	0x602A, 0x5770,
	0x6F12, 0x0000,
	0x6028, 0x4000,
	0x010C, 0x0000,
	0x6214, 0xF9F0,
	0x6218, 0xF9F0,
	0x6028, 0x4000,
	0x0100, 0x0100,
};

static kal_uint16 addr_data_pair_hs_video[] = {
	0x6028, 0x4000,
	0x6214, 0xF970,
	0x6218, 0xF150,
	0x0344, 0x00C0,
	0x0346, 0x01B0,
	0x0348, 0x1EBF,
	0x034A, 0x0A1F,
	0x034C, 0x0780,
	0x034E, 0x0438,
	0x0350, 0x0000,
	0x0352, 0x0000,
	0x0900, 0x0112,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0386, 0x0003,
	0x0404, 0x2000,
	0x0402, 0x1010,
	0x0114, 0x0301,
	0x0110, 0x1002,
	0x0136, 0x1800,
	0x0304, 0x0006,
	0x0306, 0x012C,
	0x0302, 0x0001,
	0x0300, 0x0005,
	0x030C, 0x0000,
	0x030E, 0x0003,
	0x0310, 0x00AC,
	0x0312, 0x0000,
	0x030A, 0x0001,
	0x0308, 0x0008,
	0x0342, 0x18F8,
	0x0340, 0x04E2,
	0x0202, 0x0100,
	0x021E, 0x0000,
	0x0C00, 0x0000,
	0x0C02, 0x0000,
	0x0C04, 0x0000,
	0x0C06, 0x0000,
	0x2B94, 0x0000,
	0x2B98, 0x0000,
	0x0B02, 0x0203,
	0x0B80, 0x0100,
	0x0B84, 0x0201,
	0x0B04, 0x0001,
	0x0902, 0x0001,
	0x0118, 0x0102,
	0x6028, 0x2000,
	0x602A, 0x0EF0,
	0x6F12, 0x0101,
	0x602A, 0x0F12,
	0x6F12, 0xFFFF,
	0x602A, 0x39BA,
	0x6F12, 0x2448,
	0x6F12, 0x5308,
	0x602A, 0x3982,
	0x6F12, 0x0D0D,
	0x602A, 0x3988,
	0x6F12, 0x2E1E,
	0x602A, 0x398E,
	0x6F12, 0x2E2E,
	0x602A, 0x39BE,
	0x6F12, 0x0806,
	0x602A, 0x3984,
	0x6F12, 0x0F0B,
	0x602A, 0x398A,
	0x6F12, 0x2E1E,
	0x602A, 0x3990,
	0x6F12, 0x2E2E,
	0x602A, 0x2586,
	0x6F12, 0x0009,
	0x602A, 0x2592,
	0x6F12, 0x0010,
	0x602A, 0x259E,
	0x6F12, 0x0007,
	0x602A, 0x25A4,
	0x6F12, 0x0007,
	0x602A, 0x25C2,
	0x6F12, 0x0007,
	0x602A, 0x25C8,
	0x6F12, 0x0007,
	0x602A, 0x25FE,
	0x6F12, 0x0002,
	0x602A, 0x262E,
	0x6F12, 0x0006,
	0x602A, 0x2736,
	0x6F12, 0x000D,
	0x602A, 0x273C,
	0x6F12, 0x0033,
	0x602A, 0x2742,
	0x6F12, 0x0044,
	0x602A, 0x27E4,
	0x6F12, 0x0009,
	0x602A, 0x27F0,
	0x6F12, 0x002C,
	0x602A, 0x27F6,
	0x6F12, 0x0037,
	0x602A, 0x27FC,
	0x6F12, 0x0042,
	0x602A, 0x2802,
	0x6F12, 0x004D,
	0x602A, 0x2808,
	0x6F12, 0x0058,
	0x602A, 0x280E,
	0x6F12, 0x0063,
	0x602A, 0x2814,
	0x6F12, 0x006E,
	0x602A, 0x281A,
	0x6F12, 0x0079,
	0x602A, 0x2820,
	0x6F12, 0x0084,
	0x602A, 0x2826,
	0x6F12, 0x008F,
	0x602A, 0x282C,
	0x6F12, 0x029E,
	0x602A, 0x2832,
	0x6F12, 0x02A9,
	0x602A, 0x2838,
	0x6F12, 0x02B4,
	0x602A, 0x283E,
	0x6F12, 0x02BF,
	0x602A, 0x2844,
	0x6F12, 0x02CA,
	0x602A, 0x284A,
	0x6F12, 0x02D5,
	0x602A, 0x2850,
	0x6F12, 0x02E0,
	0x602A, 0x2856,
	0x6F12, 0x02EB,
	0x602A, 0x285C,
	0x6F12, 0x02F6,
	0x602A, 0x2862,
	0x6F12, 0x0301,
	0x602A, 0x2868,
	0x6F12, 0x030C,
	0x602A, 0x28BC,
	0x6F12, 0x0015,
	0x602A, 0x28E6,
	0x6F12, 0x0083,
	0x602A, 0x28EC,
	0x6F12, 0x0083,
	0x602A, 0x28F8,
	0x6F12, 0x00B5,
	0x602A, 0x3798,
	0x6F12, 0x0447,
	0x602A, 0x25CE,
	0x6F12, 0x0030,
	0x602A, 0x25E0,
	0x6F12, 0x001E,
	0x602A, 0x2D1E,
	0x6F12, 0x0437,
	0x602A, 0x379E,
	0x6F12, 0x000D,
	0x602A, 0x2640,
	0x6F12, 0x0009,
	0x602A, 0x2532,
	0x6F12, 0x01A0,
	0x602A, 0x253E,
	0x6F12, 0x04C0,
	0x602A, 0x256E,
	0x6F12, 0x0880,
	0x602A, 0x3994,
	0x6F12, 0x0507,
	0x602A, 0x399A,
	0x6F12, 0x0F04,
	0x602A, 0x39A0,
	0x6F12, 0x0404,
	0x602A, 0x39A6,
	0x6F12, 0x0B07,
	0x602A, 0x3970,
	0x6F12, 0x0300,
	0x602A, 0x258A,
	0x6F12, 0x0002,
	0x602A, 0x2596,
	0x6F12, 0x0004,
	0x602A, 0x25A2,
	0x6F12, 0x000B,
	0x602A, 0x25A8,
	0x6F12, 0x000B,
	0x602A, 0x25B4,
	0x6F12, 0x0003,
	0x602A, 0x25C0,
	0x6F12, 0x000B,
	0x602A, 0x25C6,
	0x6F12, 0x000B,
	0x602A, 0x25CC,
	0x6F12, 0x000B,
	0x602A, 0x25D2,
	0x6F12, 0x0048,
	0x602A, 0x25E4,
	0x6F12, 0x0006,
	0x602A, 0x25EA,
	0x6F12, 0x0003,
	0x602A, 0x25F0,
	0x6F12, 0x0002,
	0x602A, 0x25F6,
	0x6F12, 0x0001,
	0x602A, 0x25FC,
	0x6F12, 0x0001,
	0x602A, 0x2602,
	0x6F12, 0x0020,
	0x602A, 0x260E,
	0x6F12, 0x0001,
	0x602A, 0x2614,
	0x6F12, 0x001E,
	0x602A, 0x261A,
	0x6F12, 0x0039,
	0x602A, 0x2620,
	0x6F12, 0x002B,
	0x602A, 0x2626,
	0x6F12, 0x0032,
	0x602A, 0x2632,
	0x6F12, 0x0000,
	0x602A, 0x264A,
	0x6F12, 0x0009,
	0x602A, 0x2650,
	0x6F12, 0x0009,
	0x602A, 0x273A,
	0x6F12, 0x000F,
	0x602A, 0x2740,
	0x6F12, 0x0020,
	0x602A, 0x2746,
	0x6F12, 0x0030,
	0x602A, 0x27E8,
	0x6F12, 0x000A,
	0x602A, 0x27EE,
	0x6F12, 0x0022,
	0x602A, 0x27F4,
	0x6F12, 0x002D,
	0x602A, 0x27FA,
	0x6F12, 0x0038,
	0x602A, 0x2800,
	0x6F12, 0x0043,
	0x602A, 0x2806,
	0x6F12, 0x004E,
	0x602A, 0x280C,
	0x6F12, 0x0059,
	0x602A, 0x2812,
	0x6F12, 0x0064,
	0x602A, 0x2818,
	0x6F12, 0x00C8,
	0x602A, 0x281E,
	0x6F12, 0x00D3,
	0x602A, 0x2824,
	0x6F12, 0x00DE,
	0x602A, 0x282A,
	0x6F12, 0x00E9,
	0x602A, 0x2830,
	0x6F12, 0x01DB,
	0x602A, 0x2836,
	0x6F12, 0x01E6,
	0x602A, 0x283C,
	0x6F12, 0x01F1,
	0x602A, 0x2842,
	0x6F12, 0x01FC,
	0x602A, 0x2848,
	0x6F12, 0x0207,
	0x602A, 0x284E,
	0x6F12, 0x0212,
	0x602A, 0x2854,
	0x6F12, 0x021D,
	0x602A, 0x285A,
	0x6F12, 0x0228,
	0x602A, 0x2860,
	0x6F12, 0x02E6,
	0x602A, 0x2866,
	0x6F12, 0x02F1,
	0x602A, 0x286C,
	0x6F12, 0x02FC,
	0x602A, 0x28C0,
	0x6F12, 0x001E,
	0x602A, 0x28EA,
	0x6F12, 0x005A,
	0x602A, 0x28F0,
	0x6F12, 0x005A,
	0x602A, 0x28FC,
	0x6F12, 0x002E,
	0x602A, 0x379C,
	0x6F12, 0x02EC,
	0x602A, 0x2D22,
	0x6F12, 0x8000,
	0x602A, 0x2536,
	0x6F12, 0x01E0,
	0x602A, 0x2542,
	0x6F12, 0x0220,
	0x602A, 0x2572,
	0x6F12, 0x0600,
	0x602A, 0x3996,
	0x6F12, 0x0707,
	0x602A, 0x399C,
	0x6F12, 0x0F04,
	0x602A, 0x39A2,
	0x6F12, 0x0404,
	0x602A, 0x39A8,
	0x6F12, 0x1307,
	0x602A, 0x39AC,
	0x6F12, 0x0002,
	0x6F12, 0x0102,
	0x602A, 0x39DA,
	0x6F12, 0x03AA,
	0x602A, 0x3B0E,
	0x6F12, 0x0100,
	0x602A, 0x3B0A,
	0x6F12, 0x0100,
	0x6F12, 0x06C0,
	0x602A, 0x3B98,
	0x6F12, 0x0064,
	0x602A, 0x3B96,
	0x6F12, 0x076C,
	0x602A, 0x3B94,
	0x6F12, 0x0400,
	0x602A, 0x3B9C,
	0x6F12, 0x0420,
	0x602A, 0x3B92,
	0x6F12, 0x041E,
	0x602A, 0x3B9A,
	0x6F12, 0x076C,
	0x602A, 0x39D4,
	0x6F12, 0x19C0,
	0x602A, 0x0DDA,
	0x6F12, 0x0000,
	0x602A, 0x3B00,
	0x6F12, 0x7082,
	0x6F12, 0x0CC2,
	0x602A, 0x51A6,
	0x6F12, 0xFFC0,
	0x602A, 0x51E8,
	0x6F12, 0xFFC0,
	0x602A, 0x51AA,
	0x6F12, 0x0001,
	0x6F12, 0x0101,
	0x602A, 0x51D2,
	0x6F12, 0x3F2D,
	0x602A, 0x51D6,
	0x6F12, 0x2D3F,
	0x602A, 0x51B6,
	0x6F12, 0x02BC,
	0x602A, 0x51C6,
	0x6F12, 0x001F,
	0x6F12, 0x002D,
	0x6F12, 0x001F,
	0x6F12, 0x001F,
	0x6F12, 0x002D,
	0x6F12, 0x001F,
	0x602A, 0x51B0,
	0x6F12, 0x0000,
	0x6F12, 0x0240,
	0x6F12, 0x0000,
	0x602A, 0x51EC,
	0x6F12, 0x0001,
	0x6F12, 0x0101,
	0x602A, 0x5214,
	0x6F12, 0x3F2D,
	0x602A, 0x5218,
	0x6F12, 0x2D3F,
	0x602A, 0x51F8,
	0x6F12, 0x02BC,
	0x602A, 0x5208,
	0x6F12, 0x001F,
	0x6F12, 0x002D,
	0x6F12, 0x001F,
	0x6F12, 0x001F,
	0x6F12, 0x002D,
	0x6F12, 0x001F,
	0x602A, 0x51F2,
	0x6F12, 0x0000,
	0x6F12, 0x0240,
	0x6F12, 0x0000,
	0x602A, 0x0E64,
	0x6F12, 0x2500,
	0x602A, 0x0E78,
	0x6F12, 0x0000,
	0x6F12, 0x7C00,
	0x602A, 0x5770,
	0x6F12, 0x0000,
	0x6028, 0x4000,
	0x010C, 0x0000,
	0x6214, 0xF9F0,
	0x6218, 0xF9F0,
	0x6028, 0x4000,
	0x0100, 0x0100,
};

static kal_uint16 addr_data_pair_custom3[] = {
	0x6028, 0x4000,
	0x6214, 0xF970,
	0x6218, 0xF150,
	0x0344, 0x00C0,
	0x0346, 0x01B0,
	0x0348, 0x1EBF,
	0x034A, 0x0A1F,
	0x034C, 0x0780,
	0x034E, 0x0438,
	0x0350, 0x0000,
	0x0352, 0x0000,
	0x0900, 0x0112,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0386, 0x0003,
	0x0404, 0x2000,
	0x0402, 0x1010,
	0x0114, 0x0301,
	0x0110, 0x1002,
	0x0136, 0x1800,
	0x0304, 0x0006,
	0x0306, 0x00F0,
	0x0302, 0x0001,
	0x0300, 0x0004,
	0x030C, 0x0000,
	0x030E, 0x0004,
	0x0310, 0x00A4,
	0x0312, 0x0001,
	0x030A, 0x0001,
	0x0308, 0x0008,
	0x0342, 0x34D8,
	0x0340, 0x049E,
	0x0202, 0x0100,
	0x021E, 0x0000,
	0x0B02, 0x0203,
	0x0B80, 0x0100,
	0x0C00, 0x0000,
	0x0C02, 0x0000,
	0x0C04, 0x0000,
	0x0C06, 0x0000,
	0x0B90, 0x0011,
	0x0B94, 0x0000,
	0x0B98, 0x0000,
	0x0B84, 0x0201,
	0x0B04, 0x0001,
	0xF462, 0x0017,
	0xF464, 0x0018,
	0xF46C, 0x0010,
	0xF466, 0x0018,
	0x0902, 0x0001,
	0x0BC0, 0x0040,
	0x0118, 0x0102,
	0x6028, 0x2000,
	0x602A, 0x0E74,
	0x6F12, 0x0003,
	0x602A, 0x0EF0,
	0x6F12, 0x0101,
	0x602A, 0x0F4A,
	0x6F12, 0x0003,
	0x6F12, 0x0005,
	0x602A, 0x0E64,
	0x6F12, 0x2500,
	0x602A, 0x398A,
	0x6F12, 0x1E1E,
	0x602A, 0x3984,
	0x6F12, 0x0D0B,
	0x602A, 0x39BE,
	0x6F12, 0x0808,
	0x602A, 0x39BC,
	0x6F12, 0x4404,
	0x602A, 0x3982,
	0x6F12, 0x0F0D,
	0x602A, 0x3988,
	0x6F12, 0x2E1E,
	0x602A, 0x39A8,
	0x6F12, 0x0707,
	0x602A, 0x399C,
	0x6F12, 0x0404,
	0x602A, 0x3996,
	0x6F12, 0x0707,
	0x602A, 0x3B98,
	0x6F12, 0x0064,
	0x602A, 0x3B96,
	0x6F12, 0x076C,
	0x602A, 0x3B94,
	0x6F12, 0x05A0,
	0x602A, 0x3B9C,
	0x6F12, 0x06E0,
	0x602A, 0x3B92,
	0x6F12, 0x06DE,
	0x602A, 0x3B9A,
	0x6F12, 0x076C,
	0x602A, 0x39D4,
	0x6F12, 0x19CC,
	0x602A, 0x0DDA,
	0x6F12, 0x0000,
	0x602A, 0x39BA,
	0x6F12, 0x2848,
	0x602A, 0x3B0A,
	0x6F12, 0x012F,
	0x6F12, 0x06C0,
	0x6F12, 0x0101,
	0x602A, 0x3B00,
	0x6F12, 0x7092,
	0x6F12, 0x0CCA,
	0x6F12, 0x2006,
	0x6F12, 0x0004,
	0x602A, 0x2572,
	0x6F12, 0x0880,
	0x602A, 0x2542,
	0x6F12, 0x04C0,
	0x602A, 0x2536,
	0x6F12, 0x01A0,
	0x602A, 0x379C,
	0x6F12, 0x0320,
	0x602A, 0x28FC,
	0x6F12, 0x0063,
	0x602A, 0x28F0,
	0x6F12, 0x003C,
	0x602A, 0x28EA,
	0x6F12, 0x003C,
	0x602A, 0x28E4,
	0x6F12, 0x0014,
	0x602A, 0x28C0,
	0x6F12, 0x0014,
	0x602A, 0x286C,
	0x6F12, 0x02F8,
	0x602A, 0x2866,
	0x6F12, 0x02ED,
	0x602A, 0x2860,
	0x6F12, 0x02E2,
	0x602A, 0x285A,
	0x6F12, 0x02D7,
	0x602A, 0x2854,
	0x6F12, 0x02CC,
	0x602A, 0x284E,
	0x6F12, 0x02C1,
	0x602A, 0x2848,
	0x6F12, 0x01E1,
	0x602A, 0x2842,
	0x6F12, 0x01D6,
	0x602A, 0x283C,
	0x6F12, 0x01CB,
	0x602A, 0x2836,
	0x6F12, 0x01C0,
	0x602A, 0x2830,
	0x6F12, 0x01B5,
	0x602A, 0x282A,
	0x6F12, 0x00C9,
	0x602A, 0x2824,
	0x6F12, 0x00BE,
	0x602A, 0x281E,
	0x6F12, 0x00B3,
	0x602A, 0x2818,
	0x6F12, 0x00A8,
	0x602A, 0x2812,
	0x6F12, 0x009D,
	0x602A, 0x280C,
	0x6F12, 0x0058,
	0x602A, 0x2806,
	0x6F12, 0x004D,
	0x602A, 0x2800,
	0x6F12, 0x0042,
	0x602A, 0x27FA,
	0x6F12, 0x0037,
	0x602A, 0x27F4,
	0x6F12, 0x002C,
	0x602A, 0x27EE,
	0x6F12, 0x0021,
	0x602A, 0x27E8,
	0x6F12, 0x0009,
	0x602A, 0x2746,
	0x6F12, 0x0044,
	0x602A, 0x2740,
	0x6F12, 0x0033,
	0x602A, 0x273A,
	0x6F12, 0x000D,
	0x602A, 0x2692,
	0x6F12, 0x0006,
	0x602A, 0x2650,
	0x6F12, 0x000E,
	0x602A, 0x264A,
	0x6F12, 0x000E,
	0x602A, 0x2632,
	0x6F12, 0x0006,
	0x602A, 0x2626,
	0x6F12, 0x0035,
	0x602A, 0x2620,
	0x6F12, 0x002F,
	0x602A, 0x261A,
	0x6F12, 0x0036,
	0x602A, 0x2614,
	0x6F12, 0x0015,
	0x602A, 0x260E,
	0x6F12, 0x0002,
	0x602A, 0x2602,
	0x6F12, 0x0002,
	0x602A, 0x25FC,
	0x6F12, 0x0002,
	0x602A, 0x25F6,
	0x6F12, 0x0008,
	0x602A, 0x25F0,
	0x6F12, 0x0006,
	0x602A, 0x25EA,
	0x6F12, 0x0002,
	0x602A, 0x25CC,
	0x6F12, 0x0007,
	0x602A, 0x25C6,
	0x6F12, 0x0007,
	0x602A, 0x25C0,
	0x6F12, 0x0006,
	0x602A, 0x25B4,
	0x6F12, 0x0002,
	0x602A, 0x25A8,
	0x6F12, 0x0007,
	0x602A, 0x25A2,
	0x6F12, 0x0007,
	0x602A, 0x259C,
	0x6F12, 0x0014,
	0x602A, 0x2590,
	0x6F12, 0x000F,
	0x602A, 0x2592,
	0x6F12, 0x000B,
	0x602A, 0x259E,
	0x6F12, 0x000B,
	0x602A, 0x25A4,
	0x6F12, 0x000B,
	0x602A, 0x25C2,
	0x6F12, 0x000B,
	0x602A, 0x25C8,
	0x6F12, 0x000B,
	0x602A, 0x25FE,
	0x6F12, 0x0018,
	0x602A, 0x262E,
	0x6F12, 0x0000,
	0x602A, 0x2736,
	0x6F12, 0x000F,
	0x602A, 0x273C,
	0x6F12, 0x002D,
	0x602A, 0x2742,
	0x6F12, 0x0046,
	0x602A, 0x282C,
	0x6F12, 0x02DD,
	0x602A, 0x2832,
	0x6F12, 0x02E8,
	0x602A, 0x2838,
	0x6F12, 0x02F3,
	0x602A, 0x283E,
	0x6F12, 0x02FE,
	0x602A, 0x2844,
	0x6F12, 0x0309,
	0x602A, 0x284A,
	0x6F12, 0x0314,
	0x602A, 0x2850,
	0x6F12, 0x031F,
	0x602A, 0x2856,
	0x6F12, 0x032A,
	0x602A, 0x285C,
	0x6F12, 0x0335,
	0x602A, 0x2862,
	0x6F12, 0x0340,
	0x602A, 0x2868,
	0x6F12, 0x034B,
	0x602A, 0x28BC,
	0x6F12, 0x002A,
	0x602A, 0x28E6,
	0x6F12, 0x00BE,
	0x602A, 0x28EC,
	0x6F12, 0x00BE,
	0x602A, 0x28F8,
	0x6F12, 0x0020,
	0x602A, 0x3798,
	0x6F12, 0x0449,
	0x602A, 0x2532,
	0x6F12, 0x01E0,
	0x602A, 0x253E,
	0x6F12, 0x03C0,
	0x602A, 0x256E,
	0x6F12, 0x08C0,
	0x602A, 0x3994,
	0x6F12, 0x0507,
	0x602A, 0x399A,
	0x6F12, 0x0F04,
	0x602A, 0x39A6,
	0x6F12, 0x0B07,
	0x602A, 0x39AC,
	0x6F12, 0x0002,
	0x602A, 0x40F0,
	0x6F12, 0x0100,
	0x602A, 0x0D8C,
	0x6F12, 0x0004,
	0x602A, 0x51A6,
	0x6F12, 0xFFC0,
	0x602A, 0x51E8,
	0x6F12, 0xFFC0,
	0x602A, 0x51AA,
	0x6F12, 0x0001,
	0x6F12, 0x0101,
	0x6F12, 0x0109,
	0x602A, 0x51D2,
	0x6F12, 0x3F2D,
	0x6F12, 0x3F3F,
	0x6F12, 0x2D3F,
	0x602A, 0x51B6,
	0x6F12, 0x02BC,
	0x602A, 0x51C6,
	0x6F12, 0x001F,
	0x6F12, 0x002D,
	0x6F12, 0x001F,
	0x6F12, 0x001F,
	0x6F12, 0x002D,
	0x6F12, 0x001F,
	0x602A, 0x51D8,
	0x6F12, 0x0FFF,
	0x6F12, 0x0FFF,
	0x6F12, 0x0FFF,
	0x6F12, 0x0FFF,
	0x6F12, 0x0FFF,
	0x6F12, 0x0FFF,
	0x602A, 0x51B0,
	0x6F12, 0x0000,
	0x6F12, 0x0240,
	0x6F12, 0x0000,
	0x602A, 0x51EC,
	0x6F12, 0x0001,
	0x6F12, 0x0101,
	0x6F12, 0x0109,
	0x602A, 0x5214,
	0x6F12, 0x3F2D,
	0x6F12, 0x3F3F,
	0x6F12, 0x2D3F,
	0x602A, 0x51F8,
	0x6F12, 0x02BC,
	0x602A, 0x5208,
	0x6F12, 0x001F,
	0x6F12, 0x002D,
	0x6F12, 0x001F,
	0x6F12, 0x001F,
	0x6F12, 0x002D,
	0x6F12, 0x001F,
	0x602A, 0x521A,
	0x6F12, 0x0FFF,
	0x6F12, 0x0FFF,
	0x6F12, 0x0FFF,
	0x6F12, 0x0FFF,
	0x6F12, 0x0FFF,
	0x6F12, 0x0FFF,
	0x602A, 0x51F2,
	0x6F12, 0x0000,
	0x6F12, 0x0240,
	0x6F12, 0x0000,
	0x602A, 0x25CE,
	0x6F12, 0x0048,
	0x602A, 0x25E0,
	0x6F12, 0x0006,
	0x602A, 0x25D2,
	0x6F12, 0x0030,
	0x602A, 0x25E4,
	0x6F12, 0x001E,
	0x602A, 0x2D1E,
	0x6F12, 0x8000,
	0x602A, 0x2D22,
	0x6F12, 0x0310,
	0x602A, 0x379E,
	0x6F12, 0x000D,
	0x602A, 0x39D0,
	0x6F12, 0x00A0,
	0x602A, 0x5770,
	0x6F12, 0x0000,
	0x602A, 0x5772,
	0x6F12, 0x0002,
	0x602A, 0x5766,
	0x6F12, 0x0100,
	0x602A, 0x0DDE,
	0x6F12, 0x0101,
	0x602A, 0x3E10,
	0x6F12, 0x1E3C,
	0x6028, 0x4000,
	0x010C, 0x0000,
	0x6214, 0xF9F0,
	0x6218, 0xF9F0,
};

static void sensor_init(void)
{
	/*Global setting*/
	pr_debug("start\n");
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0000, 0x000B);
	write_cmos_sensor(0x0000, 0x2C1A);
	write_cmos_sensor(0x6010, 0x0001);
	mdelay(3);
	table_write_cmos_sensor(addr_data_pair_init,
		   sizeof(addr_data_pair_init) / sizeof(kal_uint16));
}	/*	sensor_init  */


static void preview_setting(void)
{
	pr_debug("start\n");
	if (bIsLongExposure) {
		pr_debug("enter normal shutter.\n");
		write_cmos_sensor(0x0342, 0x4CB8);
		write_cmos_sensor(0x0340, 0x065C);
		write_cmos_sensor(0x0202, 0x0100);
		write_cmos_sensor(0x0702, 0x0000);
		bIsLongExposure = KAL_FALSE;
	}
	table_write_cmos_sensor(addr_data_pair_preview,
		   sizeof(addr_data_pair_preview) / sizeof(kal_uint16));
}	/*	preview_setting  */

// Pll Setting - VCO = 280Mhz
static void capture_setting(kal_uint16 currefps)
{
	pr_debug("start\n");
	if (bIsLongExposure) {
		pr_debug("enter normal shutter.\n");
		write_cmos_sensor(0x0342, 0x27B0);
		write_cmos_sensor(0x0340, 0x0C4C);
		write_cmos_sensor(0x0202, 0x0100);
		write_cmos_sensor(0x0702, 0x0000);
		bIsLongExposure = KAL_FALSE;
	}

	table_write_cmos_sensor(addr_data_pair_capture,
		   sizeof(addr_data_pair_capture) / sizeof(kal_uint16));
}


static void normal_video_setting(kal_uint16 currefps)
{
	pr_debug("start\n");
	table_write_cmos_sensor(addr_data_pair_normal_video,
		   sizeof(addr_data_pair_normal_video) / sizeof(kal_uint16));
}


static void hs_video_setting(void)
{
	pr_debug("start\n");
	table_write_cmos_sensor(addr_data_pair_hs_video,
		   sizeof(addr_data_pair_hs_video) / sizeof(kal_uint16));
}

static void slim_video_setting(void)
{
	pr_debug("E\n");
	preview_setting();
}

static void custom3_setting(void)
{
	pr_debug("start\n");
	table_write_cmos_sensor(addr_data_pair_custom3,
		   sizeof(addr_data_pair_custom3) / sizeof(kal_uint16));
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

	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = read_cmos_sensor(0x0000);
			pr_debug("read out sensor id 0x%x\n", *sensor_id);
			if (*sensor_id == imgsensor_info.sensor_id) {
				pr_debug(
					"i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			pr_debug("Read sensor id fail, id: 0x%x\n",
				imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/* if Sensor ID is not correct,
		 * Must set *sensor_id to 0xFFFFFFFF
		 */
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
	kal_uint16 sensor_id = 0;

	pr_debug("MIPI 4LANE\n");

	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = read_cmos_sensor(0x0000);
			if (sensor_id == imgsensor_info.sensor_id) {
				pr_debug(
				"i2c write id: 0x%x, sensor id: 0x%x\n",
				imgsensor.i2c_write_id, sensor_id);
			break;
			}
			pr_debug("Read sensor id fail, id: 0x%x\n",
				imgsensor.i2c_write_id);
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
	imgsensor.shutter = 0x3D0;
	imgsensor.gain = 0x100;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_mode = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);



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
static kal_uint32 close(void)
{
	pr_debug("E\n");
	streaming_control(KAL_FALSE);

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
 * *image_window : address pointer of pixel numbers in one period of HSYNC
 * *sensor_config_data : address pointer of line numbers in one period of VSYNC
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
	pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	preview_setting();
	set_mirror_flip(imgsensor.mirror);

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
	pr_debug("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps != imgsensor_info.cap.max_framerate) {
		pr_debug(
		"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
		imgsensor.current_fps, imgsensor_info.cap.max_framerate / 10);
	}
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	 capture_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/* capture() */
static kal_uint32 normal_video(
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	normal_video_setting(imgsensor.current_fps);
	/*preview_setting();*/
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	slim_video	 */

static kal_uint32 Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);/*using caputre_setting*/
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	custom1   */

static kal_uint32 Custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	preview_setting();/*using preview setting*/
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	custom2   */

static kal_uint32 Custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	custom3_setting();/*using preview setting*/
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	custom3   */

static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
{
	pr_debug("E\n");
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

	return ERROR_NONE;
}	/*	get_resolution	*/

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
				MSDK_SENSOR_INFO_STRUCT *sensor_info,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;
	sensor_info->SensorResetActiveHigh = FALSE;
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
		imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
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
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->PDAF_Support = 5; /* PDAF_SUPPORT_CAMSV_DUALPD*/
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  /* 0 is default 1x*/
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x*/
	sensor_info->SensorPacketECCOrder = 1;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX =
			imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX =
			imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.cap.starty;

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
	default:
		sensor_info->SensorGrabStartX =
			imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}	/*	get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("scenario_id = %d\n", scenario_id);
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
		case MSDK_SCENARIO_ID_CUSTOM1:
			Custom1(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			Custom2(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			Custom3(image_window, sensor_config_data);
			break;
		default:
			pr_debug("Warning: wrong ScenarioId setting");
			preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
		}

	return ERROR_NONE;
}	/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	pr_debug("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate*/
	if (framerate == 0) {
		/* Dynamic frame rate*/
		return ERROR_NONE;
	}
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
	pr_debug("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) /*enable auto flicker*/
		imgsensor.autoflicker_en = KAL_TRUE;
	else /*Cancel Auto flick*/
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	pr_debug("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length =
			imgsensor_info.pre.pclk /
			framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk /
			framerate * 10 / imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.normal_video.framelength) ?
		  (frame_length - imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length =
		 imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate) {
			pr_debug(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			framerate, imgsensor_info.cap.max_framerate / 10);
		}
		frame_length = imgsensor_info.cap.pclk /
			framerate * 10 / imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.cap.framelength) ?
			(frame_length - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.cap.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length =
			imgsensor_info.hs_video.pclk /
			framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.hs_video.framelength) ?
		  (frame_length - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length =
		  imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk /
			framerate * 10 / imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.slim_video.framelength) ?
		  (frame_length - imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length =
		  imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk /
			framerate * 10 / imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom1.framelength) ?
			(frame_length - imgsensor_info.custom1.framelength) : 0;
		imgsensor.frame_length =
		  imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length =
			imgsensor_info.custom2.pclk /
			framerate * 10 / imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom2.framelength) ?
			(frame_length - imgsensor_info.custom2.framelength) : 0;
		imgsensor.frame_length =
		  imgsensor_info.custom2.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		frame_length = imgsensor_info.custom3.pclk /
			framerate * 10 / imgsensor_info.custom3.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom3.framelength) ?
			(frame_length - imgsensor_info.custom3.framelength) : 0;
		imgsensor.frame_length =
		  imgsensor_info.custom3.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:  /*coding with  preview scenario by default*/
		frame_length = imgsensor_info.pre.pclk /
			framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		pr_debug("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	pr_debug("scenario_id = %d\n", scenario_id);

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
	case MSDK_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		*framerate = imgsensor_info.custom3.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	pr_debug("enable: %d\n", enable);

	if (enable) {
	/* 0x5E00[8]: 1 enable,  0 disable*/
	/* 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK*/
		write_cmos_sensor(0x0600, 0x0002);
	} else {
	/* 0x5E00[8]: 1 enable,  0 disable*/
	/* 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK*/
		write_cmos_sensor(0x0600, 0x0000);
	}
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
	unsigned long long *feature_data = (unsigned long long *) feature_para;

	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;


	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
			(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	pr_debug("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
		break;
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
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
			= (imgsensor_info.hs_video.framelength << 16)
				+ imgsensor_info.slim_video.linelength;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
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
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		/*night_mode((BOOL) *feature_data);*/
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
		/* get the lens driver ID from EEPROM or
		 * just return LENS_DRIVER_ID_DO_NOT_CARE
		 */
		/* if EEPROM does not exist in camera module.*/
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
		set_auto_flicker_mode(
			(BOOL)(*feature_data_16), *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)(*feature_data),
			*(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)(*feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	/* case SENSOR_FEATURE_GET_PDAF_DATA:
	 * pr_debug("SENSOR_FEATURE_GET_PDAF_DATA\n");
	 * read_2L9_eeprom((kal_uint16 )(*feature_data),
	 * (char*)(uintptr_t)(*(feature_data+1)),
	 * (kal_uint32)(*(feature_data+2)));
	 * break;
	 */

	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		/*for factory mode auto testing*/
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		spin_lock(&imgsensor_drv_lock);
			imgsensor.current_fps = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
		*feature_data_32);
	wininfo =
	  (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data + 1));

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
	case SENSOR_FEATURE_GET_PDAF_INFO:
		pr_debug("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%lld\n",
			*feature_data);
		PDAFinfo =
		(struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data + 1));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		pr_debug("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%lld\n",
				*feature_data);
		/*PDAF capacity enable or not, 2p8 only full size support PDAF*/
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		/* video & capture use same setting*/
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;

		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;

		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
/*
 * case SENSOR_FEATURE_SET_PDAF:
 * pr_debug("PDAF mode :%d\n", *feature_data_16);
 * imgsensor.pdaf_mode= *feature_data_16;
 * break;
 */
	case SENSOR_FEATURE_GET_VC_INFO:
	pr_debug("SENSOR_FEATURE_GET_VC_INFO %d\n", (UINT16)*feature_data);
	pvcinfo =
	(struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data + 1));
		switch (*feature_data_32) {

		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CUSTOM1:
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
		case MSDK_SCENARIO_ID_CUSTOM2:
		default:
		    memcpy((void *)pvcinfo,
			(void *) &SENSOR_VC_INFO[0],
			sizeof(struct SENSOR_VC_INFO_STRUCT));
		break;
		}
		break;

	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16)(*feature_data),
			(UINT16)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_AE_FRAME_MODE_FOR_LE:
	pr_debug("SENSOR_FEATURE_GET_AE_FRAME_MODE_FOR_LE\n");
		memcpy(feature_return_para_32,
		&imgsensor.ae_frm_mode, sizeof(struct IMGSENSOR_AE_FRM_MODE));
		break;
	case SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE:
		pr_debug("SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE\n");
		*feature_return_para_32 =  imgsensor.current_ae_effective_frame;
		break;

	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	{
		kal_uint32 rate;

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			rate = imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			rate = imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			rate = imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			rate = imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			rate = imgsensor_info.pre.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			rate = imgsensor_info.custom3.mipi_pixel_rate;
			break;
		default:
			rate = 0;
			break;
		}
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
	}
		break;

	default:
		break;
	}

	return ERROR_NONE;
}	/*	feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};


UINT32 S5K2LQSX_MIPI_RAW_SensorInit(
	struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}	/*	S5K2LQSX_MIPI_RAW_SensorInit	*/
