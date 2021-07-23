/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
//#include <asm/system.h>
//#include <linux/xlog.h>

#include "imgsensor_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "hi259ofilm_mipiraw_Sensor.h"

/****************************Modify Following Strings for Debug****************************/
#define PFX "HI259_camera_sensor  "
#define LOG_1 LOG_INF("HI259,MIPI 1LANE\n")
#define LOG_2 LOG_INF("preview 1600*1200@30fps,600Mbps/lane; video 1600*1200@30fps,600Mbps/lane; 1600*1200@30fps,600Mbps/lane\n")
/****************************   Modify end    *******************************************/
//#define LOG_INF(format, args...)	pr_debug(PFX "[%s] " format, __FUNCTION__, ##args)
#define LOG_INF(fmt, args...)  printk(KERN_INFO PFX fmt, ##args)

#define MULTI_WRITE 1
#define ENABLE_PDAF 0

static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = HI259OFILM_SENSOR_ID, //HI259_SENSOR_ID,  /*sensor_id = 0x00E1*/ //record sensor id defined in Kd_imgsensor.h

	.checksum_value = 0xcdcc26f2, //checksum value for Camera Auto Test

	.pre = {
		.pclk = 78400000,	 //78.4M 	//record different mode's pclk
		.linelength =  1800, //1800		//record different mode's linelength
		.framelength = 1450, //1450		//record different mode's framelength
		.startx = 0,				    //record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 1600, 		//record different mode's width of grabwindow
		.grabwindow_height = 1200,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 78400000, //(784Mbps*1/10)
	},
	.cap = {
		.pclk =78400000,
		.linelength = 1800,
		.framelength = 1450,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 78400000,
	},

    .cap1 = {
		.pclk =78400000,
		.linelength = 1800,
		.framelength = 1450,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 78400000,
    },
	.normal_video = {
		.pclk =78400000,
		.linelength = 1800,
		.framelength = 1450,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 78400000,
	},
	.hs_video = {
		.pclk =78400000,
		.linelength = 1800,
		.framelength = 1450,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 78400000,
	},
    .slim_video = {
		.pclk =78400000,
		.linelength = 1800,
		.framelength = 1450,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 78400000,
    },

	.margin = 4,
	.min_shutter = 4,
	.max_frame_length = 0x7FFF,
	.min_gain = BASEGAIN,
	.max_gain = 8 * BASEGAIN,
	.min_gain_iso = 100,
	.gain_step = 64,
	.gain_type = 0,
#if 0
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
#else
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 1,
	.ae_ispGain_delay_frame = 2,
#endif

	.ihdr_support = 0,      //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 5,	  //support sensor mode num

	.cap_delay_frame = 1,		//enter capture delay frame num
	.pre_delay_frame = 2,		//enter preview delay frame num
	.video_delay_frame = 1, 	//enter video delay frame num
	.hs_video_delay_frame = 2,	//enter high speed video  delay frame num
	.slim_video_delay_frame = 2,//enter slim video delay frame num

	.isp_driving_current = ISP_DRIVING_4MA, //mclk driving current
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO, //0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_1_LANE,
	.i2c_addr_table = {0x60, 0xff},
	.i2c_speed = 400,
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x014d,
	.gain = 0xe000,						//current gain
	.dummy_pixel = 0,
	.dummy_line = 0,
//full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x60,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{ 1600, 1200,   0,   0, 1600, 1200,	 1600, 1200,  0,  0, 1600, 1200, 0, 0, 1600, 1200},		// preview (1600 x 1200)
	{ 1600, 1200,   0,   0, 1600, 1200,	 1600, 1200,  0,  0, 1600, 1200, 0, 0, 1600, 1200},		// capture (1600 x 1200)
	{ 1600, 1200,   0,   0, 1600, 1200,	 1600, 1200,  0,  0, 1600, 1200, 0, 0, 1600, 1200},		// VIDEO (4208 x 3120)
	{ 1600, 1200,   0,   0, 1600, 1200,	 1600, 1200,  0,  0, 1600, 1200, 0, 0, 1600, 1200},		// hight speed video (1280 x 720)
	{ 1600, 1200,   0,   0, 1600, 1200,	 1600, 1200,  0,  0, 1600, 1200, 0, 0, 1600, 1200},     // slim video (1920 x 1080)
};


#if ENABLE_PDAF
//Hi-259 don't support PDAF
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3]=
{
	/* Preview mode setting */
	 {0x02, //VC_Num
	  0x0a, //VC_PixelNum
	  0x00, //ModeSelect	/* 0:auto 1:direct */
	  0x00, //EXPO_Ratio	/* 1/1, 1/2, 1/4, 1/8 */
	  0x00, //0DValue		/* 0D Value */
	  0x00, //RG_STATSMODE	/* STATS divistion mode 0:16x16  1:8x8	2:4x4  3:1x1 */
	  0x00, 0x2B, 0x0838, 0x0618,	// VC0 Maybe image data?
	  0x00, 0x00, 0x0000, 0x0000,	// VC1 MVHDR
	  0x01, 0x30, 0x0000, 0x0000,   // VC2 PDAF
	  0x00, 0x00, 0x0000, 0x0000},	// VC3 ??
	/* Capture mode setting */
	 {0x02, //VC_Num
	  0x0a, //VC_PixelNum
	  0x00, //ModeSelect	/* 0:auto 1:direct */
	  0x00, //EXPO_Ratio	/* 1/1, 1/2, 1/4, 1/8 */
	  0x00, //0DValue		/* 0D Value */
	  0x00, //RG_STATSMODE	/* STATS divistion mode 0:16x16  1:8x8	2:4x4  3:1x1 */
	  0x00, 0x2B, 0x1070, 0x0C30,	// VC0 Maybe image data?
	  0x00, 0x00, 0x0000, 0x0000,	// VC1 MVHDR
	  //Data type 0x00, 0x30, 0x0140, 0x0300,   // VC2 PDAF
	  0x01, 0x30, 0x0140, 0x0300,   // VC2 PDAF
	  0x00, 0x00, 0x0000, 0x0000},	// VC3 ??
	/* Video mode setting */
	 {0x02, //VC_Num
	  0x0a, //VC_PixelNum
	  0x00, //ModeSelect	/* 0:auto 1:direct */
	  0x00, //EXPO_Ratio	/* 1/1, 1/2, 1/4, 1/8 */
	  0x00, //0DValue		/* 0D Value */
	  0x00, //RG_STATSMODE	/* STATS divistion mode 0:16x16  1:8x8	2:4x4  3:1x1 */
	  0x00, 0x2B, 0x1070, 0x0C30,	// VC0 Maybe image data?
	  0x00, 0x00, 0x0000, 0x0000,	// VC1 MVHDR
	  0x01, 0x30, 0x0140, 0x0300,   // VC2 PDAF
	  0x00, 0x00, 0x0000, 0x0000},	// VC3 ??

};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info =
{
    .i4OffsetX = 56,
    .i4OffsetY = 24,
    .i4PitchX = 32,
    .i4PitchY = 32,
    .i4PairNum =8,
    .i4SubBlkW =16,
    .i4SubBlkH =8,
    .i4PosL = {{60,29},{76,29},{68,33},{84,33},{60,45},{76,45},{68,49},{84,49}},
    .i4PosR = {{60,25},{76,25},{68,37},{84,37},{60,41},{76,41},{68,53},{84,53}},
    .i4BlockNumX = 128,
    .i4BlockNumY = 96,
    /* 0:IMAGE_NORMAL,1:IMAGE_H_MIRROR,2:IMAGE_V_MIRROR,3:IMAGE_HV_MIRROR */
    .iMirrorFlip = 0,
};
#endif


#if MULTI_WRITE
#define I2C_BUFFER_LEN 510
#else
#define I2C_BUFFER_LEN 2
#endif

#if MULTI_WRITE
static kal_uint16 hi259_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	tosend = 0;
	IDX = 0;
	while (len > IDX) {
		addr = para[IDX];

		{
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;
		}
#if MULTI_WRITE
		if ((I2C_BUFFER_LEN - tosend) < 2 || len == IDX || addr != addr_last) {
			LOG_INF("burst write\n");
			iBurstWriteReg_multi(puSendCmd, tosend,	imgsensor.i2c_write_id, 2, imgsensor_info.i2c_speed);

			tosend = 0;
		}
#else
		iWriteRegI2C(puSendCmd, 2, imgsensor.i2c_write_id);
		tosend = 0;
#endif
	}
	return 0;
}
#endif

//Hi-259 support 1A1D type
static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	char pu_send_cmd[1] = {(char)(addr & 0xFF) };
	iReadRegI2C(pu_send_cmd, 1, (u8*)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

//Hi-259 support 1A1D type
static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[2] = {(char)(addr & 0xFF), (char)(para & 0xFF)};

	pr_err("addr val = 0x%x, value = 0x%x=======================================\n",
		addr, para);
	iWriteRegI2C(pu_send_cmd, 2, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	//write_cmos_sensor(0x03, 0x00);
	//write_cmos_sensor(0x4e, imgsensor.frame_length >> 8);
	//write_cmos_sensor(0x4f, imgsensor.frame_length & 0xFF);
}	/*	set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	return (read_cmos_sensor(0x04));

}

static kal_uint16 get_vendor_id(void)
{
        kal_uint16 get_byte = 0;
        char pusendcmd[2] = { (char)(0x01 >> 8), (char)(0x01 & 0xFF) };

        iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 1, 0xA2);

        return get_byte;
}
static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ?	frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length -	imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) 	{
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length -	imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */

static void write_shutter(kal_uint32 shutter)
{
    unsigned long flags;
	kal_uint32 realtime_fps = 0;
    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);

	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	LOG_INF("shutter = %d, imgsensor.frame_length = %d, imgsensor.min_frame_length = %d\n",
		shutter, imgsensor.frame_length, imgsensor.min_frame_length);


	shutter = (shutter < imgsensor_info.min_shutter) ?	imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ?  (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;
		//realtime_fps = imgsensor.pclk /	(imgsensor.line_length * imgsensor.frame_length) * 10;
	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
        	else {
			//write_cmos_sensor(0x03, 0x00);
			//write_cmos_sensor(0x1f, 0x01);
			//write_cmos_sensor(0x03, 0x00);
			//write_cmos_sensor(0x4e, imgsensor.frame_length >> 8);
			//write_cmos_sensor(0x4f, imgsensor.frame_length & 0xFF);
		}
	} else{
		//write_cmos_sensor(0x03, 0x00);
		//write_cmos_sensor(0x1f, 0x01);
		//write_cmos_sensor(0x03, 0x00);
		//write_cmos_sensor(0x4e, imgsensor.frame_length >> 8);
		//write_cmos_sensor(0x4f, imgsensor.frame_length & 0xFF);
	}

	write_cmos_sensor(0x03, 0x00);
	write_cmos_sensor(0x1f, 0x01);
	write_cmos_sensor(0x03, 0x20);
	write_cmos_sensor(0x22, shutter >> 8);
	write_cmos_sensor(0x23, shutter & 0xFF);
	write_cmos_sensor(0x03, 0x00);
	write_cmos_sensor(0x1f, 0x00);

	LOG_INF("frame_length = %d , shutter = %d \n", imgsensor.frame_length, shutter);

}	/*	   	   write_shutter  */

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

	LOG_INF("set_shutter");
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

    write_shutter(shutter);
}	/*	set_shutter */


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
static kal_uint16 gain2reg(kal_uint16 gain)
{
    kal_uint16 reg_gain = 0xe000;

    reg_gain =  (uint16_t)(((256 * 64 / gain)-17) * 2);

    return (kal_uint16)reg_gain;

}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

	if (gain < BASEGAIN || gain >= 8 * BASEGAIN) {
        LOG_INF("Error gain setting");

        if (gain < BASEGAIN)
            gain = BASEGAIN;
	else if (gain >= 8 * BASEGAIN)
            gain = 8 * BASEGAIN;
    }

    reg_gain = gain2reg(gain);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.gain = reg_gain;
    spin_unlock(&imgsensor_drv_lock);
    LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

    write_cmos_sensor(0x03, 0x20);
    write_cmos_sensor(0x60, reg_gain>>1);
    write_cmos_sensor(0x61, reg_gain&0x01);

	return gain;
}

#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d", image_mirror);

	switch (image_mirror) {
	case IMAGE_NORMAL:
			write_cmos_sensor(0x03,0x00);
			write_cmos_sensor(0x11,0x80); // v flip  //mirror 0x80,0x81,0x82,0x83

			break;
		case IMAGE_H_MIRROR:
			write_cmos_sensor(0x03,0x00);
			write_cmos_sensor(0x11,0x81); // v flip  //mirror 0x80,0x81,0x82,0x83

			break;
		case IMAGE_V_MIRROR:
			write_cmos_sensor(0x03,0x00);
			write_cmos_sensor(0x11,0x82); // v flip  //mirror 0x80,0x81,0x82,0x83
			break;
		case IMAGE_HV_MIRROR:
			write_cmos_sensor(0x03,0x00);
			write_cmos_sensor(0x11,0x83); // v flip  //mirror 0x80,0x81,0x82,0x83

			break;
		default:
			LOG_INF("Error image_mirror setting\n");
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
}	/*	night_mode	*/

#if MULTI_WRITE
kal_uint16 addr_data_pair_init_hi259ofilm[] = {
0x03, 0x00,
0x01, 0x01,
0x01, 0x03,
0x01, 0x01,

//////////////////
// BGR enable
//////////////////
0x03, 0x02,
0x1f, 0x01,

0x03, 0x00,
0x07, 0x05,
0x08, 0x62,
0x09, 0x13,
0x07, 0x85,
0x07, 0x85,
0x07, 0x85,
0x0a, 0x80,
0x07, 0xC5,

0x03, 0x00,
0x10, 0x00,
0x11, 0x80,
0x13, 0x01,
0x14, 0x20,
0x15, 0x81,
0x17, 0x10,
0x1a, 0x00,
0x1c, 0x00,
0x1f, 0x00,

0x20, 0x00,
0x21, 0x0e,
0x22, 0x00,
0x23, 0x10,
0x24, 0x00,
0x25, 0x0e,
0x26, 0x00,
0x27, 0x10,
0x28, 0x04,
0x29, 0xb0,
0x2a, 0x06,
0x2b, 0x40,

0x30, 0x00,
0x31, 0x00,
0x32, 0x00,
0x33, 0x00,
0x34, 0x00,
0x35, 0x00,
0x36, 0x00,
0x37, 0x00,
0x38, 0x02,
0x39, 0x60,
0x3a, 0x03,
0x3b, 0x20,
0x4c, 0x07,
0x4d, 0x08,
0x4e, 0x05,
0x4f, 0xaa,
0x54, 0x02,
0x55, 0x03,
0x56, 0x04,
0x57, 0x40,
0x58, 0x03,
0x5c, 0x0a,
0x60, 0x00,
0x61, 0x00,
0x62, 0x80,
0x68, 0x03,
0x69, 0x42,
0x80, 0x00,
0x81, 0x08,
0x82, 0x00,
0x83, 0x06,
0x84, 0x06,
0x85, 0x50,
0x86, 0x04,
0x87, 0xcc,
0x88, 0x00,
0x89, 0x06,
0x8a, 0x02,
0x8b, 0x60,
0x90, 0x00,
0x91, 0x02,
0xa0, 0x01,
0xa1, 0x40,
0xa2, 0x40,
0xa3, 0x40,
0xa4, 0x40,
0xe4, 0x10,
0xe5, 0x00,

//////////////////
// Page 0x01
//////////////////
0x03, 0x01,
0x10, 0x21,
0x11, 0x00,
0x12, 0x3f,
0x13, 0x08,
0x14, 0x04,
0x15, 0x01,
0x16, 0x00,
0x17, 0x03,
0x18, 0x00,
0x19, 0x00,
0x20, 0x60,
0x21, 0x00,
0x22, 0x20,
0x23, 0x3c,
0x24, 0x5c,
0x25, 0x00,
0x26, 0x60,
0x27, 0x07,
0x28, 0x80,
0x29, 0x00,
0x2a, 0xff,
0x2b, 0x20,
0x2c, 0x80,
0x2d, 0x80,
0x2e, 0x80,
0x2f, 0x80,
0x85, 0x10,

0x30, 0x7b,
0x31, 0x01,
0x32, 0xfe,
0x33, 0x00,
0x34, 0x51,
0x35, 0xb6,
0x36, 0xfc,
0x38, 0x66,
0x39, 0x66,

0x40, 0x00,
0x41, 0x00,
0x42, 0x00,
0x43, 0x00,
0x44, 0x00,
0x45, 0x00,

0x48, 0x00,
0x49, 0x00,
0x4a, 0x00,
0x4b, 0x00,
0x4c, 0x00,
0x4d, 0x00,

0x50, 0x00,
0x51, 0x00,
0x52, 0x00,
0x53, 0x00,
0x54, 0x00,
0x55, 0x00,

0x58, 0x00,
0x59, 0x00,
0x5a, 0x00,
0x5b, 0x00,
0x5c, 0x00,
0x5d, 0x00,

0x80, 0x00,
0x81, 0x00,
0x82, 0x00,
0x83, 0x00,
0x88, 0x20,
0x8a, 0x30,
0x8c, 0x00,
0x90, 0x00,
0x91, 0x60,
0x92, 0x00,
0x93, 0x60,

//////////////////
// Page 0x02
//////////////////
0x03, 0x02,
0x10, 0x00,
0x11, 0x00,
0x12, 0x70,
0x13, 0x00,
0x16, 0x00,
0x17, 0x00,
0x19, 0x00,
0x1a, 0x10,
0x1b, 0x00,
0x1c, 0xc0,
0x1d, 0x20,
0x20, 0x04,
0x21, 0x04,
0x22, 0x06,
0x23, 0x10,
0x24, 0x04,
0x28, 0x00,
0x29, 0x06,
0x2a, 0x00,
0x2e, 0x00,
0x2f, 0x2c,
0x30, 0x00,
0x31, 0x44,
0x32, 0x02,
0x33, 0x00,
0x34, 0x00,
0x35, 0x00,
0x36, 0x06,
0x37, 0xc0,
0x38, 0x00,
0x39, 0x32,
0x3a, 0x02,
0x3b, 0x04,
0x3c, 0x04,
0x3d, 0xfe,
0x3e, 0x00,
0x3f, 0x00,
0x40, 0x00,
0x41, 0x17,
0x42, 0x11,
0x43, 0x25,
0x47, 0x00,
0x48, 0x9a,
0x49, 0x24,
0x4a, 0x0f,
0x4b, 0x20,
0x4c, 0x06,
0x4d, 0xc0,
0x50, 0xa9,
0x51, 0x1c,
0x52, 0x73,
0x54, 0xc0,
0x55, 0x40,
0x56, 0x11,
0x57, 0x00,
0x58, 0x18,
0x59, 0x16,
0x5b, 0x00,
0x62, 0x00,
0x63, 0xc8,
0x67, 0x3f,
0x68, 0xc0,
0x70, 0x03,
0x71, 0xc7,
0x72, 0x06,
0x73, 0x75,
0x74, 0x03,
0x75, 0xc7,
0x76, 0x05,
0x77, 0x1d,
0xa0, 0x01,
0xa1, 0x2c,
0xa2, 0x02,
0xa3, 0xe0,
0xa4, 0x03,
0xa5, 0x84,
0xa6, 0x06,
0xa7, 0xf6,
0xb0, 0x02,
0xb1, 0x10,
0xb2, 0x02,
0xb3, 0xdc,
0xb4, 0x03,
0xb5, 0xe2,
0xb6, 0x06,
0xb7, 0xf2,
0xc0, 0x02,
0xc1, 0x10,
0xc2, 0x02,
0xc3, 0xde,
0xc4, 0x03,
0xc5, 0xe2,
0xc6, 0x06,
0xc7, 0xf4,
0xc8, 0x01,
0xc9, 0x8e,
0xca, 0x01,
0xcb, 0x3e,
0xcc, 0x03,
0xcd, 0x1e,
0xce, 0x02,
0xcf, 0xe2,
0xd0, 0x00,
0xd1, 0x00,
0xd2, 0x00,
0xd3, 0x00,
0xd4, 0x1b,
0xd5, 0x00,
0xe0, 0x1c,
0xe1, 0x1c,
0xe2, 0x1c,
0xe3, 0x04,
0xe4, 0x1c,
0xe5, 0x01,
0xe8, 0x00,
0xe9, 0x00,
0xea, 0x00,
0xeb, 0x00,
0xec, 0x00,
0xed, 0x00,
0xf0, 0x70,
0xf1, 0x00,
0xf2, 0x82,
0xf3, 0x00,

//////////////////
// Page 0x03
//////////////////
0x03, 0x03,
0x10, 0x00,
0x11, 0x80,
0x12, 0x00,
0x13, 0x02,
0x14, 0x06,
0x15, 0xeb,
0x16, 0x06,
0x17, 0xf5,
0x18, 0x00,
0x19, 0xe8,
0x1a, 0x06,
0x1b, 0xf6,
0x1c, 0x00,
0x1d, 0xe8,
0x1e, 0x06,
0x1f, 0xf6,
0x20, 0x00,
0x21, 0xe8,
0x22, 0x01,
0x23, 0x1c,
0x24, 0x00,
0x25, 0xe8,
0x26, 0x01,
0x27, 0x1c,
0x28, 0x00,
0x29, 0xe8,
0x2a, 0x01,
0x2b, 0x1e,
0x2c, 0x00,
0x2d, 0xe8,
0x2e, 0x01,
0x2f, 0x1e,
0x30, 0x01,
0x31, 0x2c,
0x32, 0x06,
0x33, 0xf6,
0x34, 0x01,
0x35, 0x2c,
0x36, 0x06,
0x37, 0xf6,
0x38, 0x06,
0x39, 0xf1,
0x3a, 0x06,
0x3b, 0xfb,
0x3c, 0x00,
0x3d, 0x04,
0x3e, 0x00,
0x3f, 0x0a,
0x40, 0x00,
0x41, 0x04,
0x42, 0x00,
0x43, 0x3c,
0x44, 0x00,
0x45, 0x02,
0x46, 0x00,
0x47, 0x74,
0x48, 0x00,
0x49, 0x06,
0x4a, 0x00,
0x4b, 0x3a,
0x4c, 0x00,
0x4d, 0x06,
0x4e, 0x00,
0x4f, 0x3a,
0x50, 0x00,
0x51, 0x0a,
0x52, 0x00,
0x53, 0x38,
0x54, 0x00,
0x55, 0x0a,
0x56, 0x00,
0x57, 0x38,
0x58, 0x00,
0x59, 0x0a,
0x5A, 0x00,
0x5b, 0x38,
0x60, 0x00,
0x61, 0x07,
0x62, 0x00,
0x63, 0x15,
0x64, 0x00,
0x65, 0x07,
0x66, 0x00,
0x68, 0x00,
0x69, 0x04,
0x6A, 0x00,
0x6B, 0x3e,
0x70, 0x00,
0x71, 0xbe,
0x72, 0x06,
0x73, 0xfa,
0x74, 0x00,
0x75, 0xc8,
0x76, 0x00,
0x77, 0xe4,
0x78, 0x00,
0x79, 0xc8,
0x7A, 0x00,
0x7B, 0xe4,
0x7C, 0x06,
0x7D, 0xf8,
0x7E, 0x06,
0x7F, 0xfc,
0x80, 0x02,
0x81, 0xe4,
0x82, 0x03,
0x83, 0x2e,
0x84, 0x02,
0x85, 0xe4,
0x86, 0x03,
0x87, 0x2e,
0x88, 0x06,
0x89, 0xf8,
0x8A, 0x06,
0x8B, 0xfc,
0x90, 0x00,
0x91, 0xbe,
0x92, 0x06,
0x93, 0xf6,
0x94, 0x00,
0x95, 0xbe,
0x96, 0x06,
0x97, 0xf6,
0x98, 0x06,
0x99, 0xf6,
0x9a, 0x00,
0x9b, 0xbe,
0x9c, 0x06,
0x9d, 0xf6,
0x9e, 0x00,
0x9f, 0xbe,
0xa0, 0x00,
0xa1, 0x06,
0xa2, 0x00,
0xa3, 0x16,
0xa4, 0x00,
0xa5, 0x06,
0xa6, 0x00,
0xa7, 0x16,
0xa8, 0x00,
0xa9, 0xc0,
0xaa, 0x00,
0xab, 0xd0,
0xac, 0x00,
0xad, 0xc0,
0xae, 0x00,
0xaf, 0xd0,
0xb0, 0x00,
0xb1, 0xe6,
0xb2, 0x06,
0xb3, 0xfa,
0xb4, 0x00,
0xb5, 0xe6,
0xb6, 0x06,
0xb7, 0xfa,
0xe0, 0x00,
0xe1, 0xbe,
0xe2, 0x02,
0xe3, 0xe0,
0xe4, 0x03,
0xe5, 0x05,
0xe6, 0x06,
0xe7, 0xda,
0xe8, 0x00,
0xe9, 0xe6,
0xea, 0x02,
0xeb, 0xe0,
0xec, 0x06,
0xed, 0xfc,
0xee, 0x00,
0xef, 0x00,
0xf6, 0x00,
0xf7, 0x04,
0xf8, 0x00,
0xf9, 0x0a,

//////////////////
// Page 0x04
//////////////////
0x03, 0x04,
0x10, 0x02,
0x11, 0x04,
0x12, 0x00,
0x13, 0x00,
0x14, 0x02,
0x1a, 0x00,
0x1b, 0x30,
0x1c, 0x00,
0x1d, 0xc0,
0x1e, 0x44,
0x20, 0x00,
0x21, 0x38,
0x22, 0x00,
0x23, 0x70,
0x24, 0x00,
0x25, 0xa8,
0x26, 0x00,
0x27, 0xc5,
0x28, 0x01,
0x29, 0x8a,
0x2a, 0x02,
0x2b, 0x4f,
0x30, 0x01,
0x31, 0x3c,
0x32, 0x01,
0x33, 0x3c,
0x34, 0x01,
0x35, 0x34,
0x36, 0x01,
0x37, 0x02,
0x38, 0x01,
0x39, 0x02,
0x3a, 0x01,
0x3b, 0x02,
0x40, 0x01,
0x41, 0x3e,
0x42, 0x01,
0x43, 0x3e,
0x44, 0x01,
0x45, 0x3e,
0x46, 0x01,
0x47, 0x02,
0x48, 0x01,
0x49, 0x02,
0x4a, 0x01,
0x4b, 0x02,
0x50, 0x00,
0x58, 0x00,
0x59, 0xc0,
0x5a, 0x06,
0x5b, 0xfa,
0x5c, 0x00,
0x5d, 0xc0,
0x5e, 0x06,
0x5f, 0xfa,

0x60, 0x00,
0x61, 0x60,
0x62, 0x00,
0x63, 0x40,
0x64, 0x00,
0x65, 0x60,
0x66, 0x00,
0x67, 0x40,
0x68, 0x00,
0x69, 0x60,
0x6a, 0x00,
0x6b, 0x40,
0x70, 0x18,
0x71, 0x20,
0x72, 0x20,
0x73, 0x00,
0x80, 0x6f,
0x81, 0x00,
0x82, 0x2f,
0x83, 0x00,
0x84, 0x13,
0x85, 0x01,
0x86, 0x00,
0x87, 0x00,
0x90, 0x03,
0x91, 0x06,
0x92, 0x06,
0x93, 0x06,
0x94, 0x06,
0x95, 0x00,
0x96, 0x40,
0x97, 0x50,
0x98, 0x70,
0xa0, 0x06,
0xa1, 0xf6,
0xa2, 0x06,
0xa3, 0xf6,
0xa4, 0x06,
0xa5, 0xf6,
0xa6, 0x01,
0xa7, 0x02,
0xa8, 0x01,
0xa9, 0x02,
0xaa, 0x01,
0xab, 0x02,
0xb0, 0x04,
0xb1, 0x04,
0xb2, 0x00,
0xb3, 0x04,
0xb4, 0x00,
0xc0, 0x00,
0xc1, 0x48,
0xc2, 0x00,
0xc3, 0x6e,
0xc4, 0x00,
0xc5, 0x4d,
0xc6, 0x00,
0xc7, 0x6c,
0xc8, 0x00,
0xc9, 0x4f,
0xca, 0x00,
0xcb, 0x6a,
0xcc, 0x00,
0xcd, 0x50,
0xce, 0x00,
0xcf, 0x68,
0xd0, 0x07,
0xd1, 0x00,
0xd2, 0x03,
0xd3, 0x03,
0xe0, 0x00,
0xe1, 0x10,
0xe2, 0x67,
0xe3, 0x00,

//////////////////
// Page 0x08
//////////////////
0x03, 0x08,
0x10, 0x07,
0x20, 0x01,
0x21, 0x00,
0x22, 0x01,
0x23, 0x00,
0x24, 0x01,
0x25, 0x00,
0x26, 0x01,
0x27, 0x00,
0x28, 0x01,
0x29, 0x00,
0x2a, 0x01,
0x2b, 0x00,
0x2c, 0x01,
0x2d, 0x00,
0x2e, 0x01,
0x2f, 0x00,
0x30, 0x03,
0x31, 0xff,
0x32, 0x03,
0x33, 0xff,
0x34, 0x03,
0x35, 0xff,
0x36, 0x03,
0x37, 0xff,
0x40, 0x07,
0x50, 0x01,
0x51, 0x00,
0x52, 0x01,
0x53, 0x00,
0x54, 0x0f,
0x55, 0xff,

//////////////////
// Page 0x10
//////////////////
0x03, 0x10,
0x10, 0x00,
0x11, 0x00,

//////////////////
// Page 0x20
//////////////////
0x03, 0x20,
0x10, 0x00,
0x11, 0x05,
0x12, 0x03,
0x22, 0x01,
0x23, 0x6a,
0x26, 0xff,
0x27, 0xff,
0x29, 0x00,
0x2a, 0x02,
0x2b, 0x00,
0x2c, 0x04,
0x30, 0x00,
0x31, 0x04,

0x40, 0x09,
0x41, 0x1e,
0x42, 0x60,

0x52, 0x0f,
0x53, 0xf3,

0x60, 0xef,
0x61, 0x00,

0x64, 0x0f,
0x65, 0x00,

//////////////////
// MIPI Setting
//////////////////
0x03, 0x05,
0x39, 0x55,
0x4c, 0x20,
0x4d, 0x00,
0x4e, 0x40,
0x4f, 0x00,

0x11, 0x00,
0x14, 0x01,
0x16, 0x12,
0x18, 0x80,
0x19, 0x00,
0x1a, 0xf0,
0x24, 0x2b,

0x32, 0x1e,
0x33, 0x0f,
0x34, 0x06,
0x35, 0x05,
0x36, 0x01,
0x37, 0x08,

0x1c, 0x01,
0x1d, 0x09,
0x1e, 0x0f,
0x1f, 0x0b,
0x30, 0x07,
0x31, 0xd0,
0x10, 0x1d,
};
#endif


static void sensor_init(void)
{
#if MULTI_WRITE
	hi259_table_write_cmos_sensor(
		addr_data_pair_init_hi259ofilm,
		sizeof(addr_data_pair_init_hi259ofilm) /
		sizeof(kal_uint16));
#else
    write_cmos_sensor(0x03, 0x00);
    write_cmos_sensor(0x01, 0x01);
    write_cmos_sensor(0x01, 0x03);
    write_cmos_sensor(0x01, 0x01);

//////////////////
// BGR enable
//////////////////
    write_cmos_sensor(0x03, 0x02);
    write_cmos_sensor(0x1f, 0x01);

    write_cmos_sensor(0x03, 0x00);
    write_cmos_sensor(0x07, 0x05);
    write_cmos_sensor(0x08, 0x62);
    write_cmos_sensor(0x09, 0x13);
    write_cmos_sensor(0x07, 0x85);
    write_cmos_sensor(0x07, 0x85);
    write_cmos_sensor(0x07, 0x85);
    write_cmos_sensor(0x0a, 0x80);
    write_cmos_sensor(0x07, 0xC5);

    write_cmos_sensor(0x03, 0x00);
    write_cmos_sensor(0x10, 0x00);
    write_cmos_sensor(0x11, 0x80);
    write_cmos_sensor(0x13, 0x01);
    write_cmos_sensor(0x14, 0x20);
    write_cmos_sensor(0x15, 0x81);
    write_cmos_sensor(0x17, 0x10);
    write_cmos_sensor(0x1a, 0x00);
    write_cmos_sensor(0x1c, 0x00);
    write_cmos_sensor(0x1f, 0x00);

    write_cmos_sensor(0x20, 0x00);
    write_cmos_sensor(0x21, 0x0e);
    write_cmos_sensor(0x22, 0x00);
    write_cmos_sensor(0x23, 0x10);
    write_cmos_sensor(0x24, 0x00);
    write_cmos_sensor(0x25, 0x0e);
    write_cmos_sensor(0x26, 0x00);
    write_cmos_sensor(0x27, 0x10);
    write_cmos_sensor(0x28, 0x04);
    write_cmos_sensor(0x29, 0xb0);
    write_cmos_sensor(0x2a, 0x06);
    write_cmos_sensor(0x2b, 0x40);

    write_cmos_sensor(0x30, 0x00);
    write_cmos_sensor(0x31, 0x00);
    write_cmos_sensor(0x32, 0x00);
    write_cmos_sensor(0x33, 0x00);
    write_cmos_sensor(0x34, 0x00);
    write_cmos_sensor(0x35, 0x00);
    write_cmos_sensor(0x36, 0x00);
    write_cmos_sensor(0x37, 0x00);
    write_cmos_sensor(0x38, 0x02);
    write_cmos_sensor(0x39, 0x60);
    write_cmos_sensor(0x3a, 0x03);
    write_cmos_sensor(0x3b, 0x20);
    write_cmos_sensor(0x4c, 0x07);
    write_cmos_sensor(0x4d, 0x08);
    write_cmos_sensor(0x4e, 0x05);
    write_cmos_sensor(0x4f, 0xaa);
    write_cmos_sensor(0x54, 0x02);
    write_cmos_sensor(0x55, 0x03);
    write_cmos_sensor(0x56, 0x04);
    write_cmos_sensor(0x57, 0x40);
    write_cmos_sensor(0x58, 0x03);
    write_cmos_sensor(0x5c, 0x0a);
    write_cmos_sensor(0x60, 0x00);
    write_cmos_sensor(0x61, 0x00);
    write_cmos_sensor(0x62, 0x80);
    write_cmos_sensor(0x68, 0x03);
    write_cmos_sensor(0x69, 0x42);
    write_cmos_sensor(0x80, 0x00);
    write_cmos_sensor(0x81, 0x08);
    write_cmos_sensor(0x82, 0x00);
    write_cmos_sensor(0x83, 0x06);
    write_cmos_sensor(0x84, 0x06);
    write_cmos_sensor(0x85, 0x50);
    write_cmos_sensor(0x86, 0x04);
    write_cmos_sensor(0x87, 0xcc);
    write_cmos_sensor(0x88, 0x00);
    write_cmos_sensor(0x89, 0x06);
    write_cmos_sensor(0x8a, 0x02);
    write_cmos_sensor(0x8b, 0x60);
    write_cmos_sensor(0x90, 0x00);
    write_cmos_sensor(0x91, 0x02);
    write_cmos_sensor(0xa0, 0x01);
    write_cmos_sensor(0xa1, 0x40);
    write_cmos_sensor(0xa2, 0x40);
    write_cmos_sensor(0xa3, 0x40);
    write_cmos_sensor(0xa4, 0x40);
    write_cmos_sensor(0xe4, 0x10);
    write_cmos_sensor(0xe5, 0x00);

//////////////////
// Page 0x01
//////////////////
    write_cmos_sensor(0x03, 0x01);
    write_cmos_sensor(0x10, 0x21);
    write_cmos_sensor(0x11, 0x00);
    write_cmos_sensor(0x12, 0x3f);
    write_cmos_sensor(0x13, 0x08);
    write_cmos_sensor(0x14, 0x04);
    write_cmos_sensor(0x15, 0x01);
    write_cmos_sensor(0x16, 0x00);
    write_cmos_sensor(0x17, 0x03);
    write_cmos_sensor(0x18, 0x00);
    write_cmos_sensor(0x19, 0x00);
    write_cmos_sensor(0x20, 0x60);
    write_cmos_sensor(0x21, 0x00);
    write_cmos_sensor(0x22, 0x20);
    write_cmos_sensor(0x23, 0x3c);
    write_cmos_sensor(0x24, 0x5c);
    write_cmos_sensor(0x25, 0x00);
    write_cmos_sensor(0x26, 0x60);
    write_cmos_sensor(0x27, 0x07);
    write_cmos_sensor(0x28, 0x80);
    write_cmos_sensor(0x29, 0x00);
    write_cmos_sensor(0x2a, 0xff);
    write_cmos_sensor(0x2b, 0x20);
    write_cmos_sensor(0x2c, 0x80);
    write_cmos_sensor(0x2d, 0x80);
    write_cmos_sensor(0x2e, 0x80);
    write_cmos_sensor(0x2f, 0x80);
    write_cmos_sensor(0x85, 0x10);

    write_cmos_sensor(0x30, 0x7b);
    write_cmos_sensor(0x31, 0x01);
    write_cmos_sensor(0x32, 0xfe);
    write_cmos_sensor(0x33, 0x00);
    write_cmos_sensor(0x34, 0x51);
    write_cmos_sensor(0x35, 0xb6);
    write_cmos_sensor(0x36, 0xfc);
    write_cmos_sensor(0x38, 0x66);
    write_cmos_sensor(0x39, 0x66);

    write_cmos_sensor(0x40, 0x00);
    write_cmos_sensor(0x41, 0x00);
    write_cmos_sensor(0x42, 0x00);
    write_cmos_sensor(0x43, 0x00);
    write_cmos_sensor(0x44, 0x00);
    write_cmos_sensor(0x45, 0x00);

    write_cmos_sensor(0x48, 0x00);
    write_cmos_sensor(0x49, 0x00);
    write_cmos_sensor(0x4a, 0x00);
    write_cmos_sensor(0x4b, 0x00);
    write_cmos_sensor(0x4c, 0x00);
    write_cmos_sensor(0x4d, 0x00);

    write_cmos_sensor(0x50, 0x00);
    write_cmos_sensor(0x51, 0x00);
    write_cmos_sensor(0x52, 0x00);
    write_cmos_sensor(0x53, 0x00);
    write_cmos_sensor(0x54, 0x00);
    write_cmos_sensor(0x55, 0x00);

    write_cmos_sensor(0x58, 0x00);
    write_cmos_sensor(0x59, 0x00);
    write_cmos_sensor(0x5a, 0x00);
    write_cmos_sensor(0x5b, 0x00);
    write_cmos_sensor(0x5c, 0x00);
    write_cmos_sensor(0x5d, 0x00);

    write_cmos_sensor(0x80, 0x00);
    write_cmos_sensor(0x81, 0x00);
    write_cmos_sensor(0x82, 0x00);
    write_cmos_sensor(0x83, 0x00);
    write_cmos_sensor(0x88, 0x20);
    write_cmos_sensor(0x8a, 0x30);
    write_cmos_sensor(0x8c, 0x00);
    write_cmos_sensor(0x90, 0x00);
    write_cmos_sensor(0x91, 0x60);
    write_cmos_sensor(0x92, 0x00);
    write_cmos_sensor(0x93, 0x60);

//////////////////
// Page 0x02
//////////////////
    write_cmos_sensor(0x03, 0x02);
    write_cmos_sensor(0x10, 0x00);
    write_cmos_sensor(0x11, 0x00);
    write_cmos_sensor(0x12, 0x70);
    write_cmos_sensor(0x13, 0x00);
    write_cmos_sensor(0x16, 0x00);
    write_cmos_sensor(0x17, 0x00);
    write_cmos_sensor(0x19, 0x00);
    write_cmos_sensor(0x1a, 0x10);
    write_cmos_sensor(0x1b, 0x00);
    write_cmos_sensor(0x1c, 0xc0);
    write_cmos_sensor(0x1d, 0x20);
    write_cmos_sensor(0x20, 0x04);
    write_cmos_sensor(0x21, 0x04);
    write_cmos_sensor(0x22, 0x06);
    write_cmos_sensor(0x23, 0x10);
    write_cmos_sensor(0x24, 0x04);
    write_cmos_sensor(0x28, 0x00);
    write_cmos_sensor(0x29, 0x06);
    write_cmos_sensor(0x2a, 0x00);
    write_cmos_sensor(0x2e, 0x00);
    write_cmos_sensor(0x2f, 0x2c);
    write_cmos_sensor(0x30, 0x00);
    write_cmos_sensor(0x31, 0x44);
    write_cmos_sensor(0x32, 0x02);
    write_cmos_sensor(0x33, 0x00);
    write_cmos_sensor(0x34, 0x00);
    write_cmos_sensor(0x35, 0x00);
    write_cmos_sensor(0x36, 0x06);
    write_cmos_sensor(0x37, 0xc0);
    write_cmos_sensor(0x38, 0x00);
    write_cmos_sensor(0x39, 0x32);
    write_cmos_sensor(0x3a, 0x02);
    write_cmos_sensor(0x3b, 0x04);
    write_cmos_sensor(0x3c, 0x04);
    write_cmos_sensor(0x3d, 0xfe);
    write_cmos_sensor(0x3e, 0x00);
    write_cmos_sensor(0x3f, 0x00);
    write_cmos_sensor(0x40, 0x00);
    write_cmos_sensor(0x41, 0x17);
    write_cmos_sensor(0x42, 0x11);
    write_cmos_sensor(0x43, 0x25);
    write_cmos_sensor(0x47, 0x00);
    write_cmos_sensor(0x48, 0x9a);
    write_cmos_sensor(0x49, 0x24);
    write_cmos_sensor(0x4a, 0x0f);
    write_cmos_sensor(0x4b, 0x20);
    write_cmos_sensor(0x4c, 0x06);
    write_cmos_sensor(0x4d, 0xc0);
    write_cmos_sensor(0x50, 0xa9);
    write_cmos_sensor(0x51, 0x1c);
    write_cmos_sensor(0x52, 0x73);
    write_cmos_sensor(0x54, 0xc0);
    write_cmos_sensor(0x55, 0x40);
    write_cmos_sensor(0x56, 0x11);
    write_cmos_sensor(0x57, 0x00);
    write_cmos_sensor(0x58, 0x18);
    write_cmos_sensor(0x59, 0x16);
    write_cmos_sensor(0x5b, 0x00);
    write_cmos_sensor(0x62, 0x00);
    write_cmos_sensor(0x63, 0xc8);
    write_cmos_sensor(0x67, 0x3f);
    write_cmos_sensor(0x68, 0xc0);
    write_cmos_sensor(0x70, 0x03);
    write_cmos_sensor(0x71, 0xc7);
    write_cmos_sensor(0x72, 0x06);
    write_cmos_sensor(0x73, 0x75);
    write_cmos_sensor(0x74, 0x03);
    write_cmos_sensor(0x75, 0xc7);
    write_cmos_sensor(0x76, 0x05);
    write_cmos_sensor(0x77, 0x1d);
    write_cmos_sensor(0xa0, 0x01);
    write_cmos_sensor(0xa1, 0x2c);
    write_cmos_sensor(0xa2, 0x02);
    write_cmos_sensor(0xa3, 0xe0);
    write_cmos_sensor(0xa4, 0x03);
    write_cmos_sensor(0xa5, 0x84);
    write_cmos_sensor(0xa6, 0x06);
    write_cmos_sensor(0xa7, 0xf6);
    write_cmos_sensor(0xb0, 0x02);
    write_cmos_sensor(0xb1, 0x10);
    write_cmos_sensor(0xb2, 0x02);
    write_cmos_sensor(0xb3, 0xdc);
    write_cmos_sensor(0xb4, 0x03);
    write_cmos_sensor(0xb5, 0xe2);
    write_cmos_sensor(0xb6, 0x06);
    write_cmos_sensor(0xb7, 0xf2);
    write_cmos_sensor(0xc0, 0x02);
    write_cmos_sensor(0xc1, 0x10);
    write_cmos_sensor(0xc2, 0x02);
    write_cmos_sensor(0xc3, 0xde);
    write_cmos_sensor(0xc4, 0x03);
    write_cmos_sensor(0xc5, 0xe2);
    write_cmos_sensor(0xc6, 0x06);
    write_cmos_sensor(0xc7, 0xf4);
    write_cmos_sensor(0xc8, 0x01);
    write_cmos_sensor(0xc9, 0x8e);
    write_cmos_sensor(0xca, 0x01);
    write_cmos_sensor(0xcb, 0x3e);
    write_cmos_sensor(0xcc, 0x03);
    write_cmos_sensor(0xcd, 0x1e);
    write_cmos_sensor(0xce, 0x02);
    write_cmos_sensor(0xcf, 0xe2);
    write_cmos_sensor(0xd0, 0x00);
    write_cmos_sensor(0xd1, 0x00);
    write_cmos_sensor(0xd2, 0x00);
    write_cmos_sensor(0xd3, 0x00);
    write_cmos_sensor(0xd4, 0x1d);
    write_cmos_sensor(0xd5, 0x00);
    write_cmos_sensor(0xe0, 0x1c);
    write_cmos_sensor(0xe1, 0x1c);
    write_cmos_sensor(0xe2, 0x1c);
    write_cmos_sensor(0xe3, 0x04);
    write_cmos_sensor(0xe4, 0x1c);
    write_cmos_sensor(0xe5, 0x01);
    write_cmos_sensor(0xe8, 0x00);
    write_cmos_sensor(0xe9, 0x00);
    write_cmos_sensor(0xea, 0x00);
    write_cmos_sensor(0xeb, 0x00);
    write_cmos_sensor(0xec, 0x00);
    write_cmos_sensor(0xed, 0x00);
    write_cmos_sensor(0xf0, 0x70);
    write_cmos_sensor(0xf1, 0x00);
    write_cmos_sensor(0xf2, 0x82);
    write_cmos_sensor(0xf3, 0x00);

//////////////////
// Page 0x03
//////////////////
    write_cmos_sensor(0x03, 0x03);
    write_cmos_sensor(0x10, 0x00);
    write_cmos_sensor(0x11, 0x80);
    write_cmos_sensor(0x12, 0x00);
    write_cmos_sensor(0x13, 0x02);
    write_cmos_sensor(0x14, 0x06);
    write_cmos_sensor(0x15, 0xeb);
    write_cmos_sensor(0x16, 0x06);
    write_cmos_sensor(0x17, 0xf5);
    write_cmos_sensor(0x18, 0x00);
    write_cmos_sensor(0x19, 0xe8);
    write_cmos_sensor(0x1a, 0x06);
    write_cmos_sensor(0x1b, 0xf6);
    write_cmos_sensor(0x1c, 0x00);
    write_cmos_sensor(0x1d, 0xe8);
    write_cmos_sensor(0x1e, 0x06);
    write_cmos_sensor(0x1f, 0xf6);
    write_cmos_sensor(0x20, 0x00);
    write_cmos_sensor(0x21, 0xe8);
    write_cmos_sensor(0x22, 0x01);
    write_cmos_sensor(0x23, 0x1c);
    write_cmos_sensor(0x24, 0x00);
    write_cmos_sensor(0x25, 0xe8);
    write_cmos_sensor(0x26, 0x01);
    write_cmos_sensor(0x27, 0x1c);
    write_cmos_sensor(0x28, 0x00);
    write_cmos_sensor(0x29, 0xe8);
    write_cmos_sensor(0x2a, 0x01);
    write_cmos_sensor(0x2b, 0x1e);
    write_cmos_sensor(0x2c, 0x00);
    write_cmos_sensor(0x2d, 0xe8);
    write_cmos_sensor(0x2e, 0x01);
    write_cmos_sensor(0x2f, 0x1e);
    write_cmos_sensor(0x30, 0x01);
    write_cmos_sensor(0x31, 0x2c);
    write_cmos_sensor(0x32, 0x06);
    write_cmos_sensor(0x33, 0xf6);
    write_cmos_sensor(0x34, 0x01);
    write_cmos_sensor(0x35, 0x2c);
    write_cmos_sensor(0x36, 0x06);
    write_cmos_sensor(0x37, 0xf6);
    write_cmos_sensor(0x38, 0x06);
    write_cmos_sensor(0x39, 0xf1);
    write_cmos_sensor(0x3a, 0x06);
    write_cmos_sensor(0x3b, 0xfb);
    write_cmos_sensor(0x3c, 0x00);
    write_cmos_sensor(0x3d, 0x04);
    write_cmos_sensor(0x3e, 0x00);
    write_cmos_sensor(0x3f, 0x0a);
    write_cmos_sensor(0x40, 0x00);
    write_cmos_sensor(0x41, 0x04);
    write_cmos_sensor(0x42, 0x00);
    write_cmos_sensor(0x43, 0x3c);
    write_cmos_sensor(0x44, 0x00);
    write_cmos_sensor(0x45, 0x02);
    write_cmos_sensor(0x46, 0x00);
    write_cmos_sensor(0x47, 0x74);
    write_cmos_sensor(0x48, 0x00);
    write_cmos_sensor(0x49, 0x06);
    write_cmos_sensor(0x4a, 0x00);
    write_cmos_sensor(0x4b, 0x3a);
    write_cmos_sensor(0x4c, 0x00);
    write_cmos_sensor(0x4d, 0x06);
    write_cmos_sensor(0x4e, 0x00);
    write_cmos_sensor(0x4f, 0x3a);
    write_cmos_sensor(0x50, 0x00);
    write_cmos_sensor(0x51, 0x0a);
    write_cmos_sensor(0x52, 0x00);
    write_cmos_sensor(0x53, 0x38);
    write_cmos_sensor(0x54, 0x00);
    write_cmos_sensor(0x55, 0x0a);
    write_cmos_sensor(0x56, 0x00);
    write_cmos_sensor(0x57, 0x38);
    write_cmos_sensor(0x58, 0x00);
    write_cmos_sensor(0x59, 0x0a);
    write_cmos_sensor(0x5A, 0x00);
    write_cmos_sensor(0x5b, 0x38);
    write_cmos_sensor(0x60, 0x00);
    write_cmos_sensor(0x61, 0x07);
    write_cmos_sensor(0x62, 0x00);
    write_cmos_sensor(0x63, 0x15);
    write_cmos_sensor(0x64, 0x00);
    write_cmos_sensor(0x65, 0x07);
    write_cmos_sensor(0x66, 0x00);
    write_cmos_sensor(0x68, 0x00);
    write_cmos_sensor(0x69, 0x04);
    write_cmos_sensor(0x6A, 0x00);
    write_cmos_sensor(0x6B, 0x3e);
    write_cmos_sensor(0x70, 0x00);
    write_cmos_sensor(0x71, 0xbe);
    write_cmos_sensor(0x72, 0x06);
    write_cmos_sensor(0x73, 0xfa);
    write_cmos_sensor(0x74, 0x00);
    write_cmos_sensor(0x75, 0xc8);
    write_cmos_sensor(0x76, 0x00);
    write_cmos_sensor(0x77, 0xe4);
    write_cmos_sensor(0x78, 0x00);
    write_cmos_sensor(0x79, 0xc8);
    write_cmos_sensor(0x7A, 0x00);
    write_cmos_sensor(0x7B, 0xe4);
    write_cmos_sensor(0x7C, 0x06);
    write_cmos_sensor(0x7D, 0xf8);
    write_cmos_sensor(0x7E, 0x06);
    write_cmos_sensor(0x7F, 0xfc);
    write_cmos_sensor(0x80, 0x02);
    write_cmos_sensor(0x81, 0xe4);
    write_cmos_sensor(0x82, 0x03);
    write_cmos_sensor(0x83, 0x2e);
    write_cmos_sensor(0x84, 0x02);
    write_cmos_sensor(0x85, 0xe4);
    write_cmos_sensor(0x86, 0x03);
    write_cmos_sensor(0x87, 0x2e);
    write_cmos_sensor(0x88, 0x06);
    write_cmos_sensor(0x89, 0xf8);
    write_cmos_sensor(0x8A, 0x06);
    write_cmos_sensor(0x8B, 0xfc);
    write_cmos_sensor(0x90, 0x00);
    write_cmos_sensor(0x91, 0xbe);
    write_cmos_sensor(0x92, 0x06);
    write_cmos_sensor(0x93, 0xf6);
    write_cmos_sensor(0x94, 0x00);
    write_cmos_sensor(0x95, 0xbe);
    write_cmos_sensor(0x96, 0x06);
    write_cmos_sensor(0x97, 0xf6);
    write_cmos_sensor(0x98, 0x06);
    write_cmos_sensor(0x99, 0xf6);
    write_cmos_sensor(0x9a, 0x00);
    write_cmos_sensor(0x9b, 0xbe);
    write_cmos_sensor(0x9c, 0x06);
    write_cmos_sensor(0x9d, 0xf6);
    write_cmos_sensor(0x9e, 0x00);
    write_cmos_sensor(0x9f, 0xbe);
    write_cmos_sensor(0xa0, 0x00);
    write_cmos_sensor(0xa1, 0x06);
    write_cmos_sensor(0xa2, 0x00);
    write_cmos_sensor(0xa3, 0x16);
    write_cmos_sensor(0xa4, 0x00);
    write_cmos_sensor(0xa5, 0x06);
    write_cmos_sensor(0xa6, 0x00);
    write_cmos_sensor(0xa7, 0x16);
    write_cmos_sensor(0xa8, 0x00);
    write_cmos_sensor(0xa9, 0xc0);
    write_cmos_sensor(0xaa, 0x00);
    write_cmos_sensor(0xab, 0xd0);
    write_cmos_sensor(0xac, 0x00);
    write_cmos_sensor(0xad, 0xc0);
    write_cmos_sensor(0xae, 0x00);
    write_cmos_sensor(0xaf, 0xd0);
    write_cmos_sensor(0xb0, 0x00);
    write_cmos_sensor(0xb1, 0xe6);
    write_cmos_sensor(0xb2, 0x06);
    write_cmos_sensor(0xb3, 0xfa);
    write_cmos_sensor(0xb4, 0x00);
    write_cmos_sensor(0xb5, 0xe6);
    write_cmos_sensor(0xb6, 0x06);
    write_cmos_sensor(0xb7, 0xfa);
    write_cmos_sensor(0xe0, 0x00);
    write_cmos_sensor(0xe1, 0xbe);
    write_cmos_sensor(0xe2, 0x02);
    write_cmos_sensor(0xe3, 0xe0);
    write_cmos_sensor(0xe4, 0x03);
    write_cmos_sensor(0xe5, 0x05);
    write_cmos_sensor(0xe6, 0x06);
    write_cmos_sensor(0xe7, 0xda);
    write_cmos_sensor(0xe8, 0x00);
    write_cmos_sensor(0xe9, 0xe6);
    write_cmos_sensor(0xea, 0x02);
    write_cmos_sensor(0xeb, 0xe0);
    write_cmos_sensor(0xec, 0x06);
    write_cmos_sensor(0xed, 0xfc);
    write_cmos_sensor(0xee, 0x00);
    write_cmos_sensor(0xef, 0x00);
    write_cmos_sensor(0xf6, 0x00);
    write_cmos_sensor(0xf7, 0x04);
    write_cmos_sensor(0xf8, 0x00);
    write_cmos_sensor(0xf9, 0x0a);

//////////////////
// Page 0x04
//////////////////
    write_cmos_sensor(0x03, 0x04);
    write_cmos_sensor(0x10, 0x02);
    write_cmos_sensor(0x11, 0x04);
    write_cmos_sensor(0x12, 0x00);
    write_cmos_sensor(0x13, 0x00);
    write_cmos_sensor(0x14, 0x02);
    write_cmos_sensor(0x1a, 0x00);
    write_cmos_sensor(0x1b, 0x30);
    write_cmos_sensor(0x1c, 0x00);
    write_cmos_sensor(0x1d, 0xc0);
    write_cmos_sensor(0x1e, 0x44);
    write_cmos_sensor(0x20, 0x00);
    write_cmos_sensor(0x21, 0x38);
    write_cmos_sensor(0x22, 0x00);
    write_cmos_sensor(0x23, 0x70);
    write_cmos_sensor(0x24, 0x00);
    write_cmos_sensor(0x25, 0xa8);
    write_cmos_sensor(0x26, 0x00);
    write_cmos_sensor(0x27, 0xc5);
    write_cmos_sensor(0x28, 0x01);
    write_cmos_sensor(0x29, 0x8a);
    write_cmos_sensor(0x2a, 0x02);
    write_cmos_sensor(0x2b, 0x4f);
    write_cmos_sensor(0x30, 0x01);
    write_cmos_sensor(0x31, 0x3c);
    write_cmos_sensor(0x32, 0x01);
    write_cmos_sensor(0x33, 0x3c);
    write_cmos_sensor(0x34, 0x01);
    write_cmos_sensor(0x35, 0x34);
    write_cmos_sensor(0x36, 0x01);
    write_cmos_sensor(0x37, 0x02);
    write_cmos_sensor(0x38, 0x01);
    write_cmos_sensor(0x39, 0x02);
    write_cmos_sensor(0x3a, 0x01);
    write_cmos_sensor(0x3b, 0x02);
    write_cmos_sensor(0x40, 0x01);
    write_cmos_sensor(0x41, 0x3e);
    write_cmos_sensor(0x42, 0x01);
    write_cmos_sensor(0x43, 0x3e);
    write_cmos_sensor(0x44, 0x01);
    write_cmos_sensor(0x45, 0x3e);
    write_cmos_sensor(0x46, 0x01);
    write_cmos_sensor(0x47, 0x02);
    write_cmos_sensor(0x48, 0x01);
    write_cmos_sensor(0x49, 0x02);
    write_cmos_sensor(0x4a, 0x01);
    write_cmos_sensor(0x4b, 0x02);
    write_cmos_sensor(0x50, 0x00);
    write_cmos_sensor(0x58, 0x00);
    write_cmos_sensor(0x59, 0xc0);
    write_cmos_sensor(0x5a, 0x06);
    write_cmos_sensor(0x5b, 0xfa);
    write_cmos_sensor(0x5c, 0x00);
    write_cmos_sensor(0x5d, 0xc0);
    write_cmos_sensor(0x5e, 0x06);
    write_cmos_sensor(0x5f, 0xfa);

    write_cmos_sensor(0x60, 0x00);
    write_cmos_sensor(0x61, 0x60);
    write_cmos_sensor(0x62, 0x00);
    write_cmos_sensor(0x63, 0x40);
    write_cmos_sensor(0x64, 0x00);
    write_cmos_sensor(0x65, 0x60);
    write_cmos_sensor(0x66, 0x00);
    write_cmos_sensor(0x67, 0x40);
    write_cmos_sensor(0x68, 0x00);
    write_cmos_sensor(0x69, 0x60);
    write_cmos_sensor(0x6a, 0x00);
    write_cmos_sensor(0x6b, 0x40);
    write_cmos_sensor(0x70, 0x18);
    write_cmos_sensor(0x71, 0x20);
    write_cmos_sensor(0x72, 0x20);
    write_cmos_sensor(0x73, 0x00);
    write_cmos_sensor(0x80, 0x6f);
    write_cmos_sensor(0x81, 0x00);
    write_cmos_sensor(0x82, 0x2f);
    write_cmos_sensor(0x83, 0x00);
    write_cmos_sensor(0x84, 0x13);
    write_cmos_sensor(0x85, 0x01);
    write_cmos_sensor(0x86, 0x00);
    write_cmos_sensor(0x87, 0x00);
    write_cmos_sensor(0x90, 0x03);
    write_cmos_sensor(0x91, 0x06);
    write_cmos_sensor(0x92, 0x06);
    write_cmos_sensor(0x93, 0x06);
    write_cmos_sensor(0x94, 0x06);
    write_cmos_sensor(0x95, 0x00);
    write_cmos_sensor(0x96, 0x40);
    write_cmos_sensor(0x97, 0x50);
    write_cmos_sensor(0x98, 0x70);
    write_cmos_sensor(0xa0, 0x06);
    write_cmos_sensor(0xa1, 0xf6);
    write_cmos_sensor(0xa2, 0x06);
    write_cmos_sensor(0xa3, 0xf6);
    write_cmos_sensor(0xa4, 0x06);
    write_cmos_sensor(0xa5, 0xf6);
    write_cmos_sensor(0xa6, 0x01);
    write_cmos_sensor(0xa7, 0x02);
    write_cmos_sensor(0xa8, 0x01);
    write_cmos_sensor(0xa9, 0x02);
    write_cmos_sensor(0xaa, 0x01);
    write_cmos_sensor(0xab, 0x02);
    write_cmos_sensor(0xb0, 0x04);
    write_cmos_sensor(0xb1, 0x04);
    write_cmos_sensor(0xb2, 0x00);
    write_cmos_sensor(0xb3, 0x04);
    write_cmos_sensor(0xb4, 0x00);
    write_cmos_sensor(0xc0, 0x00);
    write_cmos_sensor(0xc1, 0x48);
    write_cmos_sensor(0xc2, 0x00);
    write_cmos_sensor(0xc3, 0x6e);
    write_cmos_sensor(0xc4, 0x00);
    write_cmos_sensor(0xc5, 0x4d);
    write_cmos_sensor(0xc6, 0x00);
    write_cmos_sensor(0xc7, 0x6c);
    write_cmos_sensor(0xc8, 0x00);
    write_cmos_sensor(0xc9, 0x4f);
    write_cmos_sensor(0xca, 0x00);
    write_cmos_sensor(0xcb, 0x6a);
    write_cmos_sensor(0xcc, 0x00);
    write_cmos_sensor(0xcd, 0x50);
    write_cmos_sensor(0xce, 0x00);
    write_cmos_sensor(0xcf, 0x68);
    write_cmos_sensor(0xd0, 0x07);
    write_cmos_sensor(0xd1, 0x00);
    write_cmos_sensor(0xd2, 0x03);
    write_cmos_sensor(0xd3, 0x03);
    write_cmos_sensor(0xe0, 0x00);
    write_cmos_sensor(0xe1, 0x10);
    write_cmos_sensor(0xe2, 0x67);
    write_cmos_sensor(0xe3, 0x00);

//////////////////
// Page 0x08
//////////////////
    write_cmos_sensor(0x03, 0x08);
    write_cmos_sensor(0x10, 0x07);
    write_cmos_sensor(0x20, 0x01);
    write_cmos_sensor(0x21, 0x00);
    write_cmos_sensor(0x22, 0x01);
    write_cmos_sensor(0x23, 0x00);
    write_cmos_sensor(0x24, 0x01);
    write_cmos_sensor(0x25, 0x00);
    write_cmos_sensor(0x26, 0x01);
    write_cmos_sensor(0x27, 0x00);
    write_cmos_sensor(0x28, 0x01);
    write_cmos_sensor(0x29, 0x00);
    write_cmos_sensor(0x2a, 0x01);
    write_cmos_sensor(0x2b, 0x00);
    write_cmos_sensor(0x2c, 0x01);
    write_cmos_sensor(0x2d, 0x00);
    write_cmos_sensor(0x2e, 0x01);
    write_cmos_sensor(0x2f, 0x00);
    write_cmos_sensor(0x30, 0x03);
    write_cmos_sensor(0x31, 0xff);
    write_cmos_sensor(0x32, 0x03);
    write_cmos_sensor(0x33, 0xff);
    write_cmos_sensor(0x34, 0x03);
    write_cmos_sensor(0x35, 0xff);
    write_cmos_sensor(0x36, 0x03);
    write_cmos_sensor(0x37, 0xff);
    write_cmos_sensor(0x40, 0x07);
    write_cmos_sensor(0x50, 0x01);
    write_cmos_sensor(0x51, 0x00);
    write_cmos_sensor(0x52, 0x01);
    write_cmos_sensor(0x53, 0x00);
    write_cmos_sensor(0x54, 0x0f);
    write_cmos_sensor(0x55, 0xff);

//////////////////
// Page 0x10
//////////////////
    write_cmos_sensor(0x03, 0x10);
    write_cmos_sensor(0x10, 0x00);
    write_cmos_sensor(0x11, 0x00);

//////////////////
// Page 0x20
//////////////////
    write_cmos_sensor(0x03, 0x20);
    write_cmos_sensor(0x10, 0x00);
    write_cmos_sensor(0x11, 0x05);
    write_cmos_sensor(0x12, 0x03);
    write_cmos_sensor(0x22, 0x01);
    write_cmos_sensor(0x23, 0x6a);
    write_cmos_sensor(0x26, 0xff);
    write_cmos_sensor(0x27, 0xff);
    write_cmos_sensor(0x29, 0x00);
    write_cmos_sensor(0x2a, 0x02);
    write_cmos_sensor(0x2b, 0x00);
    write_cmos_sensor(0x2c, 0x04);
    write_cmos_sensor(0x30, 0x00);
    write_cmos_sensor(0x31, 0x04);

    write_cmos_sensor(0x40, 0x09);
    write_cmos_sensor(0x41, 0x1e);
    write_cmos_sensor(0x42, 0x60);

    write_cmos_sensor(0x52, 0x0f);
    write_cmos_sensor(0x53, 0xf3);

    write_cmos_sensor(0x60, 0xef);
    write_cmos_sensor(0x61, 0x00);

    write_cmos_sensor(0x64, 0x0f);
    write_cmos_sensor(0x65, 0x00);

//////////////////
// MIPI Setting
//////////////////
    write_cmos_sensor(0x03, 0x05);
    write_cmos_sensor(0x39, 0x55);
    write_cmos_sensor(0x4c, 0x20);
    write_cmos_sensor(0x4d, 0x00);
    write_cmos_sensor(0x4e, 0x40);
    write_cmos_sensor(0x4f, 0x00);

    write_cmos_sensor(0x11, 0x00);
    write_cmos_sensor(0x14, 0x01);
    write_cmos_sensor(0x16, 0x12);
    write_cmos_sensor(0x18, 0x80);
    write_cmos_sensor(0x19, 0x00);
    write_cmos_sensor(0x1a, 0xf0);
    write_cmos_sensor(0x24, 0x2b);

    write_cmos_sensor(0x32, 0x1e);
    write_cmos_sensor(0x33, 0x0f);
    write_cmos_sensor(0x34, 0x06);
    write_cmos_sensor(0x35, 0x05);
    write_cmos_sensor(0x36, 0x01);
    write_cmos_sensor(0x37, 0x08);

    write_cmos_sensor(0x1c, 0x01);
    write_cmos_sensor(0x1d, 0x09);
    write_cmos_sensor(0x1e, 0x0f);
    write_cmos_sensor(0x1f, 0x0b);
    write_cmos_sensor(0x30, 0x07);
    write_cmos_sensor(0x31, 0xd0);
    write_cmos_sensor(0x10, 0x1d);

#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_preview_hi259ofilm[] = {

/*
//Sensor Information////////////////////////////
//Sensor            : Hi-259
//Date              : 2020-01-10
//Customer          :
//Image size        : 1600x1200_30fps
//MCLK              : 24MHz
//PLCK				: 78.4MHz
//MIPI speed(Mbps)  : 784Mbps x 1Lane
//Frame Length	    : 1450
//Line Length 	    : 1800
//Pixel order       : B
//Frame rate        : 30.00fps
////////////////////////////////////////////////
*/

0x03, 0x00,
0x07, 0x05,
0x08, 0x62,
0x09, 0x13,
0x07, 0x85,
0x07, 0x85,
0x07, 0x85,
0x0a, 0x80,
0x07, 0xC5,

0x03, 0x00,
0x10, 0x00,

0x20, 0x00,
0x21, 0x0e,
0x22, 0x00,
0x23, 0x10,
0x24, 0x00,
0x25, 0x0e,
0x26, 0x00,
0x27, 0x10,
0x28, 0x04,
0x29, 0xb0,
0x2a, 0x06,
0x2b, 0x40,

0x30, 0x00,
0x31, 0x10,
0x32, 0x00,
0x33, 0x10,
0x34, 0x00,
0x35, 0x10,
0x36, 0x00,
0x37, 0x10,
0x38, 0x04,
0x39, 0xb0,
0x3a, 0x06,
0x3b, 0x40,

0x4e, 0x05, //framelength_H
0x4f, 0xaa, //framelength_L

0x80, 0x00,
0x81, 0x00,
0x82, 0x00,
0x83, 0x00,
0x84, 0x06,
0x85, 0x60,
0x86, 0x04,
0x87, 0xcc,
0x88, 0x00,
0x89, 0x00,
0x8a, 0x04,
0x8b, 0xcc,

0x03, 0x02,
0x3a, 0x02,

0x03, 0x04,
0xb0, 0x04,
0xb1, 0x04,
0xb2, 0x00,
0xb3, 0x04,
0xb4, 0x00,

0x03, 0x10,
0x10, 0x00,

0x03, 0x05,

0x32, 0x1e,
0x33, 0x0f,
0x34, 0x06,
0x35, 0x05,
0x36, 0x01,
0x37, 0x08,

0x1a, 0xf0,
0x1c, 0x01,
0x1d, 0x09,
0x1e, 0x0f,
0x1f, 0x0b,
0x30, 0x07,
0x31, 0xd0,
};
#endif

static void preview_setting(void)
{
#if MULTI_WRITE
	hi259_table_write_cmos_sensor(
		addr_data_pair_preview_hi259ofilm,
		sizeof(addr_data_pair_preview_hi259ofilm) /
		sizeof(kal_uint16));
#else
    write_cmos_sensor(0x03, 0x00);
    write_cmos_sensor(0x07, 0x05);
    write_cmos_sensor(0x08, 0x62);
    write_cmos_sensor(0x09, 0x13);
    write_cmos_sensor(0x07, 0x85);
    write_cmos_sensor(0x07, 0x85);
    write_cmos_sensor(0x07, 0x85);
    write_cmos_sensor(0x0a, 0x80);
    write_cmos_sensor(0x07, 0xC5);

    write_cmos_sensor(0x03, 0x00);
    write_cmos_sensor(0x10, 0x00);

    write_cmos_sensor(0x20, 0x00);
    write_cmos_sensor(0x21, 0x0e);
    write_cmos_sensor(0x22, 0x00);
    write_cmos_sensor(0x23, 0x10);
    write_cmos_sensor(0x24, 0x00);
    write_cmos_sensor(0x25, 0x0e);
    write_cmos_sensor(0x26, 0x00);
    write_cmos_sensor(0x27, 0x10);
    write_cmos_sensor(0x28, 0x04);
    write_cmos_sensor(0x29, 0xb0);
    write_cmos_sensor(0x2a, 0x06);
    write_cmos_sensor(0x2b, 0x40);

    write_cmos_sensor(0x30, 0x00);
    write_cmos_sensor(0x31, 0x10);
    write_cmos_sensor(0x32, 0x00);
    write_cmos_sensor(0x33, 0x10);
    write_cmos_sensor(0x34, 0x00);
    write_cmos_sensor(0x35, 0x10);
    write_cmos_sensor(0x36, 0x00);
    write_cmos_sensor(0x37, 0x10);
    write_cmos_sensor(0x38, 0x04);
    write_cmos_sensor(0x39, 0xb0);
    write_cmos_sensor(0x3a, 0x06);
    write_cmos_sensor(0x3b, 0x40);

    write_cmos_sensor(0x4e, 0x05); //framelength_H
    write_cmos_sensor(0x4f, 0xaa); //framelength_L

    write_cmos_sensor(0x80, 0x00);
    write_cmos_sensor(0x81, 0x00);
    write_cmos_sensor(0x82, 0x00);
    write_cmos_sensor(0x83, 0x00);
    write_cmos_sensor(0x84, 0x06);
    write_cmos_sensor(0x85, 0x60);
    write_cmos_sensor(0x86, 0x04);
    write_cmos_sensor(0x87, 0xcc);
    write_cmos_sensor(0x88, 0x00);
    write_cmos_sensor(0x89, 0x00);
    write_cmos_sensor(0x8a, 0x04);
    write_cmos_sensor(0x8b, 0xcc);

    write_cmos_sensor(0x03, 0x02);
    write_cmos_sensor(0x3a, 0x02);

    write_cmos_sensor(0x03, 0x04);
    write_cmos_sensor(0xb0, 0x04);
    write_cmos_sensor(0xb1, 0x04);
    write_cmos_sensor(0xb2, 0x00);
    write_cmos_sensor(0xb3, 0x04);
    write_cmos_sensor(0xb4, 0x00);

    write_cmos_sensor(0x03, 0x10);
    write_cmos_sensor(0x10, 0x00);

    write_cmos_sensor(0x03, 0x05);

    write_cmos_sensor(0x32, 0x1e);
    write_cmos_sensor(0x33, 0x0f);
    write_cmos_sensor(0x34, 0x06);
    write_cmos_sensor(0x35, 0x05);
    write_cmos_sensor(0x36, 0x01);
    write_cmos_sensor(0x37, 0x08);

    write_cmos_sensor(0x1a, 0xf0);
    write_cmos_sensor(0x1c, 0x01);
    write_cmos_sensor(0x1d, 0x09);
    write_cmos_sensor(0x1e, 0x0f);
    write_cmos_sensor(0x1f, 0x0b);
    write_cmos_sensor(0x30, 0x07);
    write_cmos_sensor(0x31, 0xd0);

#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_capture_30fps_hi259ofilm[] = {

/*
//Sensor Information////////////////////////////
//Sensor            : Hi-259
//Date              : 2020-01-10
//Customer          :
//Image size        : 1600x1200_30fps
//MCLK              : 24MHz
//PLCK				: 78.4MHz
//MIPI speed(Mbps)  : 784Mbps x 1Lane
//Frame Length	    : 1450
//Line Length 	    : 1800
//Pixel order       : B
//Frame rate        : 30.00fps
////////////////////////////////////////////////
*/

0x03, 0x00,
0x07, 0x05,
0x08, 0x62,
0x09, 0x13,
0x07, 0x85,
0x07, 0x85,
0x07, 0x85,
0x0a, 0x80,
0x07, 0xC5,

0x03, 0x00,
0x10, 0x00,

0x20, 0x00,
0x21, 0x0e,
0x22, 0x00,
0x23, 0x10,
0x24, 0x00,
0x25, 0x0e,
0x26, 0x00,
0x27, 0x10,
0x28, 0x04,
0x29, 0xb0,
0x2a, 0x06,
0x2b, 0x40,

0x30, 0x00,
0x31, 0x10,
0x32, 0x00,
0x33, 0x10,
0x34, 0x00,
0x35, 0x10,
0x36, 0x00,
0x37, 0x10,
0x38, 0x04,
0x39, 0xb0,
0x3a, 0x06,
0x3b, 0x40,

0x4e, 0x05, //framelength_H
0x4f, 0xaa, //framelength_L

0x80, 0x00,
0x81, 0x00,
0x82, 0x00,
0x83, 0x00,
0x84, 0x06,
0x85, 0x60,
0x86, 0x04,
0x87, 0xcc,
0x88, 0x00,
0x89, 0x00,
0x8a, 0x04,
0x8b, 0xcc,

0x03, 0x02,
0x3a, 0x02,

0x03, 0x04,
0xb0, 0x04,
0xb1, 0x04,
0xb2, 0x00,
0xb3, 0x04,
0xb4, 0x00,

0x03, 0x10,
0x10, 0x00,

0x03, 0x05,

0x32, 0x1e,
0x33, 0x0f,
0x34, 0x06,
0x35, 0x05,
0x36, 0x01,
0x37, 0x08,

0x1a, 0xf0,
0x1c, 0x01,
0x1d, 0x09,
0x1e, 0x0f,
0x1f, 0x0b,
0x30, 0x07,
0x31, 0xd0,
};

kal_uint16 addr_data_pair_capture_15fps_hi259ofilm[] = {
0x03, 0x00,
0x01, 0x01,
};
#endif


static void capture_setting(kal_uint16 currefps)
{
#if MULTI_WRITE
	if (currefps == 300) {
	hi259_table_write_cmos_sensor(
		addr_data_pair_capture_30fps_hi259ofilm,
		sizeof(addr_data_pair_capture_30fps_hi259ofilm) /
		sizeof(kal_uint16));

	} else {
	hi259_table_write_cmos_sensor(
		addr_data_pair_capture_15fps_hi259ofilm,
		sizeof(addr_data_pair_capture_15fps_hi259ofilm) /
		sizeof(kal_uint16));
	}
#else
  if( currefps == 300) {
    write_cmos_sensor(0x03, 0x00);
    write_cmos_sensor(0x07, 0x05);
    write_cmos_sensor(0x08, 0x62);
    write_cmos_sensor(0x09, 0x13);
    write_cmos_sensor(0x07, 0x85);
    write_cmos_sensor(0x07, 0x85);
    write_cmos_sensor(0x07, 0x85);
    write_cmos_sensor(0x0a, 0x80);
    write_cmos_sensor(0x07, 0xC5);

    write_cmos_sensor(0x03, 0x00);
    write_cmos_sensor(0x10, 0x00);

    write_cmos_sensor(0x20, 0x00);
    write_cmos_sensor(0x21, 0x0e);
    write_cmos_sensor(0x22, 0x00);
    write_cmos_sensor(0x23, 0x10);
    write_cmos_sensor(0x24, 0x00);
    write_cmos_sensor(0x25, 0x0e);
    write_cmos_sensor(0x26, 0x00);
    write_cmos_sensor(0x27, 0x10);
    write_cmos_sensor(0x28, 0x04);
    write_cmos_sensor(0x29, 0xb0);
    write_cmos_sensor(0x2a, 0x06);
    write_cmos_sensor(0x2b, 0x40);

    write_cmos_sensor(0x30, 0x00);
    write_cmos_sensor(0x31, 0x10);
    write_cmos_sensor(0x32, 0x00);
    write_cmos_sensor(0x33, 0x10);
    write_cmos_sensor(0x34, 0x00);
    write_cmos_sensor(0x35, 0x10);
    write_cmos_sensor(0x36, 0x00);
    write_cmos_sensor(0x37, 0x10);
    write_cmos_sensor(0x38, 0x04);
    write_cmos_sensor(0x39, 0xb0);
    write_cmos_sensor(0x3a, 0x06);
    write_cmos_sensor(0x3b, 0x40);

    write_cmos_sensor(0x4e, 0x05); //framelength_H
    write_cmos_sensor(0x4f, 0xaa); //framelength_L

    write_cmos_sensor(0x80, 0x00);
    write_cmos_sensor(0x81, 0x00);
    write_cmos_sensor(0x82, 0x00);
    write_cmos_sensor(0x83, 0x00);
    write_cmos_sensor(0x84, 0x06);
    write_cmos_sensor(0x85, 0x60);
    write_cmos_sensor(0x86, 0x04);
    write_cmos_sensor(0x87, 0xcc);
    write_cmos_sensor(0x88, 0x00);
    write_cmos_sensor(0x89, 0x00);
    write_cmos_sensor(0x8a, 0x04);
    write_cmos_sensor(0x8b, 0xcc);

    write_cmos_sensor(0x03, 0x02);
    write_cmos_sensor(0x3a, 0x02);

    write_cmos_sensor(0x03, 0x04);
    write_cmos_sensor(0xb0, 0x04);
    write_cmos_sensor(0xb1, 0x04);
    write_cmos_sensor(0xb2, 0x00);
    write_cmos_sensor(0xb3, 0x04);
    write_cmos_sensor(0xb4, 0x00);

    write_cmos_sensor(0x03, 0x10);
    write_cmos_sensor(0x10, 0x00);

    write_cmos_sensor(0x03, 0x05);

    write_cmos_sensor(0x32, 0x1e);
    write_cmos_sensor(0x33, 0x0f);
    write_cmos_sensor(0x34, 0x06);
    write_cmos_sensor(0x35, 0x05);
    write_cmos_sensor(0x36, 0x01);
    write_cmos_sensor(0x37, 0x08);

    write_cmos_sensor(0x1a, 0xf0);
    write_cmos_sensor(0x1c, 0x01);
    write_cmos_sensor(0x1d, 0x09);
    write_cmos_sensor(0x1e, 0x0f);
    write_cmos_sensor(0x1f, 0x0b);
    write_cmos_sensor(0x30, 0x07);
    write_cmos_sensor(0x31, 0xd0);


// PIP
  } else {
    write_cmos_sensor(0x03, 0x00);
    write_cmos_sensor(0x01, 0x01);
  }
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_video_hi259ofilm[] = {
0x03, 0x00,
0x07, 0x05,
0x08, 0x62,
0x09, 0x13,
0x07, 0x85,
0x07, 0x85,
0x07, 0x85,
0x0a, 0x80,
0x07, 0xC5,

0x03, 0x00,
0x10, 0x00,

0x20, 0x00,
0x21, 0x0e,
0x22, 0x00,
0x23, 0x10,
0x24, 0x00,
0x25, 0x0e,
0x26, 0x00,
0x27, 0x10,
0x28, 0x04,
0x29, 0xb0,
0x2a, 0x06,
0x2b, 0x40,

0x30, 0x00,
0x31, 0x10,
0x32, 0x00,
0x33, 0x10,
0x34, 0x00,
0x35, 0x10,
0x36, 0x00,
0x37, 0x10,
0x38, 0x04,
0x39, 0xb0,
0x3a, 0x06,
0x3b, 0x40,

0x4e, 0x05, //framelength_H
0x4f, 0xaa, //framelength_L

0x80, 0x00,
0x81, 0x00,
0x82, 0x00,
0x83, 0x00,
0x84, 0x06,
0x85, 0x60,
0x86, 0x04,
0x87, 0xcc,
0x88, 0x00,
0x89, 0x00,
0x8a, 0x04,
0x8b, 0xcc,

0x03, 0x02,
0x3a, 0x02,

0x03, 0x04,
0xb0, 0x04,
0xb1, 0x04,
0xb2, 0x00,
0xb3, 0x04,
0xb4, 0x00,

0x03, 0x10,
0x10, 0x00,

0x03, 0x05,

0x32, 0x1e,
0x33, 0x0f,
0x34, 0x06,
0x35, 0x05,
0x36, 0x01,
0x37, 0x08,

0x1a, 0xf0,
0x1c, 0x01,
0x1d, 0x09,
0x1e, 0x0f,
0x1f, 0x0b,
0x30, 0x07,
0x31, 0xd0,
};
#endif

static void normal_video_setting(void)
{
#if MULTI_WRITE
	hi259_table_write_cmos_sensor(
		addr_data_pair_video_hi259ofilm,
		sizeof(addr_data_pair_video_hi259ofilm) /
		sizeof(kal_uint16));
#else
    write_cmos_sensor(0x03, 0x00);
    write_cmos_sensor(0x07, 0x05);
    write_cmos_sensor(0x08, 0x62);
    write_cmos_sensor(0x09, 0x13);
    write_cmos_sensor(0x07, 0x85);
    write_cmos_sensor(0x07, 0x85);
    write_cmos_sensor(0x07, 0x85);
    write_cmos_sensor(0x0a, 0x80);
    write_cmos_sensor(0x07, 0xC5);

    write_cmos_sensor(0x03, 0x00);
    write_cmos_sensor(0x10, 0x00);

    write_cmos_sensor(0x20, 0x00);
    write_cmos_sensor(0x21, 0x0e);
    write_cmos_sensor(0x22, 0x00);
    write_cmos_sensor(0x23, 0x10);
    write_cmos_sensor(0x24, 0x00);
    write_cmos_sensor(0x25, 0x0e);
    write_cmos_sensor(0x26, 0x00);
    write_cmos_sensor(0x27, 0x10);
    write_cmos_sensor(0x28, 0x04);
    write_cmos_sensor(0x29, 0xb0);
    write_cmos_sensor(0x2a, 0x06);
    write_cmos_sensor(0x2b, 0x40);

    write_cmos_sensor(0x30, 0x00);
    write_cmos_sensor(0x31, 0x10);
    write_cmos_sensor(0x32, 0x00);
    write_cmos_sensor(0x33, 0x10);
    write_cmos_sensor(0x34, 0x00);
    write_cmos_sensor(0x35, 0x10);
    write_cmos_sensor(0x36, 0x00);
    write_cmos_sensor(0x37, 0x10);
    write_cmos_sensor(0x38, 0x04);
    write_cmos_sensor(0x39, 0xb0);
    write_cmos_sensor(0x3a, 0x06);
    write_cmos_sensor(0x3b, 0x40);

    write_cmos_sensor(0x4e, 0x05); //framelength_H
    write_cmos_sensor(0x4f, 0xaa); //framelength_L

    write_cmos_sensor(0x80, 0x00);
    write_cmos_sensor(0x81, 0x00);
    write_cmos_sensor(0x82, 0x00);
    write_cmos_sensor(0x83, 0x00);
    write_cmos_sensor(0x84, 0x06);
    write_cmos_sensor(0x85, 0x60);
    write_cmos_sensor(0x86, 0x04);
    write_cmos_sensor(0x87, 0xcc);
    write_cmos_sensor(0x88, 0x00);
    write_cmos_sensor(0x89, 0x00);
    write_cmos_sensor(0x8a, 0x04);
    write_cmos_sensor(0x8b, 0xcc);

    write_cmos_sensor(0x03, 0x02);
    write_cmos_sensor(0x3a, 0x02);

    write_cmos_sensor(0x03, 0x04);
    write_cmos_sensor(0xb0, 0x04);
    write_cmos_sensor(0xb1, 0x04);
    write_cmos_sensor(0xb2, 0x00);
    write_cmos_sensor(0xb3, 0x04);
    write_cmos_sensor(0xb4, 0x00);

    write_cmos_sensor(0x03, 0x10);
    write_cmos_sensor(0x10, 0x00);

    write_cmos_sensor(0x03, 0x05);

    write_cmos_sensor(0x32, 0x1e);
    write_cmos_sensor(0x33, 0x0f);
    write_cmos_sensor(0x34, 0x06);
    write_cmos_sensor(0x35, 0x05);
    write_cmos_sensor(0x36, 0x01);
    write_cmos_sensor(0x37, 0x08);

    write_cmos_sensor(0x1a, 0xf0);
    write_cmos_sensor(0x1c, 0x01);
    write_cmos_sensor(0x1d, 0x09);
    write_cmos_sensor(0x1e, 0x0f);
    write_cmos_sensor(0x1f, 0x0b);
    write_cmos_sensor(0x30, 0x07);
    write_cmos_sensor(0x31, 0xd0);

#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_hs_video_hi259ofilm[] = {
0x03, 0x00,
0x01, 0x01,
};
#endif

static void hs_video_setting(void)
{

#if MULTI_WRITE
	hi259_table_write_cmos_sensor(
		addr_data_pair_hs_video_hi259ofilm,
		sizeof(addr_data_pair_hs_video_hi259ofilm) /
		sizeof(kal_uint16));
#else
    write_cmos_sensor(0x03, 0x00);
    write_cmos_sensor(0x01, 0x01);

#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_slim_video_hi259ofilm[] = {
0x03, 0x00,
0x01, 0x01,
};
#endif


static void slim_video_setting(void)
{

#if MULTI_WRITE
	hi259_table_write_cmos_sensor(
		addr_data_pair_slim_video_hi259ofilm,
		sizeof(addr_data_pair_slim_video_hi259ofilm) /
		sizeof(kal_uint16));
#else
    write_cmos_sensor(0x03, 0x00);
    write_cmos_sensor(0x01, 0x01);

#endif
}

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0, vendor_id = 0;
	kal_uint8 retry = 2;

        vendor_id = get_vendor_id();
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
                    if (vendor_id == 0x07) {
                        *sensor_id = return_sensor_id();
                        LOG_INF("HI259  get_imgsensor_id sensor id: 0x%x\n",*sensor_id);
			if (*sensor_id == imgsensor_info.sensor_id) {
			LOG_INF("i2c write id : 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
			return ERROR_NONE;
			}
                    } else {
                        LOG_INF("hi259 ofilm vendor id not match : 0x%x\n", vendor_id);
                    }
			LOG_INF("Read sensor id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		LOG_INF("Read id fail,sensor id: 0x%x\n", *sensor_id);
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

	LOG_INF("[open]: PLATFORM:MT6765,MIPI 1LANE\n");
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id) {
		LOG_INF("open sensor id fail: 0x%x\n", sensor_id);
		return ERROR_SENSOR_CONNECT_FAIL;
	}
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
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	//imgsensor.pdaf_mode = 1;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}	/*	open  */
static kal_uint32 close(void)
{
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
	LOG_INF("E");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
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
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

	if (imgsensor.current_fps == imgsensor_info.cap.max_framerate)	{
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
	 //PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}

	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("Caputre fps:%d\n", imgsensor.current_fps);
	capture_setting(imgsensor.current_fps);

	return ERROR_NONE;

}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting();
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	return ERROR_NONE;
}    /*    hs_video   */

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

	return ERROR_NONE;
}    /*    slim_video     */

static kal_uint32 get_resolution(
		MSDK_SENSOR_RESOLUTION_INFO_STRUCT * sensor_resolution)
{
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;
	sensor_resolution->SensorPreviewWidth =	imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;
	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;
	sensor_resolution->SensorHighSpeedVideoWidth = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight =	imgsensor_info.hs_video.grabwindow_height;
	sensor_resolution->SensorSlimVideoWidth = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight = imgsensor_info.slim_video.grabwindow_height;
	return ERROR_NONE;
}    /*    get_resolution    */


static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_INFO_STRUCT *sensor_info,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame =	imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =	imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent =	imgsensor_info.isp_driving_current;
/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame =	imgsensor_info.ae_shut_delay_frame;
/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame =	imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =	imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber =	imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
	sensor_info->SensorHightSampling = 0;    // 0 is default 1x
	sensor_info->SensorPacketECCOrder = 1;

#if ENABLE_PDAF
	sensor_info->PDAF_Support = PDAF_SUPPORT_CAMSV;
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
	default:
	    sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
	break;
	}

	return ERROR_NONE;
}    /*    get_info  */


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
		LOG_INF("[odin]preview\n");
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		LOG_INF("[odin]capture\n");
	//case MSDK_SCENARIO_ID_CAMERA_ZSD:
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		LOG_INF("[odin]video preview\n");
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	    slim_video(image_window, sensor_config_data);
		break;
	default:
		LOG_INF("[odin]default mode\n");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d ", framerate);
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
	set_max_framerate(imgsensor.current_fps,1);

	return ERROR_NONE;
}


static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d ", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario( enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		//Test
		//framerate = 1200;

	    frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >	imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		//if (imgsensor.frame_length > imgsensor.shutter)
		//set_dummy();
	break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
	    frame_length = imgsensor_info.normal_video.pclk /
			framerate * 10 / imgsensor_info.normal_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.normal_video.framelength) ?
		(frame_length - imgsensor_info.normal_video.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		//if (imgsensor.frame_length > imgsensor.shutter)
		//set_dummy();
	break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps ==
				imgsensor_info.cap1.max_framerate) {
		frame_length = imgsensor_info.cap1.pclk / framerate * 10 /
				imgsensor_info.cap1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.cap1.framelength) ?
			(frame_length - imgsensor_info.cap1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap1.framelength +
				imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps !=
				imgsensor_info.cap.max_framerate)
			LOG_INF("fps %d fps not support,use cap: %d fps!\n",
			framerate, imgsensor_info.cap.max_framerate/10);
			frame_length = imgsensor_info.cap.pclk /
				framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length >
				imgsensor_info.cap.framelength) ?
			(frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length =
				imgsensor_info.cap.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		//if (imgsensor.frame_length > imgsensor.shutter)
			//set_dummy();
	break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	    frame_length = imgsensor_info.hs_video.pclk /
			framerate * 10 / imgsensor_info.hs_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.hs_video.framelength) ? (frame_length -
			imgsensor_info.hs_video.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.hs_video.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		//if (imgsensor.frame_length > imgsensor.shutter)
			//set_dummy();
	break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	    frame_length = imgsensor_info.slim_video.pclk /
			framerate * 10 / imgsensor_info.slim_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.slim_video.framelength) ? (frame_length -
			imgsensor_info.slim_video.framelength) : 0;
	    imgsensor.frame_length =
			imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		//if (imgsensor.frame_length > imgsensor.shutter)
			//set_dummy();
	break;
	default:  //coding with  preview scenario by default
	    frame_length = imgsensor_info.pre.pclk / framerate * 10 /
						imgsensor_info.pre.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.pre.framelength +
				imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		//if (imgsensor.frame_length > imgsensor.shutter)
			//set_dummy();
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
	LOG_INF("set_test_pattern_mode enable: %d", enable);
	if (enable) {
        write_cmos_sensor(0x03, 0x00);
        write_cmos_sensor(0x60, 0x04);
	} else {
        write_cmos_sensor(0x03, 0x00);
        write_cmos_sensor(0x60, 0x00);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
        write_cmos_sensor(0x03, 0x00);
        write_cmos_sensor(0x01, 0x00); //stream on
	}
	else{
        write_cmos_sensor(0x03, 0x00);
        write_cmos_sensor(0x01, 0x01); //stream off
	}
	mdelay(10);
	return ERROR_NONE;
}

static kal_uint32 feature_control(
			MSDK_SENSOR_FEATURE_ENUM feature_id,
			UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;

#if ENABLE_PDAF
    struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
#endif

	unsigned long long *feature_data =
		(unsigned long long *) feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_gain;
		*(feature_data + 2) = imgsensor_info.max_gain;
		break;
	case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
		*(feature_data + 0) = imgsensor_info.min_gain_iso;
		*(feature_data + 1) = imgsensor_info.gain_step;
		*(feature_data + 2) = imgsensor_info.gain_type;
		break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_shutter;
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
        write_cmos_sensor(sensor_reg_data->RegAddr,
						sensor_reg_data->RegData);
	break;
	case SENSOR_FEATURE_GET_REGISTER:
	    sensor_reg_data->RegData =
				read_cmos_sensor(sensor_reg_data->RegAddr);
	break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
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
	    set_auto_flicker_mode((BOOL)*feature_data_16,
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
	case SENSOR_FEATURE_SET_TEST_PATTERN:
	    set_test_pattern_mode((BOOL)*feature_data);
	break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
	    *feature_return_para_32 = imgsensor_info.checksum_value;
	    *feature_para_len = 4;
	break;
	case SENSOR_FEATURE_SET_FRAMERATE:
	    LOG_INF("current fps :%d\n", (UINT32)*feature_data);
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.current_fps = *feature_data;
	    spin_unlock(&imgsensor_drv_lock);
	break;

	case SENSOR_FEATURE_SET_HDR:
	    LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data);
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.ihdr_en = (BOOL)*feature_data;
	    spin_unlock(&imgsensor_drv_lock);
	break;
	case SENSOR_FEATURE_GET_CROP_INFO:
	    LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
				(UINT32)*feature_data);

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
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
	    LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
	#if 0
	    ihdr_write_shutter_gain((UINT16)*feature_data,
			(UINT16)*(feature_data+1), (UINT16)*(feature_data+2));
	#endif
	break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = 0;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
#if ENABLE_PDAF

		case SENSOR_FEATURE_GET_VC_INFO:
				LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n", (UINT16)*feature_data);
				pvcinfo = (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
				switch (*feature_data_32)
				{
					case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
						LOG_INF("SENSOR_FEATURE_GET_VC_INFO CAPTURE_JPEG\n");
						memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[1],sizeof(struct SENSOR_VC_INFO_STRUCT));
						break;
					case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
						LOG_INF("SENSOR_FEATURE_GET_VC_INFO VIDEO PREVIEW\n");
						memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[2],sizeof(struct SENSOR_VC_INFO_STRUCT));
						break;
					case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
					default:
						LOG_INF("SENSOR_FEATURE_GET_VC_INFO DEFAULT_PREVIEW\n");
						memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[0],sizeof(struct SENSOR_VC_INFO_STRUCT));
						break;
				}
				break;

		case SENSOR_FEATURE_GET_PDAF_DATA:
		LOG_INF("odin GET_PDAF_DATA EEPROM\n");
		// read from e2prom
#if e2prom
		read_eeprom((kal_uint16)(*feature_data),
				(char *)(uintptr_t)(*(feature_data+1)),
				(kal_uint32)(*(feature_data+2)) );
#else
		// read from file

	        LOG_INF("READ PDCAL DATA\n");
		read_hi259_eeprom((kal_uint16)(*feature_data),
				(char *)(uintptr_t)(*(feature_data+1)),
				(kal_uint32)(*(feature_data+2)) );

#endif
		break;
		case SENSOR_FEATURE_GET_PDAF_INFO:
			PDAFinfo= (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
			switch( *feature_data)
			{
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info, sizeof(struct SET_PD_BLOCK_INFO_T));
					break;
                //case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                case MSDK_SCENARIO_ID_SLIM_VIDEO:
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	 	 		default:
					break;
			}
		break;

	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%lld\n", *feature_data);
		//PDAF capacity enable or not, 2p8 only full size support PDAF
		switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1; // type2 - VC enable
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
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
		LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%lld\n", *feature_data);
		break;

	case SENSOR_FEATURE_SET_PDAF:
                imgsensor.pdaf_mode = *feature_data_16;
                LOG_INF("[odin] pdaf mode : %d \n", imgsensor.pdaf_mode);
				break;

#endif

	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	{
		kal_uint32 rate;
		switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				 rate =	imgsensor_info.cap.mipi_pixel_rate;
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
			default:
				rate = 0;
				break;
			}
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
	}
	break;



	default:
	break;
	}

	return ERROR_NONE;
}    /*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 HI259OFILM_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc =  &sensor_func;
	return ERROR_NONE;
}	/*	HI259_MIPI_RAW_SensorInit	*/

