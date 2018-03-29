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
 *	 OV8865mipi_Sensor.c
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
 *---------------------------------------------------------------------------
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
#include <asm/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
/* #include "kd_camera_hw.h" */
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "ov2724mipiraw_Sensor.h"

extern int iReadReg(u16 a_u2Addr, u8 *a_puBuff, u16 i2cId);
extern int iWriteReg(u16 a_u2Addr, u32 a_u4Data, u32 a_u4Bytes, u16 i2cId);

/* for link error, now set it to 0 */
static int main_sensor_init_setting_switch;


#define PFX "[OV2724mipiraw]"
/* #define LOG_WRN(format, args...) xlog_printk(ANDROID_LOG_WARN ,PFX, "[%S] " format, __FUNCTION__, ##args) */
/* #defineLOG_INF(format, args...) xlog_printk(ANDROID_LOG_INFO ,PFX, "[%s] " format, __FUNCTION__, ##args) */
/* #define LOG_DBG(format, args...) xlog_printk(ANDROID_LOG_DEBUG ,PFX, "[%S] " format, __FUNCTION__, ##args) */
#define LOG_INF(format, args...)	pr_debug(PFX "[%s] " format, __func__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);


static imgsensor_info_struct imgsensor_info = {
	.sensor_id = OV2724MIPI_SENSOR_ID,

	.checksum_value = 0x756a277f,

	.pre = {
		.pclk = 160000000,	/* record different mode's pclk */
		.linelength = 2282,	/* record different mode's linelength */
		.framelength = 2336,	/* record different mode's framelength */
		.startx = 0,	/* record different mode's startx of grabwindow */
		.starty = 0,	/* record different mode's starty of grabwindow */
		.grabwindow_width = 1920,	/* record different mode's width of grabwindow */
		.grabwindow_height = 1080,	/* record different mode's height of grabwindow */
		/*       following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
		.mipi_data_lp2hs_settle_dc = 0,
		/*       following for GetDefaultFramerateByScenario()  */
		.max_framerate = 300,
		},
	.cap = {		/*  */
		.pclk = 160000000,	/* this value just for calculate shutter,actual pclk */
		.linelength = 2282,	/*[actually 2008*2]        */
		.framelength = 2336,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 0,
		.max_framerate = 150,
		},
	.cap1 = {		/* 24fps 600M bps/lane */
		 .pclk = 160000000,	/* this value just for calculate shutter,actual pclk */
		 .linelength = 2282,
		 .framelength = 2336,
		 .startx = 0,
		 .starty = 0,
		 .grabwindow_width = 1920,
		 .grabwindow_height = 1080,
		 .mipi_data_lp2hs_settle_dc = 0,
		 .max_framerate = 150,
		 },
	.normal_video = {
			 .pclk = 160000000,
			 .linelength = 2282,	/*[actually 1923*2]   */
			 .framelength = 2336,
			 .startx = 0,
			 .starty = 0,
			 .grabwindow_width = 1920,
			 .grabwindow_height = 1080,
			 .mipi_data_lp2hs_settle_dc = 0,
			 .max_framerate = 300,
			 },
	.hs_video = {		/* neec check,   use video setting now */
		     .pclk = 160000000,
		     .linelength = 2282,
		     .framelength = 2336,
		     .startx = 0,
		     .starty = 0,
		     .grabwindow_width = 1920,
		     .grabwindow_height = 1080,
		     .mipi_data_lp2hs_settle_dc = 0,
		     .max_framerate = 300,
		     },
	.slim_video = {		/* equal preview setting */
		       .pclk = 160000000,
			   /*[actually 1923*2] record different mode 's linelength */
		       .linelength =2282,
		       .framelength = 2336,	/* record different mode' s framelength */
		       .startx = 0,	/* record different mode's startx of grabwindow */
		       .starty = 0,	/* record different mode's starty of grabwindow */
		       .grabwindow_width = 1920,	/* record different mode's width of grabwindow */
		       .grabwindow_height = 1080,	/* record different mode's height of grabwindow */
		       .mipi_data_lp2hs_settle_dc = 0,
		       .max_framerate = 300,
		       },
	.custom1 = {
		    .pclk = 160000000,	/* record different mode's pclk */
		    .linelength = 2282,	/* record different mode's linelength */
		    .framelength = 2336,	/* record different mode's framelength */
		    .startx = 0,	/* record different mode's startx of grabwindow */
		    .starty = 0,	/* record different mode's starty of grabwindow */
		    .grabwindow_width = 1920,	/* record different mode's width of grabwindow */
		    .grabwindow_height = 1080,	/* record different mode's height of grabwindow */
		    /*       following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
		    .mipi_data_lp2hs_settle_dc = 0,
		    /*       following for GetDefaultFramerateByScenario()  */
		    .max_framerate = 300,
		    },
	.custom2 = {
		    .pclk = 160000000,	/* record different mode's pclk */
		    .linelength = 2282,	/* record different mode's linelength */
		    .framelength = 2336,	/* record different mode's framelength */
		    .startx = 0,	/* record different mode's startx of grabwindow */
		    .starty = 0,	/* record different mode's starty of grabwindow */
		    .grabwindow_width = 1920,	/* record different mode's width of grabwindow */
		    .grabwindow_height = 1080,	/* record different mode's height of grabwindow */
		    /*       following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
		    .mipi_data_lp2hs_settle_dc = 0,
		    /*       following for GetDefaultFramerateByScenario()  */
		    .max_framerate = 300,

		    },
	.custom3 = {
		    .pclk = 160000000,	/* record different mode's pclk */
		    .linelength = 2282,	/* record different mode's linelength */
		    .framelength = 2336,	/* record different mode's framelength */
		    .startx = 0,	/* record different mode's startx of grabwindow */
		    .starty = 0,	/* record different mode's starty of grabwindow */
		    .grabwindow_width = 1920,	/* record different mode's width of grabwindow */
		    .grabwindow_height = 1080,	/* record different mode's height of grabwindow */
		    /*       following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
		    .mipi_data_lp2hs_settle_dc = 0,
		    /*       following for GetDefaultFramerateByScenario()  */
		    .max_framerate = 300,

		    },
	.custom4 = {
		    .pclk = 160000000,	/* record different mode's pclk */
		    .linelength = 2282,	/* record different mode's linelength */
		    .framelength = 2336,	/* record different mode's framelength */
		    .startx = 0,	/* record different mode's startx of grabwindow */
		    .starty = 0,	/* record different mode's starty of grabwindow */
		    .grabwindow_width = 1920,	/* record different mode's width of grabwindow */
		    .grabwindow_height = 1080,	/* record different mode's height of grabwindow */
		    /*       following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
		    .mipi_data_lp2hs_settle_dc = 0,
		    /*       following for GetDefaultFramerateByScenario()  */
		    .max_framerate = 300,

		    },
	.custom5 = {
		    .pclk = 160000000,	/* record different mode's pclk */
		    .linelength = 2282,	/* record different mode's linelength */
		    .framelength = 2336,	/* record different mode's framelength */
		    .startx = 0,	/* record different mode's startx of grabwindow */
		    .starty = 0,	/* record different mode's starty of grabwindow */
		    .grabwindow_width = 1920,	/* record different mode's width of grabwindow */
		    .grabwindow_height = 1080,	/* record different mode's height of grabwindow */
		    /*       following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
		    .mipi_data_lp2hs_settle_dc = 0,
		    /*       following for GetDefaultFramerateByScenario()  */
		    .max_framerate = 300,
		    },

	.margin = 6,
	.min_shutter = 1,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 1,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 5,	/* support sensor mode num ,don't support Slow motion */

	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 5,
	.hs_video_delay_frame = 5,
	.slim_video_delay_frame = 5,
	.custom1_delay_frame = 2,
	.custom2_delay_frame = 2,
	.custom3_delay_frame = 2,
	.custom4_delay_frame = 2,
	.custom5_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_2_LANE,
	.i2c_addr_table = {0x6c, 0x20, 0xff},
};




static imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,	/* IMGSENSOR_MODE enum value*/
	.shutter = 0x20,	/* current shutter */
	.gain = 0x20,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 300,	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	/* auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker */
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,	/* test pattern mode or not. */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,	/* current scenario id */
	.ihdr_en = 0,		/* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x6c,
};


/* Sensor output window information */
static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
{1920, 1080, 0, 0, 1920, 1080, 1920, 1080, 0, 0, 1920, 1080, 0, 0, 1920, 1080},	/* Preview */
{1920, 1080, 0, 0, 1920, 1080, 1920, 1080, 0, 0, 1920, 1080, 0, 0, 1920, 1080},	/* capture */
{1920, 1080, 0, 0, 1920, 1080, 1920, 1080, 0, 0, 1920, 1080, 0, 0, 1920, 1080},	/* video */
{1920, 1080, 0, 0, 1920, 1080, 1920, 1080, 0, 0, 1920, 1080, 0, 0, 1920, 1080},	/* hight speed video */
{1920, 1080, 0, 0, 1920, 1080, 1920, 1080, 0, 0, 1920, 1080, 0, 0, 1920, 1080},	/* slim video */
{1920, 1080, 0, 0, 1920, 1080, 1920, 1080, 0, 0, 1920, 1080, 0, 0, 1920, 1080},	/* Custom1 (defaultuse preview) */
{1920, 1080, 0, 0, 1920, 1080, 1920, 1080, 0, 0, 1920, 1080, 0, 0, 1920, 1080},	/* Custom2 */
{1920, 1080, 0, 0, 1920, 1080, 1920, 1080, 0, 0, 1920, 1080, 0, 0, 1920, 1080},	/* Custom3 */
{1920, 1080, 0, 0, 1920, 1080, 1920, 1080, 0, 0, 1920, 1080, 0, 0, 1920, 1080},	/* Custom4 */
{1920, 1080, 0, 0, 1920, 1080, 1920, 1080, 0, 0, 1920, 1080, 0, 0, 1920, 1080}
};				/* Custom5 */


static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	/* char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) }; */
	/* iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, imgsensor.i2c_write_id); */
	iReadReg((u16) addr, (u8 *) &get_byte, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	/* char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)}; */
	/* iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id); */
	iWriteReg((u16) addr, (u32) para, 1, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	kal_uint16 line_length, frame_height;

	LOG_INF("dummyline = %d, dummypixels = %d\n", imgsensor.dummy_line, imgsensor.dummy_pixel);

	/* todo */
	if (main_sensor_init_setting_switch != 0)	/* FATP bypass */
		return;

	line_length = imgsensor.dummy_pixel;
	frame_height = imgsensor.dummy_line;
	if ((line_length >= 0xFFFF) || (frame_height >= 0xFFFF)) {
		pr_warn("Warnning: line length or frame height is overflow!!!!!!!!\n");
		return;
	}
#if 1				/* add by chenqiang */
	spin_lock(&imgsensor_drv_lock);
	imgsensor.line_length = line_length;
	imgsensor.frame_length = frame_height;
	/* OV2724MIPI_sensor.default_height=frame_height; */
	spin_unlock(&imgsensor_drv_lock);

	write_cmos_sensor(0x0342, line_length >> 8);
	write_cmos_sensor(0x0343, line_length & 0xFF);
	write_cmos_sensor(0x0340, frame_height >> 8);
	write_cmos_sensor(0x0341, frame_height & 0xFF);
#endif
	LOG_INF("[OV2724MIPI]exit OV2724MIPI_Set_Dummy function\n");

}


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	/*kal_int16 dummy_line; */
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d, min framelength (%d) should enable?\n", framerate,
		min_framelength_en);

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
	kal_uint16 frame_height = 0, current_fps = 0;
	/*unsigned long flags;
	LOG_INF("[OV2724MIPI]enter OV2724MIPI_Write_Shutter function iShutter=%d\n", shutter);*/

	if (main_sensor_init_setting_switch != 0)	/* FATP bypass */
		return;

	if (!shutter)
		shutter = 1;	/* avoid 0 */
	else if (shutter >= 0x7FFF - 1) {
		shutter = 0x7FFF - 0xF;
		pr_err("Error: The Shutter is too big, The Sensor is dead!!!!!!!!\n");
	}
	if (shutter > imgsensor.frame_length - 4)
		frame_height = shutter + 4;
	else
		frame_height = imgsensor.frame_length;

	/*LOG_INF("[OV2724MIPI_Write_Shutter]iShutter:%x; frame_height:%x\n", shutter, frame_height);*/

	current_fps = imgsensor.pclk / imgsensor.line_length / frame_height;
	/*LOG_INF("CURRENT FPS:%d,OV2724MIPI_sensor.default_height=%d",
		current_fps, imgsensor.frame_length);*/
	if (current_fps == 30 || current_fps == 15) {
		if (imgsensor.autoflicker_en == TRUE)
			frame_height = frame_height + (frame_height >> 7);
	}
	write_cmos_sensor(0x0340, frame_height >> 8);
	write_cmos_sensor(0x0341, frame_height & 0xFF);
	write_cmos_sensor(0x3500, shutter >> 8);
	write_cmos_sensor(0x3501, shutter & 0xFF);
	/*LOG_INF("[OV2724MIPI]exit OV2724MIPI_Write_Shutter function 2722MIPI_frame_length=%x\n",
		frame_height);*/

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
	kal_uint16 iReg = 0x00;

	/*LOG_INF("[%s] iGain = %d\n", __func__, gain);*/
	iReg = ((gain / BASEGAIN) << 4) + ((gain % BASEGAIN) * 16 / BASEGAIN);
	iReg = iReg & 0xFF;
	/*LOG_INF("[%s] iGain2Reg = %d\n", __func__, iReg);*/
	return (kal_uint16) iReg;
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
static void set_gain(kal_uint16 gain)
{
#if 1
	kal_uint8 iReg;
	/* V5647_sensor.gain = iGain; */
	/* 0x350a[0:1], 0x350b AGC real gain */
	/* [0:3] = N meams N /16 X      */
	/* [4:9] = M meams M X  */
	/* Total gain = M + N /16 X */
	switch (main_sensor_init_setting_switch) {
	case 1:
	case 2:
		iReg = 0x10;
		break;
	case 3:
		iReg = 0xF8;
		break;
	default:
		{
			iReg = gain2reg(gain);
			if (iReg < 0x10)
				iReg = 0x10;
		}
		break;
	}
	write_cmos_sensor(0x3509, iReg);
	/*LOG_INF("[OV2724MIPI_SetGain] FATP mode (%d) Analog Gain = %d\n",
		main_sensor_init_setting_switch, iReg);*/

#endif
}				/*      set_gain  */


static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{

}


#if 0				/* Temp mark for build warning: [-Wunused-function] */
static void set_mirror_flip(kal_uint8 image_mirror)
{

	kal_int16 mir_flip = 0;

	mir_flip = read_cmos_sensor(0x0101);
	LOG_INF("image_mirror = %d, mirror_flip:%#x\n", image_mirror, mir_flip);

	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor(0x0101, (mir_flip & (0xFC)));
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor(0x0101, (mir_flip | (0x01)));
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor(0x0101, (mir_flip | (0x02)));
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor(0x0101, (mir_flip | (0x03)));
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

}				/*      night_mode      */


static void sensor_init(void)
{
	LOG_INF("[OV2724MIPI]enter OV2724MIPI_init function\n");
	LOG_INF("[OV2724MIPI]init config node = %d\n", main_sensor_init_setting_switch);
	write_cmos_sensor(0x0100, 0x00);	/* standby */
	write_cmos_sensor(0x0103, 0x01);	/* software reset */
	mdelay(5);		/* delay 200ms */
	write_cmos_sensor(0x0101, 0x01);
	write_cmos_sensor(0x0301, 0x0a);
	write_cmos_sensor(0x0303, 0x05);
	write_cmos_sensor(0x0307, 0x64);

	switch (main_sensor_init_setting_switch) {
	case 1:
	case 2:
		{		/* exposure time : 1/30s */
			write_cmos_sensor(0x0340, 0x09);
			write_cmos_sensor(0x0341, 0x20);
			write_cmos_sensor(0x3500, 0x09);
			write_cmos_sensor(0x3501, 0x1c);
		}
		break;
	case 3:
		{		/* exposure time : 1/15s */
			write_cmos_sensor(0x0340, 0x12);
			write_cmos_sensor(0x0341, 0x40);
			write_cmos_sensor(0x3500, 0x12);
			write_cmos_sensor(0x3501, 0x3c);
		}
		break;
	case 0:		/* Shipping mdoe */
	default:
		{		/* 30fps */
			write_cmos_sensor(0x0340, 0x09);
			write_cmos_sensor(0x0341, 0x20);	/* ;v05 */
			write_cmos_sensor(0x3500, 0x04);	/* 0x460 */
			write_cmos_sensor(0x3501, 0x66);
		}
		break;
	}

	write_cmos_sensor(0x0342, 0x08);
	write_cmos_sensor(0x0343, 0xea);
	write_cmos_sensor(0x0344, 0x00);
	write_cmos_sensor(0x0345, 0x00);

	write_cmos_sensor(0x0346, 0x00);
	write_cmos_sensor(0x0347, 0x00);
	write_cmos_sensor(0x0348, 0x07);
	write_cmos_sensor(0x0349, 0x9f);
	write_cmos_sensor(0x034a, 0x04);
	write_cmos_sensor(0x034b, 0x47);
	write_cmos_sensor(0x034c, 0x07);	/* 08 */
	write_cmos_sensor(0x034d, 0x80);
	write_cmos_sensor(0x034e, 0x04);	/* 02 */
	write_cmos_sensor(0x034f, 0x38);	/* 9b */

	write_cmos_sensor(0x0381, 0x01);
	write_cmos_sensor(0x0383, 0x01);
	write_cmos_sensor(0x0385, 0x01);	/* 45 */
	write_cmos_sensor(0x0387, 0x01);
	write_cmos_sensor(0x3014, 0x28);	/* 80 */
	write_cmos_sensor(0x3019, 0xd2);
	write_cmos_sensor(0x301f, 0x63);	/* 38 */
	write_cmos_sensor(0x3020, 0x09);
	write_cmos_sensor(0x3103, 0x02);	/* 0x85c */
	write_cmos_sensor(0x3106, 0x10);

	write_cmos_sensor(0x3502, 0x01);
	write_cmos_sensor(0x3503, 0x20);
	write_cmos_sensor(0x3504, 0x02);
	write_cmos_sensor(0x3505, 0x20);	/* add by chenqiang */
	write_cmos_sensor(0x3508, 0x00);
	write_cmos_sensor(0x3509, 0x7f);
	write_cmos_sensor(0x350a, 0x00);
	write_cmos_sensor(0x350b, 0x7f);

	write_cmos_sensor(0x350c, 0x00);
	write_cmos_sensor(0x350d, 0x7f);	/* ;BA_V03 */
	write_cmos_sensor(0x350f, 0x83);	/* Sync Gain ecffective time with Exposure */
	write_cmos_sensor(0x3510, 0x00);
	write_cmos_sensor(0x3511, 0x20);
	write_cmos_sensor(0x3512, 0x00);
	write_cmos_sensor(0x3513, 0x20);
	write_cmos_sensor(0x3514, 0x00);
	write_cmos_sensor(0x3515, 0x20);	/* 03 */
	write_cmos_sensor(0x3518, 0x00);

	write_cmos_sensor(0x3519, 0x7f);
	write_cmos_sensor(0x351a, 0x00);
	write_cmos_sensor(0x351b, 0x10);
	write_cmos_sensor(0x351c, 0x00);
	write_cmos_sensor(0x351d, 0x10);
	write_cmos_sensor(0x3602, 0x7c);
	write_cmos_sensor(0x3603, 0x22);	/* ;v06 */
	write_cmos_sensor(0x3620, 0x80);	/* ;v06 */
	write_cmos_sensor(0x3622, 0x0b);	/* ;R1A_AM02 */
	write_cmos_sensor(0x3623, 0x48);

	write_cmos_sensor(0x3632, 0xa0);
	write_cmos_sensor(0x3703, 0x23);
	write_cmos_sensor(0x3707, 0x93);
	write_cmos_sensor(0x3708, 0x46);
	write_cmos_sensor(0x370a, 0x33);
	write_cmos_sensor(0x3716, 0x50);
	write_cmos_sensor(0x3717, 0x00);
	write_cmos_sensor(0x3718, 0x10);
	write_cmos_sensor(0x371c, 0xfe);
	write_cmos_sensor(0x371d, 0x44);

	write_cmos_sensor(0x371e, 0x61);
	write_cmos_sensor(0x3721, 0x10);
	write_cmos_sensor(0x3725, 0xd1);
	write_cmos_sensor(0x3730, 0x01);	/* 00 */
	write_cmos_sensor(0x3731, 0xd0);
	write_cmos_sensor(0x3732, 0x02);
	write_cmos_sensor(0x3733, 0x60);
	write_cmos_sensor(0x3734, 0x00);
	write_cmos_sensor(0x3735, 0x00);
	write_cmos_sensor(0x3736, 0x00);

	write_cmos_sensor(0x3737, 0x00);	/* 00 */
	write_cmos_sensor(0x3738, 0x02);
	write_cmos_sensor(0x3739, 0x20);	/* a1 */
	write_cmos_sensor(0x373a, 0x01);
	write_cmos_sensor(0x373b, 0xb0);
	write_cmos_sensor(0x3748, 0x0b);
	write_cmos_sensor(0x3749, 0x9c);
	write_cmos_sensor(0x3759, 0x50);
	write_cmos_sensor(0x3810, 0x00);
	write_cmos_sensor(0x3811, 0x0f);

	write_cmos_sensor(0x3812, 0x00);
	write_cmos_sensor(0x3813, 0x08);
	write_cmos_sensor(0x3820, 0x80);
	write_cmos_sensor(0x3821, 0x00);
	write_cmos_sensor(0x382d, 0x00);
	write_cmos_sensor(0x3831, 0x00);	/* add by chenqiang for r4800 bit[3]  Line sync enable. */
	write_cmos_sensor(0x3b00, 0x50);	/* len start/end */
	write_cmos_sensor(0x3b01, 0x24);
	write_cmos_sensor(0x3b02, 0x34);
	write_cmos_sensor(0x3b04, 0xdc);

	write_cmos_sensor(0x3b09, 0x62);
	write_cmos_sensor(0x4001, 0x00);
	write_cmos_sensor(0x4008, 0x04);
	write_cmos_sensor(0x4009, 0x0d);
	write_cmos_sensor(0x400a, 0x01);
	write_cmos_sensor(0x400b, 0x80);
	write_cmos_sensor(0x400c, 0x00);
	write_cmos_sensor(0x400d, 0x01);
	write_cmos_sensor(0x4010, 0xd0);
	write_cmos_sensor(0x4017, 0x08);

	write_cmos_sensor(0x4042, 0x12);
	write_cmos_sensor(0x4303, 0x00);
	write_cmos_sensor(0x4307, 0x3a);
	write_cmos_sensor(0x4320, 0x80);
	write_cmos_sensor(0x4322, 0x00);
	write_cmos_sensor(0x4323, 0x00);
	write_cmos_sensor(0x4324, 0x00);
	write_cmos_sensor(0x4325, 0x00);
	write_cmos_sensor(0x4326, 0x00);
	write_cmos_sensor(0x4327, 0x00);

	write_cmos_sensor(0x4328, 0x00);
	write_cmos_sensor(0x4329, 0x00);
	write_cmos_sensor(0x4501, 0x08);
	write_cmos_sensor(0x4505, 0x05);
	write_cmos_sensor(0x4601, 0x0a);
	write_cmos_sensor(0x4800, 0x04);
	write_cmos_sensor(0x4816, 0x52);
	write_cmos_sensor(0x481f, 0x32);
	write_cmos_sensor(0x4837, 0x14);
	write_cmos_sensor(0x4838, 0x00);

	write_cmos_sensor(0x490b, 0x00);
	write_cmos_sensor(0x4a00, 0x01);
	write_cmos_sensor(0x4a01, 0xff);
	write_cmos_sensor(0x4a02, 0x59);
	write_cmos_sensor(0x4a03, 0xd7);
	write_cmos_sensor(0x4a04, 0xff);
	write_cmos_sensor(0x4a05, 0x30);
	write_cmos_sensor(0x4a07, 0xff);
	write_cmos_sensor(0x4d00, 0x04);
	write_cmos_sensor(0x4d01, 0x51);

	write_cmos_sensor(0x4d02, 0xd0);
	write_cmos_sensor(0x4d03, 0x7f);
	write_cmos_sensor(0x4d04, 0x92);
	write_cmos_sensor(0x4d05, 0xcf);
	write_cmos_sensor(0x4d0b, 0x01);
	write_cmos_sensor(0x5000, 0x1f);
	write_cmos_sensor(0x5080, 0x00);
	write_cmos_sensor(0x5101, 0x0a);
	write_cmos_sensor(0x5103, 0x69);
	write_cmos_sensor(0x3021, 0x00);

	write_cmos_sensor(0x3022, 0x00);
	write_cmos_sensor(0x0100, 0x01);

	imgsensor.autoflicker_en = KAL_FALSE;
	mdelay(1);
	LOG_INF("[OV2724MIPI]exit OV2724MIPI_init function\n");
}

/*	sensor_init  */

static void preview_setting(void)
{
	LOG_INF("Enter\n");

	write_cmos_sensor(0x0100, 0x01);	/* streaming */
}				/*      preview_setting  */


static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("Enter! currefps:%d\n", currefps);

	write_cmos_sensor(0x0100, 0x01);	/* streaming */

}


static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("Enter\n");

	write_cmos_sensor(0x0100, 0x01);	/* streaming */
}


static void hs_video_setting(void)
{
	LOG_INF("Enter\n");

	write_cmos_sensor(0x0100, 0x01);	/* streaming */
}



static void slim_video_setting(void)
{
	LOG_INF("Enter\n");

	write_cmos_sensor(0x0100, 0x01);	/* streaming */
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

	/* write_cmos_sensor(0x0103,0x01);// Reset sensor */
	/* mdelay(2); */

	/* module have defferent  i2c address; */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = ((read_cmos_sensor(0x300A) << 8) | read_cmos_sensor(0x300B));
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail, write id: 0x%x, sensor id:0x%x\n",
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

	LOG_INF("lanes:MIPI 2LANE\n");
	LOG_INF("preview/video:30fps,Capture:15 fps\n");
	LOG_INF("...\n");


	/* write_cmos_sensor(0x0103,0x01);// Reset sensor */
	/* mdelay(2); */

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = ((read_cmos_sensor(0x300A) << 8) | read_cmos_sensor(0x300B));
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, id: 0x%x\n", sensor_id);
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
	imgsensor.ihdr_en = 0;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.shutter = 0x100;
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
	write_cmos_sensor(0x0101, 0x02);
	/* set_mirror_flip(imgsensor.mirror); */
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
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF
			    ("Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n",
			     imgsensor.current_fps, imgsensor_info.cap1.max_framerate / 10);

		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);

	if (imgsensor.test_pattern == KAL_TRUE)
		write_cmos_sensor(0x5080, 0x80);

	write_cmos_sensor(0x0101, 0x02);
	/* set_mirror_flip(imgsensor.mirror); */
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
	/* imgsensor.current_fps = imgsensor_info.normal_video.max_framerate; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	write_cmos_sensor(0x0101, 0x02);
	/* set_mirror_flip(imgsensor.mirror); */
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
	/* imgsensor.current_fps = imgsensor_info.hs_video.max_framerate;; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	write_cmos_sensor(0x0101, 0x02);
	/* set_mirror_flip(imgsensor.mirror); */
	return ERROR_NONE;
}				/*      hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = imgsensor_info.slim_video.max_framerate;; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	write_cmos_sensor(0x0101, 0x02);
	/* set_mirror_flip(imgsensor.mirror); */
	return ERROR_NONE;
}				/*      slim_video       */

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
	LOG_INF("+++\n");
	LOG_INF("imgsensor_info.cap.grabwindow_width: %d\n", imgsensor_info.cap.grabwindow_width);
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
	LOG_INF("---\n");
	return ERROR_NONE;
}				/*      get_resolution  */

static kal_uint32 get_info(MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);


	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;	/* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;	/* inverse with datasheet */
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;	/* not use */
	sensor_info->SensorResetActiveHigh = TRUE;	/* not use */
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
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:	/* 2 */
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:	/* 3 */
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:	/* 9 */
		slim_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		Custom1(image_window, sensor_config_data);	/* Custom1 */
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		Custom2(image_window, sensor_config_data);	/* Custom1 */
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		Custom3(image_window, sensor_config_data);	/* Custom1 */
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		Custom4(image_window, sensor_config_data);	/* Custom1 */
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		Custom5(image_window, sensor_config_data);	/* Custom1 */
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
	if (enable)
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
		frame_length =
		    imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.pre.framelength) ? (frame_length -
							imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length =
		    imgsensor_info.normal_video.pclk / framerate * 10 /
		    imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.normal_video.framelength) ? (frame_length -
								 imgsensor_info.
								 normal_video.framelength) : 0;
		imgsensor.frame_length =
		    imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		frame_length =
		    imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.cap.framelength) ? (frame_length -
							imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length =
		    imgsensor_info.hs_video.pclk / framerate * 10 /
		    imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.hs_video.framelength) ? (frame_length -
							     imgsensor_info.
							     hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length =
		    imgsensor_info.slim_video.pclk / framerate * 10 /
		    imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.slim_video.framelength) ? (frame_length -
							       imgsensor_info.
							       slim_video.framelength) : 0;
		imgsensor.frame_length =
		    imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length =
		    imgsensor_info.custom1.pclk / framerate * 10 /
		    imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.custom1.framelength) ? (frame_length -
							    imgsensor_info.custom1.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length =
		    imgsensor_info.custom2.pclk / framerate * 10 /
		    imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.custom2.framelength) ? (frame_length -
							    imgsensor_info.custom2.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		frame_length =
		    imgsensor_info.custom3.pclk / framerate * 10 /
		    imgsensor_info.custom3.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.custom3.framelength) ? (frame_length -
							    imgsensor_info.custom3.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom3.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		frame_length =
		    imgsensor_info.custom4.pclk / framerate * 10 /
		    imgsensor_info.custom4.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.custom4.framelength) ? (frame_length -
							    imgsensor_info.custom4.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom4.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		frame_length =
		    imgsensor_info.custom5.pclk / framerate * 10 /
		    imgsensor_info.custom5.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.custom5.framelength) ? (frame_length -
							    imgsensor_info.custom5.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	default:		/* coding with  preview scenario by default */
		frame_length =
		    imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.pre.framelength) ? (frame_length -
							imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
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
		LOG_INF("Warning: Invalid scenario_id = %d\n", scenario_id);
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);

	if (enable)
		write_cmos_sensor(0x5080, 0x80);
	else
		write_cmos_sensor(0x5080, 0x00);

	return ERROR_NONE;
}

static int strcmp1(MUINT8 *s1, MUINT8 *s2)
{
	while (*s1 && *s2) {
		if (*s1 > *s2)
			return 1;
		else if (*s1 < *s2)
			return -1;
		else
		{
			++s1;
			++s2;
			continue;
		}
	}
	if (*s1)
		return 1;
	if (*s2)
		return -1;
	return 0;
}

static void debug_imgsensor(MSDK_SENSOR_FEATURE_ENUM feature_id,
			    UINT8 *feature_para, UINT32 *feature_para_len)
{
	MSDK_SENSOR_DBG_IMGSENSOR_INFO_STRUCT *debug_info =
	    (MSDK_SENSOR_DBG_IMGSENSOR_INFO_STRUCT *) feature_para;
	if (strcmp1(debug_info->debugStruct, (MUINT8 *) "sensor_output_dataformat") == 0) {
		LOG_INF("enter sensor_output_dataformat\n");
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.sensor_output_dataformat;
		else
			imgsensor_info.sensor_output_dataformat = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "sensor_id") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.sensor_id;
		else
			imgsensor_info.sensor_id = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "checksum_value") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.checksum_value;
		else
			imgsensor_info.checksum_value = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "isp_driving_current") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.isp_driving_current;
		else
			imgsensor_info.isp_driving_current = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "sensor_interface_type") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.sensor_interface_type;
		else
			imgsensor_info.sensor_interface_type = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "mclk") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.mclk;
		else
			imgsensor_info.mclk = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "mipi_lane_num") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.mipi_lane_num;
		else
			imgsensor_info.mipi_lane_num = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "i2c_addr_table") == 0) {
		/* just use the first address */
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.i2c_addr_table[0];
		else
			imgsensor_info.i2c_addr_table[0] = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "mipi_sensor_type") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.mipi_sensor_type;
		else
			imgsensor_info.mipi_sensor_type = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "mipi_settle_delay_mode") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.mipi_settle_delay_mode;
		else
			imgsensor_info.mipi_settle_delay_mode = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "cap_delay_frame") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.cap_delay_frame;
		else
			imgsensor_info.cap_delay_frame = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "pre_delay_frame") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.pre_delay_frame;
		else
			imgsensor_info.pre_delay_frame = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "video_delay_frame") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.video_delay_frame;
		else
			imgsensor_info.video_delay_frame = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "hs_video_delay_frame") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.hs_video_delay_frame;
		else
			imgsensor_info.hs_video_delay_frame = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "slim_video_delay_frame") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.slim_video_delay_frame;
		else
			imgsensor_info.slim_video_delay_frame = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "sensor_mode_num") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.sensor_mode_num;
		else
			imgsensor_info.sensor_mode_num = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "ihdr_le_firstline") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.ihdr_le_firstline;
		else
			imgsensor_info.ihdr_le_firstline = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "ihdr_support") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.ihdr_support;
		else
			imgsensor_info.ihdr_support = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "ae_ispGain_delay_frame") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.ae_ispGain_delay_frame;
		else
			imgsensor_info.ae_ispGain_delay_frame = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "ae_sensor_gain_delay_frame") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.ae_sensor_gain_delay_frame;
		else
			imgsensor_info.ae_sensor_gain_delay_frame = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "ae_shut_delay_frame") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.ae_shut_delay_frame;
		else
			imgsensor_info.ae_shut_delay_frame = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "max_frame_length") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.max_frame_length;
		else
			imgsensor_info.max_frame_length = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "min_shutter") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.min_shutter;
		else
			imgsensor_info.min_shutter = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "margin") == 0) {
		if (debug_info->isGet == 1)
			debug_info->value = imgsensor_info.margin;
		else
			imgsensor_info.margin = debug_info->value;
	} else if (strcmp1(debug_info->debugStruct, (MUINT8 *) "pre") == 0) {
		if (strcmp1(debug_info->debugSubstruct, (MUINT8 *) "pclk") == 0) {
			if (debug_info->isGet == 1)
				debug_info->value = imgsensor_info.pre.pclk;
			else
				imgsensor_info.pre.pclk = debug_info->value;
		} else if (strcmp1(debug_info->debugSubstruct, (MUINT8 *) "linelength") == 0) {
			if (debug_info->isGet == 1)
				debug_info->value = imgsensor_info.pre.linelength;
			else
				imgsensor_info.pre.linelength = debug_info->value;
		} else if (strcmp1(debug_info->debugSubstruct, (MUINT8 *) "framelength") == 0) {
			if (debug_info->isGet == 1)
				debug_info->value = imgsensor_info.pre.framelength;
			else
				imgsensor_info.pre.framelength = debug_info->value;
		} else if (strcmp1(debug_info->debugSubstruct, (MUINT8 *) "startx") == 0) {
			if (debug_info->isGet == 1)
				debug_info->value = imgsensor_info.pre.startx;
			else
				imgsensor_info.pre.startx = debug_info->value;
		} else if (strcmp1(debug_info->debugSubstruct, (MUINT8 *) "starty") == 0) {
			if (debug_info->isGet == 1)
				debug_info->value = imgsensor_info.pre.starty;
			else
				imgsensor_info.pre.starty = debug_info->value;
		} else if (strcmp1(debug_info->debugSubstruct, (MUINT8 *) "grabwindow_width") == 0) {
			if (debug_info->isGet == 1)
				debug_info->value = imgsensor_info.pre.grabwindow_width;
			else
				imgsensor_info.pre.grabwindow_width = debug_info->value;
		} else if (strcmp1(debug_info->debugSubstruct, (MUINT8 *) "grabwindow_height") == 0) {
			if (debug_info->isGet == 1)
				debug_info->value = imgsensor_info.pre.grabwindow_height;
			else
				imgsensor_info.pre.grabwindow_height = debug_info->value;
		} else
		    if (strcmp1(debug_info->debugSubstruct, (MUINT8 *) "mipi_data_lp2hs_settle_dc")
			== 0) {
			if (debug_info->isGet == 1)
				debug_info->value = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			else
				imgsensor_info.pre.mipi_data_lp2hs_settle_dc = debug_info->value;
		} else if (strcmp1(debug_info->debugSubstruct, (MUINT8 *) "max_framerate") == 0) {
			if (debug_info->isGet == 1)
				debug_info->value = imgsensor_info.pre.max_framerate;
			else
				imgsensor_info.pre.max_framerate = debug_info->value;
		}
	} else {
		LOG_INF("unknown debug\n");
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
	/*unsigned long long *feature_return_para=(unsigned long long *) feature_para; */

	SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/*LOG_INF("feature_id = %d\n", feature_id);*/
	switch (feature_id) {
	case SENSOR_FEATURE_DEBUG_IMGSENSOR:
		debug_imgsensor(feature_id, feature_para, feature_para_len);
		break;

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
		set_auto_flicker_mode((BOOL) * feature_data_16, *(feature_data_16 + 1));
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
		set_test_pattern_mode((BOOL) * feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:	/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", (int)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", (BOOL) * feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (BOOL) * feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (int)*feature_data);
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
	default:
		/*LOG_INF("WARNING: Unsupported cmd id: %d\n", feature_id);*/
		return ERROR_INVALID_FEATURE_ID;
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

UINT32 OV2724MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	else {
		LOG_INF("****critical: pfFunc is NULL!");
		return ERROR_DRIVER_INIT_FAIL;
	}
	return ERROR_NONE;
}				/*      IMX208_MIPI_RAW_SensorInit      */
