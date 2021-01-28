// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
/* #include <asm/system.h> */

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "s5k3l8mipiraw_Sensor.h"

/*===FEATURE SWITH===*/
/* #define FPTPDAFSUPPORT   //for pdaf switch */
/* #define FANPENGTAO   //for debug log */
#define LOG_INF LOG_INF_NEW
/* #define NONCONTINUEMODE */
/*===FEATURE SWITH===*/

/********************Modify Following Strings for Debug************************/
#define PFX "S5K3L8"
#define LOG_INF_NEW(format, args...)    pr_debug(PFX "[%s] " format, __func__, ##args)
#define LOG_1 LOG_INF("S5K3L8,MIPI 4LANE\n")
#define SENSORDB LOG_INF
/****************************   Modify end    *********************************/

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static MUINT32 g_sync_mode = SENSOR_NO_SYNC_MODE;

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5K3L8_SENSOR_ID,
/* Sensor ID Value: 0x30C8//record sensor id defined in Kd_imgsensor.h */

	.checksum_value = 0x49c09f86, /* checksum value for Camera Auto Test */

	.pre = {
		.pclk = 560000000,	/* record different mode's pclk */
		.linelength = 5808,	/* record different mode's linelength */
		.framelength = 3224, /* record different mode's framelength */
		.startx = 0, /* record different mode's startx of grabwindow */
		.starty = 0, /* record different mode's starty of grabwindow */
		.grabwindow_width = 2104,
		/* record different mode's width of grabwindow */
		.grabwindow_height = 1560,
		/* record different mode's height of grabwindow */
/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount
 * by different scenario
 */
		.mipi_data_lp2hs_settle_dc = 85,
		/*       following for GetDefaultFramerateByScenario()  */
		.max_framerate = 300,
	},
#ifdef NONCONTINUEMODE
	.cap = {
		.pclk = 560000000,	/* record different mode's pclk */
		.linelength = 5920,
		/* 5808, record different mode's linelength */
		.framelength = 3206, /* record different mode's framelength */
		.startx = 0,
		/* record different mode's startx of grabwindow */
		.starty = 0,
		/* record different mode's starty of grabwindow */
		.grabwindow_width = 4208,
		/* record different mode's width of grabwindow */
		.grabwindow_height = 3120,
		/* record different mode's height of grabwindow */
		/*      following for MIPIDataLowPwr2HighSpeedSettleDelayCount
		* by different scenario
		*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*       following for GetDefaultFramerateByScenario()  */
		.max_framerate = 300,
	},
#else				/* CONTINUEMODE */
	.cap = {
		.pclk = 560000000,	/* record different mode's pclk */
		.linelength = 5808,	/* record different mode's linelength */
		.framelength = 3224,	/* record different mode's framelength */
		.startx = 0, /* record different mode's startx of grabwindow */
		.starty = 0, /* record different mode's starty of grabwindow */
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		/*       following for GetDefaultFramerateByScenario()  */
		.max_framerate = 300,
	},
#endif

	/*
	 * capture for PIP 15ps relative information,
	 * capture1 mode must use same framelength,
	 * linelength with Capture mode for shutter calculate
	 */
	.cap1 = {
		.pclk = 400000000,
		.linelength = 5808,
		.framelength = 4589,/
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		/*       following for GetDefaultFramerateByScenario()  */
		.max_framerate = 150,
	},

	.normal_video = {
		.pclk = 560000000,	/* record different mode's pclk */
		.linelength = 5808,	/* record different mode's linelength */
		.framelength = 3224,	/* record different mode's framelength */
		.startx = 0,	/* record different mode's startx of grabwindow */
		.starty = 0,	/* record different mode's starty of grabwindow */
		.grabwindow_width = 4208,	/* record different mode's width of grabwindow */
		.grabwindow_height = 3120,	/* record different mode's height of grabwindow */
		/*       following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
		.mipi_data_lp2hs_settle_dc = 85,
		/*       following for GetDefaultFramerateByScenario()  */
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 560000000,	/* record different mode's pclk */
		.linelength = 5808,	/* record different mode's linelength */
		.framelength = 803,	/* record different mode's framelength */
		.startx = 0,	/* record different mode's startx of grabwindow */
		.starty = 0,	/* record different mode's starty of grabwindow */
		.grabwindow_width = 640,	/* record different mode's width of grabwindow */
		.grabwindow_height = 480,	/* record different mode's height of grabwindow */
		/*       following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
		.mipi_data_lp2hs_settle_dc = 85,
		/*       following for GetDefaultFramerateByScenario()  */
		.max_framerate = 1200,
	},
	.slim_video = {
		.pclk = 560000000,	/* record different mode's pclk */
		.linelength = 5808,	/* record different mode's linelength */
		.framelength = 803,	/* record different mode's framelength */
		.startx = 0,	/* record different mode's startx of grabwindow */
		.starty = 0,	/* record different mode's starty of grabwindow */
		.grabwindow_width = 1280,	/* record different mode's width of grabwindow */
		.grabwindow_height = 720,	/* record different mode's height of grabwindow */
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
	},
	.custom1 = {
		.pclk = 440000000,
		.linelength = 4592,
		.framelength = 3188,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2096,
		.grabwindow_height = 1552,

		/*       following for MIPIDataLowPwr2HighSpeedSettleDelayCount
		 * by different scenario
		 */
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		/*       following for GetDefaultFramerateByScenario()  */
		.max_framerate = 300,
	},
	.custom2 = {
		.pclk = 440000000,	/* record different mode's pclk */
		.linelength = 4592,	/* record different mode's linelength */
		.framelength = 3188,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2096,
		.grabwindow_height = 1552,

		/*       following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		/*       following for GetDefaultFramerateByScenario()  */
		.max_framerate = 300,
	},
	.custom3 = {
		.pclk = 440000000,
		.linelength = 4592,
		.framelength = 3188,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2096,	/* record different mode's width of grabwindow */
		.grabwindow_height = 1552,	/* record different mode's height of grabwindow */

		/*       following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		/*       following for GetDefaultFramerateByScenario()  */
		.max_framerate = 300,
	},
	.custom4 = {
		.pclk = 440000000,
		.linelength = 4592,
		.framelength = 3188,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2096,
		.grabwindow_height = 1552,

		/*       following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		/*       following for GetDefaultFramerateByScenario()  */
		.max_framerate = 300,
	},
	.custom5 = {
		.pclk = 440000000,
		.linelength = 4592,
		.framelength = 3188,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2096,
		.grabwindow_height = 1552,

		/*       following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		/*       following for GetDefaultFramerateByScenario()  */
		.max_framerate = 300,
	},

	.margin = 5,		/* sensor framelength & shutter margin */
	.min_shutter = 4,	/* min shutter */
	.max_frame_length = 0xFFFF,	/* REG0x0202 <=REG0x0340-5//max framelength by sensor register's limitation */
	.ae_shut_delay_frame = 0,	/* shutter delay frame for AE cycle,
					 * 2 frame with ispGain_delay-shut_delay=2-0=2
					 */
	.ae_sensor_gain_delay_frame = 0,	/* sensor gain delay frame for AE cycle,
						 * 2 frame with ispGain_delay-sensor_gain_delay=2-0=2
						 */
	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 5,	/* support sensor mode num ,don't support Slow motion */

	.cap_delay_frame = 3,	/* enter capture delay frame num */
	.pre_delay_frame = 3,	/* enter preview delay frame num */
	.video_delay_frame = 3,	/* enter video delay frame num */
	.hs_video_delay_frame = 3,	/* enter high speed video  delay frame num */
	.slim_video_delay_frame = 3,	/* enter slim video delay frame num */
	.custom1_delay_frame = 2,
	.custom2_delay_frame = 2,
	.custom3_delay_frame = 2,
	.custom4_delay_frame = 2,
	.custom5_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_8MA,	/* mclk driving current */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,	/* sensor_interface_type */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode = 1,	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb,	/* sensor output first pixel color */
	.mclk = 24,		/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_4_LANE,	/* mipi lane num */
	.i2c_addr_table = {0x5a, /*0x20, */ 0xff},	/* record sensor support all write id addr,
							 * only supprt 4must end with 0xff
							 */
	.i2c_speed = 300,	/* i2c read/write speed */
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,	/* IMGSENSOR_MODE enum value,record current sensor mode,
						 * such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
						 */
	.shutter = 0x200,	/* current shutter */
	.gain = 0x200,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 0,	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.autoflicker_en = KAL_FALSE,	/* auto flicker enable: KAL_FALSE for disable auto flicker,
					 * KAL_TRUE for enable auto flicker
					 */
	.test_pattern = KAL_FALSE,	/* test pattern mode or not.
					 * KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
					 */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,	/* current scenario id */
	.ihdr_en = KAL_FALSE,	/* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0,	/* record current sensor's i2c write id */
};


/* Sensor output window information*/
static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] = {
	{4208, 3120, 0, 0, 4208, 3120, 2104, 1560, 0, 0, 2104, 1560, 0, 0, 2104, 1560},	/* Preview */
	{4208, 3120, 0, 0, 4208, 3120, 4208, 3120, 0, 0, 4208, 3120, 0, 0, 4208, 3120},	/* capture */
	{4208, 3120, 0, 0, 4208, 3120, 4208, 3120, 0, 0, 4208, 3120, 0, 0, 4208, 3120},	/* video */
	{4208, 3120, 184, 120, 3840, 2880, 640, 480, 0, 0, 640, 480, 0, 0, 640, 480},	/* hight speed video */
	{4208, 3120, 184, 480, 3840, 2160, 1280, 720, 0, 0, 1280, 720, 0, 0, 1280, 720},	/* slim video */
	{4192, 3104, 0, 0, 4192, 3104, 2096, 1552, 0000, 0000, 2096, 1552, 0, 0, 2096, 1552},	/* Custom1 */
	{4192, 3104, 0, 0, 4192, 3104, 2096, 1552, 0000, 0000, 2096, 1552, 0, 0, 2096, 1552},	/* Custom2 */
	{4192, 3104, 0, 0, 4192, 3104, 2096, 1552, 0000, 0000, 2096, 1552, 0, 0, 2096, 1552},	/* Custom3 */
	{4192, 3104, 0, 0, 4192, 3104, 2096, 1552, 0000, 0000, 2096, 1552, 0, 0, 2096, 1552},	/* Custom4 */
	{4192, 3104, 0, 0, 4192, 3104, 2096, 1552, 0000, 0000, 2096, 1552, 0, 0, 2096, 1552},	/* Custom5 */
};

static SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 28,
	.i4OffsetY = 31,
	.i4PitchX = 64,
	.i4PitchY = 64,
	.i4PairNum = 16,
	.i4SubBlkW = 16,
	.i4SubBlkH = 16,
	.i4PosL = {
		{28, 31}, {80, 31}, {44, 35}, {64, 35},
		{32, 51}, {76, 51}, {48, 55}, {60, 55},
		{48, 63}, {60, 63}, {32, 67}, {76, 67},
		{44, 83}, {64, 83}, {28, 87}, {80, 87}
	},
	.i4PosR = {
		{28, 35}, {80, 35}, {44, 39}, {64, 39},
		{32, 47}, {76, 47}, {48, 51}, {60, 51},
		{48, 67}, {60, 67}, {32, 71}, {76, 71},
		{44, 79}, {64, 79}, {28, 83}, {80, 83}
	},

};

static kal_uint16 read_cmos_sensor_byte(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	kdSetI2CSpeed(imgsensor_info.i2c_speed);	/* Add this func to set i2c speed by each sensor */
	iReadRegI2C(pu_send_cmd, 2, (u8 *) &get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	kdSetI2CSpeed(imgsensor_info.i2c_speed);	/* Add this func to set i2c speed by each sensor */
	iReadRegI2C(pu_send_cmd, 2, (u8 *) &get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor_byte(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = { (char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	kdSetI2CSpeed(imgsensor_info.i2c_speed);	/* Add this func to set i2c speed by each sensor */
	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para >> 8), (char)(para & 0xFF) };

	kdSetI2CSpeed(imgsensor_info.i2c_speed);	/* Add this func to set i2c speed by each sensor */
	iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n", imgsensor.dummy_line, imgsensor.dummy_pixel);
	/*
	 * you can set dummy by imgsensor.dummy_line and imgsensor.dummy_pixel,
	 * or you can set dummy by imgsensor.frame_length and imgsensor.line_length
	 */
	write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
	write_cmos_sensor(0x0342, imgsensor.line_length & 0xFFFF);
}				/*      set_dummy  */


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	/* kal_int16 dummy_line; */
	kal_uint32 frame_length = imgsensor.frame_length;
	/* unsigned long flags; */

	LOG_INF("framerate = %d, min framelength should enable(%d)\n", framerate,
		min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length)
		? frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	/* dummy_line = frame_length - imgsensor.min_frame_length; */
	/* if (dummy_line < 0) */
	/* imgsensor.dummy_line = 0; */
	/* else */
	/* imgsensor.dummy_line = dummy_line; */
	/* imgsensor.frame_length = frame_length + imgsensor.dummy_line; */
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}				/*      set_max_framerate  */

#if 0
static void write_shutter(kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;
	/* kal_uint32 frame_length = 0; */

	/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
	/* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */

	/* OV Recommend Solution */
	/* if shutter bigger than frame_length, should extend frame length first */
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
		? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
	}

	/* Update Shutter */
	write_cmos_sensor(0x0202, (shutter) & 0xFFFF);
	LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);

	/* LOG_INF("frame_length = %d ", frame_length); */

}				/*      write_shutter  */
#endif


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
	kal_uint16 realtime_fps = 0;
	/* kal_uint32 frame_length = 0; */
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/* write_shutter(shutter); */
	/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
	/* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */

	/* OV Recommend Solution */
	/* if shutter bigger than frame_length, should extend frame length first */
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
		? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
	}

	/* Update Shutter */
	write_cmos_sensor(0X0202, shutter & 0xFFFF);
	LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);

}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0000;
	/* gain = 64 = 1x real gain. */
	reg_gain = gain / 2;
	/* reg_gain = reg_gain & 0xFFFF; */
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

	LOG_INF("%s %d\n", gain, __func__);
	/* gain = 64 = 1x real gain. */
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

	write_cmos_sensor(0x0204, (reg_gain & 0xFFFF));
	return gain;
}				/*      set_gain  */

/* ihdr_write_shutter_gain not support for s5k3l8 */
static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
	if (imgsensor.ihdr_en) {

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
	spin_lock(&imgsensor_drv_lock);
	imgsensor.mirror = image_mirror;
	spin_unlock(&imgsensor_drv_lock);
	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor_byte(0x0101, 0X00);	/* GR */
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor_byte(0x0101, 0X01);	/* R */
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor_byte(0x0101, 0X02);	/* B */
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor_byte(0x0101, 0X03);	/* GB */
		break;
	default:
		LOG_INF("Error image_mirror setting\n");
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
}				/*      night_mode      */

static void sensor_set_sync_mode(void)
{
	LOG_INF("E\n");

	write_cmos_sensor(0x0100, 0x0000);

	if (g_sync_mode == SENSOR_MASTER_SYNC_MODE) {
		LOG_INF("set to master mode\n");
		/* master mode */
		write_cmos_sensor(0x6028, 0x4000);
		write_cmos_sensor(0x30C0, 0x0300); /* Vsync out GPIO3 */
	} else if (g_sync_mode == SENSOR_SLAVE_SYNC_MODE) {
		LOG_INF("set to slave mode\n");
		/* slave mode */
		write_cmos_sensor(0x6028, 0x2000);
		write_cmos_sensor(0x602A, 0x4908);
		write_cmos_sensor(0x6F12, 0x0100);
		write_cmos_sensor(0x602A, 0x490A);
		write_cmos_sensor(0x6F12, 0x0000);
		write_cmos_sensor(0x6F12, 0xFFF3);
		write_cmos_sensor(0x602A, 0x4910);
		write_cmos_sensor(0x6F12, 0x0000);
		write_cmos_sensor(0x602A, 0x4914);
		write_cmos_sensor(0x6F12, 0x0000);
		write_cmos_sensor(0x6F12, 0x0002);
		write_cmos_sensor(0x602A, 0x4902);
		write_cmos_sensor(0x6F12, 0x4000);

		write_cmos_sensor(0x6028, 0x4000);
		/* write_cmos_sensor(0x30C0, 0x0300); */
		write_cmos_sensor(0x30BE, 0x0004); /* Vsync in GPIO4 */
		write_cmos_sensor(0x30C2, 0x0101);
		write_cmos_sensor(0x3814, 0x0001);
		write_cmos_sensor(0x3816, 0x0000);
		write_cmos_sensor(0x30C4, 0x0100);
		write_cmos_sensor(0x3812, 0x0002);
	} else {
		LOG_INF("set to no sync mode\n");
	}

	write_cmos_sensor(0x0100, 0x0100);
}

static void sensor_init(void)
{
	LOG_INF("E\n");

	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x6214, 0xFFFF);
	write_cmos_sensor(0x6216, 0xFFFF);
	write_cmos_sensor(0x6218, 0x0000);
	write_cmos_sensor(0x621A, 0x0000);
	/*Swpage*/
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x2450);
	write_cmos_sensor(0x6F12, 0x0448);
	write_cmos_sensor(0x6F12, 0x0349);
	write_cmos_sensor(0x6F12, 0x0160);
	write_cmos_sensor(0x6F12, 0xC26A);
	write_cmos_sensor(0x6F12, 0x511A);
	write_cmos_sensor(0x6F12, 0x8180);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x28B9);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x27F4);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x16C0);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x10B5);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x6BF9);
	write_cmos_sensor(0x6F12, 0xA248);
	write_cmos_sensor(0x6F12, 0x4078);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x6F12, 0x0AD0);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x6AF9);
	write_cmos_sensor(0x6F12, 0xA049);
	write_cmos_sensor(0x6F12, 0xB1F8);
	write_cmos_sensor(0x6F12, 0x1403);
	write_cmos_sensor(0x6F12, 0x4200);
	write_cmos_sensor(0x6F12, 0x9F48);
	write_cmos_sensor(0x6F12, 0x4282);
	write_cmos_sensor(0x6F12, 0x91F8);
	write_cmos_sensor(0x6F12, 0x9610);
	write_cmos_sensor(0x6F12, 0x4187);
	write_cmos_sensor(0x6F12, 0x10BD);
	write_cmos_sensor(0x6F12, 0x70B5);
	write_cmos_sensor(0x6F12, 0x0446);
	write_cmos_sensor(0x6F12, 0x9C48);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x4068);
	write_cmos_sensor(0x6F12, 0x86B2);
	write_cmos_sensor(0x6F12, 0x050C);
	write_cmos_sensor(0x6F12, 0x3146);
	write_cmos_sensor(0x6F12, 0x2846);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x5AF9);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x5CF9);
	write_cmos_sensor(0x6F12, 0x14F8);
	write_cmos_sensor(0x6F12, 0x680F);
	write_cmos_sensor(0x6F12, 0x6178);
	write_cmos_sensor(0x6F12, 0x40EA);
	write_cmos_sensor(0x6F12, 0x4100);
	write_cmos_sensor(0x6F12, 0x9249);
	write_cmos_sensor(0x6F12, 0xC886);
	write_cmos_sensor(0x6F12, 0x9348);
	write_cmos_sensor(0x6F12, 0x2278);
	write_cmos_sensor(0x6F12, 0x007C);
	write_cmos_sensor(0x6F12, 0x4240);
	write_cmos_sensor(0x6F12, 0x8E48);
	write_cmos_sensor(0x6F12, 0xA230);
	write_cmos_sensor(0x6F12, 0x8378);
	write_cmos_sensor(0x6F12, 0x43EA);
	write_cmos_sensor(0x6F12, 0xC202);
	write_cmos_sensor(0x6F12, 0x0378);
	write_cmos_sensor(0x6F12, 0x4078);
	write_cmos_sensor(0x6F12, 0x9B00);
	write_cmos_sensor(0x6F12, 0x43EA);
	write_cmos_sensor(0x6F12, 0x4000);
	write_cmos_sensor(0x6F12, 0x0243);
	write_cmos_sensor(0x6F12, 0xD0B2);
	write_cmos_sensor(0x6F12, 0x0882);
	write_cmos_sensor(0x6F12, 0x3146);
	write_cmos_sensor(0x6F12, 0x2846);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0x7040);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x38B9);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF041);
	write_cmos_sensor(0x6F12, 0x0546);
	write_cmos_sensor(0x6F12, 0x8548);
	write_cmos_sensor(0x6F12, 0x0E46);
	write_cmos_sensor(0x6F12, 0x8268);
	write_cmos_sensor(0x6F12, 0x140C);
	write_cmos_sensor(0x6F12, 0x97B2);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x2BF9);
	write_cmos_sensor(0x6F12, 0x3146);
	write_cmos_sensor(0x6F12, 0x2846);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x31F9);
	write_cmos_sensor(0x6F12, 0x8046);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x21F9);
	write_cmos_sensor(0x6F12, 0x7D4A);
	write_cmos_sensor(0x6F12, 0xB2F8);
	write_cmos_sensor(0x6F12, 0xD802);
	write_cmos_sensor(0x6F12, 0x0128);
	write_cmos_sensor(0x6F12, 0x03D1);
	write_cmos_sensor(0x6F12, 0x7B48);
	write_cmos_sensor(0x6F12, 0xB0F8);
	write_cmos_sensor(0x6F12, 0xE210);
	write_cmos_sensor(0x6F12, 0x11B1);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0xF081);
	write_cmos_sensor(0x6F12, 0xD0F8);
	write_cmos_sensor(0x6F12, 0xEC00);
	write_cmos_sensor(0x6F12, 0x784B);
	write_cmos_sensor(0x6F12, 0x05FB);
	write_cmos_sensor(0x6F12, 0x0601);
	write_cmos_sensor(0x6F12, 0xD2F8);
	write_cmos_sensor(0x6F12, 0xEC02);
	write_cmos_sensor(0x6F12, 0xB3F9);
	write_cmos_sensor(0x6F12, 0x0220);
	write_cmos_sensor(0x6F12, 0x8C1A);
	write_cmos_sensor(0x6F12, 0xA042);
	write_cmos_sensor(0x6F12, 0x01DD);
	write_cmos_sensor(0x6F12, 0x8842);
	write_cmos_sensor(0x6F12, 0x03DD);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x6F12, 0x03DB);
	write_cmos_sensor(0x6F12, 0x9042);
	write_cmos_sensor(0x6F12, 0x01DA);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0x00E0);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x1880);
	write_cmos_sensor(0x6F12, 0x4046);
	write_cmos_sensor(0x6F12, 0xE5E7);
	write_cmos_sensor(0x6F12, 0x6A4A);
	write_cmos_sensor(0x6F12, 0x6C49);
	write_cmos_sensor(0x6F12, 0xB2F8);
	write_cmos_sensor(0x6F12, 0x2401);
	write_cmos_sensor(0x6F12, 0x0883);
	write_cmos_sensor(0x6F12, 0x1378);
	write_cmos_sensor(0x6F12, 0x23B1);
	write_cmos_sensor(0x6F12, 0x92F8);
	write_cmos_sensor(0x6F12, 0x2231);
	write_cmos_sensor(0x6F12, 0xA0EB);
	write_cmos_sensor(0x6F12, 0x4300);
	write_cmos_sensor(0x6F12, 0x0883);
	write_cmos_sensor(0x6F12, 0x6048);
	write_cmos_sensor(0x6F12, 0x408C);
	write_cmos_sensor(0x6F12, 0x4883);
	write_cmos_sensor(0x6F12, 0xD27A);
	write_cmos_sensor(0x6F12, 0x002A);
	write_cmos_sensor(0x6F12, 0x05D1);
	write_cmos_sensor(0x6F12, 0x654A);
	write_cmos_sensor(0x6F12, 0x5288);
	write_cmos_sensor(0x6F12, 0x002A);
	write_cmos_sensor(0x6F12, 0x01D0);
	write_cmos_sensor(0x6F12, 0x801C);
	write_cmos_sensor(0x6F12, 0x4883);
	write_cmos_sensor(0x6F12, 0x7047);
	write_cmos_sensor(0x6F12, 0x70B5);
	write_cmos_sensor(0x6F12, 0x0646);
	write_cmos_sensor(0x6F12, 0x0D46);
	write_cmos_sensor(0x6F12, 0x1446);
	write_cmos_sensor(0x6F12, 0xFFF7);
	write_cmos_sensor(0x6F12, 0xE1FF);
	write_cmos_sensor(0x6F12, 0x5D49);
	write_cmos_sensor(0x6F12, 0x087A);
	write_cmos_sensor(0x6F12, 0x20B9);
	write_cmos_sensor(0x6F12, 0x5E48);
	write_cmos_sensor(0x6F12, 0x0068);
	write_cmos_sensor(0x6F12, 0x90F8);
	write_cmos_sensor(0x6F12, 0xC200);
	write_cmos_sensor(0x6F12, 0x90B1);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0xC883);
	write_cmos_sensor(0x6F12, 0x8889);
	write_cmos_sensor(0x6F12, 0x2044);
	write_cmos_sensor(0x6F12, 0x80B2);
	write_cmos_sensor(0x6F12, 0x8883);
	write_cmos_sensor(0x6F12, 0xA842);
	write_cmos_sensor(0x6F12, 0x07D3);
	write_cmos_sensor(0x6F12, 0x584A);
	write_cmos_sensor(0x6F12, 0xB1F9);
	write_cmos_sensor(0x6F12, 0x1430);
	write_cmos_sensor(0x6F12, 0xD288);
	write_cmos_sensor(0x6F12, 0x1A44);
	write_cmos_sensor(0x6F12, 0x3244);
	write_cmos_sensor(0x6F12, 0x8242);
	write_cmos_sensor(0x6F12, 0x04D9);
	write_cmos_sensor(0x6F12, 0x2044);
	write_cmos_sensor(0x6F12, 0x0222);
	write_cmos_sensor(0x6F12, 0x02E0);
	write_cmos_sensor(0x6F12, 0x2846);
	write_cmos_sensor(0x6F12, 0x70BD);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0xCA83);
	write_cmos_sensor(0x6F12, 0x70BD);
	write_cmos_sensor(0x6F12, 0x10B5);
	write_cmos_sensor(0x6F12, 0x0446);
	write_cmos_sensor(0x6F12, 0x5048);
	write_cmos_sensor(0x6F12, 0xB0F8);
	write_cmos_sensor(0x6F12, 0x1421);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0xFFF7);
	write_cmos_sensor(0x6F12, 0xD2FF);
	write_cmos_sensor(0x6F12, 0x211D);
	write_cmos_sensor(0x6F12, 0x8142);
	write_cmos_sensor(0x6F12, 0x02D2);
	write_cmos_sensor(0x6F12, 0x001B);
	write_cmos_sensor(0x6F12, 0x001F);
	write_cmos_sensor(0x6F12, 0x00E0);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x4849);
	write_cmos_sensor(0x6F12, 0x424A);
	write_cmos_sensor(0x6F12, 0x0968);
	write_cmos_sensor(0x6F12, 0x936B);
	write_cmos_sensor(0x6F12, 0xB2F8);
	write_cmos_sensor(0x6F12, 0x0821);
	write_cmos_sensor(0x6F12, 0x898F);
	write_cmos_sensor(0x6F12, 0x5B09);
	write_cmos_sensor(0x6F12, 0x5943);
	write_cmos_sensor(0x6F12, 0x4FF4);
	write_cmos_sensor(0x6F12, 0x7A73);
	write_cmos_sensor(0x6F12, 0x5A43);
	write_cmos_sensor(0x6F12, 0x5209);
	write_cmos_sensor(0x6F12, 0xB1FB);
	write_cmos_sensor(0x6F12, 0xF2F1);
	write_cmos_sensor(0x6F12, 0x8842);
	write_cmos_sensor(0x6F12, 0x00D8);
	write_cmos_sensor(0x6F12, 0x0846);
	write_cmos_sensor(0x6F12, 0x4249);
	write_cmos_sensor(0x6F12, 0x0880);
	write_cmos_sensor(0x6F12, 0x10BD);
	write_cmos_sensor(0x6F12, 0x0821);
	write_cmos_sensor(0x6F12, 0x0220);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xA5B8);
	write_cmos_sensor(0x6F12, 0x3F4A);
	write_cmos_sensor(0x6F12, 0x02EB);
	write_cmos_sensor(0x6F12, 0x8000);
	write_cmos_sensor(0x6F12, 0x028E);
	write_cmos_sensor(0x6F12, 0x0A40);
	write_cmos_sensor(0x6F12, 0x0286);
	write_cmos_sensor(0x6F12, 0x7047);
	write_cmos_sensor(0x6F12, 0x70B5);
	write_cmos_sensor(0x6F12, 0x3548);
	write_cmos_sensor(0x6F12, 0x007A);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x6F12, 0x19D0);
	write_cmos_sensor(0x6F12, 0x3548);
	write_cmos_sensor(0x6F12, 0x0068);
	write_cmos_sensor(0x6F12, 0x90F8);
	write_cmos_sensor(0x6F12, 0xC200);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x6F12, 0x13D0);
	write_cmos_sensor(0x6F12, 0x2D4C);
	write_cmos_sensor(0x6F12, 0x0125);
	write_cmos_sensor(0x6F12, 0x5E34);
	write_cmos_sensor(0x6F12, 0x2078);
	write_cmos_sensor(0x6F12, 0x401E);
	write_cmos_sensor(0x6F12, 0x05FA);
	write_cmos_sensor(0x6F12, 0x00F1);
	write_cmos_sensor(0x6F12, 0x0220);
	write_cmos_sensor(0x6F12, 0xFFF7);
	write_cmos_sensor(0x6F12, 0xE4FF);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x8BF8);
	write_cmos_sensor(0x6F12, 0x3049);
	write_cmos_sensor(0x6F12, 0x488F);
	write_cmos_sensor(0x6F12, 0x2278);
	write_cmos_sensor(0x6F12, 0x521E);
	write_cmos_sensor(0x6F12, 0x9540);
	write_cmos_sensor(0x6F12, 0x2843);
	write_cmos_sensor(0x6F12, 0x4887);
	write_cmos_sensor(0x6F12, 0x70BD);
	write_cmos_sensor(0x6F12, 0x10B5);
	write_cmos_sensor(0x6F12, 0x0723);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x5F01);
	write_cmos_sensor(0x6F12, 0x0220);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x80F8);
	write_cmos_sensor(0x6F12, 0xFFF7);
	write_cmos_sensor(0x6F12, 0xD6FF);
	write_cmos_sensor(0x6F12, 0x2148);
	write_cmos_sensor(0x6F12, 0xC08A);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x7FF8);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x81F8);
	write_cmos_sensor(0x6F12, 0x1A48);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0x1040);
	write_cmos_sensor(0x6F12, 0x7030);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x80B8);
	write_cmos_sensor(0x6F12, 0x10B5);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x4721);
	write_cmos_sensor(0x6F12, 0x1F48);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x7EF8);
	write_cmos_sensor(0x6F12, 0x134C);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x2D21);
	write_cmos_sensor(0x6F12, 0x2060);
	write_cmos_sensor(0x6F12, 0x1C48);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x76F8);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0xE311);
	write_cmos_sensor(0x6F12, 0x6060);
	write_cmos_sensor(0x6F12, 0x1A48);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x6FF8);
	write_cmos_sensor(0x6F12, 0xA060);
	write_cmos_sensor(0x6F12, 0x0F49);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0x0246);
	write_cmos_sensor(0x6F12, 0x0880);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x6501);
	write_cmos_sensor(0x6F12, 0x1648);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x65F8);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x0F11);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0x1040);
	write_cmos_sensor(0x6F12, 0x1348);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x5DB8);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x0550);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x0C60);
	write_cmos_sensor(0x6F12, 0x4000);
	write_cmos_sensor(0x6F12, 0xD000);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x27E0);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x16F0);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x1B70);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x0B50);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x4900);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x1490);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x0500);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x02B0);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x1520);
	write_cmos_sensor(0x6F12, 0x4000);
	write_cmos_sensor(0x6F12, 0xF000);
	write_cmos_sensor(0x6F12, 0x4000);
	write_cmos_sensor(0x6F12, 0x7000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x2221);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x2249);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x4073);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x879F);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x8669);
	write_cmos_sensor(0x6F12, 0x42F2);
	write_cmos_sensor(0x6F12, 0x351C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x42F2);
	write_cmos_sensor(0x6F12, 0xE11C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x40F2);
	write_cmos_sensor(0x6F12, 0x077C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x42F2);
	write_cmos_sensor(0x6F12, 0x492C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x44F2);
	write_cmos_sensor(0x6F12, 0x730C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x40F2);
	write_cmos_sensor(0x6F12, 0x851C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x40F2);
	write_cmos_sensor(0x6F12, 0xFF0C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x40F2);
	write_cmos_sensor(0x6F12, 0xEF1C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x48F2);
	write_cmos_sensor(0x6F12, 0xB34C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x48F2);
	write_cmos_sensor(0x6F12, 0xBB6C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x45F6);
	write_cmos_sensor(0x6F12, 0xA15C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4BF2);
	write_cmos_sensor(0x6F12, 0x453C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x30C8);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x004B);
	write_cmos_sensor(0x602A, 0x4900);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	/*Global*/
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x1082);
	write_cmos_sensor(0x6F12, 0x8010);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x31CE, 0x0001);
	write_cmos_sensor(0x0200, 0x00C6);
	write_cmos_sensor(0x3734, 0x0010);
	write_cmos_sensor(0x3736, 0x0001);
	write_cmos_sensor(0x3738, 0x0001);
	write_cmos_sensor(0x37CC, 0x0001);
	write_cmos_sensor(0x3744, 0x0100);
	write_cmos_sensor(0x3762, 0x0105);
	write_cmos_sensor(0x3764, 0x0105);
	write_cmos_sensor(0x376A, 0x00F0);
	write_cmos_sensor(0x344A, 0x000F);
	write_cmos_sensor(0x344C, 0x003D);
	write_cmos_sensor(0xF460, 0x0030);
	write_cmos_sensor(0xF414, 0x24C2);
	write_cmos_sensor(0xF416, 0x0183);
	write_cmos_sensor(0xF468, 0x4005);
	write_cmos_sensor(0x3424, 0x0A07);
	write_cmos_sensor(0x3426, 0x0F07);
	write_cmos_sensor(0x3428, 0x0F07);
	write_cmos_sensor(0x341E, 0x0804);
	write_cmos_sensor(0x3420, 0x0C0C);
	write_cmos_sensor(0x3422, 0x2D2D);
	write_cmos_sensor(0xF462, 0x003A);
	write_cmos_sensor(0x3450, 0x0010);
	write_cmos_sensor(0x3452, 0x0010);
	write_cmos_sensor(0xF446, 0x0020);
	write_cmos_sensor(0xF44E, 0x000C);
	write_cmos_sensor(0x31FA, 0x0007);
	write_cmos_sensor(0x31FC, 0x0161);
	write_cmos_sensor(0x31FE, 0x0009);
	write_cmos_sensor(0x3200, 0x000C);
	write_cmos_sensor(0x3202, 0x007F);
	write_cmos_sensor(0x3204, 0x00A2);
	write_cmos_sensor(0x3206, 0x007D);
	write_cmos_sensor(0x3208, 0x00A4);
	write_cmos_sensor(0x3334, 0x00A7);
	write_cmos_sensor(0x3336, 0x00A5);
	write_cmos_sensor(0x3338, 0x0033);
	write_cmos_sensor(0x333A, 0x0006);
	write_cmos_sensor(0x333C, 0x009F);
	write_cmos_sensor(0x333E, 0x008C);
	write_cmos_sensor(0x3340, 0x002D);
	write_cmos_sensor(0x3342, 0x000A);
	write_cmos_sensor(0x3344, 0x002F);
	write_cmos_sensor(0x3346, 0x0008);
	write_cmos_sensor(0x3348, 0x009F);
	write_cmos_sensor(0x334A, 0x008C);
	write_cmos_sensor(0x334C, 0x002D);
	write_cmos_sensor(0x334E, 0x000A);
	write_cmos_sensor(0x3350, 0x000A);
	write_cmos_sensor(0x320A, 0x007B);
	write_cmos_sensor(0x320C, 0x0161);
	write_cmos_sensor(0x320E, 0x007F);
	write_cmos_sensor(0x3210, 0x015F);
	write_cmos_sensor(0x3212, 0x007B);
	write_cmos_sensor(0x3214, 0x00B0);
	write_cmos_sensor(0x3216, 0x0009);
	write_cmos_sensor(0x3218, 0x0038);
	write_cmos_sensor(0x321A, 0x0009);
	write_cmos_sensor(0x321C, 0x0031);
	write_cmos_sensor(0x321E, 0x0009);
	write_cmos_sensor(0x3220, 0x0038);
	write_cmos_sensor(0x3222, 0x0009);
	write_cmos_sensor(0x3224, 0x007B);
	write_cmos_sensor(0x3226, 0x0001);
	write_cmos_sensor(0x3228, 0x0010);
	write_cmos_sensor(0x322A, 0x00A2);
	write_cmos_sensor(0x322C, 0x00B1);
	write_cmos_sensor(0x322E, 0x0002);
	write_cmos_sensor(0x3230, 0x015D);
	write_cmos_sensor(0x3232, 0x0001);
	write_cmos_sensor(0x3234, 0x015D);
	write_cmos_sensor(0x3236, 0x0001);
	write_cmos_sensor(0x3238, 0x000B);
	write_cmos_sensor(0x323A, 0x0016);
	write_cmos_sensor(0x323C, 0x000D);
	write_cmos_sensor(0x323E, 0x001C);
	write_cmos_sensor(0x3240, 0x000D);
	write_cmos_sensor(0x3242, 0x0054);
	write_cmos_sensor(0x3244, 0x007B);
	write_cmos_sensor(0x3246, 0x00CC);
	write_cmos_sensor(0x3248, 0x015D);
	write_cmos_sensor(0x324A, 0x007E);
	write_cmos_sensor(0x324C, 0x0095);
	write_cmos_sensor(0x324E, 0x0085);
	write_cmos_sensor(0x3250, 0x009D);
	write_cmos_sensor(0x3252, 0x008D);
	write_cmos_sensor(0x3254, 0x009D);
	write_cmos_sensor(0x3256, 0x007E);
	write_cmos_sensor(0x3258, 0x0080);
	write_cmos_sensor(0x325A, 0x0001);
	write_cmos_sensor(0x325C, 0x0005);
	write_cmos_sensor(0x325E, 0x0085);
	write_cmos_sensor(0x3260, 0x009D);
	write_cmos_sensor(0x3262, 0x0001);
	write_cmos_sensor(0x3264, 0x0005);
	write_cmos_sensor(0x3266, 0x007E);
	write_cmos_sensor(0x3268, 0x0080);
	write_cmos_sensor(0x326A, 0x0053);
	write_cmos_sensor(0x326C, 0x007D);
	write_cmos_sensor(0x326E, 0x00CB);
	write_cmos_sensor(0x3270, 0x015E);
	write_cmos_sensor(0x3272, 0x0001);
	write_cmos_sensor(0x3274, 0x0005);
	write_cmos_sensor(0x3276, 0x0009);
	write_cmos_sensor(0x3278, 0x000C);
	write_cmos_sensor(0x327A, 0x007E);
	write_cmos_sensor(0x327C, 0x0098);
	write_cmos_sensor(0x327E, 0x0009);
	write_cmos_sensor(0x3280, 0x000C);
	write_cmos_sensor(0x3282, 0x007E);
	write_cmos_sensor(0x3284, 0x0080);
	write_cmos_sensor(0x3286, 0x0044);
	write_cmos_sensor(0x3288, 0x0163);
	write_cmos_sensor(0x328A, 0x0045);
	write_cmos_sensor(0x328C, 0x0047);
	write_cmos_sensor(0x328E, 0x007D);
	write_cmos_sensor(0x3290, 0x0080);
	write_cmos_sensor(0x3292, 0x015F);
	write_cmos_sensor(0x3294, 0x0162);
	write_cmos_sensor(0x3296, 0x007D);
	write_cmos_sensor(0x3298, 0x0000);
	write_cmos_sensor(0x329A, 0x0000);
	write_cmos_sensor(0x329C, 0x0000);
	write_cmos_sensor(0x329E, 0x0000);
	write_cmos_sensor(0x32A0, 0x0008);
	write_cmos_sensor(0x32A2, 0x0010);
	write_cmos_sensor(0x32A4, 0x0018);
	write_cmos_sensor(0x32A6, 0x0020);
	write_cmos_sensor(0x32A8, 0x0000);
	write_cmos_sensor(0x32AA, 0x0008);
	write_cmos_sensor(0x32AC, 0x0010);
	write_cmos_sensor(0x32AE, 0x0018);
	write_cmos_sensor(0x32B0, 0x0020);
	write_cmos_sensor(0x32B2, 0x0020);
	write_cmos_sensor(0x32B4, 0x0020);
	write_cmos_sensor(0x32B6, 0x0020);
	write_cmos_sensor(0x32B8, 0x0000);
	write_cmos_sensor(0x32BA, 0x0000);
	write_cmos_sensor(0x32BC, 0x0000);
	write_cmos_sensor(0x32BE, 0x0000);
	write_cmos_sensor(0x32C0, 0x0000);
	write_cmos_sensor(0x32C2, 0x0000);
	write_cmos_sensor(0x32C4, 0x0000);
	write_cmos_sensor(0x32C6, 0x0000);
	write_cmos_sensor(0x32C8, 0x0000);
	write_cmos_sensor(0x32CA, 0x0000);
	write_cmos_sensor(0x32CC, 0x0000);
	write_cmos_sensor(0x32CE, 0x0000);
	write_cmos_sensor(0x32D0, 0x0000);
	write_cmos_sensor(0x32D2, 0x0000);
	write_cmos_sensor(0x32D4, 0x0000);
	write_cmos_sensor(0x32D6, 0x0000);
	write_cmos_sensor(0x32D8, 0x0000);
	write_cmos_sensor(0x32DA, 0x0000);
	write_cmos_sensor(0x32DC, 0x0000);
	write_cmos_sensor(0x32DE, 0x0000);
	write_cmos_sensor(0x32E0, 0x0000);
	write_cmos_sensor(0x32E2, 0x0000);
	write_cmos_sensor(0x32E4, 0x0000);
	write_cmos_sensor(0x32E6, 0x0000);
	write_cmos_sensor(0x32E8, 0x0000);
	write_cmos_sensor(0x32EA, 0x0000);
	write_cmos_sensor(0x32EC, 0x0000);
	write_cmos_sensor(0x32EE, 0x0000);
	write_cmos_sensor(0x32F0, 0x0000);
	write_cmos_sensor(0x32F2, 0x0000);
	write_cmos_sensor(0x32F4, 0x000A);
	write_cmos_sensor(0x32F6, 0x0002);
	write_cmos_sensor(0x32F8, 0x0008);
	write_cmos_sensor(0x32FA, 0x0010);
	write_cmos_sensor(0x32FC, 0x0020);
	write_cmos_sensor(0x32FE, 0x0028);
	write_cmos_sensor(0x3300, 0x0038);
	write_cmos_sensor(0x3302, 0x0040);
	write_cmos_sensor(0x3304, 0x0050);
	write_cmos_sensor(0x3306, 0x0058);
	write_cmos_sensor(0x3308, 0x0068);
	write_cmos_sensor(0x330A, 0x0070);
	write_cmos_sensor(0x330C, 0x0080);
	write_cmos_sensor(0x330E, 0x0088);
	write_cmos_sensor(0x3310, 0x0098);
	write_cmos_sensor(0x3312, 0x00A0);
	write_cmos_sensor(0x3314, 0x00B0);
	write_cmos_sensor(0x3316, 0x00B8);
	write_cmos_sensor(0x3318, 0x00C8);
	write_cmos_sensor(0x331A, 0x00D0);
	write_cmos_sensor(0x331C, 0x00E0);
	write_cmos_sensor(0x331E, 0x00E8);
	write_cmos_sensor(0x3320, 0x0017);
	write_cmos_sensor(0x3322, 0x002F);
	write_cmos_sensor(0x3324, 0x0047);
	write_cmos_sensor(0x3326, 0x005F);
	write_cmos_sensor(0x3328, 0x0077);
	write_cmos_sensor(0x332A, 0x008F);
	write_cmos_sensor(0x332C, 0x00A7);
	write_cmos_sensor(0x332E, 0x00BF);
	write_cmos_sensor(0x3330, 0x00D7);
	write_cmos_sensor(0x3332, 0x00EF);
	write_cmos_sensor(0x3352, 0x00A5);
	write_cmos_sensor(0x3354, 0x00AF);
	write_cmos_sensor(0x3356, 0x0187);
	write_cmos_sensor(0x3358, 0x0000);
	write_cmos_sensor(0x335A, 0x009E);
	write_cmos_sensor(0x335C, 0x016B);
	write_cmos_sensor(0x335E, 0x0015);
	write_cmos_sensor(0x3360, 0x00A5);
	write_cmos_sensor(0x3362, 0x00AF);
	write_cmos_sensor(0x3364, 0x01FB);
	write_cmos_sensor(0x3366, 0x0000);
	write_cmos_sensor(0x3368, 0x009E);
	write_cmos_sensor(0x336A, 0x016B);
	write_cmos_sensor(0x336C, 0x0015);
	write_cmos_sensor(0x336E, 0x00A5);
	write_cmos_sensor(0x3370, 0x00A6);
	write_cmos_sensor(0x3372, 0x0187);
	write_cmos_sensor(0x3374, 0x0000);
	write_cmos_sensor(0x3376, 0x009E);
	write_cmos_sensor(0x3378, 0x016B);
	write_cmos_sensor(0x337A, 0x0015);
	write_cmos_sensor(0x337C, 0x00A5);
	write_cmos_sensor(0x337E, 0x00A6);
	write_cmos_sensor(0x3380, 0x01FB);
	write_cmos_sensor(0x3382, 0x0000);
	write_cmos_sensor(0x3384, 0x009E);
	write_cmos_sensor(0x3386, 0x016B);
	write_cmos_sensor(0x3388, 0x0015);
	write_cmos_sensor(0x319A, 0x0005);
	write_cmos_sensor(0x1006, 0x0005);
	write_cmos_sensor(0x3416, 0x0001);
	write_cmos_sensor(0x308C, 0x0008);
	write_cmos_sensor(0x307C, 0x0240);
	write_cmos_sensor(0x375E, 0x0050);
	write_cmos_sensor(0x31CE, 0x0101);
	write_cmos_sensor(0x374E, 0x0007);
	write_cmos_sensor(0x3460, 0x0001);
	write_cmos_sensor(0x3052, 0x0002);
	write_cmos_sensor(0x3058, 0x0100);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x108A);
	write_cmos_sensor(0x6F12, 0x0359);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x1124, 0x4100);
	write_cmos_sensor(0x1126, 0x0000);
	write_cmos_sensor(0x112C, 0x4100);
	write_cmos_sensor(0x112E, 0x0000);
	write_cmos_sensor(0x3442, 0x0100);

	/* control continue mode use B0A0 is better */
#ifdef NONCONTINUEMODE
	write_cmos_sensor_byte(0xB0A0, 0x7C);	/* non continue mode */
#else
	write_cmos_sensor_byte(0xB0A0, 0x7D);	/* continue mode */
#endif

}				/*      sensor_init  */


static void preview_setting(void)
{
	LOG_INF("E\n");
	/*
	 * $MV1[MCLK:24,Width:2104,Height:1560,Format:MIPI_Raw10,mipi_lane:4,
	 * mipi_datarate:1124,pvi_pclk_inverse:0]
	 */
	write_cmos_sensor(0x0100, 0x0000);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x0F74);
	write_cmos_sensor(0x6F12, 0x0040);
	write_cmos_sensor(0x6F12, 0x0040);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0344, 0x0008);
	write_cmos_sensor(0x0346, 0x0008);
	write_cmos_sensor(0x0348, 0x1077);
	write_cmos_sensor(0x034A, 0x0C37);
	write_cmos_sensor(0x034C, 0x0838);
	write_cmos_sensor(0x034E, 0x0618);
	write_cmos_sensor(0x0900, 0x0112);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0003);
	write_cmos_sensor(0x0400, 0x0001);
	write_cmos_sensor(0x0404, 0x0020);
	write_cmos_sensor(0x0114, 0x0300);
	write_cmos_sensor(0x0110, 0x0002);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0304, 0x0006);
	write_cmos_sensor(0x0306, 0x00AF);
	write_cmos_sensor(0x0302, 0x0001);
	write_cmos_sensor(0x0300, 0x0005);
	write_cmos_sensor(0x030C, 0x0006);
	write_cmos_sensor(0x030E, 0x0119);
	write_cmos_sensor(0x030A, 0x0001);
	write_cmos_sensor(0x0308, 0x0008);
#ifdef NONCONTINUEMODE
	write_cmos_sensor(0x0342, 0x1720);
	LOG_INF("===NONCONTINUEMODE===");
#else
	write_cmos_sensor(0x0342, 0x16B0);
#endif
	write_cmos_sensor(0x0340, 0x0C98);
	write_cmos_sensor(0x0202, 0x0200);
	write_cmos_sensor(0x0200, 0x00C6);
	write_cmos_sensor(0x0B04, 0x0101);
	write_cmos_sensor(0x0B08, 0x0000);
	write_cmos_sensor(0x0B00, 0x0007);
	write_cmos_sensor(0x316A, 0x00A0);

}				/*      preview_setting  */

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n", currefps);
	if (currefps == 300) {
		/*
		 * $MV1[MCLK:24,Width:4208,Height:3120,Format:MIPI_Raw10,mipi_lane:4,
		 * mipi_datarate:1124,pvi_pclk_inverse:0]
		 */
		write_cmos_sensor(0x0100, 0x0000);
		write_cmos_sensor(0x6028, 0x2000);
		write_cmos_sensor(0x602A, 0x0F74);
		write_cmos_sensor(0x6F12, 0x0040);
		write_cmos_sensor(0x6F12, 0x0040);
		write_cmos_sensor(0x6028, 0x4000);
		write_cmos_sensor(0x0344, 0x0008);
		write_cmos_sensor(0x0346, 0x0008);
		write_cmos_sensor(0x0348, 0x1077);
		write_cmos_sensor(0x034A, 0x0C37);
		write_cmos_sensor(0x034C, 0x1070);
		write_cmos_sensor(0x034E, 0x0C30);
		write_cmos_sensor(0x0900, 0x0011);
		write_cmos_sensor(0x0380, 0x0001);
		write_cmos_sensor(0x0382, 0x0001);
		write_cmos_sensor(0x0384, 0x0001);
		write_cmos_sensor(0x0386, 0x0001);
		write_cmos_sensor(0x0400, 0x0000);
		write_cmos_sensor(0x0404, 0x0010);
		write_cmos_sensor(0x0114, 0x0300);
		write_cmos_sensor(0x0110, 0x0002);
		write_cmos_sensor(0x0136, 0x1800);
		write_cmos_sensor(0x0304, 0x0006);
		write_cmos_sensor(0x0306, 0x00AF);
		write_cmos_sensor(0x0302, 0x0001);
		write_cmos_sensor(0x0300, 0x0005);
		write_cmos_sensor(0x030C, 0x0006);
		write_cmos_sensor(0x030E, 0x0119);
		write_cmos_sensor(0x030A, 0x0001);
		write_cmos_sensor(0x0308, 0x0008);
#ifdef NONCONTINUEMODE
		write_cmos_sensor(0x0342, 0x1720);
#else
		write_cmos_sensor(0x0342, 0x16B0);
#endif
		write_cmos_sensor(0x0340, 0x0C98);
		write_cmos_sensor(0x0202, 0x0200);
		write_cmos_sensor(0x0200, 0x00C6);
		write_cmos_sensor(0x0B04, 0x0101);
		write_cmos_sensor(0x0B08, 0x0000);
		write_cmos_sensor(0x0B00, 0x0007);
		write_cmos_sensor(0x316A, 0x00A0);

	} else if (currefps == 240) {
		LOG_INF("else if (currefps == 240)\n");
		/*
		 * $MV1[MCLK:24,Width:4208,Height:3120,Format:MIPI_Raw10,mipi_lane:4,
		 * mipi_datarate:1124,pvi_pclk_inverse:0]
		 */
		write_cmos_sensor(0x0100, 0x0000);
		write_cmos_sensor(0x6028, 0x2000);
		write_cmos_sensor(0x602A, 0x0F74);
		write_cmos_sensor(0x6F12, 0x0040);
		write_cmos_sensor(0x6F12, 0x0040);
		write_cmos_sensor(0x6028, 0x4000);
		write_cmos_sensor(0x0344, 0x0008);
		write_cmos_sensor(0x0346, 0x0008);
		write_cmos_sensor(0x0348, 0x1077);
		write_cmos_sensor(0x034A, 0x0C37);
		write_cmos_sensor(0x034C, 0x1070);
		write_cmos_sensor(0x034E, 0x0C30);
		write_cmos_sensor(0x0900, 0x0011);
		write_cmos_sensor(0x0380, 0x0001);
		write_cmos_sensor(0x0382, 0x0001);
		write_cmos_sensor(0x0384, 0x0001);
		write_cmos_sensor(0x0386, 0x0001);
		write_cmos_sensor(0x0400, 0x0000);
		write_cmos_sensor(0x0404, 0x0010);
		write_cmos_sensor(0x0114, 0x0300);
		write_cmos_sensor(0x0110, 0x0002);
		write_cmos_sensor(0x0136, 0x1800);
		write_cmos_sensor(0x0304, 0x0006);
		write_cmos_sensor(0x0306, 0x008C);
		write_cmos_sensor(0x0302, 0x0001);
		write_cmos_sensor(0x0300, 0x0005);
		write_cmos_sensor(0x030C, 0x0006);
		write_cmos_sensor(0x030E, 0x0119);
		write_cmos_sensor(0x030A, 0x0001);
		write_cmos_sensor(0x0308, 0x0008);
#ifdef NONCONTINUEMODE
		write_cmos_sensor(0x0342, 0x1720);
#else
		write_cmos_sensor(0x0342, 0x16B0);
#endif
		write_cmos_sensor(0x0340, 0x0C86);
		write_cmos_sensor(0x0202, 0x0200);
		write_cmos_sensor(0x0200, 0x00C6);
		write_cmos_sensor(0x0B04, 0x0101);
		write_cmos_sensor(0x0B08, 0x0000);
		write_cmos_sensor(0x0B00, 0x0007);
		write_cmos_sensor(0x316A, 0x00A0);

	} else if (currefps == 150) {
		/* PIP 15fps settings,相比Full 30fps */
		/* -VT : 560-> 400M */
		/* -Frame length: 3206-> 4589 */
		/* -Linelength: 5808不變 */

		/*
		 * $MV1[MCLK:24,Width:4208,Height:3120,Format:MIPI_Raw10,mipi_lane:4,
		 * mipi_datarate:1124,pvi_pclk_inverse:0]
		 */
		LOG_INF("else if (currefps == 150)\n");
		write_cmos_sensor(0x0100, 0x0000);
		write_cmos_sensor(0x6028, 0x2000);
		write_cmos_sensor(0x602A, 0x0F74);
		write_cmos_sensor(0x6F12, 0x0040);	/* 64 */
		write_cmos_sensor(0x6F12, 0x0040);	/* 64 */
		write_cmos_sensor(0x6028, 0x4000);
		write_cmos_sensor(0x0344, 0x0008);	/* 8 */
		write_cmos_sensor(0x0346, 0x0008);	/* 8 */
		write_cmos_sensor(0x0348, 0x1077);	/* 4215 */
		write_cmos_sensor(0x034A, 0x0C37);	/* 3127 */
		write_cmos_sensor(0x034C, 0x1070);	/* 4208 */
		write_cmos_sensor(0x034E, 0x0C30);	/* 3120 */
		write_cmos_sensor(0x0900, 0x0011);
		write_cmos_sensor(0x0380, 0x0001);
		write_cmos_sensor(0x0382, 0x0001);
		write_cmos_sensor(0x0384, 0x0001);
		write_cmos_sensor(0x0386, 0x0001);
		write_cmos_sensor(0x0400, 0x0000);
		write_cmos_sensor(0x0404, 0x0010);
		write_cmos_sensor(0x0114, 0x0300);
		write_cmos_sensor(0x0110, 0x0002);
		write_cmos_sensor(0x0136, 0x1800);	/* 24MHz */
		write_cmos_sensor(0x0304, 0x0006);	/* 6 */
		write_cmos_sensor(0x0306, 0x007D);	/* 125 */
		write_cmos_sensor(0x0302, 0x0001);	/* 1 */
		write_cmos_sensor(0x0300, 0x0005);	/* 5 */
		write_cmos_sensor(0x030C, 0x0006);	/* 6 */
		write_cmos_sensor(0x030E, 0x00c8);	/* 281 */
		write_cmos_sensor(0x030A, 0x0001);	/* 1 */
		write_cmos_sensor(0x0308, 0x0008);	/* 8 */
#ifdef NONCONTINUEMODE
		write_cmos_sensor(0x0342, 0x1720);
#else
		write_cmos_sensor(0x0342, 0x16B0);
#endif
		write_cmos_sensor(0x0340, 0x11ED);	/* 4589 */
		write_cmos_sensor(0x0202, 0x0200);	/* 512 */
		write_cmos_sensor(0x0200, 0x00C6);	/* 198 */
		write_cmos_sensor(0x0B04, 0x0101);	/* M.BPC_On */
		write_cmos_sensor(0x0B08, 0x0000);	/* D.BPC_Off */
		write_cmos_sensor(0x0B00, 0x0007);	/* LSC_Off */
		write_cmos_sensor(0x316A, 0x00A0);	/* OUTIF threshold */
	} else {		/* default fps =15 */
		/* PIP 15fps settings,相比Full 30fps */
		/* -VT : 560-> 400M */
		/* -Frame length: 3206-> 4589 */
		/* -Linelength: 5808不變 */

		/*
		 * $MV1[MCLK:24,Width:4208,Height:3120,Format:MIPI_Raw10,mipi_lane:4,
		 * mipi_datarate:1124,pvi_pclk_inverse:0]
		 */
		LOG_INF("else  150fps\n");
		write_cmos_sensor_byte(0x0100, 0x00);
		write_cmos_sensor(0x6028, 0x2000);
		write_cmos_sensor(0x602A, 0x0F74);
		write_cmos_sensor(0x6F12, 0x0040);	/* 64 */
		write_cmos_sensor(0x6F12, 0x0040);	/* 64 */
		write_cmos_sensor(0x6028, 0x4000);
		write_cmos_sensor(0x0344, 0x0008);	/* 8 */
		write_cmos_sensor(0x0346, 0x0008);	/* 8 */
		write_cmos_sensor(0x0348, 0x1077);	/* 4215 */
		write_cmos_sensor(0x034A, 0x0C37);	/* 3127 */
		write_cmos_sensor(0x034C, 0x1070);	/* 4208 */
		write_cmos_sensor(0x034E, 0x0C30);	/* 3120 */
		write_cmos_sensor(0x0900, 0x0011);
		write_cmos_sensor(0x0380, 0x0001);
		write_cmos_sensor(0x0382, 0x0001);
		write_cmos_sensor(0x0384, 0x0001);
		write_cmos_sensor(0x0386, 0x0001);
		write_cmos_sensor(0x0400, 0x0000);
		write_cmos_sensor(0x0404, 0x0010);
		write_cmos_sensor(0x0114, 0x0300);
		write_cmos_sensor(0x0110, 0x0002);
		write_cmos_sensor(0x0136, 0x1800);	/* 24MHz */
		write_cmos_sensor(0x0304, 0x0006);	/* 6 */
		write_cmos_sensor(0x0306, 0x007D);	/* 125 */
		write_cmos_sensor(0x0302, 0x0001);	/* 1 */
		write_cmos_sensor(0x0300, 0x0005);	/* 5 */
		write_cmos_sensor(0x030C, 0x0006);	/* 6 */
		write_cmos_sensor(0x030E, 0x00c8);	/* 281 */
		write_cmos_sensor(0x030A, 0x0001);	/* 1 */
		write_cmos_sensor(0x0308, 0x0008);	/* 8 */
#ifdef NONCONTINUEMODE
		write_cmos_sensor(0x0342, 0x1720);
#else
		write_cmos_sensor(0x0342, 0x16B0);
#endif
		write_cmos_sensor(0x0340, 0x11ED);	/* 4589 */
		write_cmos_sensor(0x0202, 0x0200);	/* 512 */
		write_cmos_sensor(0x0200, 0x00C6);	/* 198 */
		write_cmos_sensor(0x0B04, 0x0101);	/* M.BPC_On */
		write_cmos_sensor(0x0B08, 0x0000);	/* D.BPC_Off */
		write_cmos_sensor(0x0B00, 0x0007);	/* LSC_Off */
		write_cmos_sensor(0x316A, 0x00A0);	/* OUTIF threshold */
	}
}

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n", currefps);
	capture_setting(currefps);
}

static void hs_video_setting(void)
{
	LOG_INF("E\n");
	/* $MV1[MCLK:24,Width:640,Height:480,Format:MIPI_Raw10,mipi_lane:4,mipi_datarate:1124,pvi_pclk_inverse:0] */
	write_cmos_sensor(0x0100, 0x0000);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x0F74);
	write_cmos_sensor(0x6F12, 0x0040);
	write_cmos_sensor(0x6F12, 0x0040);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0344, 0x00C0);
	write_cmos_sensor(0x0346, 0x0080);
	write_cmos_sensor(0x0348, 0x0FBF);
	write_cmos_sensor(0x034A, 0x0BBF);
	write_cmos_sensor(0x034C, 0x0280);
	write_cmos_sensor(0x034E, 0x01E0);
	write_cmos_sensor(0x0900, 0x0116);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x000B);
	write_cmos_sensor(0x0400, 0x0001);
	write_cmos_sensor(0x0404, 0x0060);
	write_cmos_sensor(0x0114, 0x0300);
	write_cmos_sensor(0x0110, 0x0002);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0304, 0x0006);
	write_cmos_sensor(0x0306, 0x00AF);
	write_cmos_sensor(0x0302, 0x0001);
	write_cmos_sensor(0x0300, 0x0005);
	write_cmos_sensor(0x030C, 0x0006);
	write_cmos_sensor(0x030E, 0x0119);
	write_cmos_sensor(0x030A, 0x0001);
	write_cmos_sensor(0x0308, 0x0008);
#ifdef NONCONTINUEMODE
	write_cmos_sensor(0x0342, 0x1720);
#else
	write_cmos_sensor(0x0342, 0x16B0);
#endif
	write_cmos_sensor(0x0340, 0x0323);
	write_cmos_sensor(0x0202, 0x0200);
	write_cmos_sensor(0x0200, 0x00C6);
	write_cmos_sensor(0x0B04, 0x0101);
	write_cmos_sensor(0x0B08, 0x0000);
	write_cmos_sensor(0x0B00, 0x0007);
	write_cmos_sensor(0x316A, 0x00A0);

}

static void slim_video_setting(void)
{
	LOG_INF("E\n");
	/* $MV1[MCLK:24,Width:1280,Height:720,Format:MIPI_Raw10,mipi_lane:4,mipi_datarate:1124,pvi_pclk_inverse:0] */
	write_cmos_sensor(0x0100, 0x0000);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x0F74);
	write_cmos_sensor(0x6F12, 0x0040);
	write_cmos_sensor(0x6F12, 0x0040);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0344, 0x00C0);
	write_cmos_sensor(0x0346, 0x01E8);
	write_cmos_sensor(0x0348, 0x0FBF);
	write_cmos_sensor(0x034A, 0x0A57);
	write_cmos_sensor(0x034C, 0x0500);
	write_cmos_sensor(0x034E, 0x02D0);
	write_cmos_sensor(0x0900, 0x0113);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0005);
	write_cmos_sensor(0x0400, 0x0001);
	write_cmos_sensor(0x0404, 0x0030);
	write_cmos_sensor(0x0114, 0x0300);
	write_cmos_sensor(0x0110, 0x0002);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0304, 0x0006);
	write_cmos_sensor(0x0306, 0x00AF);
	write_cmos_sensor(0x0302, 0x0001);
	write_cmos_sensor(0x0300, 0x0005);
	write_cmos_sensor(0x030C, 0x0006);
	write_cmos_sensor(0x030E, 0x0119);
	write_cmos_sensor(0x030A, 0x0001);
	write_cmos_sensor(0x0308, 0x0008);
#ifdef NONCONTINUEMODE
	write_cmos_sensor(0x0342, 0x1720);
#else
	write_cmos_sensor(0x0342, 0x16B0);
#endif
	write_cmos_sensor(0x0340, 0x0323);
	write_cmos_sensor(0x0202, 0x0200);
	write_cmos_sensor(0x0200, 0x00C6);
	write_cmos_sensor(0x0B04, 0x0101);
	write_cmos_sensor(0x0B08, 0x0000);
	write_cmos_sensor(0x0B00, 0x0007);
	write_cmos_sensor(0x316A, 0x00A0);

}


static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor_byte(0x0000) << 8) | read_cmos_sensor_byte(0x0001));
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
	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
#ifdef CONFIG_MTK_CAM_CAL
				/* read_imx135_otp_mtk_fmt(); */
#endif
				LOG_INF("i2c write id: 0x%x, ReadOut sensor id: 0x%x, imgsensor_info.sensor_id:0x%x.\n",
					imgsensor.i2c_write_id, *sensor_id, imgsensor_info.sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("%s: 0x%x, ReadOut sensor id: 0x%x, imgsensor_info.sensor_id:0x%x.\n",
				"Read sensor id fail, i2c write id", imgsensor.i2c_write_id, *sensor_id,
				imgsensor_info.sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 1;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
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
	/* const kal_uint8 i2c_addr[] = {IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2}; */
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

	LOG_1;
	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address */
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
			LOG_INF("Read sensor id fail, id: 0x%x, sensor id: 0x%x\n",
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
	imgsensor.ihdr_en = KAL_FALSE;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}				/*  open  */



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
	mdelay(10);
#ifdef FANPENGTAO
	int i = 0;

	for (i = 0; i < 10; i++) {
		LOG_INF("delay time = %d, the frame no = %d\n", i * 10, read_cmos_sensor(0x0005));
		mdelay(10);
	}
#endif
	return ERROR_NONE;
}				/*      preview   */

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
	if (imgsensor.current_fps == imgsensor_info.cap.max_framerate) {
		LOG_INF("capture30fps: use cap30FPS's setting: %d fps!\n",
			imgsensor.current_fps / 10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		/* PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M */
		LOG_INF("cap115fps: use cap1's setting: %d fps!\n", imgsensor.current_fps / 10);
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {		/* PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M */
		LOG_INF("Warning:=== current_fps %d fps is not support, so use cap1's setting\n",
			imgsensor.current_fps / 10);
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	mdelay(10);

#if 0
	if (imgsensor.test_pattern == KAL_TRUE)
		/* write_cmos_sensor(0x5002,0x00); */
#endif

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
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);

	return ERROR_NONE;
}				/*      normal_video   */

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

	return ERROR_NONE;
}				/*      hs_video   */

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
	return ERROR_NONE;
}

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
	LOG_INF("E\n");

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
	LOG_INF("E\n");

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
	LOG_INF("E\n");

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
	LOG_INF("E\n");

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
	LOG_INF("E\n");

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

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight = imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight = imgsensor_info.slim_video.grabwindow_height;
	sensor_resolution->SensorCustom1Width = imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height = imgsensor_info.custom1.grabwindow_height;

	sensor_resolution->SensorCustom2Width = imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height = imgsensor_info.custom2.grabwindow_height;

	sensor_resolution->SensorCustom3Width = imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height = imgsensor_info.custom3.grabwindow_height;

	sensor_resolution->SensorCustom4Width = imgsensor_info.custom4.grabwindow_width;
	sensor_resolution->SensorCustom4Height = imgsensor_info.custom4.grabwindow_height;

	sensor_resolution->SensorCustom5Width = imgsensor_info.custom5.grabwindow_width;
	sensor_resolution->SensorCustom5Height = imgsensor_info.custom5.grabwindow_height;
	return ERROR_NONE;
}				/*      get_resolution  */

static kal_uint32 get_info(MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);


	/* sensor_info->SensorVideoFrameRate = imgsensor_info.normal_video.max_framerate/10; // not use */
	/* sensor_info->SensorStillCaptureFrameRate= imgsensor_info.cap.max_framerate/10; // not use */
	/* imgsensor_info->SensorWebCamCaptureFrameRate= imgsensor_info.v.max_framerate; // not use */

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;	/* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;	/* inverse with datasheet */
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;	/* not use */
	sensor_info->SensorResetActiveHigh = FALSE;	/* not use */
	sensor_info->SensorResetDelayCount = 5;	/* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
	sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;
	sensor_info->Custom5DelayFrame = imgsensor_info.custom5_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
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
	sensor_info->SensorWidthSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;
#ifdef FPTPDAFSUPPORT
	sensor_info->PDAF_Support = 1;
#else
	sensor_info->PDAF_Support = 0;
#endif

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

		sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

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
		sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

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
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		sensor_info->SensorGrabStartX = imgsensor_info.custom4.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom4.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		sensor_info->SensorGrabStartX = imgsensor_info.custom5.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom5.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

		break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}				/*      get_info  */


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
	case MSDK_SCENARIO_ID_CUSTOM1:
		Custom1(image_window, sensor_config_data);	/* Custom1 */
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		Custom2(image_window, sensor_config_data);	/* Custom2 */
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		Custom3(image_window, sensor_config_data);	/* Custom3 */
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		Custom4(image_window, sensor_config_data);	/* Custom4 */
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		Custom5(image_window, sensor_config_data);	/* Custom5 */
		break;
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}

	sensor_set_sync_mode();
	set_mirror_flip(IMAGE_HV_MIRROR);

	return ERROR_NONE;
}				/* control() */



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
	if (enable)		/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id,
						MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk / framerate * 10
			/ imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength)
			? (frame_length - imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (framerate == 300) {
			frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength)
				? (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength)
				? (frame_length - imgsensor_info.cap1.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength)
			? (frame_length - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength)
			? (frame_length - imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength)
			? (frame_length - imgsensor_info.custom1.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk / framerate * 10 / imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength)
			? (frame_length - imgsensor_info.custom2.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		frame_length = imgsensor_info.custom3.pclk / framerate * 10 / imgsensor_info.custom3.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom3.framelength)
			? (frame_length - imgsensor_info.custom3.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom3.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		frame_length = imgsensor_info.custom4.pclk / framerate * 10 / imgsensor_info.custom4.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom4.framelength)
			? (frame_length - imgsensor_info.custom4.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom4.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		frame_length = imgsensor_info.custom5.pclk / framerate * 10 / imgsensor_info.custom5.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom5.framelength)
			? (frame_length - imgsensor_info.custom5.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	default:		/* coding with  preview scenario by default */
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		LOG_INF("error scenario_id = %d, we use preview scenario\n", scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id,
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

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable) {
		/* 0x5E00[8]: 1 enable,  0 disable */
		/* 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK */
		write_cmos_sensor(0x6028, 0x2000);
		write_cmos_sensor(0x602A, 0x1082);
		write_cmos_sensor(0x6F12, 0x0000);
		write_cmos_sensor(0x3734, 0x0001);
		write_cmos_sensor(0x0600, 0x0308);
	} else {
		/* 0x5E00[8]: 1 enable,  0 disable */
		/* 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK */
		write_cmos_sensor(0x6028, 0x2000);
		write_cmos_sensor(0x602A, 0x1082);
		write_cmos_sensor(0x6F12, 0x8010);
		write_cmos_sensor(0x3734, 0x0010);
		write_cmos_sensor(0x0600, 0x0300);
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
	unsigned long long *feature_data = (unsigned long long *)feature_para;
	/* unsigned long long *feature_return_para=(unsigned long long *) feature_para; */

	SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;
	SET_PD_BLOCK_INFO_T *PDAFinfo;

	LOG_INF("feature_id = %d\n", feature_id);
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
		night_mode((BOOL) (*feature_data));
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) (*feature_data));
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
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
		set_auto_flicker_mode((BOOL) (*feature_data_16), *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM) (*feature_data), *(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM) (*feature_data),
						  (MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) (*feature_data));
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:	/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", (UINT32) (*feature_data));
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", (BOOL) (*feature_data));
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (BOOL) (*feature_data);
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32) (*feature_data));

		wininfo = (SENSOR_WINSIZE_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n", (UINT16) *feature_data,
			(UINT16) *(feature_data + 1), (UINT16) *(feature_data + 2));
		ihdr_write_shutter_gain((UINT16) *feature_data, (UINT16) *(feature_data + 1),
					(UINT16) *(feature_data + 2));
		break;
		/******************** PDAF START >>> *********/
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%llu\n", *feature_data);
		/* PDAF capacity enable or not, 2p8 only full size support PDAF */
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;	/* video & capture use same setting */
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
			break;
		default:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%llu\n", *feature_data);
		PDAFinfo = (SET_PD_BLOCK_INFO_T *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
			       sizeof(SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
		S5K3L8_read_eeprom((kal_uint16) (*feature_data),
				   (char *)(uintptr_t) (*(feature_data + 1)),
				   (kal_uint32) (*(feature_data + 2)));
		break;
		/******************** PDAF END   <<< *********/
	case SENSOR_FEATURE_GET_SENSOR_SYNC_MODE_CAPACITY:
		LOG_INF("SENSOR_FEATURE_GET_SENSOR_SYNC_MODE_CAPACITY\n");
		*feature_return_para_32 = SENSOR_MASTER_SYNC_MODE | SENSOR_SLAVE_SYNC_MODE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_SENSOR_SYNC_MODE:
		LOG_INF("SENSOR_FEATURE_GET_SENSOR_SYNC_MODE\n");
		*feature_return_para_32 = g_sync_mode;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_SENSOR_SYNC_MODE:
		LOG_INF("SENSOR_FEATURE_SET_SENSOR_SYNC_MODE\n");
		g_sync_mode = (MUINT32) (*feature_data_32);
		LOG_INF("mode = %d\n", g_sync_mode);
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


UINT32 S5K3L8_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*      S5K3L8_MIPI_RAW_SensorInit      */
