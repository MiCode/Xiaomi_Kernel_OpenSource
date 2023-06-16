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
 *	 HI846mipi_Sensor.c
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
#define PFX "HI846_camera_sensor"
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

#include "hi846mipiraw_Sensor.h"

#undef VENDOR_EDIT

#define USE_BURST_MODE 1

//#define I2C_BUFFER_LEN 255 /* trans# max is 255, each 3 bytes */

#if USE_BURST_MODE
static kal_uint16 hi846_table_write_cmos_sensor(
		kal_uint16 * para, kal_uint32 len);
#endif
static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = HI846_SENSOR_ID,

	.checksum_value = 0xdf4593fd,
	.pre = {
		.pclk = 288000000,
		.linelength = 3800,
		.framelength = 2526,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1632,
		.grabwindow_height = 1224,
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 144000000, //(7200M*2/10)
	},
	.cap = {
		.pclk = 288000000,
		.linelength = 3800,
		.framelength = 2526,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000, //(1440M * 2 / 10 )
	},
	.normal_video = {
		.pclk = 288000000,				//record different mode's pclk
		.linelength = 3800, 			//record different mode's linelength
		.framelength = 2526,			//record different mode's framelength
		.startx =0, 				//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 3264,		//record different mode's width of grabwindow
		.grabwindow_height = 2448,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000, //( 1440M*2/10)
	},
	.hs_video = {
		.pclk = 288000000,
		.linelength = 3800, 			//record different mode's linelength
		.framelength = 2526, 		//record different mode's framelength
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 640 ,		//record different mode's width of grabwindow
		.grabwindow_height = 480 ,
       .mipi_data_lp2hs_settle_dc = 85,//unit , ns
        .max_framerate = 300,
        .mipi_pixel_rate = 72000000, //( 360M * 2 / 10 )
	},
    .slim_video = {
		.pclk = 288000000,
		.linelength = 3800,
		.framelength = 2526,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns
        .max_framerate = 300,
        .mipi_pixel_rate = 144000000, //( 720M * 2 / 10 )
    },

	.margin = 6,		/* sensor framelength & shutter margin */
	.min_shutter = 6,	/* min shutter */

	/*
	.min_gain = BASEGAIN,
	.max_gain = 16 * BASEGAIN,
	.min_gain_iso = 100,
	.gain_step = 64,
	.gain_type = 3, // Hynix:type 3
	*/

	.min_gain = 64,
	.max_gain = 1024,
	.min_gain_iso = 100,
	.exp_step = 2,
	.gain_step = 4,
	.gain_type = 3,

	.max_frame_length = 0xffffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.temperature_support = 0,/* 1, support; 0,not support */
	.sensor_mode_num = 5,	/* support sensor mode num */

	.cap_delay_frame = 1,	/* enter capture delay frame num */
	.pre_delay_frame = 1,	/* enter preview delay frame num */
	.video_delay_frame = 1,	/* enter video delay frame num */
	.hs_video_delay_frame = 1,
	.slim_video_delay_frame = 1,	/* enter slim video delay frame num */
	.frame_time_delay_frame = 1,

	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_2_LANE,
	.i2c_addr_table = {0x40, 0xff},
	.i2c_speed = 400,
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.shutter = 0x100,	/* current shutter */
	.gain = 0xe0,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_mode = 0, /* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x40, /* record current sensor's i2c write id */
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
 { 3280, 2464,   8,   8,   3264, 2448,   1632, 1224,   0, 0, 1632, 1224,   0, 0, 1632, 1224}, // preview
 { 3280, 2464,   8,   8,   3264, 2448,   3264, 2448,   0, 0, 3264, 2448,   0, 0, 3264, 2448}, // capture
 { 3280, 2464,   8,   8,   3264, 2448,   3264, 2448,   0, 0, 3264, 2448,   0, 0, 3264, 2448}, // video
 { 3280, 2464,   8, 272,   3264, 1920,    816,  480,  88, 0,  640,  480,   0, 0,  640, 480 }, // hight speed video
 { 3280, 2464,   8, 512,   3264, 1440,   1632,  720, 176, 0, 1280,  720,   0, 0, 1280, 720 }, // slim video
};


static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF),
			     (char)(para >> 8), (char)(para & 0xFF)};

	/*kdSetI2CSpeed(imgsensor_info.i2c_speed);*/
	/* Add this func to set i2c speed by each sensor */
	iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF),
			(char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	pr_debug("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	write_cmos_sensor(0x0006, imgsensor.frame_length & 0xFFFF);
	write_cmos_sensor(0x0008, imgsensor.line_length & 0xFFFF);

}	/*	set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor_8(0x0F17) << 8) | read_cmos_sensor_8(0x0F16));

}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	/*kal_int16 dummy_line;*/
	kal_uint32 frame_length = imgsensor.frame_length;

	pr_debug("framerate = %d, min framelength should enable %d\n", framerate, min_framelength_en);

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

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10
				/ imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(0x0006, imgsensor.frame_length);
		}
	} else {
		/* Extend frame length*/
		write_cmos_sensor(0x0006, imgsensor.frame_length);
	}

	/* Update Shutter */
	write_cmos_sensor_8(0x0073, (shutter & 0xFF0000) >> 16 );
	write_cmos_sensor(0x0074, shutter & 0x00FFFF);

	pr_debug("frame_length = %d , shutter = %d \n", imgsensor.frame_length, shutter);

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
static void set_shutter_frame_length(kal_uint16 shutter,
				     kal_uint16 frame_length,
				     kal_bool auto_extend_en)
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

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 /
				imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(0x0006, imgsensor.frame_length);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x0006, imgsensor.frame_length);
	}

	/* Update Shutter */
	write_cmos_sensor_8(0x0073, (shutter & 0xFF0000) >> 16 );
	write_cmos_sensor(0x0074, shutter & 0x00FFFF);
    pr_debug("frame_length = %d , shutter = %d \n", imgsensor.frame_length, shutter);

}	/* set_shutter_frame_length */

static kal_uint16 gain2reg(const kal_uint16 gain)
{
    kal_uint16 reg_gain = 0x0000;
    reg_gain = gain / 4 - 16;

    return (kal_uint16)reg_gain;
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
	kal_uint16 reg_gain, max_gain = 16 * BASEGAIN;

	if (gain < BASEGAIN || gain > max_gain) {
		pr_debug("Error max gain setting: %d\n", max_gain);

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > max_gain)
			gain = max_gain;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	pr_debug("gain = %d, reg_gain = 0x%x, max_gain:0x%x\n ",
		gain, reg_gain, max_gain);

	reg_gain = reg_gain & 0x00FF;
    write_cmos_sensor(0x0077,reg_gain);

	return gain;
} /* set_gain */


static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable)
		write_cmos_sensor(0x0a00, 0x0100); // stream on
	else
		write_cmos_sensor(0x0a00, 0x0000); // stream off

	mdelay(10);
	return ERROR_NONE;
}

#if USE_BURST_MODE
#define I2C_BUFFER_LEN 1020 /* trans# max is 255, each 3 bytes */
static kal_uint16 hi846_table_write_cmos_sensor(kal_uint16 *para,
						 kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;
	int ret = 0;
	int retry_cnt = 0;

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
		/* Write when remain buffer size is less than 3 bytes
		 * or reach end of data
		 */
		if ((I2C_BUFFER_LEN - tosend) < 4
			|| IDX == len || addr != addr_last) {
			ret = iBurstWriteReg_multi(puSendCmd,
						tosend,
						imgsensor.i2c_write_id,
						4,
						imgsensor_info.i2c_speed);

			if( ret < 0 )
			{
				while(ret < 0 ){
					ret = iBurstWriteReg_multi(puSendCmd,
							tosend,
							imgsensor.i2c_write_id,
							4,
							imgsensor_info.i2c_speed);
					retry_cnt++;

					if( retry_cnt >3 )
					{
						retry_cnt =0 ;
						break;
					}
				}
			}

			tosend = 0;
		}
	}
	return 0;
}
#endif


// Hi-846_Xiaomi_L83_v10.5.1_24MHz_1440Mbps_2lane
static kal_uint16 hi846_init_setting[] = {
0x0066, 0x0101,
0x2000, 0x98E8,
0x2002, 0x00FF,
0x2004, 0x0006,
0x2008, 0x3FFF,
0x200A, 0xC314,
0x2022, 0x4130,
0x2034, 0x1292,
0x2036, 0xC02E,
0x2038, 0x4130,
0x206E, 0xF0B2,
0x2070, 0xFFBF,
0x2072, 0x2004,
0x2074, 0x43C2,
0x2076, 0x82FA,
0x2078, 0x12B0,
0x207A, 0xCAB0,
0x207C, 0x42A2,
0x207E, 0x7324,
0x2080, 0x4130,
0x2082, 0x120B,
0x2084, 0x425B,
0x2086, 0x008C,
0x2088, 0x4292,
0x208A, 0x7300,
0x208C, 0x82F2,
0x208E, 0x4292,
0x2090, 0x7302,
0x2092, 0x82F4,
0x2094, 0x1292,
0x2096, 0xC006,
0x2098, 0x421F,
0x209A, 0x0710,
0x209C, 0x523F,
0x209E, 0x4F82,
0x20A0, 0x82E4,
0x20A2, 0x93C2,
0x20A4, 0x829F,
0x20A6, 0x241E,
0x20A8, 0x403E,
0x20AA, 0xFFFE,
0x20AC, 0x40B2,
0x20AE, 0xEC78,
0x20B0, 0x82EC,
0x20B2, 0x40B2,
0x20B4, 0xEC78,
0x20B6, 0x82EE,
0x20B8, 0x40B2,
0x20BA, 0xEC78,
0x20BC, 0x82F0,
0x20BE, 0x934B,
0x20C0, 0x2405,
0x20C2, 0x4E0F,
0x20C4, 0x503F,
0x20C6, 0xFFD8,
0x20C8, 0x4F82,
0x20CA, 0x82EC,
0x20CC, 0x907B,
0x20CE, 0x0003,
0x20D0, 0x200B,
0x20D2, 0x421F,
0x20D4, 0x82EC,
0x20D6, 0x5E0F,
0x20D8, 0x4F82,
0x20DA, 0x82EE,
0x20DC, 0x5E0F,
0x20DE, 0x4F82,
0x20E0, 0x82F0,
0x20E2, 0x3C02,
0x20E4, 0x432E,
0x20E6, 0x3FE2,
0x20E8, 0x413B,
0x20EA, 0x4130,
0x20EC, 0x421F,
0x20EE, 0x7100,
0x20F0, 0x4F0E,
0x20F2, 0x503E,
0x20F4, 0xFFD8,
0x20F6, 0x4E82,
0x20F8, 0x7A04,
0x20FA, 0x421E,
0x20FC, 0x82EC,
0x20FE, 0x5F0E,
0x2100, 0x4E82,
0x2102, 0x7A06,
0x2104, 0x0B00,
0x2106, 0x7304,
0x2108, 0x0050,
0x210A, 0x40B2,
0x210C, 0xD081,
0x210E, 0x0B88,
0x2110, 0x421E,
0x2112, 0x82EE,
0x2114, 0x5F0E,
0x2116, 0x4E82,
0x2118, 0x7A0E,
0x211A, 0x521F,
0x211C, 0x82F0,
0x211E, 0x4F82,
0x2120, 0x7A10,
0x2122, 0x0B00,
0x2124, 0x7304,
0x2126, 0x007A,
0x2128, 0x40B2,
0x212A, 0x0081,
0x212C, 0x0B88,
0x212E, 0x4392,
0x2130, 0x7A0A,
0x2132, 0x0800,
0x2134, 0x7A0C,
0x2136, 0x0B00,
0x2138, 0x7304,
0x213A, 0x022B,
0x213C, 0x40B2,
0x213E, 0xD081,
0x2140, 0x0B88,
0x2142, 0x0B00,
0x2144, 0x7304,
0x2146, 0x0255,
0x2148, 0x40B2,
0x214A, 0x0081,
0x214C, 0x0B88,
0x214E, 0x9382,
0x2150, 0x7112,
0x2152, 0x2402,
0x2154, 0x4392,
0x2156, 0x760E,
0x2158, 0x4130,
0x215A, 0x120B,
0x215C, 0x120A,
0x215E, 0x4E0A,
0x2160, 0x4F0B,
0x2162, 0x4C0E,
0x2164, 0x4D0F,
0x2166, 0x8A0E,
0x2168, 0x7B0F,
0x216A, 0x2C02,
0x216C, 0x4A0C,
0x216E, 0x4B0D,
0x2170, 0x4C0E,
0x2172, 0x4D0F,
0x2174, 0x413A,
0x2176, 0x413B,
0x2178, 0x4130,
0x217A, 0x120B,
0x217C, 0x120A,
0x217E, 0x1209,
0x2180, 0x1208,
0x2182, 0x1207,
0x2184, 0x1206,
0x2186, 0x1205,
0x2188, 0x42D2,
0x218A, 0x82FA,
0x218C, 0x82A0,
0x218E, 0x403B,
0x2190, 0x00C1,
0x2192, 0x4B6F,
0x2194, 0x4FC2,
0x2196, 0x82D4,
0x2198, 0x43C2,
0x219A, 0x82D5,
0x219C, 0x1292,
0x219E, 0xC046,
0x21A0, 0x4292,
0x21A2, 0x7560,
0x21A4, 0x82F6,
0x21A6, 0x4292,
0x21A8, 0x7562,
0x21AA, 0x82F8,
0x21AC, 0x93CB,
0x21AE, 0x0000,
0x21B0, 0x2452,
0x21B2, 0x4215,
0x21B4, 0x7316,
0x21B6, 0x4216,
0x21B8, 0x7318,
0x21BA, 0x421F,
0x21BC, 0x0710,
0x21BE, 0x4F0E,
0x21C0, 0x430F,
0x21C2, 0x4507,
0x21C4, 0x4608,
0x21C6, 0x8E07,
0x21C8, 0x7F08,
0x21CA, 0x421F,
0x21CC, 0x82E2,
0x21CE, 0x522F,
0x21D0, 0x4F09,
0x21D2, 0x430A,
0x21D4, 0x470D,
0x21D6, 0x480E,
0x21D8, 0x490B,
0x21DA, 0x4A0C,
0x21DC, 0x870B,
0x21DE, 0x780C,
0x21E0, 0x2C02,
0x21E2, 0x490D,
0x21E4, 0x4A0E,
0x21E6, 0x4D0F,
0x21E8, 0x43D2,
0x21EA, 0x01B3,
0x21EC, 0x4D82,
0x21EE, 0x7324,
0x21F0, 0x4292,
0x21F2, 0x7540,
0x21F4, 0x82E8,
0x21F6, 0x4292,
0x21F8, 0x7542,
0x21FA, 0x82EA,
0x21FC, 0x434B,
0x21FE, 0x823F,
0x2200, 0x4F0C,
0x2202, 0x430D,
0x2204, 0x421E,
0x2206, 0x82E8,
0x2208, 0x421F,
0x220A, 0x82EA,
0x220C, 0x5E0C,
0x220E, 0x6F0D,
0x2210, 0x870C,
0x2212, 0x780D,
0x2214, 0x2801,
0x2216, 0x435B,
0x2218, 0x4BC2,
0x221A, 0x82FA,
0x221C, 0x93C2,
0x221E, 0x829A,
0x2220, 0x201A,
0x2222, 0x93C2,
0x2224, 0x82A0,
0x2226, 0x2404,
0x2228, 0x43B2,
0x222A, 0x7540,
0x222C, 0x43B2,
0x222E, 0x7542,
0x2230, 0x93C2,
0x2232, 0x82FA,
0x2234, 0x2410,
0x2236, 0x503E,
0x2238, 0x0003,
0x223A, 0x630F,
0x223C, 0x4E82,
0x223E, 0x82E8,
0x2240, 0x4F82,
0x2242, 0x82EA,
0x2244, 0x450C,
0x2246, 0x460D,
0x2248, 0x8E0C,
0x224A, 0x7F0D,
0x224C, 0x2C04,
0x224E, 0x4582,
0x2250, 0x82E8,
0x2252, 0x4682,
0x2254, 0x82EA,
0x2256, 0x4135,
0x2258, 0x4136,
0x225A, 0x4137,
0x225C, 0x4138,
0x225E, 0x4139,
0x2260, 0x413A,
0x2262, 0x413B,
0x2264, 0x4130,
0x2266, 0x403E,
0x2268, 0x00C2,
0x226A, 0x421F,
0x226C, 0x7314,
0x226E, 0xF07F,
0x2270, 0x000C,
0x2272, 0x5F4F,
0x2274, 0x5F4F,
0x2276, 0xDFCE,
0x2278, 0x0000,
0x227A, 0xF0FE,
0x227C, 0x000F,
0x227E, 0x0000,
0x2280, 0x4130,
0x2282, 0x120B,
0x2284, 0x120A,
0x2286, 0x1209,
0x2288, 0x1208,
0x228A, 0x1207,
0x228C, 0x1206,
0x228E, 0x93C2,
0x2290, 0x00C1,
0x2292, 0x249F,
0x2294, 0x425E,
0x2296, 0x00C2,
0x2298, 0xC35E,
0x229A, 0x425F,
0x229C, 0x82A0,
0x229E, 0xDF4E,
0x22A0, 0x4EC2,
0x22A2, 0x00C2,
0x22A4, 0x934F,
0x22A6, 0x248F,
0x22A8, 0x4217,
0x22AA, 0x7316,
0x22AC, 0x4218,
0x22AE, 0x7318,
0x22B0, 0x4326,
0x22B2, 0xB3E2,
0x22B4, 0x00C2,
0x22B6, 0x2482,
0x22B8, 0x0900,
0x22BA, 0x731C,
0x22BC, 0x0800,
0x22BE, 0x731C,
0x22C0, 0x421A,
0x22C2, 0x7300,
0x22C4, 0x421B,
0x22C6, 0x7302,
0x22C8, 0x421F,
0x22CA, 0x7304,
0x22CC, 0x9F82,
0x22CE, 0x829C,
0x22D0, 0x2C02,
0x22D2, 0x531A,
0x22D4, 0x630B,
0x22D6, 0x4A0E,
0x22D8, 0x4B0F,
0x22DA, 0x821E,
0x22DC, 0x82F2,
0x22DE, 0x721F,
0x22E0, 0x82F4,
0x22E2, 0x2C68,
0x22E4, 0x4A09,
0x22E6, 0x9339,
0x22E8, 0x3460,
0x22EA, 0x0B00,
0x22EC, 0x7304,
0x22EE, 0x0320,
0x22F0, 0x421E,
0x22F2, 0x7300,
0x22F4, 0x421F,
0x22F6, 0x7302,
0x22F8, 0x531E,
0x22FA, 0x630F,
0x22FC, 0x4E0C,
0x22FE, 0x4F0D,
0x2300, 0x821C,
0x2302, 0x82F6,
0x2304, 0x721D,
0x2306, 0x82F8,
0x2308, 0x2C0E,
0x230A, 0x93B2,
0x230C, 0x7560,
0x230E, 0x2003,
0x2310, 0x93B2,
0x2312, 0x7562,
0x2314, 0x2408,
0x2316, 0x4E82,
0x2318, 0x7540,
0x231A, 0x4F82,
0x231C, 0x7542,
0x231E, 0x4E82,
0x2320, 0x82F6,
0x2322, 0x4F82,
0x2324, 0x82F8,
0x2326, 0x4E82,
0x2328, 0x7316,
0x232A, 0x12B0,
0x232C, 0xFE66,
0x232E, 0x0900,
0x2330, 0x730E,
0x2332, 0x403F,
0x2334, 0x7316,
0x2336, 0x4A09,
0x2338, 0x8F29,
0x233A, 0x478F,
0x233C, 0x0000,
0x233E, 0x460C,
0x2340, 0x430D,
0x2342, 0x421E,
0x2344, 0x7300,
0x2346, 0x421F,
0x2348, 0x7302,
0x234A, 0x9C0E,
0x234C, 0x23F8,
0x234E, 0x9D0F,
0x2350, 0x23F6,
0x2352, 0x0B00,
0x2354, 0x7304,
0x2356, 0x01F4,
0x2358, 0x5036,
0x235A, 0x0006,
0x235C, 0x460C,
0x235E, 0x430D,
0x2360, 0x490E,
0x2362, 0x4E0F,
0x2364, 0x5F0F,
0x2366, 0x7F0F,
0x2368, 0xE33F,
0x236A, 0x521E,
0x236C, 0x82E8,
0x236E, 0x621F,
0x2370, 0x82EA,
0x2372, 0x12B0,
0x2374, 0xFD5A,
0x2376, 0x4E82,
0x2378, 0x7540,
0x237A, 0x4F82,
0x237C, 0x7542,
0x237E, 0x403B,
0x2380, 0x7316,
0x2382, 0x421C,
0x2384, 0x82E4,
0x2386, 0x430D,
0x2388, 0x4B2F,
0x238A, 0x590F,
0x238C, 0x4F0E,
0x238E, 0x430F,
0x2390, 0x12B0,
0x2392, 0xFD5A,
0x2394, 0x4E8B,
0x2396, 0x0000,
0x2398, 0x4BA2,
0x239A, 0x82CE,
0x239C, 0x4382,
0x239E, 0x82D0,
0x23A0, 0x12B0,
0x23A2, 0xFE66,
0x23A4, 0xD3D2,
0x23A6, 0x00C2,
0x23A8, 0x3C16,
0x23AA, 0x9329,
0x23AC, 0x3BC8,
0x23AE, 0x4906,
0x23B0, 0x5326,
0x23B2, 0x3FC5,
0x23B4, 0x4A09,
0x23B6, 0x8219,
0x23B8, 0x82CE,
0x23BA, 0x3F95,
0x23BC, 0x0800,
0x23BE, 0x731C,
0x23C0, 0x0900,
0x23C2, 0x731C,
0x23C4, 0x3F7D,
0x23C6, 0x0900,
0x23C8, 0x730C,
0x23CA, 0x0B00,
0x23CC, 0x7304,
0x23CE, 0x01F4,
0x23D0, 0x3FE9,
0x23D2, 0x0900,
0x23D4, 0x732C,
0x23D6, 0x425F,
0x23D8, 0x0788,
0x23DA, 0x4136,
0x23DC, 0x4137,
0x23DE, 0x4138,
0x23E0, 0x4139,
0x23E2, 0x413A,
0x23E4, 0x413B,
0x23E6, 0x4130,
0x23FE, 0xC056,
0x3236, 0xFC22,
0x323A, 0xFCEC,
0x323C, 0xFC82,
0x323E, 0xFD7A,
0x3246, 0xFE82,
0x3248, 0xFC34,
0x324E, 0xFC6E,
0x326A, 0xC374,
0x326C, 0xC37C,
0x326E, 0x0000,
0x3270, 0xC378,
0x32E2, 0x0020,
0x0A00, 0x0000,
0x0E04, 0x0012,
0x002E, 0x1111,
0x0032, 0x1111,
0x0022, 0x0008,
0x0026, 0x0040,
0x0028, 0x0017,
0x002C, 0x09CF,
0x005C, 0x2101,
0x0006, 0x09DE,
0x0008, 0x0ED8,
0x000E, 0x0200,
0x000C, 0x0022,
0x0A22, 0x0000,
0x0A24, 0x0000,
0x0804, 0x0000,
0x0A12, 0x0CC0,
0x0A14, 0x0990,
0x0710, 0x09B0,
0x0074, 0x09D8,
0x0076, 0x0000,
0x051E, 0x0000,
0x0200, 0x0400,
0x0A1A, 0x0C00,
0x0A0C, 0x0010,
0x0A1E, 0x0CCF,
0x0402, 0x0110,
0x0404, 0x00F4,
0x0408, 0x0000,
0x0410, 0x008D,
0x0412, 0x011A,
0x0414, 0x864C,
0x021C, 0x0001,
0x0C00, 0x9950,
0x0C06, 0x0021,
0x0C10, 0x0040,
0x0C12, 0x0040,
0x0C14, 0x0040,
0x0C16, 0x0040,
0x0A02, 0x0100,
0x0A04, 0x014A,
0x0418, 0x0000,
0x0128, 0x0028,
0x012A, 0xFFFF,
0x0120, 0x0046,
0x0122, 0x0376,
0x012C, 0x0020,
0x012E, 0xFFFF,
0x0124, 0x0040,
0x0126, 0x0378,
0x0746, 0x0050,
0x0748, 0x01D5,
0x074A, 0x022B,
0x074C, 0x03B0,
0x0756, 0x043F,
0x0758, 0x3F1D,
0x0B02, 0xE04D,
0x0B10, 0x6821,
0x0B12, 0x0020,
0x0B14, 0x0001,
0x2008, 0x38FD,
0x326E, 0x0000,
0x0900, 0x0320,
0x0902, 0xC31A,
0x0914, 0xC109,
0x0916, 0x061A,
0x0918, 0x0306,
0x091A, 0x0B09,
0x091C, 0x0C07,
0x091E, 0x0A00,
0x090C, 0x042A,
0x090E, 0x005B,
0x0954, 0x0089,
0x0956, 0x0000,
0x0958, 0x9900,
0x095A, 0x4A46,
0x0040, 0x0200,
0x0042, 0x0100,
0x0D04, 0x0000,
0x0F08, 0x2F04,
0x0F30, 0x001F,
0x0F36, 0x001F,
0x0F04, 0x3A00,
0x0F32, 0x025A,
0x0F38, 0x025A,
0x0F2A, 0x0024,
0x006A, 0x0100,
0x004C, 0x0100,
0x0044, 0x0001,

};

static kal_uint16 hi846_capture_setting[] = {
//Sensor Information////////////////////////////
//Sensor	  : Hi-846
//Image size      : 3264x2448
//MCLK/PCLK       : 24MHz /288Mhz
//MIPI speed(Mbps): 1440Mbps x 2Lane
//Frame Length    : 2526
//Line Length     : 3800
//Max Fps         : 30.00fps(vblank>1.0ms)
//Pixel order     : Green 1st (=GB)
//X/Y-flip        : X-flip
//Firmware Ver.   : v10.5.1
////////////////////////////////////////////////
0x002E, 0x1111,
0x0032, 0x1111,
0x0026, 0x0040,
0x002C, 0x09CF,
0x005C, 0x2101,
0x0006, 0x09DE,
0x0008, 0x0ED8,
0x000C, 0x0022,
0x0A22, 0x0000,
0x0A24, 0x0000,
0x0804, 0x0000,
0x0A12, 0x0CC0,
0x0A14, 0x0990,
0x0074, 0x09D8,
0x021C, 0x0001,
0x0A04, 0x014A,
0x0418, 0x0000,
0x0128, 0x0028,
0x012A, 0xFFFF,
0x0120, 0x0046,
0x0122, 0x0376,
0x012C, 0x0020,
0x012E, 0xFFFF,
0x0124, 0x0040,
0x0126, 0x0378,
0x0B02, 0xE04D,
0x0B10, 0x6821,
0x0B12, 0x0020,
0x0B14, 0x0001,
0x2008, 0x38FD,
0x326E, 0x0000,
0x0710, 0x09B0,
//========================================================
//		MIPI 2lane 1440Mbps
//========================================================
0x0900, 0x0320,
0x0902, 0xC31A,
0x0914, 0xC109,
0x0916, 0x061A,
0x0918, 0x0306,
0x091A, 0x0B09,
0x091C, 0x0C07,
0x091E, 0x0A00,
0x090C, 0x042A,
0x090E, 0x005B,
0x0954, 0x0089,
0x0956, 0x0000,
0x0958, 0x9900,
0x095A, 0x4A46,
0x0F32, 0x025A,
0x0F38, 0x025A,
0x0F2A, 0x0024,
0x004C, 0x0100,
};


static kal_uint16 hi846_preview_setting[] = {

//Sensor Information////////////////////////////
//Sensor	  : Hi-846
//Image size      : 1632x1224
//MCLK/PCLK       : 24MHz /288Mhz
//MIPI speed(Mbps): 720Mbps x 2Lane
//Frame Length    : 2526
//Line Length     : 3800
//Max Fps         : 30.00fps(vblank>17.1ms)
//Pixel order     : Green 1st (=GB)
//X/Y-flip        : X-flip
//Firmware Ver.   : v10.5.1
////////////////////////////////////////////////

0x002E, 0x3311,
0x0032, 0x3311,
0x0026, 0x0040,
0x002C, 0x09CF,
0x005C, 0x4202,
0x0006, 0x09DE,
0x0008, 0x0ED8,
0x000C, 0x0122,
0x0A22, 0x0100,
0x0A24, 0x0000,
0x0804, 0x0000,
0x0A12, 0x0660,
0x0A14, 0x04C8,
0x0074, 0x09D8,
0x021C, 0x0001,
0x0A04, 0x016A,
0x0418, 0x0000,
0x0128, 0x0028,
0x012A, 0xFFFF,
0x0120, 0x0046,
0x0122, 0x0376,
0x012C, 0x0020,
0x012E, 0xFFFF,
0x0124, 0x0040,
0x0126, 0x0378,
0x0B02, 0xE04D,
0x0B10, 0x6C21,
0x0B12, 0x0020,
0x0B14, 0x0005,
0x2008, 0x38FD,
0x326E, 0x0000,
0x0710, 0x04E0,
//========================================================
//		MIPI 2lane 720Mbps
//========================================================
0x0900, 0x0300,
0x0902, 0x4319,
0x0914, 0xC109,
0x0916, 0x061A,
0x0918, 0x0407,
0x091A, 0x0A0B,
0x091C, 0x0E08,
0x091E, 0x0A00,
0x090C, 0x0427,
0x090E, 0x0059,
0x0954, 0x0089,
0x0956, 0x0000,
0x0958, 0x9980,
0x095A, 0x4A46,
0x0F32, 0x025A,
0x0F38, 0x025A,
0x0F2A, 0x4124,
0x004C, 0x0100,
};


static kal_uint16 hi846_normal_video_setting[] = {
//Sensor Information////////////////////////////
//Sensor	  : Hi-846
//Image size      : 3264x2448
//MCLK/PCLK       : 24MHz /288Mhz
//MIPI speed(Mbps): 1440Mbps x 2Lane
//Frame Length    : 2526
//Line Length     : 3800
//Max Fps         : 30.00fps(vblank>1.0ms)
//Pixel order     : Green 1st (=GB)
//X/Y-flip        : X-flip
//Firmware Ver.   : v10.5.1
////////////////////////////////////////////////
0x002E, 0x1111,
0x0032, 0x1111,
0x0026, 0x0040,
0x002C, 0x09CF,
0x005C, 0x2101,
0x0006, 0x09DE,
0x0008, 0x0ED8,
0x000C, 0x0022,
0x0A22, 0x0000,
0x0A24, 0x0000,
0x0804, 0x0000,
0x0A12, 0x0CC0,
0x0A14, 0x0990,
0x0074, 0x09D8,
0x021C, 0x0001,
0x0A04, 0x014A,
0x0418, 0x0000,
0x0128, 0x0028,
0x012A, 0xFFFF,
0x0120, 0x0046,
0x0122, 0x0376,
0x012C, 0x0020,
0x012E, 0xFFFF,
0x0124, 0x0040,
0x0126, 0x0378,
0x0B02, 0xE04D,
0x0B10, 0x6821,
0x0B12, 0x0020,
0x0B14, 0x0001,
0x2008, 0x38FD,
0x326E, 0x0000,
0x0710, 0x09B0,
//========================================================
//		MIPI 2lane 1440Mbps
//========================================================
0x0900, 0x0320,
0x0902, 0xC31A,
0x0914, 0xC109,
0x0916, 0x061A,
0x0918, 0x0306,
0x091A, 0x0B09,
0x091C, 0x0C07,
0x091E, 0x0A00,
0x090C, 0x042A,
0x090E, 0x005B,
0x0954, 0x0089,
0x0956, 0x0000,
0x0958, 0x9900,
0x095A, 0x4A46,
0x0F32, 0x025A,
0x0F38, 0x025A,
0x0F2A, 0x0024,
0x004C, 0x0100,
};

static kal_uint16 hi846_hs_video_setting[] = {
//Sensor Information////////////////////////////
//Sensor	  : Hi-846
//Image size	  : 640x480
//MCLK/PCLK       : 24MHz /288Mhz
//MIPI speed(Mbps): 360Mbps x 2Lane
//Frame Length    : 2526
//Line Length     : 3800
//Max Fps 	  : 30.00fps(vblank>1.9ms)
//Pixel order     : Green 1st (=GB)
//X/Y-flip        : X-flip
//Firmware Ver.   : v10.5.1
////////////////////////////////////////////////
0x002E, 0x7711,
0x0032, 0x7711,
0x0026, 0x0148,
0x002C, 0x08C7,
0x005C, 0x4404,
0x0006, 0x09DE,
0x0008, 0x0ED8,
0x000C, 0x0322,
0x0A22, 0x0200,
0x0A24, 0x0000,
0x0804, 0x0058,
0x0A12, 0x0280,
0x0A14, 0x01E0,
0x0074, 0x09D8,
0x021C, 0x0001,
0x0A04, 0x016A,
0x0418, 0x0210,
0x0128, 0x0028,
0x012A, 0xFFFF,
0x0120, 0x0046,
0x0122, 0x0376,
0x012C, 0x0020,
0x012E, 0xFFFF,
0x0124, 0x0040,
0x0126, 0x0378,
0x0B02, 0xE04D,
0x0B10, 0x7021,
0x0B12, 0x0120,
0x0B14, 0x0001,
0x2008, 0x38FD,
0x326E, 0x0000,
0x0710, 0x01F2,
//========================================================
//		MIPI 2lane 360Mbps
//========================================================
0x0900, 0x0300,
0x0902, 0x4319,
0x0914, 0xC105,
0x0916, 0x030C,
0x0918, 0x0304,
0x091A, 0x0708,
0x091C, 0x0B04,
0x091E, 0x0500,
0x090C, 0x0208,
0x090E, 0x002C,
0x0954, 0x0089,
0x0956, 0x0000,
0x0958, 0xCA80,
0x095A, 0x9240,
0x0F32, 0x025A,
0x0F38, 0x025A,
0x0F2A, 0x4924,
0x004C, 0x0100,
};

static kal_uint16 hi846_slim_video_setting[] = {
//Sensor Information////////////////////////////
//Sensor	  : Hi-846
//Image size	  : 1280x720
//MCLK/PCLK 	  : 24MHz /288Mhz
//MIPI speed(Mbps): 720Mbps x 2Lane
//Frame Length	  : 2526
//Line Length	  : 3800
//Max Fps	  : 30.01fps(vblank>1.6ms)
//Pixel order	  : Green 1st (=GB)
//X/Y-flip		  : X-flip
//Firmware Ver.   : v10.5.1
////////////////////////////////////////////////
0x002E, 0x3311,
0x0032, 0x3311,
0x0026, 0x0238,
0x002C, 0x07D7,
0x005C, 0x4202,
0x0006, 0x09DE,
0x0008, 0x0ED8,
0x000C, 0x0122,
0x0A22, 0x0100,
0x0A24, 0x0000,
0x0804, 0x00B0,
0x0A12, 0x0500,
0x0A14, 0x02D0,
0x0074, 0x09D8,
0x021C, 0x0001,
0x0A04, 0x016A,
0x0418, 0x0410,
0x0128, 0x0028,
0x012A, 0xFFFF,
0x0120, 0x0046,
0x0122, 0x0376,
0x012C, 0x0020,
0x012E, 0xFFFF,
0x0124, 0x0040,
0x0126, 0x0378,
0x0B02, 0xE04D,
0x0B10, 0x6C21,
0x0B12, 0x0120,
0x0B14, 0x0005,
0x2008, 0x38FD,
0x326E, 0x0000,
0x0710, 0x02E8,
//========================================================
//		MIPI 2lane 720Mbps
//========================================================
0x0900, 0x0300,
0x0902, 0x4319,
0x0914, 0xC109,
0x0916, 0x061A,
0x0918, 0x0407,
0x091A, 0x0A0B,
0x091C, 0x0E08,
0x091E, 0x0A00,
0x090C, 0x0427,
0x090E, 0x0135,
0x0954, 0x0089,
0x0956, 0x0000,
0x0958, 0xCA80,
0x095A, 0x9240,
0x0F32, 0x025A,
0x0F38, 0x025A,
0x0F2A, 0x4124,
0x004C, 0x0100,
};



static void sensor_init(void)
{
	pr_debug("[%s] start\n", __func__);
	hi846_table_write_cmos_sensor(hi846_init_setting,
		sizeof(hi846_init_setting)/sizeof(kal_uint16));
}	/*	  sensor_init  */

static void preview_setting(void)
{
	pr_debug("%s +\n", __func__);

	hi846_table_write_cmos_sensor(hi846_preview_setting,
		sizeof(hi846_preview_setting)/sizeof(kal_uint16));

} /* preview_setting */


/*full size 30fps*/
static void capture_setting(kal_uint16 currefps)
{
	pr_debug("%s 30 fps E! currefps:%d\n", __func__, currefps);
	/*************MIPI output setting************/
	hi846_table_write_cmos_sensor(hi846_capture_setting,
		sizeof(hi846_capture_setting)/sizeof(kal_uint16));

   	pr_debug("X\n");
}

static void normal_video_setting(kal_uint16 currefps)
{
	pr_debug("%s E! currefps:%d\n", __func__, currefps);

	hi846_table_write_cmos_sensor(hi846_normal_video_setting,
		sizeof(hi846_normal_video_setting)/sizeof(kal_uint16));

   	pr_debug("X\n");
}

static void hs_video_setting(void)
{
	pr_debug("%s E! currefps 30\n", __func__);

	hi846_table_write_cmos_sensor(hi846_hs_video_setting,
		sizeof(hi846_hs_video_setting)/sizeof(kal_uint16));

	pr_debug("X\n");
}

static void slim_video_setting(void)
{
	pr_debug("%s E! 1920*1080@30fps\n", __func__);

	hi846_table_write_cmos_sensor(hi846_slim_video_setting,
		sizeof(hi846_slim_video_setting)/sizeof(kal_uint16));

	pr_debug("X\n");
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
	/*sensor have two i2c address 0x34 & 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}

			pr_debug("Read sensor id fail,read:0x%x id: 0x%x\n", return_sensor_id(), imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/*if Sensor ID is not correct,
		 *Must set *sensor_id to 0xFFFFFFFF
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
	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
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

	pr_debug("%s HI-846 initial setting start \n", __func__);
	sensor_init();
	pr_debug("%s HI-846 initial setting end \n", __func__);

	spin_lock(&imgsensor_drv_lock);

	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	spin_unlock(&imgsensor_drv_lock);
	pr_debug("%s -\n", __func__);

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
static kal_uint32 close(void)
{
	pr_debug("E\n");
	/* No Need to implement this function */
	streaming_control(KAL_FALSE);
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

	pr_debug("%s HI-846 preview setting start \n", __func__);
	preview_setting();
	pr_debug("%s HI-846 preview setting end \n", __func__);


	return ERROR_NONE;
} /* preview */

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

	pr_debug("%s HI-846 capture setting start \n", __func__);
	capture_setting(imgsensor.current_fps);
	pr_debug("%s HI-846 capture setting end \n", __func__);

	return ERROR_NONE;
}	/* capture() */
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

	pr_debug("%s HI-846 normal_video setting start \n", __func__);
	normal_video_setting(imgsensor.current_fps);
	pr_debug("%s HI-846 normal_video setting emd \n", __func__);

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

	pr_debug("%s HI-846 hs_video setting start \n", __func__);
	hs_video_setting();
	pr_debug("%s HI-846 hs_video setting end \n", __func__);

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s. 720P@240FPS\n", __func__);

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

	pr_debug("%s HI-846 slim_video setting start \n", __func__);
	slim_video_setting();
	pr_debug("%s HI-846 slim_video setting end \n", __func__);

	return ERROR_NONE;
}	/* slim_video */


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

	return ERROR_NONE;
} /* get_resolution */

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
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
	sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;

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
	sensor_info->PDAF_Support = 0;
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
}	/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	pr_debug("framerate = %d\n ", framerate);
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

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	pr_debug("set_test_pattern_mode enable: %d", enable);

	if (enable) {
		write_cmos_sensor(0x0a04, 0x0141);
		write_cmos_sensor(0x020a, 0x0100);
		write_cmos_sensor(0x020c, 0x0000);
		write_cmos_sensor(0x020d, 0x0000);
		write_cmos_sensor(0x0210, 0x0000);
		write_cmos_sensor(0x0212, 0x0000);
	} else {
		write_cmos_sensor(0x0a04, 0x0140);
		write_cmos_sensor(0x020a, 0x0000);
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
	/* unsigned long long *feature_return_para
	 *  = (unsigned long long *) feature_para;
	 */
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
#if 0
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
#endif
	/* SET_SENSOR_AWB_GAIN *pSetSensorAWB
	 *  = (SET_SENSOR_AWB_GAIN *)feature_para;
	 */
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data
		= (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/*pr_debug("feature_id = %d\n", feature_id);*/
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
	#ifdef VENDOR_EDIT
	case SENSOR_FEATURE_CHECK_MODULE_ID:
		*feature_return_para_32 = imgsensor_info.module_id;
		break;
	#endif
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor_8(sensor_reg_data->RegAddr,
				    sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor_8(sensor_reg_data->RegAddr);
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
	case SENSOR_FEATURE_GET_PDAF_DATA:
		pr_debug("SENSOR_FEATURE_GET_PDAF_DATA\n");
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
#if 0
	case SENSOR_FEATURE_GET_PDAF_INFO:
		pr_debug("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			(UINT16) *feature_data);
		PDAFinfo =
		  (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			imgsensor_pd_info_binning.i4BlockNumX = 0;
			imgsensor_pd_info_binning.i4BlockNumY = 0;
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_binning,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW: 
			imgsensor_pd_info_binning.i4BlockNumX = 0;
			imgsensor_pd_info_binning.i4BlockNumY = 0;
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_binning,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			imgsensor_pd_info_binning.i4BlockNumX = 0;
			imgsensor_pd_info_binning.i4BlockNumY = 0;
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_binning,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		pr_debug(
		"SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
			(UINT16) *feature_data);
		/*PDAF capacity enable or not, 2p8 only full size support PDAF*/
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
		pr_debug("SENSOR_FEATURE_GET_PDAF_REG_SETTING %d",
			(*feature_para_len));
		hi846_get_pdaf_reg_setting((*feature_para_len) / sizeof(UINT32)
					   , feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
		pr_debug("SENSOR_FEATURE_SET_PDAF_REG_SETTING %d",
			(*feature_para_len));
		hi846_set_pdaf_reg_setting((*feature_para_len) / sizeof(UINT32)
					   , feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF:
		pr_debug("PDAF mode :%d\n", *feature_data_16);
		imgsensor.pdaf_mode = *feature_data_16;
		break;
#endif
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data,
			(UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16) (*feature_data),
					(UINT16) (*(feature_data + 1)),
					(BOOL) (*(feature_data + 2)));
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
		switch (*(feature_data + 1)) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*feature_return_para_32 = 1; /*BINNING_AVERAGED*/
			break;
		}
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

#if 0
	case SENSOR_FEATURE_GET_VC_INFO:
		pvcinfo = (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[4],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[3],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		default:
			pr_info("error: get wrong vc_INFO id = %d",
			*feature_data_32);
			break;
		}
	break;
#endif
	default:
		break;
	}
	return ERROR_NONE;
} /* feature_control() */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 HI846_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
} /* HI846_MIPI_RAW_SensorInit */
