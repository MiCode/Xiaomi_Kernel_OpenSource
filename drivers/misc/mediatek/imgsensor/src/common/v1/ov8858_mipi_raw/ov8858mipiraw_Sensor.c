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
 *	 OV8858mipiraw_sensor.c
 *
 * Project:
 * --------
 *	 ALPS MT6580 2lane ov8858r2a
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *	PengtaoFan
 *  0528:ä¿®æ”¹get_info,??mipi ??? *  0604:å¢?init preview å»¶æ—¶??0ms
 *  0604:??­init settingä¸?stream on,?otp??è®?
 *  0608:??°ov???setting
 *  0703:for  ä¿®æ”¹?non continue mode 4800
 *  0703:for  crc test 0x5002=00
 *  ---RD ??????? *  0714 ??¥capture setting full size@30fps 24fps 15fps
 *  15072115172729: ??¥nick??full size 30fps settingï¼Œfor ???size ä¸???®é??
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

/************************Modify Following Strings for Debug****************/
#define PFX "OV88582LANE"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__
/****************************   Modify end    *****************************/

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
#include "kd_camera_feature.h"

#include "ov8858mipiraw_Sensor.h"








enum OV8858_VERSION {
	OV8858R2A,
	OV8858R1A
};

enum OV8858_VERSION ov8858version = OV8858R2A;
enum boot_mode_t bm;

static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {

	/* record sensor id defined in Kd_imgsensor.h */
	.sensor_id = OV8858_SENSOR_ID,

	.checksum_value = 0xc2ded17b, /* checksum value for Camera Auto Test */

	.pre = {
		.pclk = 144000000,	/* record different mode's pclk */
		.linelength = 1928, /* record different mode's linelength */
		.framelength = 2488, /* record different mode's framelength */
		.startx = 0, /* record different mode's startx of grabwindow */
		.starty = 0, /* record different mode's starty of grabwindow */

		/* record different mode's width of grabwindow */
		.grabwindow_width = 1632,
		/* record different mode's height of grabwindow */
		.grabwindow_height = 1224,

		/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount
		 * by different scenario
		 */
		.mipi_data_lp2hs_settle_dc = 30,

		/*       following for GetDefaultFramerateByScenario()  */
		.max_framerate = 300,
		},
	.cap = {
		.pclk = 144000000,
		.linelength = 1956,
		.framelength = 2530,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 30,
		.max_framerate = 300,
		},
	.cap1 = {		/* capture for PIP 15fps */
		 .pclk = 72000000,
		 .linelength = 1940,
		 .framelength = 2474,
		 .startx = 0,
		 .starty = 0,
		 .grabwindow_width = 3264,
		 .grabwindow_height = 2448,
		 .mipi_data_lp2hs_settle_dc = 30,
		 .max_framerate = 150,
		 },
	.cap2 = {		/* capture for PIP 24fps */
		 .pclk = 144000000,
		 .linelength = 2344,
		 .framelength = 2556,
		 .startx = 0,
		 .starty = 0,
		 .grabwindow_width = 3264,
		 .grabwindow_height = 2448,
		 .mipi_data_lp2hs_settle_dc = 30,
		 .max_framerate = 240,
		 },
	.normal_video = {
			 .pclk = 144000000, /* record different mode's pclk */

			 /* record different mode's linelength */
			 .linelength = 1928,
			 /* record different mode's framelength */
			 .framelength = 2488,

			 /* record different mode's startx of grabwindow */
			 .startx = 0,
			 /* record different mode's starty of grabwindow */
			 .starty = 0,

			 /* record different mode's width of grabwindow */
			 .grabwindow_width = 1632,
			 /* record different mode's height of grabwindow */
			 .grabwindow_height = 1224,

		/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount
		 * by different scenario
		 */
			 .mipi_data_lp2hs_settle_dc = 30,
			 /* following for GetDefaultFramerateByScenario()  */
			 .max_framerate = 300,
			 },
	.hs_video = {
			/* record different mode's pclk,Pengtao Modify */
		   .pclk = 144000000,

		   .linelength = 2306,/* record different mode's linelength */
		   .framelength = 520,/* record different mode's framelength */

		    /* record different mode's startx of grabwindow */
		   .startx = 0,
		    /* record different mode's starty of grabwindow */
		   .starty = 0,

		    /* record different mode's width of grabwindow */
		   .grabwindow_width = 640,
		    /* record different mode's height of grabwindow */
			.grabwindow_height = 480,

		    /* following for  MIPIDataLowPwr2HighSpeedSettleDelayCount
		     * by different scenario
		     */
		   .mipi_data_lp2hs_settle_dc = 30,
		   /*       following for GetDefaultFramerateByScenario()  */
		   .max_framerate = 1200,
		   },
	.slim_video = {
		       .pclk = 144000000, /* record different mode's pclk */

			/* record different mode's linelength */
		       .linelength = 1928,
			/* record different mode's framelength */
		       .framelength = 2488,

			/* record different mode's startx of grabwindow */
		       .startx = 0,
			/* record different mode's starty of grabwindow */
		       .starty = 0,

			/* record different mode's width of grabwindow */
		       .grabwindow_width = 1632,
			/* record different mode's height of grabwindow */
		       .grabwindow_height = 1224,

		       /* following for MIPIDataLowPwr2HighSpeedSettleDelayCount
			* by different scenario
			*/
		       .mipi_data_lp2hs_settle_dc = 30,
		       /*  following for GetDefaultFramerateByScenario() */
		       .max_framerate = 300,
		       },
	.margin = 4,		/* sensor framelength & shutter margin */
	.min_shutter = 4,	/* min shutter */

	/* max framelength by sensor register's limitation */
	.max_frame_length = 0x90f7,
	.ae_shut_delay_frame = 0,
	/*shutter delay frame for AE cycle,
	 *2 frame with ispGain_delay-shut_delay=2-0=2
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

	.cap_delay_frame = 1,	/* enter capture delay frame num */
	.pre_delay_frame = 1,	/* enter preview delay frame num */
	.video_delay_frame = 1,	/* enter video delay frame num */
	.hs_video_delay_frame = 3, /* enter high speed video  delay frame num */
	.slim_video_delay_frame = 3,	/* enter slim video delay frame num */

	.isp_driving_current = ISP_DRIVING_4MA,	/* mclk driving current */

	/* sensor_interface_type */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,

	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,

	/* sensor output first pixel color */
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW8_B,
	.mclk = 24,	/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_2_LANE,	/* mipi lane num */
	.i2c_addr_table = {0x42, 0x6c, 0xff},
	.i2c_speed = 400,	/* i2c read/write speed */
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x4C00,	/* current shutter */
	.gain = 0x200,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */

	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.current_fps = 0,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,

	/* current scenario id */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,	/* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0,	/* record current sensor's i2c write id */
};


/* Sensor output window information*/
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{3264, 2448, 0, 0, 3264, 2448, 1632,
	 1224, 0, 0, 1632, 1224, 0, 0, 1632, 1224},	/* Preview */
	{3264, 2448, 0, 0, 3264, 2448, 3264,
	 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448},	/* capture */
	{3264, 2448, 0, 0, 3264, 2448, 3264,
	 2448, 0, 0, 3264, 2448, 0, 0, 1632, 1224},	/* video */
	{3264, 2448, 0, 0, 3264, 2448,
	  816, 612, 88, 66, 640, 480, 0, 0, 640, 480},	/* hight speed video */
	{3264, 2448, 0, 0, 3264, 2448, 3264,
	  2448, 0, 0, 1632, 1224, 0, 0, 1632, 1224}	/* slim video */
};


enum SENSOR_DPCM_TYPE_ENUM imgsensor_dpcm_info_ov8858[10] = {
	COMP8_NONE,
	COMP8_NONE,
	COMP8_NONE,
	COMP8_NONE,
	COMP8_NONE,
	COMP8_NONE,
	COMP8_NONE,
	COMP8_NONE,
	COMP8_NONE,
	COMP8_NONE
};


#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 225
#else
#define I2C_BUFFER_LEN 3

#endif
static kal_uint16 ov8858_table_write_cmos_sensor(
					kal_uint16 *para, kal_uint32 len)
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

		if ((I2C_BUFFER_LEN - tosend) < 3 ||
			len == IDX ||
			addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd, tosend,
				imgsensor.i2c_write_id,
				3, imgsensor_info.i2c_speed);

			tosend = 0;
		}
#else
		iWriteRegI2C(puSendCmd, 3, imgsensor.i2c_write_id);
		tosend = 0;

#endif
	}
	return 0;
}

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(
		pu_send_cmd, 2, (u8 *) &get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	pr_debug("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
   /*
    * you can set dummy by imgsensor.dummy_line and imgsensor.dummy_pixel,
    * or you can set dummy by imgsensor.frame_length and imgsensor.line_length
    */
	write_cmos_sensor(0x380e, (imgsensor.frame_length >> 8) & 0xFF);
	write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	write_cmos_sensor(0x380c, (imgsensor.line_length >> 8) & 0xFF);
	write_cmos_sensor(0x380d, imgsensor.line_length & 0xFF);

}				/*      set_dummy  */


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;
	/* unsigned long flags; */

	pr_debug("framerate = %d, min framelength should enable? %d\n",
		framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);

	imgsensor.frame_length =
	    (frame_length > imgsensor.min_frame_length)
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
}			/*      set_max_framerate  */


/*************************************************************************
 * FUNCTION
 *	set_shutter
 *
 * DESCRIPTION
 *	This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 * iShutter : exposured lines
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
	 (shutter >  (imgsensor_info.max_frame_length - imgsensor_info.margin))
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

#if 0
static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0000;

	reg_gain = gain * 2;
	/* reg_gain = reg_gain & 0xFFFF; */
	return (kal_uint16) reg_gain;
}
#endif
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
/* pr_debug("set_gain %d\n", gain); */
	if (gain < BASEGAIN || gain > 10 * BASEGAIN) {
		pr_debug("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 10 * BASEGAIN)
			gain = 10 * BASEGAIN;
	}
	/* reg_gain = gain2reg(gain); */
	reg_gain = gain * 2;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	pr_debug("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor(0x3508, (reg_gain >> 8));
	write_cmos_sensor(0x3509, (reg_gain & 0xFF));
	return gain;
}				/*      set_gain  */

static void ihdr_write_shutter_gain(
				kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	pr_debug("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
	if (imgsensor.ihdr_en) {

		spin_lock(&imgsensor_drv_lock);
		if (le > imgsensor.min_frame_length - imgsensor_info.margin)
			imgsensor.frame_length = le + imgsensor_info.margin;
		else
			imgsensor.frame_length = imgsensor.min_frame_length;
		if (imgsensor.frame_length > imgsensor_info.max_frame_length)
			imgsensor.frame_length =
			imgsensor_info.max_frame_length;
		spin_unlock(&imgsensor_drv_lock);

		if (le < imgsensor_info.min_shutter)
			le = imgsensor_info.min_shutter;
		if (se < imgsensor_info.min_shutter)
			se = imgsensor_info.min_shutter;


		/* Extend frame length first */
		write_cmos_sensor(0x380e, (imgsensor.frame_length >> 8) & 0xFF);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);

		write_cmos_sensor(0x3502, (le << 4) & 0xFF);
		write_cmos_sensor(0x3501, (le >> 4) & 0xFF);
		write_cmos_sensor(0x3500, (le >> 12) & 0x0F);

		write_cmos_sensor(0x3508, (se << 4) & 0xFF);
		write_cmos_sensor(0x3507, (se >> 4) & 0xFF);
		write_cmos_sensor(0x3506, (se >> 12) & 0x0F);

		set_gain(gain);
	}

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

	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor(
			0x3820, ((read_cmos_sensor(0x3820) & 0xF9) | 0x00));
		write_cmos_sensor(
			0x3821, ((read_cmos_sensor(0x3821) & 0xF9) | 0x06));
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor(
			0x3820, ((read_cmos_sensor(0x3820) & 0xF9) | 0x00));
		write_cmos_sensor(
			0x3821, ((read_cmos_sensor(0x3821) & 0xF9) | 0x00));
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor(
			0x3820, ((read_cmos_sensor(0x3820) & 0xF9) | 0x06));
		write_cmos_sensor(
			0x3821, ((read_cmos_sensor(0x3821) & 0xF9) | 0x06));
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor(
			0x3820, ((read_cmos_sensor(0x3820) & 0xF9) | 0x06));
		write_cmos_sensor(
			0x3821, ((read_cmos_sensor(0x3821) & 0xF9) | 0x00));
		break;
	default:
		pr_debug("Error image_mirror setting\n");
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

kal_uint16 addr_data_pair_init_ov8858[] = {
	0x100, 0x00,
	0x302, 0x1e,
	0x303, 0x00,
	0x304, 0x03,
	0x30e, 0x02,
	0x30f, 0x04,
	0x312, 0x03,
	0x31e, 0x0c,
	0x3600, 0x00,
	0x3601, 0x00,
	0x3602, 0x00,
	0x3603, 0x00,
	0x3604, 0x22,
	0x3605, 0x20,
	0x3606, 0x00,
	0x3607, 0x20,
	0x3608, 0x11,
	0x3609, 0x28,
	0x360a, 0x00,
	0x360b, 0x05,
	0x360c, 0xd4,
	0x360d, 0x40,
	0x360e, 0x0c,
	0x360f, 0x20,
	0x3610, 0x07,
	0x3611, 0x20,
	0x3612, 0x88,
	0x3613, 0x80,
	0x3614, 0x58,
	0x3615, 0x00,
	0x3616, 0x4a,
	0x3617, 0x40,		/* 90 */
	0x3618, 0x5a,
	0x3619, 0x70,
	0x361a, 0x99,
	0x361b, 0x0a,
	0x361c, 0x07,
	0x361d, 0x00,
	0x361e, 0x00,
	0x361f, 0x00,
	0x3638, 0xff,
	0x3633, 0x0f,
	0x3634, 0x0f,
	0x3635, 0x0f,
	0x3636, 0x12,
	0x3645, 0x13,
	0x3646, 0x83,
	0x364a, 0x07,
	0x3015, 0x00,
	0x3018, 0x32,		/* 32 : 2LANE , 72 : 4lane */
	0x3020, 0x93,
	0x3022, 0x01,

	/* write_cmos_sensor(0x3031, 0x0a); */

	0x3031, 0x08,

	0x3034, 0x00,
	0x3106, 0x01,
	0x3305, 0xf1,
	0x3308, 0x00,
	0x3309, 0x28,
	0x330a, 0x00,
	0x330b, 0x20,
	0x330c, 0x00,
	0x330d, 0x00,
	0x330e, 0x00,
	0x330f, 0x40,
	0x3307, 0x04,
	0x3500, 0x00,
	0x3501, 0x4d,
	0x3502, 0x40,
	0x3503, 0x80,
	0x3505, 0x80,
	0x3508, 0x02,
	0x3509, 0x00,
	0x350c, 0x00,
	0x350d, 0x80,
	0x3510, 0x00,
	0x3511, 0x02,
	0x3512, 0x00,
	0x3700, 0x18,
	0x3701, 0x0c,
	0x3702, 0x28,
	0x3703, 0x19,
	0x3704, 0x14,
	0x3705, 0x00,
	0x3706, 0x82,
	0x3707, 0x04,
	0x3708, 0x24,
	0x3709, 0x33,
	0x370a, 0x01,
	0x370b, 0x82,
	0x370c, 0x04,
	0x3718, 0x12,
	0x3719, 0x31,
	0x3712, 0x42,
	0x3714, 0x24,
	0x371e, 0x19,
	0x371f, 0x40,
	0x3720, 0x05,
	0x3721, 0x05,
	0x3724, 0x06,
	0x3725, 0x01,
	0x3726, 0x06,
	0x3728, 0x05,
	0x3729, 0x02,
	0x372a, 0x03,
	0x372b, 0x53,
	0x372c, 0xa3,
	0x372d, 0x53,
	0x372e, 0x06,
	0x372f, 0x10,
	0x3730, 0x01,
	0x3731, 0x06,
	0x3732, 0x14,
	0x3733, 0x10,
	0x3734, 0x40,
	0x3736, 0x20,
	0x373a, 0x05,
	0x373b, 0x06,
	0x373c, 0x0a,
	0x373e, 0x03,
	0x3750, 0x0a,
	0x3751, 0x0e,
	0x3755, 0x10,
	0x3758, 0x00,
	0x3759, 0x4c,
	0x375a, 0x06,
	0x375b, 0x13,
	0x375c, 0x20,
	0x375d, 0x02,
	0x375e, 0x00,
	0x375f, 0x14,
	0x3768, 0x22,
	0x3769, 0x44,
	0x376a, 0x44,
	0x3761, 0x00,
	0x3762, 0x00,
	0x3763, 0x00,
	0x3766, 0xff,
	0x376b, 0x00,
	0x3772, 0x23,
	0x3773, 0x02,
	0x3774, 0x16,
	0x3775, 0x12,
	0x3776, 0x04,
	0x3777, 0x00,
	0x3778, 0x17,
	0x37a0, 0x44,
	0x37a1, 0x3d,
	0x37a2, 0x3d,
	0x37a3, 0x00,
	0x37a4, 0x00,
	0x37a5, 0x00,
	0x37a6, 0x00,
	0x37a7, 0x44,
	0x37a8, 0x4c,
	0x37a9, 0x4c,
	0x3760, 0x00,
	0x376f, 0x01,
	0x37aa, 0x44,
	0x37ab, 0x2e,
	0x37ac, 0x2e,
	0x37ad, 0x33,
	0x37ae, 0x0d,
	0x37af, 0x0d,
	0x37b0, 0x00,
	0x37b1, 0x00,
	0x37b2, 0x00,
	0x37b3, 0x42,
	0x37b4, 0x42,
	0x37b5, 0x31,
	0x37b6, 0x00,
	0x37b7, 0x00,
	0x37b8, 0x00,
	0x37b9, 0xff,
	0x3800, 0x00,
	0x3801, 0x0c,
	0x3802, 0x00,
	0x3803, 0x0c,
	0x3804, 0x0c,
	0x3805, 0xd3,
	0x3806, 0x09,
	0x3807, 0xa3,
	0x3808, 0x06,
	0x3809, 0x60,
	0x380a, 0x04,
	0x380b, 0xc8,
	0x380c, 0x07,
	0x380d, 0x88,
	0x380e, 0x04,
	0x380f, 0xdc,
	0x3810, 0x00,
	0x3811, 0x04,
	0x3813, 0x02,
	0x3814, 0x03,
	0x3815, 0x01,
	0x3820, 0x00,		/* mirror */
	0x3821, 0x67,		/* flip //0x67 */
	0x382a, 0x03,
	0x382b, 0x01,
	0x3830, 0x08,
	0x3836, 0x02,
	0x3837, 0x18,
	0x3841, 0xff,
	0x3846, 0x48,
	0x3d85, 0x16,
	0x3d8c, 0x73,
	0x3d8d, 0xde,
	0x3f08, 0x08,
	0x3f0a, 0x00,
	0x4000, 0xf1,
	0x4001, 0x10,
	0x4005, 0x10,
	0x4002, 0x27,
	0x4009, 0x81,
	0x400b, 0x0c,
	0x4011, 0x20,		/* add */
	0x401b, 0x00,
	0x401d, 0x00,
	0x4020, 0x00,
	0x4021, 0x04,
	0x4022, 0x06,
	0x4023, 0x00,
	0x4024, 0x0f,
	0x4025, 0x2a,
	0x4026, 0x0f,
	0x4027, 0x2b,
	0x4028, 0x00,
	0x4029, 0x02,
	0x402a, 0x04,
	0x402b, 0x04,
	0x402c, 0x00,
	0x402d, 0x02,
	0x402e, 0x04,
	0x402f, 0x04,
	0x401f, 0x00,
	0x4034, 0x3f,
	0x403d, 0x04,
	0x4300, 0xff,
	0x4301, 0x00,
	0x4302, 0x0f,
	0x4316, 0x00,
	0x4500, 0x58,
	0x4503, 0x18,
	0x4600, 0x00,
	0x4601, 0xcb,

	0x4800, 0x24,		/* MIPI line sync enable */

	0x481f, 0x32,
	0x4837, 0x16,
	0x4850, 0x10,
	0x4851, 0x32,
	0x4b00, 0x2a,
	0x4b0d, 0x00,
	0x4d00, 0x04,
	0x4d01, 0x18,
	0x4d02, 0xc3,
	0x4d03, 0xff,
	0x4d04, 0xff,
	0x4d05, 0xff,
	0x5000, 0x7e,
	0x5001, 0x01,
	0x5002, 0x08,
	0x5003, 0x20,
	0x5046, 0x12,
	0x5780, 0x3e,
	0x5781, 0x0f,
	0x5782, 0x44,
	0x5783, 0x02,
	0x5784, 0x01,
	0x5785, 0x00,
	0x5786, 0x00,
	0x5787, 0x04,
	0x5788, 0x02,
	0x5789, 0x0f,
	0x578a, 0xfd,
	0x578b, 0xf5,
	0x578c, 0xf5,
	0x578d, 0x03,
	0x578e, 0x08,
	0x578f, 0x0c,
	0x5790, 0x08,
	0x5791, 0x04,
	0x5792, 0x00,
	0x5793, 0x52,
	0x5794, 0xa3,
	0x5871, 0x0d,
	0x5870, 0x18,
	0x586e, 0x10,
	0x586f, 0x08,
	0x58f8, 0x3d,		/* add */
	0x5901, 0x00,
	0x5b00, 0x02,
	0x5b01, 0x10,
	0x5b02, 0x03,
	0x5b03, 0xcf,
	0x5b05, 0x6c,
	0x5e00, 0x00,
	0x5e01, 0x41,
	0x382d, 0x7f,
	0x4825, 0x3a,
	0x4826, 0x40,
	0x4808, 0x25,
	0x3763, 0x18,
	0x3768, 0xcc,
	0x470b, 0x28,
	0x4202, 0x00,
	0x400d, 0x10,
	0x4040, 0x07,		/* 04 */
	0x403e, 0x08,		/* 04 */
	0x4041, 0xc6,
	0x3007, 0x80,
	0x400a, 0x01,

};

static void sensor_init(void)
{
	pr_debug("E\n");
	/* OV8858 R2A setting */
	/* 3.1 Initialization (Global Setting) */
	/* XVCLK=24Mhz, SCLK=72Mhz, MIPI 720Mbps, DACCLK=180Mhz */
	write_cmos_sensor(0x103, 0x01);
	mdelay(5);
#if 1				/* MULTI_WRITE */
	ov8858_table_write_cmos_sensor(addr_data_pair_init_ov8858,
		    sizeof(addr_data_pair_init_ov8858) / sizeof(kal_uint16));

#else
	write_cmos_sensor(0x100, 0x00);
	write_cmos_sensor(0x302, 0x1e);
	write_cmos_sensor(0x303, 0x00);
	write_cmos_sensor(0x304, 0x03);
	write_cmos_sensor(0x30e, 0x02);
	write_cmos_sensor(0x30f, 0x04);
	write_cmos_sensor(0x312, 0x03);
	write_cmos_sensor(0x31e, 0x0c);
	write_cmos_sensor(0x3600, 0x00);
	write_cmos_sensor(0x3601, 0x00);
	write_cmos_sensor(0x3602, 0x00);
	write_cmos_sensor(0x3603, 0x00);
	write_cmos_sensor(0x3604, 0x22);
	write_cmos_sensor(0x3605, 0x20);
	write_cmos_sensor(0x3606, 0x00);
	write_cmos_sensor(0x3607, 0x20);
	write_cmos_sensor(0x3608, 0x11);
	write_cmos_sensor(0x3609, 0x28);
	write_cmos_sensor(0x360a, 0x00);
	write_cmos_sensor(0x360b, 0x05);
	write_cmos_sensor(0x360c, 0xd4);
	write_cmos_sensor(0x360d, 0x40);
	write_cmos_sensor(0x360e, 0x0c);
	write_cmos_sensor(0x360f, 0x20);
	write_cmos_sensor(0x3610, 0x07);
	write_cmos_sensor(0x3611, 0x20);
	write_cmos_sensor(0x3612, 0x88);
	write_cmos_sensor(0x3613, 0x80);
	write_cmos_sensor(0x3614, 0x58);
	write_cmos_sensor(0x3615, 0x00);
	write_cmos_sensor(0x3616, 0x4a);
	write_cmos_sensor(0x3617, 0x40);	/* 90 */
	write_cmos_sensor(0x3618, 0x5a);
	write_cmos_sensor(0x3619, 0x70);
	write_cmos_sensor(0x361a, 0x99);
	write_cmos_sensor(0x361b, 0x0a);
	write_cmos_sensor(0x361c, 0x07);
	write_cmos_sensor(0x361d, 0x00);
	write_cmos_sensor(0x361e, 0x00);
	write_cmos_sensor(0x361f, 0x00);
	write_cmos_sensor(0x3638, 0xff);
	write_cmos_sensor(0x3633, 0x0f);
	write_cmos_sensor(0x3634, 0x0f);
	write_cmos_sensor(0x3635, 0x0f);
	write_cmos_sensor(0x3636, 0x12);
	write_cmos_sensor(0x3645, 0x13);
	write_cmos_sensor(0x3646, 0x83);
	write_cmos_sensor(0x364a, 0x07);
	write_cmos_sensor(0x3015, 0x00);
	write_cmos_sensor(0x3018, 0x32);	/* 32 : 2LANE , 72 : 4lane */
	write_cmos_sensor(0x3020, 0x93);
	write_cmos_sensor(0x3022, 0x01);
	if (bm == FACTORY_BOOT || bm == ATE_FACTORY_BOOT)
		write_cmos_sensor(0x3031, 0x0a);
	else
		write_cmos_sensor(0x3031, 0x08);

	write_cmos_sensor(0x3034, 0x00);
	write_cmos_sensor(0x3106, 0x01);
	write_cmos_sensor(0x3305, 0xf1);
	write_cmos_sensor(0x3308, 0x00);
	write_cmos_sensor(0x3309, 0x28);
	write_cmos_sensor(0x330a, 0x00);
	write_cmos_sensor(0x330b, 0x20);
	write_cmos_sensor(0x330c, 0x00);
	write_cmos_sensor(0x330d, 0x00);
	write_cmos_sensor(0x330e, 0x00);
	write_cmos_sensor(0x330f, 0x40);
	write_cmos_sensor(0x3307, 0x04);
	write_cmos_sensor(0x3500, 0x00);
	write_cmos_sensor(0x3501, 0x4d);
	write_cmos_sensor(0x3502, 0x40);
	write_cmos_sensor(0x3503, 0x80);
	write_cmos_sensor(0x3505, 0x80);
	write_cmos_sensor(0x3508, 0x02);
	write_cmos_sensor(0x3509, 0x00);
	write_cmos_sensor(0x350c, 0x00);
	write_cmos_sensor(0x350d, 0x80);
	write_cmos_sensor(0x3510, 0x00);
	write_cmos_sensor(0x3511, 0x02);
	write_cmos_sensor(0x3512, 0x00);
	write_cmos_sensor(0x3700, 0x18);
	write_cmos_sensor(0x3701, 0x0c);
	write_cmos_sensor(0x3702, 0x28);
	write_cmos_sensor(0x3703, 0x19);
	write_cmos_sensor(0x3704, 0x14);
	write_cmos_sensor(0x3705, 0x00);
	write_cmos_sensor(0x3706, 0x82);
	write_cmos_sensor(0x3707, 0x04);
	write_cmos_sensor(0x3708, 0x24);
	write_cmos_sensor(0x3709, 0x33);
	write_cmos_sensor(0x370a, 0x01);
	write_cmos_sensor(0x370b, 0x82);
	write_cmos_sensor(0x370c, 0x04);
	write_cmos_sensor(0x3718, 0x12);
	write_cmos_sensor(0x3719, 0x31);
	write_cmos_sensor(0x3712, 0x42);
	write_cmos_sensor(0x3714, 0x24);
	write_cmos_sensor(0x371e, 0x19);
	write_cmos_sensor(0x371f, 0x40);
	write_cmos_sensor(0x3720, 0x05);
	write_cmos_sensor(0x3721, 0x05);
	write_cmos_sensor(0x3724, 0x06);
	write_cmos_sensor(0x3725, 0x01);
	write_cmos_sensor(0x3726, 0x06);
	write_cmos_sensor(0x3728, 0x05);
	write_cmos_sensor(0x3729, 0x02);
	write_cmos_sensor(0x372a, 0x03);
	write_cmos_sensor(0x372b, 0x53);
	write_cmos_sensor(0x372c, 0xa3);
	write_cmos_sensor(0x372d, 0x53);
	write_cmos_sensor(0x372e, 0x06);
	write_cmos_sensor(0x372f, 0x10);
	write_cmos_sensor(0x3730, 0x01);
	write_cmos_sensor(0x3731, 0x06);
	write_cmos_sensor(0x3732, 0x14);
	write_cmos_sensor(0x3733, 0x10);
	write_cmos_sensor(0x3734, 0x40);
	write_cmos_sensor(0x3736, 0x20);
	write_cmos_sensor(0x373a, 0x05);
	write_cmos_sensor(0x373b, 0x06);
	write_cmos_sensor(0x373c, 0x0a);
	write_cmos_sensor(0x373e, 0x03);
	write_cmos_sensor(0x3750, 0x0a);
	write_cmos_sensor(0x3751, 0x0e);
	write_cmos_sensor(0x3755, 0x10);
	write_cmos_sensor(0x3758, 0x00);
	write_cmos_sensor(0x3759, 0x4c);
	write_cmos_sensor(0x375a, 0x06);
	write_cmos_sensor(0x375b, 0x13);
	write_cmos_sensor(0x375c, 0x20);
	write_cmos_sensor(0x375d, 0x02);
	write_cmos_sensor(0x375e, 0x00);
	write_cmos_sensor(0x375f, 0x14);
	write_cmos_sensor(0x3768, 0x22);
	write_cmos_sensor(0x3769, 0x44);
	write_cmos_sensor(0x376a, 0x44);
	write_cmos_sensor(0x3761, 0x00);
	write_cmos_sensor(0x3762, 0x00);
	write_cmos_sensor(0x3763, 0x00);
	write_cmos_sensor(0x3766, 0xff);
	write_cmos_sensor(0x376b, 0x00);
	write_cmos_sensor(0x3772, 0x23);
	write_cmos_sensor(0x3773, 0x02);
	write_cmos_sensor(0x3774, 0x16);
	write_cmos_sensor(0x3775, 0x12);
	write_cmos_sensor(0x3776, 0x04);
	write_cmos_sensor(0x3777, 0x00);
	write_cmos_sensor(0x3778, 0x17);
	write_cmos_sensor(0x37a0, 0x44);
	write_cmos_sensor(0x37a1, 0x3d);
	write_cmos_sensor(0x37a2, 0x3d);
	write_cmos_sensor(0x37a3, 0x00);
	write_cmos_sensor(0x37a4, 0x00);
	write_cmos_sensor(0x37a5, 0x00);
	write_cmos_sensor(0x37a6, 0x00);
	write_cmos_sensor(0x37a7, 0x44);
	write_cmos_sensor(0x37a8, 0x4c);
	write_cmos_sensor(0x37a9, 0x4c);
	write_cmos_sensor(0x3760, 0x00);
	write_cmos_sensor(0x376f, 0x01);
	write_cmos_sensor(0x37aa, 0x44);
	write_cmos_sensor(0x37ab, 0x2e);
	write_cmos_sensor(0x37ac, 0x2e);
	write_cmos_sensor(0x37ad, 0x33);
	write_cmos_sensor(0x37ae, 0x0d);
	write_cmos_sensor(0x37af, 0x0d);
	write_cmos_sensor(0x37b0, 0x00);
	write_cmos_sensor(0x37b1, 0x00);
	write_cmos_sensor(0x37b2, 0x00);
	write_cmos_sensor(0x37b3, 0x42);
	write_cmos_sensor(0x37b4, 0x42);
	write_cmos_sensor(0x37b5, 0x31);
	write_cmos_sensor(0x37b6, 0x00);
	write_cmos_sensor(0x37b7, 0x00);
	write_cmos_sensor(0x37b8, 0x00);
	write_cmos_sensor(0x37b9, 0xff);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x0c);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x0c);
	write_cmos_sensor(0x3804, 0x0c);
	write_cmos_sensor(0x3805, 0xd3);
	write_cmos_sensor(0x3806, 0x09);
	write_cmos_sensor(0x3807, 0xa3);
	write_cmos_sensor(0x3808, 0x06);
	write_cmos_sensor(0x3809, 0x60);
	write_cmos_sensor(0x380a, 0x04);
	write_cmos_sensor(0x380b, 0xc8);
	write_cmos_sensor(0x380c, 0x07);
	write_cmos_sensor(0x380d, 0x88);
	write_cmos_sensor(0x380e, 0x04);
	write_cmos_sensor(0x380f, 0xdc);
	write_cmos_sensor(0x3810, 0x00);
	write_cmos_sensor(0x3811, 0x04);
	write_cmos_sensor(0x3813, 0x02);
	write_cmos_sensor(0x3814, 0x03);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3820, 0x00);	/* mirror */
	write_cmos_sensor(0x3821, 0x67);	/* flip //0x67 */
	write_cmos_sensor(0x382a, 0x03);
	write_cmos_sensor(0x382b, 0x01);
	write_cmos_sensor(0x3830, 0x08);
	write_cmos_sensor(0x3836, 0x02);
	write_cmos_sensor(0x3837, 0x18);
	write_cmos_sensor(0x3841, 0xff);
	write_cmos_sensor(0x3846, 0x48);
	write_cmos_sensor(0x3d85, 0x16);
	write_cmos_sensor(0x3d8c, 0x73);
	write_cmos_sensor(0x3d8d, 0xde);
	write_cmos_sensor(0x3f08, 0x08);
	write_cmos_sensor(0x3f0a, 0x00);
	write_cmos_sensor(0x4000, 0xf1);
	write_cmos_sensor(0x4001, 0x10);
	write_cmos_sensor(0x4005, 0x10);
	write_cmos_sensor(0x4002, 0x27);
	write_cmos_sensor(0x4009, 0x81);
	write_cmos_sensor(0x400b, 0x0c);
	write_cmos_sensor(0x4011, 0x20);	/* add */
	write_cmos_sensor(0x401b, 0x00);
	write_cmos_sensor(0x401d, 0x00);
	write_cmos_sensor(0x4020, 0x00);
	write_cmos_sensor(0x4021, 0x04);
	write_cmos_sensor(0x4022, 0x06);
	write_cmos_sensor(0x4023, 0x00);
	write_cmos_sensor(0x4024, 0x0f);
	write_cmos_sensor(0x4025, 0x2a);
	write_cmos_sensor(0x4026, 0x0f);
	write_cmos_sensor(0x4027, 0x2b);
	write_cmos_sensor(0x4028, 0x00);
	write_cmos_sensor(0x4029, 0x02);
	write_cmos_sensor(0x402a, 0x04);
	write_cmos_sensor(0x402b, 0x04);
	write_cmos_sensor(0x402c, 0x00);
	write_cmos_sensor(0x402d, 0x02);
	write_cmos_sensor(0x402e, 0x04);
	write_cmos_sensor(0x402f, 0x04);
	write_cmos_sensor(0x401f, 0x00);
	write_cmos_sensor(0x4034, 0x3f);
	write_cmos_sensor(0x403d, 0x04);
	write_cmos_sensor(0x4300, 0xff);
	write_cmos_sensor(0x4301, 0x00);
	write_cmos_sensor(0x4302, 0x0f);
	write_cmos_sensor(0x4316, 0x00);
	write_cmos_sensor(0x4500, 0x58);
	write_cmos_sensor(0x4503, 0x18);
	write_cmos_sensor(0x4600, 0x00);
	write_cmos_sensor(0x4601, 0xcb);

	write_cmos_sensor(0x4800, 0x24);	/* MIPI line sync enable */

	write_cmos_sensor(0x481f, 0x32);
	write_cmos_sensor(0x4837, 0x16);
	write_cmos_sensor(0x4850, 0x10);
	write_cmos_sensor(0x4851, 0x32);
	write_cmos_sensor(0x4b00, 0x2a);
	write_cmos_sensor(0x4b0d, 0x00);
	write_cmos_sensor(0x4d00, 0x04);
	write_cmos_sensor(0x4d01, 0x18);
	write_cmos_sensor(0x4d02, 0xc3);
	write_cmos_sensor(0x4d03, 0xff);
	write_cmos_sensor(0x4d04, 0xff);
	write_cmos_sensor(0x4d05, 0xff);
	write_cmos_sensor(0x5000, 0x7e);
	write_cmos_sensor(0x5001, 0x01);
	write_cmos_sensor(0x5002, 0x08);
	write_cmos_sensor(0x5003, 0x20);
	write_cmos_sensor(0x5046, 0x12);
	write_cmos_sensor(0x5780, 0x3e);
	write_cmos_sensor(0x5781, 0x0f);
	write_cmos_sensor(0x5782, 0x44);
	write_cmos_sensor(0x5783, 0x02);
	write_cmos_sensor(0x5784, 0x01);
	write_cmos_sensor(0x5785, 0x00);
	write_cmos_sensor(0x5786, 0x00);
	write_cmos_sensor(0x5787, 0x04);
	write_cmos_sensor(0x5788, 0x02);
	write_cmos_sensor(0x5789, 0x0f);
	write_cmos_sensor(0x578a, 0xfd);
	write_cmos_sensor(0x578b, 0xf5);
	write_cmos_sensor(0x578c, 0xf5);
	write_cmos_sensor(0x578d, 0x03);
	write_cmos_sensor(0x578e, 0x08);
	write_cmos_sensor(0x578f, 0x0c);
	write_cmos_sensor(0x5790, 0x08);
	write_cmos_sensor(0x5791, 0x04);
	write_cmos_sensor(0x5792, 0x00);
	write_cmos_sensor(0x5793, 0x52);
	write_cmos_sensor(0x5794, 0xa3);
	write_cmos_sensor(0x5871, 0x0d);
	write_cmos_sensor(0x5870, 0x18);
	write_cmos_sensor(0x586e, 0x10);
	write_cmos_sensor(0x586f, 0x08);
	write_cmos_sensor(0x58f8, 0x3d);	/* add */
	write_cmos_sensor(0x5901, 0x00);
	write_cmos_sensor(0x5b00, 0x02);
	write_cmos_sensor(0x5b01, 0x10);
	write_cmos_sensor(0x5b02, 0x03);
	write_cmos_sensor(0x5b03, 0xcf);
	write_cmos_sensor(0x5b05, 0x6c);
	write_cmos_sensor(0x5e00, 0x00);
	write_cmos_sensor(0x5e01, 0x41);
	write_cmos_sensor(0x382d, 0x7f);
	write_cmos_sensor(0x4825, 0x3a);
	write_cmos_sensor(0x4826, 0x40);
	write_cmos_sensor(0x4808, 0x25);
	write_cmos_sensor(0x3763, 0x18);
	write_cmos_sensor(0x3768, 0xcc);
	write_cmos_sensor(0x470b, 0x28);
	write_cmos_sensor(0x4202, 0x00);
	write_cmos_sensor(0x400d, 0x10);
	write_cmos_sensor(0x4040, 0x07);	/* 04 */
	write_cmos_sensor(0x403e, 0x08);	/* 04 */
	write_cmos_sensor(0x4041, 0xc6);
	write_cmos_sensor(0x3007, 0x80);
	write_cmos_sensor(0x400a, 0x01);
#endif
	/*
	 * write_cmos_sensor(0x4009, 0x83);
	 * write_cmos_sensor(0x4020, 0x00);
	 * write_cmos_sensor(0x4021, 0x04);
	 * write_cmos_sensor(0x4022, 0x04);
	 * write_cmos_sensor(0x4023, 0xb9);
	 * write_cmos_sensor(0x4024, 0x05);
	 * write_cmos_sensor(0x4025, 0x2a);
	 * write_cmos_sensor(0x4026, 0x05);
	 * write_cmos_sensor(0x4027, 0x2b);
	 * write_cmos_sensor(0x4028, 0x00);
	 * write_cmos_sensor(0x4029, 0x02);
	 * write_cmos_sensor(0x402a, 0x04);
	 * write_cmos_sensor(0x402b, 0x04);
	 * write_cmos_sensor(0x402c, 0x02);
	 * write_cmos_sensor(0x402d, 0x02);
	 * write_cmos_sensor(0x402e, 0x08);
	 * write_cmos_sensor(0x402f, 0x02);
	 */
/* write_cmos_sensor(0x100 , 0x01); */

/* mdelay(50); */
}				/*      sensor_init  */

kal_uint16 addr_data_pair_preview_ov8858[] = {
	0x0302, 0x1e,
	0x030e, 0x00,
	0x0312, 0x01,
	0x3015, 0x01,
	0x3501, 0x4d,
	0x3502, 0x40,
	0x3700, 0x30,
	0x3701, 0x18,
	0x3702, 0x50,
	0x3703, 0x32,
	0x3704, 0x28,
	0x3707, 0x08,
	0x3708, 0x48,
	0x3709, 0x66,
	0x370c, 0x07,
	0x3718, 0x14,
	0x3712, 0x44,
	0x371e, 0x31,
	0x371f, 0x7f,
	0x3720, 0x0a,
	0x3721, 0x0a,
	0x3724, 0x0c,
	0x3725, 0x02,
	0x3726, 0x0c,
	0x3728, 0x0a,
	0x3729, 0x03,
	0x372a, 0x06,
	0x372b, 0xa6,
	0x372c, 0xa6,
	0x372d, 0xa6,
	0x372e, 0x0c,
	0x372f, 0x20,
	0x3730, 0x02,
	0x3731, 0x0c,
	0x3732, 0x28,
	0x3736, 0x30,
	0x373a, 0x0a,
	0x373b, 0x0b,
	0x373c, 0x14,
	0x373e, 0x06,
	0x375a, 0x0c,
	0x375b, 0x26,
	0x375d, 0x04,
	0x375f, 0x28,
	0x3768, 0xcc,
	0x3769, 0x44,
	0x376a, 0x44,
	0x3772, 0x46,
	0x3773, 0x04,
	0x3774, 0x2c,
	0x3775, 0x13,
	0x3776, 0x08,
	0x3778, 0x17,
	0x37a0, 0x88,
	0x37a1, 0x7a,
	0x37a2, 0x7a,
	0x37a7, 0x88,
	0x37a8, 0x98,
	0x37a9, 0x98,
	0x37aa, 0x88,
	0x37ab, 0x5c,
	0x37ac, 0x5c,
	0x37ad, 0x55,
	0x37ae, 0x19,
	0x37af, 0x19,
	0x37b3, 0x84,
	0x37b4, 0x84,
	0x37b5, 0x60,
	0x3808, 0x06,
	0x3809, 0x60,
	0x380a, 0x04,
	0x380b, 0xc8,
	0x380c, 0x07,
	0x380d, 0x88,
	0x380e, 0x09,
	0x380f, 0xb8,
	0x3814, 0x03,
	0x3821, 0x67,
	0x382a, 0x03,
	0x382b, 0x01,
	0x3830, 0x08,
	0x3836, 0x02,
	0x3f08, 0x10,
	0x4001, 0x10,
	0x4022, 0x06,
	0x4023, 0x00,
	0x4025, 0x2a,
	0x4027, 0x2b,
	0x402a, 0x04,
	0x402b, 0x04,
	0x402e, 0x04,
	0x402f, 0x04,
	0x4600, 0x00,
	0x4601, 0xcb,
	0x4837, 0x16,
	0x5901, 0x00,
	0x382d, 0x7f,
	0x3031, 0x08,		/* 8 bits */
	0x4316, 0x00,		/* DPCM off */

};

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
/* ;XVCLK=24Mhz, SCLK=72Mhz, MIPI 720Mbps, DACCLK=180Mhz, Tline = 8.925926us */
	/* mdelay(5); */
#if 1				/* MULTI_WRITE */
	ov8858_table_write_cmos_sensor(addr_data_pair_preview_ov8858,
		    sizeof(addr_data_pair_preview_ov8858) / sizeof(kal_uint16));

#else
	write_cmos_sensor(0x0302, 0x1e);
	write_cmos_sensor(0x030e, 0x00);
	write_cmos_sensor(0x0312, 0x01);
	write_cmos_sensor(0x3015, 0x01);
	write_cmos_sensor(0x3501, 0x4d);
	write_cmos_sensor(0x3502, 0x40);
	write_cmos_sensor(0x3700, 0x30);
	write_cmos_sensor(0x3701, 0x18);
	write_cmos_sensor(0x3702, 0x50);
	write_cmos_sensor(0x3703, 0x32);
	write_cmos_sensor(0x3704, 0x28);
	write_cmos_sensor(0x3707, 0x08);
	write_cmos_sensor(0x3708, 0x48);
	write_cmos_sensor(0x3709, 0x66);
	write_cmos_sensor(0x370c, 0x07);
	write_cmos_sensor(0x3718, 0x14);
	write_cmos_sensor(0x3712, 0x44);
	write_cmos_sensor(0x371e, 0x31);
	write_cmos_sensor(0x371f, 0x7f);
	write_cmos_sensor(0x3720, 0x0a);
	write_cmos_sensor(0x3721, 0x0a);
	write_cmos_sensor(0x3724, 0x0c);
	write_cmos_sensor(0x3725, 0x02);
	write_cmos_sensor(0x3726, 0x0c);
	write_cmos_sensor(0x3728, 0x0a);
	write_cmos_sensor(0x3729, 0x03);
	write_cmos_sensor(0x372a, 0x06);
	write_cmos_sensor(0x372b, 0xa6);
	write_cmos_sensor(0x372c, 0xa6);
	write_cmos_sensor(0x372d, 0xa6);
	write_cmos_sensor(0x372e, 0x0c);
	write_cmos_sensor(0x372f, 0x20);
	write_cmos_sensor(0x3730, 0x02);
	write_cmos_sensor(0x3731, 0x0c);
	write_cmos_sensor(0x3732, 0x28);
	write_cmos_sensor(0x3736, 0x30);
	write_cmos_sensor(0x373a, 0x0a);
	write_cmos_sensor(0x373b, 0x0b);
	write_cmos_sensor(0x373c, 0x14);
	write_cmos_sensor(0x373e, 0x06);
	write_cmos_sensor(0x375a, 0x0c);
	write_cmos_sensor(0x375b, 0x26);
	write_cmos_sensor(0x375d, 0x04);
	write_cmos_sensor(0x375f, 0x28);
	write_cmos_sensor(0x3768, 0xcc);
	write_cmos_sensor(0x3769, 0x44);
	write_cmos_sensor(0x376a, 0x44);
	write_cmos_sensor(0x3772, 0x46);
	write_cmos_sensor(0x3773, 0x04);
	write_cmos_sensor(0x3774, 0x2c);
	write_cmos_sensor(0x3775, 0x13);
	write_cmos_sensor(0x3776, 0x08);
	write_cmos_sensor(0x3778, 0x17);
	write_cmos_sensor(0x37a0, 0x88);
	write_cmos_sensor(0x37a1, 0x7a);
	write_cmos_sensor(0x37a2, 0x7a);
	write_cmos_sensor(0x37a7, 0x88);
	write_cmos_sensor(0x37a8, 0x98);
	write_cmos_sensor(0x37a9, 0x98);
	write_cmos_sensor(0x37aa, 0x88);
	write_cmos_sensor(0x37ab, 0x5c);
	write_cmos_sensor(0x37ac, 0x5c);
	write_cmos_sensor(0x37ad, 0x55);
	write_cmos_sensor(0x37ae, 0x19);
	write_cmos_sensor(0x37af, 0x19);
	write_cmos_sensor(0x37b3, 0x84);
	write_cmos_sensor(0x37b4, 0x84);
	write_cmos_sensor(0x37b5, 0x60);
	write_cmos_sensor(0x3808, 0x06);
	write_cmos_sensor(0x3809, 0x60);
	write_cmos_sensor(0x380a, 0x04);
	write_cmos_sensor(0x380b, 0xc8);
	write_cmos_sensor(0x380c, 0x07);
	write_cmos_sensor(0x380d, 0x88);
	write_cmos_sensor(0x380e, 0x09);
	write_cmos_sensor(0x380f, 0xb8);
	write_cmos_sensor(0x3814, 0x03);
	write_cmos_sensor(0x3821, 0x67);
	write_cmos_sensor(0x382a, 0x03);
	write_cmos_sensor(0x382b, 0x01);
	write_cmos_sensor(0x3830, 0x08);
	write_cmos_sensor(0x3836, 0x02);
	write_cmos_sensor(0x3f08, 0x10);
	write_cmos_sensor(0x4001, 0x10);
	write_cmos_sensor(0x4022, 0x06);
	write_cmos_sensor(0x4023, 0x00);
	write_cmos_sensor(0x4025, 0x2a);
	write_cmos_sensor(0x4027, 0x2b);
	write_cmos_sensor(0x402a, 0x04);
	write_cmos_sensor(0x402b, 0x04);
	write_cmos_sensor(0x402e, 0x04);
	write_cmos_sensor(0x402f, 0x04);
	write_cmos_sensor(0x4600, 0x00);
	write_cmos_sensor(0x4601, 0xcb);
	write_cmos_sensor(0x4837, 0x16);
	write_cmos_sensor(0x5901, 0x00);
	write_cmos_sensor(0x382d, 0x7f);
	write_cmos_sensor(0x3031, 0x08);	/* 8 bits */
	write_cmos_sensor(0x4316, 0x00);	/* DPCM off */
	write_cmos_sensor(0x0100, 0x01);
#endif
	/* mdelay(10); */

}				/*      preview_setting  */

kal_uint16 addr_data_pair_capture_30fps_ov8858[] = {
	0x0302, 0x2e,
	0x030e, 0x00,
	0x0312, 0x01,
	0x3015, 0x01,
	0x3501, 0x9a,
	0x3502, 0x20,
	0x3700, 0x30,
	0x3701, 0x18,
	0x3702, 0x50,
	0x3703, 0x32,
	0x3704, 0x28,
	0x3707, 0x08,
	0x3708, 0x48,
	0x3709, 0x66,
	0x370c, 0x07,
	0x3718, 0x14,
	0x3712, 0x44,
	0x371e, 0x31,
	0x371f, 0x7f,
	0x3720, 0x0a,
	0x3721, 0x0a,
	0x3724, 0x0c,
	0x3725, 0x02,
	0x3726, 0x0c,
	0x3728, 0x0a,
	0x3729, 0x03,
	0x372a, 0x06,
	0x372b, 0xa6,
	0x372c, 0xa6,
	0x372d, 0xa6,
	0x372e, 0x0c,
	0x372f, 0x20,
	0x3730, 0x02,
	0x3731, 0x0c,
	0x3732, 0x28,
	0x3736, 0x30,
	0x373a, 0x0a,
	0x373b, 0x0b,
	0x373c, 0x14,
	0x373e, 0x06,
	0x375a, 0x0c,
	0x375b, 0x26,
	0x375d, 0x04,
	0x375f, 0x28,
	0x3768, 0xcc,
	0x3769, 0x44,
	0x376a, 0x44,
	0x3772, 0x46,
	0x3773, 0x04,
	0x3774, 0x2c,
	0x3775, 0x13,
	0x3776, 0x08,
	0x3778, 0x17,
	0x37a0, 0x88,
	0x37a1, 0x7a,
	0x37a2, 0x7a,
	0x37a7, 0x88,
	0x37a8, 0x98,
	0x37a9, 0x98,
	0x37aa, 0x88,
	0x37ab, 0x5c,
	0x37ac, 0x5c,
	0x37ad, 0x55,
	0x37ae, 0x19,
	0x37af, 0x19,
	0x37b3, 0x84,
	0x37b4, 0x84,
	0x37b5, 0x60,
	0x3808, 0x0c,
	0x3809, 0xc0,
	0x380a, 0x09,
	0x380b, 0x90,
	0x380c, 0x07,
	0x380d, 0xa4,
	0x380e, 0x09,
	0x380f, 0xe2,
	0x3814, 0x01,
	0x3821, 0x46,
	0x382a, 0x01,
	0x382b, 0x01,
	0x3830, 0x06,
	0x3836, 0x01,
	0x3f08, 0x10,
	0x4001, 0x00,
	0x4022, 0x0c,
	0x4023, 0x60,
	0x4025, 0x36,
	0x4027, 0x37,
	0x402a, 0x04,
	0x402b, 0x08,
	0x402e, 0x04,
	0x402f, 0x08,
	0x4600, 0x01,
	0x4601, 0x97,
	0x4837, 0x0e,
	0x5901, 0x00,
	0x382d, 0xff,
	0x3031, 0x08,

/* 0x4316, 0x01,//DPCM//factory */

	0x4316, 0x00,		/* DPCM */
};

kal_uint16 addr_data_pair_capture_15fps_ov8858[] = {
	0x0302, 0x1e,
	0x030e, 0x02,
	0x0312, 0x03,
	0x3015, 0x00,
	0x3501, 0x9a,
	0x3502, 0x20,
	0x3700, 0x18,
	0x3701, 0x0c,
	0x3702, 0x28,
	0x3703, 0x19,
	0x3704, 0x14,
	0x3707, 0x04,
	0x3708, 0x24,
	0x3709, 0x33,
	0x370c, 0x04,
	0x3718, 0x12,
	0x3712, 0x42,
	0x371e, 0x19,
	0x371f, 0x40,
	0x3720, 0x05,
	0x3721, 0x05,
	0x3724, 0x06,
	0x3725, 0x01,
	0x3726, 0x06,
	0x3728, 0x05,
	0x3729, 0x02,
	0x372a, 0x03,
	0x372b, 0x53,
	0x372c, 0xa3,
	0x372d, 0x53,
	0x372e, 0x06,
	0x372f, 0x10,
	0x3730, 0x01,
	0x3731, 0x06,
	0x3732, 0x14,
	0x3736, 0x20,
	0x373a, 0x05,
	0x373b, 0x06,
	0x373c, 0x0a,
	0x373e, 0x03,
	0x375a, 0x06,
	0x375b, 0x13,
	0x375d, 0x02,
	0x375f, 0x14,
	0x3768, 0xcc,
	0x3769, 0x44,
	0x376a, 0x44,
	0x3772, 0x23,
	0x3773, 0x02,
	0x3774, 0x16,
	0x3775, 0x12,
	0x3776, 0x04,
	0x3778, 0x1a,
	0x37a0, 0x44,
	0x37a1, 0x3d,
	0x37a2, 0x3d,
	0x37a7, 0x44,
	0x37a8, 0x4c,
	0x37a9, 0x4c,
	0x37aa, 0x44,
	0x37ab, 0x2e,
	0x37ac, 0x2e,
	0x37ad, 0x33,
	0x37ae, 0x0d,
	0x37af, 0x0d,
	0x37b3, 0x42,
	0x37b4, 0x42,
	0x37b5, 0x31,
	0x3808, 0x0c,
	0x3809, 0xc0,
	0x380a, 0x09,
	0x380b, 0x90,
	0x380c, 0x07,
	0x380d, 0x94,
	0x380e, 0x09,
	0x380f, 0xaa,
	0x3814, 0x01,
	0x3821, 0x46,
	0x382a, 0x01,
	0x382b, 0x01,
	0x3830, 0x06,
	0x3836, 0x01,
	0x3f08, 0x08,
	0x4001, 0x00,
	0x4022, 0x0c,
	0x4023, 0x60,
	0x4025, 0x36,
	0x4027, 0x37,
	0x402a, 0x04,
	0x402b, 0x08,
	0x402e, 0x04,
	0x402f, 0x08,
	0x4600, 0x01,
	0x4601, 0x97,
	0x4837, 0x10,
	0x5901, 0x00,
	0x382d, 0xff,
	0x3031, 0x08,

/* 0x4316, 0x01,//DPCM//factory */

	0x4316, 0x00,		/* DPCM */

};

kal_uint16 addr_data_pair_capture_24fps_ov8858[] = {
	0x0302, 0x2e,
	0x030e, 0x00,
	0x0312, 0x01,
	0x3015, 0x01,
	0x3501, 0x9a,
	0x3502, 0x20,
	0x3700, 0x30,
	0x3701, 0x18,
	0x3702, 0x50,
	0x3703, 0x32,
	0x3704, 0x28,
	0x3707, 0x08,
	0x3708, 0x48,
	0x3709, 0x66,
	0x370c, 0x07,
	0x3718, 0x14,
	0x3712, 0x44,
	0x371e, 0x31,
	0x371f, 0x7f,
	0x3720, 0x0a,
	0x3721, 0x0a,
	0x3724, 0x0c,
	0x3725, 0x02,
	0x3726, 0x0c,
	0x3728, 0x0a,
	0x3729, 0x03,
	0x372a, 0x06,
	0x372b, 0xa6,
	0x372c, 0xa6,
	0x372d, 0xa6,
	0x372e, 0x0c,
	0x372f, 0x20,
	0x3730, 0x02,
	0x3731, 0x0c,
	0x3732, 0x28,
	0x3736, 0x30,
	0x373a, 0x0a,
	0x373b, 0x0b,
	0x373c, 0x14,
	0x373e, 0x06,
	0x375a, 0x0c,
	0x375b, 0x26,
	0x375d, 0x04,
	0x375f, 0x28,
	0x3768, 0xcc,
	0x3769, 0x44,
	0x376a, 0x44,
	0x3772, 0x46,
	0x3773, 0x04,
	0x3774, 0x2c,
	0x3775, 0x13,
	0x3776, 0x08,
	0x3778, 0x17,
	0x37a0, 0x88,
	0x37a1, 0x7a,
	0x37a2, 0x7a,
	0x37a7, 0x88,
	0x37a8, 0x98,
	0x37a9, 0x98,
	0x37aa, 0x88,
	0x37ab, 0x5c,
	0x37ac, 0x5c,
	0x37ad, 0x55,
	0x37ae, 0x19,
	0x37af, 0x19,
	0x37b3, 0x84,
	0x37b4, 0x84,
	0x37b5, 0x60,
	0x3808, 0x0c,
	0x3809, 0xc0,
	0x380a, 0x09,
	0x380b, 0x90,
	0x380c, 0x09,
	0x380d, 0x28,
	0x380e, 0x09,
	0x380f, 0xfc,
	0x3814, 0x01,
	0x3821, 0x46,
	0x382a, 0x01,
	0x382b, 0x01,
	0x3830, 0x06,
	0x3836, 0x01,
	0x3f08, 0x10,
	0x4001, 0x00,
	0x4022, 0x0c,
	0x4023, 0x60,
	0x4025, 0x36,
	0x4027, 0x37,
	0x402a, 0x04,
	0x402b, 0x08,
	0x402e, 0x04,
	0x402f, 0x08,
	0x4600, 0x01,
	0x4601, 0x97,
	0x4837, 0x0d,
	0x5901, 0x00,
	0x382d, 0xff,

	0x3031, 0x08,


	0x4316, 0x00,		/* DPCM */


};

static void capture_setting(kal_uint16 currefps)
{
	pr_debug("E! currefps:%d\n", currefps);
	if (currefps == 300) {
		ov8858_table_write_cmos_sensor(
		addr_data_pair_capture_30fps_ov8858,
	    sizeof(addr_data_pair_capture_30fps_ov8858) / sizeof(kal_uint16));
	}
	if (currefps == 150) {
		ov8858_table_write_cmos_sensor(
		addr_data_pair_capture_15fps_ov8858,
	    sizeof(addr_data_pair_capture_15fps_ov8858) / sizeof(kal_uint16));
	}
	if (currefps == 240) {
		ov8858_table_write_cmos_sensor(
			addr_data_pair_capture_24fps_ov8858,
	    sizeof(addr_data_pair_capture_24fps_ov8858) / sizeof(kal_uint16));

	}
	if (bm == FACTORY_BOOT || bm == ATE_FACTORY_BOOT)
		write_cmos_sensor(0x4316, 0x01);	/* DPCM */

}


static void normal_video_setting(kal_uint16 currefps)
{
	pr_debug("E! currefps:%d\n", currefps);

	pr_debug(
"E! video just has 30fps preview size setting ,NOT HAS 24FPS SETTING!\n");

	preview_setting();
}

static void hs_video_setting(void)
{
	pr_debug("E\n");
/* @@640x480 120fps 2lane 720M bps/lane */
/* ;;MIPI=720Mbps, SysClk=144Mhz,Dac Clock=360Mhz */
/* ;;2306x520x120 */
/* 100 99 640 480 */

	write_cmos_sensor(0x0302, 0x1e);
	write_cmos_sensor(0x030e, 0x00);
	write_cmos_sensor(0x0312, 0x01);
	write_cmos_sensor(0x3015, 0x01);
	write_cmos_sensor(0x3501, 0x20);
	write_cmos_sensor(0x3502, 0x00);
	write_cmos_sensor(0x3700, 0x30);
	write_cmos_sensor(0x3701, 0x18);
	write_cmos_sensor(0x3702, 0x50);
	write_cmos_sensor(0x3703, 0x32);
	write_cmos_sensor(0x3704, 0x28);
	write_cmos_sensor(0x3707, 0x08);
	write_cmos_sensor(0x3708, 0x48);
	write_cmos_sensor(0x3709, 0x66);
	write_cmos_sensor(0x370c, 0x07);
	write_cmos_sensor(0x3718, 0x14);
	write_cmos_sensor(0x3712, 0x44);
	write_cmos_sensor(0x371e, 0x31);
	write_cmos_sensor(0x371f, 0x7f);
	write_cmos_sensor(0x3720, 0x0a);
	write_cmos_sensor(0x3721, 0x0a);
	write_cmos_sensor(0x3724, 0x0c);
	write_cmos_sensor(0x3725, 0x02);
	write_cmos_sensor(0x3726, 0x0c);
	write_cmos_sensor(0x3728, 0x0a);
	write_cmos_sensor(0x3729, 0x03);
	write_cmos_sensor(0x372a, 0x06);
	write_cmos_sensor(0x372b, 0xa6);
	write_cmos_sensor(0x372c, 0xa6);
	write_cmos_sensor(0x372d, 0xa6);
	write_cmos_sensor(0x372e, 0x0c);
	write_cmos_sensor(0x372f, 0x20);
	write_cmos_sensor(0x3730, 0x02);
	write_cmos_sensor(0x3731, 0x0c);
	write_cmos_sensor(0x3732, 0x28);
	write_cmos_sensor(0x3736, 0x30);
	write_cmos_sensor(0x373a, 0x0a);
	write_cmos_sensor(0x373b, 0x0b);
	write_cmos_sensor(0x373c, 0x14);
	write_cmos_sensor(0x373e, 0x06);
	write_cmos_sensor(0x375a, 0x0c);
	write_cmos_sensor(0x375b, 0x26);
	write_cmos_sensor(0x375d, 0x04);
	write_cmos_sensor(0x375f, 0x28);
	write_cmos_sensor(0x3768, 0x00);
	write_cmos_sensor(0x3769, 0xc0);
	write_cmos_sensor(0x376a, 0x42);
	write_cmos_sensor(0x3772, 0x46);
	write_cmos_sensor(0x3773, 0x04);
	write_cmos_sensor(0x3774, 0x2c);
	write_cmos_sensor(0x3775, 0x13);
	write_cmos_sensor(0x3776, 0x08);
	write_cmos_sensor(0x3778, 0x17);
	write_cmos_sensor(0x37a0, 0x88);
	write_cmos_sensor(0x37a1, 0x7a);
	write_cmos_sensor(0x37a2, 0x7a);
	write_cmos_sensor(0x37a7, 0x88);
	write_cmos_sensor(0x37a8, 0x98);
	write_cmos_sensor(0x37a9, 0x98);
	write_cmos_sensor(0x37aa, 0x88);
	write_cmos_sensor(0x37ab, 0x5c);
	write_cmos_sensor(0x37ac, 0x5c);
	write_cmos_sensor(0x37ad, 0x55);
	write_cmos_sensor(0x37ae, 0x19);
	write_cmos_sensor(0x37af, 0x19);
	write_cmos_sensor(0x37b3, 0x84);
	write_cmos_sensor(0x37b4, 0x84);
	write_cmos_sensor(0x37b5, 0x60);
	write_cmos_sensor(0x3808, 0x02);
	write_cmos_sensor(0x3809, 0x80);
	write_cmos_sensor(0x380a, 0x01);
	write_cmos_sensor(0x380b, 0xe0);
	write_cmos_sensor(0x380c, 0x09);
	write_cmos_sensor(0x380d, 0x02);
	write_cmos_sensor(0x380e, 0x02);
	write_cmos_sensor(0x380f, 0x08);
	write_cmos_sensor(0x3814, 0x03);
	write_cmos_sensor(0x3821, 0x6f);
	write_cmos_sensor(0x382a, 0x05);
	write_cmos_sensor(0x382b, 0x03);
	write_cmos_sensor(0x3830, 0x0c);
	write_cmos_sensor(0x3836, 0x02);
	write_cmos_sensor(0x3f08, 0x10);
	write_cmos_sensor(0x4001, 0x10);
	write_cmos_sensor(0x4022, 0x02);
	write_cmos_sensor(0x4023, 0x20);
	write_cmos_sensor(0x4025, 0xe0);
	write_cmos_sensor(0x4027, 0x5f);
	write_cmos_sensor(0x402a, 0x02);
	write_cmos_sensor(0x402b, 0x04);
	write_cmos_sensor(0x402e, 0x02);
	write_cmos_sensor(0x402f, 0x04);
	write_cmos_sensor(0x4600, 0x00);
	write_cmos_sensor(0x4601, 0x4f);
	write_cmos_sensor(0x4837, 0x16);
	write_cmos_sensor(0x5901, 0x04);
	write_cmos_sensor(0x382d, 0x7f);

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
 *      *sensorID : return the sensor ID
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
			*sensor_id =
		  ((read_cmos_sensor(0x300B) << 8) | read_cmos_sensor(0x300C));

		if (*sensor_id == imgsensor_info.sensor_id) {
			if ((read_cmos_sensor(0x302A)) == 0XB2) {
				ov8858version = OV8858R2A;

	  pr_debug("i2c write id: 0x%x, id: 0x%x, version = %d(0=r2a,1=r1a)\n",
			imgsensor.i2c_write_id, *sensor_id, ov8858version);

					bm = get_boot_mode();
					pr_debug("bm %d\n", bm);
			if (bm == FACTORY_BOOT || bm == ATE_FACTORY_BOOT) {
				imgsensor_info.sensor_output_dataformat =
					    SENSOR_OUTPUT_FORMAT_RAW_B;

				imgsensor_dpcm_info_ov8858[1] = COMP8_DI_2A; }

			return ERROR_NONE;
			} else if ((read_cmos_sensor(0x302A)) == 0XB1) {
				pr_debug("=R1A sensor,contact OV please!=\n");

					return ERROR_SENSOR_CONNECT_FAIL;
		}
				pr_debug("read ov8858 R1A R2A bate fail\n");
				return ERROR_SENSOR_CONNECT_FAIL;

		}

		pr_debug("Read sensor id fail, id: 0x%x,sensor_id=0x%x\n",
		imgsensor.i2c_write_id, *sensor_id);

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

#ifdef OV8858R2AOTP

struct otp_struct {
	int flag;	/* bit[7]: info, bit[6]:wb, bit[5]:vcm, bit[4]:lenc */
	int module_integrator_id;
	int lens_id;
	int production_year;
	int production_month;
	int production_day;
	int rg_ratio;
	int bg_ratio;
	int lenc[240];
	int checksum;
	int VCM_start;
	int VCM_end;
	int VCM_dir;
};

/* return value: */
/* bit[7]: 0 no otp info, 1 valid otp info */
/* bit[6]: 0 no otp wb, 1 valib otp wb */
/* bit[5]: 0 no otp vcm, 1 valid otp vcm */
/* bit[4]: 0 no otp lenc/invalid otp lenc, 1 valid otp lenc */
int read_otp(struct otp_struct *otp_ptr)
{
	int otp_flag, addr, temp, i;
	/* set 0x5002[3] to ?0? */
	int temp1;

	temp1 = read_cmos_sensor(0x5002);
	write_cmos_sensor(0x5002, (0x00 & 0x08) | (temp1 & (~0x08)));
	/* read OTP into buffer */
	write_cmos_sensor(0x3d84, 0xC0);
	write_cmos_sensor(0x3d88, 0x70);	/* OTP start address */
	write_cmos_sensor(0x3d89, 0x10);
	write_cmos_sensor(0x3d8A, 0x72);	/* OTP end address */
	write_cmos_sensor(0x3d8B, 0x0a);
	write_cmos_sensor(0x3d81, 0x01);	/* load otp into buffer */
	mdelay(10);
	/* OTP base information and WB calibration data */
	otp_flag = read_cmos_sensor(0x7010);
	addr = 0;
	if ((otp_flag & 0xc0) == 0x40)
		addr = 0x7011;	/* base address of info group 1 */
	else if ((otp_flag & 0x30) == 0x10)
		addr = 0x7019;	/* base address of info group 2 */

	if (addr != 0) {
		(*otp_ptr).flag = 0xC0;	/* valid info and AWB in OTP */
		(*otp_ptr).module_integrator_id = read_cmos_sensor(addr);
		(*otp_ptr).lens_id = read_cmos_sensor(addr + 1);
		(*otp_ptr).production_year = read_cmos_sensor(addr + 2);
		(*otp_ptr).production_month = read_cmos_sensor(addr + 3);
		(*otp_ptr).production_day = read_cmos_sensor(addr + 4);
		temp = read_cmos_sensor(addr + 7);

		(*otp_ptr).rg_ratio =
		    (read_cmos_sensor(addr + 5) << 2) + ((temp >> 6) & 0x03);

		(*otp_ptr).bg_ratio =
		    (read_cmos_sensor(addr + 6) << 2) + ((temp >> 4) & 0x03);

		pr_debug("rg_ratio: %d, bg_ratio: %d\n",
			(*otp_ptr).rg_ratio, (*otp_ptr).bg_ratio);
	} else {
		(*otp_ptr).flag = 0x00;	/* not info and AWB in OTP */
		(*otp_ptr).module_integrator_id = 0;
		(*otp_ptr).lens_id = 0;
		(*otp_ptr).production_year = 0;
		(*otp_ptr).production_month = 0;
		(*otp_ptr).production_day = 0;
		(*otp_ptr).rg_ratio = 0;
		(*otp_ptr).bg_ratio = 0;
	}
	/* OTP VCM Calibration */
	otp_flag = read_cmos_sensor(0x7021);
	addr = 0;
	if ((otp_flag & 0xc0) == 0x40)
		addr = 0x7022;	/* base address of VCM Calibration group 1 */
	else if ((otp_flag & 0x30) == 0x10)
		addr = 0x7025;	/* base address of VCM Calibration group 2 */

	if (addr != 0) {
		(*otp_ptr).flag |= 0x20;
		temp = read_cmos_sensor(addr + 2);

		(*otp_ptr).VCM_start =
			(read_cmos_sensor(addr) << 2) | ((temp >> 6) & 0x03);
		(*otp_ptr).VCM_end =
		    (read_cmos_sensor(addr + 1) << 2) | ((temp >> 4) & 0x03);

		(*otp_ptr).VCM_dir = (temp >> 2) & 0x03;
	} else {
		(*otp_ptr).VCM_start = 0;
		(*otp_ptr).VCM_end = 0;
		(*otp_ptr).VCM_dir = 0;
	}
	/* OTP Lenc Calibration */
	otp_flag = read_cmos_sensor(0x7028);
	addr = 0;
	int checksum2 = 0;

	if ((otp_flag & 0xc0) == 0x40)
		addr = 0x7029;	/* base address of Lenc Calibration group 1 */
	else if ((otp_flag & 0x30) == 0x10)
		addr = 0x711a;	/* base address of Lenc Calibration group 2 */

	if (addr != 0) {
		for (i = 0; i < 240; i++) {
			(*otp_ptr).lenc[i] = read_cmos_sensor(addr + i);
			checksum2 += (*otp_ptr).lenc[i];
		}
		checksum2 = (checksum2) % 255 + 1;
		(*otp_ptr).checksum = read_cmos_sensor(addr + 240);
		if ((*otp_ptr).checksum == checksum2)
			(*otp_ptr).flag |= 0x10;

	} else {
		for (i = 0; i < 240; i++)
			(*otp_ptr).lenc[i] = 0;
	}
	for (i = 0x7010; i <= 0x720a; i++)
		write_cmos_sensor(i, 0);
	/* clear OTP buffer, recommended use continuous write to accelarate */

	/* set 0x5002[3] to ?1? */
	temp1 = read_cmos_sensor(0x5002);
	write_cmos_sensor(0x5002, (0x08 & 0x08) | (temp1 & (~0x08)));
	return (*otp_ptr).flag;
}

/* return value: */
/* bit[7]: 0 no otp info, 1 valid otp info */
/* bit[6]: 0 no otp wb, 1 valib otp wb */
/* bit[5]: 0 no otp vcm, 1 valid otp vcm */
/* bit[4]: 0 no otp lenc, 1 valid otp lenc */

int apply_otp(struct otp_struct *otp_ptr)
{
	int RG_Ratio_Typical = 0x137, BG_Ratio_Typical = 0x121;
	int rg, bg, R_gain, G_gain, B_gain, Base_gain, temp, i;
	/* apply OTP WB Calibration */
	if ((*otp_ptr).flag & 0x40) {
		rg = (*otp_ptr).rg_ratio;
		bg = (*otp_ptr).bg_ratio;
		/* calculate G gain */
		R_gain = (RG_Ratio_Typical * 1000) / rg;
		B_gain = (BG_Ratio_Typical * 1000) / bg;
		G_gain = 1000;
		if (R_gain < 1000 || B_gain < 1000) {
			if (R_gain < B_gain)
				Base_gain = R_gain;
			else
				Base_gain = B_gain;
		} else {
			Base_gain = G_gain;
		}
		R_gain = 0x400 * R_gain / (Base_gain);
		B_gain = 0x400 * B_gain / (Base_gain);
		G_gain = 0x400 * G_gain / (Base_gain);

		pr_debug("R_gain: 0x%x, B_gain: 0x%x, G_gain: 0x%x\n",
			R_gain, B_gain, G_gain);
		/* update sensor WB gain */
		if (R_gain > 0x400) {
			write_cmos_sensor(0x5032, R_gain >> 8);
			write_cmos_sensor(0x5033, R_gain & 0x00ff);
		}
		if (G_gain > 0x400) {
			write_cmos_sensor(0x5034, G_gain >> 8);
			write_cmos_sensor(0x5035, G_gain & 0x00ff);
		}
		if (B_gain > 0x400) {
			write_cmos_sensor(0x5036, B_gain >> 8);
			write_cmos_sensor(0x5037, B_gain & 0x00ff);
		}
	}
	/* apply OTP Lenc Calibration */
	if ((*otp_ptr).flag & 0x10) {
		temp = read_cmos_sensor(0x5000);
		temp = 0x80 | temp;
		write_cmos_sensor(0x5000, temp);
		for (i = 0; i < 240; i++)
			write_cmos_sensor(0x5800 + i, (*otp_ptr).lenc[i]);

	}
	return (*otp_ptr).flag;
}

#endif
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
	 * n{IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2};
	 */
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id =
		  ((read_cmos_sensor(0x300B) << 8) | read_cmos_sensor(0x300C));

			if (sensor_id == imgsensor_info.sensor_id) {
				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			pr_debug(
			    "Read sensor id fail, id: 0x%x,sensor_id =0x%x\n",
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
#ifdef OV8858R2AOTP
	write_cmos_sensor(0x100, 0x01);
	mdelay(10);

	pr_debug("Apply the sensor OTP\n");
	struct otp_struct *otp_ptr =
				kzalloc(sizeof(struct otp_struct), GFP_KERNEL);
	read_otp(otp_ptr);
	apply_otp(otp_ptr);
	kfree(otp_ptr);
	write_cmos_sensor(0x100, 0x00);
	mdelay(20);
#endif
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
 * This function start the sensor preview.
 *
 * PARAMETERS
 * *image_window : address pointer of pixel numbers in one period of HSYNC
 * *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 * None
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
	preview_setting();
	mdelay(10);
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
 * None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E! current %d fps\n", imgsensor.current_fps);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
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
			  "Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			  imgsensor.current_fps / 10,
			  imgsensor_info.cap.max_framerate);

		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
		imgsensor.current_fps = imgsensor_info.cap.max_framerate;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);
	mdelay(10);

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
	normal_video_setting(30);
	mdelay(10);


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
	mdelay(10);

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
	mdelay(10);

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
	pr_debug("scenario_id = %d\n", scenario_id);

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
		sensor_info->DPCM_INFO = imgsensor_dpcm_info_ov8858[0];
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;
		sensor_info->DPCM_INFO = imgsensor_dpcm_info_ov8858[1];
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

		sensor_info->SensorGrabStartX =
			imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.normal_video.starty;

		sensor_info->DPCM_INFO = imgsensor_dpcm_info_ov8858[2];
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
		sensor_info->DPCM_INFO = imgsensor_dpcm_info_ov8858[3];
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:

		sensor_info->SensorGrabStartX =
			imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.slim_video.starty;

		sensor_info->DPCM_INFO = imgsensor_dpcm_info_ov8858[4];
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

		break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
		sensor_info->DPCM_INFO = imgsensor_dpcm_info_ov8858[0];
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
		set_dummy();
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
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		/* case MSDK_SCENARIO_ID_CAMERA_ZSD: */

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
		set_dummy();
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
		set_dummy();
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
		set_dummy();
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

	if (enable) {
		/* 0x5E00[8]: 1 enable,  0 disable */
	    /* 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK */
		write_cmos_sensor(0x5E00, 0x80);
		write_cmos_sensor(0x5000, 0x7e);
		write_cmos_sensor(0x5002, 0x00);
		write_cmos_sensor(0x3500, 0x00);
		write_cmos_sensor(0x3501, 0x2f);
		write_cmos_sensor(0x3502, 0xf0);
		write_cmos_sensor(0x3508, 0x00);
		write_cmos_sensor(0x3509, 0x80);
	} else {
		/* 0x5E00[8]: 1 enable,  0 disable */
	    /* 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK */
		write_cmos_sensor(0x5E00, 0x00);
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
		set_gain((UINT16) (*feature_data));
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
		imgsensor.ihdr_en = (UINT8)*feature_data_32;
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

		ihdr_write_shutter_gain(
			(UINT16) *feature_data,
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

UINT32 OV8858_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*      OV5693_MIPI_RAW_SensorInit      */
