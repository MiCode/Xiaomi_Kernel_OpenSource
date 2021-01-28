// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 s5k4h7mipi_Sensor.c
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

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_typedef.h"
#include "imgsensor_ca.h"

#define PFX "s5k4h7_camera_sensor"
#define LOG_INF(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)
static DEFINE_SPINLOCK(imgsensor_drv_lock);
#ifndef VENDOR_EDIT
//#define VENDOR_EDIT
#endif

#include "s5k4h7mipiraw_Sensor.h"
#include "s5k4h7otp.h"

#define MULTI_WRITE 1
#if MULTI_WRITE
#define I2C_BUFFER_LEN 765 /* trans# max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN 3
#endif

#ifdef VENDOR_EDIT
/*zhengjiang.zhu@Camera.Drv, 2017/10/2 add for register device info*/
#define DEVICE_VERSION_S5K4H7    "s5k4h7"
/*Caohua.Lin@Camera.Drv, 20180126 remove register device adapt with mt6771*/
static kal_uint32 streaming_control(kal_bool enable);
static uint8_t deviceInfo_register_value;
#endif

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5K4H7_SENSOR_ID,	/*S5K4H7_SENSOR_ID = 0x487B */

	.checksum_value = 0xf16e8197,
	.pre = {
		.pclk = 272000000,
		.linelength = 3580,
		.framelength = 2530,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1632,
		.grabwindow_height = 1224,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 294000000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 272000000,
		.linelength = 3580,
		.framelength = 2530,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 293000000,
		.max_framerate = 300,
	},
	.cap1 = {
		.pclk = 272000000,
		.linelength = 3580,
		.framelength = 5060,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 293000000,
		.max_framerate = 150,
	},
	.normal_video = {
		.pclk = 272000000,
		.linelength = 3580,
		.framelength = 2530,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 1836,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 293000000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 272000000,
		.linelength = 3580,
		.framelength = 632,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 640,
		.grabwindow_height = 480,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 295000000,
		.max_framerate = 1200,
	},
	.slim_video = {
		.pclk = 272000000,
		.linelength = 3580,
		.framelength = 2530,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 294000000,
		.max_framerate = 300,
	},
	.margin = 4,
	.min_shutter = 4,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	//1, support; 0,not support
	.ihdr_le_firstline = 0,	//1,le first ; 0, se first
	.sensor_mode_num = 5,	//support sensor mode num

	.cap_delay_frame = 3,
	.pre_delay_frame = 3,
	.video_delay_frame = 3,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,

	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	//0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.mipi_settle_delay_mode = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
#ifndef VENDOR_EDIT
	/*Caohua.Lin@Camera.Driver  add for 17175  board 20180205 */
	.i2c_addr_table = {0x20},
#else
	.i2c_addr_table = {0x20, 0xff},
#endif
	.i2c_speed = 400,
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	//mirrorflip information
	// IMGSENSOR_MODE enum value,record current sensor mode,such as:
	// INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,	// current shutter
	.gain = 0x100,		// current gain
	.dummy_pixel = 0,	// current dummypixel
	.dummy_line = 0,	// current dummyline
	// full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.current_fps = 30,
	// auto flicker enable: KAL_FALSE for disable auto flicker,
	// KAL_TRUE for enable auto flicker
	.autoflicker_en = KAL_FALSE,
	// test pattern mode or not. KAL_TRUE for in test pattern mode,
	// KAL_FALSE for normal output
	.test_pattern = KAL_FALSE,
	.enable_secure = KAL_FALSE,
	//current scenario id
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,		// sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x20,
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{3264, 2448, 0, 0, 3264, 2448, 1632, 1224,
		0000, 0000, 1632, 1224, 0, 0, 1632, 1224},	// Preview
	{3264, 2448, 0, 0, 3264, 2448, 3264, 2448,
		0000, 0000, 3264, 2448, 0, 0, 3264, 2448},	// capture
	{3264, 2448, 0, 0, 3264, 2448, 3264, 1836,
		0000, 0000, 3264, 1836, 0, 0, 3264, 1836},	// video
	{3264, 2448, 0, 0, 3264, 2448, 640, 480,
		0000, 0000, 640, 480, 0, 0, 640, 480},	//hight speed video
	{3264, 2448, 351, 514, 2562, 1440, 1280, 720,
		0000, 0000, 1280, 720, 0, 0, 1280, 720} // slim video
};


static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 2, imgsensor.i2c_write_id);
	return ((get_byte << 8) & 0xff00) | ((get_byte >> 8) & 0x00ff);
}

/*
 * static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
 * {
 * char pusendcmd[4] = {
 * (char)(addr >> 8), (char)(addr & 0xFF),
 * (char)(para >> 8), (char)(para & 0xFF)};
 * iWriteRegI2C(pusendcmd , 4, imgsensor.i2c_write_id);
 * }
 */
static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[4] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)
	};

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static kal_uint16 table_write_cmos_sensor(kal_uint16 *para,
					  kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	tosend = 0;
	IDX = 0;

	while (len > IDX) {
		addr = para[IDX];

		{
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}
#if MULTI_WRITE
		/* Write when remain buffer size is less than 3 bytes
		 * or reach end of data
		 */
		if ((I2C_BUFFER_LEN - tosend) < 3
			|| IDX == len || addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd,
						tosend,
						imgsensor.i2c_write_id,
						3,
						imgsensor_info.i2c_speed);
			tosend = 0;
		}
#else
		iWriteRegI2C(puSendCmd, 3, imgsensor.i2c_write_id);
		tosend = 0;
#endif
	}

#if 0 /*for debug*/
	for (int i = 0; i < len/2; i++)
		LOG_INF("readback addr(0x%x)=0x%x\n",
			para[2*i], read_cmos_sensor_8(para[2*i]));
#endif
	return 0;
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);

	write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
	write_cmos_sensor_8(0x0342, imgsensor.line_length >> 8);
	write_cmos_sensor_8(0x0343, imgsensor.line_length & 0xFF);
}


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;
	//unsigned long flags;

	LOG_INF("framerate = %d, min_framelength_en=%d\n",
		framerate, min_framelength_en);
	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	LOG_INF("frame_length =%d\n", frame_length);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length)
		? frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length
		- imgsensor.min_frame_length;
	//dummy_line = frame_length - imgsensor.min_frame_length;
	//if (dummy_line < 0)
	//imgsensor.dummy_line = 0;
	//else
	//imgsensor.dummy_line = dummy_line;
	//imgsensor.frame_length = frame_length + imgsensor.dummy_line;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length
			- imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}				/*      set_max_framerate  */


static void write_shutter(kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;
	// shutter=2512;//add for debug capture framerate
	/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure
	 * larger than frame exposure
	 * AE doesn't update sensor gain at capture mode,
	 * thus extra exposure lines must be updated here.
	 */

	// OV Recommend Solution
	// if shutter bigger than frame_length, should extend frame length first

	// In order to keep the framerate of hs video mode,
	// preventing to enlarge the frame length
	if (imgsensor.sensor_mode == IMGSENSOR_MODE_HIGH_SPEED_VIDEO) {
		if (shutter >
		    imgsensor.min_frame_length - imgsensor_info.margin)
			shutter = imgsensor.min_frame_length
				- imgsensor_info.margin;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.frame_length = imgsensor.min_frame_length;
		spin_unlock(&imgsensor_drv_lock);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0202, (shutter >> 8) & 0xFF);
		write_cmos_sensor_8(0x0203, shutter & 0xFF);
		LOG_INF("shutter =%d, framelength =%d\n",
			shutter, imgsensor.frame_length);
		return;
	}
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

	if (imgsensor.autoflicker_en == KAL_TRUE) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10
			/ imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305) {
			set_max_framerate(296, 0);
			//set_dummy();
		} else if (realtime_fps >= 147 && realtime_fps <= 150) {
			set_max_framerate(146, 0);
			//set_dummy();
		} else {
			write_cmos_sensor_8(0x0340,
					    imgsensor.frame_length >> 8);
			write_cmos_sensor_8(0x0341,
					    imgsensor.frame_length & 0xFF);
		}
	} else {
		// Extend frame length
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
	}

	// Update Shutter
	write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
	write_cmos_sensor_8(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(0x0203, shutter & 0xFF);
	LOG_INF("realtime_fps =%d\n", realtime_fps);
	LOG_INF("shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);

	//LOG_INF("frame_length = %d ", frame_length);

}




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
}				/*      set_shutter */

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
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

	if (gain < BASEGAIN || gain > 32 * BASEGAIN) {
		LOG_INF("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 32 * BASEGAIN)
			gain = 32 * BASEGAIN;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	//write_cmos_sensor(0x0204,reg_gain);
	write_cmos_sensor_8(0x0204, (reg_gain >> 8));
	write_cmos_sensor_8(0x0205, (reg_gain & 0xff));

	return gain;
}				/*  s5k4h7MIPI_SetGain  */

#if 0
static void ihdr_write_shutter_gain(kal_uint16 le,
				    kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);

}
#endif

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
	case IMAGE_NORMAL:	//B
		write_cmos_sensor(0x0101, 0x00);	//Set normal
		break;
	case IMAGE_V_MIRROR:	//Gr X
		write_cmos_sensor(0x0101, 0x01);	//Set flip
		break;
	case IMAGE_H_MIRROR:	//Gb
		write_cmos_sensor(0x0101, 0x02);	//Set mirror
		break;
	case IMAGE_HV_MIRROR:	//R
		write_cmos_sensor(0x0101, 0x03);	//Set mirror and flip
		break;
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

static kal_uint32 streaming_control(kal_bool enable)
{
	int timeout = 100;
	int i = 0;
	int framecnt = 0;

	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable) {
		//write_cmos_sensor(0x6028,0x4000);
		write_cmos_sensor_8(0x0100, 0X01);
		mDELAY(10);
	} else {
		//write_cmos_sensor(0x6028,0x4000);
		write_cmos_sensor_8(0x0100, 0x00);
		for (i = 0; i < timeout; i++) {
			mDELAY(10);
			framecnt = read_cmos_sensor_8(0x0005);
			if (framecnt == 0xFF) {
				LOG_INF(" Stream Off OK at i=%d.\n", i);
				return ERROR_NONE;
			}
		}
		LOG_INF("Stream Off Fail! framecnt=%d.\n", framecnt);
	}
	return ERROR_NONE;
}

static kal_uint16 s5k4h7_init_setting[] = {
	0x0100, 0x00,
	0x0B05, 0x01,
	0x3074, 0x06,
	0x3075, 0x2F,
	0x308A, 0x20,
	0x308B, 0x08,
	0x308C, 0x0B,
	0x3081, 0x07,
	0x307B, 0x85,
	0x307A, 0x0A,
	0x3079, 0x0A,
	0x306E, 0x71,
	0x306F, 0x28,
	0x301F, 0x20,
	0x306B, 0x9A,
	0x3091, 0x1F,
	0x30C4, 0x06,
	0x3200, 0x09,
	0x306A, 0x79,
	0x30B0, 0xFF,
	0x306D, 0x08,
	0x3080, 0x00,
	0x3929, 0x3F,
	0x3084, 0x16,
	0x3070, 0x0F,
	0x3B45, 0x01,
	0x30C2, 0x05,
	0x3069, 0x87,
	0x3924, 0x7F,
	0x3925, 0xFD,
	0x3C08, 0xFF,
	0x3C09, 0xFF,
	0x3C31, 0xFF,
	0x3C32, 0xFF,
	0x0A02, 0x14,
	0x300A, 0x52,
	0x3012, 0x52,
	0x3013, 0x36,
	0x3019, 0x5F,
	0x301A, 0x57,
	0x3024, 0x10,
	0x3025, 0x4E,
	0x3026, 0x9A,
	0x302D, 0x0B,
	0x302E, 0x09,
};

static kal_uint16 s5k4h7_preview_setting[] = {
	0x0136, 0x18,
	0x0137, 0x00,
	0x0305, 0x06,
	0x0306, 0x00,
	0x0307, 0x88,
	0x030D, 0x06,
	0x030E, 0x00,
	0x030F, 0xB7,
	0x3C1F, 0x00,
	0x3C17, 0x00,
	0x3C1C, 0x05,
	0x3C1D, 0x15,
	0x0301, 0x04,
	0x0820, 0x02,
	0x0821, 0xDC,
	0x0822, 0x00,
	0x0823, 0x00,
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x3906, 0x00,
	0x0344, 0x00,
	0x0345, 0x08,
	0x0346, 0x00,
	0x0347, 0x08,
	0x0348, 0x0C,
	0x0349, 0xC7,
	0x034A, 0x09,
	0x034B, 0x97,
	0x034C, 0x06,
	0x034D, 0x60,
	0x034E, 0x04,
	0x034F, 0xC8,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x03,
	0x0101, 0x00,
	0x0340, 0x09,
	0x0341, 0xE2,
	0x0342, 0x0D,
	0x0343, 0xFC,
	0x0200, 0x0D,
	0x0201, 0x6C,
	0x0202, 0x00,
	0x0203, 0x02,
	0x3400, 0x01,
};

static kal_uint16 s5k4h7_cap_setting[] = {
	0x0136, 0x18,
	0x0137, 0x00,
	0x0305, 0x06,
	0x0306, 0x00,
	0x0307, 0x88,
	0x030D, 0x06,
	0x030E, 0x00,
	0x030F, 0xB7,
	0x3C1F, 0x00,
	0x3C17, 0x00,
	0x3C1C, 0x05,
	0x3C1D, 0x15,
	0x0301, 0x04,
	0x0820, 0x02,
	0x0821, 0xDC,
	0x0822, 0x00,
	0x0823, 0x00,
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x3906, 0x00,
	0x0344, 0x00,
	0x0345, 0x08,
	0x0346, 0x00,
	0x0347, 0x08,
	0x0348, 0x0C,
	0x0349, 0xC7,
	0x034A, 0x09,
	0x034B, 0x97,
	0x034C, 0x0C,
	0x034D, 0xC0,
	0x034E, 0x09,
	0x034F, 0x90,
	0x0900, 0x00,
	0x0901, 0x00,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0101, 0x00,
	0x0340, 0x09,
	0x0341, 0xE2,
	0x0342, 0x0D,
	0x0343, 0xFC,
	0x0200, 0x0D,
	0x0201, 0x6C,
	0x0202, 0x00,
	0x0203, 0x02,
	0x3400, 0x01,
};

static kal_uint16 s5k4h7_cap1_setting[] = {
	0x0136, 0x18,
	0x0137, 0x00,
	0x0305, 0x06,
	0x0306, 0x00,
	0x0307, 0x88,
	0x030D, 0x06,
	0x030E, 0x00,
	0x030F, 0xB7,
	0x3C1F, 0x00,
	0x3C17, 0x00,
	0x3C1C, 0x05,
	0x3C1D, 0x15,
	0x0301, 0x04,
	0x0820, 0x02,
	0x0821, 0xDC,
	0x0822, 0x00,
	0x0823, 0x00,
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x3906, 0x00,
	0x0344, 0x00,
	0x0345, 0x08,
	0x0346, 0x00,
	0x0347, 0x08,
	0x0348, 0x0C,
	0x0349, 0xC7,
	0x034A, 0x09,
	0x034B, 0x97,
	0x034C, 0x0C,
	0x034D, 0xC0,
	0x034E, 0x09,
	0x034F, 0x90,
	0x0900, 0x00,
	0x0901, 0x00,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0101, 0x00,
	0x0340, 0x13,
	0x0341, 0xC4,
	0x0342, 0x0D,
	0x0343, 0xFC,
	0x0200, 0x0D,
	0x0201, 0x6C,
	0x0202, 0x00,
	0x0203, 0x02,
	0x3400, 0x01,
};

static kal_uint16 s5k4h7_normal_video_setting[] = {
	0x0136, 0x18,
	0x0137, 0x00,
	0x0305, 0x06,
	0x0306, 0x00,
	0x0307, 0x88,
	0x030D, 0x06,
	0x030E, 0x00,
	0x030F, 0xB7,
	0x3C1F, 0x00,
	0x3C17, 0x00,
	0x3C1C, 0x05,
	0x3C1D, 0x15,
	0x0301, 0x04,
	0x0820, 0x02,
	0x0821, 0xDC,
	0x0822, 0x00,
	0x0823, 0x00,
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x3906, 0x00,
	0x0344, 0x00,
	0x0345, 0x08,
	0x0346, 0x01,
	0x0347, 0x3A,
	0x0348, 0x0C,
	0x0349, 0xC7,
	0x034A, 0x08,
	0x034B, 0x65,
	0x034C, 0x0C,
	0x034D, 0xC0,
	0x034E, 0x07,
	0x034F, 0x2C,
	0x0900, 0x00,
	0x0901, 0x00,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0101, 0x00,
	0x0340, 0x09,
	0x0341, 0xE2,
	0x0342, 0x0D,
	0x0343, 0xFC,
	0x0200, 0x0D,
	0x0201, 0x6C,
	0x0202, 0x00,
	0x0203, 0x02,
	0x3400, 0x01,
};

static kal_uint16 s5k4h7_hs_video_setting[] = {
	0x0136, 0x18,
	0x0137, 0x00,
	0x0305, 0x06,
	0x0306, 0x00,
	0x0307, 0x88,
	0x030D, 0x06,
	0x030E, 0x00,
	0x030F, 0xB7,
	0x3C1F, 0x00,
	0x3C17, 0x00,
	0x3C1C, 0x05,
	0x3C1D, 0x15,
	0x0301, 0x04,
	0x0820, 0x02,
	0x0821, 0xDC,
	0x0822, 0x00,
	0x0823, 0x00,
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x3906, 0x00,
	0x0344, 0x01,
	0x0345, 0x68,
	0x0346, 0x01,
	0x0347, 0x10,
	0x0348, 0x0B,
	0x0349, 0x67,
	0x034A, 0x08,
	0x034B, 0x8F,
	0x034C, 0x02,
	0x034D, 0x80,
	0x034E, 0x01,
	0x034F, 0xE0,
	0x0900, 0x01,
	0x0901, 0x44,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x07,
	0x0101, 0x00,
	0x0340, 0x02,
	0x0341, 0x78,
	0x0342, 0x0D,
	0x0343, 0xFC,
	0x0200, 0x0D,
	0x0201, 0x6C,
	0x0202, 0x02,
	0x0203, 0x08,
	0x3400, 0x01,
};

static kal_uint16 s5k4h7_slim_video_setting[] = {
	0x0136, 0x18,
	0x0137, 0x00,
	0x0305, 0x06,
	0x0306, 0x00,
	0x0307, 0x88,
	0x030D, 0x06,
	0x030E, 0x00,
	0x030F, 0xB7,
	0x3C1F, 0x00,
	0x3C17, 0x00,
	0x3C1C, 0x05,
	0x3C1D, 0x15,
	0x0301, 0x04,
	0x0820, 0x02,
	0x0821, 0xDC,
	0x0822, 0x00,
	0x0823, 0x00,
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x3906, 0x00,
	0x0344, 0x01,
	0x0345, 0x68,
	0x0346, 0x02,
	0x0347, 0x00,
	0x0348, 0x0B,
	0x0349, 0x67,
	0x034A, 0x07,
	0x034B, 0x9F,
	0x034C, 0x05,
	0x034D, 0x00,
	0x034E, 0x02,
	0x034F, 0xD0,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x03,
	0x0101, 0x00,
	0x0340, 0x09,
	0x0341, 0xE2,
	0x0342, 0x0D,
	0x0343, 0xFC,
	0x0200, 0x0D,
	0x0201, 0x6C,
	0x0202, 0x02,
	0x0203, 0x08,
	0x3400, 0x01,
};

static void sensor_init(void)
{
	table_write_cmos_sensor(s5k4h7_init_setting,
		sizeof(s5k4h7_init_setting) / sizeof(kal_uint16));
}				/*  s5k4h7MIPI_Sensor_Init  */


static void preview_setting(void)
{
	table_write_cmos_sensor(s5k4h7_preview_setting,
		sizeof(s5k4h7_preview_setting) / sizeof(kal_uint16));
}				/*  s5k4h7MIPI_Capture_Setting  */

static void capture_setting(kal_uint16 currefps)
{
	if (currefps == 150) {
		table_write_cmos_sensor(s5k4h7_cap1_setting,
			sizeof(s5k4h7_cap1_setting) / sizeof(kal_uint16));
	} else {
		table_write_cmos_sensor(s5k4h7_cap_setting,
			sizeof(s5k4h7_cap_setting) / sizeof(kal_uint16));
	}
}

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n", currefps);
	table_write_cmos_sensor(s5k4h7_normal_video_setting,
		sizeof(s5k4h7_normal_video_setting) / sizeof(kal_uint16));
}

static void hs_video_setting(void)
{
	LOG_INF("EVGA 120fps");
	table_write_cmos_sensor(s5k4h7_hs_video_setting,
		sizeof(s5k4h7_hs_video_setting) / sizeof(kal_uint16));
}

static void slim_video_setting(void)
{
	LOG_INF("E");
	table_write_cmos_sensor(s5k4h7_slim_video_setting,
		sizeof(s5k4h7_slim_video_setting) / sizeof(kal_uint16));
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
	int retry = 1;
	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	//we should detect the module used i2c address
	//while (imgsensor_info.i2c_addr_table[i] != 0xff) {
	spin_lock(&imgsensor_drv_lock);
	imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
	spin_unlock(&imgsensor_drv_lock);
	do {
		*sensor_id = ((read_cmos_sensor_8(0x0000) << 8)
			      | read_cmos_sensor_8(0x0001));
		LOG_INF("read_0x0000=0x%x, 0x0001=0x%x,0x0000_0001=0x%x\n",
			read_cmos_sensor_8(0x0000), read_cmos_sensor_8(0x0001),
			read_cmos_sensor(0x0000));
		if (*sensor_id == imgsensor_info.sensor_id) {
#ifdef VENDOR_EDIT
			/*
			 * zhengjiang.zhu@Camera.Drv,
			 * 2017/10/18 add for register device info
			 */
			imgsensor_info.module_id = s5k4h7_get_module_id();
			/*
			 * Caohua.Lin@Camera.Drv,
			 * 20180126 remove to adapt with mt6771
			 */
			if (deviceInfo_register_value == 0x00) {
				register_imgsensor_deviceinfo("Cam_f",
					DEVICE_VERSION_S5K4H7,
					imgsensor_info.module_id);
				deviceInfo_register_value = 0x01;
			}
#endif
			LOG_INF(
				"i2c write id: 0x%x, sensor id: 0x%x module_id 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id,
				imgsensor_info.module_id);
			break;
		}
		LOG_INF("Read sensor id fail, id: 0x%x,0x%x\n",
			imgsensor.i2c_write_id, *sensor_id);
		retry--;
	} while (retry > 0);
	//i++;
	//retry = 2;
	//      }
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
#define i2c_fixed
static kal_uint32 open(void)
{
	//const kal_uint8 i2c_addr[] =
	//{IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2};
	kal_uint8 retry = 1;
	kal_uint16 sensor_id = 0;

#ifdef VENDOR_EDIT
	/*zhengjiang.zhu@Camera.Drv, 2017/10/18 add for otp */
	bool otp_flag = 0;
#endif

	LOG_INF("%s imgsensor.enable_secure %d\n",
		__func__, imgsensor.enable_secure);

	LOG_INF("PLATFORM:MT6595,MIPI 2LANE\n");
	LOG_INF(
		"preview 1280*960@30fps,864Mbps/lane; video 1280*960@30fps,864Mbps/lane; capture 5M@30fps,864Mbps/lane\n");

	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	//we should detect the module used i2c address
	//while (imgsensor_info.i2c_addr_table[i] != 0xff) {
	spin_lock(&imgsensor_drv_lock);
	imgsensor.i2c_write_id = 0x20;	//imgsensor_info.i2c_addr_table[i];
	spin_unlock(&imgsensor_drv_lock);
	do {
		sensor_id = ((read_cmos_sensor_8(0x0000) << 8)
			     | read_cmos_sensor_8(0x0001));
		if (sensor_id == imgsensor_info.sensor_id) {
			LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
				imgsensor.i2c_write_id,
				sensor_id);
			break;
		}
		LOG_INF("Read sensor id fail, id: 0x%x,0x%x\n",
			imgsensor.i2c_write_id, sensor_id);
		retry--;
	} while (retry > 0);
	//i++;
	//if (sensor_id == imgsensor_info.sensor_id)
	//      break;
	//retry = 2;
	//}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail sequence write in  */
	sensor_init();
#ifdef VENDOR_EDIT
	/*zhengjiang.zhu@Camera.Drv, 2017/10/18 add for otp */
	otp_flag = S5K4H7_otp_update();
	if (otp_flag)
		LOG_INF("Load otp succeed\n");
	else
		LOG_INF("Load otp failed\n");
#endif
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
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}		/* open */



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
	MUINT32 ret = ERROR_NONE;

	return ret;
}		/* close */


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
	//imgsensor.video_mode = KAL_FALSE;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}		/* s5k4h7MIPIPreview */

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
	//PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF(
				"Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n",
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
}		/* capture() */

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
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	return ERROR_NONE;
}		/* s5k4h7MIPIPreview */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	return ERROR_NONE;
}		/* s5k4h7MIPIPreview */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	return ERROR_NONE;
}		/* s5k4h7MIPIPreview */



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
}				/*      get_resolution  */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	/* not use */
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	// inverse with datasheet
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

	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
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

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;	/* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;	/* not use */
	sensor_info->SensorPixelClockCount = 3;	/* not use */
	sensor_info->SensorDataLatchCount = 2;	/* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;	// 0 is default 1x
	sensor_info->SensorHightSampling = 0;	// 0 is default 1x
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
}	/* get_info */


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
	// SetVideoMode Function should fix framerate
	if (framerate == 0)
		// Dynamic frame rate
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
	if (enable)	//enable auto flicker
		imgsensor.autoflicker_en = KAL_TRUE;
	else		//Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MUINT32 framerate)
{
	kal_uint32 frameHeight;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);
	if (framerate == 0)
		return ERROR_NONE;
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frameHeight = imgsensor_info.pre.pclk / framerate * 10
			/ imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		if (frameHeight > imgsensor_info.pre.framelength)
			imgsensor.dummy_line = frameHeight
				- imgsensor_info.pre.framelength;
		else
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		frameHeight = imgsensor_info.normal_video.pclk / framerate * 10
			/ imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		if (frameHeight > imgsensor_info.normal_video.framelength)
			imgsensor.dummy_line = frameHeight
				- imgsensor_info.normal_video.framelength;
		else
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	// case MSDK_SCENARIO_ID_CAMERA_ZSD:
		if (imgsensor.current_fps ==
		    imgsensor_info.cap1.max_framerate) {
			frameHeight = imgsensor_info.cap1.pclk / framerate * 10
				/ imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			if (frameHeight > imgsensor_info.cap1.framelength)
				imgsensor.dummy_line = frameHeight
					- imgsensor_info.cap1.framelength;
			else
				imgsensor.dummy_line = 0;
			imgsensor.frame_length = imgsensor_info.cap1.framelength
				+ imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			LOG_INF(
				"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				framerate,
				imgsensor_info.cap.max_framerate / 10);
			frameHeight = imgsensor_info.cap.pclk / framerate * 10
				/ imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			if (frameHeight > imgsensor_info.cap.framelength)
				imgsensor.dummy_line = frameHeight
					- imgsensor_info.cap.framelength;
			else
				imgsensor.dummy_line = 0;
			imgsensor.frame_length = imgsensor_info.cap.framelength
				+ imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}

		set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frameHeight = imgsensor_info.hs_video.pclk / framerate * 10
			/ imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		if (frameHeight > imgsensor_info.hs_video.framelength)
			imgsensor.dummy_line = frameHeight
				- imgsensor_info.hs_video.framelength;
		else
			imgsensor.dummy_line = 0;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frameHeight = imgsensor_info.slim_video.pclk / framerate * 10
			/ imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		if (frameHeight > imgsensor_info.slim_video.framelength)
			imgsensor.dummy_line = frameHeight
				- imgsensor_info.slim_video.framelength;
		else
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	default:	//coding with  preview scenario by default
		frameHeight = imgsensor_info.pre.pclk / framerate * 10
			/ imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		if (frameHeight > imgsensor_info.pre.framelength)
			imgsensor.dummy_line = frameHeight
				- imgsensor_info.pre.framelength;
		else
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength
			+ imgsensor.dummy_line;
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
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable)
		write_cmos_sensor_8(0x0601, 0x02);
	else
		write_cmos_sensor_8(0x0601, 0x00);
	write_cmos_sensor_8(0x3200, 0x00);

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
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	//LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
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
		write_cmos_sensor_8(sensor_reg_data->RegAddr,
				    sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		// get the lens driver ID from EEPROM or just
		// return LENS_DRIVER_ID_DO_NOT_CARE
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
		set_auto_flicker_mode((BOOL) (*feature_data_16),
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
			(MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) * feature_data);
		break;
	case SENSOR_FEATURE_SET_AS_SECURE_DRIVER:
		spin_lock(&imgsensor_drv_lock);
		imgsensor.enable_secure = ((kal_bool) *feature_data);
		spin_unlock(&imgsensor_drv_lock);
		LOG_INF("imgsensor.enable_secure :%d\n",
			imgsensor.enable_secure);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		//for factory mode auto testing
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", (UINT32) *feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
		// case SENSOR_FEATURE_SET_HDR:
		// LOG_INF("ihdr enable :%d\n", *feature_data_16);
		// spin_lock(&imgsensor_drv_lock);
		//         imgsensor.ihdr_en = *feature_data_16;
		// spin_unlock(&imgsensor_drv_lock);
		//         break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		/*
		 * LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
		 * *feature_data_32);
		 * wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)
		 * (*(feature_data_32+1));
		 */
		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)
			(*(feature_data + 1));
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
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		//ihdr_write_shutter_gain((UINT16)*feature_data,
		//(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
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
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
	}
	break;

	default:
		break;
	}

	return ERROR_NONE;
}				/*      feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 S5K4H7_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*      s5k4h7_MIPI_RAW_SensorInit      */
