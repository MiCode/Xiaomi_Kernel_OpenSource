/*
 * Copyright (C) 2020 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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
 *     s5kdj1_mipiraw_Sensor.c
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

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "s5kjd1mipiraw_Sensor.h"

/*********************Modify Following Strings for Debug******************/
#define PFX "s5kjd1mipiraw_Sensor"
#define LOG_1 LOG_INF("S5KJD1,MIPI 4LANE\n")

/**********************   Modify end    **********************************/
#define NO_USE_3HDR 1

#define LOG_INF(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5KJD1_SENSOR_ID,
	.checksum_value = 0xfb225e4d,
	.pre = {
		.pclk = 1056000000,
		.linelength = 12108,
		.framelength = 2906,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3280,
		.grabwindow_height = 2460,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1200000000,
		.max_framerate = 300,
		},
	.cap = {
		.pclk = 1056000000,
		.linelength = 6784,
		.framelength = 5120,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 6560,
		.grabwindow_height = 4920,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1200000000,
		.max_framerate = 300,
		},
	.normal_video = {
		.pclk = 1056000000,
		.linelength = 14464,
		.framelength = 2432,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 150000000,
		.max_framerate = 300,
		},
	.hs_video = {
		.pclk = 105600000,
		.linelength = 4036,
		.framelength = 8718,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3280,
		.grabwindow_height = 2460,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 300000000,
		.max_framerate = 300,
		},
	.slim_video = {
		.pclk = 105600000,
		.linelength = 4036,
		.framelength = 8718,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3280,
		.grabwindow_height = 2460,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 300000000,
		.max_framerate = 300,
		},
	.custom1 = {
		.pclk = 1056000000,
		.linelength = 4036,
		.framelength = 8718,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3280,
		.grabwindow_height = 2460,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1200000000,
		.max_framerate = 300,
		},
	.custom2 = {
		.pclk = 1056000000,
		.linelength = 4804,
		.framelength = 7326,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 480000000,
		.max_framerate = 300,
		},

	.margin = 10,		/* sensor framelength & shutter margin */
	.min_shutter = 9,	/* min shutter */
	.min_gain = 64, /*1x gain*/
	.max_gain = 15872, /*16x gain*/
	.min_gain_iso = 100,
	.gain_step = 16,
	.gain_type = 0,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = HDR_SUPPORT_STAGGER,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 7,	/* support sensor mode num */

	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,
	.custom1_delay_frame = 2,
	.custom2_delay_frame = 2,
	.isp_driving_current = ISP_DRIVING_6MA,	/* mclk driving current */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gr,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,	/* mipi lane num */
	.i2c_addr_table = {0x20, 0xff},
	.i2c_speed = 400,
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x20,	/* record current sensor's i2c write id */
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[7] = {
	/* Preview */
	{6560, 4920, 0, 0, 6560, 4920, 6560, 4920,
	    0,   0, 3280, 2460, 0, 0, 3280, 2460},
	/* capture */
	{6560, 4920, 0, 0, 6560, 4920, 6560, 4920,
	    0,   0, 6560, 4920, 0, 0, 6560, 4920},
	/* video*/
	{6560, 4920, 0, 0, 6560, 4920, 3280, 2460,
	   0,   0, 1920, 1080, 0, 0, 1920, 1080},
	/* hight speed video */
	{6560, 4920, 0, 0, 6560, 4920, 3280, 2460,
	   0,   0, 3280, 2460, 0, 0, 3280, 2460},
	/* Slim_Video*/
	{6560, 4920, 0, 0, 6560, 4920, 3280, 2460,
	   0,   0, 3280, 2460, 0, 0, 3280, 2460},
	/* custom1 staggered HDR */
	{6560, 4920, 0, 0, 6560, 4920, 3280, 2460,
	   0,   0, 3280, 2460, 0, 0, 3280, 2460},
	/* custom2 normal video staggered HDR */
	{6560, 4920, 0, 0, 6560, 4920, 3280, 2460,
	   0,   0, 1920, 1080, 0, 0, 1920, 1080}
};

static struct SENSOR_VC_INFO2_STRUCT SENSOR_VC_INFO2[5] = {
	{ //Preview
		/* number of stream, legacy field, legacy field, */
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,
		{
		/* feature type, channel id, data type,*/
		/* width in pixel, height in pixel*/
			{VC_STAGGER_NE, 0x00, 0x2b, 0x0CD0, 0x99C},
		},
		1
	},
	{ //capture
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0x19A0, 0x1338},
		},
		1
	},
	{//video feature type
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0x0780, 0x0438},
		},
		1
	},
	{//custom1 3264*2460 stagger HDR 3exp
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0x0CD0, 0x99C},
			{VC_STAGGER_ME, 0x01, 0x2b, 0x0CD0, 0x99C},
			{VC_STAGGER_SE, 0x02, 0x2b, 0x0CD0, 0x99C},
	 },
		1
	},
	{//custom2 1080p stagger HDR 3exp
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0x0780, 0x438},
			{VC_STAGGER_ME, 0x01, 0x2b, 0x0780, 0x438},
			{VC_STAGGER_SE, 0x02, 0x2b, 0x0780, 0x438},
		},//custom2
		1
	},
};




static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *) &get_byte, 1,
		imgsensor.i2c_write_id);

	return get_byte;
}


static void write_cmos_sensor_8(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = { (char)(addr >> 8), (char)(addr & 0xFF),
		(char)(para & 0xFF) };
	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {
		(char)(addr >> 8), (char)(addr & 0xFF),
		(char)(para >> 8), (char)(para & 0xFF) };
	iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
	write_cmos_sensor(0x0342, imgsensor.line_length & 0xFFFF);
} /* set_dummy */

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d, min framelength should enable = %d\n",
		framerate,
		min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;

	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length =
		(frame_length > imgsensor.min_frame_length) ?
		frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length -
		imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length -
		imgsensor.min_frame_length;
	}

	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);

	set_dummy();
} /* set_max_framerate */

static void write_shutter(kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;

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
			imgsensor_info.margin)) ?
			(imgsensor_info.max_frame_length -
			imgsensor_info.margin) : shutter;

	// Framelength should be an even number
	shutter = (shutter >> 1) << 1;
	imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 /
			imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			// Extend frame length
			write_cmos_sensor(0x0340,
				imgsensor.frame_length & 0xFFFF);
		}
	} else {
		// Extend frame length
		write_cmos_sensor(0x0340,
			imgsensor.frame_length & 0xFFFF);
	}

	// Update Shutter
	write_cmos_sensor(0X0202, shutter & 0xFFFF);

	LOG_INF("Exit shutter =%d, framelength =%d\n",
		shutter,
		imgsensor.frame_length);
}

static void hdr_write_shutter(kal_uint16 LE, kal_uint16 SE)
{
	kal_uint16 realtime_fps = 0;
	kal_uint16 TE = 0;
	//kal_uint16 reg_gain;
	TE = LE + SE + imgsensor_info.margin;
	LOG_INF("write_shutter LE:0x%x, SE:0x%x\n", LE, SE);
	spin_lock(&imgsensor_drv_lock);
	if (imgsensor.min_frame_length - imgsensor_info.margin < TE)
		imgsensor.frame_length = TE + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	if (imgsensor_info.min_shutter > TE)
		TE = imgsensor_info.min_shutter;

	imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;

	if (imgsensor.autoflicker_en) {
		realtime_fps =
		imgsensor.pclk / imgsensor.line_length *
			10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			write_cmos_sensor_8(0x380e,
				imgsensor.frame_length >> 8);
			write_cmos_sensor_8(0x380f,
				imgsensor.frame_length & 0xFF);
		}
	} else {
		write_cmos_sensor_8(0x380e,
			imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x380f,
			imgsensor.frame_length & 0xFF);
	}
	//Short exposure
	//write_cmos_sensor(0x3511, (SE >> 8) & 0xFF);
	//write_cmos_sensor(0x3512, SE & 0xFF);
	//Long exposure
	write_cmos_sensor_8(0x3501, (LE >> 8) & 0xFF);
	write_cmos_sensor_8(0x3502, LE & 0xFF);

	LOG_INF("Exit shutter LE =%d SE =%d, framelength =%d\n",
		LE, SE,
		imgsensor.frame_length);
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
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	LOG_INF("set shutter = %ld\n", shutter);
	write_shutter(shutter);
}

static kal_uint16 gain2reg(const kal_uint16 gain)
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

static kal_uint32 set_gain(kal_uint32 gain)
{
	kal_uint32 reg_gain;

	if (gain < BASEGAIN || gain > 16 * BASEGAIN) {
		LOG_INF("bradder Error gain setting");

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
	write_cmos_sensor(0x0204, reg_gain);


	return gain;
}				/*    set_gain  */

static void hdr_write_gain(kal_uint16 lgain, kal_uint16 sgain)
{
#if 0
	kal_uint32 reg_lgain;
	kal_uint32 reg_sgain;
	kal_uint32 reg_d_lgain;
	kal_uint32 reg_d_sgain;
	kal_uint32 reg_a_gain;   // match to gain_table
	kal_uint32 reg_d_gain;

	LOG_INF("setting lgain=%d sgain=%d\n", lgain, sgain);

	reg_lgain = gain2reg(lgain);
	reg_sgain = gain2reg(sgain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_lgain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("setting reg_lgain = %d reg_sgain = %d\n ",
		reg_lgain, reg_sgain);

	if (reg_lgain > 0x7c0) {
		reg_d_lgain = reg_lgain*1024/1984;

		if (reg_d_lgain < 0x400) { // sensor 1xGain
			reg_d_lgain = 0x400;
		}
		if (reg_d_lgain > 0x3fff) { // sensor 16xGain
			reg_d_lgain = 0x3fff;
		}
		/* long exposure */
		write_cmos_sensor_8(0x0350a, (reg_d_lgain >> 8));
		write_cmos_sensor_8(0x0350b, (reg_d_lgain&0xff));
		write_cmos_sensor_8(0x03508, 0x07);
		write_cmos_sensor_8(0x03509, 0xc0);
		LOG_INF("setting reg_d_lgain = %d\n ", reg_d_lgain);
	} else {
		if (reg_lgain < 0x80) { // sensor 1xGain
			reg_lgain = 0x80;
		}
		if (reg_lgain > 0x7c0) {// sensor 15.5xGain
			reg_lgain = 0x7c0;
		}

		/* binary to find A_Gain */
		reg_a_gain = binary_find_AGain(AGain_table,
			reg_lgain,
			ARRAY_SIZE(AGain_table);

		/* in case of insufficient accurary, */
		/* use D_Gain supplement A_Gain */
		reg_d_gain = reg_lgain*1024/reg_a_gain;

		/* long gain */
		write_cmos_sensor_8(0x0350a, (reg_d_gain >> 8));
		write_cmos_sensor_8(0x0350b, (reg_d_gain&0xff));
		write_cmos_sensor_8(0x03508, (reg_a_gain >> 8));
		write_cmos_sensor_8(0x03509, (reg_a_gain&0xff));
		LOG_INF("setting LONG Gain reg_a_gain =%d  reg_d_gain =%d\n ",
			reg_a_gain, reg_d_gain);
	}

	if (reg_sgain > 0x7c0) {
		reg_d_sgain = reg_sgain*1024/1984;

		if (reg_d_sgain < 0x400) {// sensor 1xGain
			reg_d_sgain = 0x400;
		}
		if (reg_d_sgain > 0x3fff) {// sensor 16xGain
			reg_d_sgain = 0x3fff;
		}
		/* short gain */
		write_cmos_sensor_8(0x0350e, (reg_d_sgain >> 8));
		write_cmos_sensor_8(0x0350f, (reg_d_sgain&0xff));
		write_cmos_sensor_8(0x0350c, 0x07);
		write_cmos_sensor_8(0x0350d, 0xc0);
		LOG_INF("setting reg_d_sgain = %d\n ", reg_d_sgain);
	} else {
		if (reg_sgain < 0x80) { // sensor 1xGain
			reg_sgain = 0x80;
		}
		if (reg_sgain > 0x7c0) {// sensor 15.5xGain
			reg_sgain = 0x7c0;
		}

		/* binary to find A_Gain */
		reg_a_gain = binary_find_AGain(AGain_table,
			reg_sgain,
			ARRAY_SIZE(AGain_table);

		/* in case of insufficient accurary */
		/* use D_Gain supplement A_Gain */
		reg_d_gain = reg_sgain*1024/reg_a_gain;

		/* short exposure */
		write_cmos_sensor_8(0x0350e, (reg_d_gain >> 8));
		write_cmos_sensor_8(0x0350f, (reg_d_gain&0xff));
		write_cmos_sensor_8(0x0350c, (reg_a_gain >> 8));
		write_cmos_sensor_8(0x0350d, (reg_a_gain&0xff));
		LOG_INF("setting SHORT Gain reg_a_gain = %d reg_d_gain = %d\n ",
			reg_a_gain, reg_d_gain);
	}
#endif
	LOG_INF("Exit setting hdr_dual_gain\n");
}

static void ihdr_write_shutter_gain(kal_uint16 le,
	kal_uint16 se, kal_uint16 gain)
{
	/* not support HDR */
	/* LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n",le,se,gain); */
}

#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d\n", image_mirror);

	/********************************************************
	 *
	 *   0x3820[2] ISP Vertical flip
	 *   0x3820[1] Sensor Vertical flip
	 *
	 *   0x3821[2] ISP Horizontal mirror
	 *   0x3821[1] Sensor Horizontal mirror
	 *
	 *   ISP and Sensor flip or mirror register bit should be the same!!
	 *
	 ********************************************************/

	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor_8(0x3820,
			((read_cmos_sensor(0x3820) & 0xF9) | 0x00));
		write_cmos_sensor_8(0x3821,
			((read_cmos_sensor(0x3821) & 0xF9) | 0x06));
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor_8(0x3820,
			((read_cmos_sensor(0x3820) & 0xF9) | 0x00));
		write_cmos_sensor_8(0x3821,
			((read_cmos_sensor(0x3821) & 0xF9) | 0x00));
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor_8(0x3820,
			((read_cmos_sensor(0x3820) & 0xF9) | 0x06));
		write_cmos_sensor_8(0x3821,
			((read_cmos_sensor(0x3821) & 0xF9) | 0x06));
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor_8(0x3820,
			((read_cmos_sensor(0x3820) & 0xF9) | 0x06));
		write_cmos_sensor_8(0x3821,
			((read_cmos_sensor(0x3821) & 0xF9) | 0x00));
		break;
	default:
		LOG_INF("Error image_mirror setting\n");
	}

}
#endif

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
}				/*      night_mode      */

static void sensor_init(void)
{
	LOG_INF("E\n");
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x0000, 0x0001);
	write_cmos_sensor(0x0000, 0x3841);
	write_cmos_sensor(0x0102, 0x0001);
	mdelay(10);
	write_cmos_sensor(0x0A02, 0x01F4);
	write_cmos_sensor(0x6028, 0x2001);
	write_cmos_sensor(0x602A, 0x2064);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0348);
	write_cmos_sensor(0x6F12, 0x90F8);
	write_cmos_sensor(0x6F12, 0xCA0C);
	write_cmos_sensor(0x6F12, 0x0128);
	write_cmos_sensor(0x6F12, 0x01D0);
	write_cmos_sensor(0x6F12, 0x01F0);
	write_cmos_sensor(0x6F12, 0x61B9);
	write_cmos_sensor(0x6F12, 0x7047);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x1370);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF04F);
	write_cmos_sensor(0x6F12, 0x8BB0);
	write_cmos_sensor(0x6F12, 0x8346);
	write_cmos_sensor(0x6F12, 0x0027);
	write_cmos_sensor(0x6F12, 0x2021);
	write_cmos_sensor(0x6F12, 0x6846);
	write_cmos_sensor(0x6F12, 0x01F0);
	write_cmos_sensor(0x6F12, 0x2DF8);
	write_cmos_sensor(0x6F12, 0x0026);
	write_cmos_sensor(0x6F12, 0x3446);
	write_cmos_sensor(0x6F12, 0x0896);
	write_cmos_sensor(0x6F12, 0x01F0);
	write_cmos_sensor(0x6F12, 0x2DF8);
	write_cmos_sensor(0x6F12, 0x0546);
	write_cmos_sensor(0x6F12, 0x01F0);
	write_cmos_sensor(0x6F12, 0x2FF8);
	write_cmos_sensor(0x6F12, 0x8446);
	write_cmos_sensor(0x6F12, 0x0BF1);
	write_cmos_sensor(0x6F12, 0x8041);
	write_cmos_sensor(0x6F12, 0x8946);
	write_cmos_sensor(0x6F12, 0x3046);
	write_cmos_sensor(0x6F12, 0xF5B1);
	write_cmos_sensor(0x6F12, 0x6A46);
	write_cmos_sensor(0x6F12, 0x25FA);
	write_cmos_sensor(0x6F12, 0x00F1);
	write_cmos_sensor(0x6F12, 0xC907);
	write_cmos_sensor(0x6F12, 0x06D0);
	write_cmos_sensor(0x6F12, 0x761C);
	write_cmos_sensor(0x6F12, 0x411C);
	write_cmos_sensor(0x6F12, 0x42F8);
	write_cmos_sensor(0x6F12, 0x2410);
	write_cmos_sensor(0x6F12, 0x641C);
	write_cmos_sensor(0x6F12, 0xB6B2);
	write_cmos_sensor(0x6F12, 0xA4B2);
	write_cmos_sensor(0x6F12, 0x401C);
	write_cmos_sensor(0x6F12, 0x80B2);
	write_cmos_sensor(0x6F12, 0x0828);
	write_cmos_sensor(0x6F12, 0xF0D3);
	write_cmos_sensor(0x6F12, 0x701E);
	write_cmos_sensor(0x6F12, 0xA9F8);
	write_cmos_sensor(0x6F12, 0x9801);
	write_cmos_sensor(0x6F12, 0x13D3);
	write_cmos_sensor(0x6F12, 0xF007);
	write_cmos_sensor(0x6F12, 0x01D0);
	write_cmos_sensor(0x6F12, 0x761C);
	write_cmos_sensor(0x6F12, 0xB6B2);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x0221);
	write_cmos_sensor(0x6F12, 0x0991);
	write_cmos_sensor(0x6F12, 0x0146);
	write_cmos_sensor(0x6F12, 0x6FF0);
	write_cmos_sensor(0x6F12, 0xFF05);
	write_cmos_sensor(0x6F12, 0x2AE0);
	write_cmos_sensor(0x6F12, 0xA1F8);
	write_cmos_sensor(0x6F12, 0x9801);
	write_cmos_sensor(0x6F12, 0xA1F8);
	write_cmos_sensor(0x6F12, 0x6000);
	write_cmos_sensor(0x6F12, 0x0121);
	write_cmos_sensor(0x6F12, 0xA9F8);
	write_cmos_sensor(0x6F12, 0x9A11);
	write_cmos_sensor(0x6F12, 0xA9F8);
	write_cmos_sensor(0x6F12, 0x9C01);
	write_cmos_sensor(0x6F12, 0x0BB0);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0xF08F);
	write_cmos_sensor(0x6F12, 0x0BEB);
	write_cmos_sensor(0x6F12, 0x0102);
	write_cmos_sensor(0x6F12, 0x02F1);
	write_cmos_sensor(0x6F12, 0x8042);
	write_cmos_sensor(0x6F12, 0xB2F8);
	write_cmos_sensor(0x6F12, 0x6030);
	write_cmos_sensor(0x6F12, 0x5DF8);
	write_cmos_sensor(0x6F12, 0x2020);
	write_cmos_sensor(0x6F12, 0x6C46);
	write_cmos_sensor(0x6F12, 0x5AB1);
	write_cmos_sensor(0x6F12, 0x04EB);
	write_cmos_sensor(0x6F12, 0x8004);
	write_cmos_sensor(0x6F12, 0x6468);
	write_cmos_sensor(0x6F12, 0x3CB1);
	write_cmos_sensor(0x6F12, 0x05EB);
	write_cmos_sensor(0x6F12, 0x0424);
	write_cmos_sensor(0x6F12, 0x521E);
	write_cmos_sensor(0x6F12, 0x2243);
	write_cmos_sensor(0x6F12, 0x05E0);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0x0890);
	write_cmos_sensor(0x6F12, 0x0AE0);
	write_cmos_sensor(0x6F12, 0x03F0);
	write_cmos_sensor(0x6F12, 0x1F03);
	write_cmos_sensor(0x6F12, 0x521E);
	write_cmos_sensor(0x6F12, 0x9342);
	write_cmos_sensor(0x6F12, 0xF7D1);
	write_cmos_sensor(0x6F12, 0x891C);
	write_cmos_sensor(0x6F12, 0x801C);
	write_cmos_sensor(0x6F12, 0x89B2);
	write_cmos_sensor(0x6F12, 0x80B2);
	write_cmos_sensor(0x6F12, 0xB042);
	write_cmos_sensor(0x6F12, 0xDED3);
	write_cmos_sensor(0x6F12, 0xBCF1);
	write_cmos_sensor(0x6F12, 0xFF3F);
	write_cmos_sensor(0x6F12, 0x02D0);
	write_cmos_sensor(0x6F12, 0x0898);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x6F12, 0xD5D0);
	write_cmos_sensor(0x6F12, 0x0025);
	write_cmos_sensor(0x6F12, 0x2846);
	write_cmos_sensor(0x6F12, 0x28E0);
	write_cmos_sensor(0x6F12, 0x0024);
	write_cmos_sensor(0x6F12, 0x03E0);
	write_cmos_sensor(0x6F12, 0x14B1);
	write_cmos_sensor(0x6F12, 0x012C);
	write_cmos_sensor(0x6F12, 0x0BD0);
	write_cmos_sensor(0x6F12, 0x1EE0);
	write_cmos_sensor(0x6F12, 0x5DF8);
	write_cmos_sensor(0x6F12, 0x2510);
	write_cmos_sensor(0x6F12, 0xD9B1);
	write_cmos_sensor(0x6F12, 0xFF4B);
	write_cmos_sensor(0x6F12, 0x0BEB);
	write_cmos_sensor(0x6F12, 0x0800);
	write_cmos_sensor(0x6F12, 0x491E);
	write_cmos_sensor(0x6F12, 0x1952);
	write_cmos_sensor(0x6F12, 0x00F1);
	write_cmos_sensor(0x6F12, 0x600A);
	write_cmos_sensor(0x6F12, 0x0AE0);
	write_cmos_sensor(0x6F12, 0x5DF8);
	write_cmos_sensor(0x6F12, 0x2500);
	write_cmos_sensor(0x6F12, 0x80B1);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x6F12, 0xA0F5);
	write_cmos_sensor(0x6F12, 0x8070);
	write_cmos_sensor(0x6F12, 0x81B2);
	write_cmos_sensor(0x6F12, 0x5046);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xBDFF);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0xA840);
	write_cmos_sensor(0x6F12, 0x6D1C);
	write_cmos_sensor(0x6F12, 0x641C);
	write_cmos_sensor(0x6F12, 0x0743);
	write_cmos_sensor(0x6F12, 0xA4B2);
	write_cmos_sensor(0x6F12, 0xADB2);
	write_cmos_sensor(0x6F12, 0x022C);
	write_cmos_sensor(0x6F12, 0xDCD3);
	write_cmos_sensor(0x6F12, 0x0999);
	write_cmos_sensor(0x6F12, 0x08EB);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x6F12, 0x80B2);
	write_cmos_sensor(0x6F12, 0x8046);
	write_cmos_sensor(0x6F12, 0xB042);
	write_cmos_sensor(0x6F12, 0xD3D9);
	write_cmos_sensor(0x6F12, 0xA9F8);
	write_cmos_sensor(0x6F12, 0x9A71);
	write_cmos_sensor(0x6F12, 0x390C);
	write_cmos_sensor(0x6F12, 0xA9F8);
	write_cmos_sensor(0x6F12, 0x9C11);
	write_cmos_sensor(0x6F12, 0x0898);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x6F12, 0x9ED0);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0xA9F8);
	write_cmos_sensor(0x6F12, 0xF600);
	write_cmos_sensor(0x6F12, 0x9AE7);
	write_cmos_sensor(0x6F12, 0x10B5);
	write_cmos_sensor(0x6F12, 0xE848);
	write_cmos_sensor(0x6F12, 0x0C46);
	write_cmos_sensor(0x6F12, 0x2DED);
	write_cmos_sensor(0x6F12, 0x028B);
	write_cmos_sensor(0x6F12, 0x90ED);
	write_cmos_sensor(0x6F12, 0xF20A);
	write_cmos_sensor(0x6F12, 0xB8EE);
	write_cmos_sensor(0x6F12, 0x408A);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x9BFF);
	write_cmos_sensor(0x6F12, 0x10B9);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x9DFF);
	write_cmos_sensor(0x6F12, 0x00B1);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0x9FED);
	write_cmos_sensor(0x6F12, 0xE10A);
	write_cmos_sensor(0x6F12, 0xF4EE);
	write_cmos_sensor(0x6F12, 0x000A);
	write_cmos_sensor(0x6F12, 0x08EE);
	write_cmos_sensor(0x6F12, 0x200A);
	write_cmos_sensor(0x6F12, 0xBDEE);
	write_cmos_sensor(0x6F12, 0xC00A);
	write_cmos_sensor(0x6F12, 0x10EE);
	write_cmos_sensor(0x6F12, 0x101A);
	write_cmos_sensor(0x6F12, 0x8140);
	write_cmos_sensor(0x6F12, 0xA4F8);
	write_cmos_sensor(0x6F12, 0x6C10);
	write_cmos_sensor(0x6F12, 0xBDEC);
	write_cmos_sensor(0x6F12, 0x028B);
	write_cmos_sensor(0x6F12, 0x10BD);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF041);
	write_cmos_sensor(0x6F12, 0x0446);
	write_cmos_sensor(0x6F12, 0xD948);
	write_cmos_sensor(0x6F12, 0x0E46);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x8068);
	write_cmos_sensor(0x6F12, 0x87B2);
	write_cmos_sensor(0x6F12, 0x050C);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x2846);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x74FF);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x80FF);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x2846);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x6CFF);
	write_cmos_sensor(0x6F12, 0xD14D);
	write_cmos_sensor(0x6F12, 0xB5F8);
	write_cmos_sensor(0x6F12, 0x6C00);
	write_cmos_sensor(0x6F12, 0xC0B1);
	write_cmos_sensor(0x6F12, 0xA080);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x05EB);
	write_cmos_sensor(0x6F12, 0x4001);
	write_cmos_sensor(0x6F12, 0x04EB);
	write_cmos_sensor(0x6F12, 0x4002);
	write_cmos_sensor(0x6F12, 0xB1F8);
	write_cmos_sensor(0x6F12, 0x6010);
	write_cmos_sensor(0x6F12, 0x401C);
	write_cmos_sensor(0x6F12, 0xD180);
	write_cmos_sensor(0x6F12, 0x0628);
	write_cmos_sensor(0x6F12, 0xF5D3);
	write_cmos_sensor(0x6F12, 0xB5F8);
	write_cmos_sensor(0x6F12, 0x6E00);
	write_cmos_sensor(0x6F12, 0x6082);
	write_cmos_sensor(0x6F12, 0xB5F8);
	write_cmos_sensor(0x6F12, 0x7000);
	write_cmos_sensor(0x6F12, 0xA082);
	write_cmos_sensor(0x6F12, 0x7078);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x6F12, 0xB5F8);
	write_cmos_sensor(0x6F12, 0x7200);
	write_cmos_sensor(0x6F12, 0x04D0);
	write_cmos_sensor(0x6F12, 0x50B9);
	write_cmos_sensor(0x6F12, 0x05E0);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x61FF);
	write_cmos_sensor(0x6F12, 0xE3E7);
	write_cmos_sensor(0x6F12, 0x28B9);
	write_cmos_sensor(0x6F12, 0x317C);
	write_cmos_sensor(0x6F12, 0x00E0);
	write_cmos_sensor(0x6F12, 0x7179);
	write_cmos_sensor(0x6F12, 0x09B1);
	write_cmos_sensor(0x6F12, 0xE182);
	write_cmos_sensor(0x6F12, 0x00E0);
	write_cmos_sensor(0x6F12, 0xE082);
	write_cmos_sensor(0x6F12, 0x35F8);
	write_cmos_sensor(0x6F12, 0x740F);
	write_cmos_sensor(0x6F12, 0x2083);
	write_cmos_sensor(0x6F12, 0x6888);
	write_cmos_sensor(0x6F12, 0x6083);
	write_cmos_sensor(0x6F12, 0xA888);
	write_cmos_sensor(0x6F12, 0xA083);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0xF081);
	write_cmos_sensor(0x6F12, 0x70B5);
	write_cmos_sensor(0x6F12, 0xB748);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xC168);
	write_cmos_sensor(0x6F12, 0x0C0C);
	write_cmos_sensor(0x6F12, 0x8DB2);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x31FF);
	write_cmos_sensor(0x6F12, 0xB049);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x81F8);
	write_cmos_sensor(0x6F12, 0x5603);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x44FF);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0x7040);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x24BF);
	write_cmos_sensor(0x6F12, 0x70B5);
	write_cmos_sensor(0x6F12, 0xAB48);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x0169);
	write_cmos_sensor(0x6F12, 0x0C0C);
	write_cmos_sensor(0x6F12, 0x8DB2);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x1AFF);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x36FF);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x13FF);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x34FF);
	write_cmos_sensor(0x6F12, 0x10B9);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x36FF);
	write_cmos_sensor(0x6F12, 0x28B1);
	write_cmos_sensor(0x6F12, 0x0024);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x37FF);
	write_cmos_sensor(0x6F12, 0x0128);
	write_cmos_sensor(0x6F12, 0x02D0);
	write_cmos_sensor(0x6F12, 0x04E0);
	write_cmos_sensor(0x6F12, 0x0124);
	write_cmos_sensor(0x6F12, 0xF8E7);
	write_cmos_sensor(0x6F12, 0x0CB1);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0x00E0);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x60B1);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x9B49);
	write_cmos_sensor(0x6F12, 0x0880);
	write_cmos_sensor(0x6F12, 0x9B49);
	write_cmos_sensor(0x6F12, 0x0420);
	write_cmos_sensor(0x6F12, 0xA1F8);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0x0322);
	write_cmos_sensor(0x6F12, 0xA1F8);
	write_cmos_sensor(0x6F12, 0x2021);
	write_cmos_sensor(0x6F12, 0x9949);
	write_cmos_sensor(0x6F12, 0x0880);
	write_cmos_sensor(0x6F12, 0x70BD);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0xF1E7);
	write_cmos_sensor(0x6F12, 0x70B5);
	write_cmos_sensor(0x6F12, 0x9248);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x4169);
	write_cmos_sensor(0x6F12, 0x0C0C);
	write_cmos_sensor(0x6F12, 0x8DB2);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xE7FE);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x17FF);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xE0FE);
	write_cmos_sensor(0x6F12, 0x8849);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0xA1F8);
	write_cmos_sensor(0x6F12, 0x9007);
	write_cmos_sensor(0x6F12, 0x70BD);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF041);
	write_cmos_sensor(0x6F12, 0x0546);
	write_cmos_sensor(0x6F12, 0x8648);
	write_cmos_sensor(0x6F12, 0x9046);
	write_cmos_sensor(0x6F12, 0x0C46);
	write_cmos_sensor(0x6F12, 0x8269);
	write_cmos_sensor(0x6F12, 0x160C);
	write_cmos_sensor(0x6F12, 0x97B2);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x3046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xCDFE);
	write_cmos_sensor(0x6F12, 0x4246);
	write_cmos_sensor(0x6F12, 0x2146);
	write_cmos_sensor(0x6F12, 0x2846);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xFFFE);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x3046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xC3FE);
	write_cmos_sensor(0x6F12, 0x8048);
	write_cmos_sensor(0x6F12, 0x0278);
	write_cmos_sensor(0x6F12, 0x5AB1);
	write_cmos_sensor(0x6F12, 0x012A);
	write_cmos_sensor(0x6F12, 0x10D1);
	write_cmos_sensor(0x6F12, 0x4146);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xF6FE);
	write_cmos_sensor(0x6F12, 0x4146);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0xF041);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xF0BE);
	write_cmos_sensor(0x6F12, 0xA08B);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x6F12, 0x03D0);
	write_cmos_sensor(0x6F12, 0x95F8);
	write_cmos_sensor(0x6F12, 0x7000);
	write_cmos_sensor(0x6F12, 0x0228);
	write_cmos_sensor(0x6F12, 0xEED1);
	write_cmos_sensor(0x6F12, 0x6EE7);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF047);
	write_cmos_sensor(0x6F12, 0x0446);
	write_cmos_sensor(0x6F12, 0x6E48);
	write_cmos_sensor(0x6F12, 0x1546);
	write_cmos_sensor(0x6F12, 0x8946);
	write_cmos_sensor(0x6F12, 0xC069);
	write_cmos_sensor(0x6F12, 0x9846);
	write_cmos_sensor(0x6F12, 0x87B2);
	write_cmos_sensor(0x6F12, 0x060C);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x3046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x9DFE);
	write_cmos_sensor(0x6F12, 0x4346);
	write_cmos_sensor(0x6F12, 0x2A46);
	write_cmos_sensor(0x6F12, 0x4946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xD8FE);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x3046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x92FE);
	write_cmos_sensor(0x6F12, 0x2078);
	write_cmos_sensor(0x6F12, 0x0428);
	write_cmos_sensor(0x6F12, 0x06D0);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x30B1);
	write_cmos_sensor(0x6F12, 0x94F8);
	write_cmos_sensor(0x6F12, 0x7600);
	write_cmos_sensor(0x6F12, 0x18B1);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0x02E0);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0xF7E7);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x6F12, 0x07D0);
	write_cmos_sensor(0x6F12, 0x94F8);
	write_cmos_sensor(0x6F12, 0x7700);
	write_cmos_sensor(0x6F12, 0x00B1);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0x05F1);
	write_cmos_sensor(0x6F12, 0x8045);
	write_cmos_sensor(0x6F12, 0xA5F8);
	write_cmos_sensor(0x6F12, 0x8800);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0xF087);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF047);
	write_cmos_sensor(0x6F12, 0x8146);
	write_cmos_sensor(0x6F12, 0x5548);
	write_cmos_sensor(0x6F12, 0x8846);
	write_cmos_sensor(0x6F12, 0x1546);
	write_cmos_sensor(0x6F12, 0x016A);
	write_cmos_sensor(0x6F12, 0x1C46);
	write_cmos_sensor(0x6F12, 0x0E0C);
	write_cmos_sensor(0x6F12, 0x8FB2);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x3046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x6AFE);
	write_cmos_sensor(0x6F12, 0x2346);
	write_cmos_sensor(0x6F12, 0x2A46);
	write_cmos_sensor(0x6F12, 0x4146);
	write_cmos_sensor(0x6F12, 0x4846);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xAAFE);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x3046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x5FFE);
	write_cmos_sensor(0x6F12, 0x04F1);
	write_cmos_sensor(0x6F12, 0x8044);
	write_cmos_sensor(0x6F12, 0x8A34);
	write_cmos_sensor(0x6F12, 0x2580);
	write_cmos_sensor(0x6F12, 0x280C);
	write_cmos_sensor(0x6F12, 0x6080);
	write_cmos_sensor(0x6F12, 0xDCE7);
	write_cmos_sensor(0x6F12, 0x70B5);
	write_cmos_sensor(0x6F12, 0x0446);
	write_cmos_sensor(0x6F12, 0x4B48);
	write_cmos_sensor(0x6F12, 0x0E46);
	write_cmos_sensor(0x6F12, 0xD0F8);
	write_cmos_sensor(0x6F12, 0x005B);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x9BFE);
	write_cmos_sensor(0x6F12, 0x0441);
	write_cmos_sensor(0x6F12, 0x641B);
	write_cmos_sensor(0x6F12, 0x241F);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x96FE);
	write_cmos_sensor(0x6F12, 0x26FA);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xA042);
	write_cmos_sensor(0x6F12, 0x03DB);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x90FE);
	write_cmos_sensor(0x6F12, 0x26FA);
	write_cmos_sensor(0x6F12, 0x00F4);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x91FE);
	write_cmos_sensor(0x6F12, 0x80B1);
	write_cmos_sensor(0x6F12, 0x6019);
	write_cmos_sensor(0x6F12, 0x0321);
	write_cmos_sensor(0x6F12, 0x001D);
	write_cmos_sensor(0x6F12, 0xB0FB);
	write_cmos_sensor(0x6F12, 0xF1F2);
	write_cmos_sensor(0x6F12, 0x01FB);
	write_cmos_sensor(0x6F12, 0x1202);
	write_cmos_sensor(0x6F12, 0x32B1);
	write_cmos_sensor(0x6F12, 0xB0FB);
	write_cmos_sensor(0x6F12, 0xF1F2);
	write_cmos_sensor(0x6F12, 0x01FB);
	write_cmos_sensor(0x6F12, 0x1200);
	write_cmos_sensor(0x6F12, 0xC0F1);
	write_cmos_sensor(0x6F12, 0x0300);
	write_cmos_sensor(0x6F12, 0x00E0);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x0444);
	write_cmos_sensor(0x6F12, 0x2146);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0x7040);
	write_cmos_sensor(0x6F12, 0x4FF4);
	write_cmos_sensor(0x6F12, 0x7040);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x7DBE);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF041);
	write_cmos_sensor(0x6F12, 0x2B4D);
	write_cmos_sensor(0x6F12, 0x8046);
	write_cmos_sensor(0x6F12, 0xB5F8);
	write_cmos_sensor(0x6F12, 0x7A63);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x70FE);
	write_cmos_sensor(0x6F12, 0xB8B1);
	write_cmos_sensor(0x6F12, 0xB5F8);
	write_cmos_sensor(0x6F12, 0x6813);
	write_cmos_sensor(0x6F12, 0xD5F8);
	write_cmos_sensor(0x6F12, 0x8803);
	write_cmos_sensor(0x6F12, 0x0844);
	write_cmos_sensor(0x6F12, 0x00EB);
	write_cmos_sensor(0x6F12, 0x4004);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x70FE);
	write_cmos_sensor(0x6F12, 0x2B49);
	write_cmos_sensor(0x6F12, 0x2B4A);
	write_cmos_sensor(0x6F12, 0x0128);
	write_cmos_sensor(0x6F12, 0x02D0);
	write_cmos_sensor(0x6F12, 0xB1F8);
	write_cmos_sensor(0x6F12, 0xCE04);
	write_cmos_sensor(0x6F12, 0x01E0);
	write_cmos_sensor(0x6F12, 0xB1F8);
	write_cmos_sensor(0x6F12, 0xC604);
	write_cmos_sensor(0x6F12, 0xB2F8);
	write_cmos_sensor(0x6F12, 0x5210);
	write_cmos_sensor(0x6F12, 0x0844);
	write_cmos_sensor(0x6F12, 0x2044);
	write_cmos_sensor(0x6F12, 0xA5F8);
	write_cmos_sensor(0x6F12, 0x7A03);
	write_cmos_sensor(0x6F12, 0x1D48);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x416A);
	write_cmos_sensor(0x6F12, 0x0C0C);
	write_cmos_sensor(0x6F12, 0x8FB2);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xFEFD);
	write_cmos_sensor(0x6F12, 0x4046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x5AFE);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xF6FD);
	write_cmos_sensor(0x6F12, 0xA5F8);
	write_cmos_sensor(0x6F12, 0x7A63);
	write_cmos_sensor(0x6F12, 0xB5E6);
	write_cmos_sensor(0x6F12, 0x1148);
	write_cmos_sensor(0x6F12, 0x90F8);
	write_cmos_sensor(0x6F12, 0x3716);
	write_cmos_sensor(0x6F12, 0x80F8);
	write_cmos_sensor(0x6F12, 0x2412);
	write_cmos_sensor(0x6F12, 0x90F8);
	write_cmos_sensor(0x6F12, 0xD303);
	write_cmos_sensor(0x6F12, 0x7047);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF041);
	write_cmos_sensor(0x6F12, 0x0446);
	write_cmos_sensor(0x6F12, 0x1E46);
	write_cmos_sensor(0x6F12, 0x9046);
	write_cmos_sensor(0x6F12, 0x0F46);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x46FE);
	write_cmos_sensor(0x6F12, 0x3844);
	write_cmos_sensor(0x6F12, 0x2044);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x47FE);
	write_cmos_sensor(0x6F12, 0x074D);
	write_cmos_sensor(0x6F12, 0xD5F8);
	write_cmos_sensor(0x6F12, 0x4017);
	write_cmos_sensor(0x6F12, 0x8142);
	write_cmos_sensor(0x6F12, 0x04D2);
	write_cmos_sensor(0x6F12, 0x0E49);
	write_cmos_sensor(0x6F12, 0x0979);
	write_cmos_sensor(0x6F12, 0x81B3);
	write_cmos_sensor(0x6F12, 0xC5F8);
	write_cmos_sensor(0x6F12, 0x4007);
	write_cmos_sensor(0x6F12, 0x4046);
	write_cmos_sensor(0x6F12, 0x18E0);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x4000);
	write_cmos_sensor(0x6F12, 0x0060);
	write_cmos_sensor(0x6F12, 0x2001);
	write_cmos_sensor(0x6F12, 0x01B0);
	write_cmos_sensor(0x6F12, 0x3F7F);
	write_cmos_sensor(0x6F12, 0xFF58);
	write_cmos_sensor(0x6F12, 0x2001);
	write_cmos_sensor(0x6F12, 0x32D0);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x2430);
	write_cmos_sensor(0x6F12, 0x4000);
	write_cmos_sensor(0x6F12, 0xF40E);
	write_cmos_sensor(0x6F12, 0x4000);
	write_cmos_sensor(0x6F12, 0xF000);
	write_cmos_sensor(0x6F12, 0x4000);
	write_cmos_sensor(0x6F12, 0xB40E);
	write_cmos_sensor(0x6F12, 0x2001);
	write_cmos_sensor(0x6F12, 0xF600);
	write_cmos_sensor(0x6F12, 0x2001);
	write_cmos_sensor(0x6F12, 0x1420);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x1370);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x2420);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x20FE);
	write_cmos_sensor(0x6F12, 0xB442);
	write_cmos_sensor(0x6F12, 0x00D8);
	write_cmos_sensor(0x6F12, 0x3446);
	write_cmos_sensor(0x6F12, 0x3844);
	write_cmos_sensor(0x6F12, 0x2044);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x19FE);
	write_cmos_sensor(0x6F12, 0xD5F8);
	write_cmos_sensor(0x6F12, 0x4017);
	write_cmos_sensor(0x6F12, 0x8142);
	write_cmos_sensor(0x6F12, 0x03D2);
	write_cmos_sensor(0x6F12, 0xC5F8);
	write_cmos_sensor(0x6F12, 0x4007);
	write_cmos_sensor(0x6F12, 0xA5F8);
	write_cmos_sensor(0x6F12, 0x8403);
	write_cmos_sensor(0x6F12, 0xD5F8);
	write_cmos_sensor(0x6F12, 0x4007);
	write_cmos_sensor(0x6F12, 0x67E6);
	write_cmos_sensor(0x6F12, 0xFFE7);
	write_cmos_sensor(0x6F12, 0xD5F8);
	write_cmos_sensor(0x6F12, 0x4407);
	write_cmos_sensor(0x6F12, 0xCAE7);
	write_cmos_sensor(0x6F12, 0xFB49);
	write_cmos_sensor(0x6F12, 0x91F8);
	write_cmos_sensor(0x6F12, 0x4827);
	write_cmos_sensor(0x6F12, 0xFB49);
	write_cmos_sensor(0x6F12, 0x0AB9);
	write_cmos_sensor(0x6F12, 0x91F8);
	write_cmos_sensor(0x6F12, 0xAE04);
	write_cmos_sensor(0x6F12, 0x0146);
	write_cmos_sensor(0x6F12, 0x0128);
	write_cmos_sensor(0x6F12, 0x0BD0);
	write_cmos_sensor(0x6F12, 0xF748);
	write_cmos_sensor(0x6F12, 0x0229);
	write_cmos_sensor(0x6F12, 0xB0F8);
	write_cmos_sensor(0x6F12, 0x1001);
	write_cmos_sensor(0x6F12, 0x05D0);
	write_cmos_sensor(0x6F12, 0xF54A);
	write_cmos_sensor(0x6F12, 0xB2F8);
	write_cmos_sensor(0x6F12, 0x1201);
	write_cmos_sensor(0x6F12, 0xB2F8);
	write_cmos_sensor(0x6F12, 0x1021);
	write_cmos_sensor(0x6F12, 0x1044);
	write_cmos_sensor(0x6F12, 0x7047);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x7047);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF84F);
	write_cmos_sensor(0x6F12, 0x8246);
	write_cmos_sensor(0x6F12, 0xF048);
	write_cmos_sensor(0x6F12, 0x9146);
	write_cmos_sensor(0x6F12, 0x0E46);
	write_cmos_sensor(0x6F12, 0x426B);
	write_cmos_sensor(0x6F12, 0x9846);
	write_cmos_sensor(0x6F12, 0x150C);
	write_cmos_sensor(0x6F12, 0x97B2);
	write_cmos_sensor(0x6F12, 0x0A9C);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x2846);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x79FD);
	write_cmos_sensor(0x6F12, 0x4346);
	write_cmos_sensor(0x6F12, 0x4A46);
	write_cmos_sensor(0x6F12, 0x3146);
	write_cmos_sensor(0x6F12, 0x5046);
	write_cmos_sensor(0x6F12, 0x0094);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xE0FD);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x2846);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x6DFD);
	write_cmos_sensor(0x6F12, 0x7089);
	write_cmos_sensor(0x6F12, 0x8008);
	write_cmos_sensor(0x6F12, 0x6082);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0xF88F);
	write_cmos_sensor(0x6F12, 0x70B5);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xD8FD);
	write_cmos_sensor(0x6F12, 0xDD49);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x81F8);
	write_cmos_sensor(0x6F12, 0x3605);
	write_cmos_sensor(0x6F12, 0xDD48);
	write_cmos_sensor(0x6F12, 0x816B);
	write_cmos_sensor(0x6F12, 0x0C0C);
	write_cmos_sensor(0x6F12, 0x8DB2);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x59FD);
	write_cmos_sensor(0x6F12, 0xDA48);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xCEFD);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0x7040);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x4FBD);
	write_cmos_sensor(0x6F12, 0x70B5);
	write_cmos_sensor(0x6F12, 0xD448);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xC16B);
	write_cmos_sensor(0x6F12, 0x0C0C);
	write_cmos_sensor(0x6F12, 0x8DB2);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x45FD);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xC0FD);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x3EFD);
	write_cmos_sensor(0x6F12, 0xCE48);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x6F12, 0x05D0);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0x7040);
	write_cmos_sensor(0x6F12, 0x0421);
	write_cmos_sensor(0x6F12, 0xCB48);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xB6BD);
	write_cmos_sensor(0x6F12, 0x70BD);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF04F);
	write_cmos_sensor(0x6F12, 0x85B0);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xB4FD);
	write_cmos_sensor(0x6F12, 0xC748);
	write_cmos_sensor(0x6F12, 0x0390);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x0290);
	write_cmos_sensor(0x6F12, 0x0490);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x0190);
	write_cmos_sensor(0x6F12, 0xC548);
	write_cmos_sensor(0x6F12, 0x0499);
	write_cmos_sensor(0x6F12, 0x00EB);
	write_cmos_sensor(0x6F12, 0xC100);
	write_cmos_sensor(0x6F12, 0x0090);
	write_cmos_sensor(0x6F12, 0xDDE9);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0x4FF0);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x00EB);
	write_cmos_sensor(0x6F12, 0x810E);
	write_cmos_sensor(0x6F12, 0x6046);
	write_cmos_sensor(0x6F12, 0x6FF3);
	write_cmos_sensor(0x6F12, 0x9E00);
	write_cmos_sensor(0x6F12, 0x0328);
	write_cmos_sensor(0x6F12, 0x7ED0);
	write_cmos_sensor(0x6F12, 0xDDE9);
	write_cmos_sensor(0x6F12, 0x0201);
	write_cmos_sensor(0x6F12, 0x0023);
	write_cmos_sensor(0x6F12, 0x01EB);
	write_cmos_sensor(0x6F12, 0x4010);
	write_cmos_sensor(0x6F12, 0xBB49);
	write_cmos_sensor(0x6F12, 0xB0F9);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x01EB);
	write_cmos_sensor(0x6F12, 0x8301);
	write_cmos_sensor(0x6F12, 0xB0F9);
	write_cmos_sensor(0x6F12, 0x0470);
	write_cmos_sensor(0x6F12, 0xD1F8);
	write_cmos_sensor(0x6F12, 0xD452);
	write_cmos_sensor(0x6F12, 0xB0F9);
	write_cmos_sensor(0x6F12, 0x0A90);
	write_cmos_sensor(0x6F12, 0x02FB);
	write_cmos_sensor(0x6F12, 0x05F6);
	write_cmos_sensor(0x6F12, 0xB0F9);
	write_cmos_sensor(0x6F12, 0x0220);
	write_cmos_sensor(0x6F12, 0xB0F9);
	write_cmos_sensor(0x6F12, 0x12B0);
	write_cmos_sensor(0x6F12, 0x02FB);
	write_cmos_sensor(0x6F12, 0x05F4);
	write_cmos_sensor(0x6F12, 0xD1F8);
	write_cmos_sensor(0x6F12, 0x3C23);
	write_cmos_sensor(0x6F12, 0x07FB);
	write_cmos_sensor(0x6F12, 0x0267);
	write_cmos_sensor(0x6F12, 0xB0F9);
	write_cmos_sensor(0x6F12, 0x0660);
	write_cmos_sensor(0x6F12, 0x06FB);
	write_cmos_sensor(0x6F12, 0x05F8);
	write_cmos_sensor(0x6F12, 0xB0F9);
	write_cmos_sensor(0x6F12, 0x0860);
	write_cmos_sensor(0x6F12, 0x06FB);
	write_cmos_sensor(0x6F12, 0x0246);
	write_cmos_sensor(0x6F12, 0xD1F8);
	write_cmos_sensor(0x6F12, 0xA443);
	write_cmos_sensor(0x6F12, 0x09FB);
	write_cmos_sensor(0x6F12, 0x0477);
	write_cmos_sensor(0x6F12, 0xB0F9);
	write_cmos_sensor(0x6F12, 0x0C90);
	write_cmos_sensor(0x6F12, 0x09FB);
	write_cmos_sensor(0x6F12, 0x05FA);
	write_cmos_sensor(0x6F12, 0xB0F9);
	write_cmos_sensor(0x6F12, 0x0E90);
	write_cmos_sensor(0x6F12, 0x09FB);
	write_cmos_sensor(0x6F12, 0x0289);
	write_cmos_sensor(0x6F12, 0xB0F9);
	write_cmos_sensor(0x6F12, 0x1080);
	write_cmos_sensor(0x6F12, 0x08FB);
	write_cmos_sensor(0x6F12, 0x0468);
	write_cmos_sensor(0x6F12, 0xD1F8);
	write_cmos_sensor(0x6F12, 0x0C64);
	write_cmos_sensor(0x6F12, 0xD1F8);
	write_cmos_sensor(0x6F12, 0x7414);
	write_cmos_sensor(0x6F12, 0x0BFB);
	write_cmos_sensor(0x6F12, 0x0677);
	write_cmos_sensor(0x6F12, 0xB0F9);
	write_cmos_sensor(0x6F12, 0x14B0);
	write_cmos_sensor(0x6F12, 0x0BFB);
	write_cmos_sensor(0x6F12, 0x05F5);
	write_cmos_sensor(0x6F12, 0xB0F9);
	write_cmos_sensor(0x6F12, 0x16B0);
	write_cmos_sensor(0x6F12, 0x0BFB);
	write_cmos_sensor(0x6F12, 0x02A2);
	write_cmos_sensor(0x6F12, 0xB0F9);
	write_cmos_sensor(0x6F12, 0x18A0);
	write_cmos_sensor(0x6F12, 0x0AFB);
	write_cmos_sensor(0x6F12, 0x0494);
	write_cmos_sensor(0x6F12, 0xB0F9);
	write_cmos_sensor(0x6F12, 0x1A90);
	write_cmos_sensor(0x6F12, 0x09FB);
	write_cmos_sensor(0x6F12, 0x0688);
	write_cmos_sensor(0x6F12, 0xB0F9);
	write_cmos_sensor(0x6F12, 0x1C60);
	write_cmos_sensor(0x6F12, 0x06FB);
	write_cmos_sensor(0x6F12, 0x0171);
	write_cmos_sensor(0x6F12, 0x01F5);
	write_cmos_sensor(0x6F12, 0x8041);
	write_cmos_sensor(0x6F12, 0xCE13);
	write_cmos_sensor(0x6F12, 0x08F5);
	write_cmos_sensor(0x6F12, 0x8041);
	write_cmos_sensor(0x6F12, 0xCF13);
	write_cmos_sensor(0x6F12, 0x04F5);
	write_cmos_sensor(0x6F12, 0x8041);
	write_cmos_sensor(0x6F12, 0x4FEA);
	write_cmos_sensor(0x6F12, 0xE138);
	write_cmos_sensor(0x6F12, 0x02F5);
	write_cmos_sensor(0x6F12, 0x8041);
	write_cmos_sensor(0x6F12, 0x4FEA);
	write_cmos_sensor(0x6F12, 0xE139);
	write_cmos_sensor(0x6F12, 0x05F5);
	write_cmos_sensor(0x6F12, 0x8041);
	write_cmos_sensor(0x6F12, 0x4FEA);
	write_cmos_sensor(0x6F12, 0xE13A);
	write_cmos_sensor(0x6F12, 0xC3EB);
	write_cmos_sensor(0x6F12, 0xC302);
	write_cmos_sensor(0x6F12, 0x0021);
	write_cmos_sensor(0x6F12, 0x02EB);
	write_cmos_sensor(0x6F12, 0x8205);
	write_cmos_sensor(0x6F12, 0x8B4A);
	write_cmos_sensor(0x6F12, 0x02EB);
	write_cmos_sensor(0x6F12, 0x8102);
	write_cmos_sensor(0x6F12, 0x9469);
	write_cmos_sensor(0x6F12, 0xD2F8);
	write_cmos_sensor(0x6F12, 0xA4B0);
	write_cmos_sensor(0x6F12, 0x7443);
	write_cmos_sensor(0x6F12, 0x07FB);
	write_cmos_sensor(0x6F12, 0x0B44);
	write_cmos_sensor(0x6F12, 0xD2F8);
	write_cmos_sensor(0x6F12, 0x30B1);
	write_cmos_sensor(0x6F12, 0x08FB);
	write_cmos_sensor(0x6F12, 0x0B44);
	write_cmos_sensor(0x6F12, 0xD2F8);
	write_cmos_sensor(0x6F12, 0xBCB1);
	write_cmos_sensor(0x6F12, 0xD2F8);
	write_cmos_sensor(0x6F12, 0x4822);
	write_cmos_sensor(0x6F12, 0x09FB);
	write_cmos_sensor(0x6F12, 0x0B44);
	write_cmos_sensor(0x6F12, 0x0AFB);
	write_cmos_sensor(0x6F12, 0x0242);
	write_cmos_sensor(0x6F12, 0xC47F);
	write_cmos_sensor(0x6F12, 0x2241);
	write_cmos_sensor(0x6F12, 0x002A);
	write_cmos_sensor(0x6F12, 0x3ADD);
	write_cmos_sensor(0x6F12, 0x00E0);
	write_cmos_sensor(0x6F12, 0x14E0);
	write_cmos_sensor(0x6F12, 0xFF2A);
	write_cmos_sensor(0x6F12, 0x00DB);
	write_cmos_sensor(0x6F12, 0xFF22);
	write_cmos_sensor(0x6F12, 0xDEF8);
	write_cmos_sensor(0x6F12, 0x5440);
	write_cmos_sensor(0x6F12, 0x0CEB);
	write_cmos_sensor(0x6F12, 0xC10B);
	write_cmos_sensor(0x6F12, 0x04EB);
	write_cmos_sensor(0x6F12, 0xC504);
	write_cmos_sensor(0x6F12, 0x491C);
	write_cmos_sensor(0x6F12, 0x04F8);
	write_cmos_sensor(0x6F12, 0x0B20);
	write_cmos_sensor(0x6F12, 0x2329);
	write_cmos_sensor(0x6F12, 0xD6DB);
	write_cmos_sensor(0x6F12, 0x5B1C);
	write_cmos_sensor(0x6F12, 0x1A2B);
	write_cmos_sensor(0x6F12, 0xFFF6);
	write_cmos_sensor(0x6F12, 0x73AF);
	write_cmos_sensor(0x6F12, 0x0298);
	write_cmos_sensor(0x6F12, 0x401C);
	write_cmos_sensor(0x6F12, 0x0290);
	write_cmos_sensor(0x6F12, 0x0CF1);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0xBCF1);
	write_cmos_sensor(0x6F12, 0x080F);
	write_cmos_sensor(0x6F12, 0xFFF6);
	write_cmos_sensor(0x6F12, 0x60AF);
	write_cmos_sensor(0x6F12, 0x0198);
	write_cmos_sensor(0x6F12, 0x401C);
	write_cmos_sensor(0x6F12, 0x0190);
	write_cmos_sensor(0x6F12, 0x0228);
	write_cmos_sensor(0x6F12, 0xFFF6);
	write_cmos_sensor(0x6F12, 0x54AF);
	write_cmos_sensor(0x6F12, 0x0498);
	write_cmos_sensor(0x6F12, 0x401C);
	write_cmos_sensor(0x6F12, 0x0490);
	write_cmos_sensor(0x6F12, 0x0328);
	write_cmos_sensor(0x6F12, 0xFFF6);
	write_cmos_sensor(0x6F12, 0x47AF);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xF3FC);
	write_cmos_sensor(0x6F12, 0x6848);
	write_cmos_sensor(0x6F12, 0x694A);
	write_cmos_sensor(0x6F12, 0x90F8);
	write_cmos_sensor(0x6F12, 0x6C10);
	write_cmos_sensor(0x6F12, 0x1180);
	write_cmos_sensor(0x6F12, 0x921C);
	write_cmos_sensor(0x6F12, 0x90F8);
	write_cmos_sensor(0x6F12, 0x6D10);
	write_cmos_sensor(0x6F12, 0x1180);
	write_cmos_sensor(0x6F12, 0x911C);
	write_cmos_sensor(0x6F12, 0x90F8);
	write_cmos_sensor(0x6F12, 0x6E00);
	write_cmos_sensor(0x6F12, 0x0880);
	write_cmos_sensor(0x6F12, 0x05B0);
	write_cmos_sensor(0x6F12, 0x57E4);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xC7E7);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF041);
	write_cmos_sensor(0x6F12, 0x614D);
	write_cmos_sensor(0x6F12, 0xEC79);
	write_cmos_sensor(0x6F12, 0x012C);
	write_cmos_sensor(0x6F12, 0x07D1);
	write_cmos_sensor(0x6F12, 0xA888);
	write_cmos_sensor(0x6F12, 0x41F6);
	write_cmos_sensor(0x6F12, 0x8A32);
	write_cmos_sensor(0x6F12, 0x5F49);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xDBFC);
	write_cmos_sensor(0x6F12, 0x0220);
	write_cmos_sensor(0x6F12, 0xE871);
	write_cmos_sensor(0x6F12, 0x5348);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x416C);
	write_cmos_sensor(0x6F12, 0x0E0C);
	write_cmos_sensor(0x6F12, 0x8FB2);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x3046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x44FC);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xD3FC);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x3046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x3DFC);
	write_cmos_sensor(0x6F12, 0xEC71);
	write_cmos_sensor(0x6F12, 0xFDE4);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF047);
	write_cmos_sensor(0x6F12, 0x0446);
	write_cmos_sensor(0x6F12, 0x90F8);
	write_cmos_sensor(0x6F12, 0x8A53);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x7FFC);
	write_cmos_sensor(0x6F12, 0x0146);
	write_cmos_sensor(0x6F12, 0xD4F8);
	write_cmos_sensor(0x6F12, 0xA403);
	write_cmos_sensor(0x6F12, 0x20FA);
	write_cmos_sensor(0x6F12, 0x01F8);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x78FC);
	write_cmos_sensor(0x6F12, 0xD4F8);
	write_cmos_sensor(0x6F12, 0xA863);
	write_cmos_sensor(0x6F12, 0xC640);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xBEFC);
	write_cmos_sensor(0x6F12, 0x0446);
	write_cmos_sensor(0x6F12, 0x012D);
	write_cmos_sensor(0x6F12, 0x04D0);
	write_cmos_sensor(0x6F12, 0x0327);
	write_cmos_sensor(0x6F12, 0x022D);
	write_cmos_sensor(0x6F12, 0x04D0);
	write_cmos_sensor(0x6F12, 0x032D);
	write_cmos_sensor(0x6F12, 0x14D0);
	write_cmos_sensor(0x6F12, 0x0025);
	write_cmos_sensor(0x6F12, 0x2C46);
	write_cmos_sensor(0x6F12, 0x0FE0);
	write_cmos_sensor(0x6F12, 0xB846);
	write_cmos_sensor(0x6F12, 0xB6FB);
	write_cmos_sensor(0x6F12, 0xF8F7);
	write_cmos_sensor(0x6F12, 0x0025);
	write_cmos_sensor(0x6F12, 0x7F1C);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x61FC);
	write_cmos_sensor(0x6F12, 0x8740);
	write_cmos_sensor(0x6F12, 0xA742);
	write_cmos_sensor(0x6F12, 0x05D8);
	write_cmos_sensor(0x6F12, 0xB6FB);
	write_cmos_sensor(0x6F12, 0xF8F4);
	write_cmos_sensor(0x6F12, 0x641C);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x59FC);
	write_cmos_sensor(0x6F12, 0x8440);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x32E0);
	write_cmos_sensor(0x6F12, 0xB8FB);
	write_cmos_sensor(0x6F12, 0xF7F5);
	write_cmos_sensor(0x6F12, 0x6D1C);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x51FC);
	write_cmos_sensor(0x6F12, 0x8540);
	write_cmos_sensor(0x6F12, 0xA542);
	write_cmos_sensor(0x6F12, 0x01D9);
	write_cmos_sensor(0x6F12, 0x2546);
	write_cmos_sensor(0x6F12, 0x05E0);
	write_cmos_sensor(0x6F12, 0xB8FB);
	write_cmos_sensor(0x6F12, 0xF7F5);
	write_cmos_sensor(0x6F12, 0x6D1C);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x47FC);
	write_cmos_sensor(0x6F12, 0x8540);
	write_cmos_sensor(0x6F12, 0xB946);
	write_cmos_sensor(0x6F12, 0xB6FB);
	write_cmos_sensor(0x6F12, 0xF9F7);
	write_cmos_sensor(0x6F12, 0x7F1C);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x40FC);
	write_cmos_sensor(0x6F12, 0x8740);
	write_cmos_sensor(0x6F12, 0xA742);
	write_cmos_sensor(0x6F12, 0x01D9);
	write_cmos_sensor(0x6F12, 0x2746);
	write_cmos_sensor(0x6F12, 0x05E0);
	write_cmos_sensor(0x6F12, 0xB6FB);
	write_cmos_sensor(0x6F12, 0xF9F7);
	write_cmos_sensor(0x6F12, 0x7F1C);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x36FC);
	write_cmos_sensor(0x6F12, 0x8740);
	write_cmos_sensor(0x6F12, 0xA6EB);
	write_cmos_sensor(0x6F12, 0x0808);
	write_cmos_sensor(0x6F12, 0xB8FB);
	write_cmos_sensor(0x6F12, 0xF9F6);
	write_cmos_sensor(0x6F12, 0x761C);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x2EFC);
	write_cmos_sensor(0x6F12, 0x8640);
	write_cmos_sensor(0x6F12, 0xA642);
	write_cmos_sensor(0x6F12, 0x05D8);
	write_cmos_sensor(0x6F12, 0xB8FB);
	write_cmos_sensor(0x6F12, 0xF9F4);
	write_cmos_sensor(0x6F12, 0x641C);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x26FC);
	write_cmos_sensor(0x6F12, 0x8440);
	write_cmos_sensor(0x6F12, 0x3C44);
	write_cmos_sensor(0x6F12, 0x601B);
	write_cmos_sensor(0x6F12, 0x234A);
	write_cmos_sensor(0x6F12, 0xB2F9);
	write_cmos_sensor(0x6F12, 0x0E10);
	write_cmos_sensor(0x6F12, 0x4B19);
	write_cmos_sensor(0x6F12, 0x2249);
	write_cmos_sensor(0x6F12, 0x0968);
	write_cmos_sensor(0x6F12, 0x8B66);
	write_cmos_sensor(0x6F12, 0xB2F9);
	write_cmos_sensor(0x6F12, 0x1020);
	write_cmos_sensor(0x6F12, 0x2244);
	write_cmos_sensor(0x6F12, 0xC1E9);
	write_cmos_sensor(0x6F12, 0x1B02);
	write_cmos_sensor(0x6F12, 0x4FE5);
	write_cmos_sensor(0x6F12, 0x10B5);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x1146);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x60FC);
	write_cmos_sensor(0x6F12, 0x1B4C);
	write_cmos_sensor(0x6F12, 0x4FF6);
	write_cmos_sensor(0x6F12, 0xFF72);
	write_cmos_sensor(0x6F12, 0xB4F8);
	write_cmos_sensor(0x6F12, 0x5010);
	write_cmos_sensor(0x6F12, 0x02EA);
	write_cmos_sensor(0x6F12, 0x4102);
	write_cmos_sensor(0x6F12, 0x0221);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x56FC);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x0421);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x52FC);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x0821);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x4EFC);
	write_cmos_sensor(0x6F12, 0x1349);
	write_cmos_sensor(0x6F12, 0x0880);
	write_cmos_sensor(0x6F12, 0x94F8);
	write_cmos_sensor(0x6F12, 0x4F10);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0x8840);
	write_cmos_sensor(0x6F12, 0x1049);
	write_cmos_sensor(0x6F12, 0x3C31);
	write_cmos_sensor(0x6F12, 0x0880);
	write_cmos_sensor(0x6F12, 0x10BD);
	write_cmos_sensor(0x6F12, 0x2001);
	write_cmos_sensor(0x6F12, 0x01B0);
	write_cmos_sensor(0x6F12, 0x2001);
	write_cmos_sensor(0x6F12, 0x1420);
	write_cmos_sensor(0x6F12, 0x2001);
	write_cmos_sensor(0x6F12, 0x32D0);
	write_cmos_sensor(0x6F12, 0x2001);
	write_cmos_sensor(0x6F12, 0x0C80);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x7100);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x5FA0);
	write_cmos_sensor(0x6F12, 0x2002);
	write_cmos_sensor(0x6F12, 0x1393);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0xF7C0);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x5A70);
	write_cmos_sensor(0x6F12, 0x4000);
	write_cmos_sensor(0x6F12, 0x9E20);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0xFF80);
	write_cmos_sensor(0x6F12, 0x2001);
	write_cmos_sensor(0x6F12, 0xF800);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x39E0);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x0C60);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x64C0);
	write_cmos_sensor(0x6F12, 0x4000);
	write_cmos_sensor(0x6F12, 0x7722);
	write_cmos_sensor(0x6F12, 0x10B5);
	write_cmos_sensor(0x6F12, 0xFE48);
	write_cmos_sensor(0x6F12, 0xFE4C);
	write_cmos_sensor(0x6F12, 0xB0F8);
	write_cmos_sensor(0x6F12, 0x4C00);
	write_cmos_sensor(0x6F12, 0x50B1);
	write_cmos_sensor(0x6F12, 0xFFF7);
	write_cmos_sensor(0x6F12, 0xB6FF);
	write_cmos_sensor(0x6F12, 0x94F8);
	write_cmos_sensor(0x6F12, 0x1901);
	write_cmos_sensor(0x6F12, 0x0121);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x1CFC);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0x1040);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x1DBC);
	write_cmos_sensor(0x6F12, 0xF849);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x0880);
	write_cmos_sensor(0x6F12, 0x94F8);
	write_cmos_sensor(0x6F12, 0x1901);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0x1040);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x19BC);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF041);
	write_cmos_sensor(0x6F12, 0x0646);
	write_cmos_sensor(0x6F12, 0xF348);
	write_cmos_sensor(0x6F12, 0x0D46);
	write_cmos_sensor(0x6F12, 0xC26C);
	write_cmos_sensor(0x6F12, 0x140C);
	write_cmos_sensor(0x6F12, 0x97B2);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x62FB);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x3046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x0DFC);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x59FB);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0xF041);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x09BC);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF05F);
	write_cmos_sensor(0x6F12, 0xE54F);
	write_cmos_sensor(0x6F12, 0x0446);
	write_cmos_sensor(0x6F12, 0x2C37);
	write_cmos_sensor(0x6F12, 0x8088);
	write_cmos_sensor(0x6F12, 0xA7F1);
	write_cmos_sensor(0x6F12, 0x0208);
	write_cmos_sensor(0x6F12, 0x0026);
	write_cmos_sensor(0x6F12, 0x0D46);
	write_cmos_sensor(0x6F12, 0xA7F1);
	write_cmos_sensor(0x6F12, 0x0409);
	write_cmos_sensor(0x6F12, 0xA7F1);
	write_cmos_sensor(0x6F12, 0x060A);
	write_cmos_sensor(0x6F12, 0xA8F1);
	write_cmos_sensor(0x6F12, 0x060B);
	write_cmos_sensor(0x6F12, 0x0128);
	write_cmos_sensor(0x6F12, 0x1ED1);
	write_cmos_sensor(0x6F12, 0xDB48);
	write_cmos_sensor(0x6F12, 0xB0F8);
	write_cmos_sensor(0x6F12, 0x4000);
	write_cmos_sensor(0x6F12, 0xD0B9);
	write_cmos_sensor(0x6F12, 0x686A);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xF5FB);
	write_cmos_sensor(0x6F12, 0xA067);
	write_cmos_sensor(0x6F12, 0x288C);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xF1FB);
	write_cmos_sensor(0x6F12, 0xA4F8);
	write_cmos_sensor(0x6F12, 0x7400);
	write_cmos_sensor(0x6F12, 0x3880);
	write_cmos_sensor(0x6F12, 0xA8F8);
	write_cmos_sensor(0x6F12, 0x0060);
	write_cmos_sensor(0x6F12, 0xA06F);
	write_cmos_sensor(0x6F12, 0xA9F8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x020C);
	write_cmos_sensor(0x6F12, 0xAAF8);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0xB4F8);
	write_cmos_sensor(0x6F12, 0x7410);
	write_cmos_sensor(0x6F12, 0x0143);
	write_cmos_sensor(0x6F12, 0x01D0);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0x00E0);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0xABF8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0xE088);
	write_cmos_sensor(0x6F12, 0x0128);
	write_cmos_sensor(0x6F12, 0x39D1);
	write_cmos_sensor(0x6F12, 0xE868);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xD7FB);
	write_cmos_sensor(0x6F12, 0x2066);
	write_cmos_sensor(0x6F12, 0x2889);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xD3FB);
	write_cmos_sensor(0x6F12, 0xA4F8);
	write_cmos_sensor(0x6F12, 0x5C00);
	write_cmos_sensor(0x6F12, 0x3880);
	write_cmos_sensor(0x6F12, 0xA8F8);
	write_cmos_sensor(0x6F12, 0x0060);
	write_cmos_sensor(0x6F12, 0x206E);
	write_cmos_sensor(0x6F12, 0xA9F8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x020C);
	write_cmos_sensor(0x6F12, 0xAAF8);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0xB4F8);
	write_cmos_sensor(0x6F12, 0x5C10);
	write_cmos_sensor(0x6F12, 0x0143);
	write_cmos_sensor(0x6F12, 0x01D0);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0x00E0);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0xABF8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x6868);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xBCFB);
	write_cmos_sensor(0x6F12, 0xA065);
	write_cmos_sensor(0x6F12, 0x6888);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xB8FB);
	write_cmos_sensor(0x6F12, 0xBB49);
	write_cmos_sensor(0x6F12, 0xA4F8);
	write_cmos_sensor(0x6F12, 0x5600);
	write_cmos_sensor(0x6F12, 0x0A39);
	write_cmos_sensor(0x6F12, 0x0880);
	write_cmos_sensor(0x6F12, 0x881E);
	write_cmos_sensor(0x6F12, 0x0680);
	write_cmos_sensor(0x6F12, 0x091F);
	write_cmos_sensor(0x6F12, 0x6868);
	write_cmos_sensor(0x6F12, 0x0880);
	write_cmos_sensor(0x6F12, 0xB64A);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x103A);
	write_cmos_sensor(0x6F12, 0x1180);
	write_cmos_sensor(0x6F12, 0xB4F8);
	write_cmos_sensor(0x6F12, 0x5610);
	write_cmos_sensor(0x6F12, 0x0143);
	write_cmos_sensor(0x6F12, 0x01D0);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0x00E0);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0xB149);
	write_cmos_sensor(0x6F12, 0x1239);
	write_cmos_sensor(0x6F12, 0x0880);
	write_cmos_sensor(0x6F12, 0xAD48);
	write_cmos_sensor(0x6F12, 0xB0F8);
	write_cmos_sensor(0x6F12, 0x4C10);
	write_cmos_sensor(0x6F12, 0x0029);
	write_cmos_sensor(0x6F12, 0x21D0);
	write_cmos_sensor(0x6F12, 0xB0F8);
	write_cmos_sensor(0x6F12, 0x4000);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x6F12, 0x1DD1);
	write_cmos_sensor(0x6F12, 0x6869);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x94FB);
	write_cmos_sensor(0x6F12, 0xA066);
	write_cmos_sensor(0x6F12, 0x288A);
	write_cmos_sensor(0x6F12, 0x6434);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x8FFB);
	write_cmos_sensor(0x6F12, 0xA749);
	write_cmos_sensor(0x6F12, 0x2080);
	write_cmos_sensor(0x6F12, 0x4631);
	write_cmos_sensor(0x6F12, 0x0880);
	write_cmos_sensor(0x6F12, 0x881E);
	write_cmos_sensor(0x6F12, 0x0680);
	write_cmos_sensor(0x6F12, 0x091F);
	write_cmos_sensor(0x6F12, 0x6068);
	write_cmos_sensor(0x6F12, 0x0880);
	write_cmos_sensor(0x6F12, 0xA24A);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x4032);
	write_cmos_sensor(0x6F12, 0x1180);
	write_cmos_sensor(0x6F12, 0x2188);
	write_cmos_sensor(0x6F12, 0x0143);
	write_cmos_sensor(0x6F12, 0x01D0);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0x00E0);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x9D49);
	write_cmos_sensor(0x6F12, 0x3E31);
	write_cmos_sensor(0x6F12, 0x0880);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0xF09F);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF041);
	write_cmos_sensor(0x6F12, 0x0746);
	write_cmos_sensor(0x6F12, 0x9A48);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x406D);
	write_cmos_sensor(0x6F12, 0x85B2);
	write_cmos_sensor(0x6F12, 0x040C);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xB2FA);
	write_cmos_sensor(0x6F12, 0x3846);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x6DFB);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xAAFA);
	write_cmos_sensor(0x6F12, 0xDFF8);
	write_cmos_sensor(0x6F12, 0x3C82);
	write_cmos_sensor(0x6F12, 0xB8F8);
	write_cmos_sensor(0x6F12, 0x4C00);
	write_cmos_sensor(0x6F12, 0x0128);
	write_cmos_sensor(0x6F12, 0x39D1);
	write_cmos_sensor(0x6F12, 0x8D4E);
	write_cmos_sensor(0x6F12, 0x3546);
	write_cmos_sensor(0x6F12, 0xD6F8);
	write_cmos_sensor(0x6F12, 0xD806);
	write_cmos_sensor(0x6F12, 0x06F2);
	write_cmos_sensor(0x6F12, 0xC466);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x55FB);
	write_cmos_sensor(0x6F12, 0xC5F8);
	write_cmos_sensor(0x6F12, 0xF807);
	write_cmos_sensor(0x6F12, 0x0446);
	write_cmos_sensor(0x6F12, 0x308A);
	write_cmos_sensor(0x6F12, 0x05F2);
	write_cmos_sensor(0x6F12, 0xF475);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x4DFB);
	write_cmos_sensor(0x6F12, 0x2880);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xDCFA);
	write_cmos_sensor(0x6F12, 0xC740);
	write_cmos_sensor(0x6F12, 0x8348);
	write_cmos_sensor(0x6F12, 0xB0F8);
	write_cmos_sensor(0x6F12, 0x6A63);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x4EFB);
	write_cmos_sensor(0x6F12, 0xB8F9);
	write_cmos_sensor(0x6F12, 0x4210);
	write_cmos_sensor(0x6F12, 0x3044);
	write_cmos_sensor(0x6F12, 0x7A1A);
	write_cmos_sensor(0x6F12, 0x2988);
	write_cmos_sensor(0x6F12, 0x80B2);
	write_cmos_sensor(0x6F12, 0x521A);
	write_cmos_sensor(0x6F12, 0x101A);
	write_cmos_sensor(0x6F12, 0x7F4A);
	write_cmos_sensor(0x6F12, 0x8442);
	write_cmos_sensor(0x6F12, 0x9061);
	write_cmos_sensor(0x6F12, 0x00D3);
	write_cmos_sensor(0x6F12, 0x0446);
	write_cmos_sensor(0x6F12, 0x7B48);
	write_cmos_sensor(0x6F12, 0x4230);
	write_cmos_sensor(0x6F12, 0x0480);
	write_cmos_sensor(0x6F12, 0x794A);
	write_cmos_sensor(0x6F12, 0x200C);
	write_cmos_sensor(0x6F12, 0x4032);
	write_cmos_sensor(0x6F12, 0x1080);
	write_cmos_sensor(0x6F12, 0x901D);
	write_cmos_sensor(0x6F12, 0x0180);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x121D);
	write_cmos_sensor(0x6F12, 0x1080);
	write_cmos_sensor(0x6F12, 0x2143);
	write_cmos_sensor(0x6F12, 0x01D0);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0x00E0);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x7249);
	write_cmos_sensor(0x6F12, 0x3E31);
	write_cmos_sensor(0x6F12, 0x0880);
	write_cmos_sensor(0x6F12, 0x74E4);
	write_cmos_sensor(0x6F12, 0x70B5);
	write_cmos_sensor(0x6F12, 0x7148);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x816D);
	write_cmos_sensor(0x6F12, 0x0C0C);
	write_cmos_sensor(0x6F12, 0x8DB2);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x5FFA);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x25FB);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x58FA);
	write_cmos_sensor(0x6F12, 0x6649);
	write_cmos_sensor(0x6F12, 0xB1F8);
	write_cmos_sensor(0x6F12, 0x4C00);
	write_cmos_sensor(0x6F12, 0x0128);
	write_cmos_sensor(0x6F12, 0x0FD1);
	write_cmos_sensor(0x6F12, 0x6448);
	write_cmos_sensor(0x6F12, 0x90F8);
	write_cmos_sensor(0x6F12, 0xE52B);
	write_cmos_sensor(0x6F12, 0x002A);
	write_cmos_sensor(0x6F12, 0x0AD0);
	write_cmos_sensor(0x6F12, 0x90F8);
	write_cmos_sensor(0x6F12, 0xE42B);
	write_cmos_sensor(0x6F12, 0x6248);
	write_cmos_sensor(0x6F12, 0x801C);
	write_cmos_sensor(0x6F12, 0x32B1);
	write_cmos_sensor(0x6F12, 0xC98F);
	write_cmos_sensor(0x6F12, 0x09B1);
	write_cmos_sensor(0x6F12, 0x0321);
	write_cmos_sensor(0x6F12, 0x00E0);
	write_cmos_sensor(0x6F12, 0x0121);
	write_cmos_sensor(0x6F12, 0x0180);
	write_cmos_sensor(0x6F12, 0x70BD);
	write_cmos_sensor(0x6F12, 0x0021);
	write_cmos_sensor(0x6F12, 0xFBE7);
	write_cmos_sensor(0x6F12, 0x1CB5);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x0090);
	write_cmos_sensor(0x6F12, 0x0190);
	write_cmos_sensor(0x6F12, 0x5D48);
	write_cmos_sensor(0x6F12, 0xC08F);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x6F12, 0x11D0);
	write_cmos_sensor(0x6F12, 0x0822);
	write_cmos_sensor(0x6F12, 0x6946);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xC0FA);
	write_cmos_sensor(0x6F12, 0x5A49);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x6A46);
	write_cmos_sensor(0x6F12, 0x0B68);
	write_cmos_sensor(0x6F12, 0x32F8);
	write_cmos_sensor(0x6F12, 0x1010);
	write_cmos_sensor(0x6F12, 0x19B1);
	write_cmos_sensor(0x6F12, 0x03EB);
	write_cmos_sensor(0x6F12, 0x4004);
	write_cmos_sensor(0x6F12, 0xA4F8);
	write_cmos_sensor(0x6F12, 0x7014);
	write_cmos_sensor(0x6F12, 0x401C);
	write_cmos_sensor(0x6F12, 0x0428);
	write_cmos_sensor(0x6F12, 0xF5DB);
	write_cmos_sensor(0x6F12, 0x1CBD);
	write_cmos_sensor(0x6F12, 0x1CB5);
	write_cmos_sensor(0x6F12, 0x5348);
	write_cmos_sensor(0x6F12, 0xB0F8);
	write_cmos_sensor(0x6F12, 0xB400);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x6F12, 0x13D0);
	write_cmos_sensor(0x6F12, 0x0021);
	write_cmos_sensor(0x6F12, 0x0091);
	write_cmos_sensor(0x6F12, 0x0191);
	write_cmos_sensor(0x6F12, 0x0622);
	write_cmos_sensor(0x6F12, 0x6946);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xA4FA);
	write_cmos_sensor(0x6F12, 0x4E4B);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x6A46);
	write_cmos_sensor(0x6F12, 0x115C);
	write_cmos_sensor(0x6F12, 0x21B1);
	write_cmos_sensor(0x6F12, 0x1C18);
	write_cmos_sensor(0x6F12, 0x04F5);
	write_cmos_sensor(0x6F12, 0x8054);
	write_cmos_sensor(0x6F12, 0x84F8);
	write_cmos_sensor(0x6F12, 0xAC10);
	write_cmos_sensor(0x6F12, 0x401C);
	write_cmos_sensor(0x6F12, 0x0628);
	write_cmos_sensor(0x6F12, 0xF5DB);
	write_cmos_sensor(0x6F12, 0x1CBD);
	write_cmos_sensor(0x6F12, 0x38B5);
	write_cmos_sensor(0x6F12, 0x454C);
	write_cmos_sensor(0x6F12, 0xB4F8);
	write_cmos_sensor(0x6F12, 0x4204);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x6F12, 0x0CD0);
	write_cmos_sensor(0x6F12, 0x0021);
	write_cmos_sensor(0x6F12, 0xADF8);
	write_cmos_sensor(0x6F12, 0x0010);
	write_cmos_sensor(0x6F12, 0x0222);
	write_cmos_sensor(0x6F12, 0x6946);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x89FA);
	write_cmos_sensor(0x6F12, 0xBDF8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x6F12, 0x01D0);
	write_cmos_sensor(0x6F12, 0xA4F8);
	write_cmos_sensor(0x6F12, 0x4004);
	write_cmos_sensor(0x6F12, 0x38BD);
	write_cmos_sensor(0x6F12, 0x70B5);
	write_cmos_sensor(0x6F12, 0x0446);
	write_cmos_sensor(0x6F12, 0x3748);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xC06D);
	write_cmos_sensor(0x6F12, 0x86B2);
	write_cmos_sensor(0x6F12, 0x050C);
	write_cmos_sensor(0x6F12, 0x3146);
	write_cmos_sensor(0x6F12, 0x2846);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xEBF9);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xB5FA);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x3146);
	write_cmos_sensor(0x6F12, 0x2846);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xE3F9);
	write_cmos_sensor(0x6F12, 0x2188);
	write_cmos_sensor(0x6F12, 0x4FF0);
	write_cmos_sensor(0x6F12, 0x8040);
	write_cmos_sensor(0x6F12, 0xB0FB);
	write_cmos_sensor(0x6F12, 0xF1F1);
	write_cmos_sensor(0x6F12, 0x01F5);
	write_cmos_sensor(0x6F12, 0x0071);
	write_cmos_sensor(0x6F12, 0x890A);
	write_cmos_sensor(0x6F12, 0xE180);
	write_cmos_sensor(0x6F12, 0x6188);
	write_cmos_sensor(0x6F12, 0xB0FB);
	write_cmos_sensor(0x6F12, 0xF1F1);
	write_cmos_sensor(0x6F12, 0x01F5);
	write_cmos_sensor(0x6F12, 0x0071);
	write_cmos_sensor(0x6F12, 0x890A);
	write_cmos_sensor(0x6F12, 0x2181);
	write_cmos_sensor(0x6F12, 0xA188);
	write_cmos_sensor(0x6F12, 0xB0FB);
	write_cmos_sensor(0x6F12, 0xF1F0);
	write_cmos_sensor(0x6F12, 0x00F5);
	write_cmos_sensor(0x6F12, 0x0070);
	write_cmos_sensor(0x6F12, 0x800A);
	write_cmos_sensor(0x6F12, 0x6081);
	write_cmos_sensor(0x6F12, 0x70BD);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF05F);
	write_cmos_sensor(0x6F12, 0x0646);
	write_cmos_sensor(0x6F12, 0x2148);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x016E);
	write_cmos_sensor(0x6F12, 0x0C0C);
	write_cmos_sensor(0x6F12, 0x8DB2);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xBFF9);
	write_cmos_sensor(0x6F12, 0x3046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x8EFA);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xB7F9);
	write_cmos_sensor(0x6F12, 0x1648);
	write_cmos_sensor(0x6F12, 0x90F8);
	write_cmos_sensor(0x6F12, 0x4603);
	write_cmos_sensor(0x6F12, 0x3043);
	write_cmos_sensor(0x6F12, 0x01D0);
	write_cmos_sensor(0x6F12, 0x0121);
	write_cmos_sensor(0x6F12, 0x00E0);
	write_cmos_sensor(0x6F12, 0x0021);
	write_cmos_sensor(0x6F12, 0x1A4B);
	write_cmos_sensor(0x6F12, 0x1548);
	write_cmos_sensor(0x6F12, 0x0124);
	write_cmos_sensor(0x6F12, 0x79B1);
	write_cmos_sensor(0x6F12, 0x93F8);
	write_cmos_sensor(0x6F12, 0xBE20);
	write_cmos_sensor(0x6F12, 0x164D);
	write_cmos_sensor(0x6F12, 0xB3F8);
	write_cmos_sensor(0x6F12, 0xD810);
	write_cmos_sensor(0x6F12, 0x05EB);
	write_cmos_sensor(0x6F12, 0x4202);
	write_cmos_sensor(0x6F12, 0xB2F8);
	write_cmos_sensor(0x6F12, 0xDA21);
	write_cmos_sensor(0x6F12, 0x891A);
	write_cmos_sensor(0x6F12, 0x144A);
	write_cmos_sensor(0x6F12, 0x491E);
	write_cmos_sensor(0x6F12, 0x1180);
	write_cmos_sensor(0x6F12, 0x4178);
	write_cmos_sensor(0x6F12, 0x4177);
	write_cmos_sensor(0x6F12, 0x0477);
	write_cmos_sensor(0x6F12, 0x114A);
	write_cmos_sensor(0x6F12, 0x0025);
	write_cmos_sensor(0x6F12, 0x283A);
	write_cmos_sensor(0x6F12, 0x161D);
	write_cmos_sensor(0x6F12, 0x371D);
	write_cmos_sensor(0x6F12, 0x06F1);
	write_cmos_sensor(0x6F12, 0x0609);
	write_cmos_sensor(0x6F12, 0x417F);
	write_cmos_sensor(0x6F12, 0x02F1);
	write_cmos_sensor(0x6F12, 0x0208);
	write_cmos_sensor(0x6F12, 0x07F1);
	write_cmos_sensor(0x6F12, 0x040C);
	write_cmos_sensor(0x6F12, 0x09F1);
	write_cmos_sensor(0x6F12, 0x060A);
	write_cmos_sensor(0x6F12, 0x16E0);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x64C0);
	write_cmos_sensor(0x6F12, 0x2001);
	write_cmos_sensor(0x6F12, 0x01B0);
	write_cmos_sensor(0x6F12, 0x4000);
	write_cmos_sensor(0x6F12, 0x7722);
	write_cmos_sensor(0x6F12, 0x2001);
	write_cmos_sensor(0x6F12, 0x32D0);
	write_cmos_sensor(0x6F12, 0x2001);
	write_cmos_sensor(0x6F12, 0xF600);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x3AB0);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x0C50);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x3530);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x2530);
	write_cmos_sensor(0x6F12, 0x2001);
	write_cmos_sensor(0x6F12, 0x1420);
	write_cmos_sensor(0x6F12, 0x4000);
	write_cmos_sensor(0x6F12, 0xF852);
	write_cmos_sensor(0x6F12, 0x0AF1);
	write_cmos_sensor(0x6F12, 0x020B);
	write_cmos_sensor(0x6F12, 0x0AF1);
	write_cmos_sensor(0x6F12, 0x040E);
	write_cmos_sensor(0x6F12, 0x21B3);
	write_cmos_sensor(0x6F12, 0x037F);
	write_cmos_sensor(0x6F12, 0xDBB1);
	write_cmos_sensor(0x6F12, 0x4388);
	write_cmos_sensor(0x6F12, 0x1380);
	write_cmos_sensor(0x6F12, 0x8288);
	write_cmos_sensor(0x6F12, 0xA8F8);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0xC288);
	write_cmos_sensor(0x6F12, 0x3280);
	write_cmos_sensor(0x6F12, 0x0289);
	write_cmos_sensor(0x6F12, 0x3A80);
	write_cmos_sensor(0x6F12, 0x4389);
	write_cmos_sensor(0x6F12, 0xA9F8);
	write_cmos_sensor(0x6F12, 0x0030);
	write_cmos_sensor(0x6F12, 0x8289);
	write_cmos_sensor(0x6F12, 0xACF8);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0xC389);
	write_cmos_sensor(0x6F12, 0xAAF8);
	write_cmos_sensor(0x6F12, 0x0030);
	write_cmos_sensor(0x6F12, 0x038A);
	write_cmos_sensor(0x6F12, 0xABF8);
	write_cmos_sensor(0x6F12, 0x0030);
	write_cmos_sensor(0x6F12, 0x438A);
	write_cmos_sensor(0x6F12, 0xAEF8);
	write_cmos_sensor(0x6F12, 0x0030);
	write_cmos_sensor(0x6F12, 0x824A);
	write_cmos_sensor(0x6F12, 0x838A);
	write_cmos_sensor(0x6F12, 0x1380);
	write_cmos_sensor(0x6F12, 0x0577);
	write_cmos_sensor(0x6F12, 0x491E);
	write_cmos_sensor(0x6F12, 0x11F0);
	write_cmos_sensor(0x6F12, 0xFF01);
	write_cmos_sensor(0x6F12, 0x4177);
	write_cmos_sensor(0x6F12, 0x00D1);
	write_cmos_sensor(0x6F12, 0x0477);
	write_cmos_sensor(0x6F12, 0x8AE6);
	write_cmos_sensor(0x6F12, 0x017F);
	write_cmos_sensor(0x6F12, 0x0029);
	write_cmos_sensor(0x6F12, 0xFBD0);
	write_cmos_sensor(0x6F12, 0xB3F8);
	write_cmos_sensor(0x6F12, 0xBE18);
	write_cmos_sensor(0x6F12, 0x1180);
	write_cmos_sensor(0x6F12, 0xB3F8);
	write_cmos_sensor(0x6F12, 0xC018);
	write_cmos_sensor(0x6F12, 0xA8F8);
	write_cmos_sensor(0x6F12, 0x0010);
	write_cmos_sensor(0x6F12, 0xB3F8);
	write_cmos_sensor(0x6F12, 0xC218);
	write_cmos_sensor(0x6F12, 0x3180);
	write_cmos_sensor(0x6F12, 0xB3F8);
	write_cmos_sensor(0x6F12, 0xC618);
	write_cmos_sensor(0x6F12, 0x3980);
	write_cmos_sensor(0x6F12, 0xB3F8);
	write_cmos_sensor(0x6F12, 0xC828);
	write_cmos_sensor(0x6F12, 0xA9F8);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0xB3F8);
	write_cmos_sensor(0x6F12, 0xCA18);
	write_cmos_sensor(0x6F12, 0xACF8);
	write_cmos_sensor(0x6F12, 0x0010);
	write_cmos_sensor(0x6F12, 0xB3F8);
	write_cmos_sensor(0x6F12, 0xCE28);
	write_cmos_sensor(0x6F12, 0xAAF8);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0xB3F8);
	write_cmos_sensor(0x6F12, 0xD028);
	write_cmos_sensor(0x6F12, 0xABF8);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0xB3F8);
	write_cmos_sensor(0x6F12, 0xD228);
	write_cmos_sensor(0x6F12, 0xAEF8);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x6B49);
	write_cmos_sensor(0x6F12, 0xB1F8);
	write_cmos_sensor(0x6F12, 0x4024);
	write_cmos_sensor(0x6F12, 0x6949);
	write_cmos_sensor(0x6F12, 0x0A80);
	write_cmos_sensor(0x6F12, 0x0577);
	write_cmos_sensor(0x6F12, 0x5FE6);
	write_cmos_sensor(0x6F12, 0x70B5);
	write_cmos_sensor(0x6F12, 0x684C);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF6);
	write_cmos_sensor(0x6F12, 0x5361);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x6748);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0x674D);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x2860);
	write_cmos_sensor(0x6F12, 0xAFF6);
	write_cmos_sensor(0x6F12, 0x1B51);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x6548);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0x6860);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF6);
	write_cmos_sensor(0x6F12, 0xE741);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x6248);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0xA860);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF6);
	write_cmos_sensor(0x6F12, 0x6B41);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x6048);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0xE860);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF6);
	write_cmos_sensor(0x6F12, 0x4941);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x5D48);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0x2861);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF6);
	write_cmos_sensor(0x6F12, 0xF331);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x5B48);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0x6861);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF6);
	write_cmos_sensor(0x6F12, 0xD331);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x5848);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0xA861);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF6);
	write_cmos_sensor(0x6F12, 0x8531);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x5648);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0xE861);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF6);
	write_cmos_sensor(0x6F12, 0x2B31);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x5348);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0x2862);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF6);
	write_cmos_sensor(0x6F12, 0xF921);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x5148);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF6);
	write_cmos_sensor(0x6F12, 0xA521);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x4F48);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0x6862);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF6);
	write_cmos_sensor(0x6F12, 0x4721);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x4C48);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0xA862);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF6);
	write_cmos_sensor(0x6F12, 0x4721);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x4A48);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0xE862);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF6);
	write_cmos_sensor(0x6F12, 0xBF11);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x4748);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0x2863);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF6);
	write_cmos_sensor(0x6F12, 0x9F11);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x4548);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0x6863);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF6);
	write_cmos_sensor(0x6F12, 0x6911);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x4248);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0xA863);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF6);
	write_cmos_sensor(0x6F12, 0x4711);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x4048);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0xE863);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF6);
	write_cmos_sensor(0x6F12, 0x1B11);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x3D48);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0x2864);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x7B71);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x3B48);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0x6864);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x4770);
	write_cmos_sensor(0x6F12, 0xC4F8);
	write_cmos_sensor(0x6F12, 0x8000);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0xEB51);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x3648);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0xA864);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0xC751);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x3448);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0xE864);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x9F51);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x3148);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0x2865);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x8141);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x2F48);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0x6865);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0xE331);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x2C48);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0xA865);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0xA131);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x2A48);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x7731);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x2848);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x4D31);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x2648);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x3131);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x2448);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0xE865);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0xE721);
	write_cmos_sensor(0x6F12, 0x236A);
	write_cmos_sensor(0x6F12, 0x2148);
	write_cmos_sensor(0x6F12, 0x9847);
	write_cmos_sensor(0x6F12, 0x2866);
	write_cmos_sensor(0x6F12, 0x70BD);
	write_cmos_sensor(0x6F12, 0x4000);
	write_cmos_sensor(0x6F12, 0xF456);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x3530);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x6DF0);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0x2969);
	write_cmos_sensor(0x6F12, 0x2001);
	write_cmos_sensor(0x6F12, 0x32D0);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0x2CE7);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0x3325);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0x7715);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x6F12, 0x0909);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0xC449);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0x2697);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0x2585);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0x255F);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0xDBAD);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0x63D7);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0xD311);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0x8267);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0xE1FF);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x1537);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0xE0A9);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x7721);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x8B55);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0xB965);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0xC3F3);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0xC6AF);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0xCC03);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0xCDCD);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0xCEA1);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x7B65);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0xEAA1);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0xECCB);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x4463);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0x6C37);
	write_cmos_sensor(0x6F12, 0x43F2);
	write_cmos_sensor(0x6F12, 0x593C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x020C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x46F2);
	write_cmos_sensor(0x6F12, 0xD77C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4DF2);
	write_cmos_sensor(0x6F12, 0xB52C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x44F6);
	write_cmos_sensor(0x6F12, 0x7D5C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4FF6);
	write_cmos_sensor(0x6F12, 0x2B7C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4CF6);
	write_cmos_sensor(0x6F12, 0xE57C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x43F2);
	write_cmos_sensor(0x6F12, 0x253C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4DF2);
	write_cmos_sensor(0x6F12, 0x6B0C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x47F2);
	write_cmos_sensor(0x6F12, 0x157C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x40F6);
	write_cmos_sensor(0x6F12, 0x091C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x020C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4FF6);
	write_cmos_sensor(0x6F12, 0x1B7C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4FF6);
	write_cmos_sensor(0x6F12, 0x237C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4CF6);
	write_cmos_sensor(0x6F12, 0x057C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4CF2);
	write_cmos_sensor(0x6F12, 0x494C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x42F2);
	write_cmos_sensor(0x6F12, 0x976C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x44F2);
	write_cmos_sensor(0x6F12, 0x950C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x42F2);
	write_cmos_sensor(0x6F12, 0x855C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x42F2);
	write_cmos_sensor(0x6F12, 0x5F5C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x40F2);
	write_cmos_sensor(0x6F12, 0x6F5C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x020C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4CF6);
	write_cmos_sensor(0x6F12, 0xED7C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x44F6);
	write_cmos_sensor(0x6F12, 0xDD5C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4EF6);
	write_cmos_sensor(0x6F12, 0xE35C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x46F2);
	write_cmos_sensor(0x6F12, 0xD73C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4EF2);
	write_cmos_sensor(0x6F12, 0xAF0C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x46F6);
	write_cmos_sensor(0x6F12, 0xA35C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x41F2);
	write_cmos_sensor(0x6F12, 0x375C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4FF6);
	write_cmos_sensor(0x6F12, 0x4F1C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4EF2);
	write_cmos_sensor(0x6F12, 0xA90C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x47F2);
	write_cmos_sensor(0x6F12, 0x217C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4CF6);
	write_cmos_sensor(0x6F12, 0x654C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x48F6);
	write_cmos_sensor(0x6F12, 0x3F3C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x41F6);
	write_cmos_sensor(0x6F12, 0xF12C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4BF6);
	write_cmos_sensor(0x6F12, 0x651C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x48F6);
	write_cmos_sensor(0x6F12, 0x351C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x44F6);
	write_cmos_sensor(0x6F12, 0x915C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4FF6);
	write_cmos_sensor(0x6F12, 0xA54C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4BF6);
	write_cmos_sensor(0x6F12, 0x396C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4FF6);
	write_cmos_sensor(0x6F12, 0x7F4C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4CF2);
	write_cmos_sensor(0x6F12, 0xAF6C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4CF2);
	write_cmos_sensor(0x6F12, 0xF33C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4BF6);
	write_cmos_sensor(0x6F12, 0xE95C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4CF6);
	write_cmos_sensor(0x6F12, 0xCD5C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4CF2);
	write_cmos_sensor(0x6F12, 0x2D7C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4CF6);
	write_cmos_sensor(0x6F12, 0xA16C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x44F2);
	write_cmos_sensor(0x6F12, 0x634C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x46F6);
	write_cmos_sensor(0x6F12, 0x374C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x602A, 0x3334);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0548);
	write_cmos_sensor(0x6F12, 0x0449);
	write_cmos_sensor(0x6F12, 0x054A);
	write_cmos_sensor(0x6F12, 0xC0F8);
	write_cmos_sensor(0x6F12, 0x9818);
	write_cmos_sensor(0x6F12, 0x511A);
	write_cmos_sensor(0x6F12, 0xC0F8);
	write_cmos_sensor(0x6F12, 0x9C18);
	write_cmos_sensor(0x6F12, 0xFFF7);
	write_cmos_sensor(0x6F12, 0xC3BD);
	write_cmos_sensor(0x6F12, 0x2001);
	write_cmos_sensor(0x6F12, 0x3380);
	write_cmos_sensor(0x6F12, 0x2001);
	write_cmos_sensor(0x6F12, 0x01B0);
	write_cmos_sensor(0x6F12, 0x2001);
	write_cmos_sensor(0x6F12, 0xF600);
	write_cmos_sensor(0x6F12, 0x7047);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x3841);
	write_cmos_sensor(0x6F12, 0x02FB);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x03FF);
	write_cmos_sensor(0x6F12, 0xFFFF);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x13C2);
	write_cmos_sensor(0x6F12, 0x000B);
	write_cmos_sensor(0x602A, 0x13C8);
	write_cmos_sensor(0x6F12, 0x0101);
	write_cmos_sensor(0x6F12, 0x0101);
	write_cmos_sensor(0x602A, 0x143E);
	write_cmos_sensor(0x6F12, 0x0200);
	write_cmos_sensor(0x602A, 0x2040);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x2064);
	write_cmos_sensor(0x6F12, 0x4000);
	write_cmos_sensor(0x6F12, 0x9F00);
	write_cmos_sensor(0x602A, 0x245C);
	write_cmos_sensor(0x6F12, 0x0101);
	write_cmos_sensor(0x602A, 0x2520);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x602A, 0x2546);
	write_cmos_sensor(0x6F12, 0x2710);
	write_cmos_sensor(0x602A, 0x25DA);
	write_cmos_sensor(0x6F12, 0x0304);
	write_cmos_sensor(0x602A, 0x25E0);
	write_cmos_sensor(0x6F12, 0x0800);
	write_cmos_sensor(0x602A, 0x25FA);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x602A, 0x261A);
	write_cmos_sensor(0x6F12, 0x0007);
	write_cmos_sensor(0x6F12, 0x0007);
	write_cmos_sensor(0x6F12, 0x0007);
	write_cmos_sensor(0x602A, 0x263A);
	write_cmos_sensor(0x6F12, 0x0007);
	write_cmos_sensor(0x6F12, 0x0007);
	write_cmos_sensor(0x6F12, 0x0007);
	write_cmos_sensor(0x602A, 0x264A);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x602A, 0x265A);
	write_cmos_sensor(0x6F12, 0x0007);
	write_cmos_sensor(0x6F12, 0x0007);
	write_cmos_sensor(0x6F12, 0x0007);
	write_cmos_sensor(0x602A, 0x267A);
	write_cmos_sensor(0x6F12, 0x0007);
	write_cmos_sensor(0x6F12, 0x0007);
	write_cmos_sensor(0x6F12, 0x0007);
	write_cmos_sensor(0x602A, 0x26B2);
	write_cmos_sensor(0x6F12, 0x0027);
	write_cmos_sensor(0x6F12, 0x0027);
	write_cmos_sensor(0x6F12, 0x0027);
	write_cmos_sensor(0x602A, 0x26C2);
	write_cmos_sensor(0x6F12, 0x003B);
	write_cmos_sensor(0x6F12, 0x003B);
	write_cmos_sensor(0x6F12, 0x003B);
	write_cmos_sensor(0x602A, 0x26E2);
	write_cmos_sensor(0x6F12, 0x0008);
	write_cmos_sensor(0x6F12, 0x0008);
	write_cmos_sensor(0x602A, 0x275A);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x000E);
	write_cmos_sensor(0x6F12, 0x002E);
	write_cmos_sensor(0x6F12, 0x014F);
	write_cmos_sensor(0x602A, 0x277A);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x000E);
	write_cmos_sensor(0x6F12, 0x002E);
	write_cmos_sensor(0x6F12, 0x014F);
	write_cmos_sensor(0x602A, 0x35E4);
	write_cmos_sensor(0x6F12, 0x0042);
	write_cmos_sensor(0x602A, 0x35EE);
	write_cmos_sensor(0x6F12, 0x00BD);
	write_cmos_sensor(0x602A, 0x38BC);
	write_cmos_sensor(0x6F12, 0x41AE);
	write_cmos_sensor(0x602A, 0x38CA);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x3970);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x0048);
	write_cmos_sensor(0x6F12, 0x0019);
	write_cmos_sensor(0x6F12, 0x0011);
	write_cmos_sensor(0x6F12, 0x00C0);
	write_cmos_sensor(0x602A, 0x3996);
	write_cmos_sensor(0x6F12, 0x01C0);
	write_cmos_sensor(0x6F12, 0x01D0);
	write_cmos_sensor(0x6F12, 0x01E0);
	write_cmos_sensor(0x6F12, 0x01F0);
	write_cmos_sensor(0x602A, 0x39A2);
	write_cmos_sensor(0x6F12, 0xE05E);
	write_cmos_sensor(0x602A, 0x39AA);
	write_cmos_sensor(0x6F12, 0xE05E);
	write_cmos_sensor(0x602A, 0x39B2);
	write_cmos_sensor(0x6F12, 0x9F1E);
	write_cmos_sensor(0x602A, 0x3AEE);
	write_cmos_sensor(0x6F12, 0x004A);
	write_cmos_sensor(0x602A, 0x3F10);
	write_cmos_sensor(0x6F12, 0x193C);
	write_cmos_sensor(0x602A, 0x4060);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x4100);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x5144);
	write_cmos_sensor(0x6F12, 0x1441);
	write_cmos_sensor(0x6F12, 0x2825);
	write_cmos_sensor(0x602A, 0x6D60);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x6D6E);
	write_cmos_sensor(0x6F12, 0x0080);
	write_cmos_sensor(0x602A, 0x6D8A);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x6D94);
	write_cmos_sensor(0x6F12, 0x0080);
	write_cmos_sensor(0x6F12, 0x0080);
	write_cmos_sensor(0x6F12, 0x0080);
	write_cmos_sensor(0x602A, 0x6DAC);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x0200);
	write_cmos_sensor(0x6F12, 0x0200);
	write_cmos_sensor(0x6F12, 0x0200);
	write_cmos_sensor(0x0110, 0x1002);
	write_cmos_sensor(0x0200, 0xFFFF);
	write_cmos_sensor(0x0224, 0xFFFF);
	write_cmos_sensor(0x022A, 0xFFFF);
	write_cmos_sensor(0x0260, 0x0002);
	write_cmos_sensor(0x0262, 0x0001);
	write_cmos_sensor(0x0264, 0x0203);
	write_cmos_sensor(0x0268, 0x1230);
	write_cmos_sensor(0x026A, 0x2B2B);
	write_cmos_sensor(0x026C, 0x2B30);
	write_cmos_sensor(0x0B08, 0x0101);
	write_cmos_sensor(0x0FE8, 0x42C1);
	write_cmos_sensor(0xBF06, 0x0206);
	write_cmos_sensor(0xF440, 0x000F);
	write_cmos_sensor(0xF442, 0x001F);
	write_cmos_sensor(0xFB0A, 0x0567);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x612C);
	write_cmos_sensor(0x6F12, 0x0035);
	write_cmos_sensor(0x602A, 0x6132);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x602A, 0x614A);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x6358);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x602A, 0x6368);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x602A, 0x6378);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x602A, 0x6388);
	write_cmos_sensor(0x6F12, 0x00C8);
	write_cmos_sensor(0x6F12, 0x00C8);
	write_cmos_sensor(0x6F12, 0x00C8);
	write_cmos_sensor(0x6F12, 0x00C8);
	write_cmos_sensor(0x6F12, 0x00C8);
	write_cmos_sensor(0x6F12, 0x00C8);
	write_cmos_sensor(0x602A, 0x6398);
	write_cmos_sensor(0x6F12, 0x00C8);
	write_cmos_sensor(0x6F12, 0x00C8);
	write_cmos_sensor(0x6F12, 0x00C8);
	write_cmos_sensor(0x6F12, 0x00C8);
	write_cmos_sensor(0x6F12, 0x00C8);
	write_cmos_sensor(0x6F12, 0x00C8);
	write_cmos_sensor(0x602A, 0x63A8);
	write_cmos_sensor(0x6F12, 0x0032);
	write_cmos_sensor(0x6F12, 0x0032);
	write_cmos_sensor(0x6F12, 0x0032);
	write_cmos_sensor(0x6F12, 0x0032);
	write_cmos_sensor(0x6F12, 0x0032);
	write_cmos_sensor(0x6F12, 0x0032);
	write_cmos_sensor(0x602A, 0x63B8);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x602A, 0x63C8);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x602A, 0x63D8);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x602A, 0x63E8);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x602A, 0x63F8);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x6F12, 0x006E);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x6F12, 0x005A);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x602A, 0x6408);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x6F12, 0x006E);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x6F12, 0x005A);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x602A, 0x6418);
	write_cmos_sensor(0x6F12, 0x005A);
	write_cmos_sensor(0x6F12, 0x005A);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x6F12, 0x0096);
	write_cmos_sensor(0x6F12, 0x00B4);
	write_cmos_sensor(0x6F12, 0x00B4);
	write_cmos_sensor(0x602A, 0x6428);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x6F12, 0x0085);
	write_cmos_sensor(0x6F12, 0x00A7);
	write_cmos_sensor(0x602A, 0x6438);
	write_cmos_sensor(0x6F12, 0x0053);
	write_cmos_sensor(0x6F12, 0x0053);
	write_cmos_sensor(0x6F12, 0x0070);
	write_cmos_sensor(0x6F12, 0x008B);
	write_cmos_sensor(0x6F12, 0x00A7);
	write_cmos_sensor(0x6F12, 0x00A7);
	write_cmos_sensor(0x602A, 0x6448);
	write_cmos_sensor(0x6F12, 0x0053);
	write_cmos_sensor(0x6F12, 0x0053);
	write_cmos_sensor(0x6F12, 0x0070);
	write_cmos_sensor(0x6F12, 0x008B);
	write_cmos_sensor(0x6F12, 0x00A7);
	write_cmos_sensor(0x6F12, 0x00A7);
	write_cmos_sensor(0x602A, 0x6458);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x6F12, 0x0086);
	write_cmos_sensor(0x6F12, 0x00A7);
	write_cmos_sensor(0x6F12, 0x00C8);
	write_cmos_sensor(0x6F12, 0x00C8);
	write_cmos_sensor(0x602A, 0x6468);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x001B);
	write_cmos_sensor(0x6F12, 0x0021);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x602A, 0x6478);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x001B);
	write_cmos_sensor(0x6F12, 0x0021);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x602A, 0x6488);
	write_cmos_sensor(0x6F12, 0x001B);
	write_cmos_sensor(0x6F12, 0x001B);
	write_cmos_sensor(0x6F12, 0x0023);
	write_cmos_sensor(0x6F12, 0x002D);
	write_cmos_sensor(0x6F12, 0x0036);
	write_cmos_sensor(0x6F12, 0x0036);
	write_cmos_sensor(0x602A, 0x6498);
	write_cmos_sensor(0x6F12, 0x003C);
	write_cmos_sensor(0x6F12, 0x003C);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x602A, 0x64A8);
	write_cmos_sensor(0x6F12, 0x003C);
	write_cmos_sensor(0x6F12, 0x003C);
	write_cmos_sensor(0x6F12, 0x0050);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x602A, 0x5FE2);
	write_cmos_sensor(0x6F12, 0x2008);
	write_cmos_sensor(0x602A, 0x5FE8);
	write_cmos_sensor(0x6F12, 0x2008);
	write_cmos_sensor(0x602A, 0x5FEE);
	write_cmos_sensor(0x6F12, 0x2008);
	write_cmos_sensor(0x602A, 0x5FF4);
	write_cmos_sensor(0x6F12, 0x003C);
	write_cmos_sensor(0x602A, 0x5FFA);
	write_cmos_sensor(0x6F12, 0x003C);
	write_cmos_sensor(0x602A, 0x6000);
	write_cmos_sensor(0x6F12, 0x003C);
	write_cmos_sensor(0x602A, 0x600C);
	write_cmos_sensor(0x6F12, 0x2008);
	write_cmos_sensor(0x602A, 0x6012);
	write_cmos_sensor(0x6F12, 0x2008);
	write_cmos_sensor(0x602A, 0x6018);
	write_cmos_sensor(0x6F12, 0x2008);
	write_cmos_sensor(0x602A, 0x601E);
	write_cmos_sensor(0x6F12, 0x003C);
	write_cmos_sensor(0x602A, 0x6024);
	write_cmos_sensor(0x6F12, 0x003C);
	write_cmos_sensor(0x602A, 0x602A);
	write_cmos_sensor(0x6F12, 0x003C);
	write_cmos_sensor(0x602A, 0x6036);
	write_cmos_sensor(0x6F12, 0x2008);
	write_cmos_sensor(0x602A, 0x603C);
	write_cmos_sensor(0x6F12, 0x2008);
	write_cmos_sensor(0x602A, 0x6042);
	write_cmos_sensor(0x6F12, 0x2008);
	write_cmos_sensor(0x602A, 0x6048);
	write_cmos_sensor(0x6F12, 0x003C);
	write_cmos_sensor(0x602A, 0x604E);
	write_cmos_sensor(0x6F12, 0x003C);
	write_cmos_sensor(0x602A, 0x6054);
	write_cmos_sensor(0x6F12, 0x003C);
	write_cmos_sensor(0x602A, 0x6066);
	write_cmos_sensor(0x6F12, 0x2008);
	write_cmos_sensor(0x602A, 0x606C);
	write_cmos_sensor(0x6F12, 0x2008);
	write_cmos_sensor(0x602A, 0x6072);
	write_cmos_sensor(0x6F12, 0x2008);
	write_cmos_sensor(0x602A, 0x6078);
	write_cmos_sensor(0x6F12, 0x003C);
	write_cmos_sensor(0x602A, 0x607E);
	write_cmos_sensor(0x6F12, 0x003C);
	write_cmos_sensor(0x602A, 0x6084);
	write_cmos_sensor(0x6F12, 0x003C);
	write_cmos_sensor(0x602A, 0x608A);
	write_cmos_sensor(0x6F12, 0x0384);
	write_cmos_sensor(0x602A, 0x6090);
	write_cmos_sensor(0x6F12, 0x0384);
	write_cmos_sensor(0x602A, 0x6096);
	write_cmos_sensor(0x6F12, 0x0514);
	write_cmos_sensor(0x602A, 0x609C);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x602A, 0x60A2);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x602A, 0x60A8);
	write_cmos_sensor(0x6F12, 0x0064);
	write_cmos_sensor(0x602A, 0x60AE);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x60B4);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x60BA);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x60C0);
	write_cmos_sensor(0x6F12, 0x0006);
	write_cmos_sensor(0x602A, 0x60CC);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x602A, 0x60DE);
	write_cmos_sensor(0x6F12, 0x008C);
	write_cmos_sensor(0x602A, 0x6108);
	write_cmos_sensor(0x6F12, 0x0007);
	write_cmos_sensor(0x602A, 0x6120);
	write_cmos_sensor(0x6F12, 0x0C04);
	write_cmos_sensor(0x602A, 0x6178);
	write_cmos_sensor(0x6F12, 0x0230);
	write_cmos_sensor(0x6F12, 0x0230);
	write_cmos_sensor(0x6F12, 0x0230);
	write_cmos_sensor(0x6F12, 0x0230);
	write_cmos_sensor(0x6F12, 0x0230);
	write_cmos_sensor(0x6F12, 0x0230);
	write_cmos_sensor(0x602A, 0x6198);
	write_cmos_sensor(0x6F12, 0x0666);
	write_cmos_sensor(0x6F12, 0x0666);
	write_cmos_sensor(0x6F12, 0x0666);
	write_cmos_sensor(0x6F12, 0x0666);
	write_cmos_sensor(0x6F12, 0x0666);
	write_cmos_sensor(0x6F12, 0x0666);
	write_cmos_sensor(0x602A, 0x61A8);
	write_cmos_sensor(0x6F12, 0x0023);
	write_cmos_sensor(0x6F12, 0x0023);
	write_cmos_sensor(0x6F12, 0x0023);
	write_cmos_sensor(0x6F12, 0x0023);
	write_cmos_sensor(0x6F12, 0x0023);
	write_cmos_sensor(0x6F12, 0x0023);
	write_cmos_sensor(0x602A, 0x61B8);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x602A, 0x61D8);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x602A, 0x61E8);
	write_cmos_sensor(0x6F12, 0x0444);
	write_cmos_sensor(0x6F12, 0x0444);
	write_cmos_sensor(0x6F12, 0x0444);
	write_cmos_sensor(0x6F12, 0x0444);
	write_cmos_sensor(0x6F12, 0x0444);
	write_cmos_sensor(0x6F12, 0x0444);
	write_cmos_sensor(0x602A, 0x6208);
	write_cmos_sensor(0x6F12, 0x000A);
	write_cmos_sensor(0x6F12, 0x000A);
	write_cmos_sensor(0x6F12, 0x000A);
	write_cmos_sensor(0x6F12, 0x000A);
	write_cmos_sensor(0x6F12, 0x000A);
	write_cmos_sensor(0x6F12, 0x000A);
	write_cmos_sensor(0x602A, 0x6228);
	write_cmos_sensor(0x6F12, 0x001E);
	write_cmos_sensor(0x6F12, 0x001E);
	write_cmos_sensor(0x6F12, 0x001E);
	write_cmos_sensor(0x6F12, 0x001E);
	write_cmos_sensor(0x6F12, 0x001E);
	write_cmos_sensor(0x6F12, 0x001E);
	write_cmos_sensor(0x602A, 0x6248);
	write_cmos_sensor(0x6F12, 0x0666);
	write_cmos_sensor(0x6F12, 0x0666);
	write_cmos_sensor(0x6F12, 0x0666);
	write_cmos_sensor(0x6F12, 0x0666);
	write_cmos_sensor(0x6F12, 0x0666);
	write_cmos_sensor(0x6F12, 0x0666);
	write_cmos_sensor(0x602A, 0x6268);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x602A, 0x6288);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x602A, 0x62A8);
	write_cmos_sensor(0x6F12, 0x02BC);
	write_cmos_sensor(0x6F12, 0x02BC);
	write_cmos_sensor(0x6F12, 0x02BC);
	write_cmos_sensor(0x6F12, 0x02BC);
	write_cmos_sensor(0x6F12, 0x02BC);
	write_cmos_sensor(0x6F12, 0x02BC);
	write_cmos_sensor(0x602A, 0x62B8);
	write_cmos_sensor(0x6F12, 0x02BC);
	write_cmos_sensor(0x6F12, 0x02BC);
	write_cmos_sensor(0x6F12, 0x02BC);
	write_cmos_sensor(0x6F12, 0x02BC);
	write_cmos_sensor(0x6F12, 0x02BC);
	write_cmos_sensor(0x6F12, 0x02BC);
	write_cmos_sensor(0x602A, 0x62C8);
	write_cmos_sensor(0x6F12, 0x03FF);
	write_cmos_sensor(0x6F12, 0x03FF);
	write_cmos_sensor(0x6F12, 0x03FF);
	write_cmos_sensor(0x6F12, 0x03FF);
	write_cmos_sensor(0x6F12, 0x03FF);
	write_cmos_sensor(0x6F12, 0x03FF);
	write_cmos_sensor(0x602A, 0x62D8);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x602A, 0x62E8);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x602A, 0x62F8);
	write_cmos_sensor(0x6F12, 0x0096);
	write_cmos_sensor(0x6F12, 0x0096);
	write_cmos_sensor(0x6F12, 0x0096);
	write_cmos_sensor(0x6F12, 0x0096);
	write_cmos_sensor(0x6F12, 0x0096);
	write_cmos_sensor(0x6F12, 0x0096);
	write_cmos_sensor(0x602A, 0x6308);
	write_cmos_sensor(0x6F12, 0x0190);
	write_cmos_sensor(0x6F12, 0x0190);
	write_cmos_sensor(0x6F12, 0x0190);
	write_cmos_sensor(0x6F12, 0x0190);
	write_cmos_sensor(0x6F12, 0x0190);
	write_cmos_sensor(0x6F12, 0x0190);
	write_cmos_sensor(0x602A, 0x6318);
	write_cmos_sensor(0x6F12, 0x04B0);
	write_cmos_sensor(0x6F12, 0x04B0);
	write_cmos_sensor(0x6F12, 0x04B0);
	write_cmos_sensor(0x6F12, 0x04B0);
	write_cmos_sensor(0x6F12, 0x04B0);
	write_cmos_sensor(0x6F12, 0x04B0);
	write_cmos_sensor(0x602A, 0x6328);
	write_cmos_sensor(0x6F12, 0x02BC);
	write_cmos_sensor(0x6F12, 0x02BC);
	write_cmos_sensor(0x6F12, 0x02BC);
	write_cmos_sensor(0x6F12, 0x02BC);
	write_cmos_sensor(0x6F12, 0x02BC);
	write_cmos_sensor(0x6F12, 0x02BC);
	write_cmos_sensor(0x602A, 0x6338);
	write_cmos_sensor(0x6F12, 0x0690);
	write_cmos_sensor(0x6F12, 0x0690);
	write_cmos_sensor(0x6F12, 0x0690);
	write_cmos_sensor(0x6F12, 0x0690);
	write_cmos_sensor(0x6F12, 0x0690);
	write_cmos_sensor(0x6F12, 0x0690);
	write_cmos_sensor(0x602A, 0x6348);
	write_cmos_sensor(0x6F12, 0x0096);
	write_cmos_sensor(0x6F12, 0x0096);
	write_cmos_sensor(0x6F12, 0x0096);
	write_cmos_sensor(0x6F12, 0x0096);
	write_cmos_sensor(0x6F12, 0x0096);
	write_cmos_sensor(0x6F12, 0x0096);
	write_cmos_sensor(0x6028, 0x2000); //DCG
	write_cmos_sensor(0x602A, 0x3984);
	write_cmos_sensor(0x6F12, 0xFA00);
	write_cmos_sensor(0x6F12, 0xFA00);
	write_cmos_sensor(0x6F12, 0xFA00);
	write_cmos_sensor(0x6F12, 0xFA00);
	write_cmos_sensor(0x6F12, 0x3F38);
	write_cmos_sensor(0x6F12, 0x3FBA);
	write_cmos_sensor(0x6F12, 0x3FB1);
	write_cmos_sensor(0x6F12, 0x3F36);
}				/*    MIPI_sensor_Init  */

static void check_stream_is_on(void)
{
	int i = 0;
	UINT32 framecnt;

	for (i = 0; i < 100; i++) {

		framecnt = read_cmos_sensor(0x0005);
		if (framecnt != 0xFF) {
			LOG_INF("stream is  on, %d \\n", framecnt);
			break;
		}
		LOG_INF("stream is not on %d \\n", framecnt);
		mdelay(1);
	}
}

static void check_stream_is_off(void)
{
	int i = 0;
	UINT32 framecnt;

	for (i = 0; i < 100; i++) {

		framecnt = read_cmos_sensor(0x0005);
		if (framecnt == 0xFF) {
			LOG_INF("stream is  off\\n");
			break;
		}
			LOG_INF("stream is not off\\n");
			mdelay(1);
	}
}

static void preview_setting(void)
{
	/********************************************************
	 *
	 *   3280x2460 30fps 4 lane MIPI 3000Mbps/lane
	 *
	 ********************************************************/
	LOG_INF("E\n");
	write_cmos_sensor(0x0100, 0x0000);
	check_stream_is_off();
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x013E, 0x0000);
	write_cmos_sensor(0x0304, 0x0004);
	write_cmos_sensor(0x0306, 0x0108);
	write_cmos_sensor(0x030C, 0x0000);
	write_cmos_sensor(0x0302, 0x0003);
	write_cmos_sensor(0x0300, 0x0002);
	write_cmos_sensor(0x030E, 0x0004);
	write_cmos_sensor(0x0310, 0x00FA);
	write_cmos_sensor(0x0312, 0x0000);
	write_cmos_sensor(0x030A, 0x0002);
	write_cmos_sensor(0x0308, 0x0008);
	write_cmos_sensor(0x602A, 0x35EA);
	write_cmos_sensor(0x6F12, 0x0096);
	write_cmos_sensor(0x0344, 0x0008);
	write_cmos_sensor(0x0346, 0x0000);
	write_cmos_sensor(0x0348, 0x19A7);
	write_cmos_sensor(0x034A, 0x1347);
	write_cmos_sensor(0x0350, 0x0000);
	write_cmos_sensor(0x0352, 0x0004);
	write_cmos_sensor(0x034C, 0x0CD0);
	write_cmos_sensor(0x034E, 0x099C);
	write_cmos_sensor(0x0900, 0x2222);
	write_cmos_sensor(0x040C, 0x0000);
	write_cmos_sensor(0x0400, 0x1010);
	write_cmos_sensor(0x0408, 0x0100);
	write_cmos_sensor(0x040A, 0x0100);
	write_cmos_sensor(0x0380, 0x0002);
	write_cmos_sensor(0x0382, 0x0002);
	write_cmos_sensor(0x0384, 0x0002);
	write_cmos_sensor(0x0386, 0x0002);
	write_cmos_sensor(0x602A, 0x25CC);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x0342, 0x2F4C); //Preview LL 12108
	write_cmos_sensor(0x0340, 0x0B5A); //preview FL 2906
	write_cmos_sensor(0x0114, 0x0301);
	write_cmos_sensor(0x011C, 0x0100);
	write_cmos_sensor(0x602A, 0x13EE);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0004);
	write_cmos_sensor(0x602A, 0x13F6);
	write_cmos_sensor(0x6F12, 0x0003);
	write_cmos_sensor(0x0B00, 0x0080);
	write_cmos_sensor(0x0B06, 0x0101);
	write_cmos_sensor(0x0BC6, 0x0000);
	write_cmos_sensor(0x602A, 0x1378);
	write_cmos_sensor(0x6F12, 0x00C9);
	write_cmos_sensor(0x602A, 0x5120);
	write_cmos_sensor(0x6F12, 0x0101);
	write_cmos_sensor(0x602A, 0x2060);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x3994);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x021E, 0x0000);
	write_cmos_sensor(0x0250, 0x0100);
	write_cmos_sensor(0x602A, 0x2028);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x2420);
	write_cmos_sensor(0x6F12, 0x0004);
	write_cmos_sensor(0x602A, 0x3F20);
	write_cmos_sensor(0x6F12, 0x19B2);
	write_cmos_sensor(0x602A, 0x25D8);
	write_cmos_sensor(0x6F12, 0x0404);
	write_cmos_sensor(0x602A, 0x35AA);
	write_cmos_sensor(0x6F12, 0x0401);
	write_cmos_sensor(0x602A, 0x2566);
	write_cmos_sensor(0x6F12, 0x0101);
	write_cmos_sensor(0x602A, 0x2702);
	write_cmos_sensor(0x6F12, 0x0049);
	write_cmos_sensor(0x6F12, 0x0086);
	write_cmos_sensor(0x602A, 0x270A);
	write_cmos_sensor(0x6F12, 0x0009);
	write_cmos_sensor(0x6F12, 0x000B);
	write_cmos_sensor(0x6F12, 0x002B);
	write_cmos_sensor(0x6F12, 0x014C);
	write_cmos_sensor(0x602A, 0x2712);
	write_cmos_sensor(0x6F12, 0x003B);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x602A, 0x35F0);
	write_cmos_sensor(0x6F12, 0xC25E);
	write_cmos_sensor(0x602A, 0x39CA);
	write_cmos_sensor(0x6F12, 0x0101);
	write_cmos_sensor(0x602A, 0x39D2);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x2552);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x397C);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x35E2);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x35C0);
	write_cmos_sensor(0x6F12, 0x0504);
	write_cmos_sensor(0x6F12, 0x0505);
	write_cmos_sensor(0x6F12, 0x0707);
	write_cmos_sensor(0x602A, 0x35DC);
	write_cmos_sensor(0x6F12, 0x5353);
	write_cmos_sensor(0x6F12, 0x3838);
	write_cmos_sensor(0x602A, 0x35CC);
	write_cmos_sensor(0x6F12, 0x0B08);
	write_cmos_sensor(0x6F12, 0x0F0F);
	write_cmos_sensor(0x602A, 0x39A4);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0B1F);
	write_cmos_sensor(0x602A, 0x39AC);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0B1F);
	write_cmos_sensor(0x602A, 0x39B4);
	write_cmos_sensor(0x6F12, 0x0012);
	write_cmos_sensor(0x6F12, 0x0B1F);
	write_cmos_sensor(0x602A, 0x6D70);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x602A, 0x35EC);
	write_cmos_sensor(0x6F12, 0x19C5);
	write_cmos_sensor(0x602A, 0x35D8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0480);
	write_cmos_sensor(0x602A, 0x13C4);
	write_cmos_sensor(0x6F12, 0x00DB);
	write_cmos_sensor(0xBF02, 0x0CDC);
	write_cmos_sensor(0xBF04, 0x0CDF);
	write_cmos_sensor(0xBF24, 0x04C4);
	write_cmos_sensor(0x6028, 0x2001);
	write_cmos_sensor(0x602A, 0xF600);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0x004D);
	write_cmos_sensor(0x6F12, 0x0140);
	write_cmos_sensor(0x6F12, 0x019F);
	write_cmos_sensor(0x6F12, 0x004D);
	write_cmos_sensor(0x6F12, 0x0140);
	write_cmos_sensor(0x6F12, 0x019F);
	write_cmos_sensor(0x6F12, 0x004A);
	write_cmos_sensor(0x6F12, 0x0143);
	write_cmos_sensor(0x6F12, 0x019C);
	write_cmos_sensor(0x6F12, 0x000F);
	LOG_INF("X\n");
} /* preview_setting */

static void capture_setting(void)
{
	/********************************************************
	 *
	 *   6560x4920 30fps 4 lane MIPI 3000Mbps/lane
	 *
	 ********************************************************/
	LOG_INF("E\n");
	write_cmos_sensor(0x0100, 0x0000);
	check_stream_is_off();
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x013E, 0x0000);
	write_cmos_sensor(0x0304, 0x0004);
	write_cmos_sensor(0x0306, 0x0108);
	write_cmos_sensor(0x030C, 0x0000);
	write_cmos_sensor(0x0302, 0x0003);
	write_cmos_sensor(0x0300, 0x0002);
	write_cmos_sensor(0x030E, 0x0004);
	write_cmos_sensor(0x0310, 0x00FA);
	write_cmos_sensor(0x0312, 0x0000);
	write_cmos_sensor(0x030A, 0x0002);
	write_cmos_sensor(0x0308, 0x0008);
	write_cmos_sensor(0x602A, 0x35EA);
	write_cmos_sensor(0x6F12, 0x0096);
	write_cmos_sensor(0x0344, 0x0008);
	write_cmos_sensor(0x0346, 0x0000);
	write_cmos_sensor(0x0348, 0x19A7);
	write_cmos_sensor(0x034A, 0x1347);
	write_cmos_sensor(0x0350, 0x0000);
	write_cmos_sensor(0x0352, 0x0008);
	write_cmos_sensor(0x034C, 0x19A0);
	write_cmos_sensor(0x034E, 0x1338);
	write_cmos_sensor(0x0900, 0x0011);
	write_cmos_sensor(0x040C, 0x0000);
	write_cmos_sensor(0x0400, 0x1010);
	write_cmos_sensor(0x0408, 0x0100);
	write_cmos_sensor(0x040A, 0x0100);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0001);
	write_cmos_sensor(0x602A, 0x25CC);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x0342, 0x1A80);// capture LL 6784
	write_cmos_sensor(0x0340, 0x1400);// capture FL 5120
	write_cmos_sensor(0x0114, 0x0301);
	write_cmos_sensor(0x011C, 0x0100);
	write_cmos_sensor(0x602A, 0x13EE);
	write_cmos_sensor(0x6F12, 0x0400);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x602A, 0x13F6);
	write_cmos_sensor(0x6F12, 0x050C);
	write_cmos_sensor(0x0B00, 0x0080);
	write_cmos_sensor(0x0B06, 0x0101);
	write_cmos_sensor(0x0BC6, 0x0000);
	write_cmos_sensor(0x602A, 0x1378);
	write_cmos_sensor(0x6F12, 0x00C9);
	write_cmos_sensor(0x602A, 0x5120);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x2060);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x3994);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x021E, 0x0000);
	write_cmos_sensor(0x0250, 0x0100);
	write_cmos_sensor(0x602A, 0x2028);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x2420);
	write_cmos_sensor(0x6F12, 0x0004);
	write_cmos_sensor(0x602A, 0x3F20);
	write_cmos_sensor(0x6F12, 0x19B7);
	write_cmos_sensor(0x602A, 0x25D8);
	write_cmos_sensor(0x6F12, 0x0804);
	write_cmos_sensor(0x602A, 0x35AA);
	write_cmos_sensor(0x6F12, 0x0400);
	write_cmos_sensor(0x602A, 0x2566);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x2702);
	write_cmos_sensor(0x6F12, 0x0049);
	write_cmos_sensor(0x6F12, 0x0086);
	write_cmos_sensor(0x602A, 0x270A);
	write_cmos_sensor(0x6F12, 0x0009);
	write_cmos_sensor(0x6F12, 0x000B);
	write_cmos_sensor(0x6F12, 0x002B);
	write_cmos_sensor(0x6F12, 0x014C);
	write_cmos_sensor(0x602A, 0x2712);
	write_cmos_sensor(0x6F12, 0x003B);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x602A, 0x35F0);
	write_cmos_sensor(0x6F12, 0xC256);
	write_cmos_sensor(0x602A, 0x39CA);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x39D2);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x602A, 0x2552);
	write_cmos_sensor(0x6F12, 0x0004);
	write_cmos_sensor(0x602A, 0x397C);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x35E2);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x35C0);
	write_cmos_sensor(0x6F12, 0x0505);
	write_cmos_sensor(0x6F12, 0x0505);
	write_cmos_sensor(0x6F12, 0x0707);
	write_cmos_sensor(0x602A, 0x35DC);
	write_cmos_sensor(0x6F12, 0x531C);
	write_cmos_sensor(0x6F12, 0x3838);
	write_cmos_sensor(0x602A, 0x35CC);
	write_cmos_sensor(0x6F12, 0x0B0F);
	write_cmos_sensor(0x6F12, 0x0F0F);
	write_cmos_sensor(0x602A, 0x39A4);
	write_cmos_sensor(0x6F12, 0x0012);
	write_cmos_sensor(0x6F12, 0x0B1F);
	write_cmos_sensor(0x602A, 0x39AC);
	write_cmos_sensor(0x6F12, 0x0012);
	write_cmos_sensor(0x6F12, 0x0B1F);
	write_cmos_sensor(0x602A, 0x39B4);
	write_cmos_sensor(0x6F12, 0x0012);
	write_cmos_sensor(0x6F12, 0x0B1F);
	write_cmos_sensor(0x602A, 0x6D70);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x602A, 0x35EC);
	write_cmos_sensor(0x6F12, 0x19CC);
	write_cmos_sensor(0x602A, 0x35D8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0280);
	write_cmos_sensor(0x602A, 0x13C4);
	write_cmos_sensor(0x6F12, 0x003F);
	write_cmos_sensor(0xBF02, 0x19B8);
	write_cmos_sensor(0xBF04, 0x19BF);
	write_cmos_sensor(0xBF24, 0x0988);
	write_cmos_sensor(0x6028, 0x2001);
	write_cmos_sensor(0x602A, 0xF600);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0x004D);
	write_cmos_sensor(0x6F12, 0x0103);
	write_cmos_sensor(0x6F12, 0x0162);
	write_cmos_sensor(0x6F12, 0x004D);
	write_cmos_sensor(0x6F12, 0x0103);
	write_cmos_sensor(0x6F12, 0x0162);
	write_cmos_sensor(0x6F12, 0x004A);
	write_cmos_sensor(0x6F12, 0x0106);
	write_cmos_sensor(0x6F12, 0x015F);
	write_cmos_sensor(0x6F12, 0x000F);
	LOG_INF("X\n");
} /* capture_setting */

static void normal_video_setting(void)
{
	/********************************************************
	 *
	 *   1920x1080 30fps 4 lane MIPI 375Mbps/lane
	 *
	 ********************************************************/
	LOG_INF("E\n");
	write_cmos_sensor(0x0100, 0x0000);
	check_stream_is_off();
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x013E, 0x0000);
	write_cmos_sensor(0x0304, 0x0004);
	write_cmos_sensor(0x0306, 0x0108);
	write_cmos_sensor(0x030C, 0x0000);
	write_cmos_sensor(0x0302, 0x0003);
	write_cmos_sensor(0x0300, 0x0002);
	write_cmos_sensor(0x030E, 0x0004);
	write_cmos_sensor(0x0310, 0x00FA);
	write_cmos_sensor(0x0312, 0x0003);
	write_cmos_sensor(0x030A, 0x0002);
	write_cmos_sensor(0x0308, 0x0008);
	write_cmos_sensor(0x602A, 0x35EA);
	write_cmos_sensor(0x6F12, 0x0096);
	write_cmos_sensor(0x0344, 0x0558);
	write_cmos_sensor(0x0346, 0x0560);
	write_cmos_sensor(0x0348, 0x1457);
	write_cmos_sensor(0x034A, 0x0DE7);
	write_cmos_sensor(0x0350, 0x0000);
	write_cmos_sensor(0x0352, 0x0006);
	write_cmos_sensor(0x034C, 0x0780);
	write_cmos_sensor(0x034E, 0x0438);
	write_cmos_sensor(0x0900, 0x2222);
	write_cmos_sensor(0x040C, 0x0000);
	write_cmos_sensor(0x0400, 0x1010);
	write_cmos_sensor(0x0408, 0x0100);
	write_cmos_sensor(0x040A, 0x0100);
	write_cmos_sensor(0x0380, 0x0002);
	write_cmos_sensor(0x0382, 0x0002);
	write_cmos_sensor(0x0384, 0x0002);
	write_cmos_sensor(0x0386, 0x0002);
	write_cmos_sensor(0x602A, 0x25CC);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x0342, 0x3880);// Video LL 14464
	write_cmos_sensor(0x0340, 0x0980);// video FL 2432
	write_cmos_sensor(0x0114, 0x0301);
	write_cmos_sensor(0x011C, 0x0100);
	write_cmos_sensor(0x602A, 0x13EE);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0004);
	write_cmos_sensor(0x602A, 0x13F6);
	write_cmos_sensor(0x6F12, 0x0003);
	write_cmos_sensor(0x0B00, 0x0080);
	write_cmos_sensor(0x0B06, 0x0101);
	write_cmos_sensor(0x0BC6, 0x0000);
	write_cmos_sensor(0x602A, 0x1378);
	write_cmos_sensor(0x6F12, 0x00C9);
	write_cmos_sensor(0x602A, 0x5120);
	write_cmos_sensor(0x6F12, 0x0101);
	write_cmos_sensor(0x602A, 0x2060);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x3994);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x021E, 0x0000);
	write_cmos_sensor(0x0250, 0x0100);
	write_cmos_sensor(0x602A, 0x2028);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x2420);
	write_cmos_sensor(0x6F12, 0x0004);
	write_cmos_sensor(0x602A, 0x3F20);
	write_cmos_sensor(0x6F12, 0x19B2);
	write_cmos_sensor(0x602A, 0x25D8);
	write_cmos_sensor(0x6F12, 0x0404);
	write_cmos_sensor(0x602A, 0x35AA);
	write_cmos_sensor(0x6F12, 0x0401);
	write_cmos_sensor(0x602A, 0x2566);
	write_cmos_sensor(0x6F12, 0x0101);
	write_cmos_sensor(0x602A, 0x2702);
	write_cmos_sensor(0x6F12, 0x0049);
	write_cmos_sensor(0x6F12, 0x0086);
	write_cmos_sensor(0x602A, 0x270A);
	write_cmos_sensor(0x6F12, 0x0009);
	write_cmos_sensor(0x6F12, 0x000B);
	write_cmos_sensor(0x6F12, 0x002B);
	write_cmos_sensor(0x6F12, 0x014C);
	write_cmos_sensor(0x602A, 0x2712);
	write_cmos_sensor(0x6F12, 0x003B);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x602A, 0x35F0);
	write_cmos_sensor(0x6F12, 0xC25E);
	write_cmos_sensor(0x602A, 0x39CA);
	write_cmos_sensor(0x6F12, 0x0101);
	write_cmos_sensor(0x602A, 0x39D2);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x2552);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x397C);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x35E2);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x35C0);
	write_cmos_sensor(0x6F12, 0x0504);
	write_cmos_sensor(0x6F12, 0x0505);
	write_cmos_sensor(0x6F12, 0x0707);
	write_cmos_sensor(0x602A, 0x35DC);
	write_cmos_sensor(0x6F12, 0x5353);
	write_cmos_sensor(0x6F12, 0x3838);
	write_cmos_sensor(0x602A, 0x35CC);
	write_cmos_sensor(0x6F12, 0x0B08);
	write_cmos_sensor(0x6F12, 0x0F0F);
	write_cmos_sensor(0x602A, 0x39A4);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0B1F);
	write_cmos_sensor(0x602A, 0x39AC);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0B1F);
	write_cmos_sensor(0x602A, 0x39B4);
	write_cmos_sensor(0x6F12, 0x0012);
	write_cmos_sensor(0x6F12, 0x0B1F);
	write_cmos_sensor(0x602A, 0x6D70);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x602A, 0x35EC);
	write_cmos_sensor(0x6F12, 0x19C5);
	write_cmos_sensor(0x602A, 0x35D8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0480);
	write_cmos_sensor(0x602A, 0x13C4);
	write_cmos_sensor(0x6F12, 0x00DB);
	write_cmos_sensor(0xBF02, 0x0CDC);
	write_cmos_sensor(0xBF04, 0x0CDF);
	write_cmos_sensor(0xBF24, 0x04C4);
	write_cmos_sensor(0x6028, 0x2001);
	write_cmos_sensor(0x602A, 0xF600);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0x004D);
	write_cmos_sensor(0x6F12, 0x0140);
	write_cmos_sensor(0x6F12, 0x019F);
	write_cmos_sensor(0x6F12, 0x004D);
	write_cmos_sensor(0x6F12, 0x0140);
	write_cmos_sensor(0x6F12, 0x019F);
	write_cmos_sensor(0x6F12, 0x004A);
	write_cmos_sensor(0x6F12, 0x0143);
	write_cmos_sensor(0x6F12, 0x019C);
	write_cmos_sensor(0x6F12, 0x000F);
	LOG_INF("X\n");
} /* capture_setting */

static void video_1080p_setting(void)
{
	LOG_INF("E\n");
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x013E, 0x0000);
	write_cmos_sensor(0x0304, 0x0004);
	write_cmos_sensor(0x0306, 0x0108);
	write_cmos_sensor(0x030C, 0x0000);
	write_cmos_sensor(0x0302, 0x0003);
	write_cmos_sensor(0x0300, 0x0002);
	write_cmos_sensor(0x030E, 0x0004);
	write_cmos_sensor(0x0310, 0x00FA);
	write_cmos_sensor(0x0312, 0x0000);
	write_cmos_sensor(0x030A, 0x0002);
	write_cmos_sensor(0x0308, 0x0008);
	write_cmos_sensor(0x602A, 0x35EA);
	write_cmos_sensor(0x6F12, 0x0096);
	write_cmos_sensor(0x0344, 0x0008);
	write_cmos_sensor(0x0346, 0x0000);
	write_cmos_sensor(0x0348, 0x19A7);
	write_cmos_sensor(0x034A, 0x1347);
	write_cmos_sensor(0x0350, 0x0000);
	write_cmos_sensor(0x0352, 0x0004);
	write_cmos_sensor(0x034C, 0x0CD0);
	write_cmos_sensor(0x034E, 0x099C);
	write_cmos_sensor(0x0900, 0x2222);
	write_cmos_sensor(0x040C, 0x0000);
	write_cmos_sensor(0x0400, 0x1010);
	write_cmos_sensor(0x0408, 0x0100);
	write_cmos_sensor(0x040A, 0x0100);
	write_cmos_sensor(0x0380, 0x0002);
	write_cmos_sensor(0x0382, 0x0002);
	write_cmos_sensor(0x0384, 0x0002);
	write_cmos_sensor(0x0386, 0x0002);
	write_cmos_sensor(0x602A, 0x25CC);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x0342, 0x0FC4); //video 1080 LL 4036
	write_cmos_sensor(0x0340, 0x0B5A); //video 1080 FL 2906
	write_cmos_sensor(0x0114, 0x0301);
	write_cmos_sensor(0x011C, 0x0100);
	write_cmos_sensor(0x602A, 0x13EE);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0004);
	write_cmos_sensor(0x602A, 0x13F6);
	write_cmos_sensor(0x6F12, 0x0003);
	write_cmos_sensor(0x0B00, 0x0080);
	write_cmos_sensor(0x0B06, 0x0101);
	write_cmos_sensor(0x0BC6, 0x0000);
	write_cmos_sensor(0x602A, 0x1378);
	write_cmos_sensor(0x6F12, 0x00C9);
	write_cmos_sensor(0x602A, 0x5120);
	write_cmos_sensor(0x6F12, 0x0101);
	write_cmos_sensor(0x602A, 0x2060);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x021E, 0x0000);
	write_cmos_sensor(0x0250, 0x0100);
	write_cmos_sensor(0x602A, 0x2028);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x2420);
	write_cmos_sensor(0x6F12, 0x0004);
	write_cmos_sensor(0x602A, 0x3F20);
	write_cmos_sensor(0x6F12, 0x19B2);
	write_cmos_sensor(0x602A, 0x25D8);
	write_cmos_sensor(0x6F12, 0x0404);
	write_cmos_sensor(0x602A, 0x35AA);
	write_cmos_sensor(0x6F12, 0x0401);
	write_cmos_sensor(0x602A, 0x2566);
	write_cmos_sensor(0x6F12, 0x0101);
	write_cmos_sensor(0x602A, 0x2702);
	write_cmos_sensor(0x6F12, 0x0049);
	write_cmos_sensor(0x6F12, 0x0086);
	write_cmos_sensor(0x602A, 0x2712);
	write_cmos_sensor(0x6F12, 0x003B);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x602A, 0x35F0);
	write_cmos_sensor(0x6F12, 0xC25E);
	write_cmos_sensor(0x602A, 0x39CA);
	write_cmos_sensor(0x6F12, 0x0101);
	write_cmos_sensor(0x602A, 0x39D2);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x2552);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x397C);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x35E2);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x35C0);
	write_cmos_sensor(0x6F12, 0x0504);
	write_cmos_sensor(0x6F12, 0x0505);
	write_cmos_sensor(0x6F12, 0x0707);
	write_cmos_sensor(0x602A, 0x35DC);
	write_cmos_sensor(0x6F12, 0x5353);
	write_cmos_sensor(0x6F12, 0x3838);
	write_cmos_sensor(0x602A, 0x35CC);
	write_cmos_sensor(0x6F12, 0x0B08);
	write_cmos_sensor(0x6F12, 0x0F0F);
	write_cmos_sensor(0x602A, 0x39A4);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0B1F);
	write_cmos_sensor(0x602A, 0x39AC);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0B1F);
	write_cmos_sensor(0x602A, 0x39B4);
	write_cmos_sensor(0x6F12, 0x0012);
	write_cmos_sensor(0x6F12, 0x0B1F);
	write_cmos_sensor(0x602A, 0x6D70);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x602A, 0x35EC);
	write_cmos_sensor(0x6F12, 0x19C5);
	write_cmos_sensor(0x602A, 0x35D8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0480);
	write_cmos_sensor(0x602A, 0x13C4);
	write_cmos_sensor(0x6F12, 0x00DB);
	write_cmos_sensor(0xBF02, 0x0CDC);
	write_cmos_sensor(0xBF04, 0x0CDF);
	write_cmos_sensor(0xBF24, 0x04C4);
}

static void video_720p_setting(void)
{
	/********************************************************
	 *
	 *   720p 30fps 2 lane MIPI 420Mbps/lane
	 *    @@720p_30fps
	 *     ;;pclk=84M,HTS=3728,VTS=748
	 ********************************************************/
	LOG_INF("E\n");
	write_cmos_sensor(0x0100, 0x00);	/* Stream Off */

	mdelay(5);
	write_cmos_sensor(0x3500, 0x00);	/* exposure [19:16] */
	write_cmos_sensor(0x3501, 0x2d);	/* exposure */
	write_cmos_sensor(0x3502, 0xc0);	/* exposure */

	write_cmos_sensor(0x3708, 0x66);

	write_cmos_sensor(0x3709, 0x52);
	write_cmos_sensor(0x370c, 0xcf);
	write_cmos_sensor(0x3800, 0x00);	/* x start = 16 */
	write_cmos_sensor(0x3801, 0x10);	/* x start */
	write_cmos_sensor(0x3802, 0x00);	/* y start = 254 */
	write_cmos_sensor(0x3803, 0xfe);	/* y start */
	write_cmos_sensor(0x3804, 0x0a);	/* xend = 2607 */
	write_cmos_sensor(0x3805, 0x2f);	/* xend */
	write_cmos_sensor(0x3806, 0x06);	/* yend = 1701 */
	write_cmos_sensor(0x3807, 0xa5);	/* yend */
	write_cmos_sensor(0x3808, 0x05);	/* x output size = 1280 */
	write_cmos_sensor(0x3809, 0x00);	/* x output size */
	write_cmos_sensor(0x380a, 0x02);	/* y output size = 720 */
	write_cmos_sensor(0x380b, 0xd0);	/* y output size */

	write_cmos_sensor(0x380c,
		((imgsensor_info.slim_video.linelength >> 8) & 0xFF));
	write_cmos_sensor(0x380d,
		(imgsensor_info.slim_video.linelength & 0xFF));
	write_cmos_sensor(0x380e,
		((imgsensor_info.slim_video.framelength >> 8) & 0xFF));
	write_cmos_sensor(0x380f,
		(imgsensor_info.slim_video.framelength & 0xFF));

	write_cmos_sensor(0x3810, 0x00);	/* isp x win = 8 */
	write_cmos_sensor(0x3811, 0x08);	/* isp x win */
	write_cmos_sensor(0x3812, 0x00);	/* isp y win = 2 */
	write_cmos_sensor(0x3813, 0x02);	/* isp y win */
	write_cmos_sensor(0x3814, 0x31);	/* x inc */
	write_cmos_sensor(0x3815, 0x31);	/* y inc */
	write_cmos_sensor(0x3817, 0x00);	/* hsync start */


	/********************************************************
	 *
	 *   0x3820[2] ISP Vertical flip
	 *   0x3820[1] Sensor Vertical flip
	 *
	 *   0x3821[2] ISP Horizontal mirror
	 *   0x3821[1] Sensor Horizontal mirror
	 *
	 *   ISP and Sensor flip or mirror register bit should be the same!!
	 *
	 ********************************************************/
	write_cmos_sensor(0x3820, 0x08);	/* flip off, v bin off */
	write_cmos_sensor(0x3821, 0x07);	/* mirror on, h bin on */


	write_cmos_sensor(0x4004, 0x02);	/* black line number */
	write_cmos_sensor(0x4005, 0x18);	/* blc normal freeze */


	/* write_cmos_sensor(0x350b, 0x80); // gain = 8x */
	write_cmos_sensor(0x4837, 0x18);	/* MIPI global timing */

	write_cmos_sensor(0x0100, 0x01);	/* Stream On */

	LOG_INF("Exit!");
}				/*    preview_setting  */

static void hs_video_setting(void)
{
	LOG_INF("E\n");

	video_1080p_setting();
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");

	video_720p_setting();
}
static void custom1_setting(void)
{
	/********************************************************
	 *
	 *   3280x2460 Stagger HDR 30fps 4 lane MIPI 3000Mbps/lane
	 *
	 ********************************************************/
	LOG_INF("E\n");
	write_cmos_sensor(0x0100, 0x0000);
	check_stream_is_off();
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x013E, 0x0000);
	write_cmos_sensor(0x0304, 0x0004);
	write_cmos_sensor(0x0306, 0x0108);
	write_cmos_sensor(0x030C, 0x0000);
	write_cmos_sensor(0x0302, 0x0003);
	write_cmos_sensor(0x0300, 0x0002);
	write_cmos_sensor(0x030E, 0x0004);
	write_cmos_sensor(0x0310, 0x00FA);
	write_cmos_sensor(0x0312, 0x0000);
	write_cmos_sensor(0x030A, 0x0002);
	write_cmos_sensor(0x0308, 0x0008);
	write_cmos_sensor(0x602A, 0x35EA);
	write_cmos_sensor(0x6F12, 0x0096);
	write_cmos_sensor(0x0344, 0x0008);
	write_cmos_sensor(0x0346, 0x0000);
	write_cmos_sensor(0x0348, 0x19A7);
	write_cmos_sensor(0x034A, 0x1347);
	write_cmos_sensor(0x0350, 0x0000);
	write_cmos_sensor(0x0352, 0x0004);
	write_cmos_sensor(0x034C, 0x0CD0);
	write_cmos_sensor(0x034E, 0x099C);
	write_cmos_sensor(0x0900, 0x2222);
	write_cmos_sensor(0x040C, 0x0000);
	write_cmos_sensor(0x0400, 0x1010);
	write_cmos_sensor(0x0408, 0x0100);
	write_cmos_sensor(0x040A, 0x0100);
	write_cmos_sensor(0x0380, 0x0002);
	write_cmos_sensor(0x0382, 0x0002);
	write_cmos_sensor(0x0384, 0x0002);
	write_cmos_sensor(0x0386, 0x0002);
	write_cmos_sensor(0x602A, 0x25CC);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x0342, 0x0FC4); //custom1 LL 4036
	write_cmos_sensor(0x0340, 0x220E); //custom1 FL 8718
	write_cmos_sensor(0x0114, 0x0301);
	write_cmos_sensor(0x011C, 0x0100);
	write_cmos_sensor(0x602A, 0x13EE);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0004);
	write_cmos_sensor(0x602A, 0x13F6);
	write_cmos_sensor(0x6F12, 0x0003);
	write_cmos_sensor(0x0B00, 0x0080);
	write_cmos_sensor(0x0B06, 0x0101);
	write_cmos_sensor(0x0BC6, 0x0000);
	write_cmos_sensor(0x602A, 0x1378);
	write_cmos_sensor(0x6F12, 0x00C9);
	write_cmos_sensor(0x602A, 0x5120);
	write_cmos_sensor(0x6F12, 0x0101);
	write_cmos_sensor(0x602A, 0x2060);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x3994);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x021E, 0x0300);
	write_cmos_sensor(0x0250, 0x0300);
	write_cmos_sensor(0x602A, 0x2028);
	write_cmos_sensor(0x6F12, 0x0021);
	write_cmos_sensor(0x602A, 0x2420);
	write_cmos_sensor(0x6F12, 0x0008);
	write_cmos_sensor(0x602A, 0x3F20);
	write_cmos_sensor(0x6F12, 0x19B2);
	write_cmos_sensor(0x602A, 0x25D8);
	write_cmos_sensor(0x6F12, 0x0404);
	write_cmos_sensor(0x602A, 0x35AA);
	write_cmos_sensor(0x6F12, 0x0401);
	write_cmos_sensor(0x602A, 0x2566);
	write_cmos_sensor(0x6F12, 0x0101);
	write_cmos_sensor(0x602A, 0x2702);
	write_cmos_sensor(0x6F12, 0x0049);
	write_cmos_sensor(0x6F12, 0x0086);
	write_cmos_sensor(0x602A, 0x270A);
	write_cmos_sensor(0x6F12, 0x0009);
	write_cmos_sensor(0x6F12, 0x000B);
	write_cmos_sensor(0x6F12, 0x002B);
	write_cmos_sensor(0x6F12, 0x014C);
	write_cmos_sensor(0x602A, 0x2712);
	write_cmos_sensor(0x6F12, 0x003B);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x602A, 0x35F0);
	write_cmos_sensor(0x6F12, 0xC25E);
	write_cmos_sensor(0x602A, 0x39CA);
	write_cmos_sensor(0x6F12, 0x0101);
	write_cmos_sensor(0x602A, 0x39D2);
	write_cmos_sensor(0x6F12, 0x0003);
	write_cmos_sensor(0x602A, 0x2552);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x397C);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x35E2);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x35C0);
	write_cmos_sensor(0x6F12, 0x0504);
	write_cmos_sensor(0x6F12, 0x0505);
	write_cmos_sensor(0x6F12, 0x0707);
	write_cmos_sensor(0x602A, 0x35DC);
	write_cmos_sensor(0x6F12, 0x5353);
	write_cmos_sensor(0x6F12, 0x3838);
	write_cmos_sensor(0x602A, 0x35CC);
	write_cmos_sensor(0x6F12, 0x0B08);
	write_cmos_sensor(0x6F12, 0x0F0F);
	write_cmos_sensor(0x602A, 0x39A4);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0B1F);
	write_cmos_sensor(0x602A, 0x39AC);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0B1F);
	write_cmos_sensor(0x602A, 0x39B4);
	write_cmos_sensor(0x6F12, 0x0012);
	write_cmos_sensor(0x6F12, 0x0B1F);
	write_cmos_sensor(0x602A, 0x6D70);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x602A, 0x35EC);
	write_cmos_sensor(0x6F12, 0x19C5);
	write_cmos_sensor(0x602A, 0x35D8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0480);
	write_cmos_sensor(0x602A, 0x13C4);
	write_cmos_sensor(0x6F12, 0x00DB);
	write_cmos_sensor(0xBF02, 0x0CDC);
	write_cmos_sensor(0xBF04, 0x0CDF);
	write_cmos_sensor(0xBF24, 0x04C4);
	write_cmos_sensor(0x6028, 0x2001);
	write_cmos_sensor(0x602A, 0xF600);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0x004D);
	write_cmos_sensor(0x6F12, 0x0140);
	write_cmos_sensor(0x6F12, 0x019F);
	write_cmos_sensor(0x6F12, 0x004D);
	write_cmos_sensor(0x6F12, 0x0140);
	write_cmos_sensor(0x6F12, 0x019F);
	write_cmos_sensor(0x6F12, 0x004A);
	write_cmos_sensor(0x6F12, 0x0143);
	write_cmos_sensor(0x6F12, 0x019C);
	write_cmos_sensor(0x6F12, 0x000F);

	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0226, 0x1000);//Long Shutter
	write_cmos_sensor(0x022C, 0x0F00);//Middle Shutter
	write_cmos_sensor(0x0202, 0x0800);//Short Shutter

	write_cmos_sensor(0x0230, 0x0300);//Long Digital Gain
	write_cmos_sensor(0x0240, 0x0300);//Middle Digital Gain
	write_cmos_sensor(0x020E, 0x0300);//Short Digital Gain
	LOG_INF("EXIT\n");

}

static void custom2_setting(void)
{
	/********************************************************
	 *
	 *   1920x1080 Stagger HDR 30fps 4 lane MIPI 1200Mbps/lane
	 *
	 ********************************************************/
	LOG_INF("E\n");
	write_cmos_sensor(0x0100, 0x0000);
	check_stream_is_off();
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x013E, 0x0000);
	write_cmos_sensor(0x0304, 0x0004);
	write_cmos_sensor(0x0306, 0x0108);
	write_cmos_sensor(0x030C, 0x0000);
	write_cmos_sensor(0x0302, 0x0003);
	write_cmos_sensor(0x0300, 0x0002);
	write_cmos_sensor(0x030E, 0x0004);
	write_cmos_sensor(0x0310, 0x00C8);
	write_cmos_sensor(0x0312, 0x0001);
	write_cmos_sensor(0x030A, 0x0002);
	write_cmos_sensor(0x0308, 0x0008);
	write_cmos_sensor(0x602A, 0x35EA);
	write_cmos_sensor(0x6F12, 0x0096);
	write_cmos_sensor(0x0344, 0x0558);
	write_cmos_sensor(0x0346, 0x0560);
	write_cmos_sensor(0x0348, 0x1457);
	write_cmos_sensor(0x034A, 0x0DE7);
	write_cmos_sensor(0x0350, 0x0000);
	write_cmos_sensor(0x0352, 0x0006);
	write_cmos_sensor(0x034C, 0x0780);
	write_cmos_sensor(0x034E, 0x0438);
	write_cmos_sensor(0x0900, 0x2222);
	write_cmos_sensor(0x040C, 0x0000);
	write_cmos_sensor(0x0400, 0x1010);
	write_cmos_sensor(0x0408, 0x0100);
	write_cmos_sensor(0x040A, 0x0100);
	write_cmos_sensor(0x0380, 0x0002);
	write_cmos_sensor(0x0382, 0x0002);
	write_cmos_sensor(0x0384, 0x0002);
	write_cmos_sensor(0x0386, 0x0002);
	write_cmos_sensor(0x602A, 0x25CC);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x0342, 0x12C4); // custom2 FL 4084
	write_cmos_sensor(0x0340, 0x1C9E); // custom2 FL 7326
	write_cmos_sensor(0x0114, 0x0301);
	write_cmos_sensor(0x011C, 0x0100);
	write_cmos_sensor(0x602A, 0x13EE);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0004);
	write_cmos_sensor(0x602A, 0x13F6);
	write_cmos_sensor(0x6F12, 0x0003);
	write_cmos_sensor(0x0B00, 0x0080);
	write_cmos_sensor(0x0B06, 0x0101);
	write_cmos_sensor(0x0BC6, 0x0000);
	write_cmos_sensor(0x602A, 0x1378);
	write_cmos_sensor(0x6F12, 0x00C9);
	write_cmos_sensor(0x602A, 0x5120);
	write_cmos_sensor(0x6F12, 0x0101);
	write_cmos_sensor(0x602A, 0x2060);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x3994);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x021E, 0x0300);
	write_cmos_sensor(0x0250, 0x0300);
	write_cmos_sensor(0x602A, 0x2028);
	write_cmos_sensor(0x6F12, 0x0021);
	write_cmos_sensor(0x602A, 0x2420);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x602A, 0x3F20);
	write_cmos_sensor(0x6F12, 0x19B2);
	write_cmos_sensor(0x602A, 0x25D8);
	write_cmos_sensor(0x6F12, 0x0404);
	write_cmos_sensor(0x602A, 0x35AA);
	write_cmos_sensor(0x6F12, 0x0401);
	write_cmos_sensor(0x602A, 0x2566);
	write_cmos_sensor(0x6F12, 0x0101);
	write_cmos_sensor(0x602A, 0x2702);
	write_cmos_sensor(0x6F12, 0x0049);
	write_cmos_sensor(0x6F12, 0x0086);
	write_cmos_sensor(0x602A, 0x270A);
	write_cmos_sensor(0x6F12, 0x0009);
	write_cmos_sensor(0x6F12, 0x000B);
	write_cmos_sensor(0x6F12, 0x002B);
	write_cmos_sensor(0x6F12, 0x014C);
	write_cmos_sensor(0x602A, 0x2712);
	write_cmos_sensor(0x6F12, 0x003B);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x602A, 0x35F0);
	write_cmos_sensor(0x6F12, 0xC25E);
	write_cmos_sensor(0x602A, 0x39CA);
	write_cmos_sensor(0x6F12, 0x0101);
	write_cmos_sensor(0x602A, 0x39D2);
	write_cmos_sensor(0x6F12, 0x0003);
	write_cmos_sensor(0x602A, 0x2552);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x397C);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x35E2);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x35C0);
	write_cmos_sensor(0x6F12, 0x0504);
	write_cmos_sensor(0x6F12, 0x0505);
	write_cmos_sensor(0x6F12, 0x0707);
	write_cmos_sensor(0x602A, 0x35DC);
	write_cmos_sensor(0x6F12, 0x5353);
	write_cmos_sensor(0x6F12, 0x3838);
	write_cmos_sensor(0x602A, 0x35CC);
	write_cmos_sensor(0x6F12, 0x0B08);
	write_cmos_sensor(0x6F12, 0x0F0F);
	write_cmos_sensor(0x602A, 0x39A4);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0B1F);
	write_cmos_sensor(0x602A, 0x39AC);
	write_cmos_sensor(0x6F12, 0x0014);
	write_cmos_sensor(0x6F12, 0x0B1F);
	write_cmos_sensor(0x602A, 0x39B4);
	write_cmos_sensor(0x6F12, 0x0012);
	write_cmos_sensor(0x6F12, 0x0B1F);
	write_cmos_sensor(0x602A, 0x6D70);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x602A, 0x35EC);
	write_cmos_sensor(0x6F12, 0x19C5);
	write_cmos_sensor(0x602A, 0x35D8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0480);
	write_cmos_sensor(0x602A, 0x13C4);
	write_cmos_sensor(0x6F12, 0x00DB);
	write_cmos_sensor(0xBF02, 0x0CDC);
	write_cmos_sensor(0xBF04, 0x0CDF);
	write_cmos_sensor(0xBF24, 0x04C4);
	write_cmos_sensor(0x6028, 0x2001);
	write_cmos_sensor(0x602A, 0xF600);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0x004D);
	write_cmos_sensor(0x6F12, 0x0140);
	write_cmos_sensor(0x6F12, 0x019F);
	write_cmos_sensor(0x6F12, 0x004D);
	write_cmos_sensor(0x6F12, 0x0140);
	write_cmos_sensor(0x6F12, 0x019F);
	write_cmos_sensor(0x6F12, 0x004A);
	write_cmos_sensor(0x6F12, 0x0143);
	write_cmos_sensor(0x6F12, 0x019C);
	write_cmos_sensor(0x6F12, 0x000F);

	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0226, 0x1000);//Long Shutter
	write_cmos_sensor(0x022C, 0x0F00);//Middle Shutter
	write_cmos_sensor(0x0202, 0x0800);//Short Shutter

	write_cmos_sensor(0x0230, 0x0300);//Long Digital Gain
	write_cmos_sensor(0x0240, 0x0300);//Middle Digital Gain
	write_cmos_sensor(0x020E, 0x0300);//Short Digital Gain
	LOG_INF("EXIT\n");
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
#if 1
	int retry = 1;
#endif
	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	//we should detect the module used i2c address
	//while (imgsensor_info.i2c_addr_table[i] != 0xff) {
	//spin_lock(&imgsensor_drv_lock);
	//imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
	//spin_unlock(&imgsensor_drv_lock);
	do {
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		*sensor_id = ((read_cmos_sensor(0x0000) << 8)
			      | read_cmos_sensor(0x0001));
		LOG_INF("read_0x0000=0x%x, 0x0001=0x%x,0x0000_0001=0x%x\n",
			read_cmos_sensor(0x0000), read_cmos_sensor(0x0001),
			read_cmos_sensor(0x0000));
		if (*sensor_id == imgsensor_info.sensor_id) {
			LOG_INF(
				"i2c write id: 0x%x, sensor id: 0x%x module_id 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id,
				imgsensor_info.module_id);
			break;
		}
		LOG_INF("Read sensor id fail, id: 0x%x,0x%x\n",
			imgsensor.i2c_write_id, *sensor_id);
		retry--;
		i++;
	} while (retry > 0);

	if (*sensor_id != imgsensor_info.sensor_id) {
		// if Sensor ID is not correct,
		// Must set *sensor_id to 0xFFFFFFFF
		*sensor_id = 0xFFFFFFFF;
		return ERROR_NONE;
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
	kal_uint8 retry = 1;
	kal_uint32 sensor_id = 0;

	LOG_1;
	do {
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		sensor_id = ((read_cmos_sensor(0x0000) << 8)
			      | read_cmos_sensor(0x0001));
		LOG_INF("read_0x0000=0x%x, 0x0001=0x%x,0x0000_0001=0x%x\n",
			read_cmos_sensor(0x0000), read_cmos_sensor(0x0001),
			read_cmos_sensor(0x0000));
		if (sensor_id == imgsensor_info.sensor_id) {

			LOG_INF(
				"i2c write id: 0x%x, sensor id: 0x%x module_id 0x%x\n",
				imgsensor.i2c_write_id, sensor_id,
				imgsensor_info.module_id);
			break;
		}
		LOG_INF("Read sensor id fail, id: 0x%x,0x%x\n",
			imgsensor.i2c_write_id, sensor_id);
		retry--;
		i++;
	} while (retry > 0);

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
	LOG_INF("E\n");
	//streaming_control(KAL_FALSE);
	write_cmos_sensor(0x0100, 0x00);
	/*No Need to implement this function */

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

	return ERROR_NONE;
}				/*    preview   */

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
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

	if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
		LOG_INF("Warning: cur fps %d not support, use cap's fps: %d\n",
			imgsensor.current_fps,
			imgsensor_info.cap.max_framerate / 10);
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);
	capture_setting();
	return ERROR_NONE;
}				/* capture() */

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			       MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	normal_video_setting();

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
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */
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
	//preview_setting();
	slim_video_setting();
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */

	return ERROR_NONE;
}				/*    slim_video     */

static kal_uint32 Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */

	return ERROR_NONE;
}				/*    Custome1  staggered HDR    */

static kal_uint32 Custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom2_setting();
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */

	return ERROR_NONE;
}				/*    Custome1  staggered HDR    */

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

	sensor_resolution->SensorCustom1Width =
		imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height =
		imgsensor_info.custom1.grabwindow_height;

	sensor_resolution->SensorCustom2Width =
		imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height =
		imgsensor_info.custom2.grabwindow_height;

	return ERROR_NONE;
}				/*    get_resolution    */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_INFO_STRUCT *sensor_info,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
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

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
		imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame =
		imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame =
		imgsensor_info.custom2_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
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
	sensor_info->HDR_Support = HDR_SUPPORT_STAGGER;

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
			imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;

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
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}

	return ERROR_NONE;
}				/* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);

	if (framerate == 0)
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
	enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n",
		scenario_id, framerate);

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
		imgsensor.frame_length = imgsensor_info.pre.framelength +
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
			imgsensor_info.normal_video.framelength) ?
			(frame_length -
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
			imgsensor_info.cap.framelength) ?
			(frame_length -
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
			imgsensor_info.hs_video.framelength) ?
			(frame_length -
			imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength +
			imgsensor.dummy_line;
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
			imgsensor_info.slim_video.framelength) ?
			(frame_length -
			imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length =
			imgsensor_info.custom1.pclk / framerate * 10 /
			imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.custom1.framelength) ?
			(frame_length -
			imgsensor_info.custom1.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.custom1.framelength +
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
			imgsensor_info.custom2.framelength) ?
			(frame_length -
			imgsensor_info.custom2.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.custom2.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	default:
		frame_length =
			imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.pre.framelength) ?
			(frame_length -
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
	enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

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
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable)
		write_cmos_sensor_8(0x5081, 0x80);
	else
		write_cmos_sensor_8(0x5081, 0x00);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n",
		enable);

	if (enable) {
		write_cmos_sensor_8(0x0100, 0x01);
		check_stream_is_on();
	} else
		write_cmos_sensor_8(0x0100, 0x00);

//mdelay(20);
	return ERROR_NONE;
}

static void hdr_write_tri_shutter(kal_uint32 le, kal_uint32 me, kal_uint32 se)
{
	LOG_INF("E! le:0x%x, me:0x%x, se:0x%x\n", le, me, se);
	write_cmos_sensor(0x0226, le);//Long Shutter
	write_cmos_sensor(0x022C, me);//Middle Shutter
	write_cmos_sensor(0x0202, se);//Short Shutter

}

static void hdr_write_tri_gain(kal_uint16 lgain, kal_uint16 mg, kal_uint16 sg)
{
	LOG_INF("lgain:0x%x, mg:0x%x, sg:0x%x\n", lgain, mg, sg);
	write_cmos_sensor(0x0230, lgain);//Long Digital Gain
	write_cmos_sensor(0x0240, mg);//Middle Digital Gain
	write_cmos_sensor(0x020E, sg);//Short Digital Gain
}


static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	//struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	struct SENSOR_VC_INFO2_STRUCT *pvcinfo2;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
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
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM4:
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(feature_data + 2) = 2;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
		default:
			*(feature_data + 2) = 1;
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
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL) * feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT32) *feature_data);
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
			(enum MSDK_SCENARIO_ID_ENUM) *feature_data,
			(MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
		pr_debug("SENSOR_FEATURE_GET_PDAF_DATA\n");
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) * feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16) *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		//imgsensor.ihdr_en = (BOOL) * feature_data_32;
		imgsensor.hdr_mode = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
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
		case MSDK_SCENARIO_ID_CUSTOM4:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[8],
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
		pr_debug("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			(UINT16) *feature_data);
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
		case MSDK_SCENARIO_ID_CUSTOM1:
			rate = imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			rate = imgsensor_info.custom2.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			rate = imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = rate;
	}
		break;
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
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom2.pclk;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
#if NO_USE_3HDR
			if (imgsensor.hdr_mode)
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
		} break;
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.pre.framelength << 16)
					+ imgsensor_info.pre.linelength;
				break;
			}
		break;
	case SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t) (*(feature_data + 1))
			= HDR_RAW_STAGGER_3EXP;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		default:
			*(MUINT32 *)(uintptr_t) (*(feature_data + 1))
			= HDR_NONE;
			// other scenario do not support HDR
			break;
		}
		LOG_INF(
			"SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY scenarioId:%llu, HDR:%llu\n",
			*feature_data, *(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_VC_INFO2:
		LOG_INF("SENSOR_FEATURE_GET_VC_INFO2 %d\n",
		(UINT16)(*feature_data));
		pvcinfo2 =
		(struct SENSOR_VC_INFO2_STRUCT *)
		(uintptr_t)(*(feature_data + 1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[0],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[1],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[2],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[3],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[4],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		default:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[0],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		(*(feature_data + 1)) = 1;
		(*(feature_data + 2)) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_GET_STAGGER_MAX_EXP_TIME:
		if (*feature_data == MSDK_SCENARIO_ID_CUSTOM1) {
			switch (*(feature_data + 1)) {
			case VC_STAGGER_NE:
					*(feature_data + 2) = 32757;
					break;
			case VC_STAGGER_ME:
					*(feature_data + 2) = 32757;
					break;
			case VC_STAGGER_SE:
					*(feature_data + 2) = 32757;
					break;
			default:
					*(feature_data + 2) = 32757;
					break;
			}
		} else if (*feature_data == MSDK_SCENARIO_ID_CUSTOM2) {
			switch (*(feature_data + 1)) {
			case VC_STAGGER_NE:
					*(feature_data + 2) = 32757;
					break;
			case VC_STAGGER_ME:
					*(feature_data + 2) = 32757;
					break;
			case VC_STAGGER_SE:
					*(feature_data + 2) = 32757;
					break;
			default:
					*(feature_data + 2) = 32757;
					break;
			}
		} else {
			*(feature_data + 2) = 0;
		}
		break;
	case SENSOR_FEATURE_GET_STAGGER_TARGET_SCENARIO:
		if (*feature_data == MSDK_SCENARIO_ID_CAMERA_PREVIEW) {
			switch (*(feature_data + 1)) {
			case HDR_RAW_STAGGER_3EXP:
				*(feature_data + 2) = MSDK_SCENARIO_ID_CUSTOM1;
			default:
					break;
				}
		}
		if (*feature_data == MSDK_SCENARIO_ID_VIDEO_PREVIEW) {
			switch (*(feature_data + 1)) {
			case HDR_RAW_STAGGER_3EXP:
				*(feature_data + 2) = MSDK_SCENARIO_ID_CUSTOM2;
			default:
				break;
				}
		}
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER: // for 2EXP
		LOG_INF(
		"SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
		(UINT16)*(feature_data),
		(UINT16) *(feature_data + 1));
		// implement write shutter for NE/ME/SE
		hdr_write_shutter((UINT16)*feature_data,
				(UINT16)*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_DUAL_GAIN:
		LOG_INF(
			"SENSOR_FEATURE_SET_DUAL_GAIN LG=%d, SG=%d\n",
			(UINT16)*(feature_data), (UINT16) *(feature_data + 1));
		hdr_write_gain((UINT16)*feature_data,
				(UINT16)*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_HDR_TRI_SHUTTER: // for 3EXP
		LOG_INF(
			"SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d, ME=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		//NE ME SE feature_data_check
		if (((UINT16)*(feature_data+2) > 1) &&
			((UINT16)*(feature_data+1) > 1) &&
			((UINT16)*(feature_data) > 1))
			hdr_write_tri_shutter((UINT16)*feature_data,
				(UINT16)*(feature_data+1),
				(UINT16)*(feature_data+2));
		else
			LOG_INF("Value Violation : feature_data<1");
		break;
	case SENSOR_FEATURE_SET_HDR_TRI_GAIN: // for 3EXP
		LOG_INF(
			"SENSOR_FEATURE_SET_HDR_TRI_GAIN LGain=%d, SGain=%d, MGain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		//LGain SGain MGain feature_data_check
		if (((UINT16)*(feature_data+2) > 1) &&
			((UINT16)*(feature_data+1) > 1) &&
			((UINT16)*(feature_data) > 1))
			hdr_write_tri_gain((UINT16)*feature_data,
				(UINT16)*(feature_data+1),
				(UINT16)*(feature_data+2));
		else
			LOG_INF("Value Violation : feature_data<1");
		break;
	default:
		break;
	}
	return ERROR_NONE;
}		/*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 S5KJD1_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*    OV5648MIPISensorInit    */
