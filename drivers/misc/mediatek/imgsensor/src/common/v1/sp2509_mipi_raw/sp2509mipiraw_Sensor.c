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

#include "sp2509mipiraw_Sensor.h"

/*********************Modify Following Strings for Debug*********************/
#define PFX "SP2509_camera_sensor"

#define LOG_1 cam_pr_debug("SP2509,MIPI 1LANE\n")
#define LOG_2 cam_pr_debug("preview 1600*1200@30fps,864Mbps/lane\n")
/***************************   Modify end    ********************************/

#define cam_pr_debug(format, args...) \
		pr_info("[%s] " format, __func__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

#define MIPI_SETTLEDELAY_AUTO     0
#define MIPI_SETTLEDELAY_MANNUAL  1
#define SETTLE_DELAY 85

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = SP2509_SENSOR_ID,

	.checksum_value = 0x3ca80b08,

	.pre = {
		.pclk = 36000000,
		.linelength = 947,
		.framelength = 1234,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,
		.mipi_pixel_rate = 72000000,
		.max_framerate = 300,
		},
	.cap = {
		.pclk = 36000000,
		.linelength = 947,
		.framelength = 1234,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,
		.mipi_pixel_rate = 72000000,
		.max_framerate = 300,
		},
	.cap1 = {
		.pclk = 36000000,
		.linelength = 947,
		.framelength = 1234,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,
		.mipi_pixel_rate = 72000000,
		.max_framerate = 300,
		},
	.normal_video = {
		.pclk = 36000000,
		.linelength = 947,
		.framelength = 1234,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,
		.mipi_pixel_rate = 72000000,
		.max_framerate = 300,
		},
	.hs_video = {
		.pclk = 36000000,
		.linelength = 947,
		.framelength = 1234,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,
		.mipi_pixel_rate = 72000000,
		.max_framerate = 600,
		},
	.slim_video = {
		.pclk = 36000000,
		.linelength = 947,
		.framelength = 1234,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,
		.mipi_pixel_rate = 72000000,
		.max_framerate = 300,
		},
	.custom1 = {
		.pclk = 36000000,
		.linelength = 947,
		.framelength = 1234,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,
		.mipi_pixel_rate = 72000000,
		.max_framerate = 300,
		},
	.custom2 = {
		.pclk = 36000000,
		.linelength = 947,
		.framelength = 1234,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,
		.mipi_pixel_rate = 72000000,
		.max_framerate = 300,
		},
	.custom3 = {
		.pclk = 36000000,
		.linelength = 947,
		.framelength = 1234,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,
		.mipi_pixel_rate = 72000000,
		.max_framerate = 300,
		},
	.custom4 = {
		.pclk = 36000000,
		.linelength = 947,
		.framelength = 1234,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,
		.mipi_pixel_rate = 72000000,
		.max_framerate = 300,
		},
	.custom5 = {
		.pclk = 36000000,
		.linelength = 947,
		.framelength = 1234,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,
		.mipi_pixel_rate = 72000000,
		.max_framerate = 300,
		},
	.margin = 0,
	.min_shutter = 7,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	/* support; 0,not support */
	.ihdr_le_firstline = 0,	/* le first ; 0, se first */
	.sensor_mode_num = 10,	/* support sensor mode num */

	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 5,
	.hs_video_delay_frame = 5,
	.slim_video_delay_frame = 5,
	.custom1_delay_frame = 2,
	.custom2_delay_frame = 2,
	.custom3_delay_frame = 2,
	.custom4_delay_frame = 2,
	.custom5_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_8MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_MANUAL,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_1_LANE,
	.i2c_addr_table = {0x7a, 0xff},
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x46e,	/* current shutter   // Danbo ?? */
	.gain = 0x40,		/* current gain     // Danbo ?? */
	.dummy_pixel = 272,	/* current dummypixel */
	.dummy_line = 32,	/* current dummyline */
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x7a,
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] = {
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000,
		1600, 1200, 0, 0, 1600, 1200},	/* Preview 2112*1558 */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000,
		1600, 1200, 0, 0, 1600, 1200},	/* capture 4206*3128 */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000,
		1600, 1200, 0, 0, 1600, 1200},	/* video */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000,
		1600, 1200, 0, 0, 1600, 1200},	/* hight speed video */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000,
		1600, 1200, 0, 0, 1600, 1200},	/* slim video */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000,
		1600, 1200, 0, 0, 1600, 1200},  /* Custom1 */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000,
		1600, 1200, 0, 0, 1600, 1200},	/* Custom2 */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000,
		1600, 1200, 0, 0, 1600, 1200},	/* Custom3 */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000,
		1600, 1200, 0, 0, 1600, 1200},	/* Custom4 */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000,
		1600, 1200, 0, 0, 1600, 1200},	/* Custom5 */
};


static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	char pu_send_cmd[1] = { (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 1, (u8 *) &get_byte, 1,
		imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[2] = { (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pu_send_cmd, 2, imgsensor.i2c_write_id);
}


static void set_dummy(void)
{
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x05, (imgsensor.dummy_line >> 8) & 0xFF);
	write_cmos_sensor(0x06, imgsensor.dummy_line & 0xFF);
	write_cmos_sensor(0x01, 0x01);
}				/*  set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	write_cmos_sensor(0xfd, 0x00);
	return ((read_cmos_sensor(0x02) << 8) | read_cmos_sensor(0x03));
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	cam_pr_debug("framerate = %d, min framelength should enable = %d\n",
		framerate,
		min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length =
		(frame_length > imgsensor.min_frame_length)
		? frame_length
		: imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length -
		imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line =
			imgsensor.frame_length -
			imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}

static void write_shutter(kal_uint16 shutter)
{
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x03, (shutter >> 8) & 0xFF);
	write_cmos_sensor(0x04, shutter & 0xFF);
	write_cmos_sensor(0x01, 0x01);
	cam_pr_debug("shutter =%d, framelength =%d\n",
		shutter,
		imgsensor.frame_length);
}				/*  write_shutter  */

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

	if (shutter < 7)
		shutter = 7;
	else if (shutter > 0xffff)
		shutter = 0xffff;
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}				/*  set_shutter */



#if 0
static kal_uint16 gain2reg(const kal_uint16 gain)
{
/* #if 0 */
	kal_uint16 reg_gain = 0x0000;

	reg_gain = ((gain / BASEGAIN) << 4) +
		((gain % BASEGAIN) * 16 / BASEGAIN);

	reg_gain = reg_gain & 0xFFFF;

	return (kal_uint16) reg_gain;

}
#endif



/*************************************************************************
 * FUNCTION
 *   set_gain
 *
 * DESCRIPTION
 *   This function is to set global gain to sensor.
 *
 * PARAMETERS
 *   iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *   the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint8 iReg;

	if (gain >= BASEGAIN && gain <= 15 * BASEGAIN) {
		iReg = 0x10 * gain / BASEGAIN;

		if (iReg <= 0x10) {
			write_cmos_sensor(0xfd, 0x01);
			write_cmos_sensor(0x24, 0x10);	/* 0x23 */
			write_cmos_sensor(0x01, 0x01);
			cam_pr_debug("SP2509MIPI_SetGain = 16");
		} else if (iReg >= 0xa0) {	/* gpw */
			write_cmos_sensor(0xfd, 0x01);
			write_cmos_sensor(0x24, 0xa0);
			write_cmos_sensor(0x01, 0x01);
			cam_pr_debug("SP2509MIPI_SetGain = 160");
		} else {
			write_cmos_sensor(0xfd, 0x01);
			write_cmos_sensor(0x24, (kal_uint8) iReg);
			write_cmos_sensor(0x01, 0x01);
			cam_pr_debug("SP2509MIPI_SetGain = %d", iReg);
		}
	} else
		cam_pr_debug("error gain setting");
	return gain;
}				/*  set_gain  */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se,
	kal_uint16 gain)
{
	/* kal_uint8 iRation; */
	/* kal_uint8 iReg; */
	cam_pr_debug("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
}

#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
	cam_pr_debug("image_mirror = %d\n", image_mirror);

	/********************************************************
	 *
	 *   0x0101 Sensor mirror flip
	 *
	 *   ISP and Sensor flip or mirror register bit should be the same!!
	 *
	 ********************************************************/

	/* kal_uint8  iTemp; */
	/* write_cmos_sensor(0xfd, 0x01); */
	/* iTemp = read_cmos_sensor(0x3f) & 0x03; */
	/* iTemp&= ~0x03; //Clear the mirror and flip bits. */

	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor(0x3f, 0x00);	/* Set normal */
		break;
	case IMAGE_H_MIRROR:
		/* write_cmos_sensor(0x3f, iTemp | 0x01); //Set mirror */
		write_cmos_sensor(0x3f, 0x01);
		break;
	case IMAGE_V_MIRROR:
		/* write_cmos_sensor(0x3f, iTemp | 0x02); //Set flip */
		write_cmos_sensor(0x3f, 0x02);
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor(0x3f, 0x03);	/* Set mirror and flip */
		break;
	default:
		cam_pr_debug("Error image_mirror setting\n");
	}

}
#endif

/*************************************************************************
 * FUNCTION
 *   night_mode
 *
 * DESCRIPTION
 *   This function night mode of sensor.
 *
 * PARAMETERS
 *   bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 *   None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void night_mode(kal_bool enable)
{
	/*No Need to implement this function*/
}				/*  night_mode  */

static void sensor_init(void)
{
	cam_pr_debug("Enter %s\n", __func__);
	/* update by xingdy, 20150309 */
	/* pll clk 60M */
	/* p1:0x2b=0xc4, p1:0x58=0x30, pixel bias 2u */
	/* add offset h'20 and  remove the digital gain */
	/* T_horizontal=1160, base is h'103, 21fps */
	/* add digital gain 1.3x ½â¾ö¸ßÁÁ·¢·ÛÎÊÌâ */
#if 0				/* 21fps, verified ok! */
	{
		write_cmos_sensor(0xfd, 0x00);
		write_cmos_sensor(0x2f, 0x04);	/* 0c */
		write_cmos_sensor(0x34, 0x00);
		write_cmos_sensor(0x35, 0x00);	/* 21 */
		write_cmos_sensor(0x30, 0x1d);
		write_cmos_sensor(0x33, 0x05);
		write_cmos_sensor(0xfd, 0x01);
		write_cmos_sensor(0x44, 0x00);
		write_cmos_sensor(0x2a, 0x4c);
		write_cmos_sensor(0x2b, 0x1e);
		write_cmos_sensor(0x2c, 0x60);
		write_cmos_sensor(0x25, 0x11);
		write_cmos_sensor(0x03, 0x03);
		write_cmos_sensor(0x04, 0xb3);
		write_cmos_sensor(0x09, 0x00);
		write_cmos_sensor(0x0a, 0x02);
		write_cmos_sensor(0x06, 0x0a);
		write_cmos_sensor(0x24, 0x60);
		write_cmos_sensor(0x3f, 0x00);
		write_cmos_sensor(0x01, 0x01);
		write_cmos_sensor(0xfb, 0x73);
		write_cmos_sensor(0xfd, 0x01);
		write_cmos_sensor(0x16, 0x04);
		write_cmos_sensor(0x1c, 0x09);
		write_cmos_sensor(0x21, 0x46);
		write_cmos_sensor(0x6c, 0x00);
		write_cmos_sensor(0x6b, 0x00);
		write_cmos_sensor(0x84, 0x00);
		write_cmos_sensor(0x85, 0x10);
		write_cmos_sensor(0x86, 0x10);
		write_cmos_sensor(0x12, 0x04);
		write_cmos_sensor(0x13, 0x40);
		write_cmos_sensor(0x11, 0x20);
		write_cmos_sensor(0x33, 0x40);
		write_cmos_sensor(0xd0, 0x03);
		write_cmos_sensor(0xd1, 0x01);
		write_cmos_sensor(0xd2, 0x00);
		write_cmos_sensor(0xd3, 0x01);
		write_cmos_sensor(0xd4, 0x20);
		write_cmos_sensor(0x51, 0x14);
		write_cmos_sensor(0x52, 0x10);
		write_cmos_sensor(0x55, 0x30);
		write_cmos_sensor(0x58, 0x10);
		write_cmos_sensor(0x71, 0x10);
		write_cmos_sensor(0x6f, 0x40);
		write_cmos_sensor(0x75, 0x60);
		write_cmos_sensor(0x76, 0x10);
		write_cmos_sensor(0x8a, 0x22);
		write_cmos_sensor(0x8b, 0x22);
		write_cmos_sensor(0x19, 0x71);
		write_cmos_sensor(0x29, 0x01);
		write_cmos_sensor(0xfd, 0x01);
		write_cmos_sensor(0x9d, 0xea);	/* 96 */
		write_cmos_sensor(0xa0, 0x05);	/* 05 */
		write_cmos_sensor(0xa1, 0x03);	/* 0x02 --->0x03//04 */
		write_cmos_sensor(0xad, 0x62);
		write_cmos_sensor(0xae, 0x00);
		write_cmos_sensor(0xaf, 0x85);
		write_cmos_sensor(0xb1, 0x01);
		write_cmos_sensor(0xac, 0x01);
		write_cmos_sensor(0xfd, 0x01);
		write_cmos_sensor(0xfc, 0x10);
		write_cmos_sensor(0xfe, 0x10);
		write_cmos_sensor(0xf9, 0x00);
		write_cmos_sensor(0xfa, 0x00);

		write_cmos_sensor(0x8e, 0x06);
		write_cmos_sensor(0x8f, 0x40);
		write_cmos_sensor(0x90, 0x04);
		write_cmos_sensor(0x91, 0xb0);
		write_cmos_sensor(0x45, 0x01);
		write_cmos_sensor(0x46, 0x00);
		write_cmos_sensor(0x47, 0x6c);
		write_cmos_sensor(0x48, 0x03);
		write_cmos_sensor(0x49, 0x8b);
		write_cmos_sensor(0x4a, 0x00);
		write_cmos_sensor(0x4b, 0x07);
		write_cmos_sensor(0x4c, 0x04);
		write_cmos_sensor(0x4d, 0xb7);

	}
#else				/* 30fps, testing... */
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x2f, 0x0c);
	write_cmos_sensor(0x34, 0x00);
	write_cmos_sensor(0x35, 0x21);	/* wu */
	write_cmos_sensor(0x30, 0x1d);
	write_cmos_sensor(0x33, 0x05);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x3f, 0x00);	/* sanford.lin mirror */
	write_cmos_sensor(0x44, 0x00);
	write_cmos_sensor(0x2a, 0x4c);	/* 9.29canshu 7c */
	write_cmos_sensor(0x2b, 0x1e);
	write_cmos_sensor(0x2c, 0x60);
	write_cmos_sensor(0x25, 0x11);
	write_cmos_sensor(0x03, 0x07);
	write_cmos_sensor(0x04, 0x85);
	write_cmos_sensor(0x09, 0x00);
	write_cmos_sensor(0x0a, 0x02);
	write_cmos_sensor(0x06, 0x0a);
	write_cmos_sensor(0x24, 0xf0);
	write_cmos_sensor(0x01, 0x01);
	write_cmos_sensor(0xfb, 0x73);	/* 9.29canshu 63 */
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x16, 0x04);
	write_cmos_sensor(0x1c, 0x09);
	write_cmos_sensor(0x21, 0x46);
	write_cmos_sensor(0x6c, 0x00);
	write_cmos_sensor(0x6b, 0x00);
	write_cmos_sensor(0x84, 0x00);
	write_cmos_sensor(0x85, 0x10);
	write_cmos_sensor(0x86, 0x10);
	write_cmos_sensor(0x12, 0x04);
	write_cmos_sensor(0x13, 0x40);
	write_cmos_sensor(0x11, 0x20);
	write_cmos_sensor(0x33, 0x40);
	write_cmos_sensor(0xd0, 0x03);
	write_cmos_sensor(0xd1, 0x01);
	write_cmos_sensor(0xd2, 0x00);
	write_cmos_sensor(0xd3, 0x01);
	write_cmos_sensor(0xd4, 0x20);
	write_cmos_sensor(0x51, 0x14);
	write_cmos_sensor(0x52, 0x10);
	write_cmos_sensor(0x55, 0x30);
	write_cmos_sensor(0x58, 0x10);
	write_cmos_sensor(0x71, 0x10);
	write_cmos_sensor(0x6f, 0x40);
	write_cmos_sensor(0x75, 0x60);
	write_cmos_sensor(0x76, 0x10);
	write_cmos_sensor(0x8a, 0x22);
	write_cmos_sensor(0x8b, 0x22);
	write_cmos_sensor(0x19, 0x71);
	write_cmos_sensor(0x29, 0x01);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x9d, 0xea);	/* 96 */
	write_cmos_sensor(0xa0, 0x29);	/* 09 */
	write_cmos_sensor(0xa1, 0x04);
	write_cmos_sensor(0xad, 0x62);	/*  */
	write_cmos_sensor(0xae, 0x00);	/*  */
	write_cmos_sensor(0xaf, 0x85);	/*  */
	write_cmos_sensor(0xb1, 0x01);
	write_cmos_sensor(0xac, 0x01);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xfc, 0x10);	/* 9.29wu */
	write_cmos_sensor(0xfe, 0x10);	/* 9.29wu */
	write_cmos_sensor(0xf9, 0x00);	/* 9.29wu */
	write_cmos_sensor(0xfa, 0x00);	/* 9.29wu */
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xac, 0x00);

#endif
	cam_pr_debug("Exit %s\n", __func__);
}				/*  sensor_init  */

static void preview_setting(void)
{				/* PreviewSetting */
	cam_pr_debug("E!\n");
}				/*  preview_setting  */

static void capture_setting(kal_uint16 currefps)
{
	cam_pr_debug("E!\n");
}

static void normal_video_setting(kal_uint16 currefps)
{				/* VideoFullSizeSetting */
	cam_pr_debug("E!\n");
}

static void hs_video_setting(void)
{				/* VideoHDSetting_60fps */
	cam_pr_debug("E!\n");
}

static void slim_video_setting(void)
{				/* VideoHDSetting */
	cam_pr_debug("E!\n");
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	cam_pr_debug("enable: %d\n", enable);

	if (enable) {
		write_cmos_sensor(0xfd, 0x01);
		write_cmos_sensor(0x0d, 0x01);
	} else {
		write_cmos_sensor(0xfd, 0x01);
		write_cmos_sensor(0x0d, 0x00);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


/*************************************************************************
 * FUNCTION
 *   get_imgsensor_id
 *
 * DESCRIPTION
 *   This function get the sensor ID
 *
 * PARAMETERS
 *   *sensorID : return the sensor ID
 *
 * RETURNS
 *   None
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

			if (*sensor_id == imgsensor_info.sensor_id) {
				cam_pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id,
					*sensor_id);
				return ERROR_NONE;
			}
			cam_pr_debug("Read sensor id fail:0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id,
				*sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

/*************************************************************************
 * FUNCTION
 *   open
 *
 * DESCRIPTION
 *   This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *   None
 *
 * RETURNS
 *   None
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

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			/* sensor_id = 0x2509; */
			if (sensor_id == imgsensor_info.sensor_id) {
				cam_pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			cam_pr_debug("Read sensor id fail, write id:0x%x id: 0x%x\n",
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
}				/*  open  */

/*************************************************************************
 * FUNCTION
 *   close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *   None
 *
 * RETURNS
 *   None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 close(void)
{
	cam_pr_debug("E\n");
	/*No Need to implement this function */
	return ERROR_NONE;
}				/*  close  */

/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *   This function start the sensor preview.
 *
 * PARAMETERS
 *   *image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *   None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("Enter %s\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();

	return ERROR_NONE;
}				/*  preview   */

/*************************************************************************
 * FUNCTION
 *   capture
 *
 * DESCRIPTION
 *   This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *   None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("Enter %s\n", __func__);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == 300) {
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	cam_pr_debug("Caputre fps:%d\n", imgsensor.current_fps);
	/* set_dummy(); */
	capture_setting(imgsensor.current_fps);
	return ERROR_NONE;
}				/* capture() */

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			       MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("Enter %s\n", __func__);

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
}				/*  normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("Enter %s\n", __func__);

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
}				/*  hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("Enter %s\n", __func__);

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
}				/*  slim_video   */

/*************************************************************************
 * FUNCTION
 * Custom1
 *
 * DESCRIPTION
 *   This function start the sensor Custom1.
 *
 * PARAMETERS
 *   *image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *   None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("Enter %s\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}				/*  Custom1   */

static kal_uint32 Custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("Enter %s\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}				/*  Custom2   */

static kal_uint32 Custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("Enter %s\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}				/*  Custom3   */

static kal_uint32 Custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("Enter %s\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	imgsensor.pclk = imgsensor_info.custom4.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.custom4.linelength;
	imgsensor.frame_length = imgsensor_info.custom4.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}				/*  Custom4   */


static kal_uint32 Custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("Enter %s\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	imgsensor.pclk = imgsensor_info.custom5.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.custom5.linelength;
	imgsensor.frame_length = imgsensor_info.custom5.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}				/*  Custom5   */

static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT * sensor_resolution)
{
	cam_pr_debug("E\n");
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

	sensor_resolution->SensorCustom4Width =
		imgsensor_info.custom4.grabwindow_width;
	sensor_resolution->SensorCustom4Height =
		imgsensor_info.custom4.grabwindow_height;

	sensor_resolution->SensorCustom5Width =
		imgsensor_info.custom5.grabwindow_width;
	sensor_resolution->SensorCustom5Height =
		imgsensor_info.custom5.grabwindow_height;

	return ERROR_NONE;
}				/*  get_resolution  */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 1;	/* not use */
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
	sensor_info->SlimVideoDelayFrame =
		imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame =
		imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame =
		imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame =
		imgsensor_info.custom3_delay_frame;
	sensor_info->Custom4DelayFrame =
		imgsensor_info.custom4_delay_frame;
	sensor_info->Custom5DelayFrame =
		imgsensor_info.custom5_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0;
	sensor_info->SensorDrivingCurrent =
		imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame =
		imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
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
	sensor_info->SensorClockDividCount = 5;
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;
	sensor_info->SensorPixelClockCount = 3;
	sensor_info->SensorDataLatchCount = 2;

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;
	sensor_info->SensorHightSampling = 0;
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
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom1.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom2.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom3.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom3.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom4.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom4.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom5.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom5.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

		break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}				/*  get_info  */

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("scenario_id = %d\n", scenario_id);
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
	case MSDK_SCENARIO_ID_CUSTOM4:
		Custom4(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		Custom5(image_window, sensor_config_data);
		break;
	default:
		cam_pr_debug("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}				/* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{
	cam_pr_debug("framerate = %d\n ", framerate);
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
	cam_pr_debug("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)		/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MUINT32 framerate)
{
	kal_uint32 frame_length;

	cam_pr_debug("scenario_id = %d, framerate = %d\n",
		scenario_id,
		framerate);

	if (framerate == 0)
		return ERROR_NONE;
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length =
			imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.pre.framelength)
			? (frame_length -
			imgsensor_info.pre.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		frame_length =
			imgsensor_info.normal_video.pclk / framerate * 10 /
			imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.normal_video.framelength)
			? (frame_length -
			imgsensor_info.normal_video.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps ==
			imgsensor_info.cap1.max_framerate) {
			frame_length =
				imgsensor_info.cap1.pclk / framerate * 10 /
				imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
				(frame_length >
				imgsensor_info.cap1.framelength)
				? (frame_length -
				imgsensor_info.cap1.framelength)
				: 0;
			imgsensor.frame_length =
				imgsensor_info.cap1.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps !=
				imgsensor_info.cap.max_framerate)
				cam_pr_debug("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
					framerate,
					imgsensor_info.cap.max_framerate / 10);
			frame_length =
				imgsensor_info.cap.pclk / framerate * 10 /
				imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
				(frame_length >
				imgsensor_info.cap.framelength)
				? (frame_length -
				imgsensor_info.cap.framelength)
				: 0;
			imgsensor.frame_length =
				imgsensor_info.cap.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length =
			imgsensor_info.hs_video.pclk / framerate * 10 /
			imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.hs_video.framelength)
			? (frame_length -
			imgsensor_info.hs_video.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.hs_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length =
			imgsensor_info.slim_video.pclk / framerate * 10 /
			imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.slim_video.framelength)
			? (frame_length -
			imgsensor_info.slim_video.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length =
		    imgsensor_info.custom1.pclk / framerate * 10 /
		    imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.custom1.framelength)
			? (frame_length -
			imgsensor_info.custom1.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length =
		    imgsensor_info.custom2.pclk / framerate * 10 /
		    imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.custom2.framelength)
			? (frame_length -
			imgsensor_info.custom2.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom2.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		frame_length =
		    imgsensor_info.custom3.pclk / framerate * 10 /
		    imgsensor_info.custom3.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.custom3.framelength)
			? (frame_length -
			imgsensor_info.custom3.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom3.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		frame_length =
		    imgsensor_info.custom4.pclk / framerate * 10 /
		    imgsensor_info.custom4.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.custom4.framelength)
			? (frame_length -
			imgsensor_info.custom4.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom4.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		frame_length =
		    imgsensor_info.custom5.pclk / framerate * 10 /
		    imgsensor_info.custom5.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.custom5.framelength)
			? (frame_length -
			imgsensor_info.custom5.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom5.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	default:	/* coding with  preview scenario by default */
		frame_length =
			imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.pre.framelength)
			? (frame_length -
			imgsensor_info.pre.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		cam_pr_debug("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MUINT32 *framerate)
{
	cam_pr_debug("scenario_id = %d\n", scenario_id);
	if (framerate == NULL) {
		cam_pr_debug("---- framerate is NULL ---- check here\n");
		return ERROR_NONE;
	}

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
	case MSDK_SCENARIO_ID_CUSTOM4:
		*framerate = imgsensor_info.custom4.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		*framerate = imgsensor_info.custom5.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}


static kal_uint32 streaming_control(kal_bool enable)
{
	cam_pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n",
		enable);
	if (enable) {
		/* MIPI// */
		write_cmos_sensor(0xfd, 0x01);
		write_cmos_sensor(0xac, 0x01);	/*Stream on */
	} else {
		/* MIPI// */
		write_cmos_sensor(0xfd, 0x01);
		write_cmos_sensor(0xac, 0x00);	/*Stream off */
	}

	mdelay(10);
	return ERROR_NONE;
}


static kal_uint32 feature_control(
	MSDK_SENSOR_FEATURE_ENUM feature_id,
	UINT8 *feature_para,
	UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
	    (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	if (feature_para == NULL) {
		cam_pr_debug(" ---- %s ---- error params1\n", __func__);
		return ERROR_NONE;
	}

	cam_pr_debug("feature_id = %d\n", feature_id);
	switch (feature_id) {
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
			sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData = read_cmos_sensor(
			sensor_reg_data->RegAddr);
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
		default:
			rate = imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = rate;
	}
		break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:
	{
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.cap.pclk /
			(imgsensor_info.cap.linelength - 80))*
			imgsensor_info.cap.grabwindow_width;

			break;

		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.normal_video.pclk /
			(imgsensor_info.normal_video.linelength - 80))*
			imgsensor_info.normal_video.grabwindow_width;

			break;

		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.hs_video.pclk /
			(imgsensor_info.hs_video.linelength - 80))*
			imgsensor_info.hs_video.grabwindow_width;

			break;

		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.slim_video.pclk /
			(imgsensor_info.slim_video.linelength - 80))*
			imgsensor_info.slim_video.grabwindow_width;

		break;

		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.pre.pclk /
			(imgsensor_info.pre.linelength - 80))*
			imgsensor_info.pre.grabwindow_width;

		break;
		}
	}
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
		set_auto_flicker_mode((BOOL) * feature_data_16,
			*(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *feature_data,
			*(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *(feature_data),
			(MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) * feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		cam_pr_debug("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16) *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		cam_pr_debug("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (BOOL) * feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		cam_pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32) *feature_data);

		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)
			(uintptr_t) (*(feature_data + 1));

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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		cam_pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		cam_pr_debug("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		cam_pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data, (UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		ihdr_write_shutter_gain((UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		break;
	default:
		break;
	}

	return ERROR_NONE;
}				/*  feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 SP2509_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*  SP2509_MIPI_RAW_SensorInit  */
