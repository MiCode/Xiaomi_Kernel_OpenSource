// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define PFX "ov20880mipiraw_Sensor"
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
#include <mt-plat/mtk_boot.h>
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "ov20880mipiraw_Sensor.h"

#undef CAPTURE_SIZE_5M


/****************************   Modify end    *********************************/




/* extern void kdSetI2CSpeed(u16 i2cSpeed); */
static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {

	/* record sensor id defined in Kd_imgsensor.h */
	.sensor_id = OV20880MIPI_SENSOR_ID,

	.checksum_value = 0xc2ded17b, /* checksum value for Camera Auto Test */

	.pre = {
		.pclk = 137142857,
		.linelength = 1524,
		.framelength = 1750,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		},

#ifdef CAPTURE_SIZE_5M
	.cap = {
		.pclk = 137142857,
		.linelength = 1524,
		.framelength = 1750,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,

		},
	.cap1 = {
		 .pclk = 137142857,
		 .linelength = 1524,
		 .framelength = 1750,
		 .startx = 0,
		 .starty = 0,
		 .grabwindow_width = 2592,
		 .grabwindow_height = 1944,
		 .mipi_data_lp2hs_settle_dc = 85,
		 .max_framerate = 300,
		 },
#else
	.cap = {
		.pclk = 160000000,
		.linelength = 1524,
		.framelength = 4200,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 5184,
		.grabwindow_height = 3888,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 150,
		},
	.cap1 = {
		 .pclk = 160000000,
		 .linelength = 2438,
		 .framelength = 4374,
		 .startx = 0,
		 .starty = 0,
		 .grabwindow_width = 5184,
		 .grabwindow_height = 3888,
		 .mipi_data_lp2hs_settle_dc = 85,
		 .max_framerate = 150,
		 },
#endif
	.normal_video = {
			 .pclk = 137142857,
			 .linelength = 1524,
			 .framelength = 3000,
			 .startx = 0,
			 .starty = 0,
			 .grabwindow_width = 2592,
			 .grabwindow_height = 1944,
			 .mipi_data_lp2hs_settle_dc = 85,
			 .max_framerate = 300,
			 },
	.hs_video = {
		     .pclk = 137142857,
		     .linelength = 1524,
		     .framelength = 1496,
		     .startx = 0,
		     .starty = 0,
		     .grabwindow_width = 2592,
		     .grabwindow_height = 1458,
		     .mipi_data_lp2hs_settle_dc = 85,
		     .max_framerate = 600,
		     },
	.slim_video = {
		       .pclk = 137142857,
		       .linelength = 1524,
		       .framelength = 3000,
		       .startx = 0,
		       .starty = 0,
		       .grabwindow_width = 2592,
		       .grabwindow_height = 1944,
		       .mipi_data_lp2hs_settle_dc = 85,
		       .max_framerate = 300,
		       },
	.margin = 8,		/* sensor framelength & shutter margin */
	.min_shutter = 4,	/* min shutter */

	 /* max framelength by sensor register's limitation */
	.max_frame_length = 0x7ff0,

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

	/* support sensor mode num ,don't support Slow motion */
	.sensor_mode_num = 5,

	.cap_delay_frame = 0,	/* enter capture delay frame num */
	.pre_delay_frame = 0,	/* enter preview delay frame num */
	.video_delay_frame = 0,	/* enter video delay frame num */
	.hs_video_delay_frame = 0, /* enter high speed video  delay frame num */
	.slim_video_delay_frame = 0,	/* enter slim video delay frame num */

	.isp_driving_current = ISP_DRIVING_8MA,	/* mclk driving current */

	/* sensor_interface_type */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,

	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,

	.mipi_settle_delay_mode = 1,
	/* 0,MIPI_SETTLEDELAY_AUTO;
	 * 1,MIPI_SETTLEDELAY_MANNUAL
	 */

	/* SENSOR_OUTPUT_FORMAT_RAW_B, sensor output first pixel color */
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,

	.mclk = 24,	/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_4_LANE,	/* mipi lane num */

	.i2c_addr_table = { /*0x42, */ 0x20, 0xff},
	/* record sensor support all write id addr,
	 * only supprt 4must end with 0xff
	 */
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	/* IMGSENSOR_MODE enum value,
	 * record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */

	.shutter = 0x4C00,	/* current shutter */
	.gain = 0x200,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */

	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.current_fps = 30,

	.autoflicker_en = KAL_FALSE,
	/* auto flicker enable:
	 * KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	 */

	.test_pattern = KAL_FALSE,
	/* test pattern mode or not.
	 * KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
	 */

	/* current scenario id */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,

	.ihdr_en = 0,	/* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x20,	/* record current sensor's i2c write id */
};


/* Sensor output window information*/
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{5184, 3888, 0, 0, 5184, 3888, 2592,
	 1944, 0, 0, 2592, 1944, 0, 0, 2592, 1944},	/* Preview */
#ifdef CAPTURE_SIZE_5M
	{5184, 3888, 0, 0, 5184, 3888, 2592,
	 1944, 0, 0, 2592, 1944, 0, 0, 2592, 1944},	/* capture */
#else
	{5184, 3888, 0, 0, 5184, 3888, 5184,
	 3888, 0, 0, 5184, 3888, 0, 0, 5184, 3888},	/* capture */
#endif
	{5184, 3888, 0, 0, 5184, 3888, 2592,
	 1944, 0, 0, 2592, 1944, 0, 0, 2592, 1944},	/* video */
	{5184, 3888, 0, 0, 5184, 3888, 2592,
	 1458, 0, 0, 2592, 1458, 0, 0, 2592, 1458},	/* hight speed video */
	{5184, 3888, 0, 0, 5184, 3888, 2592,
	 1944, 0, 0, 2592, 1944, 0, 0, 2592, 1944},	/* slim video */
};



static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pusendcmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	/* check */
	/* pr_debug("dummyline = %d, dummypixels = %d\n",
	 * imgsensor.dummy_line, imgsensor.dummy_pixel);
	 */

	write_cmos_sensor(0x380c, imgsensor.line_length >> 8);
	write_cmos_sensor(0x380d, imgsensor.line_length & 0xFF);
	write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
}


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;
	/* unsigned long flags; */

	pr_debug("framerate = %d, min framelength should enable? %d\n",
		framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);

	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length)
		? frame_length : imgsensor.min_frame_length;

	imgsensor.dummy_line =
		imgsensor.frame_length - imgsensor.min_frame_length;

	/* dummy_line = frame_length - imgsensor.min_frame_length; */
	/* if (dummy_line < 0) */
	/* imgsensor.dummy_line = 0; */
	/* else */
	/* imgsensor.dummy_line = dummy_line; */
	/* imgsensor.frame_length = frame_length + imgsensor.dummy_line; */

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

#if 0
static void write_shutter(kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;

	/* 0x3500, 0x3501, 0x3502 will increase VBLANK
	 * to get exposure larger than frame exposure
	 */

	/* AE doesn't update sensor gain at capture mode,
	 * thus extra exposure lines must be updated here.
	 */

	/* OV Recommend Solution */
	/* if shutter bigger than frame_length,
	 * should extend frame length first
	 */

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

	shutter = (shutter >> 1) << 1;
	imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk
			/ imgsensor.line_length * 10 / imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */

			imgsensor.frame_length =
			(imgsensor.frame_length >> 1) << 1;

			write_cmos_sensor(
				0x380e, (imgsensor.frame_length >> 8) & 0xFF);

			write_cmos_sensor(
				0x380f, imgsensor.frame_length & 0xFF);
		}
	} else {
		/* Extend frame length */
		imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;
		write_cmos_sensor(0x380e, (imgsensor.frame_length >> 8) & 0xFF);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	}

	/* Update Shutter */
	write_cmos_sensor(0x3500, (shutter >> 12) & 0x0F);
	write_cmos_sensor(0x3501, (shutter >> 4) & 0xFF);
	write_cmos_sensor(0x3502, (shutter << 4) & 0xF0);

	pr_debug("Exit! shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);

	/* pr_debug("frame_length = %d ", frame_length); */

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

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/* write_shutter(shutter); */
	/* 0x3500, 0x3501, 0x3502 will increase VBLANK
	 * to get exposure larger than frame exposure
	 */

	/* AE doesn't update sensor gain at capture mode,
	 * thus extra exposure lines must be updated here.
	 */

	/* OV Recommend Solution */
	/* if shutter bigger than frame_length,
	 * should extend frame length first
	 */
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter)
		? imgsensor_info.min_shutter : shutter;

	shutter =
	  (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
	  ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	shutter = (shutter >> 1) << 1;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk
			/ imgsensor.line_length * 10 / imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */

			write_cmos_sensor(
				0x380e, (imgsensor.frame_length >> 8) & 0xFF);

			write_cmos_sensor(
				0x380f, imgsensor.frame_length & 0xFF);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x380e, (imgsensor.frame_length >> 8) & 0xFF);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	}

	/* Update Shutter */
	write_cmos_sensor(0x3502, (shutter << 4) & 0xF0);
	write_cmos_sensor(0x3501, (shutter >> 4) & 0xFF);
	write_cmos_sensor(0x3500, (shutter >> 12) & 0x0F);

	pr_debug(
	    "Exit! shutter =%d, framelength =%d, for flicker realtime_fps=%d\n",
	    shutter, imgsensor.frame_length, realtime_fps);

}


static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 iReg = 0x0000;

	/* platform 1xgain = 64, sensor driver 1*gain = 0x80 */
	iReg = gain * 128 / BASEGAIN;

	if (iReg < 0x80)	/* sensor 1xGain */
		iReg = 0X80;

	if (iReg > 0x7c0)	/* sensor 15.5xGain */
		iReg = 0X7C0;

	return iReg;
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

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	pr_debug("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor(0x3508, (reg_gain >> 8));
	write_cmos_sensor(0x3509, (reg_gain & 0xFF));
	write_cmos_sensor(0x350c, (reg_gain >> 8));
	write_cmos_sensor(0x350d, (reg_gain & 0xFF));
	return gain;
}				/*      set_gain  */

static void ihdr_write_shutter_gain(
	kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	pr_debug("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
}


#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
	pr_debug("image_mirror = %d\n", image_mirror);

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
	/*No Need to implement this function */
}				/*      night_mode      */

static void sensor_init(void)
{
	pr_debug("E\n");
	/* OV20880 4c setting */
	write_cmos_sensor(0x0103, 0x01);	/* SW Reset, need delay */
	mdelay(10);
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x0300, 0x04);
	write_cmos_sensor(0x0301, 0xa1);
	write_cmos_sensor(0x0302, 0x14);
	write_cmos_sensor(0x0303, 0x11);
	write_cmos_sensor(0x0304, 0xc8);
	write_cmos_sensor(0x0314, 0x00);
	write_cmos_sensor(0x0316, 0x28);
	write_cmos_sensor(0x0317, 0x02);
	write_cmos_sensor(0x0318, 0x05);
	write_cmos_sensor(0x031d, 0x01);
	write_cmos_sensor(0x031e, 0x09);
	write_cmos_sensor(0x0320, 0x1f);
	write_cmos_sensor(0x0322, 0x29);
	write_cmos_sensor(0x3005, 0x00);
	write_cmos_sensor(0x300d, 0x11);
	write_cmos_sensor(0x3010, 0x01);
	write_cmos_sensor(0x3012, 0x41);
	write_cmos_sensor(0x3016, 0xf0);
	write_cmos_sensor(0x301e, 0x98);
	write_cmos_sensor(0x3031, 0x88);
	write_cmos_sensor(0x3032, 0x06);
	write_cmos_sensor(0x3400, 0x00);
	write_cmos_sensor(0x3408, 0x03);
	write_cmos_sensor(0x3409, 0x08);
	write_cmos_sensor(0x340c, 0x04);
	write_cmos_sensor(0x340d, 0x3c);
	write_cmos_sensor(0x3500, 0x00);
	write_cmos_sensor(0x3501, 0xBB);
	write_cmos_sensor(0x3502, 0x00);
	write_cmos_sensor(0x3503, 0x08);
	write_cmos_sensor(0x3505, 0x8c);
	write_cmos_sensor(0x3507, 0x00);
	write_cmos_sensor(0x3508, 0x01);
	write_cmos_sensor(0x3509, 0x80);
	write_cmos_sensor(0x350c, 0x01);
	write_cmos_sensor(0x350d, 0x80);
	write_cmos_sensor(0x3510, 0x00);
	write_cmos_sensor(0x3511, 0x10);
	write_cmos_sensor(0x3512, 0x00);
	write_cmos_sensor(0x3602, 0x86);
	write_cmos_sensor(0x3608, 0x2e);
	write_cmos_sensor(0x3609, 0x20);
	write_cmos_sensor(0x360b, 0x40);
	write_cmos_sensor(0x3619, 0x03);
	write_cmos_sensor(0x361a, 0x00);
	write_cmos_sensor(0x361b, 0x01);
	write_cmos_sensor(0x361c, 0xc7);
	write_cmos_sensor(0x3626, 0x03);
	write_cmos_sensor(0x3627, 0x88);
	write_cmos_sensor(0x3632, 0x00);
	write_cmos_sensor(0x3658, 0x00);
	write_cmos_sensor(0x3659, 0x00);
	write_cmos_sensor(0x365a, 0x00);
	write_cmos_sensor(0x365b, 0x00);
	write_cmos_sensor(0x365c, 0x00);
	write_cmos_sensor(0x3660, 0x40);
	write_cmos_sensor(0x3661, 0x0c);
	write_cmos_sensor(0x3663, 0x40);
	write_cmos_sensor(0x3664, 0x03);
	write_cmos_sensor(0x3668, 0xf0);
	write_cmos_sensor(0x3669, 0x0e);
	write_cmos_sensor(0x366a, 0x10);
	write_cmos_sensor(0x366b, 0x42);
	write_cmos_sensor(0x366c, 0x00);
	write_cmos_sensor(0x3674, 0x07);
	write_cmos_sensor(0x3675, 0x00);
	write_cmos_sensor(0x3700, 0x13);
	write_cmos_sensor(0x3701, 0x0e);
	write_cmos_sensor(0x3702, 0x12);
	write_cmos_sensor(0x3703, 0x30);
	write_cmos_sensor(0x3706, 0x28);
	write_cmos_sensor(0x3709, 0x3b);
	write_cmos_sensor(0x3714, 0x63);
	write_cmos_sensor(0x371a, 0x1d);
	write_cmos_sensor(0x3726, 0x00);
	write_cmos_sensor(0x3727, 0x03);
	write_cmos_sensor(0x372d, 0x26);
	write_cmos_sensor(0x373b, 0x03);
	write_cmos_sensor(0x373d, 0x05);
	write_cmos_sensor(0x374f, 0x0d);
	write_cmos_sensor(0x3754, 0x88);
	write_cmos_sensor(0x375a, 0x08);
	write_cmos_sensor(0x3764, 0x12);
	write_cmos_sensor(0x3765, 0x0b);
	write_cmos_sensor(0x3767, 0x0c);
	write_cmos_sensor(0x3768, 0x18);
	write_cmos_sensor(0x3769, 0x08);
	write_cmos_sensor(0x376a, 0x0c);
	write_cmos_sensor(0x37a2, 0x04);
	write_cmos_sensor(0x37b1, 0x40);
	write_cmos_sensor(0x37c8, 0x03);
	write_cmos_sensor(0x37c9, 0x02);
	write_cmos_sensor(0x37d9, 0x00);
	write_cmos_sensor(0x37f4, 0x80);
	write_cmos_sensor(0x37f9, 0x00);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x02);
	write_cmos_sensor(0x3804, 0x14);
	write_cmos_sensor(0x3805, 0x5f);
	write_cmos_sensor(0x3806, 0x0f);
	write_cmos_sensor(0x3807, 0x4d);
	write_cmos_sensor(0x3808, 0x0a);
	write_cmos_sensor(0x3809, 0x20);
	write_cmos_sensor(0x380a, 0x07);
	write_cmos_sensor(0x380b, 0x98);
	write_cmos_sensor(0x380c, 0x0b);
	write_cmos_sensor(0x380d, 0xe8);
	write_cmos_sensor(0x380e, 0x06);
	write_cmos_sensor(0x380f, 0xd6);
	write_cmos_sensor(0x3810, 0x00);
	write_cmos_sensor(0x3811, 0x07);
	write_cmos_sensor(0x3812, 0x00);
	write_cmos_sensor(0x3813, 0x07);
	write_cmos_sensor(0x3814, 0x11);
	write_cmos_sensor(0x3815, 0x44);
	write_cmos_sensor(0x3820, 0x01);
	write_cmos_sensor(0x3821, 0x00);
	write_cmos_sensor(0x382f, 0x84);
	write_cmos_sensor(0x3830, 0x00);
	write_cmos_sensor(0x3831, 0x00);
	write_cmos_sensor(0x3832, 0x00);
	write_cmos_sensor(0x3833, 0x01);
	write_cmos_sensor(0x3834, 0xf1);
	write_cmos_sensor(0x3836, 0x28);
	write_cmos_sensor(0x3841, 0x20);
	write_cmos_sensor(0x3891, 0x0f);
	write_cmos_sensor(0x38a0, 0x04);
	write_cmos_sensor(0x38a1, 0x00);
	write_cmos_sensor(0x38a2, 0x04);
	write_cmos_sensor(0x38a3, 0x04);
	write_cmos_sensor(0x38b0, 0x02);
	write_cmos_sensor(0x38b1, 0x02);
	write_cmos_sensor(0x3900, 0x00);
	write_cmos_sensor(0x3b8e, 0x01);
	write_cmos_sensor(0x3d8c, 0x79);
	write_cmos_sensor(0x3d8d, 0xc0);
	write_cmos_sensor(0x3f00, 0xca);
	write_cmos_sensor(0x3f05, 0x38);
	write_cmos_sensor(0x4008, 0x00);
	write_cmos_sensor(0x4009, 0x02);
	write_cmos_sensor(0x400e, 0x00);
	write_cmos_sensor(0x4010, 0x29);
	write_cmos_sensor(0x4011, 0x01);
	write_cmos_sensor(0x4012, 0x0d);
	write_cmos_sensor(0x4013, 0x16);
	write_cmos_sensor(0x4014, 0x0a);
	write_cmos_sensor(0x4015, 0x02);
	write_cmos_sensor(0x4016, 0x15);
	write_cmos_sensor(0x4017, 0x00);
	write_cmos_sensor(0x4018, 0x09);
	write_cmos_sensor(0x4019, 0x00);
	write_cmos_sensor(0x401a, 0x40);
	write_cmos_sensor(0x4020, 0x04);
	write_cmos_sensor(0x4021, 0x00);
	write_cmos_sensor(0x4022, 0x04);
	write_cmos_sensor(0x4023, 0x00);
	write_cmos_sensor(0x4024, 0x04);
	write_cmos_sensor(0x4025, 0x00);
	write_cmos_sensor(0x4026, 0x04);
	write_cmos_sensor(0x4027, 0x00);
	write_cmos_sensor(0x4056, 0x05);
	write_cmos_sensor(0x4500, 0x20);
	write_cmos_sensor(0x4501, 0x04);
	write_cmos_sensor(0x4502, 0x80);
	write_cmos_sensor(0x4503, 0x20);
	write_cmos_sensor(0x450c, 0x05);
	write_cmos_sensor(0x450e, 0x16);
	write_cmos_sensor(0x450f, 0x88);
	write_cmos_sensor(0x4540, 0x86);
	write_cmos_sensor(0x4541, 0x07);
	write_cmos_sensor(0x4542, 0x04);
	write_cmos_sensor(0x4543, 0x05);
	write_cmos_sensor(0x4544, 0x06);
	write_cmos_sensor(0x4545, 0x07);
	write_cmos_sensor(0x4546, 0x04);
	write_cmos_sensor(0x4547, 0x05);
	write_cmos_sensor(0x4548, 0x02);
	write_cmos_sensor(0x4549, 0x03);
	write_cmos_sensor(0x454a, 0x00);
	write_cmos_sensor(0x454b, 0x01);
	write_cmos_sensor(0x454c, 0x02);
	write_cmos_sensor(0x454d, 0x03);
	write_cmos_sensor(0x454e, 0x00);
	write_cmos_sensor(0x454f, 0x01);
	write_cmos_sensor(0x4550, 0x06);
	write_cmos_sensor(0x4551, 0x07);
	write_cmos_sensor(0x4552, 0x04);
	write_cmos_sensor(0x4553, 0x05);
	write_cmos_sensor(0x4554, 0x06);
	write_cmos_sensor(0x4555, 0x07);
	write_cmos_sensor(0x4556, 0x04);
	write_cmos_sensor(0x4557, 0x05);
	write_cmos_sensor(0x4558, 0x02);
	write_cmos_sensor(0x4559, 0x03);
	write_cmos_sensor(0x455a, 0x00);
	write_cmos_sensor(0x455b, 0x01);
	write_cmos_sensor(0x455c, 0x02);
	write_cmos_sensor(0x455d, 0x03);
	write_cmos_sensor(0x455e, 0x00);
	write_cmos_sensor(0x455f, 0x01);
	write_cmos_sensor(0x4604, 0x08);
	write_cmos_sensor(0x4640, 0x01);
	write_cmos_sensor(0x4641, 0x04);
	write_cmos_sensor(0x4642, 0x02);
	write_cmos_sensor(0x4644, 0x08);
	write_cmos_sensor(0x4645, 0x03);
	write_cmos_sensor(0x4809, 0x2b);
	write_cmos_sensor(0x480e, 0x02);
	write_cmos_sensor(0x4815, 0x40);
	write_cmos_sensor(0x4813, 0x90);
	write_cmos_sensor(0x4817, 0x04);
	write_cmos_sensor(0x481b, 0x3c);
	write_cmos_sensor(0x481f, 0x30);
	write_cmos_sensor(0x4837, 0x14);
	write_cmos_sensor(0x4847, 0x01);
	write_cmos_sensor(0x484b, 0x01);
	write_cmos_sensor(0x4850, 0x7c);
	write_cmos_sensor(0x4852, 0x03);
	write_cmos_sensor(0x4853, 0x12);
	write_cmos_sensor(0x4856, 0x58);
	write_cmos_sensor(0x4a01, 0x00);
	write_cmos_sensor(0x4a02, 0x00);
	write_cmos_sensor(0x4a03, 0x00);
	write_cmos_sensor(0x4a0a, 0x00);
	write_cmos_sensor(0x4a0b, 0x00);
	write_cmos_sensor(0x4a0c, 0x00);
	write_cmos_sensor(0x4a0d, 0x00);
	write_cmos_sensor(0x4a0e, 0x00);
	write_cmos_sensor(0x4a0f, 0x00);
	write_cmos_sensor(0x4a14, 0x00);
	write_cmos_sensor(0x4a16, 0x00);
	write_cmos_sensor(0x4a18, 0x00);
	write_cmos_sensor(0x4a19, 0x00);
	write_cmos_sensor(0x4a1a, 0x00);
	write_cmos_sensor(0x4a1c, 0x00);
	write_cmos_sensor(0x4a1e, 0x00);
	write_cmos_sensor(0x4a20, 0x00);
	write_cmos_sensor(0x4a22, 0x00);
	write_cmos_sensor(0x4a24, 0x00);
	write_cmos_sensor(0x4a25, 0x00);
	write_cmos_sensor(0x4a26, 0x00);
	write_cmos_sensor(0x4a27, 0x00);
	write_cmos_sensor(0x4a28, 0x00);
	write_cmos_sensor(0x4a29, 0x00);
	write_cmos_sensor(0x4a2a, 0x00);
	write_cmos_sensor(0x4a2b, 0x00);
	write_cmos_sensor(0x4a2c, 0x00);
	write_cmos_sensor(0x4c01, 0x00);
	write_cmos_sensor(0x4c02, 0x00);
	write_cmos_sensor(0x4c03, 0x00);
	write_cmos_sensor(0x4c0a, 0x00);
	write_cmos_sensor(0x4c0b, 0x00);
	write_cmos_sensor(0x4c0c, 0x00);
	write_cmos_sensor(0x4c0d, 0x00);
	write_cmos_sensor(0x4c0e, 0x00);
	write_cmos_sensor(0x4c0f, 0x00);
	write_cmos_sensor(0x5000, 0x81);
	write_cmos_sensor(0x5001, 0x43);
	write_cmos_sensor(0x5002, 0x09);
	write_cmos_sensor(0x5006, 0x40);
	write_cmos_sensor(0x500f, 0x10);
	write_cmos_sensor(0x5011, 0x10);
	write_cmos_sensor(0x5013, 0x10);
	write_cmos_sensor(0x5015, 0x10);
	write_cmos_sensor(0x5020, 0x00);
	write_cmos_sensor(0x5021, 0x88);
	write_cmos_sensor(0x5022, 0x00);
	write_cmos_sensor(0x5023, 0x8a);
	write_cmos_sensor(0x5024, 0x00);
	write_cmos_sensor(0x5025, 0x03);
	write_cmos_sensor(0x5026, 0x00);
	write_cmos_sensor(0x5027, 0x05);
	write_cmos_sensor(0x5080, 0x04);
	write_cmos_sensor(0x5081, 0x10);
	write_cmos_sensor(0x5084, 0x0a);
	write_cmos_sensor(0x5085, 0x30);
	write_cmos_sensor(0x5086, 0x07);
	write_cmos_sensor(0x5087, 0xa6);
	write_cmos_sensor(0x5146, 0x04);
	write_cmos_sensor(0x5180, 0x06);
	write_cmos_sensor(0x5181, 0x30);
	write_cmos_sensor(0x5184, 0x61);
	write_cmos_sensor(0x5185, 0x1c);
	write_cmos_sensor(0x518c, 0x01);
	write_cmos_sensor(0x518d, 0x01);
	write_cmos_sensor(0x518e, 0x01);
	write_cmos_sensor(0x518f, 0x01);
	write_cmos_sensor(0x5190, 0x00);
	write_cmos_sensor(0x5191, 0x00);
	write_cmos_sensor(0x5192, 0x0a);
	write_cmos_sensor(0x5193, 0x30);
	write_cmos_sensor(0x5194, 0x00);
	write_cmos_sensor(0x5195, 0x01);
	write_cmos_sensor(0x5200, 0xbf);
	write_cmos_sensor(0x5201, 0xf3);
	write_cmos_sensor(0x5202, 0x09);
	write_cmos_sensor(0x5203, 0x1b);
	write_cmos_sensor(0x5204, 0xe0);
	write_cmos_sensor(0x5205, 0x10);
	write_cmos_sensor(0x5206, 0x3f);
	write_cmos_sensor(0x5207, 0x3c);
	write_cmos_sensor(0x5208, 0x24);
	write_cmos_sensor(0x5209, 0x0f);
	write_cmos_sensor(0x520a, 0x43);
	write_cmos_sensor(0x520b, 0x3b);
	write_cmos_sensor(0x520c, 0x33);
	write_cmos_sensor(0x520d, 0x33);
	write_cmos_sensor(0x520e, 0x63);
	write_cmos_sensor(0x5210, 0x06);
	write_cmos_sensor(0x5211, 0x03);
	write_cmos_sensor(0x5212, 0x08);
	write_cmos_sensor(0x5213, 0x08);
	write_cmos_sensor(0x5217, 0x04);
	write_cmos_sensor(0x5218, 0x02);
	write_cmos_sensor(0x5219, 0x01);
	write_cmos_sensor(0x521a, 0x04);
	write_cmos_sensor(0x521b, 0x02);
	write_cmos_sensor(0x521c, 0x01);
	write_cmos_sensor(0x5297, 0x04);
	write_cmos_sensor(0x5298, 0x02);
	write_cmos_sensor(0x5299, 0x01);
	write_cmos_sensor(0x529a, 0x04);
	write_cmos_sensor(0x529b, 0x02);
	write_cmos_sensor(0x529c, 0x01);
	write_cmos_sensor(0x534a, 0x07);
	write_cmos_sensor(0x534f, 0x10);
	write_cmos_sensor(0x5404, 0x00);
	write_cmos_sensor(0x5405, 0x00);
	write_cmos_sensor(0x5406, 0x05);
	write_cmos_sensor(0x5407, 0x7e);
	write_cmos_sensor(0x5408, 0x07);
	write_cmos_sensor(0x5409, 0x50);
	write_cmos_sensor(0x5410, 0x03);
	write_cmos_sensor(0x5411, 0x40);
	write_cmos_sensor(0x5413, 0x00);
	write_cmos_sensor(0x5800, 0x01);
	write_cmos_sensor(0x5801, 0x00);
	write_cmos_sensor(0x5820, 0x18);
	write_cmos_sensor(0x5821, 0x08);
	write_cmos_sensor(0x5822, 0x08);
	write_cmos_sensor(0x5823, 0x18);
	write_cmos_sensor(0x5824, 0x18);
	write_cmos_sensor(0x5825, 0x08);
	write_cmos_sensor(0x5826, 0x08);
	write_cmos_sensor(0x5827, 0x18);
	write_cmos_sensor(0x582c, 0x08);
	write_cmos_sensor(0x582d, 0x18);
	write_cmos_sensor(0x582e, 0x00);
	write_cmos_sensor(0x582f, 0x00);
	write_cmos_sensor(0x5830, 0x08);
	write_cmos_sensor(0x5831, 0x18);
	write_cmos_sensor(0x5836, 0x08);
	write_cmos_sensor(0x5837, 0x18);
	write_cmos_sensor(0x5838, 0x00);
	write_cmos_sensor(0x5839, 0x00);
	write_cmos_sensor(0x583a, 0x08);
	write_cmos_sensor(0x583b, 0x18);
	write_cmos_sensor(0x583c, 0x55);
	write_cmos_sensor(0x583e, 0x03);
	write_cmos_sensor(0x58a1, 0x04);
	write_cmos_sensor(0x58a2, 0x00);
	write_cmos_sensor(0x58a3, 0x00);
	write_cmos_sensor(0x58a4, 0x02);
	write_cmos_sensor(0x58a5, 0x00);
	write_cmos_sensor(0x58a6, 0x02);
	write_cmos_sensor(0x58a7, 0x00);
	write_cmos_sensor(0x58a8, 0x00);
	write_cmos_sensor(0x58a9, 0x00);
	write_cmos_sensor(0x58aa, 0x00);
	write_cmos_sensor(0x58ab, 0x00);
	write_cmos_sensor(0x58ac, 0x14);
	write_cmos_sensor(0x58ad, 0x60);
	write_cmos_sensor(0x58ae, 0x0f);
	write_cmos_sensor(0x58af, 0x50);
	write_cmos_sensor(0x58c0, 0x00);
	write_cmos_sensor(0x58c1, 0x00);
	write_cmos_sensor(0x58c3, 0x00);
	write_cmos_sensor(0x58c4, 0x02);
	write_cmos_sensor(0x58c5, 0x80);
	write_cmos_sensor(0x58c6, 0x01);
	write_cmos_sensor(0x58c7, 0xe0);
	write_cmos_sensor(0x58cb, 0x00);
	write_cmos_sensor(0x58cd, 0x00);
	write_cmos_sensor(0x58cf, 0x00);
	write_cmos_sensor(0x58d1, 0x00);
	write_cmos_sensor(0x58d2, 0x00);
	write_cmos_sensor(0x58d3, 0x00);
	write_cmos_sensor(0x58d4, 0x00);
	write_cmos_sensor(0x58d5, 0x00);
	write_cmos_sensor(0x58d7, 0x00);
	write_cmos_sensor(0x58d9, 0x00);
	write_cmos_sensor(0x58db, 0x00);
	write_cmos_sensor(0x5900, 0x00);
	write_cmos_sensor(0x5901, 0x00);
	write_cmos_sensor(0x5902, 0x00);
	write_cmos_sensor(0x5903, 0x00);
	write_cmos_sensor(0x5904, 0x00);
	write_cmos_sensor(0x5905, 0x00);
	write_cmos_sensor(0x5906, 0x00);
	write_cmos_sensor(0x5907, 0x00);
	write_cmos_sensor(0x5908, 0x00);
	write_cmos_sensor(0x5909, 0x00);
	write_cmos_sensor(0x590a, 0x00);
	write_cmos_sensor(0x590b, 0x00);
	write_cmos_sensor(0x590c, 0x00);
	write_cmos_sensor(0x590d, 0x00);
	write_cmos_sensor(0x590e, 0x00);
	write_cmos_sensor(0x590f, 0x00);
	write_cmos_sensor(0x5910, 0x00);
	write_cmos_sensor(0x5911, 0x00);
	write_cmos_sensor(0x5912, 0x00);
	write_cmos_sensor(0x5913, 0x00);
	write_cmos_sensor(0x5914, 0x00);
	write_cmos_sensor(0x5915, 0x00);
	write_cmos_sensor(0x5916, 0x00);
	write_cmos_sensor(0x5917, 0x00);
	write_cmos_sensor(0x5918, 0x00);
	write_cmos_sensor(0x5919, 0x00);
	write_cmos_sensor(0x591a, 0x00);
	write_cmos_sensor(0x591b, 0x00);
	write_cmos_sensor(0x591c, 0x00);
	write_cmos_sensor(0x591d, 0x00);
	write_cmos_sensor(0x591e, 0x00);
	write_cmos_sensor(0x591f, 0x00);
	write_cmos_sensor(0x5920, 0x00);
	write_cmos_sensor(0x5921, 0x00);
	write_cmos_sensor(0x5922, 0x00);
	write_cmos_sensor(0x5923, 0x00);
	write_cmos_sensor(0x5924, 0x00);
	write_cmos_sensor(0x5925, 0x00);
	write_cmos_sensor(0x5926, 0x00);
	write_cmos_sensor(0x5927, 0x00);
	write_cmos_sensor(0x5928, 0x00);
	write_cmos_sensor(0x5929, 0x00);
	write_cmos_sensor(0x592a, 0x00);
	write_cmos_sensor(0x592b, 0x00);
	write_cmos_sensor(0x592c, 0x00);
	write_cmos_sensor(0x592d, 0x00);
	write_cmos_sensor(0x592e, 0x00);
	write_cmos_sensor(0x592f, 0x00);
	write_cmos_sensor(0x5930, 0x00);
	write_cmos_sensor(0x5931, 0x00);
	write_cmos_sensor(0x5932, 0x00);
	write_cmos_sensor(0x5933, 0x00);
	write_cmos_sensor(0x5934, 0x00);
	write_cmos_sensor(0x5935, 0x00);
	write_cmos_sensor(0x5936, 0x00);
	write_cmos_sensor(0x5937, 0x00);
	write_cmos_sensor(0x5938, 0x00);
	write_cmos_sensor(0x5939, 0x00);
	write_cmos_sensor(0x593a, 0x00);
	write_cmos_sensor(0x593b, 0x00);
	write_cmos_sensor(0x593c, 0x00);
	write_cmos_sensor(0x593d, 0x00);
	write_cmos_sensor(0x593e, 0x00);
	write_cmos_sensor(0x593f, 0x00);
	write_cmos_sensor(0x5940, 0x00);
	write_cmos_sensor(0x5941, 0x00);
	write_cmos_sensor(0x5942, 0x00);
	write_cmos_sensor(0x5943, 0x00);
	write_cmos_sensor(0x5944, 0x00);
	write_cmos_sensor(0x5945, 0x00);
	write_cmos_sensor(0x5946, 0x00);
	write_cmos_sensor(0x5947, 0x00);
	write_cmos_sensor(0x5948, 0x00);
	write_cmos_sensor(0x5949, 0x00);
	write_cmos_sensor(0x594a, 0x00);
	write_cmos_sensor(0x594b, 0x00);
	write_cmos_sensor(0x594c, 0x00);
	write_cmos_sensor(0x594d, 0x00);
	write_cmos_sensor(0x594e, 0x00);
	write_cmos_sensor(0x594f, 0x00);
	write_cmos_sensor(0x5950, 0x00);
	write_cmos_sensor(0x5951, 0x00);
	write_cmos_sensor(0x5952, 0x00);
	write_cmos_sensor(0x5953, 0x00);
	write_cmos_sensor(0x5954, 0x00);
	write_cmos_sensor(0x5955, 0x00);
	write_cmos_sensor(0x5956, 0x00);
	write_cmos_sensor(0x5957, 0x00);
	write_cmos_sensor(0x5958, 0x00);
	write_cmos_sensor(0x5959, 0x00);
	write_cmos_sensor(0x595a, 0x00);
	write_cmos_sensor(0x595b, 0x00);
	write_cmos_sensor(0x595c, 0x00);
	write_cmos_sensor(0x595d, 0x00);
	write_cmos_sensor(0x595e, 0x00);
	write_cmos_sensor(0x595f, 0x00);
	write_cmos_sensor(0x5960, 0x00);
	write_cmos_sensor(0x5961, 0x00);
	write_cmos_sensor(0x5962, 0x00);
	write_cmos_sensor(0x5963, 0x00);
	write_cmos_sensor(0x5964, 0x00);
	write_cmos_sensor(0x5965, 0x00);
	write_cmos_sensor(0x5966, 0x00);
	write_cmos_sensor(0x5967, 0x00);
	write_cmos_sensor(0x5968, 0x00);
	write_cmos_sensor(0x5969, 0x00);
	write_cmos_sensor(0x596a, 0x00);
	write_cmos_sensor(0x596b, 0x00);
	write_cmos_sensor(0x596c, 0x00);
	write_cmos_sensor(0x596d, 0x00);
	write_cmos_sensor(0x596e, 0x00);
	write_cmos_sensor(0x596f, 0x00);
	write_cmos_sensor(0x5970, 0x00);
	write_cmos_sensor(0x5971, 0x00);
	write_cmos_sensor(0x5972, 0x00);
	write_cmos_sensor(0x5973, 0x00);
	write_cmos_sensor(0x5974, 0x00);
	write_cmos_sensor(0x5975, 0x00);
	write_cmos_sensor(0x5976, 0x00);
	write_cmos_sensor(0x5977, 0x00);
	write_cmos_sensor(0x5978, 0x00);
	write_cmos_sensor(0x5979, 0x00);
	write_cmos_sensor(0x597a, 0x00);
	write_cmos_sensor(0x597b, 0x00);
	write_cmos_sensor(0x597c, 0x00);
	write_cmos_sensor(0x597d, 0x00);
	write_cmos_sensor(0x597e, 0x00);
	write_cmos_sensor(0x597f, 0x00);
	write_cmos_sensor(0x5980, 0x00);
	write_cmos_sensor(0x5981, 0x00);
	write_cmos_sensor(0x5982, 0x00);
	write_cmos_sensor(0x5983, 0x00);
	write_cmos_sensor(0x5984, 0x00);
	write_cmos_sensor(0x5985, 0x00);
	write_cmos_sensor(0x5986, 0x00);
	write_cmos_sensor(0x5987, 0x00);
	write_cmos_sensor(0x5988, 0x00);
	write_cmos_sensor(0x5989, 0x00);
	write_cmos_sensor(0x598a, 0x00);
	write_cmos_sensor(0x598b, 0x00);
	write_cmos_sensor(0x598c, 0x00);
	write_cmos_sensor(0x598d, 0x00);
	write_cmos_sensor(0x598e, 0x00);
	write_cmos_sensor(0x598f, 0x00);
	write_cmos_sensor(0x5990, 0x00);
	write_cmos_sensor(0x5991, 0x00);
	write_cmos_sensor(0x5992, 0x00);
	write_cmos_sensor(0x5993, 0x00);
	write_cmos_sensor(0x5994, 0x00);
	write_cmos_sensor(0x5995, 0x00);
	write_cmos_sensor(0x5996, 0x00);
	write_cmos_sensor(0x5997, 0x00);
	write_cmos_sensor(0x5998, 0x00);
	write_cmos_sensor(0x5999, 0x00);
	write_cmos_sensor(0x599a, 0x00);
	write_cmos_sensor(0x599b, 0x00);
	write_cmos_sensor(0x599c, 0x00);
	write_cmos_sensor(0x599d, 0x00);
	write_cmos_sensor(0x599e, 0x00);
	write_cmos_sensor(0x599f, 0x00);
	write_cmos_sensor(0x59a0, 0x00);
	write_cmos_sensor(0x59a1, 0x00);
	write_cmos_sensor(0x59a2, 0x00);
	write_cmos_sensor(0x59a3, 0x00);
	write_cmos_sensor(0x59a4, 0x00);
	write_cmos_sensor(0x59a5, 0x00);
	write_cmos_sensor(0x59a6, 0x00);
	write_cmos_sensor(0x59a7, 0x00);
	write_cmos_sensor(0x59a8, 0x00);
	write_cmos_sensor(0x59a9, 0x00);
	write_cmos_sensor(0x59aa, 0x00);
	write_cmos_sensor(0x59ab, 0x00);
	write_cmos_sensor(0x59ac, 0x00);
	write_cmos_sensor(0x59ad, 0x00);
	write_cmos_sensor(0x59ae, 0x00);
	write_cmos_sensor(0x59af, 0x00);
	write_cmos_sensor(0x59b0, 0x00);
	write_cmos_sensor(0x59b1, 0x00);
	write_cmos_sensor(0x59b2, 0x00);
	write_cmos_sensor(0x59b3, 0x00);
	write_cmos_sensor(0x59b4, 0x00);
	write_cmos_sensor(0x59b5, 0x00);
	write_cmos_sensor(0x59bc, 0x06);
	write_cmos_sensor(0x59bd, 0x40);
	write_cmos_sensor(0x59be, 0x04);
	write_cmos_sensor(0x59bf, 0xb0);
	write_cmos_sensor(0x59c5, 0x88);
	write_cmos_sensor(0x59c6, 0x00);
	write_cmos_sensor(0x59c7, 0x8a);
	write_cmos_sensor(0x59c9, 0x03);
	write_cmos_sensor(0x59ca, 0x00);
	write_cmos_sensor(0x59cb, 0x05);
	write_cmos_sensor(0x59d4, 0x00);
	write_cmos_sensor(0x59d6, 0x00);
	write_cmos_sensor(0x59dc, 0x00);
	write_cmos_sensor(0x59de, 0x00);
	write_cmos_sensor(0x59e5, 0x00);
	write_cmos_sensor(0x59e7, 0x00);
	write_cmos_sensor(0x59ec, 0x00);
	write_cmos_sensor(0x59ed, 0x00);
	write_cmos_sensor(0x59ee, 0x00);
	write_cmos_sensor(0x59ef, 0x00);
	write_cmos_sensor(0x59f4, 0x00);
	write_cmos_sensor(0x59f6, 0x00);
	write_cmos_sensor(0x59fc, 0x00);
	write_cmos_sensor(0x59fd, 0x00);
	write_cmos_sensor(0x59fe, 0x00);
	write_cmos_sensor(0x59ff, 0x00);
	write_cmos_sensor(0x5a00, 0x00);
	write_cmos_sensor(0x5a01, 0x00);
	write_cmos_sensor(0x5a02, 0x00);
	write_cmos_sensor(0x5a03, 0x00);
	write_cmos_sensor(0x5a04, 0x00);
	write_cmos_sensor(0x5a05, 0x00);
	write_cmos_sensor(0x5a06, 0x00);
	write_cmos_sensor(0x5a07, 0x00);
	write_cmos_sensor(0x5a08, 0x00);
	write_cmos_sensor(0x5a09, 0x00);
	write_cmos_sensor(0x5a0a, 0x00);
	write_cmos_sensor(0x5a0b, 0x00);
	write_cmos_sensor(0x5a0c, 0x00);
	write_cmos_sensor(0x5a0d, 0x00);
	write_cmos_sensor(0x5a0e, 0x00);
	write_cmos_sensor(0x5a0f, 0x00);
	write_cmos_sensor(0x5a10, 0x00);
	write_cmos_sensor(0x5a11, 0x00);
	write_cmos_sensor(0x5a12, 0x00);
	write_cmos_sensor(0x5a13, 0x00);
	write_cmos_sensor(0x5a14, 0x00);
	write_cmos_sensor(0x5a15, 0x00);
	write_cmos_sensor(0x5a16, 0x00);
	write_cmos_sensor(0x5a17, 0x00);
	write_cmos_sensor(0x5a18, 0x00);
	write_cmos_sensor(0x5a19, 0x00);
	write_cmos_sensor(0x5a1a, 0x00);
	write_cmos_sensor(0x5a1b, 0x00);
	write_cmos_sensor(0x5a1c, 0x00);
	write_cmos_sensor(0x5a1d, 0x00);
	write_cmos_sensor(0x5a1e, 0x00);
	write_cmos_sensor(0x5a1f, 0x00);
	write_cmos_sensor(0x5a20, 0x00);
	write_cmos_sensor(0x5a21, 0x00);
	write_cmos_sensor(0x5a22, 0x00);
	write_cmos_sensor(0x5a23, 0x00);
	write_cmos_sensor(0x5a24, 0x00);
	write_cmos_sensor(0x5a25, 0x00);
	write_cmos_sensor(0x5a26, 0x00);
	write_cmos_sensor(0x5a27, 0x00);
	write_cmos_sensor(0x5a28, 0x00);
	write_cmos_sensor(0x5a29, 0x00);
	write_cmos_sensor(0x5a2a, 0x00);
	write_cmos_sensor(0x5a2b, 0x00);
	write_cmos_sensor(0x5a2c, 0x00);
	write_cmos_sensor(0x5a2d, 0x00);
	write_cmos_sensor(0x5a2e, 0x00);
	write_cmos_sensor(0x5a2f, 0x00);
	write_cmos_sensor(0x5a30, 0x00);
	write_cmos_sensor(0x5a31, 0x00);
	write_cmos_sensor(0x5a32, 0x00);
	write_cmos_sensor(0x5a33, 0x00);
	write_cmos_sensor(0x5a34, 0x00);
	write_cmos_sensor(0x5a35, 0x00);
	write_cmos_sensor(0x5a36, 0x00);
	write_cmos_sensor(0x5a37, 0x00);
	write_cmos_sensor(0x5a38, 0x00);
	write_cmos_sensor(0x5a39, 0x00);
	write_cmos_sensor(0x5a3a, 0x00);
	write_cmos_sensor(0x5a3b, 0x00);
	write_cmos_sensor(0x5a3c, 0x00);
	write_cmos_sensor(0x5a3d, 0x00);
	write_cmos_sensor(0x5a3e, 0x00);
	write_cmos_sensor(0x5a3f, 0x00);
	write_cmos_sensor(0x5a40, 0x00);
	write_cmos_sensor(0x5a41, 0x00);
	write_cmos_sensor(0x5a42, 0x00);
	write_cmos_sensor(0x5a43, 0x00);
	write_cmos_sensor(0x5a44, 0x00);
	write_cmos_sensor(0x5a45, 0x00);
	write_cmos_sensor(0x5a46, 0x00);
	write_cmos_sensor(0x5a47, 0x00);
	write_cmos_sensor(0x5a48, 0x00);
	write_cmos_sensor(0x5a49, 0x00);
	write_cmos_sensor(0x5a4a, 0x00);
	write_cmos_sensor(0x5a4b, 0x00);
	write_cmos_sensor(0x5a4c, 0x00);
	write_cmos_sensor(0x5a4d, 0x00);
	write_cmos_sensor(0x5a4e, 0x00);
	write_cmos_sensor(0x5a4f, 0x00);
	write_cmos_sensor(0x5a50, 0x00);
	write_cmos_sensor(0x5a51, 0x00);
	write_cmos_sensor(0x5a52, 0x00);
	write_cmos_sensor(0x5a53, 0x00);
	write_cmos_sensor(0x5a54, 0x00);
	write_cmos_sensor(0x5a55, 0x00);
	write_cmos_sensor(0x5a56, 0x00);
	write_cmos_sensor(0x5a57, 0x00);
	write_cmos_sensor(0x5a58, 0x00);
	write_cmos_sensor(0x5a59, 0x00);
	write_cmos_sensor(0x5a5a, 0x00);
	write_cmos_sensor(0x5a5b, 0x00);
	write_cmos_sensor(0x5a5c, 0x00);
	write_cmos_sensor(0x5a5d, 0x00);
	write_cmos_sensor(0x5a5e, 0x00);
	write_cmos_sensor(0x5a5f, 0x00);
	write_cmos_sensor(0x5a60, 0x00);
	write_cmos_sensor(0x5a61, 0x00);
	write_cmos_sensor(0x5a62, 0x00);
	write_cmos_sensor(0x5a63, 0x00);
	write_cmos_sensor(0x5a64, 0x00);
	write_cmos_sensor(0x5a65, 0x00);
	write_cmos_sensor(0x5a66, 0x00);
	write_cmos_sensor(0x5a67, 0x00);
	write_cmos_sensor(0x5a68, 0x00);
	write_cmos_sensor(0x5a69, 0x00);
	write_cmos_sensor(0x5a6a, 0x00);
	write_cmos_sensor(0x5a6b, 0x00);
	write_cmos_sensor(0x5a6c, 0x00);
	write_cmos_sensor(0x5a6d, 0x00);
	write_cmos_sensor(0x5a6e, 0x00);
	write_cmos_sensor(0x5a6f, 0x00);
	write_cmos_sensor(0x5a70, 0x00);
	write_cmos_sensor(0x5a71, 0x00);
	write_cmos_sensor(0x5a72, 0x00);
	write_cmos_sensor(0x5a73, 0x00);
	write_cmos_sensor(0x5a74, 0x00);
	write_cmos_sensor(0x5a75, 0x00);
	write_cmos_sensor(0x5a76, 0x00);
	write_cmos_sensor(0x5a77, 0x00);
	write_cmos_sensor(0x5a78, 0x00);
	write_cmos_sensor(0x5a79, 0x00);
	write_cmos_sensor(0x5a7a, 0x00);
	write_cmos_sensor(0x5a7b, 0x00);
	write_cmos_sensor(0x5a7c, 0x00);
	write_cmos_sensor(0x5a7d, 0x00);
	write_cmos_sensor(0x5a7e, 0x00);
	write_cmos_sensor(0x5a7f, 0x00);
	write_cmos_sensor(0x5a80, 0x00);
	write_cmos_sensor(0x5a81, 0x00);
	write_cmos_sensor(0x5a82, 0x00);
	write_cmos_sensor(0x5a83, 0x00);
	write_cmos_sensor(0x5a84, 0x00);
	write_cmos_sensor(0x5a85, 0x00);
	write_cmos_sensor(0x5a86, 0x00);
	write_cmos_sensor(0x5a87, 0x00);
	write_cmos_sensor(0x5a88, 0x00);
	write_cmos_sensor(0x5a89, 0x00);
	write_cmos_sensor(0x5a8a, 0x00);
	write_cmos_sensor(0x5a8b, 0x00);
	write_cmos_sensor(0x5a8c, 0x00);
	write_cmos_sensor(0x5a8d, 0x00);
	write_cmos_sensor(0x5a8e, 0x00);
	write_cmos_sensor(0x5a8f, 0x00);
	write_cmos_sensor(0x5a90, 0x00);
	write_cmos_sensor(0x5a91, 0x00);
	write_cmos_sensor(0x5a92, 0x00);
	write_cmos_sensor(0x5a93, 0x00);
	write_cmos_sensor(0x5a94, 0x00);
	write_cmos_sensor(0x5a95, 0x00);
	write_cmos_sensor(0x5a96, 0x00);
	write_cmos_sensor(0x5a97, 0x00);
	write_cmos_sensor(0x5a98, 0x00);
	write_cmos_sensor(0x5a99, 0x00);
	write_cmos_sensor(0x5a9a, 0x00);
	write_cmos_sensor(0x5a9b, 0x00);
	write_cmos_sensor(0x5a9c, 0x00);
	write_cmos_sensor(0x5a9d, 0x00);
	write_cmos_sensor(0x5a9e, 0x00);
	write_cmos_sensor(0x5a9f, 0x00);
	write_cmos_sensor(0x5aa0, 0x00);
	write_cmos_sensor(0x5aa1, 0x00);
	write_cmos_sensor(0x5aa2, 0x00);
	write_cmos_sensor(0x5aa3, 0x00);
	write_cmos_sensor(0x5aa4, 0x00);
	write_cmos_sensor(0x5aa5, 0x00);
	write_cmos_sensor(0x5aa6, 0x00);
	write_cmos_sensor(0x5aa7, 0x00);
	write_cmos_sensor(0x5aa8, 0x00);
	write_cmos_sensor(0x5aa9, 0x00);
	write_cmos_sensor(0x5aaa, 0x00);
	write_cmos_sensor(0x5aab, 0x00);
	write_cmos_sensor(0x5aac, 0x00);
	write_cmos_sensor(0x5aad, 0x00);
	write_cmos_sensor(0x5aae, 0x00);
	write_cmos_sensor(0x5aaf, 0x00);
	write_cmos_sensor(0x5ab0, 0x00);
	write_cmos_sensor(0x5ab1, 0x00);
	write_cmos_sensor(0x5ab2, 0x00);
	write_cmos_sensor(0x5ab3, 0x00);
	write_cmos_sensor(0x5ab4, 0x00);
	write_cmos_sensor(0x5ab5, 0x00);
	write_cmos_sensor(0x5ab6, 0x00);
	write_cmos_sensor(0x5ab7, 0x00);
	write_cmos_sensor(0x5ab8, 0x00);
	write_cmos_sensor(0x5ab9, 0x00);
	write_cmos_sensor(0x5aba, 0x00);
	write_cmos_sensor(0x5abb, 0x00);
	write_cmos_sensor(0x5abc, 0x00);
	write_cmos_sensor(0x5abd, 0x00);
	write_cmos_sensor(0x5abe, 0x00);
	write_cmos_sensor(0x5abf, 0x00);
	write_cmos_sensor(0x5ac0, 0x00);
	write_cmos_sensor(0x5ac1, 0x00);
	write_cmos_sensor(0x5ac2, 0x00);
	write_cmos_sensor(0x5ac3, 0x00);
	write_cmos_sensor(0x5ac4, 0x00);
	write_cmos_sensor(0x5ac5, 0x00);
	write_cmos_sensor(0x5ac6, 0x00);
	write_cmos_sensor(0x5ac7, 0x00);
	write_cmos_sensor(0x5ac8, 0x00);
	write_cmos_sensor(0x5ac9, 0x00);
	write_cmos_sensor(0x5aca, 0x00);
	write_cmos_sensor(0x5acb, 0x00);
	write_cmos_sensor(0x5acc, 0x00);
	write_cmos_sensor(0x5acd, 0x00);
	write_cmos_sensor(0x5ace, 0x00);
	write_cmos_sensor(0x5acf, 0x00);
	write_cmos_sensor(0x5ad0, 0x00);
	write_cmos_sensor(0x5ad1, 0x00);
	write_cmos_sensor(0x5ad2, 0x00);
	write_cmos_sensor(0x5ad3, 0x00);
	write_cmos_sensor(0x5ad4, 0x00);
	write_cmos_sensor(0x5ad5, 0x00);
	write_cmos_sensor(0x5ad6, 0x00);
	write_cmos_sensor(0x5ad7, 0x00);
	write_cmos_sensor(0x5ad8, 0x00);
	write_cmos_sensor(0x5ad9, 0x00);
	write_cmos_sensor(0x5ada, 0x00);
	write_cmos_sensor(0x5adb, 0x00);
	write_cmos_sensor(0x5adc, 0x00);
	write_cmos_sensor(0x5add, 0x00);
	write_cmos_sensor(0x5ade, 0x00);
	write_cmos_sensor(0x5adf, 0x00);
	write_cmos_sensor(0x5ae0, 0x00);
	write_cmos_sensor(0x5ae1, 0x00);
	write_cmos_sensor(0x5ae2, 0x00);
	write_cmos_sensor(0x5ae3, 0x00);
	write_cmos_sensor(0x5ae4, 0x00);
	write_cmos_sensor(0x5ae5, 0x00);
	write_cmos_sensor(0x5ae6, 0x00);
	write_cmos_sensor(0x5ae7, 0x00);
	write_cmos_sensor(0x5ae8, 0x00);
	write_cmos_sensor(0x5ae9, 0x00);
	write_cmos_sensor(0x5aea, 0x00);
	write_cmos_sensor(0x5aeb, 0x00);
	write_cmos_sensor(0x5aec, 0x00);
	write_cmos_sensor(0x5aed, 0x00);
	write_cmos_sensor(0x5aee, 0x00);
	write_cmos_sensor(0x5aef, 0x00);
	write_cmos_sensor(0x5af0, 0x00);
	write_cmos_sensor(0x5af1, 0x00);
	write_cmos_sensor(0x5af2, 0x00);
	write_cmos_sensor(0x5af3, 0x00);
	write_cmos_sensor(0x5af4, 0x00);
	write_cmos_sensor(0x5af5, 0x00);
	write_cmos_sensor(0x5af6, 0x00);
	write_cmos_sensor(0x5af7, 0x00);
	write_cmos_sensor(0x5af8, 0x00);
	write_cmos_sensor(0x5af9, 0x00);
	write_cmos_sensor(0x5afa, 0x00);
	write_cmos_sensor(0x5afb, 0x00);
	write_cmos_sensor(0x5afc, 0x00);
	write_cmos_sensor(0x5afd, 0x00);
	write_cmos_sensor(0x5afe, 0x00);
	write_cmos_sensor(0x5aff, 0x00);
	write_cmos_sensor(0x5b00, 0x00);
	write_cmos_sensor(0x5b01, 0x00);
	write_cmos_sensor(0x5b02, 0x00);
	write_cmos_sensor(0x5b03, 0x00);
	write_cmos_sensor(0x5b04, 0x00);
	write_cmos_sensor(0x5b05, 0x00);
	write_cmos_sensor(0x5b06, 0x00);
	write_cmos_sensor(0x5b07, 0x00);
	write_cmos_sensor(0x5b08, 0x00);
	write_cmos_sensor(0x5b09, 0x00);
	write_cmos_sensor(0x5b0a, 0x00);
	write_cmos_sensor(0x5b0b, 0x00);
	write_cmos_sensor(0x5b0c, 0x00);
	write_cmos_sensor(0x5b0d, 0x00);
	write_cmos_sensor(0x5b0e, 0x00);
	write_cmos_sensor(0x5b0f, 0x00);
	write_cmos_sensor(0x5b10, 0x00);
	write_cmos_sensor(0x5b11, 0x00);
	write_cmos_sensor(0x5b12, 0x00);
	write_cmos_sensor(0x5b13, 0x00);
	write_cmos_sensor(0x5b14, 0x00);
	write_cmos_sensor(0x5b15, 0x00);
	write_cmos_sensor(0x5b16, 0x00);
	write_cmos_sensor(0x5b17, 0x00);
	write_cmos_sensor(0x5b18, 0x00);
	write_cmos_sensor(0x5b19, 0x00);
	write_cmos_sensor(0x5b1a, 0x00);
	write_cmos_sensor(0x5b1b, 0x00);
	write_cmos_sensor(0x5b1c, 0x00);
	write_cmos_sensor(0x5b1d, 0x00);
	write_cmos_sensor(0x5b1e, 0x00);
	write_cmos_sensor(0x5b1f, 0x00);
	write_cmos_sensor(0x5b20, 0x00);
	write_cmos_sensor(0x5b21, 0x00);
	write_cmos_sensor(0x5b22, 0x00);
	write_cmos_sensor(0x5b23, 0x00);
	write_cmos_sensor(0x5b24, 0x00);
	write_cmos_sensor(0x5b25, 0x00);
	write_cmos_sensor(0x5b26, 0x00);
	write_cmos_sensor(0x5b27, 0x00);
	write_cmos_sensor(0x5b28, 0x00);
	write_cmos_sensor(0x5b29, 0x00);
	write_cmos_sensor(0x5b2a, 0x00);
	write_cmos_sensor(0x5b2b, 0x00);
	write_cmos_sensor(0x5b2c, 0x00);
	write_cmos_sensor(0x5b2d, 0x00);
	write_cmos_sensor(0x5b2e, 0x00);
	write_cmos_sensor(0x5b2f, 0x00);
	write_cmos_sensor(0x5b30, 0x00);
	write_cmos_sensor(0x5b31, 0x00);
	write_cmos_sensor(0x5b32, 0x00);
	write_cmos_sensor(0x5b33, 0x00);
	write_cmos_sensor(0x5b34, 0x00);
	write_cmos_sensor(0x5b35, 0x00);
	write_cmos_sensor(0x5b36, 0x00);
	write_cmos_sensor(0x5b37, 0x00);
	write_cmos_sensor(0x5b38, 0x00);
	write_cmos_sensor(0x5b39, 0x00);
	write_cmos_sensor(0x5b3a, 0x00);
	write_cmos_sensor(0x5b3b, 0x00);
	write_cmos_sensor(0x5b3c, 0x00);
	write_cmos_sensor(0x5b3d, 0x00);
	write_cmos_sensor(0x5b3e, 0x00);
	write_cmos_sensor(0x5b3f, 0x00);
	write_cmos_sensor(0x5b40, 0x00);
	write_cmos_sensor(0x5b41, 0x00);
	write_cmos_sensor(0x5b42, 0x00);
	write_cmos_sensor(0x5b43, 0x00);
	write_cmos_sensor(0x5b44, 0x00);
	write_cmos_sensor(0x5b45, 0x00);
	write_cmos_sensor(0x5b46, 0x00);
	write_cmos_sensor(0x5b47, 0x00);
	write_cmos_sensor(0x5b48, 0x00);
	write_cmos_sensor(0x5b49, 0x00);
	write_cmos_sensor(0x5b4a, 0x00);
	write_cmos_sensor(0x5b4b, 0x00);
	write_cmos_sensor(0x5b4c, 0x00);
	write_cmos_sensor(0x5b4d, 0x00);
	write_cmos_sensor(0x5b4e, 0x00);
	write_cmos_sensor(0x5b4f, 0x00);
	write_cmos_sensor(0x5b50, 0x00);
	write_cmos_sensor(0x5b51, 0x00);
	write_cmos_sensor(0x5b52, 0x00);
	write_cmos_sensor(0x5b53, 0x00);
	write_cmos_sensor(0x5b56, 0x10);
	write_cmos_sensor(0x5b57, 0x00);
	write_cmos_sensor(0x5d00, 0x00);
	write_cmos_sensor(0x5d01, 0x00);
	write_cmos_sensor(0x5d02, 0x00);
	write_cmos_sensor(0x5d04, 0x00);
	write_cmos_sensor(0x5d1b, 0x00);
	write_cmos_sensor(0x5d1c, 0x00);
	write_cmos_sensor(0x5d1d, 0x00);
	write_cmos_sensor(0x5d1e, 0x00);
	write_cmos_sensor(0x5d32, 0x00);
	write_cmos_sensor(0x5d60, 0x00);
	write_cmos_sensor(0x5d61, 0x00);
	write_cmos_sensor(0x5d62, 0x00);
	write_cmos_sensor(0x5d63, 0x00);
	write_cmos_sensor(0x5d69, 0x00);
	write_cmos_sensor(0x5d6a, 0x00);
	write_cmos_sensor(0x5d6b, 0x00);
	write_cmos_sensor(0x5d6d, 0x00);
	write_cmos_sensor(0x5d6e, 0x00);
	write_cmos_sensor(0x5d6f, 0x00);
	write_cmos_sensor(0x5d78, 0x00);
	write_cmos_sensor(0x5d7a, 0x00);
	write_cmos_sensor(0x5d88, 0x00);
	write_cmos_sensor(0x5d8a, 0x00);
	write_cmos_sensor(0x5d99, 0x00);
	write_cmos_sensor(0x5d9b, 0x00);
	write_cmos_sensor(0x5da9, 0x00);
	write_cmos_sensor(0x5dab, 0x00);
	write_cmos_sensor(0x5db8, 0x00);
	write_cmos_sensor(0x5dba, 0x00);
	write_cmos_sensor(0x5dc8, 0x00);
	write_cmos_sensor(0x5dca, 0x00);
	write_cmos_sensor(0x5dd9, 0x00);
	write_cmos_sensor(0x5ddb, 0x00);
	write_cmos_sensor(0x5de9, 0x00);
	write_cmos_sensor(0x5deb, 0x00);
	write_cmos_sensor(0x5df4, 0x00);
	write_cmos_sensor(0x5df6, 0x00);
	write_cmos_sensor(0x5df8, 0x00);
	write_cmos_sensor(0x5dfa, 0x00);
	write_cmos_sensor(0x5dfc, 0x00);
	write_cmos_sensor(0x5dfe, 0x00);
	write_cmos_sensor(0x5e04, 0x00);
	write_cmos_sensor(0x5e06, 0x00);
	write_cmos_sensor(0x5e08, 0x00);
	write_cmos_sensor(0x5e0a, 0x00);
	write_cmos_sensor(0x5e0c, 0x00);
	write_cmos_sensor(0x5e0e, 0x00);
	write_cmos_sensor(0x5e15, 0x00);
	write_cmos_sensor(0x5e17, 0x00);
	write_cmos_sensor(0x5e19, 0x00);
	write_cmos_sensor(0x5e1b, 0x00);
	write_cmos_sensor(0x5e1d, 0x00);
	write_cmos_sensor(0x5e1f, 0x00);
	write_cmos_sensor(0x5e25, 0x00);
	write_cmos_sensor(0x5e27, 0x00);
	write_cmos_sensor(0x5e29, 0x00);
	write_cmos_sensor(0x5e2b, 0x00);
	write_cmos_sensor(0x5e2d, 0x00);
	write_cmos_sensor(0x5e2f, 0x00);
	write_cmos_sensor(0x5e34, 0x00);
	write_cmos_sensor(0x5e36, 0x00);
	write_cmos_sensor(0x5e38, 0x00);
	write_cmos_sensor(0x5e3a, 0x00);
	write_cmos_sensor(0x5e3c, 0x00);
	write_cmos_sensor(0x5e3e, 0x00);
	write_cmos_sensor(0x5e44, 0x00);
	write_cmos_sensor(0x5e46, 0x00);
	write_cmos_sensor(0x5e48, 0x00);
	write_cmos_sensor(0x5e4a, 0x00);
	write_cmos_sensor(0x5e4c, 0x00);
	write_cmos_sensor(0x5e4e, 0x00);
	write_cmos_sensor(0x5e55, 0x00);
	write_cmos_sensor(0x5e57, 0x00);
	write_cmos_sensor(0x5e59, 0x00);
	write_cmos_sensor(0x5e5b, 0x00);
	write_cmos_sensor(0x5e5d, 0x00);
	write_cmos_sensor(0x5e5f, 0x00);
	write_cmos_sensor(0x5e65, 0x00);
	write_cmos_sensor(0x5e67, 0x00);
	write_cmos_sensor(0x5e69, 0x00);
	write_cmos_sensor(0x5e6b, 0x00);
	write_cmos_sensor(0x5e6d, 0x00);
	write_cmos_sensor(0x5e6f, 0x00);
	write_cmos_sensor(0x5e70, 0x00);
	write_cmos_sensor(0x5e71, 0x00);
	write_cmos_sensor(0x5e72, 0x00);
	write_cmos_sensor(0x5e73, 0x00);
	write_cmos_sensor(0x6000, 0x00);
	write_cmos_sensor(0x6001, 0x00);
	write_cmos_sensor(0x6003, 0x00);
	write_cmos_sensor(0x6004, 0x00);
	write_cmos_sensor(0x6005, 0x00);
	write_cmos_sensor(0x6011, 0x00);
	write_cmos_sensor(0x6013, 0x00);
	write_cmos_sensor(0x6015, 0x00);
	write_cmos_sensor(0x6017, 0x00);
	write_cmos_sensor(0x6098, 0x00);
	write_cmos_sensor(0x6099, 0x00);
	write_cmos_sensor(0x609a, 0x00);
	write_cmos_sensor(0x609b, 0x00);
	write_cmos_sensor(0x609d, 0x00);
	write_cmos_sensor(0x609f, 0x00);
	write_cmos_sensor(0x60a1, 0x00);
	write_cmos_sensor(0x60a3, 0x00);
	write_cmos_sensor(0x60a6, 0x00);
	write_cmos_sensor(0x60a7, 0x00);
	write_cmos_sensor(0x60a8, 0x00);
	write_cmos_sensor(0x60a9, 0x00);
	write_cmos_sensor(0x60aa, 0x00);
	write_cmos_sensor(0x60ab, 0x00);
	write_cmos_sensor(0x60ae, 0x00);
	write_cmos_sensor(0x60af, 0x00);
}

static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable)
		write_cmos_sensor(0x0100, 0X01);
	else
		write_cmos_sensor(0x0100, 0x00);
	return ERROR_NONE;
}

static void preview_setting(void)
{
	pr_debug("E\n");

	/* 3.2 Raw 10bit 1632x1224 30fps 2lane 720M bps/lane */
	/* ;XVCLK=24Mhz, SCLK=72Mhz, MIPI 720Mbps,
	 * DACCLK=180Mhz, Tline = 8.925926us
	 */

	write_cmos_sensor(0x0304, 0xc8);	/* 800M */
	write_cmos_sensor(0x0318, 0x05);
	write_cmos_sensor(0x340c, 0x04);
	write_cmos_sensor(0x340d, 0x3c);
	write_cmos_sensor(0x3500, 0x00);
	write_cmos_sensor(0x3501, 0xBB);
	write_cmos_sensor(0x3511, 0x10);
	write_cmos_sensor(0x3602, 0x86);
	write_cmos_sensor(0x3609, 0x20);
	write_cmos_sensor(0x360b, 0x40);
	write_cmos_sensor(0x3658, 0x00);
	write_cmos_sensor(0x3659, 0x00);
	write_cmos_sensor(0x365a, 0x00);
	write_cmos_sensor(0x365b, 0x00);
	write_cmos_sensor(0x365c, 0x00);
	write_cmos_sensor(0x3674, 0x07);
	write_cmos_sensor(0x37f4, 0x80);
	write_cmos_sensor(0x37f9, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x02);
	write_cmos_sensor(0x3806, 0x0f);
	write_cmos_sensor(0x3807, 0x4d);
	write_cmos_sensor(0x3808, 0x0a);
	write_cmos_sensor(0x3809, 0x20);
	write_cmos_sensor(0x380a, 0x07);
	write_cmos_sensor(0x380b, 0x98);
	write_cmos_sensor(0x380e, 0x06);
	write_cmos_sensor(0x380f, 0xd6);
	write_cmos_sensor(0x3811, 0x07);
	write_cmos_sensor(0x3813, 0x07);
	write_cmos_sensor(0x3815, 0x44);
	write_cmos_sensor(0x3820, 0x01);
	write_cmos_sensor(0x3834, 0xf1);
	write_cmos_sensor(0x4010, 0x29);
	write_cmos_sensor(0x4012, 0x0d);
	write_cmos_sensor(0x4013, 0x16);
	write_cmos_sensor(0x4014, 0x0a);
	write_cmos_sensor(0x4016, 0x15);
	write_cmos_sensor(0x4018, 0x09);
	write_cmos_sensor(0x4500, 0x20);
	write_cmos_sensor(0x4501, 0x04);
	write_cmos_sensor(0x4503, 0x20);
	write_cmos_sensor(0x450f, 0x88);
	write_cmos_sensor(0x4540, 0x86);
	write_cmos_sensor(0x4541, 0x07);
	write_cmos_sensor(0x4542, 0x04);
	write_cmos_sensor(0x4543, 0x05);
	write_cmos_sensor(0x4544, 0x06);
	write_cmos_sensor(0x4545, 0x07);
	write_cmos_sensor(0x4546, 0x04);
	write_cmos_sensor(0x4547, 0x05);
	write_cmos_sensor(0x4548, 0x02);
	write_cmos_sensor(0x4549, 0x03);
	write_cmos_sensor(0x454a, 0x00);
	write_cmos_sensor(0x454b, 0x01);
	write_cmos_sensor(0x454c, 0x02);
	write_cmos_sensor(0x454d, 0x03);
	write_cmos_sensor(0x454e, 0x00);
	write_cmos_sensor(0x454f, 0x01);
	write_cmos_sensor(0x4550, 0x06);
	write_cmos_sensor(0x4551, 0x07);
	write_cmos_sensor(0x4552, 0x04);
	write_cmos_sensor(0x4553, 0x05);
	write_cmos_sensor(0x4554, 0x06);
	write_cmos_sensor(0x4555, 0x07);
	write_cmos_sensor(0x4556, 0x04);
	write_cmos_sensor(0x4557, 0x05);
	write_cmos_sensor(0x4558, 0x02);
	write_cmos_sensor(0x4559, 0x03);
	write_cmos_sensor(0x455a, 0x00);
	write_cmos_sensor(0x455b, 0x01);
	write_cmos_sensor(0x455c, 0x02);
	write_cmos_sensor(0x455d, 0x03);
	write_cmos_sensor(0x455e, 0x00);
	write_cmos_sensor(0x455f, 0x01);
	write_cmos_sensor(0x5000, 0x81);
	write_cmos_sensor(0x5001, 0x43);
	write_cmos_sensor(0x5002, 0x09);
	write_cmos_sensor(0x5006, 0x40);
	write_cmos_sensor(0x5080, 0x04);
	write_cmos_sensor(0x5084, 0x0a);
	write_cmos_sensor(0x5085, 0x30);
	write_cmos_sensor(0x5086, 0x07);
	write_cmos_sensor(0x5087, 0xa6);
	write_cmos_sensor(0x5184, 0x61);
	write_cmos_sensor(0x5185, 0x1c);
	write_cmos_sensor(0x5192, 0x0a);
	write_cmos_sensor(0x5195, 0x01);
	write_cmos_sensor(0x5200, 0xbf);

}				/*      preview_setting  */


#ifdef CAPTURE_SIZE_5M
#else
static void capture_setting(kal_uint16 currefps)
{

	write_cmos_sensor(0x0304, 0xf0);	/* 960M */
	write_cmos_sensor(0x0318, 0x04);
	write_cmos_sensor(0x340c, 0x0f);
	write_cmos_sensor(0x340d, 0xe0);
	write_cmos_sensor(0x3500, 0x01);
	write_cmos_sensor(0x3501, 0x04);
	write_cmos_sensor(0x3511, 0x41);
	write_cmos_sensor(0x3602, 0x8e);
	write_cmos_sensor(0x3609, 0xa0);
	write_cmos_sensor(0x360b, 0x4c);
	write_cmos_sensor(0x3658, 0xff);
	write_cmos_sensor(0x3659, 0x14);
	write_cmos_sensor(0x365a, 0x21);
	write_cmos_sensor(0x365b, 0x43);
	write_cmos_sensor(0x365c, 0xff);
	write_cmos_sensor(0x3674, 0x04);
	write_cmos_sensor(0x37f4, 0x00);
	write_cmos_sensor(0x37f9, 0x02);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x00);
	write_cmos_sensor(0x3806, 0x0f);
	write_cmos_sensor(0x3807, 0x4f);
	write_cmos_sensor(0x3808, 0x14);
	write_cmos_sensor(0x3809, 0x40);
	write_cmos_sensor(0x380a, 0x0f);
	write_cmos_sensor(0x380b, 0x30);
	write_cmos_sensor(0x380e, 0x10);
	write_cmos_sensor(0x380f, 0x68);
	write_cmos_sensor(0x3811, 0x0e);
	write_cmos_sensor(0x3813, 0x10);
	write_cmos_sensor(0x3815, 0x11);
	write_cmos_sensor(0x3820, 0x00);
	write_cmos_sensor(0x3834, 0xf0);
	write_cmos_sensor(0x4010, 0x28);
	write_cmos_sensor(0x4012, 0x6d);
	write_cmos_sensor(0x4013, 0x28);
	write_cmos_sensor(0x4014, 0x10);
	write_cmos_sensor(0x4016, 0x25);
	write_cmos_sensor(0x4018, 0x0f);
	write_cmos_sensor(0x4500, 0x00);
	write_cmos_sensor(0x4501, 0x00);
	write_cmos_sensor(0x4503, 0x60);
	write_cmos_sensor(0x450f, 0x80);
	write_cmos_sensor(0x4540, 0x99);
	write_cmos_sensor(0x4541, 0x1b);
	write_cmos_sensor(0x4542, 0x18);
	write_cmos_sensor(0x4543, 0x1a);
	write_cmos_sensor(0x4544, 0x1d);
	write_cmos_sensor(0x4545, 0x1f);
	write_cmos_sensor(0x4546, 0x1c);
	write_cmos_sensor(0x4547, 0x1e);
	write_cmos_sensor(0x4548, 0x09);
	write_cmos_sensor(0x4549, 0x0b);
	write_cmos_sensor(0x454a, 0x08);
	write_cmos_sensor(0x454b, 0x0a);
	write_cmos_sensor(0x454c, 0x0d);
	write_cmos_sensor(0x454d, 0x0f);
	write_cmos_sensor(0x454e, 0x0c);
	write_cmos_sensor(0x454f, 0x0e);
	write_cmos_sensor(0x4550, 0x09);
	write_cmos_sensor(0x4551, 0x0b);
	write_cmos_sensor(0x4552, 0x08);
	write_cmos_sensor(0x4553, 0x0a);
	write_cmos_sensor(0x4554, 0x0d);
	write_cmos_sensor(0x4555, 0x0f);
	write_cmos_sensor(0x4556, 0x0c);
	write_cmos_sensor(0x4557, 0x0e);
	write_cmos_sensor(0x4558, 0x19);
	write_cmos_sensor(0x4559, 0x1b);
	write_cmos_sensor(0x455a, 0x18);
	write_cmos_sensor(0x455b, 0x1a);
	write_cmos_sensor(0x455c, 0x1d);
	write_cmos_sensor(0x455d, 0x1f);
	write_cmos_sensor(0x455e, 0x1c);
	write_cmos_sensor(0x455f, 0x1e);
	write_cmos_sensor(0x5000, 0x01);
	write_cmos_sensor(0x5001, 0x42);
	write_cmos_sensor(0x5002, 0x0d);
	write_cmos_sensor(0x5006, 0x00);
	write_cmos_sensor(0x5080, 0x00);
	write_cmos_sensor(0x5084, 0x00);
	write_cmos_sensor(0x5085, 0x00);
	write_cmos_sensor(0x5086, 0x00);
	write_cmos_sensor(0x5087, 0x00);
	write_cmos_sensor(0x5184, 0x03);
	write_cmos_sensor(0x5185, 0x07);
	write_cmos_sensor(0x5192, 0x09);
	write_cmos_sensor(0x5195, 0x00);
	write_cmos_sensor(0x5200, 0xbc);

}
#endif

static void normal_video_setting(kal_uint16 currefps)
{
	pr_debug("E! currefps:%d\n", currefps);

	pr_debug(
"E! video just has 30fps preview size setting ,NOT HAS 24FPS SETTING!\n");

	preview_setting();
}

static void hs_video_setting(void)
{


	write_cmos_sensor(0x0318, 0x06);
	write_cmos_sensor(0x340c, 0x04);
	write_cmos_sensor(0x340d, 0x3c);
	write_cmos_sensor(0x3500, 0x00);
	write_cmos_sensor(0x3501, 0x5D);
	write_cmos_sensor(0x3511, 0x10);
	write_cmos_sensor(0x3602, 0x86);
	write_cmos_sensor(0x3609, 0x20);
	write_cmos_sensor(0x360b, 0x40);
	write_cmos_sensor(0x3658, 0x00);
	write_cmos_sensor(0x3659, 0x00);
	write_cmos_sensor(0x365a, 0x00);
	write_cmos_sensor(0x365b, 0x00);
	write_cmos_sensor(0x365c, 0x00);
	write_cmos_sensor(0x3674, 0x07);
	write_cmos_sensor(0x37f4, 0x80);
	write_cmos_sensor(0x37f9, 0x00);
	write_cmos_sensor(0x3802, 0x01);
	write_cmos_sensor(0x3803, 0xEA);
	write_cmos_sensor(0x3806, 0x0D);
	write_cmos_sensor(0x3807, 0x65);
	write_cmos_sensor(0x3808, 0x0a);
	write_cmos_sensor(0x3809, 0x20);
	write_cmos_sensor(0x380a, 0x05);
	write_cmos_sensor(0x380b, 0xB2);
	write_cmos_sensor(0x380e, 0x05);
	write_cmos_sensor(0x380f, 0xD8);
	write_cmos_sensor(0x3811, 0x07);
	write_cmos_sensor(0x3813, 0x07);
	write_cmos_sensor(0x3815, 0x44);
	write_cmos_sensor(0x3820, 0x01);
	write_cmos_sensor(0x3834, 0xf1);
	write_cmos_sensor(0x4010, 0x29);
	write_cmos_sensor(0x4012, 0x0d);
	write_cmos_sensor(0x4013, 0x16);
	write_cmos_sensor(0x4014, 0x0a);
	write_cmos_sensor(0x4016, 0x15);
	write_cmos_sensor(0x4018, 0x09);
	write_cmos_sensor(0x4500, 0x20);
	write_cmos_sensor(0x4501, 0x04);
	write_cmos_sensor(0x4503, 0x20);
	write_cmos_sensor(0x450f, 0x88);
	write_cmos_sensor(0x4540, 0x86);
	write_cmos_sensor(0x4541, 0x07);
	write_cmos_sensor(0x4542, 0x04);
	write_cmos_sensor(0x4543, 0x05);
	write_cmos_sensor(0x4544, 0x06);
	write_cmos_sensor(0x4545, 0x07);
	write_cmos_sensor(0x4546, 0x04);
	write_cmos_sensor(0x4547, 0x05);
	write_cmos_sensor(0x4548, 0x02);
	write_cmos_sensor(0x4549, 0x03);
	write_cmos_sensor(0x454a, 0x00);
	write_cmos_sensor(0x454b, 0x01);
	write_cmos_sensor(0x454c, 0x02);
	write_cmos_sensor(0x454d, 0x03);
	write_cmos_sensor(0x454e, 0x00);
	write_cmos_sensor(0x454f, 0x01);
	write_cmos_sensor(0x4550, 0x06);
	write_cmos_sensor(0x4551, 0x07);
	write_cmos_sensor(0x4552, 0x04);
	write_cmos_sensor(0x4553, 0x05);
	write_cmos_sensor(0x4554, 0x06);
	write_cmos_sensor(0x4555, 0x07);
	write_cmos_sensor(0x4556, 0x04);
	write_cmos_sensor(0x4557, 0x05);
	write_cmos_sensor(0x4558, 0x02);
	write_cmos_sensor(0x4559, 0x03);
	write_cmos_sensor(0x455a, 0x00);
	write_cmos_sensor(0x455b, 0x01);
	write_cmos_sensor(0x455c, 0x02);
	write_cmos_sensor(0x455d, 0x03);
	write_cmos_sensor(0x455e, 0x00);
	write_cmos_sensor(0x455f, 0x01);
	write_cmos_sensor(0x5000, 0x81);
	write_cmos_sensor(0x5001, 0x43);
	write_cmos_sensor(0x5002, 0x09);
	write_cmos_sensor(0x5006, 0x40);
	write_cmos_sensor(0x5080, 0x04);
	write_cmos_sensor(0x5084, 0x0a);
	write_cmos_sensor(0x5085, 0x30);
	write_cmos_sensor(0x5086, 0x05);
	write_cmos_sensor(0x5087, 0xBE);
	write_cmos_sensor(0x5184, 0x61);
	write_cmos_sensor(0x5185, 0x1c);
	write_cmos_sensor(0x5192, 0x0a);
	write_cmos_sensor(0x5195, 0xF5);
	write_cmos_sensor(0x5200, 0xbf);

}

static void slim_video_setting(void)
{
	pr_debug("E\n");
	preview_setting();
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

static kal_uint32 return_sensor_id(void)
{
	return (((read_cmos_sensor(0x300a) << 16) |
		  (read_cmos_sensor(0x300b) << 8) |
		  read_cmos_sensor(0x300c))
		  & 0xFFFFFF);
}

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
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {

				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			pr_debug("Read sensor id fail:0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
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
	/* const kal_uint8 i2c_addr[] =
	 * {IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2};
	 */

	kal_uint8 i = 0;
	kal_uint8 retry = 1;
	kal_uint32 sensor_id = 0;

	pr_debug("PLATFORM:MIPI 4LANE ov20880+++++ ++++\n");

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
				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
		    pr_debug("Read sensor id fail, id: 0x%x,sensor_id =0x%x\n",
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
	imgsensor.shutter = 0x2D00;
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
	pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.current_fps = imgsensor.current_fps;
/* imgsensor.autoflicker_en = KAL_FALSE; */
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
/* mdelay(10); */
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
	pr_debug("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
	/* PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M */
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		/* imgsensor.autoflicker_en = KAL_FALSE; */

	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			pr_debug(
			"Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n",
			     imgsensor_info.cap1.max_framerate / 10,
			     imgsensor_info.cap.max_framerate);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		/* imgsensor.autoflicker_en = KAL_FALSE; */
	}
	spin_unlock(&imgsensor_drv_lock);

#ifdef CAPTURE_SIZE_5M
	preview_setting();
#else
	capture_setting(imgsensor.current_fps);
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
	/* imgsensor.autoflicker_en = KAL_FALSE; */
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(30);

	return ERROR_NONE;
}				/*      normal_video   */

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
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
/* mdelay(10); */

	return ERROR_NONE;
}				/*      hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");

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
/* mdelay(10); */

	return ERROR_NONE;
}				/*      slim_video       */



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
}				/*      get_resolution  */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("scenario_id = %d %d\n",
		scenario_id, sensor_info->SensorOutputDataFormat);

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

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	/* The frame of setting
	 * shutter default 0 for TG int
	 */

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
	pr_debug("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)	/* Dynamic frame rate */
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
	/* kal_int16 dummyLine; */
	kal_uint32 frameHeight;

	pr_debug("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	if (framerate == 0)
		return ERROR_NONE;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frameHeight = imgsensor_info.pre.pclk
			/ framerate * 10 / imgsensor_info.pre.linelength;

		pr_debug("frameHeight = %d\n", frameHeight);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frameHeight > imgsensor_info.pre.framelength)
		      ? (frameHeight - imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frameHeight =
		    imgsensor_info.normal_video.pclk
		    / framerate * 10 / imgsensor_info.normal_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frameHeight >  imgsensor_info.normal_video.framelength)
		  ? (frameHeight - imgsensor_info.normal_video.framelength) : 0;

		imgsensor.frame_length =
		 imgsensor_info.normal_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		frameHeight = imgsensor_info.cap.pclk
			/ framerate * 10 / imgsensor_info.cap.linelength;

		spin_lock(&imgsensor_drv_lock);

		imgsensor.dummy_line =
			(frameHeight > imgsensor_info.cap.framelength)
		      ? (frameHeight - imgsensor_info.cap.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.cap.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frameHeight = imgsensor_info.hs_video.pclk
		    / framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frameHeight > imgsensor_info.hs_video.framelength)
		      ? (frameHeight - imgsensor_info.hs_video.framelength) : 0;

		imgsensor.frame_length =
		    imgsensor_info.hs_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frameHeight = imgsensor_info.slim_video.pclk
		    / framerate * 10 / imgsensor_info.slim_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		      (frameHeight > imgsensor_info.slim_video.framelength)
		    ? (frameHeight - imgsensor_info.slim_video.framelength) : 0;

		imgsensor.frame_length =
		   imgsensor_info.slim_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	default:		/* coding with  preview scenario by default */
		frameHeight = imgsensor_info.pre.pclk
		    / framerate * 10 / imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frameHeight > imgsensor_info.pre.framelength)
		      ? (frameHeight - imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

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
	pr_debug("enable: %d\n", enable);



	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

#define EEPROM_READ_ID  0xA0
static void read_eeprom(int offset, char *data, kal_uint32 size)
{
	int i = 0, addr = offset;
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	for (i = 0; i < size; i++) {
		pu_send_cmd[0] = (char)(addr >> 8);
		pu_send_cmd[1] = (char)(addr & 0xFF);
		iReadRegI2C(pu_send_cmd, 2, &data[i], 1, EEPROM_READ_ID);

		addr++;
	}
}



static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;
	/* unsigned long long *feature_return_para =
	 * (unsigned long long *) feature_para;
	 */

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/* pr_debug("feature_id = %d\n", feature_id); */
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
		set_gain((UINT16) *feature_data);
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
		/* get the lens driver ID from EEPROM
		 * or just return LENS_DRIVER_ID_DO_NOT_CARE
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
			(BOOL) (*feature_data_16), (*(feature_data_16 + 1)));
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
		imgsensor.current_fps = (UINT16) *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		pr_debug("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (UINT8) *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32) *feature_data);

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

	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));

		ihdr_write_shutter_gain((UINT16) *feature_data,
					(UINT16) *(feature_data + 1),
					(UINT16) *(feature_data + 2));
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

	case SENSOR_FEATURE_GET_4CELL_DATA:/*get 4 cell data from eeprom*/
	{
		int type = (kal_uint16)(*feature_data);
		char *data = (char *)(*(feature_data+1));

		/*only copy Cross Talk calibration data*/
		if (type == FOUR_CELL_CAL_TYPE_XTALK_CAL) {
			read_eeprom(0x763, data, 600+2);
		      pr_debug("read Cross Talk calibration data size= %d %d\n",
				data[0], data[1]);
		} else if (type == FOUR_CELL_CAL_TYPE_DPC) {
			read_eeprom(0x9BE, data, 832+2);
			pr_debug("read DPC calibration data size= %d %d\n",
				data[0], data[1]);
		}
		break;
	}
	default:
		break;
	}

	return ERROR_NONE;
}				/*    feature_control()  */


static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 OV20880_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*      OV5693_MIPI_RAW_SensorInit      */
