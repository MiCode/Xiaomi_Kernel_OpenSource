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

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     gc08a3mipi_Sensor.c
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
 *
 * Version:  V20211202143358 by GC-S-TEAM
 *

 */
#define PFX "gc08a3_camera_sensor"
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

#include "gc08a3mipi_Sensor.h"

static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = GC08A3_SENSOR_ID,
	.checksum_value = 0xe5d32119,
	.pre = {
		.pclk = 280000000,
		.linelength = 3640,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1632,
		.grabwindow_height = 1224,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 136800000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 280000000,
		.linelength = 3640,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 268800000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 280000000,
		.linelength = 3640,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 268800000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 280000000,
		.linelength = 3640,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1632,
		.grabwindow_height = 1224,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 136800000,
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 280000000,
		.linelength = 3640,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 111600000,
		.max_framerate = 300,
	},

	.margin = 16,
	.min_shutter = 2,
	.min_gain = 64,/* 1x */
	.max_gain = 1024,/* 16x */
	.min_gain_iso = 100,
	.exp_step = 1,
	.gain_step = 1,
	.gain_type = 4,
	.max_frame_length = 0xfffe,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1, le first ; 0, se first */
	.temperature_support = 0,/* 1, support; 0, not support */
	.sensor_mode_num = 5,	/* support sensor mode num */
	.cap_delay_frame = 2,	/* enter capture delay frame num */
	.pre_delay_frame = 2,	/* enter preview delay frame num */
	.video_delay_frame = 2,	/* enter video delay frame num */
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,	/* enter slim video delay frame num */
	.frame_time_delay_frame = 2,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
#if GC08A3_MIRROR_NORMAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
#elif GC08A3_MIRROR_H
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
#elif GC08A3_MIRROR_V
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb,
#elif GC08A3_MIRROR_HV
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
#else
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
#endif
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x62, 0x20, 0x22, 0x24, 0xff},
	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
	.i2c_speed = 400,
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.shutter = 0x480,
	.gain = 0x40,
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_mode = 0, /* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x20, /* record current sensor's i2c write id */
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{ 3264, 2448, 0, 0, 3264, 2448, 1632, 1224, 0, 0, 1632, 1224, 0, 0, 1632, 1224}, /* Preview */
	{ 3264, 2448, 0, 0, 3264, 2448, 3264, 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448}, /* capture */
	{ 3264, 2448, 0, 0, 3264, 2448, 3264, 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448}, /* video */
	{ 3264, 2448, 0, 0, 3264, 2448, 1632, 1224, 0, 0, 1632, 1224, 0, 0, 1632, 1224}, /* hs video */
	{ 3264, 2448, 0, 0, 3264, 2448, 1632, 1224, 176, 252, 1280, 720, 0, 0, 1280, 720}  /* slim video */
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {
		(char)((addr >> 8) & 0xff),
		(char)(addr & 0xff)
	};

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {
		(char)((addr >> 8) & 0xff),
		(char)(addr & 0xff),
		(char)((para >> 8) & 0xff),
		(char)(para & 0xff)
	};

	iWriteRegI2C(pu_send_cmd, 4, imgsensor.i2c_write_id);
}

static void write_cmos_sensor_8bit(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {
		(char)((addr >> 8) & 0xff),
		(char)(addr & 0xff),
		(char)(para & 0xff)
	};

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}


static void table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend = 0, idx = 0;
	kal_uint16 addr = 0, data = 0;

	while (len > idx) {
		addr = para[idx];
		puSendCmd[tosend++] = (char)((addr >> 8) & 0xff);
		puSendCmd[tosend++] = (char)(addr & 0xff);
		data = para[idx + 1];
		puSendCmd[tosend++] = (char)(data & 0xff);
		idx += 2;
#if MULTI_WRITE
		if (tosend >= I2C_BUFFER_LEN || idx == len) {
			iBurstWriteReg_multi(puSendCmd, tosend, imgsensor.i2c_write_id,
					3, imgsensor_info.i2c_speed);
			tosend = 0;
		}
#else
		iWriteRegI2CTiming(puSendCmd, 3, imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
		tosend = 0;
#endif
	}
}

static kal_uint32 return_sensor_id(void)
{
	kal_uint32 sensor_id = 0;

	sensor_id = (read_cmos_sensor(0x03f0) << 8) | read_cmos_sensor(0x03f1);
	return sensor_id;
}
static void set_dummy(void)
{
	pr_debug("frame length = %d\n", imgsensor.frame_length);
	if (imgsensor.frame_length < 0xfffe)
		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xfffe);
}	/*	set_dummy  */


static void set_max_framerate(kal_uint16 framerate, kal_bool min_framelength_en)
{
	/*kal_int16 dummy_line;*/
	kal_uint32 frame_length = imgsensor.frame_length;

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


static void set_mirror_flip(kal_uint8 image_mirror)
{
}

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
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;
	realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
	if (imgsensor.autoflicker_en) {
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			write_cmos_sensor(0x0340, imgsensor.frame_length & 0xfffe);
	} else
			write_cmos_sensor(0x0340, imgsensor.frame_length & 0xfffe);
	write_cmos_sensor(0x0202, shutter & 0xffff);

	pr_debug("shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);
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
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
} /* set_shutter */


/*************************************************************************
 * FUNCTION
 *	set_shutter_frame_length
 *
 * DESCRIPTION
 *	for frame & 3A sync
 *
 *************************************************************************/
static void set_shutter_frame_length(kal_uint32 shutter,
				     kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/* if shutter bigger than frame_length, should extend frame length first*/
	spin_lock(&imgsensor_drv_lock);
	if (frame_length > 1)
		dummy_line = frame_length - imgsensor.frame_length;

	imgsensor.frame_length = imgsensor.frame_length + dummy_line;

	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter)
			? imgsensor_info.min_shutter : shutter;
	shutter =
	(shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
		? (imgsensor_info.max_frame_length - imgsensor_info.margin)
		: shutter;
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
	if (imgsensor.autoflicker_en) {
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			set_max_framerate(realtime_fps, 0);
	} else
			set_max_framerate(realtime_fps, 0);
	/* Update Shutter */
	write_cmos_sensor(0x0202, shutter & 0xffff);
	pr_debug(
		"Exit! shutter =%d, framelength =%d/%d, dummy_line=%d,\n",
		shutter, imgsensor.frame_length, frame_length,
		dummy_line);

}	/* set_shutter_frame_length */

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	 kal_uint16 reg_gain = 0x0;

	reg_gain = gain << 4;
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
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain, max_gain = imgsensor_info.max_gain;

	if (gain < imgsensor_info.min_gain || gain > max_gain) {
		pr_debug("Error max gain setting: %d\n", max_gain);

		if (gain < imgsensor_info.min_gain)
			gain = imgsensor_info.min_gain;
		else if (gain > max_gain)
			gain = max_gain;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	pr_debug("gain = %d, reg_gain = 0x%x, max_gain:0x%x\n ",
		gain, reg_gain, max_gain);
	write_cmos_sensor(0x0204, reg_gain & 0xffff);

	return gain;
} /* set_gain */
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
static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable)
		write_cmos_sensor_8bit(0x0100, 0x01);
	else
		write_cmos_sensor_8bit(0x0100, 0x00);
	return ERROR_NONE;
}

kal_uint16 gc08a3_init_addr_data[] = {
/*system*/
	0x031c, 0x60,
	0x0337, 0x04,
	0x0335, 0x51,
	0x0336, 0x70,
	0x0383, 0xbb,
	0x031a, 0x00,
	0x0321, 0x10,
	0x0327, 0x03,
	0x0325, 0x40,
	0x0326, 0x23,
	0x0314, 0x11,
	0x0315, 0xd6,
	0x0316, 0x01,
	0x0334, 0x40,
	0x0324, 0x42,
	0x031c, 0x00,
	0x031c, 0x9f,
	0x039a, 0x13,
	0x0084, 0x30,
	0x02b3, 0x08,
	0x0057, 0x0c,
	0x05c3, 0x50,
	0x0311, 0x90,
	0x05a0, 0x02,
	0x0074, 0x0a,
	0x0059, 0x11,
	0x0070, 0x05,
	0x0101, GC08A3_MIRROR,
/*analog*/
	0x0344, 0x00,
	0x0345, 0x06,
	0x0346, 0x00,
	0x0347, 0x04,
	0x0348, 0x0c,
	0x0349, 0xd0,
	0x034a, 0x09,
	0x034b, 0x9c,
	0x0202, 0x09,
	0x0203, 0x04,
	0x0340, 0x09,
	0x0341, 0xf4,
	0x0342, 0x07,
	0x0343, 0x1c,
	0x0219, 0x05,
	0x0226, 0x00,
	0x0227, 0x28,
	0x0e0a, 0x00,
	0x0e0b, 0x00,
	0x0e24, 0x04,
	0x0e25, 0x04,
	0x0e26, 0x00,
	0x0e27, 0x10,
	0x0e01, 0x74,
	0x0e03, 0x47,
	0x0e04, 0x33,
	0x0e05, 0x44,
	0x0e06, 0x44,
	0x0e0c, 0x1e,
	0x0e17, 0x3a,
	0x0e18, 0x3c,
	0x0e19, 0x40,
	0x0e1a, 0x42,
	0x0e28, 0x21,
	0x0e2b, 0x68,
	0x0e2c, 0x0d,
	0x0e2d, 0x08,
	0x0e34, 0xf4,
	0x0e35, 0x44,
	0x0e36, 0x07,
	0x0e38, 0x49,
	0x0210, 0x13,
	0x0218, 0x00,
	0x0241, 0x88,
	0x0e32, 0x00,
	0x0e33, 0x18,
	0x0e42, 0x03,
	0x0e43, 0x80,
	0x0e44, 0x04,
	0x0e45, 0x00,
	0x0e4f, 0x04,
	0x057a, 0x20,
	0x0381, 0x7c,
	0x0382, 0x9b,
	0x0384, 0xfb,
	0x0389, 0x38,
	0x038a, 0x03,
	0x0390, 0x6a,
	0x0391, 0x0b,
	0x0392, 0x60,
	0x0393, 0xc1,
	0x0396, 0xff,
	0x0398, 0x62,
/*cisctl reset*/
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9f,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9f,
	0x0360, 0x01,
	0x0360, 0x00,
	0x0316, 0x09,
	0x0a67, 0x80,
	0x0313, 0x00,
	0x0a53, 0x0e,
	0x0a65, 0x17,
	0x0a68, 0xa1,
	0x0a58, 0x00,
	0x0ace, 0x0c,
	0x00a4, 0x00,
	0x00a5, 0x01,
	0x00a7, 0x09,
	0x00a8, 0x9c,
	0x00a9, 0x0c,
	0x00aa, 0xd0,
	0x0a8a, 0x00,
	0x0a8b, 0xe0,
	0x0a8c, 0x13,
	0x0a8d, 0xe8,
	0x0a90, 0x0a,
	0x0a91, 0x10,
	0x0a92, 0xf8,
	0x0a71, 0xf2,
	0x0a72, 0x12,
	0x0a73, 0x64,
	0x0a75, 0x41,
	0x0a70, 0x07,
	0x0313, 0x80,
/*ISP*/
	0x00a0, 0x01,
	0x0080, 0xd2,
	0x0081, 0x3f,
	0x0087, 0x51,
	0x0089, 0x03,
	0x009b, 0x40,
	0x05a0, 0x82,
	0x05ac, 0x00,
	0x05ad, 0x01,
	0x05ae, 0x00,
	0x0800, 0x0a,
	0x0801, 0x14,
	0x0802, 0x28,
	0x0803, 0x34,
	0x0804, 0x0e,
	0x0805, 0x33,
	0x0806, 0x03,
	0x0807, 0x8a,
	0x0808, 0x50,
	0x0809, 0x00,
	0x080a, 0x34,
	0x080b, 0x03,
	0x080c, 0x26,
	0x080d, 0x03,
	0x080e, 0x18,
	0x080f, 0x03,
	0x0810, 0x10,
	0x0811, 0x03,
	0x0812, 0x00,
	0x0813, 0x00,
	0x0814, 0x01,
	0x0815, 0x00,
	0x0816, 0x01,
	0x0817, 0x00,
	0x0818, 0x00,
	0x0819, 0x0a,
	0x081a, 0x01,
	0x081b, 0x6c,
	0x081c, 0x00,
	0x081d, 0x0b,
	0x081e, 0x02,
	0x081f, 0x00,
	0x0820, 0x00,
	0x0821, 0x0c,
	0x0822, 0x02,
	0x0823, 0xd9,
	0x0824, 0x00,
	0x0825, 0x0d,
	0x0826, 0x03,
	0x0827, 0xf0,
	0x0828, 0x00,
	0x0829, 0x0e,
	0x082a, 0x05,
	0x082b, 0x94,
	0x082c, 0x09,
	0x082d, 0x6e,
	0x082e, 0x07,
	0x082f, 0xe6,
	0x0830, 0x10,
	0x0831, 0x0e,
	0x0832, 0x0b,
	0x0833, 0x2c,
	0x0834, 0x14,
	0x0835, 0xae,
	0x0836, 0x0f,
	0x0837, 0xc4,
	0x0838, 0x18,
	0x0839, 0x0e,
	0x05ac, 0x01,
	0x059a, 0x00,
	0x059b, 0x00,
	0x059c, 0x01,
	0x0598, 0x00,
	0x0597, 0x14,
	0x05ab, 0x09,
	0x05a4, 0x02,
	0x05a3, 0x05,
	0x05a0, 0xc2,
	0x0207, 0xc4,
/*GAIN*/
	0x0208, 0x01,
	0x0209, 0x72,
	0x0204, 0x04,
	0x0205, 0x00,
	0x0040, 0x22,
	0x0041, 0x20,
	0x0043, 0x10,
	0x0044, 0x00,
	0x0046, 0x08,
	0x0047, 0xf0,
	0x0048, 0x0f,
	0x004b, 0x0f,
	0x004c, 0x00,
	0x0050, 0x5c,
	0x0051, 0x44,
	0x005b, 0x03,
	0x00c0, 0x00,
	0x00c1, 0x80,
	0x00c2, 0x31,
	0x00c3, 0x00,
	0x0460, 0x04,
	0x0462, 0x08,
	0x0464, 0x0e,
	0x0466, 0x0a,
	0x0468, 0x12,
	0x046a, 0x12,
	0x046c, 0x10,
	0x046e, 0x0c,
	0x0461, 0x03,
	0x0463, 0x03,
	0x0465, 0x03,
	0x0467, 0x03,
	0x0469, 0x04,
	0x046b, 0x04,
	0x046d, 0x04,
	0x046f, 0x04,
	0x0470, 0x04,
	0x0472, 0x10,
	0x0474, 0x26,
	0x0476, 0x38,
	0x0478, 0x20,
	0x047a, 0x30,
	0x047c, 0x38,
	0x047e, 0x60,
	0x0471, 0x05,
	0x0473, 0x05,
	0x0475, 0x05,
	0x0477, 0x05,
	0x0479, 0x04,
	0x047b, 0x04,
	0x047d, 0x04,
	0x047f, 0x04,
};

static kal_uint16 gc08a3_1632x1224_addr_data[] = {
/*system*/
	0x031c, 0x60,
	0x0337, 0x04,
	0x0335, 0x55,
	0x0336, 0x72,
	0x0383, 0xbb,
	0x031a, 0x00,
	0x0321, 0x10,
	0x0327, 0x03,
	0x0325, 0x40,
	0x0326, 0x23,
	0x0314, 0x11,
	0x0315, 0xd6,
	0x0316, 0x01,
	0x0334, 0x40,
	0x0324, 0x42,
	0x031c, 0x00,
	0x031c, 0x9f,
	0x0344, 0x00,
	0x0345, 0x06,
	0x0346, 0x00,
	0x0347, 0x04,
	0x0348, 0x0c,
	0x0349, 0xd0,
	0x034a, 0x09,
	0x034b, 0x9c,
	0x0202, 0x03,
	0x0203, 0x00,
	0x0340, 0x09,
	0x0341, 0xf4,
	0x0342, 0x07,
	0x0343, 0x1c,
	0x0226, 0x00,
	0x0227, 0x06,
	0x0e38, 0x49,
	0x0210, 0x53,
	0x0218, 0x80,
	0x0241, 0x8c,
	0x0392, 0x3b,
/*ISP*/
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9f,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9f,
	0x00a2, 0x00,
	0x00a3, 0x00,
	0x00ab, 0x00,
	0x00ac, 0x00,
	0x05a0, 0x82,
	0x05ac, 0x00,
	0x05ad, 0x01,
	0x05ae, 0x00,
	0x0800, 0x0a,
	0x0801, 0x14,
	0x0802, 0x28,
	0x0803, 0x34,
	0x0804, 0x0e,
	0x0805, 0x33,
	0x0806, 0x03,
	0x0807, 0x8a,
	0x0808, 0x50,
	0x0809, 0x00,
	0x080a, 0x34,
	0x080b, 0x03,
	0x080c, 0x26,
	0x080d, 0x03,
	0x080e, 0x18,
	0x080f, 0x03,
	0x0810, 0x10,
	0x0811, 0x03,
	0x0812, 0x00,
	0x0813, 0x00,
	0x0814, 0x01,
	0x0815, 0x00,
	0x0816, 0x01,
	0x0817, 0x00,
	0x0818, 0x00,
	0x0819, 0x0a,
	0x081a, 0x01,
	0x081b, 0x6c,
	0x081c, 0x00,
	0x081d, 0x0b,
	0x081e, 0x02,
	0x081f, 0x00,
	0x0820, 0x00,
	0x0821, 0x0c,
	0x0822, 0x02,
	0x0823, 0xd9,
	0x0824, 0x00,
	0x0825, 0x0d,
	0x0826, 0x03,
	0x0827, 0xf0,
	0x0828, 0x00,
	0x0829, 0x0e,
	0x082a, 0x05,
	0x082b, 0x94,
	0x082c, 0x09,
	0x082d, 0x6e,
	0x082e, 0x07,
	0x082f, 0xe6,
	0x0830, 0x10,
	0x0831, 0x0e,
	0x0832, 0x0b,
	0x0833, 0x2c,
	0x0834, 0x14,
	0x0835, 0xae,
	0x0836, 0x0f,
	0x0837, 0xc4,
	0x0838, 0x18,
	0x0839, 0x0e,
	0x05ac, 0x01,
	0x059a, 0x00,
	0x059b, 0x00,
	0x059c, 0x01,
	0x0598, 0x00,
	0x0597, 0x14,
	0x05ab, 0x09,
	0x05a4, 0x02,
	0x05a3, 0x05,
	0x05a0, 0xc2,
	0x0207, 0xc4,
/*GAIN*/
	0x0204, 0x04,
	0x0205, 0x00,
	0x0050, 0x5c,
	0x0051, 0x44,
/*out window*/
	0x009a, 0x00,
	0x0351, 0x00,
	0x0352, 0x04,
	0x0353, 0x00,
	0x0354, 0x04,
	0x034c, 0x06,
	0x034d, 0x60,
	0x034e, 0x04,
	0x034f, 0xc8,
/*MIPI*/
	0x0114, 0x03,
	0x0180, 0x66,//vv
	0x0181, 0xf0,
	0x0185, 0x01,
	0x0115, 0x30,
	0x011b, 0x12,
	0x011c, 0x12,
	0x0121, 0x02,
	0x0122, 0x03,
	0x0123, 0x09,
	0x0124, 0x00,
	0x0125, 0x08,
	0x0126, 0x05,
	0x0129, 0x03,
	0x012a, 0x02,
	0x012b, 0x05,
	0x0a73, 0x60,
	0x0a70, 0x11,
	0x0313, 0x80,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0a70, 0x00,
	0x00a4, 0x80,
	0x0316, 0x01,
	0x0a67, 0x00,
	0x0084, 0x10,
	0x0102, 0x09,
};

static kal_uint16 gc08a3_3264x2448_addr_data[] = {
/*system*/
	0x031c, 0x60,
	0x0337, 0x04,
	0x0335, 0x51,
	0x0336, 0x70,
	0x0383, 0xbb,
	0x031a, 0x00,
	0x0321, 0x10,
	0x0327, 0x03,
	0x0325, 0x40,
	0x0326, 0x23,
	0x0314, 0x11,
	0x0315, 0xd6,
	0x0316, 0x01,
	0x0334, 0x40,
	0x0324, 0x42,
	0x031c, 0x00,
	0x031c, 0x9f,
	0x0344, 0x00,
	0x0345, 0x06,
	0x0346, 0x00,
	0x0347, 0x04,
	0x0348, 0x0c,
	0x0349, 0xd0,
	0x034a, 0x09,
	0x034b, 0x9c,
	0x0202, 0x09,
	0x0203, 0x04,
	0x0340, 0x09,
	0x0341, 0xf4,
	0x0342, 0x07,
	0x0343, 0x1c,
	0x0226, 0x00,
	0x0227, 0x28,
	0x0e38, 0x49,
	0x0210, 0x13,
	0x0218, 0x00,
	0x0241, 0x88,
	0x0392, 0x60,
/*ISP*/
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9f,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9f,
	0x00a2, 0x00,
	0x00a3, 0x00,
	0x00ab, 0x00,
	0x00ac, 0x00,
	0x05a0, 0x82,
	0x05ac, 0x00,
	0x05ad, 0x01,
	0x05ae, 0x00,
	0x0800, 0x0a,
	0x0801, 0x14,
	0x0802, 0x28,
	0x0803, 0x34,
	0x0804, 0x0e,
	0x0805, 0x33,
	0x0806, 0x03,
	0x0807, 0x8a,
	0x0808, 0x50,
	0x0809, 0x00,
	0x080a, 0x34,
	0x080b, 0x03,
	0x080c, 0x26,
	0x080d, 0x03,
	0x080e, 0x18,
	0x080f, 0x03,
	0x0810, 0x10,
	0x0811, 0x03,
	0x0812, 0x00,
	0x0813, 0x00,
	0x0814, 0x01,
	0x0815, 0x00,
	0x0816, 0x01,
	0x0817, 0x00,
	0x0818, 0x00,
	0x0819, 0x0a,
	0x081a, 0x01,
	0x081b, 0x6c,
	0x081c, 0x00,
	0x081d, 0x0b,
	0x081e, 0x02,
	0x081f, 0x00,
	0x0820, 0x00,
	0x0821, 0x0c,
	0x0822, 0x02,
	0x0823, 0xd9,
	0x0824, 0x00,
	0x0825, 0x0d,
	0x0826, 0x03,
	0x0827, 0xf0,
	0x0828, 0x00,
	0x0829, 0x0e,
	0x082a, 0x05,
	0x082b, 0x94,
	0x082c, 0x09,
	0x082d, 0x6e,
	0x082e, 0x07,
	0x082f, 0xe6,
	0x0830, 0x10,
	0x0831, 0x0e,
	0x0832, 0x0b,
	0x0833, 0x2c,
	0x0834, 0x14,
	0x0835, 0xae,
	0x0836, 0x0f,
	0x0837, 0xc4,
	0x0838, 0x18,
	0x0839, 0x0e,
	0x05ac, 0x01,
	0x059a, 0x00,
	0x059b, 0x00,
	0x059c, 0x01,
	0x0598, 0x00,
	0x0597, 0x14,
	0x05ab, 0x09,
	0x05a4, 0x02,
	0x05a3, 0x05,
	0x05a0, 0xc2,
	0x0207, 0xc4,
/*GAIN*/
	0x0204, 0x04,
	0x0205, 0x00,
	0x0050, 0x5c,
	0x0051, 0x44,
/*out window*/
	0x009a, 0x00,
	0x0351, 0x00,
	0x0352, 0x06,
	0x0353, 0x00,
	0x0354, 0x08,
	0x034c, 0x0c,
	0x034d, 0xc0,
	0x034e, 0x09,
	0x034f, 0x90,
/*MIPI*/
	0x0114, 0x03,
	0x0180, 0x66,//vv
	0x0181, 0xf0,
	0x0185, 0x01,
	0x0115, 0x30,
	0x011b, 0x12,
	0x011c, 0x12,
	0x0121, 0x06,
	0x0122, 0x06,
	0x0123, 0x15,
	0x0124, 0x01,
	0x0125, 0x0b,
	0x0126, 0x08,
	0x0129, 0x06,
	0x012a, 0x08,
	0x012b, 0x08,
	0x0a73, 0x60,
	0x0a70, 0x11,
	0x0313, 0x80,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0a70, 0x00,
	0x00a4, 0x80,
	0x0316, 0x01,
	0x0a67, 0x00,
	0x0084, 0x10,
	0x0102, 0x09,
};

static kal_uint16 gc08a3_1280x720_addr_data[] = {
/*system*/
	0x031c, 0x60,
	0x0337, 0x04,
	0x0335, 0x55,
	0x0336, 0x5d,
	0x0383, 0x9b,
	0x031a, 0x00,
	0x0321, 0x10,
	0x0327, 0x03,
	0x0325, 0x40,
	0x0326, 0x23,
	0x0314, 0x11,
	0x0315, 0xd6,
	0x0316, 0x01,
	0x0334, 0x40,
	0x0324, 0x42,
	0x031c, 0x00,
	0x031c, 0x9f,
	0x0344, 0x01,
	0x0345, 0x66,
	0x0346, 0x01,
	0x0347, 0xfc,
	0x0348, 0x0a,
	0x0349, 0x10,
	0x034a, 0x05,
	0x034b, 0xac,
	0x0202, 0x03,
	0x0203, 0x00,
	0x0340, 0x09,
	0x0341, 0xf4,
	0x0342, 0x07,
	0x0343, 0x1c,
	0x0226, 0x00,
	0x0227, 0x56,
	0x0e38, 0x49,
	0x0210, 0x53,
	0x0218, 0x80,
	0x0241, 0x8c,
	0x0392, 0x3b,
/*ISP*/
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9f,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9f,
	0x00a2, 0xf8,
	0x00a3, 0x01,
	0x00ab, 0x60,
	0x00ac, 0x01,
	0x05a0, 0x82,
	0x05ac, 0x00,
	0x05ad, 0x01,
	0x05ae, 0x00,
	0x0800, 0x0a,
	0x0801, 0x14,
	0x0802, 0x28,
	0x0803, 0x34,
	0x0804, 0x0e,
	0x0805, 0x33,
	0x0806, 0x03,
	0x0807, 0x8a,
	0x0808, 0x50,
	0x0809, 0x00,
	0x080a, 0x34,
	0x080b, 0x03,
	0x080c, 0x26,
	0x080d, 0x03,
	0x080e, 0x18,
	0x080f, 0x03,
	0x0810, 0x10,
	0x0811, 0x03,
	0x0812, 0x00,
	0x0813, 0x00,
	0x0814, 0x01,
	0x0815, 0x00,
	0x0816, 0x01,
	0x0817, 0x00,
	0x0818, 0x00,
	0x0819, 0x0a,
	0x081a, 0x01,
	0x081b, 0x6c,
	0x081c, 0x00,
	0x081d, 0x0b,
	0x081e, 0x02,
	0x081f, 0x00,
	0x0820, 0x00,
	0x0821, 0x0c,
	0x0822, 0x02,
	0x0823, 0xd9,
	0x0824, 0x00,
	0x0825, 0x0d,
	0x0826, 0x03,
	0x0827, 0xf0,
	0x0828, 0x00,
	0x0829, 0x0e,
	0x082a, 0x05,
	0x082b, 0x94,
	0x082c, 0x09,
	0x082d, 0x6e,
	0x082e, 0x07,
	0x082f, 0xe6,
	0x0830, 0x10,
	0x0831, 0x0e,
	0x0832, 0x0b,
	0x0833, 0x2c,
	0x0834, 0x14,
	0x0835, 0xae,
	0x0836, 0x0f,
	0x0837, 0xc4,
	0x0838, 0x18,
	0x0839, 0x0e,
	0x05ac, 0x01,
	0x059a, 0x00,
	0x059b, 0x00,
	0x059c, 0x01,
	0x0598, 0x00,
	0x0597, 0x14,
	0x05ab, 0x09,
	0x05a4, 0x02,
	0x05a3, 0x05,
	0x05a0, 0xc2,
	0x0207, 0xc4,
/*GAIN*/
	0x0204, 0x04,
	0x0205, 0x00,
	0x0050, 0x48,
	0x0051, 0x30,
/*out window*/
	0x009a, 0x00,
	0x0351, 0x00,
	0x0352, 0x04,
	0x0353, 0x00,
	0x0354, 0x04,
	0x034c, 0x05,
	0x034d, 0x00,
	0x034e, 0x02,
	0x034f, 0xd0,
/*MIPI*/
	0x0114, 0x03,
	0x0180, 0x66,//vv
	0x0181, 0xf0,
	0x0185, 0x01,
	0x0115, 0x30,
	0x011b, 0x12,
	0x011c, 0x12,
	0x0121, 0x01,
	0x0122, 0x02,
	0x0123, 0x07,
	0x0124, 0x00,
	0x0125, 0x07,
	0x0126, 0x04,
	0x0129, 0x02,
	0x012a, 0x01,
	0x012b, 0x04,
	0x0a73, 0x60,
	0x0a70, 0x11,
	0x0313, 0x80,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0a70, 0x00,
	0x00a4, 0x80,
	0x0316, 0x01,
	0x0a67, 0x00,
	0x0084, 0x10,
	0x0102, 0x09,
};


static void sensor_init(void)
{
	pr_debug("[%s] init_start\n", __func__);
	table_write_cmos_sensor(gc08a3_init_addr_data,
		sizeof(gc08a3_init_addr_data)/sizeof(kal_uint16));
	pr_debug("[%s] init_End\n", __func__);
}	/*	  sensor_init  */

static void preview_setting(void)
{
	pr_debug("%s preview_Start\n", __func__);
	table_write_cmos_sensor(gc08a3_1632x1224_addr_data,
		sizeof(gc08a3_1632x1224_addr_data)/sizeof(kal_uint16));
	pr_debug("%s preview_End\n", __func__);
}

static void capture_setting(kal_uint16 currefps)
{
	pr_debug("[%s] capture_Start, currefps:%d\n", __func__, currefps);
	table_write_cmos_sensor(gc08a3_3264x2448_addr_data,
		sizeof(gc08a3_3264x2448_addr_data)/sizeof(kal_uint16));
	pr_debug("[%s] capture_End\n", __func__);
}

static void normal_video_setting(kal_uint16 currefps)
{
	pr_debug("[%s] normal_video_Start, currefps:%d\n", __func__, currefps);
	table_write_cmos_sensor(gc08a3_3264x2448_addr_data,
		sizeof(gc08a3_3264x2448_addr_data)/sizeof(kal_uint16));
	pr_debug("[%s] normal_video_End\n", __func__);
}

static void hs_video_setting(void)
{
	pr_debug("[%s] hs_video_Start, 1632x1224@30.19fps\n", __func__);
	table_write_cmos_sensor(gc08a3_1632x1224_addr_data,
		sizeof(gc08a3_1632x1224_addr_data)/sizeof(kal_uint16));
	pr_debug("[%s] hs_video_End\n", __func__);
}

static void slim_video_setting(void)
{
	pr_debug("[%s] slim_video_Start, 1280x720@30.19fps\n", __func__);
	table_write_cmos_sensor(gc08a3_1280x720_addr_data,
		sizeof(gc08a3_1280x720_addr_data)/sizeof(kal_uint16));
	pr_debug("[%s] slim_video_End\n", __func__);
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	pr_debug("enable: %d\n", enable);

	if (enable) {
		write_cmos_sensor_8bit(0x008c, 0x01);
		write_cmos_sensor_8bit(0x008d, 0x00);
	} else {
		write_cmos_sensor_8bit(0x008c, 0x00);
		write_cmos_sensor_8bit(0x008d, 0x10);
	}

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
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
	/* sensor have two i2c address 0x62 & 0x20,
	 * we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				pr_debug("[gc08a3_camera_sensor]get_imgsensor_id:i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			pr_debug("[gc08a3_camera_sensor]get_imgsensor_id:Read sensor id fail, write id: 0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
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

	pr_debug("%s +\n", __func__);

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				pr_debug("[gc08a3_camera_sensor]open:i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			pr_debug("[gc08a3_camera_sensor]open:Read sensor id fail, write id: 0x%x, id: 0x%x\n",
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
	imgsensor.shutter = 0x480;
	imgsensor.gain = 0x40;
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
	pr_debug("%s -\n", __func__);

	return ERROR_NONE;
}

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
	/* No Need to implement this function */
	streaming_control(KAL_FALSE);
	return ERROR_NONE;
}


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
	pr_debug("%s E\n", __func__);

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
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

	if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
		pr_debug(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			imgsensor.current_fps,
			imgsensor_info.cap.max_framerate / 10);
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
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
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

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
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s. 720P@30FPS\n", __func__);

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
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32
get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
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


	return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
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


	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->TEMPERATURE_SUPPORT = imgsensor_info.temperature_support;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0; /* 0 is default 1x */
	sensor_info->SensorHightSampling = 0; /* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

	sensor_info->FrameTimeDelayFrame =
		imgsensor_info.frame_time_delay_frame;

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
	default:
		pr_debug("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}

	return ERROR_NONE;
}



static kal_uint32 set_video_mode(UINT16 framerate)
{
	pr_debug("framerate = %d\n ", framerate);
	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	spin_lock(&imgsensor_drv_lock);
	if (enable) /*enable auto flicker*/ {
		imgsensor.autoflicker_en = KAL_TRUE;
		pr_debug("enable! fps = %d", framerate);
	} else {
		 /*Cancel Auto flick*/
		imgsensor.autoflicker_en = KAL_FALSE;
	}
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
		frame_length = imgsensor_info.pre.pclk / framerate * 10
				/ imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
		? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk /
				framerate * 10 /
				imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.normal_video.framelength)
		? (frame_length - imgsensor_info.normal_video.framelength)
		: 0;
		imgsensor.frame_length =
			imgsensor_info.normal_video.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
		pr_debug(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n"
			, framerate, imgsensor_info.cap.max_framerate/10);
		frame_length = imgsensor_info.cap.pclk / framerate * 10
				/ imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			(frame_length > imgsensor_info.cap.framelength)
			  ? (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length =
				imgsensor_info.cap.framelength
				+ imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10
				/ imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.hs_video.framelength)
			  ? (frame_length - imgsensor_info.hs_video.framelength)
			  : 0;
		imgsensor.frame_length =
			imgsensor_info.hs_video.framelength
				+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10
			/ imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.slim_video.framelength)
			? (frame_length - imgsensor_info.slim_video.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.slim_video.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:  /*coding with  preview scenario by default*/
		frame_length = imgsensor_info.pre.pclk / framerate * 10
			/ imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;
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
	default:
		break;
	}

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

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data
		= (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/* pr_debug("feature_id = %d\n", feature_id); */
	switch (feature_id) {
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
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
		*(feature_data + 2) = imgsensor_info.exp_step;
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
	case SENSOR_FEATURE_SET_NIGHTMODE:
		 /* night_mode((BOOL) *feature_data); */
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor_8bit(sensor_reg_data->RegAddr,
				    sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/*get the lens driver ID from EEPROM
		 * or just return LENS_DRIVER_ID_DO_NOT_CARE
		 * if EEPROM does not exist in camera module.
		 */
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
				      *(feature_data_16+1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		 set_max_framerate_by_scenario(
				(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
				*(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		 get_default_framerate_by_scenario(
				(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
				(MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		pr_debug("current fps :%d\n", (UINT32)*feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		pr_debug("ihdr enable :%d\n", (BOOL)*feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
	#if 0
		pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32)*feature_data);
	#endif
		wininfo =
	(struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

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
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data,
			(UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16) (*feature_data),
					(UINT16) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		/*
		 * 1, if driver support new sw frame sync
		 * set_shutter_frame_length() support third para auto_extend_en
		 */
		*(feature_data + 1) = 1;
		/* margin info by scenario */
		*(feature_data + 2) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		pr_debug("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1));
		#if 0
		ihdr_write_shutter((UINT16)*feature_data,
				   (UINT16)*(feature_data+1));
		#endif
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
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		*feature_return_para_32 = 1; /* BINNING_AVERAGED */
		pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			*feature_return_para_32);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	{
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
	}
break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
	case SENSOR_FEATURE_SET_LSC_TBL:
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
UINT32 GC08A3_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
