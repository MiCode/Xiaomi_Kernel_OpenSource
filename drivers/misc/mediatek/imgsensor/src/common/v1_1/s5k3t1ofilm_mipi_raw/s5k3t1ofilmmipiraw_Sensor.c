/*
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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
 *	 s5k3t1ofilmmipiraw_Sensor.c
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

#define PFX "S5K3T1OFILM_camera_sensor"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__
#define LOG_INF(format, args...) printk(PFX "[JW][%s] " format, __FUNCTION__, ##args)
# define LOG_INF_U(format, args...) printk(PFX "[JW][%s] " format, __func__, ##args)

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

#include "s5k3t1ofilmmipiraw_Sensor.h"

#define MULTI_WRITE 1

#if MULTI_WRITE
static const int I2C_BUFFER_LEN = 1020; /*trans# max is 255, each 4 bytes*/
#else
static const int I2C_BUFFER_LEN = 4;
#endif

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5K3T1OFILM_SENSOR_ID,
	.checksum_value = 0xc9e2d2c4,

	.pre = {
		.pclk = 800000000, /*//30fps case*/
		.linelength = 9216, /*//0x3300*/
		.framelength = 2880, /*//0x07F8*/
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592, /*//0x0A20*/
		.grabwindow_height = 1940, /*//0x0794*/
		//grabwindow_height should be 16's N times
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 300,
		.mipi_pixel_rate = 240000000,
	},
	.cap = {
		.pclk = 800000000, /*//30fps case*/
		.linelength = 9216, /*//0x3300*/
		.framelength = 2880, /*//0x07F8*/
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592, /*//0x0A20*/
		.grabwindow_height = 1940, /*//0x0794*/
		//grabwindow_height should be 16's N times*/
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 300,
		.mipi_pixel_rate = 240000000,
	},
	.cap1 = {
		.pclk = 678000000, /*//30fps case*/
		.linelength = 5640, /*//0x1608*/
		.framelength = 4008, /*//0x0FA8*/
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 5184, /*//0x1440*/
		.grabwindow_height = 3880, /*//0x0F28*/
		/*//grabwindow_height should be 16's N times*/
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.cap2 = {
		.pclk = 678000000, /*//30fps case*/
		.linelength = 5640, /*//0x1608*/
		.framelength = 4008, /*//0x0FA8*/
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 5184, /*//0x1440*/
		.grabwindow_height = 3880, /*//0x0F28*/
		/*//grabwindow_height should be 16's N times*/
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 800000000, /*//30fps case*/
		.linelength = 9216, /*//0x3300*/
		.framelength = 2880, /*//0x07F8*/
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592, /*//0x0A20*/
		.grabwindow_height = 1940, /*//0x0794*/
		//grabwindow_height should be 16's N times
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 300,
		.mipi_pixel_rate = 240000000,
	},
	.hs_video = {/*slow motion*/
		.pclk = 688000000, /*//30fps case*/
		.linelength = 5608, /*0x0500*/
		.framelength = 1022, /*//0x02d0*/
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280, /*//0x1440*/
		.grabwindow_height = 720, /*//0x0F28*/
		/*//grabwindow_height should be 16's N times*/
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
		.mipi_pixel_rate = 400800000,
	},
	.slim_video = {/*VT Call*/
		.pclk = 688000000, /*//30fps case*/
		.linelength = 9008, /*//0x1608*/
		.framelength = 2544, /*//0x0FA8*/
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280, /*//0x1440*/
		.grabwindow_height = 720, /*//0x0F28*/
		/*//grabwindow_height should be 16's N times*/
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 213600000,
	},
	.margin = 8,		/* sensor framelength & shutter margin */
	.min_shutter = 4,	/* min shutter */

	/* max framelength by sensor register's limitation */
	.max_frame_length = 0xffff,

	/* shutter delay frame for AE cycle,
	 * 2 frame with ispGain_delay-shut_delay=2-0=2
	 */
	.ae_shut_delay_frame = 0,

	/* sensor gain delay frame for AE cycle,
	 * 2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	 */
	.ae_sensor_gain_delay_frame = 0,

	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 5,	/* support sensor mode num */

	.cap_delay_frame = 3,	/* enter capture delay frame num */
	.pre_delay_frame = 3,	/* enter preview delay frame num */
	.video_delay_frame = 3,	/* enter video delay frame num */

	/* enter high speed video  delay frame num */
	.hs_video_delay_frame = 3,

	.slim_video_delay_frame = 3,	/* enter slim video delay frame num */

	.isp_driving_current = ISP_DRIVING_4MA,	/* mclk driving current */

	/* sensor_interface_type */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,

	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,

	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.mipi_settle_delay_mode = 1,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_BAYER_Gb,
	//.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.mclk = 24,	/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	//.i2c_speed = 1000, /*support 1MHz write*/
	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
	//.i2c_addr_table = { 0x20, 0x5a, 0xff},
	.i2c_addr_table = { 0x5a, 0xff},
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_HV_MIRROR,	/* mirrorflip information */

	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.sensor_mode = IMGSENSOR_MODE_INIT,

	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 0,	/* full size current fps : 24fps for PIP,
				 * 30fps for Normal or ZSD
				 */

	/* auto flicker enable: KAL_FALSE for disable auto flicker,
	 * KAL_TRUE for enable auto flicker
	 */
	.autoflicker_en = KAL_FALSE,

		/* test pattern mode or not.
		 * KAL_FALSE for in test pattern mode,
		 * KAL_TRUE for normal output
		 */
	.test_pattern = KAL_FALSE,

	/* current scenario id */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,		/* sensor need support LE, SE with HDR feature */

	.hdr_mode = KAL_FALSE,	/* HDR Mode : 0: disable HDR, 1:IHDR, 2:HDR, 9:ZHDR */
	.i2c_write_id = 0x20,	/* record current sensor's i2c write id */

};

//int chip_id;
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
	{ 2592, 1940, 0, 0,  2592, 1940, 2592, 1940, 0, 0, 2592, 1940,
	0, 0, 2592, 1940},

	{ 2592, 1940, 0, 0,  2592, 1940, 2592, 1940, 0, 0, 2592, 1940,
	0, 0, 2592, 1940},

	{ 2592, 1940, 0, 0,  2592, 1940, 2592, 1940, 0, 0, 2592, 1940,
	0, 0, 2592, 1940},

	{5200, 3880, 1312, 1228, 2576, 1440, 1280,  720, 4, 0, 1280,  720,
	0, 0, 1280,  720},

	{5200, 3896, 1320, 1220, 2576, 1440, 1280,  720, 4, 0, 1280,  720,
	0, 0, 1280,  720},
};


/* no mirror flip, and no binning -revised by dj */
/* static struct struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
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

#define write_cmos_sensor(addr, para) iWriteReg(\
	(u16) addr, (u32) para, 1,  imgsensor.i2c_write_id)
#endif
#define RWB_ID_OFFSET 0x0F73
#define EEPROM_READ_ID  0x19
#define EEPROM_WRITE_ID   0x18

#if 0
static kal_uint16 is_RWB_sensor(void)
{
	kal_uint16 get_byte = 0;

	char pusendcmd[2] = {
		(char)(RWB_ID_OFFSET >> 8), (char)(RWB_ID_OFFSET & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 1, EEPROM_READ_ID);
	return get_byte;
}
#endif

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
	char pusendcmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static kal_uint16 read_eeprom_module_id(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *) &get_byte, 1, 0xA0);//Ofilm

	return get_byte;
}

static kal_uint32 return_sensor_id(void)
{
	kal_uint32 sensor_id = 0;
	kal_uint16 module_id = 0;

	sensor_id = ((read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));
	mdelay(5);
	module_id = read_eeprom_module_id(0x0001);

	if (0x07 == module_id)
		sensor_id += 0;

	printk("[%s] sensor_id: 0x%x module_id: 0x%x", __func__, sensor_id, module_id);

	return sensor_id;
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);

	/* return; //for test */
	write_cmos_sensor(0x0340, imgsensor.frame_length);
	write_cmos_sensor(0x0342, imgsensor.line_length);
}				/*      set_dummy  */


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
}				/*      set_max_framerate  */

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
		realtime_fps = imgsensor.pclk
			/ imgsensor.line_length * 10 / imgsensor.frame_length;

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
		LOG_INF("(else)imgsensor.frame_length = %d\n",
			imgsensor.frame_length);

	}
	/* Update Shutter */
	write_cmos_sensor(0x0202, shutter);
	LOG_INF("shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);

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

	write_cmos_sensor(0x0204, reg_gain);
	/* write_cmos_sensor_8(0x0204,(reg_gain>>8)); */
	/* write_cmos_sensor_8(0x0205,(reg_gain&0xff)); */

	return gain;
}				/*      set_gain  */

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

	mdelay(5);
	for (i = 0; i < timeout; i++) {
		if (read_cmos_sensor_8(0x0005) != 0xFF)
			mdelay(1);
		else
			break;
	}
	LOG_INF(" check_streamoff exit! %d\n", i);
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
		mdelay(5);
		write_cmos_sensor_8(0x0100, 0x01);
		mdelay(5);
	} else {
		write_cmos_sensor_8(0x0100, 0x00);
		check_streamoff();
	}
	return ERROR_NONE;
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
	if ((I2C_BUFFER_LEN - tosend) < 4 || IDX == len || addr != addr_last) {
		iBurstWriteReg_multi(puSendCmd, tosend,
			imgsensor.i2c_write_id, 4, imgsensor_info.i2c_speed);

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

static kal_uint16 addr_data_pair_init_3t1[] = {
	0x6028, 0x2000,
	0x602A, 0x5128,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0449,
	0x6F12, 0x0348,
	0x6F12, 0x044A,
	0x6F12, 0x0860,
	0x6F12, 0x101A,
	0x6F12, 0x8880,
	0x6F12, 0x00F0,
	0x6F12, 0xCFB8,
	0x6F12, 0x2000,
	0x6F12, 0x5404,
	0x6F12, 0x2000,
	0x6F12, 0x4540,
	0x6F12, 0x2000,
	0x6F12, 0x6A00,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x70B5,
	0x6F12, 0x0546,
	0x6F12, 0x754C,
	0x6F12, 0x0022,
	0x6F12, 0xB021,
	0x6F12, 0x00F0,
	0x6F12, 0xFFF8,
	0x6F12, 0x208D,
	0x6F12, 0xE18C,
	0x6F12, 0x411A,
	0x6F12, 0x2846,
	0x6F12, 0x00F0,
	0x6F12, 0xFEF8,
	0x6F12, 0x608D,
	0x6F12, 0xA189,
	0x6F12, 0x411A,
	0x6F12, 0x6F48,
	0x6F12, 0xA0F8,
	0x6F12, 0x1A13,
	0x6F12, 0xB0F8,
	0x6F12, 0x1A13,
	0x6F12, 0x0029,
	0x6F12, 0xFBD1,
	0x6F12, 0x70BD,
	0x6F12, 0x2DE9,
	0x6F12, 0xF047,
	0x6F12, 0x9046,
	0x6F12, 0x0E46,
	0x6F12, 0x0446,
	0x6F12, 0x1EE0,
	0x6F12, 0x3846,
	0x6F12, 0x00F0,
	0x6F12, 0xEFF8,
	0x6F12, 0x8146,
	0x6F12, 0x7D03,
	0x6F12, 0x3846,
	0x6F12, 0x05F5,
	0x6F12, 0x0055,
	0x6F12, 0xFFF7,
	0x6F12, 0xD8FF,
	0x6F12, 0x6349,
	0x6F12, 0x7020,
	0x6F12, 0xA1F8,
	0x6F12, 0x0E03,
	0x6F12, 0xC4F3,
	0x6F12, 0x0C01,
	0x6F12, 0xAE42,
	0x6F12, 0x00D2,
	0x6F12, 0x3546,
	0x6F12, 0x2D1B,
	0x6F12, 0x2B46,
	0x6F12, 0x4246,
	0x6F12, 0x3846,
	0x6F12, 0x00F0,
	0x6F12, 0xDEF8,
	0x6F12, 0x2C44,
	0x6F12, 0xA844,
	0x6F12, 0x4946,
	0x6F12, 0x3846,
	0x6F12, 0x00F0,
	0x6F12, 0xDDF8,
	0x6F12, 0x670B,
	0x6F12, 0x01D1,
	0x6F12, 0xB442,
	0x6F12, 0xDCD3,
	0x6F12, 0xBDE8,
	0x6F12, 0xF087,
	0x6F12, 0x70B5,
	0x6F12, 0x0446,
	0x6F12, 0x5648,
	0x6F12, 0x0022,
	0x6F12, 0x4168,
	0x6F12, 0x0D0C,
	0x6F12, 0x8EB2,
	0x6F12, 0x3146,
	0x6F12, 0x2846,
	0x6F12, 0x00F0,
	0x6F12, 0xD1F8,
	0x6F12, 0x2046,
	0x6F12, 0x00F0,
	0x6F12, 0xD3F8,
	0x6F12, 0x34F8,
	0x6F12, 0x660F,
	0x6F12, 0x3146,
	0x6F12, 0xA080,
	0x6F12, 0x2846,
	0x6F12, 0xBDE8,
	0x6F12, 0x7040,
	0x6F12, 0x0122,
	0x6F12, 0x00F0,
	0x6F12, 0xC4B8,
	0x6F12, 0x2DE9,
	0x6F12, 0xF041,
	0x6F12, 0x0646,
	0x6F12, 0x4948,
	0x6F12, 0x0D46,
	0x6F12, 0x8268,
	0x6F12, 0x140C,
	0x6F12, 0x97B2,
	0x6F12, 0x0022,
	0x6F12, 0x3946,
	0x6F12, 0x2046,
	0x6F12, 0x00F0,
	0x6F12, 0xB7F8,
	0x6F12, 0x2946,
	0x6F12, 0x3046,
	0x6F12, 0x00F0,
	0x6F12, 0xBDF8,
	0x6F12, 0x434D,
	0x6F12, 0x4010,
	0x6F12, 0x0122,
	0x6F12, 0xE860,
	0x6F12, 0x3946,
	0x6F12, 0x2046,
	0x6F12, 0x00F0,
	0x6F12, 0xABF8,
	0x6F12, 0xE868,
	0x6F12, 0x4000,
	0x6F12, 0xE860,
	0x6F12, 0xBDE8,
	0x6F12, 0xF081,
	0x6F12, 0x2DE9,
	0x6F12, 0xF047,
	0x6F12, 0x0446,
	0x6F12, 0x3A48,
	0x6F12, 0x9146,
	0x6F12, 0x8846,
	0x6F12, 0xC068,
	0x6F12, 0x1E46,
	0x6F12, 0x87B2,
	0x6F12, 0x050C,
	0x6F12, 0x0022,
	0x6F12, 0x3946,
	0x6F12, 0x2846,
	0x6F12, 0x00F0,
	0x6F12, 0x97F8,
	0x6F12, 0x3346,
	0x6F12, 0x4A46,
	0x6F12, 0x4146,
	0x6F12, 0x2046,
	0x6F12, 0x00F0,
	0x6F12, 0xA0F8,
	0x6F12, 0x0646,
	0x6F12, 0x0122,
	0x6F12, 0x3946,
	0x6F12, 0x2846,
	0x6F12, 0x00F0,
	0x6F12, 0x8BF8,
	0x6F12, 0x3048,
	0x6F12, 0x90F8,
	0x6F12, 0xD100,
	0x6F12, 0x0128,
	0x6F12, 0x04D1,
	0x6F12, 0x2068,
	0x6F12, 0xC107,
	0x6F12, 0x01D0,
	0x6F12, 0x401C,
	0x6F12, 0x2060,
	0x6F12, 0x2C48,
	0x6F12, 0x6188,
	0x6F12, 0xA0F8,
	0x6F12, 0x4013,
	0x6F12, 0x2168,
	0x6F12, 0x090C,
	0x6F12, 0xA0F8,
	0x6F12, 0x4213,
	0x6F12, 0x3046,
	0x6F12, 0x98E7,
	0x6F12, 0x70B5,
	0x6F12, 0x2848,
	0x6F12, 0x0088,
	0x6F12, 0x40B1,
	0x6F12, 0x2449,
	0x6F12, 0x096E,
	0x6F12, 0x4843,
	0x6F12, 0x4FF4,
	0x6F12, 0x7A71,
	0x6F12, 0xB0FB,
	0x6F12, 0xF1F0,
	0x6F12, 0x00F0,
	0x6F12, 0x7EF8,
	0x6F12, 0x1E48,
	0x6F12, 0x0022,
	0x6F12, 0x0169,
	0x6F12, 0x0C0C,
	0x6F12, 0x8DB2,
	0x6F12, 0x2946,
	0x6F12, 0x2046,
	0x6F12, 0x00F0,
	0x6F12, 0x61F8,
	0x6F12, 0x00F0,
	0x6F12, 0x78F8,
	0x6F12, 0x2946,
	0x6F12, 0x2046,
	0x6F12, 0xBDE8,
	0x6F12, 0x7040,
	0x6F12, 0x0122,
	0x6F12, 0x00F0,
	0x6F12, 0x58B8,
	0x6F12, 0x10B5,
	0x6F12, 0x0022,
	0x6F12, 0xAFF2,
	0x6F12, 0x6311,
	0x6F12, 0x1848,
	0x6F12, 0x00F0,
	0x6F12, 0x6FF8,
	0x6F12, 0x114C,
	0x6F12, 0x0022,
	0x6F12, 0xAFF2,
	0x6F12, 0x1D11,
	0x6F12, 0x2060,
	0x6F12, 0x1548,
	0x6F12, 0x00F0,
	0x6F12, 0x67F8,
	0x6F12, 0x0022,
	0x6F12, 0xAFF2,
	0x6F12, 0xF901,
	0x6F12, 0x6060,
	0x6F12, 0x1248,
	0x6F12, 0x00F0,
	0x6F12, 0x60F8,
	0x6F12, 0x0022,
	0x6F12, 0xAFF2,
	0x6F12, 0xCD01,
	0x6F12, 0xA060,
	0x6F12, 0x1048,
	0x6F12, 0x00F0,
	0x6F12, 0x59F8,
	0x6F12, 0x0022,
	0x6F12, 0xAFF2,
	0x6F12, 0x7B01,
	0x6F12, 0xE060,
	0x6F12, 0x0D48,
	0x6F12, 0x00F0,
	0x6F12, 0x52F8,
	0x6F12, 0x2061,
	0x6F12, 0x10BD,
	0x6F12, 0x0000,
	0x6F12, 0x2000,
	0x6F12, 0x4F80,
	0x6F12, 0x4000,
	0x6F12, 0x8000,
	0x6F12, 0x2000,
	0x6F12, 0x53F0,
	0x6F12, 0x2000,
	0x6F12, 0x42B0,
	0x6F12, 0x2000,
	0x6F12, 0x4580,
	0x6F12, 0x4000,
	0x6F12, 0xC000,
	0x6F12, 0x2000,
	0x6F12, 0x6A00,
	0x6F12, 0x0000,
	0x6F12, 0xF66F,
	0x6F12, 0x0000,
	0x6F12, 0x8F23,
	0x6F12, 0x0000,
	0x6F12, 0x72F5,
	0x6F12, 0x0000,
	0x6F12, 0xDB2D,
	0x6F12, 0x0000,
	0x6F12, 0x6DE3,
	0x6F12, 0x4FF2,
	0x6F12, 0x992C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x4FF2,
	0x6F12, 0x2D3C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x4FF2,
	0x6F12, 0x1B2C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x4FF2,
	0x6F12, 0x554C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x4FF2,
	0x6F12, 0x8F2C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x40F6,
	0x6F12, 0xE71C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x48F6,
	0x6F12, 0x237C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x47F2,
	0x6F12, 0xF52C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x4DF6,
	0x6F12, 0x2D3C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x4CF6,
	0x6F12, 0x114C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x46F6,
	0x6F12, 0xE35C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x43F6,
	0x6F12, 0x756C,
	0x6F12, 0xC0F2,
	0x6F12, 0x010C,
	0x6F12, 0x6047,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6028, 0x2000,
	0x602A, 0x5122,
	0x6F12, 0x0002,
	0x6F12, 0x0100,
	0x602A, 0x5112,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x6A00,
	0x6F12, 0x0F00,
	0x602A, 0x1D66,
	0x6F12, 0xAD10,
	0x602A, 0x1D60,
	0x6F12, 0x006F,
	0x602A, 0x1D2E,
	0x6F12, 0x1310,
	0x602A, 0x1D32,
	0x6F12, 0x0605,
	0x602A, 0x1D36,
	0x6F12, 0x0305,
	0x602A, 0x1D3A,
	0x6F12, 0x0305,
	0x602A, 0x1D52,
	0x6F12, 0x6819,
	0x602A, 0x1D56,
	0x6F12, 0x01FF,
	0xF262, 0x0001,
	0xF404, 0x0FFF,
	0xF42A, 0x0080,
	0xF430, 0x84A1,
	0xF432, 0x7542,
	0xF464, 0x000E,
	0xF466, 0x000E,
	0xF468, 0x0018,
	0xF46A, 0x0010,
	0xF474, 0x0014,
	0xF476, 0x000A,
	0xF478, 0x0000,
	0xF484, 0x0000,
	0xF4B6, 0x003C,
	0xF4B8, 0x0044,
	0xF4BE, 0x0FAC,
	0xF4C0, 0x0FBC,
	0xF4C2, 0x0FAD,
	0xF4C4, 0x0FBD,
	0xF4C6, 0x0FB4,
	0xF4C8, 0x0FC4,
	0xF4CA, 0x0FB5,
	0xF4CC, 0x0FC5,
	0xF48E, 0x0200,
	0xF42E, 0x0000,
	0x6028, 0x2000,
	0x602A, 0x106E,
	0x6F12, 0x0004,
	0x602A, 0x1086,
	0x6F12, 0x0005,
	0x602A, 0x10E6,
	0x6F12, 0x0010,
	0x602A, 0x10EE,
	0x6F12, 0x000A,
	0x602A, 0x10F6,
	0x6F12, 0x0006,
	0x602A, 0x110E,
	0x6F12, 0x0014,
	0x602A, 0x113E,
	0x6F12, 0x0000,
	0x602A, 0x126E,
	0x6F12, 0x0011,
	0x602A, 0x1276,
	0x6F12, 0x0032,
	0x602A, 0x13D6,
	0x6F12, 0x0017,
	0x602A, 0x15EE,
	0x6F12, 0x0000,
	0x602A, 0x1068,
	0x6F12, 0x0009,
	0x602A, 0x1070,
	0x6F12, 0x000F,
	0x602A, 0x1080,
	0x6F12, 0x0020,
	0x602A, 0x1088,
	0x6F12, 0x0010,
	0x602A, 0x1090,
	0x6F12, 0x0014,
	0x602A, 0x10E8,
	0x6F12, 0x0010,
	0x602A, 0x10F0,
	0x6F12, 0x000A,
	0x602A, 0x10F8,
	0x6F12, 0x0014,
	0x602A, 0x1108,
	0x6F12, 0x0129,
	0x602A, 0x1110,
	0x6F12, 0x0014,
	0x602A, 0x1140,
	0x6F12, 0x0000,
	0x602A, 0x1148,
	0x6F12, 0x0009,
	0x602A, 0x1150,
	0x6F12, 0x000F,
	0x602A, 0x1160,
	0x6F12, 0x001E,
	0x602A, 0x1268,
	0x6F12, 0x0007,
	0x602A, 0x1270,
	0x6F12, 0x0011,
	0x602A, 0x1278,
	0x6F12, 0x0032,
	0x602A, 0x13D0,
	0x6F12, 0x0014,
	0x602A, 0x13D8,
	0x6F12, 0x008C,
	0x602A, 0x13E0,
	0x6F12, 0x008C,
	0x602A, 0x1D08,
	0x6F12, 0x0330,
	0x021E, 0x0000,
	0x0202, 0x0200,
	0x0226, 0x0400,
	0x0704, 0x0000,
	0x6028, 0x2000,
	0x602A, 0x0FEC,
	0x6F12, 0x2020,
	0x602A, 0x1000,
	0x6F12, 0x14B0,
	0x6F12, 0x0058,
	0x6F12, 0x1008,
	0x6F12, 0x0100,
	0x6F12, 0x0000,
	0x6F12, 0x0800,
	0x602A, 0x100E,
	0x6F12, 0x0020,
	0x6F12, 0x0000,
	0x602A, 0x0FE6,
	0x6F12, 0x0006,
	0x602A, 0x0FE4,
	0x6F12, 0x0010,
	0x602A, 0x0FE2,
	0x6F12, 0x0010,
	0x602A, 0x0FF8,
	0x6F12, 0x0FE0,
	0x6F12, 0x0008,
	0x602A, 0x0FFE,
	0x6F12, 0x0008,
	0x602A, 0x0FCE,
	0x6F12, 0x0004,
	0x602A, 0x2F88,
	0x6F12, 0x0000,
	0x602A, 0x2E6C,
	0x6F12, 0x0001,
	0x602A, 0x2F90,
	0x6F12, 0x2001,
	0x602A, 0x2F8C,
	0x6F12, 0x8010,
	0x602A, 0x2070,
	0x6F12, 0x0000,
	0x602A, 0x2C02,
	0x6F12, 0x1010,
	0x6F12, 0x1010,
	0x602A, 0x2B0C,
	0x6F12, 0x0030,
	0x6F12, 0x0081,
	0x602A, 0x2B78,
	0x6F12, 0x02C0,
	0x6F12, 0xC0C0,
	0x602A, 0x2B4A,
	0x6F12, 0x0001,
	0x602A, 0x2BFC,
	0x6F12, 0x0D00,
	0x0BC0, 0x0040,
	0x0BCE, 0x0040,
	0x6028, 0x2000,
	0x602A, 0x00AE,
	0x6F12, 0x0100,
	0x602A, 0x005C,
	0x6F12, 0x0100,
	0x0B06, 0x0100,
	0x6028, 0x2000,
	0x602A, 0x00B4,
	0x6F12, 0x0000,
	0x6F12, 0x03FF,
	0x6F12, 0x0000,
	0x6F12, 0x03FF,
	0x602A, 0x0060,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x05B4,
	0x6F12, 0x0000,
	0x6F12, 0x0020,
	0x602A, 0x4226,
	0x6F12, 0x0001,
	0x602A, 0x00E0,
	0x6F12, 0x0000,
	0x602A, 0x0108,
	0x6F12, 0x1000,
	0x6F12, 0x4000,
	0x6F12, 0x8000,
	0x6F12, 0xC000,
	0x6F12, 0xFFFF,
	0x602A, 0x036A,
	0x6F12, 0x0001,
	0x602A, 0x0398,
	0x6F12, 0x0001,
	0x602A, 0x03C6,
	0x6F12, 0x0001,
	0x602A, 0x03F4,
	0x6F12, 0x0001,
	0x602A, 0x0422,
	0x6F12, 0x0001,
	0x0FE8, 0x0B41,
	0x3036, 0x0000,
	0x0D00, 0x0001,
	0x6028, 0x2000,
	0x602A, 0x3000,
	0x6F12, 0x0101,
	0x602A, 0x39C4,
	0x6F12, 0x0100,
	0x602A, 0x0FDA,
	0x6F12, 0x0006,
	0x3070, 0x0300,
	0x306E, 0x0004,
	0xB134, 0x0080,
	0xB13C, 0x0CD0,
	0x6592, 0x0010,
	0x3026, 0x0001,
	0x602A, 0x2BEE,
	0x6F12, 0x143C,
	0x602A, 0x2BF2,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000
};

static void sensor_init(void)
{
	/* initial sequence */
	// Convert from : "InitGlobal.sset"
	LOG_INF("[%s] +", __FUNCTION__);

	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0000, 0x0005);
	write_cmos_sensor(0x0000, 0x3141);
	write_cmos_sensor(0x6010, 0x0001);
	mdelay(3);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0A02, 0x007C);

	table_write_cmos_sensor(addr_data_pair_init_3t1,
		sizeof(addr_data_pair_init_3t1) / sizeof(kal_uint16));
	LOG_INF("[%s] -", __FUNCTION__);
}				/*      sensor_init  */

static kal_uint16 addr_data_pair_pre_3t1[] = {
	0x6028, 0x4000,
	0x0136, 0x1800,
	0x013E, 0x0000,
	0x0304, 0x0003,
	0x0306, 0x00AF,
	0x0302, 0x0001,
	0x0300, 0x0007,
	0x030E, 0x0004,
	0x0310, 0x0190,
	0x030C, 0x0000,
	0x0312, 0x0002,
	0x3004, 0x0005,
	0x6028, 0x2000,
	0x602A, 0x1D50,
	0x6F12, 0x0096,
	0x0344, 0x0000,
	0x0346, 0x0000,
	0x0348, 0x143F,
	0x034A, 0x0F27,
	0x034C, 0x0A20,
	0x034E, 0x0794,
	0x0350, 0x0000,
	0x0352, 0x0000,
	0x0342, 0x2400,
	0x0340, 0x0B40,
	0x0702, 0x0000,
	0x0900, 0x0111,
	0x0380, 0x0002,
	0x0382, 0x0002,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0BC6, 0x0000,
	0x0400, 0x1010,
	0x0402, 0x1010,
	0x6028, 0x2000,
	0x602A, 0x1056,
	0x6F12, 0x0300,
	0x602A, 0x0FC0,
	0x6F12, 0x0004,
	0x6F12, 0x0A58,
	0x6F12, 0x0804,
	0x6F12, 0x1440,
	0x6F12, 0x0F28,
	0x602A, 0x2BFC,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0010,
	0x6F12, 0x1000,
	0x6F12, 0x1010,
	0x3030, 0x0000,
	0x3034, 0x0000,
	0x6028, 0x2000,
	0x602A, 0x0FA0,
	0x6F12, 0xFFFF,
	0x303C, 0x0000,
	0x0114, 0x0300,
	0x6028, 0x2000,
	0x602A, 0x5110,
	0x6F12, 0x0000,
	0x602A, 0x5116,
	0x6F12, 0x0048,
	0x6F12, 0x0048,
	0x6F12, 0x0048,
	0x602A, 0x1D16,
	0x6F12, 0x0101,
	0x602A, 0x1D10,
	0x6F12, 0x0101,
	0x602A, 0x1D54,
	0x6F12, 0x00BF,
	0x602A, 0x3900,
	0x6F12, 0x0100,
	0x602A, 0x3904,
	0x6F12, 0x005F,
	0x602A, 0x390A,
	0x6F12, 0x005F,
	0x602A, 0x3910,
	0x6F12, 0x0020,
	0x6F12, 0x0020,
	0x6F12, 0xF42E,
	0x6218, 0x7150,
	0xF482, 0x01A4
};

static void preview_setting(void)
{
	LOG_INF("[%s] +\n", __FUNCTION__);

	/* Convert from : "3T1_5M_2592x1940_30fps_MIPI534mbps.sset"*/


	/*$MV1[MCLK:24,Width:2592,Height:1940,Format:MIPI_RAW10,mipi_lane:4*/
	/*,mipi_datarate:534,pvi_pclk_inverse:0]*/

	/* ExtClk :	24	MHz*/
	/* Vt_pix_clk :	688	MHz*/
	/* MIPI_output_speed :	534	Mbps/lane*/
	/* Crop_Width :	5200	px*/
	/* Crop_Height :	3880	px*/
	/* Output_Width :	2592	px*/
	/* Output_Height :	1940	px*/
	/* Frame rate :	30.02	fps*/
	/* Output format :	Raw10*/
	/* H-size :	9008	px*/
	/* H-blank :	6416	px*/
	/* V-size :	2544	line*/
	/* V-blank :	604	line*/
	/* Lane :	4	lane*/
	/* First Pixel :	Gr	First*/

	table_write_cmos_sensor(addr_data_pair_pre_3t1,
			sizeof(addr_data_pair_pre_3t1) / sizeof(kal_uint16));
	LOG_INF("[%s] -", __FUNCTION__);

}				/*      preview_setting  */

static kal_uint16 addr_data_pair_cap_3t1[] = {
	0x6028, 0x4000,
	0x0136, 0x1800,
	0x013E, 0x0000,
	0x0304, 0x0003,
	0x0306, 0x00AF,
	0x0302, 0x0001,
	0x0300, 0x0007,
	0x030E, 0x0004,
	0x0310, 0x0190,
	0x030C, 0x0000,
	0x0312, 0x0002,
	0x3004, 0x0005,
	0x6028, 0x2000,
	0x602A, 0x1D50,
	0x6F12, 0x0096,
	0x0344, 0x0000,
	0x0346, 0x0000,
	0x0348, 0x143F,
	0x034A, 0x0F27,
	0x034C, 0x0A20,
	0x034E, 0x0794,
	0x0350, 0x0000,
	0x0352, 0x0000,
	0x0342, 0x2400,
	0x0340, 0x0B40,
	0x0702, 0x0000,
	0x0900, 0x0111,
	0x0380, 0x0002,
	0x0382, 0x0002,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0BC6, 0x0000,
	0x0400, 0x1010,
	0x0402, 0x1010,
	0x6028, 0x2000,
	0x602A, 0x1056,
	0x6F12, 0x0300,
	0x602A, 0x0FC0,
	0x6F12, 0x0004,
	0x6F12, 0x0A58,
	0x6F12, 0x0804,
	0x6F12, 0x1440,
	0x6F12, 0x0F28,
	0x602A, 0x2BFC,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0010,
	0x6F12, 0x1000,
	0x6F12, 0x1010,
	0x3030, 0x0000,
	0x3034, 0x0000,
	0x6028, 0x2000,
	0x602A, 0x0FA0,
	0x6F12, 0xFFFF,
	0x303C, 0x0000,
	0x0114, 0x0300,
	0x6028, 0x2000,
	0x602A, 0x5110,
	0x6F12, 0x0000,
	0x602A, 0x5116,
	0x6F12, 0x0048,
	0x6F12, 0x0048,
	0x6F12, 0x0048,
	0x602A, 0x1D16,
	0x6F12, 0x0101,
	0x602A, 0x1D10,
	0x6F12, 0x0101,
	0x602A, 0x1D54,
	0x6F12, 0x00BF,
	0x602A, 0x3900,
	0x6F12, 0x0100,
	0x602A, 0x3904,
	0x6F12, 0x005F,
	0x602A, 0x390A,
	0x6F12, 0x005F,
	0x602A, 0x3910,
	0x6F12, 0x0020,
	0x6F12, 0x0020,
	0x6F12, 0xF42E,
	0x6218, 0x7150,
	0xF482, 0x01A4
};

static void capture_setting(kal_uint16 currefps)
{
/*
 * /  write_cmos_sensor(0x6028, 0x4000);
 *    write_cmos_sensor_8(0x0100, 0x00);
 *    if (currefps == 150){
 *	 Convert from : "Init.txt"
 *	 No setfile ready yet    } else if (currefps == 240){
 *	 Convert from : "Init.txt"
 *
 *
 *	No setfile ready yet    } else {	//30fps
 *	Convert from : "3T1_20M_5184x3880_30fps_MIPI1680mbps.sset"
 *
 *
 *	$MV1[MCLK:24,Width:5184,Height:3880,Format:MIPI_RAW10,mipi_lane:4
 *         ,mipi_datarate:1680,pvi_pclk_inverse:0]
 *	 ExtClk :	24	MHz
 *	 Vt_pix_clk :	678.4	MHz
 *	 MIPI_output_speed :	1680	Mbps/lane
 *	 Crop_Width :	5184	px
 *	 Crop_Height :	3880	px
 *	 Output_Width :	5184	px
 *	 Output_Height :	3880	px
 *	 Frame rate :	30.01	fps
 *	 Output format :	Raw10
 *	 H-size :	5640	px
 *	 H-blank :	456	px
 *	 V-size :	4008	line
 *	 V-blank :	128	line
 *	 Lane :	4	lane
 *	 First Pixel :	Gr	First
 */
	LOG_INF("[%s] currefps:%d +", __FUNCTION__, currefps);
	table_write_cmos_sensor(addr_data_pair_cap_3t1,
			sizeof(addr_data_pair_cap_3t1) / sizeof(kal_uint16));
	LOG_INF("[%s] -", __FUNCTION__);

}

static kal_uint16 addr_data_pair_video_3t1[] = {
	0x6028, 0x4000,
	0x0136, 0x1800,
	0x013E, 0x0000,
	0x0304, 0x0003,
	0x0306, 0x00AF,
	0x0302, 0x0001,
	0x0300, 0x0007,
	0x030E, 0x0004,
	0x0310, 0x0190,
	0x030C, 0x0000,
	0x0312, 0x0002,
	0x3004, 0x0005,
	0x6028, 0x2000,
	0x602A, 0x1D50,
	0x6F12, 0x0096,
	0x0344, 0x0000,
	0x0346, 0x0000,
	0x0348, 0x143F,
	0x034A, 0x0F27,
	0x034C, 0x0A20,
	0x034E, 0x0794,
	0x0350, 0x0000,
	0x0352, 0x0000,
	0x0342, 0x2400,
	0x0340, 0x0B40,
	0x0702, 0x0000,
	0x0900, 0x0111,
	0x0380, 0x0002,
	0x0382, 0x0002,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0BC6, 0x0000,
	0x0400, 0x1010,
	0x0402, 0x1010,
	0x6028, 0x2000,
	0x602A, 0x1056,
	0x6F12, 0x0300,
	0x602A, 0x0FC0,
	0x6F12, 0x0004,
	0x6F12, 0x0A58,
	0x6F12, 0x0804,
	0x6F12, 0x1440,
	0x6F12, 0x0F28,
	0x602A, 0x2BFC,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0010,
	0x6F12, 0x1000,
	0x6F12, 0x1010,
	0x3030, 0x0000,
	0x3034, 0x0000,
	0x6028, 0x2000,
	0x602A, 0x0FA0,
	0x6F12, 0xFFFF,
	0x303C, 0x0000,
	0x0114, 0x0300,
	0x6028, 0x2000,
	0x602A, 0x5110,
	0x6F12, 0x0000,
	0x602A, 0x5116,
	0x6F12, 0x0048,
	0x6F12, 0x0048,
	0x6F12, 0x0048,
	0x602A, 0x1D16,
	0x6F12, 0x0101,
	0x602A, 0x1D10,
	0x6F12, 0x0101,
	0x602A, 0x1D54,
	0x6F12, 0x00BF,
	0x602A, 0x3900,
	0x6F12, 0x0100,
	0x602A, 0x3904,
	0x6F12, 0x005F,
	0x602A, 0x390A,
	0x6F12, 0x005F,
	0x602A, 0x3910,
	0x6F12, 0x0020,
	0x6F12, 0x0020,
	0x6F12, 0xF42E,
	0x6218, 0x7150,
	0xF482, 0x01A4

};

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("normal_video_setting() E! currefps:%d\n", currefps);

	/*Convert from : "3T1_20M_5184x3880_30fps_MIPI1680mbps.sset"*/



	/*//$MV1[MCLK:24,Width:5184,Height:3880,
	 *Format:MIPI_RAW10,mipi_lane:4,mipi_datarate:1680,pvi_pclk_inverse:0]
	 */
	/*
	 * ExtClk :	24	MHz
	 * Vt_pix_clk :	678.4	MHz
	 * MIPI_output_speed :	1680	Mbps/lane
	 * Crop_Width :	5184	px
	 * Crop_Height :	3880	px
	 * Output_Width :	5184	px
	 * Output_Height :	3880	px
	 * Frame rate :	30.01	fps
	 * Output format :	Raw10
	 * H-size :	5640	px
	 * H-blank :	456	px
	 * V-size :	4008	line
	 * V-blank :	128	line
	 * Lane :	4	lane
	 * First Pixel :	Gr	First
	 */

	table_write_cmos_sensor(addr_data_pair_video_3t1,
		sizeof(addr_data_pair_video_3t1) / sizeof(kal_uint16));

//	table_write_cmos_sensor(addr_data_pair_pre_3t1,
//		sizeof(addr_data_pair_pre_3t1) / sizeof(kal_uint16));

	LOG_INF("[%s] -", __FUNCTION__);
}

static kal_uint16 addr_data_pair_hs_3t1[] = {
	0x6028, 0x2000, /*new*/
	0x0344, 0x0520,
	0x0346, 0x04CC,
	0x0348, 0x0F2F,
	0x034A, 0x0A6B,
	0x034C, 0x0500,
	0x034E, 0x02D0,
	0x0408, 0x0004,
	0x040A, 0x0000,
	0x0900, 0x0112,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0400, 0x0000,
	0x0404, 0x0010,
	0x301E, 0x0210,
	0x0110, 0x0002,
	0x0114, 0x0300,
	0x0136, 0x1800,
	0x0300, 0x0005,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00D7,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0004,
	0x030E, 0x00a7,
	0x0310, 0x0100,
	0x0312, 0x0100,
	0x0340, 0x03FE,
	0x0342, 0x15E8,
	0x602A, 0x1C78,
	0x6F12, 0x8101
};

static void hs_video_setting(void)
{
	LOG_INF("hs_video_setting() E\n");

	/*//VGA 120fps*/

	/*// Convert from : "Init.txt"*/
	/*check_streamoff();*/
	table_write_cmos_sensor(addr_data_pair_hs_3t1,
			sizeof(addr_data_pair_hs_3t1) / sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_slim_3t1[] = {
	0x6028, 0x2000,
	0x0344, 0x0520,
	0x0346, 0x04CC,
	0x0348, 0x0F2F,
	0x034A, 0x0A6B,
	0x034C, 0x0500,
	0x034E, 0x02D0,
	0x0408, 0x0004,
	0x040A, 0x0000,
	0x0900, 0x0112,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0400, 0x0000,
	0x0404, 0x0010,
	0x301E, 0x0210,
	0x0110, 0x0002,
	0x0114, 0x0300,
	0x0136, 0x1800,
	0x0300, 0x0005,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00D7,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0004,
	0x030E, 0x0059,
	0x0310, 0x0100,
	0x0312, 0x0100,
	0x0340, 0x09F0,
	0x0342, 0x2330,
	0x602A, 0x1C78,
	0x6F12, 0x8101
};

static void slim_video_setting(void)
{
	LOG_INF("slim_video_setting() E\n");
	/* 1080p 60fps */

	/* Convert from : "Init.txt"*/

	table_write_cmos_sensor(addr_data_pair_slim_3t1,
		sizeof(addr_data_pair_slim_3t1) / sizeof(kal_uint16));

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
//	kal_uint16 sp8spFlag = 0;

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();

			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail, id: 0x%x\n",
				imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
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
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;

	LOG_INF("%s +", __func__);

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */
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

			LOG_INF("Read sensor id fail, id: 0x%x\n",
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
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	LOG_INF("%s -", __func__);
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

		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate) {
			LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				imgsensor.current_fps,
				imgsensor_info.cap.max_framerate / 10);
		}

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

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
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
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
{
	LOG_INF("get_resolution E\n");
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
	/*LOG_INF("get_info -> scenario_id = %d\n", scenario_id);*/

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

	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;

	/* The frame of setting sensor gain*/
	sensor_info->AESensorGainDelayFrame =
				imgsensor_info.ae_sensor_gain_delay_frame;

	sensor_info->AEISPGainDelayFrame =
				imgsensor_info.ae_ispGain_delay_frame;

	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	/* change pdaf support mode to pdaf VC mode */
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
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}				/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	/* //LOG_INF("framerate = %d\n ", framerate); */
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
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)		/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id,	MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

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
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		else {
			/*No need to set*/
			LOG_INF("frame_length %d < shutter %d",
				imgsensor.frame_length, imgsensor.shutter);
		}
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk
		    / framerate * 10 / imgsensor_info.normal_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
	    (frame_length > imgsensor_info.normal_video.framelength)
	  ? (frame_length - imgsensor_info.normal_video.  framelength) : 0;

		imgsensor.frame_length =
		 imgsensor_info.normal_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		else {
			/*No need to set*/
			LOG_INF("frame_length %d < shutter %d",
				imgsensor.frame_length, imgsensor.shutter);
		}
		break;

	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {

		frame_length = imgsensor_info.cap1.pclk
			/ framerate * 10 / imgsensor_info.cap1.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		      (frame_length > imgsensor_info.cap1.framelength)
		    ? (frame_length - imgsensor_info.cap1.  framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.cap1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
	} else if (imgsensor.current_fps == imgsensor_info.cap2.max_framerate) {
		frame_length = imgsensor_info.cap2.pclk
			/ framerate * 10 / imgsensor_info.cap2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		      (frame_length > imgsensor_info.cap2.framelength)
		    ? (frame_length - imgsensor_info.cap2.  framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.cap2.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				framerate,
				imgsensor_info.cap.max_framerate / 10);

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
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		else {
			/*No need to set*/
			LOG_INF("frame_length %d < shutter %d",
				imgsensor.frame_length, imgsensor.shutter);
		}
		break;

	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk
			/ framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.hs_video.framelength)
		? (frame_length - imgsensor_info.hs_video.  framelength) : 0;

		imgsensor.frame_length =
		    imgsensor_info.hs_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		else {
			/*No need to set*/
			LOG_INF("frame_length %d < shutter %d",
				imgsensor.frame_length, imgsensor.shutter);
		}
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk
			/ framerate * 10 / imgsensor_info.slim_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.slim_video.framelength)
		? (frame_length - imgsensor_info.slim_video.  framelength) : 0;

		imgsensor.frame_length =
		  imgsensor_info.slim_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		else {
			/*No need to set*/
			LOG_INF("frame_length %d < shutter %d",
				imgsensor.frame_length, imgsensor.shutter);
		}
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
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		else {
			/*No need to set*/
			LOG_INF("frame_length %d < shutter %d",
				imgsensor.frame_length, imgsensor.shutter);
		}
		LOG_INF("error scenario_id = %d, we use preview scenario\n",
		scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	/*LOG_INF("scenario_id = %d\n", scenario_id);*/

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
/* 0 : Normal, 1 : Solid Color, 2 : Color Bar, 3 : Shade Color Bar, 4 : PN9 */
		write_cmos_sensor(0x0600, 0x0002);
	} else {
		write_cmos_sensor(0x0600, 0x0000);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}
static kal_uint32 get_sensor_temperature(void)
{
	UINT8 temperature;
	INT32 temperature_convert;

	temperature = read_cmos_sensor_8(0x013a);

	if (temperature >= 0x0 && temperature <= 0x78)
		temperature_convert = temperature;
	else
		temperature_convert = -1;

	/*pr_info("temp_c(%d), read_reg(%d), enable %d\n",
	 *	temperature_convert, temperature, read_cmos_sensor_8(0x0138));
	 */

	return temperature_convert;
}
#if 1
#define FOUR_CELL_SIZE 3072//size = 3072 = 0xc00
static int Is_Read_4Cell;
static char Four_Cell_Array[FOUR_CELL_SIZE + 2];

static void read_4cell_from_eeprom(char *data)
{
	int ret;
	int addr = 0x763;/*Start of 4 cell data*/
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	pu_send_cmd[0] = (char)(addr >> 8);
	pu_send_cmd[1] = (char)(addr & 0xFF);

	/* Check I2C is normal */
	ret = iReadRegI2C(pu_send_cmd, 2, data, 1, EEPROM_READ_ID);
	if (ret != 0) {
		LOG_INF("iReadRegI2C error");
		return;
	}

	if (Is_Read_4Cell != 1) {
		LOG_INF("Need to read i2C");

		Four_Cell_Array[0] = (FOUR_CELL_SIZE & 0xff);/*Low*/
		Four_Cell_Array[1] = ((FOUR_CELL_SIZE >> 8) & 0xff);/*High*/

		/*Multi-Read*/
		iReadRegI2C(pu_send_cmd, 2, &Four_Cell_Array[2],
					FOUR_CELL_SIZE, EEPROM_READ_ID);
		Is_Read_4Cell = 1;
	}

	memcpy(data, Four_Cell_Array, FOUR_CELL_SIZE);
}
#endif

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id, UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;

	/* struct SET_PD_BLOCK_INFO_T *PDAFinfo; */
	/* struct SENSOR_VC_INFO_STRUCT *pvcinfo; */
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

//	LOG_INF("feature_id = %d[%s]\n", feature_id, features[feature_id-SENSOR_FEATURE_START]);
//	LOG_INF_U("feature_id = %d[%s]\n", feature_id, features[feature_id-SENSOR_FEATURE_START]);
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

	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
#if 0
		LOG_INF(
			"feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n",
			imgsensor.pclk, imgsensor.current_fps);
#endif
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
	/* night_mode((BOOL) *feature_data); no need to implement this mode */
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
			read_cmos_sensor_8(sensor_reg_data->RegAddr);
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
		set_auto_flicker_mode((BOOL) (*feature_data_16),
					*(feature_data_16 + 1));
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
		LOG_INF("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.hdr_mode = (UINT8)*feature_data_32;
		/* imgsensor.hdr_mode = 9;                                               //force set hdr_mode to zHDR */
		spin_unlock(&imgsensor_drv_lock);
		break;

	case SENSOR_FEATURE_GET_CROP_INFO:
		/* LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
		 *	(UINT32) *feature_data);
		 */

		wininfo =
	(struct SENSOR_WINSIZE_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

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

/* ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),
 * (UINT16)*(feature_data+2));
 */
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1));
/* ihdr_write_shutter((UINT16)*feature_data,(UINT16)*(feature_data+1)); */
		break;

	case SENSOR_FEATURE_GET_4CELL_DATA:/*get 4 cell data from eeprom*/
	{
		int type = (kal_uint16)(*feature_data);
		char *data = (char *)(uintptr_t)(*(feature_data+1));

		if (type == FOUR_CELL_CAL_TYPE_XTALK_CAL) {
			LOG_INF("Read Cross Talk Start");
			read_4cell_from_eeprom(data);
			LOG_INF("Read Cross Talk = %02x %02x %02x %02x %02x %02x\n",
				(UINT16)data[0], (UINT16)data[1],
				(UINT16)data[2], (UINT16)data[3],
				(UINT16)data[4], (UINT16)data[5]);
		}
		break;
	}


		/******************** PDAF START >>> *********/
		/*
		 * case SENSOR_FEATURE_GET_PDAF_INFO:
		 * LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
		 * (UINT16)*feature_data);
		 * PDAFinfo =
		 * (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
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
		 * LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n",
		 * (UINT16)*feature_data);
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
		 * LOG_INF(
		 * "SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
		 * (UINT16)*feature_data);
		 * //PDAF capacity enable or not
		 * switch (*feature_data) {
		 * case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		 * (MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
		 * break;
		 * case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		 * *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
		 * // video & capture use same setting
		 * break;
		 * case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		 * *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
		 * break;
		 * case MSDK_SCENARIO_ID_SLIM_VIDEO:
		 * //need to check
		 * *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
		 * break;
		 * case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		 * *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
		 * break;
		 * default:
		 * *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
		 * break;
		 * }
		 * break;
		 * case SENSOR_FEATURE_GET_PDAF_DATA: //get cal data from eeprom
		 * LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
		 * read_2T7_eeprom((kal_uint16 )(*feature_data),
		 * (char*)(uintptr_t)(*(feature_data+1)),
		 * (kal_uint32)(*(feature_data+2)));
		 * LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA success\n");
		 * break;
		 * case SENSOR_FEATURE_SET_PDAF:
		 * LOG_INF("PDAF mode :%d\n", *feature_data_16);
		 * imgsensor.pdaf_mode= *feature_data_16;
		 * break;
		 */
		/******************** PDAF END   <<< *********/
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
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = get_sensor_temperature();
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:
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
		break;

	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
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

UINT32 S5K3T1OFILM_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
