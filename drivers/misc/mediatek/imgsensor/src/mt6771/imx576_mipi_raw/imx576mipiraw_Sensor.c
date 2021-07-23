/*
 * Copyright (C) 2017 MediaTek Inc.
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
/* #include <asm/system.h> */
/* #include <linux/xlog.h> */
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "imx576mipiraw_Sensor.h"

#define PFX "IMX576_camera_sensor"

/* Add by LiuBin for register device info at 20160616 */
#define DEVICE_VERSION_IMX576     "imx576"
/* extern void register_imgsensor_deviceinfo(
 *	char *name, char *version, u8 module_id);
 */
/* static uint8_t deviceInfo_register_value = 0x00; */


/* #define LOG_WRN(format, args...) xlog_printk(ANDROID_LOG_WARN ,PFX,\ */
/*					"[%S] " format, __FUNCTION__, ##args) */
/* #defineLOG_INF(format, args...) xlog_printk(ANDROID_LOG_INFO ,PFX,\ */
/*					"[%s] " format, __FUNCTION__, ##args) */
/* #define LOG_DBG(format, args...) xlog_printk(ANDROID_LOG_DEBUG ,PFX, "[%S]\*/
/*					" format, __FUNCTION__, ##args) */
#define LOG_INF(format, args...)  pr_debug(PFX "[%s] " format, __func__, ##args)
static DEFINE_SPINLOCK(imgsensor_drv_lock);




static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = IMX576_SENSOR_ID,

	.module_id = 0x01,  /* 0x01 Sunny,0x05 QTEK */
	.sensor_ver = 0x01,  /*0x01:Cut0.91 ES1, 0x01:Cut0.91 ES2*/

	.checksum_value = 0xffb1ec31,
	.pre = {  /*ver3_01_HDR*/
		.pclk = 820800000,
		.linelength = 6144,
		.framelength = 4453,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2880, /*0x0B40*/
		.grabwindow_height = 2156,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 445891290,
	},
	.cap = {  /*full size*/
		.pclk = 816000000,
		.linelength = 6144,
		.framelength = 4427,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 5760,
		.grabwindow_height = 4312,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 814213669,
	},
	.normal_video = { /*C-Normal-2x2-Binning-30FPS*/
		.pclk = 211200000,
		.linelength = 3160,
		.framelength = 2227,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2880,
		.grabwindow_height = 2156,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 435982666,
	},
	.hs_video = {
		.pclk = 211200000,
		.linelength = 3160,
		.framelength = 2227,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2816,
		.grabwindow_height = 2112,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 211200000,
		.linelength = 3160,
		.framelength = 2227,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2816,
		.grabwindow_height = 2112,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.margin = 5,
	.min_shutter = 1,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	  /* 1, support; 0,not support */
	.ihdr_le_firstline = 0,  /* 1,le first ; 0, se first */
	.sensor_mode_num = 5,	  /* support sensor mode num */

	.cap_delay_frame = 2,/* 3 guanjd modify */
	.pre_delay_frame = 2,/* 3 guanjd modify */
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,
	/*zhengjiang.zhu@EXP CameraDrv, 2017/03/03 Increase ISP drive ability*/
	.isp_driving_current = ISP_DRIVING_8MA,  /* 2ma */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,

	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,

	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.mipi_settle_delay_mode = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x20, 0x21, 0xff},
	.i2c_speed = 320, /* i2c read/write speed */
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,		/* mirrorflip information */
	/* IMGSENSOR_MODE enum value,record current sensor mode,
	 * such as: INIT, Preview, Capture, Video
	 * High Speed Video, Slim Video
	 */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,		/* current shutter */
	.gain = 0x100,			/* current gain */
	.dummy_pixel = 0,		/* current dummypixel */
	.dummy_line = 0,		/* current dummyline */

	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.current_fps = 0,

	/* auto flicker enable: KAL_FALSE for disable auto flicker,
	 * KAL_TRUE for enable auto flicker
	 */
	.autoflicker_en = KAL_FALSE,

	/* test pattern mode or not. KAL_FALSE for in test pattern mode,
	 * KAL_TRUE for normal output
	 */
	.test_pattern = KAL_FALSE,

	/* current scenario id */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,

	.ihdr_mode = 0, /* sensor need support LE, SE with HDR feature */
	.hdr_mode = 0, /* HDR mODE : 0: disable HDR, 1:IHDR, 2:HDR, 9:ZHDR */
	.i2c_write_id = 0x20,

};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{ 5760,  4312,  0,  0,  5760,  4312,  2880,  2156,
	  0000,  0000,  2880,  2156,  0,  0,  2880,  2156},/*Preview*/
	{ 5760,  4312,  0,  0,  5760,  4312,  5760,  4312,
	  0000,  0000,  5760,  4312,  0,  0,  5760,  4312},/*capture*/
	{ 5760,  4312,  0,  0,  5760,  4312,  2880,  2156,
	  0000,  0000,  2880,  2156,  0,  0,  2880,  2156}, /* video */
	{ 5760,  4312,  0,  0,  5760,  4312,  2880,  2156,
	  0000,  0000,  2880,  2156,  0,  0,  2880,  2156}, /*hs video*/
	{ 5760,  4312,  0,  0,  5760,  4312,  2880,  2156,
	  0000,  0000,  2880,  2156,  0,  0,  2880,  2156}, /* slim video*/
};


#if 0 /*Description*/
	MUINT16 VC_Num;
	MUINT16 VC_PixelNum;
	MUINT16 ModeSelect;   /* 0: auto mode, 1:direct mode  */
	MUINT16 EXPO_Ratio;   /* 1/1, 1/2, 1/4, 1/8 */
	MUINT16 ODValue;      /* OD Value */
	/* STATS divistion mdoe 0: 16x16, 1:8x8, 2:4x4, 3:1x1*/
	MUINT16 RG_STATSMODE;
#endif

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
	 0x00, 0x2b, 0x0B40, 0x086C, 0x00, 0x00, 0x0000, 0x0000,
	 0x00, 0x00, 0x0000, 0x0000, 0x00, 0x00, 0x0000, 0x0000}
};

/*zhaozhengtao 2016/02/19,modify for different module*/
#define MODULE_ID_OFFSET 0x0000

#define EEPROM_READ_ID  0xA0
#define EEPROM_WRITE_ID   0xA1

/*zhaozhengtao 2016/02/19,modify for different module*/
static kal_uint16 read_module_id(void)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(MODULE_ID_OFFSET >> 8),
			     (char)(MODULE_ID_OFFSET & 0xFF)};

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, 0xA2/*EEPROM_READ_ID*/);
	return get_byte;

}

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 2, imgsensor.i2c_write_id);
	return ((get_byte<<8)&0xff00)|((get_byte>>8)&0x00ff);
}


static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {(char)(addr >> 8),
			     (char)(addr & 0xFF),
			     (char)(para >> 8),
			     (char)(para & 0xFF)};

	/*kdSetI2CSpeed(imgsensor_info.i2c_speed);*/
	/* Add this func to set i2c speed by each sensor */
	iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	/*kdSetI2CSpeed(imgsensor_info.i2c_speed);*/
	/* Add this func to set i2c speed by each sensor */
	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[4] = {(char)(addr >> 8),
			     (char)(addr & 0xFF),
			     (char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	LOG_INF("frame_length = %d, line_length = %d\n",
			imgsensor.frame_length,
			imgsensor.line_length);

	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
	write_cmos_sensor_8(0x0342, imgsensor.line_length >> 8);
	write_cmos_sensor_8(0x0343, imgsensor.line_length & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);

}	/*	set_dummy  */


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{

	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d, min framelength should enable %d\n",
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

static void write_shutter(kal_uint16 shutter)
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
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 /
							imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor_8(0x0104, 0x01);
			write_cmos_sensor_8(0x0340,
						imgsensor.frame_length >> 8);
			write_cmos_sensor_8(0x0341,
						imgsensor.frame_length & 0xFF);
			write_cmos_sensor_8(0x0104, 0x00);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
	}

	/* Update Shutter */
	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor_8(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(0x0203, shutter  & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);
	LOG_INF("Exit! shutter =%d, framelength =%d\n",
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
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}	/*	set_shutter */




static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain;

	reg_gain = 1024 - (1024*64)/gain;

	LOG_INF("imx576 gain =%d, reg_gain =%d\n", gain, reg_gain);

	return reg_gain;
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

    /* gain=1024;//for test */
    /* return; //for test */

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

	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor_8(0x0204, (reg_gain>>8) & 0xFF);
	write_cmos_sensor_8(0x0205, reg_gain & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);

	return gain;
}	/*	set_gain  */

static void set_mirror_flip(kal_uint8 image_mirror)
{
	kal_uint8 itemp;

	LOG_INF("image_mirror = %d\n", image_mirror);
	itemp = read_cmos_sensor_8(0x0101);
	itemp &= ~0x03;

	switch (image_mirror) {

	case IMAGE_NORMAL:
	write_cmos_sensor_8(0x0101, itemp);
	break;

	case IMAGE_V_MIRROR:
	write_cmos_sensor_8(0x0101, itemp | 0x02);
	break;

	case IMAGE_H_MIRROR:
	write_cmos_sensor_8(0x0101, itemp | 0x01);
	break;

	case IMAGE_HV_MIRROR:
	write_cmos_sensor_8(0x0101, itemp | 0x03);
	break;
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
#if 0
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}	/*	night_mode	*/
#endif

static void sensor_init(void)
{
	write_cmos_sensor_8(0x0136, 0x18);
	write_cmos_sensor_8(0x0137, 0x00);
	write_cmos_sensor_8(0x3C7E, 0x01);
	write_cmos_sensor_8(0x3C7F, 0x04);
	write_cmos_sensor_8(0x37EA, 0xFF);
	write_cmos_sensor_8(0x37EE, 0xFF);
	write_cmos_sensor_8(0x380D, 0x00);
	write_cmos_sensor_8(0x3C00, 0x10);
	write_cmos_sensor_8(0x3C01, 0x10);
	write_cmos_sensor_8(0x3C02, 0x10);
	write_cmos_sensor_8(0x3C03, 0x10);
	write_cmos_sensor_8(0x3C04, 0x10);
	write_cmos_sensor_8(0x3F89, 0x01);
	write_cmos_sensor_8(0x4430, 0x02);
	write_cmos_sensor_8(0x4B8E, 0x18);
	write_cmos_sensor_8(0x4B8F, 0x10);
	write_cmos_sensor_8(0x4BA8, 0x08);
	write_cmos_sensor_8(0x4BAA, 0x08);
	write_cmos_sensor_8(0x4BAB, 0x08);
	write_cmos_sensor_8(0x4BC9, 0x10);
	write_cmos_sensor_8(0x5511, 0x01);
	write_cmos_sensor_8(0x560B, 0x5B);
	write_cmos_sensor_8(0x56A7, 0x60);
	write_cmos_sensor_8(0x5B3B, 0x60);
	write_cmos_sensor_8(0x5BA7, 0x60);
	write_cmos_sensor_8(0x6002, 0x00);
	write_cmos_sensor_8(0x6014, 0x01);
	write_cmos_sensor_8(0x6118, 0x0A);
	write_cmos_sensor_8(0x6122, 0x0A);
	write_cmos_sensor_8(0x6128, 0x0A);
	write_cmos_sensor_8(0x6132, 0x0A);
	write_cmos_sensor_8(0x6138, 0x0A);
	write_cmos_sensor_8(0x6142, 0x0A);
	write_cmos_sensor_8(0x6148, 0x0A);
	write_cmos_sensor_8(0x6152, 0x0A);
	write_cmos_sensor_8(0x617B, 0x04);
	write_cmos_sensor_8(0x617E, 0x04);
	write_cmos_sensor_8(0x6181, 0x04);
	write_cmos_sensor_8(0x6184, 0x04);
	write_cmos_sensor_8(0x6187, 0x04);
	write_cmos_sensor_8(0x618A, 0x04);
	write_cmos_sensor_8(0x618D, 0x04);
	write_cmos_sensor_8(0x6190, 0x04);
	write_cmos_sensor_8(0x6193, 0x04);
	write_cmos_sensor_8(0x6196, 0x04);
	write_cmos_sensor_8(0x6199, 0x04);
	write_cmos_sensor_8(0x619C, 0x04);
	write_cmos_sensor_8(0x619F, 0x04);
	write_cmos_sensor_8(0x61A2, 0x04);
	write_cmos_sensor_8(0x61A5, 0x04);
	write_cmos_sensor_8(0x61A8, 0x04);
	write_cmos_sensor_8(0x61AB, 0x04);
	write_cmos_sensor_8(0x61AE, 0x04);
	write_cmos_sensor_8(0x61B1, 0x04);
	write_cmos_sensor_8(0x61B4, 0x04);
	write_cmos_sensor_8(0x61B7, 0x04);
	write_cmos_sensor_8(0x61BA, 0x04);
	write_cmos_sensor_8(0x61BD, 0x04);
	write_cmos_sensor_8(0x61C0, 0x04);
	write_cmos_sensor_8(0x61C3, 0x04);
	write_cmos_sensor_8(0x61C6, 0x04);
	write_cmos_sensor_8(0x61C9, 0x04);
	write_cmos_sensor_8(0x61CC, 0x04);
	write_cmos_sensor_8(0x61CF, 0x04);
	write_cmos_sensor_8(0x61D2, 0x04);
	write_cmos_sensor_8(0x61D5, 0x04);
	write_cmos_sensor_8(0x61D8, 0x04);
	write_cmos_sensor_8(0x61DB, 0x04);
	write_cmos_sensor_8(0x61DE, 0x04);
	write_cmos_sensor_8(0x61E1, 0x04);
	write_cmos_sensor_8(0x61E4, 0x04);
	write_cmos_sensor_8(0x61E7, 0x04);
	write_cmos_sensor_8(0x61EA, 0x04);
	write_cmos_sensor_8(0x61ED, 0x04);
	write_cmos_sensor_8(0x61F0, 0x04);
	write_cmos_sensor_8(0x61F3, 0x04);
	write_cmos_sensor_8(0x61F6, 0x04);
	write_cmos_sensor_8(0x61F9, 0x04);
	write_cmos_sensor_8(0x61FC, 0x04);
	write_cmos_sensor_8(0x61FF, 0x04);
	write_cmos_sensor_8(0x6202, 0x04);
	write_cmos_sensor_8(0x6205, 0x04);
	write_cmos_sensor_8(0x6208, 0x04);
	write_cmos_sensor_8(0x620B, 0x04);
	write_cmos_sensor_8(0x620E, 0x04);
	write_cmos_sensor_8(0x6211, 0x04);
	write_cmos_sensor_8(0x6214, 0x04);
	write_cmos_sensor_8(0x6217, 0x04);
	write_cmos_sensor_8(0x621A, 0x04);
	write_cmos_sensor_8(0x621D, 0x04);
	write_cmos_sensor_8(0x6220, 0x04);
	write_cmos_sensor_8(0x6223, 0x04);
	write_cmos_sensor_8(0x6226, 0x04);
	write_cmos_sensor_8(0x6229, 0x04);
	write_cmos_sensor_8(0x622C, 0x04);
	write_cmos_sensor_8(0x681D, 0xBA);
	write_cmos_sensor_8(0x681E, 0xA0);
	write_cmos_sensor_8(0x6823, 0x32);
	write_cmos_sensor_8(0x6824, 0x6C);
	write_cmos_sensor_8(0x6828, 0x9B);
	write_cmos_sensor_8(0x682A, 0x13);
	write_cmos_sensor_8(0x682C, 0x82);
	write_cmos_sensor_8(0x682D, 0x6C);
	write_cmos_sensor_8(0x6835, 0x84);
	write_cmos_sensor_8(0x6836, 0xB8);
	write_cmos_sensor_8(0x6837, 0xC4);
	write_cmos_sensor_8(0x6838, 0x5C);
	write_cmos_sensor_8(0x6839, 0x63);
	write_cmos_sensor_8(0x683A, 0xAE);
	write_cmos_sensor_8(0x683B, 0x51);
	write_cmos_sensor_8(0x683C, 0x17);
	write_cmos_sensor_8(0x683E, 0x4B);
	write_cmos_sensor_8(0x683F, 0x0C);
	write_cmos_sensor_8(0x6840, 0xA5);
	write_cmos_sensor_8(0x6841, 0x86);
	write_cmos_sensor_8(0x6842, 0x52);
	write_cmos_sensor_8(0x6843, 0xC3);
	write_cmos_sensor_8(0x6844, 0x21);
	write_cmos_sensor_8(0x6845, 0x71);
	write_cmos_sensor_8(0x6847, 0xB8);
	write_cmos_sensor_8(0x6849, 0x5C);
	write_cmos_sensor_8(0x684A, 0x63);
	write_cmos_sensor_8(0x684B, 0xAE);
	write_cmos_sensor_8(0x684C, 0x51);
	write_cmos_sensor_8(0x684D, 0x97);
	write_cmos_sensor_8(0x684F, 0x4B);
	write_cmos_sensor_8(0x6851, 0xA5);
	write_cmos_sensor_8(0x6852, 0xC6);
	write_cmos_sensor_8(0x6853, 0x42);
	write_cmos_sensor_8(0x6854, 0xE3);
	write_cmos_sensor_8(0x6856, 0x71);
	write_cmos_sensor_8(0x6857, 0x84);
	write_cmos_sensor_8(0x6858, 0xB0);
	write_cmos_sensor_8(0x6859, 0xC4);
	write_cmos_sensor_8(0x685A, 0x58);
	write_cmos_sensor_8(0x685B, 0x63);
	write_cmos_sensor_8(0x685C, 0xAE);
	write_cmos_sensor_8(0x685D, 0x51);
	write_cmos_sensor_8(0x685E, 0x17);
	write_cmos_sensor_8(0x685F, 0x18);
	write_cmos_sensor_8(0x6860, 0x0B);
	write_cmos_sensor_8(0x6861, 0x0C);
	write_cmos_sensor_8(0x6862, 0x45);
	write_cmos_sensor_8(0x6863, 0x86);
	write_cmos_sensor_8(0x6864, 0x32);
	write_cmos_sensor_8(0x6865, 0xC3);
	write_cmos_sensor_8(0x6866, 0x21);
	write_cmos_sensor_8(0x6867, 0x71);
	write_cmos_sensor_8(0x6868, 0x84);
	write_cmos_sensor_8(0x6869, 0xA8);
	write_cmos_sensor_8(0x686A, 0xC4);
	write_cmos_sensor_8(0x686B, 0x54);
	write_cmos_sensor_8(0x686C, 0x63);
	write_cmos_sensor_8(0x686D, 0xAC);
	write_cmos_sensor_8(0x686E, 0x51);
	write_cmos_sensor_8(0x686F, 0x17);
	write_cmos_sensor_8(0x6870, 0x18);
	write_cmos_sensor_8(0x6871, 0x0A);
	write_cmos_sensor_8(0x6873, 0x25);
	write_cmos_sensor_8(0x6875, 0x2A);
	write_cmos_sensor_8(0x6876, 0xA3);
	write_cmos_sensor_8(0x6877, 0x21);
	write_cmos_sensor_8(0x6878, 0x61);
	write_cmos_sensor_8(0x6879, 0xAA);
	write_cmos_sensor_8(0x687A, 0x88);
	write_cmos_sensor_8(0x687B, 0x02);
	write_cmos_sensor_8(0x687C, 0x08);
	write_cmos_sensor_8(0x687D, 0x42);
	write_cmos_sensor_8(0x687E, 0x0D);
	write_cmos_sensor_8(0x687F, 0x04);
	write_cmos_sensor_8(0x6880, 0x14);
	write_cmos_sensor_8(0x6881, 0x48);
	write_cmos_sensor_8(0x6882, 0x55);
	write_cmos_sensor_8(0x6883, 0x10);
	write_cmos_sensor_8(0x6884, 0x04);
	write_cmos_sensor_8(0x6885, 0x10);
	write_cmos_sensor_8(0x6886, 0x84);
	write_cmos_sensor_8(0x6887, 0x1A);
	write_cmos_sensor_8(0x6888, 0x08);
	write_cmos_sensor_8(0x6889, 0x28);
	write_cmos_sensor_8(0x688A, 0x90);
	write_cmos_sensor_8(0x688B, 0xAA);
	write_cmos_sensor_8(0x688C, 0x1E);
	write_cmos_sensor_8(0x688D, 0x08);
	write_cmos_sensor_8(0x688E, 0x20);
	write_cmos_sensor_8(0x688F, 0x08);
	write_cmos_sensor_8(0x6890, 0x21);
	write_cmos_sensor_8(0x6891, 0x08);
	write_cmos_sensor_8(0x6892, 0x34);
	write_cmos_sensor_8(0x6893, 0x11);
	write_cmos_sensor_8(0x6894, 0x54);
	write_cmos_sensor_8(0x6895, 0x30);
	write_cmos_sensor_8(0x6897, 0x32);
	write_cmos_sensor_8(0x6899, 0x52);
	write_cmos_sensor_8(0x689B, 0x82);
	write_cmos_sensor_8(0x689C, 0x02);
	write_cmos_sensor_8(0x689D, 0xA8);
	write_cmos_sensor_8(0x689E, 0x58);
	write_cmos_sensor_8(0x68A0, 0x60);
	write_cmos_sensor_8(0x68A2, 0x64);
	write_cmos_sensor_8(0x68A4, 0xA4);
	write_cmos_sensor_8(0x68A5, 0x05);
	write_cmos_sensor_8(0x68A6, 0x50);
	write_cmos_sensor_8(0x68A7, 0xC0);
	write_cmos_sensor_8(0x68A8, 0x04);
	write_cmos_sensor_8(0x68A9, 0xC3);
	write_cmos_sensor_8(0x68AA, 0x20);
	write_cmos_sensor_8(0x68AB, 0x05);
	write_cmos_sensor_8(0x68AC, 0x20);
	write_cmos_sensor_8(0x68AD, 0x08);
	write_cmos_sensor_8(0x68AE, 0x20);
	write_cmos_sensor_8(0x68AF, 0x2A);
	write_cmos_sensor_8(0x68B0, 0x86);
	write_cmos_sensor_8(0x68B2, 0x26);
	write_cmos_sensor_8(0x68B3, 0x19);
	write_cmos_sensor_8(0x68B5, 0x29);
	write_cmos_sensor_8(0x68B7, 0x41);
	write_cmos_sensor_8(0x68B8, 0x01);
	write_cmos_sensor_8(0x68B9, 0x54);
	write_cmos_sensor_8(0x68BA, 0x30);
	write_cmos_sensor_8(0x68BC, 0x32);
	write_cmos_sensor_8(0x68BE, 0x52);
	write_cmos_sensor_8(0x68C0, 0x82);
	write_cmos_sensor_8(0x68C1, 0x02);
	write_cmos_sensor_8(0x68C2, 0xA8);
	write_cmos_sensor_8(0x68C3, 0x58);
	write_cmos_sensor_8(0x68C5, 0x60);
	write_cmos_sensor_8(0x68C7, 0x64);
	write_cmos_sensor_8(0x68C9, 0xA4);
	write_cmos_sensor_8(0x68CA, 0x05);
	write_cmos_sensor_8(0x68CB, 0x51);
	write_cmos_sensor_8(0x68CD, 0x41);
	write_cmos_sensor_8(0x68CE, 0x08);
	write_cmos_sensor_8(0x68CF, 0x41);
	write_cmos_sensor_8(0x68D0, 0xA0);
	write_cmos_sensor_8(0x68D1, 0x82);
	write_cmos_sensor_8(0x68D2, 0x91);
	write_cmos_sensor_8(0x68D3, 0x0A);
	write_cmos_sensor_8(0x68D4, 0xA1);
	write_cmos_sensor_8(0x68D5, 0xD0);
	write_cmos_sensor_8(0x68D6, 0x82);
	write_cmos_sensor_8(0x68D8, 0x82);
	write_cmos_sensor_8(0x68D9, 0x10);
	write_cmos_sensor_8(0x68DA, 0x83);
	write_cmos_sensor_8(0x68DB, 0x41);
	write_cmos_sensor_8(0x68DC, 0x15);
	write_cmos_sensor_8(0x68DD, 0x44);
	write_cmos_sensor_8(0x68DE, 0x01);
	write_cmos_sensor_8(0x68DF, 0x04);
	write_cmos_sensor_8(0x68E0, 0x21);
	write_cmos_sensor_8(0x68E1, 0x06);
	write_cmos_sensor_8(0x68E2, 0x82);
	write_cmos_sensor_8(0x68E3, 0x0A);
	write_cmos_sensor_8(0x68E4, 0x44);
	write_cmos_sensor_8(0x68E5, 0x2A);
	write_cmos_sensor_8(0x68E6, 0x87);
	write_cmos_sensor_8(0x68E7, 0x42);
	write_cmos_sensor_8(0x68E8, 0x08);
	write_cmos_sensor_8(0x68E9, 0x02);
	write_cmos_sensor_8(0x68EA, 0x08);
	write_cmos_sensor_8(0x68EB, 0x42);
	write_cmos_sensor_8(0x68EC, 0x0D);
	write_cmos_sensor_8(0x68ED, 0x04);
	write_cmos_sensor_8(0x68EE, 0x55);
	write_cmos_sensor_8(0x68EF, 0x0F);
	write_cmos_sensor_8(0x68F0, 0x84);
	write_cmos_sensor_8(0x68F1, 0x10);
	write_cmos_sensor_8(0x68F2, 0x04);
	write_cmos_sensor_8(0x68F3, 0x19);
	write_cmos_sensor_8(0x68F4, 0x88);
	write_cmos_sensor_8(0x68F5, 0x28);
	write_cmos_sensor_8(0x68F6, 0x10);
	write_cmos_sensor_8(0x68F7, 0xAA);
	write_cmos_sensor_8(0x68F8, 0x1C);
	write_cmos_sensor_8(0x68F9, 0x08);
	write_cmos_sensor_8(0x68FA, 0x1F);
	write_cmos_sensor_8(0x68FB, 0x08);
	write_cmos_sensor_8(0x68FC, 0x20);
	write_cmos_sensor_8(0x68FD, 0x08);
	write_cmos_sensor_8(0x68FE, 0x33);
	write_cmos_sensor_8(0x68FF, 0x10);
	write_cmos_sensor_8(0x690C, 0x04);
	write_cmos_sensor_8(0x690D, 0x04);
	write_cmos_sensor_8(0x690F, 0x04);
	write_cmos_sensor_8(0x6910, 0x04);
	write_cmos_sensor_8(0x6911, 0x04);
	write_cmos_sensor_8(0x6912, 0x05);
	write_cmos_sensor_8(0x6930, 0xB5);
	write_cmos_sensor_8(0x6931, 0xF6);
	write_cmos_sensor_8(0x6933, 0xF3);
	write_cmos_sensor_8(0x6939, 0x5B);
	write_cmos_sensor_8(0x693A, 0x5A);
	write_cmos_sensor_8(0x693B, 0xDA);
	write_cmos_sensor_8(0x693C, 0xD6);
	write_cmos_sensor_8(0x693D, 0xD6);
	write_cmos_sensor_8(0x693E, 0xB6);
	write_cmos_sensor_8(0x693F, 0xB5);
	write_cmos_sensor_8(0x6940, 0xB5);
	write_cmos_sensor_8(0x6941, 0xAD);
	write_cmos_sensor_8(0x6942, 0xAD);
	write_cmos_sensor_8(0x6943, 0x6D);
	write_cmos_sensor_8(0x6944, 0x6B);
	write_cmos_sensor_8(0x6945, 0x6B);
	write_cmos_sensor_8(0x6946, 0x5B);
	write_cmos_sensor_8(0x6947, 0x5A);
	write_cmos_sensor_8(0x6948, 0xDA);
	write_cmos_sensor_8(0x6949, 0xD6);
	write_cmos_sensor_8(0x694A, 0xD6);
	write_cmos_sensor_8(0x694B, 0xB6);
	write_cmos_sensor_8(0x694C, 0xB5);
	write_cmos_sensor_8(0x694D, 0xB5);
	write_cmos_sensor_8(0x694E, 0xAD);
	write_cmos_sensor_8(0x694F, 0xAD);
	write_cmos_sensor_8(0x6950, 0x6C);
	write_cmos_sensor_8(0x6B0B, 0x02);
	write_cmos_sensor_8(0x6B0C, 0x01);
	write_cmos_sensor_8(0x6B0D, 0x05);
	write_cmos_sensor_8(0x6B0F, 0x04);
	write_cmos_sensor_8(0x6B10, 0x02);
	write_cmos_sensor_8(0x6B11, 0x06);
	write_cmos_sensor_8(0x6B12, 0x03);
	write_cmos_sensor_8(0x6B13, 0x07);
	write_cmos_sensor_8(0x6B14, 0x0D);
	write_cmos_sensor_8(0x6B15, 0x09);
	write_cmos_sensor_8(0x6B16, 0x0C);
	write_cmos_sensor_8(0x6B17, 0x08);
	write_cmos_sensor_8(0x6B18, 0x0E);
	write_cmos_sensor_8(0x6B19, 0x0A);
	write_cmos_sensor_8(0x6B1A, 0x0F);
	write_cmos_sensor_8(0x6B1B, 0x0B);
	write_cmos_sensor_8(0x6B1C, 0x01);
	write_cmos_sensor_8(0x6B1D, 0x05);
	write_cmos_sensor_8(0x6B1F, 0x04);
	write_cmos_sensor_8(0x6B20, 0x02);
	write_cmos_sensor_8(0x6B21, 0x06);
	write_cmos_sensor_8(0x6B22, 0x03);
	write_cmos_sensor_8(0x6B23, 0x07);
	write_cmos_sensor_8(0x6B24, 0x0D);
	write_cmos_sensor_8(0x6B25, 0x09);
	write_cmos_sensor_8(0x6B26, 0x0C);
	write_cmos_sensor_8(0x6B27, 0x08);
	write_cmos_sensor_8(0x6B28, 0x0E);
	write_cmos_sensor_8(0x6B29, 0x0A);
	write_cmos_sensor_8(0x6B2A, 0x0F);
	write_cmos_sensor_8(0x6B2B, 0x0B);
	write_cmos_sensor_8(0x7948, 0x01);
	write_cmos_sensor_8(0x7949, 0x0F);
	write_cmos_sensor_8(0x794A, 0x0F);
	write_cmos_sensor_8(0x794B, 0x14);
	write_cmos_sensor_8(0x794C, 0x14);
	write_cmos_sensor_8(0x794D, 0x3A);
	write_cmos_sensor_8(0x7951, 0x00);
	write_cmos_sensor_8(0x7952, 0x01);
	write_cmos_sensor_8(0x7955, 0x00);
	write_cmos_sensor_8(0x9004, 0x05);
	write_cmos_sensor_8(0x9200, 0xA0);
	write_cmos_sensor_8(0x9201, 0xA7);
	write_cmos_sensor_8(0x9202, 0xA0);
	write_cmos_sensor_8(0x9203, 0xAA);
	write_cmos_sensor_8(0x9204, 0xA0);
	write_cmos_sensor_8(0x9205, 0xAD);
	write_cmos_sensor_8(0x9206, 0xA0);
	write_cmos_sensor_8(0x9207, 0xB0);
	write_cmos_sensor_8(0x9208, 0xA0);
	write_cmos_sensor_8(0x9209, 0xB3);
	write_cmos_sensor_8(0x9385, 0x4B);
	write_cmos_sensor_8(0x9387, 0x46);
	write_cmos_sensor_8(0x9389, 0x1E);
	write_cmos_sensor_8(0x938B, 0x1E);
	write_cmos_sensor_8(0x938D, 0x4B);
	write_cmos_sensor_8(0x938F, 0x46);
	write_cmos_sensor_8(0x9391, 0x1E);
	write_cmos_sensor_8(0x9393, 0x1E);
	write_cmos_sensor_8(0x9395, 0x50);
	write_cmos_sensor_8(0x9397, 0x4B);
	write_cmos_sensor_8(0x9399, 0x46);
	write_cmos_sensor_8(0x939B, 0x1E);
	write_cmos_sensor_8(0x939D, 0x4B);
	write_cmos_sensor_8(0x939F, 0x46);
	write_cmos_sensor_8(0x93A1, 0x1E);
	write_cmos_sensor_8(0x93A3, 0x1E);
	write_cmos_sensor_8(0x93A5, 0x50);
	write_cmos_sensor_8(0x93A7, 0x4B);
	write_cmos_sensor_8(0x93A9, 0x41);
	write_cmos_sensor_8(0x93AB, 0x1E);
	write_cmos_sensor_8(0x93AD, 0x4B);
	write_cmos_sensor_8(0x93AF, 0x46);
	write_cmos_sensor_8(0x93B1, 0x1E);
	write_cmos_sensor_8(0x93B3, 0x1E);
	write_cmos_sensor_8(0x93B5, 0x4B);
	write_cmos_sensor_8(0x93B7, 0x46);
	write_cmos_sensor_8(0x93B9, 0x1E);
	write_cmos_sensor_8(0x93BB, 0x1E);
	write_cmos_sensor_8(0x93BD, 0x4B);
	write_cmos_sensor_8(0x93BF, 0x46);
	write_cmos_sensor_8(0x93C1, 0x1E);
	write_cmos_sensor_8(0x93C3, 0x1E);
	write_cmos_sensor_8(0x93C5, 0x50);
	write_cmos_sensor_8(0x93C7, 0x4B);
	write_cmos_sensor_8(0x93C9, 0x46);
	write_cmos_sensor_8(0x93CB, 0x1E);
	write_cmos_sensor_8(0x93CD, 0x4B);
	write_cmos_sensor_8(0x93CF, 0x46);
	write_cmos_sensor_8(0x93D1, 0x1E);
	write_cmos_sensor_8(0x93D3, 0x1E);
	write_cmos_sensor_8(0x93D5, 0x50);
	write_cmos_sensor_8(0x93D7, 0x4B);
	write_cmos_sensor_8(0x93D9, 0x46);
	write_cmos_sensor_8(0x93DB, 0x1E);
	write_cmos_sensor_8(0x93DD, 0x4B);
	write_cmos_sensor_8(0x93DF, 0x46);
	write_cmos_sensor_8(0x93E1, 0x1E);
	write_cmos_sensor_8(0x93E3, 0x1E);
	write_cmos_sensor_8(0x93E5, 0x50);
	write_cmos_sensor_8(0x93E7, 0x4B);
	write_cmos_sensor_8(0x93E9, 0x46);
	write_cmos_sensor_8(0x93EB, 0x1E);
	write_cmos_sensor_8(0x93ED, 0x55);
	write_cmos_sensor_8(0x93EF, 0x50);
	write_cmos_sensor_8(0x93F1, 0x1E);
	write_cmos_sensor_8(0x93F3, 0x1E);
	write_cmos_sensor_8(0x93F5, 0x5A);
	write_cmos_sensor_8(0x93F7, 0x55);
	write_cmos_sensor_8(0x93F9, 0x50);
	write_cmos_sensor_8(0x93FB, 0x1E);
	write_cmos_sensor_8(0x99AF, 0x0F);
	write_cmos_sensor_8(0x99B0, 0x0F);
	write_cmos_sensor_8(0x99B1, 0x0F);
	write_cmos_sensor_8(0x99B2, 0x0F);
	write_cmos_sensor_8(0x99B3, 0x0F);
	write_cmos_sensor_8(0x99E1, 0x0F);
	write_cmos_sensor_8(0x99E2, 0x0F);
	write_cmos_sensor_8(0x99E3, 0x0F);
	write_cmos_sensor_8(0x99E4, 0x0F);
	write_cmos_sensor_8(0x99E5, 0x0F);
	write_cmos_sensor_8(0x99E6, 0x0F);
	write_cmos_sensor_8(0x99E7, 0x0F);
	write_cmos_sensor_8(0x99E8, 0x0F);
	write_cmos_sensor_8(0x99E9, 0x0F);
	write_cmos_sensor_8(0x99EA, 0x0F);
	write_cmos_sensor_8(0xCD32, 0x02);
	write_cmos_sensor_8(0xE0CE, 0x03);
	write_cmos_sensor_8(0xE0D1, 0x0B);
	write_cmos_sensor_8(0xE0D2, 0x0B);
	write_cmos_sensor_8(0xE286, 0x31);
	write_cmos_sensor_8(0xE2A6, 0x32);
	write_cmos_sensor_8(0xE2C6, 0x33);
}

static void preview_setting(void)
{
	/*C	HDR 3-Exp HDR 4:3, 30FPS*/
	LOG_INF(
	    "E ver4_02_HDR. 0xBCF1:0x%x, DATA type 0xe286:0x%x, 0xe2A6:0x%x, 0xe2C6:0x%x\n",
	    read_cmos_sensor_8(0xBCF1),
	    read_cmos_sensor_8(0xe286),
	    read_cmos_sensor_8(0xe2A6),
	    read_cmos_sensor_8(0xe2C6));

	write_cmos_sensor_8(0x0B00, 0x01); /*lsc enable*/

	/*enable STATS data*/
	write_cmos_sensor_8(0x323A, 0x01);
	write_cmos_sensor_8(0x323B, 0x01);
	write_cmos_sensor_8(0x323C, 0x01);

	write_cmos_sensor_8(0x7343, 0x00);

	/* AEHIST_AREA size */
	write_cmos_sensor_8(0x37E4, 0x0B);
	write_cmos_sensor_8(0x37E5, 0x40);
	write_cmos_sensor_8(0x37E6, 0x08);
	write_cmos_sensor_8(0x37E7, 0x6C);
	LOG_INF("AEHIST_AREA size: 0x%x %x, 0x%x %x\n",
			read_cmos_sensor_8(0x37E4), read_cmos_sensor_8(0x37E5),
			read_cmos_sensor_8(0x37E6), read_cmos_sensor_8(0x37E7));

	/* AEHIST_LINER_AUTO_THRESH 0:manual 1:auto */
	write_cmos_sensor_8(0xB734, 0x01);
	/* AEHIST_LOG_AUTO_THRESH 0:manual 1:auto */
	write_cmos_sensor_8(0xB73A, 0x00);
	write_cmos_sensor_8(0x37EC, 0x00); /*AEHIST_LOG_LOWER_TH*/
	write_cmos_sensor_8(0x37ED, 0x00); /*AEHIST_LOG_LOWER_TH*/
	write_cmos_sensor_8(0x37EE, 0x03); /*AEHIST_LOG_UPPER_TH*/
	write_cmos_sensor_8(0x37EF, 0xFF); /*AEHIST_LOG_UPPER_TH*/
	LOG_INF(
	    "AEHIST_LOG 0xB73A:0x%x 0x37EC:0x%x 0x37ED:0x%x 0x37EE:0x%x 0x37EF:0x%x\n",
	    read_cmos_sensor_8(0xB73A),
	    read_cmos_sensor_8(0x37EC),
	    read_cmos_sensor_8(0x37ED),
	    read_cmos_sensor_8(0x37EE),
	    read_cmos_sensor_8(0x37EF));

	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x0114, 0x03);
	write_cmos_sensor_8(0x0342, 0x18);
	write_cmos_sensor_8(0x0343, 0x00);
	write_cmos_sensor_8(0x0340, 0x11);
	write_cmos_sensor_8(0x0341, 0x65);
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x00);
	write_cmos_sensor_8(0x0346, 0x00);
	write_cmos_sensor_8(0x0347, 0x00);
	write_cmos_sensor_8(0x0348, 0x16);
	write_cmos_sensor_8(0x0349, 0x7F);
	write_cmos_sensor_8(0x034A, 0x10);
	write_cmos_sensor_8(0x034B, 0xD7);
	write_cmos_sensor_8(0x0220, 0x63);
	write_cmos_sensor_8(0x0900, 0x00);
	write_cmos_sensor_8(0x0901, 0x11);
	write_cmos_sensor_8(0x0902, 0x0A);
	write_cmos_sensor_8(0x3140, 0x04);
	write_cmos_sensor_8(0x3246, 0x01);
	write_cmos_sensor_8(0x3247, 0x01);
	write_cmos_sensor_8(0x0401, 0x00);
	write_cmos_sensor_8(0x0404, 0x00);
	write_cmos_sensor_8(0x0405, 0x10);
	write_cmos_sensor_8(0x0408, 0x00);
	write_cmos_sensor_8(0x0409, 0x00);
	write_cmos_sensor_8(0x040A, 0x00);
	write_cmos_sensor_8(0x040B, 0x00);
	write_cmos_sensor_8(0x040C, 0x0B);
	write_cmos_sensor_8(0x040D, 0x40);
	write_cmos_sensor_8(0x040E, 0x08);
	write_cmos_sensor_8(0x040F, 0x6C);
	write_cmos_sensor_8(0x034C, 0x0B);
	write_cmos_sensor_8(0x034D, 0x40);
	write_cmos_sensor_8(0x034E, 0x08);
	write_cmos_sensor_8(0x034F, 0x6C);
	write_cmos_sensor_8(0x0301, 0x05);
	write_cmos_sensor_8(0x0303, 0x02);
	write_cmos_sensor_8(0x0305, 0x04);
	write_cmos_sensor_8(0x0306, 0x01);
	write_cmos_sensor_8(0x0307, 0x56);
	write_cmos_sensor_8(0x030B, 0x02);
	write_cmos_sensor_8(0x030D, 0x0C);
	write_cmos_sensor_8(0x030E, 0x04);
	write_cmos_sensor_8(0x030F, 0x57);
	write_cmos_sensor_8(0x0310, 0x01);
	write_cmos_sensor_8(0x0B06, 0x00);
	write_cmos_sensor_8(0x3620, 0x00);
	write_cmos_sensor_8(0x3F0C, 0x00);
	write_cmos_sensor_8(0x3F14, 0x01);
	write_cmos_sensor_8(0x0202, 0x07);
	write_cmos_sensor_8(0x0203, 0xD0);
	write_cmos_sensor_8(0x0224, 0x01);
	write_cmos_sensor_8(0x0225, 0xF4);
	write_cmos_sensor_8(0x3FE0, 0x03);
	write_cmos_sensor_8(0x3FE1, 0xE8);
	write_cmos_sensor_8(0x0204, 0x00);
	write_cmos_sensor_8(0x0205, 0x00);
	write_cmos_sensor_8(0x0216, 0x00);
	write_cmos_sensor_8(0x0217, 0x00);
	write_cmos_sensor_8(0x0218, 0x01);
	write_cmos_sensor_8(0x0219, 0x00);
	write_cmos_sensor_8(0x020E, 0x01);
	write_cmos_sensor_8(0x020F, 0x00);
	write_cmos_sensor_8(0x3FE2, 0x00);
	write_cmos_sensor_8(0x3FE3, 0x00);
	write_cmos_sensor_8(0x3FE4, 0x01);
	write_cmos_sensor_8(0x3FE5, 0x00);

	LOG_INF("L ver4_02_HDR\n");
}

static void capture_setting(void)
{
/*
 * reg_A-1
 * 5760x4312 30FPS
 * H: 5760
 * V: 4312
 */
write_cmos_sensor_8(0x0112, 0x0A);
write_cmos_sensor_8(0x0113, 0x0A);
write_cmos_sensor_8(0x0114, 0x03);
write_cmos_sensor_8(0x0342, 0x18);
write_cmos_sensor_8(0x0343, 0x00);
write_cmos_sensor_8(0x0340, 0x11);
write_cmos_sensor_8(0x0341, 0x4B);
write_cmos_sensor_8(0x0344, 0x00);
write_cmos_sensor_8(0x0345, 0x00);
write_cmos_sensor_8(0x0346, 0x00);
write_cmos_sensor_8(0x0347, 0x00);
write_cmos_sensor_8(0x0348, 0x16);
write_cmos_sensor_8(0x0349, 0x7F);
write_cmos_sensor_8(0x034A, 0x10);
write_cmos_sensor_8(0x034B, 0xD7);
write_cmos_sensor_8(0x0220, 0x00);
write_cmos_sensor_8(0x0900, 0x00);
write_cmos_sensor_8(0x0901, 0x11);
write_cmos_sensor_8(0x0902, 0x0A);
write_cmos_sensor_8(0x3140, 0x00);
write_cmos_sensor_8(0x3246, 0x01);
write_cmos_sensor_8(0x3247, 0x01);
write_cmos_sensor_8(0x0401, 0x00);
write_cmos_sensor_8(0x0404, 0x00);
write_cmos_sensor_8(0x0405, 0x10);
write_cmos_sensor_8(0x0408, 0x00);
write_cmos_sensor_8(0x0409, 0x00);
write_cmos_sensor_8(0x040A, 0x00);
write_cmos_sensor_8(0x040B, 0x00);
write_cmos_sensor_8(0x040C, 0x16);
write_cmos_sensor_8(0x040D, 0x80);
write_cmos_sensor_8(0x040E, 0x10);
write_cmos_sensor_8(0x040F, 0xD8);
write_cmos_sensor_8(0x034C, 0x16);
write_cmos_sensor_8(0x034D, 0x80);
write_cmos_sensor_8(0x034E, 0x10);
write_cmos_sensor_8(0x034F, 0xD8);
write_cmos_sensor_8(0x0301, 0x05);
write_cmos_sensor_8(0x0303, 0x02);
write_cmos_sensor_8(0x0305, 0x04);
write_cmos_sensor_8(0x0306, 0x01);
write_cmos_sensor_8(0x0307, 0x54);
write_cmos_sensor_8(0x030B, 0x01);
write_cmos_sensor_8(0x030D, 0x04);
write_cmos_sensor_8(0x030E, 0x01);
write_cmos_sensor_8(0x030F, 0x53);
write_cmos_sensor_8(0x0310, 0x01);
write_cmos_sensor_8(0x0B06, 0x01);
write_cmos_sensor_8(0x3620, 0x01);
write_cmos_sensor_8(0x3F0C, 0x00);
write_cmos_sensor_8(0x3F14, 0x01);
write_cmos_sensor_8(0x0202, 0x07);
write_cmos_sensor_8(0x0203, 0xD0);
write_cmos_sensor_8(0x0224, 0x01);
write_cmos_sensor_8(0x0225, 0xF4);
write_cmos_sensor_8(0x3FE0, 0x03);
write_cmos_sensor_8(0x3FE1, 0xE8);
write_cmos_sensor_8(0x0204, 0x00);
write_cmos_sensor_8(0x0205, 0x00);
write_cmos_sensor_8(0x0216, 0x00);
write_cmos_sensor_8(0x0217, 0x00);
write_cmos_sensor_8(0x0218, 0x01);
write_cmos_sensor_8(0x0219, 0x00);
write_cmos_sensor_8(0x020E, 0x01);
write_cmos_sensor_8(0x020F, 0x00);
write_cmos_sensor_8(0x3FE2, 0x00);
write_cmos_sensor_8(0x3FE3, 0x00);
write_cmos_sensor_8(0x3FE4, 0x01);
write_cmos_sensor_8(0x3FE5, 0x00);
write_cmos_sensor_8(0x323A, 0x00);
write_cmos_sensor_8(0x323B, 0x00);
write_cmos_sensor_8(0x323C, 0x00);
}

static void normal_video_setting(void)
{
/*ver3 reg_C
 *	2x2 Binning 30FPS
 *	H: 2880
 *	V: 2156
 */
	LOG_INF("E\n");
	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x0114, 0x03);
	write_cmos_sensor_8(0x0342, 0x0C);
	write_cmos_sensor_8(0x0343, 0x58);
	write_cmos_sensor_8(0x0340, 0x08);
	write_cmos_sensor_8(0x0341, 0xB3);
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x00);
	write_cmos_sensor_8(0x0346, 0x00);
	write_cmos_sensor_8(0x0347, 0x00);
	write_cmos_sensor_8(0x0348, 0x16);
	write_cmos_sensor_8(0x0349, 0x7F);
	write_cmos_sensor_8(0x034A, 0x10);
	write_cmos_sensor_8(0x034B, 0xD7);
	write_cmos_sensor_8(0x0220, 0x00);
	write_cmos_sensor_8(0x0900, 0x01);
	write_cmos_sensor_8(0x0901, 0x22);
	write_cmos_sensor_8(0x0902, 0x08);
	write_cmos_sensor_8(0x3140, 0x00);
	write_cmos_sensor_8(0x3246, 0x81);
	write_cmos_sensor_8(0x3247, 0x81);
	write_cmos_sensor_8(0x0401, 0x00);
	write_cmos_sensor_8(0x0404, 0x00);
	write_cmos_sensor_8(0x0405, 0x10);
	write_cmos_sensor_8(0x0408, 0x00);
	write_cmos_sensor_8(0x0409, 0x00);
	write_cmos_sensor_8(0x040A, 0x00);
	write_cmos_sensor_8(0x040B, 0x00);
	write_cmos_sensor_8(0x040C, 0x0B);
	write_cmos_sensor_8(0x040D, 0x40);
	write_cmos_sensor_8(0x040E, 0x08);
	write_cmos_sensor_8(0x040F, 0x6C);
	write_cmos_sensor_8(0x034C, 0x0B);
	write_cmos_sensor_8(0x034D, 0x40);
	write_cmos_sensor_8(0x034E, 0x08);
	write_cmos_sensor_8(0x034F, 0x6C);
	write_cmos_sensor_8(0x0301, 0x05);
	write_cmos_sensor_8(0x0303, 0x04);
	write_cmos_sensor_8(0x0305, 0x04);
	write_cmos_sensor_8(0x0306, 0x00);
	write_cmos_sensor_8(0x0307, 0xB0);
	write_cmos_sensor_8(0x030B, 0x04);
	write_cmos_sensor_8(0x030D, 0x04);
	write_cmos_sensor_8(0x030E, 0x01);
	write_cmos_sensor_8(0x030F, 0x59);
	write_cmos_sensor_8(0x0310, 0x01);
	write_cmos_sensor_8(0x0B06, 0x01);
	write_cmos_sensor_8(0x3620, 0x00);
	write_cmos_sensor_8(0x3F0C, 0x00);
	write_cmos_sensor_8(0x3F14, 0x01);
	write_cmos_sensor_8(0x0202, 0x07);
	write_cmos_sensor_8(0x0203, 0xD0);
	write_cmos_sensor_8(0x0224, 0x01);
	write_cmos_sensor_8(0x0225, 0xF4);
	write_cmos_sensor_8(0x3FE0, 0x03);
	write_cmos_sensor_8(0x3FE1, 0xE8);
	write_cmos_sensor_8(0x0204, 0x00);
	write_cmos_sensor_8(0x0205, 0x00);
	write_cmos_sensor_8(0x0216, 0x00);
	write_cmos_sensor_8(0x0217, 0x00);
	write_cmos_sensor_8(0x0218, 0x01);
	write_cmos_sensor_8(0x0219, 0x00);
	write_cmos_sensor_8(0x020E, 0x01);
	write_cmos_sensor_8(0x020F, 0x00);
	write_cmos_sensor_8(0x3FE2, 0x00);
	write_cmos_sensor_8(0x3FE3, 0x00);
	write_cmos_sensor_8(0x3FE4, 0x01);
	write_cmos_sensor_8(0x3FE5, 0x00);
	write_cmos_sensor_8(0x323A, 0x00);
	write_cmos_sensor_8(0x323B, 0x00);
	write_cmos_sensor_8(0x323C, 0x00);
	LOG_INF("L\n");

}

static void hs_video_setting(void)
{
	LOG_INF("E\n");
	preview_setting();  /* Tower modify 20160214 */
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");
	preview_setting();
}

static kal_uint16 read_sensor_version(void)
{
	UINT32 ver = 0;

	LOG_INF("0x0018:0x%x, 0x9001:0x%x\n",
		read_cmos_sensor_8(0x0018),
		read_cmos_sensor_8(0x9001));

	if (read_cmos_sensor_8(0x0018) == 0x1) {
		if (read_cmos_sensor_8(0x9001) == 0x14)
			ver = 2; /*Cut0.91 ES2,0x0018=0x01,0x9001=0x14*/
		else
			ver = 1; /*Cut0.91 ES1,0x0018=0x01,0x9001=0x12*/
	}
	return ver;
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

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = ((read_cmos_sensor_8(0x0016) << 8) |
						read_cmos_sensor_8(0x0017));
			LOG_INF(
			    "read_0x0016=0x%x, 0x0017=0x%x,0x0016_0017=0x%x\n",
			    read_cmos_sensor_8(0x0016),
			    read_cmos_sensor_8(0x0017),
			    read_cmos_sensor(0x0016));
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);

				/* zhaozhengtao 2016/02/19,
				 * modify for different module
				 */
				imgsensor_info.module_id = read_module_id();
				LOG_INF("IMX576_module_id=%d\n",
						imgsensor_info.module_id);

				imgsensor_info.sensor_ver =
							read_sensor_version();
				LOG_INF("IMX576_sensor_verion=%d\n",
					imgsensor_info.sensor_ver);
				return ERROR_NONE;
			}
			LOG_INF(
			    "Read sensor id fail, i2c_write_id: 0x%x  sensor_id: 0x%x\n",
			    imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/* if Sensor ID is not correct, Must set *sensor_id
		 * to 0xFFFFFFFF
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

	LOG_INF(
	    "PLATFORM:MT6595,MIPI 2LANE\n");
	LOG_INF(
	    "preview 1280*960@30fps,864Mbps/lane; video 1280*960@30fps,864Mbps/lane;\n");
	LOG_INF(
	    "capture 5M@30fps,864Mbps/lane\n");

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = ((read_cmos_sensor_8(0x0016) << 8) |
						read_cmos_sensor_8(0x0017));
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF(
				    "i2c write id: 0x%x, sensor id: 0x%x\n",
				    imgsensor.i2c_write_id,
				    sensor_id);
			break;
		}
			LOG_INF(
			    "Read sensor id fail, id: 0x%x sensor_id=0x%x\n ",
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
	LOG_INF("E\n");
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
 *  *image_window : address pointer of pixel numbers in one period of HSYNC
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
	LOG_INF("E imgsensor.hdr_mode=%d\n", imgsensor.hdr_mode);
if (imgsensor.hdr_mode) {
	LOG_INF("E HDR\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
} else {
	LOG_INF("E normal\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting();
}
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
	LOG_INF("E\n");

		spin_lock(&imgsensor_drv_lock);
		imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
		spin_unlock(&imgsensor_drv_lock);

		capture_setting();

	LOG_INF("BK_ imgsensor.hdr_mode=%d\n", imgsensor.hdr_mode);
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}	/* capture() */
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
	/* preview_setting(); */
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	normal_video   */

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
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(imgsensor.mirror);
	capture_setting();

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	slim_video	 */



static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
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


	sensor_resolution->SensorHighSpeedVideoWidth	 =
			imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight	 =
			imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth	 =
			imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
			imgsensor_info.slim_video.grabwindow_height;
	/* zhaozhengtao add 20160215 */
	sensor_resolution->SensorCustom1Width  =
			imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorCustom1Height  =
			imgsensor_info.cap.grabwindow_height;
	sensor_resolution->SensorCustom2Width   =
			imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorCustom2Height  =
			imgsensor_info.cap.grabwindow_height;
	sensor_resolution->SensorCustom3Width   =
			imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorCustom3Height  =
			imgsensor_info.cap.grabwindow_height;
	sensor_resolution->SensorCustom4Width   =
			imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorCustom4Height  =
			imgsensor_info.cap.grabwindow_height;
	sensor_resolution->SensorCustom5Width   =
			imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorCustom5Height  =
			imgsensor_info.cap.grabwindow_height;

	return ERROR_NONE;
}	/*	get_resolution	*/

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MSDK_SENSOR_INFO_STRUCT *sensor_info,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* not use */
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* inverse with datasheet */
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

	/*0: NO HDR, 1: iHDR, 2:mvHDR, 3:zHDR, 4:four-cell mVHDR*/
	sensor_info->HDR_Support = 4;

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
				imgsensor_info.normal_video
				.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			sensor_info->SensorGrabStartX =
				imgsensor_info.hs_video.startx;
			sensor_info->SensorGrabStartY =
				imgsensor_info.hs_video.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.hs_video
				.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			sensor_info->SensorGrabStartX =
				imgsensor_info.slim_video.startx;
			sensor_info->SensorGrabStartY =
				imgsensor_info.slim_video.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.slim_video
				.mipi_data_lp2hs_settle_dc;

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
}	/*	get_info  */


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
}	/* control() */



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
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;
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
		  (frame_length > imgsensor_info.normal_video.framelength) ?
		  (frame_length - imgsensor_info.normal_video.framelength) : 0;

		imgsensor.frame_length =
				imgsensor_info.normal_video.framelength +
				imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps ==
		    imgsensor_info.cap1.max_framerate) {
			frame_length = imgsensor_info.cap1.pclk /
				framerate * 10 / imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			    (frame_length > imgsensor_info.cap1.framelength) ?
			    (frame_length - imgsensor_info.cap1.framelength) :
			    0;
			imgsensor.frame_length =
			    imgsensor_info.cap1.framelength +
			    imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else if (imgsensor.current_fps ==
		    imgsensor_info.cap2.max_framerate) {
			frame_length = imgsensor_info.cap2.pclk /
				framerate * 10 / imgsensor_info.cap2.linelength;

			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			    (frame_length > imgsensor_info.cap2.framelength) ?
			    (frame_length - imgsensor_info.cap2.framelength) :
			    0;

			imgsensor.frame_length =
				imgsensor_info.cap2.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps !=
			    imgsensor_info.cap.max_framerate)
				LOG_INF(
				    "Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				    framerate,
				    imgsensor_info.cap.max_framerate/10);

			frame_length = imgsensor_info.cap.pclk /
				framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			    (frame_length > imgsensor_info.cap.framelength) ?
			    (frame_length - imgsensor_info.cap.framelength) :
			     0;
			imgsensor.frame_length =
			    imgsensor_info.cap.framelength +
			    imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10 /
			imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.hs_video.framelength) ?
			(frame_length - imgsensor_info.hs_video.framelength) :
			0;
		imgsensor.frame_length =
			imgsensor_info.hs_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10 /
			imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.slim_video.framelength) ?
			(frame_length - imgsensor_info.slim_video.framelength) :
			0;
		imgsensor.frame_length =
			imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	default:  /* coding with  preview scenario by default */
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
		set_dummy();
		LOG_INF("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
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

	if (enable) {
		/* 0x5E00[8]: 1 enable,  0 disable */
		/* 0x5E00[1:0]; 00 Color bar, 01 Random Data,
		 * 10 Square, 11 BLACK
		 */
		write_cmos_sensor(0x0601, 0x0002);
	} else {
		/* 0x5E00[8]: 1 enable,  0 disable */
		/* 0x5E00[1:0]; 00 Color bar, 01 Random Data,
		 * 10 Square, 11 BLACK
		 */
		write_cmos_sensor(0x0601, 0x0000);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static void hdr_write_tri_shutter(kal_uint16 le, kal_uint16 me, kal_uint16 se)
{
	kal_uint16 realtime_fps = 0;

	LOG_INF("E! le:0x%x, me:0x%x, se:0x%x\n", le, me, se);
	spin_lock(&imgsensor_drv_lock);
	if (le > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = le + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	if (le < imgsensor_info.min_shutter)
		le = imgsensor_info.min_shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk /
			imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			write_cmos_sensor_8(0x0104, 0x01);
			/*FRM_LENGTH_LINES[15:8]*/
			write_cmos_sensor_8(0x0340,
				imgsensor.frame_length >> 8);
			/*FRM_LENGTH_LINES[7:0]*/
			write_cmos_sensor_8(0x0341,
				imgsensor.frame_length & 0xFF);
			write_cmos_sensor_8(0x0104, 0x00);
		}
	} else {
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
	}

	write_cmos_sensor_8(0x0104, 0x01);
	/* Long exposure */
	write_cmos_sensor_8(0x0202, (le >> 8) & 0xFF);
	write_cmos_sensor_8(0x0203, le & 0xFF);
	/* Muddle exposure */ /*MID_COARSE_INTEG_TIME[15:8]*/
	write_cmos_sensor_8(0x3FE0, (me >> 8) & 0xFF);
	write_cmos_sensor_8(0x3FE1, me & 0xFF); /*MID_COARSE_INTEG_TIME[7:0]*/
	/* Short exposure */
	write_cmos_sensor_8(0x0224, (se >> 8) & 0xFF);
	write_cmos_sensor_8(0x0225, se & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);

	LOG_INF("L! le:0x%x, me:0x%x, se:0x%x\n", le, me, se);

}

static void hdr_write_tri_gain(kal_uint16 lgain, kal_uint16 mg, kal_uint16 sg)
{
	kal_uint16 reg_lg, reg_mg, reg_sg;

	if (lgain < BASEGAIN || lgain > 16 * BASEGAIN) {
		LOG_INF("Error gain setting");

		if (lgain < BASEGAIN)
			lgain = BASEGAIN;
		else if (lgain > 16 * BASEGAIN)
			lgain = 16 * BASEGAIN;
	}

	reg_lg = gain2reg(lgain);
	reg_mg = gain2reg(mg);
	reg_sg = gain2reg(sg);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_lg;
	spin_unlock(&imgsensor_drv_lock);
	write_cmos_sensor_8(0x0104, 0x01);
	/* Long Gian */
	write_cmos_sensor_8(0x0204, (reg_lg>>8) & 0xFF);
	write_cmos_sensor_8(0x0205, reg_lg & 0xFF);
	/* Middle Gian */
	write_cmos_sensor_8(0x3FE2, (reg_mg>>8) & 0xFF);
	write_cmos_sensor_8(0x3FE3, reg_mg & 0xFF);
	/* Short Gian */
	write_cmos_sensor_8(0x0216, (reg_sg>>8) & 0xFF);
	write_cmos_sensor_8(0x0217, reg_sg & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);

	LOG_INF(
	"lgain:0x%x, reg_lg:0x%x, sg:0x%x, reg_mg:0x%x, mg:0x%x, reg_sg:0x%x\n",
	lgain, reg_lg, mg, reg_mg, sg, reg_sg);

}

static void imx576_set_lsc_reg_setting(
	kal_uint8 index, kal_uint16 *regDa, MUINT32 regNum)
{
	int i;
	/*0:B,1:Gb,2:Gr,3:R*/
	int startAddr[4] = {0x9D88, 0x9CB0, 0x9BD8, 0x9B00};

#if 0
	int R_startAddr  = 0x9B00; /*0x9B00-0x9BD7*/
	int GR_startAddr = 0x9BD8; /*0x9BD8-0x9CAF*/
	int GB_startAddr = 0x9CB0; /*0x9CB0-0x9D87*/
	int B_startAddr  = 0x9D88; /*0x9D88-0x9E5F*/
#endif
	LOG_INF("E! index:%d, regNum:%d\n", index, regNum);

	write_cmos_sensor_8(0x0B00, 0x01); /*lsc enable*/
	/*define Knot point, 2'b01:u3.7*/
	write_cmos_sensor_8(0x9750, 0x01);
	write_cmos_sensor_8(0x9751, 0x01);
	write_cmos_sensor_8(0x9752, 0x01);
	write_cmos_sensor_8(0x9753, 0x01);

	for (i = 0; i < regNum; i++)
		write_cmos_sensor(startAddr[index] + 2*i, regDa[i]);

	for (i = 0; i < regNum; i++) {
		LOG_INF("l Addr:0x%x Value:0x%x %x Table:0x%x\n",
			(startAddr[index] + 2*i),
			read_cmos_sensor_8(startAddr[index] + 2*i),
			read_cmos_sensor_8(startAddr[index] + 2*i + 1),
			regDa[i]);
	}
	write_cmos_sensor_8(0x0B00, 0x00); /*lsc disable*/
}

static void set_imx576_ATR(
	kal_uint16 LimitGain, kal_uint16 LtcRate, kal_uint16 PostGain)
{
	LOG_INF("Limit Gain:0x%x LTC Rate:0x%x Post Gain:0x%x\n",
		LimitGain, LtcRate, PostGain);

	write_cmos_sensor_8(0x7F77, PostGain); /*RG_TC_VE_POST_GAIN*/

	write_cmos_sensor_8(0x3C00, LtcRate & 0xFF); /*TC_LTC_RATIO_1*/
	write_cmos_sensor_8(0x3C01, LtcRate & 0xFF); /*TC_LTC_RATIO_2*/
	write_cmos_sensor_8(0x3C02, LtcRate & 0xFF); /*TC_LTC_RATIO_3*/
	write_cmos_sensor_8(0x3C03, LtcRate & 0xFF); /*TC_LTC_RATIO_4*/
	write_cmos_sensor_8(0x3C04, LtcRate & 0xFF); /*TC_LTC_RATIO_5*/

	write_cmos_sensor_8(0xA141, LimitGain & 0xFF); /*TC_LIMIT_GAIN_1*/
	write_cmos_sensor_8(0xA140, (LimitGain >> 8) & 0xFF);
	write_cmos_sensor_8(0xA147, LimitGain & 0xFF); /*TC_LIMIT_GAIN_2*/
	write_cmos_sensor_8(0xA146, (LimitGain >> 8) & 0xFF);
	write_cmos_sensor_8(0xA14D, LimitGain & 0xFF); /*TC_LIMIT_GAIN_3*/
	write_cmos_sensor_8(0xA14C, (LimitGain >> 8) & 0xFF);
	write_cmos_sensor_8(0xA153, LimitGain & 0xFF); /*TC_LIMIT_GAIN_4*/
	write_cmos_sensor_8(0xA152, (LimitGain >> 8) & 0xFF);
	write_cmos_sensor_8(0xA159, LimitGain & 0xFF); /*TC_LIMIT_GAIN_5*/
	write_cmos_sensor_8(0xA158, (LimitGain >> 8) & 0xFF);

}

static kal_uint32 imx576_awb_gain(struct SET_SENSOR_AWB_GAIN *pSetSensorAWB)
{
	UINT32 rgain_32, grgain_32, gbgain_32, bgain_32;

	LOG_INF("E\n");

	grgain_32 = (pSetSensorAWB->ABS_GAIN_GR + 1) >> 1;
	rgain_32 = (pSetSensorAWB->ABS_GAIN_R + 1) >> 1;
	bgain_32 = (pSetSensorAWB->ABS_GAIN_B + 1) >> 1;
	gbgain_32 = (pSetSensorAWB->ABS_GAIN_GB + 1) >> 1;

	LOG_INF("ABS_GAIN_GR:%d, grgain_32:%d\n",
		pSetSensorAWB->ABS_GAIN_GR,	grgain_32);
	LOG_INF("ABS_GAIN_R:%d, rgain_32:%d\n",
		pSetSensorAWB->ABS_GAIN_R, rgain_32);
	LOG_INF("ABS_GAIN_B:%d, bgain_32:%d\n",
		pSetSensorAWB->ABS_GAIN_B, bgain_32);
	LOG_INF("ABS_GAIN_GB:%d, gbgain_32:%d\n",
		pSetSensorAWB->ABS_GAIN_GB,	gbgain_32);

	write_cmos_sensor_8(0x0b8e, (grgain_32 >> 8) & 0xFF);
	write_cmos_sensor_8(0x0b8f, grgain_32 & 0xFF);
	write_cmos_sensor_8(0x0b90, (rgain_32 >> 8) & 0xFF);
	write_cmos_sensor_8(0x0b91, rgain_32 & 0xFF);
	write_cmos_sensor_8(0x0b92, (bgain_32 >> 8) & 0xFF);
	write_cmos_sensor_8(0x0b93, bgain_32 & 0xFF);
	write_cmos_sensor_8(0x0b94, (gbgain_32 >> 8) & 0xFF);
	write_cmos_sensor_8(0x0b95, gbgain_32 & 0xFF);

	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable)
		write_cmos_sensor_8(0x0100, 0x01);
	else
		write_cmos_sensor_8(0x0100, 0x00);

	mdelay(10);
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
	struct SET_SENSOR_AWB_GAIN *pSetSensorAWB =
		(struct SET_SENSOR_AWB_GAIN *) feature_para;

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
		    "feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n",
		    imgsensor.pclk,
		    imgsensor.current_fps);
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
		write_cmos_sensor_8(
			sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor_8(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or just
		 * return LENS_DRIVER_ID_DO_NOT_CARE
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
		set_auto_flicker_mode(
			(BOOL)*feature_data_16,
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
		LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
		/* read_3P3_eeprom((kal_uint16 )(*feature_data),
		 *	(char*)(uintptr_t)(*(feature_data+1)),
		 *	(kal_uint32)(*(feature_data+2)));
		 */
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	/* for factory mode auto testing */
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
	LOG_INF("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
	imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32) *feature_data);
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
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
		set_imx576_ATR((UINT16)*feature_data,
					(UINT16)*(feature_data + 1),
					(UINT16)*(feature_data + 2));
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("hdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.hdr_mode = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		LOG_INF(
		    "SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d, no support\n",
		    (UINT16) *feature_data,
		    (UINT16) *(feature_data + 1));
		/* hdr_write_shutter((UINT16) *feature_data,
		 *	(UINT16) *(feature_data + 1),
		 *	(UINT16) (*(feature_data + 2)));
		 */
		break;
	case SENSOR_FEATURE_SET_HDR_TRI_SHUTTER:
		LOG_INF(
		    "SENSOR_FEATURE_SET_HDR_TRI_SHUTTER LE=%d, SE=%d, ME=%d\n",
		    (UINT16)*feature_data,
		    (UINT16)*(feature_data + 1),
		    (UINT16)*(feature_data + 2));
		hdr_write_tri_shutter((UINT16)*feature_data,
				(UINT16)*(feature_data+1),
				(UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_SET_HDR_TRI_GAIN:
		LOG_INF(
		    "SENSOR_FEATURE_SET_HDR_TRI_GAIN LGain=%d, SGain=%d, MGain=%d\n",
		    (UINT16)*feature_data,
		    (UINT16)*(feature_data + 1),
		    (UINT16)*(feature_data + 2));
		hdr_write_tri_gain((UINT16)*feature_data,
				   (UINT16)*(feature_data+1),
				   (UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n",
				(UINT16) *feature_data);
		pvcinfo = (struct SENSOR_VC_INFO_STRUCT *)
				(uintptr_t) (*(feature_data + 1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		imx576_awb_gain(pSetSensorAWB);
		break;
	case SENSOR_FEATURE_SET_LSC_TBL:
		{
			kal_uint8 index = *(((kal_uint8 *)feature_para) +
							(*feature_para_len));

			imx576_set_lsc_reg_setting(index, feature_data_16,
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x2;
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		default:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x0;
			break;
		}
		LOG_INF(
		    "SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY scenarioId:%llu, HDR:%llu\n",
		    *feature_data,
		    *(feature_data+1));
		break;
		/*END OF HDR CMD */
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
		default:
			rate = 0;
			break;
		}
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
	}
	break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		/* LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
		 *	*feature_data);
		 */
		PDAFinfo = (struct SET_PD_BLOCK_INFO_T *)
				(uintptr_t)(*(feature_data+1));
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			#if 0
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
			#endif
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		/* LOG_INF(
		 *   "SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
		 *   *feature_data);
		 */
		/*PDAF capacity enable or not,2p8 only full size support PDAF*/
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			/* video & capture use same setting */
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
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

/* kin0603 */
UINT32 IMX576_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc =  &sensor_func;
	return ERROR_NONE;
}	/*	OV5693_MIPI_RAW_SensorInit	*/
