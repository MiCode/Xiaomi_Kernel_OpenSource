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
 *	 s5k3p8spmipiraw_Sensor.c
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
 *----------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#define PFX "S5K3P8SP_camera_sensor"
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

#include "s5k3p8spmipiraw_Sensor.h"





static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {

	/* record sensor id defined in kd_imgsensor.h */
	.sensor_id = S5K3P8SP_SENSOR_ID,

	.checksum_value = 0xB1F1B3CC,

	.pre = {
		.pclk = 560000000,	/* record different mode's pclk */
		.linelength = 5120,	/* record different mode's linelength */

		/* record different mode's framelength */
		.framelength = 3643,

		/* record different mode's startx of grabwindow */
		.startx = 0,

		/* record different mode's starty of grabwindow */
		.starty = 0,

		/* record different mode's width of grabwindow */
		.grabwindow_width = 2304,

		/* record different mode's height of grabwindow */
		.grabwindow_height = 1728,

		/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount
		 * by different scenario
		 */
		.mipi_data_lp2hs_settle_dc = 85,

		/*       following for GetDefaultFramerateByScenario()  */
		.max_framerate = 300,	/* 30fps */
	},
	.cap = {
		.pclk = 560000000,
		.linelength = 5120,
		.framelength = 3643,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 3456,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.cap1 = {
		.pclk = 560000000,
		.linelength = 6400,
		.framelength = 3643,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4640,
		.grabwindow_height = 3488,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 240,
	},
	.cap2 = {
		.pclk = 560000000,
		.linelength = 10240,
		.framelength = 3643,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4640,
		.grabwindow_height = 3488,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 150,
	},
	.normal_video = {
		.pclk = 560000000,
		.linelength = 5120,
		.framelength = 3643,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 3456,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.hs_video = {		/*slow motion */
		.pclk = 560000000,
		.linelength = 4832,
		.framelength = 965,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
	},
	.slim_video = {		/*VT Call */

		/* record different mode's pclk */
		.pclk = 560000000,

		/* record different mode's linelength */
		.linelength = 5120,

		/* record different mode's framelength */
		.framelength = 3644,
		/* .framelength = 1822, */
		/* @2016.07.19 revised by dj for v0.03 */

		/* record different mode's startx of grabwindow */
		.startx = 0,
		/* record different mode's starty of grabwindow */
		.starty = 0,

		/* record different mode's width of grabwindow */
		.grabwindow_width = 1920,
		/* record different mode's height of grabwindow */
		.grabwindow_height = 1080,

		/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount
		 * by different scenario
		 */
		.mipi_data_lp2hs_settle_dc = 85,

		/* following for GetDefaultFramerateByScenario()  */
		.max_framerate = 300,
		/* .max_framerate = 600,*/
		/*@2016.07.19 revised by dj for v0.03 */
	},
	.margin = 4,		/* sensor framelength & shutter margin */
	.min_shutter = 4,	/* min shutter */

		/* max framelength by sensor register's limitation */
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	/* shutter delay frame for AE cycle,
	 * 2 frame with ispGain_delay-shut_delay=2-0=2
	 */

	.ae_sensor_gain_delay_frame = 0,
	/* sensor gain delay frame for AE cycle,
	 * 2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	 */

	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 5,	/* support sensor mode num */

	.cap_delay_frame = 3,	/* enter capture delay frame num */
	.pre_delay_frame = 3,	/* enter preview delay frame num */
	.video_delay_frame = 3,	/* enter video delay frame num */

	/* enter high speed video  delay frame num */
	.hs_video_delay_frame = 3,

	/* enter slim video delay frame num */
	.slim_video_delay_frame = 3,

	.isp_driving_current = ISP_DRIVING_2MA,	/* mclk driving current */
	/* sensor_interface_type */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,

	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,

	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.mipi_settle_delay_mode = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gb,
	.mclk = 24,	/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x20, 0x5a, 0xff},
	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_HV_MIRROR,	/* mirrorflip information */

	.sensor_mode = IMGSENSOR_MODE_INIT,
	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */

	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 0,	/* full size current fps : 24fps for PIP,
				 * 30fps for Normal or ZSD
				 */
	.autoflicker_en = KAL_FALSE,
	/* auto flicker enable: KAL_FALSE for disable auto flicker,
	 * KAL_TRUE for enable auto flicker
	 */

	.test_pattern = KAL_FALSE,
	/* test pattern mode or not.
	 * KAL_FALSE for in test pattern mode,
	 * KAL_TRUE for normal output
	 */

	 /* current scenario id */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,

	/* sensor need support LE, SE with HDR feature */
	.ihdr_mode = KAL_FALSE,

	/* record current sensor's i2c write id */
	.i2c_write_id = 0x20,

};


int chip_id;
/* VC_Num, VC_PixelNum, ModeSelect, EXPO_Ratio, ODValue, RG_STATSMODE */

	/* VC0_ID, VC0_DataType, VC0_SIZEH, VC0_SIZE,
	 * VC1_ID, VC1_DataType, VC1_SIZEH, VC1_SIZEV
	 */
	/* VC2_ID, VC2_DataType, VC2_SIZEH, VC2_SIZE,
	 * VC3_ID, VC3_DataType, VC3_SIZEH, VC3_SIZEV
	 */

/*static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3]=
 *  {// Preview mode setting
 *  {0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
 *  0x00, 0x2B, 0x0910, 0x06D0, 0x01, 0x00, 0x0000, 0x0000,
 *  0x02, 0x30, 0x00B4, 0x0360, 0x03, 0x00, 0x0000, 0x0000},
 * // Video mode setting
 *{0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
 *0x00, 0x2B, 0x1220, 0x0DA0, 0x01, 0x00, 0x0000, 0x0000,
 *0x02, 0x30, 0x00B4, 0x0360, 0x03, 0x00, 0x0000, 0x0000},
 * // Capture mode setting
 *{0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
 *0x00, 0x2B, 0x1220, 0x0DA0, 0x01, 0x00, 0x0000, 0x0000,
 *0x02, 0x30, 0x00B4, 0x0360, 0x03, 0x00, 0x0000, 0x0000}};
 */

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{4608, 3456, 0, 0, 4608, 3456, 2304, 1728,
	 0000, 0000, 2304, 1728, 0, 0, 2304, 1728},	/* Preview */
	{4608, 3456, 0, 0, 4608, 3456, 4608, 3456,
	 0000, 0000, 4608, 3456, 0, 0, 4608, 3456},	/* capture */
	{4608, 3456, 0, 0, 4608, 3456, 4608, 3456,
	 0000, 0000, 4608, 3456, 0, 0, 4608, 3456},	/* video */
	{4640, 3488, 400, 664, 3840, 2160, 1280,
	  720, 0000, 0000, 1280, 720, 0, 0, 1280, 720},	/* hight speed video */
	{4640, 3488, 400, 664, 3840, 2160, 1920, 1080,
	 0000, 0000, 1920, 1080, 0, 0, 1920, 1080},	/* slim video */
};

/* no mirror flip, and no binning -revised by dj */
/* static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
 * .i4OffsetX = 16,
 * .i4OffsetY = 16,
 * .i4PitchX  = 64,
 * .i4PitchY  = 64,
 * .i4PairNum  =16,
 * .i4SubBlkW  =16,
 * .i4SubBlkH  =16,
 * .i4PosL = {{20,23},{72,23},{36,27},{56,27},{24,43},{68,43},{40,47},
 * {52,47},{40,55},{52,55},{24,59},{68,59},{36,75},{56,75},{20,79},{72,79}},
 * .i4PosR = {{20,27},{72,27},{36,31},{56,31},{24,39},{68,39},{40,43},{52,43},
 * {40,59},{52,59},{24,63},{68,63},{36,71},{56,71},{20,75},{72,75}},
 * .iMirrorFlip = 0,
 * .i4BlockNumX = 72,
 * .i4BlockNumY = 54,
 * };
 */

#if 0
static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	iReadReg((u16) addr, (u8 *) &get_byte, imgsensor.i2c_write_id);
	return get_byte;
}

#define write_cmos_sensor(addr, para)
(iWriteReg((u16) addr, (u32) para, 1,  imgsensor.i2c_write_id))
#endif

#define RWB_ID_OFFSET     0x0F73
#define EEPROM_READ_ID    0xA0
#define EEPROM_WRITE_ID   0xA1

#if 0
static kal_uint16 is_RWB_sensor(void)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(RWB_ID_OFFSET >> 8),
			      (char)(RWB_ID_OFFSET & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 1, EEPROM_READ_ID);
	return get_byte;
}
#endif

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 2, imgsensor.i2c_write_id);
	return ((get_byte << 8) & 0xff00) | ((get_byte >> 8) & 0x00ff);
}


static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {
		(char)(addr >> 8), (char)(addr & 0xFF),
		(char)(para >> 8), (char)(para & 0xFF) };
	iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[4] = { (char)(addr >> 8),
			      (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	pr_debug("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);

	/* return; //for test */
	write_cmos_sensor(0x0340, imgsensor.frame_length);
	write_cmos_sensor(0x0342, imgsensor.line_length);
}				/*      set_dummy  */


static void set_max_framerate(
	UINT16 framerate,
	kal_bool min_framelength_en)
{

	kal_uint32 frame_length = imgsensor.frame_length;

	pr_debug("framerate = %d, min framelength should enable %d\n",
		framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	if (frame_length >= imgsensor.min_frame_length)
		imgsensor.frame_length = frame_length;
	else {
		imgsensor.frame_length = imgsensor.min_frame_length;
		imgsensor.dummy_line   =
		imgsensor.frame_length - imgsensor.min_frame_length;
	}

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;

		imgsensor.dummy_line =
		imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}			/*      set_max_framerate  */

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
		realtime_fps = imgsensor.pclk /
			       imgsensor.line_length * 10 /
			       imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(0x0340, imgsensor.frame_length);

		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x0340, imgsensor.frame_length);
		pr_debug("(else)imgsensor.frame_length = %d\n",
			imgsensor.frame_length);

	}
	/* Update Shutter */
	write_cmos_sensor(0x0202, shutter);
	pr_debug("shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);

}				/*      write_shutter  */



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

	/* gain=1024;//for test */
	/* return; //for test */

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
	/* write_cmos_sensor_8(0x0204,(reg_gain>>8)); */
	/* write_cmos_sensor_8(0x0205,(reg_gain&0xff)); */

	return gain;
}				/*      set_gain  */

static void set_mirror_flip(kal_uint8 image_mirror)
{

	kal_uint8 itemp;

	pr_debug("image_mirror = %d\n", image_mirror);
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
}				/*      night_mode      */
#endif



/*************************************************************************
 * FUNCTION
 *	check_stremoff
 *
 * DESCRIPTION
 *	waiting function until sensor streaming finish.
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

static void check_streamoff(void)
{
	unsigned int i = 0;
	int timeout = (10000 / imgsensor.current_fps) + 1;

	mdelay(3);
	for (i = 0; i < timeout; i++) {
		if (read_cmos_sensor_8(0x0005) != 0xFF)
			mdelay(1);
		else
			break;
	}
	pr_debug(" check_streamoff exit!\n");
}

static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
		write_cmos_sensor_8(0x0100, 0X01);
	} else {
		write_cmos_sensor_8(0x0100, 0x00);
		check_streamoff();
	}
	return ERROR_NONE;
}


#define USE_TNP_BURST	0
#if USE_TNP_BURST
const u16 uTnpArray[] = {
	0x126F,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x4904,
	0x4803,
	0x4A04,
	0x6008,
	0x1A10,
	0x8088,
	0xF000,
	0xB852,
	0x0020,
	0xE831,
	0x0020,
	0x801E,
	0x0020,
	0x004B,
	0x0000,
	0x0000,
	0xB570,
	0x4606,
	0x4832,
	0x2200,
	0x6801,
	0x0C0C,
	0xB28D,
	0x4629,
	0x4620,
	0xF000,
	0xF873,
	0x4630,
	0xF000,
	0xF875,
	0x482D,
	0x4A2E,
	0x8801,
	0x8011,
	0x1C91,
	0x8840,
	0x8008,
	0x4629,
	0x4620,
	0xE8BD,
	0x4070,
	0x2201,
	0xF000,
	0xB862,
	0xE92D,
	0x41F0,
	0x4680,
	0x4824,
	0x4614,
	0x460F,
	0x6840,
	0x2200,
	0xB285,
	0x0C06,
	0x4629,
	0x4630,
	0xF000,
	0xF854,
	0x4821,
	0x8C40,
	0xB170,
	0xF894,
	0x006B,
	0x2140,
	0xFBB1,
	0xF0F0,
	0x491E,
	0x4A1F,
	0x8B09,
	0x8852,
	0x1A89,
	0x0109,
	0xFB91,
	0xF0F0,
	0xB284,
	0xE005,
	0x4622,
	0x4639,
	0x4640,
	0xF000,
	0xF847,
	0x4604,
	0x2201,
	0x4629,
	0x4630,
	0xF000,
	0xF837,
	0x4620,
	0xE8BD,
	0x81F0,
	0xB510,
	0x2200,
	0xF2AF,
	0x019B,
	0x4812,
	0xF000,
	0xF83C,
	0x4C0B,
	0x2200,
	0xF2AF,
	0x016F,
	0x6020,
	0x480F,
	0xF000,
	0xF834,
	0x6060,
	0x4C0E,
	0xF640,
	0x308C,
	0x8CE1,
	0xF000,
	0xF832,
	0x8CE0,
	0x4A0D,
	0x490B,
	0xF842,
	0x1020,
	0x1C40,
	0x84E0,
	0xBD10,
	0x0020,
	0xE031,
	0x0020,
	0x004B,
	0x0040,
	0x0C95,
	0x0020,
	0x102F,
	0x0020,
	0x101B,
	0x0020,
	0x402F,
	0x0000,
	0xE915,
	0x0000,
	0xCD9E,
	0x0020,
	0x801E,
	0xF895,
	0x3068,
	0x0020,
	0x102E,
	0xF240,
	0x7CA3,
	0xF2C0,
	0x0C00,
	0x4760,
	0xF241,
	0x5CE9,
	0xF2C0,
	0x0C00,
	0x4760,
	0xF649,
	0x6CCD,
	0xF2C0,
	0x0C00,
	0x4760,
	0xF64D,
	0x0C1B,
	0xF2C0,
	0x0C00,
	0x4760,
	0xF24D,
	0x7CBB,
	0xF2C0,
	0x0C00,
	0x4760,
	0x0000,
	0x0831,
	0x9F02,
	0x0000,
	0x1500,
};
#endif

static void sensor_init(void)
{
	pr_debug("sensor_init() E\n");
	chip_id = read_cmos_sensor_8(0x0002);
	pr_debug("chip id:%d E\n", chip_id);

	/* initial sequence */
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x6010, 0x0001);
	mdelay(3);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);

	/* global */
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x2F38);
	write_cmos_sensor(0x6F12, 0x0088);
	write_cmos_sensor(0x6F12, 0x0D70);
	write_cmos_sensor(0x0202, 0x0200);
	write_cmos_sensor(0x0200, 0x0618);
	write_cmos_sensor(0x3604, 0x0002);
	write_cmos_sensor(0x3606, 0x0103);
	write_cmos_sensor(0xF496, 0x0048);
	write_cmos_sensor(0xF470, 0x0020);
	write_cmos_sensor(0xF43A, 0x0015);
	write_cmos_sensor(0xF484, 0x0006);
	write_cmos_sensor(0xF440, 0x00AF);
	write_cmos_sensor(0xF442, 0x44C6);
	write_cmos_sensor(0xF408, 0xFFF7);
	write_cmos_sensor(0x3664, 0x0019);
	write_cmos_sensor(0xF494, 0x1010);
	write_cmos_sensor(0x367A, 0x0100);
	write_cmos_sensor(0x362A, 0x0104);
	write_cmos_sensor(0x362E, 0x0404);
	write_cmos_sensor(0x32B2, 0x0008);
	write_cmos_sensor(0x3286, 0x0003);
	write_cmos_sensor(0x328A, 0x0005);
	write_cmos_sensor(0xF47C, 0x001F);
	write_cmos_sensor(0xF62E, 0x00C5);
	write_cmos_sensor(0xF630, 0x00CD);
	write_cmos_sensor(0xF632, 0x00DD);
	write_cmos_sensor(0xF634, 0x00E5);
	write_cmos_sensor(0xF636, 0x00F5);
	write_cmos_sensor(0xF638, 0x00FD);
	write_cmos_sensor(0xF63a, 0x010D);
	write_cmos_sensor(0xF63C, 0x0115);
	write_cmos_sensor(0xF63E, 0x0125);
	write_cmos_sensor(0xF640, 0x012D);
	write_cmos_sensor(0x3070, 0x0000);
	write_cmos_sensor(0x0B0E, 0x0000);
	write_cmos_sensor(0x31C0, 0x00C8);
	write_cmos_sensor(0x1006, 0x0004);
}				/*      sensor_init  */


static void preview_setting(void)
{
	pr_debug("preview_setting() E\n");

	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0304, 0x0006);
	write_cmos_sensor(0x0306, 0x0069);
	write_cmos_sensor(0x0302, 0x0001);
	write_cmos_sensor(0x0300, 0x0003);
	write_cmos_sensor(0x030C, 0x0004);
	write_cmos_sensor(0x030E, 0x0071);
	write_cmos_sensor(0x030A, 0x0001);
	write_cmos_sensor(0x0308, 0x0008);
	write_cmos_sensor(0x0344, 0x0018);
	write_cmos_sensor(0x0346, 0x0018);
	write_cmos_sensor(0x0348, 0x1217);
	write_cmos_sensor(0x034A, 0x0D97);
	write_cmos_sensor(0x034C, 0x0900);
	write_cmos_sensor(0x034E, 0x06C0);
	write_cmos_sensor(0x0408, 0x0000);
	write_cmos_sensor(0x0900, 0x0122);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0003);
	write_cmos_sensor(0x0384, 0x0003);
	write_cmos_sensor(0x0386, 0x0001);
	write_cmos_sensor(0x0400, 0x0000);
	write_cmos_sensor(0x0404, 0x0010);
	write_cmos_sensor(0x0342, 0x1400);
	write_cmos_sensor(0x0340, 0x0E3B);
	write_cmos_sensor(0x602A, 0x1704);
	write_cmos_sensor(0x6F12, 0x8011);
	write_cmos_sensor(0x317A, 0x0007);
	write_cmos_sensor(0x31A4, 0x0102);

	/* stream on */
	write_cmos_sensor_8(0x0100, 0x01);
}				/*      preview_setting  */

static void capture_setting(kal_uint16 currefps)
{
	pr_debug("capture_setting() E! currefps:%d\n", currefps);



	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0304, 0x0006);
	write_cmos_sensor(0x0306, 0x0069);
	write_cmos_sensor(0x0302, 0x0001);
	write_cmos_sensor(0x0300, 0x0003);
	write_cmos_sensor(0x030C, 0x0004);
	write_cmos_sensor(0x030E, 0x0071);
	write_cmos_sensor(0x030A, 0x0001);
	write_cmos_sensor(0x0308, 0x0008);
	write_cmos_sensor(0x0344, 0x0018);
	write_cmos_sensor(0x0346, 0x0018);
	write_cmos_sensor(0x0348, 0x1217);
	write_cmos_sensor(0x034A, 0x0D97);
	write_cmos_sensor(0x034C, 0x1200);
	write_cmos_sensor(0x034E, 0x0D80);
	write_cmos_sensor(0x0408, 0x0000);
	write_cmos_sensor(0x0900, 0x0011);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0001);
	write_cmos_sensor(0x0400, 0x0000);
	write_cmos_sensor(0x0404, 0x0010);
	write_cmos_sensor(0x0342, 0x1400);
	write_cmos_sensor(0x0340, 0x0E3B);
	write_cmos_sensor(0x602A, 0x1704);
	write_cmos_sensor(0x6F12, 0x8010);
	write_cmos_sensor(0x317A, 0x0130);
	write_cmos_sensor(0x31A4, 0x0102);

}

static void normal_video_setting(kal_uint16 currefps)
{
	pr_debug("normal_video_setting() E! currefps:%d\n", currefps);


	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0304, 0x0006);
	write_cmos_sensor(0x0306, 0x0069);
	write_cmos_sensor(0x0302, 0x0001);
	write_cmos_sensor(0x0300, 0x0003);
	write_cmos_sensor(0x030C, 0x0004);
	write_cmos_sensor(0x030E, 0x0071);
	write_cmos_sensor(0x030A, 0x0001);
	write_cmos_sensor(0x0308, 0x0008);
	write_cmos_sensor(0x0344, 0x0018);
	write_cmos_sensor(0x0346, 0x0018);
	write_cmos_sensor(0x0348, 0x1217);
	write_cmos_sensor(0x034A, 0x0D97);
	write_cmos_sensor(0x034C, 0x1200);
	write_cmos_sensor(0x034E, 0x0D80);
	write_cmos_sensor(0x0408, 0x0000);
	write_cmos_sensor(0x0900, 0x0011);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0001);
	write_cmos_sensor(0x0400, 0x0000);
	write_cmos_sensor(0x0404, 0x0010);
	write_cmos_sensor(0x0342, 0x1400);
	write_cmos_sensor(0x0340, 0x0E3B);
	write_cmos_sensor(0x602A, 0x1704);
	write_cmos_sensor(0x6F12, 0x8010);
	write_cmos_sensor(0x317A, 0x0130);
	write_cmos_sensor(0x31A4, 0x0102);

}

static void hs_video_setting(void)
{
	pr_debug("hs_video_setting() E\n");
	/* 720p 120fps */
	if (chip_id == 0xA0) {
		write_cmos_sensor(0x6028, 0x2000);
		write_cmos_sensor(0x6214, 0x7971);
		write_cmos_sensor(0x6218, 0x7150);
		write_cmos_sensor(0x3004, 0x0004);
		write_cmos_sensor(0x3000, 0x0000);
		write_cmos_sensor(0xF444, 0x0000);
		write_cmos_sensor(0x3604, 0x0001);
		write_cmos_sensor(0x3058, 0x0000);
		write_cmos_sensor(0x0306, 0x008C);
		write_cmos_sensor(0x0300, 0x0004);
		write_cmos_sensor(0x030E, 0x007A);
		write_cmos_sensor(0x0344, 0x0198);
		write_cmos_sensor(0x0348, 0x1097);
		write_cmos_sensor(0x0346, 0x02A0);
		write_cmos_sensor(0x034A, 0x0B0F);
		write_cmos_sensor(0x034C, 0x0500);
		write_cmos_sensor(0x034E, 0x02D0);
		write_cmos_sensor(0x0342, 0x12E0);
		write_cmos_sensor(0x0340, 0x03C5);
		write_cmos_sensor(0x0216, 0x0000);
		write_cmos_sensor(0x3664, 0x1019);
		write_cmos_sensor(0x1006, 0x0003);
		write_cmos_sensor(0x0900, 0x0133);
		write_cmos_sensor(0x0380, 0x0001);
		write_cmos_sensor(0x0382, 0x0005);
		write_cmos_sensor(0x0384, 0x0001);
		write_cmos_sensor(0x0386, 0x0005);
		write_cmos_sensor(0x0400, 0x0000);
		write_cmos_sensor(0x0404, 0x0010);
		write_cmos_sensor(0x0B0E, 0x0100);
		write_cmos_sensor(0x6028, 0x2000);
		write_cmos_sensor(0x602A, 0x19E0);
		write_cmos_sensor(0x6F12, 0x0001);
		write_cmos_sensor(0x317A, 0xFFFF);
		write_cmos_sensor(0x0100, 0x0100);
	} else {
		write_cmos_sensor(0x6028, 0x4000);
		write_cmos_sensor(0x0136, 0x1800);
		write_cmos_sensor(0x0304, 0x0006);
		write_cmos_sensor(0x0306, 0x008C);
		write_cmos_sensor(0x0302, 0x0001);
		write_cmos_sensor(0x0300, 0x0004);
		write_cmos_sensor(0x030C, 0x0004);
		write_cmos_sensor(0x030E, 0x007A);
		write_cmos_sensor(0x030A, 0x0001);
		write_cmos_sensor(0x0308, 0x0008);
		write_cmos_sensor(0x0344, 0x0198);
		write_cmos_sensor(0x0346, 0x02A0);
		write_cmos_sensor(0x0348, 0x1097);
		write_cmos_sensor(0x034A, 0x0B0F);
		write_cmos_sensor(0x034C, 0x0500);
		write_cmos_sensor(0x034E, 0x02D0);
		write_cmos_sensor(0x0408, 0x0000);
		write_cmos_sensor(0x0900, 0x0113);
		write_cmos_sensor(0x0380, 0x0001);
		write_cmos_sensor(0x0382, 0x0001);
		write_cmos_sensor(0x0384, 0x0001);
		write_cmos_sensor(0x0386, 0x0005);
		write_cmos_sensor(0x0400, 0x0001);
		write_cmos_sensor(0x0404, 0x0030);
		write_cmos_sensor(0x0342, 0x12E0);
		write_cmos_sensor(0x0340, 0x03C5);
		write_cmos_sensor(0x0B0E, 0x0100);
		write_cmos_sensor(0x0216, 0x0000);
		write_cmos_sensor(0x3604, 0x0001);
		write_cmos_sensor(0x3664, 0x0011);
		write_cmos_sensor(0x3004, 0x0004);
		write_cmos_sensor(0x3000, 0x0000);
		write_cmos_sensor(0x317A, 0x0007);
		write_cmos_sensor(0x1006, 0x0003);
		write_cmos_sensor(0x6028, 0x2000);
		write_cmos_sensor(0x602A, 0x19E0);
		write_cmos_sensor(0x6F12, 0x0001);

		write_cmos_sensor(0x602A, 0x18F6);
		write_cmos_sensor(0x6F12, 0x00AF);
		write_cmos_sensor(0x6F12, 0x00AF);

		write_cmos_sensor(0x31A4, 0x0102);
		write_cmos_sensor_8(0x0100, 0x01);
	}
}

static void slim_video_setting(void)
{
	pr_debug("slim_video_setting() E\n");
	/* 1080p 60fps */
	if (chip_id == 0xA0) {
		write_cmos_sensor(0x6028, 0x2000);
		write_cmos_sensor(0x6214, 0x7971);
		write_cmos_sensor(0x6218, 0x7150);
		write_cmos_sensor(0x3004, 0x0003);
		write_cmos_sensor(0x3000, 0x0001);
		write_cmos_sensor(0xF444, 0x8000);
		write_cmos_sensor(0x3604, 0x0002);
		write_cmos_sensor(0x3058, 0x0001);
		write_cmos_sensor(0x0306, 0x0069);
		write_cmos_sensor(0x0300, 0x0003);
		write_cmos_sensor(0x030E, 0x007A);
		write_cmos_sensor(0x0344, 0x0198);
		write_cmos_sensor(0x0348, 0x1097);
		write_cmos_sensor(0x0346, 0x02A0);
		write_cmos_sensor(0x034A, 0x0B0F);
		write_cmos_sensor(0x034C, 0x0780);
		write_cmos_sensor(0x034E, 0x0438);
		write_cmos_sensor(0x0342, 0x1400);
		write_cmos_sensor(0x0340, 0x0E3C);
		write_cmos_sensor(0x0216, 0x0000);
		write_cmos_sensor(0x3664, 0x0019);
		write_cmos_sensor(0x1006, 0x0002);
		write_cmos_sensor(0x0900, 0x0122);
		write_cmos_sensor(0x0380, 0x0001);
		write_cmos_sensor(0x0382, 0x0003);
		write_cmos_sensor(0x0384, 0x0001);
		write_cmos_sensor(0x0386, 0x0003);
		write_cmos_sensor(0x0400, 0x0000);
		write_cmos_sensor(0x0404, 0x0010);
		write_cmos_sensor(0x0B0E, 0x0100);
		write_cmos_sensor(0x6028, 0x2000);
		write_cmos_sensor(0x602A, 0x19E0);
		write_cmos_sensor(0x6F12, 0x0001);
		write_cmos_sensor(0x317A, 0xFFFF);
		write_cmos_sensor(0x0100, 0x0100);
	} else {
		write_cmos_sensor(0x6028, 0x4000);
		write_cmos_sensor(0x0136, 0x1800);
		write_cmos_sensor(0x0304, 0x0006);
		write_cmos_sensor(0x0306, 0x0069);
		write_cmos_sensor(0x0302, 0x0001);
		write_cmos_sensor(0x0300, 0x0003);
		write_cmos_sensor(0x030C, 0x0004);
		write_cmos_sensor(0x030E, 0x007A);
		write_cmos_sensor(0x030A, 0x0001);
		write_cmos_sensor(0x0308, 0x0008);
		write_cmos_sensor(0x0344, 0x0198);
		write_cmos_sensor(0x0346, 0x02A0);
		write_cmos_sensor(0x0348, 0x1097);
		write_cmos_sensor(0x034A, 0x0B0F);
		write_cmos_sensor(0x034C, 0x0780);
		write_cmos_sensor(0x034E, 0x0438);
		write_cmos_sensor(0x0408, 0x0000);
		write_cmos_sensor(0x0900, 0x0112);
		write_cmos_sensor(0x0380, 0x0001);
		write_cmos_sensor(0x0382, 0x0001);
		write_cmos_sensor(0x0384, 0x0001);
		write_cmos_sensor(0x0386, 0x0003);
		write_cmos_sensor(0x0400, 0x0001);
		write_cmos_sensor(0x0404, 0x0020);
		write_cmos_sensor(0x0342, 0x1400);
		write_cmos_sensor(0x0340, 0x0E3C);
		write_cmos_sensor(0x0B0E, 0x0100);
		write_cmos_sensor(0x0216, 0x0000);
		write_cmos_sensor(0x3604, 0x0002);
		write_cmos_sensor(0x3664, 0x0019);
		write_cmos_sensor(0x3004, 0x0003);
		write_cmos_sensor(0x3000, 0x0001);
		write_cmos_sensor(0x317A, 0x00A0);
		write_cmos_sensor(0x1006, 0x0002);
		write_cmos_sensor(0x6028, 0x2000);
		write_cmos_sensor(0x602A, 0x19E0);
		write_cmos_sensor(0x6F12, 0x0001);

		write_cmos_sensor(0x602A, 0x18F6);
		write_cmos_sensor(0x6F12, 0x002F);
		write_cmos_sensor(0x6F12, 0x002F);

		write_cmos_sensor(0x31A4, 0x0102);
		write_cmos_sensor_8(0x0100, 0x01);
	}


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
	kal_uint16 sp8spFlag = 0;

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = ((read_cmos_sensor_8(0x0000) << 8) |
				       read_cmos_sensor_8(0x0001));

		pr_debug("read_0x0000=0x%x, 0x0001=0x%x,	0x0000_0001=0x%x\n",
			read_cmos_sensor_8(0x0000),
			read_cmos_sensor_8(0x0001),
			read_cmos_sensor(0x0000));

			if (*sensor_id == imgsensor_info.sensor_id) {
				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
#if 0
			if (is_RWB_sensor() == 0x1) {
				imgsensor_info.sensor_output_dataformat =
				SENSOR_OUTPUT_FORMAT_RAW_RWB_Wr;
				pr_debug("RWB sensor of S5k3p8\n");
				}
#endif
			sp8spFlag = (((read_cmos_sensor(0x000C) & 0xFF) << 8)
				    |((read_cmos_sensor(0x000E) >> 8) & 0xFF));
			pr_debug("sp8Flag(0x%x),0x5003 used by s5k3p8sp\n",
				sp8spFlag);

			if (sp8spFlag == 0x5003) {
				pr_debug("it is s5k3p8sp\n");
				return ERROR_NONE;
			}
		pr_debug("3p8 type is 0x(%x),0x000C(0x%x),0x000E(0x%x)\n",
			sp8spFlag,
			read_cmos_sensor(0x000C),
			read_cmos_sensor(0x000E));
			*sensor_id = 0xFFFFFFFF;
			return ERROR_SENSOR_CONNECT_FAIL;
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

	pr_debug("%s", __func__);

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = ((read_cmos_sensor_8(0x0000) << 8) |
				      read_cmos_sensor_8(0x0001));

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
}				/*      open  */



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
	pr_debug("preview E\n");

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
	pr_debug("capture E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {

	/* PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M */
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;

	} else if (imgsensor.current_fps == imgsensor_info.cap2.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap2.pclk;
		imgsensor.line_length = imgsensor_info.cap2.linelength;
		imgsensor.frame_length = imgsensor_info.cap2.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap2.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
		pr_debug(
		"Warning: current_fps %d fps is not support,so use cap's setting: %d fps!\n",
		imgsensor.current_fps, imgsensor_info.cap.max_framerate / 10);


		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}				/* capture() */

static kal_uint32 normal_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("normal_video E\n");

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
}				/*      normal_video   */

static kal_uint32 hs_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("hs_video E\n");

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

	return ERROR_NONE;
}				/*      hs_video   */

static kal_uint32 slim_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("slim_video E\n");

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
}				/*      slim_video       */



static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	pr_debug("get_resolution E\n");
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
	pr_debug("get_info -> scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* not use */
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* inverse with datasheet */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
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
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	/* The frame of setting shutter
	 * default 0 for TG int
	 */

	sensor_info->AESensorGainDelayFrame =
	imgsensor_info.ae_sensor_gain_delay_frame;
	/* The frame of setting
	 * sensor gain
	 */

	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support =
		imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine =
		imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum =
		imgsensor_info.sensor_mode_num;

	/* change pdaf support mode to pdaf VC mode */
	sensor_info->PDAF_Support = 0;

	sensor_info->SensorMIPILaneNumber =
		imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;		/* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;	/* not use */
	sensor_info->SensorPixelClockCount = 3;		/* not use */
	sensor_info->SensorDataLatchCount = 2;		/* not use */

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
}				/*      get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
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
	/* //pr_debug("framerate = %d\n ", framerate); */
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

static kal_uint32 set_auto_flicker_mode(
	kal_bool enable, UINT16 framerate)
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
	enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MUINT32 framerate)
{
	kal_uint32 frame_length;

	pr_debug("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10 /
			       imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length -
			imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length = imgsensor_info.pre.framelength +
		imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
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
			? (frame_length -
			imgsensor_info.normal_video.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps ==
		     imgsensor_info.cap1.max_framerate) {
			frame_length = imgsensor_info.cap1.pclk /
			framerate * 10 /
			imgsensor_info.cap1.linelength;

			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			(frame_length > imgsensor_info.cap1.framelength)
			? (frame_length -
			imgsensor_info.cap1.framelength) : 0;

			imgsensor.frame_length =
			imgsensor_info.cap1.framelength +
			imgsensor.dummy_line;

			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else if (imgsensor.current_fps ==
			   imgsensor_info.cap2.max_framerate) {
			frame_length = imgsensor_info.cap2.pclk /
					framerate * 10 /
					imgsensor_info.cap2.linelength;

			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
				(frame_length > imgsensor_info.cap2.framelength)
				? (frame_length -
				imgsensor_info.cap2.framelength) : 0;

			imgsensor.frame_length =
			imgsensor_info.cap2.framelength +
			imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;

			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps !=
			imgsensor_info.cap.max_framerate)
			pr_debug(
			"Warning: current_fps %d fps is not support,so use cap's setting: %d fps!\n",
			framerate,
			imgsensor_info.cap.max_framerate / 10);
			frame_length = imgsensor_info.cap.pclk
			    / framerate * 10 / imgsensor_info.cap.linelength;

			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			(frame_length > imgsensor_info.cap.framelength)
			? (frame_length - imgsensor_info.cap.framelength) : 0;

			imgsensor.frame_length =
				imgsensor_info.cap.framelength +
				imgsensor.dummy_line;

			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk /
			framerate * 10 / imgsensor_info.hs_video.linelength;

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
			imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	default:	/* coding with  preview scenario by default */
		frame_length = imgsensor_info.pre.pclk /
			framerate * 10 /
			imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.pre.framelength)
		? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;

		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
pr_debug("error scenario_id = %d,we use preview scenario\n", scenario_id);

		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MUINT32 *framerate)
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
	pr_debug("enable: %d\n", enable);

	if (enable) {

		/* 0 : Normal,
		 * 1 : Solid Color,
		 * 2 : Color Bar,
		 * 3 : Shade Color Bar,
		 * 4 : PN9
		 */

		write_cmos_sensor(0x0600, 0x0002);
	} else {
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
	unsigned long long *feature_data = (unsigned long long *)feature_para;

	/* struct SET_PD_BLOCK_INFO_T *PDAFinfo; */
	/* struct SENSOR_VC_INFO_STRUCT *pvcinfo; */
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	pr_debug("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
pr_debug("feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n",
	imgsensor.pclk, imgsensor.current_fps);
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		/* night_mode((BOOL) *feature_data);
		 * no need to implement this mode
		 */
		break;

	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
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
	     (enum MSDK_SCENARIO_ID_ENUM) *feature_data, *(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *(feature_data),
			 (MUINT32 *) (uintptr_t) (*(feature_data + 1)));
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
		pr_debug("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32) *feature_data);
		wininfo =
			(struct SENSOR_WINSIZE_INFO_STRUCT *)
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
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		/* ihdr_write_shutter_gain((UINT16)*feature_data,
		 *(UINT16)*(feature_data+1),
		 * (UINT16)*(feature_data+2));
		 */
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		pr_debug("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1));
		/* ihdr_write_shutter((UINT16)*feature_data,
		 * (UINT16)*(feature_data+1));
		 */
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:	/*get cal data from eeprom*/
		pr_debug("SENSOR_FEATURE_GET_PDAF_DATA\n");
		s5k3p8_read_otp_pdaf_data(
					(kal_uint16)(*feature_data),
					(BYTE *)(uintptr_t)(*(feature_data+1)),
					(kal_uint32)(*(feature_data+2)));
		pr_debug("SENSOR_FEATURE_GET_PDAF_DATA success\n");
		break;
	/******************** PDAF START >>> *********/
	/*
	 * case SENSOR_FEATURE_GET_PDAF_INFO:
	 * pr_debug("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
	 * (UINT16)*feature_data);
	 * PDAFinfo = (struct SET_PD_BLOCK_INFO_T *)
	 * (uintptr_t)(*(feature_data+1));
	 * switch (*feature_data) {
	 * case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG: //full
	 * case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	 * case MSDK_SCENARIO_ID_CAMERA_PREVIEW: //2x2 binning
	 * memcpy((void *)PDAFinfo,
	 * (void *)&imgsensor_pd_info,
	 * sizeof(struct SET_PD_BLOCK_INFO_T)); //need to check
	 * break;
	 * case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	 * case MSDK_SCENARIO_ID_SLIM_VIDEO:
	 * default:
	 * break;
	 * }
	 * break;
	 * case SENSOR_FEATURE_GET_VC_INFO:
	 * pr_debug("SENSOR_FEATURE_GET_VC_INFO %d\n", (UINT16)*feature_data);
	 * pvcinfo =
	 * (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
	 * switch (*feature_data_32) {
	 * case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	 * memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[2],
	 * sizeof(struct SENSOR_VC_INFO_STRUCT));
	 * break;
	 * case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	 * memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[1],
	 * sizeof(struct SENSOR_VC_INFO_STRUCT));
	 * break;
	 * case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	 * default:
	 * memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[0],
	 * sizeof(struct SENSOR_VC_INFO_STRUCT));
	 * break;
	 * }
	 * break;
	 * case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
	 * pr_debug("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
	 * (UINT16)*feature_data);
	 * //PDAF capacity enable or not
	 * switch (*feature_data) {
	 * case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	 * (MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
	 * break;
	 * case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	 */
	 /*video & capture use same setting*/
	/* *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
	 * break;
	 * case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	 * *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
	 * break;
	 * case MSDK_SCENARIO_ID_SLIM_VIDEO:
	 * *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0; //need to check
	 * break;
	 * case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	 * *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
	 * break;
	 * default:
	 * *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
	 * break;
	 * }
	 * break;
	 * case SENSOR_FEATURE_GET_PDAF_DATA:    //get cal data from eeprom
	 * pr_debug("SENSOR_FEATURE_GET_PDAF_DATA\n");
	 * read_3P8_eeprom((kal_uint16 )(*feature_data),
	 * (char*)(uintptr_t)(*(feature_data+1)),
	 * (kal_uint32)(*(feature_data+2)));
	 * pr_debug("SENSOR_FEATURE_GET_PDAF_DATA success\n");
	 * break;
	 * case SENSOR_FEATURE_SET_PDAF:
	 * pr_debug("PDAF mode :%d\n", *feature_data_16);
	 * imgsensor.pdaf_mode= *feature_data_16;
	 * break;
	 */

	/******************** PDAF END   <<< *********/
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

UINT32 S5K3P8SP_MIPI_RAW_SensorInit(
	struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
