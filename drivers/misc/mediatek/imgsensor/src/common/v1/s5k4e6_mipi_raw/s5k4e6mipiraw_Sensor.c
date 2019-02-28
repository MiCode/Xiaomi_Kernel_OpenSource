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
/* *****************************************************************************
 *
 * Filename:
 * ---------
 *	 s5k4E6XXmipi_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 * Author:
 * -------
 * Dream Yeh (MTK08783)
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

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "s5k4e6mipiraw_Sensor.h"

/*WDR auto ration mode*/
/* #define ENABLE_WDR_AUTO_RATION */

/****************************Modify following Strings for debug****************************/
#define PFX "s5k4e6_camera_sensor"
#define LOG_1 LOG_INF("s5k4e6,MIPI 2LANE\n")
/****************************   Modify end    *******************************************/
#define LOG_INF(fmt, args...)	pr_debug(PFX "[%s] " fmt, __func__, ##args)
/* static int first_flag = 1; */
static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5K4E6_SENSOR_ID,

	.checksum_value = 0x318134c,	/*Check by Dream */

	.pre = {
		.pclk = 90000000,	/*record different mode's pclk */
		.linelength = 2820,	/*record different mode's linelength */
		.framelength = 1060,	/*record different mode's framelength */
		.startx = 0,	/*record different mode's startx of grabwindow */
		.starty = 0,	/*record different mode's starty of grabwindow */
		.grabwindow_width = 1304,	/*record different mode's width of grabwindow */
		.grabwindow_height = 980,	/*record different mode's height of grabwindow */
		/*       following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
		.mipi_data_lp2hs_settle_dc = 85,
		/*       following for GetDefaultFramerateByScenario()  */
		.max_framerate = 300,
		},
	.cap = {
		.pclk = 180000000,
		.linelength = 2920,
		.framelength = 2040,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2608,
		.grabwindow_height = 1960,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		},
	.cap1 = {
		 .pclk = 180000000,
		 .linelength = 5840,
		 .framelength = 2040,
		 .startx = 0,
		 .starty = 0,
		 .grabwindow_width = 2608,
		 .grabwindow_height = 1960,
		 .mipi_data_lp2hs_settle_dc = 85,
		 .max_framerate = 150,
		 },
	.cap2 = {
		 .pclk = 180000000,
		 .linelength = 3676,
		 .framelength = 2040,
		 .startx = 0,
		 .starty = 0,
		 .grabwindow_width = 2608,
		 .grabwindow_height = 1960,
		 .mipi_data_lp2hs_settle_dc = 85,
		 .max_framerate = 240,
		 },
	.normal_video = {
			 .pclk = 180000000,
			 .linelength = 2920,
			 .framelength = 2040,
			 .startx = 0,
			 .starty = 0,
			 .grabwindow_width = 2608,
			 .grabwindow_height = 1960,
			 .mipi_data_lp2hs_settle_dc = 85,
			 .max_framerate = 300,
			 },
	.hs_video = {
		     .pclk = 180000000,
		     .linelength = 2600,
		     .framelength = 574,
		     .startx = 0,
		     .starty = 0,
		     .grabwindow_width = 652,	/* 640, */
		     .grabwindow_height = 488,	/* 480, */
		     .mipi_data_lp2hs_settle_dc = 85,
		     .max_framerate = 1200,
		     },
	.slim_video = {
		       .pclk = 90000000,
		       .linelength = 2820,
		       .framelength = 1060,
		       .startx = 0,
		       .starty = 0,
		       .grabwindow_width = 1280,
		       .grabwindow_height = 720,
		       .mipi_data_lp2hs_settle_dc = 85,
		       .max_framerate = 300,
		       },
	.margin = 6,
	.min_shutter = 2,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 1,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 7,	/* support sensor mode num */

	.cap_delay_frame = 3,
	.pre_delay_frame = 3,
	.video_delay_frame = 3,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,

	.isp_driving_current = ISP_DRIVING_8MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode = 0,	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,	/* SENSOR_OUTPUT_FORMAT_RAW_Gr, */
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_2_LANE,
	/* .i2c_speed = 100, */
	.i2c_addr_table = {0x6a, 0xff},
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 0,	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,	/* current scenario id */
	.ihdr_en = 0,		/* sensor need support LE, SE with HDR feature */

	.hdr_mode = KAL_FALSE,	/* HDR Mode : 0: disable HDR, 1:IHDR, 2:HDR, 9:ZHDR */

	.i2c_write_id = 0x6A,
};


/* Sensor output window information */
static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
{2608, 1960, 0, 0, 2608, 1960, 1304, 980, 0000, 0000, 1304, 980, 0, 0, 1304, 980},	/* Preview */
{2608, 1960, 0, 0, 2608, 1960, 2608, 1960, 0000, 0000, 2608, 1960, 0, 0, 2608, 1960},	/* capture */
{2608, 1960, 0, 0, 2608, 1960, 2608, 1960, 0000, 0000, 2608, 1960, 0, 0, 2608, 1960},	/* video */
{2608, 1960, 0, 4, 2608, 1952, 652, 488, 0000, 0000, 652, 488, 0, 0, 652, 488},	/* hight speed video */
{2608, 1960, 24, 260, 2560, 1440, 1280, 720, 0000, 0000, 1280, 720, 0, 0, 1280, 720}
};				/* slim video */

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
	    (char)(addr >> 8), (char)(addr & 0xFF), (char)(para >> 8), (char)(para & 0xFF) };
	iWriteRegI2CTiming(pusendcmd, 4, imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
}

static kal_uint16 read_cmos_sensor_8(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *) &get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = { (char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2CTiming(pusendcmd, 3, imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
}

#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 765	/* trans# max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN 3

#endif
static kal_uint16 s5k4e6_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
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
		/* Write when remain buffer size is less than 4 bytes or reach end of data */
		if ((I2C_BUFFER_LEN - tosend) < 4 || IDX == len || addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd, tosend, imgsensor.i2c_write_id,
								4, imgsensor_info.i2c_speed);
			tosend = 0;
		}
#else
		iWriteRegI2CTiming(puSendCmd, 4, imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
		tosend = 0;

#endif
	}
	return 0;
}

static void set_dummy(void)
{
	LOG_INF("frame_length = %d, line_length = %d\n",
		imgsensor.frame_length,	imgsensor.line_length);

	write_cmos_sensor(0x0340, imgsensor.frame_length);
	/* write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF); */
	write_cmos_sensor(0x0342, imgsensor.line_length);
	/* write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF); */

}				/*      set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));

}


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	/* kal_int16 dummy_line; */
	kal_uint32 frame_length = imgsensor.frame_length;
	/* unsigned long flags; */

	LOG_INF("framerate = %d, min framelength should enable = %d\n",
				framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length =
	    (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length;
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


static void write_shutter(kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;
	/* kal_uint32 frame_length = 0; */


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
	shutter =
	    (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
			? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(0x0340, imgsensor.frame_length);
			/* write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF); */
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x0340, imgsensor.frame_length);
		/* write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF); */
	}

	/* Update Shutter */
	/* write_cmos_sensor(0x0104, 0x01);   //group hold */
	write_cmos_sensor(0x0202, shutter);
	/* write_cmos_sensor(0x0203, shutter & 0xFF); */
	write_cmos_sensor(0x021E, shutter);
	/* write_cmos_sensor(0x0104, 0x00);   //group hold */

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
	/* LOG_INF("shutter =%d\n", shutter); */
	write_shutter(shutter);
}				/*      set_shutter */


static void hdr_write_shutter(kal_uint16 le, kal_uint16 se)
{
	unsigned int iRation;
	unsigned long flags;
	/* kal_uint16 realtime_fps = 0; */
	/* kal_uint32 frame_length = 0; */
	/* LOG_INF("enter xxxx  set_shutter, shutter =%d\n", shutter); */
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

	le = (le < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : le;
	le = (le > (imgsensor_info.max_frame_length - imgsensor_info.margin))
			? (imgsensor_info.max_frame_length - imgsensor_info.margin) : le;

	/* Frame length :4000 C340 */
	/* write_cmos_sensor(0x6028,0x4000); */
	/* write_cmos_sensor(0x602A,0xC340 ); */
	write_cmos_sensor(0x0340, imgsensor.frame_length);

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
	write_cmos_sensor(0x0218, iRation);
	write_cmos_sensor(0x021a, 0x100);
#endif
	/*Short exposure */
	write_cmos_sensor(0x0202, se);
	/*Log exposure ratio */
	write_cmos_sensor(0x021e, le);

	LOG_INF("HDR set shutter LE=%d, SE=%d, iRation=0x%x\n", le, se, iRation);

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

	/* 0x350A[0:1], 0x350B[0:7] AGC real gain */
	/* [0:3] = N meams N /16 X  */
	/* [4:9] = M meams M X       */
	/* Total gain = M + N /16 X   */

	/*  */
	if (gain < BASEGAIN || gain > 32 * BASEGAIN) {
		LOG_INF("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 32 * BASEGAIN)
			gain = 32 * BASEGAIN;
	}

	reg_gain = gain >> 1;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n", gain, reg_gain);

	/* write_cmos_sensor_8(0x0104, 0x01); */
	write_cmos_sensor(0x0204, reg_gain);
	write_cmos_sensor(0x0220, reg_gain);

	/* write_cmos_sensor_8(0x0104, 0x00); */

	return gain;
}				/*      set_gain  */


/* defined but not used */
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

		write_cmos_sensor(0x0340, imgsensor.frame_length);	/* or 0x380e? */


		write_cmos_sensor(0x602A, 0x021e);
		write_cmos_sensor(0x6f12, le);
		write_cmos_sensor(0x602A, 0x0202);
		write_cmos_sensor(0x6f12, se);


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

	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor_8(0x0101, 0x00);	/* Gr */
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor_8(0x0101, 0x01);	/* R */
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor_8(0x0101, 0x02);	/* B */
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor_8(0x0101, 0x03);	/* Gb */
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

static void sensor_WDR_zhdr(void)
{
	if (imgsensor.hdr_mode == 9) {
		LOG_INF("sensor_WDR_zhdr\n");
		/*it would write 0x216 = 0x1, 0x217=0x00 */
		/*0x216=1 , Enable WDR */
		/*0x217=0x00, Use Manual mode to set short /long exp */
#if defined(ENABLE_WDR_AUTO_RATION)
		write_cmos_sensor(0x0216, 0x0101);	/*For WDR auto ration using 0x0218/0x021A */
		LOG_INF("WDR auto ration)\n");
#else
		write_cmos_sensor(0x0216, 0x0100);	/*For WDR manual ration - 0x0202 for SE, 0x021e for LE */
		LOG_INF("WDR manual ration\n");
#endif

		/* write_cmos_sensor(0x0218, 0x0200); */
		/*
		*x0218/0x021A is the ratio of LE.
		*For example,0x0218=0x0800,0x021A=0x0200,the the LE is 4 times of SE.
		*/

		/* write_cmos_sensor(0x021A, 0x0100); */

		/* write_cmos_sensor(0x6028, 0x2000); */
		/* write_cmos_sensor(0x602A, 0x6944); */
		write_cmos_sensor(0x6F12, 0x0000);
	} else {
		LOG_INF("hdr_mode is not zhdr!\n");
		write_cmos_sensor(0x0216, 0x0000);
		write_cmos_sensor(0x0218, 0x0000);

		/* write_cmos_sensor(0x6028, 0x2000); */
		/* write_cmos_sensor(0x602A, 0x6944); */
		write_cmos_sensor(0x6F12, 0x0000);	/* Normal case also should turn off the Recon Block. */

	}
	/*for LE/SE Test */
	/* hdr_write_shutter(3460,800); */

}


static void sensor_init(void)
{
	LOG_INF("E\n");
	/* +++++++++++++++++++++++++++// */
	/* +++++++++++++++++++++++++++// */
	/* Every mode change need sensor SW reset, so no need nitial.  Caval,2016/11/10 */

}				/*      sensor_init  */

kal_uint16 addr_data_pair_preview_s5k4e6[] = {
	0X535A, 0XC700,
	0X5400, 0X061D,
	0X5402, 0X1500,
	0X6102, 0XC000,
	0X614C, 0X25AA,
	0X614E, 0X25B8,
	0X618C, 0X08D4,
	0X618E, 0X08D6,
	0X6028, 0X2000,
	0X602A, 0X11A8,
	0X6F12, 0X3AF9,
	0X6F12, 0X1410,
	0X6F12, 0X39F9,
	0X6F12, 0X1410,
	0X6028, 0X2000,
	0X602A, 0X0668,
	0X6F12, 0X8011,
	0X602A, 0X12BC,
	0X6F12, 0X1020,
	0X602A, 0X12C2,
	0X6F12, 0X1020,
	0X6F12, 0X1020,
	0X602A, 0X12CA,
	0X6F12, 0X1020,
	0X6F12, 0X1010,
	0X602A, 0X12FC,
	0X6F12, 0X1020,
	0X602A, 0X1302,
	0X6F12, 0X1020,
	0X6F12, 0X1020,
	0X602A, 0X130A,
	0X6F12, 0X1020,
	0X6F12, 0X1010,
	0X602A, 0X14B8,
	0X6F12, 0X0101,
	0X602A, 0X14C0,
	0X6F12, 0X0000,
	0X6F12, 0XFFDA,
	0X6F12, 0XFFDA,
	0X6F12, 0X0000,
	0X6F12, 0X0000,
	0X6F12, 0XFFDA,
	0X6F12, 0XFFDA,
	0X6F12, 0X0000,
	0X602A, 0X1488,
	0X6F12, 0XFF80,
	0X6F12, 0XFF80,
	0X6F12, 0XFF80,
	0X6F12, 0XFF80,
	0X602A, 0X1496,
	0X6F12, 0XFF80,
	0X6F12, 0XFF80,
	0X6F12, 0XFF80,
	0X6F12, 0XFF80,
	0X602A, 0X14A4,
	0X6F12, 0XFFC0,
	0X6F12, 0XFFC0,
	0X6F12, 0XFFC0,
	0X6F12, 0XFFC0,
	0X602A, 0X147A,
	0X6F12, 0X0000,
	0X6F12, 0X0002,
	0X6F12, 0XFFFC,
	0X602A, 0X0512,
	0X6F12, 0X0111,
	0X602A, 0X066A,
	0X6F12, 0X4110,
	0X602A, 0X14AC,
	0X6F12, 0X0000,
	0X602A, 0X0524,
	0X6F12, 0X0007,
	0X3284, 0X3800,
	0X327A, 0X0001,
	0X3282, 0X000A,
	0X3296, 0X0418,
	0X32E0, 0X0000,
	0X3286, 0X9000,
	0X3298, 0X4007,
	0X32AA, 0X0100,
	0X327C, 0X0400,
	0X328A, 0X0800,
	0X3284, 0X3700,
	0X32A0, 0X0320,
	0X32A2, 0X1000,
	0X32A4, 0X0C00,
	0X3204, 0X000C,
	0X3206, 0X000B,
	0X3208, 0X0009,
	0X3210, 0X0007,
	0X3212, 0X0007,
	0X0200, 0X0408,
	0X3218, 0X031C,
	0X321A, 0X3224,
	0X321C, 0X0700,
	0X321E, 0X0800,
	0X3220, 0X1300,
	0X3226, 0X525C,
	0X3228, 0X0304,
	0X5330, 0XD403,
	0X5428, 0X1800,
	0X3300, 0X0001,
	0X0304, 0X0006,
	0X0306, 0X00B4,
	0X5362, 0X0A00,
	0X5364, 0X4299,
	0X534E, 0X4910,
	0X0340, 0X0424,
	0X0342, 0X0B04,
	0X021E, 0X03FC,
	0X0344, 0X0000,
	0X0346, 0X0000,
	0X0348, 0X0A2F,
	0X034A, 0X07A7,
	0X034C, 0X0518,
	0X034E, 0X03D4,
	0X3500, 0X0122,
	0X3088, 0X0001,
	0X0216, 0X0000,
	0X5332, 0X04E0,
	0X5080, 0X0100,
};

static void preview_setting(void)
{
	/* +++++++++++++++++++++++++++// */
	/* Streaming off */
	write_cmos_sensor(0XFCFC, 0X4000);
	write_cmos_sensor(0X6010, 0X0001);
	mDELAY(3);
	s5k4e6_table_write_cmos_sensor(addr_data_pair_preview_s5k4e6,
				       sizeof(addr_data_pair_preview_s5k4e6) / sizeof(kal_uint16));


	/*Set WDR */
	sensor_WDR_zhdr();


}				/*      preview_setting  */

kal_uint16 addr_data_pair_capture_15fps_s5k4e6[] = {
	0x535A, 0xC700,
	0x5400, 0x061D,
	0x5402, 0x1500,
	0x6102, 0xC000,
	0x614C, 0x25AA,
	0x614E, 0x25B8,
	0x618C, 0x08D4,
	0x618E, 0x08D6,
	0x6028, 0x2000,
	0x602A, 0x11A8,
	0x6F12, 0x3AF9,
	0x6F12, 0x1410,
	0x6F12, 0x39F9,
	0x6F12, 0x1410,
	0x6028, 0x2000,
	0x602A, 0x0668,
	0x6F12, 0x4010,
	0x602A, 0x12BC,
	0x6F12, 0x1020,
	0x602A, 0x12C2,
	0x6F12, 0x1020,
	0x6F12, 0x1020,
	0x602A, 0x12CA,
	0x6F12, 0x1020,
	0x6F12, 0x1010,
	0x602A, 0x12FC,
	0x6F12, 0x1020,
	0x602A, 0x1302,
	0x6F12, 0x1020,
	0x6F12, 0x1020,
	0x602A, 0x130A,
	0x6F12, 0x1020,
	0x6F12, 0x1010,
	0x602A, 0x14B8,
	0x6F12, 0x0101,
	0x602A, 0x14C0,
	0x6F12, 0x0000,
	0x6F12, 0xFFDA,
	0x6F12, 0xFFDA,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0xFFDA,
	0x6F12, 0xFFDA,
	0x6F12, 0x0000,
	0x602A, 0x1488,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x602A, 0x1496,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x602A, 0x14A4,
	0x6F12, 0xFFC0,
	0x6F12, 0xFFC0,
	0x6F12, 0xFFC0,
	0x6F12, 0xFFC0,
	0x602A, 0x147A,
	0x6F12, 0x0000,
	0x6F12, 0x0002,
	0x6F12, 0xFFFC,
	0x602A, 0x0512,
	0x6F12, 0x0111,
	0x602A, 0x066A,
	0x6F12, 0x0111,
	0x602A, 0x14AC,
	0x6F12, 0x0000,
	0x602A, 0x0524,
	0x6F12, 0x0007,
	0x3284, 0x3800,
	0x327A, 0x0001,
	0x3282, 0x000A,
	0x3296, 0x0418,
	0x32E0, 0x0000,
	0x3286, 0x9000,
	0x3298, 0x4007,
	0x32AA, 0x0000,
	0x327C, 0x0400,
	0x328A, 0x0800,
	0x3284, 0x3700,
	0x32A0, 0x0320,
	0x32A2, 0x1000,
	0x32A4, 0x0C00,
	0x3204, 0x000C,
	0x3206, 0x000B,
	0x3208, 0x0009,
	0x3210, 0x0007,
	0x3212, 0x0007,
	0x0200, 0x0408,
	0x3218, 0x031C,
	0x321A, 0x3224,
	0x321C, 0x0700,
	0x321E, 0x0800,
	0x3220, 0x1300,
	0x3226, 0x525C,
	0x3228, 0x0304,
	0x5330, 0x5403,
	0x5428, 0x1800,
	0x3300, 0x0000,
	0x0304, 0x0006,
	0x0306, 0x00B4,
	0x5362, 0x0A00,
	0x5364, 0x4298,
	0x534E, 0x4910,
	0x0340, 0x07F8,
	0x0342, 0x16D0,
	0x0202, 0x03FC,
	0x0344, 0x0000,
	0x0346, 0x0000,
	0x0348, 0x0A2F,
	0x034A, 0x07A7,
	0x034C, 0x0A30,
	0x034E, 0x07A8,
	0x3500, 0x0022,
	0x3088, 0x0000,
	0x0216, 0x0000,
	0x5332, 0x04E0,
	0x5080, 0x0100,
};

kal_uint16 addr_data_pair_capture_24fps_s5k4e6[] = {
	0x535A, 0xC700,
	0x5400, 0x061D,
	0x5402, 0x1500,
	0x6102, 0xC000,
	0x614C, 0x25AA,
	0x614E, 0x25B8,
	0x618C, 0x08D4,
	0x618E, 0x08D6,
	0x6028, 0x2000,
	0x602A, 0x11A8,
	0x6F12, 0x3AF9,
	0x6F12, 0x1410,
	0x6F12, 0x39F9,
	0x6F12, 0x1410,
	0x6028, 0x2000,
	0x602A, 0x0668,
	0x6F12, 0x4010,
	0x602A, 0x12BC,
	0x6F12, 0x1020,
	0x602A, 0x12C2,
	0x6F12, 0x1020,
	0x6F12, 0x1020,
	0x602A, 0x12CA,
	0x6F12, 0x1020,
	0x6F12, 0x1010,
	0x602A, 0x12FC,
	0x6F12, 0x1020,
	0x602A, 0x1302,
	0x6F12, 0x1020,
	0x6F12, 0x1020,
	0x602A, 0x130A,
	0x6F12, 0x1020,
	0x6F12, 0x1010,
	0x602A, 0x14B8,
	0x6F12, 0x0101,
	0x602A, 0x14C0,
	0x6F12, 0x0000,
	0x6F12, 0xFFDA,
	0x6F12, 0xFFDA,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0xFFDA,
	0x6F12, 0xFFDA,
	0x6F12, 0x0000,
	0x602A, 0x1488,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x602A, 0x1496,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x602A, 0x14A4,
	0x6F12, 0xFFC0,
	0x6F12, 0xFFC0,
	0x6F12, 0xFFC0,
	0x6F12, 0xFFC0,
	0x602A, 0x147A,
	0x6F12, 0x0000,
	0x6F12, 0x0002,
	0x6F12, 0xFFFC,
	0x602A, 0x0512,
	0x6F12, 0x0111,
	0x602A, 0x066A,
	0x6F12, 0x0111,
	0x602A, 0x14AC,
	0x6F12, 0x0000,
	0x602A, 0x0524,
	0x6F12, 0x0007,
	0x3284, 0x3800,
	0x327A, 0x0001,
	0x3282, 0x000A,
	0x3296, 0x0418,
	0x32E0, 0x0000,
	0x3286, 0x9000,
	0x3298, 0x4007,
	0x32AA, 0x0000,
	0x327C, 0x0400,
	0x328A, 0x0800,
	0x3284, 0x3700,
	0x32A0, 0x0320,
	0x32A2, 0x1000,
	0x32A4, 0x0C00,
	0x3204, 0x000C,
	0x3206, 0x000B,
	0x3208, 0x0009,
	0x3210, 0x0007,
	0x3212, 0x0007,
	0x0200, 0x0408,
	0x3218, 0x031C,
	0x321A, 0x3224,
	0x321C, 0x0700,
	0x321E, 0x0800,
	0x3220, 0x1300,
	0x3226, 0x525C,
	0x3228, 0x0304,
	0x5330, 0xD403,
	0x5428, 0x1800,
	0x3300, 0x0000,
	0x0304, 0x0006,
	0x0306, 0x00B4,
	0x5362, 0x0A00,
	0x5364, 0x4298,
	0x534E, 0x4910,
	0x0340, 0x07F8,
	0x0342, 0x0E5C,
	0x0202, 0x03FC,
	0x0344, 0x0000,
	0x0346, 0x0000,
	0x0348, 0x0A2F,
	0x034A, 0x07A7,
	0x034C, 0x0A30,
	0x034E, 0x07A8,
	0x3500, 0x0022,
	0x3088, 0x0000,
	0x0216, 0x0000,
	0x5332, 0x04E0,
	0x5080, 0x0100,
};

kal_uint16 addr_data_pair_capture_30fps_s5k4e6[] = {
	0x535A, 0xC700,
	0x5400, 0x061D,
	0x5402, 0x1500,
	0x6102, 0xC000,
	0x614C, 0x25AA,
	0x614E, 0x25B8,
	0x618C, 0x08D4,
	0x618E, 0x08D6,
	0x6028, 0x2000,
	0x602A, 0x11A8,
	0x6F12, 0x3AF9,
	0x6F12, 0x1410,
	0x6F12, 0x39F9,
	0x6F12, 0x1410,
	0x6028, 0x2000,
	0x602A, 0x0668,
	0x6F12, 0x4010,
	0x602A, 0x12BC,
	0x6F12, 0x1020,
	0x602A, 0x12C2,
	0x6F12, 0x1020,
	0x6F12, 0x1020,
	0x602A, 0x12CA,
	0x6F12, 0x1020,
	0x6F12, 0x1010,
	0x602A, 0x12FC,
	0x6F12, 0x1020,
	0x602A, 0x1302,
	0x6F12, 0x1020,
	0x6F12, 0x1020,
	0x602A, 0x130A,
	0x6F12, 0x1020,
	0x6F12, 0x1010,
	0x602A, 0x14B8,
	0x6F12, 0x0101,
	0x602A, 0x14C0,
	0x6F12, 0x0000,
	0x6F12, 0xFFDA,
	0x6F12, 0xFFDA,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0xFFDA,
	0x6F12, 0xFFDA,
	0x6F12, 0x0000,
	0x602A, 0x1488,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x602A, 0x1496,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x602A, 0x14A4,
	0x6F12, 0xFFC0,
	0x6F12, 0xFFC0,
	0x6F12, 0xFFC0,
	0x6F12, 0xFFC0,
	0x602A, 0x147A,
	0x6F12, 0x0000,
	0x6F12, 0x0002,
	0x6F12, 0xFFFC,
	0x602A, 0x0512,
	0x6F12, 0x0111,
	0x602A, 0x066A,
	0x6F12, 0x0111,
	0x602A, 0x14AC,
	0x6F12, 0x0000,
	0x602A, 0x0524,
	0x6F12, 0x0007,
	0x3284, 0x3800,
	0x327A, 0x0001,
	0x3282, 0x000A,
	0x3296, 0x0418,
	0x32E0, 0x0000,
	0x3286, 0x9000,
	0x3298, 0x4007,
	0x32AA, 0x0000,
	0x327C, 0x0400,
	0x328A, 0x0800,
	0x3284, 0x3700,
	0x32A0, 0x0320,
	0x32A2, 0x1000,
	0x32A4, 0x0C00,
	0x3204, 0x000C,
	0x3206, 0x000B,
	0x3208, 0x0009,
	0x3210, 0x0007,
	0x3212, 0x0007,
	0x0200, 0x0408,
	0x3218, 0x031C,
	0x321A, 0x3224,
	0x321C, 0x0700,
	0x321E, 0x0800,
	0x3220, 0x1300,
	0x3226, 0x525C,
	0x3228, 0x0304,
	0x5330, 0xD403,
	0x5428, 0x1800,
	0x3300, 0x0000,
	0x0304, 0x0006,
	0x0306, 0x00B4,
	0x5362, 0x0A00,
	0x5364, 0x4298,
	0x534E, 0x4910,
	0x0340, 0x07F8,
	0x0342, 0x0B68,
	0x0202, 0x03FC,
	0x0344, 0x0000,
	0x0346, 0x0000,
	0x0348, 0x0A2F,
	0x034A, 0x07A7,
	0x034C, 0x0A30,
	0x034E, 0x07A8,
	0x3500, 0x0022,
	0x3088, 0x0000,
	0x0216, 0x0000,
	0x5332, 0x04E0,
	0x5080, 0x0100,
};

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n", currefps);

	/* Reset for operation */
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6010, 0x0001);
	mDELAY(3);
	if (currefps == 150) {
		s5k4e6_table_write_cmos_sensor(addr_data_pair_capture_15fps_s5k4e6,
		       sizeof(addr_data_pair_capture_15fps_s5k4e6) / sizeof(kal_uint16));
	} else if (currefps == 240) {	/* 24fps for PIP */
		s5k4e6_table_write_cmos_sensor(addr_data_pair_capture_24fps_s5k4e6,
		       sizeof(addr_data_pair_capture_24fps_s5k4e6) / sizeof(kal_uint16));
	} else {
		s5k4e6_table_write_cmos_sensor(addr_data_pair_capture_30fps_s5k4e6,
		       sizeof(addr_data_pair_capture_30fps_s5k4e6) / sizeof(kal_uint16));

	}
	/*Set WDR */
	sensor_WDR_zhdr();

}

kal_uint16 addr_data_pair_normal_video_s5k4e6[] = {
	0x535A, 0xC700,
	0x5400, 0x061D,
	0x5402, 0x1500,
	0x6102, 0xC000,
	0x614C, 0x25AA,
	0x614E, 0x25B8,
	0x618C, 0x08D4,
	0x618E, 0x08D6,
	0x6028, 0x2000,
	0x602A, 0x11A8,
	0x6F12, 0x3AF9,
	0x6F12, 0x1410,
	0x6F12, 0x39F9,
	0x6F12, 0x1410,
	0x6028, 0x2000,
	0x602A, 0x0668,
	0x6F12, 0x4010,
	0x602A, 0x12BC,
	0x6F12, 0x1020,
	0x602A, 0x12C2,
	0x6F12, 0x1020,
	0x6F12, 0x1020,
	0x602A, 0x12CA,
	0x6F12, 0x1020,
	0x6F12, 0x1010,
	0x602A, 0x12FC,
	0x6F12, 0x1020,
	0x602A, 0x1302,
	0x6F12, 0x1020,
	0x6F12, 0x1020,
	0x602A, 0x130A,
	0x6F12, 0x1020,
	0x6F12, 0x1010,
	0x602A, 0x14B8,
	0x6F12, 0x0101,
	0x602A, 0x14C0,
	0x6F12, 0x0000,
	0x6F12, 0xFFDA,
	0x6F12, 0xFFDA,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0xFFDA,
	0x6F12, 0xFFDA,
	0x6F12, 0x0000,
	0x602A, 0x1488,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x602A, 0x1496,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x602A, 0x14A4,
	0x6F12, 0xFFC0,
	0x6F12, 0xFFC0,
	0x6F12, 0xFFC0,
	0x6F12, 0xFFC0,
	0x602A, 0x147A,
	0x6F12, 0x0000,
	0x6F12, 0x0002,
	0x6F12, 0xFFFC,
	0x602A, 0x0512,
	0x6F12, 0x0111,
	0x602A, 0x066A,
	0x6F12, 0x0111,
	0x602A, 0x14AC,
	0x6F12, 0x0000,
	0x602A, 0x0524,
	0x6F12, 0x0007,
	0x3284, 0x3800,
	0x327A, 0x0001,
	0x3282, 0x000A,
	0x3296, 0x0418,
	0x32E0, 0x0000,
	0x3286, 0x9000,
	0x3298, 0x4007,
	0x32AA, 0x0000,
	0x327C, 0x0400,
	0x328A, 0x0800,
	0x3284, 0x3700,
	0x32A0, 0x0320,
	0x32A2, 0x1000,
	0x32A4, 0x0C00,
	0x3204, 0x000C,
	0x3206, 0x000B,
	0x3208, 0x0009,
	0x3210, 0x0007,
	0x3212, 0x0007,
	0x0200, 0x0408,
	0x3218, 0x031C,
	0x321A, 0x3224,
	0x321C, 0x0700,
	0x321E, 0x0800,
	0x3220, 0x1300,
	0x3226, 0x525C,
	0x3228, 0x0304,
	0x5330, 0xD403,
	0x5428, 0x1800,
	0x3300, 0x0000,
	0x0304, 0x0006,
	0x0306, 0x00B4,
	0x5362, 0x0A00,
	0x5364, 0x4298,
	0x534E, 0x4910,
	0x0340, 0x07F8,
	0x0342, 0x0B68,
	0x0202, 0x03FC,
	0x0344, 0x0000,
	0x0346, 0x0000,
	0x0348, 0x0A2F,
	0x034A, 0x07A7,
	0x034C, 0x0A30,
	0x034E, 0x07A8,
	0x3500, 0x0022,
	0x3088, 0x0000,
	0x0216, 0x0000,
	0x5332, 0x04E0,
	0x5080, 0x0100,

};

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n", currefps);
	/* Reset for operation */
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6010, 0x0001);
	mDELAY(3);
	s5k4e6_table_write_cmos_sensor(addr_data_pair_normal_video_s5k4e6,
		       sizeof(addr_data_pair_normal_video_s5k4e6) / sizeof(kal_uint16));
	/*Set WDR */
	sensor_WDR_zhdr();


}

kal_uint16 addr_data_pair_hs_video_s5k4e6[] = {
	0x535A, 0xC700,
	0x5400, 0x061D,
	0x5402, 0x1500,
	0x6102, 0xC000,
	0x614C, 0x25AA,
	0x614E, 0x25B8,
	0x618C, 0x08D4,
	0x618E, 0x08D6,
	0x6028, 0x2000,
	0x602A, 0x11A8,
	0x6F12, 0x3AF9,
	0x6F12, 0x1410,
	0x6F12, 0x39F9,
	0x6F12, 0x1410,
	0x6028, 0x2000,
	0x602A, 0x0668,
	0x6F12, 0x8011,
	0x602A, 0x12BC,
	0x6F12, 0x1020,
	0x602A, 0x12C2,
	0x6F12, 0x1020,
	0x6F12, 0x1020,
	0x602A, 0x12CA,
	0x6F12, 0x1020,
	0x6F12, 0x1010,
	0x602A, 0x12FC,
	0x6F12, 0x1020,
	0x602A, 0x1302,
	0x6F12, 0x1020,
	0x6F12, 0x1020,
	0x602A, 0x130A,
	0x6F12, 0x1020,
	0x6F12, 0x1010,
	0x602A, 0x14B8,
	0x6F12, 0x0101,
	0x602A, 0x14C0,
	0x6F12, 0x0000,
	0x6F12, 0xFFDA,
	0x6F12, 0xFFDA,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0xFFDA,
	0x6F12, 0xFFDA,
	0x6F12, 0x0000,
	0x602A, 0x1488,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x602A, 0x1496,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x6F12, 0xFF80,
	0x602A, 0x14A4,
	0x6F12, 0xFFC0,
	0x6F12, 0xFFC0,
	0x6F12, 0xFFC0,
	0x6F12, 0xFFC0,
	0x602A, 0x147A,
	0x6F12, 0x0000,
	0x6F12, 0x0002,
	0x6F12, 0xFFFC,
	0x602A, 0x0512,
	0x6F12, 0x0111,
	0x602A, 0x066A,
	0x6F12, 0x4110,
	0x602A, 0x14AC,
	0x6F12, 0x0000,
	0x602A, 0x0524,
	0x6F12, 0x0007,
	0x3284, 0x3800,
	0x327A, 0x0001,
	0x3282, 0x000A,
	0x3296, 0x0418,
	0x32E0, 0x0000,
	0x3286, 0x9000,
	0x3298, 0x4007,
	0x32AA, 0x0100,
	0x327C, 0x0400,
	0x328A, 0x0800,
	0x3284, 0x3700,
	0x32A0, 0x0320,
	0x32A2, 0x1000,
	0x32A4, 0x0C00,
	0x3204, 0x000C,
	0x3206, 0x000B,
	0x3208, 0x0009,
	0x3210, 0x0007,
	0x3212, 0x0007,
	0x0200, 0x0408,
	0x3218, 0x031C,
	0x321A, 0x3224,
	0x321C, 0x0700,
	0x321E, 0x0800,
	0x3220, 0x1300,
	0x3226, 0x525C,
	0x3228, 0x0304,
	0x5330, 0xD403,
	0x5428, 0x1800,
	0x3300, 0x0000,
	0x0304, 0x0006,
	0x0306, 0x00B4,
	0x5362, 0x0A00,
	0x5364, 0x4298,
	0x534E, 0x4910,
	0x0340, 0x023E,
	0x0342, 0x0A28,
	0x021E, 0x03FC,
	0x0344, 0x0000,
	0x0346, 0x0000,
	0x0348, 0x0A2F,
	0x034A, 0x079F,
	0x034C, 0x028C,
	0x034E, 0x01E8,
	0x3500, 0x0122,
	0x3088, 0x0001,
	0x0216, 0x0000,
	0x5332, 0x04E0,
	0x5080, 0x0100,
	0x0382, 0x0003,
	0x0386, 0x0003,
};

static void hs_video_setting(void)
{
	LOG_INF("E! VGA 120fps\n");
	/* VGA 120fps */
	/*
	*MCLK:24,Width:640,Height:480,Format:MIPI_RAW10,mipi_lane:2,
	*mipi_datarate:836,pvi_pclk_inverwrite_cmos_sensor(0xe:0]
	*/
	/* Reset for operation */
	/* Streaming off */
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6010, 0x0001);
	mDELAY(3);

	s5k4e6_table_write_cmos_sensor(addr_data_pair_hs_video_s5k4e6,
				       sizeof(addr_data_pair_hs_video_s5k4e6) / sizeof(kal_uint16));


}


kal_uint16 addr_data_pair_slim_video_s5k4e6[] = {
	0X535A, 0XC700,
	0X5400, 0X061D,
	0X5402, 0X1500,
	0X6102, 0XC000,
	0X614C, 0X25AA,
	0X614E, 0X25B8,
	0X618C, 0X08D4,
	0X618E, 0X08D6,
	0X6028, 0X2000,
	0X602A, 0X11A8,
	0X6F12, 0X3AF9,
	0X6F12, 0X1410,
	0X6F12, 0X39F9,
	0X6F12, 0X1410,
	0X6028, 0X2000,
	0X602A, 0X0668,
	0X6F12, 0X8011,
	0X602A, 0X12BC,
	0X6F12, 0X1020,
	0X602A, 0X12C2,
	0X6F12, 0X1020,
	0X6F12, 0X1020,
	0X602A, 0X12CA,
	0X6F12, 0X1020,
	0X6F12, 0X1010,
	0X602A, 0X12FC,
	0X6F12, 0X1020,
	0X602A, 0X1302,
	0X6F12, 0X1020,
	0X6F12, 0X1020,
	0X602A, 0X130A,
	0X6F12, 0X1020,
	0X6F12, 0X1010,
	0X602A, 0X14B8,
	0X6F12, 0X0101,
	0X602A, 0X14C0,
	0X6F12, 0X0000,
	0X6F12, 0XFFDA,
	0X6F12, 0XFFDA,
	0X6F12, 0X0000,
	0X6F12, 0X0000,
	0X6F12, 0XFFDA,
	0X6F12, 0XFFDA,
	0X6F12, 0X0000,
	0X602A, 0X1488,
	0X6F12, 0XFF80,
	0X6F12, 0XFF80,
	0X6F12, 0XFF80,
	0X6F12, 0XFF80,
	0X602A, 0X1496,
	0X6F12, 0XFF80,
	0X6F12, 0XFF80,
	0X6F12, 0XFF80,
	0X6F12, 0XFF80,
	0X602A, 0X14A4,
	0X6F12, 0XFFC0,
	0X6F12, 0XFFC0,
	0X6F12, 0XFFC0,
	0X6F12, 0XFFC0,
	0X602A, 0X147A,
	0X6F12, 0X0000,
	0X6F12, 0X0002,
	0X6F12, 0XFFFC,
	0X602A, 0X0512,
	0X6F12, 0X0111,
	0X602A, 0X066A,
	0X6F12, 0X4110,
	0X602A, 0X14AC,
	0X6F12, 0X0000,
	0X602A, 0X0524,
	0X6F12, 0X0007,
	0X3284, 0X3800,
	0X327A, 0X0001,
	0X3282, 0X000A,
	0X3296, 0X0418,
	0X32E0, 0X0000,
	0X3286, 0X9000,
	0X3298, 0X4007,
	0X32AA, 0X0100,
	0X327C, 0X0400,
	0X328A, 0X0800,
	0X3284, 0X3700,
	0X32A0, 0X0320,
	0X32A2, 0X1000,
	0X32A4, 0X0C00,
	0X3204, 0X000C,
	0X3206, 0X000B,
	0X3208, 0X0009,
	0X3210, 0X0007,
	0X3212, 0X0007,
	0X0200, 0X0408,
	0X3218, 0X031C,
	0X321A, 0X3224,
	0X321C, 0X0700,
	0X321E, 0X0800,
	0X3220, 0X1300,
	0X3226, 0X525C,
	0X3228, 0X0304,
	0X5330, 0XD403,
	0X5428, 0X1800,
	0X3300, 0X0001,
	0X0304, 0X0006,
	0X0306, 0X00B4,
	0X5362, 0X0A00,
	0X5364, 0X4299,
	0X534E, 0X4910,
	0X0340, 0X0424,
	0X0342, 0X0B04,
	0X021E, 0X03FC,
	0X0344, 0X000C,
	0X0346, 0X0154,
	0X0348, 0X0A23,
	0X034A, 0X0794,
	0X034C, 0X0500,
	0X034E, 0X02D0,
	0X3500, 0X0122,
	0X3088, 0X0001,
	0X0216, 0X0000,
	0X5332, 0X04E0,
	0X5080, 0X0100,
};

static void slim_video_setting(void)
{
	LOG_INF("E! HD 30fps\n");
	/* +++++++++++++++++++++++++++// */
	/* Reset for operation */
	/*
	*$MV1[MCLK:24,Width:1280,Height:720,Format:MIPI_RAW10,mipi_lane:2,
	*mipi_datarate:836,pvi_pclk_inverwrite_cmos_sensor(0xe:0]);
	*/
	/* Streaming off */
	write_cmos_sensor(0XFCFC, 0X4000);
	write_cmos_sensor(0X6010, 0X0001);
	mDELAY(3);
	s5k4e6_table_write_cmos_sensor(addr_data_pair_slim_video_s5k4e6,
			       sizeof(addr_data_pair_slim_video_s5k4e6) / sizeof(kal_uint16));
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable)
		write_cmos_sensor(0x0100, 0x0100);
	else
		write_cmos_sensor(0x0100, 0x0000);
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
	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address */
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
			LOG_INF("Read sensor id fail, i2c_write_id: 0x%x\n",
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
	/* const kal_uint8 i2c_addr[] = {IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2}; */
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

	LOG_1;

	/* sensor have two i2c address 0x20,0x5a  we should detect the module used i2c address */
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
			LOG_INF("Read sensor id fail, i2c write id: 0x%x\n",
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
	imgsensor.shutter = 0x3D0;	/*  */
	imgsensor.gain = 0x100;	/*  */
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
	/* imgsensor.video_mode = KAL_FALSE; */
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
	/* imgsensor.current_fps = 240; */
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {	/* PIP capture:15fps */
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {		/* PIP capture: 24fps */
		imgsensor.pclk = imgsensor_info.cap2.pclk;
		imgsensor.line_length = imgsensor_info.cap2.linelength;
		imgsensor.frame_length = imgsensor_info.cap2.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap2.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);


	return ERROR_NONE;
}				/* capture() */

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
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
}

/*	normal_video   */

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
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}				/*      hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

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
}				/*      slim_video       */



static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{

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
	return ERROR_NONE;
}				/*      get_resolution  */

static kal_uint32 get_info(MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	/* LOG_INF("scenario_id = %d\n", scenario_id); */



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

	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;


	/*0: no support, 1: G0,R0.B0, 2: G0,R0.B1, 3: G0,R1.B0, 4: G0,R1.B1 */
	/*                    5: G1,R0.B0, 6: G1,R0.B1, 7: G1,R1.B0, 8: G1,R1.B1 */
	sensor_info->ZHDR_Mode = 5;


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
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
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
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}				/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	/* LOG_INF("framerate = %d\n ", framerate); */
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

	/* LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate); */

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length =
		    imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
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
		frame_length =
		    imgsensor_info.normal_video.pclk / framerate * 10 /
		    imgsensor_info.normal_video.linelength;
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
		if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
			frame_length =
			    imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength)
				? (frame_length - imgsensor_info.cap1.framelength) : 0;

			imgsensor.frame_length =
			    imgsensor_info.cap1.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else if (imgsensor.current_fps == imgsensor_info.cap2.max_framerate) {
			frame_length =
			    imgsensor_info.cap2.pclk / framerate * 10 / imgsensor_info.cap2.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			    (frame_length > imgsensor_info.cap2.framelength)
			    ? (frame_length - imgsensor_info.cap2.framelength) : 0;

			imgsensor.frame_length =
			    imgsensor_info.cap2.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
				LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				     framerate, imgsensor_info.cap.max_framerate / 10);

			frame_length =
			    imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength)
				? (frame_length - imgsensor_info.cap.framelength) : 0;

			imgsensor.frame_length =
			    imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length =
		    imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length > imgsensor_info.hs_video.framelength)
		    ? (frame_length - imgsensor_info.hs_video.framelength) : 0;

		imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length =
		    imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength)
			? (frame_length - imgsensor_info.slim_video.framelength) : 0;

		imgsensor.frame_length =
		    imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	default:		/* coding with  preview scenario by default */
		frame_length =
		    imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length > imgsensor_info.pre.framelength)
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
	/* enable = false; */
	if (enable) {

		/* 0x0601[2:0]; 0=no pattern,1=solid colour,2 = 100% colour bar ,3 = Fade to gray' colour bar */
		write_cmos_sensor_8(0x0601, 0x02);
	} else {
		write_cmos_sensor_8(0x0601, 0x00);
	}
	write_cmos_sensor(0x3200, 0x00);
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

	/* LOG_INF("feature_id = %d", feature_id); */
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		LOG_INF("feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n",
			imgsensor.pclk, imgsensor.current_fps);
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		if ((sensor_reg_data->RegData >> 8) > 0)
			write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		else
			write_cmos_sensor_8(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
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
		set_auto_flicker_mode((BOOL) *feature_data_16, *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM) *feature_data,
					      *(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM) *(feature_data),
						  (MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) *feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:	/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (MUINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
		/* zhdr,wdrs */
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("hdr mode :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.hdr_mode = (UINT8)*feature_data_32;
		/* imgsensor.hdr_mode = 9;                                               //force set hdr_mode to zHDR */
		spin_unlock(&imgsensor_drv_lock);
		/* LOG_INF("hdr mode :%d\n", imgsensor.hdr_mode); */
		break;

	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (MUINT32) *feature_data);
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
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n", (UINT16) *feature_data,
			(UINT16) *(feature_data + 1));
		hdr_write_shutter((UINT16) *feature_data, (UINT16) *(feature_data + 1));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n", *feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	default:
		break;
	}

	return ERROR_NONE;
}				/*      feature_control()  */

static SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 S5K4E6_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*      S5K4E6_MIPI_RAW_SensorInit      */
