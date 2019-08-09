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
 *	   s5k2l7mipi_Sensor.c
 *
 * Project:
 * --------
 *	   ALPS
 *
 * Description:
 * ------------
 *	   Source code of Sensor driver
 *
 *
 *-----------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#undef USE_OIS
#undef CAPTURE_WDR
/*******************Modify Following Strings for Debug*************************/
#define PFX "s5k2l7_camera_sensor"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

/****************************	 Modify end ***********************************/


#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/err.h>
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "s5k2l7mipiraw_Sensor.h"
#include "s5k2l7_setting.h"

/* #include "s5k2l7_otp.h" */


/* #define TEST_PATTERN_EN */
/*WDR auto ration mode*/
/* #define ENABLE_WDR_AUTO_RATION */
#undef ENABLE_WDR_AUTO_RATION

#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 1020	/* trans# max is 255, each 4 bytes */
#else
#define I2C_BUFFER_LEN 4

#endif

/******************************************************************************
 * Proifling
 ******************************************************************************/
#define PROFILE 1
#if PROFILE
static struct timeval tv1, tv2;
static DEFINE_SPINLOCK(kdsensor_drv_lock);
/******************************************************************************
 *
 ******************************************************************************/
static void KD_SENSOR_PROFILE_INIT(void)
{
	do_gettimeofday(&tv1);
}

/******************************************************************************
 *
 ******************************************************************************/
static void KD_SENSOR_PROFILE(char *tag)
{
	unsigned long TimeIntervalUS;

	spin_lock(&kdsensor_drv_lock);

	do_gettimeofday(&tv2);
	TimeIntervalUS =
	    (tv2.tv_sec - tv1.tv_sec) * 1000000 + (tv2.tv_usec - tv1.tv_usec);
	tv1 = tv2;

	spin_unlock(&kdsensor_drv_lock);
	pr_debug("[%s]Profile = %lu us\n", tag, TimeIntervalUS);
}
#else
static void KD_SENSOR_PROFILE_INIT(void)
{
}

static void KD_SENSOR_PROFILE(char *tag)
{
}
#endif

static kal_uint32 chip_id;

static DEFINE_SPINLOCK(imgsensor_drv_lock);
#define ORIGINAL_VERSION 1
/* #define SLOW_MOTION_120FPS */

#define MODULE_V1_ID 0XD001
/*
 * XXX: The official module v2 id should be 0XD101.
 * Please modify below line if your module v2 id is 0xD101.
 */
#define MODULE_V2_ID 0XD101 /* 0XD101 */

/* sensor information is defined in each mode. */
static struct imgsensor_info_struct imgsensor_info = {

	/* record sensor id defined in Kd_imgsensor.h */
	.sensor_id = S5K2L7_SENSOR_ID,

	.checksum_value = 0xb4cb9203, /* checksum value for Camera Auto Test  */

	.pre = {

		.pclk = 960000000,	/* record different mode's pclk */
		.linelength = 10160, /* record different mode's linelength */
		.framelength = 3149, /* record different mode's framelength */
		.startx = 0, /* record different mode's startx of grabwindow */
		.starty = 0, /* record different mode's starty of grabwindow */

		/* Dual PD: need to tg grab width / 2, p1 drv will * 2 itself */
		.grabwindow_width = 2016,
		/* record different mode's height of grabwindow */
		.grabwindow_height = 1512,

		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.mipi_pixel_rate = 281000000,
		.max_framerate = 300},
	.cap = {
		.pclk = 960000000,	/* record different mode's pclk */
		.linelength = 10256, /* record different mode's linelength */
		.framelength = 3120, /* record different mode's framelength */
		.startx = 0, /* record different mode's startx of grabwindow */
		.starty = 0, /* record different mode's starty of grabwindow */

		/* Dual PD: need to tg grab width / 2, p1 drv will * 2 itself */
		.grabwindow_width = 4032,
		/* record different mode's height of grabwindow */
		.grabwindow_height = 3024,

		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.mipi_pixel_rate = 512000000,
		.max_framerate = 300},
	.cap1 = {
		.pclk = 960000000,	/* record different mode's pclk  */
		.linelength = 10256, /* record different mode's linelength  */
		.framelength = 3120, /* record different mode's framelength */
		.startx = 0, /* record different mode's startx of grabwindow */
		.starty = 0, /* record different mode's starty of grabwindow */

		/* Dual PD: need to tg grab width / 2, p1 drv will * 2 itself */
		.grabwindow_width = 4032,
		/* record different mode's height of grabwindow */
		.grabwindow_height = 3024,
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.mipi_pixel_rate = 512000000,
		.max_framerate = 300},

	.normal_video = {

		.pclk = 960000000,	/* record different mode's pclk  */
		.linelength = 10256, /* record different mode's linelength */
		.framelength = 3120, /* record different mode's framelength */
		.startx = 0, /* record different mode's startx of grabwindow */
		.starty = 0, /* record different mode's starty of grabwindow */

		/* Dual PD: need to tg grab width / 2, p1 drv will * 2 itself */
		.grabwindow_width = 4032,
		/* record different mode's height of grabwindow */
		.grabwindow_height = 3024,
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.mipi_pixel_rate = 512000000,
		.max_framerate = 300},

	.hs_video = {
		.pclk = 960000000,
		.linelength = 10160,
		.framelength = 1049,
		.startx = 0,
		.starty = 0,

		/* record different mode's width of grabwindow */
		.grabwindow_width = 1344,
		/* record different mode's height of grabwindow */
		.grabwindow_height = 756,

		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.mipi_pixel_rate = 361000000,
		.max_framerate = 1200,
	},

	.slim_video = {
		.pclk = 960000000,
		.linelength = 10160,
		.framelength = 3149,
		.startx = 0,
		.starty = 0,

		/* record different mode's width of grabwindow */
		.grabwindow_width = 1344,
		/* record different mode's height of grabwindow */
		.grabwindow_height = 756,

		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.mipi_pixel_rate = 361000000,
		.max_framerate = 300,

	},
	.margin = 16,
	.min_shutter = 1,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 5,	/* support sensor mode num */
	.cap_delay_frame = 3,
	.pre_delay_frame = 3,
	.video_delay_frame = 3,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,
	.isp_driving_current = ISP_DRIVING_8MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,

	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,

	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	/* should list all v1/v2 module possible i2c addr here. */
	.i2c_addr_table = {0x20, 0x5A, 0xFF},
	.i2c_speed = 300,
};

_S5K2L7_MODE1_SENSOR_INFO_;
_S5K2L7_MODE2_SENSOR_INFO_;
_S5K2L7_MODE3_SENSOR_INFO_;
_S5K2L7_MODE1_V2_SENSOR_INFO_;
_S5K2L7_MODE2_V2_SENSOR_INFO_;
_S5K2L7_MODE3_V2_SENSOR_INFO_;

static struct imgsensor_struct imgsensor = {
#ifdef	HV_MIRROR_FLIP
	.mirror = IMAGE_HV_MIRROR, /* mirrorflip information */
#else
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
#endif
	.sensor_mode = IMGSENSOR_MODE_INIT,
	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */

	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.current_fps = 0,

	.autoflicker_en = KAL_FALSE,
	/*  auto flicker enable: KAL_FALSE for disable auto flicker,
	 *  KAL_TRUE for enable auto flicker
	 */
	.test_pattern = KAL_FALSE,
	/*  test pattern mode or not.
	 *  KAL_FALSE for in test pattern mode,
	 *  KAL_TRUE for normal output
	 */

	/* current scenario id */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.hdr_mode = KAL_FALSE, /* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x20,
};


/* Sensor output window information */
/* full_w; full_h; x0_offset; y0_offset; w0_size; h0_size;
 * scale_w; scale_h; x1_offset;  y1_offset;  w1_size;  h1_size;
 * x2_tg_offset;   y2_tg_offset;  w2_tg_size;  h2_tg_size;
 */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{4032, 3024, 0, 0, 4032, 3024, 2016,
	 1512, 0, 0, 2016, 1512, 0, 0, 2016, 1512},	/* Preview */
	{4032, 3024, 0, 0, 4032, 3024, 4032,
	 3024, 0, 0, 4032, 3024, 0, 0, 4032, 3024},	/* capture */
	{4032, 3024, 0, 0, 4032, 3024, 4032,
	  3024, 0, 0, 4032, 3024, 0, 0, 4032, 3024},	/* normal_video */
	{4032, 3024, 0, 378, 4032, 2328, 1344,
	  756, 0, 0, 1344, 756, 0, 0, 1344, 756},	/* hs_video */
	{4032, 3024, 0, 378, 4032, 2328, 1344,
	  756, 0, 0, 1336, 756, 0, 0, 1344, 756},	/* slim_video */
};

_S5K2L7_MODE1_WINSIZE_INFO_;
_S5K2L7_MODE2_WINSIZE_INFO_;
_S5K2L7_MODE3_WINSIZE_INFO_;
_S5K2L7_MODE1_V2_WINSIZE_INFO_;
_S5K2L7_MODE2_V2_WINSIZE_INFO_;
_S5K2L7_MODE3_V2_WINSIZE_INFO_;

/*VC1 None , VC2 for PDAF(DT=0X36), unit : 8bit*/
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[5] = {
	/* Preview mode setting */
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 0x0834, 0x0618, 0x00, 0x00, 0x0280, 0x0001,
		0x01, 0x31, 0x09D8, 0x017a, 0x03, 0x00, 0x0000, 0x0000
	},
	/* Capture mode setting */
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 0x1070, 0x0C30, 0x00, 0x00, 0x0280, 0x0001,
		0x01, 0x31, 0x13b0, 0x02f4, 0x03, 0x00, 0x0000, 0x0000
	},
	/* Video mode setting */
	{
		0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 0x1070, 0x0C30, 0x01, 0x00, 0x0000, 0x0000,
		0x01, 0x31, 0x13b0, 0x02f4, 0x03, 0x00, 0x0000, 0x0000
	},
	/* HS video mode setting */
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 0x1070, 0x0C30, 0x00, 0x00, 0x0280, 0x0001,
		0x01, 0x31, 0x0690, 0x00BD, 0x03, 0x00, 0x0000, 0x0000
	},
	/* Slim video mode setting */
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 0x1070, 0x0C30, 0x00, 0x00, 0x0280, 0x0001,
		0x01, 0x31, 0x0690, 0x00BD, 0x03, 0x00, 0x0000, 0x0000
	}
};


/* #define USE_OIS */
#ifdef USE_OIS
#define OIS_I2C_WRITE_ID 0x48
#define OIS_I2C_READ_ID 0x49

#define RUMBA_OIS_CTRL	 0x0000
#define RUMBA_OIS_STATUS 0x0001
#define RUMBA_OIS_MODE	 0x0002
#define CENTERING_MODE	 0x05
#define RUMBA_OIS_OFF	 0x0030

#define RUMBA_OIS_SETTING_ADD 0x0002
#define RUMBA_OIS_PRE_SETTING 0x02
#define RUMBA_OIS_CAP_SETTING 0x01


#define RUMBA_OIS_PRE	 0
#define RUMBA_OIS_CAP	 1


static void OIS_write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pusendcmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pusendcmd, 3, OIS_I2C_WRITE_ID);
}

static kal_uint16 OIS_read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 1, OIS_I2C_READ_ID);
	return get_byte;
}

static int OIS_on(int mode)
{
	int ret = 0;

	if (mode == RUMBA_OIS_PRE_SETTING)
		OIS_write_cmos_sensor(
			RUMBA_OIS_SETTING_ADD, RUMBA_OIS_PRE_SETTING);

	if (mode == RUMBA_OIS_CAP_SETTING)
		OIS_write_cmos_sensor(
			RUMBA_OIS_SETTING_ADD, RUMBA_OIS_CAP_SETTING);

	OIS_write_cmos_sensor(RUMBA_OIS_MODE, CENTERING_MODE);
	ret = OIS_read_cmos_sensor(RUMBA_OIS_MODE);
	pr_debug("OIS ret=%d %s %d\n", ret, __func__, __LINE__);

	if (ret != CENTERING_MODE) {
		/* not used now */
		/* return -1; */
	}
	OIS_write_cmos_sensor(RUMBA_OIS_CTRL, 0x01);
	ret = OIS_read_cmos_sensor(RUMBA_OIS_CTRL);
	pr_debug("OIS ret=%d %s %d\n", ret, __func__, __LINE__);
	if (ret != 0x01) {
		/* not used now */
		/* return -1; */
	}

}

static int OIS_off(void)
{
	int ret = 0;

	OIS_write_cmos_sensor(RUMBA_OIS_OFF, 0x01);
	ret = OIS_read_cmos_sensor(RUMBA_OIS_OFF);
	pr_debug("OIS ret=%d %s %d\n", ret, __func__, __LINE__);
}
#endif
/* add for s5k2l7 pdaf */
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX = 0,
	.i4PitchY = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0} },
	.i4PosR = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.i4Crop = {
		{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
		{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}
	},
#ifdef HV_MIRROR_FLIP
	.iMirrorFlip = 3,
#else
	.iMirrorFlip = 0,
#endif
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };
 /* iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, imgsensor.i2c_write_id); */

	iReadRegI2CTiming(pu_send_cmd, 2, (u8 *) &get_byte, 1,
			imgsensor.i2c_write_id, imgsensor_info.i2c_speed);

	return get_byte;
}

static kal_uint16 read_cmos_sensor_twobyte(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char get_word[2] = { 0, 0};
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };
	/* iReadRegI2C(pu_send_cmd, 2, get_word, 2, imgsensor.i2c_write_id); */
	iReadRegI2CTiming(pu_send_cmd, 2, get_word, 2, imgsensor.i2c_write_id,
			  imgsensor_info.i2c_speed);
	get_byte = (((int)get_word[0]) << 8) | get_word[1];
	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	/* iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id); */

	iWriteRegI2CTiming(
	    pu_send_cmd, 3, imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
}

static void write_cmos_sensor_twobyte(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {
		(char)(addr >> 8), (char)(addr & 0xFF),
		(char)(para >> 8), (char)(para & 0xFF) };

	/* pr_debug("write_cmos_sensor_twobyte is %x,%x,%x,%x\n",
	 * pu_send_cmd[0], pu_send_cmd[1], pu_send_cmd[2], pu_send_cmd[3]);
	 */
	/* iWriteRegI2C(pu_send_cmd, 4, imgsensor.i2c_write_id); */

	iWriteRegI2CTiming(
	    pu_send_cmd, 4, imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
}

static kal_uint16 table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
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
			puSendCmd[tosend++] = (char)(data >> 8);
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}
#if MULTI_WRITE

		if ((I2C_BUFFER_LEN - tosend) < 4 ||
			len == IDX ||
			addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd,
					tosend,
					imgsensor.i2c_write_id,
					4,
					imgsensor_info.i2c_speed);
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
#if 1
	pr_debug("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);

	/* write_cmos_sensor(0x0104, 0x01); */
	/* write_cmos_sensor_twobyte(0x6028,0x4000); */
	/* write_cmos_sensor_twobyte(0x602A,0xC340 ); */
	write_cmos_sensor_twobyte(0x0340, imgsensor.frame_length);

	/* write_cmos_sensor_twobyte(0x602A,0xC342 ); */
	write_cmos_sensor_twobyte(0x0342, imgsensor.line_length);

	/* write_cmos_sensor(0x0104, 0x00); */
#endif
#if 0
	pr_debug("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	write_cmos_sensor(0x0104, 0x01);
	write_cmos_sensor_twobyte(0x0340, imgsensor.frame_length);
	write_cmos_sensor_twobyte(0x0342, imgsensor.line_length);
	write_cmos_sensor(0x0104, 0x00);
#endif
}				/*    set_dummy  */

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	/* kal_int16 dummy_line; */
	kal_uint32 frame_length = imgsensor.frame_length;
	/* unsigned long flags; */

	pr_debug("framerate = %d, min framelength should enable = %d\n",
		framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length)
		? frame_length : imgsensor.min_frame_length;
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
}			/*    set_max_framerate  */


/*************************************************************************
 * FUNCTION
 *	 set_shutter
 *
 * DESCRIPTION
 *	 This function set e-shutter of sensor to change exposure time.
 *	 The registers 0x3500 ,0x3501 and 0x3502 control exposure of s5k2l7.
 *	 The exposure value is in number of Tline,
 *	 where Tline is the time of sensor one line.
 *
 *	 Exposure = [reg 0x3500]<<12 + [reg 0x3501]<<4 + [reg 0x3502]>>4;
 *	 The maximum exposure value is limited by VTS
 *	 defined by register 0x380e and 0x380f.
 *	 Maximum Exposure <= VTS -4
 *
 * PARAMETERS
 *	 iShutter : exposured lines
 *
 * RETURNS
 *	 None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
	/* pr_debug("enter xxxx  set_shutter, shutter =%d\n", shutter); */

	unsigned long flags;
	/* kal_uint16 realtime_fps = 0; */
	/* kal_uint32 frame_length = 0; */
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	pr_debug("%s =%d\n", __func__, shutter);
	/* OV Recommend Solution */
  /* if shutter bigger than frame_length, should extend frame length first */
	if (!shutter)
		shutter = 1;	/*avoid 0 */
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter)
			? imgsensor_info.min_shutter : shutter;

	shutter =
	  (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
	  ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;


	/* Frame length :4000 C340 */
	/* write_cmos_sensor_twobyte(0x6028,0x4000); */
	/* write_cmos_sensor_twobyte(0x602A,0xC340 ); */
	write_cmos_sensor_twobyte(0x0340, imgsensor.frame_length);

	/* Shutter reg : 4000 C202 */
	/* write_cmos_sensor_twobyte(0x6028,0x4000); */
	/* write_cmos_sensor_twobyte(0x602A,0xC202 ); */
	write_cmos_sensor_twobyte(0x0202, shutter);
	write_cmos_sensor_twobyte(0x0226, shutter);

}				/*    set_shutter */

static void hdr_write_shutter(kal_uint16 le, kal_uint16 se)
{
	/* pr_debug("enter xxxx  set_shutter, shutter =%d\n", shutter); */
	unsigned int iRation;
	unsigned long flags;

	/* kal_uint16 realtime_fps = 0; */
	/* kal_uint32 frame_length = 0; */
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = le;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	if (!le)
		le = 1;		/*avoid 0 */

	spin_lock(&imgsensor_drv_lock);
	if (le > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = le + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	spin_unlock(&imgsensor_drv_lock);

	le =
	    (le < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : le;

	le =
	    (le > (imgsensor_info.max_frame_length - imgsensor_info.margin))
	      ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : le;

	/* Frame length :4000 C340 */
	/* write_cmos_sensor_twobyte(0x6028,0x4000); */
	/* write_cmos_sensor_twobyte(0x602A,0xC340 ); */
	write_cmos_sensor_twobyte(0x0340, imgsensor.frame_length);

	/* SET LE/SE ration */
	/* iRation = (((LE + SE/2)/SE) >> 1 ) << 1 ; */
	iRation = ((10 * le / se) + 5) / 10;
	if (iRation < 2)
		iRation = 1;
	else if (iRation < 4)
		iRation = 2;
	else if (iRation < 8)
		iRation = 4;
	else if (iRation < 16)
		iRation = 8;
	else if (iRation < 32)
		iRation = 16;
	else
		iRation = 1;

	/*set ration for auto */
	iRation = 0x100 * iRation;
#if defined(ENABLE_WDR_AUTO_RATION)
	/*LE / SE ration ,  0x218/0x21a =  LE Ration */
	/*0x218 =0x400, 0x21a=0x100, LE/SE = 4x */
	write_cmos_sensor_twobyte(0x0218, iRation);
	write_cmos_sensor_twobyte(0x021a, 0x100);
#endif
	/*Short exposure */
	write_cmos_sensor_twobyte(0x0202, se);
	/*Log exposure ratio */
	write_cmos_sensor_twobyte(0x0226, le);

	pr_debug("HDR set shutter LE=%d, SE=%d, iRation=0x%x\n",
		le, se, iRation);

}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0;

#ifdef S5K2L7_DIGITAL_GAIN
	reg_gain = gain << 2;
#else
	reg_gain = gain >> 1;
#endif

	return (kal_uint16) reg_gain;
}

/*************************************************************************
 * FUNCTION
 *	 set_gain
 *
 * DESCRIPTION
 *	 This function is to set global gain to sensor.
 *
 * PARAMETERS
 *	 iGain : sensor global gain(base: 0x80)
 *
 * RETURNS
 *	 the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint16 set_gain(kal_uint16 leGain, kal_uint16 seGain)
{
	/* 0x350A[0:1], 0x350B[0:7] AGC real gain */
	/* [0:3] = N meams N /16 X      */
	/* [4:9] = M meams M X           */
	/* Total gain = M + N /16 X   */

	if (leGain < S5K2L7_GAIN_MIN) {
		pr_debug("Error gain setting:LE=%x\n", leGain);
		leGain = S5K2L7_GAIN_MIN;
	}
	if (leGain > S5K2L7_GAIN_MAX) {
		pr_debug("Error gain setting:LE=%x\n", leGain);
		leGain = S5K2L7_GAIN_MAX;
	}
	if (seGain < S5K2L7_GAIN_MIN) {
		pr_debug("Error gain setting:SE=%x\n", seGain);
		seGain = S5K2L7_GAIN_MIN;
	}
	if (seGain > S5K2L7_GAIN_MAX) {
		pr_debug("Error gain setting:SE=%x\n", seGain);
		seGain = S5K2L7_GAIN_MAX;
	}
#ifdef S5K2L7_DIGITAL_GAIN
	write_cmos_sensor_twobyte(0x6028, 0x4000);
	write_cmos_sensor_twobyte(0x602A, 0x020E);

	/* Short exposure gain */
	write_cmos_sensor_twobyte(0x6F12, gain2reg(seGain));

	write_cmos_sensor_twobyte(0x602A, 0x3072);

	/* Long exposure gain */
	write_cmos_sensor_twobyte(0x6F12, gain2reg(leGain));

	/* 0x0208 = 0x0100 to enable long exposure gain */
	write_cmos_sensor(0x0208, 0x01);

	write_cmos_sensor_twobyte(0x602C, 0x4000);
	write_cmos_sensor_twobyte(0x602E, 0x020E);
	seGain = read_cmos_sensor_twobyte(0x6F12);

	write_cmos_sensor_twobyte(0x602C, 0x4000);
	write_cmos_sensor_twobyte(0x602E, 0x3072);
	leGain = read_cmos_sensor_twobyte(0x6F12);
#else
	/* Analog gain HW reg : 4000 C204 */
	write_cmos_sensor_twobyte(0x6028, 0x4000);
	write_cmos_sensor_twobyte(0x602A, 0x0204);

	/* Short exposure gain */
	write_cmos_sensor_twobyte(0x6F12, gain2reg(seGain));

	/* Long exposure gain */
	write_cmos_sensor_twobyte(0x6F12, gain2reg(leGain));

	/* 0x0208 = 0x0100 to enable long exposure gain */
	write_cmos_sensor(0x0208, 0x01);

	write_cmos_sensor_twobyte(0x602C, 0x4000);
	write_cmos_sensor_twobyte(0x602E, 0x0204);
	seGain = read_cmos_sensor_twobyte(0x6F12);

	write_cmos_sensor_twobyte(0x602C, 0x4000);
	write_cmos_sensor_twobyte(0x602E, 0x0206);
	leGain = read_cmos_sensor_twobyte(0x6F12);
#endif
	pr_debug("LE(0x%x), SE(0x%x)\n", leGain, seGain);

	return leGain;
}				/* set_gain */

/*************************************************************************
 * FUNCTION
 *      is_module_v2
 *
 * DESCRIPTION
 *      This function is to check module version.
 *
 * RETURNS
 *      version 2 or not.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static BOOL is_module_v2(void)
{
	static UINT32 module_id;

	/* If never read module id */
	if (module_id == 0) {
		/* Read from sensor */
		module_id = read_cmos_sensor_twobyte(0x0002);

		pr_debug("i2c write id: 0x%x, module id: 0x%x\n",
			 imgsensor.i2c_write_id,
			 module_id);
	}

	if (module_id == MODULE_V2_ID) {
		pr_debug("It is module v2");

		return TRUE;
	}
	pr_debug("It is module v1");
	return FALSE;
}

static void set_mirror_flip(kal_uint8 image_mirror)
{
	pr_debug("image_mirror = %d\n", image_mirror);
#if 1
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
	spin_lock(&imgsensor_drv_lock);
	imgsensor.mirror = image_mirror;
	spin_unlock(&imgsensor_drv_lock);
	switch (image_mirror) {

	case IMAGE_NORMAL:
		write_cmos_sensor(0x0101, 0x00);	/* Gr */
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor(0x0101, 0x01);
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor(0x0101, 0x02);
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor(0x0101, 0x03);	/* Gb */
		break;
	default:
		pr_debug("Error image_mirror setting\n");

	}
#endif
}

/*************************************************************************
 * FUNCTION
 *	 night_mode
 *
 * DESCRIPTION
 *	 This function night mode of sensor.
 *
 * PARAMETERS
 *	 bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 *	 None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void night_mode(kal_bool enable)
{
	/*No Need to implement this function */
}				/*    night_mode        */

/* #define      S5K2l7FW */


static void sensor_WDR_zhdr(void)
{
	if (imgsensor.hdr_mode == 9) {
		pr_debug("%s\n", __func__);
		/*it would write 0x21E = 0x1, 0x21F=0x00 */
		/*0x21E=1 , Enable WDR */
		/*0x21F=0x00, Use Manual mode to set short /long exp */
#if defined(ENABLE_WDR_AUTO_RATION)
		/*For WDR auot ration */
		write_cmos_sensor_twobyte(0x021E, 0x0101);

#else
		/*For WDR manual ration */
		write_cmos_sensor_twobyte(0x021E, 0x0100);

#endif
		write_cmos_sensor_twobyte(0x0220, 0x0801);
		write_cmos_sensor_twobyte(0x0222, 0x0100);
	} else {
		write_cmos_sensor_twobyte(0x021E, 0x0000);
		write_cmos_sensor_twobyte(0x0220, 0x0801);
	}
	/*for LE/SE Test */
	/* hdr_write_shutter(3460,800); */

}

static void sensor_init_11_new(void)
{
	pr_debug("E S5k2L7 sensor init (%d)\n", pdaf_sensor_mode);
	KD_SENSOR_PROFILE_INIT();

	if (is_module_v2() != FALSE) {
		/* New module setting */
		if (pdaf_sensor_mode == 1)
			_S5K2L7_MODE1_INIT_MODULE_V2_;

		else if (pdaf_sensor_mode == 2)
			_S5K2L7_MODE2_INIT_MODULE_V2_;
		else
			_S5K2L7_MODE3_INIT_MODULE_V2_;
	} else {
		/* Old module setting */
		write_cmos_sensor_twobyte(0X6028, 0X2000);
		write_cmos_sensor_twobyte(0X602A, 0XBBF8);
		write_cmos_sensor_twobyte(0X6F12, 0X0000);
		write_cmos_sensor_twobyte(0X6028, 0X4000);
		write_cmos_sensor_twobyte(0X6018, 0X0001);
		write_cmos_sensor_twobyte(0X7002, 0X000C);
		write_cmos_sensor_twobyte(0X6014, 0X0001);
		mdelay(8);
		if (pdaf_sensor_mode == 1)
			_S5K2L7_MODE1_INIT_;
		else if (pdaf_sensor_mode == 2)
			_S5K2L7_MODE2_INIT_;
		else
			_S5K2L7_MODE3_INIT_;
	}
	KD_SENSOR_PROFILE("S5k2L7 sensor init");
}

static void preview_setting_11_new(void)
{
	pr_debug("E S5k2L7 preview setting (%d)\n", pdaf_sensor_mode);

	if (is_module_v2() != FALSE) {
		/* New module setting */
		if (pdaf_sensor_mode == 1)
			_S5K2L7_MODE1_PREVIEW_MODULE_V2_;
		else if (pdaf_sensor_mode == 2)
			_S5K2L7_MODE2_PREVIEW_MODULE_V2_;
		else
			_S5K2L7_MODE3_PREVIEW_MODULE_V2_;
	} else {
		/* Old module setting */
		if (pdaf_sensor_mode == 1)
			_S5K2L7_MODE1_PREVIEW_;
		else if (pdaf_sensor_mode == 2)
			_S5K2L7_MODE2_PREVIEW_;
		else
			_S5K2L7_MODE3_PREVIEW_;
	}
}

#ifdef CAPTURE_WDR
static void capture_setting_WDR(kal_uint16 currefps)
{
	pr_debug("E S5k2L7 Capture WDR, fps = %d (%d)\n",
		currefps, pdaf_sensor_mode);

	if (is_module_v2() != FALSE) {
		/* New module setting */
		if (pdaf_sensor_mode == 1)
			_S5K2L7_MODE1_CAPTURE_WDR_MODULE_V2_;
		else if (pdaf_sensor_mode == 2)
			_S5K2L7_MODE2_CAPTURE_WDR_MODULE_V2_;
		else
			_S5K2L7_MODE3_CAPTURE_WDR_MODULE_V2_;
	} else {
		/* Old module setting */
		if (pdaf_sensor_mode == 1)
			_S5K2L7_MODE1_CAPTURE_WDR_;
		else if (pdaf_sensor_mode == 2)
			_S5K2L7_MODE2_CAPTURE_WDR_;
		else
			_S5K2L7_MODE3_CAPTURE_WDR_;
	}
}
#endif

static void capture_setting(void)
{
	pr_debug("E S5k2L7 capture setting (%d)\n", pdaf_sensor_mode);

	if (is_module_v2() != FALSE) {
		/* New module setting */
		if (pdaf_sensor_mode == 1)
			_S5K2L7_MODE1_CAPTURE_MODULE_V2_;
		else if (pdaf_sensor_mode == 2)
			_S5K2L7_MODE2_CAPTURE_MODULE_V2_;
		else
			_S5K2L7_MODE3_CAPTURE_MODULE_V2_;
	} else {
		/* Old module setting */
		if (pdaf_sensor_mode == 1)
			_S5K2L7_MODE1_CAPTURE_;
		else if (pdaf_sensor_mode == 2)
			_S5K2L7_MODE2_CAPTURE_;
		else
			_S5K2L7_MODE3_CAPTURE_;
	}
}

#if 0
static void normal_video_setting_11_new(kal_uint16 currefps)
{
}
#endif

static void hs_video_setting_11(void)
{
	pr_debug("E S5k2L7 HS video setting (%d)\n", pdaf_sensor_mode);
	KD_SENSOR_PROFILE_INIT();

	if (is_module_v2() != FALSE) {
		/* New module setting */
		if (pdaf_sensor_mode == 1)
			_S5K2L7_MODE1_HS_VIDEO_MODULE_V2_;
		else if (pdaf_sensor_mode == 2)
			_S5K2L7_MODE2_HS_VIDEO_MODULE_V2_;
		else
			_S5K2L7_MODE3_HS_VIDEO_MODULE_V2_;
	} else {
		/* Old module setting */
		if (pdaf_sensor_mode == 1)
			_S5K2L7_MODE1_HS_VIDEO_;
		else if (pdaf_sensor_mode == 2)
			_S5K2L7_MODE2_HS_VIDEO_;
		else
			_S5K2L7_MODE3_HS_VIDEO_;
	}
	KD_SENSOR_PROFILE("2l7 hs video");
}


static void slim_video_setting(void)
{
	pr_debug("E S5k2L7 slim video setting (%d)\n", pdaf_sensor_mode);
	KD_SENSOR_PROFILE_INIT();

	if (is_module_v2() != FALSE) {
		/* New module setting */
		if (pdaf_sensor_mode == 1)
			_S5K2L7_MODE1_SLIM_VIDEO_MODULE_V2_;
		else if (pdaf_sensor_mode == 2)
			_S5K2L7_MODE2_SLIM_VIDEO_MODULE_V2_;
		else
			_S5K2L7_MODE3_SLIM_VIDEO_MODULE_V2_;
	} else {
		/* Old module setting */
		if (pdaf_sensor_mode == 1)
			_S5K2L7_MODE1_SLIM_VIDEO_;
		else if (pdaf_sensor_mode == 2)
			_S5K2L7_MODE2_SLIM_VIDEO_;
		else
			_S5K2L7_MODE3_SLIM_VIDEO_;
	}
	KD_SENSOR_PROFILE("S5k2L7 slim video setting");
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	pr_debug("enable: %d\n", enable);

	/********************************************************
	 *0x5040[7]: 1 enable,  0 disable
	 *0x5040[3:2]; color bar style 00 standard color bar
	 *0x5040[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
	 ********************************************************/


	if (enable)
		write_cmos_sensor_twobyte(0x0600, 0x0002);/* 100% colour bars */
	else
		write_cmos_sensor_twobyte(0x0600, 0x0000);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

/*************************************************************************
 * FUNCTION
 *	 get_imgsensor_id
 *
 * DESCRIPTION
 *	 This function get the sensor ID
 *
 * PARAMETERS
 *	 *sensorID : return the sensor ID
 *
 * RETURNS
 *	 None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 5;

	/* query sensor id */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {

			*sensor_id = read_cmos_sensor_twobyte(0x0000);

			if (*sensor_id == imgsensor_info.sensor_id
			    || *sensor_id == 0x20C1) {
				pr_debug(
				    "i2c write id: 0x%x, sensor id: 0x%x\n",
				    imgsensor.i2c_write_id, *sensor_id);

				break;
			}

			pr_debug(
			    "Read sensor id fail, write id: 0x%x, id: 0x%x\n",
			    imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);

		if (*sensor_id == imgsensor_info.sensor_id
		    || *sensor_id == 0x20C1) {
			break;
		}
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id && *sensor_id != 0x20C1) {
		/* if Sensor ID is not correct,
		 * Must set *sensor_id to 0xFFFFFFFF
		 */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}

	pdaf_sensor_mode = proc_pdaf_sensor_mode;

	pr_debug("%s pdaf sensor mode %d\n", __func__, pdaf_sensor_mode);

	if (is_module_v2() != FALSE) {
		if (pdaf_sensor_mode == 1)
			_SET_MODE1_V2_SENSOR_INFO_AND_WINSIZE_;
		else if (pdaf_sensor_mode == 2)
			_SET_MODE2_V2_SENSOR_INFO_AND_WINSIZE_;
		else
			_SET_MODE3_V2_SENSOR_INFO_AND_WINSIZE_;
	} else {
		if (pdaf_sensor_mode == 1)
			_SET_MODE1_SENSOR_INFO_AND_WINSIZE_;
		else if (pdaf_sensor_mode == 2)
			_SET_MODE2_SENSOR_INFO_AND_WINSIZE_;
		else
			_SET_MODE3_SENSOR_INFO_AND_WINSIZE_;
	}

#ifdef HV_MIRROR_FLIP
	imgsensor_info.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb;
#else
	imgsensor_info.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr;
#endif

	return ERROR_NONE;
}


/*************************************************************************
 * FUNCTION
 *	 open
 *
 * DESCRIPTION
 *	 This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *	 None
 *
 * RETURNS
 *	 None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 open(void)
{

	kal_uint8 i = 0;
	kal_uint8 retry = 5;
	kal_uint32 sensor_id = 0;
	/* kal_uint32 chip_id = 0; */

	pr_debug("s5k2l7,MIPI 4LANE\n");
	/* LOG_2; */
#if 1
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			write_cmos_sensor_twobyte(0x602C, 0x4000);
			write_cmos_sensor_twobyte(0x602E, 0x0000);
			sensor_id = read_cmos_sensor_twobyte(0x6F12);
    /* pr_debug("JEFF get_imgsensor_id-read sensor ID (0x%x)\n", sensor_id ); */

			write_cmos_sensor_twobyte(0x602C, 0x4000);
			write_cmos_sensor_twobyte(0x602E, 0x001A);
			chip_id = read_cmos_sensor_twobyte(0x6F12);

	/* chip_id = read_cmos_sensor_twobyte(0x001A); */
	/* pr_debug("get_imgsensor_id-read chip_id (0x%x)\n", chip_id ); */

	if (sensor_id == imgsensor_info.sensor_id || sensor_id == 0x20C1) {
		pr_debug("i2c write id: 0x%x, sensor id: 0x%x, chip_id (0x%x)\n",
			imgsensor.i2c_write_id, sensor_id, chip_id);

		break;
	}
		pr_debug("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
			imgsensor.i2c_write_id, sensor_id);
			retry--;
		} while (retry > 0);
	i++;

	if (sensor_id == imgsensor_info.sensor_id || sensor_id == 0x20C1) {
	/*20171114ken : fix coding style*/
		break;
	}
		retry = 2;
	}

	if (imgsensor_info.sensor_id != sensor_id && 0x20C1 != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;
#endif

	write_cmos_sensor_twobyte(0x602C, 0x4000);
	write_cmos_sensor_twobyte(0x602E, 0x001A);
	chip_id = read_cmos_sensor_twobyte(0x6F12);
	/* chip_id = read_cmos_sensor_twobyte(0x001A); */
	pr_debug("JEFF get_imgsensor_id-read chip_id (0x%x)\n", chip_id);
	/* initail sequence write in  */
	/* chip_id == 0x022C */
	sensor_init_11_new();

#ifdef HV_MIRROR_FLIP
	set_mirror_flip(IMAGE_HV_MIRROR);
#else
	set_mirror_flip(IMAGE_NORMAL);
#endif

#ifdef	USE_OIS
	/* OIS_on(RUMBA_OIS_CAP_SETTING);//pangfei OIS */
	pr_debug("pangfei capture OIS setting\n");
	OIS_write_cmos_sensor(0x0002, 0x05);
	OIS_write_cmos_sensor(0x0002, 0x00);
	OIS_write_cmos_sensor(0x0000, 0x01);
#endif

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.hdr_mode = KAL_FALSE;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}				/*    open  */



/*************************************************************************
 * FUNCTION
 *	 close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *	 None
 *
 * RETURNS
 *	 None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 close(void)
{
	pr_debug("E\n");

	/*No Need to implement this function */

	return ERROR_NONE;
}				/*    close  */


/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *	 This function start the sensor preview.
 *
 * PARAMETERS
 *	 *image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *	 None
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
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting_11_new();

	/* set_mirror_flip(IMAGE_NORMAL); */

#ifdef USE_OIS
	/* OIS_on(RUMBA_OIS_PRE_SETTING);    //pangfei OIS */
	pr_debug("%s OIS setting\n", __func__);
	OIS_write_cmos_sensor(0x0002, 0x05);
	OIS_write_cmos_sensor(0x0002, 0x00);
	OIS_write_cmos_sensor(0x0000, 0x01);
#endif
	return ERROR_NONE;
}				/*    preview   */

/*************************************************************************
 * FUNCTION
 *	 capture
 *
 * DESCRIPTION
 *	 This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *	 None
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

	/* Mark PIP case for dual pd */
	if (0) {
	/* (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) */
	/* PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M */
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			pr_debug(
		    "Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
		  imgsensor.current_fps, imgsensor_info.cap.max_framerate / 10);

		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);


	/* Mark HDR setting mode for dual pd sensor */
#ifdef CAPTURE_WDR
	if (imgsensor.hdr_mode == 9)
		capture_setting_WDR(imgsensor.current_fps);
	else
#endif
		capture_setting();


	/* set_mirror_flip(IMAGE_NORMAL); */
#ifdef	USE_OIS
	/* OIS_on(RUMBA_OIS_CAP_SETTING);//pangfei OIS */
	pr_debug("%s OIS setting\n", __func__);
	OIS_write_cmos_sensor(0x0002, 0x05);
	OIS_write_cmos_sensor(0x0002, 0x00);
	OIS_write_cmos_sensor(0x0000, 0x01);
#endif
	return ERROR_NONE;
}				/* capture() */

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
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	capture_setting();

	/* set_mirror_flip(IMAGE_NORMAL); */
	return ERROR_NONE;
}				/*    normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");

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

	hs_video_setting_11();

	return ERROR_NONE;
}				/*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");

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
}				/*    slim_video         */

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

	return ERROR_NONE;
}				/*    get_resolution    */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("scenario_id = %d\n", scenario_id);


	/* sensor_info->SensorVideoFrameRate =
	 * imgsensor_info.normal_video.max_framerate/10;
	 */ /* not use */

	/* sensor_info->SensorStillCaptureFrameRate =
	 * imgsensor_info.cap.max_framerate/10;
	 */ /* not use */

	/* imgsensor_info->SensorWebCamCaptureFrameRate =
	 * imgsensor_info.v.max_framerate;
	 */ /* not use */

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* not use */
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* inverse with datasheet */
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

	/* The frame of setting shutter default 0 for TG int	 */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;

	/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame =
	imgsensor_info.ae_sensor_gain_delay_frame;

	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;

	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->TEMPERATURE_SUPPORT = 1;

	/*
	 * 0: NO PDAF, 1: PDAF Raw Data mode,
	 * 2:PDAF VC mode(Full), 3:PDAF VC mode(Binning),
	 * 4: PDAF DualPD Raw Data mode, 5: PDAF DualPD VC mode
	 */
	if (pdaf_sensor_mode == 1)
		sensor_info->PDAF_Support = PDAF_SUPPORT_RAW_DUALPD;
	else if (pdaf_sensor_mode == 3)
		sensor_info->PDAF_Support = PDAF_SUPPORT_CAMSV_DUALPD;
	else
		sensor_info->PDAF_Support = PDAF_SUPPORT_NA;

	sensor_info->HDR_Support = 3;	/*0: NO HDR, 1: iHDR, 2:mvHDR, 3:zHDR */

	/*
	 * 0: no support, 1: G0,R0.B0, 2: G0,R0.B1, 3: G0,R1.B0, 4: G0,R1.B1
	 * 5: G1,R0.B0, 6: G1,R0.B1, 7: G1,R1.B0, 8: G1,R1.B1
	 */
#ifdef HV_MIRROR_FLIP
	sensor_info->ZHDR_Mode = 1;
#else
	sensor_info->ZHDR_Mode = 5;
#endif

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
}				/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	/* This Function not used after ROME */
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
	pr_debug("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)		/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id,	MUINT32 framerate)
{
	kal_uint32 frame_length;

	pr_debug("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk
			/ framerate * 10 / imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			  (frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;


		frame_length = imgsensor_info.normal_video.pclk
		    / framerate * 10 / imgsensor_info.normal_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		   (frame_length > imgsensor_info.normal_video.framelength)
		 ? (frame_length - imgsensor_info.normal_video.framelength) : 0;

		imgsensor.frame_length =
		 imgsensor_info.normal_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;

	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:

	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		frame_length = imgsensor_info.cap1.pclk
			    / framerate * 10 / imgsensor_info.cap1.linelength;

			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			   (frame_length > imgsensor_info.cap1.framelength)
			 ? (frame_length - imgsensor_info.cap1.framelength) : 0;

			imgsensor.frame_length =
			 imgsensor_info.cap1.framelength + imgsensor.dummy_line;

			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {

	if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
		pr_debug(
	    "Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
	    framerate, imgsensor_info.cap.max_framerate / 10);

			frame_length = imgsensor_info.cap.pclk
			    / framerate * 10 / imgsensor_info.cap.linelength;

			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			    (frame_length > imgsensor_info.cap.framelength)
			  ? (frame_length - imgsensor_info.cap.framelength) : 0;

			imgsensor.frame_length =
			  imgsensor_info.cap.framelength + imgsensor.dummy_line;

			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk
			/ framerate * 10 / imgsensor_info.hs_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		      (frame_length > imgsensor_info.hs_video.framelength)
		    ? (frame_length - imgsensor_info.hs_video.framelength) : 0;

		imgsensor.frame_length =
		    imgsensor_info.hs_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk
			/ framerate * 10 / imgsensor_info.slim_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length > imgsensor_info.slim_video.framelength)
		  ? (frame_length - imgsensor_info.slim_video.framelength) : 0;

		imgsensor.frame_length =
		  imgsensor_info.slim_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	default:		/* coding with  preview scenario by default */
		frame_length = imgsensor_info.pre.pclk
			/ framerate * 10 / imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			  (frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */

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

static kal_uint32 streaming_control(kal_bool enable)
{
	int timeout = (10000 / imgsensor.current_fps) + 1;
	int i = 0;

	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable) {
		write_cmos_sensor(0x0100, 0X01);
		mDELAY(10);
	} else {
		write_cmos_sensor(0x0100, 0x00);
		for (i = 0; i < timeout; i++) {
			if (read_cmos_sensor(0x0005) != 0xFF)
				mDELAY(1);
			else
				break;

		}
		pr_debug("streaming_off exit\n");
	}
	return ERROR_NONE;
}



static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT32 sensor_id = 0;
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;
	UINT32 fps = 0;

	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/* feature_id =
	 * SENSOR_FEATURE_SET_ESHUTTER(0x3004)&SENSOR_FEATURE_SET_GAIN(0x3006)
	 */

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
		pr_debug(
	    "feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n",
	    imgsensor.pclk, imgsensor.current_fps);

		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;

	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL) (*feature_data));
		break;

	case SENSOR_FEATURE_SET_GAIN:
		set_gain(
		(kal_uint16) (*feature_data), (kal_uint16) (*feature_data));
		break;

	case SENSOR_FEATURE_SET_DUAL_GAIN:
		set_gain(
	(kal_uint16) (*feature_data), (kal_uint16) (*(feature_data + 1)));
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
		/* if EEPROM does not exist in camera module. */
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
			(BOOL) (*feature_data_16), *(feature_data_16 + 1));
		break;


	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
	    (enum MSDK_SCENARIO_ID_ENUM) (*feature_data), *(feature_data + 1));
		break;

	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *(feature_data),
				(MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;

	case SENSOR_FEATURE_GET_PDAF_DATA:
		get_imgsensor_id(&sensor_id);
		/* add for s5k2l7 pdaf */
		pr_debug("s5k2l7_read_otp_pdaf_data %x\n", sensor_id);

	s5k2l7_read_otp_pdaf_data((kal_uint16) (*feature_data),
			(char *)(uintptr_t) (*(feature_data + 1)),
			(kal_uint32) (*(feature_data + 2)), sensor_id);
		break;

	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) (*feature_data));
		break;

	/* for factory mode auto testing */
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		pr_debug("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		pr_debug("hdr mode :%d\n", (*feature_data_32));
		spin_lock(&imgsensor_drv_lock);
		imgsensor.hdr_mode = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:	/*0x3080 */
/* pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%lld\n", *feature_data); */

		wininfo =
	(struct SENSOR_WINSIZE_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[1],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[3],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[4],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;

		/* add for s5k2l7 pdaf */
	case SENSOR_FEATURE_GET_PDAF_INFO:
		pr_debug("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%lld\n",
			*feature_data);

		PDAFinfo =
	    (struct SET_PD_BLOCK_INFO_T *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
			       sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		}
		break;

	case SENSOR_FEATURE_GET_VC_INFO:
		pr_debug("SENSOR_FEATURE_GET_VC_INFO %d\n",
			(UINT16) *feature_data);

		pvcinfo =
	    (struct SENSOR_VC_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		if (pdaf_sensor_mode == 3) {
			switch (*feature_data_32) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				memcpy(
					(void *)pvcinfo,
					(void *)&SENSOR_VC_INFO[1],
					sizeof(struct SENSOR_VC_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				memcpy(
					(void *)pvcinfo,
					(void *)&SENSOR_VC_INFO[2],
					sizeof(struct SENSOR_VC_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				memcpy(
					(void *)pvcinfo,
					(void *)&SENSOR_VC_INFO[3],
					sizeof(struct SENSOR_VC_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				memcpy(
					(void *)pvcinfo,
					(void *)&SENSOR_VC_INFO[4],
					sizeof(struct SENSOR_VC_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				memcpy(
					(void *)pvcinfo,
					(void *)&SENSOR_VC_INFO[0],
					sizeof(struct SENSOR_VC_INFO_STRUCT));
				break;
			}
		} else {
			memset(
		    (void *)pvcinfo, 0, sizeof(struct SENSOR_VC_INFO_STRUCT));

		}
		break;

	case SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY:
		pr_debug(
		"SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY scenarioId:%llu\n",
			*feature_data);

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x09;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x09;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x0;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x0;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x0;
			break;
		default:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x0;
			break;
		}
		break;



	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		pr_debug(
		"SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%lld\n",
		*feature_data);

	  /* PDAF capacity enable or not, s5k2l7 only full size support PDAF */
		switch (*feature_data) {

		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (pdaf_sensor_mode == 1)
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
		else if (pdaf_sensor_mode == 3)
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
		else
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
		break;

		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (pdaf_sensor_mode == 1)
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
		else if (pdaf_sensor_mode == 3)
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
		else
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
		break;

		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		if (pdaf_sensor_mode == 1)
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
		else if (pdaf_sensor_mode == 3)
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
		else
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
		break;

		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		if (pdaf_sensor_mode == 1)
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
		else if (pdaf_sensor_mode == 3)
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
		else
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
		break;

		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		if (pdaf_sensor_mode == 1)
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
		else if (pdaf_sensor_mode == 3)
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
		else
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
		break;


		default:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
			break;
		}
		break;

	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		pr_debug("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16) *feature_data,	(UINT16) *(feature_data + 1));

		hdr_write_shutter((UINT16) *feature_data,
				  (UINT16) *(feature_data + 1));
		break;

	case SENSOR_FEATURE_GET_PDAF_TYPE:
		*feature_para = pdaf_sensor_mode;
		if (pdaf_sensor_mode == 1)
			sprintf(feature_para, "configure S5K2L7 as mode 1");
		else if (pdaf_sensor_mode == 2)
			sprintf(feature_para, "configure S5K2L7 as mode 2");
		else if (pdaf_sensor_mode == 3)
			sprintf(feature_para, "configure S5K2L7 as mode 3");
		else
			sprintf(
			    feature_para, "configure S5K2L7 as unknown mode");

		pr_debug("get PDAF type = %d\n", pdaf_sensor_mode);
		break;

	case SENSOR_FEATURE_SET_PDAF_TYPE:
		if (strstr(&(*feature_para), "mode1")) {
			pr_debug("configure PDAF as mode 1\n");
			proc_pdaf_sensor_mode = 1;
		} else if (strstr(&(*feature_para), "mode3")) {
			pr_debug("configure PDAF as mode 3\n");
			proc_pdaf_sensor_mode = 3;
		} else if (strstr(&(*feature_para), "mode2")) {
			pr_debug("configure PDAF as mode 2\n");
			proc_pdaf_sensor_mode = 2;
		} else {
			pr_debug("configure PDAF as unknown mode\n");
			proc_pdaf_sensor_mode = 1;
		}
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
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		fps = (MUINT32)(*(feature_data + 2));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			if (fps == 240)
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap1.mipi_pixel_rate;
			else
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
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

UINT32 S5K2L7_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*    s5k2l7_MIPI_RAW_SensorInit        */
